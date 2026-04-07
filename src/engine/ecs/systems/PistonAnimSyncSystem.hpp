// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"

#include <entt/entt.hpp>

namespace eeng::ecs::systems
{
    class PistonAnimSyncSystem
    {
    public:
        void update(entt::registry& registry, EngineContext& ctx, float delta_time);
    };
} // namespace eeng::ecs::systems
