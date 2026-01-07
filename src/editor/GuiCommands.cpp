//
//  EditComponentCommand.cpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#include <iostream>
#include <cassert>
#include <future>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "MetaSerialize.hpp"
#include "GuiCommands.hpp"
#include "BatchRegistry.hpp"
#include "editor/CommandAsync.hpp"
#include "editor/CommandSnapshot.hpp"
#include "ecs/EntityManager.hpp"
#include "ResourceManager.hpp"
#include "ThreadPool.hpp"
#include "assets/importers/AssimpImporter.hpp"
#include "meta/EntityMetaHelpers.hpp"
#include "meta/MetaAux.h"
#include "MetaLiterals.h"
#include "LogMacros.h"
#include "engineapi/SelectionManager.hpp"

// Used by Copy command
#include "ecs/SceneGraph.hpp"

namespace
{
    using eeng::BatchId;
    using eeng::BatchRegistry;
    using eeng::Guid;
    using eeng::EngineContext;
    using eeng::EngineContextWeakPtr;
    using eeng::EntityManager;
    using eeng::ecs::Entity;
    using eeng::ecs::EntityRef;
    namespace ecs = eeng::ecs;
    using eeng::editor::CommandStatus;
    using eeng::TaskResult;
    using eeng::ResourceManager;

    TaskResult make_task_error(
        TaskResult::TaskType type,
        std::string_view message,
        const Guid& guid = {})
    {
        TaskResult res;
        res.type = type;
        res.add_result(guid, false, message);
        return res;
    }

    std::shared_future<TaskResult> queue_unimport_task(
        ResourceManager& rm,
        EngineContext& ctx,
        std::vector<Guid> roots)
    {
        return rm.queue_import_job(
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

                for (const Guid& root : roots)
                    res.add_result(root, true, "Unimport ok");

                const auto& assets_root = rm.assets_root();
                if (!assets_root.empty())
                    rm.scan_assets_async(assets_root, ctx);
                return res;
            },
            ctx);
    }

    std::shared_future<TaskResult> queue_restore_task(
        ResourceManager& rm,
        EngineContext& ctx,
        std::vector<Guid> roots)
    {
        return rm.queue_import_job(
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

    void ensure_storage(entt::registry& registry, entt::id_type component_id)
    {
        if (!registry.storage(component_id))
        {
            auto meta_type = entt::resolve(component_id);
            assert(meta_type);

            auto assure_fn = meta_type.func(eeng::literals::assure_component_storage_hs);
            assert(assure_fn);

            auto result = assure_fn.invoke(
                {},
                entt::forward_as_meta(registry)
            );
            if (!result)
            {
                throw std::runtime_error(
                    "Failed to invoke assure_storage for " + eeng::meta::get_meta_type_display_name(meta_type));
            }
        }
    }

    bool try_capture_guid(EntityManager& em, const Entity& entity, Guid& guid)
    {
        if (!entity.has_id())
            return false;
        if (!em.entity_valid(entity))
            return false;
        if (!em.scene_graph().contains(entity))
            return false;
        guid = em.get_entity_guid(entity);
        return guid.valid();
    }

    Entity resolve_entity_from_guid(EntityManager& em, const Guid& guid)
    {
        if (!guid.valid())
            return Entity{};
        auto entity_opt = em.get_entity_from_guid(guid);
        if (!entity_opt || !entity_opt->has_id())
            return Entity{};
        if (!em.entity_valid(*entity_opt))
            return Entity{};
        return *entity_opt;
    }

    void bind_refs_for_entity(Entity entity, EngineContext& ctx)
    {
        if (ctx.resource_manager)
            eeng::meta::bind_asset_refs_for_entity(entity, ctx);
        eeng::meta::bind_entity_refs_for_entity(entity, ctx);
    }

    void mark_batch_dirty_for_entity(Entity entity, EngineContext& ctx)
    {
        if (!ctx.batch_registry)
            return;
        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        br.mark_closure_dirty_for_entity(entity, ctx);
    }

    bool resolve_loaded_batch_for_entity(
        Entity entity,
        EngineContext& ctx,
        BatchId& out_batch,
        eeng::ecs::EntityRef& out_entity_ref,
        const char* context_label)
    {
        if (!ctx.batch_registry || !ctx.entity_manager)
        {
            EENG_LOG(&ctx, "%s aborted: missing batch or entity manager.", context_label);
            return false;
        }

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        if (!em.try_get_entity_ref(entity, out_entity_ref))
        {
            EENG_LOG(&ctx, "%s failed: entity not registered.", context_label);
            return false;
        }

        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        if (!br.try_get_loaded_batch_for_entity(out_entity_ref, out_batch))
        {
            EENG_LOG(&ctx, "%s failed: entity has no loaded batch.", context_label);
            return false;
        }

        return true;
    }

    // Policy: all live entities must belong to a loaded batch.
    // New entities use the parent's batch; otherwise use selected batch or fallback to "default".
    bool resolve_batch_for_new_entity(
        const Entity& parent_entity,
        EngineContext& ctx,
        BatchId& out_batch,
        eeng::ecs::EntityRef& out_parent_ref,
        const char* context_label)
    {
        if (!ctx.batch_registry || !ctx.entity_manager)
        {
            EENG_LOG(&ctx, "%s aborted: missing batch or entity manager.", context_label);
            return false;
        }

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);

        if (parent_entity.has_id())
        {
            if (!em.try_get_entity_ref(parent_entity, out_parent_ref))
            {
                EENG_LOG(&ctx, "%s failed: parent entity not registered.", context_label);
                return false;
            }

            if (!br.try_get_loaded_batch_for_entity(out_parent_ref, out_batch))
            {
                EENG_LOG(&ctx, "%s failed: parent has no loaded batch.", context_label);
                return false;
            }

            return true;
        }

        BatchId selected{};
        if (ctx.batch_selection && !ctx.batch_selection->empty())
        {
            selected = ctx.batch_selection->last();
        }
        else if (!br.try_get_batch_id_by_name(eeng::BatchRegistry::kDefaultBatchName, selected))
        {
            EENG_LOG(&ctx, "%s failed: default batch not found.", context_label);
            return false;
        }

        if (!br.is_batch_loaded(selected))
        {
            EENG_LOG(&ctx, "%s failed: target batch is not loaded.", context_label);
            return false;
        }

        out_batch = selected;
        return true;
    }

    // Policy: destruction is batch-owned; missing batch is an error.
    bool queue_destroy_entity_in_batch(
        Entity entity,
        EngineContext& ctx,
        std::shared_future<bool>& out_future,
        const char* context_label)
    {
        BatchId batch{};
        ecs::EntityRef entity_ref{};
        if (!resolve_loaded_batch_for_entity(entity, ctx, batch, entity_ref, context_label))
            return false;

        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        out_future = br.queue_destroy_entity(batch, entity_ref, ctx);
        return true;
    }

    bool queue_attach_entity_to_batch(
        Entity entity,
        const BatchId& batch,
        EngineContext& ctx,
        std::shared_future<bool>& out_future,
        const char* context_label)
    {
        if (!ctx.batch_registry || !ctx.entity_manager)
        {
            EENG_LOG(&ctx, "%s aborted: missing batch or entity manager.", context_label);
            return false;
        }

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        ecs::EntityRef entity_ref{};
        if (!em.try_get_entity_ref(entity, entity_ref))
        {
            EENG_LOG(&ctx, "%s failed: entity not registered.", context_label);
            return false;
        }

        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        if (!br.is_batch_loaded(batch))
        {
            EENG_LOG(&ctx, "%s failed: target batch is not loaded.", context_label);
            return false;
        }

        out_future = br.queue_attach_entity(batch, entity_ref, ctx);
        return true;
    }

    void sync_branch_batch_with_parent(Entity root_entity, Entity parent_entity, EngineContext& ctx)
    {
        if (!ctx.batch_registry || !ctx.entity_manager)
            return;

        auto& em = static_cast<EntityManager&>(*ctx.entity_manager);
        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        auto& scenegraph = em.scene_graph();

        if (!scenegraph.contains(root_entity))
            return;

        BatchId parent_batch{};
        bool parent_has_batch = false;

        if (parent_entity.has_id())
        {
            const auto parent_ref = em.get_entity_ref(parent_entity);
            if (parent_ref.is_bound() && parent_ref.guid.valid())
                parent_has_batch = br.try_get_loaded_batch_for_entity(parent_ref, parent_batch);
        }
        else
        {
            BatchId default_batch{};
            if (br.try_get_batch_id_by_name(BatchRegistry::kDefaultBatchName, default_batch)
                && br.is_batch_loaded(default_batch))
            {
                parent_batch = default_batch;
                parent_has_batch = true;
            }
            else
            {
                EENG_LOG(&ctx, "sync_branch_batch_with_parent: default batch missing or unloaded.");
            }
        }

        const auto branch = scenegraph.get_branch_topdown(root_entity);
        for (const auto& entity : branch)
        {
            if (!entity.has_id() || !em.entity_valid(entity))
                continue;

            const auto entity_ref = em.get_entity_ref(entity);
            if (!entity_ref.is_bound() || !entity_ref.guid.valid())
                continue;

            BatchId current_batch{};
            const bool has_current = br.try_get_loaded_batch_for_entity(entity_ref, current_batch);

            if (parent_has_batch)
            {
                if (!has_current || current_batch != parent_batch)
                {
                    if (has_current)
                        br.queue_detach_entity(current_batch, entity_ref, ctx);
                    br.queue_attach_entity(parent_batch, entity_ref, ctx);
                }
            }
            else if (has_current)
            {
                br.queue_detach_entity(current_batch, entity_ref, ctx);
            }
        }
    }
}

namespace eeng::editor {
    using ecs::Entity;

    CreateEntityCommand::CreateEntityCommand(
        const Entity& parent_entity,
        EngineContextWeakPtr ctx) :
        parent_entity(parent_entity),
        ctx(std::move(ctx)),
        display_name("Create Entity") {
    }

    CommandStatus CreateEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);

        if (async_stage != AsyncStage::None)
            return update();

        // First execution: create a fresh entity and capture a redo snapshot.
        if (entity_json.is_null())
        {
            Entity parent_current{};
            if (parent_entity.has_id()
                && em.entity_valid(parent_entity)
                && em.scene_graph().contains(parent_entity))
            {
                parent_current = parent_entity;
            }

            ecs::EntityRef parent_ref{};
            if (!resolve_batch_for_new_entity(
                    parent_current,
                    *ctx_sp,
                    created_batch,
                    parent_ref,
                    "CreateEntity"))
            {
                return CommandStatus::Failed;
            }

            auto& br = static_cast<eeng::BatchRegistry&>(*ctx_sp->batch_registry);
            create_future = br.queue_create_entity(created_batch, "", parent_ref, *ctx_sp);
            async_stage = AsyncStage::Create;
            return update();
        }
        // Redo path: recreate from the serialized snapshot.
        else
        {
            if (!created_batch.valid())
            {
                EENG_LOG(ctx_sp.get(), "CreateEntity redo failed: missing batch.");
                return CommandStatus::Failed;
            }

            auto er = meta::deserialize_entity_and_register_for_undo(
                entity_json,
                *ctx_sp);
            created_entity = er.entity;
            created_guid = er.guid;
            bind_refs_for_entity(created_entity, *ctx_sp);

            if (!queue_attach_entity_to_batch(
                    created_entity,
                    created_batch,
                    *ctx_sp,
                    attach_future,
                    "CreateEntity redo"))
            {
                return CommandStatus::Failed;
            }
            async_stage = AsyncStage::Attach;
            return update();
        }
    }

    CommandStatus CreateEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        // If we never created a live entity, only proceed if a GUID was recorded.
        if (!created_entity.has_id())
        {
            if (!created_guid.valid())
                return CommandStatus::Done;
        }

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        if (created_guid.valid())
        {
            auto entity_opt = em.get_entity_from_guid(created_guid);
            if (entity_opt && entity_opt->has_id())
            {
                mark_batch_dirty_for_entity(*entity_opt, *ctx_sp);
                if (!queue_destroy_entity_in_batch(
                        *entity_opt,
                        *ctx_sp,
                        destroy_future,
                        "CreateEntity undo"))
                {
                    return CommandStatus::Failed;
                }
                async_stage = AsyncStage::Destroy;
                return update();
            }
            return CommandStatus::Done;
        }

        if (created_entity.has_id())
        {
            mark_batch_dirty_for_entity(created_entity, *ctx_sp);
            if (!queue_destroy_entity_in_batch(
                    created_entity,
                    *ctx_sp,
                    destroy_future,
                    "CreateEntity undo"))
            {
                return CommandStatus::Failed;
            }
            async_stage = AsyncStage::Destroy;
            return update();
        }
        // destroy_func(created_entity);

        // std::cout << "CreateEntityCommand::undo() " << entt::to_integral(created_entity) << std::endl;
        return CommandStatus::Done;
    }

    CommandStatus CreateEntityCommand::update()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);

        if (async_stage == AsyncStage::Create)
        {
            bool in_flight = true;
            ecs::EntityRef created_ref{};
            auto status = poll_entity_future(create_future, in_flight, created_ref);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            created_entity = created_ref.entity;
            created_guid = created_ref.guid;

            auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
            if (!registry_sp)
                return CommandStatus::Failed;

            entity_json = meta::serialize_entity_for_undo(
                em.get_entity_ref(created_entity),
                registry_sp);
            if (!created_guid.valid())
                created_guid = guid_from_json(entity_json);

            mark_batch_dirty_for_entity(created_entity, *ctx_sp);
            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        if (async_stage == AsyncStage::Attach)
        {
            bool in_flight = true;
            auto status = poll_bool_future(attach_future, in_flight);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            mark_batch_dirty_for_entity(created_entity, *ctx_sp);
            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        if (async_stage == AsyncStage::Destroy)
        {
            bool in_flight = true;
            auto status = poll_bool_future(destroy_future, in_flight);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        return CommandStatus::Done;
    }

    std::string CreateEntityCommand::get_name() const
    {
        return display_name;
    }

    // ------------------------------------------------------------------------

    DestroyEntityCommand::DestroyEntityCommand(
        const Entity& entity,
        EngineContextWeakPtr ctx
        // const DestroyEntityFunc&& destroy_func
    ) :
        entity(entity),
        ctx(std::move(ctx))
        // destroy_func(destroy_func)
    {
        display_name = std::string("Destroy Entity ") + std::to_string(entity.to_integral());
    }

    CommandStatus DestroyEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        if (!entity_guid.valid())
        {
            if (!try_capture_guid(em, entity, entity_guid))
                return CommandStatus::Done;
        }

        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        if (entity_json.is_null())
        {
            entity_json = meta::serialize_entity_for_undo(
                em.get_entity_ref(entity_current),
                registry_sp);
        }

        ecs::EntityRef entity_ref{};
        if (!resolve_loaded_batch_for_entity(
                entity_current,
                *ctx_sp,
                entity_batch,
                entity_ref,
                "DestroyEntity"))
        {
            return CommandStatus::Failed;
        }

        auto& br = static_cast<eeng::BatchRegistry&>(*ctx_sp->batch_registry);
        destroy_future = br.queue_destroy_entity(entity_batch, entity_ref, *ctx_sp);
        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        async_stage = AsyncStage::Destroy;
        return update();
    }

    CommandStatus DestroyEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        if (entity_json.is_null())
            return CommandStatus::Done;

        if (!entity_batch.valid())
        {
            EENG_LOG(ctx_sp.get(), "DestroyEntity undo failed: missing batch.");
            return CommandStatus::Failed;
        }

        auto er = meta::deserialize_entity_and_register_for_undo(
            entity_json,
            *ctx_sp);
        entity = er.entity;
        entity_guid = er.guid;
        bind_refs_for_entity(er.entity, *ctx_sp);

        if (!queue_attach_entity_to_batch(
                er.entity,
                entity_batch,
                *ctx_sp,
                attach_future,
                "DestroyEntity undo"))
        {
            return CommandStatus::Failed;
        }

        async_stage = AsyncStage::Attach;
        return update();
    }

    CommandStatus DestroyEntityCommand::update()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage == AsyncStage::Destroy)
        {
            bool in_flight = true;
            auto status = poll_bool_future(destroy_future, in_flight);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        if (async_stage == AsyncStage::Attach)
        {
            bool in_flight = true;
            auto status = poll_bool_future(attach_future, in_flight);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            mark_batch_dirty_for_entity(entity, *ctx_sp);
            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        return CommandStatus::Done;
    }

    std::string DestroyEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- DestroyEntityBranchCommand ----------------------------------------

    DestroyEntityBranchCommand::DestroyEntityBranchCommand(
        const Entity& entity,
        EngineContextWeakPtr ctx
    ) :
        root_entity(entity),
        ctx(std::move(ctx))
    {
        display_name = std::string("Destroy Entity Branch ") + std::to_string(entity.to_integral());
    }

    CommandStatus DestroyEntityBranchCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);

        if (branch_json.is_null())
        {
            if (!root_guid.valid())
            {
                if (!try_capture_guid(em, root_entity, root_guid))
                    return CommandStatus::Done;
            }

            const auto root_current = resolve_entity_from_guid(em, root_guid);
            if (!root_current.has_id())
                return CommandStatus::Done;

            auto& scenegraph = em.scene_graph();
            if (!scenegraph.contains(root_current))
                return CommandStatus::Done;

            ecs::EntityRef root_ref{};
            if (!resolve_loaded_batch_for_entity(
                    root_current,
                    *ctx_sp,
                    branch_batch,
                    root_ref,
                    "DestroyEntityBranch"))
            {
                return CommandStatus::Failed;
            }
            auto branch = scenegraph.get_branch_topdown(root_current);

            branch_json = nlohmann::json::array();

            for (const auto& entity : branch)
            {
                branch_json.push_back(meta::serialize_entity_for_undo(
                    em.get_entity_ref(entity),
                    registry_sp));
            }
        }

        if (!branch_json.is_array())
            return CommandStatus::Done;

        destroy_futures.clear();
        for (auto it = branch_json.rbegin(); it != branch_json.rend(); ++it)
        {
            auto guid = guid_from_json(*it);
            if (!guid.valid())
                continue;

            auto entity_opt = em.get_entity_from_guid(guid);
            if (!entity_opt || !entity_opt->has_id())
                continue;

            mark_batch_dirty_for_entity(*entity_opt, *ctx_sp);
            std::shared_future<bool> future{};
            if (!queue_destroy_entity_in_batch(
                    *entity_opt,
                    *ctx_sp,
                    future,
                    "DestroyEntityBranch"))
            {
                return CommandStatus::Failed;
            }
            destroy_futures.push_back(std::move(future));
        }

        async_stage = AsyncStage::Destroy;
        return update();
    }

    CommandStatus DestroyEntityBranchCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        if (branch_json.is_null() || !branch_json.is_array())
            return CommandStatus::Done;

        if (!branch_batch.valid())
        {
            EENG_LOG(ctx_sp.get(), "DestroyEntityBranch undo failed: missing batch.");
            return CommandStatus::Failed;
        }

        std::vector<ecs::Entity> created_entities;
        created_entities.reserve(branch_json.size());

        for (const auto& entity_json : branch_json)
        {
            auto er = meta::deserialize_entity_for_undo(
                entity_json,
                *ctx_sp);
            created_entities.push_back(er.entity);
        }

        ctx_sp->entity_manager->register_entities_from_deserialization(created_entities);

        attach_futures.clear();
        attach_futures.reserve(created_entities.size());

        for (auto entity : created_entities)
        {
            bind_refs_for_entity(entity, *ctx_sp);
            std::shared_future<bool> future{};
            if (!queue_attach_entity_to_batch(
                    entity,
                    branch_batch,
                    *ctx_sp,
                    future,
                    "DestroyEntityBranch undo"))
            {
                return CommandStatus::Failed;
            }
            attach_futures.push_back(std::move(future));
        }

        async_stage = AsyncStage::Attach;
        return update();
    }

    CommandStatus DestroyEntityBranchCommand::update()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage == AsyncStage::Destroy)
        {
            bool in_flight = true;
            auto status = poll_bool_futures(destroy_futures, in_flight);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        if (async_stage == AsyncStage::Attach)
        {
            bool in_flight = true;
            auto status = poll_bool_futures(attach_futures, in_flight);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        return CommandStatus::Done;
    }

    std::string DestroyEntityBranchCommand::get_name() const
    {
        return display_name;
    }

    // --- CopyEntityCommand (REMOVE) --------------------------------------------------

    CopyEntityCommand::CopyEntityCommand(
        const Entity& entity,
        EngineContextWeakPtr ctx) :
        entity_source(entity),
        ctx(std::move(ctx))
    {
        display_name = std::string("Copy Entity ") + std::to_string(entity.to_integral());
    }

    CommandStatus CopyEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);

        if (copy_json.is_null())
        {
            if (!source_guid.valid())
            {
                if (!try_capture_guid(em, entity_source, source_guid))
                    return CommandStatus::Done;
            }

            const auto source_current = resolve_entity_from_guid(em, source_guid);
            if (!source_current.has_id())
                return CommandStatus::Done;

            BatchId source_batch{};
            ecs::EntityRef source_ref{};
            if (!resolve_loaded_batch_for_entity(
                    source_current,
                    *ctx_sp,
                    source_batch,
                    source_ref,
                    "CopyEntity"))
            {
                return CommandStatus::Failed;
            }
            target_batch = source_batch;

            copy_json = meta::serialize_entity_for_undo(em.get_entity_ref(source_current), registry_sp);
            const Guid new_guid = Guid::generate();
            if (!update_entity_guid_in_json(copy_json, new_guid))
            {
                copy_json = nlohmann::json{};
                return CommandStatus::Done;
            }

            const auto parent_ref = em.get_entity_parent(source_current);
            if (!update_parent_guid_in_json(copy_json, parent_ref.guid))
            {
                copy_json = nlohmann::json{};
                return CommandStatus::Done;
            }
        }

        auto er = meta::deserialize_entity_and_register_for_undo(
            copy_json,
            *ctx_sp);
        const auto entity_copy = er.entity;
        bind_refs_for_entity(entity_copy, *ctx_sp);

        if (!target_batch.valid())
        {
            EENG_LOG(ctx_sp.get(), "CopyEntity failed: missing target batch.");
            return CommandStatus::Failed;
        }

        if (!queue_attach_entity_to_batch(
                entity_copy,
                target_batch,
                *ctx_sp,
                attach_future,
                "CopyEntity"))
        {
            return CommandStatus::Failed;
        }

        async_stage = AsyncStage::Attach;
        return update();

        // assert(entity != entt::null);
        // entity_json = Meta::serialize_entities(&entity, 1, context.registry);
        // destroy_func(entity);
    }

    CommandStatus CopyEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        if (copy_json.is_null())
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        auto guid = guid_from_json(copy_json);
        if (!guid.valid())
            return CommandStatus::Done;

        auto entity_opt = em.get_entity_from_guid(guid);
        if (!entity_opt || !entity_opt->has_id())
            return CommandStatus::Done;

        mark_batch_dirty_for_entity(*entity_opt, *ctx_sp);
        if (!queue_destroy_entity_in_batch(
                *entity_opt,
                *ctx_sp,
                destroy_future,
                "CopyEntity undo"))
        {
            return CommandStatus::Failed;
        }
        async_stage = AsyncStage::Destroy;
        return update();

        // Meta::deserialize_entities(entity_json, context);

        // entity_json = nlohmann::json{};
    }

    CommandStatus CopyEntityCommand::update()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage == AsyncStage::Attach)
        {
            bool in_flight = true;
            auto status = poll_bool_future(attach_future, in_flight);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        if (async_stage == AsyncStage::Destroy)
        {
            bool in_flight = true;
            auto status = poll_bool_future(destroy_future, in_flight);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        return CommandStatus::Done;
    }

    std::string CopyEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- CopyEntityBranchCommand --------------------------------------------

    CopyEntityBranchCommand::CopyEntityBranchCommand(
        const Entity& entity,
        EngineContextWeakPtr ctx) :
        root_entity(entity),
        ctx(std::move(ctx))
    {
        display_name = std::string("Copy Entity ") + std::to_string(entity.to_integral());
    }

    CommandStatus CopyEntityBranchCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        auto& scenegraph = em.scene_graph();

        if (branch_json.is_null())
        {
            if (!root_guid.valid())
            {
                if (!try_capture_guid(em, root_entity, root_guid))
                    return CommandStatus::Done;
            }

            const auto root_current = resolve_entity_from_guid(em, root_guid);
            if (!root_current.has_id())
                return CommandStatus::Done;
            if (!scenegraph.contains(root_current))
                return CommandStatus::Done;

            BatchId root_batch{};
            ecs::EntityRef root_ref{};
            if (!resolve_loaded_batch_for_entity(
                    root_current,
                    *ctx_sp,
                    root_batch,
                    root_ref,
                    "CopyEntityBranch"))
            {
                return CommandStatus::Failed;
            }
            target_batch = root_batch;

            auto source_entities = scenegraph.get_branch_topdown(root_current);
            std::unordered_map<Entity, Guid> guid_map;
            guid_map.reserve(source_entities.size());

            branch_json = nlohmann::json::array();

            for (size_t i = 0; i < source_entities.size(); ++i)
            {
                const auto source_entity = source_entities[i];
                auto entity_json = meta::serialize_entity_for_undo(
                    em.get_entity_ref(source_entity),
                    registry_sp);

                const Guid new_guid = Guid::generate();
                guid_map[source_entity] = new_guid;

                if (!update_entity_guid_in_json(entity_json, new_guid))
                {
                    branch_json = nlohmann::json{};
                    return CommandStatus::Done;
                }

                Guid parent_guid = Guid::invalid();
                if (source_entity != root_current)
                {
                    const auto source_parent = scenegraph.get_parent(source_entity);
                    auto it = guid_map.find(source_parent);
                    if (it != guid_map.end())
                        parent_guid = it->second;
                }
                else
                {
                    parent_guid = em.get_entity_parent(source_entity).guid;
                }
                if (!update_parent_guid_in_json(entity_json, parent_guid))
                {
                    branch_json = nlohmann::json{};
                    return CommandStatus::Done;
                }

                branch_json.push_back(std::move(entity_json));
            }
        }

        if (!branch_json.is_array())
            return CommandStatus::Done;

        std::vector<Entity> created_entities;
        created_entities.reserve(branch_json.size());

        for (const auto& entity_json : branch_json)
        {
            auto er = meta::deserialize_entity_for_undo(
                entity_json,
                *ctx_sp);
            created_entities.push_back(er.entity);
        }

        ctx_sp->entity_manager->register_entities_from_deserialization(created_entities);

        if (!target_batch.valid())
        {
            EENG_LOG(ctx_sp.get(), "CopyEntityBranch failed: missing target batch.");
            return CommandStatus::Failed;
        }

        attach_futures.clear();
        attach_futures.reserve(created_entities.size());

        for (auto entity : created_entities)
        {
            bind_refs_for_entity(entity, *ctx_sp);
            std::shared_future<bool> future{};
            if (!queue_attach_entity_to_batch(
                    entity,
                    target_batch,
                    *ctx_sp,
                    future,
                    "CopyEntityBranch"))
            {
                return CommandStatus::Failed;
            }
            attach_futures.push_back(std::move(future));
        }

        async_stage = AsyncStage::Attach;
        return update();
    }

    CommandStatus CopyEntityBranchCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        if (branch_json.is_null() || !branch_json.is_array())
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);

        destroy_futures.clear();
        for (auto it = branch_json.rbegin(); it != branch_json.rend(); ++it)
        {
            const auto guid = guid_from_json(*it);
            if (!guid.valid())
                continue;

            auto entity_opt = em.get_entity_from_guid(guid);
            if (!entity_opt || !entity_opt->has_id())
                continue;
            mark_batch_dirty_for_entity(*entity_opt, *ctx_sp);
            std::shared_future<bool> future{};
            if (!queue_destroy_entity_in_batch(
                    *entity_opt,
                    *ctx_sp,
                    future,
                    "CopyEntityBranch undo"))
            {
                return CommandStatus::Failed;
            }
            destroy_futures.push_back(std::move(future));
        }

        async_stage = AsyncStage::Destroy;
        return update();
    }

    CommandStatus CopyEntityBranchCommand::update()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        if (async_stage == AsyncStage::Attach)
        {
            bool in_flight = true;
            auto status = poll_bool_futures(attach_futures, in_flight);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        if (async_stage == AsyncStage::Destroy)
        {
            bool in_flight = true;
            auto status = poll_bool_futures(destroy_futures, in_flight);
            if (status == CommandStatus::Failed)
            {
                async_stage = AsyncStage::None;
                return status;
            }
            if (status != CommandStatus::Done)
                return status;

            async_stage = AsyncStage::None;
            return CommandStatus::Done;
        }

        return CommandStatus::Done;
    }

    std::string CopyEntityBranchCommand::get_name() const
    {
        return display_name;
    }

    // --- ReparentEntityBranchCommand --------------------------------------------

    ReparentEntityBranchCommand::ReparentEntityBranchCommand(
        const Entity& entity,
        const Entity& parent_entity,
        EngineContextWeakPtr ctx) :
        entity(entity),
        new_parent_entity(parent_entity),
        ctx(std::move(ctx))
    {
        display_name = std::string("Reparent Entity ")
            + std::to_string(entity.to_integral())
            + " to "
            + std::to_string(parent_entity.to_integral());
    }

    CommandStatus ReparentEntityBranchCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        auto& scenegraph = em.scene_graph();

        if (!entity_guid.valid())
        {
            if (!entity.has_id())
                return CommandStatus::Done;
            if (!em.entity_valid(entity))
                return CommandStatus::Done;
            if (!scenegraph.contains(entity))
                return CommandStatus::Done;
            entity_guid = em.get_entity_guid(entity);
        }

        auto entity_opt = em.get_entity_from_guid(entity_guid);
        if (!entity_opt || !entity_opt->has_id())
            return CommandStatus::Done;
        if (!scenegraph.contains(*entity_opt))
            return CommandStatus::Done;

        auto entity_current = *entity_opt;

        if (!new_parent_guid.valid() && new_parent_entity.has_id())
        {
            if (em.entity_valid(new_parent_entity) && scenegraph.contains(new_parent_entity))
                new_parent_guid = em.get_entity_guid(new_parent_entity);
        }

        if (scenegraph.is_root(entity_current))
        {
            prev_parent_guid = Guid::invalid();
        }
        else
        {
            prev_parent_guid = em.get_entity_parent(entity_current).guid;
        }

        Entity parent_current{};
        if (new_parent_guid.valid())
        {
            auto parent_opt = em.get_entity_from_guid(new_parent_guid);
            if (!parent_opt || !parent_opt->has_id())
                return CommandStatus::Done;
            if (!scenegraph.contains(*parent_opt))
                return CommandStatus::Done;
            parent_current = *parent_opt;
        }

        // Transform dirtying on reparent is handled by EntityManager.
        ctx_sp->entity_manager->reparent_entity(entity_current, parent_current);
        sync_branch_batch_with_parent(entity_current, parent_current, *ctx_sp);
        return CommandStatus::Done;
    }

    CommandStatus ReparentEntityBranchCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        auto& scenegraph = em.scene_graph();

        if (!entity_guid.valid())
            return CommandStatus::Done;

        auto entity_opt = em.get_entity_from_guid(entity_guid);
        if (!entity_opt || !entity_opt->has_id())
            return CommandStatus::Done;
        if (!scenegraph.contains(*entity_opt))
            return CommandStatus::Done;

        Entity parent_current{};
        if (prev_parent_guid.valid())
        {
            auto parent_opt = em.get_entity_from_guid(prev_parent_guid);
            if (!parent_opt || !parent_opt->has_id())
                return CommandStatus::Done;
            if (!scenegraph.contains(*parent_opt))
                return CommandStatus::Done;
            parent_current = *parent_opt;
        }

        // Transform dirtying on reparent is handled by EntityManager.
        ctx_sp->entity_manager->reparent_entity(*entity_opt, parent_current);
        sync_branch_batch_with_parent(*entity_opt, parent_current, *ctx_sp);
        return CommandStatus::Done;
    }

    std::string ReparentEntityBranchCommand::get_name() const
    {
        return display_name;
    }

    // --- UnparentEntityBranchCommand --------------------------------------------

    // UnparentEntityBranchCommand::UnparentEntityBranchCommand(
    //     entt::entity entity,
    //     entt::entity parent_entity,
    //     const Context& context) :
    //     entity(entity),
    //     // new_parent_entity(parent_entity),
    //     context(context)
    // {
    //     display_name = std::string("Unparent Entity ") + std::to_string(entt::to_integral(entity));
    // }

    // void UnparentEntityBranchCommand::execute()
    // {
    //     assert(!context.scenegraph.expired());
    //     auto scenegraph = context.scenegraph.lock();

    //     if (scenegraph->is_root(entity))
    //     {
    //         return;
    //         // prev_parent_entity = entt::null;
    //     }
    //     // else
    //         prev_parent_entity = scenegraph->get_parent(entity);

    //     // scenegraph->reparent(entity, new_parent_entity);
    //     scenegraph->unparent(entity);
    // }

    // void UnparentEntityBranchCommand::undo()
    // {
    //     assert(!context.scenegraph.expired());
    //     auto scenegraph = context.scenegraph.lock();

    //     if (prev_parent_entity == entt::null)
    //     {
    //         assert(scenegraph->is_root(entity));
    //         // scenegraph->unparent(entity);
    //         return;
    //     }
    //     // else
    //         scenegraph->reparent(entity, prev_parent_entity);
    // }

    // std::string UnparentEntityBranchCommand::get_name() const
    // {
    //     return display_name;
    // }

    // --- AddComponentToEntityCommand ----------------------------------------

    AddComponentToEntityCommand::AddComponentToEntityCommand(
        const Entity& entity,
        entt::id_type comp_id,
        EngineContextWeakPtr ctx) :
        entity(entity),
        comp_id(comp_id),
        ctx(std::move(ctx))
    {
        display_name = std::string("Add Component ")
            + std::to_string(comp_id)
            + " to Entity "
            + std::to_string(entity.to_integral());
    }

    CommandStatus AddComponentToEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        if (!entity_guid.valid())
        {
            if (!try_capture_guid(em, entity, entity_guid))
                return CommandStatus::Done;
        }
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        if (storage->contains(entity_current))
        {
            if (auto mt = entt::resolve(comp_id); mt)
            {
                EENG_LOG_INFO(ctx_sp.get(),
                    "AddComponent: %s already present on entity %u",
                    eeng::meta::get_meta_type_display_name(mt).c_str(),
                    entity_current.to_integral());
            }
            return CommandStatus::Done;
        }

        auto comp_type = entt::resolve(comp_id);
        if (!comp_type)
            return CommandStatus::Done;

        auto comp_any = comp_type.construct();
        if (!comp_any)
        {
            EENG_LOG_WARN(ctx_sp.get(),
                "AddComponent: failed to default-construct %s",
                eeng::meta::get_meta_type_display_name(comp_type).c_str());
            return CommandStatus::Done;
        }

        storage->push(entity_current, comp_any.base().data());
        bind_refs_for_entity(entity_current, *ctx_sp);
        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        return CommandStatus::Done;
    }

    CommandStatus AddComponentToEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        if (!storage->contains(entity_current))
            return CommandStatus::Done;

        if (comp_id == header_component_id() && header_component_id() != entt::id_type{})
        {
            if (auto mt = entt::resolve(comp_id); mt)
            {
                EENG_LOG_INFO(ctx_sp.get(),
                    "Undo AddComponent: %s is protected; skipping remove on entity %u",
                    eeng::meta::get_meta_type_display_name(mt).c_str(),
                    entity_current.to_integral());
            }
            return CommandStatus::Done;
        }

        storage->remove(entity_current);
        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        return CommandStatus::Done;
    }

    std::string AddComponentToEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- RemoveComponentFromEntityCommand ----------------------------------------

    RemoveComponentFromEntityCommand::RemoveComponentFromEntityCommand(
        const Entity& entity,
        entt::id_type comp_id,
        EngineContextWeakPtr ctx) :
        entity(entity),
        comp_id(comp_id),
        ctx(std::move(ctx))
    {
        display_name = std::string("Remove Component ")
            + std::to_string(comp_id)
            + " from Entity "
            + std::to_string(entity.to_integral());
    }

    CommandStatus RemoveComponentFromEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        if (!entity_guid.valid())
        {
            if (!try_capture_guid(em, entity, entity_guid))
                return CommandStatus::Done;
        }
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        if (!storage->contains(entity_current))
        {
            if (auto mt = entt::resolve(comp_id); mt)
            {
                EENG_LOG_INFO(ctx_sp.get(),
                    "RemoveComponent: %s not present on entity %u",
                    eeng::meta::get_meta_type_display_name(mt).c_str(),
                    entity_current.to_integral());
            }
            return CommandStatus::Done;
        }

        if (comp_id == header_component_id() && header_component_id() != entt::id_type{})
        {
            if (auto mt = entt::resolve(comp_id); mt)
            {
                EENG_LOG_INFO(ctx_sp.get(),
                    "RemoveComponent: %s is protected; skipping remove on entity %u",
                    eeng::meta::get_meta_type_display_name(mt).c_str(),
                    entity_current.to_integral());
            }
            return CommandStatus::Done;
        }

        // Fetch component type
        auto comp_type = entt::resolve(comp_id);
        // Fetch component
        auto comp_any = comp_type.from_void(storage->value(entity_current));
        // Serialize component
        comp_json = meta::serialize_any(
            comp_type.from_void(storage->value(entity_current)),
            eeng::meta::SerializationPurpose::undo);
        // Remove component from entity
        storage->remove(entity_current);
        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        return CommandStatus::Done;
    }

    CommandStatus RemoveComponentFromEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        assert(!storage->contains(entity_current));

        // Fetch component type
        auto comp_type = entt::resolve(comp_id);
        // Default-construct component
        auto comp_any = comp_type.construct();
        // Deserialize component
        meta::deserialize_any(
            comp_json,
            comp_any,
            entity_current,
            *ctx_sp,
            eeng::meta::SerializationPurpose::undo);
        // Add component to entity
        storage->push(entity_current, comp_any.base().data());
        bind_refs_for_entity(entity_current, *ctx_sp);
        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        return CommandStatus::Done;
    }

    std::string RemoveComponentFromEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- ImportModelCommand --------------------------------------------------

    ImportModelCommand::ImportModelCommand(
        std::filesystem::path source_file,
        assets::ImportFlags flags,
        std::string model_name,
        EngineContextWeakPtr ctx,
        std::shared_ptr<std::atomic<bool>> in_flight)
        : source_file(std::move(source_file))
        , flags(flags)
        , model_name(std::move(model_name))
        , ctx(std::move(ctx))
        , ui_in_flight(std::move(in_flight))
        , display_name("Import Model")
    {
    }

    CommandStatus ImportModelCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->resource_manager)
        {
            set_ui_in_flight(ui_in_flight, false);
            return CommandStatus::Done;
        }

        auto& rm = static_cast<ResourceManager&>(*ctx_sp->resource_manager);

        if (was_undone && !imported_roots.empty())
        {
            pending_action = PendingAction::Restore;
            future = queue_restore_task(rm, *ctx_sp, imported_roots);
            in_flight = true;
            set_ui_in_flight(ui_in_flight, true);
            return poll_task_future(future, in_flight, [this](const TaskResult& result)
                {
                    if (result.success)
                        was_undone = false;
                    set_ui_in_flight(ui_in_flight, false);
                    pending_action = PendingAction::None;
                });
        }

        if (!ctx_sp->thread_pool)
        {
            EENG_LOG_WARN(ctx_sp.get(), "Asset import skipped: ThreadPool unavailable.");
            set_ui_in_flight(ui_in_flight, false);
            return CommandStatus::Done;
        }

        const auto& assets_root = rm.assets_root();
        if (assets_root.empty())
        {
            EENG_LOG_WARN(ctx_sp.get(), "Asset import skipped: assets root not set.");
            set_ui_in_flight(ui_in_flight, false);
            return CommandStatus::Done;
        }

        assets::AssimpImportOptions opts{};
        opts.assets_root = assets_root;
        opts.source_file = source_file;
        opts.model_name = model_name.empty() ? source_file.stem().string() : model_name;
        opts.flags = flags;

        pending_action = PendingAction::Import;
        set_ui_in_flight(ui_in_flight, true);

        auto promise = std::make_shared<std::promise<TaskResult>>();
        future = promise->get_future().share();
        in_flight = true;

        auto ctx_wptr = ctx_sp->weak_from_this();
        ctx_sp->thread_pool->queue_task([opts = std::move(opts), ctx_wptr, promise]() mutable
            {
                auto ctx_sp = ctx_wptr.lock();
                if (!ctx_sp || !ctx_sp->resource_manager)
                {
                    promise->set_value(make_task_error(
                        TaskResult::TaskType::Import,
                        "Import failed: context expired."));
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
                    const auto res = make_task_error(TaskResult::TaskType::Import, error);
                    rm.queue_import_job(
                        [res, promise](ResourceManager&, EngineContext&) mutable -> TaskResult
                        {
                            promise->set_value(res);
                            return res;
                        },
                        *ctx_sp);
                    return;
                }

                auto plan_ptr = std::make_shared<assets::AssimpImportPlan>(std::move(plan));
                rm.queue_import_job(
                    [plan_ptr, promise](ResourceManager& rm, EngineContext& ctx) mutable -> TaskResult
                    {
                        TaskResult res;
                        res.type = TaskResult::TaskType::Import;
                        try
                        {
                            const auto result = assets::AssimpImporter::apply_import_plan(*plan_ptr, ctx);
                            if (!result.success)
                            {
                                const auto error = result.error_message.empty()
                                    ? std::string("Import failed.")
                                    : result.error_message;
                                res.add_result(Guid{}, false, error);
                            }
                            else
                            {
                                const Guid root_guid = result.gpu_model.guid.valid()
                                    ? result.gpu_model.guid
                                    : result.model_guid;
                                res.add_result(root_guid, true, "Import ok");
                                if (!plan_ptr->assets_root.empty())
                                    rm.scan_assets_async(plan_ptr->assets_root, ctx);
                            }
                        }
                        catch (const std::exception& ex)
                        {
                            res.add_result(Guid{}, false, ex.what());
                        }
                        catch (...)
                        {
                            res.add_result(Guid{}, false, "unknown exception in import job");
                        }

                        promise->set_value(res);
                        return res;
                    },
                    *ctx_sp);
            });

        return poll_task_future(future, in_flight, [this](const TaskResult& result)
            {
                if (result.success && pending_action == PendingAction::Import)
                {
                    imported_roots.clear();
                    for (const auto& op : result.results)
                    {
                        if (op.guid.valid())
                            imported_roots.push_back(op.guid);
                    }
                    was_undone = false;
                }
                set_ui_in_flight(ui_in_flight, false);
                pending_action = PendingAction::None;
            });
    }

    CommandStatus ImportModelCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->resource_manager)
            return CommandStatus::Done;

        if (imported_roots.empty())
            return CommandStatus::Done;

        auto& rm = static_cast<ResourceManager&>(*ctx_sp->resource_manager);
        pending_action = PendingAction::Unimport;
        future = queue_unimport_task(rm, *ctx_sp, imported_roots);
        in_flight = true;
        return poll_task_future(future, in_flight, [this](const TaskResult& result)
            {
                if (result.success)
                    was_undone = true;
                pending_action = PendingAction::None;
            });
    }

    CommandStatus ImportModelCommand::update()
    {
        return poll_task_future(future, in_flight, [this](const TaskResult& result)
            {
                if (pending_action == PendingAction::Import && result.success)
                {
                    imported_roots.clear();
                    for (const auto& op : result.results)
                    {
                        if (op.guid.valid())
                            imported_roots.push_back(op.guid);
                    }
                    was_undone = false;
                }
                else if (pending_action == PendingAction::Unimport && result.success)
                {
                    was_undone = true;
                }
                else if (pending_action == PendingAction::Restore && result.success)
                {
                    was_undone = false;
                }

                if (pending_action == PendingAction::Import ||
                    pending_action == PendingAction::Restore)
                {
                    set_ui_in_flight(ui_in_flight, false);
                }
                pending_action = PendingAction::None;
            });
    }

    std::string ImportModelCommand::get_name() const
    {
        return display_name;
    }

    // --- UnimportAssetsCommand ----------------------------------------------

    UnimportAssetsCommand::UnimportAssetsCommand(
        std::vector<Guid> roots,
        EngineContextWeakPtr ctx)
        : roots(std::move(roots))
        , ctx(std::move(ctx))
        , display_name("Unimport Assets")
    {
    }

    CommandStatus UnimportAssetsCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->resource_manager)
            return CommandStatus::Done;

        if (roots.empty())
            return CommandStatus::Done;

        auto& rm = static_cast<ResourceManager&>(*ctx_sp->resource_manager);
        pending_action = PendingAction::Unimport;
        future = queue_unimport_task(rm, *ctx_sp, roots);
        in_flight = true;
        return poll_task_future(future, in_flight, [this](const TaskResult&)
            {
                pending_action = PendingAction::None;
            });
    }

    CommandStatus UnimportAssetsCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->resource_manager)
            return CommandStatus::Done;

        if (roots.empty())
            return CommandStatus::Done;

        auto& rm = static_cast<ResourceManager&>(*ctx_sp->resource_manager);
        pending_action = PendingAction::Restore;
        future = queue_restore_task(rm, *ctx_sp, roots);
        in_flight = true;
        return poll_task_future(future, in_flight, [this](const TaskResult&)
            {
                pending_action = PendingAction::None;
            });
    }

    CommandStatus UnimportAssetsCommand::update()
    {
        return poll_task_future(future, in_flight, [this](const TaskResult&)
            {
                pending_action = PendingAction::None;
            });
    }

    std::string UnimportAssetsCommand::get_name() const
    {
        return display_name;
    }
} // namespace Editor
