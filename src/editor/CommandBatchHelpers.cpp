// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/CommandBatchHelpers.hpp"
#include "BatchRegistry.hpp"
#include "ecs/EntityBatchPolicy.hpp"
#include "ecs/EntityManager.hpp"
#include "engineapi/SelectionManager.hpp"
#include "LogMacros.h"

namespace eeng::editor
{
    namespace
    {
        /// @brief  Helper to get a pointer to the batch registry from the engine context, with logging.
        /// @param ctx The engine context.
        /// @param out_br The output batch registry pointer.
        /// @param context_label A label for logging context.
        /// @return True if the batch registry is available, false otherwise.
        bool try_get_batch_registry(EngineContext& ctx, BatchRegistry*& out_br, const char* context_label)
        {
            out_br = ctx.batch_registry
                ? static_cast<BatchRegistry*>(ctx.batch_registry.get())
                : nullptr;
            if (!out_br)
            {
                EENG_LOG(&ctx, "%s aborted: missing batch registry.", context_label);
                return false;
            }
            return true;
        }
    }

    /// @brief  Resolve the batch and entity reference for an entity that is already loaded in a batch, if any.
    /// @param entity The entity to resolve.
    /// @param ctx The engine context.
    /// @param out_batch The resolved batch ID.
    /// @param out_entity_ref The resolved entity reference.
    /// @param context_label A label for logging context.
    /// @return True if the entity is loaded in a batch, false otherwise.
    bool resolve_loaded_batch_for_entity(
        ecs::Entity entity,
        EngineContext& ctx,
        BatchId& out_batch,
        ecs::EntityRef& out_entity_ref,
        const char* context_label)
    {
        auto* br = ctx.batch_registry
            ? static_cast<BatchRegistry*>(ctx.batch_registry.get())
            : nullptr;
        auto* em = ctx.entity_manager
            ? static_cast<EntityManager*>(ctx.entity_manager.get())
            : nullptr;
        ecs::BatchPolicyContext policy_ctx{ br, em, &ctx };
        const auto decision =
            ecs::EntityBatchPolicy::resolve_existing_entity_batch(entity, policy_ctx, context_label);
        if (!decision.ok)
            return false;

        out_batch = decision.batch;
        out_entity_ref = decision.entity_ref;
        return true;
    }

    bool resolve_batch_for_new_entity(
        const ecs::Entity& parent_entity,
        EngineContext& ctx,
        BatchId& out_batch,
        ecs::EntityRef& out_parent_ref,
        const char* context_label)
    {
        auto* br = ctx.batch_registry
            ? static_cast<BatchRegistry*>(ctx.batch_registry.get())
            : nullptr;
        auto* em = ctx.entity_manager
            ? static_cast<EntityManager*>(ctx.entity_manager.get())
            : nullptr;
        ecs::BatchPolicyContext policy_ctx{ br, em, &ctx };

        BatchId preferred_batch{};
        const BatchId* preferred_ptr = nullptr;
        if (ctx.batch_selection && !ctx.batch_selection->empty())
        {
            preferred_batch = ctx.batch_selection->last();
            preferred_ptr = &preferred_batch;
        }

        BatchId default_batch{};
        const BatchId* default_ptr = nullptr;
        if (br && br->try_get_batch_id_by_name(eeng::BatchRegistry::kDefaultBatchName, default_batch))
            default_ptr = &default_batch;

        const auto decision = ecs::EntityBatchPolicy::resolve_new_entity_batch(
            parent_entity,
            preferred_ptr,
            default_ptr,
            policy_ctx,
            context_label);
        if (!decision.ok)
            return false;
        if (br && br->is_batch_read_only(decision.batch))
        {
            EENG_LOG(&ctx, "%s blocked: target batch is read-only.", context_label);
            return false;
        }

        out_batch = decision.batch;
        out_parent_ref = decision.parent_ref;
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
        if (br.is_batch_read_only(batch))
        {
            EENG_LOG(&ctx, "%s blocked: target entity belongs to a read-only batch.", context_label);
            return false;
        }

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
        if (br.is_batch_read_only(batch))
        {
            EENG_LOG(&ctx, "%s blocked: target batch is read-only.", context_label);
            return false;
        }

        out_future = br.queue_attach_entity(batch, entity_ref, ctx);
        return true;
    }

    bool is_entity_in_read_only_batch(
        ecs::Entity entity,
        EngineContext& ctx,
        const char* context_label)
    {
        BatchId batch{};
        ecs::EntityRef entity_ref{};
        if (!resolve_loaded_batch_for_entity(entity, ctx, batch, entity_ref, context_label))
            return false;

        BatchRegistry* br = nullptr;
        if (!try_get_batch_registry(ctx, br, context_label))
            return false;
        return br->is_batch_read_only(batch);
    }

    bool is_batch_read_only(
        const BatchId& batch,
        EngineContext& ctx,
        const char* context_label)
    {
        BatchRegistry* br = nullptr;
        if (!try_get_batch_registry(ctx, br, context_label))
            return false;

        if (!batch.valid())
        {
            EENG_LOG(&ctx, "%s aborted: invalid batch id.", context_label);
            return false;
        }
        return br->is_batch_read_only(batch);
    }

    void sync_branch_batch_with_parent(
        ecs::Entity root_entity,
        ecs::Entity parent_entity,
        EngineContext& ctx)
    {
        auto* br = ctx.batch_registry
            ? static_cast<BatchRegistry*>(ctx.batch_registry.get())
            : nullptr;
        auto* em = ctx.entity_manager
            ? static_cast<EntityManager*>(ctx.entity_manager.get())
            : nullptr;
        ecs::BatchPolicyContext policy_ctx{ br, em, &ctx };

        BatchId default_batch{};
        const BatchId* default_ptr = nullptr;
        if (br && br->try_get_batch_id_by_name(BatchRegistry::kDefaultBatchName, default_batch))
            default_ptr = &default_batch;

        ecs::EntityBatchPolicy::sync_branch_to_parent_batch(
            root_entity,
            parent_entity,
            default_ptr,
            policy_ctx);
    }
}
