// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace eeng::physics
{
    struct LinearSpringDamper
    {
        float stiffness = 0.0f;
        float damping = 0.0f;
        float rest_length = 0.0f;

        glm::vec3 compute_force(
            const glm::vec3& anchor_a,
            const glm::vec3& anchor_b,
            const glm::vec3& vel_a,
            const glm::vec3& vel_b) const
        {
            const glm::vec3 r_ab = anchor_a - anchor_b;
            const float dist = glm::length(r_ab);
            if (dist <= 1e-6f)
                return glm::vec3(0.0f);

            const glm::vec3 dir = r_ab / dist;
            const glm::vec3 f_spring = (r_ab - dir * rest_length) * stiffness;
            const float rel_speed = glm::dot(vel_a - vel_b, dir);
            const glm::vec3 f_damper = dir * rel_speed * damping;
            return f_spring + f_damper;
        }
    };

    struct AngularSpringDamper
    {
        float stiffness = 0.0f;
        float damping = 0.0f;
        // Rest rotation from body A to body B.
        glm::quat rest_rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

        glm::vec3 compute_torque(
            const glm::quat& rot_a,
            const glm::quat& rot_b,
            const glm::vec3& ang_vel_a,
            const glm::vec3& ang_vel_b) const
        {
            glm::quat rel = rot_b * glm::inverse(rot_a);
            glm::quat err = rel * glm::inverse(rest_rotation);
            if (err.w < 0.0f)
                err = -err;

            const float w = std::clamp(err.w, -1.0f, 1.0f);
            const float angle = 2.0f * std::acos(w);
            const float sin_half = std::sqrt(std::max(0.0f, 1.0f - w * w));

            glm::vec3 axis(0.0f);
            if (sin_half > 1e-6f)
                axis = glm::vec3(err.x, err.y, err.z) / sin_half;

            const glm::vec3 t_spring = axis * angle * stiffness;
            const glm::vec3 t_damper = (ang_vel_b - ang_vel_a) * damping;
            return t_spring + t_damper;
        }
    };
} // namespace eeng::physics
