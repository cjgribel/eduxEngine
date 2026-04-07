// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <string>
#include <vector>

#include "assets/types/AnimationGraphAsset.hpp"

namespace eeng::assets
{
    std::vector<std::string> validate_animation_graph(const AnimationGraphAsset& graph);
}
