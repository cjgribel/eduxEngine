// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef TerrainRootComponent_hpp
#define TerrainRootComponent_hpp

#include "assets/types/TerrainAssets.hpp"

#include <format>
#include <string>

namespace eeng::ecs
{
    /**
     * @brief Optional scene-level hook for a cooked terrain collection.
     *
     * `TerrainRootComponent` is intentionally minimal. It exists so a scene
     * can carry a terrain-as-a-collection reference without implying that a
     * terrain runtime system must actively spawn or manage chunk entities.
     *
     * In the batch-backed terrain flow, the true runtime end product is the
     * generated terrain chunk batch. This component is only a root/metadata
     * anchor for the terrain collection itself.
     */
    struct TerrainRootComponent
    {
        // Runtime terrain collection/manifest referenced by this root.
        AssetRef<assets::TerrainAsset> terrain_ref;
    };

    inline std::string to_string(const TerrainRootComponent& terrain)
    {
        return std::format("TerrainRootComponent(terrain = {})", terrain.terrain_ref.guid.to_string());
    }

    template<typename Visitor>
    void visit_asset_refs(TerrainRootComponent& terrain, Visitor&& visitor)
    {
        visitor(terrain.terrain_ref);
    }

    template<typename Visitor>
    void visit_entity_refs(TerrainRootComponent&, Visitor&&)
    {
    }
}

#endif // TerrainRootComponent_hpp
