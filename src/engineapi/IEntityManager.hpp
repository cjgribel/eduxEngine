// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "ecs/Entity.hpp"
#include <cstddef>

namespace eeng
{
    // Placeholder
    // class Entity
    // {
    //     size_t id;
    // };

    class IEntityManager
    {
    public:
        virtual ~IEntityManager() = default;
        virtual ecs::Entity create_entity() = 0;
        virtual void destroy_entity(ecs::Entity entity) = 0;
    };
} // namespace eeng