// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IResourceManager.hpp"
#include "EngineContext.hpp"
#include "Storage.hpp" // Can't pimpl-away since class is templated
#include "AssetIndex.hpp"
#include "ResourceTypes.hpp" // For AssetRef<T>, visit_assets
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

        //
        std::shared_future<TaskResult> current_task_;
        mutable std::mutex task_mutex_;

    public:

        ResourceManager();
        ~ResourceManager();

        // --- IResourceManager API --------------------------------------------

        AssetStatus get_status(const Guid& guid) const override;

        std::shared_future<TaskResult> load_and_bind_async(std::deque<Guid> branch_guids, EngineContext& ctx) override;
        std::shared_future<TaskResult> unbind_and_unload_async(std::deque<Guid> branch_guids, EngineContext& ctx) override;
        std::shared_future<TaskResult> reload_and_rebind_async(std::deque<Guid> guids, EngineContext& ctx) override;

        void retain_guid(const Guid& guid) override;
        void release_guid(const Guid& guid, EngineContext& ctx) override;

        bool is_scanning() const override; // <- TaskResult + is_busy

        bool is_busy() const override;
        void wait_until_idle() const override;
        std::optional<TaskResult> last_task_result() const override;
        std::shared_future<TaskResult> active_task() const override;

        /// @brief Get a snapshot of asset index
        AssetIndexDataPtr get_index_data() const override;

        std::string to_string() const override;

        // --- Non-inherited API -------------------------------------------------------

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

        void start_async_scan(const std::filesystem::path& root, EngineContext& ctx);

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
            /* add delay */ std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            storage_->remove_now(*handle_opt);
        }

    private:

        void load_asset(const Guid& guid, EngineContext& ctx);
        void unload_asset(const Guid& guid, EngineContext& ctx);

        void bind_asset(const Guid& guid, EngineContext& ctx);
        void unbind_asset(const Guid& guid, EngineContext& ctx);

        template<class T>
        void bind_asset(AssetRef<T> ref, EngineContext& ctx)
        {
            storage_->modify(ref.handle, [&](T& asset)
                {
                    visit_assets(asset, [&](auto& ref)
                        {
                            if (storage_->validate(ref.handle)) return;

                            using CT = decltype(ref.handle)::value_type;
                            auto handle = storage_->handle_for_guid<CT>(ref.guid);
                            if (!handle)
                                throw std::runtime_error("Bind failed: referenced asset GUID not loaded " + ref.guid.to_string());

                            ref.handle = handle.value();
                            retain_guid(ref.guid);
                        });
                });
        }

    public:

        // Called via meta
        template<class T>
        void bind_asset(const Guid& guid, EngineContext& ctx)
        {
            auto maybe_ref = ref_for_guid<T>(guid);
            if (!maybe_ref)
                throw std::runtime_error("Bind failed: asset GUID not loaded " + guid.to_string());
            bind_asset<T>(maybe_ref.value(), ctx);
        }

    private:
        template<class T>
        void unbind_asset(AssetRef<T> ref, EngineContext& ctx)
        {
            if (!storage_->validate(ref.handle)) return;
            storage_->modify(ref.handle, [&](T& asset)
                {
                    visit_assets(asset, [&](auto& ref)
                        {
                            // unresolve_asset(ref, ctx);
                            release_guid(ref.guid, ctx);
                            ref.handle = {};
                        });
                });
        }

    public:
        template<class T>
        void unbind_asset(const Guid& guid, EngineContext& ctx)
        {
            auto ref_opt = ref_for_guid<T>(guid);

            // Silently return if not loaded
            if (!ref_opt) return;

            // Unbind asset
            unbind_asset<T>(ref_opt.value(), ctx);
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

        };
    } // namespace eeng