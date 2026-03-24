// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "serializers/TerrainAssetSerialization.hpp"

#include "serializers/GLMSerialize.hpp"

#include <cassert>

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

#include "MetaLiterals.h"
#include "assets/types/TerrainAssets.hpp"

namespace eeng::serializers
{
    namespace
    {
        template<typename T>
        nlohmann::json serialize_asset_ref(const AssetRef<T>& ref)
        {
            return nlohmann::json{ { "guid", ref.guid.raw() } };
        }

        template<typename T>
        AssetRef<T> deserialize_asset_ref(const nlohmann::json& j)
        {
            if (j.is_object() && j.contains("guid"))
            {
                return AssetRef<T>{ Guid{ j["guid"].get<Guid::underlying_type>() } };
            }
            if (j.is_number_integer() || j.is_number_unsigned())
            {
                return AssetRef<T>{ Guid{ j.get<Guid::underlying_type>() } };
            }
            return AssetRef<T>{};
        }

        template<typename T>
        nlohmann::json serialize_asset_ref_array(const std::vector<AssetRef<T>>& refs)
        {
            nlohmann::json j = nlohmann::json::array();
            auto& arr = j.get_ref<nlohmann::json::array_t&>();
            arr.reserve(refs.size());
            for (const auto& ref : refs)
            {
                arr.emplace_back(serialize_asset_ref(ref));
            }
            return j;
        }

        template<typename T>
        void deserialize_asset_ref_array(const nlohmann::json& j, std::vector<AssetRef<T>>& refs)
        {
            refs.clear();
            if (!j.is_array())
                return;

            refs.reserve(j.size());
            for (const auto& elem : j)
            {
                refs.push_back(deserialize_asset_ref<T>(elem));
            }
        }
    }

    void serialize_TerrainRecipeAsset(nlohmann::json& j, const entt::meta_any& any)
    {
        const auto* ptr = any.try_cast<const assets::TerrainRecipeAsset>();
        assert(ptr && "serialize_TerrainRecipeAsset: bad meta_any");
        const auto& terrain = *ptr;

        j = nlohmann::json::object();
        j["source_model_ref"] = serialize_asset_ref(terrain.source_model_ref);
        j["source_submesh_index"] = terrain.source_submesh_index;
        j["world_origin"] = serialize_vec3(terrain.world_origin);
        j["sample_spacing_x"] = terrain.sample_spacing_x;
        j["sample_spacing_z"] = terrain.sample_spacing_z;
        j["horizontal_scale_x"] = terrain.horizontal_scale_x;
        j["horizontal_scale_z"] = terrain.horizontal_scale_z;
        j["height_scale"] = terrain.height_scale;
        j["chunk_size_quads_x"] = terrain.chunk_size_quads_x;
        j["chunk_size_quads_z"] = terrain.chunk_size_quads_z;
    }

    void deserialize_TerrainRecipeAsset(const nlohmann::json& j, entt::meta_any& any)
    {
        auto* ptr = any.try_cast<assets::TerrainRecipeAsset>();
        assert(ptr && "deserialize_TerrainRecipeAsset: bad meta_any");
        auto& terrain = *ptr;

        terrain = assets::TerrainRecipeAsset{};
        if (j.contains("source_model_ref"))
            terrain.source_model_ref = deserialize_asset_ref<assets::ModelDataAsset>(j["source_model_ref"]);
        terrain.source_submesh_index = j.value("source_submesh_index", -1);
        if (j.contains("world_origin"))
            terrain.world_origin = deserialize_vec3(j["world_origin"]);
        terrain.sample_spacing_x = j.value("sample_spacing_x", 1.0f);
        terrain.sample_spacing_z = j.value("sample_spacing_z", 1.0f);
        terrain.horizontal_scale_x = j.value("horizontal_scale_x", 1.0f);
        terrain.horizontal_scale_z = j.value("horizontal_scale_z", 1.0f);
        terrain.height_scale = j.value("height_scale", 1.0f);
        terrain.chunk_size_quads_x = j.value("chunk_size_quads_x", 64u);
        terrain.chunk_size_quads_z = j.value("chunk_size_quads_z", 64u);
    }

    void serialize_TerrainAsset(nlohmann::json& j, const entt::meta_any& any)
    {
        const auto* ptr = any.try_cast<const assets::TerrainAsset>();
        assert(ptr && "serialize_TerrainAsset: bad meta_any");
        const auto& terrain = *ptr;

        j = nlohmann::json::object();
        j["world_origin"] = serialize_vec3(terrain.world_origin);
        j["total_samples_x"] = terrain.total_samples_x;
        j["total_samples_z"] = terrain.total_samples_z;
        j["cell_size_x"] = terrain.cell_size_x;
        j["cell_size_z"] = terrain.cell_size_z;
        j["chunk_size_quads_x"] = terrain.chunk_size_quads_x;
        j["chunk_size_quads_z"] = terrain.chunk_size_quads_z;
        j["chunk_count_x"] = terrain.chunk_count_x;
        j["chunk_count_z"] = terrain.chunk_count_z;
        j["chunks"] = serialize_asset_ref_array(terrain.chunks);
    }

    void deserialize_TerrainAsset(const nlohmann::json& j, entt::meta_any& any)
    {
        auto* ptr = any.try_cast<assets::TerrainAsset>();
        assert(ptr && "deserialize_TerrainAsset: bad meta_any");
        auto& terrain = *ptr;

        terrain = assets::TerrainAsset{};
        if (j.contains("world_origin"))
            terrain.world_origin = deserialize_vec3(j["world_origin"]);
        terrain.total_samples_x = j.value("total_samples_x", 0u);
        terrain.total_samples_z = j.value("total_samples_z", 0u);
        terrain.cell_size_x = j.value("cell_size_x", 1.0f);
        terrain.cell_size_z = j.value("cell_size_z", 1.0f);
        terrain.chunk_size_quads_x = j.value("chunk_size_quads_x", 64u);
        terrain.chunk_size_quads_z = j.value("chunk_size_quads_z", 64u);
        terrain.chunk_count_x = j.value("chunk_count_x", 0u);
        terrain.chunk_count_z = j.value("chunk_count_z", 0u);
        if (j.contains("chunks"))
            deserialize_asset_ref_array(j["chunks"], terrain.chunks);
    }

    void serialize_TerrainChunkAsset(nlohmann::json& j, const entt::meta_any& any)
    {
        const auto* ptr = any.try_cast<const assets::TerrainChunkAsset>();
        assert(ptr && "serialize_TerrainChunkAsset: bad meta_any");
        const auto& chunk = *ptr;

        j = nlohmann::json::object();
        j["chunk_x"] = chunk.chunk_x;
        j["chunk_z"] = chunk.chunk_z;
        j["world_origin"] = serialize_vec3(chunk.world_origin);
        j["local_bounds_min"] = serialize_vec3(chunk.local_bounds_min);
        j["local_bounds_max"] = serialize_vec3(chunk.local_bounds_max);
        j["samples_x"] = chunk.samples_x;
        j["samples_z"] = chunk.samples_z;
        j["cell_size_x"] = chunk.cell_size_x;
        j["cell_size_z"] = chunk.cell_size_z;
        j["min_height"] = chunk.min_height;
        j["max_height"] = chunk.max_height;
        j["heights"] = chunk.heights;
        j["render_model_ref"] = serialize_asset_ref(chunk.render_model_ref);
    }

    void deserialize_TerrainChunkAsset(const nlohmann::json& j, entt::meta_any& any)
    {
        auto* ptr = any.try_cast<assets::TerrainChunkAsset>();
        assert(ptr && "deserialize_TerrainChunkAsset: bad meta_any");
        auto& chunk = *ptr;

        chunk = assets::TerrainChunkAsset{};
        chunk.chunk_x = j.value("chunk_x", 0);
        chunk.chunk_z = j.value("chunk_z", 0);
        if (j.contains("world_origin"))
            chunk.world_origin = deserialize_vec3(j["world_origin"]);
        if (j.contains("local_bounds_min"))
            chunk.local_bounds_min = deserialize_vec3(j["local_bounds_min"]);
        if (j.contains("local_bounds_max"))
            chunk.local_bounds_max = deserialize_vec3(j["local_bounds_max"]);
        chunk.samples_x = j.value("samples_x", 0u);
        chunk.samples_z = j.value("samples_z", 0u);
        chunk.cell_size_x = j.value("cell_size_x", 1.0f);
        chunk.cell_size_z = j.value("cell_size_z", 1.0f);
        chunk.min_height = j.value("min_height", 0.0f);
        chunk.max_height = j.value("max_height", 0.0f);
        if (j.contains("heights") && j["heights"].is_array())
            chunk.heights = j["heights"].get<std::vector<float>>();
        if (j.contains("render_model_ref"))
            chunk.render_model_ref = deserialize_asset_ref<assets::GpuModelAsset>(j["render_model_ref"]);
    }

    void register_terrainasset_serialization()
    {
        entt::meta_factory<assets::TerrainRecipeAsset>{}
            .func<&serialize_TerrainRecipeAsset>(eeng::literals::serialize_hs)
            .func<&deserialize_TerrainRecipeAsset>(eeng::literals::deserialize_hs);

        entt::meta_factory<assets::TerrainAsset>{}
            .func<&serialize_TerrainAsset>(eeng::literals::serialize_hs)
            .func<&deserialize_TerrainAsset>(eeng::literals::deserialize_hs);

        entt::meta_factory<assets::TerrainChunkAsset>{}
            .func<&serialize_TerrainChunkAsset>(eeng::literals::serialize_hs)
            .func<&deserialize_TerrainChunkAsset>(eeng::literals::deserialize_hs);
    }
}
