// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <cstdint>
#include <vector>

#include "Guid.h"

namespace eeng::ecs
{
    struct AnimGraphStateClipCache
    {
        // Policy: Store model-resolved clip indices to avoid per-frame string lookups.
        int clip = -1;
        int clip0 = -1;
        int clip1 = -1;
        std::vector<int> samples;
        std::vector<int> blend1d_order;
    };

    struct AnimGraphLayerClipCache
    {
        std::vector<AnimGraphStateClipCache> states;
    };

    struct AnimGraphClipCache
    {
        // Policy: Rebuild when either the model or graph asset changes.
        Guid model_guid = Guid::invalid();
        Guid graph_guid = Guid::invalid();
        std::vector<AnimGraphLayerClipCache> layers;
        bool built = false;
    };

    struct AnimGraphTransitionRuntime
    {
        int from = -1;
        int to = -1;
        float time = 0.0f;
        float duration = 0.0f;
        float dest_time = 0.0f;
        bool active = false;
    };

    struct AnimGraphLayerRuntime
    {
        int state = -1;
        float state_time = 0.0f;
        AnimGraphTransitionRuntime transition{};
        AnimGraphTransitionRuntime last_transition{};
        float last_transition_ttl = 0.0f;
        float weight = 1.0f;
    };

    struct AnimGraphInstance
    {
        Guid graph_guid = Guid::invalid();
        std::vector<float> float_params;
        std::vector<int> int_params;
        std::vector<std::uint8_t> bool_params;
        std::vector<std::uint8_t> trigger_params;

        std::vector<AnimGraphLayerRuntime> layers;
        AnimGraphClipCache clip_cache;
        bool initialized = false;
    };
}
