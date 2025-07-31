// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include "ecs/Entity.hpp"
#include <entt/fwd.hpp>
#include <cstddef>

namespace eeng
{
    class IEntityManager
    {
    public:
        virtual ~IEntityManager() = default;

        virtual ecs::Entity create_entity() = 0;

        virtual void destroy_entity(ecs::Entity entity) = 0;

        virtual entt::registry& registry() noexcept = 0;
        virtual const entt::registry& registry() const noexcept = 0;
        virtual std::weak_ptr<entt::registry> registry_wptr() noexcept = 0;
        virtual std::weak_ptr<const entt::registry> registry_wptr() const noexcept = 0;

    };
} // namespace eeng