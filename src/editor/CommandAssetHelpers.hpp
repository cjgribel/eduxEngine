// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <future>
#include <string_view>
#include <vector>
#include "Guid.h"
#include "engineapi/IResourceManager.hpp"

namespace eeng
{
    struct EngineContext;
    class ResourceManager;
}

namespace eeng::editor
{
    // Build a TaskResult error with a single failure entry.
    TaskResult make_task_error(
        TaskResult::TaskType type,
        std::string_view message,
        const Guid& guid = {});

    // Queue unimport/restore tasks on the resource manager strand.
    std::shared_future<TaskResult> queue_unimport_task(
        ResourceManager& rm,
        EngineContext& ctx,
        std::vector<Guid> roots);

    std::shared_future<TaskResult> queue_restore_task(
        ResourceManager& rm,
        EngineContext& ctx,
        std::vector<Guid> roots);
}
