// Created by Carl Johan Gribel.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef ForwardRenderer_hpp
#define ForwardRenderer_hpp

#include <glm/glm.hpp>
#include "glcommon.h"
#include "RenderableMesh.hpp"

namespace eeng
{
    class ForwardRenderer
    {
        GLuint phongShader = 0;
        GLuint placeholder_texture = 0;
        int drawcallCounter;

        struct TextureDesc
        {
            PhongMaterial::TextureTypeIndex textureTypeIndex;
            GLuint textureUnit;
            const char *samplerName;
            const char *flagName;
        };

        enum TextureTypeIndex
        {
            Diffuse = 0,
            Normal,
            Specular,
            Opacity,
            Cubemap
        };

        TextureDesc texturesDescs[4] = {
            {PhongMaterial::TextureTypeIndex::Diffuse, 0, "diffuseTexture", "has_diffuseTexture"},
            {PhongMaterial::TextureTypeIndex::Normal, 1, "normalTexture", "has_normalTexture"},
            {PhongMaterial::TextureTypeIndex::Specular, 2, "specularTexture", "has_specularTexture"},
            {PhongMaterial::TextureTypeIndex::Opacity, 3, "opacityTexture", "has_opacityTexture"}};

        TextureDesc cubemapTextureDesc{PhongMaterial::TextureTypeIndex::Cubemap, 4, "cubeTexture", "has_cubemap"};

    public:
        ForwardRenderer();

        ~ForwardRenderer();

        /// @brief Initialize renderer
        /// @param vertShaderPath
        /// @param fragShaderPath
        void init(const std::string &vertShaderPath,
                  const std::string &fragShaderPath);

        /// @brief Start of a rendering pass and set common uniforms
        /// @param ProjMatrix
        /// @param ViewMatrix
        /// @param lightPos
        /// @param lightColor
        /// @param eyePos
        void beginPass(const glm::mat4 &ProjMatrix,
                       const glm::mat4 &ViewMatrix,
                       const glm::vec3 &lightPos,
                       const glm::vec3 &lightColor,
                       const glm::vec3 &eyePos);

        /// @brief Ends pass and resets GL state
        /// @return Number of drawcalls made during pass
        int endPass();

        /// @brief Render an instance of a mesh
        /// @param mesh Mesh to render
        /// @param WorldMatrix Instance world transform
        void renderMesh(const std::shared_ptr<RenderableMesh> mesh,
                        const glm::mat4 &WorldMatrix);
    };

using ForwardRendererPtr = std::shared_ptr<ForwardRenderer>;

} // namespace eeng

#endif