// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ecs/TransformGizmoSystem.hpp"

#include "EngineContext.hpp"
#include "EngineContextHelpers.hpp"
#include "editor/ecs/TransformGizmoComponent.hpp"
#include "ShapeRenderer.hpp"

namespace eeng::editor
{
    void TransformGizmoSystem::update(
        EngineContext& ctx,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& viewport,
        const glm::ivec2& window_size)
    {
        auto registry_sp = eeng::try_get_registry(ctx, "TransformGizmoSystem");
        if (!registry_sp)
            return;

        auto view_entities = registry_sp->view<TransformGizmoComponent>();
        for (auto [entity, gizmo] : view_entities.each())
        {
            (void)entity;
            if (!gizmo.enabled)
                continue;

            gizmo.sync_to_runtime();
            gizmo.runtime_gizmo.update(ctx, view, proj, viewport, window_size);
            gizmo.sync_from_runtime();
        }
    }

    void TransformGizmoSystem::render(
        EngineContext& ctx,
        ShapeRendering::ShapeRenderer& renderer,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& viewport,
        const glm::ivec2& window_size) const
    {
        auto registry_sp = eeng::try_get_registry(ctx, "TransformGizmoSystem");
        if (!registry_sp)
            return;

        auto view_entities = registry_sp->view<TransformGizmoComponent>();
        for (auto [entity, gizmo] : view_entities.each())
        {
            (void)entity;
            if (!gizmo.enabled)
                continue;

            gizmo.sync_to_runtime();
            gizmo.runtime_gizmo.render(ctx, renderer, view, proj, viewport, window_size);
            gizmo.sync_from_runtime();
        }
    }
} // namespace eeng::editor
