// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include "Handle.h"
#include "AssetIndexData.hpp"
#include <string>
#include <future>
#include <deque>

#pragma once

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

        virtual std::future<void> load_and_bind_async(std::deque<Guid> branch_guids, EngineContext& ctx) = 0;
        virtual std::future<void> unbind_and_unload_async(std::deque<Guid> branch_guids, EngineContext& ctx) = 0;
        virtual std::future<void> reload_and_rebind_async(std::deque<Guid> guids, EngineContext& ctx) = 0;

        virtual void retain_guid(const Guid& guid) = 0;
        virtual void release_guid(const Guid& guid, EngineContext& ctx) = 0;

        virtual bool is_scanning() const = 0;

        virtual AssetIndexDataPtr get_index_data() const = 0;

        virtual std::string to_string() const = 0;

        virtual ~IResourceManager() = default;
    };
} // namespace eeng