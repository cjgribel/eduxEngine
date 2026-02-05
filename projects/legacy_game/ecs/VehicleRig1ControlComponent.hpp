// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <format>
#include <string>

namespace eeng::ecs
{
    // VehicleRig1 prototype: input/control state stored on the rig root.
    struct VehicleRig1ControlComponent
    {
        // Optional label for UI/debug.
        std::string name = "VehicleRig1Control";
        bool enabled = true;

        // Controller selection (controller_id < 0 = first available).
        int controller_id = -1;
        bool use_keyboard_fallback = true;
        float stick_deadzone = 0.1f;
        float trigger_deadzone = 0.05f;

        // Steering tuning (radians, rad/s, impulse).
        float steer_limit = 0.8f;
        float steer_speed = 3.0f;
        float steer_max_impulse = 250.0f;

        // Drive/brake tuning (velocity, impulse).
        float drive_velocity = 25.0f;
        float drive_max_impulse = 750.0f;
        float brake_max_impulse = 200.0f;

        // Runtime state (updated every frame).
        float steer_input = 0.0f;
        float drive_input = 0.0f;
        float steer_target = 0.0f;
        float steer_angle = 0.0f;
    };

    inline std::string to_string(const VehicleRig1ControlComponent& t)
    {
        return std::format("VehicleRig1ControlComponent(name = {} ...)", t.name);
    }

    template<typename Visitor>
    void visit_asset_refs(VehicleRig1ControlComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(VehicleRig1ControlComponent&, Visitor&&) {}
} // namespace eeng::ecs
