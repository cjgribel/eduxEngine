// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"

class ThreadPool;

namespace eeng
{
    class BatchRegistry;
    class EntityManager;
    class ResourceManager;
}

namespace eeng::editor
{
    // Lightweight wrapper for resolving managers from a weak EngineContext.
    class CommandContext
    {
    public:
        explicit CommandContext(EngineContextWeakPtr ctx);

        // Locks the weak context for use in a command frame.
        EngineContextPtr lock() const;

        EntityManager* entity_manager(EngineContext& ctx) const;
        BatchRegistry* batch_registry(EngineContext& ctx) const;
        ResourceManager* resource_manager(EngineContext& ctx) const;
        ::ThreadPool* thread_pool(EngineContext& ctx) const;

    private:
        EngineContextWeakPtr ctx_;
    };
}
