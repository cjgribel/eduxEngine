// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/BatchCommands.hpp"
#include "editor/CommandAsync.hpp"
#include "editor/CommandContext.hpp"
#include "ThreadPool.hpp"
#include "ecs/EntityBatchPolicy.hpp"
#include "ecs/EntityManager.hpp"
#include "LogMacros.h"
#include <algorithm>

namespace eeng::editor
{
    namespace
    {
        bool contains_batch(const std::vector<BatchId>& batches, const BatchId& id)
        {
            return std::find(batches.begin(), batches.end(), id) != batches.end();
        }
    }

    BatchLoadCommand::BatchLoadCommand(const BatchId& id, EngineContextWeakPtr ctx)
        : batch_id(id)
        , ctx(std::move(ctx))
    {
        display_name = std::string("Load Batch ") + id.to_string();
    }

    CommandStatus BatchLoadCommand::execute()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        future = br->queue_load(batch_id, *ctx_sp);
        in_flight = true;
        return poll_task_future(future, in_flight);
    }

    CommandStatus BatchLoadCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        future = br->queue_unload(batch_id, *ctx_sp);
        in_flight = true;
        return poll_task_future(future, in_flight);
    }

    CommandStatus BatchLoadCommand::update()
    {
        return poll_task_future(future, in_flight);
    }

    std::string BatchLoadCommand::get_name() const
    {
        return display_name;
    }

    BatchUnloadCommand::BatchUnloadCommand(const BatchId& id, EngineContextWeakPtr ctx)
        : batch_id(id)
        , ctx(std::move(ctx))
    {
        display_name = std::string("Unload Batch ") + id.to_string();
    }

    CommandStatus BatchUnloadCommand::execute()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

#ifdef EENG_BATCH_UNLOAD_AUTOSAVE
        if (stage != UnloadStage::None)
            return update();

        if (br->is_batch_loaded(batch_id))
        {
            future = br->queue_save_batch(batch_id, *ctx_sp);
            stage = UnloadStage::Saving;
        }
        else
        {
            future = br->queue_unload(batch_id, *ctx_sp);
            stage = UnloadStage::Unloading;
        }

        in_flight = true;
        return update();
#else
        future = br->queue_unload(batch_id, *ctx_sp);
        in_flight = true;
        return poll_task_future(future, in_flight);
#endif
    }

    CommandStatus BatchUnloadCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        future = br->queue_load(batch_id, *ctx_sp);
        in_flight = true;
        return poll_task_future(future, in_flight);
    }

    CommandStatus BatchUnloadCommand::update()
    {
#ifdef EENG_BATCH_UNLOAD_AUTOSAVE
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
        {
            stage = UnloadStage::None;
            return CommandStatus::Done;
        }

        if (stage == UnloadStage::Saving)
        {
            auto status = poll_task_future(future, in_flight);
            if (status == CommandStatus::InFlight)
                return status;
            if (status == CommandStatus::Failed)
            {
                stage = UnloadStage::None;
                return status;
            }

            auto* br = cmd_ctx.batch_registry(*ctx_sp);
            if (!br)
            {
                stage = UnloadStage::None;
                return CommandStatus::Done;
            }

            future = br->queue_unload(batch_id, *ctx_sp);
            in_flight = true;
            stage = UnloadStage::Unloading;
        }

        if (stage == UnloadStage::Unloading)
        {
            auto status = poll_task_future(future, in_flight);
            if (status != CommandStatus::InFlight)
                stage = UnloadStage::None;
            return status;
        }

        return CommandStatus::Done;
#else
        return poll_task_future(future, in_flight);
#endif
    }

    std::string BatchUnloadCommand::get_name() const
    {
        return display_name;
    }

    BatchLoadAllCommand::BatchLoadAllCommand(EngineContextWeakPtr ctx)
        : ctx(std::move(ctx))
        , display_name("Load All Batches")
    {
    }

    CommandStatus BatchLoadAllCommand::execute()
    {
        if (in_flight)
            return update();

        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        undo_unload_ids.clear();
        const auto batches = br->list();
        for (const auto* batch : batches)
        {
            if (!batch)
                continue;
            if (batch->state == BatchInfo::State::Unloaded)
                undo_unload_ids.push_back(batch->id);
        }

        future = br->queue_load_all_async(*ctx_sp);
        in_flight = true;
        return poll_task_future(future, in_flight);
    }

    CommandStatus BatchLoadAllCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        if (undo_unload_ids.empty())
            return CommandStatus::Done;

        if (!ctx_sp->thread_pool)
            return CommandStatus::Done;

        auto ids = undo_unload_ids;
        future = ctx_sp->thread_pool->queue_task(
            [br, ids = std::move(ids), &ctx = *ctx_sp]() mutable -> TaskResult
            {
                TaskResult merged{};
                merged.success = true;

                std::vector<std::shared_future<TaskResult>> futs;
                futs.reserve(ids.size());
                for (const auto& id : ids)
                    futs.emplace_back(br->queue_unload(id, ctx));

                for (auto& f : futs)
                {
                    TaskResult r = f.get();
                    merged.success &= r.success;
                }

                return merged;
            }).share();

        in_flight = true;
        return poll_task_future(future, in_flight);
    }

    CommandStatus BatchLoadAllCommand::update()
    {
        return poll_task_future(future, in_flight);
    }

    std::string BatchLoadAllCommand::get_name() const
    {
        return display_name;
    }

    BatchUnloadAllCommand::BatchUnloadAllCommand(EngineContextWeakPtr ctx)
        : ctx(std::move(ctx))
        , display_name("Unload All Batches")
    {
    }

    CommandStatus BatchUnloadAllCommand::execute()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

#ifdef EENG_BATCH_UNLOAD_AUTOSAVE
        if (stage != UnloadStage::None)
            return update();

        undo_load_ids.clear();
        const auto batches = br->list();
        for (const auto* batch : batches)
        {
            if (!batch)
                continue;
            if (batch->state != BatchInfo::State::Unloaded)
                undo_load_ids.push_back(batch->id);
        }

        future = br->queue_save_all_async(*ctx_sp);
        in_flight = true;
        stage = UnloadStage::Saving;
        return update();
#else
        future = br->queue_unload_all_async(*ctx_sp);
        in_flight = true;
        return poll_task_future(future, in_flight);
#endif
    }

    CommandStatus BatchUnloadAllCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        if (undo_load_ids.empty())
            return CommandStatus::Done;

        if (!ctx_sp->thread_pool)
            return CommandStatus::Done;

        auto ids = undo_load_ids;
        future = ctx_sp->thread_pool->queue_task(
            [br, ids = std::move(ids), &ctx = *ctx_sp]() mutable -> TaskResult
            {
                TaskResult merged{};
                merged.success = true;

                std::vector<std::shared_future<TaskResult>> futs;
                futs.reserve(ids.size());
                for (const auto& id : ids)
                    futs.emplace_back(br->queue_load(id, ctx));

                for (auto& f : futs)
                {
                    TaskResult r = f.get();
                    merged.success &= r.success;
                }

                return merged;
            }).share();

        in_flight = true;
        return poll_task_future(future, in_flight);
    }

    CommandStatus BatchUnloadAllCommand::update()
    {
#ifdef EENG_BATCH_UNLOAD_AUTOSAVE
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
        {
            stage = UnloadStage::None;
            return CommandStatus::Done;
        }

        if (stage == UnloadStage::Saving)
        {
            auto status = poll_task_future(future, in_flight);
            if (status == CommandStatus::InFlight)
                return status;
            if (status == CommandStatus::Failed)
            {
                stage = UnloadStage::None;
                return status;
            }

            auto* br = cmd_ctx.batch_registry(*ctx_sp);
            if (!br)
            {
                stage = UnloadStage::None;
                return CommandStatus::Done;
            }

            future = br->queue_unload_all_async(*ctx_sp);
            in_flight = true;
            stage = UnloadStage::Unloading;
        }

        if (stage == UnloadStage::Unloading)
        {
            auto status = poll_task_future(future, in_flight);
            if (status != CommandStatus::InFlight)
                stage = UnloadStage::None;
            return status;
        }

        return CommandStatus::Done;
#else
        return poll_task_future(future, in_flight);
#endif
    }

    std::string BatchUnloadAllCommand::get_name() const
    {
        return display_name;
    }

    CreateBatchCommand::CreateBatchCommand(std::string name, EngineContextWeakPtr ctx)
        : name(std::move(name))
        , ctx(std::move(ctx))
        , display_name("Create Batch")
    {
    }

    CommandStatus CreateBatchCommand::execute()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        if (!created_id.valid())
            created_id = Guid::generate();

        const auto new_id = br->create_batch_with_id(created_id, name);
        if (!new_id.valid())
            return CommandStatus::Failed;

        br->save_index();
        return CommandStatus::Done;
    }

    CommandStatus CreateBatchCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        BatchInfo snapshot{};
        if (!br->delete_batch(created_id, &snapshot))
            return CommandStatus::Failed;

        br->save_index();
        return CommandStatus::Done;
    }

    std::string CreateBatchCommand::get_name() const
    {
        return display_name;
    }

    DeleteBatchCommand::DeleteBatchCommand(const BatchId& id, EngineContextWeakPtr ctx)
        : batch_id(id)
        , ctx(std::move(ctx))
    {
        display_name = std::string("Delete Batch ") + id.to_string();
    }

    CommandStatus DeleteBatchCommand::execute()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;
        if (br->is_batch_read_only(batch_id))
        {
            EENG_LOG(ctx_sp.get(), "DeleteBatch blocked: generated/read-only batches must be removed via their owning tool workflow.");
            return CommandStatus::Failed;
        }

        BatchInfo info{};
        if (!br->delete_batch(batch_id, &info))
            return CommandStatus::Failed;

        snapshot = std::move(info);
        br->save_index();
        return CommandStatus::Done;
    }

    CommandStatus DeleteBatchCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (!snapshot.has_value())
            return CommandStatus::Failed;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        if (!br->restore_batch(std::move(*snapshot)))
            return CommandStatus::Failed;

        snapshot.reset();
        br->save_index();
        return CommandStatus::Done;
    }

    std::string DeleteBatchCommand::get_name() const
    {
        return display_name;
    }

    AssignEntitiesToBatchCommand::AssignEntitiesToBatchCommand(
        const BatchId& target,
        std::vector<ecs::Entity> selection,
        EngineContextWeakPtr ctx)
        : target_batch(target)
        , ctx(std::move(ctx))
        , selection(std::move(selection))
    {
        display_name = std::string("Assign Entities to Batch ") + target.to_string();
    }

    CommandStatus AssignEntitiesToBatchCommand::execute()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        auto* em = cmd_ctx.entity_manager(*ctx_sp);
        if (!em)
        {
            EENG_LOG(ctx_sp.get(), "AssignEntitiesToBatch aborted: missing entity manager.");
            return CommandStatus::Failed;
        }
        if (!target_batch.valid())
            return CommandStatus::Done;

        if (!br->is_batch_loaded(target_batch))
        {
            // Policy: avoid orphaning entities by refusing to move into unloaded batches.
            return CommandStatus::Failed;
        }
        if (br->is_batch_read_only(target_batch))
        {
            EENG_LOG(ctx_sp.get(), "AssignEntitiesToBatch blocked: generated/read-only batches are not editable targets.");
            return CommandStatus::Failed;
        }

        if (!prepared)
        {
            // Snapshot previous membership once so undo restores the original state.
            assignments.clear();
            assignments.reserve(selection.size());

            ecs::BatchPolicyContext policy_ctx{ br, em, ctx_sp.get() };
            for (const auto& entity : selection)
            {
                if (!entity.has_id() || !em->entity_valid(entity))
                    continue;

                // Policy: every live entity is in exactly one loaded batch.
                const auto decision = ecs::EntityBatchPolicy::resolve_existing_entity_batch(
                    entity,
                    policy_ctx,
                    "AssignEntitiesToBatch");
                if (!decision.ok)
                {
                    assignments.clear();
                    return CommandStatus::Failed;
                }

                const bool target_in_prev = (decision.batch == target_batch);
                if (target_in_prev)
                    continue;

                std::vector<BatchId> prev_batches{ decision.batch };
                assignments.push_back(Assignment{ decision.entity_ref, std::move(prev_batches) });
            }

            prepared = true;
        }

        if (assignments.empty())
            return CommandStatus::Done;

        futures.clear();
        futures.reserve(assignments.size() * 2);

        for (const auto& assignment : assignments)
        {
            const bool target_in_prev = contains_batch(assignment.prev_batches, target_batch);

            // Policy: enforce exclusivity by detaching from every other loaded batch.
            for (const auto& prev_id : assignment.prev_batches)
            {
                if (prev_id == target_batch)
                    continue;
                futures.push_back(br->queue_detach_entity(prev_id, assignment.entity_ref, *ctx_sp));
            }

            if (!target_in_prev)
                futures.push_back(br->queue_attach_entity(target_batch, assignment.entity_ref, *ctx_sp));
        }

        in_flight = true;
        return poll_bool_futures(futures, in_flight);
    }

    CommandStatus AssignEntitiesToBatchCommand::undo()
    {
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        if (assignments.empty())
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

        futures.clear();
        futures.reserve(assignments.size() * 2);

        for (const auto& assignment : assignments)
        {
            const bool target_in_prev = contains_batch(assignment.prev_batches, target_batch);

            if (!target_in_prev)
                futures.push_back(br->queue_detach_entity(target_batch, assignment.entity_ref, *ctx_sp));

            for (const auto& prev_id : assignment.prev_batches)
            {
                if (prev_id == target_batch)
                    continue;
                futures.push_back(br->queue_attach_entity(prev_id, assignment.entity_ref, *ctx_sp));
            }
        }

        in_flight = true;
        return poll_bool_futures(futures, in_flight);
    }

    CommandStatus AssignEntitiesToBatchCommand::update()
    {
        return poll_bool_futures(futures, in_flight);
    }

    std::string AssignEntitiesToBatchCommand::get_name() const
    {
        return display_name;
    }
}
