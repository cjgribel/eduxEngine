// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

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

#include "EngineContextHelpers.hpp"
#include "editor/AnimationGraphComponentInspect.hpp"

#include "ThreadPool.hpp" // remove?
#include "MainThreadQueue.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <string>
//#include <future>

namespace eeng
{
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
        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowStorageWindow))
            draw_storage(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowProfiler))
            draw_profiler(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowResourceBrowser))
            draw_resource_browser(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowBatchRegistry))
            draw_batch_registry(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowTaskMonitor))
            draw_task_monitor(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowCommandQueue))
            draw_command_queue(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowSceneGraph))
            draw_scene_graph(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowAnimationGraphVisualizer))
            draw_animation_graph_visualizer(ctx);
    }

    void GuiManager::draw_profiler(EngineContext& ctx) const
    {
        ImGui::Begin("Profiler");
        gui::ProfilerWidget widget{ ctx };
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

        // --- Scene tree toolbar -------------------------------------------
        {
            gui::SceneTreeToolbarWidget toolbar{ ctx };
            toolbar.draw();
        }

        ImGui::Separator();

        static gui::VerticalSplitterWidget scene_splitter{};
        static bool scene_splitter_init = false;
        ImVec2 pane_avail = ImGui::GetContentRegionAvail();
        if (!scene_splitter_init)
        {
            const float line_h = ImGui::GetTextLineHeightWithSpacing();
            const float desired_top = line_h * 10.0f;
            scene_splitter.bottom_height = std::max(200.0f, pane_avail.y - desired_top);
            scene_splitter_init = true;
        }

        const float top_height = scene_splitter.calc_top_height(pane_avail.y);

        if (ImGui::BeginChild("SceneTopPane", ImVec2(0.0f, top_height), false))
        {
            auto& selection = *ctx.entity_selection;
            std::string selection_list;
            if (selection.empty())
            {
                selection_list = "(none)";
            }
            else
            {
                selection_list.reserve(selection.size() * 6); // tiny pre-reserve
                bool first = true;
                for (auto entity : selection.get_all())
                {
                    if (!first)
                        selection_list += ", ";
                    first = false;
                    selection_list += std::to_string(entity.to_integral());
                }
            }

            constexpr const char* selection_label = "Selected entities:";
            const float available_width = ImGui::GetContentRegionAvail().x;
            const float label_width = ImGui::CalcTextSize(selection_label).x + ImGui::GetStyle().ItemSpacing.x;
            const float wrap_width = std::max(0.0f, available_width - label_width);
            const float list_height = ImGui::CalcTextSize(selection_list.c_str(), nullptr, false, wrap_width).y;
            const float selection_height = std::max(ImGui::GetTextLineHeightWithSpacing(), list_height);
            const float available = ImGui::GetContentRegionAvail().y;
            const float hierarchy_height = std::max(0.0f, available - selection_height - ImGui::GetStyle().ItemSpacing.y);

            if (ImGui::BeginChild("SceneHierarchyRegion",
                ImVec2(0.0f, hierarchy_height),
                true,
                ImGuiWindowFlags_HorizontalScrollbar))
            {
                gui::SceneHierarchyWidget hierarchy{ ctx };
                hierarchy.draw();
            }
            ImGui::EndChild();

            ImGui::Separator();

            ImGui::TextUnformatted(selection_label);
            ImGui::SameLine();
            if (selection.empty())
                ImGui::TextDisabled("%s", selection_list.c_str());
            else
                ImGui::TextWrapped("%s", selection_list.c_str());
        }
        ImGui::EndChild();

        scene_splitter.draw_handle(pane_avail.y);

        if (ImGui::BeginChild("InspectorRegion", ImVec2(0.0f, scene_splitter.bottom_height), true))
        {
            gui::EntityInspectorWidget inspector{ ctx };
            inspector.draw();
        }
        ImGui::EndChild();

        // --- Command Queue (NOT HERE) ----------------------------------------

        // Inspector::inspect_command_queue(inspector);

// End window
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
