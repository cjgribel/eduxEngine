// ReferenceGame is a minimal stub kept in sync with the evolving engine API.
// It demonstrates Strict play-mode hooks and render-phase structure without
// depending on any gameplay code.

#include "ReferenceGame.hpp"
#include "BatchRegistry.hpp"
#include "FluidSandboxMetaReg.hpp"
#include "editor/OverlayRenderSettingsPersistence.hpp"
#include <glm/glm.hpp>

namespace eeng::reference_game
{
    ReferenceGame::ReferenceGame(std::shared_ptr<EngineContext> ctx)
        : ctx_(std::move(ctx))
    {
    }

    bool ReferenceGame::init()
    {
        if (ctx_)
        {
            register_fluid_sandbox_meta_types(*ctx_);
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

    void ReferenceGame::update(float time_s, float deltaTime_s)
    {
        (void)time_s;
        update_edit(time_s, deltaTime_s);
    }

    void ReferenceGame::update_edit(float time_s, float deltaTime_s)
    {
        (void)time_s;
        if (ctx_)
        {
            runtime_pipeline_.update_edit(*ctx_, deltaTime_s);
            if (ctx_->entity_manager)
                fluid_frame_system_.update(ctx_->entity_manager->registry(), *ctx_, deltaTime_s);
        }
    }

    void ReferenceGame::update_play(float time_s, float deltaTime_s)
    {
        (void)time_s;
        if (ctx_)
        {
            runtime_pipeline_.update_play(*ctx_, deltaTime_s);
            if (ctx_->entity_manager)
                fluid_frame_system_.update(ctx_->entity_manager->registry(), *ctx_, deltaTime_s);
        }
    }

    void ReferenceGame::render(float time_s, int windowWidth, int windowHeight)
    {
        (void)time_s;
        // Legacy render entry point: keep the overlay view in sync even if a
        // caller bypasses render_frame/render_overlay.
        publish_overlay_view(windowWidth, windowHeight);
    }

    void ReferenceGame::render_scene(const RenderContext& ctx)
    {
        // Scene rendering would typically call RuntimePipeline::render_entities()
        // plus any game-specific renderers. ReferenceGame renders nothing.
        (void)ctx;
    }

    void ReferenceGame::render_overlay(const RenderContext& ctx)
    {
        // Provide a view for editor overlays (gizmos, debug shapes).
        publish_overlay_view(ctx.window_width, ctx.window_height);
        if (ctx_ && ctx_->entity_manager && ctx_->shape_renderer)
            fluid_frame_system_.render_overlay(ctx_->entity_manager->registry(), *ctx_, *ctx_->shape_renderer);
    }

    void ReferenceGame::render_gui(const RenderContext& ctx)
    {
        // Games can submit ImGui windows or HUD here.
        (void)ctx;
    }

    void ReferenceGame::publish_overlay_view(int windowWidth, int windowHeight)
    {
        if (!ctx_ || !ctx_->overlay_view_state)
            return;

        if (windowWidth <= 0 || windowHeight <= 0)
            return;

        // Publish a placeholder overlay view so editor gizmos can draw, even though
        // ReferenceGame has no camera of its own yet.
        auto& overlay = *ctx_->overlay_view_state;
        overlay.view = glm::mat4(1.0f);
        overlay.proj = glm::mat4(1.0f);
        overlay.viewport = glm::mat4(1.0f);
        overlay.window_size = glm::ivec2(windowWidth, windowHeight);
        overlay.valid = true;
    }

    void ReferenceGame::destroy()
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
        fluid_frame_system_.clear();
        runtime_pipeline_.shutdown();
    }

    PlayModePolicy ReferenceGame::preferred_play_policy() const
    {
        // Prefer Preview to reuse the current edit-world state, or Strict to request
        // explicit batch loading each time play starts (the app can override).
        return PlayModePolicy::Strict;
    }

    std::vector<std::string> ReferenceGame::preferred_startup_batches() const
    {
        // In Strict mode, list preferred batch names to load at play start.
        // These are resolved through the play world's BatchRegistry by the app.
        return { std::string(BatchRegistry::kDefaultBatchName) };
    }

    void ReferenceGame::on_play_world_created(EngineContext& ctx)
    {
        // Configure the fresh play world before loading batches (e.g., set the batch index path).
        if (!ctx.batch_registry)
            return;

        auto& br = static_cast<BatchRegistry&>(*ctx.batch_registry);
        br.load_or_create_index("projects/reference_game/batches/index.json");
    }

    void ReferenceGame::on_enter_play(EngineContext& ctx)
    {
        // Runtime-only setup goes here (spawn player, reset timers, etc).
        (void)ctx;
    }

    void ReferenceGame::on_exit_play(EngineContext& ctx)
    {
        // Cleanup runtime-only state before returning to edit mode.
        (void)ctx;
    }
}
