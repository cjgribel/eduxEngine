// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <bitset>
#include <cstdint>
#include <cassert>

namespace eeng::physics::legacy
{
    // Legacy collision layer mask (pair-indexed bitset).
    template<int NbrLayers>
    class CollisionLayerMask
    {
    public:
        static inline std::bitset<NbrLayers * (NbrLayers + 1) / 2> layer_mapping;

        // Packed index for a symmetric layer pair.
        static inline int pair_index(std::uint32_t i, std::uint32_t j)
        {
            assert(i < static_cast<std::uint32_t>(NbrLayers));
            assert(j < static_cast<std::uint32_t>(NbrLayers));
            std::uint32_t i_ = i;
            std::uint32_t j_ = j;
            if (i_ > j_)
                std::swap(i_, j_);
            return static_cast<int>(NbrLayers * i_ - 0.5f * (i_ * i_ + i_) + j_);
        }

        // Query whether two layers collide.
        static inline bool check(std::uint32_t i, std::uint32_t j)
        {
            return layer_mapping[pair_index(i, j)];
        }

        // Update a layer pair.
        static inline void set(std::uint32_t i, std::uint32_t j, bool flag)
        {
            layer_mapping[pair_index(i, j)] = flag;
        }

        // Set all layer pairs to the same value.
        static inline void set_all(bool flag)
        {
            if (flag)
                layer_mapping.set();
            else
                layer_mapping.reset();
        }
    };
} // namespace eeng::physics::legacy
