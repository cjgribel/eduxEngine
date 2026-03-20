// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "AssetRef.hpp"

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace eeng::assets
{
    struct ModelDataAsset;
    struct GpuModelAsset;
    struct TerrainChunkAsset;

    // TerrainRecipeAsset
    // - Editor/tool-side source description for terrain cooking.
    // - Points at the artist-authored terrain mesh and records how we want to
    //   sample/chunk it into runtime terrain assets.
    // - This is intentionally separate from the runtime terrain manifest so
    //   the engine does not need to keep the full source mesh around.
    struct TerrainRecipeAsset
    {
        // Full source terrain mesh authored by the artist.
        AssetRef<ModelDataAsset> source_model_ref;

        // Source submesh to interpret as terrain. `-1` means "use submesh 0".
        int source_submesh_index = -1;

        // World-space origin used when sampling the source terrain.
        // This lets the cooker and runtime agree on where chunk (0, 0) starts.
        glm::vec3 world_origin{ 0.0f };

        // Spacing between sampled terrain points in X/Z.
        // Smaller spacing captures more shape detail but produces more samples.
        float sample_spacing_x = 1.0f;
        float sample_spacing_z = 1.0f;

        // Number of terrain cells per cooked chunk in X/Z.
        // The cooker should emit one extra border sample so neighboring chunks
        // share their edge heights cleanly.
        std::uint32_t chunk_size_quads_x = 64;
        std::uint32_t chunk_size_quads_z = 64;
    };

    // TerrainAsset
    // - Runtime-facing terrain manifest.
    // - References only cooked chunk assets; it should not depend on the full
    //   source terrain mesh at runtime.
    struct TerrainAsset
    {
        // Same terrain-space origin used during the cook.
        glm::vec3 world_origin{ 0.0f };

        // Total sampled terrain resolution before chunking.
        // These are sample counts, not cell counts.
        std::uint32_t total_samples_x = 0;
        std::uint32_t total_samples_z = 0;

        // Distance between neighboring samples in terrain space.
        float cell_size_x = 1.0f;
        float cell_size_z = 1.0f;

        // Chunk layout used by the cook.
        std::uint32_t chunk_size_quads_x = 64;
        std::uint32_t chunk_size_quads_z = 64;
        std::uint32_t chunk_count_x = 0;
        std::uint32_t chunk_count_z = 0;

        // Runtime chunk table. Each entry should be a self-contained streaming
        // unit with render data + Bullet heightfield data.
        std::vector<AssetRef<TerrainChunkAsset>> chunks;
    };

    // TerrainChunkAsset
    // - Runtime streaming unit for one terrain chunk.
    // - Contains the cooked heightfield data Bullet needs plus a reference to a
    //   chunk-local render model that can be uploaded to GL independently.
    struct TerrainChunkAsset
    {
        // Chunk coordinates in the terrain grid.
        std::int32_t chunk_x = 0;
        std::int32_t chunk_z = 0;

        // World-space origin of this chunk's local sample grid.
        glm::vec3 world_origin{ 0.0f };

        // Chunk bounds in local terrain space. Useful for streaming, culling,
        // and broad "is this chunk relevant?" queries without reading the
        // render mesh or rebuilding the Bullet shape.
        glm::vec3 local_bounds_min{ 0.0f };
        glm::vec3 local_bounds_max{ 0.0f };

        // Heightfield sample resolution for this chunk.
        // These are sample counts, so a chunk with N cells stores N + 1
        // samples along each shared border.
        std::uint32_t samples_x = 0;
        std::uint32_t samples_z = 0;

        // Distance between neighboring samples.
        float cell_size_x = 1.0f;
        float cell_size_z = 1.0f;

        // Precomputed height range for this chunk. Bullet needs this when
        // building a heightfield shape, and it is also useful for bounds/debug.
        float min_height = 0.0f;
        float max_height = 0.0f;

        // Row-major height samples for the chunk.
        // The cooker owns how these are produced; runtime just consumes them to
        // build `btHeightfieldTerrainShape`.
        std::vector<float> heights;

        // Render mesh for just this chunk. This is the key GL residency split:
        // loading one chunk should upload only this chunk's model, not the full
        // artist-authored terrain source mesh.
        AssetRef<GpuModelAsset> render_model_ref;
    };

    template<typename Visitor>
    void visit_asset_refs(TerrainRecipeAsset&, Visitor&&)
    {
        // Intentionally empty.
        //
        // Terrain recipes are editor/tool-side descriptors. We do not want the
        // generic asset-recursion path to pull in the full source terrain mesh
        // just because the recipe itself is loaded or inspected.
        //
        // Tools that actually perform terrain cooking should resolve
        // `source_model_ref` explicitly when they need it.
    }

    template<typename Visitor>
    void visit_asset_refs(const TerrainRecipeAsset&, Visitor&&)
    {
        // See non-const overload above.
    }

    template<typename Visitor>
    void visit_asset_refs(TerrainAsset& terrain, Visitor&& visitor)
    {
        for (auto& chunk : terrain.chunks)
        {
            visitor(chunk);
        }
    }

    template<typename Visitor>
    void visit_asset_refs(const TerrainAsset& terrain, Visitor&& visitor)
    {
        for (const auto& chunk : terrain.chunks)
        {
            visitor(chunk);
        }
    }

    template<typename Visitor>
    void visit_asset_refs(TerrainChunkAsset& chunk, Visitor&& visitor)
    {
        visitor(chunk.render_model_ref);
    }

    template<typename Visitor>
    void visit_asset_refs(const TerrainChunkAsset& chunk, Visitor&& visitor)
    {
        visitor(chunk.render_model_ref);
    }
}
