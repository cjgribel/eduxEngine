// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <entt/entt.hpp>
#include <cstdint>
#include <unordered_map>

namespace eeng
{
    struct EngineContext;
}

namespace eeng::ecs::systems
{
    class PhysicsSystem;

    class ConstraintSystem
    {
    public:
        void update(
            entt::registry& registry,
            EngineContext& ctx,
            PhysicsSystem& physics_system,
            float delta_time);

    private:
        enum class ConstraintKind : std::uint8_t
        {
            Point,
            Hinge,
            Slider,
            SixDofSpring
        };

        struct ConstraintKey
        {
            entt::entity entity{ entt::null };
            ConstraintKind kind{};

            bool operator==(const ConstraintKey& other) const
            {
                return entity == other.entity && kind == other.kind;
            }
        };

        struct ConstraintKeyHash
        {
            std::size_t operator()(const ConstraintKey& key) const noexcept
            {
                std::size_t seed = 0;
                seed ^= std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(entt::to_integral(key.entity)))
                    + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
                seed ^= std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(key.kind))
                    + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
                return seed;
            }
        };

        std::unordered_map<ConstraintKey, std::uint32_t, ConstraintKeyHash> handles_;
    };
} // namespace eeng::ecs::systems
