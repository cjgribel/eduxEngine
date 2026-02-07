// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

// Table of contents:
// - CreateEntityCommand: create an entity in a batch and serialize for undo/redo.
// - DestroyEntityCommand: destroy a single entity and restore from snapshot on undo.
// - DestroyEntityBranchCommand: destroy a branch and restore it on undo.
// - CopyEntityCommand: clone one entity into the same batch.
// - CopyEntityBranchCommand: clone an entity branch into the same batch.
// - ReparentEntityBranchCommand: reparent a branch and sync batch membership.

#include <unordered_map>
#include <utility>
#include "GuiCommands.hpp"
#include "BatchRegistry.hpp"
#include "MetaSerialize.hpp"
#include "editor/CommandAsync.hpp"
#include "editor/CommandBatchHelpers.hpp"
#include "editor/CommandContext.hpp"
#include "editor/CommandEntityHelpers.hpp"
#include "editor/CommandSnapshot.hpp"
#include "ecs/EntityBatchPolicy.hpp"
#include "ecs/EntityManager.hpp"
#include "engineapi/SelectionManager.hpp"
#include "LogMacros.h"

namespace eeng::editor {
    using eeng::BatchId;
    using eeng::BatchRegistry;
    using eeng::EngineContext;
    using eeng::EngineContextWeakPtr;
    using eeng::EntityManager;
    using eeng::Guid;
    namespace ecs = eeng::ecs;
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!em || !br)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        // First execution: create a fresh entity and capture a redo snapshot.
        if (entity_json.is_null())
        {
            Entity parent_current{};
            if (parent_entity.has_id()
                && em->entity_valid(parent_entity)
                && em->scene_graph().contains(parent_entity))
            {
                parent_current = parent_entity;
            }

            BatchId preferred_batch{};
            const BatchId* preferred_ptr = nullptr;
            if (ctx_sp->batch_selection && !ctx_sp->batch_selection->empty())
            {
                preferred_batch = ctx_sp->batch_selection->last();
                preferred_ptr = &preferred_batch;
            }

            BatchId default_batch{};
            const BatchId* default_ptr = nullptr;
            if (br->try_get_batch_id_by_name(BatchRegistry::kDefaultBatchName, default_batch))
                default_ptr = &default_batch;

            ecs::BatchPolicyContext policy_ctx{ br, em, ctx_sp.get() };
            auto decision = ecs::EntityBatchPolicy::resolve_new_entity_batch(
                parent_current,
                preferred_ptr,
                default_ptr,
                policy_ctx,
                "CreateEntity");
            if (!decision.ok)
                return CommandStatus::Failed;

            created_batch = decision.batch;
            create_future = br->queue_create_entity(created_batch, "", decision.parent_ref, *ctx_sp);
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        // If we never created a live entity, only proceed if a GUID was recorded.
        if (!created_entity.has_id())
        {
            if (!created_guid.valid())
                return CommandStatus::Done;
        }

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
            return CommandStatus::Done;

        if (created_guid.valid())
        {
            auto entity_opt = em->get_entity_from_guid(created_guid);
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
            return CommandStatus::Done;

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

            auto registry_sp = em->registry_wptr().lock();
            if (!registry_sp)
                return CommandStatus::Failed;

            entity_json = meta::serialize_entity_for_undo(
                em->get_entity_ref(created_entity),
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!em || !br)
            return CommandStatus::Done;

        auto registry_sp = em->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        if (!entity_guid.valid())
        {
            if (!try_capture_guid(*em, entity, entity_guid))
                return CommandStatus::Done;
        }

        const auto entity_current = resolve_entity_from_guid(*em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        if (entity_json.is_null())
        {
            entity_json = meta::serialize_entity_for_undo(
                em->get_entity_ref(entity_current),
                registry_sp);
        }

        ecs::BatchPolicyContext policy_ctx{ br, em, ctx_sp.get() };
        const auto decision = ecs::EntityBatchPolicy::resolve_existing_entity_batch(
            entity_current,
            policy_ctx,
            "DestroyEntity");
        if (!decision.ok)
            return CommandStatus::Failed;

        entity_batch = decision.batch;

        destroy_future = br->queue_destroy_entity(entity_batch, decision.entity_ref, *ctx_sp);
        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        async_stage = AsyncStage::Destroy;
        return update();
    }

    CommandStatus DestroyEntityCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
            return CommandStatus::Done;

        auto registry_sp = em->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        if (branch_json.is_null())
        {
            if (!root_guid.valid())
            {
                if (!try_capture_guid(*em, root_entity, root_guid))
                    return CommandStatus::Done;
            }

            const auto root_current = resolve_entity_from_guid(*em, root_guid);
            if (!root_current.has_id())
                return CommandStatus::Done;

            auto& scenegraph = em->scene_graph();
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
                    em->get_entity_ref(entity),
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

            auto entity_opt = em->get_entity_from_guid(guid);
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
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

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
            return CommandStatus::Done;

        em->register_entities_from_deserialization(created_entities);

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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
            return CommandStatus::Done;

        auto registry_sp = em->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        if (copy_json.is_null())
        {
            if (!source_guid.valid())
            {
                if (!try_capture_guid(*em, entity_source, source_guid))
                    return CommandStatus::Done;
            }

            const auto source_current = resolve_entity_from_guid(*em, source_guid);
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

            copy_json = meta::serialize_entity_for_undo(em->get_entity_ref(source_current), registry_sp);
            const Guid new_guid = Guid::generate();
            if (!update_entity_guid_in_json(copy_json, new_guid))
            {
                copy_json = nlohmann::json{};
                return CommandStatus::Done;
            }

            const auto parent_ref = em->get_entity_parent(source_current);
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        if (copy_json.is_null())
            return CommandStatus::Done;

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
            return CommandStatus::Done;

        auto guid = guid_from_json(copy_json);
        if (!guid.valid())
            return CommandStatus::Done;

        auto entity_opt = em->get_entity_from_guid(guid);
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
            return CommandStatus::Done;

        auto registry_sp = em->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& scenegraph = em->scene_graph();

        if (branch_json.is_null())
        {
            if (!root_guid.valid())
            {
                if (!try_capture_guid(*em, root_entity, root_guid))
                    return CommandStatus::Done;
            }

            const auto root_current = resolve_entity_from_guid(*em, root_guid);
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
                    em->get_entity_ref(source_entity),
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
                    parent_guid = em->get_entity_parent(source_entity).guid;
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

        em->register_entities_from_deserialization(created_entities);

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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        if (branch_json.is_null() || !branch_json.is_array())
            return CommandStatus::Done;

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
            return CommandStatus::Done;

        destroy_futures.clear();
        for (auto it = branch_json.rbegin(); it != branch_json.rend(); ++it)
        {
            const auto guid = guid_from_json(*it);
            if (!guid.valid())
                continue;

            auto entity_opt = em->get_entity_from_guid(guid);
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
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

    // --- SpawnEntityBranchCommand ------------------------------------------

    SpawnEntityBranchCommand::SpawnEntityBranchCommand(
        nlohmann::json branch_json,
        const Entity& parent_entity,
        EngineContextWeakPtr ctx,
        bool remap_guids) :
        source_json(std::move(branch_json)),
        parent_entity(parent_entity),
        ctx(std::move(ctx)),
        remap_guids(remap_guids)
    {
        display_name = "Spawn Entity Branch";
    }

    CommandStatus SpawnEntityBranchCommand::execute()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
            return CommandStatus::Done;

        auto registry_sp = em->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        if (!prepared)
        {
            // First execution: normalize JSON to an array and prepare GUID/parent mapping.
            if (source_json.is_null())
                return CommandStatus::Done;

            branch_json = source_json;
            if (branch_json.is_object())
            {
                nlohmann::json array = nlohmann::json::array();
                array.push_back(branch_json);
                branch_json = std::move(array);
            }

            if (!branch_json.is_array() || branch_json.empty())
                return CommandStatus::Done;

            // Resolve parent GUID and target batch (policy uses selection/default batch).
            if (!parent_guid.valid() && parent_entity.has_id()
                && em->entity_valid(parent_entity)
                && em->scene_graph().contains(parent_entity))
            {
                parent_guid = em->get_entity_guid(parent_entity);
            }

            ecs::Entity parent_current{};
            if (parent_guid.valid())
            {
                if (auto parent_opt = em->get_entity_from_guid(parent_guid); parent_opt && parent_opt->has_id())
                    parent_current = *parent_opt;
            }
            else if (parent_entity.has_id() && em->entity_valid(parent_entity)
                && em->scene_graph().contains(parent_entity))
            {
                parent_current = parent_entity;
            }

            if (!target_batch.valid())
            {
                ecs::EntityRef parent_ref{};
                if (!resolve_batch_for_new_entity(
                        parent_current,
                        *ctx_sp,
                        target_batch,
                        parent_ref,
                        "SpawnEntityBranch"))
                {
                    return CommandStatus::Failed;
                }
                if (parent_ref.guid.valid())
                    parent_guid = parent_ref.guid;
            }

// remap_guids: 

            if (remap_guids)
            {
                // Remap all entity GUIDs to avoid collisions, then fix parent GUIDs.
                std::unordered_map<Guid, Guid> guid_map;
                guid_map.reserve(branch_json.size());

                // First pass: generate new GUIDs and update entity JSON.
                for (auto& entity_json : branch_json)
                {
                    const Guid old_guid = guid_from_json(entity_json);
                    const Guid new_guid = Guid::generate();
                    if (old_guid.valid())
                        guid_map.emplace(old_guid, new_guid);
                    update_entity_guid_in_json(entity_json, new_guid);
                }

                // Second pass: update parent GUIDs based on mapping.
                for (std::size_t i = 0; i < branch_json.size(); ++i)
                {
                    auto& entity_json = branch_json[i];
                    Guid new_parent_guid = Guid::invalid();
                    if (i == 0)
                    {
                        // Root is parented under the requested parent.
                        new_parent_guid = parent_guid;
                    }
                    else
                    {
                        const Guid old_parent_guid = parent_guid_from_json(entity_json);
                        if (old_parent_guid.valid())
                        {
                            if (auto it = guid_map.find(old_parent_guid); it != guid_map.end())
                                new_parent_guid = it->second;
                            else
                                new_parent_guid = old_parent_guid;
                        }
                    }
                    update_parent_guid_in_json(entity_json, new_parent_guid);
                }
            }
            else
            {
                // Keep GUIDs as-is; only ensure root parent points at target parent.
                update_parent_guid_in_json(branch_json.front(), parent_guid);
            }

            prepared = true;
        }

        // Deserialize entities into the registry (unregistered), then register as a branch.
        std::vector<Entity> created_entities;
        created_entities.reserve(branch_json.size());

        for (const auto& entity_json : branch_json)
        {
            auto er = meta::deserialize_entity_for_undo(
                entity_json,
                *ctx_sp);
            created_entities.push_back(er.entity);
        }

        em->register_entities_from_deserialization(created_entities);

        if (!target_batch.valid())
        {
            EENG_LOG(ctx_sp.get(), "SpawnEntityBranch failed: missing target batch.");
            return CommandStatus::Failed;
        }

        // Attach new entities to batch and bind references.
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
                    "SpawnEntityBranch"))
            {
                return CommandStatus::Failed;
            }
            attach_futures.push_back(std::move(future));
        }

        async_stage = AsyncStage::Attach;
        return update();
    }

    CommandStatus SpawnEntityBranchCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (async_stage != AsyncStage::None)
            return update();

        if (branch_json.is_null() || !branch_json.is_array())
            return CommandStatus::Done;

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
            return CommandStatus::Done;

        // Undo = destroy all spawned entities by their (remapped) GUIDs.
        destroy_futures.clear();
        for (auto it = branch_json.rbegin(); it != branch_json.rend(); ++it)
        {
            const auto guid = guid_from_json(*it);
            if (!guid.valid())
                continue;

            auto entity_opt = em->get_entity_from_guid(guid);
            if (!entity_opt || !entity_opt->has_id())
                continue;
            mark_batch_dirty_for_entity(*entity_opt, *ctx_sp);
            std::shared_future<bool> future{};
            if (!queue_destroy_entity_in_batch(
                    *entity_opt,
                    *ctx_sp,
                    future,
                    "SpawnEntityBranch undo"))
            {
                return CommandStatus::Failed;
            }
            destroy_futures.push_back(std::move(future));
        }

        async_stage = AsyncStage::Destroy;
        return update();
    }

    CommandStatus SpawnEntityBranchCommand::update()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        // Async attach/destroy batches.
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

    std::string SpawnEntityBranchCommand::get_name() const
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!em || !br)
            return CommandStatus::Done;

        auto& scenegraph = em->scene_graph();

        if (!entity_guid.valid())
        {
            if (!entity.has_id())
                return CommandStatus::Done;
            if (!em->entity_valid(entity))
                return CommandStatus::Done;
            if (!scenegraph.contains(entity))
                return CommandStatus::Done;
            entity_guid = em->get_entity_guid(entity);
        }

        auto entity_opt = em->get_entity_from_guid(entity_guid);
        if (!entity_opt || !entity_opt->has_id())
            return CommandStatus::Done;
        if (!scenegraph.contains(*entity_opt))
            return CommandStatus::Done;

        auto entity_current = *entity_opt;

        if (!new_parent_guid.valid() && new_parent_entity.has_id())
        {
            if (em->entity_valid(new_parent_entity) && scenegraph.contains(new_parent_entity))
                new_parent_guid = em->get_entity_guid(new_parent_entity);
        }

        if (scenegraph.is_root(entity_current))
        {
            prev_parent_guid = Guid::invalid();
        }
        else
        {
            prev_parent_guid = em->get_entity_parent(entity_current).guid;
        }

        Entity parent_current{};
        if (new_parent_guid.valid())
        {
            auto parent_opt = em->get_entity_from_guid(new_parent_guid);
            if (!parent_opt || !parent_opt->has_id())
                return CommandStatus::Done;
            if (!scenegraph.contains(*parent_opt))
                return CommandStatus::Done;
            parent_current = *parent_opt;
        }

        // Transform dirtying on reparent is handled by EntityManager.
        em->reparent_entity(entity_current, parent_current);

        BatchId default_batch{};
        const BatchId* default_ptr = nullptr;
        if (br->try_get_batch_id_by_name(BatchRegistry::kDefaultBatchName, default_batch))
            default_ptr = &default_batch;

        ecs::BatchPolicyContext policy_ctx{ br, em, ctx_sp.get() };
        ecs::EntityBatchPolicy::sync_branch_to_parent_batch(
            entity_current,
            parent_current,
            default_ptr,
            policy_ctx);
        return CommandStatus::Done;
    }

    CommandStatus ReparentEntityBranchCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!em || !br)
            return CommandStatus::Done;

        auto& scenegraph = em->scene_graph();

        if (!entity_guid.valid())
            return CommandStatus::Done;

        auto entity_opt = em->get_entity_from_guid(entity_guid);
        if (!entity_opt || !entity_opt->has_id())
            return CommandStatus::Done;
        if (!scenegraph.contains(*entity_opt))
            return CommandStatus::Done;

        Entity parent_current{};
        if (prev_parent_guid.valid())
        {
            auto parent_opt = em->get_entity_from_guid(prev_parent_guid);
            if (!parent_opt || !parent_opt->has_id())
                return CommandStatus::Done;
            if (!scenegraph.contains(*parent_opt))
                return CommandStatus::Done;
            parent_current = *parent_opt;
        }

        // Transform dirtying on reparent is handled by EntityManager.
        em->reparent_entity(*entity_opt, parent_current);

        BatchId default_batch{};
        const BatchId* default_ptr = nullptr;
        if (br->try_get_batch_id_by_name(BatchRegistry::kDefaultBatchName, default_batch))
            default_ptr = &default_batch;

        ecs::BatchPolicyContext policy_ctx{ br, em, ctx_sp.get() };
        ecs::EntityBatchPolicy::sync_branch_to_parent_batch(
            *entity_opt,
            parent_current,
            default_ptr,
            policy_ctx);
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
}
