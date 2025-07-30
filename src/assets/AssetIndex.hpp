// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "Handle.h"
#include "Guid.h"
#include "AssetMetaData.hpp"
#include "AssetEntry.hpp"
#include "AssetIndexData.hpp"
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

        // std::vector<AssetEntry> get_entries_snapshot() const;
        //std::shared_ptr<const std::vector<AssetEntry>> get_entries_view() const;
        AssetIndexDataPtr get_index_data() const;

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
            // std::lock_guard lock{ entries_mutex_ }; // Protect access to entries_
            
            auto entries = index_data_->entries; // atomic snapshot
            auto it = std::find_if(entries.begin(), entries.end(), [&](const AssetEntry& entry)
                {
                    return entry.meta.guid == guid;
                });

            if (it == entries.end())
                throw std::runtime_error("AssetIndex: Failed to find asset file for guid " + guid.to_string());

            const auto& path = it->absolute_path;
            //
            // TODO: Check what file is: json, png ...
            //
            std::ifstream file(path);
            if (!file)
                throw std::runtime_error("AssetIndex: Failed to open asset file: " + path.string());

            nlohmann::json j;
            file >> j;

            entt::meta_any t = T{};
            // entt::meta_any any = entt::resolve<T>().construct(); // meta based
            meta::deserialize_any(j, t, ecs::Entity{}, ctx);
            return t.cast<T>();
            //T result;
            // meta::deserialize_any(j, entt::forward_as_meta(result), Entity{}, ctx);
            // return result;
        }

    private:

        // std::vector<AssetEntry> entries_;
        //std::shared_ptr<const std::vector<AssetEntry>> entries_ = std::make_shared<std::vector<AssetEntry>>();
        std::shared_ptr<const AssetIndexData> index_data_;
        mutable std::mutex entries_mutex_;

        std::atomic<bool> scanning_flag_{ false };

        std::vector<AssetEntry> scan_meta_files(
            const std::filesystem::path& root,
            EngineContext& ctx);
    };
    } // namespace eeng