// Created by Carl Johan Gribel 2026.
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
#include <type_traits>
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

        template<typename Ctx>
        using ResourceManagerShared = std::conditional_t<std::is_const_v<Ctx>,
            std::shared_ptr<const ResourceManager>,
            std::shared_ptr<ResourceManager>>;

        template<typename Ctx>
        using ResourceManagerPtr = std::conditional_t<std::is_const_v<Ctx>,
            const ResourceManager*,
            ResourceManager*>;

        template<typename Ctx>
        using EventQueuePtr = std::conditional_t<std::is_const_v<Ctx>,
            const EventQueue*,
            EventQueue*>;

        template<typename Ctx>
        using EntityManagerPtr = std::conditional_t<std::is_const_v<Ctx>,
            const EntityManager*,
            EntityManager*>;

        template<typename Ctx>
        using RegistryShared = std::conditional_t<std::is_const_v<Ctx>,
            std::shared_ptr<const entt::registry>,
            std::shared_ptr<entt::registry>>;

        template<typename Ctx>
        using RegistryPtr = std::conditional_t<std::is_const_v<Ctx>,
            const entt::registry*,
            entt::registry*>;

        template<typename Ctx>
        ResourceManagerShared<Ctx> try_get_resource_manager_impl(Ctx& ctx, const char* log_tag)
        {
            using Rm = std::conditional_t<std::is_const_v<Ctx>, const ResourceManager, ResourceManager>;
            auto rm = std::dynamic_pointer_cast<Rm>(ctx.resource_manager);
            if (!rm)
                detail::handle_failure(const_cast<EngineContext&>(ctx), log_tag, "ResourceManager unavailable");
            return rm;
        }

        template<typename Ctx>
        ResourceManagerPtr<Ctx> try_get_resource_manager_ptr_impl(Ctx& ctx, const char* log_tag)
        {
            auto rm = try_get_resource_manager_impl(ctx, log_tag);
            return rm ? rm.get() : nullptr;
        }

        template<typename Ctx>
        EventQueuePtr<Ctx> try_get_event_queue_impl(Ctx& ctx, const char* log_tag)
        {
            if (!ctx.event_queue)
            {
                detail::handle_failure(const_cast<EngineContext&>(ctx), log_tag, "EventQueue unavailable");
                return nullptr;
            }
            return ctx.event_queue.get();
        }

        template<typename Ctx>
        EntityManagerPtr<Ctx> try_get_entity_manager_ptr_impl(Ctx& ctx, const char* log_tag)
        {
            if (!ctx.entity_manager)
            {
                detail::handle_failure(const_cast<EngineContext&>(ctx), log_tag, "EntityManager unavailable");
                return nullptr;
            }

            using Em = std::conditional_t<std::is_const_v<Ctx>, const EntityManager, EntityManager>;
            auto* em = dynamic_cast<Em*>(ctx.entity_manager.get());
            if (!em)
                detail::handle_failure(const_cast<EngineContext&>(ctx), log_tag, "Concrete EntityManager unavailable");
            return em;
        }

        template<typename Ctx>
        RegistryShared<Ctx> try_get_registry_impl(Ctx& ctx, const char* log_tag)
        {
            if (!ctx.entity_manager)
            {
                detail::handle_failure(const_cast<EngineContext&>(ctx), log_tag, "EntityManager unavailable");
                return {};
            }

            using ManagerPtr = std::conditional_t<std::is_const_v<Ctx>, const IEntityManager*, IEntityManager*>;
            auto* manager = static_cast<ManagerPtr>(ctx.entity_manager.get());
            auto registry_sp = manager->registry_wptr().lock();
            if (!registry_sp)
                detail::handle_failure(const_cast<EngineContext&>(ctx), log_tag, "Registry expired");
            return registry_sp;
        }

        template<typename Ctx>
        RegistryPtr<Ctx> try_get_registry_ptr_impl(Ctx& ctx, const char* log_tag)
        {
            auto registry_sp = try_get_registry_impl(ctx, log_tag);
            return registry_sp ? registry_sp.get() : nullptr;
        }
    } // namespace detail

    inline std::shared_ptr<ResourceManager> try_get_resource_manager(EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_resource_manager_impl(ctx, log_tag);
    }

    inline ResourceManager* try_get_resource_manager_ptr(EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_resource_manager_ptr_impl(ctx, log_tag);
    }

    inline std::shared_ptr<const ResourceManager> try_get_resource_manager(const EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_resource_manager_impl(ctx, log_tag);
    }

    inline const ResourceManager* try_get_resource_manager_ptr(const EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_resource_manager_ptr_impl(ctx, log_tag);
    }

    inline EventQueue* try_get_event_queue(EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_event_queue_impl(ctx, log_tag);
    }

    inline const EventQueue* try_get_event_queue(const EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_event_queue_impl(ctx, log_tag);
    }

    inline EntityManager* try_get_entity_manager_ptr(EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_entity_manager_ptr_impl(ctx, log_tag);
    }

    inline const EntityManager* try_get_entity_manager_ptr(const EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_entity_manager_ptr_impl(ctx, log_tag);
    }

    inline std::shared_ptr<entt::registry> try_get_registry(EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_registry_impl(ctx, log_tag);
    }

    inline entt::registry* try_get_registry_ptr(EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_registry_ptr_impl(ctx, log_tag);
    }

    inline std::shared_ptr<const entt::registry> try_get_registry(const EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_registry_impl(ctx, log_tag);
    }

    inline const entt::registry* try_get_registry_ptr(const EngineContext& ctx, const char* log_tag)
    {
        return detail::try_get_registry_ptr_impl(ctx, log_tag);
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

    template<typename T, typename U, typename Fn>
    bool try_read_asset_pair(
        ResourceManager& rm,
        const Handle<T>& handle_a,
        const Guid& guid_a,
        const Handle<U>& handle_b,
        const Guid& guid_b,
        EngineContext& ctx,
        const char* log_tag,
        const char* missing_label_a,
        const char* missing_label_b,
        Fn&& fn)
    {
        if (!handle_a)
        {
            detail::log_warn_once(ctx, log_tag, guid_a, missing_label_a);
            return false;
        }
        if (!handle_b)
        {
            detail::log_warn_once(ctx, log_tag, guid_b, missing_label_b);
            return false;
        }
        if (!rm.storage().validate(handle_a))
        {
            detail::log_warn_once(ctx, log_tag, guid_a, missing_label_a);
            return false;
        }
        if (!rm.storage().validate(handle_b))
        {
            detail::log_warn_once(ctx, log_tag, guid_b, missing_label_b);
            return false;
        }
        try
        {
            rm.storage().read2(handle_a, handle_b, std::forward<Fn>(fn));
        }
        catch (const ValidationError&)
        {
            detail::log_warn_once(ctx, log_tag, guid_a, missing_label_a);
            detail::log_warn_once(ctx, log_tag, guid_b, missing_label_b);
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
        if (!ref.guid.valid())
            return false;

        // Most runtime paths bind AssetRef handles eagerly, but lightweight
        // editor/runtime flows can occasionally hold only the GUID while the
        // asset is already resident in the ResourceManager. Fall back to a
        // GUID lookup here so readers remain robust instead of failing only
        // because the caller has not rebound the handle yet.
        auto handle = ref.handle;
        if (!handle)
        {
            if (auto handle_opt = rm.handle_for_guid<T>(ref.guid))
                handle = *handle_opt;
        }

        return try_read_asset(rm, handle, ref.guid, ctx, log_tag, missing_label, std::forward<Fn>(fn));
    }
} // namespace eeng
