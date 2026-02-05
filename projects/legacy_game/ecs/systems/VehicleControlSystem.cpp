// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/VehicleControlSystem.hpp"

#include "engineapi/IInputManager.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"
#include "ecs/VehicleControlComponent.hpp"
#include "ecs/VehicleRigComponent.hpp"

#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

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

        float approach(float value, float target, float rate, float dt)
        {
            const float delta = target - value;
            const float step = rate * dt;
            if (std::abs(delta) <= step)
                return target;
            return value + (delta > 0.0f ? step : -step);
        }

        glm::vec3 normalize_or_default(const glm::vec3& v, const glm::vec3& fallback)
        {
            const float len2 = glm::dot(v, v);
            if (len2 <= 1e-8f)
                return fallback;
            return v / std::sqrt(len2);
        }

        float signed_angle_around_axis(
            const glm::quat& from,
            const glm::quat& to,
            const glm::vec3& axis)
        {
            const glm::quat rel = glm::normalize(glm::inverse(from) * to);
            glm::vec3 rel_axis(rel.x, rel.y, rel.z);
            const float sin_half = glm::length(rel_axis);
            if (sin_half <= 1e-6f)
                return 0.0f;

            rel_axis /= sin_half;
            float angle = 2.0f * std::atan2(sin_half, rel.w);
            if (angle > glm::pi<float>())
                angle -= 2.0f * glm::pi<float>();

            const float sign = (glm::dot(rel_axis, axis) >= 0.0f) ? 1.0f : -1.0f;
            return angle * sign;
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

    void VehicleControlSystem::update(entt::registry& registry, EngineContext& ctx, float dt)
    {
        auto* input = ctx.input_manager.get();
        if (!input)
            return;

        auto view = registry.view<ecs::VehicleControlComponent, ecs::VehicleRigComponent>();
        for (const auto entity : view)
        {
            auto& control = view.get<ecs::VehicleControlComponent>(entity);
            auto& rig = view.get<ecs::VehicleRigComponent>(entity);

            if (!control.enabled || !rig.chassis.is_bound())
                continue;

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

                // Emulate analog stick for keyboard input.
                steer_input = approach(control.steer_input, steer_target, 6.0f, dt);
                drive_input = approach(control.drive_input, drive_target, 6.0f, dt);
            }

            // Clamp inputs to [-1, 1] to keep downstream math stable.
            steer_input = clamp_axis(steer_input);
            drive_input = clamp_axis(drive_input);

            // Cache inputs for UI/debug and compute a bounded steering target (radians).
            control.steer_input = steer_input;
            control.drive_input = drive_input;
            control.steer_target = steer_input * control.steer_limit;

            // Integrate steer angle toward the target with a max rate to avoid snapping.
            const float steer_delta = control.steer_target - control.steer_angle;
            const float max_step = control.steer_speed * dt;
            if (std::abs(steer_delta) <= max_step)
                control.steer_angle = control.steer_target;
            else
                control.steer_angle += (steer_delta > 0.0f ? max_step : -max_step);

            auto* chassis_tfm = registry.try_get<ecs::TransformComponent>(rig.chassis.entity);
            if (!chassis_tfm)
                continue;

            const glm::vec3 steer_axis =
                normalize_or_default(rig.steer_axis, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::vec3 steer_axis_world =
                normalize_or_default(chassis_tfm->rotation * steer_axis, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::quat steer_rot =
                glm::angleAxis(control.steer_angle, steer_axis);

            static int steer_dbg_frame = 0;
            const bool log_steer = ((++steer_dbg_frame % 60) == 0);
            bool steer_logged = false;

            for (auto& wheel : rig.wheels)
            {
                const bool has_knuckle = wheel.knuckle.is_bound();
                const bool has_sixdof = wheel.suspension_6dof.is_bound();
                const bool has_axle_hinge = wheel.axle_hinge.is_bound();
                const bool has_steering_hinge = wheel.steering_hinge.is_bound()
                    && registry.valid(static_cast<entt::entity>(wheel.steering_hinge.entity))
                    && registry.any_of<ecs::HingeConstraintComponent>(wheel.steering_hinge.entity);
                const bool steer_via_sixdof = wheel.steerable && has_sixdof && !has_steering_hinge;
                const bool drive_via_sixdof = has_sixdof && !has_axle_hinge;
                const float drive_sign = (std::abs(wheel.drive_direction) < 1e-3f)
                    ? 1.0f
                    : (wheel.drive_direction > 0.0f ? 1.0f : -1.0f);
                const float steer_sign = (std::abs(wheel.steer_direction) < 1e-3f)
                    ? 1.0f
                    : (wheel.steer_direction > 0.0f ? 1.0f : -1.0f);
                const glm::vec3 wheel_steer_axis_local =
                    normalize_or_default(wheel.suspension_axis, steer_axis);
                const glm::vec3 wheel_steer_axis_world =
                    normalize_or_default(chassis_tfm->rotation * wheel_steer_axis_local, steer_axis_world);

                if (steer_via_sixdof || drive_via_sixdof)
                {
                    auto* sixdof = registry.try_get<ecs::SixDofSpringConstraintComponent>(wheel.suspension_6dof.entity);
                    if (sixdof)
                    {
                        if (steer_via_sixdof)
                        {
                            float measured_angle = control.steer_angle;
                            if (has_knuckle)
                            {
                                if (auto* knuckle_tfm = registry.try_get<ecs::TransformComponent>(wheel.knuckle.entity))
                                {
                                    measured_angle = signed_angle_around_axis(
                                        chassis_tfm->rotation,
                                        knuckle_tfm->rotation,
                                        wheel_steer_axis_world);
                                }
                            }

                            measured_angle *= steer_sign;
                            // Keep the debug value in sync with the physical constraint.
                            control.steer_angle = measured_angle;

                            // Steering via motor:
                            // - With input: drive directly with input sign (robust against angle sign issues).
                            // - Without input: recenter toward 0 using a proportional velocity.
                            float target_angle = control.steer_target * steer_sign;
                            if (sixdof->angular_limit_min.x <= sixdof->angular_limit_max.x)
                            {
                                target_angle = glm::clamp(
                                    target_angle,
                                    sixdof->angular_limit_min.x,
                                    sixdof->angular_limit_max.x);
                            }

                            // Servo-only steering: stick controls the target angle directly.
                            sixdof->angular_motor_enabled.x = 1.0f;
                            sixdof->angular_servo_enabled.x = 1.0f;
                            sixdof->angular_servo_target.x = target_angle;
                            // Bullet uses target velocity as a speed limit for servo motion.
                            sixdof->angular_motor_target_velocity.x = control.steer_speed;
                            sixdof->angular_motor_max_force.x = control.steer_max_impulse;

                            if (log_steer && !steer_logged)
                            {
                                std::cout
                                    << "SteerCtrl input=" << control.steer_input
                                    << " target=" << control.steer_target
                                    << " measured=" << measured_angle
                                    << " targetA=" << target_angle
                                    << " sign=" << steer_sign
                                    << " maxF=" << control.steer_max_impulse
                                    << "\n";
                                steer_logged = true;
                            }
                        }
                        else
                        {
                            sixdof->angular_motor_enabled.x = 0.0f;
                            sixdof->angular_servo_enabled.x = 0.0f;
                            sixdof->angular_servo_target.x = 0.0f;
                            sixdof->angular_motor_target_velocity.x = 0.0f;
                            sixdof->angular_motor_max_force.x = 0.0f;
                        }

                        if (drive_via_sixdof)
                        {
                            const bool driving = wheel.driven;
                            const bool drive_active = driving && (std::abs(drive_input) > 1e-3f);
                            sixdof->angular_motor_enabled.y = driving ? 1.0f : 0.0f;
                            sixdof->angular_servo_enabled.y = 0.0f;
                            sixdof->angular_servo_target.y = 0.0f;
                            sixdof->angular_motor_target_velocity.y = drive_active
                                ? drive_input * control.drive_velocity * drive_sign
                                : 0.0f;
                            sixdof->angular_motor_max_force.y = drive_active
                                ? control.drive_max_impulse
                                : (driving ? control.brake_max_impulse : 0.0f);
                        }
                    }
                }

                if (has_knuckle && rig.kinematic_knuckle)
                {
                    auto* knuckle_tfm = registry.try_get<ecs::TransformComponent>(wheel.knuckle.entity);
                    if (knuckle_tfm)
                    {
                        glm::vec3 knuckle_pos = chassis_tfm->position
                            + (chassis_tfm->rotation
                                * (wheel.mount_local + wheel.suspension_axis * wheel.suspension_rest_length));
                        if (wheel.wheel.is_bound())
                        {
                            if (auto* wheel_tfm = registry.try_get<ecs::TransformComponent>(wheel.wheel.entity))
                                knuckle_pos = wheel_tfm->position;
                        }

                        knuckle_tfm->position = knuckle_pos;
                        if (wheel.steerable)
                            knuckle_tfm->rotation = glm::normalize(chassis_tfm->rotation * steer_rot);
                        else
                            knuckle_tfm->rotation = chassis_tfm->rotation;
                        knuckle_tfm->mark_local_dirty();
                    }
                }

                const bool steer_via_axle = !steer_via_sixdof && wheel.axle_hinge.is_bound()
                    && (rig.kinematic_knuckle || !has_knuckle);
                if (steer_via_axle)
                {
                    auto* axle = registry.try_get<ecs::HingeConstraintComponent>(wheel.axle_hinge.entity);
                    if (axle)
                    {
                        glm::vec3 axis = wheel.axle_axis;
                        if (wheel.steerable)
                            axis = steer_rot * axis;
                        axle->local_axis_a = axis;
                        axle->local_axis_b = axis;
                    }
                }

                if (!steer_via_sixdof && wheel.steerable && !rig.kinematic_knuckle
                    && has_knuckle && has_steering_hinge)
                {
                    auto* hinge = registry.try_get<ecs::HingeConstraintComponent>(wheel.steering_hinge.entity);
                    if (hinge)
                    {
                        // Servo-style steering: always drive toward the target angle (recentre at idle).
                        float measured_angle = control.steer_angle;
                        if (auto* knuckle_tfm = registry.try_get<ecs::TransformComponent>(wheel.knuckle.entity))
                        {
                            measured_angle = signed_angle_around_axis(
                                chassis_tfm->rotation,
                                knuckle_tfm->rotation,
                                wheel_steer_axis_world);
                            // Keep the debug value in sync with the physical hinge.
                            control.steer_angle = measured_angle;
                        }

                        const float steer_error = control.steer_target - measured_angle;
                        const float max_vel = control.steer_speed;
                        const float vel = glm::clamp(steer_error * 4.0f, -max_vel, max_vel);

                        hinge->enable_motor = true;
                        hinge->motor_target_velocity = vel;
                        hinge->motor_max_impulse = control.steer_max_impulse;
                    }
                }

                if (!drive_via_sixdof && wheel.driven && wheel.axle_hinge.is_bound())
                {
                    auto* hinge = registry.try_get<ecs::HingeConstraintComponent>(wheel.axle_hinge.entity);
                    if (!hinge)
                        continue;

                    const bool driving = std::abs(drive_input) > 1e-3f;
                    hinge->enable_motor = true;
                    hinge->motor_target_velocity = driving ? drive_input * control.drive_velocity * drive_sign : 0.0f;
                    hinge->motor_max_impulse = driving ? control.drive_max_impulse : control.brake_max_impulse;
                }
            }
        }
    }
} // namespace eeng::ecs::systems
