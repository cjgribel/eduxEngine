// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
// #include "ecs/Entity.hpp"
// #include <entt/fwd.hpp>
// #include <cstddef>
#include <future>

namespace eeng
{
    struct EngineContext;
    struct TaskResult;

    class IBatchRegistry
    {
    public:
        // Asynchronous unload used by shutdown policy to drain batches.
        virtual std::shared_future<TaskResult> queue_unload_all_async(EngineContext& ctx) = 0;

        virtual ~IBatchRegistry() = default;
    };
} // namespace eeng
