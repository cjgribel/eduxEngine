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
        auto view = registry.view<ecs::TransformComponent, ecs::SpringDamperComponent>();
        for (const auto entity : view)
        {
            auto& tfm_a = view.get<ecs::TransformComponent>(entity);
            auto& spring = view.get<ecs::SpringDamperComponent>(entity);

            if (!spring.enabled)
                continue;

            const bool linear_active =
                (spring.linear_stiffness != 0.0f || spring.linear_damping != 0.0f);
            const bool angular_active =
                spring.enable_angular && (spring.angular_stiffness != 0.0f || spring.angular_damping != 0.0f);

            if (!linear_active && !angular_active)
                continue;

            PhysicsSystem::BodyState state_a{};
            if (!physics_system.get_body_state(entity, state_a))
                continue;

            entt::entity entity_b = entt::null;
            PhysicsSystem::BodyState state_b{};
            if (!spring.use_world_point_b)
            {
                if (!spring.entity_b.is_bound())
                    continue;

                entity_b = static_cast<entt::entity>(spring.entity_b.entity);
                if (!registry.valid(entity_b))
                    continue;

                if (!physics_system.get_body_state(entity_b, state_b))
                    continue;
            }

            const glm::vec3 anchor_a =
                glm::vec3(tfm_a.world_matrix * glm::vec4(spring.local_anchor_a, 1.0f));
            glm::vec3 anchor_b{};
            if (spring.use_world_point_b)
            {
                anchor_b = spring.world_point_b;
            }
            else
            {
                const auto& tfm_b = registry.get<ecs::TransformComponent>(entity_b);
                anchor_b = glm::vec3(tfm_b.world_matrix * glm::vec4(spring.local_anchor_b, 1.0f));
            }

            const glm::vec3 rel_a = anchor_a - state_a.position;
            const glm::vec3 vel_a = state_a.linear_velocity + glm::cross(state_a.angular_velocity, rel_a);

            glm::vec3 vel_b(0.0f);
            if (!spring.use_world_point_b)
            {
                const glm::vec3 rel_b = anchor_b - state_b.position;
                vel_b = state_b.linear_velocity + glm::cross(state_b.angular_velocity, rel_b);
            }

            if (linear_active)
            {
                physics::LinearSpringDamper linear{
                    spring.linear_stiffness,
                    spring.linear_damping,
                    spring.rest_length
                };
                const glm::vec3 force = linear.compute_force(anchor_a, anchor_b, vel_a, vel_b);
                if (glm::dot(force, force) > 0.0f)
                {
                    physics_system.submit_force(entity, -force, anchor_a);
                    if (!spring.use_world_point_b)
                        physics_system.submit_force(entity_b, force, anchor_b);
                }
            }

            if (angular_active && !spring.use_world_point_b)
            {
                physics::AngularSpringDamper angular{
                    spring.angular_stiffness,
                    spring.angular_damping,
                    spring.rest_rotation
                };
                const glm::vec3 torque = angular.compute_torque(
                    state_a.rotation,
                    state_b.rotation,
                    state_a.angular_velocity,
                    state_b.angular_velocity);
                if (glm::dot(torque, torque) > 0.0f)
                {
                    physics_system.submit_torque(entity, torque);
                    physics_system.submit_torque(entity_b, -torque);
                }
            }
        }
    }
} // namespace eeng::ecs::systems
