// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "AABB.h"
#include "assets/types/ModelAssets.hpp"

#include <glm/glm.hpp>

namespace eeng::physics
{
    // Compute a model-space AABB from raw vertex positions.
    inline AABB compute_aabb(const assets::ModelDataAsset& model)
    {
        AABB aabb;
        aabb.reset();
        for (const auto& pos : model.positions)
            aabb.grow(pos);
        return aabb;
    }

    // AABB center for positioning colliders.
    inline glm::vec3 aabb_center(const AABB& aabb)
    {
        return (aabb.min + aabb.max) * 0.5f;
    }

    // Validity check (const-safe replacement for AABB::operator bool).
    inline bool is_valid_aabb(const AABB& aabb)
    {
        return aabb.max.x > aabb.min.x
            && aabb.max.y > aabb.min.y
            && aabb.max.z > aabb.min.z;
    }

    // AABB half-extents for box collider sizing.
    inline glm::vec3 aabb_half_extents(const AABB& aabb)
    {
        return (aabb.max - aabb.min) * 0.5f;
    }
} // namespace eeng::physics
