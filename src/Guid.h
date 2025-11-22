// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#include <cstdint>       // uint64_t
#include <random>        // std::random_device, std::mt19937_64
#include <string>        // std::string, std::to_string
#include <sstream>
#include <iomanip>

#include <iostream>    // debug output

namespace eeng
{
    class Guid
    {
    public:
        using underlying_type = uint64_t;

        Guid() : value(invalid().value) {}
        explicit Guid(underlying_type val) : value(val) {}

        static Guid generate()
        {
            static thread_local std::mt19937_64 rng(std::random_device{}());
            return Guid(rng());
        }

        static Guid from_string(const std::string& str)
        {
            std::istringstream iss(str);
            uint64_t high = 0;
            uint64_t mid = 0;
            uint64_t low = 0;

            char dash1, dash2;
            iss >> std::hex >> high >> dash1 >> mid >> dash2 >> low;
            if (iss.fail() || dash1 != '-' || dash2 != '-')
            {
                throw std::invalid_argument("Invalid GUID string format: " + str);
            }

            uint64_t value = (high << 32) | (mid << 16) | low;

            // Debug output
            std::cout << "Parsed GUID string: " << str << " to value: " << value << std::endl;

            return Guid(value);
        }

        static Guid invalid() { return Guid(0); }

        bool valid() const { return value != 0; }

        bool operator==(const Guid& other) const { return value == other.value; }
        bool operator!=(const Guid& other) const { return value != other.value; }
        bool operator<(const Guid& other) const { return value < other.value; }

        underlying_type raw() const { return value; }

        // std::string to_string() const { return std::to_string(value); }
        std::string to_string() const
        {
            std::ostringstream oss;
            oss << std::hex << std::setfill('0')
                << std::setw(8) << ((value >> 32) & 0xFFFFFFFF) << '-'
                << std::setw(4) << ((value >> 16) & 0xFFFF) << '-'
                << std::setw(4) << (value & 0xFFFF);
            return oss.str();
        }

    private:
        underlying_type value;
    };
}

namespace std {
    template<>
    struct hash<eeng::Guid>
    {
        size_t operator()(const eeng::Guid& guid) const noexcept
        {
            return std::hash<uint64_t>()(guid.raw());
        }
    };
}