#pragma once

// ECSMetaHelpers

#include "MetaAux.h"
#include "Guid.h"
#include "EngineContext.hpp"
#include "ResourceManager.hpp"
#include <vector>


// REGISTER
// TODO: REGISTER to Asset & Comp types
namespace eeng::meta
{

    template<typename T>
    void collect_asset_guids(T& self, std::vector<Guid>& out)
    {
        visit_asset_refs(self, [&](auto& ref)
            {
                // ref is AssetRef<SomeAssetType>&
                out.push_back(ref.guid);
            });
    }

    // 'bind_component_asset_refs'
    // Or, integrate with RM and use it for both Asset & Comp types
    template<typename T>
    void bind_asset_refs(T& self, /*, IResourceManager& rm*/ EngineContext& ctx)
    {
        auto& rm_ = static_cast<eeng::ResourceManager&>(*ctx.resource_manager);

        visit_asset_refs(self, [&](auto& ref)
            {
                using Ref = std::decay_t<decltype(ref)>;
                using HandleT = std::decay_t<decltype(ref.handle)>;
                using AssetT = typename HandleT::value_type;  // matches your existing pattern

                const Guid g = ref.guid;
                if (!g.valid())
                    return;

                auto handle_opt = rm_.handle_for_guid<AssetT>(g);
                if (!handle_opt)
                {
                    // ref                          // The ref we are trying to bind
                    get_meta_type_name<T>();        // Component/refereer type
                    get_meta_type_name<AssetT>();   // Asset type

                    // Policy choice: leave handle empty, or log, or throw.
                    // For now: just leave it empty.
                    return;
                }

                ref.bind(*handle_opt);
                // ref.handle = *h;
            });
    }
#if 0

    // 'bind_component_entity_refs'
    // Mot relevant for Asset types (assets not reference entities)
    template<typename T>
    void resolve_entity_refs(T& self, EntityManager& em)
    {
        visit_entity_refs(self, [&](ecs::EntityRef& ref)
            {
                const Guid g = ref.get_guid();
                if (!g) // or your own validity check
                    return;

                auto ent = em.find_entity_by_guid(g);
                ref.set_entity(ent); // your EntityRef can decide what "invalid" means
            });
    }
}

namespace eeng::meta
{

    // ENTITY HELPER
    template<typename Visitor>
    void for_each_component(entt::entity e,
        entt::registry& reg,
        Visitor&& visitor)
    {
        for (auto&& [type_id, storage] : reg.storage())
        {
            if (!storage.contains(e))
                continue;

            if (auto mt = entt::resolve(type_id); mt)
            {
                void* comp_ptr = storage.value(e);
                entt::meta_any any = mt.from_void(comp_ptr);
                visitor(mt, any);
            }
        }
    }

    /*
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

    inline std::vector<Guid>
        collect_asset_guids_for_entity(entt::entity e, entt::registry& reg)
    {
        std::vector<Guid> result;

        for_each_component(e, reg, [&](entt::meta_type mt, entt::meta_any& any)
            {
                using namespace entt::literals;

                if (auto mf = mt.func("collect_asset_guids"_hs); mf)
                {
                    mf.invoke({}, any, entt::forward_as_meta(result));
                }
            });

        return result;
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
        resolve_entity_refs_for_entity(entt::entity e,
            entt::registry& reg,
            EntityManager& em)
    {
        using namespace entt::literals;

        for_each_component(e, reg, [&](entt::meta_type mt, entt::meta_any& any)
            {
                if (auto mf = mt.func("resolve_entity_refs"_hs); mf)
                {
                    mf.invoke({}, any, entt::forward_as_meta(em));
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
        resolve_asset_refs_for_entity(entt::entity e,
            entt::registry& reg,
            IResourceManager& rm,
            EngineContext& ctx)
    {
        using namespace entt::literals;

        for_each_component(e, reg, [&](entt::meta_type mt, entt::meta_any& any)
            {
                if (auto mf = mt.func("resolve_asset_refs"_hs); mf)
                {
                    mf.invoke(
                        {},
                        any,
                        entt::forward_as_meta(rm),
                        entt::forward_as_meta(ctx)
                    );
                }
            });
}
#endif
}
