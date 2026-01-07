// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/EntityBatchPolicy.hpp"
#include "BatchRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "LogMacros.h"
#include <string>

namespace eeng::ecs
{
    namespace
    {
        void policy_log(EngineContext* ctx, std::string_view context_label, const char* message)
        {
            if (!ctx)
                return;
            EENG_LOG(ctx, "%s %s", std::string(context_label).c_str(), message);
        }
    }

    BatchPolicyDecision EntityBatchPolicy::resolve_new_entity_batch(
        const ecs::Entity& parent_entity,
        const BatchId* preferred_batch,
        const BatchId* default_batch,
        const BatchPolicyContext& ctx,
        std::string_view context_label)
    {
        BatchPolicyDecision decision{};
        if (!ctx.batch_registry || !ctx.entity_manager)
        {
            policy_log(ctx.engine_context, context_label, "aborted: missing batch or entity manager.");
            return decision;
        }

        auto& em = *ctx.entity_manager;
        auto& br = *ctx.batch_registry;

        if (parent_entity.has_id())
        {
            ecs::EntityRef parent_ref{};
            if (!em.try_get_entity_ref(parent_entity, parent_ref))
            {
                policy_log(ctx.engine_context, context_label, "failed: parent entity not registered.");
                return decision;
            }

            BatchId parent_batch{};
            if (!br.try_get_loaded_batch_for_entity(parent_ref, parent_batch))
            {
                policy_log(ctx.engine_context, context_label, "failed: parent has no loaded batch.");
                return decision;
            }

            decision.ok = true;
            decision.batch = parent_batch;
            decision.parent_ref = parent_ref;
            return decision;
        }

        const BatchId* target = nullptr;
        if (preferred_batch && preferred_batch->valid())
            target = preferred_batch;
        else if (default_batch && default_batch->valid())
            target = default_batch;

        if (!target)
        {
            policy_log(ctx.engine_context, context_label, "failed: no target batch provided.");
            return decision;
        }

        if (!br.is_batch_loaded(*target))
        {
            policy_log(ctx.engine_context, context_label, "failed: target batch is not loaded.");
            return decision;
        }

        decision.ok = true;
        decision.batch = *target;
        return decision;
    }

    void EntityBatchPolicy::sync_branch_to_parent_batch(
        const ecs::Entity& root_entity,
        const ecs::Entity& parent_entity,
        const BatchId* default_batch,
        const BatchPolicyContext& ctx)
    {
        if (!ctx.batch_registry || !ctx.entity_manager)
            return;
        if (!ctx.engine_context)
            return;

        auto& em = *ctx.entity_manager;
        auto& br = *ctx.batch_registry;
        auto& scenegraph = em.scene_graph();

        if (!scenegraph.contains(root_entity))
            return;

        BatchId parent_batch{};
        bool parent_has_batch = false;

        if (parent_entity.has_id())
        {
            const auto parent_ref = em.get_entity_ref(parent_entity);
            if (parent_ref.is_bound() && parent_ref.guid.valid())
                parent_has_batch = br.try_get_loaded_batch_for_entity(parent_ref, parent_batch);

            if (!parent_has_batch)
            {
                policy_log(ctx.engine_context, "Reparent", "failed: parent has no loaded batch.");
                return;
            }
        }
        else if (default_batch && default_batch->valid() && br.is_batch_loaded(*default_batch))
        {
            parent_batch = *default_batch;
            parent_has_batch = true;
        }
        else
        {
            policy_log(ctx.engine_context, "Reparent",
                "failed: default batch missing or unloaded.");
            return;
        }

        const auto branch = scenegraph.get_branch_topdown(root_entity);
        for (const auto& entity : branch)
        {
            if (!entity.has_id() || !em.entity_valid(entity))
                continue;

            const auto entity_ref = em.get_entity_ref(entity);
            if (!entity_ref.is_bound() || !entity_ref.guid.valid())
                continue;

            BatchId current_batch{};
            const bool has_current = br.try_get_loaded_batch_for_entity(entity_ref, current_batch);

            if (!has_current || current_batch != parent_batch)
            {
                if (has_current)
                    br.queue_detach_entity(current_batch, entity_ref, *ctx.engine_context);
                br.queue_attach_entity(parent_batch, entity_ref, *ctx.engine_context);
            }
        }
    }
}
