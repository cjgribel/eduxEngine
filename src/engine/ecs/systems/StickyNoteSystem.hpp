// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <glm/glm.hpp>

#include "entt/entt.hpp"

namespace eeng
{
    struct EngineContext;
}

namespace eeng::ecs::systems
{
    class StickyNoteSystem
    {
    public:
        void update(
            entt::registry& registry,
            EngineContext& ctx,
            float delta_time);

        void render(
            entt::registry& registry,
            EngineContext& ctx,
            const glm::mat4& VP_PROJ_V,
            int window_height) const;
    };
}
