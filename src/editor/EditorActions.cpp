// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/EditorActions.hpp"
#include "editor/CommandQueue.hpp"
#include "editor/GuiCommands.hpp"
#include "editor/BatchCommands.hpp"
#include "assets/importers/AssimpImporter.hpp"
#include "ResourceManager.hpp"
#include "ThreadPool.hpp"
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

    void SceneActions::add_components(EngineContext& ctx, const std::deque<ecs::Entity>& selection, entt::id_type comp_id)
    {
        if (!can_queue(ctx) || selection.empty() || comp_id == entt::id_type{})
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

            ctx.command_queue->add(
                CommandFactory::Create<AddComponentToEntityCommand>(
                    entity,
                    comp_id,
                    ctx_wptr));
        }
    }

    void SceneActions::remove_components(EngineContext& ctx, const std::deque<ecs::Entity>& selection, entt::id_type comp_id)
    {
        if (!can_queue(ctx) || selection.empty() || comp_id == entt::id_type{})
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

            ctx.command_queue->add(
                CommandFactory::Create<RemoveComponentFromEntityCommand>(
                    entity,
                    comp_id,
                    ctx_wptr));
        }
    }

    void AssetActions::import_model(
        EngineContext& ctx,
        const std::filesystem::path& source_file,
        assets::ImportFlags flags,
        std::string model_name,
        std::shared_ptr<std::atomic<bool>> in_flight)
    {
        auto ctx_wptr = ctx.weak_from_this();
        if (ctx_wptr.expired())
            return;

        if (in_flight)
            in_flight->store(true, std::memory_order_relaxed);

        if (!ctx.thread_pool)
        {
            EENG_LOG_WARN(&ctx, "Asset import skipped: ThreadPool unavailable.");
            if (in_flight)
                in_flight->store(false, std::memory_order_relaxed);
            return;
        }

        auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
        const auto assets_root = rm.assets_root();
        if (assets_root.empty())
        {
            EENG_LOG_WARN(&ctx, "Asset import skipped: assets root not set.");
            if (in_flight)
                in_flight->store(false, std::memory_order_relaxed);
            return;
        }

        assets::AssimpImportOptions opts{};
        opts.assets_root = assets_root;
        opts.source_file = source_file;
        opts.model_name = model_name.empty() ? source_file.stem().string() : std::move(model_name);
        opts.flags = flags;

        // Policy: heavy parse on worker, file writes + scan on RM strand.
        ctx.thread_pool->queue_task([opts = std::move(opts), ctx_wptr, in_flight]() mutable
            {
                auto ctx_sp = ctx_wptr.lock();
                if (!ctx_sp)
                {
                    if (in_flight)
                        in_flight->store(false, std::memory_order_relaxed);
                    return;
                }

                auto& rm = static_cast<ResourceManager&>(*ctx_sp->resource_manager);
                assets::AssimpImporter importer;
                auto plan = importer.prepare_import_plan(opts, *ctx_sp);
                if (!plan.result.success)
                {
                    const auto error = plan.result.error_message.empty()
                        ? std::string("Import failed.")
                        : plan.result.error_message;
                    rm.queue_import_job(
                        [error](ResourceManager&, EngineContext&) mutable -> TaskResult
                        {
                            TaskResult res;
                            res.type = TaskResult::TaskType::Import;
                            res.add_result(Guid{}, false, error);
                            return res;
                        },
                        *ctx_sp);
                    if (in_flight)
                        in_flight->store(false, std::memory_order_relaxed);
                    return;
                }

                auto plan_ptr = std::make_shared<assets::AssimpImportPlan>(std::move(plan));
                rm.queue_import_job(
                    [plan_ptr](ResourceManager& rm, EngineContext& ctx) mutable -> TaskResult
                    {
                        TaskResult res;
                        res.type = TaskResult::TaskType::Import;

                        const auto result = assets::AssimpImporter::apply_import_plan(*plan_ptr, ctx);
                        if (!result.success)
                        {
                            res.add_result(Guid{}, false, result.error_message);
                            return res;
                        }

                        res.add_result(result.model_guid, true, "Import ok");
                        if (!plan_ptr->assets_root.empty())
                            rm.scan_assets_async(plan_ptr->assets_root, ctx);
                        return res;
                    },
                    *ctx_sp);

                if (in_flight)
                    in_flight->store(false, std::memory_order_relaxed);
            });
    }

    void AssetActions::unimport_assets(EngineContext& ctx, std::vector<Guid> roots)
    {
        if (roots.empty())
        {
            EENG_LOG_WARN(&ctx, "Unimport skipped: no assets selected.");
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
                res.type = TaskResult::TaskType::Unimport;

                std::string error;
                if (!rm.unimport_assets(roots, ctx, &error))
                {
                    if (error.empty())
                        error = "Unimport failed.";
                    res.add_result(Guid{}, false, error);
                    return res;
                }

                res.add_result(Guid{}, true, "Unimport ok");
                const auto& assets_root = rm.assets_root();
                if (!assets_root.empty())
                    rm.scan_assets_async(assets_root, ctx);
                return res;
            },
            ctx);
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

    void BatchActions::assign_entities_to_batch(
        EngineContext& ctx,
        const BatchId& id,
        const std::deque<ecs::Entity>& selection)
    {
        if (!can_queue(ctx) || !id.valid() || selection.empty())
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

        ctx.command_queue->add(
            CommandFactory::Create<AssignEntitiesToBatchCommand>(
                id,
                std::move(selection_snapshot),
                ctx_wptr));
    }
}
