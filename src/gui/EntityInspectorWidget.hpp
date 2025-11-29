// eeng/editor/EntityInspectorWidget.hpp (for example)

#pragma once

#include "EngineContext.hpp"
#include "editor/InspectorState.hpp"
#include "engineapi/SelectionManager.hpp"
#include "util/EventQueue.h"
#include "meta/MetaInspect.hpp"      // for eeng::meta::inspect_entity
#include "MetaLiterals.h"
#include "MetaAux.h"
//#include "FileManager.hpp"
#include "ecs/HeaderComponent.hpp" // if you still need it

#include "imgui.h"
#include <string>

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
                !selected_entity.is_null() &&
                registry_sp->valid(selected_entity);

            // --- Add / Remove Component ------------------------------------
            ImGui::TextUnformatted("Add/Remove Component");

            // Same static as before, now local to this function
            static entt::id_type selected_comp_id = 0;
            {
#if 0
                const auto get_comp_name = [](entt::id_type comp_id) -> std::string
                    {
                        return get_meta_type_name(entt::resolve(comp_id));
                    };

                std::string selected_entry_label;
                if (selected_comp_id)
                {
                    selected_entry_label = get_comp_name(selected_comp_id);
                }

                if (ImGui::BeginCombo("##addcompcombo", selected_entry_label.c_str()))
                {
                    // For all meta types
                    for (auto&& [id_, type] : entt::resolve())
                    {
                        // Skip non-component types
                        if (auto is_component =
                            get_meta_type_prop<bool, false>(type, "is_component"_hs);
                            !is_component)
                        {
                            continue;
                        }

                        // See your original note about id_ vs type.id()
                        auto id = type.id();

                        bool is_selected = (id == selected_comp_id);
                        std::string label = get_comp_name(id);

                        if (ImGui::Selectable(label.c_str(), is_selected))
                        {
                            selected_comp_id = id;
                        }

                        if (is_selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
#endif
            }

            // Add Component button
            ImGui::SameLine();
            if (ImGui::Button("Add##addcomponent") &&
                selected_comp_id &&
                !entity_selection.empty())
            {
#if 0
                AddComponentToEntitySelectionEvent event{
                    selected_comp_id,
                    entity_selection
                };
                event_queue.enqueue_event(event);
#endif
            }

            // Remove Component button
            ImGui::SameLine();
            if (ImGui::Button("Remove##removecomponent") &&
                selected_comp_id &&
                !entity_selection.empty())
            {
#if 0
                RemoveComponentFromEntitySelectionEvent event{
                    selected_comp_id,
                    entity_selection
                };
                event_queue.enqueue_event(event);
#endif
            }

            // --- Add / Remove Behavior ------------------------------------
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

            // Remove script (still always disabled until you add logic)
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
                        ImGui::TextUnformatted("Selected entity is null or invalid");
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
