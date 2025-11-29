// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "EngineContext.hpp"
#include "util/EventQueue.h"
// #include "MetaInspect.hpp"
// #include "VecTree.h"
#include "engineapi/SelectionManager.hpp"
// #include "ecs/HeaderComponent.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

namespace eeng::gui
{
    using namespace eeng::ecs;

    struct SceneTreeToolbarWidget
    {
        EngineContext& ctx;
        EventQueue& event_queue;

        SceneTreeToolbarWidget(EngineContext& ctx)
            : ctx(ctx)
            , event_queue(*ctx.event_queue)
        {
        }

        void draw()
        {
            auto& entity_selection = *ctx.entity_selection;

            bool has_selection = !entity_selection.empty();
            bool has_multi_selection = entity_selection.size() > 1;

            // Disable all for now
            ImGui::BeginDisabled();

            // New entity
            if (ImGui::Button("New"))
            {
                // Entity entity_parent;
                // if (has_selection) entity_parent = entity_selection.last();
                // CreateEntityEvent event{ .parent_entity = entity_parent };
                // event_queue.enqueue_event(event);
            }

            ImGui::SameLine();

            // Destroy selected entities
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Delete"))
            {
                // DestroyEntitySelectionEvent event{ .entity_selection = entity_selection };
                // event_queue.enqueue_event(event);
                // entity_selection.clear();
            }
            if (!has_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Copy selected entities
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Copy"))
            {
                // CopyEntitySelectionEvent event{ entity_selection };
                // event_queue.enqueue_event(event);
            }
            if (!has_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Reparent selected entities
            if (!has_multi_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Parent"))
            {
                // SetParentEntitySelectionEvent event{ .entity_selection = entity_selection };
                // event_queue.enqueue_event(event);
            }
            if (!has_multi_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Unparent selected entities (set them as roots)
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Unparent"))
            {
                // UnparentEntitySelectionEvent event{ .entity_selection = entity_selection };
                // event_queue.enqueue_event(event);
            }
            if (!has_selection) ImGui::EndDisabled();

            // Re-enable
            ImGui::EndDisabled();
        }
    };

} // namespace eeng::gui