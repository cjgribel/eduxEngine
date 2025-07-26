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
        int ref_count = 0;
        // std::optional<entt::meta_type> type;
        std::string error_message;
    };

    class IResourceManager
    {
    public:

        virtual AssetStatus get_status(const Guid& guid) const = 0;

        virtual std::future<bool> load_asset_async(const Guid& guid, EngineContext& ctx) = 0;
        virtual std::future<bool> unload_asset_async(const Guid& guid, EngineContext& ctx) = 0;

        virtual void retain_guid(const Guid& guid) = 0;
        virtual void release_guid(const Guid& guid, EngineContext& ctx) = 0;

        virtual bool is_scanning() const = 0;

        virtual AssetIndexDataPtr get_index_data() const = 0;

        virtual std::string to_string() const = 0;

        virtual ~IResourceManager() = default;

        /* 
        https://chatgpt.com/s/t_687b79b6a15881918c2a19bbb73ec609
        
        --- Accessed via cast to ResourceManager
        [meta functions]            (MetaReg)
        file                        (MockImporter)
        unfile                      (...)
        validate_asset              (GuiManager)
        validate_asset_recursive    (GuiManager)
        .
        start_async_scan            (Game) Maybe closed off from Game & called from Engine
        is_scanning                 (Game)

        Possible approach:
            Free functions in helper header, which cast to ResourceManager
            
            Usage:
                resource::load(ctx.resource_manager, my_ref, ctx);
                - instead of -
                static_cast<ResourceManager*>(ctx.resource_manager.get())->load(...);

            In resource_helpers.hpp:

            namespace resources {

                template<typename T>
                void file(IResourceManager& iface, const T& asset, const std::string& file_path,
                        const AssetMetaData& meta, const std::string& meta_file_path)
                {
                    auto* impl = dynamic_cast<ResourceManager*>(&iface);
                    assert(impl && "Expected ResourceManager");
                    impl->file(asset, file_path, meta, meta_file_path);
                }

                // ...
            }
        */
    };
} // namespace eeng