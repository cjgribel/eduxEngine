// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/RenderSystem.hpp"

#include <cstdint>
#include <fstream>
#include <sstream>

#include <glm/gtc/type_ptr.hpp>

#include "ShaderLoader.h"
#include "LogMacros.h"
#include "ResourceManager.hpp"
#include "ecs/ModelComponent.hpp"
#include "assets/types/ModelAssets.hpp"

namespace
{
    struct TextureDesc
    {
        eeng::assets::MaterialTextureSlot slot;
        GLint texture_unit;
        const char* sampler_name;
        const char* flag_name;
    };

    static constexpr TextureDesc kTextureDescs[] = {
        { eeng::assets::MaterialTextureSlot::Diffuse, 0, "diffuseTexture", "has_diffuseTexture" },
        { eeng::assets::MaterialTextureSlot::Normal, 1, "normalTexture", "has_normalTexture" },
        { eeng::assets::MaterialTextureSlot::Specular, 2, "specularTexture", "has_specularTexture" },
        { eeng::assets::MaterialTextureSlot::Opacity, 3, "opacityTexture", "has_opacityTexture" }
    };

    std::string file_to_string(const std::string& filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            throw std::runtime_error(std::string("Cannot open ") + filename);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
}

namespace eeng::ecs::systems
{
    RenderSystem::~RenderSystem()
    {
        shutdown();
    }

    void RenderSystem::init(const std::string& vertex_shader_path, const std::string& fragment_shader_path)
    {
        if (shader_program_ != 0)
            return;

        auto vert_source = file_to_string(vertex_shader_path);
        auto frag_source = file_to_string(fragment_shader_path);
        shader_program_ = createShaderProgram(vert_source.c_str(), frag_source.c_str());

        glUseProgram(shader_program_);
        for (const auto& texture_desc : kTextureDescs)
        {
            glUniform1i(glGetUniformLocation(shader_program_, texture_desc.sampler_name), texture_desc.texture_unit);
        }
        glUseProgram(0);
    }

    void RenderSystem::shutdown()
    {
        if (shader_program_ != 0)
        {
            glDeleteProgram(shader_program_);
            shader_program_ = 0;
        }
    }

    void RenderSystem::render(
        entt::registry& registry,
        EngineContext& ctx,
        const FrameUniformBinder& bind_frame_uniforms,
        const EntityUniformBinder& bind_entity_uniforms)
    {
        if (shader_program_ == 0)
            return;

        auto rm = std::dynamic_pointer_cast<ResourceManager>(ctx.resource_manager);
        assert(rm);

        glUseProgram(shader_program_);
        if (bind_frame_uniforms)
            bind_frame_uniforms(shader_program_);
        CheckAndThrowGLErrors();

        auto view = registry.view<ecs::ModelComponent>();
        for (auto [entity, model] : view.each())
        {
            if (!model.model_ref.is_bound())
                continue;

            GLuint vao = 0;
            GLuint ibo = 0;
            std::vector<assets::GpuSubMesh> submeshes;
            Handle<assets::ModelDataAsset> model_handle{};
            std::vector<assets::SubMesh> cpu_submeshes;
            size_t bone_count = 0;

            rm->storage().read(model.model_ref.handle, [&](const assets::GpuModelAsset& gpu)
                {
                    if (gpu.state != assets::GpuLoadState::Ready || gpu.vao == 0)
                        return;

                    vao = gpu.vao;
                    ibo = gpu.ibo;
                    submeshes = gpu.submeshes;
                    model_handle = gpu.model_ref.handle;
                });

            if (vao == 0 || ibo == 0 || submeshes.empty())
                continue;

            if (rm->storage().validate(model_handle))
            {
                rm->storage().read(model_handle, [&](const assets::ModelDataAsset& cpu_model)
                    {
                        cpu_submeshes = cpu_model.submeshes;
                        bone_count = cpu_model.bones.size();
                    });
            }

            if (bind_entity_uniforms)
                bind_entity_uniforms(shader_program_, entity, model);
            CheckAndThrowGLErrors();

            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
            CheckAndThrowGLErrors();

            if (bone_count > 0 && !model.bone_matrices.empty())
            {
                const auto count = static_cast<GLsizei>(std::min(bone_count, model.bone_matrices.size()));
                glUniformMatrix4fv(
                    glGetUniformLocation(shader_program_, "BoneMatrices"),
                    count,
                    0,
                    glm::value_ptr(model.bone_matrices[0]));
            }

            for (const auto& sm : submeshes)
            {
                if (sm.index_count == 0)
                    continue;

                assets::GpuMaterialAsset material{};
                bool has_material = false;
                if (sm.material.is_bound())
                {
                    rm->storage().read(sm.material.handle, [&](const assets::GpuMaterialAsset& mtl)
                        {
                            material = mtl;
                            has_material = true;
                        });
                }

                glUniform3fv(glGetUniformLocation(shader_program_, "Ka"), 1, glm::value_ptr(material.Ka));
                glUniform3fv(glGetUniformLocation(shader_program_, "Kd"), 1, glm::value_ptr(material.Kd));
                glUniform3fv(glGetUniformLocation(shader_program_, "Ks"), 1, glm::value_ptr(material.Ks));
                glUniform1f(glGetUniformLocation(shader_program_, "shininess"), material.shininess);

                for (const auto& texture_desc : kTextureDescs)
                {
                    bool has_texture = false;
                    GLuint tex_id = 0;

                    if (has_material)
                    {
                        const auto& tex_ref = material.textures[static_cast<size_t>(texture_desc.slot)];
                        if (tex_ref.is_bound())
                        {
                            rm->storage().read(tex_ref.handle, [&](const assets::GpuTextureAsset& tex)
                                {
                                    tex_id = tex.gl_id;
                                    has_texture = (tex_id != 0 && tex.state == assets::GpuLoadState::Ready);
                                });
                        }
                    }

                    glActiveTexture(GL_TEXTURE0 + texture_desc.texture_unit);
                    glBindTexture(GL_TEXTURE_2D, tex_id);
                    glUniform1i(glGetUniformLocation(shader_program_, texture_desc.flag_name), has_texture);
                }

                const size_t sm_index = &sm - submeshes.data();
                const bool submesh_skinned = (sm_index < cpu_submeshes.size())
                    ? cpu_submeshes[sm_index].is_skinned
                    : false;
                const bool use_skinning = submesh_skinned && !model.bone_matrices.empty();
                glUniform1i(glGetUniformLocation(shader_program_, "u_is_skinned"), use_skinning ? 1 : 0);

                CheckAndThrowGLErrors();
                glDrawElementsBaseVertex(
                    GL_TRIANGLES,
                    sm.index_count,
                    GL_UNSIGNED_INT,
                    reinterpret_cast<void*>(static_cast<uintptr_t>(sm.index_offset) * sizeof(uint32_t)),
                    sm.base_vertex);

                for (const auto& texture_desc : kTextureDescs)
                {
                    glActiveTexture(GL_TEXTURE0 + texture_desc.texture_unit);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
            }
        }
        glBindVertexArray(0);
        glUseProgram(0);
    }
}
