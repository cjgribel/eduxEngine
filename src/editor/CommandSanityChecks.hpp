// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"

namespace eeng::editor
{
    // Run invariant checks after command execution; logs warnings on anomalies.
    void run_command_sanity_checks(const EngineContext& ctx);
}
