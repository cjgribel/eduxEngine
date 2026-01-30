// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EngineOverlayGui.hpp"

#include "engineapi/EngineContext.hpp"
#include "engineapi/IGuiManager.hpp"
#include "LogManager.hpp"
#include "imgui.h"

namespace eeng
{
    namespace
    {
        void draw_log(EngineContext& ctx)
        {
            if (!ctx.log_manager)
                return;
            static_cast<LogManager&>(*ctx.log_manager).draw_gui_widget("Log");
        }

        void draw_engine_info(EngineContext& ctx)
        {
            if (!ctx.engine_config || !ctx.input_manager)
                return;

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
                if (ImGui::Checkbox("Wireframe", &wf))
                {
                    ctx.engine_config->set_flag(EngineFlag::WireframeRendering, wf);
                }

                // Debug logging
                ImGui::SameLine();
                bool debug_logging = ctx.engine_config->get_flag(EngineFlag::DebugLogging);
                if (ImGui::Checkbox("Debug logging", &debug_logging))
                {
                    ctx.engine_config->set_flag(EngineFlag::DebugLogging, debug_logging);
                }
            }

            if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // Read-only snapshot populated by RuntimePipeline each frame.
                if (!ctx.physics_monitor_stats || !ctx.physics_monitor_stats->valid)
                {
                    ImGui::TextDisabled("No physics stats available.");
                }
                else
                {
                    const auto& stats = *ctx.physics_monitor_stats;
                    ImGui::Text("Bodies: %zu", stats.body_count);
                    ImGui::Text("Collision objects: %d", stats.collision_objects);
                    ImGui::Text("Manifolds: %d", stats.manifolds);
                    ImGui::Text("Contact points: %d", stats.contact_points);
                    ImGui::Separator();
                    ImGui::Text("Dirty entities: %zu", stats.dirty_entities);
                    ImGui::Text("Event entities: %zu", stats.event_entities);
                    ImGui::Text("Tracked contacts: %zu", stats.tracked_contacts);
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
    }

    void EngineOverlayGui::draw(EngineContext& ctx) const
    {
        if (!ctx.gui_manager)
            return;

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowLogWindow))
            draw_log(ctx);

        if (ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowEngineInfo))
            draw_engine_info(ctx);
    }
}
