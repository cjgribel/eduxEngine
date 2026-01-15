// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include <gtest/gtest.h>

#include "assets/AnimationGraphValidation.hpp"

namespace
{
    using namespace eeng::assets;

    AnimationGraphAsset make_basic_graph()
    {
        AnimationGraphAsset graph{};
        graph.version = 1;
        graph.name = "BasicGraph";

        AnimGraphParamDef speed{};
        speed.name = "speed";
        speed.type = AnimGraphParamType::Float;
        speed.default_float = 0.0f;
        speed.has_min = true;
        speed.min_value = 0.0f;
        speed.has_max = true;
        speed.max_value = 1.0f;
        graph.params.push_back(speed);

        AnimGraphState idle{};
        idle.id = "Idle";
        idle.type = AnimGraphStateType::Clip;
        idle.playback = AnimGraphPlaybackMode::Loop;
        idle.clip = "Idle";

        AnimGraphState move{};
        move.id = "Move";
        move.type = AnimGraphStateType::Blend2;
        move.playback = AnimGraphPlaybackMode::Loop;
        move.clip0 = "Walk";
        move.clip1 = "Run";
        move.param_x = "speed";
        move.param_min_x = 0.0f;
        move.param_max_x = 1.0f;

        AnimGraphTransition to_move{};
        to_move.from = "Idle";
        to_move.to = "Move";
        to_move.duration = 0.1f;
        AnimGraphCondition cond{};
        cond.param = "speed";
        cond.op = AnimGraphConditionOp::Greater;
        cond.value = 0.1f;
        to_move.conditions.items.push_back(cond);

        AnimGraphLayer layer{};
        layer.name = "Base";
        layer.blend_mode = AnimGraphBlendMode::Override;
        layer.entry_state = "Idle";
        layer.states = { idle, move };
        layer.transitions = { to_move };
        graph.layers.push_back(std::move(layer));

        return graph;
    }
}

TEST(AnimationGraphValidation, AcceptsValidGraph)
{
    auto graph = make_basic_graph();
    auto errors = validate_animation_graph(graph);
    EXPECT_TRUE(errors.empty());
}

TEST(AnimationGraphValidation, RejectsMissingEntry)
{
    auto graph = make_basic_graph();
    graph.layers[0].entry_state.clear();
    auto errors = validate_animation_graph(graph);
    EXPECT_FALSE(errors.empty());
}
