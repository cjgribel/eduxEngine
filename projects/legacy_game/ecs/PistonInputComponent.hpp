// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <format>
#include <string>

namespace eeng::ecs
{
    struct PistonInputComponent
    {
        std::string name = "PistonInput";
        bool enabled = true;

        int controller_id = -1;           // -1 = first connected.
        bool use_keyboard_fallback = true;
        float trigger_deadzone = 0.05f;
        float input_deadzone = 0.05f;

        // Runtime/debug.
        float drive_input = 0.0f;
    };

    inline std::string to_string(const PistonInputComponent& t)
    {
        return std::format("PistonInputComponent(name = {} ...)", t.name);
    }

    template<typename Visitor>
    void visit_asset_refs(PistonInputComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(PistonInputComponent&, Visitor&&) {}
} // namespace eeng::ecs
