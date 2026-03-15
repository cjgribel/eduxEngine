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

    struct ModelDataAsset;
    struct GpuMaterialAsset;
    struct MaterialAsset;
    struct TextureAsset;

    /// @brief GPU-side submesh, used directly by the renderer.
    /// Carries draw ranges and GPU material references.
    struct GpuSubMesh
    {
        /// @brief Index offset into GPU index buffer.
        u32 index_offset = 0;
        /// @brief Number of indices to draw.
        u32 index_count = 0;
        /// @brief Base vertex offset into GPU vertex buffers.
        u32 base_vertex = 0;

        /// @brief GPU material used for this submesh at draw time.
        AssetRef<assets::GpuMaterialAsset> material;
    };

    /// @brief Generic GPU load state for runtime-bound assets.
    enum class GpuLoadState : u8
    {
        Uninitialized = 0,   // loaded from disk, not yet gpu-initialized
        Queued,              // marked for gpu init
        Ready,               // gpu resources exist
        Failed               // init failed (missing deps, GL error, etc.)
    };

    /// @brief GPU binding for a model (runtime draw data).
    /// Separate from ModelDataAsset (raw/authoring geometry).
    struct GpuModelAsset
    {
        /// @brief Source CPU model data.
        AssetRef<ModelDataAsset> model_ref;

        /// @brief Runtime-only state (do not serialize).
        GpuLoadState state = GpuLoadState::Uninitialized;

        /// @brief GL handles for mesh buffers.
        u32 vao = 0;
        u32 vbo_pos = 0;
        u32 vbo_uv = 0;
        u32 vbo_nrm = 0;
        u32 vbo_bnrm = 0;
        u32 vbo_tang = 0;
        u32 vbo_bone = 0; // opt
        u32 ibo = 0;

        /// @brief GPU draw ranges and material bindings.
        std::vector<GpuSubMesh> submeshes;

        /// @brief Optional debug counts for validation/teardown.
        u32 vertex_count = 0;
        u32 index_count = 0;
    };

    template<typename Visitor>
    void visit_asset_refs(GpuModelAsset& gm, Visitor&& visitor)
    {
        visitor(gm.model_ref);
        for (auto& sm : gm.submeshes)
        {
            visitor(sm.material);
        }
    }

    template<typename Visitor>
    void visit_asset_refs(const GpuModelAsset& gm, Visitor&& visitor)
    {
        visitor(gm.model_ref);
        for (const auto& sm : gm.submeshes)
        {
            visitor(sm.material);
        }
    }

    // -------------------------------------------------------------------------
    // GpuTextureAsset
    // -------------------------------------------------------------------------

    /// @brief GPU binding for a texture (runtime sampler/handle data).
    /// Separate from TextureAsset (raw/authoring source).
    struct GpuTextureAsset
    {
        /// @brief Source CPU texture.
        AssetRef<TextureAsset> texture_ref;

        /// @brief Runtime-only state (do not serialize).
        GpuLoadState state = GpuLoadState::Uninitialized;

        /// @brief GL texture handle and resolved image info.
        u32 gl_id = 0;
        u32 width = 0;
        u32 height = 0;
        u32 channels = 0;
    };

    template<typename Visitor>
    void visit_asset_refs(GpuTextureAsset& gt, Visitor&& visitor)
    {
        visitor(gt.texture_ref);
    }

    template<typename Visitor>
    void visit_asset_refs(const GpuTextureAsset& gt, Visitor&& visitor)
    {
        visitor(gt.texture_ref);
    }

    // -------------------------------------------------------------------------
    // Material slots (shared CPU/GPU)
    // -------------------------------------------------------------------------

    /// @brief Common texture slots for both CPU and GPU materials.
    enum class MaterialTextureSlot : u8
    {
        Diffuse = 0,
        Normal,
        Specular,
        Opacity,
        Count
    };

    // -------------------------------------------------------------------------
    // GpuMaterialAsset
    // -------------------------------------------------------------------------

    /// @brief GPU binding for a material (runtime uniform/texture refs).
    /// Mirrors MaterialAsset fields for now, but is free to diverge later.
    struct GpuMaterialAsset
    {
        /// @brief Source CPU material.
        AssetRef<MaterialAsset> material_ref;

        /// @brief Material constants for uniform binding.
        glm::vec3 Ka = { 0.25f, 0.0f, 0.0f };
        glm::vec3 Kd = { 0.75f, 0.0f, 0.0f };
        glm::vec3 Ks = { 1.0f, 1.0f, 1.0f };
        float shininess = 10.0f;

        /// @brief GPU textures for each material slot.
        std::array<AssetRef<GpuTextureAsset>, static_cast<size_t>(MaterialTextureSlot::Count)> textures{};
    };

    template<typename Visitor>
    void visit_asset_refs(GpuMaterialAsset& m, Visitor&& visitor)
    {
        visitor(m.material_ref);
        for (auto& t : m.textures)
        {
            visitor(t);
        }
    }

    template<typename Visitor>
    void visit_asset_refs(const GpuMaterialAsset& m, Visitor&& visitor)
    {
        visitor(m.material_ref);
        for (auto& t : m.textures)
        {
            visitor(t);
        }
    }

    // -------------------------------------------------------------------------
    // TextureAsset
    // -------------------------------------------------------------------------

    /// @brief Intended color space for imported texture data.
    enum class TextureColorSpace : u8
    {
        Linear = 0,
        SRgb
    };

    /// @brief Import-time settings (authoring data).
    struct TextureImportSettings
    {
        TextureColorSpace color_space = TextureColorSpace::SRgb;
        bool generate_mips = true;
        bool is_normal_map = false;
    };

    /// @brief CPU-side texture asset (authoring/source data).
    /// GPU state lives in GpuTextureAsset.
    struct TextureAsset
    {
        /// @brief Path relative to the texture asset .json folder.
        std::string source_path;
        TextureImportSettings import_settings{}; // not used yet
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

    /// @brief CPU-side material asset (authoring/source data).
    /// GPU state lives in GpuMaterialAsset.
    struct MaterialAsset
    {
        /// @brief Raw material constants.
        glm::vec3 Ka = { 0.25f, 0.0f, 0.0f };
        glm::vec3 Kd = { 0.75f, 0.0f, 0.0f };
        glm::vec3 Ks = { 1.0f, 1.0f, 1.0f };
        float shininess = 10.0f;

        /// @brief CPU texture references for each material slot.
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

    // One sequence of keyframes
    struct AnimTrack
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
        std::vector<AnimTrack> node_animations;
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
        std::vector<glm::vec2> texcoords;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec3> tangents;
        std::vector<glm::vec3> binormals;
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
