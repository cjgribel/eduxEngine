// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <queue>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "EngineContext.hpp"
#include "engineapi/SelectionManager.hpp"
#include "engineapi/EngineContextHelpers.hpp"
#include "editor/InspectorState.hpp"
#include "editor/AssetRefInspect.hpp"
#include "editor/TypeInspect.hpp"
#include "ecs/AnimationGraphComponent.hpp"
#include "assets/types/AnimationGraphAsset.hpp"

#include "imgui.h"

namespace eeng::editor
{
    namespace detail
    {
        struct GraphParamValue
        {
            assets::AnimGraphParamType type = assets::AnimGraphParamType::Invalid;
            float value = 0.0f;
            bool valid = false;
        };

        inline std::string format_float(float value, int precision = 2)
        {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(precision) << value;
            return ss.str();
        }

        inline std::unordered_map<std::string, GraphParamValue> snapshot_param_values(
            const assets::AnimationGraphAsset& graph,
            const eeng::ecs::AnimGraphInstance& instance)
        {
            // Snapshot runtime params by name so the visualizer can show live values.
            std::unordered_map<std::string, GraphParamValue> values;
            values.reserve(graph.params.size());

            std::size_t float_index = 0;
            std::size_t int_index = 0;
            std::size_t bool_index = 0;
            std::size_t trigger_index = 0;

            for (const auto& param : graph.params)
            {
                GraphParamValue entry{};
                entry.type = param.type;
                entry.valid = true;

                switch (param.type)
                {
                case assets::AnimGraphParamType::Float:
                    if (float_index < instance.float_params.size())
                        entry.value = instance.float_params[float_index];
                    ++float_index;
                    break;
                case assets::AnimGraphParamType::Int:
                    if (int_index < instance.int_params.size())
                        entry.value = static_cast<float>(instance.int_params[int_index]);
                    ++int_index;
                    break;
                case assets::AnimGraphParamType::Bool:
                    if (bool_index < instance.bool_params.size())
                        entry.value = instance.bool_params[bool_index] != 0 ? 1.0f : 0.0f;
                    ++bool_index;
                    break;
                case assets::AnimGraphParamType::Trigger:
                    if (trigger_index < instance.trigger_params.size())
                        entry.value = instance.trigger_params[trigger_index] != 0 ? 1.0f : 0.0f;
                    ++trigger_index;
                    break;
                default:
                    entry.valid = false;
                    break;
                }

                values[param.name] = entry;
            }
            return values;
        }

        inline bool try_get_param_value(
            const std::unordered_map<std::string, GraphParamValue>& values,
            std::string_view name,
            GraphParamValue& out_value)
        {
            auto it = values.find(std::string(name));
            if (it == values.end())
                return false;
            out_value = it->second;
            return it->second.valid;
        }

        inline const char* state_type_label(assets::AnimGraphStateType type)
        {
            switch (type)
            {
            case assets::AnimGraphStateType::Clip: return "Clip";
            case assets::AnimGraphStateType::Blend2: return "Blend2";
            case assets::AnimGraphStateType::BlendSpace1D: return "BlendSpace1D";
            case assets::AnimGraphStateType::BlendSpace2D: return "BlendSpace2D";
            default: return "Unknown";
            }
        }

        inline const char* condition_op_label(assets::AnimGraphConditionOp op)
        {
            switch (op)
            {
            case assets::AnimGraphConditionOp::Equal: return "==";
            case assets::AnimGraphConditionOp::NotEqual: return "!=";
            case assets::AnimGraphConditionOp::Less: return "<";
            case assets::AnimGraphConditionOp::Greater: return ">";
            case assets::AnimGraphConditionOp::LessEqual: return "<=";
            case assets::AnimGraphConditionOp::GreaterEqual: return ">=";
            case assets::AnimGraphConditionOp::IsTrue: return "is true";
            case assets::AnimGraphConditionOp::IsFalse: return "is false";
            default: return "?";
            }
        }

        inline std::string short_label(std::string_view text, std::size_t max_len)
        {
            if (text.size() <= max_len)
                return std::string(text);
            if (max_len <= 3)
                return std::string(text.substr(0, max_len));
            std::string out(text.substr(0, max_len - 3));
            out += "...";
            return out;
        }

        struct LayerLayout
        {
            std::vector<int> order;
            std::vector<int> depth;
            int max_depth = 0;
            std::vector<std::vector<int>> groups;
        };

        // Policy: Use a simple BFS depth ordering (entry -> outgoing) to reduce arrow overlap.
        inline LayerLayout build_layer_layout(const assets::AnimGraphLayer& layer)
        {
            LayerLayout layout{};
            const int state_count = static_cast<int>(layer.states.size());
            if (state_count <= 0)
                return layout;

            layout.depth.assign(state_count, -1);

            // Build a name->index map for transitions.
            std::unordered_map<std::string, int> name_to_index;
            name_to_index.reserve(layer.states.size());
            for (int i = 0; i < state_count; i++)
                name_to_index[layer.states[i].id] = i;

            // Build adjacency list from transitions (ignore invalid references).
            std::vector<std::vector<int>> adjacency(state_count);
            for (const auto& transition : layer.transitions)
            {
                auto from_it = name_to_index.find(transition.from);
                auto to_it = name_to_index.find(transition.to);
                if (from_it == name_to_index.end() || to_it == name_to_index.end())
                    continue;
                adjacency[from_it->second].push_back(to_it->second);
            }

            int root = -1;
            if (!layer.entry_state.empty())
            {
                auto root_it = name_to_index.find(layer.entry_state);
                if (root_it != name_to_index.end())
                    root = root_it->second;
            }

            if (root >= 0)
            {
                // BFS to assign depth per state (cycles are ignored once visited).
                std::queue<int> bfs;
                layout.depth[root] = 0;
                bfs.push(root);
                while (!bfs.empty())
                {
                    const int current = bfs.front();
                    bfs.pop();
                    const int next_depth = layout.depth[current] + 1;
                    for (int neighbor : adjacency[current])
                    {
                        if (neighbor < 0 || neighbor >= state_count)
                            continue;
                        if (layout.depth[neighbor] >= 0)
                            continue;
                        layout.depth[neighbor] = next_depth;
                        bfs.push(neighbor);
                    }
                }
            }

            int max_depth = -1;
            for (int d : layout.depth)
                if (d >= 0)
                    max_depth = std::max(max_depth, d);

            if (max_depth < 0)
            {
                // No valid root: keep original order in a single depth row.
                max_depth = 0;
                for (int i = 0; i < state_count; i++)
                    layout.depth[i] = 0;
            }
            else
            {
                // Place unreachable states at the far right.
                const int misc_depth = max_depth + 1;
                for (int i = 0; i < state_count; i++)
                {
                    if (layout.depth[i] < 0)
                        layout.depth[i] = misc_depth;
                }
                max_depth = misc_depth;
            }

            layout.max_depth = max_depth;

            // Preserve original ordering within each depth bucket.
            std::vector<std::vector<int>> buckets(static_cast<std::size_t>(max_depth + 1));
            for (int i = 0; i < state_count; i++)
                buckets[static_cast<std::size_t>(layout.depth[i])].push_back(i);

            layout.order.reserve(layer.states.size());
            for (const auto& bucket : buckets)
                for (int idx : bucket)
                    layout.order.push_back(idx);

            layout.groups = std::move(buckets);
            return layout;
        }

        inline ImU32 param_color(const GraphParamValue& value)
        {
            switch (value.type)
            {
            case assets::AnimGraphParamType::Bool:
            case assets::AnimGraphParamType::Trigger:
                return ImGui::GetColorU32(value.value != 0.0f
                    ? ImVec4(0.35f, 0.9f, 0.5f, 1.0f)
                    : ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
            case assets::AnimGraphParamType::Int:
                return ImGui::GetColorU32(ImVec4(0.95f, 0.75f, 0.35f, 1.0f));
            case assets::AnimGraphParamType::Float:
                return ImGui::GetColorU32(ImVec4(0.5f, 0.85f, 1.0f, 1.0f));
            default:
                return ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            }
        }

        inline std::string format_param_value(const GraphParamValue& value)
        {
            switch (value.type)
            {
            case assets::AnimGraphParamType::Bool:
            case assets::AnimGraphParamType::Trigger:
                return value.value != 0.0f ? "true" : "false";
            case assets::AnimGraphParamType::Int:
                return std::to_string(static_cast<int>(std::round(value.value)));
            case assets::AnimGraphParamType::Float:
                return format_float(value.value, 2);
            default:
                return "n/a";
            }
        }

        inline int find_state_index(const assets::AnimGraphLayer& layer, std::string_view id)
        {
            for (std::size_t i = 0; i < layer.states.size(); i++)
            {
                if (layer.states[i].id == id)
                    return static_cast<int>(i);
            }
            return -1;
        }

        inline void draw_arrow(ImDrawList* draw, const ImVec2& from, const ImVec2& to, ImU32 color, float thickness)
        {
            // Draw a simple line + triangle arrowhead between two points.
            const ImVec2 dir = ImVec2(to.x - from.x, to.y - from.y);
            const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len <= 0.0001f)
                return;

            const ImVec2 n = ImVec2(dir.x / len, dir.y / len);
            const ImVec2 perp = ImVec2(-n.y, n.x);
            const float arrow_size = 6.0f;
            const ImVec2 tip = to;
            const ImVec2 left = ImVec2(tip.x - n.x * arrow_size + perp.x * arrow_size * 0.5f,
                                       tip.y - n.y * arrow_size + perp.y * arrow_size * 0.5f);
            const ImVec2 right = ImVec2(tip.x - n.x * arrow_size - perp.x * arrow_size * 0.5f,
                                        tip.y - n.y * arrow_size - perp.y * arrow_size * 0.5f);

            draw->AddLine(from, to, color, thickness);
            draw->AddTriangleFilled(tip, left, right, color);
        }

        inline void draw_param_grid(
            ImDrawList* draw,
            const ImVec2& min,
            const ImVec2& max,
            const char* label,
            float value_x,
            float value_y,
            ImU32 frame_color,
            ImU32 dot_color,
            ImU32 text_color)
        {
            // Compact 2D grid with axes and a live dot in [-1,1] space.
            draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.1f, 1.0f)));
            draw->AddRect(min, max, frame_color);

            const ImVec2 center = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
            draw->AddLine(ImVec2(center.x, min.y), ImVec2(center.x, max.y), frame_color, 1.0f);
            draw->AddLine(ImVec2(min.x, center.y), ImVec2(max.x, center.y), frame_color, 1.0f);

            const float nx = (value_x * 0.5f) + 0.5f;
            const float ny = (-value_y * 0.5f) + 0.5f;
            const ImVec2 dot = ImVec2(min.x + nx * (max.x - min.x), min.y + ny * (max.y - min.y));
            draw->AddCircleFilled(dot, 3.5f, dot_color);

            draw->AddText(ImVec2(min.x, max.y + 2.0f), text_color, label);
        }

        inline void draw_blend_space_preview(
            ImDrawList* draw,
            const assets::AnimGraphState& state,
            const std::unordered_map<std::string, GraphParamValue>& params,
            const ImVec2& min,
            const ImVec2& max,
            ImU32 frame_color,
            ImU32 sample_color,
            ImU32 param_color)
        {
            // Draw the blend-space sample locations and current param in normalized space.
            draw->AddRect(min, max, frame_color);

            const float span_x = state.param_max_x - state.param_min_x;
            const float span_y = state.param_max_y - state.param_min_y;

            if (state.type == assets::AnimGraphStateType::BlendSpace1D)
            {
                for (const auto& sample : state.samples)
                {
                    const float t = span_x != 0.0f ? (sample.x - state.param_min_x) / span_x : 0.5f;
                    const float px = min.x + std::min(std::max(t, 0.0f), 1.0f) * (max.x - min.x);
                    const float py = (min.y + max.y) * 0.5f;
                    draw->AddCircleFilled(ImVec2(px, py), 2.5f, sample_color);
                }

                GraphParamValue value{};
                if (try_get_param_value(params, state.param_x, value))
                {
                    const float t = span_x != 0.0f ? (value.value - state.param_min_x) / span_x : 0.5f;
                    const float px = min.x + std::min(std::max(t, 0.0f), 1.0f) * (max.x - min.x);
                    const float py = (min.y + max.y) * 0.5f;
                    draw->AddCircleFilled(ImVec2(px, py), 3.5f, param_color);
                }
            }

            if (state.type == assets::AnimGraphStateType::BlendSpace2D)
            {
                for (const auto& sample : state.samples)
                {
                    const float tx = span_x != 0.0f ? (sample.x - state.param_min_x) / span_x : 0.5f;
                    const float ty = span_y != 0.0f ? (sample.y - state.param_min_y) / span_y : 0.5f;
                    const float px = min.x + std::min(std::max(tx, 0.0f), 1.0f) * (max.x - min.x);
                    const float py = max.y - std::min(std::max(ty, 0.0f), 1.0f) * (max.y - min.y);
                    draw->AddCircleFilled(ImVec2(px, py), 2.5f, sample_color);
                }

                GraphParamValue value_x{};
                GraphParamValue value_y{};
                if (try_get_param_value(params, state.param_x, value_x)
                    && try_get_param_value(params, state.param_y, value_y))
                {
                    const float tx = span_x != 0.0f ? (value_x.value - state.param_min_x) / span_x : 0.5f;
                    const float ty = span_y != 0.0f ? (value_y.value - state.param_min_y) / span_y : 0.5f;
                    const float px = min.x + std::min(std::max(tx, 0.0f), 1.0f) * (max.x - min.x);
                    const float py = max.y - std::min(std::max(ty, 0.0f), 1.0f) * (max.y - min.y);
                    draw->AddCircleFilled(ImVec2(px, py), 3.5f, param_color);
                }
            }
        }

        inline void draw_graph_visualizer(
            const assets::AnimationGraphAsset& graph,
            const eeng::ecs::AnimationGraphComponent& comp,
            const std::unordered_map<std::string, GraphParamValue>& params,
            bool fill_parent)
        {
            struct NodeRect
            {
                ImVec2 min;
                ImVec2 max;
                ImVec2 center;
            };

            // Layout and sizing constants for the node grid.
            const float node_w = 170.0f;
            const float node_h = 86.0f;
            const float node_gap = 24.0f;
            const float layer_gap = 30.0f;
            const float pad = 12.0f;
            const float group_gap_base = node_gap * 1.8f;

            // Precompute a lightweight BFS layout per layer (order only).
            std::vector<LayerLayout> layouts;
            layouts.reserve(graph.layers.size());

            std::size_t max_states = 0;
            for (const auto& layer : graph.layers)
            {
                layouts.push_back(build_layer_layout(layer));
                max_states = std::max(max_states, layer.states.size());
            }

            const float font_size = ImGui::GetFontSize();
            const float line_height = font_size + 2.0f;
            const float header_height = std::max(40.0f, pad + line_height * static_cast<float>(graph.params.size()));
            const float layer_label_pad = font_size + 4.0f;
            const float layer_stride = node_h + layer_gap + layer_label_pad;

            // Base content width: sum per-layer group widths (depth columns) + gaps.
            float base_content_w = 0.0f;
            int max_group_gaps = 0;
            for (const auto& layout : layouts)
            {
                if (layout.groups.empty())
                    continue;
                int group_gaps = static_cast<int>(layout.groups.size()) - 1;
                max_group_gaps = std::max(max_group_gaps, group_gaps);

                float layer_width = 0.0f;
                for (std::size_t g = 0; g < layout.groups.size(); g++)
                {
                    const float group_w = layout.groups[g].empty()
                        ? 0.0f
                        : layout.groups[g].size() * node_w + (layout.groups[g].size() - 1) * node_gap;
                    if (g > 0)
                        layer_width += group_gap_base;
                    layer_width += group_w;
                }
                base_content_w = std::max(base_content_w, layer_width);
            }
            base_content_w = std::max(base_content_w, max_states * node_w
                + (max_states > 0 ? (max_states - 1) * node_gap : 0.0f));
            const float content_h = pad * 2.0f + header_height
                + static_cast<float>(graph.layers.size()) * layer_stride;

            const float min_height = 220.0f;
            const float max_height = 420.0f;
            float desired_height = 0.0f;
            if (fill_parent)
            {
                // Fill available height in the standalone visualizer window.
                const float avail_h = ImGui::GetContentRegionAvail().y;
                desired_height = std::max(avail_h, min_height);
            }
            else
            {
                // Keep a compact height when embedded in the inspector.
                desired_height = std::min(std::max(content_h, min_height), max_height);
            }

            const ImGuiWindowFlags child_flags = ImGuiWindowFlags_HorizontalScrollbar;
            ImGui::BeginChild("##anim_graph_viz", ImVec2(0.0f, desired_height), true, child_flags);

            // Reserve a canvas for the full graph layout.
            ImVec2 canvas_size = ImGui::GetContentRegionAvail();
            canvas_size.x = std::max(canvas_size.x, base_content_w);
            canvas_size.y = std::max(canvas_size.y, content_h);

            float group_gap = group_gap_base;
            float layout_w = base_content_w + pad * 2.0f;
            if (max_group_gaps > 0)
            {
                // Stretch gaps between depth groups when the window is wider.
                const float usable = canvas_size.x - pad * 2.0f;
                if (usable > base_content_w)
                {
                    const float extra = usable - base_content_w;
                    group_gap = group_gap_base + extra / static_cast<float>(max_group_gaps);
                    layout_w = base_content_w + pad * 2.0f + extra;
                }
            }
            canvas_size.x = std::max(canvas_size.x, layout_w);

            ImGui::InvisibleButton("##graph_canvas", canvas_size);

            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 canvas_min = ImGui::GetItemRectMin();
            const ImVec2 canvas_max = ImGui::GetItemRectMax();

            const ImU32 bg_col = ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.1f, 1.0f));
            const ImU32 frame_col = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
            const ImU32 layer_col = ImGui::GetColorU32(ImVec4(0.14f, 0.14f, 0.18f, 0.7f));
            const ImU32 text_col = ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.92f, 1.0f));
            const ImU32 text_dim = ImGui::GetColorU32(ImVec4(0.65f, 0.65f, 0.7f, 1.0f));

            draw->AddRectFilled(canvas_min, canvas_max, bg_col);
            draw->AddRect(canvas_min, canvas_max, frame_col);

            draw->PushClipRect(canvas_min, canvas_max, true);

            // Header: current parameter values (left side).
            float header_y = canvas_min.y + pad;
            for (const auto& param : graph.params)
            {
                GraphParamValue value{};
                const bool has_value = try_get_param_value(params, param.name, value);
                const std::string value_text = has_value ? format_param_value(value) : "n/a";
                std::string line = param.name + " = " + value_text;
                ImU32 color = has_value ? param_color(value) : text_dim;
                draw->AddText(ImVec2(canvas_min.x + pad, header_y), color, line.c_str());
                header_y += line_height;
            }

            // Visualize stick-like param pairs if present.
            GraphParamValue move_x{};
            GraphParamValue move_y{};
            GraphParamValue aim_x{};
            GraphParamValue aim_y{};
            if (try_get_param_value(params, "U_LT", move_x) && try_get_param_value(params, "V_LT", move_y))
            {
                const ImVec2 box_size(70.0f, 70.0f);
                const ImVec2 box_min(canvas_max.x - pad * 2.0f - box_size.x * 2.0f, canvas_min.y + pad);
                const ImVec2 box_max(box_min.x + box_size.x, box_min.y + box_size.y);
                draw_param_grid(draw, box_min, box_max, "Move", move_x.value, move_y.value, frame_col, text_col, text_col);
            }

            if (try_get_param_value(params, "U_RT", aim_x) && try_get_param_value(params, "V_RT", aim_y))
            {
                const ImVec2 box_size(70.0f, 70.0f);
                const ImVec2 box_min(canvas_max.x - pad - box_size.x, canvas_min.y + pad);
                const ImVec2 box_max(box_min.x + box_size.x, box_min.y + box_size.y);
                draw_param_grid(draw, box_min, box_max, "Aim", aim_x.value, aim_y.value, frame_col, text_col, text_col);
            }

            // Layout nodes per layer.
            std::vector<std::vector<NodeRect>> state_rects(graph.layers.size());
            for (std::size_t layer_index = 0; layer_index < graph.layers.size(); layer_index++)
            {
                const auto& layer = graph.layers[layer_index];
                const float row_y = canvas_min.y + pad + header_height + layer_index * layer_stride + layer_label_pad;
                const float label_y = row_y - layer_label_pad;
                const ImVec2 layer_min(canvas_min.x + pad, row_y - 6.0f);
                const ImVec2 layer_max(canvas_min.x + pad + layout_w - pad * 2.0f, row_y + node_h + 6.0f);

                draw->AddRectFilled(layer_min, layer_max, layer_col, 6.0f);
                std::string layer_label = "Layer " + std::to_string(layer_index);
                if (!layer.name.empty())
                    layer_label += " (" + layer.name + ")";
                draw->AddText(ImVec2(layer_min.x + 4.0f, label_y), text_dim, layer_label.c_str());

                state_rects[layer_index].resize(layer.states.size());
                const int active_state = layer_index < comp.instance.layers.size()
                    ? comp.instance.layers[layer_index].state
                    : -1;

                // BFS ordering ensures entry and its direct transitions appear first.
                const auto& layout = layouts[layer_index];
                // First pass: compute node rectangles for all states (used by arrows).
                float group_x = canvas_min.x + pad;
                for (std::size_t group_index = 0; group_index < layout.groups.size(); group_index++)
                {
                    const auto& group = layout.groups[group_index];
                    for (std::size_t within_group = 0; within_group < group.size(); within_group++)
                    {
                        const int state_index = group[within_group];
                        const float node_x = group_x + within_group * (node_w + node_gap);
                        const ImVec2 node_min(node_x, row_y);
                        const ImVec2 node_max(node_x + node_w, row_y + node_h);
                        const ImVec2 node_center((node_min.x + node_max.x) * 0.5f, (node_min.y + node_max.y) * 0.5f);
                        state_rects[layer_index][static_cast<std::size_t>(state_index)] = NodeRect{ node_min, node_max, node_center };
                    }

                    // Advance to the next depth group column.
                    if (!group.empty())
                        group_x += group.size() * node_w + (group.size() - 1) * node_gap;
                    if (group_index + 1 < layout.groups.size())
                        group_x += group_gap;
                }

                // Draw transitions first so lines appear behind node boxes.
                const auto& runtime = layer_index < comp.instance.layers.size()
                    ? comp.instance.layers[layer_index]
                    : eeng::ecs::AnimGraphLayerRuntime{};

                struct ActiveLine
                {
                    ImVec2 start;
                    ImVec2 end;
                    ImU32 color;
                    float thickness;
                    std::string label;
                };
                std::vector<ActiveLine> active_lines;

                const bool has_last_transition = runtime.last_transition_ttl > 0.0f;
                const bool has_any_transition = runtime.transition.active || has_last_transition;
                const auto& active_transition = runtime.transition.active ? runtime.transition : runtime.last_transition;

                for (const auto& transition : layer.transitions)
                {
                    const int from_index = find_state_index(layer, transition.from);
                    const int to_index = find_state_index(layer, transition.to);
                    if (from_index < 0 || to_index < 0)
                        continue;
                    if (from_index >= static_cast<int>(state_rects[layer_index].size())
                        || to_index >= static_cast<int>(state_rects[layer_index].size()))
                        continue;

                    const NodeRect& from_rect = state_rects[layer_index][from_index];
                    const NodeRect& to_rect = state_rects[layer_index][to_index];
                    const ImVec2 from = from_rect.center;
                    const ImVec2 to = to_rect.center;
                    const bool is_active = has_any_transition
                        && active_transition.from == from_index
                        && active_transition.to == to_index;

                    // Offset arrow endpoints outside node rectangles to avoid covering labels.
                    const ImVec2 dir = ImVec2(to.x - from.x, to.y - from.y);
                    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    ImVec2 start = from;
                    ImVec2 end = to;
                    if (len > 0.0001f)
                    {
                        const ImVec2 n = ImVec2(dir.x / len, dir.y / len);
                        const float pad_out = 8.0f;
                        const ImVec2 half_from((from_rect.max.x - from_rect.min.x) * 0.5f,
                                               (from_rect.max.y - from_rect.min.y) * 0.5f);
                        const ImVec2 half_to((to_rect.max.x - to_rect.min.x) * 0.5f,
                                             (to_rect.max.y - to_rect.min.y) * 0.5f);
                        const float sx = std::fabs(n.x) > 0.0001f ? (half_from.x + pad_out) / std::fabs(n.x) : (half_from.y + pad_out);
                        const float sy = std::fabs(n.y) > 0.0001f ? (half_from.y + pad_out) / std::fabs(n.y) : (half_from.x + pad_out);
                        const float from_scale = std::min(sx, sy);
                        const float tx = std::fabs(n.x) > 0.0001f ? (half_to.x + pad_out) / std::fabs(n.x) : (half_to.y + pad_out);
                        const float ty = std::fabs(n.y) > 0.0001f ? (half_to.y + pad_out) / std::fabs(n.y) : (half_to.x + pad_out);
                        const float to_scale = std::min(tx, ty);
                        start = ImVec2(from.x + n.x * from_scale, from.y + n.y * from_scale);
                        end = ImVec2(to.x - n.x * to_scale, to.y - n.y * to_scale);
                    }

                    const ImU32 line_col = is_active
                        ? ImGui::GetColorU32(ImVec4(0.95f, 0.8f, 0.3f, 1.0f))
                        : ImGui::GetColorU32(ImVec4(0.55f, 0.55f, 0.65f, 1.0f));
                    const float thickness = is_active ? 2.0f : 1.2f;
                    if (is_active)
                    {
                        std::string label;
                        if (active_transition.duration > 0.0f)
                        {
                            label = format_float(active_transition.time, 2);
                            label += "/";
                            label += format_float(active_transition.duration, 2);
                        }
                        active_lines.push_back({ start, end, line_col, thickness, std::move(label) });
                    }
                    else if (!has_any_transition)
                    {
                        draw_arrow(draw, start, end, line_col, thickness);
                    }
                }

                // Second pass: draw nodes on top, with fade for non-participating states.
                const bool transition_active = runtime.transition.active || has_last_transition;
                group_x = canvas_min.x + pad;
                for (std::size_t group_index = 0; group_index < layout.groups.size(); group_index++)
                {
                    const auto& group = layout.groups[group_index];
                    for (std::size_t within_group = 0; within_group < group.size(); within_group++)
                    {
                        const int state_index = group[within_group];
                        const auto& state = layer.states[static_cast<std::size_t>(state_index)];
                        const NodeRect& rect = state_rects[layer_index][static_cast<std::size_t>(state_index)];

                        const bool is_active = state_index == active_state;
                        const bool is_transition_state = transition_active
                            && (state_index == active_transition.from || state_index == active_transition.to);
                        const float fade = 1.0f;

                        ImVec4 base_col;
                        switch (state.type)
                        {
                        case assets::AnimGraphStateType::Clip: base_col = ImVec4(0.18f, 0.18f, 0.22f, 1.0f); break;
                        case assets::AnimGraphStateType::Blend2: base_col = ImVec4(0.18f, 0.24f, 0.28f, 1.0f); break;
                        case assets::AnimGraphStateType::BlendSpace1D: base_col = ImVec4(0.22f, 0.2f, 0.16f, 1.0f); break;
                        case assets::AnimGraphStateType::BlendSpace2D: base_col = ImVec4(0.22f, 0.18f, 0.24f, 1.0f); break;
                        default: base_col = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); break;
                        }

                        ImVec4 active_col = base_col;
                        active_col.x = std::min(active_col.x + 0.1f, 0.9f);
                        active_col.y = std::min(active_col.y + 0.1f, 0.9f);
                        active_col.z = std::min(active_col.z + 0.1f, 0.9f);

                        ImVec4 border_color = is_active || is_transition_state
                            ? ImVec4(0.9f, 0.85f, 0.35f, 1.0f)
                            : ImVec4(0.4f, 0.4f, 0.5f, 1.0f);

                        const ImU32 fill_col = ImGui::GetColorU32(is_active ? active_col : base_col);
                        const ImU32 border_col = ImGui::GetColorU32(border_color);

                        draw->AddRectFilled(rect.min, rect.max, fill_col, 6.0f);
                        draw->AddRect(rect.min, rect.max, border_col, 6.0f, 0, 2.0f);

                        const ImVec4 text_main = ImVec4(0.9f, 0.9f, 0.92f, 1.0f);
                        const ImVec4 text_dim_local = ImVec4(0.65f, 0.65f, 0.7f, 1.0f);

                        std::string header = short_label(state.id, 18);
                        draw->AddText(ImVec2(rect.min.x + 6.0f, rect.min.y + 4.0f), ImGui::GetColorU32(text_main), header.c_str());
                        draw->AddText(ImVec2(rect.min.x + 6.0f, rect.min.y + 4.0f + line_height), ImGui::GetColorU32(text_dim_local), state_type_label(state.type));

                        std::string detail;
                        if (state.type == assets::AnimGraphStateType::Clip)
                            detail = short_label(state.clip, 20);
                        else if (state.type == assets::AnimGraphStateType::Blend2)
                            detail = short_label(state.clip0 + " | " + state.clip1, 20);
                        else if (state.type == assets::AnimGraphStateType::BlendSpace1D)
                            detail = state.param_x + " (" + std::to_string(state.samples.size()) + ")";
                        else if (state.type == assets::AnimGraphStateType::BlendSpace2D)
                            detail = state.param_x + "," + state.param_y + " (" + std::to_string(state.samples.size()) + ")";

                        if (!detail.empty())
                            draw->AddText(ImVec2(rect.min.x + 6.0f, rect.min.y + 4.0f + line_height * 2.0f),
                                          ImGui::GetColorU32(text_dim_local), detail.c_str());

                        if (state.type == assets::AnimGraphStateType::BlendSpace1D
                            || state.type == assets::AnimGraphStateType::BlendSpace2D)
                        {
                            // Mini preview of blend samples and the current parameter dot.
                            const float grid_top = rect.min.y + 4.0f + line_height * 3.0f;
                            const ImVec2 grid_min(rect.min.x + 6.0f, grid_top);
                            const ImVec2 grid_max(rect.max.x - 6.0f, rect.max.y - 6.0f);
                            const ImU32 preview_frame = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
                            const ImU32 preview_sample = ImGui::GetColorU32(ImVec4(0.65f, 0.65f, 0.75f, 1.0f));
                            const ImU32 preview_param = ImGui::GetColorU32(ImVec4(0.9f, 0.85f, 0.35f, 1.0f));
                            draw_blend_space_preview(draw, state, params, grid_min, grid_max,
                                preview_frame,
                                preview_sample,
                                preview_param);
                        }
                    }

                    if (!group.empty())
                        group_x += group.size() * node_w + (group.size() - 1) * node_gap;
                    if (group_index + 1 < layout.groups.size())
                        group_x += group_gap;
                }

                // Draw active transitions on top so they remain visible.
                for (const auto& line : active_lines)
                {
                    draw_arrow(draw, line.start, line.end, line.color, line.thickness);
                    if (!line.label.empty())
                    {
                        const ImVec2 mid((line.start.x + line.end.x) * 0.5f, (line.start.y + line.end.y) * 0.5f);
                        draw->AddText(ImVec2(mid.x + 4.0f, mid.y - 14.0f), line.color, line.label.c_str());
                    }
                }
            }

            draw->PopClipRect();
            ImGui::EndChild();
        }
    }

    inline bool inspect_AnimationGraphComponent(
        entt::meta_any& any,
        InspectorState& inspector,
        EngineContext& ctx)
    {
        auto* comp = any.try_cast<eeng::ecs::AnimationGraphComponent>();
        if (!comp)
            return false;

        bool modified = false;
        constexpr float kParamCommitEpsilon = 0.1f;

        eeng::ecs::AnimationGraphComponent* live_comp = nullptr;
        if (ctx.entity_selection && !ctx.entity_selection->empty())
        {
            auto registry_sp = eeng::try_get_registry(ctx, "AnimationGraphComponentInspect");
            if (registry_sp && registry_sp->valid(ctx.entity_selection->first()))
                live_comp = registry_sp->try_get<eeng::ecs::AnimationGraphComponent>(ctx.entity_selection->first());
        }

        inspector.begin_leaf("name");
        modified |= inspect_type(comp->name, inspector);
        inspector.end_leaf();

        inspector.begin_leaf("graph_ref");
        auto ref_any = entt::forward_as_meta(comp->graph_ref);
        modified |= inspect_AssetRef<eeng::assets::AnimationGraphAsset>(ref_any, inspector, ctx);
        inspector.end_leaf();

        inspector.begin_leaf("enabled");
        modified |= inspect_type(comp->enabled, inspector);
        inspector.end_leaf();

        if (!comp->graph_ref.is_bound() || !comp->instance.initialized)
            return modified;

        auto rm = eeng::try_get_resource_manager(ctx, "AnimationGraphComponentInspect");
        if (!rm)
            return modified;

        eeng::try_read_asset_ref(
            *rm,
            comp->graph_ref,
            ctx,
            "AnimationGraphComponent",
            "Missing AnimationGraphAsset for AnimationGraphComponent:",
            [&](const assets::AnimationGraphAsset& graph)
            {
                const auto param_values = detail::snapshot_param_values(graph, comp->instance);

                inspector.row();
                ImGui::TextDisabled("Graph Params");
                inspector.next_column();
                ImGui::TextDisabled(graph.params.empty() ? "none" : "");

                std::size_t float_index = 0;
                std::size_t int_index = 0;
                std::size_t bool_index = 0;
                std::size_t trigger_index = 0;

                for (const auto& param : graph.params)
                {
                    std::size_t slot_index = 0;
                    switch (param.type)
                    {
                    case assets::AnimGraphParamType::Float:
                        slot_index = float_index++;
                        break;
                    case assets::AnimGraphParamType::Int:
                        slot_index = int_index++;
                        break;
                    case assets::AnimGraphParamType::Bool:
                        slot_index = bool_index++;
                        break;
                    case assets::AnimGraphParamType::Trigger:
                        slot_index = trigger_index++;
                        break;
                    default:
                        continue;
                    }

                    inspector.begin_leaf(param.name.c_str());
                    switch (param.type)
                    {
                    case assets::AnimGraphParamType::Float:
                    {
                        if (slot_index >= comp->instance.float_params.size())
                            break;

                        float value = comp->instance.float_params[slot_index];
                        float new_value = value;
                        float min_value = param.has_min ? param.min_value : 0.0f;
                        float max_value = param.has_max ? param.max_value : 1.0f;
                        if (max_value < min_value)
                            std::swap(min_value, max_value);

                        const float before_value = value;
                        const bool slider_changed = ImGui::SliderFloat("##label", &new_value, min_value, max_value);
                        // Policy: Commit an undoable edit only when the drag completes and moved enough.
                        static std::unordered_map<ImGuiID, float> start_values;
                        const ImGuiID item_id = ImGui::GetItemID();
                        if (ImGui::IsItemActivated())
                            start_values[item_id] = before_value;
                        if (slider_changed)
                        {
                            comp->instance.float_params[slot_index] = new_value;
                            if (live_comp && live_comp != comp
                                && slot_index < live_comp->instance.float_params.size())
                            {
                                live_comp->instance.float_params[slot_index] = new_value;
                            }
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit())
                        {
                            float start_value = before_value;
                            auto it = start_values.find(item_id);
                            if (it != start_values.end())
                            {
                                start_value = it->second;
                                start_values.erase(it);
                            }
                            if (std::fabs(new_value - start_value) >= kParamCommitEpsilon)
                                modified = true;
                        }
                        break;
                    }
                    case assets::AnimGraphParamType::Int:
                    {
                        if (slot_index >= comp->instance.int_params.size())
                            break;

                        int value = comp->instance.int_params[slot_index];
                        int new_value = value;
                        bool changed = false;

                        if (param.has_min || param.has_max)
                        {
                            int min_value = param.has_min ? static_cast<int>(std::floor(param.min_value))
                                                          : std::numeric_limits<int>::min();
                            int max_value = param.has_max ? static_cast<int>(std::ceil(param.max_value))
                                                          : std::numeric_limits<int>::max();
                            if (max_value < min_value)
                                std::swap(min_value, max_value);
                            changed = ImGui::SliderInt("##label", &new_value, min_value, max_value);
                        }
                        else
                        {
                            changed = ImGui::InputInt("##label", &new_value);
                        }

                        if (changed)
                        {
                            comp->instance.int_params[slot_index] = new_value;
                            if (live_comp && live_comp != comp
                                && slot_index < live_comp->instance.int_params.size())
                            {
                                live_comp->instance.int_params[slot_index] = new_value;
                            }
                        }

                        if (ImGui::IsItemDeactivatedAfterEdit() && new_value != value)
                            modified = true;
                        break;
                    }
                    case assets::AnimGraphParamType::Bool:
                    {
                        if (slot_index >= comp->instance.bool_params.size())
                            break;

                        bool value = comp->instance.bool_params[slot_index] != 0;
                        bool new_value = value;
                        if (ImGui::Checkbox("##label", &new_value))
                        {
                            comp->instance.bool_params[slot_index] = new_value ? 1u : 0u;
                            if (live_comp && live_comp != comp
                                && slot_index < live_comp->instance.bool_params.size())
                            {
                                live_comp->instance.bool_params[slot_index] = new_value ? 1u : 0u;
                            }
                            modified = true;
                        }
                        break;
                    }
                    case assets::AnimGraphParamType::Trigger:
                    {
                        if (slot_index >= comp->instance.trigger_params.size())
                            break;

                        bool value = comp->instance.trigger_params[slot_index] != 0;
                        bool new_value = value;
                        if (ImGui::Checkbox("##label", &new_value))
                        {
                            comp->instance.trigger_params[slot_index] = new_value ? 1u : 0u;
                            if (live_comp && live_comp != comp
                                && slot_index < live_comp->instance.trigger_params.size())
                            {
                                live_comp->instance.trigger_params[slot_index] = new_value ? 1u : 0u;
                            }
                            modified = true;
                        }
                        break;
                    }
                    default:
                        break;
                    }
                    inspector.end_leaf();
                }

                static bool show_embedded_visualizer = false;
                inspector.begin_leaf("Graph Visualizer");
                ImGui::Checkbox("##graph_viz_embedded", &show_embedded_visualizer);
                ImGui::SameLine();
                ImGui::TextUnformatted("Embedded");
                if (show_embedded_visualizer)
                    detail::draw_graph_visualizer(graph, *comp, param_values, false);
                inspector.end_leaf();

                // Policy: Show transitions in a compact, read-only list for quick graph sanity checks.
                inspector.row();
                ImGui::TextDisabled("Transitions");
                inspector.next_column();
                ImGui::TextDisabled("");

                for (std::size_t layer_index = 0; layer_index < graph.layers.size(); layer_index++)
                {
                    const auto& layer = graph.layers[layer_index];
                    const auto& runtime = layer_index < comp->instance.layers.size()
                        ? comp->instance.layers[layer_index]
                        : eeng::ecs::AnimGraphLayerRuntime{};

                    inspector.row();
                    std::string layer_label = "Layer " + std::to_string(layer_index);
                    if (!layer.name.empty())
                        layer_label += " (" + layer.name + ")";
                    ImGui::TextDisabled("%s", layer_label.c_str());
                    inspector.next_column();
                    if (runtime.state >= 0 && runtime.state < static_cast<int>(layer.states.size()))
                        ImGui::TextDisabled("active: %s", layer.states[static_cast<std::size_t>(runtime.state)].id.c_str());
                    else
                        ImGui::TextDisabled("active: n/a");

                    for (const auto& transition : layer.transitions)
                    {
                        const int from_index = detail::find_state_index(layer, transition.from);
                        const int to_index = detail::find_state_index(layer, transition.to);
                        const bool is_active = runtime.transition.active
                            && runtime.transition.from == from_index
                            && runtime.transition.to == to_index;

                        std::string label = transition.from + " -> " + transition.to;
                        inspector.begin_leaf(label.c_str());
                        if (is_active)
                            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.35f, 1.0f), "active");

                        std::string details = "dur=" + detail::format_float(transition.duration, 2);
                        if (transition.has_exit_time)
                            details += " exit=" + detail::format_float(transition.exit_time, 2);
                        ImGui::TextUnformatted(details.c_str());

                        if (!transition.conditions.items.empty())
                        {
                            std::string conds = transition.conditions.mode == assets::AnimGraphConditionMode::Any
                                ? "any: "
                                : "all: ";
                            bool first = true;
                            for (const auto& cond : transition.conditions.items)
                            {
                                if (!first)
                                    conds += ", ";
                                first = false;
                                conds += cond.param + " " + detail::condition_op_label(cond.op);
                                if (!cond.rhs_param.empty())
                                {
                                    conds += " " + cond.rhs_param;
                                }
                                else if (std::holds_alternative<float>(cond.value))
                                {
                                    conds += " " + detail::format_float(std::get<float>(cond.value), 2);
                                }
                                else if (std::holds_alternative<int>(cond.value))
                                {
                                    conds += " " + std::to_string(std::get<int>(cond.value));
                                }
                                else if (std::holds_alternative<bool>(cond.value))
                                {
                                    conds += std::get<bool>(cond.value) ? " true" : " false";
                                }

                                detail::GraphParamValue param_value{};
                                if (detail::try_get_param_value(param_values, cond.param, param_value))
                                {
                                    conds += " [";
                                    conds += detail::format_param_value(param_value);
                                    conds += "]";
                                }
                            }
                            ImGui::TextUnformatted(conds.c_str());
                        }

                        inspector.end_leaf();
                    }
                }

            });

        return modified;
    }
}
