// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once
#ifndef TerrainComponent_hpp
#define TerrainComponent_hpp

#include "assets/types/TerrainAssets.hpp"

#include <format>
#include <string>

namespace eeng::ecs
{
    // TerrainComponent
    // - Lightweight ECS hook for terrain streaming/render/physics systems.
    // - Points at the cooked runtime terrain manifest, not the source terrain
    //   recipe or artist-authored full mesh.
    struct TerrainComponent
    {
        // Human-readable label for editor display and debugging.
        std::string name;

        // Runtime terrain manifest to stream from.
        AssetRef<assets::TerrainAsset> terrain_ref;
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
