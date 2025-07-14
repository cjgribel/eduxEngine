// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include "Handle.h"
#include "AssetEntry.hpp"
#include <string>

#pragma once

namespace eeng
{
    template<typename T, typename Visitor>
    void visit_asset_refs(T&, Visitor&&)
    {
        // No-op for asset types with no AssetRef dependencies
    }
}

namespace eeng
{
    class IResourceManager
    {
    public:

        virtual bool is_scanning() const = 0;
        virtual std::vector<AssetEntry> get_asset_entries_snapshot() const = 0;

        virtual std::string to_string() const = 0;
        virtual ~IResourceManager() = default;
    };
} // namespace eeng