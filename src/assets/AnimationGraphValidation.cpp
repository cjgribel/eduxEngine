// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "assets/AnimationGraphValidation.hpp"

#include <unordered_map>
#include <unordered_set>

namespace eeng::assets
{
    namespace
    {
        void push_error(std::vector<std::string>& errors, std::string msg)
        {
            errors.push_back(std::move(msg));
        }

        bool is_numeric(AnimGraphParamType type)
        {
            return type == AnimGraphParamType::Float || type == AnimGraphParamType::Int;
        }

        bool is_boolean(AnimGraphParamType type)
        {
            return type == AnimGraphParamType::Bool || type == AnimGraphParamType::Trigger;
        }

        bool op_requires_value(AnimGraphConditionOp op)
        {
            switch (op)
            {
            case AnimGraphConditionOp::Equal:
            case AnimGraphConditionOp::NotEqual:
            case AnimGraphConditionOp::Less:
            case AnimGraphConditionOp::Greater:
            case AnimGraphConditionOp::LessEqual:
            case AnimGraphConditionOp::GreaterEqual:
                return true;
            case AnimGraphConditionOp::IsTrue:
            case AnimGraphConditionOp::IsFalse:
                return false;
            default:
                return false;
            }
        }

        bool op_is_numeric(AnimGraphConditionOp op)
        {
            switch (op)
            {
            case AnimGraphConditionOp::Less:
            case AnimGraphConditionOp::Greater:
            case AnimGraphConditionOp::LessEqual:
            case AnimGraphConditionOp::GreaterEqual:
                return true;
            default:
                return false;
            }
        }

        bool value_is_empty(const AnimGraphValue& value)
        {
            return std::holds_alternative<std::monostate>(value);
        }

        bool value_is_bool(const AnimGraphValue& value)
        {
            return std::holds_alternative<bool>(value);
        }

        bool value_is_int(const AnimGraphValue& value)
        {
            return std::holds_alternative<int>(value);
        }

        bool value_is_float(const AnimGraphValue& value)
        {
            return std::holds_alternative<float>(value);
        }
    }

    std::vector<std::string> validate_animation_graph(const AnimationGraphAsset& graph)
    {
        std::vector<std::string> errors;

        if (graph.version != 1)
            push_error(errors, "version: unsupported version (expected 1)");

        std::unordered_map<std::string, AnimGraphParamType> param_types;
        param_types.reserve(graph.params.size());

        for (size_t i = 0; i < graph.params.size(); i++)
        {
            const auto& param = graph.params[i];
            const std::string prefix = "params[" + std::to_string(i) + "].";

            if (param.name.empty())
                push_error(errors, prefix + "name: empty name");
            if (param.type == AnimGraphParamType::Invalid)
                push_error(errors, prefix + "type: invalid param type");

            if (!param.name.empty())
            {
                if (!param_types.emplace(param.name, param.type).second)
                    push_error(errors, prefix + "name: duplicate param name '" + param.name + "'");
            }

            if (param.has_min && param.has_max && param.min_value > param.max_value)
                push_error(errors, prefix + "min/max: min > max");

            if (is_numeric(param.type))
            {
                const float value = (param.type == AnimGraphParamType::Int)
                    ? static_cast<float>(param.default_int)
                    : param.default_float;
                if (param.has_min && value < param.min_value)
                    push_error(errors, prefix + "default: below min");
                if (param.has_max && value > param.max_value)
                    push_error(errors, prefix + "default: above max");
            }
        }

        std::unordered_set<std::string> mask_names;
        mask_names.reserve(graph.masks.size());
        for (size_t i = 0; i < graph.masks.size(); i++)
        {
            const auto& mask = graph.masks[i];
            const std::string prefix = "masks[" + std::to_string(i) + "].";

            if (mask.name.empty())
                push_error(errors, prefix + "name: empty name");
            if (mask.mode == AnimGraphMaskMode::Invalid)
                push_error(errors, prefix + "mode: invalid mask mode");

            if (!mask.name.empty())
            {
                if (!mask_names.emplace(mask.name).second)
                    push_error(errors, prefix + "name: duplicate mask name '" + mask.name + "'");
            }

            std::unordered_set<std::string> weight_names;
            weight_names.reserve(mask.weights.size());
            for (size_t w = 0; w < mask.weights.size(); w++)
            {
                const auto& weight = mask.weights[w];
                const std::string wprefix = prefix + "weights[" + std::to_string(w) + "].";
                if (weight.bone.empty())
                    push_error(errors, wprefix + "bone: empty bone name");
                if (weight.weight < 0.0f || weight.weight > 1.0f)
                    push_error(errors, wprefix + "weight: expected [0,1]");
                if (!weight.bone.empty())
                {
                    if (!weight_names.emplace(weight.bone).second)
                        push_error(errors, wprefix + "bone: duplicate bone name '" + weight.bone + "'");
                }
            }
        }

        std::unordered_set<std::string> layer_names;
        layer_names.reserve(graph.layers.size());

        for (size_t i = 0; i < graph.layers.size(); i++)
        {
            const auto& layer = graph.layers[i];
            const std::string prefix = "layers[" + std::to_string(i) + "].";

            if (layer.name.empty())
                push_error(errors, prefix + "name: empty name");
            if (layer.blend_mode == AnimGraphBlendMode::Invalid)
                push_error(errors, prefix + "blend_mode: invalid blend mode");
            if (layer.weight < 0.0f)
                push_error(errors, prefix + "weight: negative layer weight");

            if (!layer.name.empty())
            {
                if (!layer_names.emplace(layer.name).second)
                    push_error(errors, prefix + "name: duplicate layer name '" + layer.name + "'");
            }

            if (!layer.mask.empty() && mask_names.find(layer.mask) == mask_names.end())
                push_error(errors, prefix + "mask: unknown mask '" + layer.mask + "'");

            std::unordered_map<std::string, size_t> state_indices;
            state_indices.reserve(layer.states.size());

            for (size_t s = 0; s < layer.states.size(); s++)
            {
                const auto& state = layer.states[s];
                const std::string sprefix = prefix + "states[" + std::to_string(s) + "].";

                if (state.id.empty())
                    push_error(errors, sprefix + "id: empty state id");
                if (state.type == AnimGraphStateType::Invalid)
                    push_error(errors, sprefix + "type: invalid state type");
                if (state.playback == AnimGraphPlaybackMode::Invalid)
                    push_error(errors, sprefix + "playback: invalid playback mode");
                if (state.trim_left < 0.0f || state.trim_left > 1.0f)
                    push_error(errors, sprefix + "trim_left: expected [0,1]");
                if (state.trim_right < 0.0f || state.trim_right > 1.0f)
                    push_error(errors, sprefix + "trim_right: expected [0,1]");
                if (state.trim_left >= state.trim_right)
                    push_error(errors, sprefix + "trim: trim_left >= trim_right");

                if (!state.id.empty())
                {
                    if (!state_indices.emplace(state.id, s).second)
                        push_error(errors, sprefix + "id: duplicate state id '" + state.id + "'");
                }

                const auto param_type_x = param_types.find(state.param_x);
                const auto param_type_y = param_types.find(state.param_y);

                if (state.type == AnimGraphStateType::Clip)
                {
                    if (state.clip.empty())
                        push_error(errors, sprefix + "clip: empty clip name");
                }
                else if (state.type == AnimGraphStateType::Blend2)
                {
                    if (state.clip0.empty() || state.clip1.empty())
                        push_error(errors, sprefix + "clip0/clip1: empty clip name");
                    if (state.param_x.empty())
                        push_error(errors, sprefix + "param: missing param");
                    else if (param_type_x == param_types.end())
                        push_error(errors, sprefix + "param: unknown param '" + state.param_x + "'");
                    else if (!is_numeric(param_type_x->second))
                        push_error(errors, sprefix + "param: expected numeric param");

                    if (state.param_min_x >= state.param_max_x)
                        push_error(errors, sprefix + "param_min/param_max: min >= max");
                }
                else if (state.type == AnimGraphStateType::BlendSpace1D)
                {
                    if (state.param_x.empty())
                        push_error(errors, sprefix + "param: missing param");
                    else if (param_type_x == param_types.end())
                        push_error(errors, sprefix + "param: unknown param '" + state.param_x + "'");
                    else if (!is_numeric(param_type_x->second))
                        push_error(errors, sprefix + "param: expected numeric param");

                    if (state.samples.empty())
                        push_error(errors, sprefix + "samples: empty blend space");

                    for (size_t b = 0; b < state.samples.size(); b++)
                    {
                        if (state.samples[b].clip.empty())
                            push_error(errors, sprefix + "samples[" + std::to_string(b) + "].clip: empty clip name");
                        if (state.samples[b].pose_time > 1.0f)
                            push_error(errors, sprefix + "samples[" + std::to_string(b) + "].pose_time: expected [0,1]");
                    }

                    if (!state.indices.empty())
                    {
                        if (state.indices.size() % 2 != 0)
                            push_error(errors, sprefix + "indices: expected even count for 1D segments");
                        const size_t sample_count = state.samples.size();
                        for (size_t idx = 0; idx < state.indices.size(); idx++)
                        {
                            const int sample_index = state.indices[idx];
                            if (sample_index < 0 || static_cast<size_t>(sample_index) >= sample_count)
                                push_error(errors, sprefix + "indices[" + std::to_string(idx) + "]: out of range");
                        }
                    }
                }
                else if (state.type == AnimGraphStateType::BlendSpace2D)
                {
                    if (state.param_x.empty() || state.param_y.empty())
                        push_error(errors, sprefix + "param_x/param_y: missing param");
                    else
                    {
                        if (param_type_x == param_types.end())
                            push_error(errors, sprefix + "param_x: unknown param '" + state.param_x + "'");
                        else if (!is_numeric(param_type_x->second))
                            push_error(errors, sprefix + "param_x: expected numeric param");

                        if (param_type_y == param_types.end())
                            push_error(errors, sprefix + "param_y: unknown param '" + state.param_y + "'");
                        else if (!is_numeric(param_type_y->second))
                            push_error(errors, sprefix + "param_y: expected numeric param");
                    }

                    if (state.samples.empty())
                        push_error(errors, sprefix + "samples: empty blend space");

                    for (size_t b = 0; b < state.samples.size(); b++)
                    {
                        if (state.samples[b].clip.empty())
                            push_error(errors, sprefix + "samples[" + std::to_string(b) + "].clip: empty clip name");
                        if (state.samples[b].pose_time > 1.0f)
                            push_error(errors, sprefix + "samples[" + std::to_string(b) + "].pose_time: expected [0,1]");
                    }

                    if (!state.indices.empty())
                    {
                        if (state.indices.size() % 4 != 0)
                            push_error(errors, sprefix + "indices: expected multiple of 4 for 2D quads");
                        const size_t sample_count = state.samples.size();
                        for (size_t idx = 0; idx < state.indices.size(); idx++)
                        {
                            const int sample_index = state.indices[idx];
                            if (sample_index < 0 || static_cast<size_t>(sample_index) >= sample_count)
                                push_error(errors, sprefix + "indices[" + std::to_string(idx) + "]: out of range");
                        }
                    }
                }
            }

            if (layer.entry_state.empty())
                push_error(errors, prefix + "entry_state: missing entry state");
            else if (state_indices.find(layer.entry_state) == state_indices.end())
                push_error(errors, prefix + "entry_state: unknown state '" + layer.entry_state + "'");

            for (size_t t = 0; t < layer.transitions.size(); t++)
            {
                const auto& trans = layer.transitions[t];
                const std::string tprefix = prefix + "transitions[" + std::to_string(t) + "].";

                if (trans.from.empty())
                    push_error(errors, tprefix + "from: empty 'from' state");
                if (trans.to.empty())
                    push_error(errors, tprefix + "to: empty 'to' state");

                if (!trans.to.empty() && state_indices.find(trans.to) == state_indices.end())
                    push_error(errors, tprefix + "to: unknown state '" + trans.to + "'");

                if (trans.from != "*" && !trans.from.empty() && state_indices.find(trans.from) == state_indices.end())
                    push_error(errors, tprefix + "from: unknown state '" + trans.from + "'");

                if (trans.duration < 0.0f)
                    push_error(errors, tprefix + "duration: negative duration");

                if (trans.has_exit_time)
                {
                    if (trans.exit_time < 0.0f || trans.exit_time > 1.0f)
                        push_error(errors, tprefix + "exit_time: expected [0,1]");
                    if (trans.from == "*")
                        push_error(errors, tprefix + "exit_time: not valid for any-state transitions");
                }

                if (trans.conditions.mode == AnimGraphConditionMode::Invalid)
                    push_error(errors, tprefix + "conditions.mode: invalid mode");

                for (size_t c = 0; c < trans.conditions.items.size(); c++)
                {
                    const auto& cond = trans.conditions.items[c];
                    const std::string cprefix = tprefix + "conditions.items[" + std::to_string(c) + "].";

                    if (cond.param.empty())
                    {
                        push_error(errors, cprefix + "param: empty param name");
                        continue;
                    }

                    auto param_it = param_types.find(cond.param);
                    if (param_it == param_types.end())
                    {
                        push_error(errors, cprefix + "param: unknown param '" + cond.param + "'");
                        continue;
                    }

                    if (cond.op == AnimGraphConditionOp::Invalid)
                        push_error(errors, cprefix + "op: invalid op");

                    const AnimGraphParamType lhs_type = param_it->second;
                    const bool rhs_is_param = !cond.rhs_param.empty();

                    if (rhs_is_param)
                    {
                        auto rhs_it = param_types.find(cond.rhs_param);
                        if (rhs_it == param_types.end())
                        {
                            push_error(errors, cprefix + "rhs_param: unknown param '" + cond.rhs_param + "'");
                        }
                        else if (rhs_it->second != lhs_type)
                        {
                            push_error(errors, cprefix + "rhs_param: type mismatch");
                        }
                    }
                    else if (op_requires_value(cond.op) && value_is_empty(cond.value))
                    {
                        push_error(errors, cprefix + "value: missing value for op");
                    }

                    if (op_is_numeric(cond.op))
                    {
                        if (!is_numeric(lhs_type))
                            push_error(errors, cprefix + "op: numeric op for non-numeric param");
                        else if (!rhs_is_param && !(value_is_float(cond.value) || value_is_int(cond.value)))
                            push_error(errors, cprefix + "value: expected numeric value");
                    }

                    if (cond.op == AnimGraphConditionOp::IsTrue || cond.op == AnimGraphConditionOp::IsFalse)
                    {
                        if (!is_boolean(lhs_type))
                            push_error(errors, cprefix + "op: boolean op for non-bool param");
                        if (rhs_is_param)
                            push_error(errors, cprefix + "rhs_param: not supported for boolean op");
                    }

                    if ((cond.op == AnimGraphConditionOp::Equal || cond.op == AnimGraphConditionOp::NotEqual) && !rhs_is_param)
                    {
                        if (is_boolean(lhs_type) && !value_is_bool(cond.value) && !value_is_empty(cond.value))
                            push_error(errors, cprefix + "value: expected bool value");
                        if (is_numeric(lhs_type) && !value_is_float(cond.value) && !value_is_int(cond.value))
                            push_error(errors, cprefix + "value: expected numeric value");
                    }
                }
            }
        }

        return errors;
    }
}
