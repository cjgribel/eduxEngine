// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "editor/ecs/EditorCameraSystem.hpp"
#include "editor/ecs/EditorBootstrapSystem.hpp"
#include "editor/ecs/TransformGizmoSystem.hpp"

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
    class EditorRuntime
    {
    public:
        void init(EngineContext& ctx);

        void update(
            EngineContext& ctx,
            const glm::mat4& view,
            const glm::mat4& proj,
            const glm::mat4& viewport,
            const glm::ivec2& window_size);

        // Update editor-owned camera components before game update.
        void update_cameras(EngineContext& ctx, float delta_time);

        void render(
            EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer,
            const glm::mat4& view,
            const glm::mat4& proj,
            const glm::mat4& viewport,
            const glm::ivec2& window_size) const;

    private:
        EditorBootstrapSystem bootstrap_;
        EditorCameraSystem camera_system_;
        TransformGizmoSystem transform_gizmo_system_;
    };
} // namespace eeng::editor
