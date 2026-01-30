// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <format>
#include <string>

namespace eeng::ecs
{
    // Mannequin-specific controller bindings for driving animation graph parameters.
    struct MannequinPlayerControllerComponent
    {
        std::string name;
        bool enabled = true;

        // -1 selects the first connected controller; otherwise treated as controller instance id.
        int controller_id = -1;
        bool use_keyboard_fallback = true;
        bool use_mouse_aim = true;

        float stick_deadzone = 0.2f;
        float trigger_deadzone = 0.1f;
        float mouse_sensitivity = 0.01f;

        // Animation graph parameter bindings.
        std::string move_x_param = "U_LT";
        std::string move_y_param = "V_LT";
        std::string aim_x_param = "U_RT";
        std::string aim_y_param = "V_RT";
        std::string jump_param = "DO_JUMP";
        std::string fire_param = "DO_FIRE_RIFLE";
        std::string reload_param = "DO_RELOAD_RIFLE";
        std::string hit0_param = "DO_HITREACTION0";
        std::string hit1_param = "DO_HITREACTION1";
        std::string hit2_param = "DO_HITREACTION2";
        std::string hit3_param = "DO_HITREACTION3";

        // Runtime state (not serialized).
        float aim_x = 0.0f;
        float aim_y = 0.0f;
        int mouse_x = 0;
        int mouse_y = 0;
        bool has_mouse_origin = false;
        bool last_jump = false;
        bool last_fire = false;
        bool last_reload = false;
        bool last_hit0 = false;
        bool last_hit1 = false;
        bool last_hit2 = false;
        bool last_hit3 = false;
    };

    inline std::string to_string(const MannequinPlayerControllerComponent& t)
    {
        return std::format("MannequinPlayerControllerComponent(name = {} ...)", t.name);
    }

    template<typename Visitor>
    void visit_asset_refs(MannequinPlayerControllerComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(MannequinPlayerControllerComponent&, Visitor&&) {}
}
