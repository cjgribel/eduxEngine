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

        glm::vec3 normalize_or_default(const glm::vec3& v, const glm::vec3& fallback)
        {
            const float len2 = glm::dot(v, v);
            if (len2 <= 1e-8f)
                return fallback;
            return v / std::sqrt(len2);
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
                steer_input = (input->IsKeyPressed(eeng::IInputManager::Key::D) ? 1.0f : 0.0f)
                    - (input->IsKeyPressed(eeng::IInputManager::Key::A) ? 1.0f : 0.0f);
                drive_input = (input->IsKeyPressed(eeng::IInputManager::Key::W) ? 1.0f : 0.0f)
                    - (input->IsKeyPressed(eeng::IInputManager::Key::S) ? 1.0f : 0.0f);
            }

            steer_input = clamp_axis(steer_input);
            drive_input = clamp_axis(drive_input);

            control.steer_input = steer_input;
            control.drive_input = drive_input;
            control.steer_target = steer_input * control.steer_limit;

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
            const glm::quat steer_rot =
                glm::angleAxis(control.steer_angle, steer_axis);

            for (auto& wheel : rig.wheels)
            {
                const bool has_knuckle = wheel.knuckle.is_bound();
                const bool has_sixdof = wheel.suspension_6dof.is_bound();

                if (has_sixdof)
                {
                    auto* sixdof = registry.try_get<ecs::SixDofSpringConstraintComponent>(wheel.suspension_6dof.entity);
                    if (sixdof)
                    {
                        const bool steering = wheel.steerable;
                        const bool driving = wheel.driven;

                        sixdof->angular_motor_enabled.x = steering ? 1.0f : 0.0f;
                        sixdof->angular_servo_enabled.x = steering ? 1.0f : 0.0f;
                        sixdof->angular_servo_target.x = steering ? control.steer_angle : 0.0f;
                        sixdof->angular_motor_target_velocity.x = 0.0f;
                        sixdof->angular_motor_max_force.x = steering ? control.steer_max_impulse : 0.0f;

                        const bool drive_active = driving && (std::abs(drive_input) > 1e-3f);
                        sixdof->angular_motor_enabled.y = driving ? 1.0f : 0.0f;
                        sixdof->angular_servo_enabled.y = 0.0f;
                        sixdof->angular_servo_target.y = 0.0f;
                        sixdof->angular_motor_target_velocity.y = drive_active
                            ? drive_input * control.drive_velocity
                            : 0.0f;
                        sixdof->angular_motor_max_force.y = drive_active
                            ? control.drive_max_impulse
                            : (driving ? control.brake_max_impulse : 0.0f);
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

                const bool steer_via_axle = !has_sixdof && wheel.axle_hinge.is_bound()
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

                if (!has_sixdof && wheel.steerable && !rig.kinematic_knuckle
                    && has_knuckle && wheel.steering_hinge.is_bound())
                {
                    auto* hinge = registry.try_get<ecs::HingeConstraintComponent>(wheel.steering_hinge.entity);
                    if (hinge)
                    {
                        hinge->enable_motor = true;
                        hinge->motor_target_velocity = steer_input * control.steer_speed;
                        hinge->motor_max_impulse = control.steer_max_impulse;
                    }
                }

                if (!has_sixdof && wheel.driven && wheel.axle_hinge.is_bound())
                {
                    auto* hinge = registry.try_get<ecs::HingeConstraintComponent>(wheel.axle_hinge.entity);
                    if (!hinge)
                        continue;

                    const bool driving = std::abs(drive_input) > 1e-3f;
                    hinge->enable_motor = true;
                    hinge->motor_target_velocity = driving ? drive_input * control.drive_velocity : 0.0f;
                    hinge->motor_max_impulse = driving ? control.drive_max_impulse : control.brake_max_impulse;
                }
            }
        }
    }
} // namespace eeng::ecs::systems
