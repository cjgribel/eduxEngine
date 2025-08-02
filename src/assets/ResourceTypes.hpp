// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef ResourceTypes_h
#define ResourceTypes_h
#include <cstddef>
#include <cstdint>
#include <string>
#include "AssetMetaData.hpp"
#include "AssetRef.hpp"
// #include "Handle.h"
// #include "Guid.h"
#include "LogGlobals.hpp"
// #include "hash_combine.h"
//#include "IResourceManager.hpp" // For AssetRef<>, visit_assets<>

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


    // struct GpuModel {
    //     std::vector<Handle<GpuMesh>> gpu_meshes;
    // };

#if 0

    // COMPONENTS

    struct ModelComponent {
        AssetRef<GpuModel> gpu_model_ref;  // shared
    };

    struct AnimationComponent {
        AnimationFSM animation_state;      // per-instance
        std::vector<Bone> bone_transforms; // per-instance
    };

    // ASSET
    // * ser/deser function that only write/read model_ref ... or, only reg. model_ref to entt::meta
    struct GpuModel {
        /* NOTE */ AssetRef<Model> model_ref;
        GLuint vao;               // Single VAO
        GLuint vertex_buffer;     // All vertices packed
        GLuint index_buffer;      // All indices packed
        struct SubMesh {
            size_t index_offset;
            size_t index_count;
            size_t base_vertex;
            Handle<GpuMaterial> material;
        };
        std::vector<SubMesh> submeshes;
    };

#endif
    /*
    1. GpuModel CREATION
        --> ??? ALWAYS create a GpuModel asset for ModelAsset (made by importer)
        ??? Create lazily, when a ModelComponent is set to reference a Model asset
        ??? INCLUDE the GpuModel inside the Model?
        !!! Take instance-unique data into consideration:
            (transform), animation stuff, bone array etc (maybe shared)

    2. GpuModel IO: just serialize/deserialize AssetRef<Model> (i.e. its GUID)

    2. LEVEL LOAD
        Load & bind assets as usual
        GpuModel now references a Model asset

    3. GPU PHASE
        For all GpuModel assets: initialize GL handles etc via AssetRef<Model>
        * If needed: append to shared GL buffers

    4. RENDERING
        entt view over ModelComponent

    ModelAsset(Persistent)
        ↑ references
        GpuModel(Persistent, but serialized as reference - only)
        ↑ references
        ModelComponent(Persistent)
    */
#if 0
    void render_gpu_model(const GpuModel& gpu_model) // glDrawElementsBaseVertex
    {
        glBindVertexArray(gpu_model.vao);

        for (const auto& submesh : gpu_model.submeshes)
        {
            bind_material(submesh.material);
            glDrawElementsBaseVertex(
                GL_TRIANGLES,
                submesh.index_count,
                GL_UNSIGNED_INT,
                reinterpret_cast<void*>(submesh.index_offset * sizeof(uint32_t)),
                submesh.base_vertex);
        }

        glBindVertexArray(0);
    }
#endif

#if 0
    // RENDER SYSTEM
    void RenderSystem::render(const RenderableComponent& renderable, const GpuResourceManager& gpu_mgr) {
        const auto& gpu_mesh = gpu_mgr.get(renderable.mesh_handle);

        glBindVertexArray(gpu_mesh.vao);
        bind_material(gpu_mesh.material, gpu_mgr);

        set_shader_uniforms(renderable.transform);

        glDrawElements(GL_TRIANGLES, gpu_mesh.index_count, GL_UNSIGNED_INT, nullptr);

        glBindVertexArray(0);
    }
#endif

} // namespace eeng

namespace eeng::mock
{
    struct Mesh
    {
        std::vector<float> vertices;
    };

    template<typename Visitor> void visit_assets(Mesh&, Visitor&&) {}
    /* try to remove */ template<typename Visitor> void visit_assets(const Mesh&, Visitor&&) {}

    struct Texture
    {
        std::string name;
    };

    template<typename Visitor> void visit_assets(Texture&, Visitor&&) {}
    /* try to remove */ template<typename Visitor> void visit_assets(const Texture&, Visitor&&) {}

    struct Model
    {
        std::vector<AssetRef<Mesh>> meshes;
        std::vector<AssetRef<Texture>> textures;
    };

    // Found via ADL if visit_assets is called unqualified
    // (e.g. visit_assets(model, visitor))
    template<typename Visitor>
    void visit_assets(Model& model, Visitor&& visitor)
    {
        for (auto& ref : model.meshes)
            visitor(ref);

        for (auto& ref : model.textures)
            visitor(ref);
    }

    // Used by ResourceManager::file
    template<typename Visitor>
    void visit_assets(const Model& model, Visitor&& visitor)
    {
        for (auto& ref : model.meshes)
            visitor(ref);

        for (auto& ref : model.textures)
            visitor(ref);
    }

    struct MockResource1
    {
        int x{};
        float y{};

        // Default constructor
        MockResource1() {
            eeng::LogGlobals::log("MockResource1 default-constructed");
        }

        // Copy constructor
        MockResource1(const MockResource1& other)
            : x(other.x), y(other.y)
        {
            eeng::LogGlobals::log("MockResource1 copy-constructed");
        }

        // Copy assignment
        MockResource1& operator=(const MockResource1& other)
        {
            if (this != &other) {
                x = other.x;
                y = other.y;
                eeng::LogGlobals::log("MockResource1 copy-assigned");
            }
            return *this;
        }

        // Move constructor
        MockResource1(MockResource1&& other) noexcept
            : x(other.x), y(other.y)
        {
            other.x = 0;
            other.y = 0;
            eeng::LogGlobals::log("MockResource1 move-constructed");
        }

        // Move assignment
        MockResource1& operator=(MockResource1&& other) noexcept
        {
            if (this != &other) {
                x = other.x;
                y = other.y;
                other.x = 0;
                other.y = 0;
                eeng::LogGlobals::log("MockResource1 move-assigned");
            }
            return *this;
        }

        // Equality
        bool operator==(const MockResource1& other) const
        {
            return x == other.x && y == other.y;
        }

        // Destructor
        ~MockResource1() {
            eeng::LogGlobals::log("MockResource1 destroyed");
        }
    };

    template<typename Visitor> void visit_assets(MockResource1&, Visitor&&) {}
    /* try to remove */ template<typename Visitor> void visit_assets(const MockResource1&, Visitor&&) {}

    struct MockResource2
    {
        size_t y = 0;
        Handle<MockResource1> ref1; // Reference to another resource

        bool operator==(const MockResource2& other) const
        {
            return y == other.y && ref1 == other.ref1;
        }
    };

    template<typename Visitor> void visit_assets(MockResource2&, Visitor&&) {}
    /* try to remove */ template<typename Visitor> void visit_assets(const MockResource2&, Visitor&&) {}

} // namespace eeng

#endif // ResourceTypes_h
