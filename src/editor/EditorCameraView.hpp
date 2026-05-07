// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "editor/ecs/FirstPersonCameraComponent.hpp"
#include "editor/ecs/ThirdPersonCameraComponent.hpp"
#include "engineapi/CameraView.hpp"
#include "glmcommon.hpp"

#include <entt/entt.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace eeng::editor
{
    struct ActiveEditorCameraView
    {
        glm::mat4 view{ 1.0f };
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        float near_plane = 1.0f;
        float far_plane = 500.0f;
    };

    inline bool try_get_active_editor_camera_view(
        entt::registry& registry,
        ActiveEditorCameraView& out_view)
    {
        auto third_view = registry.view<ThirdPersonCameraComponent>();
        for (auto entity : third_view)
        {
            const auto& camera = third_view.get<ThirdPersonCameraComponent>(entity);
            if (!camera.active)
                continue;

            out_view.view = camera.model_to_view;
            out_view.position = camera.position;
            out_view.near_plane = camera.near_plane;
            out_view.far_plane = camera.far_plane;
            return true;
        }

        auto first_view = registry.view<FirstPersonCameraComponent>();
        for (auto entity : first_view)
        {
            const auto& camera = first_view.get<FirstPersonCameraComponent>(entity);
            if (!camera.active)
                continue;

            out_view.view = camera.model_to_view;
            out_view.position = camera.position;
            out_view.near_plane = camera.near_plane;
            out_view.far_plane = camera.far_plane;
            return true;
        }

        return false;
    }

    inline bool build_active_editor_camera_view(
        EngineContext& ctx,
        glm::ivec2 window_size,
        CameraView& out_view)
    {
        if (!ctx.entity_manager)
            return false;
        if (window_size.x <= 0 || window_size.y <= 0)
            return false;

        ActiveEditorCameraView camera_view{};
        if (!try_get_active_editor_camera_view(ctx.entity_manager->registry(), camera_view))
            return false;

        const float aspect_ratio = static_cast<float>(window_size.x) / static_cast<float>(window_size.y);
        out_view.view = camera_view.view;
        out_view.proj = glm::perspective(
            glm::radians(60.0f),
            aspect_ratio,
            camera_view.near_plane,
            camera_view.far_plane);
        out_view.viewport = glm_aux::create_viewport_matrix(
            0.0f,
            0.0f,
            static_cast<float>(window_size.x),
            static_cast<float>(window_size.y),
            0.0f,
            1.0f);
        out_view.window_size = window_size;
        out_view.near_plane = camera_view.near_plane;
        out_view.far_plane = camera_view.far_plane;
        out_view.valid = true;
        finalize_camera_view(out_view);
        return true;
    }

    // Transitional wrapper while overlay/gizmo/picking code still consumes the
    // older OverlayViewState type.
    inline bool build_active_editor_overlay_view(
        EngineContext& ctx,
        glm::ivec2 window_size,
        OverlayViewState& out_view)
    {
        CameraView camera_view{};
        if (!build_active_editor_camera_view(ctx, window_size, camera_view))
            return false;
        copy_camera_view_to_overlay_view(camera_view, out_view);
        return true;
    }
} // namespace eeng::editor
