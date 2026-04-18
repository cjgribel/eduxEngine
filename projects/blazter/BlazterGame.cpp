#include "BlazterGame.hpp"

#include "BatchRegistry.hpp"
#include "editor/OverlayRenderSettingsPersistence.hpp"
#include <glm/glm.hpp>

namespace eeng::blazter
{
    BlazterGame::BlazterGame(std::shared_ptr<EngineContext> ctx)
        : ctx_(std::move(ctx))
    {
    }

    bool BlazterGame::init()
    {
        if (ctx_)
        {
            runtime_pipeline_.init(*ctx_);
            if (ctx_->services)
            {
                ctx_->services->debug_render_settings = runtime_pipeline_.debug_render_settings_edit();
                ctx_->services->debug_render_settings_edit = runtime_pipeline_.debug_render_settings_edit();
                ctx_->services->debug_render_settings_play = runtime_pipeline_.debug_render_settings_play();
                ctx_->services->overlay_render_settings = runtime_pipeline_.overlay_render_settings();

                eeng::editor::load_overlay_render_settings(
                    *ctx_,
                    *runtime_pipeline_.overlay_render_settings(),
                    *runtime_pipeline_.debug_render_settings_edit(),
                    *runtime_pipeline_.debug_render_settings_play());
            }
        }
        return true;
    }

    void BlazterGame::update(float time_s, float deltaTime_s)
    {
        (void)time_s;
        update_edit(time_s, deltaTime_s);
    }

    void BlazterGame::update_edit(float time_s, float deltaTime_s)
    {
        (void)time_s;
        if (ctx_)
            runtime_pipeline_.update_edit(*ctx_, deltaTime_s);
    }

    void BlazterGame::update_play(float time_s, float deltaTime_s)
    {
        (void)time_s;
        if (ctx_)
            runtime_pipeline_.update_play(*ctx_, deltaTime_s);
    }

    void BlazterGame::render(float time_s, int windowWidth, int windowHeight)
    {
        (void)time_s;
        publish_overlay_view(windowWidth, windowHeight);
    }

    void BlazterGame::render_scene(const RenderContext& ctx)
    {
        // Intentionally empty for now: the scaffold focuses on play-mode boot
        // and leaves gameplay/rendering code to future Blazter iterations.
        (void)ctx;
    }

    void BlazterGame::render_overlay(const RenderContext& ctx)
    {
        publish_overlay_view(ctx.window_width, ctx.window_height);
    }

    void BlazterGame::render_gui(const RenderContext& ctx)
    {
        (void)ctx;
    }

    void BlazterGame::publish_overlay_view(int windowWidth, int windowHeight)
    {
        if (!ctx_ || !ctx_->overlay_view_state)
            return;
        if (windowWidth <= 0 || windowHeight <= 0)
            return;

        auto& overlay = *ctx_->overlay_view_state;
        overlay.view = glm::mat4(1.0f);
        overlay.proj = glm::mat4(1.0f);
        overlay.viewport = glm::mat4(1.0f);
        overlay.window_size = glm::ivec2(windowWidth, windowHeight);
        overlay.valid = true;
    }

    void BlazterGame::destroy()
    {
        if (ctx_ && ctx_->services
            && ctx_->services->debug_render_settings_edit == runtime_pipeline_.debug_render_settings_edit()
            && ctx_->services->debug_render_settings_play == runtime_pipeline_.debug_render_settings_play()
            && ctx_->services->overlay_render_settings == runtime_pipeline_.overlay_render_settings())
        {
            ctx_->services->debug_render_settings = nullptr;
            ctx_->services->debug_render_settings_edit = nullptr;
            ctx_->services->debug_render_settings_play = nullptr;
            ctx_->services->overlay_render_settings = nullptr;
        }
        runtime_pipeline_.shutdown();
    }

    PlayModePolicy BlazterGame::preferred_play_policy() const
    {
        // Default to Warm Play so the new project exercises runtime-owned boot
        // through startup batch config rather than Preview snapshots.
        return PlayModePolicy::Strict;
    }

    std::vector<std::string> BlazterGame::preferred_startup_batches() const
    {
        // Fallback for non-editor hosts that do not supply project config yet.
        return { std::string("blazter_boot") };
    }

    void BlazterGame::on_play_world_created(EngineContext& ctx)
    {
        (void)ctx;
    }

    void BlazterGame::on_enter_play(EngineContext& ctx)
    {
        // Future Blazter boot code can use this to spawn the player, arm the
        // blaster, and initialize session state after startup batches load.
        (void)ctx;
    }

    void BlazterGame::on_exit_play(EngineContext& ctx)
    {
        (void)ctx;
    }
} // namespace eeng::blazter
