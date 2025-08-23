// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include "Handle.h"
#include "AssetIndexData.hpp"
#include <string>
#include <future>
#include <deque>

#pragma once

namespace eeng
{
    struct EngineContext;

    enum class LoadState {
        Loading,
        Loaded,
        Unloading,
        Unloaded,
        Failed
    };

    // Load status and reference counting for an asset
    struct AssetStatus
    {
        LoadState state = LoadState::Unloaded;
        int ref_count = 0;
        bool resolved = false;
        // std::optional<entt::meta_type> type;
        std::string error_message;
    };

    // Result of a task (load/unload/reload)
    struct TaskResult
    {
        struct OperationResult
        {
            Guid guid{};
            bool success{ true };
            std::string message{};
        };

        enum class TaskType { None, Load, Unload, Reload, Scan };

        TaskType type{ TaskType::None };
        bool success{ true };
        std::vector<OperationResult> results;

        void add_result(const Guid& guid, bool ok, std::string_view err = {})
        {
            success &= ok;
            results.push_back(OperationResult{ guid, ok, std::string(err) });
        }
    };

    // Lease
    using BatchId = Guid;              // reuse your Guid
    //    inline BatchId AdHocBatch() { return {}; } // or Guid::null()

    class IResourceManager
    {
    public:

        virtual AssetStatus get_status(const Guid& guid) const = 0;

        virtual std::shared_future<TaskResult> load_and_bind_async(std::deque<Guid> branch_guids, const BatchId& batch, EngineContext& ctx) = 0;
        virtual std::shared_future<TaskResult> unbind_and_unload_async(std::deque<Guid> branch_guids, const BatchId& batch, EngineContext& ctx) = 0;
        virtual std::shared_future<TaskResult> reload_and_rebind_async(std::deque<Guid> guids, const BatchId& batch, EngineContext& ctx) = 0;
        virtual std::shared_future<TaskResult> scan_assets_async(const std::filesystem::path& root, EngineContext& ctx) = 0;

        virtual void retain_guid(const Guid& guid) = 0;
        virtual void release_guid(const Guid& guid, EngineContext& ctx) = 0;

        // virtual bool is_scanning() const = 0;

        virtual bool is_busy() const = 0;
        virtual void wait_until_idle() const = 0;
        virtual int tasks_in_flight() const noexcept = 0;
        // virtual std::optional<TaskResult> last_task_result() const = 0;
        // virtual std::shared_future<TaskResult> active_task() const = 0;

        virtual AssetIndexDataPtr get_index_data() const = 0;

        virtual std::string to_string() const = 0;

        virtual ~IResourceManager() = default;
    };
} // namespace eeng