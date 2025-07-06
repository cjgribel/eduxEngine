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
        std::unique_ptr<Storage>    storage_;
        std::unique_ptr<AssetIndex> asset_index_;

    public:
        ResourceManager(/* any ctor args */);
        // Out-of-line dtor: ensures Storage & AssetIndex are complete when deleted
        ~ResourceManager();

        const Storage& storage();

        const AssetIndex& asset_index();

        std::string to_string() const override;

        // Must be thread-safe. Use static types.
        /// @brief Import new resource to resource index
        /// @tparam T 
        /// @param t 
        /// @return 
        template<class T>
        AssetRef<T> file(T& t)
        {
            std::cout << "[ResourceManager] Filing type: " << typeid(T).name() << "\n";

            auto guid = Guid::generate();

            // AssetIndex maps asset type to a file location - must be TS
            // Use either a) templated or b) entt::meta_type
            // asset_index.serialize<T>(t, guid, ctx?);

            return AssetRef<T>{ guid, Handle<T> {} };
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