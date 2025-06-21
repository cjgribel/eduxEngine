// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MetaSerialize_hpp
#define MetaSerialize_hpp

#include <entt/entt.hpp>
//#include <nlohmann/json_fwd.hpp> // Use nlohmann::json& as references instead
#include <nlohmann/json.hpp>
#include "EngineContext.h"

// namespace eeng
// {
//     class EngineContext;
//     class Entity; // Todo: placeholder
// }

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
#if 0
    // nlohmann::json serialize_entity(
    //     entt::entity,
    //     std::shared_ptr<entt::registry>& registry);

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
        const Entity& entity,
        EngineContext& context);
#if 0
    Entity deserialize_entity(
        const nlohmann::json& json,
        Editor::Context& context
    );

    void deserialize_entities(
        const nlohmann::json& json,
        Editor::Context& context);
#endif
} // namespace Meta

#endif /* MetaSerialize_hpp */
