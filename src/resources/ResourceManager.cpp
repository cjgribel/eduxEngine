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

    const Storage& ResourceManager::storage()
    {
        return *storage_;
    }

    const AssetIndex& ResourceManager::asset_index()
    {
        return *asset_index_;
    }

    std::string ResourceManager::to_string() const
    {
        return storage_->to_string();
    }

    ResourceManager::~ResourceManager() = default;
}