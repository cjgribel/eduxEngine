// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/BatchCommands.hpp"
#include <chrono>

namespace eeng::editor
{
    namespace
    {
        CommandStatus poll_future(std::shared_future<TaskResult>& future, bool& in_flight_flag)
        {
            if (!future.valid())
            {
                in_flight_flag = false;
                return CommandStatus::Done;
            }

            if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                return CommandStatus::InFlight;

            in_flight_flag = false;
            const auto result = future.get();
            return result.success ? CommandStatus::Done : CommandStatus::Failed;
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
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        future = br.queue_load(batch_id, *ctx_sp);
        in_flight = true;
        return poll_future(future, in_flight);
    }

    CommandStatus BatchLoadCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        future = br.queue_unload(batch_id, *ctx_sp);
        in_flight = true;
        return poll_future(future, in_flight);
    }

    CommandStatus BatchLoadCommand::update()
    {
        return poll_future(future, in_flight);
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
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        future = br.queue_unload(batch_id, *ctx_sp);
        in_flight = true;
        return poll_future(future, in_flight);
    }

    CommandStatus BatchUnloadCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        future = br.queue_load(batch_id, *ctx_sp);
        in_flight = true;
        return poll_future(future, in_flight);
    }

    CommandStatus BatchUnloadCommand::update()
    {
        return poll_future(future, in_flight);
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
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        future = br.queue_load_all_async(*ctx_sp);
        in_flight = true;
        return poll_future(future, in_flight);
    }

    CommandStatus BatchLoadAllCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        future = br.queue_unload_all_async(*ctx_sp);
        in_flight = true;
        return poll_future(future, in_flight);
    }

    CommandStatus BatchLoadAllCommand::update()
    {
        return poll_future(future, in_flight);
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
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        future = br.queue_unload_all_async(*ctx_sp);
        in_flight = true;
        return poll_future(future, in_flight);
    }

    CommandStatus BatchUnloadAllCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        future = br.queue_load_all_async(*ctx_sp);
        in_flight = true;
        return poll_future(future, in_flight);
    }

    CommandStatus BatchUnloadAllCommand::update()
    {
        return poll_future(future, in_flight);
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
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        if (!created_id.valid())
            created_id = Guid::generate();

        const auto new_id = br.create_batch_with_id(created_id, name);
        if (!new_id.valid())
            return CommandStatus::Failed;

        br.save_index();
        return CommandStatus::Done;
    }

    CommandStatus CreateBatchCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        BatchInfo snapshot{};
        if (!br.delete_batch(created_id, &snapshot))
            return CommandStatus::Failed;

        br.save_index();
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
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        BatchInfo info{};
        if (!br.delete_batch(batch_id, &info))
            return CommandStatus::Failed;

        snapshot = std::move(info);
        br.save_index();
        return CommandStatus::Done;
    }

    CommandStatus DeleteBatchCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->batch_registry)
            return CommandStatus::Done;

        if (!snapshot.has_value())
            return CommandStatus::Failed;

        auto& br = static_cast<BatchRegistry&>(*ctx_sp->batch_registry);
        if (!br.restore_batch(std::move(*snapshot)))
            return CommandStatus::Failed;

        snapshot.reset();
        br.save_index();
        return CommandStatus::Done;
    }

    std::string DeleteBatchCommand::get_name() const
    {
        return display_name;
    }
}
