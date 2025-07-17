// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IResourceManager.hpp"
#include "EngineContext.hpp"
#include "Storage.hpp" // Can't pimpl-away since class is templated
#include "AssetIndex.hpp"
#include "ResourceTypes.h" // For AssetRef<T>, visit_assets
#include "AssetMetaData.hpp"
#include "AssetRef.hpp"
#include <string>
#include <mutex> // std::mutex - > maybe AssetIndex

// -> AssetIndex ???
#include "MetaSerialize.hpp"
#include "LogMacros.h"
#include <nlohmann/json.hpp> // <nlohmann/json_fwd.hpp>
#include <fstream>

namespace eeng
{
    /*
    Avoid exposing Storage or AssetRegistry directly.
    Instead, introduce a facade like ResourceManager or EngineAssets, that:
    Encapsulates:
    Storage
    AssetRegistry
    (maybe) a ThreadPool for loading

    This layer should be thread-safe and asynchronous-aware
    (i.e. return futures or use task submission when loading/importing).
    */

    class Storage;
    class AssetIndex;

    class ResourceManager : public IResourceManager
    {
    private:
        std::unique_ptr<Storage>    storage_;
        std::unique_ptr<AssetIndex> asset_index_;

        mutable std::mutex mutex_; // Protects asset_index_ (storage_ is thread-safe)

    public:
        ResourceManager(/* any ctor args */);
        // Out-of-line dtor: ensures Storage & AssetIndex are complete when deleted
        ~ResourceManager();

        const Storage& storage() const;
        Storage& storage();

        const AssetIndex& asset_index() const;
        AssetIndex& asset_index();

        std::string to_string() const override;

        void start_async_scan(
            const std::filesystem::path& root,
            EngineContext& ctx
        )
        {
            // std::lock_guard lock{ mutex_ };
            // ??? If concurrent - Put manager in a scanning state?

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

        bool is_scanning() const override;

        /// @brief Get a snapshot of the asset index
        /// @return std::vector<AssetEntry>
        // std::vector<AssetEntry> get_asset_entries_snapshot() const override;
        AssetIndexDataPtr get_index_data() const override;

        // Must be thread-safe. Use static types, or lock meta paths.
        /// @brief Import new resource to resource index
        /// @tparam T 
        /// @param t 
        /// @return 
        template<class T>
        void file(
            const T& t,
            const std::string& file_path,
            const AssetMetaData& meta,
            const std::string& meta_file_path)
        {
            // EENG_LOG("[ResourceManager] Filing type: %s", typeid(T).name());

            // Add contained assets
            auto _meta = meta;
            visit_assets(t, [&](const auto& ref)
                {
                    //if (ref.valid())
                    _meta.contained_assets.push_back(ref.get_guid());
                });

            asset_index_->serialize_to_file<T>(t, _meta, file_path, meta_file_path);
        }

        // TS?
        template<typename T>
        void unfile(AssetRef<T>& ref)
        {
            // For now: require that asset being removed is not loaded
            assert(!ref.is_loaded());

            // asset_index.remove(ref.guid);
            //unload(ref); // optional: remove from memory too
        }

        template<typename T>
        void load(AssetRef<T>& ref, EngineContext& ctx)
        {
            if (ref.is_loaded()) return;

            std::cout << "[ResourceManager] Loading type: " << typeid(T).name()
                << ", guid = " << ref.guid.to_string() << "\n";

            // 1. Deserialize from disk
            T asset = asset_index_->deserialize_from_file<T>(ref.guid, ctx);

            // 2. Recursively load dependencies
            visit_assets(asset, [&](auto& subref)
                {
                    load(subref, ctx); // Recursively load children
                });

            // 3. Add to storage
            ref.handle = storage_->add<T>(std::move(asset), ref.guid);
        }

        // Not thread-safe (storage->get_ref)
        template<typename T>
        void unload(AssetRef<T>& ref)
        {
            if (!ref.is_loaded()) throw std::runtime_error("Asset not loaded");

#if 1
            // TS - will NOT deadlock due to recursive mutex
            storage_->modify(ref.handle, [this](T& t)
                {
                    visit_assets(t, [this](auto& subref)
                        {
                            if (subref.is_loaded()) unload(subref);
                        });
                });
#endif
#if 0
            // NON-TS (storage->get_ref)
            T& t = storage->get_ref(ref.handle);
            visit_assets(t, [&](auto& subref)
                {
                    if (subref.is_loaded()) unload(subref);
                });
#endif

            // 3. Release the resource and clear the handle
            storage_->release(ref.handle); // TS
            ref.unload();

            std::cout << storage_->to_string() << std::endl;
        }
    };

#if 0
    // Read
    std::vector<AssetInfo> list_assets_of_type(entt::meta_type);
    AssetInfo get_asset_info(Guid guid);
    bool is_loaded(Guid guid);
    entt::meta_any get_loaded_resource(Guid guid);

    // Write
    bool import_resource(const std::filesystem::path& source);
    bool reimport_resource(Guid guid);
    Guid load_resource(Guid guid);
    void unload_resource(Guid guid);
    bool serialize_resource(Guid guid);
#endif
} // namespace eeng