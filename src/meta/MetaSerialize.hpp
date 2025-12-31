// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MetaSerialize_hpp
#define MetaSerialize_hpp

#include <entt/entt.hpp>

// Note: We're including the full nlohmann header and not just 
// <nlohmann/json_fwd.hpp>. The expected usage of this header is on engine cpp:s
// where <nlohmann/json.hpp> is included anyway.
#include <nlohmann/json.hpp>

#include "MetaInfo.h"
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
        const entt::meta_any& meta_any,
        SerializationPurpose purpose = SerializationPurpose::generic);

    nlohmann::json serialize_entity(
        const ecs::EntityRef& entity_ref,
        std::shared_ptr<entt::registry>& registry,
        SerializationPurpose purpose = SerializationPurpose::generic);

#if 0
    nlohmann::json serialize_entities(
        Entity* entity_first,
        int count,
        std::shared_ptr<entt::registry>& registry,
        SerializationPurpose purpose = SerializationPurpose::generic);

    nlohmann::json serialize_registry(
        std::shared_ptr<entt::registry>& registry,
        SerializationPurpose purpose = SerializationPurpose::generic);
#endif
    void deserialize_any(
        const nlohmann::json& json,
        entt::meta_any& meta_any,
        const ecs::Entity& entity, // SKIP THIS SOMEHOW?
        EngineContext& context,
        SerializationPurpose purpose = SerializationPurpose::generic
    );

    /// One-shot deserialize-and-create entity from JSON
    // Entity is not registered to scene graph or chunk
    ecs::EntityRef deserialize_entity(
        const nlohmann::json& json,
        EngineContext& ctx,
        SerializationPurpose purpose = SerializationPurpose::generic
    );

    // Can be called off main-thread (does not touch entt::meta or entt::registry)
    EntitySpawnDesc create_entity_spawn_desc(
        const nlohmann::json& json
    );

    // Main-thread (touches entt::meta and entt::registry)
    ecs::EntityRef spawn_entity_from_desc(
        const EntitySpawnDesc& desc,
        EngineContext& ctx,
        SerializationPurpose purpose = SerializationPurpose::generic
    );

    // Purpose wrappers (keep call sites intention-revealing).
    inline nlohmann::json serialize_any_for_file(const entt::meta_any& meta_any)
    {
        return serialize_any(meta_any, SerializationPurpose::file);
    }

    inline nlohmann::json serialize_any_for_undo(const entt::meta_any& meta_any)
    {
        return serialize_any(meta_any, SerializationPurpose::undo);
    }

    inline nlohmann::json serialize_any_for_display(const entt::meta_any& meta_any)
    {
        return serialize_any(meta_any, SerializationPurpose::display);
    }

    inline void deserialize_any_for_file(
        const nlohmann::json& json,
        entt::meta_any& meta_any,
        const ecs::Entity& entity,
        EngineContext& context)
    {
        deserialize_any(json, meta_any, entity, context, SerializationPurpose::file);
    }

    inline void deserialize_any_for_undo(
        const nlohmann::json& json,
        entt::meta_any& meta_any,
        const ecs::Entity& entity,
        EngineContext& context)
    {
        deserialize_any(json, meta_any, entity, context, SerializationPurpose::undo);
    }

    inline void deserialize_any_for_display(
        const nlohmann::json& json,
        entt::meta_any& meta_any,
        const ecs::Entity& entity,
        EngineContext& context)
    {
        deserialize_any(json, meta_any, entity, context, SerializationPurpose::display);
    }

    inline nlohmann::json serialize_entity_for_file(
        const ecs::EntityRef& entity_ref,
        std::shared_ptr<entt::registry>& registry)
    {
        return serialize_entity(entity_ref, registry, SerializationPurpose::file);
    }

    inline nlohmann::json serialize_entity_for_undo(
        const ecs::EntityRef& entity_ref,
        std::shared_ptr<entt::registry>& registry)
    {
        return serialize_entity(entity_ref, registry, SerializationPurpose::undo);
    }

    inline nlohmann::json serialize_entity_for_display(
        const ecs::EntityRef& entity_ref,
        std::shared_ptr<entt::registry>& registry)
    {
        return serialize_entity(entity_ref, registry, SerializationPurpose::display);
    }

    inline ecs::EntityRef deserialize_entity_for_file(
        const nlohmann::json& json,
        EngineContext& ctx)
    {
        return deserialize_entity(json, ctx, SerializationPurpose::file);
    }

    inline ecs::EntityRef deserialize_entity_for_undo(
        const nlohmann::json& json,
        EngineContext& ctx)
    {
        return deserialize_entity(json, ctx, SerializationPurpose::undo);
    }

    inline ecs::EntityRef deserialize_entity_for_display(
        const nlohmann::json& json,
        EngineContext& ctx)
    {
        return deserialize_entity(json, ctx, SerializationPurpose::display);
    }

    inline ecs::EntityRef spawn_entity_from_desc_for_file(
        const EntitySpawnDesc& desc,
        EngineContext& ctx)
    {
        return spawn_entity_from_desc(desc, ctx, SerializationPurpose::file);
    }

    inline ecs::EntityRef spawn_entity_from_desc_for_undo(
        const EntitySpawnDesc& desc,
        EngineContext& ctx)
    {
        return spawn_entity_from_desc(desc, ctx, SerializationPurpose::undo);
    }

    inline ecs::EntityRef spawn_entity_from_desc_for_display(
        const EntitySpawnDesc& desc,
        EngineContext& ctx)
    {
        return spawn_entity_from_desc(desc, ctx, SerializationPurpose::display);
    }

#if 0
    void deserialize_entities(
        const nlohmann::json& json,
        Editor::Context& context);
#endif
} // namespace Meta

#endif /* MetaSerialize_hpp */
