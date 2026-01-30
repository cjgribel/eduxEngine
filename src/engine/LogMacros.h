// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"

#define EENG_LOG_COLOR_INFO   eeng::ILogManager::LogColor::rgba(0.55f, 0.85f, 1.0f, 1.0f)
#define EENG_LOG_COLOR_WARN   eeng::ILogManager::LogColor::rgba(1.0f, 0.75f, 0.2f, 1.0f)
#define EENG_LOG_COLOR_ERROR  eeng::ILogManager::LogColor::rgba(1.0f, 0.25f, 0.25f, 1.0f)
#define EENG_LOG_COLOR_DEBUG  eeng::ILogManager::LogColor::rgba(0.7f, 0.7f, 0.7f, 1.0f)

#define EENG_LOG_COLORED(ctx, color, ...)  (ctx)->log_manager->log(color, __VA_ARGS__)

#define EENG_LOG(ctx, ...)        (ctx)->log_manager->log(__VA_ARGS__)
#define EENG_LOG_INFO(ctx, ...)   (ctx)->log_manager->log(EENG_LOG_COLOR_INFO, "[INFO] " __VA_ARGS__)
#define EENG_LOG_WARN(ctx, ...)   (ctx)->log_manager->log(EENG_LOG_COLOR_WARN, "[WARN] " __VA_ARGS__)
#define EENG_LOG_ERROR(ctx, ...)  (ctx)->log_manager->log(EENG_LOG_COLOR_ERROR, "[ERROR] " __VA_ARGS__)

#define EENG_LOG_DEBUG(ctx, ...)  (ctx)->log_manager->log(EENG_LOG_COLOR_DEBUG, "[DEBUG] " __VA_ARGS__)
