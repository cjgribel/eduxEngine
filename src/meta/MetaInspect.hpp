// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MetaInspect_hpp
#define MetaInspect_hpp

#include "config.h"
#include "EngineContext.hpp"
// #include "editor/EditComponentCommand.hpp"
#include <entt/entt.hpp>

namespace eeng::editor {
    struct InspectorState;
    class ComponentCommandBuilder;
}

namespace eeng::meta {


    /// @brief Create a name for an entity suitable for imgui widgets
    /// @param registry 
    /// @param entity 
    /// @param comp_with_name_meta_data Meta type of a component with a "name" data field
    /// @return A string in format [entity id] or [name]##[entity id]
    std::string get_entity_name(
        const std::shared_ptr<const entt::registry>& registry,
        entt::entity entity,
        entt::meta_type meta_type_with_name);

    bool inspect_enum_any(
        entt::meta_any& any,
        eeng::editor::InspectorState& inspector);

    bool inspect_any(
        entt::meta_any& any,
        eeng::editor::InspectorState& inspector,
        editor::ComponentCommandBuilder& cmd_builder,
        EngineContext& ctx);

    bool inspect_entity(
        const eeng::ecs::Entity& entity,
        eeng::editor::InspectorState& inspector,
        EngineContext& ctx);

#if 0
    bool inspect_registry(
        entt::meta_type comp_with_name,
        InspectorState& inspector);
#endif

}

#endif /* MetaSerialize_hpp */
