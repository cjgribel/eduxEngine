// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IResourceManager.hpp"
#include "SerialExecutor.hpp"
#include "EngineContext.hpp"
#include "Storage.hpp" // Can't pimpl-away since class is templated
#include "AssetIndex.hpp"
#include "ResourceTypes.hpp" // For AssetRef<T>, visit_asset_refs
#include "AssetMetaData.hpp"
#include "AssetRef.hpp"
#include "MetaLiterals.h" // load_asset_hs, unload_asset_hs
#include "LogMacros.h"
#include <string>
#include <mutex> // std::mutex - > maybe AssetIndex

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

        // TODO: not needed if tasks are serial ?
        mutable std::mutex scan_mutex_; // serialize overlapping scans

        // Serial task queue
        std::optional<SerialExecutor>   rm_strand_;   // lazy initialization
        mutable std::mutex              strand_mutex_;
        SerialExecutor& strand(EngineContext& ctx);   // helper

        // Asset lease tracking
        struct AssetLease 
        {
            std::unordered_set<BatchId> holders; // batches that currently hold this asset
        };
        mutable std::mutex lease_mutex_;
        std::unordered_map<Guid, AssetLease> leases_;

        /// @brief Idempotently acquire a batch of assets
        /// @param batch_id Batch ID
        /// @param g Asset GUID
        /// @return True if the batch was successfully acquired
        bool batch_acquire(const BatchId& bid, const Guid& g)
        {
            std::lock_guard lk(lease_mutex_);
            auto& holders = leases_[g].holders;
            // insert returns {iterator, inserted}; inserted == true if it wasn't present
            //const bool inserted = holders.insert(bid).second;
            // (Optionally erase empty AssetLease map entries elsewhere; not needed here.)
            //return inserted; // true means "this batch newly acquired the asset"
            return holders.insert(bid).second;
        }

        bool batch_release(const BatchId& bid, const Guid& g)
        {
            std::lock_guard lk(lease_mutex_);
            auto it = leases_.find(g);
            if (it == leases_.end()) return false;

            auto& holders = it->second.holders;
            const size_t erased = holders.erase(bid); // 0 if this batch didn’t hold it
            if (holders.empty()) {
                leases_.erase(it);
                return erased > 0;   // true = this release made it drop to zero
            }
            return false;            // still held by other batches
        }

        // Asset-GUID Striping
        // static constexpr size_t kStripeCount = 64; // or 128
        // mutable std::array<std::mutex, kStripeCount> guid_stripes_;
        // size_t stripe_of(const Guid& g) const noexcept { return std::hash<Guid>{}(g)& (kStripeCount - 1); }
        // std::mutex& mutex_for(const Guid& g) const noexcept { return guid_stripes_[stripe_of(g)]; }

        // Batch-GUID Striping
        // static constexpr size_t kBatchStripeCount = 64;
        // mutable std::array<std::mutex, kBatchStripeCount> batch_stripes_;
        // size_t batch_stripe_of(const BatchId& b) const noexcept { return std::hash<BatchId>{}(b)& (kBatchStripeCount - 1); }
        // std::mutex& mutex_for_batch(const BatchId& b) const noexcept { return batch_stripes_[batch_stripe_of(b)]; }

    public:
        ResourceManager();
        ~ResourceManager();

        // --- IResourceManager API --------------------------------------------

        AssetStatus get_status(const Guid& guid) const override;

        std::shared_future<TaskResult> scan_assets_async(const std::filesystem::path& root, EngineContext& ctx) override;
        std::shared_future<TaskResult> load_and_bind_async(std::deque<Guid> branch_guids, const BatchId& batch, EngineContext& ctx) override;
        std::shared_future<TaskResult> unbind_and_unload_async(std::deque<Guid> branch_guids, const BatchId& batch, EngineContext& ctx) override;
        std::shared_future<TaskResult> reload_and_rebind_async(std::deque<Guid> guids, const BatchId& batch, EngineContext& ctx) override;

    private:
        TaskResult load_and_bind_impl(std::deque<Guid> guids, const BatchId& batch, EngineContext& ctx);
        TaskResult unbind_and_unload_impl(std::deque<Guid> guids, const BatchId& batch, EngineContext& ctx);

    public:
        // void retain_guid(const Guid& guid) override;
        // void release_guid(const Guid& guid, EngineContext& ctx) override;

        bool is_busy() const override;
        void wait_until_idle() override;
        int  queued_tasks() const noexcept override;

        /// @brief Get a snapshot of asset index
        AssetIndexDataPtr get_index_data() const override;

        std::vector<Guid> find_guids_by_name(std::string_view name) const override;

        std::string to_string() const override;

        // --- Non-inherited API -------------------------------------------------------

        // Lease-related inspection
        uint32_t total_leases(const Guid& g) const noexcept;
        bool held_by_any(const Guid& g) const noexcept;
        bool held_by_batch(const Guid& g, const BatchId& b) const noexcept;

#if 0
        template<typename T>
        T& get_asset_ref(const Handle& handle)
        {
            return storage_->get_ref<T>(handle);
        }

        template<typename T>
        std::optional<T&> try_get_asset_ref(const Handle& handle)
        {
            if (!storage_->validate<T>(handle))
                return std::nullopt;
            return storage_->get_ref<T>(handle);
        }
#endif

        const Storage& storage() const;
        Storage& storage();

        const AssetIndex& asset_index() const;
        AssetIndex& asset_index();

        // void start_async_scan(const std::filesystem::path& root, EngineContext& ctx);

        // Must be thread-safe. Use static types, or lock meta paths.
        /// @brief Import new resource to resource index
        /// @tparam T 
        /// @param t 
        /// @return 
        template<class T>
        void import(
            const T& t,
            const std::string& file_path,
            const AssetMetaData& meta,
            const std::string& meta_file_path)
        {
            // EENG_LOG("[ResourceManager] Filing type: %s", typeid(T).name());

            // Add contained assets, note that t is const
            // 'contained_assets' as such is QUESTIONABLE
            auto _meta = meta;
            visit_asset_refs(t, [&](const auto& ref)
                {
                    //if (ref.valid())
                    _meta.contained_assets.push_back(ref.get_guid());
                });

            asset_index_->serialize_to_file<T>(t, _meta, file_path, meta_file_path);
        }

        // TS?
        template<typename T>
        void unimport(AssetRef<T>& ref)
        {
            assert(0);

            // For now: require that asset being removed is not loaded
            assert(!ref.is_loaded());

            // asset_index.remove(ref.guid);
            //unload(ref); // optional: remove from memory too
        }

        template<typename T>
        void load_asset(const Guid& guid, EngineContext& ctx)
        {
            // Silently return if already loaded
            if (storage_->handle_for_guid<T>(guid)) return;

            // Derserialize
            /* add delay */ std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            T asset = asset_index_->deserialize_from_file<T>(guid, ctx);

            // Add to storage
            auto handle = storage_->add<T>(std::move(asset), guid);
        }

        template<typename T>
        void unload_asset(const Guid& guid, EngineContext& ctx)
        {
            auto handle_opt = storage_->handle_for_guid<T>(guid);

            // Silently return if not loaded
            if (!handle_opt) return;

            // Remove from storage
            // /* add short delay */ std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            storage_->remove_now(*handle_opt);
        }

    private:

        void load_asset(const Guid& guid, EngineContext& ctx);
        void unload_asset(const Guid& guid, EngineContext& ctx);

        void bind_asset(const Guid& guid, const Guid& batch_id, EngineContext& ctx);
        void unbind_asset(const Guid& guid, const Guid& batch_id, EngineContext& ctx);

        // template<class T>
        // void bind_asset(AssetRef<T> ref, EngineContext& ctx)
        // {
        //     storage_->modify(ref.handle, [&](T& asset)
        //         {
        //             visit_assets(asset, [&](auto& ref)
        //                 {
        //                     if (storage_->validate(ref.handle)) return;

        //                     using CT = decltype(ref.handle)::value_type;
        //                     auto handle = storage_->handle_for_guid<CT>(ref.guid);
        //                     if (!handle)
        //                         throw std::runtime_error("Bind failed: referenced asset GUID not loaded " + ref.guid.to_string());

        //                     ref.handle = handle.value();
        //                     retain_guid(ref.guid);
        //                 });
        //         });
        // }

        template<class T>
        void bind_asset(AssetRef<T> ref, /*not used*/ const BatchId& batch, EngineContext& ctx)
        {
            if (!storage_->validate(ref.handle)) return;

            storage_->modify(ref.handle, [&](T& asset)
                {
                    visit_asset_refs(asset, [&](auto& child_ref)
                        {
                            // If already wired, skip (idempotent bind)
                            if (storage_->validate(child_ref.handle)) return;

                            using CT = typename std::decay_t<decltype(child_ref.handle)>::value_type;
                            const Guid child = child_ref.get_guid();

                            auto h = storage_->handle_for_guid<CT>(child);
                            if (!h)
                                throw std::runtime_error("Bind failed: referenced asset not loaded " + child.to_string());

                            child_ref.handle = *h;
                        });
                });
            // std::vector<Guid> newly_acquired;
            // newly_acquired.reserve(8);

            // try {
            //     storage_->modify(ref.handle, [&](T& asset)
            //         {
            //             visit_assets(asset, [&](auto& child_ref)
            //                 {
            //                     const Guid child = child_ref.get_guid();

            //                     // Lease child to this batch before wiring the handle
            //                     // batch_acquire(batch, child);
            //                     // newly_acquired.push_back(child);
            //                     if (batch_acquire(batch, child)) newly_acquired.push_back(child); // for rollback

            //                     using CT = decltype(child_ref.handle)::value_type;
            //                     auto h = storage_->handle_for_guid<CT>(child);
            //                     if (!h)
            //                         throw std::runtime_error("Bind failed: referenced asset not loaded " + child.to_string());
            //                     child_ref.handle = *h;
            //                 });
            //         });
            // }
            // catch (...) {
            //     // Rollback child leases we added in this call
            //     for (const Guid& c : newly_acquired)
            //         (void)batch_release(batch, c);
            //     throw;
            // }
        }

    public:

        // Called via meta
        // template<class T>
        // void bind_asset(const Guid& guid, EngineContext& ctx)
        // {
        //     auto maybe_ref = ref_for_guid<T>(guid);
        //     if (!maybe_ref)
        //         throw std::runtime_error("Bind failed: asset GUID not loaded " + guid.to_string());
        //     bind_asset<T>(maybe_ref.value(), ctx);
        // }

        template<class T>
        void bind_asset(const Guid& guid, const BatchId& batch, EngineContext& ctx)
        {
            auto ref = ref_for_guid<T>(guid);
            if (!ref)
                throw std::runtime_error("Bind failed: asset GUID not loaded " + guid.to_string());
            bind_asset<T>(*ref, batch, ctx);
        }

    private:
        // template<class T>
        // void unbind_asset(AssetRef<T> ref, EngineContext& ctx)
        // {
        //     if (!storage_->validate(ref.handle)) return;
        //     storage_->modify(ref.handle, [&](T& asset)
        //         {
        //             visit_assets(asset, [&](auto& ref)
        //                 {
        //                     // unresolve_asset(ref, ctx);
        //                     release_guid(ref.guid, ctx);
        //                     ref.handle = {};
        //                 });
        //         });
        // }

        // Unbind: release child leases and clear handles (idempotent per call)
        template<class T>
        void unbind_asset(AssetRef<T> ref, const BatchId& batch, EngineContext& ctx)
        {
            if (!storage_->validate(ref.handle)) return;

            storage_->modify(ref.handle, [&](T& asset)
                {
                    visit_asset_refs(asset, [&](auto& child_ref)
                        {
                            child_ref.handle = {};
                        });
                });
            // if (!storage_->validate(ref.handle)) return;
            // storage_->modify(ref.handle, [&](T& asset)
            //     {
            //         visit_assets(asset, [&](auto& child_ref)
            //             {
            //                 const Guid child = child_ref.get_guid();
            //                 (void)batch_release(batch, child);
            //                 child_ref.handle = {};
            //             });
            //     });
        }

    public:
        // template<class T>
        // void unbind_asset(const Guid& guid, EngineContext& ctx)
        // {
        //     auto ref_opt = ref_for_guid<T>(guid);

        //     // Silently return if not loaded
        //     if (!ref_opt) return;

        //     // Unbind asset
        //     unbind_asset<T>(ref_opt.value(), ctx);
        // }

        template<class T>
        void unbind_asset(const Guid& guid, const BatchId& batch, EngineContext& ctx)
        {
            if (auto ref = ref_for_guid<T>(guid))
                unbind_asset<T>(*ref, batch, ctx);
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
                    visit_asset_refs(asset, [&](const auto& asset_ref)
                        {
                            using SubT = typename std::decay_t<decltype(asset_ref.handle)>::value_type;
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

        entt::meta_any invoke_meta_function(
            const Guid& guid,
            const Guid& batch_id,
            EngineContext& ctx,
            entt::hashed_string function_id,
            std::string_view function_label
        );

    };
} // namespace eeng