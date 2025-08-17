// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ResourceManager.hpp"
#include "AssetIndex.hpp"
#include "ThreadPool.hpp"
#include "EventQueue.h"

namespace eeng
{
    ResourceManager::ResourceManager()
        : storage_(std::make_unique<Storage>())
        , asset_index_(std::make_unique<AssetIndex>())
    {
    }

    ResourceManager::~ResourceManager() = default;

    AssetStatus ResourceManager::get_status(const Guid& guid) const
    {
        std::lock_guard lock(status_mutex_);
        auto it = statuses_.find(guid);
        if (it != statuses_.end())
            return it->second;
        return AssetStatus{};
    }

    std::shared_future<TaskResult>
        ResourceManager::load_and_bind_async(std::deque<Guid> guids, EngineContext& ctx)
    {
        using Op = TaskResult::OperationResult;

        // Wait if a task in progress
        // With a more advanced tracking system, we could avoid this
        // wait_until_idle();

        // 1) Launch parallel load tasks that each return an OperationResult
        std::vector<std::shared_future<Op>> loads;
        loads.reserve(guids.size());

        for (const Guid& guid : guids)
        {
            auto f = ctx.thread_pool->queue_task([this, guid, &ctx]() -> Op
                {
                    // Status gate
                    {
                        std::lock_guard lk(status_mutex_);
                        auto& st = statuses_[guid];
                        if (st.state == LoadState::Loading || st.state == LoadState::Loaded)
                            return Op{ guid, true, {"Load Ok"} }; // Already in-flight/done -> treat as ok
                        st.state = LoadState::Loading;
                        st.error_message.clear();
                    }

                    // Load phase
                    try {
                        this->load_asset(guid, ctx);
                        std::lock_guard lk(status_mutex_);
                        statuses_[guid].state = LoadState::Loaded;
                        return Op{ guid, true, {"Load Ok"} };
                    }
                    catch (const std::exception& ex)
                    {
                        std::lock_guard lk(status_mutex_);
                        auto& st = statuses_[guid];
                        st.state = LoadState::Failed;
                        st.error_message = ex.what();
                        return Op{ guid, false, st.error_message };
                    }
                }).share();

            loads.emplace_back(std::move(f));
        }

        // 2) Join loads, then bind serially & build the final TaskResult
        auto final_future = ctx.thread_pool->queue_task(
            [this, loads = std::move(loads), guids, &ctx]() mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Load;

                // Collect load results
                std::unordered_set<Guid> failed; // Will be skipped when binding
                failed.reserve(loads.size());

                for (auto& f : loads)
                {
                    Op op = f.get(); // Load workers return Op; never throws
                    res.add_result(op.guid, op.success, op.message);
                    if (!op.success) failed.insert(op.guid);
                }

                // Bind phase
                for (const Guid& g : guids)
                {
                    if (failed.count(g)) continue;

                    try {
                        this->bind_asset(g, ctx);
                        res.add_result(g, true, "Bind Ok");
                    }
                    catch (const std::exception& ex)
                    {
                        res.add_result(g, false, ex.what());
                    }
                }

                // Enqueue resource task completed event
                ctx.event_queue->enqueue_event<ResourceTaskCompletedEvent>({ res });

                return res;
            }
        ).share();

        // Track current task
        {
            std::lock_guard lk(task_mutex_);
            current_task_ = final_future;
        }

        return final_future;
    }

    std::shared_future<TaskResult>
        ResourceManager::unbind_and_unload_async(std::deque<Guid> guids, EngineContext& ctx)
    {
        using Op = TaskResult::OperationResult;

        // wait_until_idle();

        // 1) Unbind all GUIDs serially, return Op per GUID)
        auto unbind_fut = ctx.thread_pool->queue_task([this, guids, &ctx]() -> std::vector<Op>
            {
                std::vector<Op> ops;
                ops.reserve(guids.size());

                for (const Guid& g : guids)
                {
                    try {
                        this->unbind_asset(g, ctx);
                        ops.push_back(Op{ g, true, {"Unbind ok"} });
                    }
                    catch (const std::exception& ex)
                    {
                        ops.push_back(Op{ g, false, ex.what() });
                    }
                }
                return ops;
            }).share();

        // 2) Unload GUIDs concurrently, return Op per GUID
        std::vector<std::shared_future<Op>> unloads;
        unloads.reserve(guids.size());

        for (const Guid& g : guids)
        {
            auto f = ctx.thread_pool->queue_task([this, g, unbind_fut, &ctx]() -> Op
                {
                    // Wait until all unbinds finish
                    unbind_fut.get();

                    // Gate: only unload if loaded and no refs
                    {
                        std::lock_guard lk(status_mutex_);
                        auto it = statuses_.find(g);
                        if (it == statuses_.end())
                            return Op{ g, true, {"Not loaded"} }; // already gone -> ok

                        auto& st = it->second;
                        if (st.state != LoadState::Loaded || st.ref_count > 0)
                            return Op{ g, true, {"Not loaded"} }; // treat as no-op success

                        st.state = LoadState::Unloading;
                        st.error_message.clear();
                    }

                    try {
                        this->unload_asset(g, ctx);
                        // Remove status entirely
                        std::lock_guard lk(status_mutex_);
                        statuses_.erase(g);
                        return Op{ g, true, {"Unload Ok"} };
                    }
                    catch (const std::exception& ex) {
                        std::lock_guard lk(status_mutex_);
                        auto& st = statuses_[g];
                        st.state = LoadState::Failed;
                        st.error_message = ex.what();
                        return Op{ g, false, st.error_message };
                    }
                }).share();

            unloads.emplace_back(std::move(f));
        }

        // 3) Join & aggregate to TaskResult
        auto final_future = ctx.thread_pool->queue_task(
            [this, guids, unbind_fut, unloads = std::move(unloads), &ctx]() mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Unload;

                // Collect unbind results first (kept separate if you want to show both)
                auto unbind_ops = unbind_fut.get();
                for (auto& o : unbind_ops) res.add_result(o.guid, o.success, o.message);

                // Collect unload results
                for (auto& f : unloads) {
                    Op o = f.get();
                    res.add_result(o.guid, o.success, o.message);
                }

                // Enqueue resource task completed event
                ctx.event_queue->enqueue_event<ResourceTaskCompletedEvent>({ res });

                return res;
            }
        ).share();

        // Track current task
        { std::lock_guard lk(task_mutex_); current_task_ = final_future; }

        return final_future;
    }

    std::shared_future<TaskResult>
        ResourceManager::reload_and_rebind_async(std::deque<Guid> guids, EngineContext& ctx)
    {
        // Unbind & unload
        auto unload_res_fut = unbind_and_unload_async(guids, ctx);

        // Load & bind + merge results
        auto final_future = ctx.thread_pool->queue_task(
            [this, guids, unload_res_fut, &ctx]() -> TaskResult
            {
                TaskResult merged;
                merged.type = TaskResult::TaskType::Reload;

                // Wait & gather unload & unbind results
                TaskResult unload_res = unload_res_fut.get();
                for (auto& r : unload_res.results)
                    merged.add_result(r.guid, r.success, r.message);

                // Wait & gather load & bind results
                TaskResult load_res = load_and_bind_async(guids, ctx).get();
                for (auto& r : load_res.results)
                    merged.add_result(r.guid, r.success, r.message);

                return merged;
            }
        ).share();

        // Track current task
        { std::lock_guard lk(task_mutex_); current_task_ = final_future; }

        return final_future;
    }

    void ResourceManager::retain_guid(const Guid& guid)
    {
        std::lock_guard lock(status_mutex_);
        statuses_[guid].ref_count++;
    }

    void ResourceManager::release_guid(const Guid& guid, EngineContext& ctx)
    {
        std::lock_guard lock(status_mutex_);
        auto& status = statuses_[guid];
        if (--status.ref_count <= 0) {
            status.ref_count = 0;
        }
    }

    bool ResourceManager::is_scanning() const
    {
        return asset_index_->is_scanning();
    }

    bool ResourceManager::is_busy() const {
        std::shared_future<TaskResult> f;
        { std::lock_guard lk(task_mutex_); f = current_task_; }
        return f.valid() && f.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
    }

    void ResourceManager::wait_until_idle() const
    {
        std::shared_future<TaskResult> f;
        { std::lock_guard lk(task_mutex_); f = current_task_; }
        if (f.valid()) f.wait();   // returns immediately if already finished

        // std::cout << "wait_until_idle() ..." << std::endl;
        // for (;;) {
        //     std::shared_future<TaskResult> f;
        //     { std::lock_guard lk(task_mutex_); f = current_task_; }
        //     if (!f.valid()) return;   // nothing in flight
        //     f.wait();                 // wait outside the lock
        // }
        // std::cout << "wait_until_idle() ... done" << std::endl;
    }

    std::optional<TaskResult> ResourceManager::last_task_result() const {
        std::shared_future<TaskResult> f;
        { std::lock_guard lk(task_mutex_); f = current_task_; }
        if (!f.valid() || f.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
            return std::nullopt;
        return f.get(); // OK for shared_future, doesn’t consume the state
    }

    std::shared_future<TaskResult> ResourceManager::active_task() const {
        std::lock_guard lk(task_mutex_);
        return current_task_; // copy of shared_future is cheap
    }

    AssetIndexDataPtr ResourceManager::get_index_data() const
    {
        return asset_index_->get_index_data();
    }

    std::string ResourceManager::to_string() const
    {
        return storage_->to_string();
    }

    // Non-inherited API

    const Storage& ResourceManager::storage() const
    {
        return *storage_;
    }

    Storage& ResourceManager::storage()
    {
        return *storage_;
    }

    const AssetIndex& ResourceManager::asset_index() const
    {
        return *asset_index_;
    }

    AssetIndex& ResourceManager::asset_index()
    {
        return *asset_index_;
    }

    void ResourceManager::start_async_scan(
        const std::filesystem::path& root,
        EngineContext& ctx
    )
    {
        // std::lock_guard lock{ mutex_ };

        std::cout << "[ResourceManager] Scanning assets in: " << root.string() << "\n";
        // auto asset_index = asset_index_->scan_meta_files(root, ctx);
        asset_index_->start_async_scan(root, ctx);

        // Print asset info
#if 0
        std::cout << "[ResourceManager] Found " << asset_index.size() << " assets:\n";
        for (const auto& entry : asset_index)
        {
            std::cout << "  - " << entry.meta.name << " ("
                << entry.meta.type_name << ") at "
                << entry.relative_path.string() << "\n";
        }
#endif
    }

    void ResourceManager::load_asset(const Guid& guid, EngineContext& ctx)
    {
        invoke_meta_function(guid, ctx, literals::load_asset_hs, "load_asset");
    }

    void ResourceManager::unload_asset(const Guid& guid, EngineContext& ctx)
    {
        invoke_meta_function(guid, ctx, literals::unload_asset_hs, "unload_asset");
    }

    // public
    // std::future<void> ResourceManager::reload_asset_async(const Guid& guid, EngineContext& ctx)
    // {
    //     {
    //         std::lock_guard lock(status_mutex_);
    //         statuses_[guid].error_message.clear();
    //     }

    //     return ctx.thread_pool->queue_task([=, this, &ctx]() {
    //         try
    //         {
    //             this->unload_asset(guid, ctx);
    //             this->load_asset(guid, ctx);

    //             // Wait for unload to complete inside thread
    //             // this->unload_asset_async(guid, ctx).get();

    //             // Wait for load to complete
    //             // this->load_asset_async(guid, ctx).get();

    //             std::lock_guard lock(status_mutex_);
    //             statuses_[guid].state = LoadState::Loaded;
    //         }
    //         catch (const std::exception& ex)
    //         {
    //             std::lock_guard lock(status_mutex_);
    //             statuses_[guid].state = LoadState::Failed;
    //             statuses_[guid].error_message = ex.what();
    //         }
    //         });
    // }

    void ResourceManager::bind_asset(const Guid& guid, EngineContext& ctx)
    {
        invoke_meta_function(guid, ctx, literals::bind_asset_hs, "resolve_asset");
    }

    void ResourceManager::unbind_asset(const Guid& guid, EngineContext& ctx)
    {
        invoke_meta_function(guid, ctx, literals::unbind_asset_hs, "unresolve_asset");
    }

    bool ResourceManager::validate_asset(const Guid& guid, EngineContext& ctx)
    {
        auto res_any = invoke_meta_function(guid, ctx, literals::validate_asset_hs, "validate_asset");
        if (auto res_ptr = res_any.try_cast<bool>())
            return *res_ptr;
        throw std::runtime_error("Unexpected return type form meta function validate_asset");
    }

    bool ResourceManager::validate_asset_recursive(const Guid& guid, EngineContext& ctx)
    {
        auto res_any = invoke_meta_function(guid, ctx, literals::validate_asset_recursive_hs, "validate_asset_recursive");
        if (auto res_ptr = res_any.try_cast<bool>())
            return *res_ptr;
        throw std::runtime_error("Unexpected return type form meta function validate_asset_recursive");
    }

    entt::meta_any ResourceManager::invoke_meta_function(
        const Guid& guid,
        EngineContext& ctx,
        entt::hashed_string function_id,
        std::string_view function_label
    )
    {
        auto index_data = asset_index_->get_index_data();

        auto it = index_data->by_guid.find(guid);
        if (it == index_data->by_guid.end() || !it->second)
            throw std::runtime_error("Asset not found for GUID: " + guid.to_string());

        const auto& type_name = it->second->meta.type_name;

        entt::meta_type type = entt::resolve(entt::hashed_string{ type_name.c_str() });
        if (!type)
            throw std::runtime_error("Type not registered: " + std::string(type_name));

        auto fn = type.func(function_id);
        if (!fn)
            throw std::runtime_error(std::string(function_label) + " function not registered for type: " + type_name);

        auto result = fn.invoke({}, entt::forward_as_meta(guid), entt::forward_as_meta(ctx));
        if (!result)
            throw std::runtime_error("Failed to invoke " + std::string(function_label) + " for type: " + type_name);

        return result;
    }
}