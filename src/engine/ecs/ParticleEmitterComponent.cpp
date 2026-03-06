// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "ParticleEmitterComponent.hpp"

#include <format>

namespace eeng::ecs
{
    std::string to_string(const ParticleEmitterComponent& t)
    {
        return std::format(
            "ParticleEmitterComponent(enabled={}, emitting={}, max_particles={}, spawn_rate={:.2f})",
            t.enabled,
            t.emitting,
            t.max_particles,
            t.spawn_rate);
    }
}

