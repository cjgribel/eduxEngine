// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/PistonInputSystem.hpp"

#include "engineapi/IInputManager.hpp"
#include "ecs/PistonConstraintDriveComponent.hpp"
#include "ecs/PistonInputComponent.hpp"

#include <algorithm>
#include <cmath>

namespace eeng::ecs::systems
{
    namespace
    {
        float apply_deadzone(float value, float deadzone)
        {
            return (std::abs(value) < deadzone) ? 0.0f : value;
        }

        float clamp_axis(float value)
        {
            return std::clamp(value, -1.0f, 1.0f);
        }

        const eeng::IInputManager::ControllerState* select_controller(
            const eeng::IInputManager& input,
            int controller_id)
        {
            const auto& controllers = input.GetControllers();
            if (controllers.empty())
                return nullptr;

            if (controller_id >= 0)
            {
                auto it = controllers.find(controller_id);
                if (it != controllers.end())
                    return &it->second;
                return nullptr;
            }

            return &controllers.begin()->second;
        }
    } // namespace

    void PistonInputSystem::update(entt::registry& registry, EngineContext& ctx, float)
    {
        auto* input = ctx.input_manager.get();
        if (!input)
            return;

        auto view = registry.view<ecs::PistonInputComponent, ecs::PistonConstraintDriveComponent>();
        for (const auto entity : view)
        {
            auto& control = view.get<ecs::PistonInputComponent>(entity);
            auto& drive = view.get<ecs::PistonConstraintDriveComponent>(entity);

            if (!control.enabled || !drive.enabled)
                continue;

            const auto* controller_state = select_controller(*input, control.controller_id);
            const bool use_controller = controller_state != nullptr;
            const bool use_keyboard = !use_controller && control.use_keyboard_fallback;

            float drive_input = 0.0f;

            if (use_controller)
            {
                const float throttle = controller_state->triggerRight;
                const float brake = controller_state->triggerLeft;
                drive_input = (throttle > control.trigger_deadzone ? throttle : 0.0f)
                    - (brake > control.trigger_deadzone ? brake : 0.0f);
            }
            else if (use_keyboard)
            {
                // Keyboard fallback: P = extend, O = contract.
                const float extend = input->IsKeyPressed(eeng::IInputManager::Key::P) ? 1.0f : 0.0f;
                const float contract = input->IsKeyPressed(eeng::IInputManager::Key::O) ? 1.0f : 0.0f;
                drive_input = extend - contract;
            }

            drive_input = clamp_axis(drive_input);
            drive_input = apply_deadzone(drive_input, control.input_deadzone);
            control.drive_input = drive_input;

            if (drive_input > 0.0f)
                drive.mode = 1; // Extend
            else if (drive_input < 0.0f)
                drive.mode = 2; // Contract
            else
                drive.mode = 0; // Hold
        }
    }
} // namespace eeng::ecs::systems
