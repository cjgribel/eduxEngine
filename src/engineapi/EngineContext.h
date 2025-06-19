// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <memory>
#include "IEntityRegistry.hpp"
#include "IResourceRegistry.hpp"

namespace eeng
{

struct EngineContext
{
    EngineContext(
        IEntityRegistry* entity_registry = nullptr,
        IResourceRegistry* resource_registry = nullptr)
        : entity_registry(entity_registry)
        , resource_registry(resource_registry)
    {}

    IEntityRegistry* entity_registry;
    IResourceRegistry* resource_registry;
};

} // namespace eeng
