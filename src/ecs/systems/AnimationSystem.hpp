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
    class AnimationSystem
    {
    public:
        /// @brief Update animation state for all ModelComponent instances.
        /// @note Current implementation assumes uniform key spacing and does not
        ///       interpolate by key timestamps. It supports single-clip playback
        ///       and basic two-clip blends; future improvements can add time-based
        ///       key lookup, additive layers, and FSM-driven parameterized blends.
        void update(entt::registry& registry, EngineContext& ctx, float delta_time);
    };
}
