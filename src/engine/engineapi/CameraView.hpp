// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "engineapi/OverlayViewState.hpp"

#include <glm/glm.hpp>

namespace eeng
{
    /**
     * @brief Explicit per-frame camera snapshot used for scene rendering.
     *
     * App hosts choose the active CameraView for each render call. That keeps
     * editor-camera policy in EditorApp, gameplay-camera policy in runtimes,
     * and leaves GameApp free of editor-only concerns.
     */
    struct CameraView
    {
        glm::mat4 view{ 1.0f };
        glm::mat4 proj{ 1.0f };
        glm::mat4 view_proj{ 1.0f };
        glm::mat4 viewport{ 1.0f };

        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::vec3 forward{ 0.0f, 0.0f, -1.0f };
        glm::vec3 up{ 0.0f, 1.0f, 0.0f };
        glm::vec3 right{ 1.0f, 0.0f, 0.0f };

        float near_plane = 0.1f;
        float far_plane = 1000.0f;
        glm::ivec2 window_size{ 0, 0 };
        bool valid = false;
    };

    /**
     * @brief Fill derived camera fields after view/proj/viewport have been chosen.
     *
     * This keeps scene rendering, particles, and debug overlays reading the
     * same canonical snapshot rather than each caller rebuilding basis vectors.
     */
    inline void finalize_camera_view(CameraView& view)
    {
        view.view_proj = view.proj * view.view;
        const glm::mat4 view_to_world = glm::inverse(view.view);
        view.position = glm::vec3(view_to_world[3]);
        view.right = glm::normalize(glm::vec3(view_to_world[0]));
        view.up = glm::normalize(glm::vec3(view_to_world[1]));
        view.forward = -glm::normalize(glm::vec3(view_to_world[2]));
    }

    /**
     * @brief Mirror the active CameraView into the legacy overlay snapshot.
     *
     * OverlayViewState remains the bridge for existing debug/gizmo/picking
     * paths while rendering code moves over to the explicit CameraView contract.
     */
    inline void copy_camera_view_to_overlay_view(
        const CameraView& camera_view,
        OverlayViewState& overlay_view)
    {
        overlay_view.view = camera_view.view;
        overlay_view.proj = camera_view.proj;
        overlay_view.viewport = camera_view.viewport;
        overlay_view.window_size = camera_view.window_size;
        overlay_view.valid = camera_view.valid;
    }
} // namespace eeng
