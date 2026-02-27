// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "entt/entt.hpp"

namespace eeng
{
    struct EngineContext;
}

namespace ShapeRendering
{
    class ShapeRenderer;
}

namespace eeng::ecs::systems
{
    class TrailSystem
    {
    public:
        void update(
            entt::registry& registry,
            EngineContext& ctx,
            float delta_time);

        void render(
            entt::registry& registry,
            EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer) const;
    };
}
