// Created by Codex 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/Entity.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace eeng::module1
{
    // Simple pivot/third-person camera data stored as an ECS component.
    struct ThirdPersonCameraComponent
    {
        // True if this camera should react to input this frame.
        bool active = false;

        // Optional target to follow (EntityRef supports binding/serialization).
        eeng::ecs::EntityRef target{};

        // Extra offset applied on top of the target position (camera orbit pivot).
        glm::vec3 target_offset{ 0.0f, 0.0f, 0.0f };

        // Cached look-at point in world space (computed in the system).
        glm::vec3 look_at{ 0.0f, 0.0f, 0.0f };

        // World up-vector (kept constant for a simple orbit camera).
        glm::vec3 up{ 0.0f, 1.0f, 0.0f };

        // Orbit radius around the look-at point.
        float distance = 15.0f;

        // Mouse sensitivity (radians per pixel).
        float mouse_sensitivity = 0.005f;

        // Controller look speed (radians per second).
        float controller_look_speed = 2.5f;

        // Movement speed for WASD/left stick (units per second).
        float move_speed = 6.0f;

        // View frustum range for rendering.
        float near_plane = 1.0f;
        float far_plane = 500.0f;

        // Orbit angles in radians (yaw around Y, pitch around X).
        float yaw = 0.0f;
        float pitch = -glm::pi<float>() / 8.0f;

        // Cached camera position in world space.
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };

        // Cached forward direction (normalized, from camera to look-at).
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
    void visit_asset_refs(ThirdPersonCameraComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(ThirdPersonCameraComponent& camera, Visitor&& visitor)
    {
        visitor(camera.target);
    }
} // namespace eeng::module1
