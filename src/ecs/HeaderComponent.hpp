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
        std::string chunk_tag;
        Guid guid;
        Entity entity_parent;

        HeaderComponent() = default;
        HeaderComponent(
            const std::string& name,
            const std::string& chunk_tag,
            const Guid& guid,
            const Entity& entity_parent);
    };

    std::string to_string(const HeaderComponent& t);

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
