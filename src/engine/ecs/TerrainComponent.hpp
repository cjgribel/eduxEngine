// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef TerrainComponent_hpp
#define TerrainComponent_hpp

#include "assets/types/TerrainAssets.hpp"

#include <format>
#include <glm/glm.hpp>
#include <string>

namespace eeng::ecs
{
    /**
     * @brief Scene-level hook for cooked chunked terrain.
     *
     * `TerrainComponent` belongs on a terrain root entity and points at a
     * cooked `TerrainAsset` manifest. Runtime systems use it to decide which
     * transient terrain chunk entities should exist for rendering and physics.
     *
     * Important:
     * - This references a cooked runtime terrain manifest.
     * - It does not reference the editor-only `TerrainRecipeAsset`.
     * - The chunk entities spawned from it are runtime-owned, transient view
     *   entities rather than user-authored scene content.
     */
    struct TerrainComponent
    {
        // Human-readable label for editor display and debugging.
        std::string name;

        // Master enable for the terrain root. Disabling this lets us keep the
        // manifest reference in place while making the runtime despawn any
        // chunk entities it previously created.
        bool enabled = true;

        // Runtime terrain manifest to stream from.
        AssetRef<assets::TerrainAsset> terrain_ref;

        // MVP chunk selection policy.
        // We start with one explicitly chosen chunk so there is a single, clear
        // place in code where chunk filtering happens. Later this can be
        // replaced by camera/player radius logic without changing the overall
        // terrain-system shape.
        glm::ivec2 explicit_chunk_coord{ 0, 0 };
    };

    inline std::string to_string(const TerrainComponent& terrain)
    {
        return std::format("TerrainComponent(name = {} ...)", terrain.name);
    }

    template<typename Visitor>
    void visit_asset_refs(TerrainComponent& terrain, Visitor&& visitor)
    {
        visitor(terrain.terrain_ref);
    }

    template<typename Visitor>
    void visit_entity_refs(TerrainComponent&, Visitor&&)
    {
    }
}

#endif // TerrainComponent_hpp
