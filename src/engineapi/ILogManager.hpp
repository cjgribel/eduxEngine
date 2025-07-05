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
        virtual ~ILogManager() = default;

        virtual void log(const char* fmt, ...) = 0;
        virtual void clear() = 0;
    };
}
