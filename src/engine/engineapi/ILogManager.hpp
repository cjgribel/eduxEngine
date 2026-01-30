// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

// ILogManager.hpp
#pragma once

namespace eeng
{
    class ILogManager
    {
    public:
        struct Widget;
        struct LogColor
        {
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            float a = 1.0f;
            bool has_color = false;

            static constexpr LogColor rgba(float r, float g, float b, float a = 1.0f)
            {
                return LogColor{r, g, b, a, true};
            }
        };

        virtual ~ILogManager() = default;

        virtual void log(const char* fmt, ...) = 0;
        virtual void log(const LogColor& color, const char* fmt, ...) = 0;
        virtual void clear() = 0;
    };
}
