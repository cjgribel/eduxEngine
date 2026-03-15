// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "AssetEntry.hpp"
#include "util/VecTree.h"
#include <unordered_map>
#include <unordered_set>

namespace eeng::asset::builders {

using ContentTree = VecTree<Guid>;

inline ContentTree build_content_tree(const std::vector<AssetEntry>& entries)
{
    ContentTree tree;

    // Step 1: Map all entries by their GUID for quick lookup
    std::unordered_map<Guid, const AssetEntry*> by_guid;
    for (const auto& entry : entries)
        by_guid[entry.meta.guid] = &entry;

    // Step 2: Insert all assets as roots
    for (const auto& entry : entries)
        tree.insert_as_root(entry.meta.guid);

    // Step 3: Reparent based on contained_assets
    for (const auto& entry : entries)
    {
        const Guid& parent_guid = entry.meta.guid;
        for (const auto& child_guid : entry.meta.contained_assets)
        {
            // Confirm the child exists before attempting to reparent
            if (!by_guid.contains(child_guid))
                continue;

            tree.reparent(child_guid, parent_guid);
        }
    }

    return tree;
}

} // namespace eeng::asset::builders