// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "EngineContext.hpp"
#include "editor/CommandQueue.hpp"
#include "editor/GuiCommands.hpp"
#include "ecs/EntityManager.hpp"
// #include "MetaInspect.hpp"
// #include "VecTree.h"
#include "engineapi/SelectionManager.hpp"
// #include "ecs/HeaderComponent.hpp"
#include <vector>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

namespace eeng::gui
{
    using namespace eeng::ecs;

    namespace detail
    {
        inline std::vector<Entity>
            filter_out_descendants(SceneGraph& scenegraph, const std::deque<Entity>& entities)
        {
            std::vector<Entity> filtered_entities;
            filtered_entities.reserve(entities.size());

            for (const auto& entity : entities)
            {
                bool is_child = false;
                for (const auto& entity_other : entities)
                {
                    if (entity == entity_other)
                        continue;
                    if (scenegraph.is_descendant_of(entity, entity_other))
                    {
                        is_child = true;
                        break;
                    }
                }
                if (!is_child)
                    filtered_entities.push_back(entity);
            }
            return filtered_entities;
        }
    }

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

            auto ctx_wptr = ctx.weak_from_this();
            const bool has_selection = !entity_selection.empty();
            const bool has_multi_selection = entity_selection.size() > 1;
            const bool can_queue = (ctx.command_queue != nullptr && !ctx_wptr.expired());

            if (!can_queue)
                ImGui::BeginDisabled();

            // New entity
            if (ImGui::Button("New"))
            {
                Entity entity_parent{};
                if (has_selection)
                    entity_parent = entity_selection.last();

                ctx.command_queue->add(
                    editor::CommandFactory::Create<editor::CreateEntityCommand>(
                        entity_parent,
                        ctx_wptr));
            }

            ImGui::SameLine();

            // Destroy selected entities
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Delete"))
            {
                auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
                auto roots = detail::filter_out_descendants(
                    em.scene_graph(),
                    entity_selection.get_all());

                for (const auto& entity : roots)
                {
                    ctx.command_queue->add(
                        editor::CommandFactory::Create<editor::DestroyEntityBranchCommand>(
                            entity,
                            ctx_wptr));
                }

                entity_selection.clear();
            }
            if (!has_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Copy selected entities
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Copy"))
            {
                auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
                auto roots = detail::filter_out_descendants(
                    em.scene_graph(),
                    entity_selection.get_all());

                for (const auto& entity : roots)
                {
                    ctx.command_queue->add(
                        editor::CommandFactory::Create<editor::CopyEntityBranchCommand>(
                            entity,
                            ctx_wptr));
                }
            }
            if (!has_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Reparent selected entities
            if (!has_multi_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Parent"))
            {
                auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
                auto& scenegraph = em.scene_graph();
                const auto new_parent = entity_selection.last();

                for (const auto& entity : entity_selection.get_all())
                {
                    if (entity == new_parent)
                        continue;
                    if (scenegraph.is_descendant_of(new_parent, entity))
                        continue;

                    ctx.command_queue->add(
                        editor::CommandFactory::Create<editor::ReparentEntityBranchCommand>(
                            entity,
                            new_parent,
                            ctx_wptr));
                }
            }
            if (!has_multi_selection) ImGui::EndDisabled();

            ImGui::SameLine();

            // Unparent selected entities (set them as roots)
            if (!has_selection) ImGui::BeginDisabled();
            if (ImGui::Button("Unparent"))
            {
                auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
                auto& scenegraph = em.scene_graph();

                for (const auto& entity : entity_selection.get_all())
                {
                    if (scenegraph.is_root(entity))
                        continue;

                    ctx.command_queue->add(
                        editor::CommandFactory::Create<editor::ReparentEntityBranchCommand>(
                            entity,
                            Entity{},
                            ctx_wptr));
                }
            }
            if (!has_selection) ImGui::EndDisabled();

            if (!can_queue)
                ImGui::EndDisabled();
        }
    };

} // namespace eeng::gui
