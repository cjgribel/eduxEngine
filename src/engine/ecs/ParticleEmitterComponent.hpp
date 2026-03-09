// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "assets/AssetRef.hpp"
#include "assets/types/ModelAssets.hpp"
#include "ecs/Entity.hpp"

#include <cstdint>
#include <string>
#include <vector>

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
        bool atlas_enabled = false;
        std::uint32_t atlas_columns = 1u;
        std::uint32_t atlas_rows = 1u;
        std::uint32_t atlas_frame_count = 1u;
        float atlas_fps = 0.0f;
        bool atlas_loop = false;
        bool atlas_random_start = false;
        bool texture_key_enabled = false;
        glm::vec3 texture_key_color{ 0.0f, 0.0f, 0.0f };
        float texture_key_threshold = 0.1f;
        bool texture_flip_v = false;
        bool additive_blend = true;
        bool depth_write = false;

        // Collision is wired for later phases (raycast-based policy).
        ParticleCollisionMode collision_mode = ParticleCollisionMode::None;
        std::uint32_t collision_layer = 1u;
        std::uint32_t collision_mask = 0xFFFFFFFFu;
        float collision_radius = 0.015f;
        float collision_bounce = 0.2f;
    };

    struct ParticleHitEvent
    {
        Entity emitter_entity;
        Entity hit_entity;
        std::uint32_t hit_collider_id = 0u;
        glm::vec3 point{ 0.0f };
        glm::vec3 normal{ 0.0f };
        glm::vec3 velocity{ 0.0f };
        float particle_age = 0.0f;
        float particle_lifetime = 0.0f;
        float particle_size = 0.0f;
        std::uint32_t particle_color_abgr = 0xffffffffu;
    };

    struct ParticleEventsComponent
    {
        bool emit_hit_events = true;
        std::uint32_t max_hit_events_per_frame = 64u;
        std::vector<ParticleHitEvent> hit_events;
    };

    std::string to_string(const ParticleEmitterComponent& t);
    std::string to_string(const ParticleEventsComponent& t);

    template<typename Visitor>
    void visit_asset_refs(ParticleEmitterComponent& t, Visitor&& visitor)
    {
        visitor(t.texture_ref);
    }

    template<typename Visitor>
    void visit_entity_refs(ParticleEmitterComponent& t, Visitor&& visitor) {}

    template<typename Visitor>
    void visit_asset_refs(ParticleEventsComponent& t, Visitor&& visitor) {}

    template<typename Visitor>
    void visit_entity_refs(ParticleEventsComponent& t, Visitor&& visitor) {}
}
