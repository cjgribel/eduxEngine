// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/PistonConstraintDriveSystem.hpp"

#include "ecs/PistonConstraintDriveComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <entt/entt.hpp>

namespace
{
    constexpr float kEpsilon = 1e-5f;
    // Large limits to emulate free motion on an axis (Bullet requires finite ranges).
    constexpr float kFreeLinearLimit = 1e5f;
    constexpr int kModeHold = 0;
    constexpr int kModeExtend = 1;
    constexpr int kModeContract = 2;
    constexpr int kModePosition = 3;

    float clamp01(float value)
    {
        return std::min(std::max(value, 0.0f), 1.0f);
    }

    float safe_span(float min_value, float max_value)
    {
        const float span = max_value - min_value;
        return (std::abs(span) < kEpsilon) ? 0.0f : span;
    }

    glm::vec3 safe_normalize(const glm::vec3& v, const glm::vec3& fallback)
    {
        const float len2 = glm::dot(v, v);
        if (len2 <= kEpsilon)
            return fallback;
        return v / std::sqrt(len2);
    }

    bool compute_world_anchor(
        const eeng::ecs::TransformComponent& tfm,
        const glm::vec3& local_anchor,
        glm::vec3& out_world)
    {
        out_world = glm::vec3(tfm.world_matrix * glm::vec4(local_anchor, 1.0f));
        return true;
    }

    bool compute_extension_from_constraint(
        const entt::registry& registry,
        entt::entity body_a,
        entt::entity body_b,
        const glm::vec3& local_anchor_a,
        const glm::vec3& local_anchor_b,
        const glm::vec3& axis_local,
        float& out_position)
    {
        const auto* tfm_a = registry.try_get<eeng::ecs::TransformComponent>(body_a);
        const auto* tfm_b = registry.try_get<eeng::ecs::TransformComponent>(body_b);
        if (!tfm_a || !tfm_b)
            return false;

        glm::vec3 anchor_a_world{};
        glm::vec3 anchor_b_world{};
        compute_world_anchor(*tfm_a, local_anchor_a, anchor_a_world);
        compute_world_anchor(*tfm_b, local_anchor_b, anchor_b_world);

        const glm::vec3 axis_world = safe_normalize(tfm_a->world_rotation_matrix * axis_local, glm::vec3(1.0f, 0.0f, 0.0f));
        out_position = glm::dot(anchor_b_world - anchor_a_world, axis_world);
        return true;
    }

    bool resolve_anchor_world(
        const entt::registry& registry,
        const eeng::ecs::EntityRef& ref,
        glm::vec3& out_world)
    {
        if (!ref.is_bound())
            return false;
        const entt::entity entity = static_cast<entt::entity>(ref.entity);
        if (!registry.valid(entity))
            return false;
        const auto* tfm = registry.try_get<eeng::ecs::TransformComponent>(entity);
        if (!tfm)
            return false;
        out_world = glm::vec3(tfm->world_matrix[3]);
        return true;
    }

    bool compute_local_anchor(
        const eeng::ecs::TransformComponent& tfm,
        const glm::vec3& world_pos,
        glm::vec3& out_local)
    {
        const glm::mat4 inv_world = glm::inverse(tfm.world_matrix);
        out_local = glm::vec3(inv_world * glm::vec4(world_pos, 1.0f));
        return true;
    }
}

namespace eeng::ecs::systems
{
    void PistonConstraintDriveSystem::update_pre_physics(entt::registry& registry, EngineContext&, float delta_time)
    {
        auto view = registry.view<ecs::PistonConstraintDriveComponent>();
        for (const auto entity : view)
        {
            auto& comp = view.get<ecs::PistonConstraintDriveComponent>(entity);
            if (!comp.enabled || !comp.constraint.is_bound())
                continue;

            const entt::entity constraint_entity = static_cast<entt::entity>(comp.constraint.entity);
            auto* sixdof = registry.try_get<ecs::SixDofSpringConstraintComponent>(constraint_entity);
            if (!sixdof)
                continue;

            if (comp.anchor_a.is_bound() && comp.anchor_b.is_bound())
            {
                glm::vec3 anchor_a_world{};
                glm::vec3 anchor_b_world{};
                if (resolve_anchor_world(registry, comp.anchor_a, anchor_a_world)
                    && resolve_anchor_world(registry, comp.anchor_b, anchor_b_world))
                {
                    const glm::vec3 delta = anchor_b_world - anchor_a_world;
                    const float len = glm::length(delta);
                    const glm::vec3 axis_world = (len > kEpsilon) ? (delta / len) : glm::vec3(1.0f, 0.0f, 0.0f);

                    if (sixdof->entity_a.is_bound() && sixdof->entity_b.is_bound())
                    {
                        const entt::entity body_a = static_cast<entt::entity>(sixdof->entity_a.entity);
                        const entt::entity body_b = static_cast<entt::entity>(sixdof->entity_b.entity);
                        const auto* tfm_a = registry.try_get<ecs::TransformComponent>(body_a);
                        const auto* tfm_b = registry.try_get<ecs::TransformComponent>(body_b);
                        if (tfm_a && tfm_b)
                        {
                            compute_local_anchor(*tfm_a, anchor_a_world, sixdof->local_anchor_a);
                            compute_local_anchor(*tfm_b, anchor_b_world, sixdof->local_anchor_b);

                            if (len > kEpsilon)
                            {
                                const glm::mat3 inv_a = glm::transpose(tfm_a->world_rotation_matrix);
                                const glm::mat3 inv_b = glm::transpose(tfm_b->world_rotation_matrix);
                                const glm::vec3 axis_local_a = safe_normalize(inv_a * axis_world, glm::vec3(1.0f, 0.0f, 0.0f));
                                const glm::vec3 axis_local_b = safe_normalize(inv_b * axis_world, glm::vec3(1.0f, 0.0f, 0.0f));

                                sixdof->local_rotation_a = glm::rotation(glm::vec3(1.0f, 0.0f, 0.0f), axis_local_a);
                                sixdof->local_rotation_b = glm::rotation(glm::vec3(1.0f, 0.0f, 0.0f), axis_local_b);

                                comp.axis_local = axis_local_a;
                            }
                        }
                    }
                }
            }

            const float span = safe_span(comp.stroke_min, comp.stroke_max);
            if (!comp.command_initialized)
            {
                float measured_pos = comp.current_position;
                bool has_pos = false;
                if (sixdof->entity_a.is_bound() && sixdof->entity_b.is_bound())
                {
                    const entt::entity body_a = static_cast<entt::entity>(sixdof->entity_a.entity);
                    const entt::entity body_b = static_cast<entt::entity>(sixdof->entity_b.entity);
                    glm::vec3 axis_local = comp.axis_local;
                    if (glm::dot(axis_local, axis_local) <= kEpsilon)
                        axis_local = glm::mat3_cast(sixdof->local_rotation_a) * glm::vec3(1.0f, 0.0f, 0.0f);
                    has_pos = compute_extension_from_constraint(
                        registry, body_a, body_b,
                        sixdof->local_anchor_a, sixdof->local_anchor_b,
                        axis_local, measured_pos);
                }

                if (has_pos)
                {
                    comp.current_position = measured_pos;
                    comp.command_position = measured_pos;
                    if (span > 0.0f)
                        comp.current_extension = clamp01((measured_pos - comp.stroke_min) / span);
                }
                comp.command_initialized = true;
            }

            const float current_extension = (span > 0.0f)
                ? clamp01((comp.current_position - comp.stroke_min) / span)
                : comp.current_extension;

            float target_extension = comp.target_extension;
            switch (comp.mode)
            {
            case kModeHold:
                target_extension = current_extension;
                break;
            case kModeExtend:
                target_extension = 1.0f;
                break;
            case kModeContract:
                target_extension = 0.0f;
                break;
            case kModePosition:
            default:
                break;
            }
            target_extension = clamp01(target_extension);

            const float target_pos = comp.stroke_min + target_extension * span;
            float command_pos = comp.command_position;
            if (comp.max_velocity <= 0.0f || delta_time <= 0.0f)
            {
                command_pos = target_pos;
            }
            else
            {
                const float max_step = comp.max_velocity * delta_time;
                const float delta = target_pos - command_pos;
                if (std::abs(delta) <= max_step)
                    command_pos = target_pos;
                else
                    command_pos += (delta > 0.0f ? max_step : -max_step);
            }
            comp.command_position = command_pos;

            float limit_min = comp.stroke_min;
            float limit_max = comp.stroke_max;
            if (comp.lock_when_idle && comp.mode == kModeHold)
            {
                limit_min = comp.current_position;
                limit_max = comp.current_position;
            }
            else if (comp.mode == kModeExtend)
            {
                limit_min = comp.current_position;
            }
            else if (comp.mode == kModeContract)
            {
                limit_max = comp.current_position;
            }

            limit_min = std::clamp(limit_min, comp.stroke_min, comp.stroke_max);
            limit_max = std::clamp(limit_max, comp.stroke_min, comp.stroke_max);
            if (limit_min > limit_max)
                std::swap(limit_min, limit_max);

            const bool drive = (comp.mode != kModeHold) || comp.lock_when_idle;
            const float ang_limit = glm::pi<float>();
            const float lateral_limit = kFreeLinearLimit;

            sixdof->linear_limit_min = glm::vec3(limit_min, -lateral_limit, -lateral_limit);
            sixdof->linear_limit_max = glm::vec3(limit_max, lateral_limit, lateral_limit);
            sixdof->angular_limit_min = glm::vec3(-ang_limit);
            sixdof->angular_limit_max = glm::vec3(ang_limit);

            sixdof->linear_motor_enabled = glm::vec3(drive ? 1.0f : 0.0f, 0.0f, 0.0f);
            sixdof->linear_servo_enabled = glm::vec3(drive ? 1.0f : 0.0f, 0.0f, 0.0f);
            sixdof->linear_servo_target = glm::vec3(command_pos, 0.0f, 0.0f);
            sixdof->linear_motor_target_velocity = glm::vec3(comp.max_velocity, 0.0f, 0.0f);
            sixdof->linear_motor_max_force = glm::vec3(comp.max_force, 0.0f, 0.0f);

        }
    }

    void PistonConstraintDriveSystem::update_post_physics(entt::registry& registry, EngineContext&, float)
    {
        auto view = registry.view<ecs::PistonConstraintDriveComponent>();
        for (const auto entity : view)
        {
            auto& comp = view.get<ecs::PistonConstraintDriveComponent>(entity);
            if (!comp.enabled || !comp.constraint.is_bound())
                continue;

            const entt::entity constraint_entity = static_cast<entt::entity>(comp.constraint.entity);
            if (auto* sixdof = registry.try_get<ecs::SixDofSpringConstraintComponent>(constraint_entity))
            {
                if (sixdof->entity_a.is_bound() && sixdof->entity_b.is_bound())
                {
                    const entt::entity body_a = static_cast<entt::entity>(sixdof->entity_a.entity);
                    const entt::entity body_b = static_cast<entt::entity>(sixdof->entity_b.entity);
                    glm::vec3 axis_local = comp.axis_local;
                    if (glm::dot(axis_local, axis_local) <= kEpsilon)
                        axis_local = glm::mat3_cast(sixdof->local_rotation_a) * glm::vec3(1.0f, 0.0f, 0.0f);

                    float pos = 0.0f;
                    if (compute_extension_from_constraint(registry, body_a, body_b,
                        sixdof->local_anchor_a, sixdof->local_anchor_b, axis_local, pos))
                    {
                        comp.current_position = pos;
                    }
                }
            }

            const float span = safe_span(comp.stroke_min, comp.stroke_max);
            if (span > 0.0f)
                comp.current_extension = clamp01((comp.current_position - comp.stroke_min) / span);
        }
    }
} // namespace eeng::ecs::systems
