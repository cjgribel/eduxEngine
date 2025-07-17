// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "AssetEntry.hpp"
#include "util/VecTree.h"
#include <unordered_map>
#include <unordered_set>

namespace eeng::asset::builders {

    using DependencyTree = VecTree<Guid>;

    // template<typename Visitor>
    // void visit_asset_refs(const AssetEntry& entry, Visitor&& visitor); // defined elsewhere

    inline DependencyTree build_dependency_tree(const std::vector<AssetEntry>& entries)
    {
        DependencyTree tree;

        std::unordered_map<Guid, const AssetEntry*> by_guid;
        for (const auto& entry : entries)
            by_guid[entry.meta.guid] = &entry;

        std::unordered_set<Guid> inserted;

        auto insert_recursive = [&](auto&& self, const Guid& guid, const Guid* parent_guid) -> void
            {
                if (inserted.contains(guid)) return;
                inserted.insert(guid);

                if (parent_guid)
                    tree.insert(guid, *parent_guid);
                else
                    tree.insert_as_root(guid);

                const auto* entry = by_guid[guid];
                if (!entry) return;

                visit_asset_refs(*entry, [&](const Guid& ref_guid) {
                    self(self, ref_guid, &guid);
                    });
            };

        for (const auto& entry : entries)
            insert_recursive(insert_recursive, entry.meta.guid, nullptr);

        return tree;
    }

} // namespace eeng::asset::builders