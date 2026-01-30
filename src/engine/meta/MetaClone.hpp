//
//  MetaClone.hpp
//  engine_core_2024
//
//  Created by Carl Johan Gribel on 2024-08-08.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#ifndef MetaClone_hpp
#define MetaClone_hpp

#include <entt/entt.hpp>

namespace eeng {
    struct EngineContext;
}

namespace eeng::meta {

    /// @brief Clone a meta object by value or, if available, using a meta function.
    /// @param any
    /// @param ctx
    /// @return
    entt::meta_any clone_any(
        const entt::meta_any& any,
        EngineContext& ctx);

    /// @brief 
    /// @param registry
    /// @param src_entity
    /// @param dst_entity
    /// @param ctx
    void clone_entity(
        std::shared_ptr<entt::registry>& registry,
        entt::entity src_entity,
        entt::entity dst_entity,
        EngineContext& ctx);

}

#endif /* MetaSerialize_hpp */
