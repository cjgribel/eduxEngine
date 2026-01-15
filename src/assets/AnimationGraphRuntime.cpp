// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "assets/AnimationGraphRuntime.hpp"

namespace eeng::assets
{
    void build_animation_graph_cache(AnimationGraphAsset& graph)
    {
        auto& runtime = graph.runtime;
        runtime.param_index.clear();
        runtime.param_slots.clear();
        runtime.mask_index.clear();
        runtime.layers.clear();

        runtime.param_index.reserve(graph.params.size());
        runtime.param_slots.resize(graph.params.size());
        std::size_t float_index = 0;
        std::size_t int_index = 0;
        std::size_t bool_index = 0;
        std::size_t trigger_index = 0;
        for (std::size_t i = 0; i < graph.params.size(); i++)
        {
            const auto& param = graph.params[i];
            if (!param.name.empty())
                runtime.param_index.emplace(param.name, i);

            auto& slot = runtime.param_slots[i];
            slot.type = param.type;
            switch (param.type)
            {
            case AnimGraphParamType::Float:
                slot.index = float_index++;
                break;
            case AnimGraphParamType::Int:
                slot.index = int_index++;
                break;
            case AnimGraphParamType::Bool:
                slot.index = bool_index++;
                break;
            case AnimGraphParamType::Trigger:
                slot.index = trigger_index++;
                break;
            default:
                slot.index = 0;
                break;
            }
        }

        runtime.mask_index.reserve(graph.masks.size());
        for (std::size_t i = 0; i < graph.masks.size(); i++)
        {
            const auto& mask = graph.masks[i];
            if (!mask.name.empty())
                runtime.mask_index.emplace(mask.name, i);
        }

        runtime.layers.resize(graph.layers.size());
        for (std::size_t i = 0; i < graph.layers.size(); i++)
        {
            auto& layer_cache = runtime.layers[i];
            layer_cache.state_index.clear();
            layer_cache.state_index.reserve(graph.layers[i].states.size());
            for (std::size_t s = 0; s < graph.layers[i].states.size(); s++)
            {
                const auto& state = graph.layers[i].states[s];
                if (!state.id.empty())
                    layer_cache.state_index.emplace(state.id, s);
            }
        }

        runtime.built = true;
    }
}
