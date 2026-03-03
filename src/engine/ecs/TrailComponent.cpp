// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "TrailComponent.hpp"

#include <format>
#include <algorithm>
#include <cmath>

namespace eeng::ecs
{
    namespace
    {
        constexpr std::uint32_t kRgbMask = 0x00ffffffu;

        std::uint8_t base_alpha(std::uint32_t color)
        {
            return static_cast<std::uint8_t>((color >> 24) & 0xffu);
        }

        std::uint32_t color_with_alpha(std::uint32_t color, std::uint8_t alpha)
        {
            return (color & kRgbMask) | (static_cast<std::uint32_t>(alpha) << 24u);
        }

        std::uint8_t color_channel(std::uint32_t color, std::uint32_t shift)
        {
            return static_cast<std::uint8_t>((color >> shift) & 0xffu);
        }

        std::uint32_t pack_color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
        {
            return static_cast<std::uint32_t>(r)
                | (static_cast<std::uint32_t>(g) << 8u)
                | (static_cast<std::uint32_t>(b) << 16u)
                | (static_cast<std::uint32_t>(a) << 24u);
        }

        std::uint32_t lerp_color(std::uint32_t a, std::uint32_t b, float t)
        {
            t = std::clamp(t, 0.0f, 1.0f);
            const auto lerp_channel = [t](std::uint8_t c0, std::uint8_t c1)
            {
                const float v = static_cast<float>(c0) + (static_cast<float>(c1) - static_cast<float>(c0)) * t;
                return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 255.0f));
            };

            const std::uint8_t r = lerp_channel(color_channel(a, 0u), color_channel(b, 0u));
            const std::uint8_t g = lerp_channel(color_channel(a, 8u), color_channel(b, 8u));
            const std::uint8_t bl = lerp_channel(color_channel(a, 16u), color_channel(b, 16u));
            const std::uint8_t al = lerp_channel(color_channel(a, 24u), color_channel(b, 24u));
            return pack_color(r, g, bl, al);
        }

        float clamp01(float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        }
    }

    void TrailComponent_ClearTrail(TrailComponent::Trail& trail)
    {
        trail.start_index = 0;
        trail.count = 0;
        trail.ages.fill(0.0f);
        const std::uint32_t base_color = trail.use_color_over_age ? trail.color_start : trail.color;
        const std::uint8_t alpha = base_alpha(base_color);
        for (auto& vertex : trail.vertices)
            vertex.color = color_with_alpha(base_color, alpha);
    }

    bool TrailComponent_AddSampleIfNeeded(TrailComponent::Trail& trail, const glm::vec3& world_pos)
    {
        if (!trail.active || !trail.emitting)
            return false;

        const float min_dist = std::max(0.0f, trail.min_emit_distance);
        const float min_dist_sq = min_dist * min_dist;
        if (trail.count > 0)
        {
            const int prev_index = (trail.start_index + trail.count - 1) %
                static_cast<int>(TrailComponent::max_vertices_per_trail);
            const glm::vec3 delta = trail.vertices[prev_index].p - world_pos;
            const float dist_sq = glm::dot(delta, delta);
            if (dist_sq < min_dist_sq)
                return false;

            if (trail.clear_on_teleport && trail.clear_teleport_distance > 0.0f)
            {
                const float teleport_dist_sq = trail.clear_teleport_distance * trail.clear_teleport_distance;
                if (dist_sq > teleport_dist_sq)
                    TrailComponent_ClearTrail(trail);
            }
        }

        const int insert_index = (trail.start_index + trail.count) %
            static_cast<int>(TrailComponent::max_vertices_per_trail);
        trail.vertices[insert_index].p = world_pos;
        trail.vertices[insert_index].color = trail.use_color_over_age ? trail.color_start : trail.color;
        trail.ages[insert_index] = 0.0f;

        if (trail.count < static_cast<int>(TrailComponent::max_vertices_per_trail))
        {
            ++trail.count;
        }
        else
        {
            trail.start_index = (trail.start_index + 1) %
                static_cast<int>(TrailComponent::max_vertices_per_trail);
        }

        return true;
    }

    void TrailComponent_AgeAndPrune(TrailComponent::Trail& trail, float dt)
    {
        if (!trail.active || trail.count <= 0)
            return;

        const float step = std::max(0.0f, dt);
        const bool has_lifetime = trail.lifetime > 0.0f;
        for (int i = 0; i < trail.count; ++i)
        {
            const int index = (trail.start_index + i) %
                static_cast<int>(TrailComponent::max_vertices_per_trail);
            trail.ages[index] += step;

            if (has_lifetime && trail.ages[index] >= trail.lifetime)
            {
                trail.start_index = (trail.start_index + 1) %
                    static_cast<int>(TrailComponent::max_vertices_per_trail);
                --trail.count;
                --i;
                continue;
            }
        }

        for (int i = 0; i < trail.count; ++i)
        {
            const int index = (trail.start_index + i) %
                static_cast<int>(TrailComponent::max_vertices_per_trail);
            const float normalized_age = has_lifetime ? clamp01(trail.ages[index] / trail.lifetime) : 0.0f;

            const std::uint32_t base_color =
                (trail.use_color_over_age && has_lifetime)
                ? lerp_color(trail.color_start, trail.color_end, normalized_age)
                : trail.color;
            float alpha_factor = 1.0f;
            if (trail.fade_mode == TrailFadeMode::Linear && has_lifetime)
            {
                alpha_factor = 1.0f - normalized_age;
            }

            const std::uint8_t alpha_base = base_alpha(base_color);
            const float alpha_f = static_cast<float>(alpha_base) * alpha_factor;
            const std::uint8_t alpha = static_cast<std::uint8_t>(std::clamp(alpha_f, 0.0f, 255.0f));
            trail.vertices[index].color = color_with_alpha(base_color, alpha);
        }
    }

    std::string to_string(const TrailComponent& t)
    {
        std::size_t active = 0;
        std::size_t points = 0;
        for (const auto& trail : t.trails)
        {
            if (!trail.active)
                continue;
            ++active;
            points += static_cast<std::size_t>(trail.count);
        }

        return std::format("TrailComponent(active_trails={}, points={})", active, points);
    }
}
