// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <string_view>
#include "ecs/Entity.hpp"
#include "engineapi/IResourceManager.hpp"

namespace eeng
{
    struct EngineContext;
    class BatchRegistry;
    class EntityManager;
}

namespace eeng::ecs
{
    // Phase 2 sketch: policy layer that enforces batch/branch invariants.
    // This module should live between editor commands and core managers.

    struct BatchPolicyContext
    {
        BatchRegistry* batch_registry{};
        EntityManager* entity_manager{};
        EngineContext* engine_context{}; // Required for queue ops; optional for logging-only calls.
    };

    struct BatchPolicyDecision
    {
        bool ok{ false };
        BatchId batch{};
        ecs::EntityRef parent_ref{};
    };

    class EntityBatchPolicy
    {
    public:
        // Resolve a target batch for a new entity.
        // preferred_batch/default_batch are optional fallbacks supplied by the caller.
        static BatchPolicyDecision resolve_new_entity_batch(
            const ecs::Entity& parent_entity,
            const BatchId* preferred_batch,
            const BatchId* default_batch,
            const BatchPolicyContext& ctx,
            std::string_view context_label);

        // Ensure branch membership matches a parent batch; root uses default_batch when provided.
        static void sync_branch_to_parent_batch(
            const ecs::Entity& root_entity,
            const ecs::Entity& parent_entity,
            const BatchId* default_batch,
            const BatchPolicyContext& ctx);
    };
}
