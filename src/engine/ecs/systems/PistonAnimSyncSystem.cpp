// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/PistonAnimSyncSystem.hpp"

#include "engineapi/EngineContextHelpers.hpp"
#include "ecs/AnimationGraphComponent.hpp"
#include "ecs/EntityManager.hpp"
#include "ecs/ModelComponent.hpp"
#include "ecs/PistonAnimSyncComponent.hpp"
#include "ecs/PistonConstraintDriveComponent.hpp"
#include "assets/types/ModelAssets.hpp"

#include <algorithm>
#include <entt/entt.hpp>

namespace eeng::ecs::systems
{
    namespace
    {
        bool clip_has_tracks(const eeng::assets::AnimClip& clip)
        {
            for (const auto& track : clip.node_animations)
            {
                if (track.is_used)
                    return true;
            }
            return false;
        }

        const eeng::assets::AnimClip* resolve_clip_by_name(
            const eeng::assets::ModelDataAsset& model,
            const std::string& clip_name)
        {
            if (clip_name.empty())
                return nullptr;

            const eeng::assets::AnimClip* fallback = nullptr;
            for (const auto& clip : model.animations)
            {
                if (clip.name != clip_name)
                    continue;
                if (!fallback)
                    fallback = &clip;
                if (clip_has_tracks(clip))
                    return &clip;
            }
            return fallback;
        }

        float clip_duration_sec(const eeng::assets::AnimClip* clip)
        {
            if (!clip || clip->duration_ticks <= 0.0f || clip->ticks_per_second <= 0.0f)
                return 0.0f;
            return clip->duration_ticks / clip->ticks_per_second;
        }

        int find_state_index(const eeng::assets::AnimGraphLayer& layer, const std::string& state_id)
        {
            for (std::size_t i = 0; i < layer.states.size(); i++)
            {
                if (layer.states[i].id == state_id)
                    return static_cast<int>(i);
            }
            return -1;
        }

        std::string resolve_clip_name_from_graph(
            const eeng::assets::AnimationGraphAsset& graph,
            const eeng::ecs::AnimGraphInstance& instance)
        {
            if (graph.layers.empty())
                return {};

            const auto& layer = graph.layers[0];
            int state_index = -1;
            if (instance.initialized && !instance.layers.empty())
                state_index = instance.layers[0].state;
            if (state_index < 0 || state_index >= static_cast<int>(layer.states.size()))
                state_index = find_state_index(layer, layer.entry_state);
            if (state_index < 0 || state_index >= static_cast<int>(layer.states.size()))
                return {};

            const auto& state = layer.states[static_cast<std::size_t>(state_index)];
            if (state.type == eeng::assets::AnimGraphStateType::Clip)
                return state.clip;
            return {};
        }

        const eeng::assets::AnimClip* resolve_fallback_clip(const eeng::assets::ModelDataAsset& model)
        {
            for (const auto& clip : model.animations)
            {
                if (clip_has_tracks(clip))
                    return &clip;
            }
            return model.animations.empty() ? nullptr : &model.animations.front();
        }

        bool resolve_target_entity(
            eeng::EntityManager& em,
            entt::registry& registry,
            entt::entity root,
            eeng::ecs::PistonAnimSyncComponent& sync)
        {
            if (sync.target.is_bound())
            {
                const entt::entity target_entity = static_cast<entt::entity>(sync.target.entity);
                if (registry.valid(target_entity))
                    return true;
            }

            if (sync.target.guid.valid())
            {
                if (auto e_opt = em.get_entity_from_guid(sync.target.guid))
                {
                    if (e_opt->has_id() && registry.valid(static_cast<entt::entity>(*e_opt)))
                    {
                        sync.target.bind(*e_opt);
                        return true;
                    }
                }
            }

            if (!sync.auto_find_target)
                return false;

            const auto branch = em.scene_graph().get_branch_topdown(eeng::ecs::Entity{ root });
            for (const auto& ent : branch)
            {
                const entt::entity candidate = static_cast<entt::entity>(ent);
                if (!registry.valid(candidate))
                    continue;
                if (!registry.all_of<eeng::ecs::AnimationGraphComponent, eeng::ecs::ModelComponent>(candidate))
                    continue;

                sync.target = em.get_entity_ref(ent);
                return true;
            }

            return false;
        }
    } // namespace

    void PistonAnimSyncSystem::update(entt::registry& registry, EngineContext& ctx, float)
    {
        auto* em = eeng::try_get_entity_manager_ptr(ctx, "PistonAnimSyncSystem");
        if (!em)
            return;

        auto rm = eeng::try_get_resource_manager(ctx, "PistonAnimSyncSystem");
        if (!rm)
            return;

        auto view = registry.view<ecs::PistonAnimSyncComponent, ecs::PistonConstraintDriveComponent>();
        for (const auto entity : view)
        {
            auto& sync = view.get<ecs::PistonAnimSyncComponent>(entity);
            const auto& drive = view.get<ecs::PistonConstraintDriveComponent>(entity);
            if (!sync.enabled || !drive.enabled)
                continue;

            if (!resolve_target_entity(*em, registry, entity, sync))
                continue;

            const entt::entity target_entity = static_cast<entt::entity>(sync.target.entity);
            if (!registry.valid(target_entity))
                continue;

            auto* graph = registry.try_get<ecs::AnimationGraphComponent>(target_entity);
            auto* model = registry.try_get<ecs::ModelComponent>(target_entity);
            if (!graph || !model)
                continue;
            if (!graph->enabled || !graph->graph_ref.is_bound())
                continue;
            if (!model->model_ref.is_bound())
                continue;
            if (!graph->instance.initialized)
            {
                auto any = entt::forward_as_meta(*graph);
                ecs::AnimationGraphComponent::on_component_post_bind(any, ctx);
            }
            if (!graph->instance.initialized || graph->instance.layers.empty())
                continue;

            Handle<assets::ModelDataAsset> model_handle{};
            Guid model_guid = Guid::invalid();
            const bool gpu_read = eeng::try_read_asset_ref(
                *rm,
                model->model_ref,
                ctx,
                "PistonAnimSyncSystem",
                "Missing GpuModelAsset for PistonAnimSync:",
                [&](const assets::GpuModelAsset& gpu)
                {
                    model_handle = gpu.model_ref.handle;
                    model_guid = gpu.model_ref.guid;
                });
            if (!gpu_read)
                continue;

            float duration_sec = 0.0f;
            std::string resolved_clip = sync.clip_name;
            const bool read_ok = eeng::try_read_asset_pair(
                *rm,
                model_handle,
                model_guid,
                graph->graph_ref.handle,
                graph->graph_ref.guid,
                ctx,
                "PistonAnimSyncSystem",
                "Missing ModelDataAsset for PistonAnimSync:",
                "Missing AnimationGraphAsset for PistonAnimSync:",
                [&](const assets::ModelDataAsset& model_data, const assets::AnimationGraphAsset& graph_data)
                {
                    const auto* clip = !resolved_clip.empty()
                        ? resolve_clip_by_name(model_data, resolved_clip)
                        : nullptr;

                    if (!clip)
                    {
                        const std::string graph_clip = resolve_clip_name_from_graph(graph_data, graph->instance);
                        if (!graph_clip.empty())
                        {
                            resolved_clip = graph_clip;
                            clip = resolve_clip_by_name(model_data, resolved_clip);
                        }
                    }

                    if (!clip)
                        clip = resolve_fallback_clip(model_data);

                    duration_sec = clip_duration_sec(clip);
                });
            if (!read_ok || duration_sec <= 0.0f)
                continue;

            const float extension = std::clamp(drive.current_extension, 0.0f, 1.0f);
            const float time_sec = extension * duration_sec;

            // Policy: Piston animations are pose-driven; we overwrite state_time each frame.
            graph->instance.layers[0].state_time = time_sec;
        }
    }
} // namespace eeng::ecs::systems
