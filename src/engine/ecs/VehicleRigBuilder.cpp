// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/VehicleRigBuilder.hpp"

#include "LogMacros.h"
#include "engineapi/EngineContextHelpers.hpp"
#include "ecs/EntityManager.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/ModelComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"
#include "meta/MetaAux.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace eeng::ecs
{
    namespace
    {
        glm::vec3 normalize_or_default(const glm::vec3& v, const glm::vec3& fallback)
        {
            const float len2 = glm::dot(v, v);
            if (len2 <= 1e-8f)
                return fallback;
            return v / std::sqrt(len2);
        }

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

        float signed_angle_around_axis(
            const glm::quat& from,
            const glm::quat& to,
            const glm::vec3& axis)
        {
            const glm::quat rel = glm::normalize(glm::inverse(from) * to);
            glm::vec3 rel_axis(rel.x, rel.y, rel.z);
            const float sin_half = glm::length(rel_axis);
            if (sin_half <= 1e-6f)
                return 0.0f;

            rel_axis /= sin_half;
            float angle = 2.0f * std::atan2(sin_half, rel.w);
            if (angle > glm::pi<float>())
                angle -= 2.0f * glm::pi<float>();

            const float sign = (glm::dot(rel_axis, axis) >= 0.0f) ? 1.0f : -1.0f;
            return angle * sign;
        }

        ecs::Entity resolve_entity_ref(EntityManager& em, const ecs::EntityRef& ref)
        {
            if (ref.is_bound())
                return ref.entity;
            if (ref.guid.valid())
            {
                auto ent_opt = em.get_entity_from_guid(ref.guid);
                if (ent_opt)
                    return *ent_opt;
            }
            return ecs::Entity::EntityNull;
        }

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

        void ensure_rigidbody(
            entt::registry& registry,
            const ecs::Entity& entity,
            ecs::PhysicsMotionType motion)
        {
            if (registry.all_of<ecs::RigidBodyComponent>(entity))
                return;

            ecs::RigidBodyComponent rb{};
            rb.motion = motion;
            registry.emplace<ecs::RigidBodyComponent>(entity, rb);
        }

        AssetRef<assets::GpuModelAsset> resolve_model_ref(
            EngineContext& ctx,
            std::string_view name)
        {
            if (name.empty())
                return {};

            auto rm = eeng::try_get_resource_manager(ctx, "VehicleRigBuilder");
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

            EENG_LOG_WARN(&ctx, "VehicleRigBuilder: Model asset '%.*s' not found.", static_cast<int>(name.size()), name.data());
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

    VehicleRig build_vehicle_rig(EngineContext& ctx, const VehicleSpec& spec)
    {
        VehicleRig rig{};

        auto* em = eeng::try_get_entity_manager_ptr(ctx, "VehicleRigBuilder");
        if (!em)
            return rig;

        ecs::Entity chassis_entity = resolve_entity_ref(*em, spec.chassis);
        if (!chassis_entity.has_id())
        {
            EENG_LOG_WARN(&ctx, "VehicleRigBuilder: Missing or unresolved chassis entity.");
            return rig;
        }

        auto& registry = em->registry();
        rig.chassis = em->get_entity_ref(chassis_entity);

        const std::string prefix = spec.name_prefix.empty() ? "Vehicle" : spec.name_prefix;
        const std::string chunk_tag = spec.chunk_tag.empty() ? "vehicle_rig" : spec.chunk_tag;

        ecs::Entity root_entity = resolve_entity_ref(*em, spec.root);
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
                EENG_LOG_WARN(&ctx, "VehicleRigBuilder: Chassis missing HeaderComponent; rig root will be unparented.");
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

        const auto* chassis_tfm = registry.try_get<ecs::TransformComponent>(chassis_entity);
        const glm::vec3 chassis_pos = chassis_tfm ? chassis_tfm->position : glm::vec3(0.0f);
        const glm::quat chassis_rot = chassis_tfm ? chassis_tfm->rotation : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        if (!chassis_tfm)
        {
            EENG_LOG_WARN(&ctx, "VehicleRigBuilder: Chassis has no TransformComponent.");
        }

        const auto chassis_model_ref = resolve_model_ref(ctx, spec.chassis_model_name);
        const auto wheel_model_ref = resolve_model_ref(ctx, spec.wheel_model_name);
        const auto knuckle_model_ref = resolve_model_ref(ctx, spec.knuckle_model_name);

        if (chassis_model_ref.guid.valid())
            ensure_model_component(registry, chassis_entity, prefix + "_Chassis", chassis_model_ref);

        VehicleRigComponent rig_component{};
        rig_component.chassis = rig.chassis;
        rig_component.kinematic_knuckle = spec.kinematic_knuckle;
        rig_component.steer_axis = normalize_or_default(spec.steer_axis, glm::vec3(0.0f, 1.0f, 0.0f));

        rig.wheels.reserve(spec.wheels.size());

        for (std::size_t i = 0; i < spec.wheels.size(); ++i)
        {
            const auto& wheel_spec = spec.wheels[i];
            VehicleRig::WheelRig wheel_rig{};

            const glm::vec3 suspension_axis =
                normalize_or_default(wheel_spec.suspension_axis, glm::vec3(0.0f, -1.0f, 0.0f));
            const glm::vec3 axle_axis =
                normalize_or_default(wheel_spec.axle_axis, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::vec3 steer_axis =
                normalize_or_default(spec.steer_axis, glm::vec3(0.0f, 1.0f, 0.0f));

            const float travel = std::max(0.0f, wheel_spec.suspension_travel);
            const float rest_length = wheel_spec.suspension_rest_length;

            const glm::vec3 mount_world = chassis_pos + (chassis_rot * wheel_spec.mount_local);
            const glm::vec3 axis_world = chassis_rot * suspension_axis;
            const glm::vec3 wheel_world = mount_world + axis_world * rest_length;

            bool use_knuckle = spec.use_knuckle;
            if (spec.use_split_suspension_constraints && !use_knuckle)
            {
                use_knuckle = true;
                EENG_LOG_WARN(&ctx,
                    "VehicleRigBuilder: enabling knuckle for split suspension constraints (wheel %zu).", i);
            }
            const bool knuckle_is_kinematic = spec.kinematic_knuckle && use_knuckle;

            // --- Knuckle (optional) ---
            if (use_knuckle)
            {
                const std::string knuckle_name =
                    prefix + "_Knuckle_" + std::to_string(i);
                wheel_rig.knuckle = create_entity(*em, chunk_tag, knuckle_name, root_entity);

                ensure_transform(registry, wheel_rig.knuckle.entity, wheel_world, chassis_rot);
                ensure_rigidbody(registry, wheel_rig.knuckle.entity,
                    knuckle_is_kinematic ? ecs::PhysicsMotionType::Kinematic : ecs::PhysicsMotionType::Dynamic);
                const bool knuckle_trigger = spec.use_split_suspension_constraints || knuckle_is_kinematic;
                ensure_collider_sphere(registry, wheel_rig.knuckle.entity,
                    wheel_spec.knuckle_radius, knuckle_trigger);
                if (knuckle_model_ref.guid.valid())
                    ensure_model_component(registry, wheel_rig.knuckle.entity,
                        prefix + "_Knuckle", knuckle_model_ref);
            }

            // --- Wheel ---
            {
                ecs::Entity wheel_entity = resolve_entity_ref(*em, wheel_spec.wheel);
                if (!wheel_entity.has_id())
                {
                    const std::string wheel_name =
                        prefix + "_Wheel_" + std::to_string(i);
                    wheel_rig.wheel = create_entity(*em, chunk_tag, wheel_name, root_entity);
                    wheel_entity = wheel_rig.wheel.entity;

                    ensure_transform(registry, wheel_entity, wheel_world, chassis_rot);
                    ensure_rigidbody(registry, wheel_entity, ecs::PhysicsMotionType::Dynamic);
                    ensure_collider_sphere(registry, wheel_entity, wheel_spec.wheel_radius, false);
                    if (wheel_model_ref.guid.valid())
                        ensure_model_component(registry, wheel_entity, prefix + "_Wheel", wheel_model_ref);
                }
                else
                {
                    wheel_rig.wheel = em->get_entity_ref(wheel_entity);

                    if (!registry.all_of<ecs::TransformComponent>(wheel_entity))
                        EENG_LOG_WARN(&ctx, "VehicleRigBuilder: Wheel %zu missing TransformComponent.", i);
                    if (!registry.all_of<ecs::RigidBodyComponent>(wheel_entity))
                        EENG_LOG_WARN(&ctx, "VehicleRigBuilder: Wheel %zu missing RigidBodyComponent.", i);
                    if (!registry.all_of<ecs::ColliderComponent>(wheel_entity))
                        EENG_LOG_WARN(&ctx, "VehicleRigBuilder: Wheel %zu missing ColliderComponent.", i);

                    if (wheel_model_ref.guid.valid())
                        ensure_model_component(registry, wheel_entity, prefix + "_Wheel", wheel_model_ref);
                }
            }

            const ecs::EntityRef chassis_ref = rig.chassis;
            const ecs::EntityRef knuckle_ref = wheel_rig.knuckle;
            const ecs::EntityRef wheel_ref = wheel_rig.wheel;

            // Suspension target: wheel when there is no knuckle or knuckle is kinematic.
            // Kinematic bodies ignore forces, so we avoid using them as the spring target.
            const bool suspension_targets_wheel = !use_knuckle || knuckle_is_kinematic;
            const ecs::EntityRef suspension_ref =
                suspension_targets_wheel ? wheel_ref : knuckle_ref;
            const glm::vec3 suspension_anchor_b =
                suspension_targets_wheel ? wheel_spec.wheel_local_anchor : glm::vec3(0.0f);

            if (use_knuckle)
            {
                if (spec.use_split_suspension_constraints)
                {
                    // --- Suspension 6DoF (linear-only) ---
                    const std::string name =
                        prefix + "_Suspension6Dof_" + std::to_string(i);
                    wheel_rig.suspension_6dof = create_entity(*em, chunk_tag, name, root_entity);

                    ecs::SixDofSpringConstraintComponent sixdof{};
                    sixdof.entity_a = chassis_ref;
                    sixdof.entity_b = suspension_ref;
                    sixdof.local_anchor_a = wheel_spec.mount_local + suspension_axis * rest_length;
                    sixdof.local_anchor_b = suspension_anchor_b;

                    const glm::quat frame_rot = make_constraint_frame(suspension_axis, axle_axis);
                    sixdof.local_rotation_a = frame_rot;
                    sixdof.local_rotation_b = frame_rot;

                    sixdof.linear_limit_min = glm::vec3(-travel * 0.5f, 0.0f, 0.0f);
                    sixdof.linear_limit_max = glm::vec3(travel * 0.5f, 0.0f, 0.0f);
                    // Lock all angular axes by default (linear-only suspension).
                    sixdof.angular_limit_min = glm::vec3(0.0f);
                    sixdof.angular_limit_max = glm::vec3(0.0f);
                    // For steerable wheels, allow steering around the suspension axis (X).
                    if (wheel_spec.steerable)
                    {
                        sixdof.angular_limit_min.x = -spec.steer_limit;
                        sixdof.angular_limit_max.x = spec.steer_limit;
                    }

                    sixdof.linear_stiffness = glm::vec3(0.0f);
                    sixdof.linear_damping = glm::vec3(0.0f);
                    sixdof.angular_stiffness = glm::vec3(0.0f);
                    sixdof.angular_damping = glm::vec3(0.0f);
                    sixdof.disable_collisions = spec.disable_collisions;
                    sixdof.enabled = !knuckle_is_kinematic;

                    registry.emplace<ecs::SixDofSpringConstraintComponent>(
                        wheel_rig.suspension_6dof.entity, sixdof);
                }
                else
                {
                    // --- Suspension slider ---
                    const std::string name =
                        prefix + "_SuspensionSlider_" + std::to_string(i);
                    wheel_rig.suspension_slider = create_entity(*em, chunk_tag, name, root_entity);

                    ecs::SliderConstraintComponent slider{};
                    slider.entity_a = chassis_ref;
                    slider.entity_b = suspension_ref;
                    slider.local_anchor_a = wheel_spec.mount_local + suspension_axis * rest_length;
                    slider.local_anchor_b = suspension_anchor_b;
                    slider.local_axis_a = suspension_axis;
                    slider.local_axis_b = suspension_axis;
                    slider.linear_limit_min = -travel * 0.5f;
                    slider.linear_limit_max = travel * 0.5f;
                    slider.angular_limit_min = 0.0f;
                    slider.angular_limit_max = 0.0f;
                    slider.disable_collisions = spec.disable_collisions;
                    slider.enabled = !knuckle_is_kinematic;
                    registry.emplace<ecs::SliderConstraintComponent>(
                        wheel_rig.suspension_slider.entity, slider);
                }

                // --- Suspension spring ---
                {
                    const std::string name =
                        prefix + "_SuspensionSpring_" + std::to_string(i);
                    wheel_rig.suspension_spring = create_entity(*em, chunk_tag, name, root_entity);

                    ecs::SpringDamperComponent spring{};
                    spring.entity_a = chassis_ref;
                    spring.entity_b = suspension_ref;
                    spring.local_anchor_a = wheel_spec.mount_local;
                    spring.local_anchor_b = suspension_anchor_b;
                    spring.anchor_space_a = ecs::SpringAnchorSpace::Transform;
                    spring.anchor_space_b = ecs::SpringAnchorSpace::Transform;
                    spring.linear_stiffness = wheel_spec.spring_k;
                    spring.linear_damping = wheel_spec.spring_d;
                    spring.rest_length = std::abs(rest_length);
                    spring.enabled = !knuckle_is_kinematic;
                    registry.emplace<ecs::SpringDamperComponent>(
                        wheel_rig.suspension_spring.entity, spring);
                }

                // --- Steering hinge (chassis <-> knuckle) ---
                if (!spec.use_split_suspension_constraints
                    && wheel_spec.steerable && !knuckle_is_kinematic)
                {
                    const std::string name =
                        prefix + "_SteeringHinge_" + std::to_string(i);
                    wheel_rig.steering_hinge = create_entity(*em, chunk_tag, name, root_entity);

                    ecs::HingeConstraintComponent hinge{};
                    hinge.entity_a = chassis_ref;
                    hinge.entity_b = knuckle_ref;
                    hinge.local_anchor_a = wheel_spec.mount_local + suspension_axis * rest_length;
                    hinge.local_anchor_b = glm::vec3(0.0f);
                    hinge.local_axis_a = steer_axis;
                    hinge.local_axis_b = steer_axis;
                    if (spec.steer_limit > 0.0f)
                    {
                        hinge.use_limits = true;
                        hinge.limit_min = -spec.steer_limit;
                        hinge.limit_max = spec.steer_limit;
                    }
                    hinge.enable_motor = true;
                    hinge.motor_target_velocity = spec.steer_motor_target_velocity;
                    hinge.motor_max_impulse = spec.steer_motor_max_impulse;
                    hinge.disable_collisions = spec.disable_collisions;
                    registry.emplace<ecs::HingeConstraintComponent>(
                        wheel_rig.steering_hinge.entity, hinge);
                }

                // --- Axle hinge ---
                {
                    const std::string name =
                        prefix + "_AxleHinge_" + std::to_string(i);
                    wheel_rig.axle_hinge = create_entity(*em, chunk_tag, name, root_entity);

                    ecs::HingeConstraintComponent hinge{};
                    if (knuckle_is_kinematic)
                    {
                        hinge.entity_a = chassis_ref;
                        hinge.entity_b = wheel_ref;
                        hinge.local_anchor_a = wheel_spec.mount_local + suspension_axis * rest_length;
                    }
                    else
                    {
                        hinge.entity_a = knuckle_ref;
                        hinge.entity_b = wheel_ref;
                        hinge.local_anchor_a = glm::vec3(0.0f);
                    }
                    hinge.local_anchor_b = wheel_spec.wheel_local_anchor;
                    hinge.local_axis_a = axle_axis;
                    hinge.local_axis_b = axle_axis;
                    hinge.enable_motor = false;
                    hinge.motor_target_velocity = 0.0f;
                    hinge.motor_max_impulse = 0.0f;
                    hinge.disable_collisions = spec.disable_collisions;
                    registry.emplace<ecs::HingeConstraintComponent>(
                        wheel_rig.axle_hinge.entity, hinge);
                }
            }
            else
            {
                // --- Suspension + steering + drive via 6DoF spring constraint ---
                const std::string name =
                    prefix + "_Suspension6Dof_" + std::to_string(i);
                wheel_rig.suspension_6dof = create_entity(*em, chunk_tag, name, root_entity);

                ecs::SixDofSpringConstraintComponent sixdof{};
                sixdof.entity_a = chassis_ref;
                sixdof.entity_b = wheel_ref;
                sixdof.local_anchor_a = wheel_spec.mount_local;
                sixdof.local_anchor_b = wheel_spec.wheel_local_anchor;

                const glm::quat frame_rot = make_constraint_frame(suspension_axis, axle_axis);
                sixdof.local_rotation_a = frame_rot;
                sixdof.local_rotation_b = frame_rot;

                const glm::vec3 default_min(rest_length - travel * 0.5f, 0.0f, 0.0f);
                const glm::vec3 default_max(rest_length + travel * 0.5f, 0.0f, 0.0f);
                if (wheel_spec.sixdof_use_linear_limits)
                {
                    sixdof.linear_limit_min = wheel_spec.sixdof_linear_limit_min;
                    sixdof.linear_limit_max = wheel_spec.sixdof_linear_limit_max;
                }
                else
                {
                    sixdof.linear_limit_min = default_min;
                    sixdof.linear_limit_max = default_max;
                }

                if (wheel_spec.sixdof_use_angular_limits)
                {
                    sixdof.angular_limit_min = wheel_spec.sixdof_angular_limit_min;
                    sixdof.angular_limit_max = wheel_spec.sixdof_angular_limit_max;
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        if (wheel_spec.sixdof_free_angular_axes[axis] > 0.5f)
                        {
                            // Bullet treats lower > upper as a free axis.
                            sixdof.angular_limit_min[axis] = 1.0f;
                            sixdof.angular_limit_max[axis] = -1.0f;
                        }
                    }
                }
                else
                {
                    // Lock angular motion by default; only allow translation along suspension axis.
                    sixdof.angular_limit_min = glm::vec3(0.0f);
                    sixdof.angular_limit_max = glm::vec3(0.0f);
                }

                {
                    const glm::mat3 basis = glm::mat3_cast(frame_rot);
                    const glm::vec3 frame_y = basis[1];
                    const glm::vec3 axle_axis_world = normalize_or_default(wheel_spec.axle_axis, glm::vec3(0.0f, 0.0f, 1.0f));
                    const float align = std::abs(glm::dot(frame_y, axle_axis_world));
                    if (align < 0.9f)
                    {
                        EENG_LOG_WARN(&ctx, "6DoF wheel axis misalignment: |dot(frameY, axle)| = %.3f", align);
                    }
                }

                sixdof.linear_stiffness = glm::vec3(wheel_spec.spring_k, 0.0f, 0.0f);
                sixdof.linear_damping = glm::vec3(wheel_spec.spring_d, 0.0f, 0.0f);
                sixdof.linear_equilibrium_enabled = wheel_spec.sixdof_linear_equilibrium_enabled;
                sixdof.linear_equilibrium_target = wheel_spec.sixdof_linear_equilibrium_target;
                sixdof.angular_stiffness = glm::vec3(0.0f);
                sixdof.angular_damping = glm::vec3(0.0f);
                sixdof.disable_collisions = false;
                registry.emplace<ecs::SixDofSpringConstraintComponent>(
                    wheel_rig.suspension_6dof.entity, sixdof);
            }

            rig.wheels.push_back(wheel_rig);

            VehicleWheelLink link{};
            link.knuckle = wheel_rig.knuckle;
            link.wheel = wheel_rig.wheel;
            link.suspension_slider = wheel_rig.suspension_slider;
            link.suspension_spring = wheel_rig.suspension_spring;
            link.suspension_6dof = wheel_rig.suspension_6dof;
            link.steering_hinge = wheel_rig.steering_hinge;
            link.axle_hinge = wheel_rig.axle_hinge;
            link.steerable = wheel_spec.steerable;
            link.driven = wheel_spec.driven;
            link.drive_direction = wheel_spec.drive_direction;
            link.steer_direction = wheel_spec.steer_direction;
            link.mount_local = wheel_spec.mount_local;
            link.suspension_axis = suspension_axis;
            link.axle_axis = axle_axis;
            link.wheel_local_anchor = wheel_spec.wheel_local_anchor;
            link.suspension_rest_length = wheel_spec.suspension_rest_length;
            link.suspension_travel = wheel_spec.suspension_travel;
            link.steer_neutral_angle = 0.0f;
            if (use_knuckle && wheel_rig.knuckle.is_bound())
            {
                if (const auto* knuckle_tfm = registry.try_get<ecs::TransformComponent>(wheel_rig.knuckle.entity))
                {
                    const glm::vec3 steer_axis_world =
                        normalize_or_default(chassis_rot * steer_axis, glm::vec3(0.0f, 1.0f, 0.0f));
                    link.steer_neutral_angle = signed_angle_around_axis(
                        chassis_rot,
                        knuckle_tfm->rotation,
                        steer_axis_world);
                }
            }
            rig_component.wheels.push_back(link);
        }

        if (rig.root.is_bound())
            registry.emplace_or_replace<ecs::VehicleRigComponent>(rig.root.entity, rig_component);

        return rig;
    }
} // namespace eeng::ecs
