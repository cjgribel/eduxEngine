// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "Guid.h"
#include "Handle.h"

#pragma once

namespace eeng
{
    class IResourceManager
    {
    public:
        virtual std::string to_string() const = 0;
        virtual ~IResourceManager() = default;
    };
} // namespace eeng