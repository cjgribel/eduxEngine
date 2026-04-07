// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <entt/entt.hpp>
#include "Guid.h"
#include "ecs/Entity.hpp"

namespace eeng
{
    struct EngineContext;
    class EntityManager;
}

namespace eeng::editor
{
    // Ensure storage exists before mutating component data.
    void ensure_storage(entt::registry& registry, entt::id_type component_id);

    // Capture a stable GUID for an entity if it is valid and in the scene graph.
    bool try_capture_guid(EntityManager& em, const ecs::Entity& entity, Guid& guid);

    // Resolve a live entity by GUID, returning a null entity on failure.
    ecs::Entity resolve_entity_from_guid(EntityManager& em, const Guid& guid);

    // Rebind asset/entity references after creating or deserializing an entity.
    void bind_refs_for_entity(ecs::Entity entity, EngineContext& ctx);

    // Mark the owning batch as dirty so closure rebuilds can run.
    void mark_batch_dirty_for_entity(ecs::Entity entity, EngineContext& ctx);
}
