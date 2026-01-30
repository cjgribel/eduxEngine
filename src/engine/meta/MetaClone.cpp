//
//  MetaClone.cpp
//  engine_core_2024
//
//  Created by Carl Johan Gribel on 2024-08-08.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#include <iostream>
#include <sstream>
#include <cassert>
#include "imgui.h"
#include "MetaInspect.hpp"
#include "EngineContext.hpp"
// #include "InspectorState.hpp"
#include "MetaLiterals.h"
#include "MetaAux.h"

namespace eeng::meta 
{

    /*

    */
    // src_any can be a component or a non-component
    // if type is meta & has clone_hs: use it; else value-copy src_any
    entt::meta_any clone_any(const entt::meta_any& any, EngineContext& ctx)
    {
        if (entt::meta_type meta_type = entt::resolve(any.type().id()); meta_type)
        {
            if (entt::meta_func meta_func = meta_type.func(literals::clone_hs); meta_func)
            {
                // Policy: Context-aware clone hooks decide how to resolve selection.
                auto copy_any = meta_func.invoke(
                    {},
                    entt::forward_as_meta(any),
                    entt::forward_as_meta(ctx));
                if (copy_any)
                    return copy_any;
            }
        }

        // Policy: No clone hook matched; fall back to value copy semantics.
        return any;
    }

    // If a component is a meta type: use clone_hs if available; else use direct copy

    void clone_entity(std::shared_ptr<entt::registry>& registry, entt::entity src_entity, entt::entity dst_entity, EngineContext& ctx)
    {
        for (auto&& [id, type] : registry->storage())
        {
            if (!type.contains(src_entity)) continue;
            void* src_ptr = type.value(src_entity);
#if 1
            if (entt::meta_type meta_type = entt::resolve(id); meta_type)
            {
                auto comp_any = meta_type.from_void(type.value(src_entity)); // ref
                // mod |= inspect_any(comp_any, inspector, cmd_builder);
                auto copy_any = clone_any(comp_any, ctx);
                assert(copy_any);

                type.push(dst_entity, copy_any.base().data());
            }
            else
            {
                // Fallback: Direct copy if no meta type
                type.push(dst_entity, src_ptr);
            }
#else
            // Use clone() if it exists
            if (entt::meta_type meta_type = entt::resolve(id); meta_type)
            {

                if (entt::meta_func meta_func = meta_type.func(clone_hs); meta_func)
                {
                    std::cout << "Invoking clone() for " << type.type().name() << std::endl;

                    auto copy_any = meta_func.invoke({}, src_ptr, dst_entity);
                    assert(copy_any && "Failed to invoke clone() for type ");

                    type.push(dst_entity, copy_any.data());
                    continue;
                }
            }

            // Fallback: Direct copy if no meta type or clone function
            type.push(dst_entity, src_ptr);
#endif
        }
    }

} // namespace Editor
