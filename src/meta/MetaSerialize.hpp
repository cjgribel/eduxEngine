// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MetaSerialize_hpp
#define MetaSerialize_hpp

#include <entt/entt.hpp>

// Note: We're including the full nlohmann header and not just 
// <nlohmann/json_fwd.hpp>. The expected usage of this header is on engine cpp:s
// where <nlohmann/json.hpp> is included anyway.
#include <nlohmann/json.hpp>

#include "ecs/Entity.hpp"
#include "EngineContext.hpp"

namespace eeng::meta
{
    struct ComponentSpawnDesc
    {
        // entt::id_type type_id;
        std::string type_id_str;
        nlohmann::json data;
    };

    struct EntitySpawnDesc
    {
        Guid guid;
        std::vector<ComponentSpawnDesc> components;
    };

    nlohmann::json serialize_any(
        const entt::meta_any& meta_any);

    nlohmann::json serialize_entity(
        const ecs::EntityRef& entity_ref,
        std::shared_ptr<entt::registry>& registry);

#if 0
    nlohmann::json serialize_entities(
        Entity* entity_first,
        int count,
        std::shared_ptr<entt::registry>& registry);

    nlohmann::json serialize_registry(
        std::shared_ptr<entt::registry>& registry);
#endif
    void deserialize_any(
        const nlohmann::json& json,
        entt::meta_any& meta_any,
        const ecs::Entity& entity, // SKIP THIS SOMEHOW?
        EngineContext& context
    );

    /// One-shot deserialize-and-create entity from JSON
    // Entity is not registered to scene graph or chunk
    ecs::EntityRef deserialize_entity(
        const nlohmann::json& json,
        EngineContext& ctx
    );

    // Can be called off main-thread (does not touch entt::meta or entt::registry)
    EntitySpawnDesc create_entity_spawn_desc(
        const nlohmann::json& json
    );

    // Main-thread (touches entt::meta and entt::registry)
    ecs::EntityRef spawn_entity_from_desc(
        const EntitySpawnDesc& desc,
        EngineContext& ctx
    );

#if 0
    void deserialize_entities(
        const nlohmann::json& json,
        Editor::Context& context);
#endif
} // namespace Meta

#endif /* MetaSerialize_hpp */
