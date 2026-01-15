// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "serializers/AnimationGraphSerialization.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>
#include <string_view>

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

#include "MetaLiterals.h"
#include "assets/AnimationGraphValidation.hpp"
#include "assets/types/AnimationGraphAsset.hpp"

namespace eeng::serializers
{
    namespace
    {
        std::string to_lower(std::string_view value)
        {
            std::string out(value);
            std::transform(out.begin(), out.end(), out.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        }

        const char* to_string(assets::AnimGraphParamType type)
        {
            switch (type)
            {
            case assets::AnimGraphParamType::Float: return "float";
            case assets::AnimGraphParamType::Int: return "int";
            case assets::AnimGraphParamType::Bool: return "bool";
            case assets::AnimGraphParamType::Trigger: return "trigger";
            default: return "invalid";
            }
        }

        const char* to_string(assets::AnimGraphConditionOp op)
        {
            switch (op)
            {
            case assets::AnimGraphConditionOp::Equal: return "eq";
            case assets::AnimGraphConditionOp::NotEqual: return "neq";
            case assets::AnimGraphConditionOp::Less: return "lt";
            case assets::AnimGraphConditionOp::Greater: return "gt";
            case assets::AnimGraphConditionOp::LessEqual: return "leq";
            case assets::AnimGraphConditionOp::GreaterEqual: return "geq";
            case assets::AnimGraphConditionOp::IsTrue: return "is_true";
            case assets::AnimGraphConditionOp::IsFalse: return "is_false";
            default: return "invalid";
            }
        }

        const char* to_string(assets::AnimGraphConditionMode mode)
        {
            switch (mode)
            {
            case assets::AnimGraphConditionMode::All: return "all";
            case assets::AnimGraphConditionMode::Any: return "any";
            default: return "invalid";
            }
        }

        const char* to_string(assets::AnimGraphStateType type)
        {
            switch (type)
            {
            case assets::AnimGraphStateType::Clip: return "clip";
            case assets::AnimGraphStateType::Blend2: return "blend2";
            case assets::AnimGraphStateType::BlendSpace1D: return "blendspace1d";
            case assets::AnimGraphStateType::BlendSpace2D: return "blendspace2d";
            default: return "invalid";
            }
        }

        const char* to_string(assets::AnimGraphPlaybackMode mode)
        {
            switch (mode)
            {
            case assets::AnimGraphPlaybackMode::Pose: return "pose";
            case assets::AnimGraphPlaybackMode::Loop: return "loop";
            case assets::AnimGraphPlaybackMode::Mirror: return "mirror";
            case assets::AnimGraphPlaybackMode::Clamp: return "clamp";
            default: return "invalid";
            }
        }

        const char* to_string(assets::AnimGraphBlendMode mode)
        {
            switch (mode)
            {
            case assets::AnimGraphBlendMode::Override: return "override";
            case assets::AnimGraphBlendMode::Additive: return "additive";
            case assets::AnimGraphBlendMode::Blend: return "blend";
            default: return "invalid";
            }
        }

        const char* to_string(assets::AnimGraphMaskMode mode)
        {
            switch (mode)
            {
            case assets::AnimGraphMaskMode::Include: return "include";
            case assets::AnimGraphMaskMode::Exclude: return "exclude";
            default: return "invalid";
            }
        }

        assets::AnimGraphParamType parse_param_type(std::string_view value)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "float") return assets::AnimGraphParamType::Float;
            if (lowered == "int") return assets::AnimGraphParamType::Int;
            if (lowered == "bool") return assets::AnimGraphParamType::Bool;
            if (lowered == "trigger") return assets::AnimGraphParamType::Trigger;
            return assets::AnimGraphParamType::Invalid;
        }

        assets::AnimGraphConditionOp parse_condition_op(std::string_view value)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "eq" || lowered == "==") return assets::AnimGraphConditionOp::Equal;
            if (lowered == "neq" || lowered == "!=") return assets::AnimGraphConditionOp::NotEqual;
            if (lowered == "lt" || lowered == "<") return assets::AnimGraphConditionOp::Less;
            if (lowered == "gt" || lowered == ">") return assets::AnimGraphConditionOp::Greater;
            if (lowered == "leq" || lowered == "<=") return assets::AnimGraphConditionOp::LessEqual;
            if (lowered == "geq" || lowered == ">=") return assets::AnimGraphConditionOp::GreaterEqual;
            if (lowered == "is_true") return assets::AnimGraphConditionOp::IsTrue;
            if (lowered == "is_false") return assets::AnimGraphConditionOp::IsFalse;
            return assets::AnimGraphConditionOp::Invalid;
        }

        assets::AnimGraphConditionMode parse_condition_mode(std::string_view value)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "all") return assets::AnimGraphConditionMode::All;
            if (lowered == "any") return assets::AnimGraphConditionMode::Any;
            return assets::AnimGraphConditionMode::Invalid;
        }

        assets::AnimGraphStateType parse_state_type(std::string_view value)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "clip") return assets::AnimGraphStateType::Clip;
            if (lowered == "blend2") return assets::AnimGraphStateType::Blend2;
            if (lowered == "blendspace1d" || lowered == "blend1d") return assets::AnimGraphStateType::BlendSpace1D;
            if (lowered == "blendspace2d" || lowered == "blend2d") return assets::AnimGraphStateType::BlendSpace2D;
            return assets::AnimGraphStateType::Invalid;
        }

        assets::AnimGraphPlaybackMode parse_playback_mode(std::string_view value)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "pose") return assets::AnimGraphPlaybackMode::Pose;
            if (lowered == "loop") return assets::AnimGraphPlaybackMode::Loop;
            if (lowered == "mirror") return assets::AnimGraphPlaybackMode::Mirror;
            if (lowered == "clamp") return assets::AnimGraphPlaybackMode::Clamp;
            return assets::AnimGraphPlaybackMode::Invalid;
        }

        assets::AnimGraphBlendMode parse_blend_mode(std::string_view value)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "override") return assets::AnimGraphBlendMode::Override;
            if (lowered == "additive") return assets::AnimGraphBlendMode::Additive;
            if (lowered == "blend") return assets::AnimGraphBlendMode::Blend;
            return assets::AnimGraphBlendMode::Invalid;
        }

        assets::AnimGraphMaskMode parse_mask_mode(std::string_view value)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "include") return assets::AnimGraphMaskMode::Include;
            if (lowered == "exclude") return assets::AnimGraphMaskMode::Exclude;
            return assets::AnimGraphMaskMode::Invalid;
        }

        assets::AnimGraphValue parse_value(const nlohmann::json& j)
        {
            if (j.is_boolean())
                return j.get<bool>();
            if (j.is_number_integer())
                return static_cast<int>(j.get<long long>());
            if (j.is_number_unsigned())
                return static_cast<int>(j.get<unsigned long long>());
            if (j.is_number_float())
                return static_cast<float>(j.get<double>());
            return std::monostate{};
        }

        nlohmann::json serialize_value(const assets::AnimGraphValue& value)
        {
            if (std::holds_alternative<float>(value))
                return nlohmann::json{ std::get<float>(value) };
            if (std::holds_alternative<int>(value))
                return nlohmann::json{ std::get<int>(value) };
            if (std::holds_alternative<bool>(value))
                return nlohmann::json{ std::get<bool>(value) };
            return nlohmann::json{};
        }

        nlohmann::json serialize_param(const assets::AnimGraphParamDef& param)
        {
            nlohmann::json j;
            j["name"] = param.name;
            j["type"] = to_string(param.type);
            switch (param.type)
            {
            case assets::AnimGraphParamType::Int:
                j["default"] = param.default_int;
                break;
            case assets::AnimGraphParamType::Bool:
            case assets::AnimGraphParamType::Trigger:
                j["default"] = param.default_bool;
                break;
            case assets::AnimGraphParamType::Float:
            default:
                j["default"] = param.default_float;
                break;
            }

            if (param.has_min) j["min"] = param.min_value;
            if (param.has_max) j["max"] = param.max_value;
            return j;
        }

        assets::AnimGraphParamDef deserialize_param(const nlohmann::json& j)
        {
            assets::AnimGraphParamDef param{};
            param.name = j.value("name", "");
            param.type = parse_param_type(j.value("type", ""));

            if (j.contains("default"))
            {
                const auto& def = j["default"];
                if (def.is_boolean())
                {
                    param.default_bool = def.get<bool>();
                }
                else if (def.is_number_integer() || def.is_number_unsigned())
                {
                    param.default_int = def.get<int>();
                    param.default_float = static_cast<float>(param.default_int);
                }
                else if (def.is_number_float())
                {
                    param.default_float = def.get<float>();
                    param.default_int = static_cast<int>(param.default_float);
                }
            }

            if (j.contains("min") && j["min"].is_number())
            {
                param.has_min = true;
                param.min_value = j["min"].get<float>();
            }
            if (j.contains("max") && j["max"].is_number())
            {
                param.has_max = true;
                param.max_value = j["max"].get<float>();
            }
            return param;
        }

        nlohmann::json serialize_mask(const assets::AnimGraphMask& mask)
        {
            nlohmann::json j;
            j["name"] = mask.name;
            j["mode"] = to_string(mask.mode);
            nlohmann::json weights = nlohmann::json::array();
            for (const auto& weight : mask.weights)
            {
                nlohmann::json elem;
                elem["bone"] = weight.bone;
                elem["weight"] = weight.weight;
                weights.emplace_back(std::move(elem));
            }
            j["weights"] = std::move(weights);
            return j;
        }

        assets::AnimGraphMask deserialize_mask(const nlohmann::json& j)
        {
            assets::AnimGraphMask mask{};
            mask.name = j.value("name", "");
            mask.mode = parse_mask_mode(j.value("mode", ""));
            if (j.contains("weights") && j["weights"].is_array())
            {
                for (const auto& elem : j["weights"])
                {
                    assets::AnimGraphMaskWeight weight{};
                    weight.bone = elem.value("bone", "");
                    weight.weight = elem.value("weight", 1.0f);
                    mask.weights.push_back(std::move(weight));
                }
            }
            return mask;
        }

        nlohmann::json serialize_state(const assets::AnimGraphState& state)
        {
            nlohmann::json j;
            j["id"] = state.id;
            j["type"] = to_string(state.type);
            j["playback"] = to_string(state.playback);
            j["speed"] = state.speed;
            j["rewind_on_enter"] = state.rewind_on_enter;
            j["trim_left"] = state.trim_left;
            j["trim_right"] = state.trim_right;
            j["sync_children"] = state.sync_children;

            if (state.type == assets::AnimGraphStateType::Clip)
            {
                j["clip"] = state.clip;
            }
            else if (state.type == assets::AnimGraphStateType::Blend2)
            {
                j["clip0"] = state.clip0;
                j["clip1"] = state.clip1;
                j["param"] = state.param_x;
                j["param_min"] = state.param_min_x;
                j["param_max"] = state.param_max_x;
            }
            else if (state.type == assets::AnimGraphStateType::BlendSpace1D)
            {
                j["param"] = state.param_x;
                j["param_min"] = state.param_min_x;
                j["param_max"] = state.param_max_x;
                nlohmann::json samples = nlohmann::json::array();
                for (const auto& sample : state.samples)
                {
                    nlohmann::json elem;
                    elem["clip"] = sample.clip;
                    elem["pos"] = { sample.x };
                    samples.emplace_back(std::move(elem));
                }
                j["samples"] = std::move(samples);
                if (!state.indices.empty())
                    j["indices"] = state.indices;
            }
            else if (state.type == assets::AnimGraphStateType::BlendSpace2D)
            {
                j["param_x"] = state.param_x;
                j["param_y"] = state.param_y;
                j["param_min_x"] = state.param_min_x;
                j["param_max_x"] = state.param_max_x;
                j["param_min_y"] = state.param_min_y;
                j["param_max_y"] = state.param_max_y;
                nlohmann::json samples = nlohmann::json::array();
                for (const auto& sample : state.samples)
                {
                    nlohmann::json elem;
                    elem["clip"] = sample.clip;
                    elem["pos"] = { sample.x, sample.y };
                    samples.emplace_back(std::move(elem));
                }
                j["samples"] = std::move(samples);
                if (!state.indices.empty())
                    j["indices"] = state.indices;
            }

            return j;
        }

        assets::AnimGraphState deserialize_state(const nlohmann::json& j)
        {
            assets::AnimGraphState state{};
            state.id = j.value("id", "");
            state.type = parse_state_type(j.value("type", ""));
            state.speed = j.value("speed", 1.0f);
            state.rewind_on_enter = j.value("rewind_on_enter", false);
            state.trim_left = j.value("trim_left", 0.0f);
            state.trim_right = j.value("trim_right", 1.0f);
            state.sync_children = j.value("sync_children", true);

            if (j.contains("playback"))
                state.playback = parse_playback_mode(j.value("playback", ""));
            else if (j.contains("loop"))
                state.playback = j.value("loop", true) ? assets::AnimGraphPlaybackMode::Loop
                                                       : assets::AnimGraphPlaybackMode::Clamp;

            if (state.type == assets::AnimGraphStateType::Clip)
            {
                state.clip = j.value("clip", "");
            }
            else if (state.type == assets::AnimGraphStateType::Blend2)
            {
                state.clip0 = j.value("clip0", "");
                state.clip1 = j.value("clip1", "");
                state.param_x = j.value("param", "");
                if (j.contains("param_min"))
                    state.param_min_x = j.value("param_min", state.param_min_x);
                else if (j.contains("param_min_x"))
                    state.param_min_x = j.value("param_min_x", state.param_min_x);
                if (j.contains("param_max"))
                    state.param_max_x = j.value("param_max", state.param_max_x);
                else if (j.contains("param_max_x"))
                    state.param_max_x = j.value("param_max_x", state.param_max_x);
            }
            else if (state.type == assets::AnimGraphStateType::BlendSpace1D)
            {
                state.param_x = j.value("param", "");
                if (j.contains("param_min"))
                    state.param_min_x = j.value("param_min", state.param_min_x);
                else if (j.contains("param_min_x"))
                    state.param_min_x = j.value("param_min_x", state.param_min_x);
                if (j.contains("param_max"))
                    state.param_max_x = j.value("param_max", state.param_max_x);
                else if (j.contains("param_max_x"))
                    state.param_max_x = j.value("param_max_x", state.param_max_x);
            }
            else if (state.type == assets::AnimGraphStateType::BlendSpace2D)
            {
                if (j.contains("params") && j["params"].is_array() && j["params"].size() >= 2)
                {
                    state.param_x = j["params"][0].get<std::string>();
                    state.param_y = j["params"][1].get<std::string>();
                }
                else
                {
                    state.param_x = j.value("param_x", "");
                    state.param_y = j.value("param_y", "");
                }

                if (j.contains("param_min") && j["param_min"].is_array() && j["param_min"].size() >= 2)
                {
                    state.param_min_x = j["param_min"][0].get<float>();
                    state.param_min_y = j["param_min"][1].get<float>();
                }
                else
                {
                    state.param_min_x = j.value("param_min_x", state.param_min_x);
                    state.param_min_y = j.value("param_min_y", state.param_min_y);
                }

                if (j.contains("param_max") && j["param_max"].is_array() && j["param_max"].size() >= 2)
                {
                    state.param_max_x = j["param_max"][0].get<float>();
                    state.param_max_y = j["param_max"][1].get<float>();
                }
                else
                {
                    state.param_max_x = j.value("param_max_x", state.param_max_x);
                    state.param_max_y = j.value("param_max_y", state.param_max_y);
                }
            }

            if (j.contains("samples") && j["samples"].is_array())
            {
                for (const auto& elem : j["samples"])
                {
                    assets::AnimGraphBlendSample sample{};
                    sample.clip = elem.value("clip", "");
                    if (elem.contains("pos") && elem["pos"].is_array())
                    {
                        const auto& pos = elem["pos"];
                        if (pos.size() > 0) sample.x = pos[0].get<float>();
                        if (pos.size() > 1) sample.y = pos[1].get<float>();
                    }
                    else
                    {
                        sample.x = elem.value("x", sample.x);
                        sample.y = elem.value("y", sample.y);
                    }
                    state.samples.push_back(std::move(sample));
                }
            }

            if (j.contains("indices") && j["indices"].is_array())
            {
                state.indices = j["indices"].get<std::vector<int>>();
            }

            return state;
        }

        nlohmann::json serialize_conditions(const assets::AnimGraphConditionGroup& group)
        {
            nlohmann::json j;
            j["mode"] = to_string(group.mode);
            nlohmann::json items = nlohmann::json::array();
            for (const auto& cond : group.items)
            {
                nlohmann::json elem;
                elem["param"] = cond.param;
                elem["op"] = to_string(cond.op);
                if (!cond.rhs_param.empty())
                    elem["rhs_param"] = cond.rhs_param;
                else if (!std::holds_alternative<std::monostate>(cond.value))
                    elem["value"] = serialize_value(cond.value);
                items.emplace_back(std::move(elem));
            }
            j["items"] = std::move(items);
            return j;
        }

        assets::AnimGraphConditionGroup deserialize_conditions(const nlohmann::json& j)
        {
            assets::AnimGraphConditionGroup group{};
            if (j.is_array())
            {
                group.mode = assets::AnimGraphConditionMode::All;
                for (const auto& elem : j)
                {
                    assets::AnimGraphCondition cond{};
                    cond.param = elem.value("param", "");
                    cond.op = parse_condition_op(elem.value("op", ""));
                    cond.rhs_param = elem.value("rhs_param", "");
                    if (elem.contains("value"))
                        cond.value = parse_value(elem["value"]);
                    group.items.push_back(std::move(cond));
                }
                return group;
            }

            group.mode = parse_condition_mode(j.value("mode", "all"));
            if (j.contains("items") && j["items"].is_array())
            {
                for (const auto& elem : j["items"])
                {
                    assets::AnimGraphCondition cond{};
                    cond.param = elem.value("param", "");
                    cond.op = parse_condition_op(elem.value("op", ""));
                    cond.rhs_param = elem.value("rhs_param", "");
                    if (elem.contains("value"))
                        cond.value = parse_value(elem["value"]);
                    group.items.push_back(std::move(cond));
                }
            }
            return group;
        }

        nlohmann::json serialize_transition(const assets::AnimGraphTransition& trans)
        {
            nlohmann::json j;
            j["from"] = trans.from;
            j["to"] = trans.to;
            j["duration"] = trans.duration;
            if (trans.has_exit_time)
                j["exit_time"] = trans.exit_time;
            if (trans.priority != 0)
                j["priority"] = trans.priority;
            if (!trans.conditions.items.empty() || trans.conditions.mode != assets::AnimGraphConditionMode::All)
                j["conditions"] = serialize_conditions(trans.conditions);
            return j;
        }

        assets::AnimGraphTransition deserialize_transition(const nlohmann::json& j)
        {
            assets::AnimGraphTransition trans{};
            trans.from = j.value("from", "");
            trans.to = j.value("to", "");
            trans.duration = j.value("duration", 0.0f);
            if (j.contains("exit_time"))
            {
                trans.has_exit_time = true;
                trans.exit_time = j["exit_time"].get<float>();
            }
            trans.priority = j.value("priority", 0);
            if (j.contains("conditions"))
                trans.conditions = deserialize_conditions(j["conditions"]);
            return trans;
        }

        nlohmann::json serialize_layer(const assets::AnimGraphLayer& layer)
        {
            nlohmann::json j;
            j["name"] = layer.name;
            j["weight"] = layer.weight;
            j["blend_mode"] = to_string(layer.blend_mode);
            if (!layer.mask.empty())
                j["mask"] = layer.mask;
            j["entry"] = layer.entry_state;

            nlohmann::json states = nlohmann::json::array();
            for (const auto& state : layer.states)
                states.emplace_back(serialize_state(state));
            j["states"] = std::move(states);

            nlohmann::json transitions = nlohmann::json::array();
            for (const auto& trans : layer.transitions)
                transitions.emplace_back(serialize_transition(trans));
            j["transitions"] = std::move(transitions);
            return j;
        }

        assets::AnimGraphLayer deserialize_layer(const nlohmann::json& j)
        {
            assets::AnimGraphLayer layer{};
            layer.name = j.value("name", "");
            layer.weight = j.value("weight", 1.0f);
            layer.blend_mode = parse_blend_mode(j.value("blend_mode", ""));
            layer.mask = j.value("mask", "");
            layer.entry_state = j.value("entry", "");
            if (layer.entry_state.empty())
                layer.entry_state = j.value("entry_state", "");

            if (j.contains("states") && j["states"].is_array())
            {
                for (const auto& elem : j["states"])
                    layer.states.push_back(deserialize_state(elem));
            }

            if (j.contains("transitions") && j["transitions"].is_array())
            {
                for (const auto& elem : j["transitions"])
                    layer.transitions.push_back(deserialize_transition(elem));
            }
            return layer;
        }

        std::string format_errors(const std::vector<std::string>& errors)
        {
            std::ostringstream oss;
            for (const auto& error : errors)
                oss << "\n- " << error;
            return oss.str();
        }
    }

    void serialize_AnimationGraphAsset(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<assets::AnimationGraphAsset>();
        assert(ptr && "serialize_AnimationGraphAsset: bad meta_any");
        const auto& graph = *ptr;

        j = nlohmann::json::object();
        j["version"] = graph.version;
        j["name"] = graph.name;

        nlohmann::json params = nlohmann::json::array();
        for (const auto& param : graph.params)
            params.emplace_back(serialize_param(param));
        j["params"] = std::move(params);

        nlohmann::json masks = nlohmann::json::array();
        for (const auto& mask : graph.masks)
            masks.emplace_back(serialize_mask(mask));
        j["masks"] = std::move(masks);

        nlohmann::json layers = nlohmann::json::array();
        for (const auto& layer : graph.layers)
            layers.emplace_back(serialize_layer(layer));
        j["layers"] = std::move(layers);
    }

    void deserialize_AnimationGraphAsset(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<assets::AnimationGraphAsset>();
        assert(ptr && "deserialize_AnimationGraphAsset: bad meta_any");
        auto& graph = *ptr;

        graph = assets::AnimationGraphAsset{};
        graph.version = j.value("version", 1);
        graph.name = j.value("name", "");

        if (j.contains("params") && j["params"].is_array())
        {
            for (const auto& elem : j["params"])
                graph.params.push_back(deserialize_param(elem));
        }

        if (j.contains("masks") && j["masks"].is_array())
        {
            for (const auto& elem : j["masks"])
                graph.masks.push_back(deserialize_mask(elem));
        }

        if (j.contains("layers") && j["layers"].is_array())
        {
            for (const auto& elem : j["layers"])
                graph.layers.push_back(deserialize_layer(elem));
        }

        auto errors = assets::validate_animation_graph(graph);
        if (!errors.empty())
            throw std::runtime_error("AnimationGraphAsset validation failed:" + format_errors(errors));
    }

    void register_animationgraphasset_serialization()
    {
        entt::meta_factory<assets::AnimationGraphAsset>{}
            .func<&serialize_AnimationGraphAsset>(eeng::literals::serialize_hs)
            .func<&deserialize_AnimationGraphAsset>(eeng::literals::deserialize_hs);
    }
}
