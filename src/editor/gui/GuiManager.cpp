// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#define IMGUI_DEFINE_MATH_OPERATORS

#include "GuiManager.hpp"
#include "ResourceManager.hpp"
#include "BatchRegistry.hpp"
#include "editor/EditorActions.hpp"
#include "ecs/EntityManager.hpp"

#include "gui/SceneHierarchyWidget.hpp"
#include "gui/SceneHierarchyToolbarWidget.hpp"
#include "gui/EntityInspectorWidget.hpp"
#include "gui/CommandQueueWidget.hpp"
#include "gui/ResourceBrowserWidget.hpp"
#include "gui/ProfilerWidget.hpp"
#include "gui/RigSpawnerWidget.hpp"

#include "EngineContextHelpers.hpp"
#include "editor/AnimationGraphComponentInspect.hpp"
#include "editor/OverlayRenderSettingsPersistence.hpp"
#include "ecs/RuntimePipeline.hpp"
#include "ecs/systems/DebugRenderSystem.hpp"

#include "ThreadPool.hpp" // remove?
#include "MainThreadQueue.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <filesystem>
#include <string>
//#include <future>

namespace eeng
{
    namespace
    {
        bool g_imgui_ini_set = false;
        bool g_request_layout_reset = false;
        bool g_layout_bootstrapped = false;
        std::string g_imgui_ini_path;

        void ensure_imgui_ini_path(EngineContext& ctx)
        {
            if (g_imgui_ini_set)
                return;

            std::filesystem::path root;
            if (ctx.project_config)
                root = ctx.project_config->project_root;
            else
                root = std::filesystem::current_path();

            std::filesystem::path dir = root / ".editor";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);

            g_imgui_ini_path = (dir / "imgui.ini").string();
            auto& io = ImGui::GetIO();
            io.IniFilename = g_imgui_ini_path.c_str();

            if (std::filesystem::exists(g_imgui_ini_path, ec))
            {
                ImGui::LoadIniSettingsFromDisk(g_imgui_ini_path.c_str());
            }

            g_imgui_ini_set = true;
        }

        bool should_bootstrap_layout()
        {
            if (g_request_layout_reset)
                return true;

            if (g_layout_bootstrapped)
                return false;

            std::error_code ec;
            if (g_imgui_ini_path.empty() || !std::filesystem::exists(g_imgui_ini_path, ec))
                return true;

            return false;
        }

        void build_default_dock_layout(ImGuiID dockspace_id, const ImVec2& dockspace_size)
        {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, dockspace_size);

            ImGuiID dock_main = dockspace_id;
            ImGuiID dock_left = 0;
            ImGuiID dock_right = 0;
            ImGuiID dock_bottom = 0;
            ImGuiID dock_left_bottom = 0;
            ImGuiID dock_right_bottom = 0;

            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.23f, &dock_left, &dock_main);
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.28f, &dock_right, &dock_main);
            ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.27f, &dock_bottom, &dock_main);
            ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.45f, &dock_left_bottom, &dock_left);
            ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.45f, &dock_right_bottom, &dock_right);

            // Left: scene hierarchy + entity inspector.
            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_left);
            ImGui::DockBuilderDockWindow("Entity Inspector", dock_left_bottom);

            // Right: resource/tools.
            ImGui::DockBuilderDockWindow("Resource Browser", dock_right);
            ImGui::DockBuilderDockWindow("Storage", dock_right);
            ImGui::DockBuilderDockWindow("Storage Occupancy", dock_right);
            ImGui::DockBuilderDockWindow("Rig Spawner", dock_right);
            ImGui::DockBuilderDockWindow("Asset Inspector", dock_right_bottom);

            // Bottom: logs + diagnostics.
            ImGui::DockBuilderDockWindow("Log", dock_bottom);
            ImGui::DockBuilderDockWindow("Command Queue", dock_bottom);
            ImGui::DockBuilderDockWindow("Task Monitor", dock_bottom);
            ImGui::DockBuilderDockWindow("Batch Registry", dock_bottom);
            ImGui::DockBuilderDockWindow("Profiler", dock_bottom);
            ImGui::DockBuilderDockWindow("Engine Info", dock_bottom);

            // Center: graph tooling.
            ImGui::DockBuilderDockWindow("Animation Graph Visualizer", dock_main);

            ImGui::DockBuilderFinish(dockspace_id);

            g_layout_bootstrapped = true;
            g_request_layout_reset = false;

            if (!g_imgui_ini_path.empty())
                ImGui::SaveIniSettingsToDisk(g_imgui_ini_path.c_str());
        }
    } // namespace

    void GuiManager::init()
    {
        // ImGui::StyleColorsClassic();
        // ImGui::StyleColorsLight();
        ImGui::StyleColorsDark();
    }

    void GuiManager::release()
    {

    }

    void GuiManager::draw(EngineContext& ctx) const
    {
        const bool is_playing = ctx.services
            && ctx.services->play_mode_active.load(std::memory_order_relaxed);
        draw_main_menu(ctx);
        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowEditorControls))
            draw_editor_controls(ctx);
        draw_dockspace(ctx);
        if (is_playing)
            return;

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowStorageWindow))
            draw_storage(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowProfiler))
            draw_profiler(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowResourceBrowser))
            draw_resource_browser(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowAssetInspector))
            draw_asset_inspector(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowBatchRegistry))
            draw_batch_registry(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowTaskMonitor))
            draw_task_monitor(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowCommandQueue))
            draw_command_queue(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowRigSpawner))
            draw_rig_spawner(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowSceneGraph))
            draw_scene_graph(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowEntityInspector))
            draw_entity_inspector(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowAnimationGraphVisualizer))
            draw_animation_graph_visualizer(ctx);

        gui::draw_resource_import_dialogs(ctx);
    }

    void GuiManager::draw_main_menu(EngineContext& ctx) const
    {
        if (!ImGui::BeginMainMenuBar())
            return;

        auto toggle_gui_flag = [&](GuiFlags flag, const char* label)
        {
            if (!ctx.gui_manager)
                return;
            bool enabled = ctx.gui_manager->is_flag_enabled(flag);
            if (ImGui::MenuItem(label, nullptr, &enabled))
                ctx.gui_manager->set_flag(flag, enabled);
        };

        if (ImGui::BeginMenu("File"))
        {
            ImGui::BeginDisabled();
            ImGui::MenuItem("New Scene", "Ctrl+N");
            ImGui::MenuItem("Open Project", "Ctrl+O");
            ImGui::Separator();
            ImGui::MenuItem("Save", "Ctrl+S");
            ImGui::MenuItem("Save All", "Ctrl+Shift+S");
            ImGui::MenuItem("Export Build");
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::BeginDisabled();
            ImGui::MenuItem("Undo", "Ctrl+Z");
            ImGui::MenuItem("Redo", "Ctrl+Y");
            ImGui::Separator();
            ImGui::MenuItem("Cut", "Ctrl+X");
            ImGui::MenuItem("Copy", "Ctrl+C");
            ImGui::MenuItem("Paste", "Ctrl+V");
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene"))
        {
            const bool is_playing = ctx.services
                && ctx.services->play_mode_active.load(std::memory_order_relaxed);
            const char* play_label = is_playing ? "Stop Play" : "Play";
            if (ImGui::MenuItem(play_label, "F5"))
            {
                if (ctx.event_queue)
                    ctx.event_queue->dispatch(TogglePlayModeEvent{});
            }

            ImGui::BeginDisabled();
            ImGui::MenuItem("Reload Scene");
            ImGui::MenuItem("Rebuild Lighting");
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Assets"))
        {
            ImGui::BeginDisabled();
            ImGui::MenuItem("Import Asset");
            ImGui::MenuItem("Rebuild Asset Index");
            ImGui::MenuItem("Validate References");
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Maximize/Restore"))
            {
                if (ctx.event_queue)
                    ctx.event_queue->dispatch(ToggleWindowMaximizeEvent{});
            }

            if (ImGui::BeginMenu("Resolution"))
            {
                auto set_resolution = [&](const char* label, int width, int height)
                {
                    if (ImGui::MenuItem(label))
                    {
                        if (ctx.event_queue)
                            ctx.event_queue->dispatch(SetWindowSizeEvent{ width, height, true, true });
                    }
                };

                set_resolution("1280 x 720", 1280, 720);
                set_resolution("1600 x 900", 1600, 900);
                set_resolution("1920 x 1080", 1920, 1080);
                set_resolution("2560 x 1440", 2560, 1440);
                set_resolution("3840 x 2160", 3840, 2160);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Overlay Rendering"))
            {
                auto* overlay_settings = ctx.services ? ctx.services->overlay_render_settings : nullptr;
                auto* debug_edit = ctx.services ? ctx.services->debug_render_settings_edit : nullptr;
                auto* debug_play = ctx.services ? ctx.services->debug_render_settings_play : nullptr;
                if (!overlay_settings || !debug_edit || !debug_play)
                {
                    ImGui::TextDisabled("Overlay settings unavailable.");
                }
                else
                {
                    bool dirty = false;
                    const auto toggle_bool = [&dirty](const char* label, bool& value)
                    {
                        if (ImGui::MenuItem(label, nullptr, &value))
                            dirty = true;
                    };

                    const auto draw_debug_feature_toggles =
                        [&](eeng::ecs::systems::DebugRenderSettings& settings)
                    {
                        toggle_bool("Colliders", settings.show_colliders);

                        if (ImGui::BeginMenu("Transform Labels"))
                        {
                            toggle_bool("Show Labels", settings.show_transform_labels);
                            ImGui::Separator();
                            ImGui::TextDisabled("Label Data");
                            toggle_bool("Entity Name", settings.transform_label_show_name);
                            toggle_bool("Position", settings.transform_label_show_position);
                            toggle_bool("Rotation", settings.transform_label_show_rotation);
                            toggle_bool("Scale", settings.transform_label_show_scale);
                            ImGui::EndMenu();
                        }

                        if (ImGui::BeginMenu("Collider Labels"))
                        {
                            toggle_bool("Show Labels", settings.show_collider_labels);
                            ImGui::Separator();
                            ImGui::TextDisabled("Label Data");
                            toggle_bool("Collider Id", settings.collider_label_show_id);
                            toggle_bool("Collider Type", settings.collider_label_show_type);
                            toggle_bool("Trigger State", settings.collider_label_show_trigger_state);
                            toggle_bool("Local Position", settings.collider_label_show_local_position);
                            toggle_bool("Dimensions", settings.collider_label_show_dimensions);
                            ImGui::EndMenu();
                        }

                        if (ImGui::BeginMenu("RigidBody Labels"))
                        {
                            toggle_bool("Show Labels", settings.show_rigidbody_labels);
                            ImGui::Separator();
                            ImGui::TextDisabled("Label Data");
                            toggle_bool("Motion Type", settings.rigidbody_label_show_motion_type);
                            toggle_bool("Mass", settings.rigidbody_label_show_mass);
                            toggle_bool("Inertia", settings.rigidbody_label_show_inertia);
                            toggle_bool("Damping", settings.rigidbody_label_show_damping);
                            toggle_bool("COM Offset", settings.rigidbody_label_show_com_offset);
                            ImGui::EndMenu();
                        }

                        toggle_bool("RigidBody COM", settings.show_rigidbody_com);
                        toggle_bool("RigidBody Axes", settings.show_rigidbody_axes);
                        toggle_bool("RigidBody Offset Frame", settings.show_rigidbody_offset);
                        toggle_bool("Raycasts", settings.show_raycast_debug);
                        toggle_bool("Constraints", settings.show_constraints);
                        toggle_bool("Constraint Hinges", settings.show_constraint_hinges);
                        toggle_bool("Constraint Sliders", settings.show_constraint_sliders);
                        toggle_bool("Constraint 6DoF", settings.show_constraint_6dof);
                        toggle_bool("Constraint Anchor Points", settings.show_constraint_points);
                        toggle_bool("Springs", settings.show_springs);
                        toggle_bool("Two-Anchor Align", settings.show_two_anchor_align);
                        toggle_bool("Demo Shapes", settings.show_demo_shapes);
                        toggle_bool("Sticky Notes", settings.show_sticky_notes);

                        if (ImGui::BeginMenu("Skeleton"))
                        {
                            toggle_bool("Lines", settings.show_skeleton);
                            toggle_bool("Nodes", settings.show_skeleton_nodes);
                            toggle_bool("Axes", settings.show_skeleton_axes);
                            toggle_bool("Labels", settings.show_skeleton_labels);
                            toggle_bool("Bones Only", settings.show_skeleton_bones_only);
                            ImGui::EndMenu();
                        }
                    };

                    const auto draw_mode_menu =
                        [&](const char* label, eeng::ecs::OverlayModeSettings& mode_settings, eeng::ecs::systems::DebugRenderSettings& debug_settings)
                    {
                        if (!ImGui::BeginMenu(label))
                            return;

                        toggle_bool("Show Debug Overlays", mode_settings.show_debug);
                        toggle_bool("Show Trails", mode_settings.show_trails);

                        ImGui::Separator();
                        ImGui::TextDisabled("Debug Feature Toggles");
                        draw_debug_feature_toggles(debug_settings);

                        ImGui::EndMenu();
                    };

                    draw_mode_menu("Edit Mode", overlay_settings->edit, *debug_edit);
                    draw_mode_menu("Play Mode", overlay_settings->play, *debug_play);

                    ImGui::Separator();
                    if (ImGui::MenuItem("Copy Edit -> Play"))
                    {
                        overlay_settings->play = overlay_settings->edit;
                        *debug_play = *debug_edit;
                        dirty = true;
                    }
                    if (ImGui::MenuItem("Copy Play -> Edit"))
                    {
                        overlay_settings->edit = overlay_settings->play;
                        *debug_edit = *debug_play;
                        dirty = true;
                    }

                    if (dirty)
                    {
                        eeng::editor::save_overlay_render_settings(
                            ctx,
                            *overlay_settings,
                            *debug_edit,
                            *debug_play);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools"))
        {
            toggle_gui_flag(GuiFlags::ShowProfiler, "Profiler");
            toggle_gui_flag(GuiFlags::ShowBatchRegistry, "Batch Registry");
            toggle_gui_flag(GuiFlags::ShowTaskMonitor, "Task Monitor");
            toggle_gui_flag(GuiFlags::ShowCommandQueue, "Command Queue");
            toggle_gui_flag(GuiFlags::ShowRigSpawner, "Rig Spawner");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            toggle_gui_flag(GuiFlags::ShowSceneGraph, "Scene Hierarchy");
            toggle_gui_flag(GuiFlags::ShowEntityInspector, "Entity Inspector");
            toggle_gui_flag(GuiFlags::ShowResourceBrowser, "Resource Browser");
            toggle_gui_flag(GuiFlags::ShowAssetInspector, "Asset Inspector");
            toggle_gui_flag(GuiFlags::ShowStorageWindow, "Storage");
            toggle_gui_flag(GuiFlags::ShowProfiler, "Profiler");
            toggle_gui_flag(GuiFlags::ShowBatchRegistry, "Batch Registry");
            toggle_gui_flag(GuiFlags::ShowTaskMonitor, "Task Monitor");
            toggle_gui_flag(GuiFlags::ShowCommandQueue, "Command Queue");
            toggle_gui_flag(GuiFlags::ShowRigSpawner, "Rig Spawner");
            toggle_gui_flag(GuiFlags::ShowAnimationGraphVisualizer, "Animation Graph");
            toggle_gui_flag(GuiFlags::ShowEditorControls, "Editor Controls");
            toggle_gui_flag(GuiFlags::ShowEngineInfo, "Engine Info");
            toggle_gui_flag(GuiFlags::ShowLogWindow, "Log");
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout"))
                g_request_layout_reset = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            ImGui::BeginDisabled();
            ImGui::MenuItem("Documentation");
            ImGui::MenuItem("Report Issue");
            ImGui::Separator();
            ImGui::MenuItem("About");
            ImGui::EndDisabled();
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    void GuiManager::draw_profiler(EngineContext& ctx) const
    {
        ImGui::Begin("Profiler");
        gui::ProfilerWidget widget{ ctx };
        widget.draw();
        ImGui::End();
    }

    void GuiManager::draw_dockspace(EngineContext& ctx) const
    {
        ensure_imgui_ini_path(ctx);

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoNavFocus;

        ImGui::Begin("EditorDockspace", nullptr, flags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockspace_id = ImGui::GetID("EditorDockspaceRoot");
        const ImGuiDockNodeFlags dock_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dock_flags);

        if (should_bootstrap_layout())
            build_default_dock_layout(dockspace_id, viewport->WorkSize);

        ImGui::End();
    }

    void GuiManager::draw_rig_spawner(EngineContext& ctx) const
    {
        ImGui::Begin("Rig Spawner");
        gui::RigSpawnerWidget widget{ ctx };
        widget.draw();
        ImGui::End();
    }

    // TODO -> WIDGET not a window
    void DrawOccupancyBar(size_t used, size_t capacity)
    {
        if (capacity == 0) return;

        float cell_size = 6.0f;
        float spacing = 2.0f;
        ImVec2 p = ImGui::GetCursorScreenPos();
        auto* draw_list = ImGui::GetWindowDrawList();

        for (size_t i = 0; i < capacity; ++i)
        {
            ImU32 color = ImGui::ColorConvertFloat4ToU32(
                (i < used) ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f)
            );

            ImVec2 cell_min = ImVec2(p.x + i * (cell_size + spacing), p.y);
            ImVec2 cell_max = ImVec2(cell_min.x + cell_size, cell_min.y + cell_size);

            draw_list->AddRectFilled(cell_min, cell_max, color);
        }

        // Advance cursor
        ImGui::Dummy(ImVec2(capacity * (cell_size + spacing), cell_size));
    }

    void GuiManager::draw_storage(EngineContext& ctx) const
    {
        ImGui::Begin("Storage");

        auto& storage = static_cast<ResourceManager&>(*ctx.resource_manager).storage();

        for (const auto& row : storage.pool_stats())
        {
            const auto meta_type = entt::resolve(row.type_id);
            const auto type_name = meta::get_meta_type_id_string(meta_type);

            if (ImGui::TreeNode(type_name.c_str()))
            {
                const size_t capacity = row.capacity;
                const size_t free = row.free_count;
                const size_t used = capacity - free;
                const size_t elem_size = row.element_size;

                const float kb_total = (capacity * elem_size) / 1024.0f;
                const float kb_used = (used * elem_size) / 1024.0f;

                ImGui::Text("Entries: %zu", used);
                ImGui::Text("Capacity: %zu (%.1f kB total, %.1f kB used)",
                    capacity, kb_total, kb_used);
                ImGui::Text("Used: %zu | Free: %zu", used, free);

                // ───── Occupancy Grid ─────
                const int items_per_row = 64;
                for (size_t i = 0; i < capacity; ++i)
                {
                    if (i % items_per_row != 0)
                        ImGui::SameLine();

                    bool is_used = (i < used); // Replace tracked this precisely
                    ImVec4 color = is_used
                        ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)  // green
                        : ImVec4(0.4f, 0.4f, 0.4f, 1.0f); // gray

                    ImGui::TextColored(color, "[]");
                }

                ImGui::NewLine();

                if (ImGui::TreeNode("Details"))
                {
                    auto details = storage.pool_debug_string(row.type_id);
                    ImGui::TextUnformatted(details.c_str());
                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }

        }

        ImGui::End();

        // ───── Storage Occupancy Table ─────
        // Separate window to show a summary of all storage pools

        ImGui::Begin("Storage Occupancy");

        if (ImGui::BeginTable("StorageTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Used / Capacity");
            ImGui::TableSetupColumn("Occupancy");
            ImGui::TableHeadersRow();

            for (const auto& row : storage.pool_stats())
            {
                ImGui::TableNextRow();

                // Column 1: Type name
                ImGui::TableSetColumnIndex(0);
                auto meta_type = entt::resolve(row.type_id);
                const auto type_name = meta::get_meta_type_id_string(meta_type);
                ImGui::Text("%s", type_name.c_str());

                // Column 2: Usage summary
                ImGui::TableSetColumnIndex(1);
                const size_t capacity = row.capacity;
                const size_t used = capacity - row.free_count;
                ImGui::Text("%zu / %zu", used, capacity);

                // Column 3: Colored cell bar
                ImGui::TableSetColumnIndex(2);
                DrawOccupancyBar(used, capacity);
            }

            ImGui::EndTable();
        }

        ImGui::End();

    }

    void GuiManager::draw_resource_browser(EngineContext& ctx) const
    {
        ImGui::Begin("Resource Browser");

        gui::ResourceBrowserWidget browser{ ctx };
        browser.draw();

        ImGui::End();
    }

    void GuiManager::draw_asset_inspector(EngineContext& ctx) const
    {
        ImGui::Begin("Asset Inspector");

        gui::ResourceBrowserActionsWidget actions{ ctx };
        actions.draw();

        ImGui::Separator();

        gui::AssetInspectorWidget inspector{ ctx };
        inspector.draw();

        ImGui::End();
    }

    void GuiManager::draw_batch_registry(EngineContext& ctx) const
    {
        if (!ctx.batch_registry)
            return;

        auto& br = static_cast<BatchRegistry&>(*ctx.batch_registry);

        ImGui::Begin("Batch Registry");

        // --- Create batch ---
        {
            static char new_batch_name[128] = "";
            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputText("New Batch Name", new_batch_name, sizeof(new_batch_name));
            ImGui::SameLine();
            if (ImGui::Button("New Batch"))
            {
                editor::BatchActions::create_batch(ctx, std::string(new_batch_name));
                new_batch_name[0] = '\0';
            }
            ImGui::Separator();
        }

        // Snapshot of batches
        const auto batches = br.list();
        if (batches.empty())
        {
            ImGui::TextUnformatted("No batches found. Create some in startup or via tools.");
            ImGui::End();
            return;
        }

        BatchId default_batch_id{};
        bool has_default_batch = false;
        for (const auto* b : batches)
        {
            if (!b)
                continue;
            if (b->name == BatchRegistry::kDefaultBatchName)
            {
                default_batch_id = b->id;
                has_default_batch = true;
                break;
            }
        }

        // Selection state (shared across GUI)
        auto& batch_selection = *ctx.batch_selection;
        if (batch_selection.empty() && !batches.empty())
            batch_selection.add(has_default_batch ? default_batch_id : batches.front()->id);

        BatchId selected_id{};
        if (!batch_selection.empty())
            selected_id = batch_selection.last();

        int selected_index = -1;
        for (int i = 0; i < static_cast<int>(batches.size()); ++i)
        {
            if (batches[i]->id == selected_id)
            {
                selected_index = i;
                break;
            }
        }

        if (selected_index < 0 && !batches.empty())
        {
            batch_selection.clear();
            const auto fallback_id = has_default_batch ? default_batch_id : batches.front()->id;
            batch_selection.add(fallback_id);
            selected_id = fallback_id;
            selected_index = 0;
        }

        // Layout: left = list, right = details
        ImGui::Columns(2, nullptr, true);

        // --- Left: batch list ---
        ImGui::TextUnformatted("Batches");
        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(batches.size()); ++i)
        {
            const BatchInfo* b = batches[i];

            // Small label: "Name (state)"
            const char* state_str = "";
            switch (b->state)
            {
            case BatchInfo::State::Unloaded:   state_str = "Unloaded";   break;
            case BatchInfo::State::Queued:     state_str = "Queued";     break;
            case BatchInfo::State::Loading:    state_str = "Loading";    break;
            case BatchInfo::State::Loaded:     state_str = "Loaded";     break;
            case BatchInfo::State::Unloading:  state_str = "Unloading";  break;
            case BatchInfo::State::Error:      state_str = "Error";      break;
            }
            ImVec4 state_color{};
            switch (b->state)
            {
            case BatchInfo::State::Unloaded:   state_color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); break; // gray
            case BatchInfo::State::Queued:     state_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break; // yellow
            case BatchInfo::State::Loading:    state_color = ImVec4(1.0f, 0.6f, 0.0f, 1.0f); break; // orange
            case BatchInfo::State::Loaded:     state_color = ImVec4(0.2f, 0.9f, 0.2f, 1.0f); break; // green
            case BatchInfo::State::Unloading:  state_color = ImVec4(0.8f, 0.5f, 0.2f, 1.0f); break;
            case BatchInfo::State::Error:      state_color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break; // red
            }

            std::string label = b->name.empty()
                ? b->id.to_string()
                : b->name + " (" + state_str + ")";
            label += "##" + std::to_string(i); // ensure unique ID

            ImGui::PushStyleColor(ImGuiCol_Text, state_color);
            bool selected = ImGui::Selectable(label.c_str(), selected_index == i);
            ImGui::PopStyleColor();
            if (selected)
            {
                batch_selection.clear();
                batch_selection.add(b->id);
                selected_index = i;
            }
        }

        ImGui::NextColumn();

        // --- Right: details for selected batch ---
        ImGui::TextUnformatted("Details");
        ImGui::Separator();

        if (selected_index >= 0 && selected_index < static_cast<int>(batches.size()))
        {
            const BatchInfo* b = batches[selected_index];

            ImGui::Text("ID:   %s", b->id.to_string().c_str());
            ImGui::Text("Name: %s", b->name.c_str());
            ImGui::Text("File: %s", b->filename.string().c_str());

            const char* state_str = "";
            switch (b->state)
            {
            case BatchInfo::State::Unloaded:   state_str = "Unloaded";   break;
            case BatchInfo::State::Queued:     state_str = "Queued";     break;
            case BatchInfo::State::Loading:    state_str = "Loading";    break;
            case BatchInfo::State::Loaded:     state_str = "Loaded";     break;
            case BatchInfo::State::Unloading:  state_str = "Unloading";  break;
            case BatchInfo::State::Error:      state_str = "Error";      break;
            }

            ImGui::Text("State: %s", state_str);
            ImGui::Separator();

            ImGui::Text("Asset closure size: %zu",
                static_cast<size_t>(b->asset_closure_hdr.size()));
            ImGui::Text("Live entities (loaded only): %zu",
                static_cast<size_t>(b->live.size()));

            ImGui::Separator();

            // Selection -> assign to batch
            const auto& entity_selection = *ctx.entity_selection;
            const int selection_count = static_cast<int>(entity_selection.size());
            const bool can_queue = static_cast<bool>(ctx.command_queue);
            const bool can_assign = can_queue
                && ctx.entity_manager
                && selection_count > 0
                && b->state == BatchInfo::State::Loaded;

            ImGui::TextUnformatted("Selection");
            ImGui::Text("Selected entities: %d", selection_count);

            // Policy: only allow assignment to loaded batches to avoid orphaning entities.
            ImGui::BeginDisabled(!can_assign);
            if (ImGui::Button("Assign selection to this batch"))
            {
                editor::BatchActions::assign_entities_to_batch(
                    ctx,
                    b->id,
                    entity_selection.get_all());
            }
            ImGui::EndDisabled();
            ImGui::Separator();

            // Actions
            bool can_load = (b->state == BatchInfo::State::Unloaded ||
                b->state == BatchInfo::State::Error);
            bool can_unload = (b->state == BatchInfo::State::Loaded);
            bool can_save = (b->state == BatchInfo::State::Loaded);
            bool can_delete = (b->state == BatchInfo::State::Unloaded);

            // -- Load selected batch --
            if (can_load)
            {
                if (ImGui::Button("Load"))
                {
                    editor::BatchActions::load_batch(ctx, b->id);
                }
            }
            else
            {
                ImGui::BeginDisabled();
                ImGui::Button("Load");
                ImGui::EndDisabled();
            }

            // -- Unload selected batch --
            ImGui::SameLine();
            if (can_unload)
            {
                if (ImGui::Button("Unload"))
                {
                    editor::BatchActions::unload_batch(ctx, b->id);
                }
            }
            else
            {
                ImGui::BeginDisabled();
                ImGui::Button("Unload");
                ImGui::EndDisabled();
            }

            // -- Save selected batch --
            ImGui::SameLine();
            if (can_save)
            {
                if (ImGui::Button("Save"))
                {
                    br.save_batch(b->id, ctx);
                    // or br.queue_save_batch(b->id, ctx);
                }
            }
            else
            {
                ImGui::BeginDisabled();
                ImGui::Button("Save");
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (can_delete)
            {
                if (ImGui::Button("Delete"))
                {
                    editor::BatchActions::delete_batch(ctx, b->id);
                }
            }
            else
            {
                ImGui::BeginDisabled();
                ImGui::Button("Delete");
                ImGui::EndDisabled();
            }

            ImGui::Separator();

            // -- Load all batches --
            if (ImGui::Button("Load All"))
            {
                editor::BatchActions::load_all(ctx);
            }

            // -- Unload all batches --
            ImGui::SameLine();
            if (ImGui::Button("Unload All"))
            {
                editor::BatchActions::unload_all(ctx);
            }

            ImGui::SameLine();
            if (ImGui::Button("Save All"))
            {
                br.queue_save_all_async(ctx);
            }

            ImGui::Separator();

            // -- Rebuild --
            if (ImGui::Button("Rebuild"))
            {
                // Fire and forget, since we're on the main thread
                auto res = br.queue_rebuild_closure(b->id, ctx);
            }
        }

        ImGui::Columns(1);
        ImGui::End();
    }

    void GuiManager::draw_task_monitor(EngineContext& ctx) const
    {
        ImGui::Begin("Task Monitor");

        // Thread pool stats
        if (ctx.thread_pool)
        {
            auto* pool = ctx.thread_pool.get();

            ImGui::TextUnformatted("Thread Pool");
            ImGui::Separator();

            ImGui::BulletText("Threads:        %zu", pool->nbr_threads());
            ImGui::BulletText("Working:        %zu", pool->nbr_working_threads());
            ImGui::BulletText("Idle:           %zu", pool->nbr_idle_threads());
            ImGui::BulletText("Queued tasks:   %zu", pool->task_queue_size());
            ImGui::BulletText("Queue is empty: %s",
                pool->is_task_queue_empty() ? "yes" : "no");
        }
        else
        {
            ImGui::TextUnformatted("No thread pool available.");
        }

        ImGui::Separator();

        // Main thread queue stats
        if (ctx.main_thread_queue)
        {
            auto* mtq = ctx.main_thread_queue.get();

            ImGui::TextUnformatted("Main Thread Queue");
            ImGui::Separator();

            bool empty = mtq->empty();
            ImGui::BulletText("Pending tasks: %s", empty ? "0 (empty)" : "> 0");
        }
        else
        {
            ImGui::TextUnformatted("No main-thread queue available.");
        }

        ImGui::End();
    }

    void GuiManager::draw_command_queue(EngineContext& ctx) const
    {
        ImGui::Begin("Command Queue");

        gui::CommandQueueWidget widget{ ctx };
        widget.draw();

        ImGui::End();
    }

    void GuiManager::draw_scene_graph(EngineContext& ctx) const
    {
        // Validate selection
        {
            auto registry_sptr = ctx.entity_manager->registry_wptr().lock();
            if (!registry_sptr) return;
            ctx.entity_selection->remove_invalid([&](const ecs::Entity& entity)
                {
                    return entity.has_id() && registry_sptr->valid(entity);
                });
        }

        ImGui::Begin("Scene Hierarchy");

        gui::SceneTreeToolbarWidget toolbar{ ctx };
        toolbar.draw();

        ImGui::Separator();

        if (ImGui::BeginChild("SceneHierarchyRegion",
            ImVec2(0.0f, 0.0f),
            true,
            ImGuiWindowFlags_HorizontalScrollbar))
        {
            gui::SceneHierarchyWidget hierarchy{ ctx };
            hierarchy.draw();
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void GuiManager::draw_entity_inspector(EngineContext& ctx) const
    {
        ImGui::Begin("Entity Inspector");

        gui::EntityInspectorWidget inspector{ ctx };
        inspector.draw();

        ImGui::End();
    }

    void GuiManager::draw_editor_controls(EngineContext& ctx) const
    {
        ImGui::Begin("Editor Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        gui::SceneTreeToolbarWidget toolbar{ ctx };
        toolbar.draw_mode_row();
        ImGui::End();
    }

    void GuiManager::draw_animation_graph_visualizer(EngineContext& ctx) const
    {
        ImGui::Begin("Animation Graph Visualizer");

        // Guard rails: only draw when a valid entity and graph are available.
        if (!ctx.entity_selection || ctx.entity_selection->empty())
        {
            ImGui::TextDisabled("No entity selected.");
            ImGui::End();
            return;
        }

        auto registry = eeng::try_get_registry_ptr(ctx, "AnimationGraphVisualizer");
        if (!registry)
        {
            ImGui::TextDisabled("Registry unavailable.");
            ImGui::End();
            return;
        }

        const auto entity = ctx.entity_selection->first();
        if (!registry->valid(entity))
        {
            ImGui::TextDisabled("Selected entity is not valid.");
            ImGui::End();
            return;
        }

        auto* graph_comp = registry->try_get<ecs::AnimationGraphComponent>(entity);
        if (!graph_comp)
        {
            ImGui::TextDisabled("Selected entity has no AnimationGraphComponent.");
            ImGui::End();
            return;
        }

        if (!graph_comp->enabled)
        {
            ImGui::TextDisabled("AnimationGraphComponent is disabled.");
            ImGui::End();
            return;
        }

        if (!graph_comp->graph_ref.is_bound() || !graph_comp->instance.initialized)
        {
            ImGui::TextDisabled("Animation graph is not bound or initialized.");
            ImGui::End();
            return;
        }

        auto rm = eeng::try_get_resource_manager(ctx, "AnimationGraphVisualizer");
        if (!rm)
        {
            ImGui::TextDisabled("Resource manager unavailable.");
            ImGui::End();
            return;
        }

        eeng::try_read_asset_ref(
            *rm,
            graph_comp->graph_ref,
            ctx,
            "AnimationGraphVisualizer",
            "Missing AnimationGraphAsset for AnimationGraphVisualizer:",
            [&](const assets::AnimationGraphAsset& graph)
            {
                // Live param snapshot feeds the visualizer HUD and stick dots.
                const auto param_values = editor::detail::snapshot_param_values(graph, graph_comp->instance);
                editor::detail::draw_graph_visualizer(graph, *graph_comp, param_values, true);
            });

        ImGui::End();
    }

    void GuiManager::set_flag(GuiFlags flag, bool enabled)
    {
        flags[flag] = enabled;
    }

    bool GuiManager::is_flag_enabled(GuiFlags flag) const
    {
        auto it = flags.find(flag);
        return it != flags.end() ? it->second : false;
    }

} // namespace eeng
