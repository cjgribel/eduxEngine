// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <memory>
#include "ILogManager.hpp"

namespace eeng::LogGlobals
{
    /// Sets the global logger (typically set once in Engine::init)
    void set_logger(std::weak_ptr<ILogManager> logger);

    /// Logs a formatted message using the global logger, if available
    void log(const char* fmt, ...);

    /// Clears the log if logger is available
    void clear();

    /// Access raw logger pointer (nullptr if not available)
    ILogManager* try_get();
}
