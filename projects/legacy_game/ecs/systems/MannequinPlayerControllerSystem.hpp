// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "entt/entt.hpp"

namespace eeng
{
    struct EngineContext;
}

namespace eeng::ecs::systems
{
    class PhysicsSystem;

    class MannequinPlayerControllerSystem
    {
    public:
        // Optional hookup to the physics system for raycast queries.
        void set_physics_system(PhysicsSystem* physics_system) { physics_system_ = physics_system; }
        void update(entt::registry& registry, EngineContext& ctx, float delta_time);

    private:
        PhysicsSystem* physics_system_ = nullptr;
    };
}
