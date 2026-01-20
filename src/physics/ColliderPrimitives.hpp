// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

namespace eeng::physics::legacy
{
    using u32 = std::uint32_t;

    // Legacy primitive vertex data (useful for debug/visualization).
    struct Cube3dData
    {
        static inline constexpr std::array<glm::vec3, 8> vertices = {
            glm::vec3(0.5f, -0.5f, 0.5f),
            glm::vec3(0.5f, 0.5f, 0.5f),
            glm::vec3(-0.5f, 0.5f, 0.5f),
            glm::vec3(-0.5f, -0.5f, 0.5f),
            glm::vec3(0.5f, -0.5f, -0.5f),
            glm::vec3(0.5f, 0.5f, -0.5f),
            glm::vec3(-0.5f, 0.5f, -0.5f),
            glm::vec3(-0.5f, -0.5f, -0.5f)
        };

        static inline constexpr std::array<u32, 24> faces = {
            0, 1, 2, 3,
            4, 7, 6, 5,
            0, 4, 5, 1,
            3, 2, 6, 7,
            1, 5, 6, 2,
            0, 3, 7, 4
        };

        static inline constexpr std::array<u32, 6> face_strides = { 4, 4, 4, 4, 4, 4 };

        static inline constexpr std::array<u32, 24> edges = {
            0, 1,
            0, 3,
            2, 1,
            2, 3,
            4, 5,
            4, 7,
            6, 5,
            6, 7,
            0, 4,
            1, 5,
            2, 6,
            3, 7
        };

        static inline constexpr std::array<u32, 6> unique_edge_dirs = { 0, 1, 0, 3, 0, 4 };
    };

    // Legacy wedge primitive data (debug/visualization).
    struct Wedge3dData
    {
        static inline constexpr std::array<glm::vec3, 6> vertices = {
            glm::vec3(0.5f, -0.5f, 0.5f),
            glm::vec3(0.5f, 0.5f, 0.5f),
            glm::vec3(-0.5f, -0.5f, 0.5f),
            glm::vec3(0.5f, -0.5f, -0.5f),
            glm::vec3(0.5f, 0.5f, -0.5f),
            glm::vec3(-0.5f, -0.5f, -0.5f)
        };

        static inline constexpr std::array<u32, 18> faces = {
            0, 1, 2, 3,
            0, 3, 4, 1,
            1, 4, 5, 2,
            0, 1, 2,
            3, 5, 4
        };

        static inline constexpr std::array<u32, 5> face_strides = { 4, 4, 4, 3, 3 };

        static inline constexpr std::array<u32, 18> edges = {
            0, 1,
            1, 2,
            2, 0,
            0, 3,
            1, 4,
            2, 5,
            3, 4,
            4, 5,
            5, 3
        };

        static inline constexpr std::array<u32, 8> unique_edge_dirs = { 0, 1, 0, 2, 0, 3, 1, 2 };
    };

    // 2D square primitive data (debug/visualization).
    struct Cube2dData
    {
        static inline constexpr std::array<glm::vec2, 4> vertices = {
            glm::vec2(0.5f, -0.5f),
            glm::vec2(0.5f, 0.5f),
            glm::vec2(-0.5f, 0.5f),
            glm::vec2(-0.5f, -0.5f)
        };
    };
} // namespace eeng::physics::legacy
