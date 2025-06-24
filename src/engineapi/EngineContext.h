// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <memory>
#include "IEntityManager.hpp"
#include "IResourceManager.hpp"

namespace eeng
{

struct EngineContext
{
    EngineContext(
        std::unique_ptr<IEntityManager> entity_manager,
        std::unique_ptr<IResourceManager> resource_manager)
        : entity_manager(std::move(entity_manager))
        , resource_manager(std::move(resource_manager))
    {}

    std::unique_ptr<IEntityManager> entity_manager;
    std::unique_ptr<IResourceManager> resource_manager;
};

} // namespace eeng
