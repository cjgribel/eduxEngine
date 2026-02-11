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
    class TwoAnchorAlignSystem
    {
    public:
        void update(entt::registry& registry, EngineContext& ctx, float delta_time);
    };
} // namespace eeng::ecs::systems
