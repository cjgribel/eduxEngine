// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EngineContext.hpp"
#include "ThreadPool.hpp"

namespace eeng
{
    EngineContext::EngineContext(
        std::unique_ptr<IEntityManager> entity_manager,
        std::unique_ptr<IResourceManager> resource_manager,
            std::unique_ptr<IGuiManager> gui_manager)
        : entity_manager(std::move(entity_manager))
        , resource_manager(std::move(resource_manager))
        , gui_manager(std::move(gui_manager))
        , thread_pool(std::make_unique<ThreadPool>())
    {
    }

    EngineContext::~EngineContext() = default;

} // namespace eeng
