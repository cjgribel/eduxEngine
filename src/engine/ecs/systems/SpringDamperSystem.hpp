// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <entt/entt.hpp>

namespace eeng
{
    struct EngineContext;
}

namespace eeng::ecs::systems
{
    class PhysicsSystem;

    class SpringDamperSystem
    {
    public:
        void update(
            entt::registry& registry,
            EngineContext& ctx,
            PhysicsSystem& physics_system,
            float delta_time);
    };
} // namespace eeng::ecs::systems
