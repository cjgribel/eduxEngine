// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef HeaderComponents_hpp
#define HeaderComponents_hpp

#include "Guid.h"
#include "Entity.hpp"
#include <string>

namespace eeng::ecs
{
    struct HeaderComponent
    {
        std::string name;
        std::string chunk_tag; // PROBABLY SKIP
        Guid guid;
        Entity parent_entity;

        HeaderComponent() = default;
        HeaderComponent(
            const std::string& name,
            const std::string& chunk_tag,
            const Guid& guid,
            const Entity& entity_parent);
    };

    std::string to_string(const HeaderComponent& t);

    template<typename Visitor> void visit_asset_refs(HeaderComponent& h, Visitor&& visitor) {}

    // -> EntityRef.hpp
    // template<typename T, typename Fn>
    // void visit_entities(T& object, Fn&& func);

    template<typename Visitor> void visit_entities(HeaderComponent& h, Visitor&& visitor) {}

#if 0
    namespace {
        bool HeaderComponent_inspect(void* ptr, Editor::InspectorState& inspector)
        {
            return false;
        }
}
#endif

    struct ChunkModifiedEvent
    {
        Entity entity;
        std::string chunk_tag;
    };
}

#endif // HeaderComponents_hpp
