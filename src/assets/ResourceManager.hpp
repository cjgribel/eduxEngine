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

        // mutable std::mutex mutex_; // Protects asset_index_ (storage_ is thread-safe)

        mutable std::mutex status_mutex_;
        std::unordered_map<Guid, AssetStatus> statuses_;

    public:
        ResourceManager(/* any ctor args */);
        // Out-of-line dtor: ensures Storage & AssetIndex are complete when deleted
        ~ResourceManager();

#if 1
        AssetStatus get_status(const Guid& guid) const override;

        std::future<void> load_async(const Guid& guid, EngineContext& ctx) override;
        std::future<void> unload_async(const Guid& guid, EngineContext& ctx) override;

        std::future<void> bind_asset_async(const Guid& guid, EngineContext& ctx);
        std::future<void> unbind_asset_async(const Guid& guid, EngineContext& ctx);

        void retain_guid(const Guid& guid) override;
        void release_guid(const Guid& guid, EngineContext& ctx) override;
#endif

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



        // --- Typed file / unfile ---------------------------------------------

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

        // --- Typed load / unload ---------------------------------------------

        template<typename T>
        void load(AssetRef<T>& ref, EngineContext& ctx)
        {
            // if (ref.is_loaded()) //return;
            //     throw std::runtime_error("Asset already loaded for " + guid.to_string());

            const Guid& guid = ref.get_guid();

            {
                std::lock_guard lock(status_mutex_);
                auto& status = statuses_[guid];

                if (status.state == LoadState::Loaded || status.state == LoadState::Loading) {
                    ++status.ref_count;
                    //ref.handle = storage_->handle_for_guid(guid).value().cast<T>().value(); // Resolve existing handle
                    // THIS HANDLE ONLY CAPTURED BY A TEMP IN the load_asset<> meta func.
                    ref.handle = storage_->handle_for_guid<T>(guid).value(); // Resolve existing handle
                    return;
                }

                status.state = LoadState::Loading;
                status.ref_count = 1;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            T asset = asset_index_->deserialize_from_file<T>(guid, ctx);

#if 1
            // Async
            std::vector<std::future<void>> futures;
            visit_assets(asset, [&](auto& subref) {
                futures.emplace_back(this->load_async(subref.guid, ctx));
                });
            // Wait for all children to finish loading
            for (auto& f : futures) f.get();

            // Re-bind resolved handles back into the subrefs
            // TODO: can we use bind_refs ?
            visit_assets(asset, [&](auto& subref)
                {
                    // using SubT = typename std::decay_t<decltype(subref.handle)>::value_type;
                    using SubT = decltype(subref.handle)::value_type;

                    auto typed_handle = storage_->handle_for_guid<SubT>(subref.guid);
                    if (!typed_handle)
                        throw std::runtime_error("Failed to resolve handle for: " + subref.guid.to_string());

                    subref.handle = *typed_handle;
                });
#else
            // Serial
            visit_assets(asset, [&](auto& subref)
                {
                    load(subref, ctx); // Recursively load children
                });
#endif

            ref.handle = storage_->add<T>(std::move(asset), guid);

            {
                std::lock_guard lock(status_mutex_);
                statuses_[guid].state = LoadState::Loaded;
            }
        }

        template<typename T>
        void unload(AssetRef<T>& ref, EngineContext& ctx)
        {
            const Guid& guid = ref.get_guid();
            bool should_unload = false;

            {
                std::lock_guard lock(status_mutex_);
                auto& status = statuses_[guid];

                if (status.state == LoadState::Loading)
                    return; // Cannot unload while loading

                if (status.ref_count == 0)
                    return; // Already unloaded

                if (--status.ref_count == 0) {
                    status.state = LoadState::Unloading;
                    should_unload = true;
                }
            }

            if (!should_unload)
                return;

            if (ref.is_loaded()) // ???
            {
#if 1
                std::vector<std::future<void>> futures;
                storage_->modify(ref.handle, [&](T& asset) {
                    visit_assets(asset, [&](auto& subref) {
                        futures.emplace_back(this->unload_async(subref.guid, ctx));
                        });
                    });
                for (auto& f : futures) f.get();
#else
                // Serial
                storage_->modify(ref.handle, [&](T& asset) {
                    visit_assets(asset, [&](auto& subref) {
                        unload(subref, ctx); // Recursively unload children
                        });
                    });
#endif

                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                storage_->remove_now(ref.handle);
                ref.unload();
            }

            {
                std::lock_guard lock(status_mutex_);
                statuses_.erase(guid); // Optional: clean up status entry
            }
        }

        // -----

        template<class T>
        void bind_asset(const Guid& guid, EngineContext& ctx)
        {
            auto maybe_ref = ref_for_guid<T>(guid);
            if (!maybe_ref)
                throw std::runtime_error("Asset not loaded: " + guid.to_string());

            std::vector<std::future<void>> futures;

            storage_->modify(maybe_ref->handle, [&](T& asset)
                {
                    visit_assets(asset, [&](auto& asset_ref)
                        {
                            using SubT = decltype(asset_ref.handle)::value_type;

                            auto typed_handle = storage_->handle_for_guid<SubT>(asset_ref.guid);
                            if (!typed_handle)
                                throw std::runtime_error("Failed to resolve handle for: " + asset_ref.guid.to_string());

                            asset_ref.handle = *typed_handle;

                            // Retain and recursively bind child
                            // retain_guid(asset_ref.guid);
                            futures.emplace_back(this->bind_asset_async(asset_ref.guid, ctx));
                        });
                });

            for (auto& f : futures) f.get(); // Wait for children to finish

            retain_guid(guid);
        }

        template<class T>
        void unbind_asset(const Guid& guid, EngineContext& ctx)
        {
            auto maybe_ref = ref_for_guid<T>(guid);
            if (!maybe_ref)
                return;

            std::vector<std::future<void>> futures;

            storage_->modify(maybe_ref->handle, [&](T& asset)
                {
                    visit_assets(asset, [&](auto& asset_ref)
                        {
                            // Clear handle (optional)
                            asset_ref.handle = {};

                            // Recurse
                            futures.emplace_back(this->unbind_asset_async(asset_ref.guid, ctx));

                            // Release
                            // release_guid(asset_ref.guid, ctx);
                        });
                });

            for (auto& f : futures) f.get();

            release_guid(guid, ctx);
        }

#if 0
        // -----

// Probably never needed – assets are initially loaded on a meta basis
        template<typename T>
        void load_async(AssetRef<T>& ref, EngineContext& ctx)
        {
            const Guid guid = ref.guid;

            {
                std::lock_guard lock(status_mutex_);
                auto& status = statuses_[guid];
                if (status.state == LoadState::Loaded || status.state == LoadState::Loading)
                    return;
                status.state = LoadState::Loading;
            }

            ctx.thread_pool->queue_task([=, &ref, this]() mutable {
                try {
                    if (!ref.is_loaded()) {
                        // This internally populates handle
                        this->load(ref, ctx);
                    }

                    std::lock_guard lock(status_mutex_);
                    statuses_[guid].state = LoadState::Loaded;
                }
                catch (const std::exception& ex) {
                    std::lock_guard lock(status_mutex_);
                    statuses_[guid].state = LoadState::Failed;
                    statuses_[guid].error_message = ex.what();
                }
        });
    }
#endif

        // --- Meta based load / unload ----------------------------------------

    private:
        void load(const Guid& guid, EngineContext& ctx);
        void unload(const Guid& guid, EngineContext& ctx);

        void bind_asset(const Guid& guid, EngineContext& ctx);
        void unbind_asset(const Guid& guid, EngineContext& ctx);

        // --- Helpers ---------------------------------------------------------

    public: // <- make private
        template<typename T>
        std::optional<AssetRef<T>> ref_for_guid(const Guid& guid)
        {
            if (auto meta_handle = storage_->handle_for_guid(guid)) {
                if (auto handle = meta_handle->cast<T>())
                    return AssetRef<T>{ guid, * handle };
            }
            return std::nullopt;
        }

};
} // namespace eeng