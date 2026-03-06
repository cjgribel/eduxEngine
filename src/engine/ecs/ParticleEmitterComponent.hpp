// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "assets/AssetRef.hpp"
#include "assets/types/ModelAssets.hpp"

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace eeng::ecs
{
    enum class ParticleRenderMode : std::uint8_t
    {
        SoftCircle,
        Billboard
    };

    enum class ParticleCollisionMode : std::uint8_t
    {
        None,
        Kill,
        Bounce
    };

    struct ParticleEmitterComponent
    {
        bool enabled = true;
        bool emitting = true;
        bool looping = true;

        // <= 0 means infinite.
        float duration = 0.0f;
        // Runtime accumulator for looping windows.
        float elapsed = 0.0f;

        std::uint32_t max_particles = 1024u;
        float spawn_rate = 120.0f;

        float lifetime_min = 0.35f;
        float lifetime_max = 0.85f;

        float speed_min = 1.0f;
        float speed_max = 3.0f;
        float spread_angle_deg = 22.0f;
        glm::vec3 local_direction{ 0.0f, 1.0f, 0.0f };
        glm::vec3 local_offset{ 0.0f };

        glm::vec3 acceleration{ 0.0f, -9.81f, 0.0f };
        float drag = 0.0f;

        float size_begin = 0.05f;
        float size_end = 0.01f;
        std::uint32_t color_begin = 0xffffa070u;
        std::uint32_t color_end = 0x00a07030u;

        ParticleRenderMode render_mode = ParticleRenderMode::SoftCircle;
        bool use_texture = false;
        AssetRef<assets::GpuTextureAsset> texture_ref{};
        bool additive_blend = true;
        bool depth_write = false;

        // Collision is wired for later phases (raycast-based policy).
        ParticleCollisionMode collision_mode = ParticleCollisionMode::None;
        std::uint32_t collision_layer = 1u;
        std::uint32_t collision_mask = 0xFFFFFFFFu;
        float collision_radius = 0.015f;
        float collision_bounce = 0.2f;
    };

    std::string to_string(const ParticleEmitterComponent& t);

    template<typename Visitor>
    void visit_asset_refs(ParticleEmitterComponent& t, Visitor&& visitor)
    {
        visitor(t.texture_ref);
    }

    template<typename Visitor>
    void visit_entity_refs(ParticleEmitterComponent& t, Visitor&& visitor) {}
}

