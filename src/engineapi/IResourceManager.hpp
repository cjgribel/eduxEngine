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

    enum class LoadState { Loading, Loaded, Unloading, Unloaded, Failed };
    enum class BindState { Unbound, PartiallyBound, Bound };

    struct OperationResult
    {
        Guid guid{}; // Asset GUID
        bool success{ true };
        std::string message{};
    };

    // Load status and reference counting for an asset
    struct AssetStatus
    {
        LoadState state = LoadState::Unloaded;
        BindState bind_state = BindState::Unbound;
        std::string error_message;
        // std::vector<OperationResult> logs;
    };

    // Result of a task (load/unload/reload)
    struct TaskResult
    {
        enum class TaskType { None, Load, Unload, Reload, Scan };
        // enum class TaskType  { None, ResourceLoad, ResourceUnload, ResourceReload, ResourceScan, BatchLoad, BatchUnload, BatchSave, BatchRebindClosure };

        TaskType type{ TaskType::None };
        bool success{ true };
        std::vector<OperationResult> results;

        void add_result(const Guid& guid, bool ok, std::string_view err = {})
        {
            success &= ok;
            results.push_back(OperationResult{ guid, ok, std::string(err) });
        }
    };

    // class BindRefResult
    // {
    //     Guid guid{};
    //     bool success{ true };
    //     std::string msg;   // empty on success
    // public:
    //     BindRefResult(const Guid& guid, bool success, const std::string& msg)
    //         : guid(guid), success(success), msg(msg) {
    //     }
    // };

    // struct LoadResult
    // {
    //     // maybe other stuff
    //     std::vector<OperationResult> ref_results;
    // };

    struct BindResult
    {
        Guid guid{};                    // referrer
        bool all_refs_bound{ false };
        std::vector<OperationResult> ref_results;
    };

    // Lease
    using BatchId = Guid;
    //    inline BatchId AdHocBatch() { return {}; } // or Guid::null()

    class IResourceManager
    {
    public:

        virtual AssetStatus get_status(const Guid& guid) const = 0;

        virtual std::shared_future<TaskResult> scan_assets_async(const std::filesystem::path& root, EngineContext& ctx) = 0;
        virtual std::shared_future<TaskResult> load_and_bind_async(std::deque<Guid> branch_guids, const BatchId& batch, EngineContext& ctx) = 0;
        virtual std::shared_future<TaskResult> unbind_and_unload_async(std::deque<Guid> branch_guids, const BatchId& batch, EngineContext& ctx) = 0;
        virtual std::shared_future<TaskResult> reload_and_rebind_async(std::deque<Guid> guids, const BatchId& batch, EngineContext& ctx) = 0;

        // virtual void retain_guid(const Guid& guid) = 0;
        // virtual void release_guid(const Guid& guid, EngineContext& ctx) = 0;

        // virtual bool is_scanning() const = 0;

        virtual bool is_busy() const = 0;
        virtual void wait_until_idle() = 0;
        virtual int  queued_tasks()   const noexcept = 0;

        virtual AssetIndexDataPtr get_index_data() const = 0;

        virtual std::vector<Guid> find_guids_by_name(std::string_view name) const = 0;

        virtual std::string to_string() const = 0;

        virtual ~IResourceManager() = default;
    };
} // namespace eeng