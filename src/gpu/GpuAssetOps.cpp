// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "GpuAssetOps.hpp"
#include <algorithm>
#include <filesystem>
#include "ResourceManager.hpp"
#include "Storage.hpp"
#include "assets/types/ModelAssets.hpp" // GpuModelAsset, ModelDataAsset, TextureAsset, MaterialAsset
#include "AssetIndexData.hpp"

#include "stb_image.h"

#include "glcommon.h"

/*
for (auto [e, model] : view)
{
    auto* gpu = rm.get<GpuModelAsset>(model.model_ref);
    if (!gpu || gpu->state != Ready) continue;

    glBindVertexArray(gpu->vao);

    for (const auto& sm : gpu->submeshes)
    {
        glDrawElementsBaseVertex(
            GL_TRIANGLES,
            sm.index_count,
            GL_UNSIGNED_INT,
            (void*)(sm.index_offset * sizeof(uint32_t)),
            sm.base_vertex);
    }
}
*/

namespace eeng::gl
{
    using namespace eeng::assets;

    struct CpuUploadData
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> texcoords;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec3> binormals;
        std::vector<glm::vec3> tangents;
        std::vector<SkinData> skin;

        std::vector<u32> indices;
        std::vector<GpuSubMesh> gpu_submeshes;

        u32 vertex_count = 0;
        u32 index_count = 0;
    };

    static CpuUploadData snapshot_model_data(
        eeng::ResourceManager& rm,
        const Handle<GpuModelAsset>& gpu_handle)
    {
        CpuUploadData out{};

        // 1) Read the GpuModelAsset to get the ModelData handle
        Handle<ModelDataAsset> model_handle{};
        rm.storage().read(gpu_handle, [&](const GpuModelAsset& gpu)
            {
                model_handle = gpu.model_ref.handle;
                out.gpu_submeshes = gpu.submeshes;
            });

        if (!rm.storage().validate(model_handle))
        {
            throw std::runtime_error("GpuModel init failed: ModelDataAsset handle is invalid (not bound/loaded).");
        }

        // 2) Copy ModelDataAsset data while holding locks
        rm.storage().read(model_handle, [&](const ModelDataAsset& model)
            {
                out.positions = model.positions;
                out.texcoords = model.texcoords;
                out.normals = model.normals;
                out.binormals = model.binormals;
                out.tangents = model.tangents;
                out.skin = model.skin;

                out.indices = model.indices;

                out.vertex_count = static_cast<u32>(model.positions.size());
                out.index_count = static_cast<u32>(model.indices.size());

                if (out.gpu_submeshes.empty())
                {
                    out.gpu_submeshes.reserve(model.submeshes.size());
                    for (const SubMesh& sm : model.submeshes)
                    {
                        GpuSubMesh gsm{};
                        gsm.index_offset = sm.base_index;
                        gsm.index_count = sm.nbr_indices;
                        gsm.base_vertex = sm.base_vertex;
                        out.gpu_submeshes.push_back(gsm);
                    }
                }
            });

        return out;
    }

    static std::filesystem::path resolve_texture_path(
        eeng::ResourceManager& rm,
        const Guid& texture_guid,
        std::string_view source_path)
    {
        auto index_data = rm.get_index_data();
        if (!index_data)
            throw std::runtime_error("GpuTexture init failed: asset index is not available");

        auto it = index_data->by_guid.find(texture_guid);
        if (it == index_data->by_guid.end() || !it->second)
            throw std::runtime_error("GpuTexture init failed: texture GUID not found in asset index");

        const auto& texture_entry = *it->second;
        const auto texture_dir = texture_entry.absolute_path.parent_path();
        return texture_dir / std::filesystem::path(std::string{ source_path });
    }

    GpuModelInitResult init_gpu_model(
        const Handle<GpuModelAsset>& gpu_handle,
        EngineContext& ctx)
    {
        GpuModelInitResult result{};
        auto rm = std::dynamic_pointer_cast<ResourceManager>(ctx.resource_manager);
        auto& storage = rm->storage();
        assert(rm);

        try
        {
            // Skip if already ready
            bool already_ready = false;
            storage.modify(gpu_handle, [&](GpuModelAsset& gpu)
                {
                    // Ready or Queued: don't proceed
                    // Uninitialized or Failed: proceed (new try)

                    already_ready = (gpu.state == GpuLoadState::Ready || gpu.state == GpuLoadState::Queued);

                    if (!already_ready)
                        gpu.state = GpuLoadState::Queued;
                });

            if (already_ready) { result.ok = true; return result; }

            CpuUploadData cpu = snapshot_model_data(*rm, gpu_handle);

            u32 vao = 0;
            u32 vbo_pos = 0;
            u32 vbo_uv = 0;
            u32 vbo_nrm = 0;
            u32 vbo_bnrm = 0;
            u32 vbo_tang = 0;
            u32 vbo_bone = 0;
            u32 ibo = 0;

            // Hard coded to match shader layout
            const GLuint loc_pos = 0;
            const GLuint loc_tex = 1;
            const GLuint loc_nrm = 2;
            const GLuint loc_tang = 3;
            const GLuint loc_bnrm = 4;
            const GLuint loc_bonei = 5;
            const GLuint loc_bonew = 6;

            CheckAndThrowGLErrors();

            glGenVertexArrays(1, reinterpret_cast<GLuint*>(&vao));
            glBindVertexArray(static_cast<GLuint>(vao));

            // Positions (location = 0)
            glGenBuffers(1, reinterpret_cast<GLuint*>(&vbo_pos));
            glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vbo_pos));
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(cpu.positions.size() * sizeof(glm::vec3)),
                cpu.positions.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(loc_pos);
            glVertexAttribPointer(loc_pos, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

            // UVs (location = 1)
            glGenBuffers(1, reinterpret_cast<GLuint*>(&vbo_uv));
            glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vbo_uv));
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(cpu.texcoords.size() * sizeof(glm::vec2)),
                cpu.texcoords.data(),
                GL_STATIC_DRAW);
            glEnableVertexAttribArray(loc_tex);
            glVertexAttribPointer(loc_tex, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

            // Normals (location = 2)
            glGenBuffers(1, reinterpret_cast<GLuint*>(&vbo_nrm));
            glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vbo_nrm));
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(cpu.normals.size() * sizeof(glm::vec3)),
                cpu.normals.data(),
                GL_STATIC_DRAW);
            glEnableVertexAttribArray(loc_nrm);
            glVertexAttribPointer(loc_nrm, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

            // Tangents (location = 3)
            if (!cpu.tangents.empty())
            {
                glGenBuffers(1, reinterpret_cast<GLuint*>(&vbo_tang));
                glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vbo_tang));
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(cpu.tangents.size() * sizeof(glm::vec3)),
                    cpu.tangents.data(),
                    GL_STATIC_DRAW);
                glEnableVertexAttribArray(loc_tang);
                glVertexAttribPointer(loc_tang, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
            }
            else
            {
                glDisableVertexAttribArray(loc_tang);
            }

            // Binormals (location = 4)
            if (!cpu.binormals.empty())
            {
                glGenBuffers(1, reinterpret_cast<GLuint*>(&vbo_bnrm));
                glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vbo_bnrm));
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(cpu.binormals.size() * sizeof(glm::vec3)),
                    cpu.binormals.data(),
                    GL_STATIC_DRAW);
                glEnableVertexAttribArray(loc_bnrm);
                glVertexAttribPointer(loc_bnrm, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
            }
            else
            {
                glDisableVertexAttribArray(loc_bnrm);
            }

            // Bone indices & weights (opt)
            if (!cpu.skin.empty())
            {
                glGenBuffers(1, reinterpret_cast<GLuint*>(&vbo_bone));
                glBindBuffer(GL_ARRAY_BUFFER, vbo_bone);
                glBufferData(GL_ARRAY_BUFFER,
                    cpu.skin.size() * sizeof(SkinData),
                    cpu.skin.data(),
                    GL_STATIC_DRAW);
                glEnableVertexAttribArray(loc_bonei);
                glVertexAttribIPointer(
                    loc_bonei, 4, GL_UNSIGNED_INT, sizeof(SkinData),
                    reinterpret_cast<void*>(offsetof(SkinData, bone_indices)));
                glEnableVertexAttribArray(loc_bonew);
                glVertexAttribPointer(
                    loc_bonew, 4, GL_FLOAT, GL_FALSE, sizeof(SkinData),
                    reinterpret_cast<void*>(offsetof(SkinData, bone_weights)));
            }
            else
            {
                glDisableVertexAttribArray(loc_bonei);
                glDisableVertexAttribArray(loc_bonew);
            }

            // Indices
            glGenBuffers(1, reinterpret_cast<GLuint*>(&ibo));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(ibo));
            glBufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(cpu.indices.size() * sizeof(u32)),
                cpu.indices.data(),
                GL_STATIC_DRAW);

            // Unbind VAO last
            glBindVertexArray(0);

            CheckAndThrowGLErrors();

            // Write back data under storage lock
            storage.modify(gpu_handle, [&](GpuModelAsset& gpu)
                {
                    gpu.vao = vao;
                    gpu.vbo_pos = vbo_pos;
                    gpu.vbo_uv = vbo_uv;
                    gpu.vbo_nrm = vbo_nrm;
                    gpu.vbo_tang = vbo_tang;
                    gpu.vbo_bnrm = vbo_bnrm;
                    gpu.vbo_bone = vbo_bone;
                    gpu.ibo = ibo;

                    gpu.submeshes = std::move(cpu.gpu_submeshes);
                    gpu.vertex_count = cpu.vertex_count;
                    gpu.index_count = cpu.index_count;

                    gpu.state = GpuLoadState::Ready;
                });

            result.ok = true;
            return result;
        }
        catch (const std::exception& ex)
        {
            // Try to mark failed (best-effort)
            storage.modify(gpu_handle, [&](GpuModelAsset& gpu)
                {
                    gpu.state = GpuLoadState::Failed;
                });

            result.ok = false;
            result.error = ex.what();
            return result;
        }
    }

    void destroy_gpu_model(
        const Handle<GpuModelAsset>& gpu_handle,
        EngineContext& ctx)
    {
        // + skin weights & indices
        // + bone matrices

        auto rm = std::dynamic_pointer_cast<ResourceManager>(ctx.resource_manager);
        assert(rm);

        // Get handles under lock
        u32 vao = 0;
        u32 vbo_pos = 0;
        u32 vbo_uv = 0;
        u32 vbo_nrm = 0;
        u32 vbo_bnrm = 0;
        u32 vbo_tang = 0;
        u32 vbo_bone = 0;
        u32 ibo = 0;

        rm->storage().read(gpu_handle, [&](const GpuModelAsset& gpu)
            {
                vao = gpu.vao;
                vbo_pos = gpu.vbo_pos;
                vbo_uv = gpu.vbo_uv;
                vbo_nrm = gpu.vbo_nrm;
                vbo_bnrm = gpu.vbo_bnrm;
                vbo_tang = gpu.vbo_tang;
                vbo_bone = gpu.vbo_bone;
                ibo = gpu.ibo;
            });

        // GL teardown
        CheckAndThrowGLErrors();
        if (ibo != 0) { glDeleteBuffers(1, reinterpret_cast<GLuint*>(&ibo)); ibo = 0; }
        if (vbo_bone != 0) glDeleteBuffers(1, reinterpret_cast<GLuint*>(&vbo_bone));
        if (vbo_tang != 0) { glDeleteBuffers(1, reinterpret_cast<GLuint*>(&vbo_tang)); vbo_tang = 0; }
        if (vbo_bnrm != 0) { glDeleteBuffers(1, reinterpret_cast<GLuint*>(&vbo_bnrm)); vbo_bnrm = 0; }
        if (vbo_nrm != 0) { glDeleteBuffers(1, reinterpret_cast<GLuint*>(&vbo_nrm)); vbo_nrm = 0; }
        if (vbo_uv != 0) { glDeleteBuffers(1, reinterpret_cast<GLuint*>(&vbo_uv)); vbo_uv = 0; }
        if (vbo_pos != 0) { glDeleteBuffers(1, reinterpret_cast<GLuint*>(&vbo_pos)); vbo_pos = 0; }
        if (vao != 0) { glDeleteVertexArrays(1, reinterpret_cast<GLuint*>(&vao)); vao = 0; }
        CheckAndThrowGLErrors();

        // Write back zeros + reset state
        rm->storage().modify(gpu_handle, [&](GpuModelAsset& gpu)
            {
                gpu.vao = 0;
                gpu.vbo_pos = 0;
                gpu.vbo_nrm = 0;
                gpu.vbo_bnrm = 0;
                gpu.vbo_tang = 0;
                gpu.vbo_uv = 0;
                gpu.vbo_bone = 0;
                gpu.ibo = 0;

                gpu.submeshes.clear();
                gpu.vertex_count = 0;
                gpu.index_count = 0;

                gpu.state = GpuLoadState::Uninitialized;
            });
    }

    GpuTextureInitResult init_gpu_texture(
        const Handle<GpuTextureAsset>& gpu_handle,
        EngineContext& ctx)
    {
        GpuTextureInitResult result{};

        auto rm = std::dynamic_pointer_cast<ResourceManager>(ctx.resource_manager);
        assert(rm);

        bool already_ready = false;
        rm->storage().modify(gpu_handle, [&](GpuTextureAsset& gpu)
            {
                already_ready = (gpu.state == GpuLoadState::Ready || gpu.state == GpuLoadState::Queued);
                if (!already_ready)
                    gpu.state = GpuLoadState::Queued;
            });

        if (already_ready)
        {
            result.ok = true;
            return result;
        }

        try
        {
            Guid texture_guid{};
            Handle<TextureAsset> texture_handle{};
            rm->storage().read(gpu_handle, [&](const GpuTextureAsset& gpu)
                {
                    texture_guid = gpu.texture_ref.guid;
                    texture_handle = gpu.texture_ref.handle;
                });

            if (!rm->storage().validate(texture_handle))
                throw std::runtime_error("GpuTexture init failed: TextureAsset handle is invalid (not bound/loaded).");

            TextureAsset tex{};
            rm->storage().read(texture_handle, [&](const TextureAsset& t)
                {
                    tex = t;
                });

            const auto texture_path = resolve_texture_path(*rm, texture_guid, tex.source_path);

            int w = 0;
            int h = 0;
            int channels = 0;
            unsigned char* image = stbi_load(texture_path.string().c_str(), &w, &h, &channels, 0);
            if (!image)
                throw std::runtime_error("GpuTexture init failed: could not load image " + texture_path.string());

            GLuint internal_format = 0;
            GLuint format = 0;
            if (channels == 1) { internal_format = GL_R8; format = GL_RED; }
            else if (channels == 2) { internal_format = GL_RG8; format = GL_RG; }
            else if (channels == 3) { internal_format = GL_RGB8; format = GL_RGB; }
            else if (channels == 4) { internal_format = GL_RGBA8; format = GL_RGBA; }
            else
                throw std::runtime_error("GpuTexture init failed: unsupported channel count " + std::to_string(channels));

            GLuint tex_id = 0;
            glGenTextures(1, &tex_id);
            glBindTexture(GL_TEXTURE_2D, tex_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format, w, h, 0, format, GL_UNSIGNED_BYTE, image);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            CheckAndThrowGLErrors();

            stbi_image_free(image);

            rm->storage().modify(gpu_handle, [&](GpuTextureAsset& gpu)
                {
                    gpu.gl_id = tex_id;
                    gpu.width = static_cast<u32>(w);
                    gpu.height = static_cast<u32>(h);
                    gpu.channels = static_cast<u32>(channels);
                    gpu.state = GpuLoadState::Ready;
                });

            result.ok = true;
            return result;
        }
        catch (const std::exception& ex)
        {
            rm->storage().modify(gpu_handle, [&](GpuTextureAsset& gpu)
                {
                    gpu.state = GpuLoadState::Failed;
                });
            result.ok = false;
            result.error = ex.what();
            return result;
        }
    }

    void destroy_gpu_texture(
        const Handle<GpuTextureAsset>& gpu_handle,
        EngineContext& ctx)
    {
        auto rm = std::dynamic_pointer_cast<ResourceManager>(ctx.resource_manager);
        assert(rm);

        u32 gl_id = 0;
        rm->storage().read(gpu_handle, [&](const GpuTextureAsset& gpu)
            {
                gl_id = gpu.gl_id;
            });

        if (gl_id != 0)
        {
            glDeleteTextures(1, reinterpret_cast<GLuint*>(&gl_id));
        }

        rm->storage().modify(gpu_handle, [&](GpuTextureAsset& gpu)
            {
                gpu.gl_id = 0;
                gpu.width = 0;
                gpu.height = 0;
                gpu.channels = 0;
                gpu.state = GpuLoadState::Uninitialized;
            });
    }
}
