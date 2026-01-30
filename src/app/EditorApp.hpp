// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "engineapi/IApp.hpp"
#include "editor/EditorRuntime.hpp"
#include "editor/ProjectBootstrap.hpp"
#include "editor/ProjectConfig.hpp"
#include "EngineContext.hpp"
#include "engineapi/IGameRuntime.hpp"
#include "LogMacros.h"
#include <glm/glm.hpp>
#include <atomic>
#include <filesystem>
#include <memory>
#include <type_traits>

namespace eeng::editor
{
    template<typename TRuntime>
        requires (std::is_base_of<IGameRuntime, TRuntime>::value)
    /**
     * @brief App that hosts editor tooling plus a game runtime.
     *
     * EditorApp owns an EditorRuntime and a game runtime (IGameRuntime). The Engine
     * runs EditorApp directly; EditorApp forwards play/edit hooks to the runtime and
     * applies any editor-side play policy overrides.
     */
    class EditorApp final : public IApp
    {
    public:
        explicit EditorApp(
            std::shared_ptr<EngineContext> ctx,
            std::filesystem::path project_config_path = {})
            : ctx_(std::move(ctx))
            , project_config_path_(std::move(project_config_path))
        {
            if constexpr (requires { TRuntime(ctx_); })
                runtime_ = std::make_unique<TRuntime>(ctx_);
            else
                runtime_ = std::make_unique<TRuntime>();
        }

        bool init() override
        {
            if (!runtime_)
                return false;
            editor_.init(*ctx_);
            if (ctx_ && ctx_->services)
            {
                // Provide a hook so game renderers can draw editor overlays.
                ctx_->services->editor_render_hook =
                    [this](EngineContext& ctx, ShapeRendering::ShapeRenderer& renderer, const OverlayViewState& view)
                    {
                        editor_.render(ctx, renderer, view.view, view.proj, view.viewport, view.window_size);
                    };
            }
            if (!project_config_path_.empty())
            {
                auto config_path = project_config_path_;
                if (config_path.is_relative())
                    config_path = std::filesystem::current_path() / config_path;

                if (auto config = ProjectConfig::load_from_file(config_path))
                {
                    // Stash config for game/runtime access (assets root, batches root, etc).
                    if (ctx_ && ctx_->services)
                    {
                        ctx_->services->project_config = std::make_shared<ProjectConfig>(*config);
                        ctx_->project_config = ctx_->services->project_config;
                    }
                    bootstrap_project(*ctx_, *config);
                }
                else
                {
                    EENG_LOG_WARN(ctx_, "Project config not found: %s",
                        config_path.string().c_str());
                }
            }
            return runtime_->init();
        }

        void update(float time_s, float deltaTime_s) override
        {
            if (runtime_)
                runtime_->update(time_s, deltaTime_s);
        }

        void render(float time_s, int windowWidth, int windowHeight) override
        {
            if (runtime_)
                runtime_->render(time_s, windowWidth, windowHeight);
            last_window_size_ = glm::ivec2(windowWidth, windowHeight);
        }

        void destroy() override
        {
            if (runtime_)
                runtime_->destroy();
            if (ctx_ && ctx_->services)
            {
                // Clear the overlay hook to avoid dangling callbacks.
                ctx_->services->editor_render_hook = {};
            }
        }

        void update_edit(float time_s, float deltaTime_s) override
        {
            // Update editor cameras first so the game can consume fresh view state.
            editor_.update_cameras(*ctx_, deltaTime_s);
            if (runtime_)
                runtime_->update_edit(time_s, deltaTime_s);

            OverlayViewState view{};
            if (runtime_ && runtime_->get_editor_view(view)
                && view.window_size.x > 0 && view.window_size.y > 0)
            {
                editor_.update(*ctx_, view.view, view.proj, view.viewport, view.window_size);
            }
            else if (last_window_size_.x > 0 && last_window_size_.y > 0)
            {
                const glm::mat4 identity(1.0f);
                editor_.update(*ctx_, identity, identity, identity, last_window_size_);
            }
        }

        void update_play(float time_s, float deltaTime_s) override
        {
            if (runtime_)
                runtime_->update_play(time_s, deltaTime_s);
        }

        void render_edit(float time_s, int windowWidth, int windowHeight) override
        {
            if (runtime_)
                runtime_->render_edit(time_s, windowWidth, windowHeight);
            last_window_size_ = glm::ivec2(windowWidth, windowHeight);
            // TODO: wire editor gizmo rendering once a shared ShapeRenderer is available here.
        }

        void render_play(float time_s, int windowWidth, int windowHeight) override
        {
            if (runtime_)
                runtime_->render_play(time_s, windowWidth, windowHeight);
            last_window_size_ = glm::ivec2(windowWidth, windowHeight);
        }

        PlayModePolicy play_policy() const override
        {
            PlayModePolicy policy = runtime_
                ? runtime_->preferred_play_policy()
                : PlayModePolicy::Preview;
            if (ctx_ && ctx_->services
                && ctx_->services->play_mode_policy_override_enabled.load(std::memory_order_relaxed))
            {
                policy = ctx_->services->play_mode_policy_override.load(std::memory_order_relaxed);
            }
            return policy;
        }

        std::vector<std::string> play_startup_batches() const override
        {
            return runtime_ ? runtime_->preferred_startup_batches() : std::vector<std::string>{};
        }

        void on_play_world_created(EngineContext& ctx) override
        {
            if (runtime_)
                runtime_->on_play_world_created(ctx);
        }

        void on_enter_play(EngineContext& ctx) override
        {
            if (runtime_)
                runtime_->on_enter_play(ctx);
        }

        void on_exit_play(EngineContext& ctx) override
        {
            if (runtime_)
                runtime_->on_exit_play(ctx);
        }

    private:
        std::shared_ptr<EngineContext> ctx_;
        std::unique_ptr<TRuntime> runtime_;
        std::filesystem::path project_config_path_;
        EditorRuntime editor_{};
        glm::ivec2 last_window_size_{ 0, 0 };
    };
} // namespace eeng::editor
