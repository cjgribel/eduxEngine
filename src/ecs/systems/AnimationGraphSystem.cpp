// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/AnimationGraphSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>
#include <vector>

#include <entt/entt.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

#include "EngineContext.hpp"
#include "EngineContextHelpers.hpp"
#include "ecs/AnimationGraphComponent.hpp"
#include "ecs/ModelComponent.hpp"
#include "assets/AnimationGraphRuntime.hpp"
#include "assets/types/ModelAssets.hpp"

namespace
{
    using namespace eeng::assets;

    struct NodeSample
    {
        glm::vec3 pos{ 0.0f };
        glm::quat rot{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f };
        bool track_used = false;
    };

    struct StateEvalContext
    {
        const AnimGraphState* state = nullptr;
        std::array<const AnimClip*, 4> clips{};
        std::array<float, 4> weights{};
        float ntime = 0.0f;
        int sample_count = 0;
        bool valid = false;
    };

    struct LayerEvalContext
    {
        const AnimGraphLayer* layer = nullptr;
        StateEvalContext from_ctx{};
        StateEvalContext to_ctx{};
        float transition_alpha = 0.0f;
        bool in_transition = false;
    };

    inline glm::mat4 dualquat_to_mat4(const glm::dualquat& dq)
    {
        const glm::quat real_part = dq.real;
        const glm::quat dual_part = dq.dual;
        glm::quat t = dual_part * glm::conjugate(real_part);
        glm::vec3 translation(t.x, t.y, t.z);
        translation *= 2.0f;

        glm::mat4 transform = glm::mat4_cast(real_part);
        transform[3] = glm::vec4(translation, 1.0f);
        return transform;
    }

    NodeSample bind_pose_sample(const SkeletonNode& node)
    {
        const glm::vec3 bind_pos = glm::vec3(node.local_bind_tfm[3]);
        glm::vec3 bind_scale;
        bind_scale.x = glm::length(glm::vec3(node.local_bind_tfm[0]));
        bind_scale.y = glm::length(glm::vec3(node.local_bind_tfm[1]));
        bind_scale.z = glm::length(glm::vec3(node.local_bind_tfm[2]));
        if (bind_scale.x == 0.0f) bind_scale.x = 1.0f;
        if (bind_scale.y == 0.0f) bind_scale.y = 1.0f;
        if (bind_scale.z == 0.0f) bind_scale.z = 1.0f;

        glm::mat3 bind_rot_m(
            glm::vec3(node.local_bind_tfm[0]) / bind_scale.x,
            glm::vec3(node.local_bind_tfm[1]) / bind_scale.y,
            glm::vec3(node.local_bind_tfm[2]) / bind_scale.z);
        const glm::quat bind_rot = glm::quat_cast(bind_rot_m);

        NodeSample sample{};
        sample.pos = bind_pos;
        sample.rot = bind_rot;
        sample.scale = bind_scale;
        sample.track_used = false;
        return sample;
    }

    NodeSample sample_node_trs(
        size_t node_index,
        const AnimClip* clip,
        float ntime,
        const VecTree<SkeletonNode>& nodetree)
    {
        const auto& node = nodetree.get_payload_at(node_index);
        NodeSample sample = bind_pose_sample(node);

        if (!clip)
            return sample;

        if (node_index >= clip->node_animations.size())
            return sample;

        const auto& track = clip->node_animations[node_index];
        if (!track.is_used)
            return sample;

        sample.track_used = true;

        const size_t nbr_pos_keys = track.pos_keys.size();
        if (nbr_pos_keys > 0)
        {
            const float pos_indexf = ntime * static_cast<float>(nbr_pos_keys - 1u);
            const size_t pos_index0 = static_cast<size_t>(std::floor(pos_indexf));
            const size_t pos_index1 = std::min(pos_index0 + 1u, nbr_pos_keys - 1u);
            sample.pos = glm::mix(
                track.pos_keys[pos_index0],
                track.pos_keys[pos_index1],
                pos_indexf - static_cast<float>(pos_index0));
        }

        const size_t nbr_rot_keys = track.rot_keys.size();
        if (nbr_rot_keys > 0)
        {
            const float rot_indexf = ntime * static_cast<float>(nbr_rot_keys - 1u);
            const size_t rot_index0 = static_cast<size_t>(std::floor(rot_indexf));
            const size_t rot_index1 = std::min(rot_index0 + 1u, nbr_rot_keys - 1u);
            sample.rot = glm::slerp(
                track.rot_keys[rot_index0],
                track.rot_keys[rot_index1],
                rot_indexf - static_cast<float>(rot_index0));
        }

        const size_t nbr_scale_keys = track.scale_keys.size();
        if (nbr_scale_keys > 0)
        {
            const float scale_indexf = ntime * static_cast<float>(nbr_scale_keys - 1u);
            const size_t scale_index0 = static_cast<size_t>(std::floor(scale_indexf));
            const size_t scale_index1 = std::min(scale_index0 + 1u, nbr_scale_keys - 1u);
            sample.scale = glm::mix(
                track.scale_keys[scale_index0],
                track.scale_keys[scale_index1],
                scale_indexf - static_cast<float>(scale_index0));
        }

        return sample;
    }

    glm::mat4 compose_trs(const NodeSample& sample)
    {
        const glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), sample.pos);
        const glm::mat4 rotation_matrix = glm::mat4_cast(sample.rot);
        const glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), sample.scale);
        return translation_matrix * rotation_matrix * scale_matrix;
    }

    NodeSample blend_samples(const NodeSample& a, const NodeSample& b, float weight)
    {
        const float blend = std::min(std::max(weight, 0.0f), 1.0f);
        glm::dualquat dqA = glm::dualquat(a.rot, a.pos);
        glm::dualquat dqB = glm::dualquat(b.rot, b.pos);
        glm::dualquat dq = glm::normalize(glm::lerp(dqA, dqB, blend));

        glm::quat t = dq.dual * glm::conjugate(dq.real);
        NodeSample out{};
        out.rot = dq.real;
        out.pos = glm::vec3(t.x, t.y, t.z) * 2.0f;
        out.scale = glm::mix(a.scale, b.scale, blend);
        out.track_used = a.track_used || b.track_used;
        return out;
    }

    NodeSample blend_samples4(
        const std::array<NodeSample, 4>& samples,
        const std::array<float, 4>& weights)
    {
        float sum = weights[0] + weights[1] + weights[2] + weights[3];
        if (sum <= 0.0f)
            return samples[0];

        const float inv_sum = 1.0f / sum;
        const std::array<float, 4> w = {
            weights[0] * inv_sum,
            weights[1] * inv_sum,
            weights[2] * inv_sum,
            weights[3] * inv_sum
        };

        const glm::dualquat dq0 = glm::dualquat(samples[0].rot, samples[0].pos);
        const glm::dualquat dq1 = glm::dualquat(samples[1].rot, samples[1].pos);
        const glm::dualquat dq2 = glm::dualquat(samples[2].rot, samples[2].pos);
        const glm::dualquat dq3 = glm::dualquat(samples[3].rot, samples[3].pos);

        float c1 = (glm::dot(dq0.real, dq1.real) < 0.0f) ? -w[1] : w[1];
        float c2 = (glm::dot(dq0.real, dq2.real) < 0.0f) ? -w[2] : w[2];
        float c3 = (glm::dot(dq0.real, dq3.real) < 0.0f) ? -w[3] : w[3];
        glm::dualquat dq = dq0 * w[0] + dq1 * c1 + dq2 * c2 + dq3 * c3;
        dq = glm::normalize(dq);

        glm::quat t = dq.dual * glm::conjugate(dq.real);
        NodeSample out{};
        out.rot = dq.real;
        out.pos = glm::vec3(t.x, t.y, t.z) * 2.0f;
        out.scale = samples[0].scale * w[0]
            + samples[1].scale * w[1]
            + samples[2].scale * w[2]
            + samples[3].scale * w[3];
        out.track_used = samples[0].track_used || samples[1].track_used
            || samples[2].track_used || samples[3].track_used;
        return out;
    }

    std::string to_lower_ascii(std::string_view value)
    {
        std::string out(value);
        for (char& c : out)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return out;
    }

    bool clip_has_tracks(const AnimClip& clip)
    {
        for (const auto& track : clip.node_animations)
        {
            if (track.is_used)
                return true;
        }
        return false;
    }

    const AnimClip* resolve_clip_by_name(const ModelDataAsset& model, const std::string& clip_name)
    {
        if (clip_name.empty())
            return nullptr;

        const std::string clip_name_lower = to_lower_ascii(clip_name);
        const AnimClip* fallback = nullptr;
        for (const auto& clip : model.animations)
        {
            if (to_lower_ascii(clip.name) != clip_name_lower)
                continue;
            if (!fallback)
                fallback = &clip;
            if (clip_has_tracks(clip))
                return &clip;
        }
        return fallback;
    }

    float normalized_time(const AnimClip* clip, float time_sec, AnimGraphPlaybackMode mode)
    {
        if (!clip || clip->duration_ticks <= 0.0f || clip->ticks_per_second <= 0.0f)
            return 0.0f;

        const float animdur_sec = clip->duration_ticks / clip->ticks_per_second;
        if (animdur_sec <= 0.0f)
            return 0.0f;

        float animtime_sec = time_sec;
        switch (mode)
        {
        case AnimGraphPlaybackMode::Loop:
            animtime_sec = std::fmod(time_sec, animdur_sec);
            if (animtime_sec < 0.0f)
                animtime_sec += animdur_sec;
            break;
        case AnimGraphPlaybackMode::Mirror:
        {
            const float cycle = animdur_sec * 2.0f;
            animtime_sec = std::fmod(time_sec, cycle);
            if (animtime_sec < 0.0f)
                animtime_sec += cycle;
            if (animtime_sec > animdur_sec)
                animtime_sec = cycle - animtime_sec;
            break;
        }
        case AnimGraphPlaybackMode::Clamp:
            animtime_sec = std::min(std::max(time_sec, 0.0f), animdur_sec);
            break;
        case AnimGraphPlaybackMode::Pose:
        default:
            animtime_sec = 0.0f;
            break;
        }

        const float animtime_ticks = animtime_sec * clip->ticks_per_second;
        const float ntime = animtime_ticks / clip->duration_ticks;
        return std::min(std::max(ntime, 0.0f), 1.0f);
    }

    float trimmed_time(const AnimGraphState& state, const AnimClip* clip, float time_sec)
    {
        if (state.playback == AnimGraphPlaybackMode::Pose)
        {
            return std::min(std::max(state.trim_left, 0.0f), 1.0f);
        }

        float ntime = normalized_time(clip, time_sec, state.playback);
        ntime = state.trim_left + ntime * (state.trim_right - state.trim_left);
        return std::min(std::max(ntime, 0.0f), 1.0f);
    }

    bool get_param_slot(
        const AnimationGraphAsset& graph,
        std::string_view name,
        AnimGraphParamType& type_out,
        std::size_t& index_out)
    {
        if (graph.runtime.built)
        {
            auto it = graph.runtime.param_index.find(std::string(name));
            if (it == graph.runtime.param_index.end())
                return false;
            if (it->second >= graph.runtime.param_slots.size())
                return false;
            const auto& slot = graph.runtime.param_slots[it->second];
            type_out = slot.type;
            index_out = slot.index;
            return slot.type != AnimGraphParamType::Invalid;
        }

        std::size_t float_index = 0;
        std::size_t int_index = 0;
        std::size_t bool_index = 0;
        std::size_t trigger_index = 0;
        for (const auto& param : graph.params)
        {
            if (param.name == name)
            {
                type_out = param.type;
                switch (param.type)
                {
                case AnimGraphParamType::Float: index_out = float_index; break;
                case AnimGraphParamType::Int: index_out = int_index; break;
                case AnimGraphParamType::Bool: index_out = bool_index; break;
                case AnimGraphParamType::Trigger: index_out = trigger_index; break;
                default: index_out = 0; break;
                }
                return param.type != AnimGraphParamType::Invalid;
            }

            switch (param.type)
            {
            case AnimGraphParamType::Float: ++float_index; break;
            case AnimGraphParamType::Int: ++int_index; break;
            case AnimGraphParamType::Bool: ++bool_index; break;
            case AnimGraphParamType::Trigger: ++trigger_index; break;
            default: break;
            }
        }
        return false;
    }

    bool get_param_float(
        const AnimationGraphAsset& graph,
        const eeng::ecs::AnimGraphInstance& instance,
        std::string_view name,
        float& out_value)
    {
        AnimGraphParamType type = AnimGraphParamType::Invalid;
        std::size_t index = 0;
        if (!get_param_slot(graph, name, type, index))
            return false;

        switch (type)
        {
        case AnimGraphParamType::Float:
            if (index < instance.float_params.size())
            {
                out_value = instance.float_params[index];
                return true;
            }
            break;
        case AnimGraphParamType::Int:
            if (index < instance.int_params.size())
            {
                out_value = static_cast<float>(instance.int_params[index]);
                return true;
            }
            break;
        case AnimGraphParamType::Bool:
            if (index < instance.bool_params.size())
            {
                out_value = instance.bool_params[index] ? 1.0f : 0.0f;
                return true;
            }
            break;
        case AnimGraphParamType::Trigger:
            if (index < instance.trigger_params.size())
            {
                out_value = instance.trigger_params[index] ? 1.0f : 0.0f;
                return true;
            }
            break;
        default:
            break;
        }
        return false;
    }

    bool get_param_bool(
        const AnimationGraphAsset& graph,
        const eeng::ecs::AnimGraphInstance& instance,
        std::string_view name,
        bool& out_value)
    {
        AnimGraphParamType type = AnimGraphParamType::Invalid;
        std::size_t index = 0;
        if (!get_param_slot(graph, name, type, index))
            return false;

        switch (type)
        {
        case AnimGraphParamType::Bool:
            if (index < instance.bool_params.size())
            {
                out_value = instance.bool_params[index] != 0;
                return true;
            }
            break;
        case AnimGraphParamType::Trigger:
            if (index < instance.trigger_params.size())
            {
                out_value = instance.trigger_params[index] != 0;
                return true;
            }
            break;
        default:
            break;
        }
        return false;
    }

    bool evaluate_condition(
        const AnimGraphCondition& cond,
        const AnimationGraphAsset& graph,
        const eeng::ecs::AnimGraphInstance& instance)
    {
        AnimGraphParamType lhs_type = AnimGraphParamType::Invalid;
        std::size_t lhs_index = 0;
        if (!get_param_slot(graph, cond.param, lhs_type, lhs_index))
            return false;

        const bool rhs_param = !cond.rhs_param.empty();

        if (lhs_type == AnimGraphParamType::Bool || lhs_type == AnimGraphParamType::Trigger)
        {
            bool lhs = false;
            if (!get_param_bool(graph, instance, cond.param, lhs))
                return false;

            bool rhs = false;
            if (rhs_param)
            {
                if (!get_param_bool(graph, instance, cond.rhs_param, rhs))
                    return false;
            }
            else if (std::holds_alternative<bool>(cond.value))
            {
                rhs = std::get<bool>(cond.value);
            }

            switch (cond.op)
            {
            case AnimGraphConditionOp::Equal: return lhs == rhs;
            case AnimGraphConditionOp::NotEqual: return lhs != rhs;
            case AnimGraphConditionOp::IsTrue: return lhs;
            case AnimGraphConditionOp::IsFalse: return !lhs;
            default: return false;
            }
        }

        float lhs = 0.0f;
        if (!get_param_float(graph, instance, cond.param, lhs))
            return false;

        float rhs = 0.0f;
        if (rhs_param)
        {
            if (!get_param_float(graph, instance, cond.rhs_param, rhs))
                return false;
        }
        else if (std::holds_alternative<float>(cond.value))
        {
            rhs = std::get<float>(cond.value);
        }
        else if (std::holds_alternative<int>(cond.value))
        {
            rhs = static_cast<float>(std::get<int>(cond.value));
        }

        switch (cond.op)
        {
        case AnimGraphConditionOp::Equal: return lhs == rhs;
        case AnimGraphConditionOp::NotEqual: return lhs != rhs;
        case AnimGraphConditionOp::Less: return lhs < rhs;
        case AnimGraphConditionOp::Greater: return lhs > rhs;
        case AnimGraphConditionOp::LessEqual: return lhs <= rhs;
        case AnimGraphConditionOp::GreaterEqual: return lhs >= rhs;
        default: return false;
        }
    }

    bool evaluate_conditions(
        const AnimGraphConditionGroup& group,
        const AnimationGraphAsset& graph,
        const eeng::ecs::AnimGraphInstance& instance)
    {
        if (group.items.empty())
            return true;

        if (group.mode == AnimGraphConditionMode::Any)
        {
            for (const auto& cond : group.items)
            {
                if (evaluate_condition(cond, graph, instance))
                    return true;
            }
            return false;
        }

        for (const auto& cond : group.items)
        {
            if (!evaluate_condition(cond, graph, instance))
                return false;
        }
        return true;
    }

    void find_roots(float a, float b, float c, int& nbr_roots, float& root1, float& root2)
    {
        constexpr float tol = 0.00001f;
        if (std::fabs(a) > tol)
        {
            float d = b * b - 4.0f * a * c;
            if (d < 0.0f)
            {
                nbr_roots = 0;
            }
            else
            {
                const float denom = 1.0f / (2.0f * a);
                if (std::fabs(d) < tol)
                {
                    nbr_roots = 1;
                    root1 = -b * denom;
                }
                else
                {
                    nbr_roots = 2;
                    float sr = std::sqrt(d);
                    root1 = (-b + sr) * denom;
                    root2 = (-b - sr) * denom;
                }
            }
        }
        else
        {
            if (std::fabs(b) > tol)
            {
                nbr_roots = 1;
                root1 = -c / b;
            }
            else
            {
                nbr_roots = 0;
            }
        }
    }

    bool compute_bilinear_weights(
        const glm::vec2& x,
        const glm::vec2& v0,
        const glm::vec2& v1,
        const glm::vec2& v2,
        const glm::vec2& v3,
        glm::vec4& weights)
    {
        const glm::vec2 a = v0 - x;
        const glm::vec2 b = v1 - v0;
        const glm::vec2 c = v3 - v0;
        const glm::vec2 d = v0 - v1 - v3 + v2;

        const float c0 = c.x * d.y - c.y * d.x;
        const float c1 = c.x * b.y - c.y * b.x + a.x * d.y - a.y * d.x;
        const float c2 = a.x * b.y - a.y * b.x;

        float m = -1.0f;
        float r1 = -1.0f;
        float r2 = -1.0f;
        int rn = 0;
        find_roots(c0, c1, c2, rn, r1, r2);
        if (r1 >= 0.0f && r1 <= 1.0f)
            m = r1;
        else
            m = r2;
        if (m < 0.0f || m > 1.0f)
            return false;

        const float denom = m * (d.y - d.x) + b.y - b.x;
        if (std::fabs(denom) < 0.00001f)
            return false;

        const float l = (m * (c.x - c.y) + a.x - a.y) / denom;
        if (l < 0.0f || l > 1.0f)
            return false;

        weights = glm::vec4(
            (1.0f - l) * (1.0f - m),
            l * (1.0f - m),
            l * m,
            (1.0f - l) * m);
        return true;
    }

    StateEvalContext build_state_context(
        const AnimGraphState& state,
        const AnimationGraphAsset& graph,
        const eeng::ecs::AnimGraphInstance& instance,
        const ModelDataAsset& model,
        float time_sec)
    {
        StateEvalContext ctx{};
        ctx.state = &state;

        const AnimClip* time_clip = nullptr;

        switch (state.type)
        {
        case AnimGraphStateType::Clip:
            ctx.clips[0] = resolve_clip_by_name(model, state.clip);
            ctx.sample_count = 1;
            time_clip = ctx.clips[0];
            break;
        case AnimGraphStateType::Blend2:
            ctx.clips[0] = resolve_clip_by_name(model, state.clip0);
            ctx.clips[1] = resolve_clip_by_name(model, state.clip1);
            ctx.sample_count = 2;
            time_clip = ctx.clips[0] ? ctx.clips[0] : ctx.clips[1];
            {
                float value = 0.0f;
                if (!state.param_x.empty())
                    get_param_float(graph, instance, state.param_x, value);
                const float span = state.param_max_x - state.param_min_x;
                float t = span > 0.0f ? (value - state.param_min_x) / span : 0.0f;
                t = std::min(std::max(t, 0.0f), 1.0f);
                ctx.weights[0] = 1.0f - t;
                ctx.weights[1] = t;
            }
            break;
        case AnimGraphStateType::BlendSpace1D:
        {
            ctx.sample_count = 2;
            float x = 0.0f;
            if (!state.param_x.empty())
                get_param_float(graph, instance, state.param_x, x);

            int a = -1;
            int b = -1;
            float t = 0.0f;
            bool found = false;

            if (!state.indices.empty())
            {
                for (std::size_t i = 0; i + 1 < state.indices.size(); i += 2)
                {
                    const int i0 = state.indices[i];
                    const int i1 = state.indices[i + 1];
                    if (i0 < 0 || i1 < 0 || static_cast<std::size_t>(i0) >= state.samples.size()
                        || static_cast<std::size_t>(i1) >= state.samples.size())
                        continue;
                    const float x0 = state.samples[i0].x;
                    const float x1 = state.samples[i1].x;
                    const float minx = std::min(x0, x1);
                    const float maxx = std::max(x0, x1);
                    if (x >= minx && x <= maxx)
                    {
                        const float denom = (x1 - x0);
                        t = std::fabs(denom) > 0.00001f ? (x - x0) / denom : 0.0f;
                        a = i0;
                        b = i1;
                        found = true;
                        break;
                    }
                }
            }

            if (!found && state.samples.size() >= 2)
            {
                std::vector<int> order(state.samples.size());
                for (std::size_t i = 0; i < order.size(); i++)
                    order[i] = static_cast<int>(i);
                std::sort(order.begin(), order.end(), [&](int lhs, int rhs)
                    {
                        return state.samples[static_cast<std::size_t>(lhs)].x
                            < state.samples[static_cast<std::size_t>(rhs)].x;
                    });

                for (std::size_t i = 0; i + 1 < order.size(); i++)
                {
                    const int i0 = order[i];
                    const int i1 = order[i + 1];
                    const float x0 = state.samples[static_cast<std::size_t>(i0)].x;
                    const float x1 = state.samples[static_cast<std::size_t>(i1)].x;
                    if (x >= x0 && x <= x1)
                    {
                        const float denom = (x1 - x0);
                        t = std::fabs(denom) > 0.00001f ? (x - x0) / denom : 0.0f;
                        a = i0;
                        b = i1;
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    a = order.front();
                    b = order.back();
                    const float x0 = state.samples[static_cast<std::size_t>(a)].x;
                    const float x1 = state.samples[static_cast<std::size_t>(b)].x;
                    const float denom = (x1 - x0);
                    t = std::fabs(denom) > 0.00001f ? (x - x0) / denom : 0.0f;
                }
            }

            if (a >= 0 && b >= 0)
            {
                ctx.clips[0] = resolve_clip_by_name(model, state.samples[static_cast<std::size_t>(a)].clip);
                ctx.clips[1] = resolve_clip_by_name(model, state.samples[static_cast<std::size_t>(b)].clip);
                ctx.weights[0] = 1.0f - t;
                ctx.weights[1] = t;
                time_clip = ctx.clips[0] ? ctx.clips[0] : ctx.clips[1];
            }
            break;
        }
        case AnimGraphStateType::BlendSpace2D:
        {
            ctx.sample_count = 4;
            float x = 0.0f;
            float y = 0.0f;
            if (!state.param_x.empty())
                get_param_float(graph, instance, state.param_x, x);
            if (!state.param_y.empty())
                get_param_float(graph, instance, state.param_y, y);

            bool found = false;
            glm::vec4 w{};
            std::array<int, 4> indices{};

            if (!state.indices.empty())
            {
                for (std::size_t i = 0; i + 3 < state.indices.size(); i += 4)
                {
                    indices = { state.indices[i], state.indices[i + 1], state.indices[i + 2], state.indices[i + 3] };
                    if (indices[0] < 0 || indices[1] < 0 || indices[2] < 0 || indices[3] < 0)
                        continue;
                    if (static_cast<std::size_t>(indices[0]) >= state.samples.size()
                        || static_cast<std::size_t>(indices[1]) >= state.samples.size()
                        || static_cast<std::size_t>(indices[2]) >= state.samples.size()
                        || static_cast<std::size_t>(indices[3]) >= state.samples.size())
                        continue;

                    const glm::vec2 v0(state.samples[static_cast<std::size_t>(indices[0])].x,
                        state.samples[static_cast<std::size_t>(indices[0])].y);
                    const glm::vec2 v1(state.samples[static_cast<std::size_t>(indices[1])].x,
                        state.samples[static_cast<std::size_t>(indices[1])].y);
                    const glm::vec2 v2(state.samples[static_cast<std::size_t>(indices[2])].x,
                        state.samples[static_cast<std::size_t>(indices[2])].y);
                    const glm::vec2 v3(state.samples[static_cast<std::size_t>(indices[3])].x,
                        state.samples[static_cast<std::size_t>(indices[3])].y);
                    if (compute_bilinear_weights(glm::vec2(x, y), v0, v1, v2, v3, w))
                    {
                        found = true;
                        break;
                    }
                }
            }

            if (!found && state.samples.size() >= 4)
            {
                indices = { 0, 1, 2, 3 };
                w = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                found = true;
            }

            if (found)
            {
                ctx.clips[0] = resolve_clip_by_name(model, state.samples[static_cast<std::size_t>(indices[0])].clip);
                ctx.clips[1] = resolve_clip_by_name(model, state.samples[static_cast<std::size_t>(indices[1])].clip);
                ctx.clips[2] = resolve_clip_by_name(model, state.samples[static_cast<std::size_t>(indices[2])].clip);
                ctx.clips[3] = resolve_clip_by_name(model, state.samples[static_cast<std::size_t>(indices[3])].clip);
                ctx.weights = { w.x, w.y, w.z, w.w };
                time_clip = ctx.clips[0] ? ctx.clips[0] : ctx.clips[1];
            }
            break;
        }
        default:
            break;
        }

        if (ctx.sample_count > 0)
        {
            ctx.ntime = trimmed_time(state, time_clip, time_sec);
            ctx.valid = true;
        }

        return ctx;
    }

    NodeSample sample_state_node(
        size_t node_index,
        const StateEvalContext& ctx,
        const VecTree<SkeletonNode>& nodetree)
    {
        const auto& node = nodetree.get_payload_at(node_index);
        if (!ctx.valid || !ctx.state)
            return bind_pose_sample(node);

        if (ctx.sample_count == 1)
        {
            return sample_node_trs(node_index, ctx.clips[0], ctx.ntime, nodetree);
        }
        if (ctx.sample_count == 2)
        {
            NodeSample a = sample_node_trs(node_index, ctx.clips[0], ctx.ntime, nodetree);
            NodeSample b = sample_node_trs(node_index, ctx.clips[1], ctx.ntime, nodetree);
            return blend_samples(a, b, ctx.weights[1]);
        }
        if (ctx.sample_count == 4)
        {
            std::array<NodeSample, 4> samples = {
                sample_node_trs(node_index, ctx.clips[0], ctx.ntime, nodetree),
                sample_node_trs(node_index, ctx.clips[1], ctx.ntime, nodetree),
                sample_node_trs(node_index, ctx.clips[2], ctx.ntime, nodetree),
                sample_node_trs(node_index, ctx.clips[3], ctx.ntime, nodetree)
            };
            return blend_samples4(samples, ctx.weights);
        }
        return bind_pose_sample(node);
    }

    float layer_mask_weight(
        const AnimationGraphAsset& graph,
        const AnimGraphLayer& layer,
        const SkeletonNode& node)
    {
        if (layer.mask.empty())
            return 1.0f;

        std::size_t mask_index = graph.masks.size();
        if (graph.runtime.built)
        {
            auto it = graph.runtime.mask_index.find(layer.mask);
            if (it != graph.runtime.mask_index.end())
                mask_index = it->second;
        }
        if (mask_index >= graph.masks.size())
        {
            for (std::size_t i = 0; i < graph.masks.size(); i++)
            {
                if (graph.masks[i].name == layer.mask)
                {
                    mask_index = i;
                    break;
                }
            }
        }
        if (mask_index >= graph.masks.size())
            return 1.0f;

        const auto& mask = graph.masks[mask_index];
        const float default_weight = (mask.mode == AnimGraphMaskMode::Include) ? 0.0f : 1.0f;

        for (const auto& weight : mask.weights)
        {
            if (weight.bone == node.name)
                return (mask.mode == AnimGraphMaskMode::Exclude) ? 0.0f : weight.weight;
        }

        return default_weight;
    }

    glm::mat4 blend_matrices(const glm::mat4& a, const glm::mat4& b, float weight)
    {
        NodeSample sa{};
        NodeSample sb{};

        sa.pos = glm::vec3(a[3]);
        sb.pos = glm::vec3(b[3]);

        auto extract_sample = [](const glm::mat4& m, NodeSample& s)
        {
            glm::vec3 scale;
            scale.x = glm::length(glm::vec3(m[0]));
            scale.y = glm::length(glm::vec3(m[1]));
            scale.z = glm::length(glm::vec3(m[2]));
            if (scale.x == 0.0f) scale.x = 1.0f;
            if (scale.y == 0.0f) scale.y = 1.0f;
            if (scale.z == 0.0f) scale.z = 1.0f;

            glm::mat3 rot_m(
                glm::vec3(m[0]) / scale.x,
                glm::vec3(m[1]) / scale.y,
                glm::vec3(m[2]) / scale.z);

            s.scale = scale;
            s.rot = glm::quat_cast(rot_m);
        };

        extract_sample(a, sa);
        extract_sample(b, sb);

        NodeSample blended = blend_samples(sa, sb, weight);
        return compose_trs(blended);
    }

    void advance_state_time(
        const AnimGraphState& state,
        float delta_time,
        float& time_sec)
    {
        if (state.playback == AnimGraphPlaybackMode::Pose)
            return;
        time_sec += delta_time * state.speed;
    }

    float state_exit_time(
        const AnimGraphState& state,
        const ModelDataAsset& model,
        float time_sec)
    {
        const AnimClip* clip = nullptr;
        switch (state.type)
        {
        case AnimGraphStateType::Clip:
            clip = resolve_clip_by_name(model, state.clip);
            break;
        case AnimGraphStateType::Blend2:
            clip = resolve_clip_by_name(model, state.clip0);
            break;
        case AnimGraphStateType::BlendSpace1D:
        case AnimGraphStateType::BlendSpace2D:
            if (!state.samples.empty())
                clip = resolve_clip_by_name(model, state.samples.front().clip);
            break;
        default:
            break;
        }
        return trimmed_time(state, clip, time_sec);
    }
}

namespace eeng::ecs::systems
{
    void AnimationGraphSystem::update(entt::registry& registry, EngineContext& ctx, float delta_time)
    {
        auto rm = eeng::try_get_resource_manager(ctx, "AnimationGraphSystem");
        if (!rm) return;

        auto view = registry.view<ecs::ModelComponent, ecs::AnimationGraphComponent>();
        for (auto&& [entity, model_component, graph_component] : view.each())
        {
            if (!graph_component.enabled)
                continue;
            if (!model_component.model_ref.is_bound())
                continue;
            if (!graph_component.graph_ref.is_bound())
                continue;

            Handle<assets::ModelDataAsset> model_handle{};
            eeng::Guid model_guid = eeng::Guid::invalid();
            const bool gpu_read = eeng::try_read_asset_ref(
                *rm,
                model_component.model_ref,
                ctx,
                "AnimationGraphSystem",
                "Missing GpuModelAsset for AnimationGraphComponent:",
                [&](const assets::GpuModelAsset& gpu)
                {
                    model_handle = gpu.model_ref.handle;
                    model_guid = gpu.model_ref.guid;
                });
            if (!gpu_read)
                continue;

            if (!eeng::try_read_asset(
                *rm,
                model_handle,
                model_guid,
                ctx,
                "AnimationGraphSystem",
                "Missing ModelDataAsset for AnimationGraphComponent:",
                [&](const assets::ModelDataAsset& model)
                {
                    eeng::try_read_asset_ref(
                        *rm,
                        graph_component.graph_ref,
                        ctx,
                        "AnimationGraphSystem",
                        "Missing AnimationGraphAsset for AnimationGraphComponent:",
                        [&](const assets::AnimationGraphAsset& graph)
                        {
                            auto& instance = graph_component.instance;
                            if (!instance.initialized)
                                return;

                            std::vector<LayerEvalContext> layer_contexts;
                            layer_contexts.reserve(graph.layers.size());

                            for (std::size_t i = 0; i < graph.layers.size(); i++)
                            {
                                const auto& layer = graph.layers[i];
                                auto& runtime = instance.layers[i];
                                LayerEvalContext lctx{};
                                lctx.layer = &layer;

                                if (runtime.transition.active && runtime.transition.to >= 0
                                    && runtime.transition.to < static_cast<int>(layer.states.size()))
                                {
                                    const auto& from_state = layer.states[static_cast<std::size_t>(runtime.transition.from)];
                                    const auto& to_state = layer.states[static_cast<std::size_t>(runtime.transition.to)];
                                    lctx.from_ctx = build_state_context(from_state, graph, instance, model, runtime.state_time);
                                    lctx.to_ctx = build_state_context(to_state, graph, instance, model, runtime.transition.dest_time);
                                    lctx.transition_alpha = runtime.transition.duration > 0.0f
                                        ? std::min(runtime.transition.time / runtime.transition.duration, 1.0f)
                                        : 1.0f;
                                    lctx.in_transition = true;
                                }
                                else if (runtime.state >= 0 && runtime.state < static_cast<int>(layer.states.size()))
                                {
                                    const auto& state = layer.states[static_cast<std::size_t>(runtime.state)];
                                    lctx.from_ctx = build_state_context(state, graph, instance, model, runtime.state_time);
                                    lctx.in_transition = false;
                                }

                                layer_contexts.push_back(std::move(lctx));
                            }

                            for (std::size_t i = 0; i < graph.layers.size(); i++)
                            {
                                if (i >= instance.layers.size())
                                    continue;

                                auto& runtime = instance.layers[i];
                                const auto& layer = graph.layers[i];

                                if (runtime.transition.active)
                                {
                                    runtime.transition.time += delta_time;
                                    const auto& to_state = layer.states[static_cast<std::size_t>(runtime.transition.to)];
                                    advance_state_time(to_state, delta_time, runtime.transition.dest_time);
                                    if (runtime.transition.time >= runtime.transition.duration)
                                    {
                                        runtime.state = runtime.transition.to;
                                        runtime.state_time = runtime.transition.dest_time;
                                        runtime.transition = {};
                                    }
                                    continue;
                                }

                                if (runtime.state < 0 || runtime.state >= static_cast<int>(layer.states.size()))
                                    continue;

                                const auto& state = layer.states[static_cast<std::size_t>(runtime.state)];
                                advance_state_time(state, delta_time, runtime.state_time);

                                int best_transition = -1;
                                int best_priority = std::numeric_limits<int>::min();
                                float state_ntime = state_exit_time(state, model, runtime.state_time);

                                for (std::size_t t = 0; t < layer.transitions.size(); t++)
                                {
                                    const auto& trans = layer.transitions[t];
                                    if (trans.from != "*" && trans.from != state.id)
                                        continue;
                                    if (trans.has_exit_time && state_ntime < trans.exit_time)
                                        continue;
                                    if (!evaluate_conditions(trans.conditions, graph, instance))
                                        continue;

                                    if (trans.priority >= best_priority)
                                    {
                                        best_priority = trans.priority;
                                        best_transition = static_cast<int>(t);
                                    }
                                }

                                if (best_transition >= 0)
                                {
                                    const auto& trans = layer.transitions[static_cast<std::size_t>(best_transition)];
                                    int dest = -1;
                                    if (graph.runtime.built && i < graph.runtime.layers.size())
                                    {
                                        auto it = graph.runtime.layers[i].state_index.find(trans.to);
                                        if (it != graph.runtime.layers[i].state_index.end())
                                            dest = static_cast<int>(it->second);
                                    }
                                    if (dest < 0)
                                    {
                                        for (std::size_t s = 0; s < layer.states.size(); s++)
                                        {
                                            if (layer.states[s].id == trans.to)
                                            {
                                                dest = static_cast<int>(s);
                                                break;
                                            }
                                        }
                                    }
                                    if (dest < 0)
                                        continue;
                                    const auto& dest_state = layer.states[static_cast<std::size_t>(dest)];
                                    if (trans.duration <= 0.0f)
                                    {
                                        runtime.state = dest;
                                        runtime.state_time = dest_state.rewind_on_enter ? 0.0f : runtime.state_time;
                                    }
                                    else
                                    {
                                        runtime.transition.active = true;
                                        runtime.transition.from = runtime.state;
                                        runtime.transition.to = dest;
                                        runtime.transition.time = 0.0f;
                                        runtime.transition.duration = trans.duration;
                                        runtime.transition.dest_time = dest_state.rewind_on_enter ? 0.0f : runtime.state_time;
                                    }
                                }
                            }

                            const std::size_t node_count = model.nodetree.size();
                            if (model_component.node_global_matrices.size() != node_count)
                                model_component.node_global_matrices.assign(node_count, glm::mat4(1.0f));

                            const std::size_t bone_count = model.bones.size();
                            if (model_component.bone_matrices.size() != bone_count)
                                model_component.bone_matrices.assign(bone_count, glm::mat4(1.0f));

                            model.nodetree.traverse_depthfirst(
                                [&](const assets::SkeletonNode* node,
                                    const assets::SkeletonNode* parent,
                                    std::size_t node_index,
                                    std::size_t parent_index)
                                {
                                    glm::mat4 local = node->local_bind_tfm;

                                    for (const auto& lctx : layer_contexts)
                                    {
                                        if (!lctx.layer)
                                            continue;

                                        float weight = lctx.layer->weight;
                                        if (weight <= 0.0f)
                                            continue;

                                        const float mask_weight = layer_mask_weight(graph, *lctx.layer, *node);
                                        if (mask_weight <= 0.0f)
                                            continue;

                                        const float layer_weight = std::min(std::max(weight * mask_weight, 0.0f), 1.0f);

                                        NodeSample sample = lctx.in_transition
                                            ? blend_samples(
                                                sample_state_node(node_index, lctx.from_ctx, model.nodetree),
                                                sample_state_node(node_index, lctx.to_ctx, model.nodetree),
                                                lctx.transition_alpha)
                                            : sample_state_node(node_index, lctx.from_ctx, model.nodetree);

                                        glm::mat4 layer_local = compose_trs(sample);

                                        if (lctx.layer->blend_mode == AnimGraphBlendMode::Additive)
                                        {
                                            const glm::mat4 delta = layer_local * glm::inverse(node->local_bind_tfm);
                                            const glm::mat4 delta_blend = blend_matrices(glm::mat4(1.0f), delta, layer_weight);
                                            local = delta_blend * local;
                                        }
                                        else
                                        {
                                            local = blend_matrices(local, layer_local, layer_weight);
                                        }
                                    }

                                    if (parent)
                                        local = model_component.node_global_matrices[parent_index] * local;
                                    model_component.node_global_matrices[node_index] = local;
                                });

                            for (std::size_t i = 0; i < bone_count; i++)
                            {
                                const auto& bone = model.bones[i];
                                if (bone.node_index == assets::null_index)
                                {
                                    model_component.bone_matrices[i] = glm::mat4(1.0f);
                                    continue;
                                }

                                const auto& node_tfm = model_component.node_global_matrices[bone.node_index];
                                model_component.bone_matrices[i] = node_tfm * bone.inverse_bind_tfm;
                            }
                        });
                }))
            {
                continue;
            }
        }
    }
}
