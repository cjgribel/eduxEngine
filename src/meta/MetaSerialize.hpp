// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MetaSerialize_hpp
#define MetaSerialize_hpp

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

#include "ecs/Entity.hpp"
#include "EngineContext.hpp"

// Note: We're including the full nlohmann header and not just 
// <nlohmann/json_fwd.hpp>. The expected usage of this header is on engine cpp:s
// where <nlohmann/json.hpp> is included anyway.

namespace eeng::meta
{
#if 0
    /// @brief Makes sure entt has storage for a given component.
    /// @param registry 
    /// @param component_id 
    /// If no storage exists, a component of the given type is added to a temporary entity.
    void ensure_storage(entt::registry& registry, entt::id_type component_id);
#endif
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
        const ecs::Entity& entity,
        EngineContext& context);

    ecs::EntityRef deserialize_entity(
        const nlohmann::json& json,
        EngineContext& ctx);

#if 0
    void deserialize_entities(
        const nlohmann::json& json,
        Editor::Context& context);
#endif
} // namespace Meta

#endif /* MetaSerialize_hpp */
