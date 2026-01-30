// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "EngineContext.hpp"
#include "editor/CommandQueue.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

namespace eeng::gui
{
    struct CommandQueueWidget
    {
        EngineContext& ctx;

        explicit CommandQueueWidget(EngineContext& ctx)
            : ctx(ctx)
        {
        }

        void draw()
        {
            auto* queue = ctx.command_queue.get();
            if (!queue)
            {
                ImGui::TextUnformatted("Command queue unavailable.");
                return;
            }

            size_t total = queue->size();
            size_t current_index = queue->get_current_index();
            if (current_index > total)
                current_index = total;

            const size_t executed = current_index;
            const size_t queued = total - executed;

            ImGui::Text("Executed: %zu", executed);
            ImGui::SameLine();
            ImGui::Text("Queued: %zu", queued);
            if (queue->has_in_flight())
            {
                ImGui::SameLine();
                ImGui::TextDisabled("Busy");
            }

            const bool can_undo = queue->can_undo();
            const bool can_redo = queue->has_next_command();
            const bool can_clear = total > 0 && !queue->has_in_flight();

            bool queue_changed = false;

            if (!can_undo) ImGui::BeginDisabled();
            if (ImGui::Button("Undo"))
            {
                queue->undo_last();
                queue_changed = true;
            }
            if (!can_undo) ImGui::EndDisabled();

            ImGui::SameLine();

            if (!can_redo) ImGui::BeginDisabled();
            if (ImGui::Button("Redo"))
            {
                queue->execute_next();
                queue_changed = true;
            }
            if (!can_redo) ImGui::EndDisabled();

            ImGui::SameLine();

            if (!can_clear) ImGui::BeginDisabled();
            if (ImGui::Button("Clear"))
            {
                queue->clear();
                queue_changed = true;
            }
            if (!can_clear) ImGui::EndDisabled();

            ImGui::Separator();

            if (queue_changed)
            {
                total = queue->size();
                current_index = queue->get_current_index();
                if (current_index > total)
                    current_index = total;
            }

            if (ImGui::BeginChild("CommandQueueList", ImVec2(0.0f, 0.0f), true))
            {
                if (total == 0)
                {
                    ImGui::TextDisabled("No commands queued.");
                }
                else
                {
                    const ImVec4 executed_color(0.5f, 0.85f, 0.5f, 1.0f);
                    const ImVec4 queued_color(0.9f, 0.65f, 0.2f, 1.0f);

                    for (size_t i = 0; i < total; ++i)
                    {
                        if (i >= queue->size())
                            break;
                        const bool is_executed = queue->is_executed(i);
                        const bool is_next = (i == current_index);
                        const ImVec4& color = is_executed ? executed_color : queued_color;
                        const char* marker = is_next ? ">" : " ";
                        const auto name = queue->get_name(i);
                        ImGui::TextColored(color, "%s %zu: %s", marker, i, name.c_str());
                    }
                }
                ImGui::EndChild();
            }
        }
    };
} // namespace eeng::gui
