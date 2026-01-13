// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/AnimationSystem.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

#include "EngineContext.hpp"
#include "EngineContextHelpers.hpp"
#include "ecs/ModelComponent.hpp"
#include "assets/types/ModelAssets.hpp"

namespace
{
    using namespace eeng::assets;

    /// @brief Resolve an animation clip pointer by index (or nullptr if invalid).
    const AnimClip* resolve_clip(const ModelDataAsset& model, int clip_index)
    {
        if (clip_index < 0 || static_cast<size_t>(clip_index) >= model.animations.size())
            return nullptr;
        return &model.animations[static_cast<size_t>(clip_index)];
    }

    /// @brief Convert seconds to normalized [0,1] clip time, honoring loop/clamp.
    float normalized_time(const AnimClip* clip, float time_sec, bool loop)
    {
        if (!clip || clip->duration_ticks <= 0.0f || clip->ticks_per_second <= 0.0f)
            return 0.0f;

        const float animdur_sec = clip->duration_ticks / clip->ticks_per_second;
        if (animdur_sec <= 0.0f)
            return 0.0f;

        float animtime_sec = time_sec;
        if (loop)
            animtime_sec = std::fmod(time_sec, animdur_sec);
        else
            animtime_sec = std::min(std::max(time_sec, 0.0f), animdur_sec);

        const float animtime_ticks = animtime_sec * clip->ticks_per_second;
        const float ntime = animtime_ticks / clip->duration_ticks;
        return std::min(std::max(ntime, 0.0f), 1.0f);
    }

    inline glm::mat4 dualquat_to_mat4(const glm::dualquat& dq)
    {
        // Extract the real (rotation) part and dual part (translation info).
        const glm::quat real_part = dq.real;
        const glm::quat dual_part = dq.dual;

        // t = 2 * (dual * conjugate(real)).
        glm::quat t = dual_part * glm::conjugate(real_part);
        glm::vec3 translation(t.x, t.y, t.z);
        translation *= 2.0f;

        glm::mat4 transform = glm::mat4_cast(real_part);
        transform[3] = glm::vec4(translation, 1.0f);
        return transform;
    }

    struct NodeSample
    {
        glm::vec3 pos{ 0.0f };
        glm::quat rot{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 scale{ 1.0f };
        bool track_used = false;
    };

    NodeSample sample_node_trs(
        size_t node_index,
        const AnimClip* clip,
        float ntime,
        const VecTree<SkeletonNode>& nodetree)
    {
        const auto& node = nodetree.get_payload_at(node_index);

        // Decompose bind pose for missing key channels.
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

        if (!clip)
            return sample;

        const auto& track = clip->node_animations[node_index];
        if (!track.is_used)
            return sample;

        sample.track_used = true;

        const size_t nbr_pos_keys = track.pos_keys.size();
        if (nbr_pos_keys > 0)
        {
            const float pos_indexf = ntime * (nbr_pos_keys - 1u);
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
            const float rot_indexf = ntime * (nbr_rot_keys - 1u);
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
            const float scale_indexf = ntime * (nbr_scale_keys - 1u);
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

    /// @brief Evaluate a node local transform using uniformly spaced keys.
    /// @note This mirrors the legacy RenderableMesh behavior and ignores key timestamps.
    glm::mat4 evaluate_node_transform(
        size_t node_index,
        const AnimClip* clip,
        float ntime,
        const VecTree<SkeletonNode>& nodetree)
    {
        const auto& node = nodetree.get_payload_at(node_index);
        if (!clip)
            return node.local_bind_tfm;

        const NodeSample sample = sample_node_trs(node_index, clip, ntime, nodetree);
        if (!sample.track_used)
            return node.local_bind_tfm;

        return compose_trs(sample);
    }

    glm::mat4 evaluate_blend_node_transform(
        size_t node_index,
        const AnimClip* clip0,
        const AnimClip* clip1,
        float ntime0,
        float ntime1,
        float frac,
        const VecTree<SkeletonNode>& nodetree)
    {
        const auto& node = nodetree.get_payload_at(node_index);
        if (!clip0 || !clip1)
            return node.local_bind_tfm;

        const NodeSample sample0 = sample_node_trs(node_index, clip0, ntime0, nodetree);
        const NodeSample sample1 = sample_node_trs(node_index, clip1, ntime1, nodetree);

        if (!sample0.track_used || !sample1.track_used)
            return node.local_bind_tfm;

        const float blend = std::min(std::max(frac, 0.0f), 1.0f);

        glm::dualquat dqA = glm::dualquat(sample0.rot, sample0.pos);
        glm::dualquat dqB = glm::dualquat(sample1.rot, sample1.pos);
        glm::dualquat dq = glm::normalize(glm::lerp(dqA, dqB, blend));

        glm::mat4 transform = dualquat_to_mat4(dq);
        transform = glm::scale(transform, glm::mix(sample0.scale, sample1.scale, blend));
        return transform;
    }
}

namespace eeng::ecs::systems
{
    /// @brief Evaluate animation for all ModelComponent instances and update bone matrices.
    void AnimationSystem::update(entt::registry& registry, EngineContext& ctx, float delta_time)
    {
        auto rm = eeng::try_get_resource_manager(ctx, "AnimationSystem");
        if (!rm) return;

        auto view = registry.view<ecs::ModelComponent>();
        for (auto&& [entity, model_component] : view.each())
        {
            if (!model_component.model_ref.is_bound())
                continue;

            Handle<assets::ModelDataAsset> model_handle{};
            eeng::Guid model_guid = eeng::Guid::invalid();
            const bool gpu_read = eeng::try_read_asset_ref(
                *rm,
                model_component.model_ref,
                ctx,
                "AnimationSystem",
                "Missing GpuModelAsset for ModelComponent:",
                [&](const assets::GpuModelAsset& gpu)
                {
                    model_handle = gpu.model_ref.handle;
                    model_guid = gpu.model_ref.guid;
                });

            if (!gpu_read)
            {
                // TODO: consider binding a placeholder model for animation.
                continue;
            }

            if (!eeng::try_read_asset(
                *rm,
                model_handle,
                model_guid,
                ctx,
                "AnimationSystem",
                "Missing ModelDataAsset for ModelComponent:",
                [&](const assets::ModelDataAsset& model)
                {
                    model_component.clip_time += delta_time * model_component.clip_speed;
                    const bool blend_requested = model_component.blend_clip_index >= 0;
                    if (blend_requested)
                        model_component.blend_clip_time += delta_time * model_component.blend_clip_speed;

                    const auto* clip0 = resolve_clip(model, model_component.clip_index);
                    const auto* clip1 = blend_requested
                        ? resolve_clip(model, model_component.blend_clip_index)
                        : nullptr;

                    const float ntime0 = normalized_time(clip0, model_component.clip_time, model_component.loop);
                    const float ntime1 = blend_requested
                        ? normalized_time(clip1, model_component.blend_clip_time, model_component.blend_loop)
                        : 0.0f;
                    const float blend = blend_requested
                        ? std::min(std::max(model_component.blend_factor, 0.0f), 1.0f)
                        : 0.0f;
                    const bool use_blend = blend_requested && clip0 && clip1;

                    const size_t node_count = model.nodetree.size();
                    if (model_component.node_global_matrices.size() != node_count)
                        model_component.node_global_matrices.assign(node_count, glm::mat4(1.0f));

                    const size_t bone_count = model.bones.size();
                    if (model_component.bone_matrices.size() != bone_count)
                        model_component.bone_matrices.assign(bone_count, glm::mat4(1.0f));

                    model.nodetree.traverse_depthfirst(
                        [&](const assets::SkeletonNode*,
                            const assets::SkeletonNode* parent,
                            size_t node_index,
                            size_t parent_index)
                        {
                            glm::mat4 local = use_blend
                                ? evaluate_blend_node_transform(
                                    node_index,
                                    clip0,
                                    clip1,
                                    ntime0,
                                    ntime1,
                                    blend,
                                    model.nodetree)
                                : evaluate_node_transform(node_index, clip0, ntime0, model.nodetree);
                            // Global pose = parent global * local (model space).
                            if (parent)
                                local = model_component.node_global_matrices[parent_index] * local;
                            model_component.node_global_matrices[node_index] = local;
                        });

                    for (size_t i = 0; i < bone_count; i++)
                    {
                        const auto& bone = model.bones[i];
                        if (bone.node_index == assets::null_index)
                        {
                            model_component.bone_matrices[i] = glm::mat4(1.0f);
                            continue;
                        }

                        const auto& node_tfm = model_component.node_global_matrices[bone.node_index];
                        // Skinning matrix: pose in model space * inverse bind (bind -> bone local).
                        model_component.bone_matrices[i] = node_tfm * bone.inverse_bind_tfm;
                    }
                }))
            {
                // TODO: consider binding a placeholder model for animation.
                continue;
            }
        }
    }
}
