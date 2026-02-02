// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/SpringDamperSystem.hpp"

#include "ecs/PhysicsComponents.hpp"
#include "ecs/systems/PhysicsSystem.hpp"
#include "ecs/TransformComponent.hpp"
#include "physics/Forces.hpp"

#include <glm/glm.hpp>

namespace eeng::ecs::systems
{
    void SpringDamperSystem::update(
        entt::registry& registry,
        EngineContext&,
        PhysicsSystem& physics_system,
        float)
    {
        auto view = registry.view<ecs::SpringDamperComponent>();
        for (const auto entity : view)
        {
            auto& spring = view.get<ecs::SpringDamperComponent>(entity);

            if (!spring.enabled)
                continue;

            const bool linear_active =
                (spring.linear_stiffness != 0.0f || spring.linear_damping != 0.0f);
            const bool angular_active =
                spring.enable_angular && (spring.angular_stiffness != 0.0f || spring.angular_damping != 0.0f);

            if (!linear_active && !angular_active)
                continue;

            struct AnchorInfo
            {
                bool valid = false;
                bool has_body = false;
                entt::entity entity = entt::null;
                PhysicsSystem::BodyState state{};
                glm::vec3 anchor_world{ 0.0f };
                glm::vec3 velocity{ 0.0f };
            };

            // Resolve an anchor bound to a rigid body into world space + velocity.
            auto resolve_body_anchor = [&](entt::entity body_entity,
                                           const glm::vec3& local_anchor,
                                           ecs::SpringAnchorSpace anchor_space) -> AnchorInfo
            {
                AnchorInfo info{};
                info.entity = body_entity;

                if (!registry.valid(body_entity))
                    return info;

                const auto* tfm = registry.try_get<ecs::TransformComponent>(body_entity);
                if (!tfm)
                    return info;

                info.has_body = physics_system.get_body_state(body_entity, info.state);

                // If no rigid body exists, treat the anchor as a static point from the transform.
                if (info.has_body && anchor_space == ecs::SpringAnchorSpace::Body)
                    info.anchor_world = info.state.position + (info.state.rotation * (local_anchor * tfm->scale));
                else
                    info.anchor_world = glm::vec3(tfm->world_matrix * glm::vec4(local_anchor, 1.0f));

                if (info.has_body)
                {
                    const glm::vec3 rel = info.anchor_world - info.state.position;
                    info.velocity = info.state.linear_velocity + glm::cross(info.state.angular_velocity, rel);
                }
                info.valid = true;
                return info;
            };

            // Resolve an entity anchor into world space + velocity.
            auto resolve_anchor = [&](const ecs::EntityRef& entity_ref,
                                      const glm::vec3& local_anchor,
                                      ecs::SpringAnchorSpace anchor_space) -> AnchorInfo
            {
                if (!entity_ref.is_bound())
                    return {};
                const entt::entity body_entity = static_cast<entt::entity>(entity_ref.entity);
                return resolve_body_anchor(body_entity, local_anchor, anchor_space);
            };

            const AnchorInfo anchor_a = resolve_anchor(
                spring.entity_a,
                spring.local_anchor_a,
                spring.anchor_space_a);
            const AnchorInfo anchor_b = resolve_anchor(
                spring.entity_b,
                spring.local_anchor_b,
                spring.anchor_space_b);

            if (!anchor_a.valid || !anchor_b.valid)
                continue;

            // Self-self or same-body anchors should not inject internal forces.
            const bool same_body = anchor_a.has_body && anchor_b.has_body && anchor_a.entity == anchor_b.entity;

            if (linear_active && (anchor_a.has_body || anchor_b.has_body) && !same_body)
            {
                physics::LinearSpringDamper linear{
                    spring.linear_stiffness,
                    spring.linear_damping,
                    spring.rest_length
                };
                const glm::vec3 force =
                    linear.compute_force(anchor_a.anchor_world, anchor_b.anchor_world, anchor_a.velocity, anchor_b.velocity);
                if (glm::dot(force, force) > 0.0f)
                {
                    if (anchor_a.has_body)
                        physics_system.submit_force(anchor_a.entity, -force, anchor_a.anchor_world);
                    if (anchor_b.has_body)
                        physics_system.submit_force(anchor_b.entity, force, anchor_b.anchor_world);
                }
            }

            if (angular_active)
            {
                // Body-body: restore relative rotation between anchors.
                if (anchor_a.has_body && anchor_b.has_body && !same_body)
                {
                    physics::AngularSpringDamper angular{
                        spring.angular_stiffness,
                        spring.angular_damping,
                        spring.rest_rotation
                    };
                    const glm::vec3 torque = angular.compute_torque(
                        anchor_a.state.rotation,
                        anchor_b.state.rotation,
                        anchor_a.state.angular_velocity,
                        anchor_b.state.angular_velocity);
                    if (glm::dot(torque, torque) > 0.0f)
                    {
                        physics_system.submit_torque(anchor_a.entity, torque);
                        physics_system.submit_torque(anchor_b.entity, -torque);
                    }
                }
                // Body-world: drive the lone body anchor toward rest_rotation in world.
                else if (anchor_a.has_body != anchor_b.has_body)
                {
                    const AnchorInfo& body_anchor = anchor_a.has_body ? anchor_a : anchor_b;
                    physics::AngularSpringDamper angular{
                        spring.angular_stiffness,
                        spring.angular_damping,
                        glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
                    };
                    const glm::vec3 torque = angular.compute_torque(
                        body_anchor.state.rotation,
                        spring.rest_rotation,
                        body_anchor.state.angular_velocity,
                        glm::vec3(0.0f));
                    if (glm::dot(torque, torque) > 0.0f)
                        physics_system.submit_torque(body_anchor.entity, torque);
                }
            }
        }
    }
} // namespace eeng::ecs::systems
