// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "AABB.h"

#include <cmath>
#include <glm/glm.hpp>

namespace eeng::physics::legacy
{
    // Legacy mass property helpers (basic primitives for now).
    // TODO: port tetrahedral mesh mass properties when needed.
    struct MassProperties3d
    {
        float mass = 0.0f;
        glm::mat3 inertia{ 1.0f };
        glm::vec3 center_of_mass{ 0.0f };
    };

    // Helper for parallel-axis adjustments.
    inline glm::mat3 outer_product(const glm::vec3& v)
    {
        glm::mat3 out(1.0f);
        out[0][0] = v.x * v.x; out[0][1] = v.y * v.x; out[0][2] = v.z * v.x;
        out[1][0] = v.x * v.y; out[1][1] = v.y * v.y; out[1][2] = v.z * v.y;
        out[2][0] = v.x * v.z; out[2][1] = v.y * v.z; out[2][2] = v.z * v.z;
        return out;
    }

    // Combine mass properties with a parallel-axis shift.
    inline MassProperties3d combine(const MassProperties3d& a, const MassProperties3d& b)
    {
        MassProperties3d combined{};
        combined.mass = a.mass + b.mass;
        if (combined.mass <= 0.0f)
            return combined;

        combined.center_of_mass =
            (a.center_of_mass * a.mass + b.center_of_mass * b.mass) / combined.mass;

        const glm::mat3 identity(1.0f);
        const glm::vec3 da = a.center_of_mass - combined.center_of_mass;
        const glm::vec3 db = b.center_of_mass - combined.center_of_mass;

        combined.inertia = a.inertia + b.inertia
            + a.mass * (glm::dot(da, da) * identity - outer_product(da))
            + b.mass * (glm::dot(db, db) * identity - outer_product(db));

        return combined;
    }

    // Box mass properties from half-extents and density.
    inline MassProperties3d mass_properties_box(const glm::vec3& half_extents, float density = 1.0f)
    {
        const glm::vec3 size = half_extents * 2.0f;
        const float volume = size.x * size.y * size.z;
        const float mass = volume * density;

        const float x2 = size.x * size.x;
        const float y2 = size.y * size.y;
        const float z2 = size.z * size.z;
        const float factor = mass / 12.0f;

        MassProperties3d props{};
        props.mass = mass;
        props.inertia = glm::mat3(1.0f);
        props.inertia[0][0] = factor * (y2 + z2);
        props.inertia[1][1] = factor * (x2 + z2);
        props.inertia[2][2] = factor * (x2 + y2);
        props.center_of_mass = glm::vec3(0.0f);
        return props;
    }

    // Sphere mass properties from radius and density.
    inline MassProperties3d mass_properties_sphere(float radius, float density = 1.0f)
    {
        constexpr float kPi = 3.14159265358979323846f;
        const float volume = (4.0f / 3.0f) * kPi * radius * radius * radius;
        const float mass = volume * density;
        const float inertia_diag = 0.4f * mass * radius * radius;

        MassProperties3d props{};
        props.mass = mass;
        props.inertia = glm::mat3(1.0f);
        props.inertia[0][0] = inertia_diag;
        props.inertia[1][1] = inertia_diag;
        props.inertia[2][2] = inertia_diag;
        props.center_of_mass = glm::vec3(0.0f);
        return props;
    }

    // AABB mass properties using its extents and center.
    inline MassProperties3d mass_properties_from_aabb(const AABB& aabb, float density = 1.0f)
    {
        const glm::vec3 half_extents = (aabb.max - aabb.min) * 0.5f;
        MassProperties3d props = mass_properties_box(half_extents, density);
        props.center_of_mass = (aabb.min + aabb.max) * 0.5f;
        return props;
    }
} // namespace eeng::physics::legacy
