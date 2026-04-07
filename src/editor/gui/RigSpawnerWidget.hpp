// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"

namespace eeng::gui
{
    struct RigSpawnerWidget
    {
        explicit RigSpawnerWidget(EngineContext& ctx)
            : ctx(ctx)
        {
        }

        void draw();

    private:
        EngineContext& ctx;
    };
} // namespace eeng::gui
