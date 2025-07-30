//
//  Context.hpp
//
//  Created by Carl Johan Gribel on 2024-10-15.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef Context_hpp
#define Context_hpp

#include <memory>
//#include <entt/fwd.hpp>
#include "Entity.hpp"
#include <sol/forward.hpp>
//#include "SceneGraph.hpp"
#include "EventDispatcher.h"
class SceneGraph;

namespace Editor {

    using CreateEntityFunc          = std::function<Entity(const Entity&, const Entity&)>;
    using CreateEmptyEntityFunc     = std::function<Entity(const Entity&)>;
    using DestroyEntityFunc         = std::function<void(const Entity&)>;
    using CanRegisterEntityFunc     = std::function<bool(const Entity&)>;
    using RegisterEntityFunc        = std::function<void(const Entity&)>;
    using ReparentEntityFunc        = std::function<void(const Entity&, const Entity&)>;
    using SetEntityHeaderParentFunc = std::function<void(const Entity&, const Entity&)>;
    // using GetParentFunc = std::function<entt::entity(entt::entity)>;
    using EntityValidFunc = std::function<bool(const Entity&)>;

    struct Context
    {
        std::shared_ptr<entt::registry> registry;   // -> weak
        std::shared_ptr<sol::state> lua;            // -> weak
        std::weak_ptr<SceneGraph> scenegraph;
        std::weak_ptr<EventDispatcher> dispatcher;

        // shared_ptr<Scene>
        CreateEntityFunc            create_entity;
        CreateEmptyEntityFunc       create_empty_entity;
        DestroyEntityFunc           destroy_entity;
        CanRegisterEntityFunc       can_register_entity;
        RegisterEntityFunc          register_entity;
        ReparentEntityFunc          reparent_entity;
        SetEntityHeaderParentFunc   set_entity_header_parent;
        // GetParentFunc get_parent;
        EntityValidFunc entity_valid;

        // shared_ptr<Resources>

        std::unordered_map<Entity, Entity> entity_remap;
    };

} // namespace Editor

#endif // Command_hpp
