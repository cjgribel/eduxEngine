// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <nlohmann/json_fwd.hpp>

namespace entt
{
    class meta_any;
}

namespace eeng::serializers
{
    void serialize_TerrainRecipeAsset(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_TerrainRecipeAsset(const nlohmann::json& j, entt::meta_any& any);

    void serialize_TerrainAsset(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_TerrainAsset(const nlohmann::json& j, entt::meta_any& any);

    void serialize_TerrainChunkAsset(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_TerrainChunkAsset(const nlohmann::json& j, entt::meta_any& any);

    void register_terrainasset_serialization();
}
