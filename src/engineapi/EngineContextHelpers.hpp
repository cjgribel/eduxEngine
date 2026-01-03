// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "ResourceManager.hpp"
#include "AssetRef.hpp"
#include "Storage.hpp"
#include "LogMacros.h"
#include "ecs/EntityManager.hpp"
#include "EventQueue.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#ifndef EENG_CTX_HELPERS_STRICT
#define EENG_CTX_HELPERS_STRICT 0
#endif

namespace eeng
{
    namespace detail
    {
        inline const char* normalize_log_tag(const char* log_tag)
        {
            return (log_tag && *log_tag) ? log_tag : "Engine";
        }

        inline void log_warn_once(EngineContext& ctx, const char* log_tag, const char* message)
        {
            const char* tag = normalize_log_tag(log_tag);
            static std::unordered_set<std::string> warned;
            std::string key = std::string(tag) + "|" + message;
            if (warned.insert(key).second)
                EENG_LOG_WARN(&ctx, "[%s] %s", tag, message);
        }

        inline void log_warn_once(EngineContext& ctx, const char* log_tag, const Guid& guid, const char* message)
        {
            if (!guid.valid())
                return;
            const char* tag = normalize_log_tag(log_tag);
            const std::string guid_str = guid.to_string();
            static std::unordered_set<std::string> warned;
            std::string key = std::string(tag) + "|" + message + "|" + guid_str;
            if (warned.insert(key).second)
                EENG_LOG_WARN(&ctx, "[%s] %s %s", tag, message, guid_str.c_str());
        }

        inline void handle_failure(EngineContext& ctx, const char* log_tag, const char* message)
        {
#if EENG_CTX_HELPERS_STRICT
            const char* tag = normalize_log_tag(log_tag);
            throw std::runtime_error(std::string("[") + tag + "] " + message);
#else
            log_warn_once(ctx, log_tag, message);
#endif
        }

        inline void handle_failure(EngineContext& ctx, const char* log_tag, const Guid& guid, const char* message)
        {
#if EENG_CTX_HELPERS_STRICT
            const char* tag = normalize_log_tag(log_tag);
            if (guid.valid())
                throw std::runtime_error(std::string("[") + tag + "] " + message + " " + guid.to_string());
            throw std::runtime_error(std::string("[") + tag + "] " + message);
#else
            log_warn_once(ctx, log_tag, guid, message);
#endif
        }
    } // namespace detail

    inline std::shared_ptr<ResourceManager> try_get_resource_manager(EngineContext& ctx, const char* log_tag)
    {
        auto rm = std::dynamic_pointer_cast<ResourceManager>(ctx.resource_manager);
        if (!rm)
            detail::handle_failure(ctx, log_tag, "ResourceManager unavailable");
        return rm;
    }

    inline ResourceManager* try_get_resource_manager_ptr(EngineContext& ctx, const char* log_tag)
    {
        auto rm = try_get_resource_manager(ctx, log_tag);
        return rm ? rm.get() : nullptr;
    }

    inline EventQueue* try_get_event_queue(EngineContext& ctx, const char* log_tag)
    {
        if (!ctx.event_queue)
        {
            detail::handle_failure(ctx, log_tag, "EventQueue unavailable");
            return nullptr;
        }
        return ctx.event_queue.get();
    }

    inline EntityManager* try_get_entity_manager(EngineContext& ctx, const char* log_tag)
    {
        if (!ctx.entity_manager)
        {
            detail::handle_failure(ctx, log_tag, "EntityManager unavailable");
            return nullptr;
        }

        auto* em = dynamic_cast<EntityManager*>(ctx.entity_manager.get());
        if (!em)
            detail::handle_failure(ctx, log_tag, "Concrete EntityManager unavailable");
        return em;
    }

    inline std::shared_ptr<entt::registry> try_get_registry(EngineContext& ctx, const char* log_tag)
    {
        if (!ctx.entity_manager)
        {
            detail::handle_failure(ctx, log_tag, "EntityManager unavailable");
            return {};
        }

        auto registry_sp = ctx.entity_manager->registry_wptr().lock();
        if (!registry_sp)
            detail::handle_failure(ctx, log_tag, "Registry expired");
        return registry_sp;
    }

    inline entt::registry* try_get_registry_ptr(EngineContext& ctx, const char* log_tag)
    {
        auto registry_sp = try_get_registry(ctx, log_tag);
        return registry_sp ? registry_sp.get() : nullptr;
    }

    template<typename T, typename Fn>
    bool try_read_asset(
        ResourceManager& rm,
        const Handle<T>& handle,
        const Guid& guid,
        EngineContext& ctx,
        const char* log_tag,
        const char* missing_label,
        Fn&& fn)
    {
        if (!handle)
        {
            detail::log_warn_once(ctx, log_tag, guid, missing_label);
            return false;
        }
        if (!rm.storage().validate(handle))
        {
            detail::log_warn_once(ctx, log_tag, guid, missing_label);
            return false;
        }
        try
        {
            rm.storage().read(handle, std::forward<Fn>(fn));
        }
        catch (const ValidationError&)
        {
            detail::log_warn_once(ctx, log_tag, guid, missing_label);
            return false;
        }
        return true;
    }

    template<typename T, typename Fn>
    bool try_read_asset_ref(
        ResourceManager& rm,
        const AssetRef<T>& ref,
        EngineContext& ctx,
        const char* log_tag,
        const char* missing_label,
        Fn&& fn)
    {
        if (!ref.is_bound())
            return false;
        return try_read_asset(rm, ref.handle, ref.guid, ctx, log_tag, missing_label, std::forward<Fn>(fn));
    }
} // namespace eeng
