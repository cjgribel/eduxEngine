// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "meta/MetaInfo.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
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

    inline bool has_snap(const DataMetaInfo* meta)
    {
        return meta && meta->ui_has_snap && meta->ui_snap_step > 0.0f;
    }

    inline float snap_scalar(float value, float step)
    {
        if (step <= 0.0f)
            return value;
        return std::round(value / step) * step;
    }

    inline float clamp_scalar_to_meta_range(float value, const DataMetaInfo* meta)
    {
        if (!meta || !meta->ui_has_range)
            return value;
        return std::clamp(value, meta->ui_range_min, meta->ui_range_max);
    }

    inline float apply_snap_from_meta(float value, const DataMetaInfo* meta)
    {
        if (!has_snap(meta))
            return clamp_scalar_to_meta_range(value, meta);
        return clamp_scalar_to_meta_range(snap_scalar(value, meta->ui_snap_step), meta);
    }

    template <std::size_t N>
    inline void apply_snap_from_meta(std::array<float, N>& values, const DataMetaInfo* meta)
    {
        for (float& value : values)
            value = apply_snap_from_meta(value, meta);
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
