// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ResourceManager.hpp"
#include "AssetIndex.hpp"
// #include "Storage.hpp"

namespace eeng
{
    ResourceManager::ResourceManager()
        : storage(std::make_unique<Storage>())
        , asset_index(std::make_unique<AssetIndex>())
    {
    }

    std::string ResourceManager::to_string() const
    {
        return storage->to_string();
    }

    ResourceManager::~ResourceManager() = default;
}