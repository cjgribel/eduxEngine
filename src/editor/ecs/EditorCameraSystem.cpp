// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ecs/EditorCameraSystem.hpp"

#include "EngineContext.hpp"
#include "engineapi/IInputManager.hpp"
#include "engineapi/SelectionManager.hpp"
#include "ecs/TransformComponent.hpp"
#include "editor/ecs/FirstPersonCameraComponent.hpp"
#include "editor/ecs/FirstPersonCameraSystem.hpp"
#include "editor/ecs/ThirdPersonCameraComponent.hpp"
#include "editor/ecs/ThirdPersonCameraSystem.hpp"

#include <entt/entt.hpp>

namespace
{
    void clear_first_person_active(entt::registry& registry)
    {
        auto view = registry.view<eeng::editor::FirstPersonCameraComponent>();
        for (auto entity : view)
        {
            auto& camera = view.get<eeng::editor::FirstPersonCameraComponent>(entity);
            camera.active = false;
        }
    }

    eeng::editor::ThirdPersonCameraComponent* find_active_third_person_camera(
        entt::registry& registry,
        entt::entity& out_entity)
    {
        out_entity = entt::null;
        auto view = registry.view<eeng::editor::ThirdPersonCameraComponent>();
        for (auto entity : view)
        {
            auto& camera = view.get<eeng::editor::ThirdPersonCameraComponent>(entity);
            if (!camera.active)
                continue;
            out_entity = entity;
            return &camera;
        }
        return nullptr;
    }
}

namespace eeng::editor
{
    EditorCameraSystem::~EditorCameraSystem() = default;

    void EditorCameraSystem::update(EngineContext& ctx, float delta_time)
    {
        if (!ctx.entity_manager)
            return;

        auto& registry = ctx.entity_manager->registry();

        // Keep the editor camera state sane before applying input.
        normalize_active_camera(registry);

        if (!third_person_system_)
            third_person_system_ = std::make_unique<ThirdPersonCameraSystem>();
        if (!first_person_system_)
            first_person_system_ = std::make_unique<FirstPersonCameraSystem>();

        if (ctx.input_manager && ctx.entity_selection)
        {
            using Key = eeng::IInputManager::Key;
            const bool f_down = ctx.input_manager->IsKeyPressed(Key::F);
            if (f_down && !f_was_down_)
            {
                entt::entity active_third_entity = entt::null;
                auto* camera = find_active_third_person_camera(registry, active_third_entity);
                if (camera && !ctx.entity_selection->empty())
                {
                    auto active_entity = ctx.entity_selection->last();
                    if (active_entity.has_id() && ctx.entity_manager->entity_valid(active_entity))
                    {
                        // Focus: bind the camera pivot to the currently selected entity.
                        camera->target.unbind();
                        camera->target.bind(active_entity);
                        camera->target_offset = glm::vec3(0.0f, 0.0f, 0.0f);

                        if (const auto* tfm = registry.try_get<eeng::ecs::TransformComponent>(active_entity))
                            camera->look_at = glm::vec3(tfm->world_matrix[3]);
                    }
                }
            }
            f_was_down_ = f_down;
        }
        else
        {
            // Avoid stale edge detection if input or selection is missing.
            f_was_down_ = false;
        }

        // Drive both camera systems; inactive cameras keep cached matrices fresh.
        if (third_person_system_)
            third_person_system_->update(registry, ctx, delta_time);
        if (first_person_system_)
            first_person_system_->update(registry, ctx, delta_time);
    }

    void EditorCameraSystem::normalize_active_camera(entt::registry& registry)
    {
        bool third_active = false;
        bool first_active = false;
        entt::entity first_third = entt::null;
        entt::entity first_first = entt::null;

        auto third_view = registry.view<ThirdPersonCameraComponent>();
        for (auto entity : third_view)
        {
            if (first_third == entt::null)
                first_third = entity;

            auto& camera = third_view.get<ThirdPersonCameraComponent>(entity);
            if (!camera.active)
                continue;
            if (!third_active)
            {
                third_active = true;
            }
            else
            {
                // Only one third-person camera can be active at a time.
                camera.active = false;
            }
        }

        auto first_view = registry.view<FirstPersonCameraComponent>();
        for (auto entity : first_view)
        {
            if (first_first == entt::null)
                first_first = entity;

            auto& camera = first_view.get<FirstPersonCameraComponent>(entity);
            if (!camera.active)
                continue;
            if (!first_active)
            {
                first_active = true;
            }
            else
            {
                // Only one first-person camera can be active at a time.
                camera.active = false;
            }
        }

        if (third_active && first_active)
        {
            // Prefer third-person if both types are active.
            clear_first_person_active(registry);
            first_active = false;
        }

        if (!third_active && !first_active)
        {
            // Ensure we always have a camera, preferring third-person.
            if (first_third != entt::null)
            {
                auto& camera = registry.get<ThirdPersonCameraComponent>(first_third);
                camera.active = true;
            }
            else if (first_first != entt::null)
            {
                auto& camera = registry.get<FirstPersonCameraComponent>(first_first);
                camera.active = true;
            }
        }
    }
} // namespace eeng::editor
