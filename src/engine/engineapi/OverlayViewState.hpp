// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <glm/glm.hpp>

namespace eeng
{
    // View state used for overlay/debug rendering (gizmos, helpers, selections).
    struct OverlayViewState
    {
        glm::mat4 view{ 1.0f };
        glm::mat4 proj{ 1.0f };
        glm::mat4 viewport{ 1.0f };
        glm::ivec2 window_size{ 0, 0 };
        bool valid = false;
    };
} // namespace eeng
