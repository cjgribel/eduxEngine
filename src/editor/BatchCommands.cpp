// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/BatchCommands.hpp"
#include "editor/CommandAsync.hpp"
#include "editor/CommandContext.hpp"
#include "ecs/EntityManager.hpp"
#include "LogMacros.h"
#include <algorithm>

namespace eeng::editor
{
    namespace
    {
        bool is_batch_loaded(const std::vector<const BatchInfo*>& batches, const BatchId& id)
        {
            for (const auto* batch : batches)
            {
                if (!batch)
                    continue;
                if (batch->id == id)
                    return batch->state == BatchInfo::State::Loaded;
            }
            return false;
        }

        bool contains_batch(const std::vector<BatchId>& batches, const BatchId& id)
        {
            return std::find(batches.begin(), batches.end(), id) != batches.end();
        }

        std::vector<BatchId> find_loaded_batches_for_entity(
            const std::vector<const BatchInfo*>& batches,
            const ecs::EntityRef& entity_ref)
        {
            std::vector<BatchId> result;
            if (!entity_ref.guid.valid())
                return result;

            // Policy: loaded batches are the source of truth for membership right now.
            // This is a naive scan, but it's editor-only and keeps us consistent with BR state.
            for (const auto* batch : batches)
            {
                if (!batch || batch->state != BatchInfo::State::Loaded)
                    continue;

                const auto& live = batch->live;
                const auto it = std::find_if(live.begin(), live.end(),
                    [&](const ecs::EntityRef& er)
                    {
                        return er.guid == entity_ref.guid;
                    });

                if (it != live.end() && !contains_batch(result, batch->id))
                    result.push_back(batch->id);
            }

            return result;
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

        future = br->queue_unload(batch_id, *ctx_sp);
        in_flight = true;
        return poll_task_future(future, in_flight);
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
        return poll_task_future(future, in_flight);
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
        CommandContext cmd_ctx{ ctx };
        auto ctx_sp = cmd_ctx.lock();
        if (!ctx_sp)
            return CommandStatus::Done;

        auto* br = cmd_ctx.batch_registry(*ctx_sp);
        if (!br)
            return CommandStatus::Done;

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

        future = br->queue_unload_all_async(*ctx_sp);
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

        future = br->queue_unload_all_async(*ctx_sp);
        in_flight = true;
        return poll_task_future(future, in_flight);
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

        future = br->queue_load_all_async(*ctx_sp);
        in_flight = true;
        return poll_task_future(future, in_flight);
    }

    CommandStatus BatchUnloadAllCommand::update()
    {
        return poll_task_future(future, in_flight);
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

        const auto batches = br->list();
        if (!is_batch_loaded(batches, target_batch))
        {
            // Policy: avoid orphaning entities by refusing to move into unloaded batches.
            return CommandStatus::Failed;
        }

        if (!prepared)
        {
            // Snapshot previous membership once so undo restores the original state.
            assignments.clear();
            assignments.reserve(selection.size());

            for (const auto& entity : selection)
            {
                if (!entity.has_id() || !em->entity_valid(entity))
                    continue;

                const auto entity_ref = em->get_entity_ref(entity);
                if (!entity_ref.is_bound() || !entity_ref.guid.valid())
                    continue;

                auto prev_batches = find_loaded_batches_for_entity(batches, entity_ref);
                const bool target_in_prev = contains_batch(prev_batches, target_batch);

                bool has_other = false;
                for (const auto& id : prev_batches)
                {
                    if (id != target_batch)
                    {
                        has_other = true;
                        break;
                    }
                }

                if (!has_other && target_in_prev)
                    continue;

                assignments.push_back(Assignment{ entity_ref, std::move(prev_batches) });
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
