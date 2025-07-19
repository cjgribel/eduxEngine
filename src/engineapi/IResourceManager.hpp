// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include "Handle.h"
#include "AssetIndexData.hpp"
#include <string>

#pragma once

namespace eeng
{
    template<typename T, typename Visitor>
    void visit_assets(T&, Visitor&&)
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

        virtual AssetIndexDataPtr get_index_data() const = 0;

        virtual std::string to_string() const = 0;
        
        virtual ~IResourceManager() = default;
    };
} // namespace eeng