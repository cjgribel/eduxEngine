#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <vector>
#include "ecs/Entity.hpp"
#include "editor/Command.hpp"
#include "engineapi/IResourceManager.hpp"

namespace eeng::editor
{
    void set_ui_in_flight(
        const std::shared_ptr<std::atomic<bool>>& flag,
        bool value);

    CommandStatus poll_task_future(
        std::shared_future<TaskResult>& future,
        bool& in_flight_flag,
        const std::function<void(const TaskResult&)>& on_ready = {});

    CommandStatus poll_bool_future(
        std::shared_future<bool>& future,
        bool& in_flight_flag);

    CommandStatus poll_entity_future(
        std::shared_future<ecs::EntityRef>& future,
        bool& in_flight_flag,
        ecs::EntityRef& out_entity_ref);

    CommandStatus poll_bool_futures(
        std::vector<std::shared_future<bool>>& futures,
        bool& in_flight_flag);
}
