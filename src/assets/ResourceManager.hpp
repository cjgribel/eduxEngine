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
#include "MetaLiterals.h" // load_asset_hs, unload_asset_hs
#include <string>
#include <mutex> // std::mutex - > maybe AssetIndex

// -> AssetIndex ???
#include "MetaSerialize.hpp"
#include "LogMacros.h"
#include <nlohmann/json.hpp> // <nlohmann/json_fwd.hpp>
#include <fstream>

namespace eeng
{
    class Storage;
    class AssetIndex;

    class ResourceManager : public IResourceManager
    {
    private:
        std::unique_ptr<Storage>    storage_;
        std::unique_ptr<AssetIndex> asset_index_;

        mutable std::mutex status_mutex_;
        std::unordered_map<Guid, AssetStatus> statuses_;

    public:
        ResourceManager();
        ~ResourceManager();

        AssetStatus get_status(const Guid& guid) const override;

        std::future<bool> load_asset_async(const Guid& guid, EngineContext& ctx) override;
        std::future<bool> unload_asset_async(const Guid& guid, EngineContext& ctx) override;

    private:
        // std::future<bool> resolve_asset_async(const Guid& guid, EngineContext& ctx);
        // std::future<bool> unresolve_asset_async(const Guid& guid, EngineContext& ctx);

        void retain_guid(const Guid& guid) override;
        void release_guid(const Guid& guid, EngineContext& ctx) override;

    public:

        bool is_scanning() const override;

        /// @brief Get a snapshot of the asset index
        /// @return std::vector<AssetEntry>
        // std::vector<AssetEntry> get_asset_entries_snapshot() const override;
        AssetIndexDataPtr get_index_data() const override;

        std::string to_string() const override;

        const Storage& storage() const;
        Storage& storage();

        const AssetIndex& asset_index() const;
        AssetIndex& asset_index();

        void start_async_scan(const std::filesystem::path& root, EngineContext& ctx);

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

            // Add contained assets, note that t is const
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
        void load_asset(const Guid& guid, EngineContext& ctx)
        {
            // Throw if already loaded
            // if (storage_->handle_for_guid<T>(guid))
            //     throw std::runtime_error("Asset already loaded for " + guid.to_string());
            if (storage_->handle_for_guid<T>(guid))
            {
                // Already loaded — just ensure resolution
                resolve_asset<T>(AssetRef<T>{ guid, * storage_->handle_for_guid<T>(guid) }, ctx);
                return;
            }

            // Derserialize
            /* add delay */ std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            T asset = asset_index_->deserialize_from_file<T>(guid, ctx);

            // Async load child assets
            std::vector<std::future<bool>> futures;
            visit_assets(asset, [&](auto& subref)
                {
                    futures.emplace_back(this->load_asset_async(subref.guid, ctx));
                });
            // Check results
            for (auto& f : futures)
            {
                if (!f.get()) // Child failed to load
                    throw std::runtime_error("One or more dependencies failed to load for asset: " + guid.to_string());
            }

            // Add to storage
            // The handle is fetched when the asset is bound
            auto handle = storage_->add<T>(std::move(asset), guid);

            // -- resolve now???  --
            // resolve_asset<T>(guid, ctx);
            resolve_asset(AssetRef<T> {guid, handle}, ctx);
        }

        template<typename T>
        void unload_asset(const Guid& guid, EngineContext& ctx)
        {
            auto maybe_handle = storage_->handle_for_guid<T>(guid);
            if (!maybe_handle) {
                //unresolve_asset(AssetRef<T> {guid, handle}, ctx);
                return;
            }
            //throw std::runtime_error("Asset not loaded for " + guid.to_string());
            auto handle = *maybe_handle;

            // unresolve_asset<T>(guid, ctx);
            unresolve_asset(AssetRef<T> {guid, handle}, ctx);

            // Async unload child assets
            std::vector<std::future<bool>> futures;
            storage_->modify(handle, [&](T& asset)
                {
                    //if (!storage_->validate(handle)) return;
                    visit_assets(asset, [&](auto& subref)
                        {
                            futures.emplace_back(this->unload_asset_async(subref.guid, ctx));
                        });
                });
            // Check results
            for (auto& f : futures)
            {
                if (!f.get()) // Child failed to load
                    throw std::runtime_error("One or more dependencies failed to unload for asset: " + guid.to_string());
            }

            /* add delay */ std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            storage_->remove_now(handle);
        }

        std::future<void> reload_asset_async(const Guid& guid, EngineContext& ctx);

        void unload_unbound_assets_async(EngineContext& ctx);

    private:

        template<class T>
        void resolve_asset(AssetRef<T> ref, EngineContext& ctx)
        {
            storage_->modify(ref.handle, [&](T& asset)
                {
                    visit_assets(asset, [&](auto& ref)
                        {
                            if (storage_->validate(ref.handle)) return;

                            using CT = decltype(ref.handle)::value_type;
                            auto handle = storage_->handle_for_guid<CT>(ref.guid);
                            if (!handle)
                                throw std::runtime_error("Failed to resolve handle for: " + ref.guid.to_string());

                            ref.handle = handle.value();
                            retain_guid(ref.guid);
                            resolve_asset(ref, ctx);
                        });
                });
        }

    public:

        // Called via meta
        template<class T>
        void resolve_asset(const Guid& guid, EngineContext& ctx)
        {
            auto maybe_ref = ref_for_guid<T>(guid);
            if (!maybe_ref)
                throw std::runtime_error("Asset not loaded: " + guid.to_string());
            resolve_asset<T>(maybe_ref.value(), ctx);
        }

        // template<class T>
        // void rebind_assets()
        // {
        //     // unbind all
        //     unresolve_asset()

        //     // bind all
        // }

    private:
        template<class T>
        void unresolve_asset(AssetRef<T> ref, EngineContext& ctx)
        {
            if (!storage_->validate(ref.handle)) return;
            storage_->modify(ref.handle, [&](T& asset)
                {
                    visit_assets(asset, [&](auto& ref)
                        {
                            unresolve_asset(ref, ctx);
                            release_guid(ref.guid, ctx);
                            ref.handle = {};
                        });
                });
        }

    public:
        template<class T>
        void unresolve_asset(const Guid& guid, EngineContext& ctx)
        {
            auto maybe_ref = ref_for_guid<T>(guid);
            if (!maybe_ref)
                throw std::runtime_error("Asset not loaded: " + guid.to_string());
            unresolve_asset<T>(maybe_ref.value(), ctx);
        }

        // ---

        // IResourceManager?
        bool validate_asset(const Guid& guid, EngineContext& ctx);
        bool validate_asset_recursive(const Guid& guid, EngineContext& ctx);

        template<class T>
        bool validate_asset(const Handle<T>& handle)
        {
            return storage_->validate(handle);
        }

        template<class T>
        bool validate_asset(const Guid& guid)
        {
            if (auto handle_opt = storage_->handle_for_guid<T>(guid))
                return true;
            return false;
        }

        template<class T>
        bool validate_asset_recursive(const Handle<T>& handle)
        {
            bool valid = validate_asset(handle);
            if (!valid) return false;

            storage_->modify(handle, [&](const T& asset)
                {
                    visit_assets(asset, [&](const auto& asset_ref)
                        {
                            using SubT = typename std::decay_t<decltype(asset_ref.handle)>::value_type;
                            // valid &= validate_asset_recursive<SubT>(asset_ref.handle);
                            if (!validate_asset_recursive<SubT>(asset_ref.handle))
                                valid = false;

                        });
                });
            return valid;
        }

        template<class T>
        bool validate_asset_recursive(const Guid& guid)
        {
            if (auto handle_opt = storage_->handle_for_guid<T>(guid))
                return validate_asset_recursive(handle_opt.value());
            return false;
        }

    private:
        void load_asset(const Guid& guid, EngineContext& ctx);
        void unload_asset(const Guid& guid, EngineContext& ctx);

        void resolve_asset(const Guid& guid, EngineContext& ctx);
        void unresolve_asset(const Guid& guid, EngineContext& ctx);

        // --- Helpers ---------------------------------------------------------

    //public: // <- make private
        template<typename T>
        std::optional<AssetRef<T>> ref_for_guid(const Guid& guid)
        {
            if (auto handle_opt = storage_->handle_for_guid<T>(guid))
                return AssetRef<T>{ guid, handle_opt.value() };
            return std::nullopt;
        }

        entt::meta_any invoke_meta_function(
            const Guid& guid,
            EngineContext& ctx,
            entt::hashed_string function_id,
            std::string_view function_label
        );

    };
} // namespace eeng