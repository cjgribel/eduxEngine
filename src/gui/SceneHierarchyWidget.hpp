// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "EngineContext.hpp"
#include "MetaInspect.hpp"
#include "VecTree.h"
#include "engineapi/SelectionManager.hpp"
#include "ecs/HeaderComponent.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

namespace eeng::gui
{
    using namespace eeng::ecs;

    struct SceneHierarchyWidget
    {
        EngineContext& ctx;
        EntityManager& em;
        SceneGraph& scenegraph;
        EntitySelection& entity_selection;

        int current_level = -1;
        int closed_index = -1;

        SceneHierarchyWidget(EngineContext& ctx)
            : ctx(ctx)
            , em(static_cast<EntityManager&>(*ctx.entity_manager))
            , scenegraph(em.scene_graph())
            , entity_selection(*ctx.entity_selection)
        {
        }

        void draw()
        {
            // Reset state
            current_level = -1;
            closed_index = -1;

            if (!scenegraph.size()) return;

            // Traverse and let *this act as the visitor functor
            scenegraph.get_tree().traverse_depthfirst(*this);

            // Restore ImGui state
            while (current_level >= 0)
            {
                ImGui::TreePop();
                current_level--;
            }
        }

        void operator()(const Entity& entity, size_t index, size_t level)
        {
            visit(entity, (int)level);
        }

        void visit(const Entity& entity, int level)
        {
            while (current_level >= level) { ImGui::TreePop(); current_level--; }
            if (closed_index != -1 && level > closed_index) return;

            const auto registry = em.registry_wptr().lock();
            const std::string entity_name = meta::get_entity_name(registry, entity, entt::resolve<HeaderComponent>());
            const std::string label = "[entity#" + std::to_string(entity.to_integral()) + "] " + entity_name;

            bool is_selected = entity_selection.contains(entity);
            bool is_leaf = scenegraph.get_nbr_children(entity) == 0;

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;
            if (is_leaf) flags |= ImGuiTreeNodeFlags_Leaf;
            if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::SetNextItemOpen(true);
            if (ImGui::TreeNodeEx(label.c_str(), flags))
            {
                current_level = level;
                closed_index = -1;

                if (ImGui::IsItemClicked())
                {
                    if (bool ctrl_pressed = ImGui::IsKeyDown(ImGuiKey_ModCtrl); ctrl_pressed)
                    {
                        // Multi-selection with Ctrl: toggle selection state
                        is_selected ? entity_selection.remove(entity) : entity_selection.add(entity);
                    }
                    else
                    {
                        // Single selection: clear previous selections and select this entity
                        entity_selection.clear();
                        entity_selection.add(entity);
                    }
                }
            }
            else
            {
                closed_index = level;
            }
        }
    };
} // namespace eeng::gui