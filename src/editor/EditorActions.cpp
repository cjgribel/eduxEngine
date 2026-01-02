// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/EditorActions.hpp"
#include "editor/CommandQueue.hpp"
#include "editor/GuiCommands.hpp"
#include "editor/BatchCommands.hpp"
#include "ecs/EntityManager.hpp"

namespace eeng::editor
{
    namespace
    {
        bool can_queue(EngineContext& ctx)
        {
            return ctx.command_queue != nullptr;
        }

        std::vector<ecs::Entity> filter_out_descendants(
            eeng::ecs::SceneGraph& scenegraph,
            const std::deque<ecs::Entity>& entities)
        {
            std::vector<ecs::Entity> filtered_entities;
            filtered_entities.reserve(entities.size());

            for (const auto& entity : entities)
            {
                bool is_child = false;
                for (const auto& entity_other : entities)
                {
                    if (entity == entity_other)
                        continue;
                    if (scenegraph.is_descendant_of(entity, entity_other))
                    {
                        is_child = true;
                        break;
                    }
                }
                if (!is_child)
                    filtered_entities.push_back(entity);
            }

            return filtered_entities;
        }
    }

    void SceneActions::create_entity(EngineContext& ctx, const ecs::Entity& parent_entity)
    {
        if (!can_queue(ctx))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        ctx.command_queue->add(
            CommandFactory::Create<CreateEntityCommand>(
                parent_entity,
                ctx_wptr));
    }

    void SceneActions::delete_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        if (!can_queue(ctx) || selection.empty())
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto roots = filter_out_descendants(
            em.scene_graph(),
            selection);

        for (const auto& entity : roots)
        {
            ctx.command_queue->add(
                CommandFactory::Create<DestroyEntityBranchCommand>(
                    entity,
                    ctx_wptr));
        }
    }

    void SceneActions::copy_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        if (!can_queue(ctx) || selection.empty())
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto roots = filter_out_descendants(
            em.scene_graph(),
            selection);

        for (const auto& entity : roots)
        {
            ctx.command_queue->add(
                CommandFactory::Create<CopyEntityBranchCommand>(
                    entity,
                    ctx_wptr));
        }
    }

    void SceneActions::parent_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        if (!can_queue(ctx) || selection.size() < 2)
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();
        const auto new_parent = selection.back();

        for (const auto& entity : selection)
        {
            if (entity == new_parent)
                continue;
            if (scenegraph.is_descendant_of(new_parent, entity))
                continue;

            ctx.command_queue->add(
                CommandFactory::Create<ReparentEntityBranchCommand>(
                    entity,
                    new_parent,
                    ctx_wptr));
        }
    }

    void SceneActions::unparent_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        if (!can_queue(ctx) || selection.empty())
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();

        for (const auto& entity : selection)
        {
            if (scenegraph.is_root(entity))
                continue;

            ctx.command_queue->add(
                CommandFactory::Create<ReparentEntityBranchCommand>(
                    entity,
                    ecs::Entity{},
                    ctx_wptr));
        }
    }

    void BatchActions::load_batch(EngineContext& ctx, const BatchId& id)
    {
        if (!can_queue(ctx) || !id.valid())
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        ctx.command_queue->add(
            CommandFactory::Create<BatchLoadCommand>(
                id,
                ctx_wptr));
    }

    void BatchActions::unload_batch(EngineContext& ctx, const BatchId& id)
    {
        if (!can_queue(ctx) || !id.valid())
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        ctx.command_queue->add(
            CommandFactory::Create<BatchUnloadCommand>(
                id,
                ctx_wptr));
    }

    void BatchActions::load_all(EngineContext& ctx)
    {
        if (!can_queue(ctx))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        ctx.command_queue->add(
            CommandFactory::Create<BatchLoadAllCommand>(
                ctx_wptr));
    }

    void BatchActions::unload_all(EngineContext& ctx)
    {
        if (!can_queue(ctx))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        ctx.command_queue->add(
            CommandFactory::Create<BatchUnloadAllCommand>(
                ctx_wptr));
    }

    void BatchActions::create_batch(EngineContext& ctx, std::string name)
    {
        if (!can_queue(ctx))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        ctx.command_queue->add(
            CommandFactory::Create<CreateBatchCommand>(
                std::move(name),
                ctx_wptr));
    }

    void BatchActions::delete_batch(EngineContext& ctx, const BatchId& id)
    {
        if (!can_queue(ctx) || !id.valid())
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        ctx.command_queue->add(
            CommandFactory::Create<DeleteBatchCommand>(
                id,
                ctx_wptr));
    }
}
