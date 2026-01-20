// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "entt/entt.hpp"

namespace eeng
{
    struct EngineContext;
}

namespace ShapeRendering
{
    class ShapeRenderer;
}

namespace eeng::ecs::systems
{
    struct DebugRenderSettings
    {
        bool show_transform_labels = true;
        std::uint32_t transform_label_bg = 0x80000000u;
        // Distinct label colors make it easier to spot component types.
        std::uint32_t transform_label_text = 0xffffffffu;
        glm::vec3 transform_label_offset{ 0.0f };

        // Collider wireframe + labels.
        bool show_colliders = true;
        bool show_collider_labels = true;
        std::uint32_t collider_wire_color = 0xff00ffffu;
        std::uint32_t collider_label_bg = 0x80202020u;
        std::uint32_t collider_label_text = 0xff80ff80u;
        glm::vec3 collider_label_offset{ 0.0f, 0.15f, 0.0f };

        // RigidBody labels.
        bool show_rigidbody_labels = true;
        std::uint32_t rigidbody_label_bg = 0x80202040u;
        std::uint32_t rigidbody_label_text = 0xffffc080u;
        glm::vec3 rigidbody_label_offset{ 0.0f, 0.3f, 0.0f };
    };

    class DebugRenderSystem
    {
    public:
        void render(
            entt::registry& registry,
            EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer,
            const glm::mat4& VP_PROJ_V,
            int window_height) const;

        DebugRenderSettings settings{};
    };
}
