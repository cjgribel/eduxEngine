#pragma once

// ECSMetaHelpers

#include "MetaAux.h"
#include "MetaLiterals.h"
#include "Guid.h"
#include "EngineContext.hpp"
#include "ResourceManager.hpp"
#include "ecs/EntityManager.hpp"
#include <vector>

#include "LogGlobals.hpp" // DEBUG

// REGISTER
// REGISTERED to Comp types (later maybe also Asset types, if wired through the same system)
namespace eeng::meta
{

    // any is a component type (later: or possibly an asset type)
    template<typename T>
    // void collect_asset_guids(T& self, std::vector<Guid>& out)
    void collect_asset_guids(entt::meta_any& any, std::vector<Guid>& out)
    {
        LogGlobals::log("[collect_asset_guids]");

        auto self_ptr = any.try_cast<T>();
        assert(self_ptr && "[collect_asset_guids]: Could not cast meta_any to Guid");

        visit_asset_refs(*self_ptr, [&](auto& ref)
            {
                // ref is AssetRef<SomeAssetType>&
                out.push_back(ref.guid);

                // LogGlobals::log("[collect_asset_guids] %s", ref.guid.to_string().c_str());
            });
    }

    // 'bind_component_asset_refs'
    // Or, integrate with RM and use it for both Asset & Comp types
    template<typename T>
    // void bind_asset_refs(T& self, /*, IResourceManager& rm*/ EngineContext& ctx)
    void bind_asset_refs(entt::meta_any& any, EngineContext& ctx)
    {
        auto& rm = static_cast<eeng::ResourceManager&>(*ctx.resource_manager);

        auto self_ptr = any.try_cast<T>();
        assert(self_ptr && "[bind_asset_refs]: Could not cast meta_any to Guid");

        visit_asset_refs(*self_ptr, [&](auto& ref)
            {
                using AssetType = typename std::remove_reference_t<decltype(ref)>::asset_type;

                const Guid guid = ref.guid;
                if (!guid.valid())
                {
                    // Reference Guid invalid - ok, leave unassigned
                    return;
                }

                auto handle_opt = rm.handle_for_guid<AssetType>(guid);
                if (!handle_opt)
                {
                    // Asset handle not found for guid
                    // Ok (soft reference policy) -> leave reference unbound

                    // Log
                    {
                        auto guid_str = guid.to_string();
                        auto comp_tstr = meta::get_meta_type_display_name<T>();
                        auto asset_tstr = meta::get_meta_type_display_name<AssetType>();
                        EENG_LOG(&ctx, "[bind_asset_refs] Could not bind asset %s (%s) to %s...", asset_tstr.c_str(), guid_str.c_str(), comp_tstr.c_str());
                    }

                    return;
                }

                ref.bind(*handle_opt);
            });
    }

    // 'bind_component_entity_refs'
    // Not relevant for Asset types (assets not reference entities)
    template<typename T>
    // void bind_entity_refs(T& self, /*EntityManager& em,*/ EngineContext& ctx)
    void bind_entity_refs(entt::meta_any& any, EngineContext& ctx)
    {
        auto& em = static_cast<eeng::EntityManager&>(*ctx.entity_manager);

        auto self_ptr = any.try_cast<T>();
        assert(self_ptr && "[bind_entity_refs]: Could not cast meta_any to Guid");

        visit_entity_refs(*self_ptr, [&](ecs::EntityRef& ref)
            {
                const Guid guid = ref.guid;
                if (!guid.valid())
                {
                    // Reference Guid invalid - ok, leave unassigned
                    return;
                }

                auto ent_opt = em.get_entity_from_guid(guid);
                if (!ent_opt)
                {
                    // Entity not found for guid
                    // Ok (soft reference policy) - leave reference unbound

                    auto guid_str = guid.to_string();
                    auto comp_tstr = meta::get_meta_type_display_name<T>();
                    EENG_LOG(&ctx, "[bind_entity_refs] Could not bind entity %s to component %s...", guid_str.c_str(), comp_tstr.c_str());

                    return;
                }
                ref.bind(*ent_opt);
            });
    }
}

namespace eeng::meta
{
    // ENTITY HELPER: 
    template<typename Visitor>
    void for_each_component(
        entt::entity e,
        entt::registry& reg,
        Visitor&& visitor)
    {
        // LogGlobals::log("[for_each_component]");

        for (auto&& [type_id, storage] : reg.storage())
        {
            if (!storage.contains(e))
                continue;

            if (auto mt = entt::resolve(type_id); mt)
            {
                void* comp_ptr = storage.value(e);
                entt::meta_any any = mt.from_void(comp_ptr); // ref

                // if (any.base().policy() == entt::any_policy::ref)
                //     LogGlobals::log("[for_each_component] is_ref");

                visitor(mt, any);
            }
        }
    }

    /*
    1) WHEN AN ENTITY WITH COMPS IS ATTACHED
    2) WHEN A COMP IS ATTACHED TO AN ENTITY ALREADY IN A BATCH

    Usage in BatchRegistry when an entity is attached/created:

    if (auto reg_sp = ctx.entity_manager->registry_wptr().lock();
    reg_sp && created.has_entity())
{
    auto guids = eeng::meta::collect_asset_guids_for_entity(
        created.get_entity(), *reg_sp);

    std::lock_guard lk(mtx_);
    auto& closure = B->asset_closure_hdr;
    for (auto& g : guids)
        if (std::find(closure.begin(), closure.end(), g) == closure.end())
            closure.push_back(g);
}
    */

    // Collect asset GUIDs referenced by entity
    inline std::vector<Guid>
        collect_asset_guids_for_entity(
            entt::entity e,
            entt::registry& reg)
    {
        LogGlobals::log("[collect_asset_guids_for_entity]");
        std::vector<Guid> result;

        for_each_component(e, reg, [&](entt::meta_type mt, entt::meta_any& any)
            {
                LogGlobals::log("[collect_asset_guids_for_entity] comp type %s", meta::get_meta_type_display_name(mt).c_str());

                if (auto mf = mt.func(literals::collect_asset_guids_hs); mf)
                {
                    mf.invoke(
                        {},
                        entt::forward_as_meta(any),
                        entt::forward_as_meta(result));
                }
            });

        return result;
    }

    // Collect asset GUIDs referenced by asset
    inline std::vector<Guid>
        collect_referenced_asset_guids(
            const Guid& guid,
            EngineContext& ctx)
    {
        auto rm = std::dynamic_pointer_cast<ResourceManager>(ctx.resource_manager);
        assert(rm);

        std::vector<Guid> out;

        auto mh_opt = rm->handle_for_guid(guid);
        if (!mh_opt || !mh_opt->valid())
            return out;

        rm->storage().modify(*mh_opt, [&](entt::meta_any any)
            {
                if (auto mf = mh_opt->type.func(literals::collect_asset_guids_hs); mf)
                {
                    mf.invoke(
                        {},
                        entt::forward_as_meta(any),
                        entt::forward_as_meta(out));
                }
            });

        // (optional: dedup/filter invalid)
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }


    /*
Usage after a batch has loaded and all entities are spawned + registered:

if (auto reg_sp = ctx.entity_manager->registry_wptr().lock())
{
    auto& reg = *reg_sp;
    auto& em  = *ctx.entity_manager;

    for (auto& er : B.live)
    {
        if (!er.has_entity()) continue;
        eeng::meta::resolve_entity_refs_for_entity(er.get_entity(), reg, em);
    }
}
    */
    inline void
        bind_entity_refs_for_entity(
            entt::entity e,
            EngineContext& ctx)
    {
        using namespace entt::literals;

        auto& em = static_cast<eeng::EntityManager&>(*ctx.entity_manager);

        for_each_component(e, em.registry(), [&](entt::meta_type mt, entt::meta_any& any)
            {
                if (auto mf = mt.func(literals::bind_entity_refs_hs); mf)
                {
                    mf.invoke(
                        {},
                        entt::forward_as_meta(any),
                        entt::forward_as_meta(ctx));
                }
            });
    }

    /*
    Usage after RM has loaded/bound the batch’s asset closure:

    if (auto reg_sp = ctx.entity_manager->registry_wptr().lock())
{
    auto& reg = *reg_sp;
    auto& rm  = *ctx.resource_manager;

    for (auto& er : B.live)
    {
        if (!er.has_entity()) continue;
        eeng::meta::resolve_asset_refs_for_entity(er.get_entity(), reg, rm, ctx);
    }
}
    */
    inline void
        bind_asset_refs_for_entity(
            entt::entity e,
            // entt::registry& reg,
            // IResourceManager& rm,
            EngineContext& ctx)
    {
        using namespace entt::literals;

        auto& em = static_cast<eeng::EntityManager&>(*ctx.entity_manager);

        for_each_component(e, em.registry(), [&](entt::meta_type mt, entt::meta_any& any)
            {
                if (auto mf = mt.func(literals::bind_asset_refs_hs); mf)
                {
                    mf.invoke(
                        {},
                        entt::forward_as_meta(any),
                        // entt::forward_as_meta(rm),
                        entt::forward_as_meta(ctx));
                }
            });
    }
}
