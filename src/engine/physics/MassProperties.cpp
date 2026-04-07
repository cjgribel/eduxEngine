// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "physics/MassProperties.hpp"

#include <cmath>
#include <vector>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    // Parallel axis theorem for inertia tensors.
    // I_ref = I_com + m * (|r|^2 * I3 - r r^T)
    glm::mat3 tensor_at(const glm::mat3& inertia_com, float mass, const glm::vec3& r)
    {
        const glm::mat3 identity(1.0f);
        return inertia_com + mass * (glm::dot(r, r) * identity - eeng::physics::outer_product(r));
    }

    // Signed tetrahedron volume.
    // V = det(a - d, b - d, c - d) / 6
    float tetrahedron_volume(const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& c,
        const glm::vec3& d)
    {
        return glm::dot(a - d, glm::cross(b - d, c - d)) / 6.0f;
    }

    glm::vec3 tetrahedron_centroid(const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& c,
        const glm::vec3& d)
    {
        return (a + b + c + d) * 0.25f;
    }

    // Exact tetrahedron mass properties.
    // References:
    // - "Explicit Exact Formulas for the 3-D Tetrahedron Inertia Tensor in Terms of its Vertex Coordinates"
    // - Brian Mirtich, "How to find the inertia tensor (or other mass properties) of a 3D solid body"
    eeng::physics::MassProperties3d tetrahedron_mass_properties(const glm::vec3& p,
        const glm::vec3& q,
        const glm::vec3& r,
        const glm::vec3& s,
        float density)
    {
        // Signed volume (orientation matters).
        const float volume = -tetrahedron_volume(p, q, r, s);
        const float mass = volume * density;
        const glm::vec3 com = tetrahedron_centroid(p, q, r, s);

        // Express vertices in the tetrahedron COM frame for the inertia formula.
        const glm::vec3 a = p - com;
        const glm::vec3 b = q - com;
        const glm::vec3 c = r - com;
        const glm::vec3 d = s - com;

        // Intermediate sums from the closed-form tetrahedron tensor.
        const float j_det = volume * 6.0f;
        const float jabc_x =
            a.x * a.x + a.x * b.x + b.x * b.x +
            a.x * c.x + b.x * c.x + c.x * c.x +
            a.x * d.x + b.x * d.x + c.x * d.x + d.x * d.x;
        const float jabc_y =
            a.y * a.y + a.y * b.y + b.y * b.y +
            a.y * c.y + b.y * c.y + c.y * c.y +
            a.y * d.y + b.y * d.y + c.y * d.y + d.y * d.y;
        const float jabc_z =
            a.z * a.z + a.z * b.z + b.z * b.z +
            a.z * c.z + b.z * c.z + c.z * c.z +
            a.z * d.z + b.z * d.z + c.z * d.z + d.z * d.z;

        const float ja = (jabc_y + jabc_z) / 60.0f;
        const float jb = (jabc_x + jabc_z) / 60.0f;
        const float jc = (jabc_x + jabc_y) / 60.0f;

        const float jap =
            ((2.0f * a.y + b.y + c.y + d.y) * a.z +
             (a.y + 2.0f * b.y + c.y + d.y) * b.z +
             (a.y + b.y + 2.0f * c.y + d.y) * c.z +
             (a.y + b.y + c.y + 2.0f * d.y) * d.z) / 120.0f;

        const float jbp =
            ((2.0f * a.x + b.x + c.x + d.x) * a.z +
             (a.x + 2.0f * b.x + c.x + d.x) * b.z +
             (a.x + b.x + 2.0f * c.x + d.x) * c.z +
             (a.x + b.x + c.x + 2.0f * d.x) * d.z) / 120.0f;

        const float jcp =
            ((2.0f * a.x + b.x + c.x + d.x) * a.y +
             (a.x + 2.0f * b.x + c.x + d.x) * b.y +
             (a.x + b.x + 2.0f * c.x + d.x) * c.y +
             (a.x + b.x + c.x + 2.0f * d.x) * d.y) / 120.0f;

        // Build symmetric inertia tensor about COM.
        glm::mat3 inertia(0.0f);
        inertia[0][0] = ja;
        inertia[0][1] = -jbp;
        inertia[0][2] = -jcp;
        inertia[1][0] = -jbp;
        inertia[1][1] = jb;
        inertia[1][2] = -jap;
        inertia[2][0] = -jcp;
        inertia[2][1] = -jap;
        inertia[2][2] = jc;

        // Scale by density * |det| to keep inertia positive.
        inertia *= density * std::abs(j_det);

        return { mass, inertia, com };
    }
} // namespace

namespace eeng::physics
{
    glm::mat3 outer_product(const glm::vec3& v)
    {
        // Matrix with elements v_i * v_j.
        glm::mat3 out(0.0f);
        out[0] = glm::vec3(v.x * v.x, v.y * v.x, v.z * v.x);
        out[1] = glm::vec3(v.x * v.y, v.y * v.y, v.z * v.y);
        out[2] = glm::vec3(v.x * v.z, v.y * v.z, v.z * v.z);
        return out;
    }

    MassProperties3d combine(const MassProperties3d& a, const MassProperties3d& b)
    {
        // Combine two bodies using the parallel axis theorem.
        MassProperties3d combined{};
        combined.mass = a.mass + b.mass;
        if (combined.mass <= 0.0f)
            return combined;

        combined.center_of_mass =
            (a.center_of_mass * a.mass + b.center_of_mass * b.mass) / combined.mass;

        const glm::vec3 da = a.center_of_mass - combined.center_of_mass;
        const glm::vec3 db = b.center_of_mass - combined.center_of_mass;

        combined.inertia = a.inertia + b.inertia
            + a.mass * (glm::dot(da, da) * glm::mat3(1.0f) - outer_product(da))
            + b.mass * (glm::dot(db, db) * glm::mat3(1.0f) - outer_product(db));

        return combined;
    }

    MassProperties3d mass_properties_box(const glm::vec3& half_extents, float density)
    {
        // Solid box:
        // Ixx = 1/12 m (y^2 + z^2), with full dimensions.
        const glm::vec3 size = half_extents * 2.0f;
        const float volume = size.x * size.y * size.z;
        const float mass = volume * density;

        const float x2 = size.x * size.x;
        const float y2 = size.y * size.y;
        const float z2 = size.z * size.z;
        const float factor = mass / 12.0f;

        glm::mat3 inertia(0.0f);
        inertia[0][0] = factor * (y2 + z2);
        inertia[1][1] = factor * (x2 + z2);
        inertia[2][2] = factor * (x2 + y2);

        return { mass, inertia, glm::vec3(0.0f) };
    }

    MassProperties3d mass_properties_sphere(float radius, float density)
    {
        // Solid sphere:
        // I = 2/5 m r^2 (diagonal).
        const float volume = (4.0f / 3.0f) * kPi * radius * radius * radius;
        const float mass = volume * density;
        const float inertia_diag = 0.4f * mass * radius * radius;

        glm::mat3 inertia(0.0f);
        inertia[0][0] = inertia_diag;
        inertia[1][1] = inertia_diag;
        inertia[2][2] = inertia_diag;

        return { mass, inertia, glm::vec3(0.0f) };
    }

    MassProperties3d mass_properties_capsule_z(float radius, float height, float density)
    {
        // Capsule = cylinder (length = height) + two hemispheres.
        // Axis is +Z in local space.
        if (radius <= 0.0f)
            return {};
        if (height <= 0.0f)
            return mass_properties_sphere(radius, density);

        // Cylinder about its center.
        const float cyl_volume = kPi * radius * radius * height;
        const float cyl_mass = cyl_volume * density;
        const float r2 = radius * radius;
        const float h2 = height * height;

        glm::mat3 cyl_inertia(0.0f);
        cyl_inertia[0][0] = cyl_mass * (3.0f * r2 + h2) / 12.0f;
        cyl_inertia[1][1] = cyl_inertia[0][0];
        cyl_inertia[2][2] = 0.5f * cyl_mass * r2;

        MassProperties3d cylinder{ cyl_mass, cyl_inertia, glm::vec3(0.0f) };

        // Hemisphere about its own centroid.
        // Centroid offset from the flat face along +Z: d = 3r/8.
        const float hemi_volume = (2.0f / 3.0f) * kPi * radius * radius * radius;
        const float hemi_mass = hemi_volume * density;
        const float d = 3.0f * radius / 8.0f;
        const float inertia_z = 0.4f * hemi_mass * r2;
        const float inertia_xy = inertia_z - hemi_mass * d * d;

        glm::mat3 hemi_inertia(0.0f);
        hemi_inertia[0][0] = inertia_xy;
        hemi_inertia[1][1] = inertia_xy;
        hemi_inertia[2][2] = inertia_z;

        MassProperties3d hemi{ hemi_mass, hemi_inertia, glm::vec3(0.0f, 0.0f, d) };

        // Place hemispheres at both ends of the cylinder.
        MassProperties3d hemi_top = hemi;
        hemi_top.center_of_mass.z = height * 0.5f + d;
        MassProperties3d hemi_bottom = hemi;
        hemi_bottom.center_of_mass.z = -height * 0.5f - d;

        MassProperties3d combined = combine(cylinder, hemi_top);
        combined = combine(combined, hemi_bottom);
        return combined;
    }

    MassProperties3d mass_properties_convex_mesh(const glm::vec3* vertices,
        std::size_t vertex_count,
        const std::uint32_t* indices,
        std::size_t index_count,
        float density)
    {
        // Uses tetrahedralization with a Steiner point inside the mesh.
        // This assumes a convex, closed, consistently wound triangle mesh.
        MassProperties3d out{};
        if (!vertices || vertex_count == 0 || !indices || index_count < 3)
            return out;

        glm::vec3 steiner(0.0f);
        for (std::size_t i = 0; i < vertex_count; ++i)
            steiner += vertices[i];
        steiner /= static_cast<float>(vertex_count);

        // Aggregate tetrahedra.
        float total_mass = 0.0f;
        glm::vec3 total_com(0.0f);
        std::vector<MassProperties3d> tetra_props;
        tetra_props.reserve(index_count / 3);

        const std::size_t tri_count = index_count / 3;
        for (std::size_t t = 0; t < tri_count; ++t)
        {
            const std::uint32_t i0 = indices[t * 3 + 0];
            const std::uint32_t i1 = indices[t * 3 + 1];
            const std::uint32_t i2 = indices[t * 3 + 2];
            if (i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count)
                continue;

            const glm::vec3& q = vertices[i0];
            const glm::vec3& r = vertices[i1];
            const glm::vec3& s = vertices[i2];

            MassProperties3d tet = tetrahedron_mass_properties(steiner, q, r, s, density);
            tetra_props.push_back(tet);
            total_mass += tet.mass;
            total_com += tet.center_of_mass * tet.mass;
        }

        if (total_mass <= 0.0f || tetra_props.empty())
            return out;

        total_com /= total_mass;

        glm::mat3 total_inertia(0.0f);
        for (const auto& tet : tetra_props)
        {
            total_inertia += tensor_at(tet.inertia, tet.mass, total_com - tet.center_of_mass);
        }

        out.mass = total_mass;
        out.center_of_mass = total_com;
        out.inertia = total_inertia;
        return out;
    }

    MassProperties3d transform_mass_properties(
        const MassProperties3d& props,
        const glm::vec3& translation,
        const glm::quat& rotation)
    {
        // Rotate inertia into the target frame and translate COM.
        const glm::mat3 rot = glm::mat3_cast(rotation);

        MassProperties3d out = props;
        out.inertia = rot * props.inertia * glm::transpose(rot);
        out.center_of_mass = translation + rot * props.center_of_mass;
        return out;
    }
} // namespace eeng::physics
