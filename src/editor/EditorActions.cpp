// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/EditorActions.hpp"
#include "editor/CommandQueue.hpp"
#include "editor/GuiCommands.hpp"
#include "editor/BatchCommands.hpp"
#include "ResourceManager.hpp"
#include "ecs/EntityManager.hpp"
#include "LogMacros.h"
#include <memory>
#include <unordered_set>

namespace eeng::editor
{
    namespace
    {
        bool can_queue(EngineContext& ctx)
        {
            return ctx.command_queue != nullptr;
        }

        bool can_queue_action(EngineContext& ctx, const char* action_label)
        {
            if (!can_queue(ctx))
                return false;
            if (!ctx.command_queue->has_in_flight())
                return true;

            // Keep command order deterministic by ignoring new work while busy.
            EENG_LOG_WARN(&ctx, "%s ignored: command queue busy.", action_label);
            return false;
        }

        bool try_add_command(EngineContext& ctx, CommandPtr&& command, const char* action_label)
        {
            if (!ctx.command_queue->add(std::move(command)))
            {
                // Queue enforces the same policy as can_queue_action (defensive).
                EENG_LOG_WARN(&ctx, "%s ignored: command queue busy.", action_label);
                return false;
            }
            return true;
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
        if (!can_queue_action(ctx, "CreateEntity"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<CreateEntityCommand>(
                parent_entity,
                ctx_wptr),
            "CreateEntity");
    }

    void SceneActions::delete_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        if (selection.empty())
            return;
        if (!can_queue_action(ctx, "DeleteEntities"))
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
            try_add_command(
                ctx,
                CommandFactory::Create<DestroyEntityBranchCommand>(
                    entity,
                    ctx_wptr),
                "DeleteEntities");
        }
    }

    void SceneActions::copy_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        if (selection.empty())
            return;
        if (!can_queue_action(ctx, "CopyEntities"))
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
            try_add_command(
                ctx,
                CommandFactory::Create<CopyEntityBranchCommand>(
                    entity,
                    ctx_wptr),
                "CopyEntities");
        }
    }

    void SceneActions::parent_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        if (selection.size() < 2)
            return;
        if (!can_queue_action(ctx, "ParentEntities"))
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

            try_add_command(
                ctx,
                CommandFactory::Create<ReparentEntityBranchCommand>(
                    entity,
                    new_parent,
                    ctx_wptr),
                "ParentEntities");
        }
    }

    void SceneActions::unparent_entities(EngineContext& ctx, const std::deque<ecs::Entity>& selection)
    {
        if (selection.empty())
            return;
        if (!can_queue_action(ctx, "UnparentEntities"))
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

            try_add_command(
                ctx,
                CommandFactory::Create<ReparentEntityBranchCommand>(
                    entity,
                    ecs::Entity{},
                    ctx_wptr),
                "UnparentEntities");
        }
    }

    void SceneActions::add_components(EngineContext& ctx, const std::deque<ecs::Entity>& selection, entt::id_type comp_id)
    {
        if (selection.empty() || comp_id == entt::id_type{})
            return;
        if (!can_queue_action(ctx, "AddComponents"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();

        for (const auto& entity : selection)
        {
            if (!entity.has_id())
                continue;
            if (!em.entity_valid(entity))
                continue;
            if (!scenegraph.contains(entity))
                continue;

            try_add_command(
                ctx,
                CommandFactory::Create<AddComponentToEntityCommand>(
                    entity,
                    comp_id,
                    ctx_wptr),
                "AddComponents");
        }
    }

    void SceneActions::remove_components(EngineContext& ctx, const std::deque<ecs::Entity>& selection, entt::id_type comp_id)
    {
        if (selection.empty() || comp_id == entt::id_type{})
            return;
        if (!can_queue_action(ctx, "RemoveComponents"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();

        for (const auto& entity : selection)
        {
            if (!entity.has_id())
                continue;
            if (!em.entity_valid(entity))
                continue;
            if (!scenegraph.contains(entity))
                continue;

            try_add_command(
                ctx,
                CommandFactory::Create<RemoveComponentFromEntityCommand>(
                    entity,
                    comp_id,
                    ctx_wptr),
                "RemoveComponents");
        }
    }

    void AssetActions::import_model(
        EngineContext& ctx,
        const std::filesystem::path& source_file,
        assets::ImportFlags flags,
        std::string model_name,
        std::shared_ptr<std::atomic<bool>> in_flight)
    {
        if (!can_queue_action(ctx, "ImportModel"))
            return;

        if (in_flight)
            in_flight->store(true, std::memory_order_relaxed);

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
        {
            if (in_flight)
                in_flight->store(false, std::memory_order_relaxed);
            return;
        }

        if (!try_add_command(
                ctx,
                CommandFactory::Create<ImportModelCommand>(
                    source_file,
                    flags,
                    std::move(model_name),
                    ctx_wptr,
                    std::move(in_flight)),
                "ImportModel"))
        {
            if (in_flight)
                in_flight->store(false, std::memory_order_relaxed);
        }
    }

    void AssetActions::unimport_assets(EngineContext& ctx, std::vector<Guid> roots)
    {
        if (roots.empty())
        {
            EENG_LOG_WARN(&ctx, "Unimport skipped: no assets selected.");
            return;
        }
        if (!can_queue_action(ctx, "UnimportAssets"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<UnimportAssetsCommand>(
                std::move(roots),
                ctx_wptr),
            "UnimportAssets");
    }

    void AssetActions::restore_assets(EngineContext& ctx, std::vector<Guid> roots)
    {
        if (roots.empty())
        {
            EENG_LOG_WARN(&ctx, "Restore skipped: no assets selected.");
            return;
        }

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
        rm.queue_import_job(
            [roots = std::move(roots)](ResourceManager& rm, EngineContext& ctx) mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Restore;

                bool restored_any = false;
                for (const Guid& root : roots)
                {
                    std::string error;
                    if (!rm.restore_from_trash(root, ctx, &error))
                    {
                        if (error.empty())
                            error = "Restore failed.";
                        res.add_result(root, false, error);
                        continue;
                    }

                    restored_any = true;
                    res.add_result(root, true, "Restore ok");
                }

                if (restored_any)
                {
                    const auto& assets_root = rm.assets_root();
                    if (!assets_root.empty())
                        rm.scan_assets_async(assets_root, ctx);
                }

                return res;
            },
            ctx);
    }

    void BatchActions::load_batch(EngineContext& ctx, const BatchId& id)
    {
        if (!id.valid())
            return;
        if (!can_queue_action(ctx, "LoadBatch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<BatchLoadCommand>(
                id,
                ctx_wptr),
            "LoadBatch");
    }

    void BatchActions::unload_batch(EngineContext& ctx, const BatchId& id)
    {
        if (!id.valid())
            return;
        if (!can_queue_action(ctx, "UnloadBatch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<BatchUnloadCommand>(
                id,
                ctx_wptr),
            "UnloadBatch");
    }

    void BatchActions::load_all(EngineContext& ctx)
    {
        if (!can_queue_action(ctx, "LoadAllBatches"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<BatchLoadAllCommand>(
                ctx_wptr),
            "LoadAllBatches");
    }

    void BatchActions::unload_all(EngineContext& ctx)
    {
        if (!can_queue_action(ctx, "UnloadAllBatches"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<BatchUnloadAllCommand>(
                ctx_wptr),
            "UnloadAllBatches");
    }

    void BatchActions::create_batch(EngineContext& ctx, std::string name)
    {
        if (!can_queue_action(ctx, "CreateBatch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<CreateBatchCommand>(
                std::move(name),
                ctx_wptr),
            "CreateBatch");
    }

    void BatchActions::delete_batch(EngineContext& ctx, const BatchId& id)
    {
        if (!id.valid())
            return;
        if (!can_queue_action(ctx, "DeleteBatch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        try_add_command(
            ctx,
            CommandFactory::Create<DeleteBatchCommand>(
                id,
                ctx_wptr),
            "DeleteBatch");
    }

    void BatchActions::assign_entities_to_batch(
        EngineContext& ctx,
        const BatchId& id,
        const std::deque<ecs::Entity>& selection)
    {
        if (!id.valid() || selection.empty())
            return;
        if (!can_queue_action(ctx, "AssignEntitiesToBatch"))
            return;

        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        // Policy: keep batch membership consistent across an entity branch.
        // If a parent moves, all descendants should follow.
        if (!ctx.entity_manager)
        {
            EENG_LOG(&ctx, "AssignEntitiesToBatch aborted: missing entity manager.");
            return;
        }

        std::vector<ecs::Entity> selection_snapshot;
        selection_snapshot.reserve(selection.size());

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& scenegraph = em.scene_graph();
        std::unordered_set<ecs::Entity> seen;

        for (const auto& entity : selection)
        {
            if (!entity.has_id() || !em.entity_valid(entity)) continue;
            if (!scenegraph.contains(entity)) continue;

            const auto branch = scenegraph.get_branch_topdown(entity);
            for (const auto& branch_entity : branch)
            {
                if (!branch_entity.has_id() || !em.entity_valid(branch_entity)) continue;
                
                if (seen.insert(branch_entity).second)
                    selection_snapshot.push_back(branch_entity);
            }
        }

        try_add_command(
            ctx,
            CommandFactory::Create<AssignEntitiesToBatchCommand>(
                id,
                std::move(selection_snapshot),
                ctx_wptr),
            "AssignEntitiesToBatch");
    }
}
