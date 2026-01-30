// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ecs/FirstPersonCameraSystem.hpp"

#include "EngineContext.hpp"
#include "engineapi/IInputManager.hpp"
#include "glmcommon.hpp"
#include "editor/ecs/FirstPersonCameraComponent.hpp"

#include <cmath>

namespace
{
    float apply_deadzone(float value, float deadzone)
    {
        // Treat small controller drift as zero.
        if (std::fabs(value) <= deadzone)
            return 0.0f;
        return value;
    }

    glm::vec3 keyboard_move_axes(const eeng::IInputManager& input)
    {
        using Key = eeng::IInputManager::Key;
        const bool w = input.IsKeyPressed(Key::W);
        const bool a = input.IsKeyPressed(Key::A);
        const bool s = input.IsKeyPressed(Key::S);
        const bool d = input.IsKeyPressed(Key::D);
        const bool q = input.IsKeyPressed(Key::Q);
        const bool e = input.IsKeyPressed(Key::E);

        // X is strafe, Y is forward/back, Z is vertical.
        return glm::vec3(
            (d ? 1.0f : 0.0f) + (a ? -1.0f : 0.0f),
            (w ? 1.0f : 0.0f) + (s ? -1.0f : 0.0f),
            (e ? 1.0f : 0.0f) + (q ? -1.0f : 0.0f));
    }
}

namespace eeng::editor
{
    void FirstPersonCameraSystem::update(entt::registry& registry, EngineContext& ctx, float delta_time)
    {
        auto* input = ctx.input_manager.get();
        if (!input)
            return;

        const bool has_controller = input->GetConnectedControllerCount() > 0;
        const auto* controller = has_controller ? &input->GetControllerState(0) : nullptr;
        const auto mouse = input->GetMouseState();

        constexpr float kStickDeadzone = 0.2f;

        auto view = registry.view<FirstPersonCameraComponent>();
        for (auto entity : view)
        {
            auto& camera = view.get<FirstPersonCameraComponent>(entity);

            if (camera.active)
            {
                // Read movement/look input only for the active camera.
                // Movement input: controller if present, otherwise WASD.
                glm::vec3 move_axes{};
                if (controller)
                {
                    move_axes.x = apply_deadzone(controller->axisLeftX, kStickDeadzone);
                    move_axes.y = apply_deadzone(-controller->axisLeftY, kStickDeadzone);
                }
                else
                {
                    move_axes = keyboard_move_axes(*input);
                }

                // Look input: controller right stick or mouse drag.
                if (controller)
                {
                    camera.yaw += apply_deadzone(controller->axisRightX, kStickDeadzone) *
                        camera.controller_look_speed * delta_time;
                    camera.pitch += apply_deadzone(-controller->axisRightY, kStickDeadzone) *
                        camera.controller_look_speed * delta_time;
                }
                else
                {
                    const glm::ivec2 mouse_xy{ mouse.x, mouse.y };
                    glm::ivec2 mouse_xy_diff{ 0, 0 };

                    // Compute drag delta in pixels while the left button is held.
                    if (mouse.leftButton && camera.mouse_prev.x >= 0)
                        mouse_xy_diff = camera.mouse_prev - mouse_xy;

                    camera.mouse_prev = mouse_xy;

                    camera.yaw += static_cast<float>(mouse_xy_diff.x) * camera.mouse_sensitivity;
                    camera.pitch += static_cast<float>(mouse_xy_diff.y) * camera.mouse_sensitivity;
                }

                // Clamp pitch to avoid gimbal lock when looking straight up/down.
                camera.pitch = glm::clamp(camera.pitch, -glm::radians(89.0f), glm::radians(89.0f));

                // Build a view-facing basis from yaw/pitch.
                const glm::vec3 forward =
                    glm::normalize(glm::vec3(glm_aux::R(camera.yaw, camera.pitch) *
                        glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
                const glm::vec3 right = glm::normalize(glm::cross(forward, camera.up));

                // Convert input to world-space translation (forward/right basis + world up).
                const glm::vec3 move_delta =
                    (forward * move_axes.y + right * move_axes.x + camera.up * move_axes.z) *
                    camera.move_speed * delta_time;

                camera.position += move_delta;
                camera.forward = forward;
            }
            else
            {
                // Keep mouse state in sync so activation doesn't cause a jump.
                camera.mouse_prev = glm::ivec2(mouse.x, mouse.y);
            }

            if (!camera.active)
            {
                // Still compute forward from the current yaw/pitch when inactive.
                camera.forward =
                    glm::normalize(glm::vec3(glm_aux::R(camera.yaw, camera.pitch) *
                        glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            }

            // Cache view matrices for rendering and other systems.
            camera.model_to_view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
            camera.view_to_world = glm::inverse(camera.model_to_view);
        }
    }
} // namespace eeng::editor
