// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <functional>
#include <string>

#include "entt/entt.hpp"
#include "glcommon.h"

namespace eeng
{
    struct EngineContext;
}

namespace eeng::ecs
{
    struct ModelComponent;
}

namespace eeng::ecs::systems
{
    class RenderSystem
    {
    public:
        using FrameUniformBinder = std::function<void(GLuint)>;
        using EntityUniformBinder = std::function<void(GLuint, entt::entity, const ecs::ModelComponent&)>;

        RenderSystem() = default;
        ~RenderSystem();

        void init(const std::string& vertex_shader_path, const std::string& fragment_shader_path);
        void shutdown();

        void render(
            entt::registry& registry,
            EngineContext& ctx,
            const FrameUniformBinder& bind_frame_uniforms = {},
            const EntityUniformBinder& bind_entity_uniforms = {});

        bool initialized() const noexcept { return shader_program_ != 0; }

    private:
        GLuint shader_program_ = 0;
    };
}
