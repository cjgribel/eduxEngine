// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include <entt/entt.hpp>

namespace eeng::ecs::systems
{
    // VehicleRig1 prototype: input-to-motor/servo control system.
    class VehicleRig1ControlSystem
    {
    public:
        void update(entt::registry& registry, EngineContext& ctx, float dt);
    };
} // namespace eeng::ecs::systems
