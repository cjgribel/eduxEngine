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
#include "imgui.h"

#include <cmath>
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

        entt::entity find_gui_target_fluid_frame(entt::registry& registry)
        {
            auto fluid_view = registry.view<eeng::fluid_sandbox::ecs::FluidFrameComponent>();
            if (fluid_view.begin() != fluid_view.end())
                return *fluid_view.begin();

            return entt::null;
        }

        const char* render_mode_label(eeng::fluid_sandbox::ecs::FluidFrameRenderMode mode)
        {
            switch (mode)
            {
            case eeng::fluid_sandbox::ecs::FluidFrameRenderMode::Density:
                return "Density";
            case eeng::fluid_sandbox::ecs::FluidFrameRenderMode::VelocityGlyphs:
                return "Velocity Glyphs";
            case eeng::fluid_sandbox::ecs::FluidFrameRenderMode::Pressure:
                return "Pressure";
            case eeng::fluid_sandbox::ecs::FluidFrameRenderMode::Divergence:
                return "Divergence";
            case eeng::fluid_sandbox::ecs::FluidFrameRenderMode::Vorticity:
                return "Vorticity";
            default:
                return "Unknown";
            }
        }

        const char* obstacle_shape_label(eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleShape shape)
        {
            switch (shape)
            {
            case eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleShape::Box:
                return "Box";
            case eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleShape::Circle:
                return "Circle";
            default:
                return "Unknown";
            }
        }

        const char* obstacle_boundary_label(eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleBoundaryMode mode)
        {
            switch (mode)
            {
            case eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleBoundaryMode::NoSlip:
                return "No Slip";
            case eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleBoundaryMode::FreeSlip:
                return "Free Slip";
            default:
                return "Unknown";
            }
        }

        void reset_fluid_component_to_defaults(eeng::fluid_sandbox::ecs::FluidFrameComponent& fluid)
        {
            const std::string name = fluid.name;
            const bool enabled = fluid.enabled;
            const glm::vec2 frame_size = fluid.frame_size;
            const glm::ivec2 resolution = fluid.resolution;
            const float simulation_rate = fluid.simulation_rate;
            const float max_step_dt = fluid.max_step_dt;
            const int substeps = fluid.substeps;
            const std::string config_path = fluid.config_path;

            fluid = eeng::fluid_sandbox::ecs::FluidFrameComponent{};
            fluid.name = name;
            fluid.enabled = enabled;
            fluid.frame_size = frame_size;
            fluid.resolution = resolution;
            fluid.simulation_rate = simulation_rate;
            fluid.max_step_dt = max_step_dt;
            fluid.substeps = substeps;
            fluid.config_path = config_path;
        }

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
        (void)ctx;
        if (!ctx_ || !ctx_->entity_manager)
            return;

        auto& registry = ctx_->entity_manager->registry();
        auto fluid_view = registry.view<eeng::fluid_sandbox::ecs::FluidFrameComponent>();
        int fluid_count = 0;
        for (auto entity : fluid_view)
        {
            (void)entity;
            ++fluid_count;
        }

        if (!ImGui::Begin("Fluid Sandbox"))
        {
            ImGui::End();
            return;
        }

        ImGui::Text("Fluid frames: %d", fluid_count);
        if (fluid_count <= 0)
        {
            ImGui::TextDisabled("No FluidFrameComponent found.");
            ImGui::End();
            return;
        }

        const entt::entity target = find_gui_target_fluid_frame(registry);
        if (target == entt::null)
        {
            ImGui::TextDisabled("No editable fluid frame target.");
            ImGui::End();
            return;
        }

        auto& fluid = registry.get<eeng::fluid_sandbox::ecs::FluidFrameComponent>(target);
        auto* live_config = fluid_frame_system_.runtime_config(target);
        ImGui::Text("Editing: %s", fluid.name.c_str());
        ImGui::TextDisabled("Target source: first fluid frame in scene");

        if (ImGui::Button("Reset Grid"))
        {
            fluid_frame_system_.clear_runtime_state(target);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset All"))
        {
            reset_fluid_component_to_defaults(fluid);
            fluid_frame_system_.reset_runtime(target);
            live_config = nullptr;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Enabled", &fluid.enabled);

        if (ImGui::CollapsingHeader("Emitters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Emit Velocity", &fluid.emit_velocity);
            ImGui::DragFloat("Velocity Scale", &fluid.velocity_emitter_scale, 0.02f, 0.0f, 20.0f, "%.2f");
            ImGui::Checkbox("Emit Density", &fluid.emit_density);
            ImGui::DragFloat("Density Scale", &fluid.density_emitter_scale, 0.01f, 0.0f, 5.0f, "%.2f");

            if (live_config)
            {
                int velocity_emitter_index = 0;
                for (auto& emitter : live_config->emitters)
                {
                    if (emitter.kind != eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::EmitterKind::Velocity)
                        continue;

                    ImGui::PushID(1000 + velocity_emitter_index);
                    const std::string header = "Velocity Emitter " + std::to_string(velocity_emitter_index);
                    if (ImGui::TreeNode(header.c_str()))
                    {
                        float speed = glm::length(emitter.value);
                        float angle_deg = glm::degrees(std::atan2(emitter.value.y, emitter.value.x));

                        if (ImGui::DragFloat("Angle", &angle_deg, 0.5f, -180.0f, 180.0f, "%.1f deg"))
                        {
                            const float angle_rad = glm::radians(angle_deg);
                            emitter.value = glm::vec2(std::cos(angle_rad), std::sin(angle_rad)) * speed;
                        }

                        if (ImGui::DragFloat("Speed", &speed, 0.02f, 0.0f, 20.0f, "%.2f"))
                        {
                            const float angle_rad = glm::radians(angle_deg);
                            emitter.value = glm::vec2(std::cos(angle_rad), std::sin(angle_rad)) * speed;
                        }

                        ImGui::DragFloat2("Center UV", &emitter.center_uv.x, 0.002f, 0.0f, 1.0f, "%.3f");
                        ImGui::DragFloat("Radius UV", &emitter.radius_uv, 0.002f, 0.005f, 0.5f, "%.3f");
                        emitter.center_uv = glm::clamp(emitter.center_uv, glm::vec2(0.0f), glm::vec2(1.0f));
                        emitter.radius_uv = glm::clamp(emitter.radius_uv, 0.005f, 0.5f);
                        ImGui::TextDisabled("Vector: (%.3f, %.3f)", emitter.value.x, emitter.value.y);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    ++velocity_emitter_index;
                }

                if (velocity_emitter_index == 0)
                    ImGui::TextDisabled("No velocity emitters in the live config.");
            }
        }

        if (ImGui::CollapsingHeader("Transport", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Override Cell Size", &fluid.override_cell_size);
            if (!fluid.override_cell_size)
                ImGui::BeginDisabled();
            ImGui::DragFloat("Cell Size", &fluid.cell_size, 0.001f, 0.005f, 0.25f, "%.4f");
            if (!fluid.override_cell_size)
                ImGui::EndDisabled();

            ImGui::Checkbox("Apply Viscosity", &fluid.apply_viscosity);
            ImGui::Checkbox("Override Viscosity", &fluid.override_viscosity);
            if (!fluid.apply_viscosity)
                ImGui::BeginDisabled();
            if (!fluid.override_viscosity)
                ImGui::BeginDisabled();
            ImGui::DragFloat("Viscosity", &fluid.viscosity, 0.0001f, 0.0f, 0.02f, "%.5f");
            if (!fluid.override_viscosity)
                ImGui::EndDisabled();
            if (!fluid.apply_viscosity)
                ImGui::EndDisabled();

            ImGui::Checkbox("Apply Vorticity Confinement", &fluid.apply_vorticity_confinement);
            if (!fluid.apply_vorticity_confinement)
                ImGui::BeginDisabled();
            ImGui::DragFloat("Confinement Strength", &fluid.vorticity_confinement, 0.01f, 0.0f, 5.0f, "%.2f");
            if (!fluid.apply_vorticity_confinement)
                ImGui::EndDisabled();

            ImGui::Checkbox("Apply Velocity Damping", &fluid.apply_velocity_damping);
            ImGui::Checkbox("Override Velocity Damping", &fluid.override_velocity_damping);
            if (!fluid.apply_velocity_damping || !fluid.override_velocity_damping)
                ImGui::BeginDisabled();
            ImGui::DragFloat("Velocity Damping", &fluid.velocity_damping, 0.001f, 0.0f, 2.0f, "%.3f");
            if (!fluid.apply_velocity_damping || !fluid.override_velocity_damping)
                ImGui::EndDisabled();

            ImGui::Checkbox("Apply Density Damping", &fluid.apply_density_damping);
            ImGui::Checkbox("Override Density Damping", &fluid.override_density_damping);
            if (!fluid.apply_density_damping || !fluid.override_density_damping)
                ImGui::BeginDisabled();
            ImGui::DragFloat("Density Damping", &fluid.density_damping, 0.001f, 0.0f, 2.0f, "%.3f");
            if (!fluid.apply_density_damping || !fluid.override_density_damping)
                ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader("Obstacles", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (!live_config)
            {
                ImGui::TextDisabled("Runtime config not available yet.");
            }
            else
            {
                bool obstacles_changed = false;
                ImGui::Text("Live obstacles: %d", static_cast<int>(live_config->obstacles.size()));
                ImGui::TextDisabled("These edits are runtime-only for now and reset back to JSON.");

                if (ImGui::Button("Add Box Obstacle"))
                {
                    eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleDesc obstacle{};
                    obstacle.shape = eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleShape::Box;
                    obstacle.min_uv = glm::vec2(0.40f, 0.35f);
                    obstacle.max_uv = glm::vec2(0.48f, 0.65f);
                    live_config->obstacles.push_back(obstacle);
                    obstacles_changed = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Add Circle Obstacle"))
                {
                    eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleDesc obstacle{};
                    obstacle.shape = eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleShape::Circle;
                    obstacle.center_uv = glm::vec2(0.45f, 0.50f);
                    obstacle.radius_uv = 0.08f;
                    live_config->obstacles.push_back(obstacle);
                    obstacles_changed = true;
                }

                for (std::size_t i = 0; i < live_config->obstacles.size(); ++i)
                {
                    auto& obstacle = live_config->obstacles[i];
                    ImGui::PushID(static_cast<int>(i));

                    const std::string header = "Obstacle " + std::to_string(i);
                    if (ImGui::TreeNode(header.c_str()))
                    {
                        int shape_index =
                            obstacle.shape == eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleShape::Circle ? 1 : 0;
                        static constexpr const char* kObstacleShapes[] = { "Box", "Circle" };
                        if (ImGui::Combo("Shape", &shape_index, kObstacleShapes, IM_ARRAYSIZE(kObstacleShapes)))
                        {
                            obstacle.shape = shape_index == 1
                                ? eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleShape::Circle
                                : eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleShape::Box;
                            obstacles_changed = true;
                        }

                        int boundary_index =
                            obstacle.boundary == eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleBoundaryMode::FreeSlip ? 1 : 0;
                        static constexpr const char* kBoundaryModes[] = { "No Slip", "Free Slip" };
                        if (ImGui::Combo("Boundary", &boundary_index, kBoundaryModes, IM_ARRAYSIZE(kBoundaryModes)))
                        {
                            obstacle.boundary = boundary_index == 1
                                ? eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleBoundaryMode::FreeSlip
                                : eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleBoundaryMode::NoSlip;
                            obstacles_changed = true;
                        }

                        if (obstacle.shape == eeng::fluid_sandbox::ecs::systems::FluidFrameSystem::ObstacleShape::Box)
                        {
                            obstacles_changed = ImGui::DragFloat2("Min UV", &obstacle.min_uv.x, 0.002f, 0.0f, 1.0f, "%.3f") || obstacles_changed;
                            obstacles_changed = ImGui::DragFloat2("Max UV", &obstacle.max_uv.x, 0.002f, 0.0f, 1.0f, "%.3f") || obstacles_changed;
                            obstacle.min_uv = glm::clamp(obstacle.min_uv, glm::vec2(0.0f), glm::vec2(1.0f));
                            obstacle.max_uv = glm::clamp(obstacle.max_uv, glm::vec2(0.0f), glm::vec2(1.0f));
                            obstacle.max_uv = glm::max(obstacle.max_uv, obstacle.min_uv + glm::vec2(0.005f));
                        }
                        else
                        {
                            obstacles_changed = ImGui::DragFloat2("Center UV", &obstacle.center_uv.x, 0.002f, 0.0f, 1.0f, "%.3f") || obstacles_changed;
                            obstacles_changed = ImGui::DragFloat("Radius UV", &obstacle.radius_uv, 0.002f, 0.005f, 0.5f, "%.3f") || obstacles_changed;
                            obstacle.center_uv = glm::clamp(obstacle.center_uv, glm::vec2(0.0f), glm::vec2(1.0f));
                            obstacle.radius_uv = glm::clamp(obstacle.radius_uv, 0.005f, 0.5f);
                        }

                        ImGui::TextDisabled("Type: %s", obstacle_shape_label(obstacle.shape));
                        ImGui::TextDisabled("Wall: %s", obstacle_boundary_label(obstacle.boundary));

                        if (ImGui::Button("Remove"))
                        {
                            live_config->obstacles.erase(live_config->obstacles.begin() + static_cast<std::ptrdiff_t>(i));
                            obstacles_changed = true;
                            ImGui::TreePop();
                            ImGui::PopID();
                            break;
                        }

                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }

                if (obstacles_changed)
                    fluid_frame_system_.refresh_runtime_config(target);
            }
        }

        if (ImGui::CollapsingHeader("View", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static constexpr const char* kRenderModes[] =
            {
                "Density",
                "Velocity Glyphs",
                "Pressure",
                "Divergence",
                "Vorticity"
            };
            int render_mode = static_cast<int>(fluid.render_mode);
            if (ImGui::Combo("Render Mode", &render_mode, kRenderModes, IM_ARRAYSIZE(kRenderModes)))
                fluid.render_mode = static_cast<eeng::fluid_sandbox::ecs::FluidFrameRenderMode>(render_mode);

            ImGui::DragFloat("Density Gain", &fluid.density_gain, 0.05f, 0.0f, 10.0f, "%.2f");
            ImGui::Checkbox("Draw Velocity Glyphs", &fluid.debug_draw_velocity);
            ImGui::Checkbox("Draw Obstacle Faces", &fluid.debug_draw_obstacle_faces);
            ImGui::DragFloat("Glyph Scale", &fluid.velocity_glyph_scale, 0.005f, 0.0f, 1.0f, "%.3f");
            ImGui::TextDisabled("Current view: %s", render_mode_label(fluid.render_mode));
        }

        if (live_config)
        {
            const float effective_cell_size = fluid.override_cell_size ? fluid.cell_size : live_config->cell_size;
            const glm::vec2 domain_size =
                glm::vec2(static_cast<float>(fluid.resolution.x), static_cast<float>(fluid.resolution.y)) * effective_cell_size;
            ImGui::Separator();
            ImGui::Text("Effective domain: %.3f x %.3f", domain_size.x, domain_size.y);
            ImGui::TextDisabled("Cell size %.4f, resolution %d x %d", effective_cell_size, fluid.resolution.x, fluid.resolution.y);
        }

        ImGui::End();
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
