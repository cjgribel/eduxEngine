// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/TransformSocketSystem.hpp"

#include "ecs/TransformComponent.hpp"
#include "ecs/TransformSocketComponent.hpp"
#include "engineapi/EngineContextHelpers.hpp"

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace
{
    void apply_world_transform(
        eeng::ecs::TransformComponent& tfm,
        const glm::vec3& world_pos,
        const glm::quat& world_rot,
        const eeng::ecs::TransformComponent* parent_tfm,
        bool follow_position,
        bool follow_rotation,
        bool preserve_scale)
    {
        if (follow_position)
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

        if (follow_rotation)
        {
            if (parent_tfm)
            {
                const glm::quat local_rot = glm::normalize(glm::inverse(parent_tfm->world_rotation) * world_rot);
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
} // namespace

namespace eeng::ecs::systems
{
    void TransformSocketSystem::update(
        entt::registry& registry,
        EngineContext& ctx,
        float,
        bool is_edit) const
    {
        auto* em = eeng::try_get_entity_manager_ptr(ctx, "TransformSocketSystem");
        auto view = registry.view<ecs::TransformSocketComponent, ecs::TransformComponent>();
        for (const auto entity : view)
        {
            auto& comp = view.get<ecs::TransformSocketComponent>(entity);
            auto& tfm = view.get<ecs::TransformComponent>(entity);

            if (!comp.enabled)
            {
                comp.last_local_version = tfm.local_version;
                continue;
            }

            if (is_edit && !comp.update_in_edit)
            {
                comp.last_local_version = tfm.local_version;
                continue;
            }

            if (!comp.target.is_bound())
            {
                comp.last_local_version = tfm.local_version;
                continue;
            }

            const entt::entity target_entity = static_cast<entt::entity>(comp.target.entity);
            if (!registry.valid(target_entity))
                continue;
            const auto* target_tfm = registry.try_get<ecs::TransformComponent>(target_entity);
            if (!target_tfm)
                continue;

            const ecs::TransformComponent* parent_tfm = nullptr;
            const ecs::Entity socket_ecs{ entity };
            if (em && em->entity_valid(socket_ecs))
            {
                const auto parent_ref = em->get_entity_parent(socket_ecs);
                if (parent_ref.is_bound() && registry.valid(parent_ref.entity))
                    parent_tfm = registry.try_get<ecs::TransformComponent>(parent_ref.entity);
            }

            const bool capture_offset = is_edit && comp.capture_offset_in_edit;
            if (capture_offset && comp.last_local_version != tfm.local_version)
            {
                const glm::quat parent_world_rot = parent_tfm
                    ? parent_tfm->world_rotation
                    : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

                const glm::vec3 socket_world_pos = parent_tfm
                    ? glm::vec3(parent_tfm->world_matrix * glm::vec4(tfm.position, 1.0f))
                    : tfm.position;
                const glm::quat socket_world_rot = glm::normalize(parent_world_rot * tfm.rotation);

                const glm::quat inv_target_rot = glm::inverse(target_tfm->world_rotation);
                const glm::vec3 delta_world = socket_world_pos - glm::vec3(target_tfm->world_matrix[3]);
                comp.local_offset = glm::mat3_cast(inv_target_rot) * delta_world;
                comp.local_rotation = glm::normalize(inv_target_rot * socket_world_rot);
            }

            const glm::quat target_world_rot = target_tfm->world_rotation;
            const glm::vec3 target_world_pos = glm::vec3(target_tfm->world_matrix[3]);
            const glm::vec3 offset_world = glm::mat3_cast(target_world_rot) * comp.local_offset;
            const glm::quat desired_world_rot = glm::normalize(target_world_rot * comp.local_rotation);
            const glm::vec3 desired_world_pos = target_world_pos + offset_world;

            apply_world_transform(
                tfm,
                desired_world_pos,
                desired_world_rot,
                parent_tfm,
                comp.follow_position,
                comp.follow_rotation,
                comp.preserve_scale);

            comp.last_local_version = tfm.local_version;
        }
    }
} // namespace eeng::ecs::systems
