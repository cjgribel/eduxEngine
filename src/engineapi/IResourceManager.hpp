// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include "Handle.h"
#include "AssetIndexData.hpp"
#include <string>
#include <future>

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
    struct EngineContext;

    enum class LoadState {
        Loading,
        Loaded,
        Unloading,
        Unloaded,
        Failed
    };

    struct AssetStatus
    {
        LoadState state = LoadState::Unloaded;
        int ref_count = 0;   // starts at 0; increments via retain, decrements via release
        // std::optional<entt::meta_type> type;
        std::string error_message;
    };

    class IResourceManager
    {
    public:

#if 1
        virtual AssetStatus get_status(const Guid& guid) const = 0;
        virtual std::future<bool> load_asset_async(const Guid& guid, EngineContext& ctx) = 0;
        virtual std::future<bool> unload_asset_async(const Guid& guid, EngineContext& ctx) = 0;

        virtual void retain_guid(const Guid& guid) = 0;
        virtual void release_guid(const Guid& guid, EngineContext& ctx) = 0;
#endif

        virtual bool is_scanning() const = 0;

        virtual AssetIndexDataPtr get_index_data() const = 0;

        virtual std::string to_string() const = 0;

        virtual ~IResourceManager() = default;
    };
} // namespace eeng