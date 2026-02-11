// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/TwoAnchorAlignSystem.hpp"

#include "ecs/TwoAnchorAlignComponent.hpp"
#include "ecs/TransformComponent.hpp"
#include "engineapi/EngineContextHelpers.hpp"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace
{
    constexpr float kEpsilon = 1e-6f;

    glm::vec3 safe_normalize(const glm::vec3& v, const glm::vec3& fallback)
    {
        const float len2 = glm::dot(v, v);
        if (len2 <= kEpsilon)
            return fallback;
        return v / std::sqrt(len2);
    }

    glm::vec3 resolve_up_vector(
        const entt::registry& registry,
        const eeng::ecs::TwoAnchorAlignComponent& comp)
    {
        if (comp.up_reference.is_bound())
        {
            const entt::entity ref = static_cast<entt::entity>(comp.up_reference.entity);
            if (registry.valid(ref))
            {
                if (const auto* tfm = registry.try_get<eeng::ecs::TransformComponent>(ref))
                {
                    const glm::vec3 axis = safe_normalize(comp.up_axis_ref, glm::vec3(0.0f, 1.0f, 0.0f));
                    return safe_normalize(tfm->world_rotation_matrix * axis, glm::vec3(0.0f, 1.0f, 0.0f));
                }
            }
        }
        return glm::vec3(0.0f, 1.0f, 0.0f);
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

    glm::quat compute_rotation(
        const eeng::ecs::TwoAnchorAlignComponent& comp,
        const glm::vec3& forward_world,
        const glm::vec3& up_world)
    {
        const glm::vec3 forward = safe_normalize(forward_world, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec3 up = safe_normalize(up_world, glm::vec3(0.0f, 1.0f, 0.0f));

        // Ensure up is not parallel to forward.
        if (std::abs(glm::dot(forward, up)) > 0.999f)
        {
            const glm::vec3 fallback = (std::abs(forward.y) < 0.9f)
                ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(0.0f, 0.0f, 1.0f);
            up = safe_normalize(fallback, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        glm::vec3 right = glm::cross(up, forward);
        right = safe_normalize(right, glm::vec3(1.0f, 0.0f, 0.0f));
        up = safe_normalize(glm::cross(forward, right), glm::vec3(0.0f, 1.0f, 0.0f));

        const glm::mat3 world_basis(right, up, forward);

        glm::vec3 local_forward = safe_normalize(comp.local_forward_axis, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec3 local_up = safe_normalize(comp.local_up_axis, glm::vec3(0.0f, 1.0f, 0.0f));

        if (std::abs(glm::dot(local_forward, local_up)) > 0.999f)
            local_up = glm::vec3(0.0f, 1.0f, 0.0f);

        glm::vec3 local_right = glm::cross(local_up, local_forward);
        local_right = safe_normalize(local_right, glm::vec3(1.0f, 0.0f, 0.0f));
        local_up = safe_normalize(glm::cross(local_forward, local_right), glm::vec3(0.0f, 1.0f, 0.0f));

        const glm::mat3 local_basis(local_right, local_up, local_forward);
        const glm::mat3 rotation_matrix = world_basis * glm::transpose(local_basis);
        return glm::quat_cast(rotation_matrix);
    }

    glm::vec3 resolve_target_position(
        const eeng::ecs::TwoAnchorAlignComponent& comp,
        const glm::vec3& anchor_a,
        const glm::vec3& anchor_b,
        const glm::vec3& forward)
    {
        glm::vec3 pos = anchor_a;
        if (comp.position_mode == 1)
            pos = anchor_b;
        else if (comp.position_mode == 2)
            pos = 0.5f * (anchor_a + anchor_b);

        if (std::abs(comp.position_offset) > kEpsilon)
            pos += forward * comp.position_offset;
        return pos;
    }

    void apply_world_transform(
        eeng::ecs::TransformComponent& tfm,
        const glm::vec3& world_pos,
        const glm::quat& world_rot,
        const eeng::ecs::TransformComponent* parent_tfm,
        bool align_position,
        bool align_rotation,
        bool preserve_scale)
    {
        if (align_position)
        {
            if (parent_tfm)
            {
                const glm::mat4 inv_parent = glm::inverse(parent_tfm->world_matrix);
                const glm::vec3 local_pos = glm::vec3(inv_parent * glm::vec4(world_pos, 1.0f));
                tfm.set_position(local_pos);
            }
            else
            {
                tfm.set_position(world_pos);
            }
        }

        if (align_rotation)
        {
            if (parent_tfm)
            {
                const glm::quat local_rot = glm::normalize(glm::conjugate(parent_tfm->world_rotation) * world_rot);
                tfm.set_rotation(local_rot);
            }
            else
            {
                tfm.set_rotation(world_rot);
            }
        }

        if (!preserve_scale)
            tfm.set_scale(glm::vec3(1.0f));
    }
}

namespace eeng::ecs::systems
{
    void TwoAnchorAlignSystem::update(entt::registry& registry, EngineContext& ctx, float)
    {
        auto* em = eeng::try_get_entity_manager_ptr(ctx, "TwoAnchorAlignSystem");
        auto view = registry.view<ecs::TwoAnchorAlignComponent>();
        for (const auto entity : view)
        {
            auto& comp = view.get<ecs::TwoAnchorAlignComponent>(entity);
            if (!comp.enabled)
                continue;

            glm::vec3 anchor_a{};
            glm::vec3 anchor_b{};
            if (!resolve_anchor_world(registry, comp.anchor_a, anchor_a))
                continue;
            if (!resolve_anchor_world(registry, comp.anchor_b, anchor_b))
                continue;

            const glm::vec3 delta = anchor_b - anchor_a;
            const float length = glm::length(delta);
            if (length <= kEpsilon)
                continue;

            comp.current_length = length;

            const glm::vec3 forward = delta / length;
            const glm::vec3 up = resolve_up_vector(registry, comp);

            const glm::quat world_rot = comp.align_rotation
                ? compute_rotation(comp, forward, up)
                : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

            const glm::vec3 world_pos = comp.align_position
                ? resolve_target_position(comp, anchor_a, anchor_b, forward)
                : anchor_a;

            const entt::entity target_entity = comp.target.is_bound()
                ? static_cast<entt::entity>(comp.target.entity)
                : entity;

            if (!registry.valid(target_entity))
                continue;

            auto* tfm = registry.try_get<ecs::TransformComponent>(target_entity);
            if (!tfm)
                continue;

            const ecs::TransformComponent* parent_tfm = nullptr;
            const ecs::Entity target_ecs{ target_entity };
            if (em && em->entity_valid(target_ecs))
            {
                const auto parent_ref = em->get_entity_parent(target_ecs);
                if (parent_ref.is_bound() && registry.valid(parent_ref.entity))
                    parent_tfm = registry.try_get<ecs::TransformComponent>(parent_ref.entity);
            }

            apply_world_transform(
                *tfm,
                world_pos,
                world_rot,
                parent_tfm,
                comp.align_position,
                comp.align_rotation,
                comp.preserve_scale);
        }
    }
} // namespace eeng::ecs::systems
