// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "StickyNoteSystem.hpp"

#include "ImGuiHelpers.hpp"
#include "ecs/StickyNoteComponent.hpp"
#include "ecs/TransformComponent.hpp"

#include <string>

namespace eeng::ecs::systems
{
    void StickyNoteSystem::update(
        entt::registry& registry,
        EngineContext& ctx,
        float delta_time)
    {
        (void)ctx;
        auto view = registry.view<ecs::StickyNoteComponent>();
        for (auto entity : view)
        {
            auto& note = view.get<ecs::StickyNoteComponent>(entity);
            StickyNoteComponent_Update(note, delta_time);
        }
    }

    void StickyNoteSystem::render(
        entt::registry& registry,
        EngineContext& ctx,
        const glm::mat4& VP_PROJ_V,
        int window_height) const
    {
        (void)ctx;
        auto view = registry.view<ecs::TransformComponent, ecs::StickyNoteComponent>();
        for (auto entity : view)
        {
            const auto& transform = view.get<ecs::TransformComponent>(entity);
            const auto& note = view.get<ecs::StickyNoteComponent>(entity);
            if (!note.enabled || note.count == 0)
                continue;

            const std::string text = StickyNoteComponent_Dump(note);
            if (text.empty())
                continue;

            const auto world_pos = glm::vec3(transform.world_matrix[3]) + note.world_offset;
            const std::string window_name =
                "StickyNote##" + std::to_string(entt::to_integral(entity));

            eeng::gui::ImGuiPrintTextAt(
                world_pos,
                VP_PROJ_V,
                window_height,
                text.c_str(),
                window_name.c_str(),
                note.color_bg,
                note.color_text);
        }
    }
}
