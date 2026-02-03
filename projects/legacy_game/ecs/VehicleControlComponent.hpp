// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <format>
#include <string>

namespace eeng::ecs
{
    struct VehicleControlComponent
    {
        std::string name = "VehicleControl";
        bool enabled = true;

        int controller_id = -1;
        bool use_keyboard_fallback = true;
        float stick_deadzone = 0.1f;
        float trigger_deadzone = 0.05f;

        float steer_limit = 0.8f;
        float steer_speed = 3.0f;
        float steer_max_impulse = 250.0f;

        float drive_velocity = 25.0f;
        float drive_max_impulse = 750.0f;
        float brake_max_impulse = 200.0f;

        // Runtime state.
        float steer_input = 0.0f;
        float drive_input = 0.0f;
        float steer_target = 0.0f;
        float steer_angle = 0.0f;
    };

    inline std::string to_string(const VehicleControlComponent& t)
    {
        return std::format("VehicleControlComponent(name = {} ...)", t.name);
    }

    template<typename Visitor>
    void visit_asset_refs(VehicleControlComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(VehicleControlComponent&, Visitor&&) {}
} // namespace eeng::ecs
