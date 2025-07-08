// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"

#define EENG_LOG_(...)        (ctx)->log_manager->log(__VA_ARGS__)

#define EENG_LOG(ctx, ...)        (ctx)->log_manager->log(__VA_ARGS__)
#define EENG_LOG_INFO(ctx, ...)   (ctx)->log_manager->log("[INFO] " __VA_ARGS__)
#define EENG_LOG_WARN(ctx, ...)   (ctx)->log_manager->log("[WARN] " __VA_ARGS__)
#define EENG_LOG_ERROR(ctx, ...)  (ctx)->log_manager->log("[ERROR] " __VA_ARGS__)
