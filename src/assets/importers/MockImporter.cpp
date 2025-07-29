// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "MockImporter.hpp"
#include "ResourceManager.hpp" // Treats IResourceManager as this type
#include "Storage.hpp"
#include "ThreadPool.hpp"
// #include "Guid.h"
#include "AssetMetaData.hpp"
#include "AssetRef.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <atomic> // For thread-safe unique identifiers
#include <format> // For formatted strings

namespace eeng::mock {

    AssetRef<Model> ModelImporter::import(
        const std::filesystem::path& assets_root,
        EngineContextPtr ctx)
    {
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
            std::string(entt::resolve<Mesh>().info().name()) // type name, alt. entt::type_name<T>
        };
        resource_manager.file(
            mesh,
            mesh_file_path,
            mesh_meta,
            mesh_meta_file_path); // Serialize to file
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
            std::string(entt::resolve<Texture>().info().name()) // type name
        };
        resource_manager.file(
            texture,
            texture_file_path,
            texture_meta,
            texture_meta_file_path); // Serialize to file
        // Add mesh reference to model
        model.textures.push_back(texture_ref);

        // File model
        auto model_ref = AssetRef<Model>{ model_guid };
        auto model_meta = AssetMetaData{
            model_guid,
            Guid::invalid(), // Parent GUID
            std::string("MockModel") + std::to_string(value), // name
            std::string(entt::resolve<Model>().info().name()) // type name
            // model_file_path // desired filepath
        };
        resource_manager.file(
            model,
            model_file_path,
            model_meta,
            model_meta_file_path); // Serialize to file
        //auto model_ref = resource_manager.file(model, model_path);

        return model_ref;
    }

} // namespace eeng
