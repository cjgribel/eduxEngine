// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ecs/ThirdPersonCameraSystem.hpp"

#include "EngineContext.hpp"
#include "engineapi/IInputManager.hpp"
#include "glmcommon.hpp"
#include "ecs/TransformComponent.hpp"
#include "editor/ecs/ThirdPersonCameraComponent.hpp"

#include <algorithm>

namespace
{
    struct KeyboardOrbitInput
    {
        float yaw = 0.0f;
        float distance = 0.0f;
        float pitch = 0.0f;
    };

    KeyboardOrbitInput keyboard_orbit_axes(const eeng::IInputManager& input)
    {
        using Key = eeng::IInputManager::Key;
        const bool w = input.IsKeyPressed(Key::W);
        const bool s = input.IsKeyPressed(Key::S);
        const bool a = input.IsKeyPressed(Key::A);
        const bool d = input.IsKeyPressed(Key::D);
        const bool q = input.IsKeyPressed(Key::Q);
        const bool e = input.IsKeyPressed(Key::E);

        KeyboardOrbitInput axes{};
        // W/S zoom toward/away from the target.
        axes.distance = (s ? 1.0f : 0.0f) + (w ? -1.0f : 0.0f);
        // A/D yaw around the global Y axis.
        axes.yaw = (d ? 1.0f : 0.0f) + (a ? -1.0f : 0.0f);
        // Q/E pitch around the local X axis (camera right).
        axes.pitch = (e ? 1.0f : 0.0f) + (q ? -1.0f : 0.0f);
        return axes;
    }
}

namespace eeng::editor
{
    void ThirdPersonCameraSystem::update(entt::registry& registry, EngineContext& ctx, float delta_time)
    {
        auto* input = ctx.input_manager.get();
        if (!input)
            return;

        const auto mouse = input->GetMouseState();

        constexpr float kMinDistance = 0.5f;
        constexpr float kScrollSpeed = 1.0f;

        auto view = registry.view<ThirdPersonCameraComponent>();
        for (auto entity : view)
        {
            auto& camera = view.get<ThirdPersonCameraComponent>(entity);

            // Resolve the pivot target from the bound entity if it is still live.
            // Resolve the base target position from the bound entity (if any).
            glm::vec3 target_pos = camera.look_at;
            if (camera.target.is_bound() && registry.valid(camera.target.entity))
            {
                if (const auto* tfm = registry.try_get<ecs::TransformComponent>(camera.target.entity))
                {
                    // Use cached world position (translation column of the world matrix).
                    target_pos = glm::vec3(tfm->world_matrix[3]);
                }
            }

            if (camera.active)
            {
                // Read orbit inputs only for the active camera.
                // Orbit input: keyboard/mouse only (ignore controller for editor cameras).
                float yaw_axis = 0.0f;
                float distance_axis = 0.0f;
                float pitch_axis = 0.0f;
                const auto axes = keyboard_orbit_axes(*input);
                yaw_axis = axes.yaw;
                distance_axis = axes.distance;
                pitch_axis = axes.pitch;

                // Zoom by changing the orbit radius.
                camera.distance += distance_axis * camera.move_speed * delta_time;
                camera.distance = std::max(camera.distance, kMinDistance);

                if (mouse.scroll_y != 0.0f)
                {
                    camera.distance -= mouse.scroll_y * kScrollSpeed;
                    camera.distance = std::max(camera.distance, kMinDistance);
                }

                // Yaw is a rotation around the global Y axis (keeps orbit level).
                camera.yaw += yaw_axis * camera.controller_look_speed * delta_time;

                // Pitch is a rotation around the camera's local X axis (tilt up/down).
                camera.pitch += pitch_axis * camera.controller_look_speed * delta_time;

                // Look input: mouse drag only for editor cameras.
                const glm::ivec2 mouse_xy{ mouse.x, mouse.y };
                glm::ivec2 mouse_xy_diff{ 0, 0 };

                // Compute drag delta in pixels while the left button is held.
                if (mouse.leftButton && camera.mouse_prev.x >= 0)
                    mouse_xy_diff = camera.mouse_prev - mouse_xy;

                camera.mouse_prev = mouse_xy;

                camera.yaw += static_cast<float>(mouse_xy_diff.x) * camera.mouse_sensitivity;
                camera.pitch += static_cast<float>(mouse_xy_diff.y) * camera.mouse_sensitivity;

                // Clamp pitch to keep the camera above the horizon (no flip).
                camera.pitch = glm::clamp(camera.pitch, -glm::radians(89.0f), 0.0f);
            }
            else
            {
                // Keep mouse state in sync so activation doesn't cause a jump.
                camera.mouse_prev = glm::ivec2(mouse.x, mouse.y);
            }

            // Final look-at point = target position + user-controlled offset.
            if (camera.target.is_bound())
                camera.look_at = target_pos + camera.target_offset;

            // Orbit vector: rotate a unit vector around yaw/pitch and scale by distance.
            const glm::vec4 orbit_offset =
                glm_aux::R(camera.yaw, camera.pitch) *
                glm::vec4(0.0f, 0.0f, camera.distance, 1.0f);

            // Camera position = pivot + orbit offset.
            camera.position = camera.look_at + glm::vec3(orbit_offset);

            // Forward direction points from camera to the pivot.
            camera.forward = glm::normalize(camera.look_at - camera.position);

            // Cache view matrices for rendering and other systems.
            camera.model_to_view = glm::lookAt(camera.position, camera.look_at, camera.up);
            camera.view_to_world = glm::inverse(camera.model_to_view);
        }
    }
} // namespace eeng::editor
