// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/CommandBatchHelpers.hpp"
#include "BatchRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "engineapi/SelectionManager.hpp"
#include "LogMacros.h"

namespace eeng::editor
{
    bool resolve_loaded_batch_for_entity(
        ecs::Entity entity,
        EngineContext& ctx,
        BatchId& out_batch,
        ecs::EntityRef& out_entity_ref,
        const char* context_label)
    {
        if (!ctx.batch_registry || !ctx.entity_manager)
        {
            EENG_LOG(&ctx, "%s aborted: missing batch or entity manager.", context_label);
            return false;
        }

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        if (!em.try_get_entity_ref(entity, out_entity_ref))
        {
            EENG_LOG(&ctx, "%s failed: entity not registered.", context_label);
            return false;
        }

        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        if (!br.try_get_loaded_batch_for_entity(out_entity_ref, out_batch))
        {
            EENG_LOG(&ctx, "%s failed: entity has no loaded batch.", context_label);
            return false;
        }

        return true;
    }

    bool resolve_batch_for_new_entity(
        const ecs::Entity& parent_entity,
        EngineContext& ctx,
        BatchId& out_batch,
        ecs::EntityRef& out_parent_ref,
        const char* context_label)
    {
        // Policy: all live entities must belong to a loaded batch.
        // New entities use the parent's batch; otherwise use selected batch or fallback to "default".
        if (!ctx.batch_registry || !ctx.entity_manager)
        {
            EENG_LOG(&ctx, "%s aborted: missing batch or entity manager.", context_label);
            return false;
        }

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);

        if (parent_entity.has_id())
        {
            if (!em.try_get_entity_ref(parent_entity, out_parent_ref))
            {
                EENG_LOG(&ctx, "%s failed: parent entity not registered.", context_label);
                return false;
            }

            if (!br.try_get_loaded_batch_for_entity(out_parent_ref, out_batch))
            {
                EENG_LOG(&ctx, "%s failed: parent has no loaded batch.", context_label);
                return false;
            }

            return true;
        }

        BatchId selected{};
        if (ctx.batch_selection && !ctx.batch_selection->empty())
        {
            selected = ctx.batch_selection->last();
        }
        else if (!br.try_get_batch_id_by_name(eeng::BatchRegistry::kDefaultBatchName, selected))
        {
            EENG_LOG(&ctx, "%s failed: default batch not found.", context_label);
            return false;
        }

        if (!br.is_batch_loaded(selected))
        {
            EENG_LOG(&ctx, "%s failed: target batch is not loaded.", context_label);
            return false;
        }

        out_batch = selected;
        return true;
    }

    bool queue_destroy_entity_in_batch(
        ecs::Entity entity,
        EngineContext& ctx,
        std::shared_future<bool>& out_future,
        const char* context_label)
    {
        BatchId batch{};
        ecs::EntityRef entity_ref{};
        if (!resolve_loaded_batch_for_entity(entity, ctx, batch, entity_ref, context_label))
            return false;

        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        out_future = br.queue_destroy_entity(batch, entity_ref, ctx);
        return true;
    }

    bool queue_attach_entity_to_batch(
        ecs::Entity entity,
        const BatchId& batch,
        EngineContext& ctx,
        std::shared_future<bool>& out_future,
        const char* context_label)
    {
        if (!ctx.batch_registry || !ctx.entity_manager)
        {
            EENG_LOG(&ctx, "%s aborted: missing batch or entity manager.", context_label);
            return false;
        }

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        ecs::EntityRef entity_ref{};
        if (!em.try_get_entity_ref(entity, entity_ref))
        {
            EENG_LOG(&ctx, "%s failed: entity not registered.", context_label);
            return false;
        }

        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        if (!br.is_batch_loaded(batch))
        {
            EENG_LOG(&ctx, "%s failed: target batch is not loaded.", context_label);
            return false;
        }

        out_future = br.queue_attach_entity(batch, entity_ref, ctx);
        return true;
    }

    void sync_branch_batch_with_parent(
        ecs::Entity root_entity,
        ecs::Entity parent_entity,
        EngineContext& ctx)
    {
        if (!ctx.batch_registry || !ctx.entity_manager)
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
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
        }
        else
        {
            BatchId default_batch{};
            if (br.try_get_batch_id_by_name(BatchRegistry::kDefaultBatchName, default_batch)
                && br.is_batch_loaded(default_batch))
            {
                parent_batch = default_batch;
                parent_has_batch = true;
            }
            else
            {
                EENG_LOG(&ctx, "sync_branch_batch_with_parent: default batch missing or unloaded.");
            }
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

            if (parent_has_batch)
            {
                if (!has_current || current_batch != parent_batch)
                {
                    if (has_current)
                        br.queue_detach_entity(current_batch, entity_ref, ctx);
                    br.queue_attach_entity(parent_batch, entity_ref, ctx);
                }
            }
            else if (has_current)
            {
                br.queue_detach_entity(current_batch, entity_ref, ctx);
            }
        }
    }
}
