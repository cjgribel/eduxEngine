// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/VehicleRig1Builder.hpp"

#include "LogMacros.h"
#include "engineapi/EngineContextHelpers.hpp"
#include "ecs/EntityManager.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/ModelComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"
#include "meta/MetaAux.h"
#include "meta/EntityMetaHelpers.hpp"
#include "meta/MetaSerialize.hpp"
#include "ecs/EntityManager.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace eeng::ecs
{
    namespace
    {
        // Normalize a vector; fall back to a safe axis if it is too small.
        glm::vec3 normalize_or_default(const glm::vec3& v, const glm::vec3& fallback)
        {
            const float len2 = glm::dot(v, v);
            if (len2 <= 1e-8f)
                return fallback;
            return v / std::sqrt(len2);
        }

        // Build an orthonormal frame with X along axis_x and Y close to axis_y_hint.
        glm::quat make_constraint_frame(const glm::vec3& axis_x, const glm::vec3& axis_y_hint)
        {
            const glm::vec3 x = normalize_or_default(axis_x, glm::vec3(1.0f, 0.0f, 0.0f));
            glm::vec3 y = axis_y_hint - x * glm::dot(axis_y_hint, x);
            y = normalize_or_default(y, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::vec3 z = normalize_or_default(glm::cross(x, y), glm::vec3(0.0f, 0.0f, 1.0f));
            y = normalize_or_default(glm::cross(z, x), glm::vec3(0.0f, 1.0f, 0.0f));

            const glm::mat3 basis(x, y, z);
            return glm::quat_cast(basis);
        }

        // Create a named entity parented under `parent`, returning a bound EntityRef.
        ecs::EntityRef create_entity(
            EntityManager& em,
            const std::string& chunk_tag,
            const std::string& name,
            const ecs::Entity& parent)
        {
            auto [guid, entity] = em.create_entity_live_parent(
                chunk_tag,
                name,
                parent,
                ecs::Entity::EntityNull);
            return ecs::EntityRef{ guid, entity };
        }

        // Ensure a TransformComponent exists, initializing with position/rotation.
        void ensure_transform(
            entt::registry& registry,
            const ecs::Entity& entity,
            const glm::vec3& position,
            const glm::quat& rotation)
        {
            if (registry.all_of<ecs::TransformComponent>(entity))
                return;

            auto& tfm = registry.emplace<ecs::TransformComponent>(entity);
            tfm.set_position(position);
            tfm.set_rotation(rotation);
        }

        // Ensure a spherical collider; used for prototype wheels/knuckles.
        void ensure_collider_sphere(
            entt::registry& registry,
            const ecs::Entity& entity,
            float radius,
            bool is_trigger)
        {
            if (registry.all_of<ecs::ColliderComponent>(entity))
                return;

            ecs::ColliderComponent colliders{};
            ecs::ColliderDesc desc{};
            desc.type = ecs::ColliderType::Sphere;
            desc.radius = std::max(0.0f, radius);
            desc.is_trigger = is_trigger;
            colliders.colliders.push_back(desc);
            registry.emplace<ecs::ColliderComponent>(entity, std::move(colliders));
        }

        void ensure_wheel_collider(
            entt::registry& registry,
            const ecs::Entity& entity,
            const VehicleRig1WheelSpec& wheel_spec,
            const glm::vec3& axle_axis_local)
        {
            if (registry.all_of<ecs::ColliderComponent>(entity))
                return;

            ecs::ColliderComponent colliders{};
            ecs::ColliderDesc desc{};
            desc.is_trigger = false;

            if (wheel_spec.wheel_collider_type == WheelColliderType::Capsule)
            {
                desc.type = ecs::ColliderType::Capsule;
                desc.radius = std::max(0.0f, wheel_spec.wheel_radius);
                desc.height = std::max(0.0f, wheel_spec.wheel_width);

                const glm::vec3 axis = normalize_or_default(axle_axis_local, glm::vec3(0.0f, 0.0f, 1.0f));
                desc.local_rotation = glm::rotation(glm::vec3(0.0f, 0.0f, 1.0f), axis);
            }
            else
            {
                desc.type = ecs::ColliderType::Sphere;
                desc.radius = std::max(0.0f, wheel_spec.wheel_radius);
            }

            colliders.colliders.push_back(desc);
            registry.emplace<ecs::ColliderComponent>(entity, std::move(colliders));
        }

        void ensure_rigidbody_aux_components(
            entt::registry& registry,
            const ecs::Entity& entity)
        {
            if (!registry.all_of<ecs::RigidBodyComponent>(entity))
                return;

            if (!registry.all_of<ecs::PhysicsMaterialComponent>(entity))
                registry.emplace<ecs::PhysicsMaterialComponent>(entity);
            if (!registry.all_of<ecs::CollisionFilterComponent>(entity))
                registry.emplace<ecs::CollisionFilterComponent>(entity);
            if (!registry.all_of<ecs::PhysicsEventsComponent>(entity))
                registry.emplace<ecs::PhysicsEventsComponent>(entity);
        }

        void ensure_rigidbody(
            entt::registry& registry,
            const ecs::Entity& entity,
            ecs::PhysicsMotionType motion)
        {
            if (!registry.all_of<ecs::RigidBodyComponent>(entity))
            {
                ecs::RigidBodyComponent rb{};
                rb.motion = motion;
                registry.emplace<ecs::RigidBodyComponent>(entity, rb);
            }

            ensure_rigidbody_aux_components(registry, entity);
        }

        void apply_wheel_friction(
            entt::registry& registry,
            const ecs::Entity& entity,
            float friction)
        {
            if (!registry.all_of<ecs::RigidBodyComponent>(entity))
                return;

            auto* material = registry.try_get<ecs::PhysicsMaterialComponent>(entity);
            if (!material)
                material = &registry.emplace<ecs::PhysicsMaterialComponent>(entity);
            material->material.friction = friction;
        }

        void apply_mass_override(
            entt::registry& registry,
            const ecs::EntityRef& ref,
            float mass)
        {
            if (!ref.is_bound() || mass <= 0.0f)
                return;
            if (auto* rb = registry.try_get<ecs::RigidBodyComponent>(ref.entity))
            {
                rb->auto_mass = false;
                rb->mass = mass;
            }
        }

        AssetRef<assets::GpuModelAsset> resolve_model_ref(
            EngineContext& ctx,
            std::string_view name)
        {
            if (name.empty())
                return {};

            auto rm = eeng::try_get_resource_manager(ctx, "VehicleRig1Builder");
            if (!rm)
                return {};

            auto index = rm->get_index_data();
            if (!index)
                return {};

            const std::string type_id = meta::get_meta_type_id_string<assets::GpuModelAsset>();
            auto type_it = index->by_type.find(type_id);
            if (type_it == index->by_type.end())
                return {};

            for (const auto* entry : type_it->second)
            {
                if (entry && entry->meta.name == name)
                {
                    AssetRef<assets::GpuModelAsset> ref{ entry->meta.guid };
                    if (auto handle_opt = rm->handle_for_guid<assets::GpuModelAsset>(entry->meta.guid))
                        ref.bind(*handle_opt);
                    return ref;
                }
            }

            EENG_LOG_WARN(&ctx, "VehicleRig1Builder: Model asset '%.*s' not found.", static_cast<int>(name.size()), name.data());
            return {};
        }

        void ensure_model_component(
            entt::registry& registry,
            const ecs::Entity& entity,
            const std::string& name,
            const AssetRef<assets::GpuModelAsset>& model_ref)
        {
            if (!model_ref.guid.valid())
                return;
            if (registry.all_of<ecs::ModelComponent>(entity))
                return;

            registry.emplace<ecs::ModelComponent>(entity, name, model_ref);
        }
    } // namespace

    VehicleRig1Rig build_vehicle_rig1(EngineContext& ctx, const VehicleRig1Spec& spec)
    {
        VehicleRig1Rig rig{};

        auto* em = eeng::try_get_entity_manager_ptr(ctx, "VehicleRig1Builder");
        if (!em)
            return rig;

        ecs::Entity chassis_entity = eeng::meta::resolve_entity(*em, spec.chassis);
        if (!chassis_entity.has_id())
        {
            EENG_LOG_WARN(&ctx, "VehicleRig1Builder: Missing or unresolved chassis entity.");
            return rig;
        }

        auto& registry = em->registry();
        rig.chassis = em->get_entity_ref(chassis_entity);

        // If the chassis already has a rigid body, ensure the companion components exist
        // (matches editor bundle behavior for RBs).
        ensure_rigidbody_aux_components(registry, chassis_entity);

        const std::string prefix = spec.name_prefix.empty() ? "VehicleRig1" : spec.name_prefix;
        const std::string chunk_tag = spec.chunk_tag.empty() ? "vehicle_rig1" : spec.chunk_tag;

        ecs::Entity root_entity = eeng::meta::resolve_entity(*em, spec.root);
        if (root_entity.has_id() && !em->entity_valid(root_entity))
            root_entity = ecs::Entity::EntityNull;
        if (!root_entity.has_id())
        {
            ecs::Entity parent_entity = ecs::Entity::EntityNull;
            if (registry.all_of<ecs::HeaderComponent>(chassis_entity))
            {
                parent_entity = em->get_entity_parent(chassis_entity).entity;
            }
            else
            {
                EENG_LOG_WARN(&ctx, "VehicleRig1Builder: Chassis missing HeaderComponent; rig root will be unparented.");
            }

            const std::string root_name = prefix + "_Rig";
            rig.root = create_entity(*em, chunk_tag, root_name, parent_entity);
            root_entity = rig.root.entity;

            em->reparent_entity(chassis_entity, root_entity);
        }
        else
        {
            rig.root = em->get_entity_ref(root_entity);
        }

        // Ensure the rig root has a transform so the entire rig can be moved as a group later.
        // We keep it at identity here so existing local transforms remain valid.
        if (rig.root.is_bound())
            ensure_transform(registry, rig.root.entity, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

        const auto* chassis_tfm = registry.try_get<ecs::TransformComponent>(chassis_entity);
        const glm::vec3 chassis_pos = chassis_tfm ? chassis_tfm->position : glm::vec3(0.0f);
        const glm::quat chassis_rot = chassis_tfm ? chassis_tfm->rotation : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        if (!chassis_tfm)
        {
            EENG_LOG_WARN(&ctx, "VehicleRig1Builder: Chassis has no TransformComponent.");
        }

        const auto chassis_model_ref = resolve_model_ref(ctx, spec.chassis_model_name);
        const auto wheel_model_ref = resolve_model_ref(ctx, spec.wheel_model_name);
        const auto knuckle_model_ref = resolve_model_ref(ctx, spec.knuckle_model_name);

        if (chassis_model_ref.guid.valid())
            ensure_model_component(registry, chassis_entity, prefix + "_Chassis", chassis_model_ref);

        VehicleRig1RigComponent rig_component{};
        rig_component.chassis = rig.chassis;
        rig_component.steer_axis = normalize_or_default(spec.steer_axis, glm::vec3(0.0f, 1.0f, 0.0f));

        // VehicleRig1 rig topology (fixed for the prototype):
        //   chassis -> knuckle : 6DoF spring (suspension + steering)
        //   knuckle -> wheel   : hinge (drive)
        rig.wheels.reserve(spec.wheels.size());

        for (std::size_t i = 0; i < spec.wheels.size(); ++i)
        {
            const auto& wheel_spec = spec.wheels[i];
            VehicleRig1Rig::WheelRig wheel_rig{};

            const float wheel_friction = wheel_spec.friction_override
                ? wheel_spec.wheel_friction
                : spec.wheel_friction;
            const bool wheel_driven = wheel_spec.drive_override
                ? wheel_spec.driven
                : spec.drive_default;

            const glm::vec3 mount_local = wheel_spec.mount_override
                ? wheel_spec.mount_local
                : glm::vec3(
                    spec.chassis_half_extents.x * wheel_spec.mount_sign.x,
                    0.0f,
                    spec.chassis_half_extents.z * wheel_spec.mount_sign.y);

            // --- Axes and core geometry ---
            const glm::vec3 suspension_axis =
                normalize_or_default(wheel_spec.suspension_axis, glm::vec3(0.0f, -1.0f, 0.0f));
            const glm::vec3 axle_axis =
                normalize_or_default(wheel_spec.axle_axis, glm::vec3(0.0f, 0.0f, 1.0f));

            const float travel = std::max(0.0f, wheel_spec.suspension_travel);
            const float rest_length = wheel_spec.suspension_rest_length;

            // World-space placement for the prototype wheel/knuckle bodies.
            const glm::vec3 mount_world = chassis_pos + (chassis_rot * mount_local);
            const glm::vec3 axis_world = chassis_rot * suspension_axis;
            const glm::vec3 wheel_world = mount_world + axis_world * rest_length;

            // --- Knuckle (always created for VehicleRig1) ---
            {
                const std::string knuckle_name =
                    prefix + "_Knuckle_" + std::to_string(i);
                wheel_rig.knuckle = create_entity(*em, chunk_tag, knuckle_name, root_entity);

                // Dynamic knuckle, but trigger collider to avoid interfering with wheel contact.
                ensure_transform(registry, wheel_rig.knuckle.entity, wheel_world, chassis_rot);
                ensure_rigidbody(registry, wheel_rig.knuckle.entity, ecs::PhysicsMotionType::Dynamic);
                ensure_collider_sphere(registry, wheel_rig.knuckle.entity, wheel_spec.knuckle_radius, true);
                apply_mass_override(registry, wheel_rig.knuckle, wheel_spec.knuckle_mass);
                if (knuckle_model_ref.guid.valid())
                    ensure_model_component(registry, wheel_rig.knuckle.entity,
                        prefix + "_Knuckle", knuckle_model_ref);
            }

            // --- Wheel (reuse if provided, otherwise create a dynamic sphere) ---
            {
                ecs::Entity wheel_entity = eeng::meta::resolve_entity(*em, wheel_spec.wheel);
                if (!wheel_entity.has_id())
                {
                    const std::string wheel_name =
                        prefix + "_Wheel_" + std::to_string(i);
                    wheel_rig.wheel = create_entity(*em, chunk_tag, wheel_name, root_entity);
                    wheel_entity = wheel_rig.wheel.entity;

                    ensure_transform(registry, wheel_entity, wheel_world, chassis_rot);
                    ensure_rigidbody(registry, wheel_entity, ecs::PhysicsMotionType::Dynamic);
                    ensure_wheel_collider(registry, wheel_entity, wheel_spec, axle_axis);
                    apply_wheel_friction(registry, wheel_entity, wheel_friction);
                    apply_mass_override(registry, wheel_rig.wheel, wheel_spec.wheel_mass);
                    if (wheel_model_ref.guid.valid())
                        ensure_model_component(registry, wheel_entity, prefix + "_Wheel", wheel_model_ref);
                }
                else
                {
                    wheel_rig.wheel = em->get_entity_ref(wheel_entity);

                    if (!registry.all_of<ecs::TransformComponent>(wheel_entity))
                        EENG_LOG_WARN(&ctx, "VehicleRig1Builder: Wheel %zu missing TransformComponent.", i);
                    if (!registry.all_of<ecs::RigidBodyComponent>(wheel_entity))
                        EENG_LOG_WARN(&ctx, "VehicleRig1Builder: Wheel %zu missing RigidBodyComponent.", i);
                    if (!registry.all_of<ecs::ColliderComponent>(wheel_entity))
                        EENG_LOG_WARN(&ctx, "VehicleRig1Builder: Wheel %zu missing ColliderComponent.", i);

                    ensure_rigidbody_aux_components(registry, wheel_entity);
                    apply_wheel_friction(registry, wheel_entity, wheel_friction);

                    if (wheel_model_ref.guid.valid())
                        ensure_model_component(registry, wheel_entity, prefix + "_Wheel", wheel_model_ref);
                }
            }

            const ecs::EntityRef chassis_ref = rig.chassis;
            const ecs::EntityRef knuckle_ref = wheel_rig.knuckle;
            const ecs::EntityRef wheel_ref = wheel_rig.wheel;

            // --- Suspension + steering via 6DoF spring (chassis <-> knuckle) ---
            {
                const std::string name =
                    prefix + "_Suspension6Dof_" + std::to_string(i);
                wheel_rig.suspension_6dof = create_entity(*em, chunk_tag, name, root_entity);

                ecs::SixDofSpringConstraintComponent sixdof{};
                sixdof.entity_a = chassis_ref;
                sixdof.entity_b = knuckle_ref;
                sixdof.local_anchor_a = mount_local;
                sixdof.local_anchor_b = glm::vec3(0.0f);

                // Frame X = suspension axis, Y ~= axle axis (so Z completes the basis).
                const glm::quat frame_rot = make_constraint_frame(suspension_axis, axle_axis);
                sixdof.local_rotation_a = frame_rot;
                sixdof.local_rotation_b = frame_rot;

                // Linear limits in the constraint frame (X is along suspension travel).
                const glm::vec3 default_min(rest_length - travel * 0.5f, 0.0f, 0.0f);
                const glm::vec3 default_max(rest_length + travel * 0.5f, 0.0f, 0.0f);
                if (wheel_spec.use_linear_limits)
                {
                    sixdof.linear_limit_min = wheel_spec.linear_limit_min;
                    sixdof.linear_limit_max = wheel_spec.linear_limit_max;
                }
                else
                {
                    sixdof.linear_limit_min = default_min;
                    sixdof.linear_limit_max = default_max;
                }

                // Lock all angular axes by default (linear-only suspension),
                // but allow steering around the suspension axis when requested.
                sixdof.angular_limit_min = glm::vec3(0.0f);
                sixdof.angular_limit_max = glm::vec3(0.0f);
                if (wheel_spec.steerable)
                {
                    sixdof.angular_limit_min.x = -spec.steer_limit;
                    sixdof.angular_limit_max.x = spec.steer_limit;
                }

                // Spring/damper on the suspension axis only.
                sixdof.linear_stiffness = glm::vec3(wheel_spec.spring_k, 0.0f, 0.0f);
                sixdof.linear_damping = glm::vec3(wheel_spec.spring_d, 0.0f, 0.0f);
                sixdof.linear_equilibrium_enabled = wheel_spec.linear_equilibrium_enabled;
                sixdof.linear_equilibrium_target = wheel_spec.linear_equilibrium_target;
                if (sixdof.linear_equilibrium_enabled.x < 0.5f)
                {
                    const float min_x = std::min(sixdof.linear_limit_min.x, sixdof.linear_limit_max.x);
                    const float max_x = std::max(sixdof.linear_limit_min.x, sixdof.linear_limit_max.x);
                    sixdof.linear_equilibrium_enabled.x = 1.0f;
                    sixdof.linear_equilibrium_target.x = glm::clamp(rest_length, min_x, max_x);
                }
                sixdof.angular_stiffness = glm::vec3(0.0f);
                sixdof.angular_damping = glm::vec3(0.0f);
                sixdof.disable_collisions = spec.disable_collisions;
                sixdof.enabled = true;

                registry.emplace<ecs::SixDofSpringConstraintComponent>(
                    wheel_rig.suspension_6dof.entity, sixdof);
            }

            // --- Drive hinge (knuckle <-> wheel) ---
            {
                const std::string name =
                    prefix + "_AxleHinge_" + std::to_string(i);
                wheel_rig.axle_hinge = create_entity(*em, chunk_tag, name, root_entity);

                ecs::HingeConstraintComponent hinge{};
                hinge.entity_a = knuckle_ref;
                hinge.entity_b = wheel_ref;
                hinge.local_anchor_a = glm::vec3(0.0f);
                hinge.local_anchor_b = wheel_spec.wheel_local_anchor;
                hinge.local_axis_a = axle_axis;
                hinge.local_axis_b = axle_axis;
                // Axle should spin freely; do not enable limits.
                hinge.use_limits = false;
                // Motor parameters are set by the control system.
                hinge.enable_motor = false;
                hinge.motor_target_velocity = 0.0f;
                hinge.motor_max_impulse = 0.0f;
                hinge.disable_collisions = spec.disable_collisions;
                registry.emplace<ecs::HingeConstraintComponent>(
                    wheel_rig.axle_hinge.entity, hinge);
            }

            rig.wheels.push_back(wheel_rig);

            VehicleRig1WheelLink link{};
            link.knuckle = wheel_rig.knuckle;
            link.wheel = wheel_rig.wheel;
            link.suspension_6dof = wheel_rig.suspension_6dof;
            link.axle_hinge = wheel_rig.axle_hinge;
            link.steerable = wheel_spec.steerable;
            link.driven = wheel_driven;
            link.drive_direction = wheel_spec.drive_direction;
            link.steer_direction = wheel_spec.steer_direction;
            link.mount_local = mount_local;
            link.suspension_axis = suspension_axis;
            link.axle_axis = axle_axis;
            link.wheel_local_anchor = wheel_spec.wheel_local_anchor;
            link.suspension_rest_length = wheel_spec.suspension_rest_length;
            link.suspension_travel = wheel_spec.suspension_travel;
            rig_component.wheels.push_back(link);
        }

        if (rig.root.is_bound())
            registry.emplace_or_replace<ecs::VehicleRig1RigComponent>(rig.root.entity, rig_component);

        return rig;
    }

    nlohmann::json build_vehicle_rig1_prefab_json(
        EngineContext& ctx,
        const VehicleRig1Spec& rig_spec,
        const VehicleRig1ChassisSpec& chassis_spec)
    {
        // Build the rig in a scratch world so we can serialize it as a prefab.
        EngineContext scratch_ctx(
            std::make_unique<EntityManager>(),
            ctx.resource_manager,
            std::unique_ptr<eeng::IBatchRegistry>{},
            std::unique_ptr<eeng::IGuiManager>{},
            std::unique_ptr<eeng::IInputManager>{},
            ctx.log_manager);
        scratch_ctx.project_config = ctx.project_config;

        auto& em = static_cast<EntityManager&>(*scratch_ctx.entity_manager);
        auto& registry = em.registry();

        const std::string prefix = rig_spec.name_prefix.empty() ? "VehicleRig1" : rig_spec.name_prefix;
        const std::string chunk_tag = rig_spec.chunk_tag.empty() ? "vehicle_rig1" : rig_spec.chunk_tag;

        // --- Chassis (created in scratch registry) -------------------------
        const auto [chassis_guid, chassis_entity] = em.create_entity_live_parent(
            chunk_tag,
            prefix + "_Chassis",
            ecs::Entity::EntityNull,
            ecs::Entity::EntityNull);
        (void)chassis_guid;

        auto& chassis_tfm = registry.emplace<ecs::TransformComponent>(chassis_entity);
        chassis_tfm.set_position(chassis_spec.position);
        chassis_tfm.set_rotation(chassis_spec.rotation);

        auto& chassis_rb = registry.emplace<ecs::RigidBodyComponent>(chassis_entity);
        chassis_rb.motion = ecs::PhysicsMotionType::Dynamic;
        chassis_rb.linear_damping = chassis_spec.linear_damping;
        chassis_rb.angular_damping = chassis_spec.angular_damping;
        chassis_rb.auto_mass = chassis_spec.auto_mass;
        chassis_rb.mass = chassis_spec.mass;

        ensure_rigidbody_aux_components(registry, chassis_entity);

        ecs::ColliderComponent chassis_colliders{};
        ecs::ColliderDesc chassis_box{};
        chassis_box.type = ecs::ColliderType::Box;
        chassis_box.half_extents = chassis_spec.half_extents;
        chassis_colliders.colliders.push_back(chassis_box);
        registry.emplace<ecs::ColliderComponent>(chassis_entity, std::move(chassis_colliders));

        // --- Build rig ------------------------------------------------------
        VehicleRig1Spec spec = rig_spec;
        spec.chassis = em.get_entity_ref(chassis_entity);
        spec.root = {};

        const auto rig = build_vehicle_rig1(scratch_ctx, spec);
        if (!rig.root.is_bound())
            return nlohmann::json{};

        auto registry_sp = em.registry_wptr().lock();
        if (!registry_sp)
            return nlohmann::json{};

        auto& scenegraph = em.scene_graph();
        const auto branch = scenegraph.get_branch_topdown(rig.root.entity);

        nlohmann::json branch_json = nlohmann::json::array();
        for (const auto& entity : branch)
        {
            branch_json.push_back(meta::serialize_entity_for_file(
                em.get_entity_ref(entity),
                registry_sp));
        }

        return branch_json;
    }
} // namespace eeng::ecs
