// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/Entity.hpp"

#include <format>
#include <string>

#include <glm/glm.hpp>

namespace eeng::ecs
{
    // Drives a linear constraint (Slider or 6DoF) like a hydraulic piston.
    // This is the physical/constraint-only component.
    //
    // Mode (int for inspector support):
    //   0 = Hold, 1 = Extend, 2 = Contract, 3 = Position
    struct PistonConstraintDriveComponent
    {
        std::string name = "PistonConstraintDrive";
        bool enabled = true;

        // Constraint entity holding SliderConstraintComponent or SixDofSpringConstraintComponent.
        EntityRef constraint{};

        // Optional anchor entities used to auto-update constraint axis/anchors.
        EntityRef anchor_a{};
        EntityRef anchor_b{};
        // Anchors are always used to compute the constraint axis/anchors each frame.

        // Local axis used to measure extension (in entity_a local space).
        // When anchors are present, this is refreshed from the anchor delta each frame.
        // If zero, falls back to the constraint frame X axis.
        glm::vec3 axis_local{ 1.0f, 0.0f, 0.0f };

        // Stroke limits in local units along axis.
        float stroke_min = 0.0f;
        float stroke_max = 1.0f;

        // Motor settings.
        float max_force = 2000.0f;
        float max_velocity = 1.0f;

        int mode = 0;                 // 0=Hold, 1=Extend, 2=Contract, 3=Position
        float target_extension = 0.0f; // [0,1] for Position mode

        bool lock_when_idle = true;

        // Runtime feedback.
        float current_position = 0.0f;   // Units along axis.
        float current_extension = 0.0f;  // Normalized [0,1] if limits are valid.
        float command_position = 0.0f;   // Rate-limited target position.
        bool command_initialized = false;
    };

    inline std::string to_string(const PistonConstraintDriveComponent& t)
    {
        return std::format("PistonConstraintDriveComponent(name = {} ...)", t.name);
    }

    template<typename Visitor>
    void visit_asset_refs(PistonConstraintDriveComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(PistonConstraintDriveComponent& comp, Visitor&& visitor)
    {
        visitor(comp.constraint);
        visitor(comp.anchor_a);
        visitor(comp.anchor_b);
    }
} // namespace eeng::ecs
