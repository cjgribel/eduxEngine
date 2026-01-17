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
    class PlayerControllerSystem
    {
    public:
        void update(entt::registry& registry, EngineContext& ctx, float delta_time);
    };
}
