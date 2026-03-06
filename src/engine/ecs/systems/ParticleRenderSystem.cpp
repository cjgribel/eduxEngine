// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/ParticleRenderSystem.hpp"

#include "ShaderLoader.h"
#include "engineapi/EngineContextHelpers.hpp"
#include "assets/types/ModelAssets.hpp"
#include "ecs/ParticleEmitterComponent.hpp"
#include "ecs/systems/ParticleSystem.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace
{
    std::string file_to_string(const std::string& filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
            throw std::runtime_error(std::string("Cannot open ") + filename);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
}

namespace eeng::ecs::systems
{
    namespace
    {
        struct InstanceData
        {
            glm::vec3 center{ 0.0f };
            float size = 0.0f;
            std::uint32_t color_abgr = 0xffffffffu;
            glm::vec4 uv_rect{ 0.0f, 0.0f, 1.0f, 1.0f };
        };

        struct BatchKey
        {
            ecs::ParticleRenderMode render_mode = ecs::ParticleRenderMode::SoftCircle;
            GLuint texture = 0;
            bool use_texture = false;
            bool texture_key_enabled = false;
            glm::vec3 texture_key_color{ 0.0f, 0.0f, 0.0f };
            float texture_key_threshold = 0.0f;
            bool texture_flip_v = false;
            bool additive_blend = true;
            bool depth_write = false;

            bool operator==(const BatchKey& other) const
            {
                return render_mode == other.render_mode
                    && texture == other.texture
                    && use_texture == other.use_texture
                    && texture_key_enabled == other.texture_key_enabled
                    && texture_key_color == other.texture_key_color
                    && texture_key_threshold == other.texture_key_threshold
                    && texture_flip_v == other.texture_flip_v
                    && additive_blend == other.additive_blend
                    && depth_write == other.depth_write;
            }
        };

        struct BatchKeyHash
        {
            std::size_t operator()(const BatchKey& key) const noexcept
            {
                std::size_t seed = 0u;
                const auto hash_combine = [&seed](std::size_t value)
                {
                    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
                };

                hash_combine(std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(key.render_mode)));
                hash_combine(std::hash<std::uint32_t>{}(key.texture));
                hash_combine(std::hash<std::uint8_t>{}(key.use_texture ? 1u : 0u));
                hash_combine(std::hash<std::uint8_t>{}(key.texture_key_enabled ? 1u : 0u));
                hash_combine(std::hash<float>{}(key.texture_key_color.x));
                hash_combine(std::hash<float>{}(key.texture_key_color.y));
                hash_combine(std::hash<float>{}(key.texture_key_color.z));
                hash_combine(std::hash<float>{}(key.texture_key_threshold));
                hash_combine(std::hash<std::uint8_t>{}(key.texture_flip_v ? 1u : 0u));
                hash_combine(std::hash<std::uint8_t>{}(key.additive_blend ? 1u : 0u));
                hash_combine(std::hash<std::uint8_t>{}(key.depth_write ? 1u : 0u));
                return seed;
            }
        };
    }

    ParticleRenderSystem::~ParticleRenderSystem()
    {
        shutdown();
    }

    void ParticleRenderSystem::init(
        const std::string& vertex_shader_path,
        const std::string& fragment_shader_path)
    {
        if (initialized())
            return;

        const auto vert_source = file_to_string(vertex_shader_path);
        const auto frag_source = file_to_string(fragment_shader_path);
        shader_program_ = createShaderProgram(vert_source.c_str(), frag_source.c_str());

        static constexpr std::array<glm::vec2, 4> kQuadCorners = {
            glm::vec2(-0.5f, -0.5f),
            glm::vec2(-0.5f, 0.5f),
            glm::vec2(0.5f, -0.5f),
            glm::vec2(0.5f, 0.5f)
        };

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glGenBuffers(1, &quad_vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(sizeof(glm::vec2) * kQuadCorners.size()),
            kQuadCorners.data(),
            GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);

        glGenBuffers(1, &instance_vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData), reinterpret_cast<void*>(offsetof(InstanceData, center)));
        glVertexAttribDivisor(1, 1);

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData), reinterpret_cast<void*>(offsetof(InstanceData, size)));
        glVertexAttribDivisor(2, 1);

        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(InstanceData), reinterpret_cast<void*>(offsetof(InstanceData, color_abgr)));
        glVertexAttribDivisor(3, 1);

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), reinterpret_cast<void*>(offsetof(InstanceData, uv_rect)));
        glVertexAttribDivisor(4, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void ParticleRenderSystem::shutdown()
    {
        if (instance_vbo_ != 0)
        {
            glDeleteBuffers(1, &instance_vbo_);
            instance_vbo_ = 0;
        }
        if (quad_vbo_ != 0)
        {
            glDeleteBuffers(1, &quad_vbo_);
            quad_vbo_ = 0;
        }
        if (vao_ != 0)
        {
            glDeleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
        if (shader_program_ != 0)
        {
            glDeleteProgram(shader_program_);
            shader_program_ = 0;
        }
    }

    void ParticleRenderSystem::render(
        entt::registry& registry,
        EngineContext& ctx,
        const ParticleSystem& particle_system,
        const glm::mat4& proj_view,
        const glm::vec3& camera_right,
        const glm::vec3& camera_up)
    {
        last_draw_batch_count_ = 0;
        if (!initialized())
            return;

        std::unordered_map<BatchKey, std::vector<InstanceData>, BatchKeyHash> batches;
        std::shared_ptr<eeng::ResourceManager> rm;
        particle_system.for_each_render_emitter(
            registry,
            [&](entt::entity entity,
                const ecs::ParticleEmitterComponent& emitter,
                const std::vector<ParticleSystem::RenderParticle>& particles)
            {
                (void)entity;
                if (particles.empty() || !emitter.enabled)
                    return;

                GLuint texture_id = 0;
                bool use_texture = emitter.use_texture && emitter.render_mode == ecs::ParticleRenderMode::Billboard;
                if (use_texture && emitter.texture_ref.is_bound())
                {
                    if (!rm)
                        rm = eeng::try_get_resource_manager(ctx, "ParticleRenderSystem");
                    if (!rm)
                        use_texture = false;

                    if (use_texture)
                    {
                        eeng::try_read_asset_ref(
                            *rm,
                            emitter.texture_ref,
                            ctx,
                            "ParticleRenderSystem",
                            "Missing GpuTextureAsset for ParticleEmitterComponent:",
                            [&](const assets::GpuTextureAsset& gpu_texture)
                            {
                                if (gpu_texture.state == assets::GpuLoadState::Ready)
                                    texture_id = gpu_texture.gl_id;
                            });
                    }
                    if (texture_id == 0)
                        use_texture = false;
                }
                else
                {
                    use_texture = false;
                }

                BatchKey key{};
                key.render_mode = emitter.render_mode;
                key.texture = texture_id;
                key.use_texture = use_texture;
                key.texture_key_enabled = emitter.texture_key_enabled && use_texture;
                key.texture_key_color = emitter.texture_key_color;
                key.texture_key_threshold = std::max(0.0f, emitter.texture_key_threshold);
                key.texture_flip_v = emitter.texture_flip_v && use_texture;
                key.additive_blend = emitter.additive_blend;
                key.depth_write = emitter.depth_write;

                auto& batch_instances = batches[key];
                batch_instances.reserve(batch_instances.size() + particles.size());
                for (const auto& particle : particles)
                {
                    if (particle.size <= 0.0f)
                        continue;
                    batch_instances.push_back(InstanceData{
                        particle.position,
                        particle.size,
                        particle.color_abgr,
                        glm::vec4(particle.uv_min, particle.uv_size)
                        });
                }
            });

        if (batches.empty())
            return;

        GLint previous_program = 0;
        GLint previous_vao = 0;
        GLboolean blend_enabled = GL_FALSE;
        GLboolean cull_face_enabled = GL_FALSE;
        GLboolean depth_mask = GL_TRUE;
        GLint blend_src_rgb = GL_ONE;
        GLint blend_dst_rgb = GL_ZERO;
        GLint blend_src_alpha = GL_ONE;
        GLint blend_dst_alpha = GL_ZERO;
        GLint active_texture = GL_TEXTURE0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);
        glGetBooleanv(GL_BLEND, &blend_enabled);
        glGetBooleanv(GL_CULL_FACE, &cull_face_enabled);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blend_src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &blend_dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src_alpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst_alpha);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);

        glUseProgram(shader_program_);
        glBindVertexArray(vao_);
        glEnable(GL_BLEND);
        glDisable(GL_CULL_FACE);

        glUniformMatrix4fv(
            glGetUniformLocation(shader_program_, "uProjView"),
            1,
            GL_FALSE,
            glm::value_ptr(proj_view));
        glUniform3fv(
            glGetUniformLocation(shader_program_, "uCameraRight"),
            1,
            glm::value_ptr(camera_right));
        glUniform3fv(
            glGetUniformLocation(shader_program_, "uCameraUp"),
            1,
            glm::value_ptr(camera_up));
        glUniform1i(glGetUniformLocation(shader_program_, "uTexture"), 0);

        for (const auto& [key, instances] : batches)
        {
            if (instances.empty())
                continue;
            ++last_draw_batch_count_;

            glDepthMask(key.depth_write ? GL_TRUE : GL_FALSE);
            if (key.additive_blend)
                glBlendFunc(GL_ONE, GL_ONE);
            else
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, key.texture);
            glUniform1i(glGetUniformLocation(shader_program_, "uUseTexture"), key.use_texture ? 1 : 0);
            glUniform1i(
                glGetUniformLocation(shader_program_, "uSoftCircle"),
                key.render_mode == ecs::ParticleRenderMode::SoftCircle ? 1 : 0);
            glUniform1i(
                glGetUniformLocation(shader_program_, "uTextureKeyEnabled"),
                key.texture_key_enabled ? 1 : 0);
            glUniform3fv(
                glGetUniformLocation(shader_program_, "uTextureKeyColor"),
                1,
                glm::value_ptr(key.texture_key_color));
            glUniform1f(
                glGetUniformLocation(shader_program_, "uTextureKeyThreshold"),
                key.texture_key_threshold);
            glUniform1i(
                glGetUniformLocation(shader_program_, "uTextureFlipV"),
                key.texture_flip_v ? 1 : 0);

            glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(instances.size() * sizeof(InstanceData)),
                instances.data(),
                GL_STREAM_DRAW);

            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(instances.size()));
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(previous_vao);
        glUseProgram(previous_program);

        if (!blend_enabled)
            glDisable(GL_BLEND);
        if (cull_face_enabled)
            glEnable(GL_CULL_FACE);
        glDepthMask(depth_mask);
        glBlendFuncSeparate(blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha);
        glActiveTexture(active_texture);
    }
}
