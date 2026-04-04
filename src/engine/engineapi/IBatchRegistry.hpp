// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <future>
#include <string>

#include "Guid.h"
#include "ecs/Entity.hpp"

namespace eeng
{
    struct EngineContext;
    struct TaskResult;
    using BatchId = Guid;

    class IBatchRegistry
    {
    public:
        /// @brief Queue loading of a batch and its asset closure.
        ///
        /// This is the runtime-facing residency operation used by gameplay,
        /// bootstrapping, and future streaming systems. Tooling-specific batch
        /// authoring remains on the concrete BatchRegistry type.
        virtual std::shared_future<TaskResult> queue_load(
            const BatchId& id,
            EngineContext& ctx) = 0;

        /// @brief Queue unloading of a batch and its leased assets.
        virtual std::shared_future<TaskResult> queue_unload(
            const BatchId& id,
            EngineContext& ctx) = 0;

        /// @brief Asynchronous unload used by shutdown policy to drain batches.
        virtual std::shared_future<TaskResult> queue_unload_all_async(EngineContext& ctx) = 0;

        /// @brief Return true when the batch is currently resident.
        virtual bool is_batch_loaded(const BatchId& id) const = 0;

        /// @brief Return true when the batch exists but should not be mutated
        /// through normal authored-content workflows.
        virtual bool is_batch_read_only(const BatchId& id) const = 0;

        /// @brief Resolve the currently loaded batch that owns a live entity.
        virtual bool try_get_loaded_batch_for_entity(
            const ecs::EntityRef& entity_ref,
            BatchId& out_id) const = 0;

        /// @brief Resolve a batch id by its stored name.
        virtual bool try_get_batch_id_by_name(
            const std::string& name,
            BatchId& out_id) const = 0;

        virtual ~IBatchRegistry() = default;
    };
} // namespace eeng
