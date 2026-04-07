// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "gui/RigSpawnerWidget.hpp"

#include "BatchRegistry.hpp"
#include "LogMacros.h"
#include "editor/EditorActions.hpp"
#include "editor/EntityPickerPopup.hpp"
#include "ecs/EntityManager.hpp"
#include "ecs/PistonAnimSyncComponent.hpp"
#include "ecs/PistonConstraintDriveComponent.hpp"
#include "ecs/PistonRigBuilder.hpp"
#include "ecs/VehicleRig1Builder.hpp"
#include "ecs/VehicleRig1Component.hpp"
#include "meta/MetaAux.h"
#include "meta/MetaSerialize.hpp"
#include "misc/cpp/imgui_stdlib.h"

#if __has_include("ecs/VehicleRig1ControlComponent.hpp")
#include "ecs/VehicleRig1ControlComponent.hpp"
#define EENG_HAS_VEHICLE_RIG1_CONTROL 1
#else
#define EENG_HAS_VEHICLE_RIG1_CONTROL 0
#endif

#if __has_include("ecs/PistonInputComponent.hpp")
#include "ecs/PistonInputComponent.hpp"
#define EENG_HAS_PISTON_INPUT_COMPONENT 1
#else
#define EENG_HAS_PISTON_INPUT_COMPONENT 0
#endif

#include "imgui.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <string>

namespace eeng::gui
{
    namespace
    {
        struct VehicleRig1UiState
        {
            bool initialized = false;
            ecs::VehicleRig1Spec spec{};
            glm::vec3 spawn_pos{ 0.0f, 2.0f, 0.0f };
            float control_steer_speed = 6.0f;
            float control_steer_max_impulse = 2000.0f;
            float control_drive_velocity = 10.0f;
            float control_drive_max_impulse = 150.0f;
            float control_brake_max_impulse = 200.0f;
        };

        struct PistonRigUiState
        {
            bool initialized = false;
            ecs::PistonRigSpec spec{};
        };

        VehicleRig1UiState& vehicle_state()
        {
            static VehicleRig1UiState state{};
            return state;
        }

        PistonRigUiState& piston_state()
        {
            static PistonRigUiState state{};
            return state;
        }

        void reset_vehicle_rig1_config(VehicleRig1UiState& state)
        {
            state.initialized = true;
            state.spec = {};
            state.spawn_pos = { 0.0f, 2.0f, 0.0f };
            state.control_steer_speed = 6.0f;
            state.control_steer_max_impulse = 2000.0f;
            state.control_drive_velocity = 10.0f;
            state.control_drive_max_impulse = 150.0f;
            state.control_brake_max_impulse = 200.0f;

            state.spec.name_prefix = "VehicleRig1";
            state.spec.chunk_tag = "vehicle_rig1";
            state.spec.steer_axis = { 0.0f, 1.0f, 0.0f };
            state.spec.steer_limit = 0.8f;
            state.spec.disable_collisions = true;
            state.spec.drive_default = false;
            state.spec.wheel_friction = 0.5f;
            state.spec.chassis_model_name = "carbody";
            state.spec.wheel_model_name = "tyre";
            state.spec.chassis_half_extents = { 1.6f, 0.35f, 1.0f };

            auto make_wheel = [&](const glm::vec2& mount_sign, bool steerable, bool driven)
            {
                ecs::VehicleRig1WheelSpec wheel{};
                wheel.mount_override = false;
                wheel.mount_sign = mount_sign;

                // Hard-coded axes for the prototype.
                wheel.suspension_axis = { 0.0f, -1.0f, 0.0f };
                wheel.axle_axis = { 0.0f, 0.0f, 1.0f };

                // Suspension: rest/travel define the default linear limits in the 6DoF frame.
                wheel.suspension_rest_length = 0.9f;
                wheel.suspension_travel = 0.8f;
                wheel.use_linear_limits = true;
                wheel.linear_limit_min = { 1.0f, 0.0f, 0.0f };
                wheel.linear_limit_max = { 3.0f, 0.0f, 0.0f };
                wheel.linear_equilibrium_enabled = { 1.0f, 0.0f, 0.0f };
                wheel.linear_equilibrium_target = { 1.5f, 0.0f, 0.0f };

                // Spring tuning (works in the 6DoF constraint).
                wheel.spring_k = 500.0f;
                wheel.spring_d = 5.0f;

                // Collider sizes.
                wheel.wheel_collider_type = ecs::WheelColliderType::Sphere;
                wheel.wheel_radius = 0.35f;
                wheel.wheel_width = 0.25f;
                wheel.knuckle_radius = 0.15f;

                // Per-wheel capability flags.
                wheel.steerable = steerable;
                wheel.driven = driven;
                wheel.drive_override = true;

                // Flip drive direction for steerable wheels if needed (front wheels were observed reversed).
                wheel.drive_direction = steerable ? -1.0f : 1.0f;
                // Keep steering direction consistent for both front wheels.
                wheel.steer_direction = 1.0f;
                return wheel;
            };

            state.spec.wheels.clear();
            state.spec.wheels.push_back(make_wheel({ 1.0f, 1.0f }, true, true));
            state.spec.wheels.push_back(make_wheel({ 1.0f, -1.0f }, true, true));
            state.spec.wheels.push_back(make_wheel({ -1.0f, 1.0f }, false, false));
            state.spec.wheels.push_back(make_wheel({ -1.0f, -1.0f }, false, false));
        }

        void ensure_vehicle_rig1_config(VehicleRig1UiState& state)
        {
            if (state.initialized)
                return;
            reset_vehicle_rig1_config(state);
        }

        void reset_piston_rig_config(PistonRigUiState& state)
        {
            state.initialized = true;
            state.spec = {};
            state.spec.name_prefix = "Piston";
            state.spec.chunk_tag = "piston_rig";
            state.spec.position = { 0.0f, 2.0f, 0.0f };
            state.spec.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            state.spec.anchor_local_a = { 0.0f, 0.0f, 0.0f };
            state.spec.anchor_local_b = { 1.0f, 0.0f, 0.0f };
            state.spec.use_sockets = true;
            state.spec.axis_local = { 1.0f, 0.0f, 0.0f };
            state.spec.disable_collisions = true;
            state.spec.anim_clip_name = "Piston";
            state.spec.stroke_min = 0.0f;
            state.spec.stroke_max = 1.0f;
            state.spec.max_force = 2000.0f;
            state.spec.max_velocity = 1.0f;
            state.spec.mode = 0;
            state.spec.target_extension = 0.0f;
            state.spec.lock_when_idle = true;
        }

        void ensure_piston_rig_config(PistonRigUiState& state)
        {
            if (state.initialized)
                return;
            reset_piston_rig_config(state);
        }

        void spawn_vehicle_rig1_from_prefab(EngineContext& ctx, VehicleRig1UiState& state)
        {
            if (!ctx.entity_manager)
                return;

            ensure_vehicle_rig1_config(state);

            ecs::VehicleRig1ChassisSpec chassis_spec{};
            chassis_spec.position = state.spawn_pos;
            chassis_spec.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            chassis_spec.half_extents = state.spec.chassis_half_extents;

            nlohmann::json prefab_json = ecs::build_vehicle_rig1_prefab_json(
                ctx,
                state.spec,
                chassis_spec);
            if (!prefab_json.is_array() || prefab_json.empty())
            {
                EENG_LOG_WARN(&ctx, "VehicleRig1 prefab build failed: empty JSON.");
                return;
            }

#if EENG_HAS_VEHICLE_RIG1_CONTROL
            // Inject control component on the rig root before spawning.
            ecs::VehicleRig1ControlComponent control{};
            control.steer_limit = state.spec.steer_limit;
            control.steer_speed = state.control_steer_speed;
            control.steer_max_impulse = state.control_steer_max_impulse;
            control.drive_velocity = state.control_drive_velocity;
            control.drive_max_impulse = state.control_drive_max_impulse;
            control.brake_max_impulse = state.control_brake_max_impulse;

            const auto control_type = meta::get_meta_type_id_string<ecs::VehicleRig1ControlComponent>();
            auto& root_json = prefab_json.front();
            if (!root_json.contains("components") || !root_json["components"].is_object())
                root_json["components"] = nlohmann::json::object();
            root_json["components"][control_type] = meta::serialize_any(
                entt::forward_as_meta(control),
                meta::SerializationPurpose::file);
#else
            static bool warned = false;
            if (!warned)
            {
                EENG_LOG_WARN(&ctx, "VehicleRig1 control component not available; spawning rig without control.");
                warned = true;
            }
#endif

            // Spawn via command so undo/redo works.
            editor::SceneActions::spawn_entity_branch_from_json(
                ctx,
                std::move(prefab_json),
                ecs::Entity{},
                false);
        }

        void spawn_piston_rig_from_prefab(EngineContext& ctx, PistonRigUiState& state)
        {
            if (!ctx.entity_manager)
                return;

            ensure_piston_rig_config(state);

            nlohmann::json prefab_json = ecs::build_piston_rig_prefab_json(
                ctx,
                state.spec);
            if (!prefab_json.is_array() || prefab_json.empty())
            {
                EENG_LOG_WARN(&ctx, "Piston rig prefab build failed: empty JSON.");
                return;
            }

            // Inject piston input + animation sync on the rig root before spawning.
#if EENG_HAS_PISTON_INPUT_COMPONENT
            ecs::PistonInputComponent input{};
#endif
            ecs::PistonAnimSyncComponent anim_sync{};
            anim_sync.clip_name = state.spec.anim_clip_name;

            auto& root_json = prefab_json.front();
            if (!root_json.contains("components") || !root_json["components"].is_object())
                root_json["components"] = nlohmann::json::object();

#if EENG_HAS_PISTON_INPUT_COMPONENT
            const auto input_type = meta::get_meta_type_id_string<ecs::PistonInputComponent>();
            root_json["components"][input_type] = meta::serialize_any(
                entt::forward_as_meta(input),
                meta::SerializationPurpose::file);
#else
            static bool warned = false;
            if (!warned)
            {
                EENG_LOG_WARN(&ctx, "Piston input component not available; spawning rig without input component.");
                warned = true;
            }
#endif

            const auto sync_type = meta::get_meta_type_id_string<ecs::PistonAnimSyncComponent>();
            root_json["components"][sync_type] = meta::serialize_any(
                entt::forward_as_meta(anim_sync),
                meta::SerializationPurpose::file);

            editor::SceneActions::spawn_entity_branch_from_json(
                ctx,
                std::move(prefab_json),
                ecs::Entity{},
                false);
        }
    } // namespace

    void RigSpawnerWidget::draw()
    {
        auto& vehicle_state_ref = vehicle_state();
        auto& piston_state_ref = piston_state();

        const auto add_tooltip = [](const char* text)
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", text);
        };

        if (ImGui::CollapsingHeader("VehicleRig1 Rig", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ensure_vehicle_rig1_config(vehicle_state_ref);

            bool default_batch_loaded = false;
            if (ctx.batch_registry)
            {
                auto& br = static_cast<BatchRegistry&>(*ctx.batch_registry);
                BatchId default_id{};
                if (br.try_get_batch_id_by_name(BatchRegistry::kDefaultBatchName, default_id))
                    default_batch_loaded = br.is_batch_loaded(default_id);
            }

            if (ImGui::TreeNodeEx("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat3("Spawn position", &vehicle_state_ref.spawn_pos.x, 0.05f);
                add_tooltip("World-space position for the chassis when spawning the rig.");

                if (ImGui::Button("Spawn VehicleRig1"))
                    spawn_vehicle_rig1_from_prefab(ctx, vehicle_state_ref);

                ImGui::SameLine();
                if (ImGui::Button("Reset Config"))
                    reset_vehicle_rig1_config(vehicle_state_ref);

                ImGui::SameLine();
                ImGui::TextDisabled(default_batch_loaded ? "default batch loaded" : "default batch not loaded");
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Rig Config"))
            {
                ImGui::TextDisabled("Name prefix: %s", vehicle_state_ref.spec.name_prefix.c_str());
                ImGui::TextDisabled("Chunk tag: %s", vehicle_state_ref.spec.chunk_tag.c_str());

                glm::vec3 chassis_size = vehicle_state_ref.spec.chassis_half_extents * 2.0f;
                if (ImGui::DragFloat3("Chassis size", &chassis_size.x, 0.05f, 0.05f, 20.0f))
                {
                    chassis_size = glm::max(chassis_size, glm::vec3(0.05f));
                    vehicle_state_ref.spec.chassis_half_extents = chassis_size * 0.5f;
                }
                add_tooltip("Chassis collider size in world units (width, height, length).");

                ImGui::DragFloat3("Steer axis", &vehicle_state_ref.spec.steer_axis.x, 0.01f);
                add_tooltip("Chassis-local steering axis for the 6DoF constraint frame.");
                ImGui::DragFloat("Steer limit", &vehicle_state_ref.spec.steer_limit, 0.01f, 0.0f, 3.14f);
                add_tooltip("Max steering angle in radians (+/-) for steerable wheels.");
                ImGui::Checkbox("Disable collisions", &vehicle_state_ref.spec.disable_collisions);
                add_tooltip("Disable collision response between constrained body pairs.");

                if (ImGui::TreeNode("Wheel Defaults"))
                {
                    ImGui::DragFloat("Wheel friction", &vehicle_state_ref.spec.wheel_friction, 0.1f, 0.0f, 10.0f);
                    add_tooltip("Default wheel friction (used unless overridden per wheel).");
                    ImGui::Checkbox("Drive default", &vehicle_state_ref.spec.drive_default);
                    add_tooltip("Default driven flag for wheels without overrides.");

                    if (ImGui::Button("Clear wheel overrides"))
                    {
                        for (auto& wheel : vehicle_state_ref.spec.wheels)
                        {
                            wheel.friction_override = false;
                            wheel.drive_override = false;
                        }
                    }
                    add_tooltip("Disable per-wheel overrides so all wheels use the defaults.");

                    ImGui::TreePop();
                }

                if (ImGui::BeginTable("VehicleRig1WheelSummary", 6,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
                {
                    ImGui::TableSetupColumn("#");
                    ImGui::TableSetupColumn("Mount");
                    ImGui::TableSetupColumn("Steer");
                    ImGui::TableSetupColumn("Drive");
                    ImGui::TableSetupColumn("Friction");
                    ImGui::TableSetupColumn("Overrides");
                    ImGui::TableHeadersRow();

                    for (std::size_t i = 0; i < vehicle_state_ref.spec.wheels.size(); ++i)
                    {
                        const auto& wheel = vehicle_state_ref.spec.wheels[i];
                        const float effective_friction = wheel.friction_override
                            ? wheel.wheel_friction
                            : vehicle_state_ref.spec.wheel_friction;
                        const bool effective_driven = wheel.drive_override
                            ? wheel.driven
                            : vehicle_state_ref.spec.drive_default;

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%zu", i);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", wheel.mount_override ? "Custom" : "Derived");
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(wheel.steerable ? "Yes" : "No");
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextUnformatted(effective_driven ? "Driven" : "Free");
                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%.2f", effective_friction);
                        ImGui::TableSetColumnIndex(5);
                        ImGui::Text("%s%s",
                            wheel.drive_override ? "Drive " : "",
                            wheel.friction_override ? "Friction" : "");
                    }
                    ImGui::EndTable();
                }
                add_tooltip("Quick summary of effective per-wheel settings.");

                for (std::size_t i = 0; i < vehicle_state_ref.spec.wheels.size(); ++i)
                {
                    auto& wheel = vehicle_state_ref.spec.wheels[i];
                    ImGui::PushID(static_cast<int>(i));
                    const std::string label = "Wheel " + std::to_string(i);
                    if (ImGui::TreeNode(label.c_str()))
                    {
                        const float effective_friction = wheel.friction_override
                            ? wheel.wheel_friction
                            : vehicle_state_ref.spec.wheel_friction;
                        const bool effective_driven = wheel.drive_override
                            ? wheel.driven
                            : vehicle_state_ref.spec.drive_default;

                        ImGui::TextDisabled("Effective friction: %.2f", effective_friction);
                        ImGui::TextDisabled("Effective drive: %s", effective_driven ? "Driven" : "Free");

                        if (ImGui::BeginTabBar("WheelTabs"))
                        {
                            if (ImGui::BeginTabItem("Mount"))
                            {
                                ImGui::Checkbox("Override mount", &wheel.mount_override);
                                add_tooltip("Use a custom mount position instead of deriving from chassis size.");
                                if (wheel.mount_override)
                                {
                                    ImGui::DragFloat3("Mount local", &wheel.mount_local.x, 0.02f);
                                    add_tooltip("Chassis-local mount position for this wheel/suspension.");
                                }
                                else
                                {
                                    ImGui::DragFloat2("Mount sign", &wheel.mount_sign.x, 0.1f, -1.0f, 1.0f);
                                    add_tooltip("Sign used to place the wheel from chassis size (X/Z).");
                                    const glm::vec3 derived_mount{
                                        vehicle_state_ref.spec.chassis_half_extents.x * wheel.mount_sign.x,
                                        0.0f,
                                        vehicle_state_ref.spec.chassis_half_extents.z * wheel.mount_sign.y
                                    };
                                    ImGui::TextDisabled("Derived mount: (%.2f, %.2f, %.2f)",
                                        derived_mount.x,
                                        derived_mount.y,
                                        derived_mount.z);
                                }
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Suspension"))
                            {
                                ImGui::DragFloat3("Suspension axis", &wheel.suspension_axis.x, 0.02f);
                                add_tooltip("Chassis-local axis for suspension travel (6DoF frame X).");
                                ImGui::DragFloat3("Axle axis", &wheel.axle_axis.x, 0.02f);
                                add_tooltip("Knuckle/wheel local axle axis for the hinge.");
                                ImGui::DragFloat("Rest length", &wheel.suspension_rest_length, 0.01f, 0.0f, 10.0f);
                                add_tooltip("Rest length along suspension axis (positive direction).");
                                ImGui::DragFloat("Travel", &wheel.suspension_travel, 0.01f, 0.0f, 10.0f);
                                add_tooltip("Total suspension travel (symmetric around rest length).");

                                ImGui::Checkbox("Use linear limits", &wheel.use_linear_limits);
                                add_tooltip("Override default linear limits for the 6DoF suspension axis.");
                                if (wheel.use_linear_limits)
                                {
                                    ImGui::DragFloat("Limit min X", &wheel.linear_limit_min.x, 0.01f, -10.0f, 10.0f);
                                    add_tooltip("Minimum limit along suspension axis (constraint frame X).");
                                    ImGui::DragFloat("Limit max X", &wheel.linear_limit_max.x, 0.01f, -10.0f, 10.0f);
                                    add_tooltip("Maximum limit along suspension axis (constraint frame X).");

                                    bool equilibrium_enabled = wheel.linear_equilibrium_enabled.x > 0.5f;
                                    if (ImGui::Checkbox("Equilibrium X", &equilibrium_enabled))
                                        wheel.linear_equilibrium_enabled = equilibrium_enabled ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f);
                                    add_tooltip("Enable spring equilibrium on the suspension axis.");
                                    ImGui::DragFloat("Equilibrium target X", &wheel.linear_equilibrium_target.x, 0.01f, -10.0f, 10.0f);
                                    add_tooltip("Target equilibrium position along the suspension axis.");
                                }

                                ImGui::DragFloat("Spring K", &wheel.spring_k, 1.0f, 0.0f, 50000.0f);
                                add_tooltip("Suspension spring stiffness (6DoF spring on X).");
                                ImGui::DragFloat("Spring D", &wheel.spring_d, 1.0f, 0.0f, 50000.0f);
                                add_tooltip("Suspension damping (6DoF spring on X).");
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Collider"))
                            {
                                ImGui::DragFloat("Wheel radius", &wheel.wheel_radius, 0.01f, 0.05f, 5.0f);
                                add_tooltip("Radius for autogenerated wheel collider.");
                                int collider_choice = static_cast<int>(wheel.wheel_collider_type);
                                if (ImGui::Combo("Wheel collider", &collider_choice, "Sphere\0Capsule\0"))
                                    wheel.wheel_collider_type = static_cast<ecs::WheelColliderType>(collider_choice);
                                add_tooltip("Collider type used for autogenerated wheels.");
                                if (wheel.wheel_collider_type == ecs::WheelColliderType::Capsule)
                                {
                                    ImGui::DragFloat("Wheel width", &wheel.wheel_width, 0.01f, 0.0f, 5.0f);
                                    add_tooltip("Capsule height (wheel width) along the axle axis.");
                                }
                                ImGui::DragFloat("Knuckle radius", &wheel.knuckle_radius, 0.01f, 0.05f, 5.0f);
                                add_tooltip("Radius for autogenerated knuckle trigger collider.");
                                ImGui::DragFloat("Wheel mass", &wheel.wheel_mass, 0.1f, 0.0f, 100.0f);
                                add_tooltip("Override wheel mass in kg (<= 0 keeps auto-mass).");
                                ImGui::DragFloat("Knuckle mass", &wheel.knuckle_mass, 0.1f, 0.0f, 100.0f);
                                add_tooltip("Override knuckle mass in kg (<= 0 keeps auto-mass).");

                                ImGui::Checkbox("Override friction", &wheel.friction_override);
                                add_tooltip("Use a per-wheel friction instead of the global default.");
                                if (wheel.friction_override)
                                {
                                    ImGui::DragFloat("Wheel friction", &wheel.wheel_friction, 0.1f, 0.0f, 10.0f);
                                    add_tooltip("Wheel friction applied to the wheel rigid body.");
                                }
                                else
                                {
                                    ImGui::TextDisabled("Wheel friction (global): %.2f", vehicle_state_ref.spec.wheel_friction);
                                }
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Drive"))
                            {
                                ImGui::Checkbox("Steerable", &wheel.steerable);
                                add_tooltip("Allow steering on this wheel (6DoF angular X).");
                                ImGui::Checkbox("Override drive", &wheel.drive_override);
                                add_tooltip("Use a per-wheel driven flag instead of the global default.");
                                if (wheel.drive_override)
                                {
                                    ImGui::Checkbox("Driven", &wheel.driven);
                                    add_tooltip("Enable drive motor on this wheel hinge.");
                                }
                                else
                                {
                                    ImGui::TextDisabled("Driven (global): %s", vehicle_state_ref.spec.drive_default ? "Yes" : "No");
                                }
                                ImGui::DragFloat("Drive direction", &wheel.drive_direction, 0.1f, -5.0f, 5.0f);
                                add_tooltip("Sign flip for drive direction (+/-).");
                                ImGui::DragFloat("Steer direction", &wheel.steer_direction, 0.1f, -5.0f, 5.0f);
                                add_tooltip("Sign flip for steering direction (+/-).");
                                ImGui::EndTabItem();
                            }

                            ImGui::EndTabBar();
                        }

                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Control Config"))
            {
                ImGui::DragFloat("Steer speed", &vehicle_state_ref.control_steer_speed, 0.1f, 0.0f, 50.0f);
                add_tooltip("Steering rate limit (rad/s).");
                ImGui::DragFloat("Steer max impulse", &vehicle_state_ref.control_steer_max_impulse, 10.0f, 0.0f, 100000.0f);
                add_tooltip("Max steering motor impulse (force).");
                ImGui::DragFloat("Drive velocity", &vehicle_state_ref.control_drive_velocity, 0.1f, 0.0f, 200.0f);
                add_tooltip("Target wheel angular velocity for drive.");
                ImGui::DragFloat("Drive max impulse", &vehicle_state_ref.control_drive_max_impulse, 10.0f, 0.0f, 100000.0f);
                add_tooltip("Max drive motor impulse.");
                ImGui::DragFloat("Brake max impulse", &vehicle_state_ref.control_brake_max_impulse, 10.0f, 0.0f, 100000.0f);
                add_tooltip("Max impulse when braking/coasting.");
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Rig Monitor"))
            {
#if EENG_HAS_VEHICLE_RIG1_CONTROL
                const ecs::VehicleRig1ControlComponent* control = nullptr;
#else
                const void* control = nullptr;
#endif
                const ecs::VehicleRig1RigComponent* rig = nullptr;
                int rig_count = 0;
                if (ctx.entity_manager)
                {
                    auto& registry = ctx.entity_manager->registry();
#if EENG_HAS_VEHICLE_RIG1_CONTROL
                    auto view = registry.view<ecs::VehicleRig1ControlComponent, ecs::VehicleRig1RigComponent>();
#else
                    auto view = registry.view<ecs::VehicleRig1RigComponent>();
#endif
                    rig_count = 0;
                    for (auto it = view.begin(); it != view.end(); ++it)
                        ++rig_count;
                    if (auto it = view.begin(); it != view.end())
                    {
                        const entt::entity control_entity = *it;
#if EENG_HAS_VEHICLE_RIG1_CONTROL
                        control = registry.try_get<ecs::VehicleRig1ControlComponent>(control_entity);
#endif
                        rig = registry.try_get<ecs::VehicleRig1RigComponent>(control_entity);
                    }
                }

#if EENG_HAS_VEHICLE_RIG1_CONTROL
                if (control)
                {
                    ImGui::Text("Active rigs: %d", rig_count);
                    ImGui::Text("Input steer/drive: %.2f / %.2f", control->steer_input, control->drive_input);
                    ImGui::Text("Steer target/angle: %.2f / %.2f", control->steer_target, control->steer_angle);
                    ImGui::Text("Steer speed/impulse: %.1f / %.1f", control->steer_speed, control->steer_max_impulse);
                    ImGui::Text("Drive vel/impulse: %.1f / %.1f", control->drive_velocity, control->drive_max_impulse);
                    ImGui::Text("Brake impulse: %.1f", control->brake_max_impulse);
                    ImGui::Text("Controller id: %d", control->controller_id);
                    if (rig)
                        ImGui::Text("Wheel count: %d", static_cast<int>(rig->wheels.size()));
                }
                else
                {
                    ImGui::TextDisabled("VehicleRig1 not spawned (or control missing)");
                }
#else
                ImGui::TextDisabled("VehicleRig1 control component not available.");
                (void)rig;
#endif

                ImGui::TreePop();
            }
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Piston Rig", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ensure_piston_rig_config(piston_state_ref);

            if (ImGui::TreeNodeEx("Spawn", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat3("Position", &piston_state_ref.spec.position.x, 0.05f);
                add_tooltip("World-space position for the piston rig root.");

                if (ImGui::Button("Spawn Piston Rig"))
                    spawn_piston_rig_from_prefab(ctx, piston_state_ref);

                ImGui::SameLine();
                if (ImGui::Button("Reset Config"))
                    reset_piston_rig_config(piston_state_ref);

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Rig Config"))
            {
                ImGui::TextDisabled("Name prefix: %s", piston_state_ref.spec.name_prefix.c_str());
                ImGui::TextDisabled("Chunk tag: %s", piston_state_ref.spec.chunk_tag.c_str());
                ImGui::TextDisabled("Constraint: 6DoF distance (axis from anchors)");

                if (ctx.entity_manager)
                {
                    if (auto* em = dynamic_cast<EntityManager*>(ctx.entity_manager.get()))
                    {
                        auto draw_entity_picker = [&](const char* label, ecs::EntityRef& ref, const char* popup_id)
                        {
                            ImGui::TextUnformatted(label);
                            ImGui::SameLine();
                            const std::string current_label = editor::detail::make_entity_label(*em, ref);
                            if (ImGui::Button(current_label.c_str()))
                                ImGui::OpenPopup(popup_id);
                            if (editor::entity_picker_popup(popup_id, ref, *em))
                            {
                            }
                        };

                        draw_entity_picker("Body A", piston_state_ref.spec.body_a, "piston_body_a_picker");
                        draw_entity_picker("Body B", piston_state_ref.spec.body_b, "piston_body_b_picker");
                    }
                }

                ImGui::DragFloat3("Anchor A local", &piston_state_ref.spec.anchor_local_a.x, 0.02f);
                add_tooltip("Anchor offset: root-local when sockets are off, target-local when sockets are on.");
                ImGui::DragFloat3("Anchor B local", &piston_state_ref.spec.anchor_local_b.x, 0.02f);
                add_tooltip("Anchor offset: root-local when sockets are off, target-local when sockets are on.");
                ImGui::DragFloat3("Axis local", &piston_state_ref.spec.axis_local.x, 0.02f);
                add_tooltip("Root-local axis used by the drive and alignment.");

                ImGui::Checkbox("Use sockets", &piston_state_ref.spec.use_sockets);
                add_tooltip("Attach anchor entities via sockets instead of reparenting.");

                ImGui::Checkbox("Disable collisions", &piston_state_ref.spec.disable_collisions);
                add_tooltip("Disable collision response between constrained bodies.");

                ImGui::InputText("Anim clip", &piston_state_ref.spec.anim_clip_name);
                add_tooltip("Optional clip override for PistonAnimSync (empty = graph clip).");

                ImGui::DragFloat("Stroke min", &piston_state_ref.spec.stroke_min, 0.01f, -10.0f, 10.0f);
                add_tooltip("Minimum extension along the axis.");
                ImGui::DragFloat("Stroke max", &piston_state_ref.spec.stroke_max, 0.01f, -10.0f, 10.0f);
                add_tooltip("Maximum extension along the axis.");

                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Drive Config"))
            {
                ImGui::DragFloat("Max force", &piston_state_ref.spec.max_force, 10.0f, 0.0f, 100000.0f);
                add_tooltip("Max motor impulse/force.");
                ImGui::DragFloat("Max velocity", &piston_state_ref.spec.max_velocity, 0.1f, 0.0f, 1000.0f);
                add_tooltip("Max linear velocity (units/sec).");

                static const char* kModes[] = { "Hold", "Extend", "Contract", "Position" };
                ImGui::Combo("Mode", &piston_state_ref.spec.mode, kModes, IM_ARRAYSIZE(kModes));
                add_tooltip("Hold locks at current extension, Extend/Contract drives to limits.");

                if (piston_state_ref.spec.mode == 3)
                {
                    ImGui::DragFloat("Target extension", &piston_state_ref.spec.target_extension, 0.01f, 0.0f, 1.0f);
                    add_tooltip("Normalized [0,1] position within the stroke.");
                }

                ImGui::Checkbox("Lock when idle", &piston_state_ref.spec.lock_when_idle);
                add_tooltip("Keep constraint locked when not driving.");
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Live Control (Play)"))
            {
                ecs::PistonConstraintDriveComponent* live = nullptr;
                int rig_count = 0;
                if (ctx.entity_manager)
                {
                    auto& registry = ctx.entity_manager->registry();
                    auto view = registry.view<ecs::PistonConstraintDriveComponent>();
                    rig_count = 0;
                    for (auto it = view.begin(); it != view.end(); ++it)
                        ++rig_count;
                    if (auto it = view.begin(); it != view.end())
                    {
                        const entt::entity drive_entity = *it;
                        live = registry.try_get<ecs::PistonConstraintDriveComponent>(drive_entity);
                    }
                }

                if (!live)
                {
                    ImGui::TextDisabled("No piston rigs found.");
                }
                else
                {
                    ImGui::Text("Active rigs: %d (editing first)", rig_count);
                    ImGui::Text("Current pos/ext: %.3f / %.3f", live->current_position, live->current_extension);

                    ImGui::DragFloat("Stroke min##piston_live", &live->stroke_min, 0.01f, -10.0f, 10.0f);
                    add_tooltip("Minimum extension along the axis.");
                    ImGui::DragFloat("Stroke max##piston_live", &live->stroke_max, 0.01f, -10.0f, 10.0f);
                    add_tooltip("Maximum extension along the axis.");

                    ImGui::DragFloat("Max force##piston_live", &live->max_force, 10.0f, 0.0f, 100000.0f);
                    add_tooltip("Max motor impulse/force.");
                    ImGui::DragFloat("Max velocity##piston_live", &live->max_velocity, 0.1f, 0.0f, 1000.0f);
                    add_tooltip("Max linear velocity (units/sec).");
                }

                ImGui::TreePop();
            }
        }
    }
} // namespace eeng::gui
