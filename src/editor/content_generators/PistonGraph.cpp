// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/content_generators/PistonGraph.hpp"

namespace eeng::editor::content_generators
{
    assets::AnimationGraphAsset build_piston_graph(
        const std::string& graph_name,
        const std::string& clip_name)
    {
        assets::AnimationGraphAsset graph{};
        graph.version = 1;
        graph.name = graph_name;

        assets::AnimGraphLayer layer{};
        layer.name = "Base";
        layer.weight = 1.0f;
        layer.blend_mode = assets::AnimGraphBlendMode::Override;
        layer.entry_state = "Piston";

        assets::AnimGraphState state{};
        state.id = "Piston";
        state.type = assets::AnimGraphStateType::Clip;
        state.clip = clip_name;
        state.playback = assets::AnimGraphPlaybackMode::Clamp;
        state.speed = 0.0f; // Driven externally via state_time.
        layer.states.push_back(std::move(state));

        graph.layers.push_back(std::move(layer));

        return graph;
    }
}
