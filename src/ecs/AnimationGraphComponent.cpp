// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/AnimationGraphComponent.hpp"

#include <algorithm>

#include <entt/entt.hpp>

#include "EngineContextHelpers.hpp"
#include "editor/MetaFieldPath.hpp"

namespace eeng::ecs
{
    namespace
    {
        void reset_instance(AnimGraphInstance& instance)
        {
            instance.graph_guid = Guid::invalid();
            instance.float_params.clear();
            instance.int_params.clear();
            instance.bool_params.clear();
            instance.trigger_params.clear();
            instance.layers.clear();
            instance.initialized = false;
        }

        int find_state_index(const assets::AnimGraphLayer& layer, const std::string& state_id)
        {
            for (std::size_t i = 0; i < layer.states.size(); i++)
            {
                if (layer.states[i].id == state_id)
                    return static_cast<int>(i);
            }
            return -1;
        }

        void initialize_instance(
            AnimGraphInstance& instance,
            const assets::AnimationGraphAsset& graph,
            const Guid& graph_guid)
        {
            reset_instance(instance);

            for (const auto& param : graph.params)
            {
                switch (param.type)
                {
                case assets::AnimGraphParamType::Float:
                    instance.float_params.push_back(param.default_float);
                    break;
                case assets::AnimGraphParamType::Int:
                    instance.int_params.push_back(param.default_int);
                    break;
                case assets::AnimGraphParamType::Bool:
                    instance.bool_params.push_back(param.default_bool ? 1u : 0u);
                    break;
                case assets::AnimGraphParamType::Trigger:
                    instance.trigger_params.push_back(param.default_bool ? 1u : 0u);
                    break;
                default:
                    break;
                }
            }

            instance.layers.resize(graph.layers.size());
            for (std::size_t i = 0; i < graph.layers.size(); i++)
            {
                const auto& layer = graph.layers[i];
                auto& runtime = instance.layers[i];
                runtime.state = find_state_index(layer, layer.entry_state);
                runtime.state_time = 0.0f;
                runtime.transition = {};
                runtime.weight = layer.weight;
            }

            instance.graph_guid = graph_guid;
            instance.initialized = true;
        }
    }

    void AnimationGraphComponent::on_component_post_bind(entt::meta_any& any, EngineContext& ctx)
    {
        auto* comp = any.try_cast<AnimationGraphComponent>();
        if (!comp)
            return;

        if (!comp->graph_ref.is_bound())
            return;

        if (comp->instance.initialized && comp->instance.graph_guid == comp->graph_ref.guid)
            return;

        auto rm = eeng::try_get_resource_manager(ctx, "AnimationGraphComponent");
        if (!rm)
            return;

        eeng::try_read_asset_ref(
            *rm,
            comp->graph_ref,
            ctx,
            "AnimationGraphComponent",
            "Missing AnimationGraphAsset for AnimationGraphComponent:",
            [&](const assets::AnimationGraphAsset& graph)
            {
                initialize_instance(comp->instance, graph, comp->graph_ref.guid);
            });
    }

    void AnimationGraphComponent::on_component_post_assign(
        EngineContext& ctx,
        const ecs::Entity& entity,
        const editor::MetaFieldPath& meta_path,
        bool is_undo)
    {
        (void)meta_path;
        (void)is_undo;

        auto* registry = eeng::try_get_registry_ptr(ctx, "AnimationGraphComponent");
        if (!registry || !registry->valid(entity))
            return;

        if (auto* comp = registry->try_get<AnimationGraphComponent>(entity))
            reset_instance(comp->instance);
    }
}
