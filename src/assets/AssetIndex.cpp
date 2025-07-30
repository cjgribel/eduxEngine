// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "AssetIndex.hpp"
#include "AssetTreeViews.hpp"
#include "EngineContext.hpp"
#include "ThreadPool.hpp"
#include "MetaSerialize.hpp"
#include "builders/ContentTreeBuilder.hpp"
#include <fstream>

namespace eeng
{
    void AssetIndex::start_async_scan(
        const std::filesystem::path& root,
        EngineContext& ctx)
    {
        scanning_flag_.store(true, std::memory_order_relaxed);

        ctx.thread_pool->queue_task([this, root, &ctx]()
            {
                auto result = this->scan_meta_files(root, ctx);

                auto data = std::make_shared<AssetIndexData>();
                data->entries = std::move(result);

                // Build aux containers
                for (const auto& entry : data->entries)
                {
                    data->by_guid[entry.meta.guid] = &entry;
                    data->by_type[entry.meta.type_name].push_back(&entry);
                    data->by_parent[entry.meta.guid_parent].push_back(&entry);
                }

                // Build tree views
                auto views = std::make_shared<AssetTreeViews>();
                views->content_tree = asset::builders::build_content_tree(data->entries);
                // other tree builds ...
                data->trees = views;

                // Lock and load
                {
                    std::lock_guard lock(entries_mutex_);
                    index_data_ = std::move(data);
                }

                scanning_flag_.store(false, std::memory_order_release);
            });
    }

    // std::vector<AssetEntry> AssetIndex::get_entries_snapshot() const
    // {
    //     std::lock_guard lock(entries_mutex_);
    //     return entries_; // copy for safety
    // }

    //std::shared_ptr<const std::vector<AssetEntry>> AssetIndex::get_entries_view() const
    AssetIndexDataPtr AssetIndex::get_index_data() const
    {
        // Atomic access: no need for mutex
        return index_data_;
    }

    bool AssetIndex::is_scanning() const
    {
        return scanning_flag_.load(std::memory_order_acquire);
    }

    std::vector<AssetEntry> AssetIndex::scan_meta_files(
        const std::filesystem::path& root,
        EngineContext& ctx)
    {
        std::vector<AssetEntry> assets;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file()) continue;

            const auto& meta_path = entry.path();
            std::string filename = meta_path.filename().string();

            // Ensure the file ends with ".meta.json"
            if (!filename.ends_with(".meta.json"))
                continue;

            // Open and parse the meta file
            std::ifstream in(meta_path);
            if (!in) continue;

            nlohmann::json json;
            in >> json;

            // Deserialize into AssetMetaData
            entt::meta_any any = AssetMetaData{};
            meta::deserialize_any(json, any, ecs::Entity{}, ctx);
            AssetMetaData meta = any.cast<AssetMetaData>();

            // Derive the resource .json file path from the meta path
            std::string base_name = filename.substr(0, filename.size() - std::string(".meta.json").size());
            std::filesystem::path asset_path = meta_path.parent_path() / (base_name + ".json");

            // Compute path relative to the root (for GUI display etc.)
            std::filesystem::path relative_path = std::filesystem::relative(asset_path, root);

            assets.emplace_back(AssetEntry{
                std::move(meta),
                std::move(relative_path),
                std::move(asset_path)
                });
        }

        return assets;
    }

} // namespace eeng
