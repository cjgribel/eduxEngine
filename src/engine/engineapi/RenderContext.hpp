// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "engineapi/CameraView.hpp"

#include <cstdint>

namespace eeng
{
    enum class RenderMode : std::uint8_t
    {
        Edit,
        Play
    };

    struct RenderContext
    {
        float time_s = 0.0f;
        int window_width = 0;
        int window_height = 0;
        RenderMode mode = RenderMode::Edit;
        // Explicit active render view chosen by the hosting app for this call.
        CameraView camera_view{};
    };
}
