// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/ConstraintSystem.hpp"

#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"
#include "ecs/systems/PhysicsSystem.hpp"

#include <unordered_set>

namespace eeng::ecs::systems
{
    void ConstraintSystem::update(
        entt::registry& registry,
        EngineContext&,
        PhysicsSystem& physics_system,
        float)
    {
        std::unordered_set<ConstraintKey, ConstraintKeyHash> seen;
        auto track_handle = [&](ConstraintKey key, PhysicsSystem::ConstraintHandle handle)
        {
            if (handle == 0)
                return;
            handles_[key] = handle;
            seen.insert(key);
        };

        // --- Point constraints ---
        {
            auto view = registry.view<ecs::TransformComponent, ecs::PointConstraintComponent>();
            for (const auto entity : view)
            {
                auto& tfm_a = view.get<ecs::TransformComponent>(entity);
                auto& comp = view.get<ecs::PointConstraintComponent>(entity);
                if (!comp.enabled)
                    continue;

                PhysicsSystem::PointConstraintDesc desc{};
                desc.entity_a = entity;
                desc.entity_b = entt::null;
                desc.use_world_point_b = comp.use_world_point_b;
                desc.disable_collisions = comp.disable_collisions;
                desc.local_anchor_a = comp.local_anchor_a * tfm_a.scale;

                if (comp.use_world_point_b)
                {
                    desc.world_point_b = comp.world_point_b;
                }
                else
                {
                    if (!comp.entity_b.is_bound())
                        continue;
                    const entt::entity entity_b = static_cast<entt::entity>(comp.entity_b.entity);
                    if (!registry.valid(entity_b))
                        continue;
                    const auto* tfm_b = registry.try_get<ecs::TransformComponent>(entity_b);
                    if (!tfm_b)
                        continue;
                    desc.entity_b = entity_b;
                    desc.local_anchor_b = comp.local_anchor_b * tfm_b->scale;
                }

                const ConstraintKey key{ entity, ConstraintKind::Point };
                auto it = handles_.find(key);
                if (it != handles_.end())
                {
                    if (physics_system.update_point_constraint(it->second, desc))
                        track_handle(key, it->second);
                    else
                    {
                        physics_system.destroy_constraint(it->second);
                        handles_.erase(it);
                    }
                }
                else
                {
                    track_handle(key, physics_system.create_point_constraint(desc));
                }
            }
        }

        // --- Hinge constraints ---
        {
            auto view = registry.view<ecs::TransformComponent, ecs::HingeConstraintComponent>();
            for (const auto entity : view)
            {
                auto& tfm_a = view.get<ecs::TransformComponent>(entity);
                auto& comp = view.get<ecs::HingeConstraintComponent>(entity);
                if (!comp.enabled)
                    continue;

                PhysicsSystem::HingeConstraintDesc desc{};
                desc.entity_a = entity;
                desc.entity_b = entt::null;
                desc.use_world_point_b = comp.use_world_point_b;
                desc.disable_collisions = comp.disable_collisions;
                desc.local_anchor_a = comp.local_anchor_a * tfm_a.scale;
                desc.local_axis_a = comp.local_axis_a;
                desc.use_limits = comp.use_limits;
                desc.limit_min = comp.limit_min;
                desc.limit_max = comp.limit_max;
                desc.enable_motor = comp.enable_motor;
                desc.motor_target_velocity = comp.motor_target_velocity;
                desc.motor_max_impulse = comp.motor_max_impulse;

                if (comp.use_world_point_b)
                {
                    desc.world_anchor_b = comp.world_anchor_b;
                    desc.world_axis_b = comp.world_axis_b;
                }
                else
                {
                    if (!comp.entity_b.is_bound())
                        continue;
                    const entt::entity entity_b = static_cast<entt::entity>(comp.entity_b.entity);
                    if (!registry.valid(entity_b))
                        continue;
                    const auto* tfm_b = registry.try_get<ecs::TransformComponent>(entity_b);
                    if (!tfm_b)
                        continue;
                    desc.entity_b = entity_b;
                    desc.local_anchor_b = comp.local_anchor_b * tfm_b->scale;
                    desc.local_axis_b = comp.local_axis_b;
                }

                const ConstraintKey key{ entity, ConstraintKind::Hinge };
                auto it = handles_.find(key);
                if (it != handles_.end())
                {
                    if (physics_system.update_hinge_constraint(it->second, desc))
                        track_handle(key, it->second);
                    else
                    {
                        physics_system.destroy_constraint(it->second);
                        handles_.erase(it);
                    }
                }
                else
                {
                    track_handle(key, physics_system.create_hinge_constraint(desc));
                }
            }
        }

        // --- Slider constraints ---
        {
            auto view = registry.view<ecs::TransformComponent, ecs::SliderConstraintComponent>();
            for (const auto entity : view)
            {
                auto& tfm_a = view.get<ecs::TransformComponent>(entity);
                auto& comp = view.get<ecs::SliderConstraintComponent>(entity);
                if (!comp.enabled)
                    continue;

                PhysicsSystem::SliderConstraintDesc desc{};
                desc.entity_a = entity;
                desc.entity_b = entt::null;
                desc.use_world_point_b = comp.use_world_point_b;
                desc.disable_collisions = comp.disable_collisions;
                desc.local_anchor_a = comp.local_anchor_a * tfm_a.scale;
                desc.local_axis_a = comp.local_axis_a;
                desc.linear_limit_min = comp.linear_limit_min;
                desc.linear_limit_max = comp.linear_limit_max;
                desc.angular_limit_min = comp.angular_limit_min;
                desc.angular_limit_max = comp.angular_limit_max;
                desc.enable_linear_motor = comp.enable_linear_motor;
                desc.linear_motor_target_velocity = comp.linear_motor_target_velocity;
                desc.linear_motor_max_force = comp.linear_motor_max_force;

                if (comp.use_world_point_b)
                {
                    desc.world_anchor_b = comp.world_anchor_b;
                    desc.world_axis_b = comp.world_axis_b;
                }
                else
                {
                    if (!comp.entity_b.is_bound())
                        continue;
                    const entt::entity entity_b = static_cast<entt::entity>(comp.entity_b.entity);
                    if (!registry.valid(entity_b))
                        continue;
                    const auto* tfm_b = registry.try_get<ecs::TransformComponent>(entity_b);
                    if (!tfm_b)
                        continue;
                    desc.entity_b = entity_b;
                    desc.local_anchor_b = comp.local_anchor_b * tfm_b->scale;
                    desc.local_axis_b = comp.local_axis_b;
                }

                const ConstraintKey key{ entity, ConstraintKind::Slider };
                auto it = handles_.find(key);
                if (it != handles_.end())
                {
                    if (physics_system.update_slider_constraint(it->second, desc))
                        track_handle(key, it->second);
                    else
                    {
                        physics_system.destroy_constraint(it->second);
                        handles_.erase(it);
                    }
                }
                else
                {
                    track_handle(key, physics_system.create_slider_constraint(desc));
                }
            }
        }

        // --- 6DoF spring constraints ---
        {
            auto view = registry.view<ecs::TransformComponent, ecs::SixDofSpringConstraintComponent>();
            for (const auto entity : view)
            {
                auto& tfm_a = view.get<ecs::TransformComponent>(entity);
                auto& comp = view.get<ecs::SixDofSpringConstraintComponent>(entity);
                if (!comp.enabled)
                    continue;

                PhysicsSystem::SixDofSpringConstraintDesc desc{};
                desc.entity_a = entity;
                desc.entity_b = entt::null;
                desc.use_world_point_b = comp.use_world_point_b;
                desc.disable_collisions = comp.disable_collisions;
                desc.local_anchor_a = comp.local_anchor_a * tfm_a.scale;
                desc.local_rotation_a = comp.local_rotation_a;
                desc.linear_limit_min = comp.linear_limit_min;
                desc.linear_limit_max = comp.linear_limit_max;
                desc.angular_limit_min = comp.angular_limit_min;
                desc.angular_limit_max = comp.angular_limit_max;
                desc.linear_stiffness = comp.linear_stiffness;
                desc.linear_damping = comp.linear_damping;
                desc.angular_stiffness = comp.angular_stiffness;
                desc.angular_damping = comp.angular_damping;

                if (comp.use_world_point_b)
                {
                    desc.world_anchor_b = comp.world_anchor_b;
                    desc.world_rotation_b = comp.world_rotation_b;
                }
                else
                {
                    if (!comp.entity_b.is_bound())
                        continue;
                    const entt::entity entity_b = static_cast<entt::entity>(comp.entity_b.entity);
                    if (!registry.valid(entity_b))
                        continue;
                    const auto* tfm_b = registry.try_get<ecs::TransformComponent>(entity_b);
                    if (!tfm_b)
                        continue;
                    desc.entity_b = entity_b;
                    desc.local_anchor_b = comp.local_anchor_b * tfm_b->scale;
                    desc.local_rotation_b = comp.local_rotation_b;
                }

                const ConstraintKey key{ entity, ConstraintKind::SixDofSpring };
                auto it = handles_.find(key);
                if (it != handles_.end())
                {
                    if (physics_system.update_sixdof_spring_constraint(it->second, desc))
                        track_handle(key, it->second);
                    else
                    {
                        physics_system.destroy_constraint(it->second);
                        handles_.erase(it);
                    }
                }
                else
                {
                    track_handle(key, physics_system.create_sixdof_spring_constraint(desc));
                }
            }
        }

        for (auto it = handles_.begin(); it != handles_.end();)
        {
            if (seen.find(it->first) == seen.end())
            {
                physics_system.destroy_constraint(it->second);
                it = handles_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
} // namespace eeng::ecs::systems
