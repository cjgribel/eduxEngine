// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "glcommon.h"

#include "entt/entt.hpp"

#include <string>

#include <glm/glm.hpp>

namespace eeng
{
    struct EngineContext;
}

namespace eeng::ecs::systems
{
    class ParticleSystem;

    class ParticleRenderSystem
    {
    public:
        ParticleRenderSystem() = default;
        ~ParticleRenderSystem();

        void init(
            const std::string& vertex_shader_path = "shaders/particle_billboard_vert.glsl",
            const std::string& fragment_shader_path = "shaders/particle_billboard_frag.glsl");
        void shutdown();

        bool initialized() const noexcept { return shader_program_ != 0; }

        void render(
            entt::registry& registry,
            EngineContext& ctx,
            const ParticleSystem& particle_system,
            const glm::mat4& proj_view,
            const glm::vec3& camera_right,
            const glm::vec3& camera_up);

    private:
        GLuint shader_program_ = 0;
        GLuint vao_ = 0;
        GLuint quad_vbo_ = 0;
        GLuint instance_vbo_ = 0;
    };
}

