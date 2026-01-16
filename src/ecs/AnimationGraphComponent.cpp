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

            std::vector<assets::AnimationGraphAsset::RuntimeCache::ParamSlot> slots;
            const auto& runtime_slots = graph.runtime.param_slots;
            if (graph.runtime.built && runtime_slots.size() == graph.params.size())
            {
                slots = runtime_slots;
            }
            else
            {
                slots.resize(graph.params.size());
                std::size_t float_index = 0;
                std::size_t int_index = 0;
                std::size_t bool_index = 0;
                std::size_t trigger_index = 0;
                for (std::size_t i = 0; i < graph.params.size(); i++)
                {
                    slots[i].type = graph.params[i].type;
                    switch (graph.params[i].type)
                    {
                    case assets::AnimGraphParamType::Float:
                        slots[i].index = float_index++;
                        break;
                    case assets::AnimGraphParamType::Int:
                        slots[i].index = int_index++;
                        break;
                    case assets::AnimGraphParamType::Bool:
                        slots[i].index = bool_index++;
                        break;
                    case assets::AnimGraphParamType::Trigger:
                        slots[i].index = trigger_index++;
                        break;
                    default:
                        slots[i].index = 0;
                        break;
                    }
                }
            }

            std::size_t float_count = 0;
            std::size_t int_count = 0;
            std::size_t bool_count = 0;
            std::size_t trigger_count = 0;
            for (const auto& slot : slots)
            {
                switch (slot.type)
                {
                case assets::AnimGraphParamType::Float:
                    float_count = std::max(float_count, slot.index + 1);
                    break;
                case assets::AnimGraphParamType::Int:
                    int_count = std::max(int_count, slot.index + 1);
                    break;
                case assets::AnimGraphParamType::Bool:
                    bool_count = std::max(bool_count, slot.index + 1);
                    break;
                case assets::AnimGraphParamType::Trigger:
                    trigger_count = std::max(trigger_count, slot.index + 1);
                    break;
                default:
                    break;
                }
            }

            instance.float_params.assign(float_count, 0.0f);
            instance.int_params.assign(int_count, 0);
            instance.bool_params.assign(bool_count, 0u);
            instance.trigger_params.assign(trigger_count, 0u);

            for (std::size_t i = 0; i < graph.params.size(); i++)
            {
                const auto& param = graph.params[i];
                const auto& slot = slots[i];
                switch (param.type)
                {
                case assets::AnimGraphParamType::Float:
                    if (slot.index < instance.float_params.size())
                        instance.float_params[slot.index] = param.default_float;
                    break;
                case assets::AnimGraphParamType::Int:
                    if (slot.index < instance.int_params.size())
                        instance.int_params[slot.index] = param.default_int;
                    break;
                case assets::AnimGraphParamType::Bool:
                    if (slot.index < instance.bool_params.size())
                        instance.bool_params[slot.index] = param.default_bool ? 1u : 0u;
                    break;
                case assets::AnimGraphParamType::Trigger:
                    if (slot.index < instance.trigger_params.size())
                        instance.trigger_params[slot.index] = param.default_bool ? 1u : 0u;
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
        // Called after AssetRef<> fields are bound; sync runtime to the graph asset if needed.
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
        // Called after an inspector edit; reset or re-init runtime depending on which field changed.
        (void)is_undo;

        auto* registry = eeng::try_get_registry_ptr(ctx, "AnimationGraphComponent");
        if (!registry || !registry->valid(entity))
            return;

        auto* comp = registry->try_get<AnimationGraphComponent>(entity);
        if (!comp)
            return;

        const bool has_path = !meta_path.entries.empty();
        const bool graph_ref_changed = has_path && meta_path.entries.front().name == "graph_ref";
        const bool enabled_changed = has_path && meta_path.entries.front().name == "enabled";

        if (graph_ref_changed)
        {
            // Graph changed: drop the old runtime so post_bind can rebuild it.
            reset_instance(comp->instance);
            return;
        }

        if (enabled_changed)
        {
            // Enabled toggled on: ensure runtime is initialized without needing a closure rebuild.
            if (comp->enabled && comp->graph_ref.is_bound() && !comp->instance.initialized)
            {
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
        }
    }
}
