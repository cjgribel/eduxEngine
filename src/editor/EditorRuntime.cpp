// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/EditorRuntime.hpp"

#include "EngineContext.hpp"
#include "ShapeRenderer.hpp"

namespace eeng::editor
{
    void EditorRuntime::init(EngineContext& ctx)
    {
        bootstrap_.init(ctx);
    }

    void EditorRuntime::update(
        EngineContext& ctx,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& viewport,
        const glm::ivec2& window_size)
    {
        transform_gizmo_system_.update(ctx, view, proj, viewport, window_size);
    }

    void EditorRuntime::update_cameras(EngineContext& ctx, float delta_time)
    {
        // Editor cameras are driven separately from the game runtime.
        camera_system_.update(ctx, delta_time);
    }

    void EditorRuntime::render(
        EngineContext& ctx,
        ShapeRendering::ShapeRenderer& renderer,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& viewport,
        const glm::ivec2& window_size) const
    {
        transform_gizmo_system_.render(ctx, renderer, view, proj, viewport, window_size);
    }
} // namespace eeng::editor
