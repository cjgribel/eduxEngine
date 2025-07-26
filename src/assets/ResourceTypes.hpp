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


} // namespace eeng



namespace eeng::mock
{
    struct Mesh
    {
        std::vector<float> vertices;
    };

    struct Texture
    {
        std::string name;
    };

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

    struct MockResource2
    {
        size_t y = 0;
        Handle<MockResource1> ref1; // Reference to another resource

        bool operator==(const MockResource2& other) const
        {
            return y == other.y && ref1 == other.ref1;
        }
    };

} // namespace eeng

#endif // ResourceTypes_h
