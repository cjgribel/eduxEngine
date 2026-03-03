// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef TrailComponent_hpp
#define TrailComponent_hpp

#include <array>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace eeng::ecs
{
    struct TrailLineStyle
    {
        float thickness = 2.0f;
        float dash_period_px = 0.0f;
        float dash_ratio = 0.5f;
        float dash_offset_px = 0.0f;
    };

    enum class TrailFadeMode : std::uint8_t
    {
        None,
        Linear
    };

    struct TrailComponent
    {
        static constexpr std::size_t max_trails = 8;
        static constexpr std::size_t max_vertices_per_trail = 64;

        struct Trail
        {
            bool active = false;
            bool emitting = true;

            float lifetime = 1.0f; // <= 0 means no lifetime culling.
            float min_emit_distance = 0.1f;
            std::uint32_t color = 0xffffffffu;
            bool use_color_over_age = false;
            std::uint32_t color_start = 0xffffffffu;
            std::uint32_t color_end = 0xffffffffu;
            TrailLineStyle style{};
            glm::vec3 local_offset{ 0.0f };
            TrailFadeMode fade_mode = TrailFadeMode::Linear;

            bool clear_on_teleport = false;
            float clear_teleport_distance = 8.0f;

            int start_index = 0;
            int count = 0;
            struct Vertex
            {
                glm::vec3 p{ 0.0f };
                std::uint32_t color = 0xffffffffu;
            };
            std::array<Vertex, max_vertices_per_trail> vertices{};
            std::array<float, max_vertices_per_trail> ages{};
        };

        std::array<Trail, max_trails> trails{};
        std::uint8_t active_trail_count = 1;

        TrailComponent()
        {
            trails[0].active = true;
        }
    };

    void TrailComponent_ClearTrail(TrailComponent::Trail& trail);
    bool TrailComponent_AddSampleIfNeeded(TrailComponent::Trail& trail, const glm::vec3& world_pos);
    void TrailComponent_AgeAndPrune(TrailComponent::Trail& trail, float dt);

    std::string to_string(const TrailComponent& t);

    template<typename Visitor>
    void visit_asset_refs(TrailComponent& t, Visitor&& visitor) {}

    template<typename Visitor>
    void visit_entity_refs(TrailComponent& t, Visitor&& visitor) {}


}

#endif // TrailComponent_hpp
