// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "config.h"
#include "engineapi/IApp.hpp"
#include "app/GameApp.hpp"
#include "app/EditorApp.hpp"
#include "EngineContext.hpp"
#include "EngineOverlayGui.hpp"
#include <cstdint>
#include <future>
#include <iostream>
#include <atomic>
#include <memory>

struct SDL_Window;              // Forward declaration
typedef void* SDL_GLContext;    // Forward declaration

namespace eeng
{
    enum class EngineMode : std::uint8_t
    {
        Edit,
        Play
    };

    /**
     * @brief Main engine class handling SDL, OpenGL, ImGui initialization and the main loop.
     */
    class Engine
    {
        enum class ShutdownState : std::uint8_t
        {
            Running,
            Draining,
            Teardown
        };

    public:
        /** Constructor */
        explicit Engine(std::shared_ptr<EngineContext> ctx);

        /** Destructor, calls shutdown() */
        ~Engine();

        /**
         * @brief Initialize the engine.
         * @param title Window title
         * @param width Window width
         * @param height Window height
         * @return True if successful, false otherwise
         */
        bool init(const char* title, int width, int height);

        /// @brief Create an app of a given type and run main loop
        /// @tparam TApp App type
        /// @tparam ...Args 
        /// @param ...args 
        template<typename TApp, typename... Args>
            requires (std::is_base_of<IApp, TApp>::value)
        void run(Args&&... args)
        {
            if constexpr (requires { TApp(ctx); })
                app_ = std::make_unique<TApp>(ctx, std::forward<Args>(args)...);
            else
                app_ = std::make_unique<TApp>(std::forward<Args>(args)...);
            run();
        }

        /// @brief Convenience: run an editor-hosted game runtime.
        template<typename TRuntime, typename... Args>
            requires (std::is_base_of<IGameRuntime, TRuntime>::value)
        void run_editor(Args&&... args)
        {
            run<editor::EditorApp<TRuntime>>(std::forward<Args>(args)...);
        }

        /// @brief Convenience: run a game runtime without editor tooling.
        template<typename TRuntime>
            requires (std::is_base_of<IGameRuntime, TRuntime>::value)
        void run_game()
        {
            run<GameApp<TRuntime>>();
        }

        /** @brief Clean up and close the engine. */
        void shutdown();

        /**
         * @brief Get SDL window pointer.
         * @return SDL_Window pointer
         */
        SDL_Window* window() const { return window_; }

    private:
        SDL_Window* window_ = nullptr;        ///< SDL Window pointer
        SDL_GLContext gl_context_ = nullptr;  ///< OpenGL context

        int window_height;    ///< Window height in pixels
        int window_width;     ///< Window width in pixels
        bool vsync = false;   ///< V-sync enabled state
        bool wireframe_mode = false; ///< Wireframe rendering state
        bool debug_logging = false; ///< Debug logging enabled state
        float min_frametime_ms = 0.0f; ///< Minimum frame duration in milliseconds (default 60 FPS)
        std::atomic<bool> shutdown_started_{ false };
        EngineMode mode_{ EngineMode::Edit };

        std::shared_ptr<EngineContext> ctx;
        std::unique_ptr<IApp> app_;
        std::shared_ptr<EngineServices> services_;
        std::shared_ptr<WorldState> edit_world_;
        std::shared_ptr<WorldState> play_world_;
        EngineOverlayGui engine_overlay_{};
        
        ShutdownState shutdown_state_{ ShutdownState::Running };
        bool shutdown_drain_started_ = false;
        bool shutdown_drained_ = false;
        bool shutdown_drain_warned_ = false;
        std::uint32_t shutdown_drain_start_ms_ = 0;
        std::shared_future<TaskResult> shutdown_unload_future_;

        /**
         * @brief Start the main loop using the currently assigned app.
         */
        void run();

        /** Initialize SDL library and window. */
        bool init_sdl(const char* title, int width, int height);

        /** Initialize OpenGL context. */
        bool init_opengl();

        /** Initialize ImGui for GUI rendering. */
        bool init_imgui();

        /** Handle SDL events. */
        void process_events(bool& running);

        /** Prepare frame rendering. */
        void begin_frame();

        /** Finish frame rendering. */
        void end_frame();

        // Advances the shutdown draining phase while the main loop is still running.
        // Returns true when all async unloads are finished and it is safe to tear down.
        bool advance_shutdown_drain();

        void set_mode(EngineMode mode);
        bool enter_play_mode();
        void exit_play_mode();

        void on_set_vsync(const SetVsyncEvent& e);
        void on_set_wireframe(const SetWireFrameRenderingEvent& e);
        void on_set_debug_logging(const SetDebugLoggingEvent& e);
        void on_set_min_frametime(const SetMinFrameTimeEvent& e);
        void on_set_play_mode(const SetPlayModeEvent& e);
        void on_toggle_play_mode(const TogglePlayModeEvent& e);
        void on_resource_task_completed(const ResourceTaskCompletedEvent& e);
        void on_batch_task_completed(const BatchTaskCompletedEvent& e);

    };

    using EnginePtr = std::unique_ptr<Engine>;
#if 0
    struct EngineFactory
    {
        static EnginePtr CreateDefaultEngine()
        {
            return std::make_unique<Engine>(
                std::make_unique<EntityManager>(),
                std::make_unique<ResourceManager>()
                // std::make_unique<SceneManager>(),
                // std::make_unique<EventDispatcher>(),
                // std::make_unique<Logger>());
        }
    }
#endif

} // namespace eeng

#endif // ENGINE_HPP
