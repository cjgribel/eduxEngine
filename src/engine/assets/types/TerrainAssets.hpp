// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "AssetRef.hpp"
#include "Guid.h"

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace eeng::assets
{
    struct ModelDataAsset;
    struct GpuModelAsset;
    struct TerrainChunkAsset;
    using BatchId = Guid;

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

        // Optional cook-time scaling applied to the authored source mesh before
        // rasterization. This is the safe way to resize terrain without
        // relying on runtime transform scaling, which can diverge between
        // render and physics if parent scales are introduced later.
        float horizontal_scale_x = 1.0f;
        float horizontal_scale_z = 1.0f;
        float height_scale = 1.0f;

        // Number of chunks the cooker should produce across the sampled
        // terrain in X/Z. The cooker partitions the sampled grid across this
        // resolution and keeps shared edge samples aligned between neighbors.
        std::uint32_t chunk_count_x = 1;
        std::uint32_t chunk_count_z = 1;
    };

    // TerrainChunkEntry
    // - One entry in the terrain collection manifest.
    // - Carries only collection/index information so tooling or future
    //   residency systems can reason about the terrain without loading the
    //   terrain chunk batch itself.
    struct TerrainChunkEntry
    {
        std::int32_t chunk_x = 0;
        std::int32_t chunk_z = 0;
        AssetRef<TerrainChunkAsset> terrain_chunk_ref;
        BatchId batch_id{};
        std::string batch_name{};
        glm::vec3 world_bounds_min{ 0.0f };
        glm::vec3 world_bounds_max{ 0.0f };
    };

    // TerrainAsset
    // - Runtime-facing terrain manifest.
    // - Represents the terrain as a collection of cooked chunks.
    // - Useful for editor tooling, metadata, and future residency logic.
    // - It is not the thing that actually renders or collides at runtime; the
    //   generated chunk batches are the true runtime end products.
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

        std::uint32_t chunk_count_x = 0;
        std::uint32_t chunk_count_z = 0;

        // Chunk collection table. Each entry maps terrain coordinates to the
        // cooked chunk payload and the generated batch that owns the runtime
        // terrain chunk entity.
        std::vector<TerrainChunkEntry> chunks;
    };

    // TerrainChunkAsset
    // - Runtime streaming unit for one terrain chunk.
    // - Contains only the cooked heightfield data Bullet needs plus chunk
    //   bounds/origin metadata. Render assets live separately on the
    //   generated chunk batch entity.
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
    void visit_asset_refs(TerrainAsset&, Visitor&&)
    {
        // Intentionally empty.
        //
        // Terrain manifests are runtime lookup tables, not eager dependency
        // roots. If we recursively visited all chunk refs here then binding the
        // manifest would immediately pull in every cooked chunk, which defeats
        // the whole point of chunked terrain.
        //
        // The terrain runtime system is expected to pick the chunk coords it
        // wants and request those TerrainChunkAssets explicitly.
    }

    template<typename Visitor>
    void visit_asset_refs(const TerrainAsset&, Visitor&&)
    {
        // See non-const overload above.
    }

    template<typename Visitor>
    void visit_asset_refs(TerrainChunkAsset& chunk, Visitor&& visitor)
    {
        (void)chunk;
        (void)visitor;
    }

    template<typename Visitor>
    void visit_asset_refs(const TerrainChunkAsset& chunk, Visitor&& visitor)
    {
        (void)chunk;
        (void)visitor;
    }
}
