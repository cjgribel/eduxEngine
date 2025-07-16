// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "AssetEntry.hpp"
#include <vector>
#include <unordered_map>

namespace eeng
{
    struct AssetIndexData
    {
        std::vector<AssetEntry> entries;

        std::unordered_map<Guid, const AssetEntry*> by_guid;
        std::unordered_map<std::string, std::vector<const AssetEntry*>> by_type;
        std::unordered_map<Guid, std::vector<const AssetEntry*>> by_parent;
    };

    using AssetIndexDataPtr = std::shared_ptr<const AssetIndexData>;
}