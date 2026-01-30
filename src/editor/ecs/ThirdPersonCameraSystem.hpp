// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <entt/entt.hpp>

namespace eeng
{
    struct EngineContext;
}

namespace eeng::editor
{
    class ThirdPersonCameraSystem
    {
    public:
        // Update orbit camera input and cached matrices for active cameras.
        void update(entt::registry& registry, EngineContext& ctx, float delta_time);
    };
} // namespace eeng::editor
