// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/CommandAsync.hpp"
#include <chrono>

namespace eeng::editor
{
    // Update UI "busy" flags without coupling commands to UI internals.
    void set_ui_in_flight(
        const std::shared_ptr<std::atomic<bool>>& flag,
        bool value)
    {
        if (flag)
            flag->store(value, std::memory_order_relaxed);
    }

    // Poll a TaskResult future without blocking; clears the future when ready.
    CommandStatus poll_task_future(
        std::shared_future<TaskResult>& future,
        bool& in_flight_flag,
        const std::function<void(const TaskResult&)>& on_ready)
    {
        if (!future.valid())
        {
            in_flight_flag = false;
            return CommandStatus::Done;
        }

        if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return CommandStatus::InFlight;

        in_flight_flag = false;
        const TaskResult result = future.get();
        future = {};

        if (on_ready)
            on_ready(result);

        return result.success ? CommandStatus::Done : CommandStatus::Failed;
    }

    // Poll a bool future without blocking.
    CommandStatus poll_bool_future(
        std::shared_future<bool>& future,
        bool& in_flight_flag)
    {
        if (!future.valid())
        {
            in_flight_flag = false;
            return CommandStatus::Done;
        }

        if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return CommandStatus::InFlight;

        in_flight_flag = false;
        const bool ok = future.get();
        return ok ? CommandStatus::Done : CommandStatus::Failed;
    }

    // Poll an EntityRef future; unbound results are treated as failure.
    CommandStatus poll_entity_future(
        std::shared_future<ecs::EntityRef>& future,
        bool& in_flight_flag,
        ecs::EntityRef& out_entity_ref)
    {
        if (!future.valid())
        {
            in_flight_flag = false;
            return CommandStatus::Done;
        }

        if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return CommandStatus::InFlight;

        in_flight_flag = false;
        out_entity_ref = future.get();
        return out_entity_ref.is_bound() ? CommandStatus::Done : CommandStatus::Failed;
    }

    // Poll a batch of bool futures and aggregate success.
    CommandStatus poll_bool_futures(
        std::vector<std::shared_future<bool>>& futures,
        bool& in_flight_flag)
    {
        if (futures.empty())
        {
            in_flight_flag = false;
            return CommandStatus::Done;
        }

        bool any_in_flight = false;
        bool all_ok = true;

        for (auto& future : futures)
        {
            if (!future.valid())
                continue;
            if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            {
                any_in_flight = true;
                continue;
            }

            const bool ok = future.get();
            future = {};
            all_ok = all_ok && ok;
        }

        if (any_in_flight)
            return CommandStatus::InFlight;

        in_flight_flag = false;
        return all_ok ? CommandStatus::Done : CommandStatus::Failed;
    }
}
