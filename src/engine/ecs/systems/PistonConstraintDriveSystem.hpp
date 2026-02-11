// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <entt/entity/fwd.hpp>

namespace eeng
{
    struct EngineContext;
}

namespace eeng::ecs::systems
{
    class PistonConstraintDriveSystem
    {
    public:
        // Update constraint targets and limits before the physics step.
        void update_pre_physics(entt::registry& registry, EngineContext& ctx, float delta_time);
        // Update measured extension after the physics step.
        void update_post_physics(entt::registry& registry, EngineContext& ctx, float delta_time);
    };
} // namespace eeng::ecs::systems
