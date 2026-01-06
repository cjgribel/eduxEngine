// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ResourceManager.hpp"
#include "AssetIndex.hpp"
#include "ThreadPool.hpp"
#include "MainThreadQueue.hpp"
#include "EventQueue.h"
#include "meta/MetaAux.h"
#include "LogMacros.h"
#include "assets/AssetTreeViews.hpp"
#include <algorithm>
#include <functional>
#include <unordered_set>

namespace
{
    std::vector<eeng::Guid> collect_hook_guids(
        const std::deque<eeng::Guid>& guids,
        const std::unordered_set<eeng::Guid>& failed)
    {
        std::vector<eeng::Guid> out;
        out.reserve(guids.size());
        for (const auto& g : guids)
        {
            if (failed.count(g)) continue;
            out.push_back(g);
        }
        return out;
    }

    std::filesystem::path common_ancestor_path(const std::vector<std::filesystem::path>& paths)
    {
        if (paths.empty())
            return {};

        auto it = paths.begin();
        std::filesystem::path common = *it++;
        for (; it != paths.end(); ++it)
        {
            std::filesystem::path next;
            auto a = common.begin();
            auto b = it->begin();
            for (; a != common.end() && b != it->end(); ++a, ++b)
            {
                if (*a != *b)
                    break;
                next /= *a;
            }
            common = std::move(next);
            if (common.empty())
                break;
        }
        return common;
    }

}

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
        auto& s = strand(ctx);
        auto* ctx_ptr = &ctx;            // avoid capturing a ref that could dangle

        // capture root by value so it’s safe after return
        return s.submit([this, root, ctx_ptr]() mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Scan;

                try
                {
                    auto data = asset_index_->scan_assets(root, *ctx_ptr); // blocking scan on RM strand
                    const auto count = data ? data->entries.size() : 0;

                    {
                        std::unique_lock lk(scan_mutex_);                 // 
                        asset_index_->publish(std::move(data));           // atomic snapshot swap
                    }

                    res.add_result(Guid{}, true, "Scan OK: " + std::to_string(count) + " assets");
                }
                catch (const std::exception& ex)
                {
                    res.add_result(Guid{}, false, ex.what());
                }
                // enqueue_event doesn't throw
                (void)ctx_ptr->event_queue->enqueue_event(ResourceTaskCompletedEvent{ res });
                return res;
            });
    }

    std::shared_future<TaskResult>
        ResourceManager::queue_import_job(ImportJob job, EngineContext& ctx)
    {
        auto& s = strand(ctx);
        auto* ctx_ptr = &ctx;

        return s.submit([this, job = std::move(job), ctx_ptr]() mutable -> TaskResult
            {
                TaskResult res;
                res.type = TaskResult::TaskType::Import;
                try
                {
                    res = job(*this, *ctx_ptr);
                    if (res.type == TaskResult::TaskType::None)
                        res.type = TaskResult::TaskType::Import;
                }
                catch (const std::exception& ex)
                {
                    res.add_result(Guid{}, false, ex.what());
                }
                catch (...)
                {
                    res.add_result(Guid{}, false, "unknown exception in import job");
                }

                (void)ctx_ptr->event_queue->enqueue_event(ResourceTaskCompletedEvent{ res });
                return res;
            });
    }

    std::shared_future<TaskResult>
        ResourceManager::load_and_bind_async(std::deque<Guid> guids, const BatchId& batch, EngineContext& ctx)
    {
        auto& s = strand(ctx);
        auto* ctx_ptr = &ctx; // avoid capturing a reference that might dangle

        return s.submit([this, guids = std::move(guids), batch, ctx_ptr]() mutable -> TaskResult
            {
                TaskResult res;
                try
                {
                    res = this->load_and_bind_impl(std::move(guids), batch, *ctx_ptr);
                }
                catch (const std::exception& ex)
                {
                    res.type = TaskResult::TaskType::Load;
                    res.add_result(Guid{}, false, ex.what());
                }
                catch (...)
                {
                    res.type = TaskResult::TaskType::Load;
                    res.add_result(Guid{}, false, "unknown exception in load_and_bind_impl");
                }

                (void)ctx_ptr->event_queue->enqueue_event(ResourceTaskCompletedEvent{ res });
                return res;
            });
    }

    std::shared_future<TaskResult>
        ResourceManager::unbind_and_unload_async(std::deque<Guid> guids, const BatchId& batch, EngineContext& ctx)
    {
        auto& s = strand(ctx);
        auto* ctx_ptr = &ctx;

        return s.submit([this, guids = std::move(guids), batch, ctx_ptr]() mutable -> TaskResult
            {
                TaskResult res;
                try
                {
                    res = this->unbind_and_unload_impl(std::move(guids), batch, *ctx_ptr);
                }
                catch (const std::exception& ex)
                {
                    res.type = TaskResult::TaskType::Unload;
                    res.add_result(Guid{}, false, ex.what());
                }

                (void)ctx_ptr->event_queue->enqueue_event(ResourceTaskCompletedEvent{ res });
                return res;
            });
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
        using Op = OperationResult;
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
#if 1
        for (const Guid& g : guids)
        {
            // Skip bind if asset failed to load
            if (failed.count(g)) continue;

            try
            {
                BindResult br{};
                {
                    auto br_any = invoke_meta_function(g, batch, ctx, literals::bind_asset_hs, "bind_asset");
                    br = br_any.cast<BindResult>();
                }

                {
                    std::lock_guard lk(status_mutex_);
                    auto& st = statuses_[g];

                    if (br.all_refs_bound)
                    {
                        st.bind_state = BindState::Bound;
                    }
                    else
                    {
                        st.bind_state = BindState::PartiallyBound;
                        // TODO st.error_message ...
                    }
                }
            }
            catch (const std::exception& ex)
            {
                // Catastrophic error, but: do *not* drop the lease
                {
                    std::lock_guard lk(status_mutex_);
                    auto& st = statuses_[g];
                    st.bind_state = BindState::Unbound;
                    //st.error_message = ex.what();
                }
                res.add_result(g, false, ex.what());
            }
        }
#else
        for (const Guid& g : guids) {
            //if (failed.count(g)) { (void)batch_release(batch, g); continue; }
            if (failed.count(g)) continue;

            try {
                // invoke_meta_function(g, batch, ctx, bind_asset_hs, "bind_asset_with_batch");
                (void)invoke_meta_function(g, batch, ctx, literals::bind_asset_hs, "bind_asset");

                res.add_result(g, true, "Bind Ok");
            }
            catch (const std::exception& ex) {
                (void)batch_release(batch, g); // roll back parent lease if bind fails
                res.add_result(g, false, ex.what());
            }
        }
#endif

        // Optional asset hook after load/bind (main thread)
        {
            auto hook_guids = collect_hook_guids(guids, failed);
            try_invoke_asset_hook_on_main(ctx, hook_guids, literals::on_create_hs, "on_create");
        }

        return res;
    }

    TaskResult ResourceManager::unbind_and_unload_impl(
        std::deque<Guid> guids,
        const BatchId& batch,
        EngineContext& ctx)
    {
        using Op = OperationResult;
        TaskResult res; res.type = TaskResult::TaskType::Unload;
        size_t destroy_hook_count = 0;

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

            try
            {
                // Optional asset hook before final unload (main thread)
                bool invoked = false;
                ctx.main_thread_queue->push_and_wait([&]()
                    {
                        invoked = try_invoke_meta_function(g, ctx, literals::on_destroy_hs, "on_destroy").has_value();
                    });
                if (invoked)
                    ++destroy_hook_count;

                this->unload_asset(g, ctx);

                std::lock_guard lk(status_mutex_);
                statuses_.erase(g);
                res.add_result(g, true, "Unbind and Unload Ok");
            }
            catch (const std::exception& ex)
            {
                std::lock_guard lk(status_mutex_);
                auto& st = statuses_[g];
                st.state = LoadState::Failed;
                st.error_message = ex.what();
                res.add_result(g, false, st.error_message);
            }
        }

        if (destroy_hook_count > 0)
        {
            EENG_LOG_INFO(&ctx, "Asset hook on_destroy complete: %zu invoked", destroy_hook_count);
        }

        return res;
    }

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

    uint32_t ResourceManager::total_leases(const Guid& g) const noexcept {
        std::lock_guard lk(lease_mutex_);
        auto it = leases_.find(g);
        return (it == leases_.end()) ? 0u : static_cast<uint32_t>(it->second.holders.size());
    }

    bool ResourceManager::held_by_any(const Guid& g) const noexcept {
        std::lock_guard lk(lease_mutex_);
        return leases_.find(g) != leases_.end();
    }

    bool ResourceManager::held_by_batch(const Guid& g, const BatchId& b) const noexcept {
        std::lock_guard lk(lease_mutex_);
        auto it = leases_.find(g);
        return it != leases_.end() && it->second.holders.count(b) != 0;
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

    std::vector<Guid> ResourceManager::find_guids_by_name(std::string_view name) const
    {
        std::vector<Guid> result;

        auto index_data = asset_index_->get_index_data();
        if (!index_data) return result;

        for (const AssetEntry& entry : index_data->entries)
        {
            if (entry.meta.name == name) result.push_back(entry.meta.guid);
        }

        return result;
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

    void ResourceManager::set_assets_root(std::filesystem::path root)
    {
        assets_root_ = std::move(root);
    }

    const std::filesystem::path& ResourceManager::assets_root() const
    {
        return assets_root_;
    }

    bool ResourceManager::unimport_assets(const std::vector<Guid>& roots, EngineContext& ctx, std::string* error_out)
    {
        if (roots.empty())
        {
            if (error_out) *error_out = "No assets selected.";
            return false;
        }

        // Policy: only dependency-tree roots are unimported; remove full dependency subtree
        // only when no assets in that subtree are leased.
        auto index_data = asset_index_->get_index_data();
        if (!index_data || !index_data->trees)
        {
            if (error_out) *error_out = "Asset tree unavailable.";
            return false;
        }

        // content_tree is the dependency tree shown in the Resource Browser.
        const auto& tree = index_data->trees->content_tree;

        // 1) Validate selection: each selected GUID must be a dependency-tree root.
        std::unordered_set<Guid> unique_roots;
        unique_roots.reserve(roots.size());
        for (const Guid& guid : roots)
        {
            if (!tree.contains(guid))
            {
                if (error_out) *error_out = "Selected asset not found in dependency tree.";
                return false;
            }

            if (!tree.is_root(guid))
            {
                if (error_out) *error_out = "Only dependency-tree roots can be unimported.";
                return false;
            }

            unique_roots.insert(guid);
        }

        if (unique_roots.empty())
        {
            if (error_out) *error_out = "No root assets selected.";
            return false;
        }

        struct RootGroup
        {
            Guid root{};
            std::vector<Guid> subtree;
            std::vector<std::filesystem::path> asset_dirs;
        };

        std::vector<RootGroup> groups;
        groups.reserve(unique_roots.size());
        std::unordered_set<Guid> all_assets;

        // 2) Build per-root groups from the dependency tree (for lease checks + folder resolution).
        for (const Guid& root : unique_roots)
        {
            RootGroup group;
            group.root = root;
            bool missing_entry = false;

            tree.traverse_breadthfirst(root, [&](const Guid& guid, size_t)
                {
                    group.subtree.push_back(guid);
                    all_assets.insert(guid);

                    auto entry_it = index_data->by_guid.find(guid);
                    if (entry_it == index_data->by_guid.end() || !entry_it->second)
                    {
                        missing_entry = true;
                        return;
                    }

                    const auto& path = entry_it->second->absolute_path;
                    if (!path.empty())
                        group.asset_dirs.push_back(path.parent_path());
                });

            if (missing_entry)
            {
                if (error_out) *error_out = "Asset entry missing for subtree.";
                return false;
            }

            groups.push_back(std::move(group));
        }

        // 3) Hard-stop if any asset in the subtree is currently leased.
        for (const Guid& guid : all_assets)
        {
            if (held_by_any(guid))
            {
                if (error_out)
                    *error_out = "Asset leased: " + guid.to_string();
                return false;
            }
        }

        // 4) Resolve each subtree's on-disk root folder and move it to .trash.
        const auto assets_root = assets_root_;
        if (assets_root.empty())
        {
            if (error_out) *error_out = "Assets root not set.";
            return false;
        }

        std::error_code ec;
        // Keep soft-deleted assets under assets_root/.trash to preserve GUIDs.
        const auto trash_root = assets_root / ".trash";
        std::filesystem::create_directories(trash_root, ec);
        if (ec)
        {
            if (error_out) *error_out = "Failed to create trash folder: " + ec.message();
            return false;
        }

        for (auto& group : groups)
        {
            // Use the shared ancestor of all files in the subtree as the folder root.
            const std::filesystem::path common_root = common_ancestor_path(group.asset_dirs);
            if (common_root.empty() || common_root == assets_root)
            {
                if (error_out) *error_out = "Failed to resolve a unique asset folder.";
                return false;
            }

            if (!std::filesystem::exists(common_root))
            {
                if (error_out) *error_out = "Asset folder not found on disk.";
                return false;
            }

            // Encode root GUID in the folder name so restore can locate it later.
            std::string dest_name = common_root.filename().string() + "_" + group.root.to_string();
            std::filesystem::path dest_path = trash_root / dest_name;
            // Avoid clobbering when a prior trash entry exists.
            if (std::filesystem::exists(dest_path))
                dest_path = trash_root / (dest_name + "_" + Guid::generate().to_string());

            std::filesystem::rename(common_root, dest_path, ec);
            if (ec)
            {
                if (error_out) *error_out = "Failed to move asset folder: " + ec.message();
                return false;
            }

            EENG_LOG_INFO(&ctx, "Unimported assets (moved to trash): %s", dest_path.string().c_str());
        }
        return true;
    }

    bool ResourceManager::restore_from_trash(const Guid& root, EngineContext& ctx, std::string* error_out)
    {
        if (!root.valid())
        {
            if (error_out) *error_out = "Invalid root GUID.";
            return false;
        }

        if (assets_root_.empty())
        {
            if (error_out) *error_out = "Assets root not set.";
            return false;
        }

        const auto trash_root = assets_root_ / ".trash";
        if (!std::filesystem::exists(trash_root))
        {
            if (error_out) *error_out = "Trash folder not found.";
            return false;
        }

        const std::string suffix = "_" + root.to_string();
        std::filesystem::path trash_match;
        size_t matches = 0;

        for (const auto& entry : std::filesystem::directory_iterator(trash_root))
        {
            if (!entry.is_directory())
                continue;

            const std::string name = entry.path().filename().string();
            if (name.ends_with(suffix))
            {
                trash_match = entry.path();
                matches++;
            }
        }

        if (matches == 0)
        {
            if (error_out) *error_out = "No trashed asset root found for GUID.";
            return false;
        }

        if (matches > 1)
        {
            if (error_out) *error_out = "Multiple trashed roots found for GUID.";
            return false;
        }

        std::string original_name = trash_match.filename().string();
        if (original_name.size() <= suffix.size())
        {
            if (error_out) *error_out = "Trash entry name invalid.";
            return false;
        }
        original_name.erase(original_name.size() - suffix.size());
        if (!original_name.empty() && original_name.back() == '_')
            original_name.pop_back();

        if (original_name.empty())
        {
            if (error_out) *error_out = "Failed to resolve original folder name.";
            return false;
        }

        const auto dest_path = assets_root_ / original_name;
        if (std::filesystem::exists(dest_path))
        {
            if (error_out) *error_out = "Restore destination already exists.";
            return false;
        }

        std::error_code ec;
        std::filesystem::rename(trash_match, dest_path, ec);
        if (ec)
        {
            if (error_out) *error_out = "Failed to restore asset folder: " + ec.message();
            return false;
        }

        EENG_LOG_INFO(&ctx, "Restored assets from trash: %s", dest_path.string().c_str());
        return true;
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

    // std::vector<Guid> ResourceManager::collect_referenced_asset_guids(const Guid& guid)
    // {
    //     std::vector<Guid> out;

    //     auto mh_opt = storage_->handle_for_guid(guid); // Guid -> MetaHandle{ofs, ver, type}
    //     if (!mh_opt || !mh_opt->valid())
    //         return out;

    //     storage_->modify(*mh_opt, [&](entt::meta_any any)
    //         {
    //             using namespace entt::literals;

    //             if (auto mf = mh_opt->type.func(literals::collect_asset_guids_hs); mf)
    //             {
    //                 mf.invoke(
    //                     {},
    //                     entt::forward_as_meta(any),
    //                     entt::forward_as_meta(out));
    //             }
    //         });

    //     // optional: dedup/filter invalid
    //     std::sort(out.begin(), out.end());
    //     out.erase(std::unique(out.begin(), out.end()), out.end());
    //     return out;
    // }

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

        const auto& type_name = it->second->meta.type_id;

        // entt::meta_type type = entt::resolve(entt::hashed_string{ type_name.c_str() });
        entt::meta_type type = meta::resolve_by_type_id_string(type_name);
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

        const auto& type_name = it->second->meta.type_id;

        // entt::meta_type type = entt::resolve(entt::hashed_string{ type_name.c_str() });
        entt::meta_type type = meta::resolve_by_type_id_string(type_name);
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

    std::optional<entt::meta_any> ResourceManager::try_invoke_meta_function(
        const Guid& guid,
        EngineContext& ctx,
        entt::hashed_string function_id,
        std::string_view function_label
    )
    {
        auto index_data = asset_index_->get_index_data();
        if (!index_data) return std::nullopt;

        auto it = index_data->by_guid.find(guid);
        if (it == index_data->by_guid.end() || !it->second)
            return std::nullopt;

        const auto& type_name = it->second->meta.type_id;

        entt::meta_type type = meta::resolve_by_type_id_string(type_name);
        if (!type) return std::nullopt;

        auto fn = type.func(function_id);
        if (!fn) return std::nullopt;

        auto result = fn.invoke({}, entt::forward_as_meta(guid), entt::forward_as_meta(ctx));
        if (!result)
        {
            if (fn.ret().id() == entt::resolve<void>().id())
                return entt::meta_any{};

            EENG_LOG_ERROR(&ctx, "%s failed for type: %s", std::string(function_label).c_str(), type_name.c_str());
            return std::nullopt;
        }

        return result;
    }

    void ResourceManager::try_invoke_asset_hook_on_main(
        EngineContext& ctx,
        const std::vector<Guid>& guids,
        entt::hashed_string hook_id,
        std::string_view label
    )
    {
        if (guids.empty())
            return;

        size_t invoked_count = 0;
        ctx.main_thread_queue->push_and_wait([&]()
            {
                for (const auto& g : guids)
                {
                    if (try_invoke_meta_function(g, ctx, hook_id, label).has_value())
                        ++invoked_count;
                }
            });

        if (invoked_count > 0)
        {
            EENG_LOG_INFO(&ctx, "Asset hook %s complete: %zu invoked",
                std::string(label).c_str(), invoked_count);
        }
    }
}
