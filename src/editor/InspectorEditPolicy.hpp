// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "imgui.h"

#include <cstdint>
#include <unordered_map>

namespace eeng::editor
{
    enum class InspectCommitMode : std::uint8_t
    {
        Immediate,
        CommitOnEditEnd,
        // Backward-compatible alias (no timer semantics).
        ThrottleWhileActive = CommitOnEditEnd
    };

    inline bool should_commit_last_item_edit(
        bool widget_changed,
        InspectCommitMode mode = InspectCommitMode::CommitOnEditEnd,
        [[maybe_unused]] double throttle_seconds = 0.08)
    {
        if (!widget_changed)
            return false;

        if (mode == InspectCommitMode::Immediate)
            return true;

        // Commit only when the active edit session ends.
        return ImGui::IsItemDeactivatedAfterEdit();
    }

    template <class T, class DrawFn>
    inline bool edit_with_commit_on_end_buffered(
        const char* stable_item_label,
        const T& source_value,
        T& committed_value,
        DrawFn&& draw_widget)
    {
        static std::unordered_map<ImGuiID, T> s_pending_values;

        // Resolve id before draw so we can restore pending edits across frames.
        const ImGuiID item_id = ImGui::GetID(stable_item_label);
        T edit_value = source_value;
        if (auto it = s_pending_values.find(item_id); it != s_pending_values.end())
            edit_value = it->second;

        const bool changed = draw_widget(edit_value);

        if (ImGui::IsItemActivated())
            s_pending_values[item_id] = source_value;

        if (changed)
            s_pending_values[item_id] = edit_value;

        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            committed_value = s_pending_values.contains(item_id)
                ? s_pending_values[item_id]
                : edit_value;
            s_pending_values.erase(item_id);
            return true;
        }

        if (!ImGui::IsItemActive())
            s_pending_values.erase(item_id);

        return false;
    }
} // namespace eeng::editor
