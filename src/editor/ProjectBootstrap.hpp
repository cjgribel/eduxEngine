// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "editor/ProjectConfig.hpp"

namespace eeng
{
    struct EngineContext;
}

namespace eeng::editor
{
    bool bootstrap_project(EngineContext& ctx, const ProjectConfig& config);
} // namespace eeng::editor
