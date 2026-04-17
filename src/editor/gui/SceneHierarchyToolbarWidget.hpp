// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "EngineContext.hpp"
#include "EventQueue.h"
#include "engineapi/PlayModePolicy.hpp"
#include "editor/EditorActions.hpp"
#include "editor/ProjectConfig.hpp"
#include "ecs/EntityManager.hpp"
#include "meta/EntityMetaHelpers.hpp"
#include "meta/MetaSerialize.hpp"
// #include "ecs/EntityManager.hpp"
#include "engineapi/SelectionManager.hpp"
#include "editor/ecs/FirstPersonCameraComponent.hpp"
#include "editor/ecs/ThirdPersonCameraComponent.hpp"
// #include "ecs/HeaderComponent.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <entt/entt.hpp>
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
            draw_scene_actions_row();
            draw_prefab_row();
        }

        void draw_mode_row()
        {
            const bool is_playing = ctx.services
                && ctx.services->play_mode_active.load(std::memory_order_relaxed);
            const bool can_toggle = static_cast<bool>(ctx.event_queue);

            ImGui::TextUnformatted("Mode:");
            ImGui::SameLine();
            const ImVec4 mode_color = is_playing ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
                                                 : ImVec4(0.9f, 0.8f, 0.2f, 1.0f);
            ImGui::TextColored(mode_color, "%s", is_playing ? "Play" : "Edit");
            ImGui::SameLine();
            if (!can_toggle)
                ImGui::BeginDisabled();
            if (ImGui::Button(is_playing ? "Stop" : "Play"))
            {
                ctx.event_queue->dispatch(TogglePlayModeEvent{});
            }
            if (!can_toggle)
                ImGui::EndDisabled();

            const bool has_services = (ctx.services != nullptr);
            int policy_choice = 0; // 0 = Runtime Default, 1 = Preview, 2 = Warm Play
            if (has_services
                && ctx.services->play_mode_policy_override_enabled.load(std::memory_order_relaxed))
            {
                const auto override_policy = ctx.services->play_mode_policy_override.load(
                    std::memory_order_relaxed);
                policy_choice = (override_policy == PlayModePolicy::Preview) ? 1 : 2;
            }

            ImGui::SameLine();
            ImGui::Dummy(ImVec2(8.0f, 0.0f));
            ImGui::SameLine();
            ImGui::TextUnformatted("Policy:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150.0f);
            if (!has_services)
                ImGui::BeginDisabled();
            // "Warm Play" maps to the current Strict implementation:
            // a fresh play world + runtime-selected startup content, while the
            // edit world stays resident in the background for fast return.
            if (ImGui::Combo("##PlayPolicy", &policy_choice, "Runtime Default\0Preview\0Warm Play\0"))
            {
                if (policy_choice == 0)
                {
                    ctx.services->play_mode_policy_override_enabled.store(
                        false, std::memory_order_relaxed);
                }
                else if (policy_choice == 1)
                {
                    ctx.services->play_mode_policy_override_enabled.store(
                        true, std::memory_order_relaxed);
                    ctx.services->play_mode_policy_override.store(
                        PlayModePolicy::Preview,
                        std::memory_order_relaxed);
                }
                else
                {
                    ctx.services->play_mode_policy_override_enabled.store(
                        true, std::memory_order_relaxed);
                    ctx.services->play_mode_policy_override.store(
                        PlayModePolicy::Strict,
                        std::memory_order_relaxed);
                }
            }
            if (!has_services)
                ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::Dummy(ImVec2(8.0f, 0.0f));
            ImGui::SameLine();
            ImGui::TextUnformatted("Camera:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.0f);

            // Editor camera toggle is disabled during play.
            if (is_playing)
                ImGui::BeginDisabled();

            if (!ctx.entity_manager)
            {
                ImGui::TextDisabled("n/a");
            }
            else
            {
                // Discover editor cameras and drive their "active" flags.
                auto& registry = ctx.entity_manager->registry();
                auto third_view = registry.view<editor::ThirdPersonCameraComponent>();
                auto first_view = registry.view<editor::FirstPersonCameraComponent>();

                const bool has_third = third_view.begin() != third_view.end();
                const bool has_first = first_view.begin() != first_view.end();

                int camera_choice = -1; // 0 = Third, 1 = First

                if (has_third)
                {
                    for (auto entity : third_view)
                    {
                        const auto& camera = third_view.get<editor::ThirdPersonCameraComponent>(entity);
                        if (camera.active)
                        {
                            camera_choice = 0;
                            break;
                        }
                    }
                }

                if (camera_choice == -1 && has_first)
                {
                    for (auto entity : first_view)
                    {
                        const auto& camera = first_view.get<editor::FirstPersonCameraComponent>(entity);
                        if (camera.active)
                        {
                            camera_choice = 1;
                            break;
                        }
                    }
                }

                if (camera_choice == -1)
                {
                    if (has_third)
                        camera_choice = 0;
                    else if (has_first)
                        camera_choice = 1;
                }

                if (has_third && has_first)
                {
                    if (ImGui::Combo("##EditorCamera", &camera_choice, "Third Person\0First Person\0"))
                    {
                        const bool enable_third = (camera_choice == 0);
                        const bool enable_first = (camera_choice == 1);

                        // Only one editor camera should be active at a time.
                        for (auto entity : third_view)
                            third_view.get<editor::ThirdPersonCameraComponent>(entity).active = enable_third;
                        for (auto entity : first_view)
                            first_view.get<editor::FirstPersonCameraComponent>(entity).active = enable_first;
                    }
                }
                else if (has_third)
                {
                    ImGui::TextUnformatted("Third Person");
                }
                else if (has_first)
                {
                    ImGui::TextUnformatted("First Person");
                }
                else
                {
                    ImGui::TextDisabled("None");
                }
            }

            if (is_playing)
                ImGui::EndDisabled();
        }

        void draw_scene_actions_row()
        {
            auto& entity_selection = *ctx.entity_selection;
            const bool has_selection = !entity_selection.empty();
            const bool has_multi_selection = entity_selection.size() > 1;
            const bool can_queue = static_cast<bool>(ctx.command_queue);
            const bool is_playing = ctx.services
                && ctx.services->play_mode_active.load(std::memory_order_relaxed);
            const bool allow_edit_actions = can_queue && !is_playing;

            if (!allow_edit_actions)
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

            ImGui::SameLine();

            // Save selection as prefab (single entity root).
            const bool can_save_prefab = has_selection && !has_multi_selection;
            if (!can_save_prefab) ImGui::BeginDisabled();
            if (ImGui::Button("Save Prefab"))
            {
                ImGui::OpenPopup("Save Prefab");
            }
            if (!can_save_prefab) ImGui::EndDisabled();

            if (ImGui::BeginPopupModal("Save Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                static char prefab_name[128] = "NewPrefab";
                ImGui::InputText("File name", prefab_name, sizeof(prefab_name));

                const auto prefab_root = get_prefab_root();
                const bool valid_root = !prefab_root.empty();
                if (!valid_root)
                    ImGui::TextDisabled("Prefabs folder not available.");
                else
                    ImGui::TextDisabled("Target: %s", prefab_root.string().c_str());

                bool saved = false;
                if (ImGui::Button("Save") && can_save_prefab && valid_root)
                {
                    const auto entity = entity_selection.last();
                    const std::string filename = std::string(prefab_name) + ".json";
                    saved = save_prefab_from_entity(
                        prefab_root / filename,
                        entity);
                    if (saved)
                        ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                    ImGui::CloseCurrentPopup();

                ImGui::EndPopup();
            }

            if (!allow_edit_actions)
                ImGui::EndDisabled();
        }

        std::filesystem::path get_prefab_root() const
        {
            if (!ctx.project_config)
                return {};

            const auto root = ctx.project_config->project_root;
            if (root.empty())
                return {};

            return root / "prefabs";
        }

        bool save_prefab_from_entity(const std::filesystem::path& path, const ecs::Entity& root_entity)
        {
            if (!ctx.entity_manager)
                return false;

            auto& em = static_cast<eeng::EntityManager&>(*ctx.entity_manager);
            auto registry_sp = em.registry_wptr().lock();
            if (!registry_sp)
                return false;

            auto& scenegraph = em.scene_graph();
            if (!root_entity.has_id() || !em.entity_valid(root_entity) || !scenegraph.contains(root_entity))
                return false;

            const auto branch = scenegraph.get_branch_topdown(root_entity);
            nlohmann::json branch_json = nlohmann::json::array();
            for (const auto& entity : branch)
            {
                // Pre-prefab bind: upgrade live-only EntityRefs to GUIDs so external links
                // survive prefab serialization (e.g., constraint targets outside the branch).
                eeng::meta::bind_entity_refs_for_entity(entity, ctx);
                branch_json.push_back(meta::serialize_entity_for_file(
                    em.get_entity_ref(entity),
                    registry_sp));
            }

            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            std::ofstream output(path);
            if (!output)
                return false;

            output << branch_json.dump(2);
            return static_cast<bool>(output);
        }

        bool load_prefab_json(const std::filesystem::path& path, nlohmann::json& out_json) const
        {
            std::ifstream input(path);
            if (!input)
                return false;
            try
            {
                input >> out_json;
            }
            catch (...)
            {
                return false;
            }
            return true;
        }

        void draw_prefab_row()
        {
            const auto prefab_root = get_prefab_root();
            const bool has_prefab_root = !prefab_root.empty() && std::filesystem::exists(prefab_root);

            static std::filesystem::path selected_prefab_path{};
            static std::string selected_prefab_label{};
            std::vector<std::filesystem::path> prefab_files;

            if (has_prefab_root)
            {
                for (const auto& entry : std::filesystem::directory_iterator(prefab_root))
                {
                    if (!entry.is_regular_file())
                        continue;
                    const auto& path = entry.path();
                    if (path.extension() != ".json")
                        continue;
                    prefab_files.push_back(path);
                }
                std::sort(prefab_files.begin(), prefab_files.end());
            }

            if (!selected_prefab_path.empty())
            {
                const auto it = std::find(prefab_files.begin(), prefab_files.end(), selected_prefab_path);
                if (it == prefab_files.end())
                {
                    selected_prefab_path.clear();
                    selected_prefab_label.clear();
                }
            }

            ImGui::TextUnformatted("Prefab");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(220.0f);
            const char* preview = selected_prefab_label.empty() ? "(none)" : selected_prefab_label.c_str();
            if (ImGui::BeginCombo("##PrefabCombo", preview))
            {
                if (prefab_files.empty())
                {
                    ImGui::TextDisabled("(no prefabs)");
                }
                else
                {
                    for (const auto& path : prefab_files)
                    {
                        const std::string label = path.filename().string();
                        const bool is_selected = (path == selected_prefab_path);
                        if (ImGui::Selectable(label.c_str(), is_selected))
                        {
                            selected_prefab_path = path;
                            selected_prefab_label = label;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            static bool spawn_under_selection = true;
            const bool has_selection = ctx.entity_selection && !ctx.entity_selection->empty();
            const bool can_spawn = static_cast<bool>(ctx.command_queue)
                && !(ctx.services && ctx.services->play_mode_active.load(std::memory_order_relaxed));

            ImGui::SameLine();
            const bool can_spawn_prefab = can_spawn && !selected_prefab_path.empty();
            if (!can_spawn_prefab)
                ImGui::BeginDisabled();
            if (ImGui::Button("Spawn"))
            {
                nlohmann::json prefab_json;
                if (load_prefab_json(selected_prefab_path, prefab_json))
                {
                    ecs::Entity parent{};
                    if (spawn_under_selection && has_selection)
                        parent = ctx.entity_selection->last();
                    editor::SceneActions::spawn_entity_branch_from_json(
                        ctx,
                        std::move(prefab_json),
                        parent,
                        true);
                }
            }
            if (!can_spawn_prefab)
                ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::Checkbox("Under selection", &spawn_under_selection);
            if (!has_prefab_root)
                ImGui::TextDisabled("Prefabs folder not found.");
        }
    };

} // namespace eeng::gui
