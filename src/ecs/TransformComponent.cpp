// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "TransformComponent.hpp"
#include <format>

namespace eeng::ecs
{
    // std::string eeng::ecs::Transform::to_string() const
    std::string to_string(const TransformComponent& t)
    {
        return std::format(
            "Transform(pos = [{}, {}, {}])",
            t.position.x,
            t.position.y,
            t.position.z);
    }
}
