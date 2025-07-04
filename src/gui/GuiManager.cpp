// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "GuiManager.hpp"

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

    void GuiManager::draw_engine_info(EngineContext& ctx) const
    {
        if (!ctx.gui_manager->is_flag_enabled(eeng::GuiFlags::ShowEngineInfo))
            return;

        ImGui::Begin("Engine Info");

        // draw mouse, framerate, fps cap, controller info etc. (same code as before),
        // but now accessed via ctx.input, ctx.vsync, ctx.window, etc.

        if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Mouse state
            // TODO

            // Framerate
            ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            // Combo (drop-down) for fps settings
            // TODO

            // V-sync
            bool vsync = ctx.engine_config->is_vsync_enabled();
            if (ImGui::Checkbox("V-Sync", &vsync))
            {
                ctx.engine_config->set_vsync(vsync);
            }

            // Wireframe rendering
            ImGui::SameLine();
            bool wf = ctx.engine_config->is_wireframe_enabled();
            if (ImGui::Checkbox("Wireframe rendering", &wf))
            {
                ctx.engine_config->set_wireframe(wf);
            }
        }

        if (ImGui::CollapsingHeader("Controllers", ImGuiTreeNodeFlags_DefaultOpen))
        {

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
