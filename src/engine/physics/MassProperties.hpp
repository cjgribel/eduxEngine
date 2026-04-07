// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eeng::physics
{
    // Mass properties for a rigid body in 3D.
    // - center_of_mass is expressed in the same local frame as inertia.
    // - inertia is the 3x3 tensor about the center of mass.
    struct MassProperties3d
    {
        float mass = 0.0f;
        glm::mat3 inertia{ 0.0f };
        glm::vec3 center_of_mass{ 0.0f };
    };

    // Outer product v * v^T (used by the parallel axis theorem).
    glm::mat3 outer_product(const glm::vec3& v);

    // Combine two mass property sets with the parallel axis theorem.
    MassProperties3d combine(const MassProperties3d& a, const MassProperties3d& b);

    // Solid primitive mass properties.
    MassProperties3d mass_properties_box(const glm::vec3& half_extents, float density = 1.0f);
    MassProperties3d mass_properties_sphere(float radius, float density = 1.0f);
    // Capsule aligned with +Z (height is the cylinder length, not including hemispheres).
    MassProperties3d mass_properties_capsule_z(float radius, float height, float density = 1.0f);

    // Convex mesh mass properties from a triangle list.
    // NOTE: Assumes a closed, consistently wound convex mesh.
    MassProperties3d mass_properties_convex_mesh(const glm::vec3* vertices,
        std::size_t vertex_count,
        const std::uint32_t* indices,
        std::size_t index_count,
        float density = 1.0f);

    // Rotate and translate mass properties while keeping inertia about the COM.
    MassProperties3d transform_mass_properties(
        const MassProperties3d& props,
        const glm::vec3& translation,
        const glm::quat& rotation);
} // namespace eeng::physics
