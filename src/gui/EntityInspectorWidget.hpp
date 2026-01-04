// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "editor/InspectorState.hpp"
#include "engineapi/SelectionManager.hpp"
#include "editor/EditorActions.hpp"
#include "util/EventQueue.h"
#include "meta/MetaInspect.hpp"      // for eeng::meta::inspect_entity
#include "MetaLiterals.h"
#include "MetaAux.h"
//#include "FileManager.hpp"
// #include "ecs/HeaderComponent.hpp"

#include "imgui.h"
#include <algorithm>
#include <string>
#include <vector>

namespace eeng::gui
{
    struct EntityInspectorWidget
    {
        EngineContext& ctx;
        EventQueue& event_queue;

        EntityInspectorWidget(EngineContext& ctx)
            : ctx(ctx)
            , event_queue(*ctx.event_queue)
        {
        }

        // Returns true if anything was modified (via meta commands)
        bool draw()
        {
            bool mod = false;

            auto registry_sp = ctx.entity_manager->registry_wptr().lock();
            if (!registry_sp)
            {
                ImGui::TextUnformatted("No registry");
                return false;
            }

            auto& entity_selection = *ctx.entity_selection;

            ecs::Entity selected_entity;
            if (!entity_selection.empty())
            {
                selected_entity = entity_selection.first();
            }

            const bool selected_entity_valid =
                selected_entity.has_id()&
                registry_sp->valid(selected_entity);

            // --- Add / Remove Component ------------------------------------

            struct ComponentItem
            {
                entt::id_type id{};
                std::string name;
            };

            const auto& component_items = []() -> const std::vector<ComponentItem>&
                {
                    static std::vector<ComponentItem> items;
                    static bool built = false;

                    if (!built)
                    {
                        for (auto&& [id, type] : entt::resolve())
                        {
                            if (!type)
                                continue;
                            if (!eeng::meta::type_is_registered(type))
                                continue;
                            if (!type.func(eeng::literals::assure_component_storage_hs))
                                continue;

                            items.push_back(ComponentItem{
                                type.id(),
                                eeng::meta::get_meta_type_display_name(type)
                                });
                        }

                        std::sort(
                            items.begin(),
                            items.end(),
                            [](const ComponentItem& a, const ComponentItem& b)
                            {
                                return a.name < b.name;
                            });

                        built = true;
                    }

                    return items;
                }();

            static entt::id_type selected_comp_id{};
            static ImGuiTextFilter comp_filter;

            const bool has_selection = !entity_selection.empty();
            const auto& selection = entity_selection.get_all();
            const int selection_count = static_cast<int>(selection.size());

            ImGui::TextUnformatted("Add/Remove Component");

            if (ImGui::Button("Components..."))
            {
                ImGui::OpenPopup("component_picker_popup");
            }

            if (ImGui::BeginPopup("component_picker_popup"))
            {
                comp_filter.Draw("Filter##component_picker");
                ImGui::Separator();

                const entt::id_type header_comp_id = []()
                    {
                        auto header_type = eeng::meta::resolve_by_type_id_string("eeng.ecs.HeaderComponent");
                        return header_type ? header_type.id() : entt::id_type{};
                    }();

                int selected_present_count = 0;
                ImGui::BeginChild("component_picker_list", ImVec2(520.0f, 280.0f), true);
                for (const auto& item : component_items)
                {
                    if (!comp_filter.PassFilter(item.name.c_str()))
                        continue;

                    int present_count = 0;
                    if (registry_sp)
                    {
                        if (auto storage = registry_sp->storage(item.id); storage)
                        {
                            for (const auto& entity : selection)
                            {
                                if (!entity.has_id())
                                    continue;
                                if (!registry_sp->valid(entity))
                                    continue;
                                if (storage->contains(entity))
                                    ++present_count;
                            }
                        }
                    }

                    if (item.id == selected_comp_id)
                        selected_present_count = present_count;

                    std::string label = item.name
                        + " (" + std::to_string(present_count)
                        + "/" + std::to_string(selection_count) + ")";

                    ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    if (selection_count > 0)
                    {
                        if (present_count == 0)
                            color = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
                        else if (present_count == selection_count)
                            color = ImVec4(0.35f, 0.85f, 0.35f, 1.0f);
                        else
                            color = ImVec4(0.95f, 0.85f, 0.35f, 1.0f);
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    const bool is_selected = (item.id == selected_comp_id);
                    if (ImGui::Selectable(label.c_str(), is_selected))
                    {
                        selected_comp_id = item.id;
                        selected_present_count = present_count;
                    }
                    ImGui::PopStyleColor();
                }
                ImGui::EndChild();

                const bool has_selected_component = selected_comp_id != entt::id_type{};
                const bool can_add = has_selection && has_selected_component
                    && (selected_present_count < selection_count);
                const bool can_remove = has_selection && has_selected_component
                    && (selected_present_count > 0)
                    && (selected_comp_id != header_comp_id);

                ImGui::BeginDisabled(!can_add);
                if (ImGui::Button("Add##addcomponent"))
                {
                    editor::SceneActions::add_components(ctx, selection, selected_comp_id);
                }
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::BeginDisabled(!can_remove);
                if (ImGui::Button("Remove##removecomponent"))
                {
                    editor::SceneActions::remove_components(ctx, selection, selected_comp_id);
                }
                ImGui::EndDisabled();

                ImGui::EndPopup();
            }

            // --- Add / Remove Behavior ------------------------------------
            ImGui::BeginDisabled(); // TODO: script components are not wired yet.
            ImGui::TextUnformatted("Add/Remove Behavior");
            // Scripts combo (unchanged, just using ctx selection)
            static std::string selected_script_path;
#if 0
            auto all_scripts = FileManager::GetFilesInFolder(script_dir, "lua");
            {
                const char* preview = selected_script_path.empty()
                    ? ""
                    : selected_script_path.c_str();

                if (ImGui::BeginCombo("##addscriptcombo", preview))
                {
                    for (auto& script_path : all_scripts)
                    {
                        bool is_selected = (script_path == selected_script_path);

                        if (ImGui::Selectable(script_path.c_str(), is_selected))
                        {
                            selected_script_path = script_path;
                        }

                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
#endif

            // Add script
            ImGui::SameLine();
            if (ImGui::Button("Add##addscript") &&
                !selected_script_path.empty() &&
                !entity_selection.empty())
            {
#if 0
                AddScriptToEntitySelectionEvent event{
                    selected_script_path,
                    entity_selection
                };
                event_queue.enqueue_event(event);
#endif
            }

            // Remove script (disabled)
            ImGui::SameLine();
            ImGui::BeginDisabled();
            if (ImGui::Button("Remove##removescript") &&
                !selected_script_path.empty() &&
                !entity_selection.empty())
            {
#if 0
                RemoveScriptFromEntitySelectionEvent event{
                    selected_script_path,
                    entity_selection
                };
                event_queue.enqueue_event(event);
#endif
            }
            ImGui::EndDisabled();

            ImGui::EndDisabled();

            // --- Component inspector --------------------------------------
            // ImGui::SetNextItemOpen(true);
            // if (ImGui::TreeNode("Components"))
            // {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                const ImGuiTableFlags flags =
                    ImGuiTableFlags_BordersV |
                    ImGuiTableFlags_BordersOuterH |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_NoBordersInBody;

                if (ImGui::BeginTable("InspectorTable", 2, flags))
                {
                    editor::InspectorState inspector_state;

                    if (selected_entity_valid)
                    {
                        // Call your existing meta-driven inspector
                        mod |= eeng::meta::inspect_entity(
                            selected_entity,
                            inspector_state,
                            ctx);
                    }
                    else
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("No valid selection");
                    }

                    ImGui::EndTable();
                }

                // ImGui::TreePop();
                ImGui::PopStyleVar();
            // }

            return mod;
        }
    };
} // namespace eeng::editor
