// FluidSandboxGame is a project-local runtime for fluid experiments.
// It keeps the same editor/runtime shell as the reference sample while allowing
// fluid-specific systems and metadata to evolve independently.

#include "FluidSandboxGame.hpp"
#include "BatchRegistry.hpp"
#include "FluidSandboxMetaReg.hpp"
#include "ecs/TransformComponent.hpp"
#include "editor/ecs/FirstPersonCameraComponent.hpp"
#include "editor/ecs/ThirdPersonCameraComponent.hpp"
#include "editor/OverlayRenderSettingsPersistence.hpp"
#include "glmcommon.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace eeng::fluid_sandbox
{
    namespace
    {
        struct ActiveEditorCameraView
        {
            glm::mat4 view{ 1.0f };
            float near_plane = 1.0f;
            float far_plane = 500.0f;
        };

        void ensure_demo_fluid_frame(EngineContext& ctx)
        {
            if (!ctx.entity_manager)
                return;

            auto& registry = ctx.entity_manager->registry();
            auto fluid_view = registry.view<eeng::fluid_sandbox::ecs::FluidFrameComponent>();
            if (fluid_view.begin() != fluid_view.end())
                return;

            const auto [guid, entity] = ctx.entity_manager->create_entity_live_parent("default_chunk", "Demo Fluid Frame");
            (void)guid;

            auto& transform = registry.emplace<eeng::ecs::TransformComponent>(entity);
            transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
            transform.mark_local_dirty();

            auto& fluid = registry.emplace<eeng::fluid_sandbox::ecs::FluidFrameComponent>(entity);
            fluid.name = "Demo Fluid Frame";
            fluid.frame_size = glm::vec2(6.0f, 3.0f);
            fluid.resolution = glm::ivec2(96, 48);
            fluid.substeps = 2;
            fluid.density_gain = 3.0f;
            fluid.debug_draw_velocity = false;
            fluid.velocity_glyph_scale = 0.05f;
        }

        bool try_get_active_editor_camera_view(
            entt::registry& registry,
            ActiveEditorCameraView& out_view)
        {
            auto third_view = registry.view<eeng::editor::ThirdPersonCameraComponent>();
            for (auto entity : third_view)
            {
                const auto& camera = third_view.get<eeng::editor::ThirdPersonCameraComponent>(entity);
                if (!camera.active)
                    continue;

                out_view.view = camera.model_to_view;
                out_view.near_plane = camera.near_plane;
                out_view.far_plane = camera.far_plane;
                return true;
            }

            auto first_view = registry.view<eeng::editor::FirstPersonCameraComponent>();
            for (auto entity : first_view)
            {
                const auto& camera = first_view.get<eeng::editor::FirstPersonCameraComponent>(entity);
                if (!camera.active)
                    continue;

                out_view.view = camera.model_to_view;
                out_view.near_plane = camera.near_plane;
                out_view.far_plane = camera.far_plane;
                return true;
            }

            if (third_view.begin() != third_view.end())
            {
                const auto& camera = third_view.get<eeng::editor::ThirdPersonCameraComponent>(*third_view.begin());
                out_view.view = camera.model_to_view;
                out_view.near_plane = camera.near_plane;
                out_view.far_plane = camera.far_plane;
                return true;
            }

            if (first_view.begin() != first_view.end())
            {
                const auto& camera = first_view.get<eeng::editor::FirstPersonCameraComponent>(*first_view.begin());
                out_view.view = camera.model_to_view;
                out_view.near_plane = camera.near_plane;
                out_view.far_plane = camera.far_plane;
                return true;
            }

            return false;
        }
    }

    FluidSandboxGame::FluidSandboxGame(std::shared_ptr<EngineContext> ctx)
        : ctx_(std::move(ctx))
    {
    }

    bool FluidSandboxGame::init()
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

    void FluidSandboxGame::update(float time_s, float deltaTime_s)
    {
        (void)time_s;
        update_edit(time_s, deltaTime_s);
    }

    void FluidSandboxGame::update_edit(float time_s, float deltaTime_s)
    {
        (void)time_s;
        if (ctx_)
        {
            ensure_demo_fluid_frame(*ctx_);
            runtime_pipeline_.update_edit(*ctx_, deltaTime_s);
            if (ctx_->entity_manager)
                fluid_frame_system_.update(ctx_->entity_manager->registry(), *ctx_, deltaTime_s);
        }
    }

    void FluidSandboxGame::update_play(float time_s, float deltaTime_s)
    {
        (void)time_s;
        if (ctx_)
        {
            ensure_demo_fluid_frame(*ctx_);
            runtime_pipeline_.update_play(*ctx_, deltaTime_s);
            if (ctx_->entity_manager)
                fluid_frame_system_.update(ctx_->entity_manager->registry(), *ctx_, deltaTime_s);
        }
    }

    void FluidSandboxGame::render(float time_s, int windowWidth, int windowHeight)
    {
        (void)time_s;
        last_window_size_ = glm::ivec2(windowWidth, windowHeight);
        // Legacy render entry point: keep the overlay view in sync even if a
        // caller bypasses render_frame/render_overlay.
        publish_overlay_view(windowWidth, windowHeight);
    }

    void FluidSandboxGame::render_scene(const RenderContext& ctx)
    {
        // Scene rendering would typically call RuntimePipeline::render_entities()
        // plus any game-specific renderers. FluidSandboxGame renders nothing yet.
        (void)ctx;
    }

    void FluidSandboxGame::render_overlay(const RenderContext& ctx)
    {
        // Provide a view for editor overlays (gizmos, debug shapes).
        last_window_size_ = glm::ivec2(ctx.window_width, ctx.window_height);
        publish_overlay_view(ctx.window_width, ctx.window_height);
        if (ctx_ && ctx_->entity_manager && ctx_->shape_renderer)
            fluid_frame_system_.render_overlay(ctx_->entity_manager->registry(), *ctx_, *ctx_->shape_renderer);
    }

    void FluidSandboxGame::render_gui(const RenderContext& ctx)
    {
        // Games can submit ImGui windows or HUD here.
        (void)ctx;
    }

    bool FluidSandboxGame::get_editor_view(OverlayViewState& out) const
    {
        return build_editor_view(out, last_window_size_);
    }

    bool FluidSandboxGame::build_editor_view(OverlayViewState& out, glm::ivec2 window_size) const
    {
        if (!ctx_ || !ctx_->entity_manager)
            return false;

        if (window_size.x <= 0 || window_size.y <= 0)
            return false;

        ActiveEditorCameraView camera_view{};
        if (!try_get_active_editor_camera_view(ctx_->entity_manager->registry(), camera_view))
            return false;

        const float aspect_ratio = static_cast<float>(window_size.x) / static_cast<float>(window_size.y);
        out.view = camera_view.view;
        out.proj = glm::perspective(
            glm::radians(60.0f),
            aspect_ratio,
            camera_view.near_plane,
            camera_view.far_plane);
        out.viewport = glm_aux::create_viewport_matrix(
            0.0f,
            0.0f,
            static_cast<float>(window_size.x),
            static_cast<float>(window_size.y),
            0.0f,
            1.0f);
        out.window_size = window_size;
        out.valid = true;
        return true;
    }

    void FluidSandboxGame::publish_overlay_view(int windowWidth, int windowHeight)
    {
        if (!ctx_ || !ctx_->overlay_view_state)
            return;

        auto& overlay = *ctx_->overlay_view_state;
        if (!build_editor_view(overlay, glm::ivec2(windowWidth, windowHeight)))
            overlay.valid = false;
    }

    void FluidSandboxGame::destroy()
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

    PlayModePolicy FluidSandboxGame::preferred_play_policy() const
    {
        // Prefer Preview to reuse the current edit-world state, or Strict to request
        // explicit batch loading each time play starts (the app can override).
        return PlayModePolicy::Strict;
    }

    std::vector<std::string> FluidSandboxGame::preferred_startup_batches() const
    {
        // In Strict mode, list preferred batch names to load at play start.
        // These are resolved through the play world's BatchRegistry by the app.
        return { std::string(BatchRegistry::kDefaultBatchName) };
    }

    void FluidSandboxGame::on_play_world_created(EngineContext& ctx)
    {
        // Configure the fresh play world before loading batches (e.g., set the batch index path).
        if (!ctx.batch_registry)
            return;

        auto& br = static_cast<BatchRegistry&>(*ctx.batch_registry);
        br.load_or_create_index("projects/fluid_sandbox/batches/index.json");
    }

    void FluidSandboxGame::on_enter_play(EngineContext& ctx)
    {
        // Runtime-only setup goes here (spawn player, reset timers, etc).
        (void)ctx;
    }

    void FluidSandboxGame::on_exit_play(EngineContext& ctx)
    {
        // Cleanup runtime-only state before returning to edit mode.
        (void)ctx;
    }
}
