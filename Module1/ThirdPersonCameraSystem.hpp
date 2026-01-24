// Created by Codex 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <entt/entt.hpp>

namespace eeng
{
    struct EngineContext;
}

namespace eeng::module1::systems
{
    class ThirdPersonCameraSystem
    {
    public:
        void update(entt::registry& registry, EngineContext& ctx, float delta_time);
    };
} // namespace eeng::module1::systems
