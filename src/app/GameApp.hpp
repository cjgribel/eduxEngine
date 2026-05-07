// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "engineapi/IApp.hpp"
#include "engineapi/IGameRuntime.hpp"
#include "engineapi/RenderContext.hpp"
#include <memory>
#include <type_traits>

namespace eeng
{
    template<typename TRuntime>
        requires (std::is_base_of<IGameRuntime, TRuntime>::value)
    /**
     * @brief App that hosts only a game runtime (no editor tooling).
     *
     * GameApp forwards all play/edit hooks to the runtime and uses the runtime's
     * preferred play policy and startup batches as the effective app policy.
     */
    class GameApp final : public IApp
    {
    public:
        explicit GameApp(std::shared_ptr<EngineContext> ctx)
            : ctx_(std::move(ctx))
        {
            if constexpr (requires { TRuntime(ctx_); })
                runtime_ = std::make_unique<TRuntime>(ctx_);
            else
                runtime_ = std::make_unique<TRuntime>();
        }

        bool init() override
        {
            return runtime_ ? runtime_->init() : false;
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
        }

        void destroy() override
        {
            if (runtime_)
                runtime_->destroy();
        }

        void update_edit(float time_s, float deltaTime_s) override
        {
            if (runtime_)
                runtime_->update_edit(time_s, deltaTime_s);
        }

        void update_play(float time_s, float deltaTime_s) override
        {
            if (runtime_)
                runtime_->update_play(time_s, deltaTime_s);
        }

        void render_edit(float time_s, int windowWidth, int windowHeight) override
        {
            if (!runtime_)
                return;
            RenderContext render_ctx = build_render_context(
                time_s,
                windowWidth,
                windowHeight,
                RenderMode::Edit);
            runtime_->render_frame(render_ctx);
        }

        void render_play(float time_s, int windowWidth, int windowHeight) override
        {
            if (!runtime_)
                return;
            RenderContext render_ctx = build_render_context(
                time_s,
                windowWidth,
                windowHeight,
                RenderMode::Play);
            runtime_->render_frame(render_ctx);
        }

        PlayModePolicy play_policy() const override
        {
            return runtime_ ? runtime_->preferred_play_policy() : PlayModePolicy::Preview;
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
        RenderContext build_render_context(
            float time_s,
            int windowWidth,
            int windowHeight,
            RenderMode mode) const
        {
            RenderContext render_ctx{
                time_s,
                windowWidth,
                windowHeight,
                mode
            };

            CameraView camera_view{};
            if (runtime_ && runtime_->build_play_camera_view(camera_view, glm::ivec2(windowWidth, windowHeight)))
            {
                render_ctx.camera_view = camera_view;
                // Keep legacy overlay consumers alive while scene rendering
                // moves over to the explicit CameraView contract.
                if (ctx_ && ctx_->overlay_view_state)
                    copy_camera_view_to_overlay_view(camera_view, *ctx_->overlay_view_state);
            }
            else if (ctx_ && ctx_->overlay_view_state)
            {
                ctx_->overlay_view_state->valid = false;
            }

            return render_ctx;
        }

        std::shared_ptr<EngineContext> ctx_;
        std::unique_ptr<TRuntime> runtime_;
    };
} // namespace eeng
