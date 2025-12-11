// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "GuiManager.hpp"
#include "LogManager.hpp"
#include "ResourceManager.hpp"
#include "BatchRegistry.hpp"
#include "ecs/EntityManager.hpp"

#include "gui/SceneHierarchyWidget.hpp"
#include "gui/SceneHierarchyToolbarWidget.hpp"
#include "gui/EntityInspectorWidget.hpp"

#include "AssetTreeViews.hpp"

#include "engineapi/SelectionManager.hpp"
#include "ThreadPool.hpp" // remove?
#include "MainThreadQueue.hpp"
// #include "MetaInspect.hpp"

// Inspection TODO -> using a Comp command - need an Asset command?
#include "meta/MetaInspect.hpp"
#include "editor/EditComponentCommand.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
//#include <future>

namespace eeng
{
    using AssetBatch = std::deque<Guid>;

    namespace
    {
        // top-up, breadth-first
        AssetBatch get_branch_topdown(const Guid& guid, const EngineContext& ctx)
        {
            AssetBatch stack;
            auto& tree = ctx.resource_manager->get_index_data()->trees->content_tree;
            tree.traverse_breadthfirst(guid, [&](const Guid& guid, size_t) { stack.push_back(guid);  });
            return stack;
        }

        // bottom-up, breadth-first
        AssetBatch get_branch_bottomup(const Guid& guid, const EngineContext& ctx)
        {
            AssetBatch stack;
            auto& tree = ctx.resource_manager->get_index_data()->trees->content_tree;
            tree.traverse_breadthfirst(guid, [&](const Guid& guid, size_t) { stack.push_front(guid);  });
            return stack;
        }

        std::deque<Guid> compute_selected_bottomup_closure(const EngineContext& ctx)
        {
            std::deque<Guid> merged;
            std::unordered_set<Guid> seen;

            for (const Guid& guid : ctx.asset_selection->get_all())
            {
                auto branch = get_branch_bottomup(guid, ctx);

                for (const Guid& g : branch)
                {
                    if (seen.insert(g).second)
                        merged.push_back(g);
                }
            }

            return merged;
        }
    }

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
        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowLogWindow))
            draw_log(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowStorageWindow))
            draw_storage(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowEngineInfo))
            draw_engine_info(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowResourceBrowser))
            draw_resource_browser(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowBatchRegistry))
            draw_batch_registry(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowTaskMonitor))
            draw_task_monitor(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowSceneGraph))
            draw_scene_graph(ctx);
    }

    void GuiManager::draw_log(EngineContext& ctx) const
    {
        static_cast<LogManager&>(*ctx.log_manager).draw_gui_widget("Log");
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

        for (const auto& [type_id, pool_ptr] : storage)
        {
            const auto meta_type = entt::resolve(type_id);
            const auto type_name = std::string(meta_type.info().name());

            if (ImGui::TreeNode(type_name.c_str()))
            {
                const size_t capacity = pool_ptr->capacity();
                const size_t free = pool_ptr->count_free();
                const size_t used = capacity - free;
                const size_t elem_size = pool_ptr->element_size();

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
                    ImGui::TextUnformatted(pool_ptr->to_string().c_str());
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

            for (auto& [id_type, pool_ptr] : storage)
            {
                ImGui::TableNextRow();

                // Column 1: Type name
                ImGui::TableSetColumnIndex(0);
                auto meta_type = entt::resolve(id_type);
                const auto type_name = std::string(meta_type.info().name());
                ImGui::Text("%s", type_name.c_str());
                // ImGui::TextUnformatted(meta_type.info().name().data()); // has garbage at end

                // Column 2: Usage summary
                ImGui::TableSetColumnIndex(1);
                const size_t capacity = pool_ptr->capacity();
                const size_t used = capacity - pool_ptr->count_free();
                ImGui::Text("%zu / %zu", used, capacity);

                // Column 3: Colored cell bar
                ImGui::TableSetColumnIndex(2);
                DrawOccupancyBar(used, capacity);
            }

            ImGui::EndTable();
        }

        ImGui::End();

    }

    // TODO -> WIDGET not a window
    // Window content
    void draw_asset_flat_list(EngineContext& ctx)
    {
        auto& index = static_cast<ResourceManager&>(*ctx.resource_manager).asset_index();
        auto index_data = index.get_index_data();
        if (!index_data) {
            ImGui::Text("No asset index data available.");
            return;
        }

        const auto& entries = index_data->entries;
        const auto& by_guid = index_data->by_guid;

        ImGui::Text("Assets found: %zu", entries.size());
        ImGui::Separator();

        for (const auto& entry : entries)
        {
            ImGui::PushID(entry.meta.guid.raw()); // Avoid ImGui ID collisions

            if (ImGui::TreeNode(entry.meta.name.c_str()))
            {
                ImGui::Text("Type: %s", entry.meta.type_name.c_str());
                ImGui::Text("GUID: %s", entry.meta.guid.to_string().c_str());
                ImGui::Text("File: %s", entry.relative_path.string().c_str());

                // Show contained assets
                const auto& children = entry.meta.contained_assets;
                if (!children.empty())
                {
                    if (ImGui::TreeNode("Contained Assets"))
                    {
                        for (const auto& child_guid : children)
                        {
                            auto it = by_guid.find(child_guid);
                            if (it != by_guid.end())
                            {
                                const AssetEntry* child = it->second;
                                ImGui::BulletText("%s [%s]",
                                    child->meta.name.c_str(),
                                    child->meta.type_name.c_str());
                            }
                            else
                            {
                                ImGui::BulletText("Unknown GUID: %s", child_guid.to_string().c_str());
                            }
                        }
                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    void GuiManager::draw_resource_browser(EngineContext& ctx) const
    {
        ImGui::Begin("Resource Browser");

        // 1) persistent bottom‑pane height
        static float bottom_pane_height = 120.0f;
        const float splitter_thickness = 6.0f;

        // 2) figure out how much total vertical space we have
        ImVec2 avail = ImGui::GetContentRegionAvail();
        // clamp so neither pane shrinks below 50px
        float min_h = 50.0f;
        float max_h = avail.y - 50.0f;
        bottom_pane_height = std::min(std::max(bottom_pane_height, min_h), max_h);

        // 3) top pane size = remaining space after bottom + splitter
        float top_pane_height = avail.y - bottom_pane_height - splitter_thickness;

        // ─── TOP PANE ──────────────────────────────────────────────
        ImGui::BeginChild("##TopPane", ImVec2(0, top_pane_height), true);

        // Future: tabs or filter bar
        if (ImGui::BeginTabBar("ResourceViews"))
        {
            if (ImGui::BeginTabItem("Flat List"))
            {
                draw_asset_flat_list(ctx);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("(By Dependency)"))
            {
                // Selection
                ImGui::TextDisabled("Selected: ");
                for (auto& a : ctx.asset_selection->get_all())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s%s",
                        a.to_string().c_str(),
                        (a == ctx.asset_selection->last()) ? "" : ", ");
                }

                draw_content_tree(ctx);
                ImGui::EndTabItem();
            }

            ImGui::BeginDisabled();

            if (ImGui::BeginTabItem("By Type"))
            {
                ImGui::TextUnformatted("(Unimplemented) Grouped by resource type");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("File View"))
            {
                ImGui::TextUnformatted("(Unimplemented) Hierarchical file/folder view");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Reference View"))
            {
                ImGui::TextUnformatted("(Unimplemented) Dependencies between assets");
                ImGui::EndTabItem();
            }

            ImGui::EndDisabled();

            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        // ─── SPLITTER ──────────────────────────────────────────────
        ImGui::PushID("Splitter");  // isolate its ID
        ImGui::InvisibleButton("##SplitDrag", ImVec2(-1, splitter_thickness));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        if (hovered || active)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        // while dragging, adjust bottom height
        if (active)
            bottom_pane_height -= ImGui::GetIO().MouseDelta.y;
        // clamp again just in case
        bottom_pane_height = std::min(std::max(bottom_pane_height, min_h), max_h);
        ImGui::PopID();

        // draw a filled splitter band + center line for accent
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImU32 bg_col = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive
            : hovered ? ImGuiCol_SeparatorHovered
            : ImGuiCol_Separator);
        ImGui::GetWindowDrawList()->AddRectFilled(min, max, bg_col);
        float y = (min.y + max.y) * 0.5f;
        ImGui::GetWindowDrawList()
            ->AddLine({ min.x, y }, { max.x, y }, ImGui::GetColorU32(ImGuiCol_SeparatorActive));

        // ─── BOTTOM PANE ───────────────────────────────────────────
        ImGui::BeginChild("##BottomPane", ImVec2(0, bottom_pane_height), true);

        // Async busy flag
        auto& resource_manager = static_cast<ResourceManager&>(*ctx.resource_manager);
        //auto& content_tree = resource_manager.get_index_data()->trees->content_tree;
        // bool busy_ = ctx.asset_async_future.valid() &&
        //     ctx.asset_async_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
        bool busy = false; //resource_manager.is_busy();
        // assert(busy == busy_); // ensure consistency
        // ImGui::BeginDisabled(resource_manager.is_busy());
        if (busy) ImGui::BeginDisabled();
        // 
        if (ImGui::Button("Import")) { /* Import a batch of mock assets (META)  */ }
        ImGui::SameLine();
        if (ImGui::Button("Unimport")) { /* ... */ }
        ImGui::SameLine();
        // ->
        static auto batch_id1 = Guid::generate();
        static auto batch_id2 = Guid::generate();

        if (ImGui::Button("Load (Batch 1)"))
        {
            EENG_LOG(&ctx, "GUI load batch (1) %s", batch_id1.to_string().c_str());
            // auto batch_id = Guid::generate();
            auto to_reload = compute_selected_bottomup_closure(ctx);
            /*ctx.asset_async_future =*/ resource_manager.load_and_bind_async(to_reload, batch_id1, ctx);
        }
        ImGui::SameLine();
        if (ImGui::Button("Unload (Batch 1)"))
        {
            // auto batch_id = Guid::generate();
            auto to_reload = compute_selected_bottomup_closure(ctx);
            /*ctx.asset_async_future =*/ resource_manager.unbind_and_unload_async(to_reload, batch_id1, ctx);
        }
        // ---
        if (ImGui::Button("Import##batch2")) { /* Import a batch of mock assets (META)  */ }
        ImGui::SameLine();
        if (ImGui::Button("Unimport##batch2")) { /* ... */ }
        ImGui::SameLine();
        if (ImGui::Button("Load (Batch 2)"))
        {
            // auto batch_id = Guid::generate();
            auto to_reload = compute_selected_bottomup_closure(ctx);
            /*ctx.asset_async_future =*/ resource_manager.load_and_bind_async(to_reload, batch_id2, ctx);
        }
        ImGui::SameLine();
        if (ImGui::Button("Unload (Batch 2)"))
        {
            // auto batch_id = Guid::generate();
            auto to_reload = compute_selected_bottomup_closure(ctx);
            /*ctx.asset_async_future =*/ resource_manager.unbind_and_unload_async(to_reload, batch_id2, ctx);
        }

        // ImGui::SameLine();
        // if (ImGui::Button("Reload"))
        // {
        //     assert(ctx.thread_pool->nbr_threads() > 1);
        //     // auto batch_id = Guid::generate();
        //     auto to_reload = compute_selected_bottomup_closure(ctx);
        //     /*ctx.asset_async_future =*/ resource_manager.reload_and_rebind_async(to_reload, batch_id1, ctx);
        // }
        if (busy) ImGui::EndDisabled();

        // <-

        //ImGui::Text("Tasks in flight: %d", resource_manager.tasks_in_flight());
        // ImGui::Text("%s", resource_manager.is_busy() ? "(busy)" : "(idle)");
        // ImGui::SameLine();
        // ImGui::Text("Tasks in flight: %d", resource_manager.queued_tasks());

        // ImGui::Text("Thread utilization %zu/%zu, queued %zu",
        //     ctx.thread_pool->nbr_working_threads(),
        //     ctx.thread_pool->nbr_threads(),
        //     ctx.thread_pool->task_queue_size());

        ImGui::Separator();

        // TODO -> Widget
        // --- Asset inspector -------------------------------------------------

        // auto& resource_manager = static_cast<ResourceManager&>(*ctx.resource_manager);
        auto index_data = resource_manager.asset_index().get_index_data();
        if (index_data)
        {

            // auto& storage = resource_manager.storage();
            // const auto& tree = index_data->trees->content_tree;
            auto& selection = *ctx.asset_selection;

            ImGui::TextDisabled("%s", "Asset inspector");
            if (selection.size())
            {
                // ImGui::TextWrapped("Select an asset above to see details here...");
                auto guid = selection.first();
                auto guid_status = ctx.resource_manager->get_status(guid);

                auto it = index_data->by_guid.find(guid);
                if (it != index_data->by_guid.end())
                {
                    const AssetEntry& entry = *it->second;

                    bool valid = resource_manager.validate_asset(entry.meta.guid, ctx);
                    bool valid_rec = resource_manager.validate_asset_recursive(entry.meta.guid, ctx);
                    // --- Line 1: Status indicators ---
                    {
                        ImGui::TextColored(valid ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", valid ? "Valid" : "Invalid");
                        ImGui::SameLine();
                        ImGui::TextColored(valid_rec ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%s", valid_rec ? "Recursive" : "Recursive");
                        ImGui::SameLine();
                        ImVec4 state_color;
                        const char* state_str = nullptr;
                        switch (guid_status.state) {
                        case LoadState::Unloaded:  state_str = "Unloaded";  state_color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); break;
                        case LoadState::Unloading: state_str = "Unloading"; state_color = ImVec4(1.0f, 0.7f, 0.2f, 1.0f); break;
                        case LoadState::Loading:   state_str = "Loading";   state_color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f); break;
                        case LoadState::Loaded:    state_str = "Loaded";    state_color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); break;
                        case LoadState::Failed:    state_str = "Failed";    state_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
                        }
                        ImGui::TextColored(state_color, "%s", state_str);
                        if (!guid_status.error_message.empty())
                        {
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[%s]", guid_status.error_message.c_str());
                        }
                    }
                    // --- Line 2: Type + Ref count ---
                    {
                        ImGui::Text("Type: %s", entry.meta.type_name.c_str());
                        ImGui::SameLine();
                        // ImGui::Text("Ref Count: %d", guid_status.ref_count);
                        ImGui::Text("Nbr Leases: %d", resource_manager.total_leases(entry.meta.guid));
                    }
                    // --- Line 3: GUID ---
                    ImGui::Text("GUID: %s", entry.meta.guid.to_string().c_str());
                    // --- Line 4: Path ---
                    ImGui::Text("Path: %s", entry.relative_path.string().c_str());
                    // -- Line 5: Debug print content of certain types (if loaded) --
#if 1
                    if (auto metah_opt = resource_manager.storage().handle_for_guid(entry.meta.guid); metah_opt.has_value())
                    {
                        // Explicit type check + cast
                        if (auto h_opt = metah_opt->cast<mock::Mesh>(); h_opt.has_value())
                        {
                            // Use resource_manager.get_asset_ref/try_get_asset_ref -->
                            auto mesh = resource_manager.storage().get_ref(*h_opt);
                            ImGui::TextDisabled("%f, %f, %f", mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]);
                        }
                        //else ImGui::Text("Not a mock::Mesh");

                        // Generic access (any)
                        editor::InspectorState insp;
                        editor::ComponentCommandBuilder cmd_builder;

                        auto any = resource_manager.storage().get(*metah_opt);
                        auto type_name = get_meta_type_name(any.type());
                        // meta::inspect_any(any, insp, cmd, ctx);
                        const ImGuiTableFlags flags =
                            ImGuiTableFlags_BordersV |
                            ImGuiTableFlags_BordersOuterH |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_NoBordersInBody;
                        if (ImGui::BeginTable("InspectorTable", 2, flags)) // TODO -> part of insepctor?
                        {
                            if (insp.begin_node(type_name.c_str()))
                            {
                                // #ifdef USE_COMMANDS
                                                            // Reset meta command for component type
                                cmd_builder.reset().
                                    registry(ctx.entity_manager->registry_wptr())
                                    //.entity(ecs::Entity::EntityNull) // no entity for assets
                                    //.component(any.type().id()) // asset type - no comp type id!
                                    ;
                                // #endif
                                bool res = meta::inspect_any(any, insp, cmd_builder, ctx);
                                // ^ NOTE: Any edits are recorded -> cmd will be built -> Error, since no entity + comp!
                                //         KEEP EVERYTHING READ-ONLY until we have eg IEditCommandBuilder + AssetEditCommand!
                                

                                insp.end_node();
                            }
                            ImGui::EndTable();
                        }
                    }
                    // resource_manager.storage().get(entry.meta.guid);
#endif
                }
            }
            else
            {

            }
        }
        ImGui::EndChild();

        ImGui::End();
    }

    // TODO -> WIDGET not a window
    void GuiManager::draw_content_tree(EngineContext& ctx) const
    {
        auto& resource_manager = static_cast<ResourceManager&>(*ctx.resource_manager);
        auto index_data = resource_manager.asset_index().get_index_data();
        if (!index_data) {
            ImGui::Text("No asset index data available.");
            return;
        }
        auto& storage = resource_manager.storage();

        const auto& tree = index_data->trees->content_tree;
        auto& selection = *ctx.asset_selection;

        // Begin a 2‑column table
        if (ImGui::BeginTable("##ContentTree", 2,
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
        {
            ImVec2 table_pos = ImGui::GetCursorScreenPos();
            // Column 0: fixed width just big enough for the circle
            ImGui::TableSetupColumn("Loaded", ImGuiTableColumnFlags_WidthFixed, 20.0f);
            // Column 1: stretch for the tree
            ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthStretch);
            // no header row, skip TableHeadersRow()

            std::function<void(size_t)> draw_node_recursive;
            draw_node_recursive = [&](size_t node_idx)
                {
                    const Guid& guid = tree.get_payload_at(node_idx);
                    auto guid_status = ctx.resource_manager->get_status(guid);

                    // Get asset entry = meta data + paths
                    auto it = index_data->by_guid.find(guid);
                    if (it == index_data->by_guid.end()) return;
                    const AssetEntry& entry = *it->second;

                    // Asset loaded status
                    // Via load state
//                    bool is_loaded = guid_status.state == LoadState::Loaded ? true : false;
                    // Via Storage
                    bool is_loaded = false;
                    if (storage.handle_for_guid(guid)) is_loaded = true;

                    const bool is_leaf = tree.get_nbr_children(guid) == 0;
                    const bool is_selected = selection.contains(guid);

                    // Table: shift row and draw is_loaded-circle in absolute coordinates
                    ImGui::TableNextRow();
                    ImVec2 row_pos = ImGui::GetCursorScreenPos();  // indent + table cell origin
                    float  row_h = ImGui::GetFrameHeight();
                    float  r = row_h * 0.25f;
                    ImU32  col = is_loaded ? IM_COL32(0, 200, 0, 255) : IM_COL32(200, 0, 0, 255);
                    // draw circle at fixed X = table_pos.x
                    ImVec2 center{ table_pos.x + r + 2.0f, row_pos.y + row_h * 0.5f };
                    ImGui::GetWindowDrawList()->AddCircleFilled(center, r, col);

                    // Table: shift to column 1
                    ImGui::TableSetColumnIndex(1);

                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
                    if (is_leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet; // | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;

                    intptr_t id_int = static_cast<intptr_t>(guid.raw());
                    void* id_ptr = reinterpret_cast<void*>(id_int);

                    bool opened = ImGui::TreeNodeEx(id_ptr, flags, "%s", entry.meta.name.c_str());
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                    {
                        const bool ctrl = ImGui::GetIO().KeyCtrl;
                        if (ctrl && is_selected) selection.remove(guid);    // deselect
                        else if (ctrl) selection.add(guid);                 // multi-select
                        else { selection.clear(); selection.add(guid); }    // single-select
                    }
                    if (opened)
                    {
                        bool valid = resource_manager.validate_asset(entry.meta.guid, ctx);
                        bool valid_rec = resource_manager.validate_asset_recursive(entry.meta.guid, ctx);
                        // --- Line 1: Status indicators ---
                        {
                            ImGui::TextColored(valid ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", valid ? "Valid" : "Invalid");
                            ImGui::SameLine();
                            ImGui::TextColored(valid_rec ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "%s", valid_rec ? "Recursive" : "Recursive");
                            ImGui::SameLine();
                            ImVec4 state_color;
                            const char* state_str = nullptr;
                            switch (guid_status.state) {
                            case LoadState::Unloaded:  state_str = "Unloaded";  state_color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); break;
                            case LoadState::Unloading: state_str = "Unloading"; state_color = ImVec4(1.0f, 0.7f, 0.2f, 1.0f); break;
                            case LoadState::Loading:   state_str = "Loading";   state_color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f); break;
                            case LoadState::Loaded:    state_str = "Loaded";    state_color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); break;
                            case LoadState::Failed:    state_str = "Failed";    state_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
                            }
                            ImGui::SameLine();
                            ImGui::TextColored(state_color, "%s", state_str);
                            ImGui::SameLine();
                            ImGui::Text("Leases %u", resource_manager.total_leases(entry.meta.guid));
                            //
                            if (!guid_status.error_message.empty())
                            {
                                ImGui::SameLine();
                                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[%s]", guid_status.error_message.c_str());
                            }
                        }
                        // --- Line 2: Type + Ref count ---
                        {
                            ImGui::Text("Type: %s", entry.meta.type_name.c_str());
                            ImGui::SameLine();
                            // ImGui::Text("Ref Count: %d", guid_status.ref_count);
                            ImGui::Text("Nbr Leases: %d", resource_manager.total_leases(entry.meta.guid));
                        }
                        // --- Line 3: GUID ---
                        ImGui::Text("GUID: %s", entry.meta.guid.to_string().c_str());
                        // --- Line 4: Path ---
                        ImGui::Text("Path: %s", entry.relative_path.string().c_str());
                        // -- Line 5: Debug print content of certain types (if loaded) --
#if 1
                        if (auto metah_opt = resource_manager.storage().handle_for_guid(entry.meta.guid); metah_opt.has_value()) {
                            if (auto h_opt = metah_opt->cast<mock::Mesh>(); h_opt.has_value())
                            {
                                // Use resource_manager.get_asset_ref/try_get_asset_ref -->
                                auto mesh = resource_manager.storage().get_ref(*h_opt);
                                ImGui::TextDisabled("%f, %f, %f", mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]);
                            }
                            //else ImGui::Text("Not a mock::Mesh");
                        }
#endif
                        // --- Line 5: Contained assets ---
                        tree.traverse_children(node_idx, [&](const Guid&, size_t child_idx, size_t)
                            {
                                draw_node_recursive(child_idx);
                            });

                        ImGui::TreePop();
                    }
                };

            for (size_t root_idx : tree.get_roots())
            {
                draw_node_recursive(root_idx);
            }

            ImGui::EndTable();
        }
    }

    void GuiManager::draw_batch_registry(EngineContext& ctx) const
    {
        if (!ctx.batch_registry)
            return;

        auto& br = static_cast<BatchRegistry&>(*ctx.batch_registry);

        ImGui::Begin("Batch Registry");

        // Snapshot of batches
        const auto batches = br.list();
        if (batches.empty())
        {
            ImGui::TextUnformatted("No batches found. Create some in startup or via tools.");
            ImGui::End();
            return;
        }

        // Selection state (local to this window)
        static int selected_index = 0;
        if (selected_index >= static_cast<int>(batches.size()))
            selected_index = static_cast<int>(batches.size()) - 1;

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
            if (selected) selected_index = i;
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

            // Actions
            bool can_load = (b->state == BatchInfo::State::Unloaded ||
                b->state == BatchInfo::State::Error);
            bool can_unload = (b->state == BatchInfo::State::Loaded);
            bool can_save = (b->state == BatchInfo::State::Loaded);

            // -- Load selected batch --
            if (can_load)
            {
                if (ImGui::Button("Load"))
                {
                    br.queue_load(b->id, ctx); // fire-and-forget; strand serializes it
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
                    br.queue_unload(b->id, ctx);
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
                    // Either call sync save, or your async wrapper if you add it:
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

            ImGui::Separator();

            // -- Load all batches --
            if (ImGui::Button("Load All"))
            {
                // Using your async load-all helper:
                br.queue_load_all_async(ctx);
            }

            // -- Unload all batches --
            ImGui::SameLine();
            if (ImGui::Button("Unload All"))
            {
                br.queue_unload_all_async(ctx);
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

    void GuiManager::draw_engine_info(EngineContext& ctx) const
    {

        ImGui::Begin("Engine Info");

        if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Mouse state
            auto mouse = ctx.input_manager->GetMouseState();
            ImGui::Text("Mouse pos (%i, %i) %s%s",
                mouse.x,
                mouse.y,
                mouse.leftButton ? "L" : "",
                mouse.rightButton ? "R" : "");

            // Framerate
            ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            // Combo (drop-down) for fps settings
            static const char* items[] = { "10", "30", "60", "120", "Uncapped" };
            static int currentItem = 2;
            if (ImGui::BeginCombo("FPS cap##targetfps", items[currentItem]))
            {
                for (int i = 0; i < IM_ARRAYSIZE(items); i++)
                {
                    const bool isSelected = (currentItem == i);
                    if (ImGui::Selectable(items[i], isSelected))
                        currentItem = i;

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            float min_frametime_ms = 0.0f;
            if (currentItem == 0)
                min_frametime_ms = 1000.0f / 10;
            else if (currentItem == 1)
                min_frametime_ms = 1000.0f / 30;
            else if (currentItem == 2)
                min_frametime_ms = 1000.0f / 60;
            else if (currentItem == 3)
                min_frametime_ms = 1000.0f / 120;
            else {}
            ctx.engine_config->set_value(EngineValue::MinFrameTime, min_frametime_ms);

            // V-sync
            bool vsync = ctx.engine_config->get_flag(EngineFlag::VSync);
            if (ImGui::Checkbox("V-Sync", &vsync))
            {
                ctx.engine_config->set_flag(EngineFlag::VSync, vsync);
            }

            // Wireframe rendering
            ImGui::SameLine();
            bool wf = ctx.engine_config->get_flag(EngineFlag::WireframeRendering);
            if (ImGui::Checkbox("Wireframe rendering", &wf))
            {
                ctx.engine_config->set_flag(EngineFlag::WireframeRendering, wf);
            }
        }

        if (ImGui::CollapsingHeader("Controllers", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Controllers connected: %i", ctx.input_manager->GetConnectedControllerCount());

            for (auto& [id, state] : ctx.input_manager->GetControllers())
            {
                ImGui::PushID(id);
                ImGui::BeginChild("Controller", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 6), true);

                ImGui::Text("Controller %i: '%s'", id, state.name.c_str());
                ImGui::Text("Left stick:  X: %.2f  Y: %.2f", state.axisLeftX, state.axisLeftY);
                ImGui::Text("Right stick: X: %.2f  Y: %.2f", state.axisRightX, state.axisRightY);
                ImGui::Text("Triggers:    L: %.2f  R: %.2f", state.triggerLeft, state.triggerRight);
                std::string buttons;
                for (const auto& [buttonId, isPressed] : state.buttonStates)
                    buttons += "#" + std::to_string(buttonId) + "(" + (isPressed ? "1) " : "0) ");
                ImGui::Text("Buttons: %s", buttons.c_str());

                ImGui::EndChild();
                ImGui::PopID();
            }
        }

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
                    return entity.valid() && registry_sptr->valid(entity);
                });
        }

        ImGui::Begin("Scene Hierarchy");

        // --- Scene tree toolbar -------------------------------------------
        {
            gui::SceneTreeToolbarWidget toolbar{ ctx };
            toolbar.draw();
        }

        ImGui::Separator();

        // --- Scene graph hierarchy -------------------------------------------
        {
            static float hierarchy_height = 150.0f;

            if (ImGui::BeginChild("SceneHierarchyRegion",
                ImVec2(0.0f, hierarchy_height), // full width, fixed height
                true,                           // border
                ImGuiWindowFlags_HorizontalScrollbar))
            {
                gui::SceneHierarchyWidget hierarchy{ ctx };
                hierarchy.draw();
            }
            ImGui::EndChild();
        }

        // --- List selected ---------------------------------------------------

        ImGui::Separator();

        auto& selection = *ctx.entity_selection;
        if (selection.size() > 0)
        {

            ImGui::TextUnformatted("Selected entities (in order):");
            // ImGui::SameLine();

            std::string list;
            list.reserve(selection.size() * 6); // tiny pre-reserve

            bool first = true;
            for (auto entity : selection.get_all())
            {
                if (!first)
                    list += ", ";
                first = false;

                list += std::to_string(entity.to_integral());
            }

            // This will wrap nicely inside the window.
            ImGui::TextWrapped("%s", list.c_str());
        }

        ImGui::Separator();

        // --- Entity inspector ------------------------------------------------

        //static Editor::InspectorState inspector{};
        // if (Inspector::inspect_entity(inspector, *dispatcher)) {}

        {
            if (ImGui::BeginChild("InspectorRegion", ImVec2(0.0f, 0.0f), true))
            {
                // Entity inspector should draw *here* (no new window)
                gui::EntityInspectorWidget inspector{ ctx };
                inspector.draw();
            }
            ImGui::EndChild();
        }

        // --- Command Queue (NOT HERE) ----------------------------------------

        // Inspector::inspect_command_queue(inspector);

// End window
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

    // TODO
    void entity_inspector()
    {
        // Scene graph + entity inspector

        // renderUI()
#if 0
        static Editor::InspectorState inspector{};
        inspector.context = create_context();
        inspector.cmd_queue = cmd_queue;
        inspector.entity_selection.remove_invalid([&](const Entity& entity)
            {
                return !entity.is_null() && registry->valid(entity);
            });

        if (Inspector::inspect_entity(inspector, *dispatcher)) {}
#endif


    }

} // namespace eeng
