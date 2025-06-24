// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "IEntityManager.hpp"

namespace eeng
{
    class EntityManager : public IEntityManager
    {
        Entity create_entity() override {}
        void destroy_entity(Entity entity) override {}
    };


} // namespace eeng