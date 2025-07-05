// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#include <fstream>
#include <string>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

#include "ForwardRenderer.hpp"
#include "glcommon.h"
#include "ShaderLoader.h"
#include "LogGlobals.hpp"

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

namespace eeng
{
    ForwardRenderer::ForwardRenderer()
    {
    }

    ForwardRenderer::~ForwardRenderer()
    {
        EENG_ASSERT(phongShader, "Destrying uninitialized shader program");
        if (phongShader)
            glDeleteProgram(phongShader);
    }

    void ForwardRenderer::init(const std::string& vertShaderPath,
        const std::string& fragShaderPath)
    {
        LogGlobals::log("Compiling shaders %s, %s",
            vertShaderPath.c_str(),
            fragShaderPath.c_str());
        auto vertSource = file_to_string(vertShaderPath);
        auto fragSource = file_to_string(fragShaderPath);
        phongShader = createShaderProgram(vertSource.c_str(), fragSource.c_str());

        // Bind shader samplers to texture units
        glUseProgram(phongShader);
        for (auto& textureDesc : texturesDescs)
        {
            glUniform1i(glGetUniformLocation(phongShader, textureDesc.samplerName), textureDesc.textureUnit);
        }
        glUseProgram(0);
        CheckAndThrowGLErrors();

        // placeholder_texture = create_checker_texture();
    }

    void ForwardRenderer::beginPass(const glm::mat4& ProjMatrix,
        const glm::mat4& ViewMatrix,
        const glm::vec3& lightPos,
        const glm::vec3& lightColor,
        const glm::vec3& eyePos)
    {
        EENG_ASSERT(phongShader, "Renderer not initialized");

        // GL state

        // Face culling - takes place before rasterization
        glEnable(GL_CULL_FACE); // Perform face culling
        glFrontFace(GL_CCW);    // Define winding for a front-facing face
        glCullFace(GL_BACK);    // Cull back-facing faces
        // Rasterization stuff
        glEnable(GL_DEPTH_TEST); // Perform depth test when rasterizing
        glDepthFunc(GL_LESS);    // Depth test pass if z < existing z (closer than existing z)
        glDepthMask(GL_TRUE);    // If depth test passes, write z to z-buffer
        glDepthRange(0, 1);      // Z-buffer range is [0,1], where 0 is at z-near and 1 is at z-far

        // Define viewport transform = Clip -> Screen space (applied before rasterization)
        // TODO glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);

        // Bind the default framebuffer (only needed when using multiple render targets)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Clear depth and color attachments of frame buffer
        // glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        // glClearDepth(1.0f);
        // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // if (WIREFRAME)
        // {
        //     glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        //     glDisable(GL_CULL_FACE);
        // }
        // else
        // {
        //     glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        //     glEnable(GL_CULL_FACE);
        // }

        glUseProgram(phongShader);

        // Bind matrices
        const auto ProjViewMatrix = ProjMatrix * ViewMatrix;
        glUniformMatrix4fv(glGetUniformLocation(phongShader, "ProjViewMatrix"), 1, 0, glm::value_ptr(ProjViewMatrix));

        // Bind light & eye position
        glUniform3fv(glGetUniformLocation(phongShader, "lightpos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(phongShader, "lightColor"), 1, glm::value_ptr(lightColor));
        glUniform3fv(glGetUniformLocation(phongShader, "eyepos"), 1, glm::value_ptr(eyePos));

        // Bind cube map texture
        GLuint cubemapTextureHandle = 0; // <- PLACEHOLDER
        if (cubemapTextureHandle)
        {
            // const auto &cubemapTextureDesc = texturesDescs[TextureTypeIndex::Cubemap];
            glActiveTexture(GL_TEXTURE0 + cubemapTextureDesc.textureUnit);
            glBindTexture(GL_TEXTURE_2D, cubemapTextureHandle);

            glUniform1i(glGetUniformLocation(phongShader, cubemapTextureDesc.flagName), 1);
        }

        CheckAndThrowGLErrors();
        drawcallCounter = 0;
    }

    int ForwardRenderer::endPass()
    {
        glUseProgram(0);
        glBindVertexArray(0);

        // Possibly restore GL state

        return drawcallCounter;
    }

    void ForwardRenderer::renderMesh(const std::shared_ptr<RenderableMesh> mesh,
        const glm::mat4& WorldMatrix)
    {
        // Bind bone matrices
        if (mesh->boneMatrices.size())
            glUniformMatrix4fv(glGetUniformLocation(phongShader, "BoneMatrices"),
                (GLsizei)mesh->boneMatrices.size(),
                0,
                glm::value_ptr(mesh->boneMatrices[0]));

        glBindVertexArray(mesh->m_VAO);

        for (uint i = 0; i < mesh->m_meshes.size(); i++)
        {
            const auto& submesh = mesh->m_meshes[i];
            const auto& mtl = mesh->m_materials[submesh.mtl_index];

            if (submesh.node_index != EENG_NULL_INDEX && !submesh.is_skinned)
            {
                // Append hierarchical transform to non-skinned meshes that are linked to nodes
                const auto WorldMeshMatrix = WorldMatrix * mesh->m_nodetree.get_payload_at(submesh.node_index).global_tfm;
                glUniformMatrix4fv(glGetUniformLocation(phongShader, "WorldMatrix"), 1, 0, glm::value_ptr(WorldMeshMatrix));
            }
            else
                glUniformMatrix4fv(glGetUniformLocation(phongShader, "WorldMatrix"), 1, 0, glm::value_ptr(WorldMatrix));

            // (Could do view frustum culling (VFC) here using the projection matrix)
            // (Mesh traversal)
            // if (submesh.is_skinned)
            //     submesh.aabb = meshres.src_mesh->m_model_aabb;
            // else
            //     submesh.aabb = meshres.src_mesh->m_mesh_aabbs_pose[i];
            // (VFC)
            // v4f bs = aabb.post_transform(tfm).get_boundingsphere();

            // Color components
            glUniform3fv(glGetUniformLocation(phongShader, "Ka"), 1, glm::value_ptr(mtl.Ka));
            glUniform3fv(glGetUniformLocation(phongShader, "Kd"), 1, glm::value_ptr(mtl.Kd));
            glUniform3fv(glGetUniformLocation(phongShader, "Ks"), 1, glm::value_ptr(mtl.Ks));
            glUniform1f(glGetUniformLocation(phongShader, "shininess"), mtl.shininess);

            // Bind textures and texture flags
            for (auto& textureDesc : texturesDescs)
            {
                // if (texture.textureTypeIndex == TextureTypeIndex::Cubemap) continue;
                const int textureIndex = mtl.textureIndices[textureDesc.textureTypeIndex];
                const bool hasTexture = (textureIndex != NoTexture);
                if (hasTexture)
                {
                    glActiveTexture(GL_TEXTURE0 + textureDesc.textureUnit);
                    glBindTexture(GL_TEXTURE_2D, mesh->m_textures[textureIndex].getHandle());
                }
                glUniform1i(glGetUniformLocation(phongShader, textureDesc.flagName), hasTexture);
            }

            // Skinned flag
            glUniform1i(glGetUniformLocation(phongShader, "u_is_skinned"), (int)submesh.is_skinned);

            // Render
            glDrawElementsBaseVertex(GL_TRIANGLES,
                submesh.nbr_indices,
                GL_UNSIGNED_INT,
                (GLvoid*)(sizeof(uint) * submesh.base_index),
                submesh.base_vertex);
            drawcallCounter++;

            // Unbind textures
            for (auto& texture : texturesDescs)
            {
                glActiveTexture(GL_TEXTURE0 + texture.textureUnit);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            CheckAndThrowGLErrors();
        }

        glBindVertexArray(0);
    }

} // namespace eeng