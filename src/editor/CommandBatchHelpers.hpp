#pragma once

#include <future>
#include "ecs/Entity.hpp"
#include "engineapi/IResourceManager.hpp"

namespace eeng
{
    struct EngineContext;
    class BatchRegistry;
    class EntityManager;
}

namespace eeng::editor
{
    // Resolve the loaded batch for a live entity; logs on failure.
    bool resolve_loaded_batch_for_entity(
        ecs::Entity entity,
        EngineContext& ctx,
        BatchId& out_batch,
        ecs::EntityRef& out_entity_ref,
        const char* context_label);

    // Determine the batch for a new entity using parent or selected/default batch.
    bool resolve_batch_for_new_entity(
        const ecs::Entity& parent_entity,
        EngineContext& ctx,
        BatchId& out_batch,
        ecs::EntityRef& out_parent_ref,
        const char* context_label);

    // Policy: destruction is batch-owned; missing batch is an error.
    bool queue_destroy_entity_in_batch(
        ecs::Entity entity,
        EngineContext& ctx,
        std::shared_future<bool>& out_future,
        const char* context_label);

    // Queue an attach with safety checks against missing batch membership.
    bool queue_attach_entity_to_batch(
        ecs::Entity entity,
        const BatchId& batch,
        EngineContext& ctx,
        std::shared_future<bool>& out_future,
        const char* context_label);

    // True when the entity belongs to a loaded batch that should not be
    // mutated through normal editor commands.
    bool is_entity_in_read_only_batch(
        ecs::Entity entity,
        EngineContext& ctx,
        const char* context_label);

    // True when the batch exists and is marked read-only in the batch index.
    bool is_batch_read_only(
        const BatchId& batch,
        EngineContext& ctx,
        const char* context_label);

    // Enforce "branch follows parent" on reparent operations.
    void sync_branch_batch_with_parent(
        ecs::Entity root_entity,
        ecs::Entity parent_entity,
        EngineContext& ctx);
}
