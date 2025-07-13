// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "MockImporter.hpp"
#include "ResourceManager.hpp" // Treats IResourceManager as this type
#include "Storage.hpp"
#include "ThreadPool.hpp"
#include "Guid.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <atomic> // For thread-safe unique identifiers
#include <format> // For formatted strings

namespace eeng::mock {

    AssetRef<Model> ModelImporter::import(EngineContextPtr ctx)
    {
        std::cout << "MockImporter::import" << std::endl;
        auto& resource_manager = static_cast<ResourceManager&>(*ctx->resource_manager);

        // Generate unique identifiers for mock resources
        static std::atomic<int> counter{ 0 };  // thread-safe static counter
        int value = counter.fetch_add(1, std::memory_order_relaxed);  // or use memory_order_seq_cst
        auto asset_root = "/Users/ag1498/GitHub/eduEngine/Module1/project1/assets/MockModel1/";
        auto mesh_path = std::format("{}meshes/mock_mesh{}.json", asset_root, value);
        auto mesh_meta_path = std::format("{}meshes/mock_mesh{}.meta.json", asset_root, value);
        auto model_path = std::format("{}models/mock_model{}.json", asset_root, value);
        auto model_meta_path = std::format("{}models/mock_model{}.meta.json", asset_root, value);

        // + make sure folders exist
        // + use std::filesystem for paths
        // + use thread pool, e.g. for "textures" (by index to assimp's texture array)

        // Imported resources:
        Mesh mesh;
        Model model;

        auto model_guid = Guid::generate();

        // Mesh
        // - Create meta data
        // - File
        auto mesh_ref = AssetRef<Mesh>{ Guid::generate() };
        auto mesh_meta = AssetMetaData{
            Guid::generate(),
            model_guid, // Parent GUID
            std::string("MockMesh") + std::to_string(value), // name
            std::string(entt::resolve<Mesh>().info().name()), // type name
            mesh_path // desired filepath
        };
        resource_manager.file(
            mesh, 
            mesh_path, 
            mesh_meta, 
            mesh_meta_path); // Serialize to file
        //auto mesh_ref = resource_manager.file(mesh, mesh_path);

        // Add mesh reference to model
        model.meshes.push_back(mesh_ref);

        // File model
        auto model_ref = AssetRef<Model>{ Guid::generate() };
        auto model_meta = AssetMetaData{
            Guid::generate(),
            Guid::invalid(), // Parent GUID
            std::string("MockModel") + std::to_string(value), // name
            std::string(entt::resolve<Model>().info().name()), // type name
            model_path // desired filepath
        };
        resource_manager.file(
            model, 
            model_path, 
            model_meta, 
            model_meta_path); // Serialize to file
        //auto model_ref = resource_manager.file(model, model_path);

        return model_ref;
    }

} // namespace eeng
