// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Engine.hpp"
#include "glcommon.h"
#ifdef EENG_GLVERSION_43
#include "GLDebugMessageCallback.h"
#endif

#include "MainThreadQueue.hpp"
#include "InputManager.hpp"
#include "editor/CommandQueue.hpp"
#include "editor/CommandSanityChecks.hpp"
#include "BatchRegistry.hpp"
#include "ecs/EntityManager.hpp"

#include "LogMacros.h"
#include "LogGlobals.hpp"
#include "AssetMetaReg.hpp"
#include "ComponentMetaReg.hpp"
#include "util/Profiler.hpp"

#include "ImGuiBackendSDL.hpp"
#include "EventQueue.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include <chrono>
#include <memory>
#include <vector>

namespace
{
    void log_shutdown(eeng::EngineContext* ctx, const char* message)
    {
        std::cout << "[Shutdown] " << message << std::endl;
        if (ctx && ctx->log_manager)
            EENG_LOG_INFO(ctx, "%s", message);
    }

    void log_shutdown_warn(eeng::EngineContext* ctx, const char* message)
    {
        std::cerr << "[Shutdown] " << message << std::endl;
        if (ctx && ctx->log_manager)
            EENG_LOG_WARN(ctx, "%s", message);
    }

    void drain_shutdown_queues(eeng::EngineContext& ctx)
    {
        // Bounded drain: handle already-queued main-thread work and any events it
        // triggers, without risking an infinite loop during shutdown.
        constexpr int kMaxCycles = 8;
        for (int i = 0; i < kMaxCycles; ++i)
        {
            bool did_work = false;
            // 1) Run all currently queued main-thread tasks.
            if (ctx.main_thread_queue && !ctx.main_thread_queue->empty())
            {
                ctx.main_thread_queue->execute_all();
                did_work = true;
            }
            // 2) Dispatch any pending events. Handlers may enqueue more main-thread work.
            if (ctx.event_queue && ctx.event_queue->has_pending_events())
            {
                ctx.event_queue->dispatch_all_events();
                did_work = true;
            }
            // 3) Stop early if nothing was processed this cycle.
            if (!did_work)
                break;
        }
    }

    template<typename FutureType>
    bool pump_until_ready(
        eeng::EngineContext& ctx,
        FutureType& future,
        const char* label,
        Uint32 timeout_ms)
    {
        using namespace std::chrono_literals;
        const Uint32 start_ms = SDL_GetTicks();
        while (future.valid() &&
            future.wait_for(0ms) != std::future_status::ready)
        {
            // While waiting, keep main-thread work moving to avoid deadlocks
            // from tasks that call push_and_wait().
            drain_shutdown_queues(ctx);
            SDL_Delay(1);

            if (timeout_ms > 0 && SDL_GetTicks() - start_ms > timeout_ms)
            {
                log_shutdown_warn(&ctx, label);
                return false;
            }
        }
        return true;
    }

    bool pump_until_resource_idle(
        eeng::EngineContext& ctx,
        eeng::IResourceManager& rm,
        Uint32 timeout_ms)
    {
        const Uint32 start_ms = SDL_GetTicks();
        while (rm.is_busy())
        {
            // ResourceManager async work may block on main-thread operations.
            drain_shutdown_queues(ctx);
            SDL_Delay(1);

            if (timeout_ms > 0 && SDL_GetTicks() - start_ms > timeout_ms)
            {
                log_shutdown_warn(&ctx, "Shutdown: resource manager wait timed out");
                return false;
            }
        }
        return true;
    }
}

namespace eeng
{
    Engine::Engine(std::shared_ptr<EngineContext> ctx)
        : ctx(ctx)
    {
        if (ctx)
        {
            services_ = ctx->services_owner;
            edit_world_ = ctx->world_owner;
            if (services_)
                services_->play_mode_active.store(false, std::memory_order_relaxed);
        }
    }

    Engine::~Engine()
    {
        // Fallback cleanup: prefer running shutdown via the main loop so GL/SDL
        // are torn down on the main thread while the context is still valid.
        shutdown();
    }

    bool Engine::init(const char* title, int width, int height)
    {
        window_width = width;
        window_height = height;

        // Set the global logger
        eeng::LogGlobals::set_logger(ctx->log_manager);

        if (!init_sdl(title, width, height))    return false;
        if (!init_opengl())                     return false;
        if (!init_imgui())                      return false;

#ifdef EENG_DEBUG
        EENG_LOG_INFO(ctx, "Mode DEBUG");
#else
        EENG_LOG_INFO(ctx, "Mode RELEASE");
#endif

#ifdef EENG_COMPILER_MSVC
        EENG_LOG_INFO(ctx, "Compiler MSVC");
#elif defined(EENG_COMPILER_CLANG)
        EENG_LOG_INFO(ctx, "Compiler Clang");
#elif defined(EENG_COMPILER_GCC)
        EENG_LOG_INFO(ctx, "Compiler GCC");
#endif

#ifdef CPP20_SUPPORTED
        EENG_LOG_INFO(ctx, "C++ version 20");
#elif defined(CPP17_SUPPORTED)
        EENG_LOG_INFO(ctx, "C++ version 17");
#elif defined(CPP14_SUPPORTED)
        EENG_LOG_INFO(ctx, "C++ version 14");
#elif defined(CPP11_SUPPORTED)
        EENG_LOG_INFO(ctx, "C++ version 11");
#endif

        {
            int glMinor, glMajor;
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &glMinor);
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &glMajor);
            EENG_LOG_INFO(ctx, "GL version %i.%i (requested), %i.%i (actual)", EENG_GLVERSION_MAJOR, EENG_GLVERSION_MINOR, glMajor, glMinor);
        }

#ifdef EENG_MSAA
        {
            int actualMSAA;
            SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &actualMSAA);
            EENG_LOG_INFO(ctx, "MSAA %i (requested), %i (actual)", EENG_MSAA_SAMPLES, actualMSAA);
        }
#endif

        // Log some info about the anisotropic filtering settings
#ifdef EENG_ANISO
        {
            GLfloat maxAniso;
#if defined(EENG_GLVERSION_43)
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
#elif defined(EENG_GLVERSION_41)
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
#endif
            EENG_LOG_INFO(ctx, "Anisotropic samples %i (requested), %i (max))", EENG_ANISO_SAMPLES, (int)maxAniso);
        }
#endif
        register_asset_meta_types(*ctx);
        register_component_meta_types(*ctx);

        // Event subscriptions
        ctx->event_queue->register_callback([&](const SetVsyncEvent& event) { this->on_set_vsync(event); });
        ctx->event_queue->register_callback([&](const SetWireFrameRenderingEvent& event) { this->on_set_wireframe(event); });
        ctx->event_queue->register_callback([&](const SetDebugLoggingEvent& event) { this->on_set_debug_logging(event); });
        ctx->event_queue->register_callback([&](const SetMinFrameTimeEvent& event) { this->on_set_min_frametime(event); });
        ctx->event_queue->register_callback([&](const SetPlayModeEvent& event) { this->on_set_play_mode(event); });
        ctx->event_queue->register_callback([&](const TogglePlayModeEvent& event) { this->on_toggle_play_mode(event); });
        ctx->event_queue->register_callback([&](const ResourceTaskCompletedEvent& event) { this->on_resource_task_completed(event); });
        ctx->event_queue->register_callback([&](const BatchTaskCompletedEvent& event) { this->on_batch_task_completed(event); });

        // Engine config
        ctx->engine_config->set_flag(EngineFlag::VSync, true);
        ctx->engine_config->set_flag(EngineFlag::WireframeRendering, false);
        ctx->engine_config->set_flag(EngineFlag::DebugLogging, true);
        ctx->engine_config->set_value(EngineValue::MinFrameTime, 1000.0f / 60.0f);

        // Gui flags
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowEngineInfo, true);
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowProfiler, true);
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowLogWindow, true);
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowStorageWindow, true);
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowResourceBrowser, true);
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowSceneGraph, true);
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowEntityInspector, true);
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowBatchRegistry, true);
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowTaskMonitor, true);
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowCommandQueue, true);
        ctx->gui_manager->set_flag(eeng::GuiFlags::ShowAnimationGraphVisualizer, true);

        // Post command hook with sanity checks
#ifdef EENG_DEBUG
        ctx->command_queue->register_post_command_hook([ctx_weak = std::weak_ptr<EngineContext>{ ctx }]() {
            if (auto ctx_locked = ctx_weak.lock())
                editor::run_command_sanity_checks(*ctx_locked);
            });
#endif

        EENG_LOG(ctx, "Engine initialized successfully.");
        return true;
    }

    void Engine::run(std::unique_ptr<GameBase> game)
    {
        if (!game->init())
        {
            throw std::runtime_error("Game initialization failed");
        }

        bool running = true;
        shutdown_state_ = ShutdownState::Running;
        float time_s = 0.0f, time_ms, deltaTime_s = 0.016f;

        EENG_LOG(ctx, "Entering main loop...");
        while (running)
        {
            const auto now_ms = SDL_GetTicks();
            const auto now_s = now_ms * 0.001f;
            deltaTime_s = now_s - time_s;
            time_ms = now_ms;
            time_s = now_s;

            START_TIMER("Frame");

            START_TIMER("Frame", "Input Events");
            process_events(running); // input etc
            STOP_TIMER("Frame", "Input Events");
            if (shutdown_state_ == ShutdownState::Draining)
            {
                RESET_TIMER("Frame");
                // Shutdown is in-progress; keep the loop alive to pump queues,
                // render the GUI, and avoid deadlocks while we drain async work.
                if (advance_shutdown_drain())
                {
                    shutdown_state_ = ShutdownState::Teardown;
                    running = false;
                    continue;
                }

                // Keep GUI responsive during draining so the log window and
                // shutdown progress remain visible.
                begin_frame();
                end_frame();
                SDL_GL_SwapWindow(window_);
                SDL_Delay(1);
                continue;
            }

            START_TIMER("Frame", "Frame Begin");
            begin_frame(); // imgui_backend::show_demo_window(); ctx->gui_manager->draw(*ctx); GL setup
            STOP_TIMER("Frame", "Frame Begin");
            // =================================================================

            // update_input_lua(lua, SceneBase::axes, SceneBase::buttons);

            // Scripts may queue entities for destruction
            // ??? if (play_state == GamePlayState::Play) update_scripts(*registry, deltaTime_s);

            // -> after MT
            // ctx->entity_manager->destroy_pending_entities();

            // ??? scenegraph->traverse(registry);

            // ??? collisions

            // ??? Game thread tasks

            // ??? Physics step

            // --- Command queue execution ---
            // Can entities be destroyed here?
            // ctx->command_queue->execute_all(*registry, deltaTime_s);

            // --- Game systems ---
            START_TIMER("Frame", "Game Update");
            if (mode_ == EngineMode::Play)
                game->update_play(time_s, deltaTime_s);
            else
                game->update_edit(time_s, deltaTime_s);
            STOP_TIMER("Frame", "Game Update");

            // --- Main thread tasks ---
            // entt::storage mutations etc
            START_TIMER("Frame", "Main Thread Tasks");
            ctx->main_thread_queue->execute_all();
            STOP_TIMER("Frame", "Main Thread Tasks");

            START_TIMER("Frame", "Destroy Entities");
            int nbr_destroyed = ctx->entity_manager->destroy_pending_entities();
            STOP_TIMER("Frame", "Destroy Entities");
            // Log nbr of destroyed entities
            if (nbr_destroyed > 0) { EENG_LOG_DEBUG(ctx, "Destroyed %d pending entities", nbr_destroyed); }

            // --- Event dispatch ---
            START_TIMER("Frame", "Event Dispatch");
            ctx->event_queue->dispatch_all_events();
            STOP_TIMER("Frame", "Event Dispatch");

            // --- Event dispatch / Command execution / Entity destruction ---
#if 1
            START_TIMER("Frame", "Commands & Batches");
            if (mode_ == EngineMode::Edit
                && (ctx->command_queue->has_in_flight()
                    || ctx->command_queue->has_ready_commands()))
            {
                ctx->command_queue->process();
            }

            if (mode_ == EngineMode::Edit && ctx->batch_registry)
            {
                auto& br = static_cast<BatchRegistry&>(*ctx->batch_registry);
                br.process_dirty_batches(*ctx);
            }
            STOP_TIMER("Frame", "Commands & Batches");
#else
            //void Scene::event_loop()
            {
                int cycles = 0;
                const int max_cycles = 5;

                while ((dispatcher->has_pending_events() ||
                    cmd_queue->has_ready_commands() ||
                    dispatcher->has_pending_events())
                    && cycles++ <= max_cycles)
                {
                    // Dispatch events. May lead to commands being issued.
                    dispatcher->dispatch_all_events();

                    // Execute commands. May lead to entities being queued for destruction
                    // and new events being issued.
                    if (cmd_queue->has_ready_commands())
                        cmd_queue->process();

                    // Destroy entities flagged for destruction.
                    // May lead to additional entities being flagged for destruction.
                    destroy_pending_entities();
                }
                if (cycles > 1) std::cout << "Event loop cycles " << cycles << std::endl;
                assert(cycles <= max_cycles);
            }
#endif

            // --- Render ---
            START_TIMER("Frame", "Render");
            if (mode_ == EngineMode::Play)
                game->render_play(time_s, window_width, window_height);
            else
                game->render_edit(time_s, window_width, window_height);
            STOP_TIMER("Frame", "Render");

            // =================================================================
            START_TIMER("Frame", "Frame End");
            end_frame(); // ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            STOP_TIMER("Frame", "Frame End");

            START_TIMER("Frame", "Swap");
            SDL_GL_SwapWindow(window_);
            STOP_TIMER("Frame", "Swap");

            // Add a delay if frame time was shorter than the target frame time
            const Uint32 elapsed_ms = SDL_GetTicks() - time_ms;
            if (elapsed_ms < min_frametime_ms)
            {
                START_TIMER("Frame", "Frame Cap");
                SDL_Delay(min_frametime_ms - elapsed_ms);
                STOP_TIMER("Frame", "Frame Cap");
            }

            STOP_TIMER("Frame");
            util::Profiler::snapshot_and_reset("Frame");
        }

        game->destroy();
        shutdown();
    }

    void Engine::shutdown()
    {
        if (shutdown_started_.exchange(true))
            return;

        if (ctx)
        {
            // If the main loop already drained, skip expensive waits here and
            // proceed directly to teardown.
            if (!shutdown_drained_)
            {
                log_shutdown(ctx.get(), "Shutdown: begin");
                std::shared_future<TaskResult> unload_future;
                if (ctx->batch_registry)
                {
                    log_shutdown(ctx.get(), "Shutdown: request batch unloads");
                    unload_future = ctx->batch_registry->queue_unload_all_async(*ctx);
                }
                if (unload_future.valid())
                {
                    pump_until_ready(
                        *ctx,
                        unload_future,
                        "Shutdown: batch unload wait timed out",
                        5000);
                }

                if (ctx->resource_manager)
                {
                    log_shutdown(ctx.get(), "Shutdown: waiting for resource tasks");
                    pump_until_resource_idle(*ctx, *ctx->resource_manager, 5000);
                }

                shutdown_drained_ = true;
            }

            // Shutdown policy:
            // - Draining completes any async unloads while the main loop keeps running.
            // - Teardown happens after we gate new work, so no new tasks are enqueued
            //   while GUI/GL/SDL resources are being released.
            // We set the shutdown flag *after* draining batch/resource work so internal
            // unload tasks can still enqueue main-thread work (push_and_wait).
            // Alternative (stricter gating): set the flag before drains to block all new work.
            // Tradeoff: this can prevent unloads from completing or cause deadlocks.
            // For stricter gating with safe drains, add a separate "shutdown_draining"
            // flag or a main-loop state machine to allow internal shutdown tasks while
            // blocking external work.
            if (ctx->shutdown_requested)
                ctx->shutdown_requested->store(true, std::memory_order_relaxed);
            // Drain a few cycles to flush any already-queued main thread work and
            // dependent events before tearing down GUI/GL resources.
            log_shutdown(ctx.get(), "Shutdown: draining main-thread tasks and events");
            drain_shutdown_queues(*ctx);
        }

        // Todo: release any future subsystem managers (audio/physics/etc) before ImGui/GL teardown.

        if (ctx && ctx->gui_manager)
        {
            // GUI must be released before ImGui/GL teardown.
            log_shutdown(ctx.get(), "Shutdown: releasing GUI");
            ctx->gui_manager->release();
        }

        if (ctx)
            // ImGui shutdown requires a live GL context.
            log_shutdown(ctx.get(), "Shutdown: stopping ImGui backend");
        imgui_backend::shutdown();

        if (gl_context_)
            SDL_GL_DeleteContext(gl_context_);
        if (window_)
            SDL_DestroyWindow(window_);

        if (ctx)
            // SDL is the final platform shutdown step.
            log_shutdown(ctx.get(), "Shutdown: SDL_Quit");
        SDL_Quit();
    }

    bool Engine::init_sdl(const char* title, int width, int height)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
        {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, EENG_GLVERSION_MAJOR);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, EENG_GLVERSION_MINOR);
#ifdef EENG_MSAA
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, EENG_MSAA_SAMPLES);
#endif

        window_ = SDL_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        if (!window_)
        {
            std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
            return false;
        }

        gl_context_ = SDL_GL_CreateContext(window_);
        if (!gl_context_)
        {
            std::cerr << "Failed to create GL context: " << SDL_GetError() << std::endl;
            return false;
        }

        SDL_GL_MakeCurrent(window_, gl_context_);
        SDL_GL_SetSwapInterval(vsync);

        return true;
    }

    bool Engine::init_opengl()
    {
        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            std::cerr << "GLEW initialization failed: " << glewGetErrorString(err) << std::endl;
            return false;
        }

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        return true;
    }

    bool Engine::init_imgui()
    {
        if (!imgui_backend::init(window_, gl_context_))
        {
            std::cerr << "Failed to initialize ImGui backend" << std::endl;
            return false;
        }
        ctx->gui_manager->init();
        return true;
    }

    void Engine::process_events(bool& running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            imgui_backend::process_event(&event);

            // Skip mouse events if ImGui is capturing mouse input.
            if ((event.type == SDL_MOUSEMOTION ||
                event.type == SDL_MOUSEBUTTONDOWN ||
                event.type == SDL_MOUSEBUTTONUP) &&
                imgui_backend::want_capture_mouse())
            {
                continue;
            }

            // Skip keyboard events if ImGui is capturing keyboard input.
            if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) &&
                imgui_backend::want_capture_keyboard())
            {
                continue;
            }

            static_cast<InputManager&>(*ctx->input_manager).HandleEvent(&event);

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F5)
            {
                if (ctx->event_queue)
                    ctx->event_queue->dispatch(TogglePlayModeEvent{});
            }

            if (event.type == SDL_QUIT)
                shutdown_state_ = ShutdownState::Draining;
        }
    }

    bool Engine::advance_shutdown_drain()
    {
        // Drives the non-blocking shutdown phase inside the main loop so we can
        // keep rendering and processing main-thread work while waiting.
        if (!ctx)
            return true;

        if (!shutdown_drain_started_)
        {
            // Kick off unloads once; subsequent frames only pump and poll.
            log_shutdown(ctx.get(), "Shutdown: begin draining in main loop");
            if (ctx->batch_registry)
            {
                log_shutdown(ctx.get(), "Shutdown: request batch unloads");
                shutdown_unload_future_ = ctx->batch_registry->queue_unload_all_async(*ctx);
            }
            shutdown_drain_start_ms_ = SDL_GetTicks();
            shutdown_drain_started_ = true;
        }

        // Pump queued work every frame so background tasks that call
        // push_and_wait() can make forward progress without deadlocking.
        drain_shutdown_queues(*ctx);

        // Check completion without blocking; if either queue is still active,
        // keep draining on subsequent frames.
        const bool unload_done = !shutdown_unload_future_.valid()
            || shutdown_unload_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
        const bool rm_done = !ctx->resource_manager || !ctx->resource_manager->is_busy();

        if (!shutdown_drain_warned_
            && shutdown_drain_start_ms_ > 0
            && SDL_GetTicks() - shutdown_drain_start_ms_ > 5000)
        {
            log_shutdown_warn(ctx.get(), "Shutdown: drain still in progress");
            shutdown_drain_warned_ = true;
        }

        if (!unload_done || !rm_done)
            return false;

        // Mark drained and gate any new work before teardown.
        shutdown_drained_ = true;
        if (ctx->shutdown_requested)
            ctx->shutdown_requested->store(true, std::memory_order_relaxed);
        drain_shutdown_queues(*ctx);
        return true;
    }

    void Engine::set_mode(EngineMode mode)
    {
        if (mode_ == mode)
            return;

        if (mode == EngineMode::Play)
        {
            if (!enter_play_mode())
                return;
        }
        else
        {
            exit_play_mode();
        }
    }

    bool Engine::enter_play_mode()
    {
        if (!ctx || mode_ == EngineMode::Play)
            return false;

        if (!services_)
            services_ = ctx->services_owner;
        if (!edit_world_)
            edit_world_ = ctx->world_owner;
        if (!services_ || !edit_world_)
            return false;

        std::vector<BatchSnapshot> snapshots;
        if (ctx->batch_registry)
        {
            auto& br = static_cast<BatchRegistry&>(*ctx->batch_registry);
            snapshots = br.snapshot_loaded_batches(*ctx);
        }

        std::vector<BatchSnapshot> filtered;
        filtered.reserve(snapshots.size());
        for (auto& snapshot : snapshots)
        {
            if (snapshot.info.name == BatchRegistry::kEditorBatchName)
                continue;
            filtered.push_back(std::move(snapshot));
        }
        snapshots = std::move(filtered);

        play_world_ = std::make_shared<WorldState>(
            std::make_unique<EntityManager>(),
            std::make_unique<BatchRegistry>());

        ctx->bind(*services_, *play_world_);
        ctx->world_owner = play_world_;

        if (ctx->batch_registry)
        {
            auto& br = static_cast<BatchRegistry&>(*ctx->batch_registry);
            TaskResult res = br.load_batches_from_snapshots(snapshots, *ctx, true);
            if (!res.success)
                EENG_LOG_WARN(ctx, "Play mode: snapshot load failed");
        }

        mode_ = EngineMode::Play;
        if (services_)
            services_->play_mode_active.store(true, std::memory_order_relaxed);
        EENG_LOG(ctx, "Play mode: entered");
        return true;
    }

    void Engine::exit_play_mode()
    {
        if (!ctx || mode_ == EngineMode::Edit)
            return;
        if (!services_ || !edit_world_)
            return;

        ctx->bind(*services_, *edit_world_);
        ctx->world_owner = edit_world_;
        play_world_.reset();
        mode_ = EngineMode::Edit;
        if (services_)
            services_->play_mode_active.store(false, std::memory_order_relaxed);
        EENG_LOG(ctx, "Play mode: exited");
    }

    void Engine::begin_frame()
    {
        imgui_backend::begin_frame();

        imgui_backend::show_demo_window();
        ctx->gui_manager->draw(*ctx);

        // Set up OpenGL state:

        // Face culling - takes place before rasterization
        glEnable(GL_CULL_FACE); // Perform face culling
        glFrontFace(GL_CCW);    // Define winding for a front-facing face
        glCullFace(GL_BACK);    // Cull back-facing faces

        // Rasterization stuff
        glEnable(GL_DEPTH_TEST); // Perform depth test when rasterizing
        glDepthFunc(GL_LESS);    // Depth test pass if z < existing z (closer than existing z)
        glDepthMask(GL_TRUE);    // If depth test passes, write z to z-buffer
        glDepthRange(0, 1);      // Z-buffer range is [0,1], where 0 is at z-near and 1 is at z-far

        // Define viewport transform = Clip -> Screen space (applied before rasterization)
        glViewport(0, 0, window_width, window_height);

        // Bind the default framebuffer (only needed when using multiple render targets)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Clear depth and color attachments of frame buffer
        glClearColor(0.529f, 0.808f, 0.922f, 1.0f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Wireframe rendering
        if (wireframe_mode)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_CULL_FACE);
        }
    }

    void Engine::end_frame()
    {
        imgui_backend::end_frame();
    }

    void Engine::on_set_vsync(const SetVsyncEvent& e)
    {
        vsync = e.enabled;
        SDL_GL_SetSwapInterval(vsync);
    }

    void Engine::on_set_wireframe(const SetWireFrameRenderingEvent& e)
    {
        wireframe_mode = e.enabled;
    }

    void Engine::on_set_debug_logging(const SetDebugLoggingEvent& e)
    {
        debug_logging = e.enabled;
    }

    void Engine::on_set_min_frametime(const SetMinFrameTimeEvent& e)
    {
        min_frametime_ms = e.dt;
    }

    void Engine::on_set_play_mode(const SetPlayModeEvent& e)
    {
        set_mode(e.enabled ? EngineMode::Play : EngineMode::Edit);
    }

    void Engine::on_toggle_play_mode(const TogglePlayModeEvent& e)
    {
        (void)e;
        set_mode(mode_ == EngineMode::Play ? EngineMode::Edit : EngineMode::Play);
    }

    void Engine::on_resource_task_completed(const ResourceTaskCompletedEvent& e)
    {
        // Handle resource task completion
        // std::cout << "Resource task completed: " << e.result << std::endl;

        std::string task_name;
        switch (e.result.type)
        {
        case TaskResult::TaskType::Load:
            task_name = "Load";
            break;
        case TaskResult::TaskType::Unload:
            task_name = "Unload";
            break;
        case TaskResult::TaskType::Reload:
            task_name = "Reload";
            break;
        case TaskResult::TaskType::Scan:
            task_name = "Scan";
            break;
        case TaskResult::TaskType::Import:
            task_name = "Import";
            break;
        case TaskResult::TaskType::Unimport:
            task_name = "Unimport";
            break;
        case TaskResult::TaskType::Restore:
            task_name = "Restore";
            break;
        default:
            task_name = "Unknown";
        }

        if (e.result.success)
            EENG_LOG_INFO(ctx, "%s task completed successfully", task_name.c_str());
        else
        {
            EENG_LOG_ERROR(ctx, "%s task failed:", task_name.c_str());
            for (const auto& op : e.result.results)
            {
                if (op.success)
                    EENG_LOG_INFO(ctx, "Operation for GUID %s succeeded: %s", op.guid.to_string().c_str(), op.message.c_str());
                else
                {
                    EENG_LOG_INFO(ctx, "Operation for GUID %s failed: %s", op.guid.to_string().c_str(), op.message.c_str());
                }
            }
        }

        // EENG_LOG_INFO(ctx, "%s task completed with status: %s", task_name.c_str(), e.result.success ? "Success" : "Failure");
        // if (!e.result.success)
        // {
        //     for (const auto& op : e.result.results)
        //     {
        //         if (!op.success)
        //         {
        //             EENG_LOG_ERROR(ctx, "Operation failed for GUID %s: %s", op.guid.to_string().c_str(), op.error_message.c_str());
        //         }
        //     }
        // }
        // else
        // {
        //     EENG_LOG_INFO(ctx, "All operations completed successfully.");
        // }
    }

    void Engine::on_batch_task_completed(const BatchTaskCompletedEvent& e)
    {
        const char* task_name = "Unknown";
        switch (e.type)
        {
        case BatchTaskType::Load:
            task_name = "Load";
            break;
        case BatchTaskType::LoadAll:
            task_name = "LoadAll";
            break;
        case BatchTaskType::Unload:
            task_name = "Unload";
            break;
        case BatchTaskType::UnloadAll:
            task_name = "UnloadAll";
            break;
        case BatchTaskType::Save:
            task_name = "Save";
            break;
        case BatchTaskType::SaveAll:
            task_name = "SaveAll";
            break;
        case BatchTaskType::RebuildClosure:
            task_name = "RebuildClosure";
            break;
        case BatchTaskType::CreateEntity:
            task_name = "CreateEntity";
            break;
        case BatchTaskType::DestroyEntity:
            task_name = "DestroyEntity";
            break;
        case BatchTaskType::AttachEntity:
            task_name = "AttachEntity";
            break;
        case BatchTaskType::DetachEntity:
            task_name = "DetachEntity";
            break;
        case BatchTaskType::SpawnEntity:
            task_name = "SpawnEntity";
            break;
        case BatchTaskType::Unknown:
        default:
            task_name = "Unknown";
            break;
        }

        std::string batch_label = e.batch_id.valid()
            ? e.batch_id.to_string()
            : std::string("<n/a>");
        if (!e.batch_name.empty() && e.batch_id.valid())
        {
            batch_label = e.batch_name + " (" + batch_label + ")";
        }

        if (e.type == BatchTaskType::LoadAll ||
            e.type == BatchTaskType::UnloadAll ||
            e.type == BatchTaskType::SaveAll)
        {
            if (e.success)
            {
                EENG_LOG_INFO(ctx,
                    "[batch] %s ok: batches=%zu",
                    task_name,
                    e.batch_count);
            }
            else
            {
                EENG_LOG_WARN(ctx,
                    "[batch] %s fail: batches=%zu",
                    task_name,
                    e.batch_count);
            }
            return;
        }

        if (e.type == BatchTaskType::RebuildClosure && e.has_closure_delta)
        {
            if (e.success)
            {
                EENG_LOG_INFO(ctx,
                    "[batch] %s ok: %s roots=%zu old=%zu new=%zu +%zu -%zu assets_rebind=%s",
                    task_name,
                    batch_label.c_str(),
                    e.closure_roots,
                    e.closure_old,
                    e.closure_new,
                    e.closure_added,
                    e.closure_removed,
                    e.assets_rebound ? "ok" : "fail");
            }
            else
            {
                EENG_LOG_WARN(ctx,
                    "[batch] %s fail: %s roots=%zu old=%zu new=%zu +%zu -%zu assets_rebind=%s",
                    task_name,
                    batch_label.c_str(),
                    e.closure_roots,
                    e.closure_old,
                    e.closure_new,
                    e.closure_added,
                    e.closure_removed,
                    e.assets_rebound ? "ok" : "fail");
            }
            return;
        }

        if (e.success)
        {
            EENG_LOG_INFO(ctx,
                "[batch] %s ok: %s live=%zu closure=%zu",
                task_name,
                batch_label.c_str(),
                e.live_entities,
                e.asset_closure_size);
        }
        else
        {
            EENG_LOG_WARN(ctx,
                "[batch] %s fail: %s live=%zu closure=%zu",
                task_name,
                batch_label.c_str(),
                e.live_entities,
                e.asset_closure_size);
        }
    }

    } // namespace eeng
