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
        ImGui::Begin("Engine Info2");

        ImGui::Text("Hello");
        // draw mouse, framerate, fps cap, controller info etc. (same code as before),
        // but now accessed via ctx.input, ctx.vsync, ctx.window, etc.

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
