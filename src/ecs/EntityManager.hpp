// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IEntityManager.hpp"

namespace eeng
{
    class EntityManager : public IEntityManager
    {
        ecs::Entity create_entity() override { return ecs::Entity {}; }
        void destroy_entity(ecs::Entity entity) override {}
    };


} // namespace eeng