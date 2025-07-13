// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IResourceManager.hpp"
#include "EngineContext.hpp"
#include "Storage.hpp" // Can't pimpl-away since class is templated
#include "AssetIndex.hpp"
#include "ResourceTypes.h" // For AssetRef<T>, visit_asset_refs
#include "AssetMetaData.hpp"
#include "AssetRef.hpp"
#include <string>
#include <mutex> // std::mutex - > maybe AssetIndex

// -> AssetIndex ???
#include "MetaSerialize.hpp"
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

        std::mutex mutex_; // Protects asset_index_ (storage_ is thread-safe)

    public:
        ResourceManager(/* any ctor args */);
        // Out-of-line dtor: ensures Storage & AssetIndex are complete when deleted
        ~ResourceManager();

        const Storage& storage();

        const AssetIndex& asset_index();

        std::string to_string() const override;

        void scan_assets(
            const std::filesystem::path& root,
            EngineContext& ctx
        )
        {
            std::lock_guard lock{ mutex_ };
            // ??? If concurrent - Put manager in a scanning state?

            std::cout << "[ResourceManager] Scanning assets in: " << root.string() << "\n";
            auto asset_index = asset_index_->scan_meta_files(root, ctx);

            // Print asset info
            for (const auto& asset_meta : asset_index)
            {
                std::cout << "Asset: " << asset_meta.name
                          << ", GUID: " << asset_meta.guid.raw()
                          << ", Type: " << asset_meta.type_name
                          << ", File: " << asset_meta.file_path << "\n";
            }
        }

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
            std::lock_guard lock{ mutex_ };
            std::cout << "[ResourceManager] Filing type: " << typeid(T).name() << "\n";

            // auto guid = Guid::generate();
            // auto ref = AssetRef<T>{ guid, Handle<T> {} };

            // AssetIndex maps asset type to a file location - must be TS
            // Use either a) templated or b) entt::meta_type
            // asset_index.serialize<T>(t, guid, ctx?);

            // Serialize
            // CONCURRENT?
            std::ofstream data_file(file_path), meta_file(meta_file_path);
            assert(data_file.is_open() && "Failed to open file for writing");
            assert(meta_file.is_open() && "Failed to open meta file for writing");

            // NOT CONCURRENT
            nlohmann::json j_data, j_meta;
            // j_asset_meta["guid"] = guid.raw(); // Store as string
            // j_meta["type"] = std::string(entt::resolve<T>().info().name());
            // j_meta["data"] = meta::serialize_any(entt::forward_as_meta(t));
            j_data = meta::serialize_any(entt::forward_as_meta(t));
            j_meta = meta::serialize_any(entt::forward_as_meta(meta));
            // CONCURRENT?
            data_file << j_data.dump(4);
            meta_file << j_meta.dump(4);

            // return ref;
            // ^ handle is empty until asset is loaded
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

        // TS (storage->add)
        /// @brief Load an asset from disk to storage
        template<class T>
        void load(AssetRef<T>& ref, EngineContext& ctx)
        {
            assert(!ref.is_loaded());

            std::cout << "[ResourceManager] Loading of type: " << typeid(T).name() << "\n";
            //std::cout << "  ↳ guid = " << ref.guid.raw().to_string() << "\n";

            // Deserialize
            T t{}; // = deserialize<T>(ref.guid, *ctx);
            visit_asset_refs(t, [&](auto& subref) { load(subref, ctx); });
            // ^ not part of storage yet so can ignore threading issues

            // Add to storage and set handle
            ref.handle = storage_->add<T>(t, ref.guid);
            // ref.load(storage->add<T>(t, ref.guid));

            // Debug assets
            AssetRef<mock::Mesh> int_ref1{ Guid::generate(), Handle<mock::Mesh>{} };
            storage_->add<mock::Mesh>({}, int_ref1.guid);
            AssetRef<mock::MockResource1> int_ref2{ Guid::generate(), Handle<mock::MockResource1>{} };
            storage_->add<mock::MockResource1>({}, int_ref2.guid);
            AssetRef<mock::MockResource2> int_ref3{ Guid::generate(), Handle<mock::MockResource2>{} };
            storage_->add<mock::MockResource2>({}, int_ref3.guid);

            std::cout << storage_->to_string() << std::endl;
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
                    visit_asset_refs(t, [this](auto& subref)
                        {
                            if (subref.is_loaded()) unload(subref);
                        });
                });
#endif
#if 0
            // NON-TS (storage->get_ref)
            T& t = storage->get_ref(ref.handle);
            visit_asset_refs(t, [&](auto& subref)
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