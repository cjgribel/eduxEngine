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

    std::string to_string(const ParticleEventsComponent& t)
    {
        return std::format(
            "ParticleEventsComponent(emit_hit_events={}, max_hit_events_per_frame={}, buffered_hits={})",
            t.emit_hit_events,
            t.max_hit_events_per_frame,
            t.hit_events.size());
    }
}
