// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "EngineContext.hpp"
#include "MetaInspect.hpp"
#include "VecTree.h"
#include "engineapi/SelectionManager.hpp"
#include "ecs/HeaderComponent.hpp"
#include "BatchRegistry.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <unordered_map>
#include <unordered_set>

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

        struct BatchKey
        {
            bool known = false;
            bool mixed = false;
            BatchId id{};
        };

        std::unordered_map<Guid, const BatchInfo*> guid_to_batch;
        std::unordered_set<Guid> guid_conflicts;
        BatchKey last_root_batch{};
        bool has_last_root_batch = false;

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
            has_last_root_batch = false;
            build_batch_lookup();

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

            if (level == 0)
                maybe_draw_batch_separator(entity);

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

        void build_batch_lookup()
        {
            guid_to_batch.clear();
            guid_conflicts.clear();

            if (!ctx.batch_registry)
                return;

            auto& br = static_cast<BatchRegistry&>(*ctx.batch_registry);
            const auto batches = br.list();

            for (const auto* batch : batches)
            {
                if (!batch || batch->state != BatchInfo::State::Loaded)
                    continue;

                for (const auto& er : batch->live)
                {
                    if (!er.guid.valid())
                        continue;
                    if (guid_conflicts.contains(er.guid))
                        continue;

                    auto [it, inserted] = guid_to_batch.emplace(er.guid, batch);
                    if (!inserted && it->second != batch)
                    {
                        guid_to_batch.erase(it);
                        guid_conflicts.insert(er.guid);
                    }
                }
            }
        }

        BatchKey resolve_batch_key(const Entity& entity, const BatchInfo*& out_info) const
        {
            out_info = nullptr;
            BatchKey key{};

            if (!entity.has_id())
                return key;

            const auto entity_ref = em.get_entity_ref(entity);
            if (!entity_ref.guid.valid())
                return key;

            if (guid_conflicts.contains(entity_ref.guid))
            {
                key.mixed = true;
                return key;
            }

            const auto it = guid_to_batch.find(entity_ref.guid);
            if (it == guid_to_batch.end())
                return key;

            out_info = it->second;
            if (out_info)
            {
                key.known = true;
                key.id = out_info->id;
            }

            return key;
        }

        static bool same_batch_key(const BatchKey& a, const BatchKey& b)
        {
            if (a.known != b.known || a.mixed != b.mixed)
                return false;
            if (a.known)
                return a.id == b.id;
            return true;
        }

        void maybe_draw_batch_separator(const Entity& entity)
        {
            if (!ctx.batch_registry)
                return;

            const BatchInfo* info = nullptr;
            const BatchKey key = resolve_batch_key(entity, info);

            if (has_last_root_batch && same_batch_key(key, last_root_batch))
                return;

            std::string label;
            if (key.mixed)
                label = "Batch: Mixed";
            else if (!key.known)
                label = "Batch: (unknown/unassigned)";
            else if (info && !info->name.empty())
                label = "Batch: " + info->name;
            else
                label = "Batch: " + key.id.to_string();

            ImGui::Separator();
            ImGui::TextDisabled("%s", label.c_str());

            last_root_batch = key;
            has_last_root_batch = true;
        }
    };
} // namespace eeng::gui
