// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "Handle.h"
#include "Guid.h"
#include "AssetMetaData.hpp"
#include "AssetEntry.hpp"
#include "MetaSerialize.hpp"
#include "EngineContext.hpp"

#include <nlohmann/json.hpp> // <nlohmann/json_fwd.hpp>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>
#include "LogMacros.h"

namespace eeng
{
    struct EngineContext; // Forward declaration

    class AssetIndex
    {
    public:
        AssetIndex() = default;
        ~AssetIndex() = default;

        void start_async_scan(const std::filesystem::path& root, EngineContext& ctx);
        std::vector<AssetEntry> get_entries_snapshot() const;
        bool is_scanning() const;

        /// @brief Serializes an asset to disk.
        template<typename T>
        void serialize_to_file(const T& asset, const AssetMetaData& meta,
            const std::filesystem::path& asset_path,
            const std::filesystem::path& meta_path)
        {
            // Serialize data first
            nlohmann::json j_asset = meta::serialize_any(entt::forward_as_meta(asset));
            nlohmann::json j_meta = meta::serialize_any(entt::forward_as_meta(meta));

            // Write asset first (may be large)
            {
                std::ofstream out_asset(asset_path);
                if (!out_asset)
                    throw std::runtime_error("Failed to open asset file for writing: " + asset_path.string());
                out_asset << j_asset.dump(4);
            }

            // Then write meta last (signals scan readiness)
            {
                std::ofstream out_meta(meta_path);
                if (!out_meta)
                    throw std::runtime_error("Failed to open meta file for writing: " + meta_path.string());
                out_meta << j_meta.dump(4);
            }
        }

        template<typename T>
        T deserialize_from_file(
            const Guid& guid,
            EngineContext& ctx) const
        {
            std::lock_guard lock{ entries_mutex_ }; // Protect access to entries_

            auto it = std::find_if(entries_.begin(), entries_.end(), [&](const AssetEntry& entry)
                {
                    return entry.meta.guid == guid;
                });

            if (it == entries_.end())
                throw std::runtime_error("AssetIndex: Failed to find asset file for guid " + guid.to_string());

            const auto& path = it->absolute_path;
            std::ifstream file(path);
            if (!file)
                throw std::runtime_error("AssetIndex: Failed to open asset file: " + path.string());

            nlohmann::json j;
            file >> j;

            entt::meta_any t = T{};
            // entt::meta_any any = entt::resolve<T>().construct(); // meta based
            meta::deserialize_any(j, t, Entity{}, ctx);
            return t.cast<T>();
            //T result;
            // meta::deserialize_any(j, entt::forward_as_meta(result), Entity{}, ctx);
            // return result;
        }

    private:

        std::vector<AssetEntry> entries_;
        mutable std::mutex entries_mutex_;

        std::atomic<bool> scanning_flag_{ false };

        std::vector<AssetEntry> scan_meta_files(
            const std::filesystem::path& root,
            EngineContext& ctx);
    };

#if 0
    class AssetIndex
    {
    public:
        void add_asset(Guid guid, const AssetMetadata& meta)
        {
            std::lock_guard lock(mutex_);
            assets_[guid] = meta;
        }

        // template<typename T>
        // handle<T> load(Guid guid)
        // {
        //     if (auto existing = storage.get_handle<T>(guid))
        //         return existing;

        //     auto serialized_data = deserialize_from_disk<T>(guid); // returns object with only GUIDs
        //     handle<T> h = storage.insert<T>(guid, std::move(serialized_data));

        //     resolve_handles(h); // 🔥 critical step
        //     return h;
        // }
        std::optional<AssetMetadata> get_asset(Guid guid) const
        {
            std::shared_lock lock(mutex_);
            if (auto it = assets_.find(guid); it != assets_.end())
                return it->second;
            return {};
        }

    private:
        mutable std::shared_mutex mutex_;
        std::unordered_map<Guid, AssetMetadata> assets_;
    };
#endif

#if 0
    class AssetIndex
    {
    public:
        // Initialization
        void initialize(const fs::path& meta_folder);

        // Asset Management (Add/Remove)
        template<typename T>
        Guid add_asset(const std::string& original_name, const T& asset_data, const ImportSettings& settings);

        bool remove_asset(const Guid& guid);

        // Querying Assets
        std::optional<Guid> find_guid_by_name(const std::string& asset_name, ResourceType type) const;
        std::vector<AssetEntry> list_assets(ResourceType type) const;

        // Loading/Deserialization
        template<typename T>
        std::optional<T> load_asset(const Guid& guid) const;

        // Batch operations (future concurrency support)
        template<typename T>
        std::vector<std::optional<T>> load_assets(const std::vector<Guid>& guids) const;
    };
#endif

#if 0
    class AssetDatabase {
        fs::path meta_root;

    public:
        AssetDatabase(const fs::path& project_path)
            : meta_root(project_path / "meta")
        {
        }

        template<typename T>
        Guid import_and_serialize(const std::string& original_name, const T& resource) {
            Guid guid = create_guid();
            fs::path type_folder = meta_root / resource_type_folder<T>();
            fs::create_directories(type_folder);

            fs::path resource_path = type_folder / (guid.to_string() + resource_extension<T>());
            fs::path meta_path = resource_path;
            meta_path += ".meta";

            serialize_to_disk(resource, resource_path);

            nlohmann::json meta_json = {
                {"guid", guid.to_string()},
                {"original_name", original_name}
            };
            std::ofstream meta_file(meta_path);
            meta_file << meta_json.dump(4);

            return guid;
        }

        template<typename T>
        std::optional<T> deserialize_resource(const Guid& guid) const {
            fs::path resource_path = meta_root / resource_type_folder<T>() / (guid.to_string() + resource_extension<T>());
            return deserialize_from_disk<T>(resource_path);
        }

    private:
        template<typename T>
        std::string resource_type_folder() const; // specialize per type

        template<typename T>
        std::string resource_extension() const;   // specialize per type

        Guid create_guid() const {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            uint64_t high = gen(), low = gen();
            return Guid(high, low);
        }

        template<typename T>
        void serialize_to_disk(const T& resource, const fs::path& path) {
            std::ofstream ofs(path, std::ios::binary);
            resource.serialize(ofs);
        }

        template<typename T>
        std::optional<T> deserialize_from_disk(const fs::path& path) const {
            if (!fs::exists(path)) return std::nullopt;
            std::ifstream ifs(path, std::ios::binary);
            T resource;
            resource.deserialize(ifs);
            return resource;
        }
    };
#endif

    } // namespace eeng