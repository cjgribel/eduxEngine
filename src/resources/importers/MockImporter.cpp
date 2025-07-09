// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "MockImporter.hpp"
#include "ResourceManager.hpp" // Treats IResourceManager as this type
#include "Storage.hpp"
#include "ThreadPool.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <atomic> // For thread-safe unique identifiers
#include <format>

namespace eeng::mock {

    AssetRef<Model> ModelImporter::import(EngineContextPtr ctx)
    {
        std::cout << "MockImporter::import" << std::endl;
        auto& resource_manager = static_cast<ResourceManager&>(*ctx->resource_manager);

        // Generate unique identifiers for mock resources
        static std::atomic<int> counter{ 0 };  // thread-safe static counter
        int value = counter.fetch_add(1, std::memory_order_relaxed);  // or use memory_order_seq_cst
        auto mesh_path = std::format("/Users/ag1498/GitHub/eduEngine/Module1/project1/meshes/mock_mesh{}.json", value);
        auto model_path = std::format("/Users/ag1498/GitHub/eduEngine/Module1/project1/models/mock_model{}.json", value);

        // + use thread pool, e.g. for "textures" (by index to assimp's texture array)

        // Imported resources:
        Mesh mesh;
        Model model;

        // File mesh
        auto mesh_ref = resource_manager.file(mesh, mesh_path);

        // Add mesh reference to model
        model.meshes.push_back(mesh_ref);

        // File model
        auto model_ref = resource_manager.file(model, model_path);

        return model_ref;
    }

} // namespace eeng
