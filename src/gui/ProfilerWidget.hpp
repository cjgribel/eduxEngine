// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"

namespace eeng::gui
{
    struct ProfilerWidget
    {
        EngineContext& ctx;

        explicit ProfilerWidget(EngineContext& ctx);

        void draw();
    };
} // namespace eeng::gui
