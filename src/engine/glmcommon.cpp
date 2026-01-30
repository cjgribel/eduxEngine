// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#include "glmcommon.hpp"

namespace
{
#define IR(x)    ((udword&)x)
#define RayAABBEps 0.00001f
    typedef unsigned int udword;

    inline bool RayAABBIntersection(
        const glm_aux::Ray& ray,
        const glm::vec3& aabb_min,
        const glm::vec3& aabb_max,
        float& t_min)
    {
        const glm::vec3& origin = ray.origin;
        const glm::vec3& dir = ray.dir;
        glm::vec3 coord;

        bool Inside = true;
        const glm::vec3 MinB = aabb_min; // aabb.mCenter - aabb.mExtents;
        const glm::vec3 MaxB = aabb_max; // aabb.mCenter + aabb.mExtents;
        glm::vec3 MaxT{ -1.0f, -1.0f, -1.0f };

        //    Point MinB = aabb.mCenter - aabb.mExtents;
        //    Point MaxB = aabb.mCenter + aabb.mExtents;
        //    Point MaxT;
        //    MaxT.x=MaxT.y=MaxT.z=-1.0f;

            // Find candidate planes.
        for (udword i = 0;i < 3;i++)
        {
            if (origin[i] < MinB[i])
            {
                coord[i] = MinB[i];
                Inside = false;

                // Calculate T distances to candidate planes
                if (IR(dir[i]))
                    MaxT[i] = (MinB[i] - origin[i]) / dir[i];
            }
            else if (origin[i] > MaxB[i])
            {
                coord[i] = MaxB[i];
                Inside = false;

                // Calculate T distances to candidate planes
                if (IR(dir[i]))
                    MaxT[i] = (MaxB[i] - origin[i]) / dir[i];
            }
        }

        // Ray origin inside bounding box
        if (Inside)
        {
            coord = origin;
            return true;
        }

        // Get largest of the maxT's for final choice of intersection
        udword WhichPlane = 0;
        if (MaxT[1] > MaxT[WhichPlane])    WhichPlane = 1;
        if (MaxT[2] > MaxT[WhichPlane])    WhichPlane = 2;

        // Check final candidate actually inside box (MaxT is > 0)
        if (IR(MaxT[WhichPlane]) & 0x80000000) return false;

        for (udword i = 0; i < 3; i++)
        {
            if (i != WhichPlane)
            {
                coord[i] = origin[i] + MaxT[WhichPlane] * dir[i];
#ifdef RayAABBEps
                if (coord[i] < MinB[i] - RayAABBEps || coord[i] > MaxB[i] + RayAABBEps)    return false;
#else
                if (coord[i] < MinB[i] || coord[i] > MaxB[i])    return false;
#endif
            }
        }

        t_min = MaxT[WhichPlane];
        return true;    // ray hits box
    }
}

namespace glm_aux {

    Ray::Ray() {}

    Ray::Ray(
        const glm::vec3& origin,
        const glm::vec3& dir)
        : origin(origin), dir(dir) {
    }

    glm::vec3 Ray::point_of_contact() const { return origin + dir * z_near; }

    Ray::operator bool() { return z_near < std::numeric_limits<float>::max(); }

    bool intersect_ray_AABB(
        Ray& ray,
        const glm::vec3& aabb_min,
        const glm::vec3& aabb_max
    )
    {
        float t_min{};
        if (!RayAABBIntersection(ray, aabb_min, aabb_max, t_min)) return false;

        if (t_min > ray.z_near) return false;

        ray.z_near = t_min;
        return true;
    }

    std::string to_string(const glm::vec3& vec)
    {
        std::ostringstream oss;
        oss << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
        return oss.str();
    }

    std::string to_string(const glm::vec4& vec)
    {
        std::ostringstream oss;
        oss << "(" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << ")";
        return oss.str();
    }

    std::string to_string(const glm::mat4& mat)
    {
        std::ostringstream oss;
        oss << "[";
        for (int i = 0; i < 4; ++i) {
            oss << "(";
            for (int j = 0; j < 4; ++j) {
                oss << mat[i][j];
                if (j < 3) oss << ", ";
            }
            oss << ")";
            if (i < 3) oss << ", ";
        }
        oss << "]";
        return oss.str();
    }

    glm::mat4 T(
        const glm::vec3& translation)
    {
        return glm::translate(glm::mat4(1.0f), translation);
    }

    glm::mat4 R(
        float angle,
        const glm::vec3& axis)
    {
        return glm::rotate(glm::mat4(1.0f), angle, axis);
    }

    glm::mat4 R(
        float yaw,
        float pitch)
    {
        const float sin_yaw = sin(yaw);
        const float cos_yaw = cos(yaw);
        const float sin_pitch = sin(pitch);
        const float cos_pitch = cos(pitch);

        return glm::mat4(
            glm::vec4(cos_yaw, 0.0f, -sin_yaw, 0.0f),
            glm::vec4(sin_yaw * sin_pitch, cos_pitch, cos_yaw * sin_pitch, 0.0f),
            glm::vec4(sin_yaw * cos_pitch, -sin_pitch, cos_yaw * cos_pitch, 0.0f),
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
        );
    }

    glm::mat4 S(
        const glm::vec3& scale)
    {
        return glm::scale(glm::mat4(1.0f), scale);
    }

    glm::mat4 TR(
        const glm::vec3& translation,
        float angle,
        const glm::vec3& axis)
    {
        return T(translation) * R(angle, axis); // Translation * Rotation
    }

    glm::mat4 TS(
        const glm::vec3& translation,
        const glm::vec3& scale)
    {
        return T(translation) * S(scale); // Translation * Scale
    }

    glm::mat4 RS(
        float angle,
        const glm::vec3& axis,
        const glm::vec3& scale)
    {
        return R(angle, axis) * S(scale); // Rotation * Scale
    }

    glm::mat4 TRS(
        const glm::vec3& translation,
        float angle,
        const glm::vec3& axis,
        const glm::vec3& scale)
    {
        return T(translation) * R(angle, axis) * S(scale); // Translation * Rotation * Scale
    }

    glm::mat4 create_viewport_matrix(
        float x,
        float y,
        float width,
        float height,
        float near,
        float far)
    {
        glm::mat4 viewportMatrix(1.0f);

        // Scale part
        viewportMatrix[0][0] = width / 2.0f;
        viewportMatrix[1][1] = height / 2.0f;
        viewportMatrix[2][2] = far - near;

        // Translation part
        viewportMatrix[3][0] = x + width / 2.0f;
        viewportMatrix[3][1] = y + height / 2.0f;
        viewportMatrix[3][2] = near;

        return viewportMatrix;
    }

    Ray world_ray_from_window_coords(
        const glm::ivec2& window_coords,
        const glm::mat4& V,
        const glm::mat4& P,
        const glm::mat4& VP)
    {
        // Step 1: Invert the viewport matrix to map window coordinates to normalized device coordinates (NDC)
        glm::vec4 ndc_coords = glm::inverse(VP) * glm::vec4(window_coords, 0.0f, 1.0f);

        float x = ndc_coords.x; // NDC X-coordinate
        float y = ndc_coords.y; // NDC Y-coordinate
        float z_near = -1.0f;   // Near clip plane in NDC
        float z_far = 1.0f;     // Far clip plane in NDC

        // Step 2: Compute the inverse of the view-projection matrix
        glm::mat4 inverseVP = glm::inverse(P * V);

        // Step 3: Unproject the NDC points to world space
        glm::vec4 nearNDC(x, y, z_near, 1.0f);
        glm::vec4 farNDC(x, y, z_far, 1.0f);

        // Transform NDC points to world space
        glm::vec4 nearWorld = inverseVP * nearNDC;
        glm::vec4 farWorld = inverseVP * farNDC;

        // Perform perspective divide
        nearWorld /= nearWorld.w;
        farWorld /= farWorld.w;

        // Step 4: Define the ray in world space
        glm::vec3 ray_origin = glm::vec3(nearWorld); // Ray origin
        glm::vec3 ray_dir = glm::normalize(glm::vec3(farWorld - nearWorld)); // Ray direction

        return Ray{ ray_origin, ray_dir };
    }

    bool window_coords_from_world_pos(
        const glm::vec3& world_pos,
        const glm::mat4& VP_PROJ_V,
        glm::ivec2& window_coords)
    {
        // Transform World -> View -> Clip -> NDC (unprojected) -> Screen-space (unprojected)
        auto pos_ss = VP_PROJ_V * glm::vec4(world_pos, 1.0f);

        // Cull against near plane
        if (pos_ss.w < 0) return false;

        // Divide by the w-component to project to Screen-space
        int window_x = static_cast<int>(std::round(pos_ss.x / pos_ss.w));
        int window_y = static_cast<int>(std::round(pos_ss.y / pos_ss.w));
        window_coords = glm::ivec2(window_x, window_y);

        return true;
    }
} // namespace glm_aux