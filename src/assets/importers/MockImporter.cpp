// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "MockImporter.hpp"
#include "assets/types/ModelAssets.hpp"
#include "meta/MetaAux.h"
#include "ResourceManager.hpp" // Treats IResourceManager as this type
// #include "Storage.hpp"
#include "ThreadPool.hpp"
// #include "Guid.h"
#include "AssetMetaData.hpp"
#include "AssetRef.hpp"
#include "Guid.h"
//#include "LogGlobals.hpp"

// #include <assimp/Importer.hpp>
// #include <assimp/scene.h>
// #include <assimp/postprocess.h>
#include <iostream>
#include <atomic> // For thread-safe counter
#include <format> // For formatted strings
#include <filesystem>

using namespace eeng::assets;

namespace
{

    struct QuadDesc
    {
        glm::vec3 origin;
        glm::vec2 size;
        glm::vec3 normal;
    };

    void append_quad(
        ModelDataAsset& model,
        const QuadDesc& q,
        const eeng::AssetRef<MaterialAsset>& material_ref)
    {
        const u32 base_vertex = static_cast<u32>(model.positions.size());
        const u32 base_index = static_cast<u32>(model.indices.size());

        // Four corners in XY plane at origin, offset by origin
        // (Keep it dead simple; UVs 0..1)
        const glm::vec3 p0 = q.origin + glm::vec3(0.0f, 0.0f, 0.0f);
        const glm::vec3 p1 = q.origin + glm::vec3(q.size.x, 0.0f, 0.0f);
        const glm::vec3 p2 = q.origin + glm::vec3(q.size.x, q.size.y, 0.0f);
        const glm::vec3 p3 = q.origin + glm::vec3(0.0f, q.size.y, 0.0f);

        model.positions.push_back(p0);
        model.positions.push_back(p1);
        model.positions.push_back(p2);
        model.positions.push_back(p3);

        model.texcoords.push_back(glm::vec2(0.0f, 0.0f));
        model.texcoords.push_back(glm::vec2(1.0f, 0.0f));
        model.texcoords.push_back(glm::vec2(1.0f, 1.0f));
        model.texcoords.push_back(glm::vec2(0.0f, 1.0f));
        
        model.normals.push_back(q.normal);
        model.normals.push_back(q.normal);
        model.normals.push_back(q.normal);
        model.normals.push_back(q.normal);

        // model.tangents.resize(4);

        // model.binormals.resize(4);

        // Optional channels empty for now (tangents/binormals)
        // Skin: keep sized == positions (zero weights = non-skinned)
        // model.skin.resize(model.positions.size());

        // Two triangles (0,1,2) (0,2,3)
        model.indices.push_back(0);
        model.indices.push_back(1);
        model.indices.push_back(2);

        model.indices.push_back(0);
        model.indices.push_back(2);
        model.indices.push_back(3);

        SubMesh sm{};
        sm.base_vertex = base_vertex;
        sm.nbr_vertices = 4;
        sm.base_index = base_index;
        sm.nbr_indices = 6;
        sm.material = material_ref;
        sm.is_skinned = false;
        sm.node_index = null_index;

        model.submeshes.push_back(sm);
    }
}

namespace eeng::mock {

    AssetRef<Model> ModelImporter::import(
        const std::filesystem::path& assets_root,
        EngineContextPtr ctx)
    {
        assert(meta::type_is_registered<Model>());
        assert(meta::type_is_registered<Mesh>());
        assert(meta::type_is_registered<Texture>());

        std::cout << "MockImporter::import" << std::endl;
        auto& resource_manager = static_cast<ResourceManager&>(*ctx->resource_manager);

        // Generate unique identifiers for mock resources
        static std::atomic<int> counter{ 0 };  // thread-safe static counter
        int value = counter.fetch_add(1, std::memory_order_relaxed);  // or use memory_order_seq_cst

        auto asset_path = assets_root / std::format("MockModel{}", value);
        auto mesh_path = asset_path / "meshes";
        auto texture_path = asset_path / "textures";
        std::filesystem::create_directories(asset_path);
        std::filesystem::create_directories(mesh_path);
        std::filesystem::create_directories(texture_path);

        auto model_file_path = asset_path / "mock_model.json";
        auto model_meta_file_path = asset_path / "mock_model.meta.json";

        auto mesh_file_path = mesh_path / "mock_mesh.json";
        auto mesh_meta_file_path = mesh_path / "mock_mesh.meta.json";

        auto texture_file_path = texture_path / "mock_texture.json";
        auto texture_meta_file_path = texture_path / "mock_texture.meta.json";

        // + make sure folders exist
        // + use std::filesystem for paths
        // + use thread pool, e.g. for "textures" (by index to assimp's texture array)

        // Imported resources:
        Mesh mesh{ {1.0f, 2.0f, 3.0f} };
        Texture texture{ {"texture.png"} };
        Model model;

        auto model_guid = Guid::generate();

        // Mesh
        // - Create meta data
        // - File
        auto mesh_guid = Guid::generate();
        auto mesh_ref = AssetRef<Mesh>{ mesh_guid };
        auto mesh_meta = AssetMetaData{
            mesh_guid,
            model_guid, // Parent GUID
            std::string("MockMesh") + std::to_string(value), // name
            meta::get_meta_type_id_string<Mesh>()
        };
        resource_manager.import(
            mesh,
            mesh_file_path.string(),
            mesh_meta,
            mesh_meta_file_path.string()); // Serialize to file
        // Add mesh reference to model
        model.meshes.push_back(mesh_ref);

        // Texture
        // - Create meta data
        // - File
        auto texture_guid = Guid::generate();
        auto texture_ref = AssetRef<Texture>{ texture_guid };
        auto texture_meta = AssetMetaData{
            texture_guid,
            model_guid, // Parent GUID
            std::string("MockTexture") + std::to_string(value), // name
            meta::get_meta_type_id_string<Texture>(),
        };
        resource_manager.import(
            texture,
            texture_file_path.string(),
            texture_meta,
            texture_meta_file_path.string()); // Serialize to file
        // Add mesh reference to model
        model.textures.push_back(texture_ref);

        // File model
        auto model_ref = AssetRef<Model>{ model_guid };
        auto model_meta = AssetMetaData{
            model_guid,
            Guid::invalid(), // Parent GUID
            std::string("MockModel") + std::to_string(value), // name
            meta::get_meta_type_id_string<Model>(),
            // model_file_path // desired filepath
        };
        resource_manager.import(
            model,
            model_file_path.string(),
            model_meta,
            model_meta_file_path.string()); // Serialize to file
        //auto model_ref = resource_manager.file(model, model_path);

        // DELAY
        // std::this_thread::sleep_for(std::chrono::seconds(2));

        return model_ref;
    }

    AssetRef<GpuModelAsset> ModelImporter::import_quads_modeldata(
        const std::filesystem::path& assets_root,
        EngineContextPtr ctx)
    {
        assert(meta::type_is_registered<ModelDataAsset>());
        assert(meta::type_is_registered<MaterialAsset>());
        assert(meta::type_is_registered<TextureAsset>());
        assert(meta::type_is_registered<GpuMaterialAsset>());
        assert(meta::type_is_registered<GpuTextureAsset>());

        std::cout << "MockImporter::import_quads_modeldata\n";

        auto& resource_manager = static_cast<ResourceManager&>(*ctx->resource_manager);

        static std::atomic<int> counter{ 0 };
        const int value = counter.fetch_add(1, std::memory_order_relaxed);

        const auto asset_path = assets_root / std::format("MockQuadsModel{}", value);
        const auto model_path = asset_path / "model";
        const auto mtl_path = asset_path / "materials";
        const auto tex_path = asset_path / "textures";
        const auto gpumodel_path = asset_path / "gpu_model";
        const auto gpumtl_path = asset_path / "gpu_materials";
        const auto gputex_path = asset_path / "gpu_textures";

        std::filesystem::create_directories(model_path);
        std::filesystem::create_directories(mtl_path);
        std::filesystem::create_directories(tex_path);
        std::filesystem::create_directories(gpumodel_path);
        std::filesystem::create_directories(gpumtl_path);
        std::filesystem::create_directories(gputex_path);

        const auto model_file_path = model_path / "model_data.json";
        const auto model_meta_file_path = model_path / "model_data.meta.json";

        const auto mtl_file_path = mtl_path / "material.json";
        const auto mtl_meta_file_path = mtl_path / "material.meta.json";

        const auto tex_file_path = tex_path / "texture.json";
        const auto tex_meta_file_path = tex_path / "texture.meta.json";

        const auto gpumodel_file_path = gpumodel_path / "gpu_model.json";
        const auto gpumodel_meta_file_path = gpumodel_path / "gpu_model.meta.json";

        const auto gpumtl_file_path = gpumtl_path / "gpu_material.json";
        const auto gpumtl_meta_file_path = gpumtl_path / "gpu_material.meta.json";

        const auto gputex_file_path = gputex_path / "gpu_texture.json";
        const auto gputex_meta_file_path = gputex_path / "gpu_texture.meta.json";

        // GUIDs
        const Guid model_guid = Guid::generate();
        const Guid mtl_guid = Guid::generate();
        const Guid tex_guid = Guid::generate();
        const Guid gpu_model_guid = Guid::generate();
        const Guid gpu_mtl_guid = Guid::generate();
        const Guid gpu_tex_guid = Guid::generate();

        // Optional texture asset (kept simple: reference a fake path)

        TextureAsset tex{};
        tex.source_path = "texture.png";
        tex.import_settings.color_space = TextureColorSpace::SRgb;

        const auto tex_meta = AssetMetaData{
            tex_guid,
            model_guid,
            std::string("MockTexture") + std::to_string(value),
            meta::get_meta_type_id_string<TextureAsset>()
        };

        resource_manager.import(
            tex,
            tex_file_path.string(),
            tex_meta,
            tex_meta_file_path.string());

        // GpuTextureAsset referencing the TextureAsset

        const auto gpu_tex = GpuTextureAsset{
            .texture_ref = AssetRef<TextureAsset>{ tex_guid },
            .state = GpuLoadState::Uninitialized
        };

        resource_manager.import(
            gpu_tex,
            gputex_file_path.string(),
            AssetMetaData{
                gpu_tex_guid,
                tex_guid,
                std::string("MockGpuTexture") + std::to_string(value),
                meta::get_meta_type_id_string<GpuTextureAsset>()
            },
            gputex_meta_file_path.string());

        // Material referencing the texture

        MaterialAsset mtl{};
        mtl.Ka = glm::vec3(1.0f, 0.0f, 0.0f);
        mtl.Kd = glm::vec3(0.0f, 1.0f, 0.0f);
        mtl.Ks = glm::vec3(0.0f, 0.0f, 1.0f);
        mtl.textures[static_cast<size_t>(MaterialTextureSlot::Diffuse)] = AssetRef<TextureAsset>{ tex_guid };

        const auto mtl_meta = AssetMetaData{
            mtl_guid,
            gpu_model_guid, // parent not used
            std::string("MockMaterial") + std::to_string(value),
            meta::get_meta_type_id_string<MaterialAsset>()
        };

        resource_manager.import(
            mtl,
            mtl_file_path.string(),
            mtl_meta,
            mtl_meta_file_path.string());

        // GpuMaterialAsset referencing the GpuTextureAsset

        GpuMaterialAsset gpu_mtl{};
        gpu_mtl.material_ref = AssetRef<MaterialAsset>{ mtl_guid };
        gpu_mtl.Ka = mtl.Ka;
        gpu_mtl.Kd = mtl.Kd;
        gpu_mtl.Ks = mtl.Ks;
        gpu_mtl.shininess = mtl.shininess;
        gpu_mtl.textures[static_cast<size_t>(MaterialTextureSlot::Diffuse)] = AssetRef<GpuTextureAsset>{ gpu_tex_guid };

        resource_manager.import(
            gpu_mtl,
            gpumtl_file_path.string(),
            AssetMetaData{
                gpu_mtl_guid,
                mtl_guid,
                std::string("MockGpuMaterial") + std::to_string(value),
                meta::get_meta_type_id_string<GpuMaterialAsset>()
            },
            gpumtl_meta_file_path.string());

        // ModelData containing two quads, both using the same material

        ModelDataAsset model{};
        const auto mtl_ref = AssetRef<MaterialAsset>{ mtl_guid };

        append_quad(model, QuadDesc{ .origin = { -1.0f, 0.0f, 0.0f }, .size = { 1.0f, 1.0f }, .normal = { 0.0f, 0.0f, 1.0f } }, mtl_ref);
        append_quad(model, QuadDesc{ .origin = {  0.25f, 0.0f, 0.0f }, .size = { 1.0f, 1.0f }, .normal = { 0.0f, 0.0f, 1.0f } }, mtl_ref);

        const auto model_meta = AssetMetaData{
            model_guid,
            gpu_model_guid, // parent not used
            std::string("MockQuadsModel") + std::to_string(value),
            meta::get_meta_type_id_string<ModelDataAsset>()
        };

        resource_manager.import(
            model,
            model_file_path.string(),
            model_meta,
            model_meta_file_path.string());

        // GpuModelAsset referencing the ModelDataAsset and GPU materials

        GpuModelAsset gpu_model{};
        gpu_model.model_ref = AssetRef<ModelDataAsset>{ model_guid };
        gpu_model.state = GpuLoadState::Uninitialized;
        for (const auto& sm : model.submeshes)
        {
            GpuSubMesh gsm{};
            gsm.index_offset = sm.base_index;
            gsm.index_count = sm.nbr_indices;
            gsm.base_vertex = sm.base_vertex;
            gsm.material = AssetRef<GpuMaterialAsset>{ gpu_mtl_guid };
            gpu_model.submeshes.push_back(gsm);
        }

        resource_manager.import(
            gpu_model,
            gpumodel_file_path.string(),
            AssetMetaData{
                gpu_model_guid,
                model_guid,
                std::string("MockGpuModel") + std::to_string(value),
                meta::get_meta_type_id_string<GpuModelAsset>()
            },
            gpumodel_meta_file_path.string());

        return AssetRef<GpuModelAsset>{ gpu_model_guid };
    }

} // namespace eeng
