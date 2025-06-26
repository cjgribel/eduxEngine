// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef ResourceTypes_h
#define ResourceTypes_h
#include <cstddef>
#include <cstdint>
#include "Handle.h"
#include "Log.hpp"
// #include "hash_combine.h"

namespace eeng
{
    struct MockResource1
    {
        int x{};
        float y{};

        // Default constructor
        MockResource1() {
            eeng::Log("MockResource1 default-constructed");
        }

        // Copy constructor
        MockResource1(const MockResource1& other)
            : x(other.x), y(other.y)
        {
            eeng::Log("MockResource1 copy-constructed");
        }

        // Copy assignment
        MockResource1& operator=(const MockResource1& other)
        {
            if (this != &other) {
                x = other.x;
                y = other.y;
                eeng::Log("MockResource1 copy-assigned");
            }
            return *this;
        }

        // Move constructor
        MockResource1(MockResource1&& other) noexcept
            : x(other.x), y(other.y)
        {
            other.x = 0;
            other.y = 0;
            eeng::Log("MockResource1 move-constructed");
        }

        // Move assignment
        MockResource1& operator=(MockResource1&& other) noexcept
        {
            if (this != &other) {
                x = other.x;
                y = other.y;
                other.x = 0;
                other.y = 0;
                eeng::Log("MockResource1 move-assigned");
            }
            return *this;
        }

        // Equality
        bool operator==(const MockResource1& other) const
        {
            return x == other.x && y == other.y;
        }

        // Destructor
        ~MockResource1() {
            eeng::Log("MockResource1 destroyed");
        }
    };

    struct MockResource2
    {
        size_t y = 0;
        Handle<MockResource1> ref1; // Reference to another resource

        bool operator==(const MockResource2& other) const
        {
            return y == other.y && ref1 == other.ref1;
        }
    };

} // namespace eeng

#endif // ResourceTypes_h
