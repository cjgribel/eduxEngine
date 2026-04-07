// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/CommandEntityHelpers.hpp"
#include "BatchRegistry.hpp"
#include "EngineContext.hpp"
#include "ecs/EntityManager.hpp"
#include "meta/EntityMetaHelpers.hpp"
#include "meta/MetaAux.h"
#include "MetaLiterals.h"
#include <cassert>
#include <stdexcept>

namespace eeng::editor
{
    // Ensure component storage exists before editing or removing data.
    void ensure_storage(entt::registry& registry, entt::id_type component_id)
    {
        if (!registry.storage(component_id))
        {
            auto meta_type = entt::resolve(component_id);
            assert(meta_type);

            auto assure_fn = meta_type.func(eeng::literals::assure_component_storage_hs);
            assert(assure_fn);

            auto result = assure_fn.invoke(
                {},
                entt::forward_as_meta(registry)
            );
            if (!result)
            {
                throw std::runtime_error(
                    "Failed to invoke assure_storage for " + eeng::meta::get_meta_type_display_name(meta_type));
            }
        }
    }

    bool try_capture_guid(EntityManager& em, const ecs::Entity& entity, Guid& guid)
    {
        // Guard against invalid or detached entities.
        if (!entity.has_id())
            return false;
        if (!em.entity_valid(entity))
            return false;
        if (!em.scene_graph().contains(entity))
            return false;
        guid = em.get_entity_guid(entity);
        return guid.valid();
    }

    ecs::Entity resolve_entity_from_guid(EntityManager& em, const Guid& guid)
    {
        // Return a null entity when the guid cannot be resolved.
        if (!guid.valid())
            return ecs::Entity{};
        auto entity_opt = em.get_entity_from_guid(guid);
        if (!entity_opt || !entity_opt->has_id())
            return ecs::Entity{};
        if (!em.entity_valid(*entity_opt))
            return ecs::Entity{};
        return *entity_opt;
    }

    void bind_refs_for_entity(ecs::Entity entity, EngineContext& ctx)
    {
        // Rebind both asset and entity refs after deserialization.
        if (ctx.resource_manager)
            eeng::meta::bind_asset_refs_for_entity(entity, ctx);
        eeng::meta::bind_entity_refs_for_entity(entity, ctx);
    }

    void mark_batch_dirty_for_entity(ecs::Entity entity, EngineContext& ctx)
    {
        // Flag closure rebuilds so batch membership stays coherent.
        if (!ctx.batch_registry)
            return;
        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        br.mark_closure_dirty_for_entity(entity, ctx);
    }
}
