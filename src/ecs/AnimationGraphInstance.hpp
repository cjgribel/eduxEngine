// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <cstdint>
#include <vector>

#include "AssetRef.hpp"
#include "assets/types/AnimationGraphAsset.hpp"

namespace eeng::ecs
{
    struct AnimGraphTransitionRuntime
    {
        int from = -1;
        int to = -1;
        float time = 0.0f;
        float duration = 0.0f;
        bool active = false;
    };

    struct AnimGraphLayerRuntime
    {
        int state = -1;
        float state_time = 0.0f;
        AnimGraphTransitionRuntime transition{};
        float weight = 1.0f;
    };

    struct AnimGraphInstance
    {
        AssetRef<assets::AnimationGraphAsset> graph_ref;

        std::vector<float> float_params;
        std::vector<int> int_params;
        std::vector<std::uint8_t> bool_params;
        std::vector<std::uint8_t> trigger_params;

        std::vector<AnimGraphLayerRuntime> layers;
        bool initialized = false;
    };
}
