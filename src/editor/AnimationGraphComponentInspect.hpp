// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <algorithm>
#include <cmath>
#include <unordered_map>

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
                if (graph.params.empty())
                    return;

                inspector.row();
                ImGui::TextDisabled("Graph Params");
                inspector.next_column();
                ImGui::TextDisabled("");

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

                    if (param.type != assets::AnimGraphParamType::Float)
                        continue;
                    if (slot_index >= comp->instance.float_params.size())
                        continue;

                    float value = comp->instance.float_params[slot_index];
                    float new_value = value;
                    float min_value = param.has_min ? param.min_value : 0.0f;
                    float max_value = param.has_max ? param.max_value : 1.0f;
                    if (max_value < min_value)
                        std::swap(min_value, max_value);

                    inspector.begin_leaf(param.name.c_str());
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
                    inspector.end_leaf();
                }
            });

        return modified;
    }
}
