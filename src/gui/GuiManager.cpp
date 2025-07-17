// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "GuiManager.hpp"
#include "LogManager.hpp"
#include "ResourceManager.hpp"
#include "AssetTreeViews.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

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
        // auto* index = ctx.resource_manager->asset_index();
        auto& index = static_cast<ResourceManager&>(*ctx.resource_manager).asset_index();
        //if (!index) return;

        // const auto& entries = index->get_entries();
        // auto entries = index.get_entries_snapshot();
        //auto entry_view = index.get_entries_view();
        auto index_data = index.get_index_data();
        if (!index_data) return;
        const auto& entries = index_data->entries; // *entry_view;

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

                // Future: Add preview, buttons for loading, etc.

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    void GuiManager::draw_resource_browser(EngineContext& ctx) const
    {
        ImGui::Begin("Resource Browser");

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
                //ImGui::TextUnformatted("(Unimplemented) Grouped by resource type");
                draw_dependency_tree(ctx);
                ImGui::EndTabItem();
            }

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

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

#if 0
    // Selection system
    struct GuiContext {
        std::unordered_set<Guid> selected_assets;
    };

    // in ImGui code:
    bool selected = ctx.gui_context.selected_assets.contains(entry.meta.guid);
    if (ImGui::Selectable(entry.meta.name.c_str(), selected)) {
        if (!selected)
            ctx.gui_context.selected_assets.insert(entry.meta.guid);
        else
            ctx.gui_context.selected_assets.erase(entry.meta.guid);
    }
#endif
#if 1
    void GuiManager::draw_dependency_tree(
        //const eeng::asset::builders::DependencyTree& tree,
        // const eeng::AssetIndexData& index_data,
        EngineContext& ctx) const
    {
        ImGui::Begin("Asset Dependency Tree");

        auto index_data = static_cast<ResourceManager&>(*ctx.resource_manager).asset_index().get_index_data();
        // auto& index_data = index_data.get_index_data();

        //auto index_data = index.get_index_data();
        if (!index_data) return;

        auto& tree = index_data->trees->dependency_tree; // index_data.trees->dependency_tree;

        std::function<void(size_t)> draw_node_recursive;
        draw_node_recursive = [&](size_t node_idx) {
            const Guid& guid = tree.get_payload_at(node_idx);
            auto it = index_data->by_guid.find(guid);
            if (it == index_data->by_guid.end()) return;

            const AssetEntry& entry = *it->second;

            ImGuiTreeNodeFlags flags = (tree.get_nbr_children(guid) == 0)
                ? ImGuiTreeNodeFlags_Leaf
                : 0;

            bool opened = ImGui::TreeNodeEx(entry.meta.name.c_str(), flags);

            if (opened)
            {
                ImGui::Text("Type: %s", entry.meta.type_name.c_str());
                ImGui::Text("GUID: %s", entry.meta.guid.to_string().c_str());
                ImGui::Text("File: %s", entry.relative_path.string().c_str());

                tree.traverse_children(node_idx, [&](const Guid& child_guid, size_t child_idx, size_t) {
                    draw_node_recursive(child_idx);
                    });

                ImGui::TreePop();
            }
            };

        // Start traversal from all roots
        size_t idx = 0;
        while (idx < tree.size())
        {
            draw_node_recursive(idx);
            idx += tree.get_branch_size(tree.get_payload_at(idx));
        }

        ImGui::End();
    }
#endif

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
