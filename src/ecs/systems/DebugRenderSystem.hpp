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

namespace eeng::ecs::systems
{
    struct DebugRenderSettings
    {
        bool show_transform_labels = true;
        std::uint32_t transform_label_bg = 0x80000000u;
        std::uint32_t transform_label_text = 0xffffffffu;
        glm::vec3 transform_label_offset{ 0.0f };
    };

    class DebugRenderSystem
    {
    public:
        void render(
            entt::registry& registry,
            EngineContext& ctx,
            const glm::mat4& VP_PROJ_V,
            int window_height) const;

        DebugRenderSettings settings{};
    };
}
