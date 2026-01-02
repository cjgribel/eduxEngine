// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "EngineContext.hpp"
#include "editor/EditorActions.hpp"
// #include "ecs/EntityManager.hpp"
#include "engineapi/SelectionManager.hpp"
// #include "ecs/HeaderComponent.hpp"
#include <vector>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

namespace eeng::gui
{
    using namespace eeng::ecs;

    struct SceneTreeToolbarWidget
    {
        EngineContext& ctx;

        SceneTreeToolbarWidget(EngineContext& ctx)
            : ctx(ctx)
        {
        }

        void draw()
        {
            auto& entity_selection = *ctx.entity_selection;

            const bool has_selection = !entity_selection.empty();
            const bool has_multi_selection = entity_selection.size() > 1;
            const bool can_queue = (ctx.command_queue != nullptr);

            if (!can_queue)
                ImGui::BeginDisabled();

            // New entity
            if (ImGui::Button("New"))
            {
                Entity entity_parent{};
                if (has_selection)
                    entity_parent = entity_selection.last();

                editor::SceneActions::create_entity(ctx, entity_parent);
            }

            ImGui::SameLine();

            // Destroy selected entities
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Delete"))
            {
                editor::SceneActions::delete_entities(ctx, entity_selection.get_all());

                entity_selection.clear();
            }
            if (!has_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Copy selected entities
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Copy"))
            {
                editor::SceneActions::copy_entities(ctx, entity_selection.get_all());
            }
            if (!has_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Reparent selected entities
            if (!has_multi_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Parent"))
            {
                editor::SceneActions::parent_entities(ctx, entity_selection.get_all());
            }
            if (!has_multi_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Unparent selected entities (set them as roots)
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Unparent"))
            {
                editor::SceneActions::unparent_entities(ctx, entity_selection.get_all());
            }
            if (!has_selection) ImGui::EndDisabled();

            if (!can_queue)
                ImGui::EndDisabled();
        }
    };

} // namespace eeng::gui
