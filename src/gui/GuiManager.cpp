// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "GuiManager.hpp"
#include "LogManager.hpp"
#include "ResourceManager.hpp"
#include "AssetTreeViews.hpp"
#include "engineapi/SelectionManager.hpp"
#include "ThreadPool.hpp" // remove

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
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
        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowLogWindow))
            draw_log(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowStorageWindow))
            draw_storage(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowEngineInfo))
            draw_engine_info(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowResourceBrowser))
            draw_resource_browser(ctx);
    }

    void GuiManager::draw_log(EngineContext& ctx) const
    {
        static_cast<LogManager&>(*ctx.log_manager).draw_gui_widget("Log");
    }

    // Content
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

                    bool is_used = (i < used); // Replace if you track this precisely
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

    // Window content
    void draw_asset_flat_list(EngineContext& ctx)
    {
        auto& index = static_cast<ResourceManager&>(*ctx.resource_manager).asset_index();
        auto index_data = index.get_index_data();
        if (!index_data) return;

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
            if (ImGui::BeginTabItem("By Dependency"))
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
        if (ImGui::Button("Import")) { /* Import a batch of mock assets (META)  */ }
        ImGui::SameLine();
        if (ImGui::Button("Unimport")) { /* ... */ }
        ImGui::SameLine();
        // ->

        auto& resource_manager = static_cast<ResourceManager&>(*ctx.resource_manager);
        auto& content_tree = resource_manager.get_index_data()->trees->content_tree;
        if (ImGui::Button("Load"))
        {
#if 1
            // load_async - tracks load status of guid:s
            for (auto& guid : ctx.asset_selection->get_all()) {
                if (content_tree.is_root(guid))
                    resource_manager.load_async(guid, ctx);
            }
#endif
#if 0
            // NOTE
            // - Avoid capturing references
            // - Spamming the button causes multiple load commands
            // - Need to: track guid:s and their load status
            std::vector<std::future<void>> load_futures;
            for (auto& guid : ctx.asset_selection->get_all()) {
                load_futures.emplace_back(
                    ctx.thread_pool->queue_task([&]() {
                        if (content_tree.is_root(guid)) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            resource_manager.load(guid, ctx);
                        }
                        })
                );
            }
            //for (auto& future : load_futures) future.get(); // BLOCK AND WAIT
#endif
#if 0
            auto& resource_manager = static_cast<ResourceManager&>(*ctx.resource_manager);
            if (ctx.asset_selection->size() == 1) {
                auto& selected_guid = ctx.asset_selection->first();
                if (!resource_manager.storage().handle_for_guid(selected_guid))
                    resource_manager.load(selected_guid, ctx);
            }
#endif
        }
        ImGui::SameLine();
        if (ImGui::Button("Unload"))
        {
#if 1
            // load_async - tracks load status of guid:s
            for (auto& guid : ctx.asset_selection->get_all()) {
                if (content_tree.is_root(guid))
                    resource_manager.unload_async(guid, ctx);
            }
#endif
#if 0
            // Unloads ONLY IF ONE (1) IS SELECTED
            auto& resource_manager = static_cast<ResourceManager&>(*ctx.resource_manager);
            if (ctx.asset_selection->size() == 1) {
                const Guid& guid = ctx.asset_selection->first();
                if (resource_manager.storage().handle_for_guid(guid))
                    resource_manager.unload(guid, ctx);
            }
#endif
        }

        // <-
        ImGui::Separator();
        ImGui::TextUnformatted("Inspection:");
        ImGui::TextWrapped("Select an asset above to see details here...");
        ImGui::EndChild();

        ImGui::End();
    }

    void GuiManager::draw_content_tree(EngineContext& ctx) const
    {
        auto& resource_manager = static_cast<ResourceManager&>(*ctx.resource_manager);
        auto index_data = resource_manager.asset_index().get_index_data();
        if (!index_data) return;
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

                    // Get asset entry = meta data + paths
                    auto it = index_data->by_guid.find(guid);
                    if (it == index_data->by_guid.end()) return;
                    const AssetEntry& entry = *it->second;

                    // Asset loaded status
                    bool is_loaded = false;
                    if (auto maybe_handle = storage.handle_for_guid(guid))
                    {
                        auto handle = *maybe_handle;
                        is_loaded = storage.validate(handle);
                    }

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
                        ImGui::Text("Type: %s", entry.meta.type_name.c_str());
                        ImGui::Text("GUID: %s", entry.meta.guid.to_string().c_str());
                        ImGui::Text("File: %s", entry.relative_path.string().c_str());

                        auto status = resource_manager.get_status(guid);
                        switch (status.state) {
                        case LoadState::Unloaded: ImGui::Text("Not loaded"); break;
                        case LoadState::Unloading: ImGui::Text("Unloading"); break;
                        case LoadState::Loading:  ImGui::Text("Loading"); break;
                        case LoadState::Loaded:   ImGui::Text("Loaded"); break;
                        case LoadState::Failed:   ImGui::Text("Failed"); break;
                        }

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
