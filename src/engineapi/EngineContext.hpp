// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <memory>
#include "IEntityManager.hpp"
#include "IResourceManager.hpp"
class ThreadPool;

namespace eeng
{
    /*
    Engine context facilities:
    - Entity manager
    - Resource manager
    - Thread pool
    - (TODO) Event dispatcher
    - (TODO?) Scene manager
    - (TODO?) Logger
    */

    struct EngineContext
    {
        EngineContext(
            std::unique_ptr<IEntityManager> entity_manager,
            std::unique_ptr<IResourceManager> resource_manager);

        ~EngineContext();

        std::unique_ptr<IEntityManager> entity_manager;
        std::unique_ptr<IResourceManager> resource_manager;
        std::unique_ptr<ThreadPool> thread_pool;
    };

    using EngineContextPtr = std::shared_ptr<EngineContext>;
    using EngineContextWeakPtr = std::weak_ptr<EngineContext>;

} // namespace eeng
