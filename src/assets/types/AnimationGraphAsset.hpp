// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace eeng::assets
{
    enum class AnimGraphParamType : std::uint8_t
    {
        Invalid = 0,
        Float,
        Int,
        Bool,
        Trigger
    };

    enum class AnimGraphConditionOp : std::uint8_t
    {
        Invalid = 0,
        Equal,
        NotEqual,
        Less,
        Greater,
        LessEqual,
        GreaterEqual,
        IsTrue,
        IsFalse
    };

    enum class AnimGraphConditionMode : std::uint8_t
    {
        Invalid = 0,
        All,
        Any
    };

    enum class AnimGraphStateType : std::uint8_t
    {
        Invalid = 0,
        Clip,
        Blend2,
        BlendSpace1D,
        BlendSpace2D
    };

    enum class AnimGraphPlaybackMode : std::uint8_t
    {
        Invalid = 0,
        Pose,
        Loop,
        Mirror,
        Clamp
    };

    enum class AnimGraphBlendMode : std::uint8_t
    {
        Invalid = 0,
        Override,
        Additive,
        Blend
    };

    enum class AnimGraphMaskMode : std::uint8_t
    {
        Invalid = 0,
        Include,
        Exclude
    };

    struct AnimGraphParamDef
    {
        std::string name;
        AnimGraphParamType type = AnimGraphParamType::Invalid;

        float default_float = 0.0f;
        int default_int = 0;
        bool default_bool = false;

        bool has_min = false;
        bool has_max = false;
        float min_value = 0.0f;
        float max_value = 0.0f;
    };

    using AnimGraphValue = std::variant<std::monostate, float, int, bool>;

    struct AnimGraphCondition
    {
        std::string param;
        AnimGraphConditionOp op = AnimGraphConditionOp::Invalid;
        AnimGraphValue value{};
        std::string rhs_param;
    };

    struct AnimGraphConditionGroup
    {
        AnimGraphConditionMode mode = AnimGraphConditionMode::All;
        std::vector<AnimGraphCondition> items;
    };

    struct AnimGraphTransition
    {
        std::string from;
        std::string to;
        float duration = 0.0f;
        bool has_exit_time = false;
        float exit_time = 0.0f;
        AnimGraphConditionGroup conditions{};
        int priority = 0;
    };

    struct AnimGraphBlendSample
    {
        std::string clip;
        float x = 0.0f;
        float y = 0.0f;
    };

    struct AnimGraphState
    {
        std::string id;
        AnimGraphStateType type = AnimGraphStateType::Invalid;

        AnimGraphPlaybackMode playback = AnimGraphPlaybackMode::Loop;
        float speed = 1.0f;
        bool rewind_on_enter = false;

        float trim_left = 0.0f;
        float trim_right = 1.0f;

        std::string clip;
        std::string clip0;
        std::string clip1;

        std::string param_x;
        std::string param_y;
        float param_min_x = 0.0f;
        float param_max_x = 1.0f;
        float param_min_y = 0.0f;
        float param_max_y = 1.0f;

        std::vector<AnimGraphBlendSample> samples;
        std::vector<int> indices;

        bool sync_children = true;
    };

    struct AnimGraphMaskWeight
    {
        std::string bone;
        float weight = 1.0f;
    };

    struct AnimGraphMask
    {
        std::string name;
        AnimGraphMaskMode mode = AnimGraphMaskMode::Include;
        std::vector<AnimGraphMaskWeight> weights;
    };

    struct AnimGraphLayer
    {
        std::string name;
        float weight = 1.0f;
        AnimGraphBlendMode blend_mode = AnimGraphBlendMode::Override;
        std::string mask;
        std::string entry_state;

        std::vector<AnimGraphState> states;
        std::vector<AnimGraphTransition> transitions;
    };

    struct AnimationGraphAsset
    {
        int version = 1;
        std::string name;
        std::vector<AnimGraphParamDef> params;
        std::vector<AnimGraphMask> masks;
        std::vector<AnimGraphLayer> layers;
        struct RuntimeCache
        {
            struct LayerCache
            {
                std::unordered_map<std::string, std::size_t> state_index;
            };

            std::unordered_map<std::string, std::size_t> param_index;
            std::unordered_map<std::string, std::size_t> mask_index;
            std::vector<LayerCache> layers;
            bool built = false;
        } runtime;
    };

    template<typename Visitor>
    void visit_asset_refs(AnimationGraphAsset&, Visitor&&)
    {
    }

    template<typename Visitor>
    void visit_asset_refs(const AnimationGraphAsset&, Visitor&&)
    {
    }
}
