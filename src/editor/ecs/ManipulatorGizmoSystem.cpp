// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ecs/ManipulatorGizmoSystem.hpp"

#include "EngineContext.hpp"
#include "EngineContextHelpers.hpp"
#include "editor/ecs/ManipulatorGizmoComponent.hpp"
#include "ShapeRenderer.hpp"

namespace eeng::editor
{
    void ManipulatorGizmoSystem::update(
        EngineContext& ctx,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& viewport,
        const glm::ivec2& window_size)
    {
        auto registry_sp = eeng::try_get_registry(ctx, "ManipulatorGizmoSystem");
        if (!registry_sp)
            return;

        auto view_entities = registry_sp->view<ManipulatorGizmoComponent>();
        for (auto [entity, gizmo] : view_entities.each())
        {
            (void)entity;
            if (!gizmo.enabled)
                continue;

            gizmo.sync_to_runtime();
            gizmo.runtime.update(ctx, view, proj, viewport, window_size);
            gizmo.sync_from_runtime();
        }
    }

    void ManipulatorGizmoSystem::render(
        EngineContext& ctx,
        ShapeRendering::ShapeRenderer& renderer,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& viewport,
        const glm::ivec2& window_size) const
    {
        auto registry_sp = eeng::try_get_registry(ctx, "ManipulatorGizmoSystem");
        if (!registry_sp)
            return;

        auto view_entities = registry_sp->view<ManipulatorGizmoComponent>();
        for (auto [entity, gizmo] : view_entities.each())
        {
            (void)entity;
            if (!gizmo.enabled)
                continue;

            gizmo.sync_to_runtime();
            gizmo.runtime.render(ctx, renderer, view, proj, viewport, window_size);
            gizmo.sync_from_runtime();
        }
    }
} // namespace eeng::editor
