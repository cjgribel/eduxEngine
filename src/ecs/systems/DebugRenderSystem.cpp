// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "DebugRenderSystem.hpp"

#include "ImGuiHelpers.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/TransformComponent.hpp"

#include <cstdio>
#include <string>

namespace eeng::ecs::systems
{
    void DebugRenderSystem::render(
        entt::registry& registry,
        EngineContext& ctx,
        const glm::mat4& VP_PROJ_V,
        int window_height) const
    {
        (void)ctx;
        if (!settings.show_transform_labels)
            return;

        auto view = registry.view<ecs::TransformComponent>();
        for (const auto entity : view)
        {
            const auto& transform = view.get<ecs::TransformComponent>(entity);
            const auto* header = registry.try_get<ecs::HeaderComponent>(entity);
            const auto world_pos = glm::vec3(transform.world_matrix[3]) + settings.transform_label_offset;

            char label[128];
            if (header && !header->name.empty())
            {
                std::snprintf(label, sizeof(label), "%s", header->name.c_str());
            }
            else
            {
                std::snprintf(label, sizeof(label), "Entity %u",
                    static_cast<unsigned>(entt::to_integral(entity)));
            }

            const std::string window_name =
                "TransformLabel##" + std::to_string(entt::to_integral(entity));

            eeng::gui::ImGuiPrintTextAt(
                world_pos,
                VP_PROJ_V,
                window_height,
                label,
                window_name.c_str(),
                settings.transform_label_bg,
                settings.transform_label_text);
        }
    }
}
