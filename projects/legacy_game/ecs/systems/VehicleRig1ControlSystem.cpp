// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/VehicleRig1ControlSystem.hpp"

#include "engineapi/IInputManager.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/VehicleRig1ControlComponent.hpp"
#include "ecs/VehicleRig1Component.hpp"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace eeng::ecs::systems
{
    namespace
    {
        // Zero small values so sticks/triggers don't jitter.
        float apply_deadzone(float value, float deadzone)
        {
            return (std::abs(value) < deadzone) ? 0.0f : value;
        }

        // Clamp to a standard input range.
        float clamp_axis(float value)
        {
            return std::clamp(value, -1.0f, 1.0f);
        }

        // First-order approach toward a target at a fixed rate.
        float approach(float value, float target, float rate, float dt)
        {
            const float delta = target - value;
            const float step = rate * dt;
            if (std::abs(delta) <= step)
                return target;
            return value + (delta > 0.0f ? step : -step);
        }

        // Select a controller by id (or first available if id < 0).
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

    void VehicleRig1ControlSystem::update(entt::registry& registry, EngineContext& ctx, float dt)
    {
        auto* input = ctx.input_manager.get();
        if (!input)
            return;

        // Process every rig root that has both control and rig components.
        auto view = registry.view<ecs::VehicleRig1ControlComponent, ecs::VehicleRig1RigComponent>();
        for (const auto entity : view)
        {
            auto& control = view.get<ecs::VehicleRig1ControlComponent>(entity);
            auto& rig = view.get<ecs::VehicleRig1RigComponent>(entity);

            if (!control.enabled || !rig.chassis.is_bound())
                continue;

            // --- Input sampling -------------------------------------------------
            const auto* controller_state = select_controller(*input, control.controller_id);
            const bool use_controller = controller_state != nullptr;
            const bool use_keyboard = !use_controller && control.use_keyboard_fallback;

            float steer_input = 0.0f;
            float drive_input = 0.0f;

            if (use_controller)
            {
                steer_input = apply_deadzone(controller_state->axisLeftX, control.stick_deadzone);
                const float throttle = controller_state->triggerRight;
                const float brake = controller_state->triggerLeft;
                drive_input = (throttle > control.trigger_deadzone ? throttle : 0.0f)
                    - (brake > control.trigger_deadzone ? brake : 0.0f);
            }
            else if (use_keyboard)
            {
                const float steer_target =
                    (input->IsKeyPressed(eeng::IInputManager::Key::D) ? 1.0f : 0.0f)
                    - (input->IsKeyPressed(eeng::IInputManager::Key::A) ? 1.0f : 0.0f);
                const float drive_target =
                    (input->IsKeyPressed(eeng::IInputManager::Key::W) ? 1.0f : 0.0f)
                    - (input->IsKeyPressed(eeng::IInputManager::Key::S) ? 1.0f : 0.0f);

                // Emulate analog input for keyboard by smoothing toward the target.
                steer_input = approach(control.steer_input, steer_target, 6.0f, dt);
                drive_input = approach(control.drive_input, drive_target, 6.0f, dt);
            }

            // Clamp and store inputs for UI/debug.
            steer_input = clamp_axis(steer_input);
            drive_input = clamp_axis(drive_input);
            control.steer_input = steer_input;
            control.drive_input = drive_input;

            // --- Steering target ------------------------------------------------
            control.steer_target = steer_input * control.steer_limit;

            // Rate-limit the steering angle so the servo target moves smoothly.
            const float steer_delta = control.steer_target - control.steer_angle;
            const float max_step = control.steer_speed * dt;
            if (std::abs(steer_delta) <= max_step)
                control.steer_angle = control.steer_target;
            else
                control.steer_angle += (steer_delta > 0.0f ? max_step : -max_step);

            // --- Apply steering + drive ----------------------------------------
            for (auto& wheel : rig.wheels)
            {
                // Steering: 6DoF servo on the suspension axis (constraint frame X).
                if (wheel.steerable && wheel.suspension_6dof.is_bound())
                {
                    auto* sixdof = registry.try_get<ecs::SixDofSpringConstraintComponent>(wheel.suspension_6dof.entity);
                    if (sixdof)
                    {
                        const float steer_sign = (std::abs(wheel.steer_direction) < 1e-3f)
                            ? 1.0f
                            : (wheel.steer_direction > 0.0f ? 1.0f : -1.0f);
                        float target_angle = control.steer_angle * steer_sign;

                        // Respect angular limits if they are in a valid range.
                        if (sixdof->angular_limit_min.x <= sixdof->angular_limit_max.x)
                        {
                            target_angle = glm::clamp(
                                target_angle,
                                sixdof->angular_limit_min.x,
                                sixdof->angular_limit_max.x);
                        }

                        // Servo steering: drive to the target angle with a capped speed.
                        sixdof->angular_motor_enabled.x = 1.0f;
                        sixdof->angular_servo_enabled.x = 1.0f;
                        sixdof->angular_servo_target.x = target_angle;
                        sixdof->angular_motor_target_velocity.x = control.steer_speed;
                        sixdof->angular_motor_max_force.x = control.steer_max_impulse;
                    }
                }
                else if (wheel.suspension_6dof.is_bound())
                {
                    // Clear any stale steering motor state on non-steerable wheels.
                    if (auto* sixdof = registry.try_get<ecs::SixDofSpringConstraintComponent>(wheel.suspension_6dof.entity))
                    {
                        sixdof->angular_motor_enabled.x = 0.0f;
                        sixdof->angular_servo_enabled.x = 0.0f;
                        sixdof->angular_servo_target.x = 0.0f;
                        sixdof->angular_motor_target_velocity.x = 0.0f;
                        sixdof->angular_motor_max_force.x = 0.0f;
                    }
                }

                // Drive: hinge motor on the wheel axle.
                if (wheel.driven && wheel.axle_hinge.is_bound())
                {
                    auto* hinge = registry.try_get<ecs::HingeConstraintComponent>(wheel.axle_hinge.entity);
                    if (!hinge)
                        continue;

                    const float drive_sign = (std::abs(wheel.drive_direction) < 1e-3f)
                        ? 1.0f
                        : (wheel.drive_direction > 0.0f ? 1.0f : -1.0f);
                    const bool driving = std::abs(drive_input) > 1e-3f;

                    hinge->enable_motor = true;
                    // Use the motor as a brake when there is no drive input.
                    hinge->motor_target_velocity = driving
                        ? drive_input * control.drive_velocity * drive_sign
                        : 0.0f;
                    hinge->motor_max_impulse = driving
                        ? control.drive_max_impulse
                        : control.brake_max_impulse;
                }
                else if (wheel.axle_hinge.is_bound())
                {
                    // Ensure non-driven wheels do not keep a motor enabled.
                    if (auto* hinge = registry.try_get<ecs::HingeConstraintComponent>(wheel.axle_hinge.entity))
                    {
                        hinge->enable_motor = false;
                        hinge->motor_target_velocity = 0.0f;
                        hinge->motor_max_impulse = 0.0f;
                    }
                }
            }
        }
    }
} // namespace eeng::ecs::systems
