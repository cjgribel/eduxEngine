// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "TrailSystem.hpp"

#include "ShapeRenderer.hpp"
#include "ecs/TrailComponent.hpp"
#include "ecs/TransformComponent.hpp"

#include <string>

namespace eeng::ecs::systems
{
    void TrailSystem::update(
        entt::registry& registry,
        EngineContext& ctx,
        float delta_time)
    {
        (void)ctx;
        auto view = registry.view<ecs::TrailComponent>();
        // for (auto entity : view)
        // {
        //     auto& note = view.get<ecs::StickyNoteComponent>(entity);
        //     StickyNoteComponent_Update(note, delta_time);
        // }
    }

    void TrailSystem::render(
        entt::registry& registry,
        EngineContext& ctx,
        ShapeRendering::ShapeRenderer& renderer) const
    {
        (void)ctx;
        (void)renderer;
        auto view = registry.view<ecs::TransformComponent, ecs::TrailComponent>();
        for (auto entity : view)
        {
            (void)entity;
            // const auto& transform = view.get<ecs::TransformComponent>(entity);
            // const auto& note = view.get<ecs::StickyNoteComponent>(entity);
            // if (!note.enabled || note.count == 0)
            //     continue;

            // const std::string text = StickyNoteComponent_Dump(note);
            // if (text.empty())
            //     continue;

            // const auto world_pos = glm::vec3(transform.world_matrix[3]) + note.world_offset;
            // const std::string window_name =
            //     "StickyNote##" + std::to_string(entt::to_integral(entity));

            // eeng::gui::ImGuiPrintTextAt(
            //     world_pos,
            //     VP_PROJ_V,
            //     window_height,
            //     text.c_str(),
            //     window_name.c_str(),
            //     note.color_bg,
            //     note.color_text);
        }
    }
}
