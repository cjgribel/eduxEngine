// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "TrailSystem.hpp"

#include "ShapeRenderer.hpp"
#include "ecs/TrailComponent.hpp"
#include "ecs/TransformComponent.hpp"

#include <algorithm>

namespace eeng::ecs::systems
{
    void TrailSystem::update(
        entt::registry& registry,
        EngineContext& ctx,
        float delta_time)
    {
        (void)ctx;
        auto view = registry.view<ecs::TransformComponent, ecs::TrailComponent>();
        for (auto entity : view)
        {
            const auto& transform = view.get<ecs::TransformComponent>(entity);
            auto& trail_component = view.get<ecs::TrailComponent>(entity);
            const std::size_t trail_count = std::min(
                static_cast<std::size_t>(trail_component.active_trail_count),
                ecs::TrailComponent::max_trails);
            for (std::size_t trail_index = 0; trail_index < trail_count; ++trail_index)
            {
                auto& trail = trail_component.trails[trail_index];
                if (!trail.active)
                    continue;

                const glm::vec3 world_pos = glm::vec3(
                    transform.world_matrix * glm::vec4(trail.local_offset, 1.0f));
                ecs::TrailComponent_AddSampleIfNeeded(trail, world_pos);
                ecs::TrailComponent_AgeAndPrune(trail, delta_time);
            }
        }
    }

    void TrailSystem::render(
        entt::registry& registry,
        EngineContext& ctx,
        ShapeRendering::ShapeRenderer& renderer) const
    {
        (void)ctx;
        auto view = registry.view<ecs::TrailComponent>();
        for (auto entity : view)
        {
            (void)entity;
            const auto& trail_component = view.get<ecs::TrailComponent>(entity);
            const std::size_t trail_count = std::min(
                static_cast<std::size_t>(trail_component.active_trail_count),
                ecs::TrailComponent::max_trails);
            for (std::size_t trail_index = 0; trail_index < trail_count; ++trail_index)
            {
                const auto& trail = trail_component.trails[trail_index];
                if (!trail.active || trail.count < 2)
                    continue;

                const ShapeRendering::LineStyle line_style{
                    trail.style.thickness,
                    trail.style.dash_period_px,
                    trail.style.dash_ratio,
                    trail.style.dash_offset_px
                };
                ShapeRendering::CyclicLineBufferView vertex_view{};
                vertex_view.positions = &trail.vertices[0].p;
                vertex_view.position_stride = sizeof(ecs::TrailComponent::Trail::Vertex);
                vertex_view.colors = &trail.vertices[0].color;
                vertex_view.color_stride = sizeof(ecs::TrailComponent::Trail::Vertex);
                vertex_view.fallback_color = trail.color;

                renderer.push_states(ShapeRendering::LineType::Thick, line_style);
                renderer.push_lines_from_cyclic_source(
                    vertex_view,
                    trail.start_index,
                    trail.count,
                    static_cast<int>(ecs::TrailComponent::max_vertices_per_trail),
                    ShapeRendering::CoordinateSpace::World);
                renderer.pop_states<ShapeRendering::LineType, ShapeRendering::LineStyle>();
            }
        }
    }
}
