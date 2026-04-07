// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <entt/entt.hpp>

namespace eeng
{
    struct EngineContext;
}

namespace eeng::editor
{
    class FirstPersonCameraSystem
    {
    public:
        // Update camera input and cached matrices for any active first-person cameras.
        void update(entt::registry& registry, EngineContext& ctx, float delta_time);
    };
} // namespace eeng::editor
