// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IResourceManager.hpp"
#include "EngineContext.hpp"
#include "Storage.hpp" // Can't pimpl-away since class is templated

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
        std::unique_ptr<Storage>    storage;
        std::unique_ptr<AssetIndex> asset_index;

    public:
        ResourceManager(/* any ctor args */);
        // Out-of-line dtor: ensures Storage & AssetIndex are complete when deleted
        ~ResourceManager();

        // NOTE: The main reason to use static types here is to support CONCURRENCY
        /// @brief Import new resource to resource index
        /// @tparam T 
        /// @param t 
        /// @return 
        template<class T>
        AssetRef<T> file(T& t)
        {
            auto guid = Guid::generate();

            // AssetIndex maps asset type to a file location
            // Use either a) templated or b) entt::meta_type
            // + ~ asset_index.serialize<T>(t, guid, ctx?);
/*
            {
                "guid": "acdb01b9-f34e-4c68-818a-98eabc22f54e",
                    "resource" : {
                    "name": "TestModel",
                        "meshes" : [
                    { "guid": "mesh1" },
                    { "guid": "mesh2" }
                        ]
                }
            }
*/

            return AssetRef<T>{ guid, Handle<T> {} }; // Handle is empty until asset is loaded
        }

        // + unfile

        /// @brief Load an asset from disk to storage
        template<class T>
        void load(AssetRef<T>& asset, EngineContext& ctx)
        {
            // CHECK: asset.handle == null
            // CHECK: asset-handle is not already loaded to storage

            // Deserialize
            // AssetRef will deserialize recursively
            T t{}; // <- deserialized

            // Add to storage and set handle
            asset.handle = storage->add_typed<T>(t, asset.guid); // could take AssetRef

            std::cout << storage->to_string() << std::endl;
        }

        // template<typename T>
        // void ResourceManager::unload(AssetRef<T>& ref)
        // {
        //     if (ref.is_loaded())
        //     {
        //         storage.remove<T>(ref.handle);
        //         ref.unload();
        //     }
        // }
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