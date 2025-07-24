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
            if (status.state == LoadState::Loaded || status.state == LoadState::Loading) {
                return std::async(std::launch::deferred, [] { return false; });
            }
            status.state = LoadState::Loading;
        }

        return ctx.thread_pool->queue_task([=, this, &ctx]()
            {
                try
                {
                    invoke_meta_function(guid, ctx, literals::load_asset_hs, "load_asset");
                    // invoke_meta_function(guid, ctx, literals::resolve_asset_hs, "resolve_asset");

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

    std::future<bool> ResourceManager::unload_asset_async(const Guid& guid, EngineContext& ctx)
    {
        {
            std::lock_guard lock(status_mutex_);
            auto& status = statuses_[guid];

            if (status.state != LoadState::Loaded) {
                return std::async(std::launch::deferred, [] { return false; });
            }
            status.state = LoadState::Unloading;
        }

        return ctx.thread_pool->queue_task([guid, this, &ctx]()
            {
                try
                {
                    // invoke_meta_function(guid, ctx, literals::unresolve_asset_hs, "unresolve_asset");
                    invoke_meta_function(guid, ctx, literals::unload_asset_hs, "unload_asset");

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

    // std::future<bool> ResourceManager::resolve_asset_async(const Guid& guid, EngineContext& ctx)
    // {
    //     return ctx.thread_pool->queue_task([=, this, &ctx]()
    //         {
    //             try
    //             {
    //                 invoke_meta_function(guid, ctx, literals::resolve_asset_hs, "resolve_asset");
    //                 return true;
    //             }
    //             catch (const std::exception& ex)
    //             {
    //                 std::cout << ex.what() << std::endl;
    //                 return false;
    //             }
    //         });
    // }

    std::future<bool> ResourceManager::unresolve_asset_async(const Guid& guid, EngineContext& ctx)
    {
        return ctx.thread_pool->queue_task([=, this, &ctx]() {
            try {
                // this->unbind_asset(guid, ctx);
                invoke_meta_function(guid, ctx, literals::unresolve_asset_hs, "unresolve_asset");
                return true;
            }
            catch (const std::exception& ex)
            {
                std::cout << ex.what() << std::endl;
                return false;
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
            status.ref_count = 0;
            // unload when no more references
            // unload_async(guid, ctx);
            // statuses_.erase(guid);
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



    // void ResourceManager::load(const Guid& guid, EngineContext& ctx)
    // {
    //     invoke_meta_function(guid, ctx, literals::load_asset_hs, "load_asset");
    // }

    // void ResourceManager::unload(const Guid& guid, EngineContext& ctx)
    // {
    //     invoke_meta_function(guid, ctx, literals::unload_asset_hs, "unload_asset");
    // }

    void ResourceManager::resolve_asset(const Guid& guid, EngineContext& ctx)
    {
        invoke_meta_function(guid, ctx, literals::resolve_asset_hs, "resolve_asset");
    }

    // void ResourceManager::unbind_asset(const Guid& guid, EngineContext& ctx)
    // {
    //     invoke_meta_function(guid, ctx, literals::unbind_asset_hs, "unbind_asset");
    // }

    void ResourceManager::invoke_meta_function(
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
    }
}