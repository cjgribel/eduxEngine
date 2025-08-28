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

    SerialExecutor& ResourceManager::strand(EngineContext& ctx)
    {
        std::scoped_lock lk(strand_mutex_);
        if (!rm_strand_) {
            // thread_pool is owned by EngineContext; we just borrow it
            rm_strand_.emplace(*ctx.thread_pool);
        }
        return *rm_strand_;
    }

    AssetStatus ResourceManager::get_status(const Guid& guid) const
    {
        std::lock_guard lock(status_mutex_);
        auto it = statuses_.find(guid);
        if (it != statuses_.end())
            return it->second;
        return AssetStatus{};
    }

    std::shared_future<TaskResult>
        ResourceManager::scan_assets_async(const std::filesystem::path& root, EngineContext& ctx)
    {
        // tasks_in_flight_.fetch_add(1, std::memory_order_relaxed);

        auto prom = std::make_shared<std::promise<TaskResult>>();
        auto fut = prom->get_future().share();

        strand(ctx).post([this, root, &ctx, prom]() mutable {
            TaskResult res; res.type = TaskResult::TaskType::Scan;
            try {
                auto data = asset_index_->scan_assets(root, ctx);       // blocking scan (no RM locks)
                const auto count = data ? data->entries.size() : 0;
                {
                    std::unique_lock lk(scan_mutex_);                   // (if you keep one)
                    asset_index_->publish(std::move(data));             // atomic snapshot swap
                }
                res.add_result(Guid{}, true, "Scan OK: " + std::to_string(count) + " assets");
            }
            catch (const std::exception& ex) {
                res.add_result(Guid{}, false, ex.what());
            }
            (void)ctx.event_queue->enqueue_event(ResourceTaskCompletedEvent{ res });
            prom->set_value(std::move(res));
            });

        return fut;
    }

    std::shared_future<TaskResult>
        ResourceManager::load_and_bind_async(std::deque<Guid> guids, const BatchId& batch, EngineContext& ctx)
    {
        // tasks_in_flight_.fetch_add(1, std::memory_order_relaxed);

        auto prom = std::make_shared<std::promise<TaskResult>>();
        auto fut = prom->get_future().share();

        strand(ctx).post([this, guids = std::move(guids), batch, &ctx, prom]() mutable {
            TaskResult res;
            try {
                res = this->load_and_bind_impl(std::move(guids), batch, ctx);
            }
            catch (const std::exception& ex) {
                res.type = TaskResult::TaskType::Load;
                res.add_result(Guid{}, false, ex.what());
            }
            (void)ctx.event_queue->enqueue_event(ResourceTaskCompletedEvent{ res });
            prom->set_value(std::move(res));
            });

        return fut;
    }

    std::shared_future<TaskResult>
        ResourceManager::unbind_and_unload_async(std::deque<Guid> guids, const BatchId& batch, EngineContext& ctx)
    {
        // tasks_in_flight_.fetch_add(1, std::memory_order_relaxed);

        auto prom = std::make_shared<std::promise<TaskResult>>();
        auto fut = prom->get_future().share();

        strand(ctx).post([this, guids = std::move(guids), batch, &ctx, prom]() mutable {
            TaskResult res;
            try {
                res = this->unbind_and_unload_impl(std::move(guids), batch, ctx);
            }
            catch (const std::exception& ex) {
                res.type = TaskResult::TaskType::Unload;
                res.add_result(Guid{}, false, ex.what());
            }
            (void)ctx.event_queue->enqueue_event(ResourceTaskCompletedEvent{ res });
            prom->set_value(std::move(res));
            });

        return fut;
    }

    std::shared_future<TaskResult>
        ResourceManager::reload_and_rebind_async(std::deque<Guid> guids, const BatchId& batch, EngineContext& ctx)
    {
        // tasks_in_flight_.fetch_add(1, std::memory_order_relaxed);

        auto prom = std::make_shared<std::promise<TaskResult>>();
        auto fut = prom->get_future().share();

        strand(ctx).post([this, guids = std::move(guids), batch, &ctx, prom]() mutable {
            TaskResult merged; merged.type = TaskResult::TaskType::Reload;
            try {
                // serialize inside the same strand task
                TaskResult r1 = this->unbind_and_unload_impl(guids, batch, ctx);
                merged.results.insert(merged.results.end(), r1.results.begin(), r1.results.end());

                TaskResult r2 = this->load_and_bind_impl(std::move(guids), batch, ctx);
                merged.results.insert(merged.results.end(), r2.results.begin(), r2.results.end());
            }
            catch (const std::exception& ex) {
                merged.add_result(Guid{}, false, ex.what());
            }
            (void)ctx.event_queue->enqueue_event(ResourceTaskCompletedEvent{ merged });
            prom->set_value(std::move(merged));
            });

        return fut;
    }

    TaskResult ResourceManager::load_and_bind_impl(
        std::deque<Guid> guids,
        const BatchId& batch,
        EngineContext& ctx)
    {
        using Op = TaskResult::OperationResult;
        TaskResult res; res.type = TaskResult::TaskType::Load;

        // Not needed, mostly for predictability
        std::sort(guids.begin(), guids.end());
        guids.erase(std::unique(guids.begin(), guids.end()), guids.end());

        // Acquire parent leases up-front so overlapping unloads can't drop them
        for (const Guid& g : guids) batch_acquire(batch, g);

        // Parallel loads (status-gated)
        std::vector<std::shared_future<Op>> loads;
        loads.reserve(guids.size());
        for (const Guid& g : guids) {
            loads.emplace_back(
                ctx.thread_pool->queue_task([this, g, &ctx]() -> Op
                    {
                        // auto& mx = mutex_for(g);
                        // std::lock_guard gguard(mx);

                        {   // status gate
                            std::lock_guard lk(status_mutex_);
                            auto& st = statuses_[g];
                            if (st.state == LoadState::Loading || st.state == LoadState::Loaded)
                                return Op{ g, true, "Load Ok" };
                            st.state = LoadState::Loading;
                            st.error_message.clear();
                        }
                        try {
                            this->load_asset(g, ctx);
                            { std::lock_guard lk(status_mutex_); statuses_[g].state = LoadState::Loaded; }
                            return Op{ g, true, "Load Ok" };
                        }
                        catch (const std::exception& ex) {
                            std::lock_guard lk(status_mutex_);
                            auto& st = statuses_[g];
                            st.state = LoadState::Failed;
                            st.error_message = ex.what();
                            return Op{ g, false, st.error_message };
                        }
                    }).share()
                        );
        }

        // Collect load results
        std::unordered_set<Guid> failed; failed.reserve(guids.size());
        for (auto& f : loads) {
            Op op = f.get();
            res.add_result(op.guid, op.success, op.message);
            if (!op.success) failed.insert(op.guid);
        }

        // Bind phase (meta bind that takes batch)
        for (const Guid& g : guids) {
            //if (failed.count(g)) { (void)batch_release(batch, g); continue; }
            if (failed.count(g)) continue;

            try {
                // invoke_meta_function(g, batch, ctx, bind_asset_hs, "bind_asset_with_batch");
                // (Use your overload that takes batch_id)
                (void)invoke_meta_function(g, batch, ctx, literals::bind_asset_hs, "bind_asset");

                res.add_result(g, true, "Bind Ok");
            }
            catch (const std::exception& ex) {
                (void)batch_release(batch, g); // roll back parent lease if bind fails
                res.add_result(g, false, ex.what());
            }
        }

        return res;
    }

    TaskResult ResourceManager::unbind_and_unload_impl(
        std::deque<Guid> guids,
        const BatchId& batch,
        EngineContext& ctx)
    {
        using Op = TaskResult::OperationResult;
        TaskResult res; res.type = TaskResult::TaskType::Unload;

        // Optional determinism:
        // std::sort(guids.begin(), guids.end());
        // guids.erase(std::unique(guids.begin(), guids.end()), guids.end());

        for (const Guid& g : guids)
        {
            // auto& mx = mutex_for(g);
            // std::lock_guard gguard(mx);

            // 1) Drop this batch’s lease for g. Only proceed if we’re the last holder.
            const bool last = batch_release(batch, g);
            if (!last) {
                res.add_result(g, true, "Lease remains (kept)");
                continue;
            }

            // 2) We are the last holder → unbind (idempotent; doesn’t touch leases in closure mode)
            try {
                (void)invoke_meta_function(g, batch, ctx, literals::unbind_asset_hs, "unbind_asset");
            }
            catch (const std::exception& ex) {
                // Leave asset loaded; another attempt can be made later.
                res.add_result(g, false, std::string("Unbind failed: ") + ex.what());
                continue;
            }

            // 3) Status gate + unload
            {
                std::lock_guard lk(status_mutex_);
                auto it = statuses_.find(g);
                if (it == statuses_.end()) {
                    res.add_result(g, true, "Not loaded");
                    continue;
                }
                auto& st = it->second;
                if (st.state != LoadState::Loaded) {
                    res.add_result(g, true, "Skip unload (not Loaded)");
                    continue;
                }
                st.state = LoadState::Unloading;
                st.error_message.clear();
            }

            try {
                this->unload_asset(g, ctx);
                std::lock_guard lk(status_mutex_);
                statuses_.erase(g);
                res.add_result(g, true, "Unbind+Unload Ok");
            }
            catch (const std::exception& ex) {
                std::lock_guard lk(status_mutex_);
                auto& st = statuses_[g];
                st.state = LoadState::Failed;
                st.error_message = ex.what();
                res.add_result(g, false, st.error_message);
            }
        }

        return res;
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

    // bool ResourceManager::is_scanning() const
    // {
    //     return asset_index_->is_scanning();
    // }

    bool ResourceManager::is_busy() const {
        std::scoped_lock lk(strand_mutex_);
        return rm_strand_ ? rm_strand_->is_busy() : false;
    }

    void ResourceManager::wait_until_idle() {
        std::unique_lock lk(strand_mutex_);
        if (!rm_strand_) return;
        auto* s = &*rm_strand_;
        lk.unlock();                // avoid holding RM lock while waiting
        s->wait_idle();
    }

    int ResourceManager::queued_tasks() const noexcept {
        std::scoped_lock lk(strand_mutex_);
        return rm_strand_ ? static_cast<int>(rm_strand_->queued()) : 0;
    }

    uint32_t ResourceManager::total_leases(const Guid& g) const noexcept
    {
        std::lock_guard lk(lease_mutex_);
        auto it = leases_.find(g);
        return it == leases_.end() ? 0u : it->second.total;
    }

    bool ResourceManager::held_by_any(const Guid& g) const noexcept
    {
        std::lock_guard lk(lease_mutex_);
        return leases_.find(g) != leases_.end();
    }

    bool ResourceManager::held_by_batch(const Guid& g, const BatchId& b) const noexcept
    {
        std::lock_guard lk(lease_mutex_);
        auto it = leases_.find(g);
        if (it == leases_.end()) return false;
        auto bit = it->second.by_batch.find(b);
        return bit != it->second.by_batch.end() && bit->second > 0;
    }

    // std::optional<TaskResult> ResourceManager::last_task_result() const {
    //     std::shared_future<TaskResult> f;
    //     { std::lock_guard lk(task_mutex_); f = current_task_; }
    //     if (!f.valid() || f.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    //         return std::nullopt;
    //     return f.get(); // OK for shared_future, doesn’t consume the state
    // }

    // std::shared_future<TaskResult> ResourceManager::active_task() const {
    //     std::lock_guard lk(task_mutex_);
    //     return current_task_; // copy of shared_future is cheap
    // }

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

    void ResourceManager::bind_asset(const Guid& guid, const Guid& batch_id, EngineContext& ctx)
    {
        invoke_meta_function(guid, batch_id, ctx, literals::bind_asset_hs, "resolve_asset");
    }

    void ResourceManager::unbind_asset(const Guid& guid, const Guid& batch_id, EngineContext& ctx)
    {
        invoke_meta_function(guid, batch_id, ctx, literals::unbind_asset_hs, "unresolve_asset");
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

    // remove
    entt::meta_any ResourceManager::invoke_meta_function(
        const Guid& asset_guid,
        const Guid& batch_id,
        EngineContext& ctx,
        entt::hashed_string function_id,
        std::string_view function_label
    )
    {
        auto index_data = asset_index_->get_index_data();

        auto it = index_data->by_guid.find(asset_guid);
        if (it == index_data->by_guid.end() || !it->second)
            throw std::runtime_error("Asset not found for GUID: " + asset_guid.to_string());

        const auto& type_name = it->second->meta.type_name;

        entt::meta_type type = entt::resolve(entt::hashed_string{ type_name.c_str() });
        if (!type)
            throw std::runtime_error("Type not registered: " + std::string(type_name));

        auto fn = type.func(function_id);
        if (!fn)
            throw std::runtime_error(std::string(function_label) + " function not registered for type: " + type_name);

        auto result = fn.invoke({}, entt::forward_as_meta(asset_guid), entt::forward_as_meta(batch_id), entt::forward_as_meta(ctx));
        if (!result)
            throw std::runtime_error("Failed to invoke " + std::string(function_label) + " for type: " + type_name);

        return result;
    }
}