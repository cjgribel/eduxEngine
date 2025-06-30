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

namespace eeng::mock {

    void Importer::import(EngineContextPtr ctx)
    {
        std::cout << "MockImporter::import" << std::endl;
        auto& resource_manager = static_cast<ResourceManager&>(*ctx->resource_manager);

        // + use thread pool

        // Imported resources:
        Mesh mesh;
        Model model;

        // File mesh and add to model
        auto mesh_ref = resource_manager.file(mesh);
        model.meshes.push_back(mesh_ref);
        // File model
        auto model_ref = resource_manager.file(model);

        // Later: load asset to memory
        resource_manager.load(model_ref, *ctx);

        // VISUALIZE storage + asset_index (available assets as a file structure)

        // Later: unload asset
        // ...

        // Later: unfile the asset
        // ...
    }

} // namespace eeng
