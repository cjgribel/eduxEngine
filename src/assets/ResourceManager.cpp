// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ResourceManager.hpp"
#include "AssetIndex.hpp"
#include "ThreadPool.hpp"
// #include <async>
// #include "Storage.hpp"



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

    std::future<bool> ResourceManager::load_asset_async(const Guid& guid, EngineContext& ctx)
    {
        {
            std::lock_guard lock(status_mutex_);
            auto& status = statuses_[guid];
            // if (status.state == LoadState::Loaded || status.state == LoadState::Loading) {
            if (status.state == LoadState::Loading) {
                return std::async(std::launch::deferred, [] { return false; });
            }
            status.state = LoadState::Loading;
        }

        return ctx.thread_pool->queue_task([=, this, &ctx]()
            {
                try
                {
                    load_asset(guid, ctx);

                    std::lock_guard lock(status_mutex_);
                    statuses_[guid].state = LoadState::Loaded;
                    return true;
                }
                catch (const std::exception& ex)
                {
                    std::lock_guard lock(status_mutex_);
                    statuses_[guid].state = LoadState::Failed;
                    statuses_[guid].error_message = ex.what();
                    return false;
                }
            });
    }

    //
    // NEW
    //
    std::future<void> ResourceManager::load_branch_async(
        std::deque<Guid> branch_guids,  // bottom-up order
        EngineContext& ctx)
    {
        // Use shared_ptr to safely share future list with the final join task
        auto futures = std::make_shared<std::vector<std::shared_future<bool>>>();

        auto begin_load_status = [&](const Guid& guid)
            {
                std::lock_guard lock(status_mutex_);
                auto& status = statuses_[guid];
                if (status.state == LoadState::Loading || status.state == LoadState::Loaded)
                    return false;

                status.state = LoadState::Loading;
                return true;
            };

        for (const Guid& guid : branch_guids)
        {
            futures->emplace_back(
                ctx.thread_pool->queue_task([=, this, &ctx]() -> bool
                    {
                        if (!begin_load_status(guid)) return false;  // Already loading or loaded

                        try {
                            this->load_asset(guid, ctx);  // no recursion inside
                            std::lock_guard lock(status_mutex_);
                            statuses_[guid].state = LoadState::Loaded;
                            return true;
                        }
                        catch (const std::exception& ex) {
                            std::lock_guard lock(status_mutex_);
                            statuses_[guid].state = LoadState::Failed;
                            statuses_[guid].error_message = ex.what();
                            return false;
                        }
                    })
            );
        }

        // Join task: wait for all, then resolve in top-down order
        return ctx.thread_pool->queue_task([=, this, &ctx]() {
            for (auto& f : *futures)
                f.get(); // wait for all loads to complete

            for (const Guid& guid : branch_guids | std::views::reverse) {
                // std::cout << guid.to_string() << " is valid: " << storage_->handle_for_guid(guid).has_value() << std::endl;
                this->resolve_asset(guid, ctx);
            }
            });
    }

    //
    // NEW
    //

    std::future<void> ResourceManager::unload_branch_async(
        std::deque<Guid> branch_guids,
        EngineContext& ctx)
    {
        // Step 1: Unresolve assets in the branch (before launching unloads)
        for (const Guid& guid : branch_guids)
        {
            // std::lock_guard lock(status_mutex_);
            // auto& status = statuses_[guid];
            // if (status.state == LoadState::Loaded)
                unresolve_asset(guid, ctx); // drops references, decrements ref counts
        }

        // Step 2: Launch unloads (if ref_count == 0)
        auto futures = std::make_shared<std::vector<std::future<bool>>>();

        for (const Guid& guid : branch_guids)
        {
            {
                std::lock_guard lock(status_mutex_);
                auto& status = statuses_[guid];

                // Only unload if it’s loaded and no one else is using it
                if (status.state != LoadState::Loaded || status.ref_count > 0)
                {
                    continue; // skip it
                }

                status.state = LoadState::Unloading;
            }

            futures->emplace_back(ctx.thread_pool->queue_task([=, this, &ctx]() {
                try {
                    this->unload_asset(guid, ctx);  // dispatches to unload<T>
                    std::lock_guard lock(status_mutex_);
                    statuses_.erase(guid); // completely gone
                    return true;
                }
                catch (const std::exception& ex) {
                    std::lock_guard lock(status_mutex_);
                    statuses_[guid].state = LoadState::Failed;
                    statuses_[guid].error_message = ex.what();
                    return false;
                }
                }));
        }

        // Step 3: Wait for all unloads to complete
        return ctx.thread_pool->queue_task([futures]() {
            for (auto& f : *futures)
                f.get(); // safe to call now
            });
    }


    std::future<bool> ResourceManager::unload_asset_async(const Guid& guid, EngineContext& ctx)
    {
        {
            std::lock_guard lock(status_mutex_);
            auto& status = statuses_[guid];

            if (status.state == LoadState::Loading /*|| status.ref_count > 0*/) {
                return std::async(std::launch::deferred, [] { return false; });
            }
            status.state = LoadState::Unloading;
        }

        return ctx.thread_pool->queue_task([guid, this, &ctx]()
            {
                try
                {
                    unload_asset(guid, ctx);

                    std::lock_guard lock(status_mutex_);
                    statuses_.erase(guid);
                    return true;
                }
                catch (const std::exception& ex)
                {
                    std::lock_guard lock(status_mutex_);
                    statuses_[guid].state = LoadState::Failed;
                    statuses_[guid].error_message = ex.what();
                    return false;
                }
            });
    }

    void ResourceManager::unload_unbound_assets_async(EngineContext& ctx)
    {
        std::vector<Guid> to_unload;
        {
            std::lock_guard lock(status_mutex_);
            for (const auto& [guid, status] : statuses_)
            {
                if (status.ref_count == 0 && status.state == LoadState::Loaded)
                    to_unload.push_back(guid);
            }
        }
        for (const auto& guid : to_unload)
        {
            unload_asset_async(guid, ctx);
        }
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

    AssetIndexDataPtr ResourceManager::get_index_data() const
    {
        return asset_index_->get_index_data();
    }

    std::string ResourceManager::to_string() const
    {
        return storage_->to_string();
    }

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
    std::future<void> ResourceManager::reload_asset_async(const Guid& guid, EngineContext& ctx)
    {
        {
            std::lock_guard lock(status_mutex_);
            statuses_[guid].error_message.clear();
        }

        return ctx.thread_pool->queue_task([=, this, &ctx]() {
            try
            {
                this->unload_asset(guid, ctx);
                this->load_asset(guid, ctx);

                // Wait for unload to complete inside thread
                // this->unload_asset_async(guid, ctx).get();

                // Wait for load to complete
                // this->load_asset_async(guid, ctx).get();

                std::lock_guard lock(status_mutex_);
                statuses_[guid].state = LoadState::Loaded;
            }
            catch (const std::exception& ex)
            {
                std::lock_guard lock(status_mutex_);
                statuses_[guid].state = LoadState::Failed;
                statuses_[guid].error_message = ex.what();
            }
            });
    }

    void ResourceManager::resolve_asset(const Guid& guid, EngineContext& ctx)
    {
        invoke_meta_function(guid, ctx, literals::resolve_asset_hs, "resolve_asset");
    }

    void ResourceManager::unresolve_asset(const Guid& guid, EngineContext& ctx)
    {
        invoke_meta_function(guid, ctx, literals::unresolve_asset_hs, "unresolve_asset");
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