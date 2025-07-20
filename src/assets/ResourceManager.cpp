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

    AssetStatus ResourceManager::get_status(const Guid& guid) const
    {
        std::lock_guard lock(status_mutex_);
        auto it = statuses_.find(guid);
        if (it != statuses_.end())
            return it->second;
        return AssetStatus{};
    }

    std::future<void> ResourceManager::load_async(const Guid& guid, EngineContext& ctx)
    {
        // {
        //     std::lock_guard lock(status_mutex_);
        //     auto& status = statuses_[guid];

        //     if (status.state == LoadState::Loaded || status.state == LoadState::Loading)
        //     {
        //         ++status.ref_count;
        //         return std::async(std::launch::deferred, [] {});
        //     }
        //     status.state = LoadState::Loading;
        //     status.ref_count = 1;
        // }

        return ctx.thread_pool->queue_task([=, this, &ctx]() {
            try {
                // std::this_thread::sleep_for(std::chrono::milliseconds(500)); // DELAY
                this->load(guid, ctx); // loads recursively
                // std::lock_guard lock(status_mutex_);
                // statuses_[guid].state = LoadState::Loaded;
            }
            catch (const std::exception& ex) {
                std::lock_guard lock(status_mutex_);
                statuses_[guid].state = LoadState::Failed;
                statuses_[guid].error_message = ex.what();
            }
            });
    }

    std::future<void> ResourceManager::unload_async(const Guid& guid, EngineContext& ctx)
    {
        // {
        //     std::lock_guard lock(status_mutex_);
        //     auto& status = statuses_[guid];

        //     if (status.ref_count == 0) {
        //         EENG_LOG(&ctx, "unload_async: ref_count already 0");
        //         return std::async(std::launch::deferred, [] {});
        //     }

        //     if (status.state == LoadState::Loading) {
        //         EENG_LOG(&ctx, "unload_async: can't unload asset while it's loading");
        //         return std::async(std::launch::deferred, [] {});
        //     }

        //     if (--status.ref_count > 0) {
        //         return std::async(std::launch::deferred, [] {});
        //     }
        // }

        return ctx.thread_pool->queue_task([guid, this, &ctx]() {
            try {
                // std::this_thread::sleep_for(std::chrono::milliseconds(500)); // DELAY
                this->unload(guid, ctx); // unloads recursively
                // std::lock_guard lock(status_mutex_);
                // statuses_.erase(guid); // or set state = Unloaded
            }
            catch (const std::exception& ex) {
                std::lock_guard lock(status_mutex_);
                statuses_[guid].state = LoadState::Failed;
                statuses_[guid].error_message = ex.what();
            }
            });
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
            // unload when no more references
            // unload_async(guid, ctx);
            statuses_.erase(guid);
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

    ResourceManager::~ResourceManager() = default;

    void ResourceManager::load(const Guid& guid, EngineContext& ctx)
    {
        if (storage_->handle_for_guid(guid))
            throw std::runtime_error("Asset already loaded for " + guid.to_string());

        // AssetEntry const* ResourceManager::entry_for_guid(const Guid & guid) const
        // {
        //     auto index_data = asset_index_->get_index_data();
        //     auto it = index_data->by_guid.find(guid);
        //     return (it != index_data->by_guid.end()) ? it->second : nullptr;
        // }
        // auto entry = entry_for_guid(guid);
        // if (!entry) throw ...;
        // auto type = entt::resolve(...);
        // auto fn = type.func(...);

        auto index_data = asset_index_->get_index_data();

        auto it = index_data->by_guid.find(guid);
        if (it == index_data->by_guid.end() || !it->second)
            throw std::runtime_error("Asset not found for GUID " + guid.to_string());

        const auto& type_name = it->second->meta.type_name;

        entt::meta_type type = entt::resolve(entt::hashed_string{ type_name.c_str() });
        if (!type)
            throw std::runtime_error("Type not registered: " + type_name);

        auto load_fn = type.func(literals::load_asset_hs);
        if (!load_fn)
            throw std::runtime_error("load_asset function not registered for type: " + type_name);

        // invoke registered load_asset function (must be provided for each asset type)
        auto res = load_fn.invoke({}, entt::forward_as_meta(guid), entt::forward_as_meta(ctx));
        assert(res);
    }

    void ResourceManager::unload(const Guid& guid, EngineContext& ctx)
    {
        if (!storage_->handle_for_guid(guid))
            return; // Already unloaded, no-op

        auto index_data = asset_index_->get_index_data();

        auto it = index_data->by_guid.find(guid);
        if (it == index_data->by_guid.end() || !it->second)
            throw std::runtime_error("Asset not found for GUID " + guid.to_string());

        const auto& type_name = it->second->meta.type_name;

        entt::meta_type type = entt::resolve(entt::hashed_string{ type_name.c_str() });
        if (!type)
            throw std::runtime_error("Type not registered: " + type_name);

        auto unload_fn = type.func(literals::unload_asset_hs);
        if (!unload_fn)
            throw std::runtime_error("unload_asset function not registered for type: " + type_name);

        auto res = unload_fn.invoke({}, entt::forward_as_meta(guid), entt::forward_as_meta(ctx));
        assert(res && "Failed to invoke unload_asset");
    }
}