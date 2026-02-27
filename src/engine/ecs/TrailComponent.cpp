// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "TrailComponent.hpp"
#include <format>

namespace eeng::ecs
{

    std::string to_string(const TrailComponent& t)
    {
        return std::format(
            "TrailComponent()");
    }
}
