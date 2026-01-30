// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "glmcommon.hpp"
#include "imgui.h"

namespace eeng::gui
{
    inline ImGuiWindowFlags default_in_world_text_flags()
    {
        return ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            //ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNavFocus;
    }

    inline bool ImGuiPrintTextAt(
        const glm::ivec2& window_coords,
        int window_height,
        const char* str,
        const char* window_name,
        ImU32 color_bg,
        ImU32 color_text,
        ImVec2 pivot = ImVec2{ 0.0f, 0.0f },
        ImGuiWindowFlags extra_flags = 0)
    {
        if (!str || !window_name)
            return false;

        ImGui::SetNextWindowPos(
            ImVec2{ static_cast<float>(window_coords.x),
                    static_cast<float>(window_height - window_coords.y) },
            ImGuiCond_Always,
            pivot);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, color_bg);
        ImGui::PushStyleColor(ImGuiCol_Text, color_text);

        ImGuiWindowFlags flags = default_in_world_text_flags() | extra_flags;

        bool visible = ImGui::Begin(window_name, nullptr, flags);
        if (visible)
        {
            ImGui::TextUnformatted(str);
            ImGui::End();
        }

        ImGui::PopStyleColor(2);
        return visible;
    }

    inline bool ImGuiPrintTextAt(
        const glm::vec3& world_pos,
        const glm::mat4& VP_PROJ_V,
        int window_height,
        const char* str,
        const char* window_name,
        ImU32 color_bg,
        ImU32 color_text,
        ImVec2 pivot = ImVec2{ 0.0f, 0.0f },
        ImGuiWindowFlags extra_flags = 0,
        glm::ivec2* window_coords_out = nullptr)
    {
        glm::ivec2 window_coords;
        if (!glm_aux::window_coords_from_world_pos(world_pos, VP_PROJ_V, window_coords))
            return false;

        if (window_coords_out)
            *window_coords_out = window_coords;

        return ImGuiPrintTextAt(
            window_coords,
            window_height,
            str,
            window_name,
            color_bg,
            color_text,
            pivot,
            extra_flags);
    }
} // namespace eeng::gui
