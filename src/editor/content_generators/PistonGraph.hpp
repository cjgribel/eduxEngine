// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <string>
#include "assets/types/AnimationGraphAsset.hpp"

namespace eeng::editor::content_generators
{
    assets::AnimationGraphAsset build_piston_graph(
        const std::string& graph_name,
        const std::string& clip_name);
}
