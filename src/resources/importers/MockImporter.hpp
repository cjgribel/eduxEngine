// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef MockImporter_hpp
#define MockImporter_hpp

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "Handle.h"
#include "EngineContext.hpp"

// Placeholder resources
namespace eeng
{
    // struct Mesh { size_t x; };
    // struct Material { size_t x; };
    // struct Skeleton { size_t x; };
    // struct AnimationClip { size_t x; };

    // using MeshHandle = Handle<Mesh>;
    // using MaterialHandle = Handle<Material>;
    // using TextureHandle = Handle<Texture2D>;
    // using SkeletonHandle = Handle<Skeleton>;
    // using AnimationClipHandle = Handle<AnimationClip>;

    // struct Model
    // {
    //     std::vector<Handle<Mesh>> meshes;
    //     std::vector<Handle<Texture2D>> textures;
    //     std::vector<Handle<Material>> materials;
    //     std::optional<Handle<Skeleton>> skeleton;
    //     std::vector<Handle<AnimationClip>> animation_clips;

    //     std::string source_file;
    //     uint32_t loading_flags;
    // };
    // using ModelHandle = Handle<Model>;


} // namespace eeng

namespace eeng::mock
{
    struct Mesh
    {
        std::vector<float> vertices;
    };
    
    struct Model
    {
        std::vector<AssetRef<Mesh>> meshes;
    };

    class Importer
    {
    public:
        static void import(EngineContextPtr ctx);

        
    };

#if 0
    class MockImporter
    {
    public:
        MockImporter(IResourceManager* rm) : resource_manager_(rm) {}

        void import(const std::string& source)
        {
            // Simulate model import with meshes
            auto model_guid = Guid::create();
            MockModel model{ model_guid };

            std::vector<std::future<void>> mesh_futures;

            for (int i = 0; i < 5; ++i)
            {
                mesh_futures.push_back(
                    resource_manager_->get_thread_pool().submit([this, &model, i] {
                        import_mesh(model, i);
                        })
                );
            }

            for (auto& fut : mesh_futures)
                fut.get(); // wait for all meshes

            resource_manager_->add_resource(model_guid, model);
        }

    private:
        void import_mesh(MockModel& model, int mesh_index)
        {
            // Create mock mesh
            auto mesh_guid = Guid::create();
            MockMesh mesh{ mesh_guid };

            // Concurrently import material
            auto material_future = resource_manager_->get_thread_pool().submit([this, mesh_index] {
                return import_material(mesh_index);
                });

            mesh.material_guid = material_future.get();

            resource_manager_->add_resource(mesh_guid, mesh);

            // Add mesh to model
            std::lock_guard lock(model_mutex_);
            model.mesh_guids.push_back(mesh_guid);
        }

        Guid import_material(int index)
        {
            auto material_guid = Guid::create();
            MockMaterial material{ material_guid };
            resource_manager_->add_resource(material_guid, material);
            return material_guid;
        }

        IResourceManager* resource_manager_;
        std::mutex model_mutex_; // protects model.mesh_guids vector
};
#endif

} // namespace eeng

#endif // ASSIMP_IMPORTER_HPP