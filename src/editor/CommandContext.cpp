// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/CommandContext.hpp"
#include "BatchRegistry.hpp"
#include "ResourceManager.hpp"
#include "ecs/EntityManager.hpp"
#include "ThreadPool.hpp"

namespace eeng::editor
{
    CommandContext::CommandContext(EngineContextWeakPtr ctx)
        : ctx_(std::move(ctx))
    {
    }

    EngineContextPtr CommandContext::lock() const
    {
        return ctx_.lock();
    }

    EntityManager* CommandContext::entity_manager(EngineContext& ctx) const
    {
        return ctx.entity_manager
            ? static_cast<EntityManager*>(ctx.entity_manager.get())
            : nullptr;
    }

    BatchRegistry* CommandContext::batch_registry(EngineContext& ctx) const
    {
        return ctx.batch_registry
            ? static_cast<BatchRegistry*>(ctx.batch_registry.get())
            : nullptr;
    }

    ResourceManager* CommandContext::resource_manager(EngineContext& ctx) const
    {
        return ctx.resource_manager
            ? static_cast<ResourceManager*>(ctx.resource_manager.get())
            : nullptr;
    }

    ::ThreadPool* CommandContext::thread_pool(EngineContext& ctx) const
    {
        return ctx.thread_pool.get();
    }
}
