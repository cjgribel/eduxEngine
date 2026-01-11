// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <glm/glm.hpp>

namespace ShapeRendering
{
    class ShapeRenderer;
}

namespace eeng
{
    struct EngineContext;
}

namespace eeng::editor
{
    class ManipulatorGizmoSystem
    {
    public:
        void update(
            EngineContext& ctx,
            const glm::mat4& view,
            const glm::mat4& proj,
            const glm::mat4& viewport,
            const glm::ivec2& window_size);

        void render(
            EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer,
            const glm::mat4& view,
            const glm::mat4& proj,
            const glm::mat4& viewport,
            const glm::ivec2& window_size) const;
    };
} // namespace eeng::editor
