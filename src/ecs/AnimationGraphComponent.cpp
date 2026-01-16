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

        // Policy: Runtime is initialized from the bound asset when needed, never from stale data.
        bool try_init_from_asset(AnimationGraphComponent& component, EngineContext& ctx)
        {
            if (!component.graph_ref.is_bound())
                return false;

            auto rm = eeng::try_get_resource_manager(ctx, "AnimationGraphComponent");
            if (!rm)
                return false;

            eeng::try_read_asset_ref(
                *rm,
                component.graph_ref,
                ctx,
                "AnimationGraphComponent",
                "Missing AnimationGraphAsset for AnimationGraphComponent:",
                [&](const assets::AnimationGraphAsset& graph)
                {
                    initialize_instance(component.instance, graph, component.graph_ref.guid);
                });
            return true;
        }

        // Policy: Root entries are ignored; only Data entries name a field.
        const editor::MetaFieldPath::Entry* find_first_data_entry(const editor::MetaFieldPath& meta_path)
        {
            for (const auto& entry : meta_path.entries)
            {
                if (entry.type == editor::MetaFieldPath::Entry::Type::Data)
                    return &entry;
            }
            return nullptr;
        }

        // Policy: If the bound graph changes or is unbound, drop cached runtime to avoid stale data.
        void reset_if_unbound_or_mismatch(AnimationGraphComponent& component)
        {
            if (!component.graph_ref.is_bound())
            {
                if (component.instance.initialized)
                    reset_instance(component.instance);
                return;
            }

            if (component.instance.initialized && component.instance.graph_guid != component.graph_ref.guid)
                reset_instance(component.instance);
        }
    }

    void AnimationGraphComponent::on_component_post_bind(entt::meta_any& any, EngineContext& ctx)
    {
        // Called after AssetRef<> fields are bound; sync runtime to the graph asset if needed.
        auto* comp = any.try_cast<AnimationGraphComponent>();
        if (!comp)
            return;

        reset_if_unbound_or_mismatch(*comp);
        if (comp->graph_ref.is_bound() && !comp->instance.initialized)
            try_init_from_asset(*comp, ctx);
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

        // Policy: Root entries are ignored; we react to the first Data entry if present.
        const editor::MetaFieldPath::Entry* first_data = find_first_data_entry(meta_path);
        const bool graph_ref_changed = first_data && first_data->name == "graph_ref";
        const bool enabled_changed = first_data && first_data->name == "enabled";

        if (!first_data)
        {
            reset_if_unbound_or_mismatch(*comp);
            if (comp->enabled && !comp->instance.initialized)
                try_init_from_asset(*comp, ctx);
            return;
        }

        if (graph_ref_changed)
        {
            // Policy: Graph change invalidates cached runtime; bind hook will rebuild.
            reset_instance(comp->instance);
            return;
        }

        if (enabled_changed)
        {
            // Policy: Enabling should initialize runtime immediately, without waiting for a closure rebuild.
            if (comp->enabled && comp->graph_ref.is_bound() && !comp->instance.initialized)
                try_init_from_asset(*comp, ctx);
        }
    }
}
