// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
// #include <cstddef>

namespace eeng
{
    class IGuiManager
    {
    public:
        virtual void init() = 0;
        virtual void release() = 0;

        virtual ~IGuiManager() = default;
    };
} // namespace eeng