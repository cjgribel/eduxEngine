// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ResourceManager.hpp"
#include "AssetIndex.hpp"
// #include "Storage.hpp"

namespace eeng
{
    ResourceManager::ResourceManager()
        : storage_(std::make_unique<Storage>())
        , asset_index_(std::make_unique<AssetIndex>())
    {
    }

    const Storage& ResourceManager::storage() const
    {
        return *storage_;
    }

    Storage& ResourceManager::storage()
    {
        return *storage_;
    }

    const AssetIndex& ResourceManager::asset_index() const
    {
        return *asset_index_;
    }

    AssetIndex& ResourceManager::asset_index()
    {
        return *asset_index_;
    }

    bool ResourceManager::is_scanning() const
    {
        return asset_index_->is_scanning();
    }

    /// @brief Get a snapshot of the asset index
    /// @return std::vector<AssetEntry>
    // std::vector<AssetEntry> ResourceManager::get_asset_entries_snapshot() const
    // {
    //     std::lock_guard lock{ mutex_ }; // Is this needed?
    //     return asset_index_->get_entries_snapshot();
    // }

    AssetIndexDataPtr ResourceManager::get_index_data() const
    {
        return asset_index_->get_index_data();
    }

    std::string ResourceManager::to_string() const
    {
        return storage_->to_string();
    }

    ResourceManager::~ResourceManager() = default;
}