// Created by Codex 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace eeng::module1
{
    // Simple first-person/free-look camera data stored as an ECS component.
    struct FirstPersonCameraComponent
    {
        // True if this camera should react to input this frame.
        bool active = false;

        // Camera position in world space.
        glm::vec3 position{ 0.0f, 1.8f, 5.0f };

        // World up-vector (kept constant for a simple FPS camera).
        glm::vec3 up{ 0.0f, 1.0f, 0.0f };

        // Movement speed for WASD/left stick (units per second).
        float move_speed = 6.0f;

        // Mouse sensitivity (radians per pixel).
        float mouse_sensitivity = 0.005f;

        // Controller look speed (radians per second).
        float controller_look_speed = 2.5f;

        // View frustum range for rendering.
        float near_plane = 1.0f;
        float far_plane = 500.0f;

        // Look angles in radians (yaw around Y, pitch around X).
        float yaw = 0.0f;
        float pitch = 0.0f;

        // Cached forward direction (normalized).
        glm::vec3 forward{ 0.0f, 0.0f, -1.0f };

        // Cached matrices:
        // - model_to_view: world -> camera space (used for rendering).
        // - view_to_world: camera -> world space (inverse of model_to_view).
        glm::mat4 model_to_view{ 1.0f };
        glm::mat4 view_to_world{ 1.0f };

        // Previous mouse position for relative motion (pixels).
        glm::ivec2 mouse_prev{ -1, -1 };
    };

    template<typename Visitor>
    void visit_asset_refs(FirstPersonCameraComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(FirstPersonCameraComponent&, Visitor&&) {}
} // namespace eeng::module1
