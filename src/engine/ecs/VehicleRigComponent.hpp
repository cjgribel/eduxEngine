// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/Entity.hpp"
#include <glm/glm.hpp>
#include <format>
#include <string>
#include <vector>

namespace eeng::ecs
{
    struct VehicleWheelLink
    {
        EntityRef knuckle{};
        EntityRef wheel{};
        EntityRef suspension_slider{};
        EntityRef suspension_spring{};
        EntityRef suspension_6dof{};
        EntityRef steering_hinge{};
        EntityRef axle_hinge{};

        bool steerable = false;
        bool driven = false;
        float drive_direction = 1.0f;
        float steer_direction = 1.0f;
        float steer_neutral_angle = 0.0f;

        glm::vec3 mount_local{ 0.0f };
        glm::vec3 suspension_axis{ 0.0f, -1.0f, 0.0f };
        glm::vec3 axle_axis{ 0.0f, 0.0f, 1.0f };
        glm::vec3 wheel_local_anchor{ 0.0f };

        float suspension_rest_length = 0.0f;
        float suspension_travel = 0.5f;
    };

    struct VehicleRigComponent
    {
        EntityRef chassis{};
        bool kinematic_knuckle = true;
        glm::vec3 steer_axis{ 0.0f, 1.0f, 0.0f };
        std::vector<VehicleWheelLink> wheels;
    };

    inline std::string to_string(const VehicleRigComponent& rig)
    {
        return std::format("VehicleRigComponent(wheels = {})", rig.wheels.size());
    }

    template<typename Visitor>
    void visit_entity_refs(VehicleRigComponent& rig, Visitor&& visitor)
    {
        visitor(rig.chassis);
        for (auto& wheel : rig.wheels)
        {
            visitor(wheel.knuckle);
            visitor(wheel.wheel);
            visitor(wheel.suspension_slider);
            visitor(wheel.suspension_spring);
            visitor(wheel.suspension_6dof);
            visitor(wheel.steering_hinge);
            visitor(wheel.axle_hinge);
        }
    }
} // namespace eeng::ecs
