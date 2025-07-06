// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "GuiManager.hpp"
#include "LogManager.hpp"
#include "ResourceManager.hpp"

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
    }

    void GuiManager::draw_log(EngineContext& ctx) const
    {
        static_cast<LogManager&>(*ctx.log_manager).draw_gui_widget("Log");
    }

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
            const std::string type_name = std::string(meta_type.info().name());

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
        // This is a separate window to show a summary of all storage pools
        
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
                ImGui::TextUnformatted(meta_type.info().name().data());

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
