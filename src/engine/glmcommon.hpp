// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef glmcommob_h
#define glmcommob_h
#pragma once

#include <sstream>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace glm_aux {

    constexpr glm::vec3 vec3_000{ 0.0f, 0.0f, 0.0f };
    constexpr glm::vec3 vec3_100{ 1.0f, 0.0f, 0.0f };
    constexpr glm::vec3 vec3_010{ 0.0f, 1.0f, 0.0f };
    constexpr glm::vec3 vec3_001{ 0.0f, 0.0f, 1.0f };
    constexpr glm::vec3 vec3_111{ 1.0f, 1.0f, 1.0f };

    struct Ray
    {
        glm::vec3 origin, dir;
        float z_near = std::numeric_limits<float>::max();

        Ray();
        Ray(const glm::vec3& origin, const glm::vec3& dir);
        glm::vec3 point_of_contact() const;
        operator bool();
    };

    // Integer representation of a floating-point value.


    bool intersect_ray_AABB(
        Ray& ray,
        const glm::vec3& aabb_min,
        const glm::vec3& aabb_max
    );

    /// @brief Convert glm::vec3 to string
    /// @param vec Vector to convert
    std::string to_string(const glm::vec3& vec);

    /// @brief Convert glm::vec4 to string
    /// @param vec Vector to convert
    std::string to_string(const glm::vec4& vec);

    /// @brief Convert glm::mat4 to string
    /// @param matrix Matrix to convert
    std::string to_string(const glm::mat4& mat);

    /**
     * @brief Creates a translation matrix.
     * @param translation A 3D vector representing the translation.
     * @return A 4x4 translation matrix.
     */
    glm::mat4 T(
        const glm::vec3& translation);

    /**
     * @brief Creates a rotation matrix.
     * @param angle The rotation angle in radians.
     * @param axis A 3D vector representing the axis of rotation (must be normalized).
     * @return A 4x4 rotation matrix.
     */
    glm::mat4 R(
        float angle,
        const glm::vec3& axis);

    /**
     * @brief Creates a rotation matrix from yaw and pitch
     * @param yaw Rotation angle around y in radians.
     * @param pitch Rotation angle around x in radians.
     * @return A 4x4 rotation matrix.
     */
    glm::mat4 R(
        float yaw,
        float pitch);

    /**
     * @brief Creates a scaling matrix.
     * @param scale A 3D vector representing the scale along each axis.
     * @return A 4x4 scaling matrix.
     */
    glm::mat4 S(
        const glm::vec3& scale);

    /**
     * @brief Creates a combined Translation * Rotation matrix.
     * @param translation A 3D vector representing the translation.
     * @param angle The rotation angle in radians.
     * @param axis A 3D vector representing the axis of rotation (must be normalized).
     * @return A 4x4 matrix representing Translation * Rotation.
     */
    glm::mat4 TR(
        const glm::vec3& translation,
        float angle,
        const glm::vec3& axis);

    /**
     * @brief Creates a combined Translation * Scale matrix.
     * @param translation A 3D vector representing the translation.
     * @param scale A 3D vector representing the scale along each axis.
     * @return A 4x4 matrix representing Translation * Scale.
     */
    glm::mat4 TS(
        const glm::vec3& translation,
        const glm::vec3& scale);

    /**
     * @brief Creates a combined Rotation * Scale matrix.
     * @param angle The rotation angle in radians.
     * @param axis A 3D vector representing the axis of rotation (must be normalized).
     * @param scale A 3D vector representing the scale along each axis.
     * @return A 4x4 matrix representing Rotation * Scale.
     */
    glm::mat4 RS(
        float angle,
        const glm::vec3& axis,
        const glm::vec3& scale);

    /**
     * @brief Creates a combined Translation * Rotation * Scale matrix.
     * @param translation A 3D vector representing the translation.
     * @param angle The rotation angle in radians.
     * @param axis A 3D vector representing the axis of rotation (must be normalized).
     * @param scale A 3D vector representing the scale along each axis.
     * @return A 4x4 matrix representing Translation * Rotation * Scale.
     */
    glm::mat4 TRS(
        const glm::vec3& translation,
        float angle,
        const glm::vec3& axis,
        const glm::vec3& scale);

    /**
     * @brief Creates a viewport transformation matrix for mapping normalized device coordinates (NDC)
     *        to the specified viewport rectangle.
     *
     * This function generates a 4x4 matrix that transforms coordinates from the normalized device
     * coordinate (NDC) space, which ranges from [-1, 1], to the viewport rectangle specified by
     * the given position, size, and depth range. It is commonly used in OpenGL to implement viewport
     * transformations.
     *
     * @param x      The x-coordinate of the viewport's bottom-left corner in screen space.
     * @param y      The y-coordinate of the viewport's bottom-left corner in screen space.
     * @param width  The width of the viewport in pixels.
     * @param height The height of the viewport in pixels.
     * @param near   The near depth range value (typically 0.0).
     * @param far    The far depth range value (typically 1.0).
     * @return glm::mat4 A 4x4 matrix representing the viewport transformation.
     *
     * @note The depth range maps [0, 1] in normalized device coordinates to [near, far] in viewport space.
     *
     * @example
     * // Create a viewport matrix for a window-sized viewport (800x600).
     * glm::mat4 viewportMatrix = create_viewport_matrix(0, 0, 800, 600, 0, 1);
     *
     * // Transform an NDC coordinate (-1, -1, 0.5) to viewport space.
     * glm::vec4 ndc = glm::vec4(-1.0f, -1.0f, 0.5f, 1.0f);
     * glm::vec4 viewportCoords = viewportMatrix * ndc;
     */
    glm::mat4 create_viewport_matrix(
        float x,
        float y,
        float width,
        float height,
        float near,
        float far);

    /**
     * @brief Computes a world-space ray from 2D window coordinates, given view, projection, and viewport matrices.
     *
     * This function converts the provided window coordinates to normalized device coordinates (NDC) using the inverse
     * of the viewport matrix, and then unprojects these NDC points into world space using the inverse view-projection matrix.
     *
     * @param windowCoordinates The 2D coordinates in window space (e.g., mouse position).
     * @param viewMatrix The view matrix, representing the camera transformation.
     * @param projectionMatrix The projection matrix, representing the perspective or orthographic projection.
     * @param viewportMatrix The matrix that maps NDC coordinates to window coordinates (requires inversion here).
     * @return Ray The resulting ray in world space, containing an origin and a normalized direction.
     */
    Ray world_ray_from_window_coords(
        const glm::ivec2& window_coords,
        const glm::mat4& V,
        const glm::mat4& P,
        const glm::mat4& VP);

    /// @brief Transforms a world position to window/screen coordinates.
    /// @param world_pos The position in world space.
    /// @param VP_PROJ_V The combined Viewport, Projection, and View matrix (Viewport * Projection * View).
    /// @param window_coord Output parameter for the resulting 2D window coordinate (in screen space).
    /// @return True if the position is in front of the near-plane, false otherwise.
    bool window_coords_from_world_pos(
        const glm::vec3& world_pos,
        const glm::mat4& VP_PROJ_V,
        glm::ivec2& window_coords);

}

#endif