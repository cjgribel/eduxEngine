// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/Entity.hpp"
#include <glm/glm.hpp>
#include <format>
#include <string>
#include <vector>

namespace eeng::ecs
{
    // VehicleRig1 prototype linkage data.
    // This is intentionally minimal and only reflects the constraint setup we use right now.
    struct VehicleRig1WheelLink
    {
        // Core rigid bodies.
        EntityRef knuckle{};
        EntityRef wheel{};

        // Constraints that define the rig (split suspension + drive).
        EntityRef suspension_6dof{};
        EntityRef axle_hinge{};

        // Flags used by the control system.
        bool steerable = false;
        bool driven = false;

        // Optional per-wheel sign flips (handedness, axis parity, etc).
        float drive_direction = 1.0f;
        float steer_direction = 1.0f;

        // Geometry and axis definitions in chassis local space.
        glm::vec3 mount_local{ 0.0f };
        glm::vec3 suspension_axis{ 0.0f, -1.0f, 0.0f };
        glm::vec3 axle_axis{ 0.0f, 0.0f, 1.0f };
        glm::vec3 wheel_local_anchor{ 0.0f };

        // Suspension metadata (stored for debug/monitoring).
        float suspension_rest_length = 0.0f;
        float suspension_travel = 0.5f;
    };

    // VehicleRig1 prototype rig component stored on the rig root entity.
    struct VehicleRig1RigComponent
    {
        EntityRef chassis{};
        glm::vec3 steer_axis{ 0.0f, 1.0f, 0.0f };
        std::vector<VehicleRig1WheelLink> wheels;
    };

    inline std::string to_string(const VehicleRig1RigComponent& rig)
    {
        return std::format("VehicleRig1RigComponent(wheels = {})", rig.wheels.size());
    }

    template<typename Visitor>
    void visit_entity_refs(VehicleRig1RigComponent& rig, Visitor&& visitor)
    {
        visitor(rig.chassis);
        for (auto& wheel : rig.wheels)
        {
            visitor(wheel.knuckle);
            visitor(wheel.wheel);
            visitor(wheel.suspension_6dof);
            visitor(wheel.axle_hinge);
        }
    }
} // namespace eeng::ecs
