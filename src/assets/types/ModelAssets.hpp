// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AssetRef.hpp"
#include "VecTree.h"

namespace eeng::assets
{
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using i32 = std::int32_t;

    static constexpr int bones_per_vertex = 4;
    static constexpr int null_index = -1;

    // -------------------------------------------------------------------------
    // GpuModelAsset
    // -------------------------------------------------------------------------

    struct ModelDataAsset; // forward

    enum class GpuModelState : u8
    {
        Uninitialized = 0,   // loaded from disk, not yet gpu-initialized
        Queued,              // marked for gpu init
        Ready,               // gpu resources exist
        Failed               // init failed (missing deps, GL error, etc.)
    };

    struct GpuModelAsset
    {
        AssetRef<ModelDataAsset> model_ref;

        // Runtime-only state (do not serialize)
        GpuModelState state = GpuModelState::Uninitialized;

        // v1: no GL handles yet. Add later.
        // GLuint vao = 0;
        // ...
    };

    template<typename Visitor>
    void visit_asset_refs(GpuModelAsset& gm, Visitor&& visitor)
    {
        visitor(gm.model_ref);
    }

    template<typename Visitor>
    void visit_asset_refs(const GpuModelAsset& gm, Visitor&& visitor)
    {
        visitor(gm.model_ref);
    }

    // -------------------------------------------------------------------------
    // TextureAsset
    // -------------------------------------------------------------------------

    enum class TextureColorSpace : u8
    {
        Linear = 0,
        SRgb
    };

    struct TextureImportSettings
    {
        TextureColorSpace color_space = TextureColorSpace::SRgb;
        bool generate_mips = true;
        bool is_normal_map = false;
    };

    struct TextureAsset
    {
        std::string source_path;
        TextureImportSettings import_settings{};
    };

    template<typename Visitor>
    void visit_asset_refs(TextureAsset&, Visitor&&)
    {
    }

    template<typename Visitor>
    void visit_asset_refs(const TextureAsset&, Visitor&&)
    {
    }

    // -------------------------------------------------------------------------
    // MaterialAsset
    // -------------------------------------------------------------------------

    enum class MaterialTextureSlot : u8
    {
        Diffuse = 0,
        Normal,
        Specular,
        Opacity,
        Count
    };

    struct MaterialAsset
    {
        glm::vec3 Ka = { 0.25f, 0.0f, 0.0f };
        glm::vec3 Kd = { 0.75f, 0.0f, 0.0f };
        glm::vec3 Ks = { 1.0f, 1.0f, 1.0f };
        float shininess = 10.0f;

        std::array<AssetRef<TextureAsset>, static_cast<size_t>(MaterialTextureSlot::Count)> textures{};
    };

    template<typename Visitor>
    void visit_asset_refs(MaterialAsset& m, Visitor&& visitor)
    {
        for (auto& t : m.textures)
        {
            visitor(t);
        }
    }

    template<typename Visitor>
    void visit_asset_refs(const MaterialAsset& m, Visitor&& visitor)
    {
        for (auto& t : m.textures)
        {
            visitor(t);
        }
    }

    // -------------------------------------------------------------------------
    // Skeleton / skin / animation (owned by ModelData)
    // -------------------------------------------------------------------------

    struct SkeletonNode
    {
        glm::mat4 local_bind_tfm{ 1.0f };

        // Runtime and not part of raw model data, keep for now.
        glm::mat4 global_tfm{ 1.0f };

        i32 bone_index = null_index;
        i32 nbr_meshes = 0;

        std::string name;

        bool operator==(const SkeletonNode& other) const
        {
            return name == other.name;
        }
    };

    struct Bone
    {
        glm::mat4 inverse_bind_tfm{ 1.0f };
        i32 node_index = null_index;
        std::string name;
    };

    struct SkinData
    {
        std::array<u32, bones_per_vertex> bone_indices{ 0, 0, 0, 0 };
        std::array<float, bones_per_vertex> bone_weights{ 0.0f, 0.0f, 0.0f, 0.0f };
    };

    struct NodeKeyframes
    {
        bool is_used = false;
        std::vector<glm::vec3> pos_keys;
        std::vector<glm::vec3> scale_keys;
        std::vector<glm::quat> rot_keys;
    };

    struct AnimClip
    {
        std::string name;
        float duration_ticks = 0.0f;
        float ticks_per_second = 25.0f;

        // Indexed by VecTree node index
        std::vector<NodeKeyframes> node_animations;
    };

    // -------------------------------------------------------------------------
    // ModelData (raw model asset)
    // -------------------------------------------------------------------------

    struct SubMesh
    {
        u32 base_index = 0;
        u32 nbr_indices = 0;

        u32 base_vertex = 0;
        u32 nbr_vertices = 0;

        AssetRef<MaterialAsset> material;

        i32 node_index = null_index;
        bool is_skinned = false;
    };

    struct ModelDataAsset
    {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec3> tangents;
        std::vector<glm::vec3> binormals;
        std::vector<glm::vec2> texcoords;

        // Size should match positions.size() if skinning is present.
        // v1: we resize to positions.size() always (zero weights = non-skinned).
        std::vector<SkinData> skin;

        std::vector<u32> indices;
        std::vector<SubMesh> submeshes;

        VecTree<SkeletonNode> nodetree;
        std::vector<Bone> bones;
        std::vector<AnimClip> animations;
    };

    // -------------------------------------------------------------------------
    // AssetRef traversal (ADL hooks)
    // -------------------------------------------------------------------------

    template<typename Visitor>
    void visit_asset_refs(SubMesh& sm, Visitor&& visitor)
    {
        visitor(sm.material);
    }

    template<typename Visitor>
    void visit_asset_refs(const SubMesh& sm, Visitor&& visitor)
    {
        visitor(sm.material);
    }

    template<typename Visitor>
    void visit_asset_refs(ModelDataAsset& model, Visitor&& visitor)
    {
        for (auto& sm : model.submeshes)
        {
            visit_asset_refs(sm, visitor);
        }
    }

    template<typename Visitor>
    void visit_asset_refs(const ModelDataAsset& model, Visitor&& visitor)
    {
        for (auto& sm : model.submeshes)
        {
            visit_asset_refs(sm, visitor);
        }
    }
}
