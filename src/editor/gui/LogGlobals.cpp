// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "LogGlobals.hpp"
#include <cstdarg>
#include <string>
#include <cstdio>

namespace
{
    std::weak_ptr<eeng::ILogManager> g_logger;

    std::string vformat(const char* fmt, va_list args)
    {
        va_list args_copy;
        va_copy(args_copy, args);
        int len = vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);

        std::string result(len + 1, '\0');
        vsnprintf(&result[0], len + 1, fmt, args);
        result.resize(len);
        return result;
    }
}

namespace eeng::LogGlobals
{
    void set_logger(std::weak_ptr<ILogManager> logger)
    {
        g_logger = std::move(logger);
    }

    void log(const char* fmt, ...)
    {
        if (auto logger = g_logger.lock())
        {
            va_list args;
            va_start(args, fmt);
            std::string msg = vformat(fmt, args);
            va_end(args);
            logger->log("%s", msg.c_str());
        }
    }

    void clear()
    {
        if (auto logger = g_logger.lock())
        {
            logger->clear();
        }
    }

    ILogManager* try_get()
    {
        if (auto logger = g_logger.lock())
            return logger.get();
        return nullptr;
    }
}
