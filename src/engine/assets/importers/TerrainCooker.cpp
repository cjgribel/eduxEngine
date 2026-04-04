// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#include "assets/importers/TerrainCooker.hpp"

#include "AssetIndexData.hpp"
#include "AssetMetaData.hpp"
#include "BatchRegistry.hpp"
#include "LogMacros.h"
#include "ResourceManager.hpp"
#include "assets/types/ModelAssets.hpp"
#include "assets/types/TerrainAssets.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/ModelComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"
#include "meta/MetaSerialize.hpp"
#include "meta/MetaAux.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

namespace eeng::assets
{
    namespace
    {
        // Source-mesh X/Z extents after cook-time recipe scaling is applied.
        struct TerrainBounds
        {
            float min_x = 0.0f;
            float max_x = 0.0f;
            float min_z = 0.0f;
            float max_z = 0.0f;
        };

        // Regular terrain sample grid used as the common source for both
        // cooked collision data and cooked render chunk meshes.
        struct HeightGrid
        {
            float origin_x = 0.0f;
            float origin_z = 0.0f;
            std::uint32_t samples_x = 0;
            std::uint32_t samples_z = 0;
            std::vector<float> heights{};
        };

        // All generated runtime assets for one cooked terrain chunk.
        struct TerrainChunkCookOutput
        {
            TerrainChunkAsset chunk_asset{};
            ModelDataAsset render_model{};
            GpuModelAsset render_gpu_model{};
            Guid chunk_guid{};
            Guid render_model_guid{};
            Guid render_gpu_guid{};
            BatchId batch_id{};
            std::string batch_name{};
            std::vector<Guid> asset_closure{};
        };

        // One axis partition of the sampled terrain grid into chunk-sized
        // ranges. Counts are expressed in terrain cells/quads; the extra
        // shared border sample is recovered when building the chunk payload.
        struct ChunkAxisRange
        {
            std::uint32_t start_quad = 0;
            std::uint32_t quad_count = 0;
        };

        // Keep generated cook output folder/file names deterministic and
        // filesystem-friendly without leaking arbitrary source asset names to disk.
        std::string sanitize_name(std::string name)
        {
            for (char& ch : name)
            {
                const unsigned char uch = static_cast<unsigned char>(ch);
                if ((uch >= 'a' && uch <= 'z')
                    || (uch >= 'A' && uch <= 'Z')
                    || (uch >= '0' && uch <= '9')
                    || ch == '_'
                    || ch == '-')
                {
                    continue;
                }
                ch = '_';
            }

            while (!name.empty() && (name.back() == '_' || name.back() == ' '))
                name.pop_back();
            if (name.empty())
                name = "Terrain";
            return name;
        }

        std::optional<Guid> guid_for_asset_path(
            const AssetIndexDataPtr& index_data,
            const std::filesystem::path& asset_path)
        {
            if (!index_data)
                return std::nullopt;

            const auto target = asset_path.lexically_normal();
            for (const auto& entry : index_data->entries)
            {
                if (entry.absolute_path.lexically_normal() == target)
                    return entry.meta.guid;
            }
            return std::nullopt;
        }

        Guid guid_for_asset_path_or_new(
            const AssetIndexDataPtr& index_data,
            const std::filesystem::path& asset_path)
        {
            if (auto guid = guid_for_asset_path(index_data, asset_path))
                return *guid;
            return Guid::generate();
        }

        Guid deterministic_guid(
            const Guid& seed_guid,
            std::string_view artifact_kind,
            std::int32_t x = 0,
            std::int32_t z = 0)
        {
            // Terrain chunk batches should retain stable identities across
            // re-cooks. A deterministic hash is enough for generated content
            // as long as the inputs stay stable.
            std::uint64_t hash = 1469598103934665603ull;
            const auto mix_byte = [&](std::uint8_t byte)
            {
                hash ^= static_cast<std::uint64_t>(byte);
                hash *= 1099511628211ull;
            };
            const auto mix_u64 = [&](std::uint64_t value)
            {
                for (int i = 0; i < 8; ++i)
                    mix_byte(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFu));
            };

            mix_u64(seed_guid.raw());
            for (char ch : artifact_kind)
                mix_byte(static_cast<std::uint8_t>(ch));
            mix_u64(static_cast<std::uint64_t>(static_cast<std::int64_t>(x)));
            mix_u64(static_cast<std::uint64_t>(static_cast<std::int64_t>(z)));

            if (hash == 0ull)
                hash = 1ull;
            return Guid{ hash };
        }

        std::optional<Guid> first_child_guid_by_type(
            const AssetIndexDataPtr& index_data,
            const Guid& parent_guid,
            std::string_view type_id)
        {
            if (!index_data || !parent_guid.valid())
                return std::nullopt;

            const auto parent_it = index_data->by_parent.find(parent_guid);
            if (parent_it == index_data->by_parent.end())
                return std::nullopt;

            for (const auto* entry : parent_it->second)
            {
                if (!entry)
                    continue;
                if (entry->meta.type_id == type_id)
                    return entry->meta.guid;
            }

            return std::nullopt;
        }

        AssetMetaData make_meta(
            const Guid& guid,
            const Guid& parent_guid,
            const std::string& name,
            const std::string& type_id)
        {
            AssetMetaData meta{};
            meta.guid = guid;
            meta.guid_parent = parent_guid;
            meta.name = name;
            meta.type_id = type_id;
            return meta;
        }

        std::vector<ChunkAxisRange> build_chunk_axis_ranges(
            const std::uint32_t total_quads,
            const std::uint32_t requested_chunk_count)
        {
            std::vector<ChunkAxisRange> ranges{};
            const std::uint32_t chunk_count = std::max(1u, requested_chunk_count);
            ranges.reserve(chunk_count);

            const std::uint32_t base = total_quads / chunk_count;
            const std::uint32_t remainder = total_quads % chunk_count;

            std::uint32_t cursor = 0;
            for (std::uint32_t chunk = 0; chunk < chunk_count; ++chunk)
            {
                const std::uint32_t quad_count = base + (chunk < remainder ? 1u : 0u);
                ranges.push_back(ChunkAxisRange{
                    .start_quad = cursor,
                    .quad_count = quad_count
                    });
                cursor += quad_count;
            }
            return ranges;
        }

        std::vector<Guid> read_existing_chunk_batch_ids(const std::filesystem::path& terrain_asset_path)
        {
            std::vector<Guid> batch_ids{};
            std::ifstream file(terrain_asset_path);
            if (!file.is_open())
                return batch_ids;

            nlohmann::json j;
            try
            {
                file >> j;
            }
            catch (...)
            {
                return batch_ids;
            }

            const auto it = j.find("chunks");
            if (it == j.end() || !it->is_array())
                return batch_ids;

            batch_ids.reserve(it->size());
            for (const auto& elem : *it)
            {
                if (!elem.is_object() || !elem.contains("batch_id"))
                    continue;
                const auto& batch_json = elem["batch_id"];
                if (batch_json.is_object() && batch_json.contains("guid"))
                {
                    const auto raw = batch_json["guid"].get<Guid::underlying_type>();
                    if (raw != 0)
                        batch_ids.emplace_back(raw);
                }
            }

            return batch_ids;
        }

        std::size_t sample_index(
            const std::uint32_t samples_x,
            const std::uint32_t x,
            const std::uint32_t z)
        {
            return static_cast<std::size_t>(z) * static_cast<std::size_t>(samples_x)
                + static_cast<std::size_t>(x);
        }

        glm::vec3 scale_source_position(
            const glm::vec3& position,
            const TerrainRecipeAsset& recipe)
        {
            // Recipe scaling is applied at cook time so render and collision
            // stay aligned in the generated runtime data.
            return glm::vec3(
                position.x * recipe.horizontal_scale_x,
                position.y * recipe.height_scale,
                position.z * recipe.horizontal_scale_z);
        }

        TerrainBounds compute_submesh_bounds(
            const ModelDataAsset& model,
            const SubMesh& submesh,
            const TerrainRecipeAsset& recipe)
        {
            TerrainBounds bounds{};
            bounds.min_x = std::numeric_limits<float>::max();
            bounds.max_x = std::numeric_limits<float>::lowest();
            bounds.min_z = std::numeric_limits<float>::max();
            bounds.max_z = std::numeric_limits<float>::lowest();

            for (std::uint32_t i = 0; i < submesh.nbr_indices; ++i)
            {
                const auto index = model.indices[submesh.base_index + i];
                const glm::vec3 pos = scale_source_position(model.positions[index], recipe);
                bounds.min_x = std::min(bounds.min_x, pos.x);
                bounds.max_x = std::max(bounds.max_x, pos.x);
                bounds.min_z = std::min(bounds.min_z, pos.z);
                bounds.max_z = std::max(bounds.max_z, pos.z);
            }

            return bounds;
        }

        bool barycentric_contains_point(
            const glm::vec2& p,
            const glm::vec2& a,
            const glm::vec2& b,
            const glm::vec2& c,
            float& out_u,
            float& out_v,
            float& out_w)
        {
            const glm::vec2 v0 = b - a;
            const glm::vec2 v1 = c - a;
            const glm::vec2 v2 = p - a;

            const float denom = v0.x * v1.y - v1.x * v0.y;
            if (std::abs(denom) <= 1e-8f)
                return false;

            out_v = (v2.x * v1.y - v1.x * v2.y) / denom;
            out_w = (v0.x * v2.y - v2.x * v0.y) / denom;
            out_u = 1.0f - out_v - out_w;

            constexpr float eps = 1e-4f;
            return out_u >= -eps && out_v >= -eps && out_w >= -eps;
        }

        HeightGrid rasterize_height_grid(
            const ModelDataAsset& model,
            const SubMesh& submesh,
            const TerrainRecipeAsset& recipe)
        {
            // The source terrain mesh is treated as a heightfield-like surface:
            // one sampled height per X/Z location after cook-time scaling.
            const TerrainBounds bounds = compute_submesh_bounds(model, submesh, recipe);

            const float extent_x = std::max(0.0f, bounds.max_x - bounds.min_x);
            const float extent_z = std::max(0.0f, bounds.max_z - bounds.min_z);
            const auto total_quads_x = static_cast<std::uint32_t>(
                std::max(1.0f, static_cast<float>(std::ceil(
                    extent_x / std::max(0.001f, recipe.sample_spacing_x)))));
            const auto total_quads_z = static_cast<std::uint32_t>(
                std::max(1.0f, static_cast<float>(std::ceil(
                    extent_z / std::max(0.001f, recipe.sample_spacing_z)))));

            HeightGrid grid{};
            grid.origin_x = bounds.min_x;
            grid.origin_z = bounds.min_z;
            grid.samples_x = total_quads_x + 1;
            grid.samples_z = total_quads_z + 1;
            grid.heights.assign(
                static_cast<std::size_t>(grid.samples_x) * static_cast<std::size_t>(grid.samples_z),
                0.0f);

            std::vector<bool> filled(grid.heights.size(), false);

            // Offline cook step:
            // project each triangle into X/Z and stamp heights into the sample
            // cells it overlaps. We keep the highest sample when overlaps occur,
            // which matches the "single visible ground surface" assumption.
            for (std::uint32_t tri = 0; tri + 2 < submesh.nbr_indices; tri += 3)
            {
                const auto ia = model.indices[submesh.base_index + tri + 0];
                const auto ib = model.indices[submesh.base_index + tri + 1];
                const auto ic = model.indices[submesh.base_index + tri + 2];

                const glm::vec3 a = scale_source_position(model.positions[ia], recipe);
                const glm::vec3 b = scale_source_position(model.positions[ib], recipe);
                const glm::vec3 c = scale_source_position(model.positions[ic], recipe);

                const glm::vec2 axz{ a.x, a.z };
                const glm::vec2 bxz{ b.x, b.z };
                const glm::vec2 cxz{ c.x, c.z };

                const float tri_min_x = std::min({ a.x, b.x, c.x });
                const float tri_max_x = std::max({ a.x, b.x, c.x });
                const float tri_min_z = std::min({ a.z, b.z, c.z });
                const float tri_max_z = std::max({ a.z, b.z, c.z });

                const int ix0 = std::max(0, static_cast<int>(std::floor((tri_min_x - grid.origin_x) / recipe.sample_spacing_x)));
                const int ix1 = std::min(
                    static_cast<int>(grid.samples_x) - 1,
                    static_cast<int>(std::ceil((tri_max_x - grid.origin_x) / recipe.sample_spacing_x)));
                const int iz0 = std::max(0, static_cast<int>(std::floor((tri_min_z - grid.origin_z) / recipe.sample_spacing_z)));
                const int iz1 = std::min(
                    static_cast<int>(grid.samples_z) - 1,
                    static_cast<int>(std::ceil((tri_max_z - grid.origin_z) / recipe.sample_spacing_z)));

                for (int iz = iz0; iz <= iz1; ++iz)
                {
                    for (int ix = ix0; ix <= ix1; ++ix)
                    {
                        const glm::vec2 p{
                            grid.origin_x + static_cast<float>(ix) * recipe.sample_spacing_x,
                            grid.origin_z + static_cast<float>(iz) * recipe.sample_spacing_z
                        };

                        float u = 0.0f;
                        float v = 0.0f;
                        float w = 0.0f;
                        if (!barycentric_contains_point(p, axz, bxz, cxz, u, v, w))
                            continue;

                        const float height = u * a.y + v * b.y + w * c.y;
                        const std::size_t idx = sample_index(
                            grid.samples_x,
                            static_cast<std::uint32_t>(ix),
                            static_cast<std::uint32_t>(iz));

                        if (!filled[idx] || height > grid.heights[idx])
                        {
                            grid.heights[idx] = height;
                            filled[idx] = true;
                        }
                    }
                }
            }

            // Fill gaps left by triangle rasterization. Clean terrain should not
            // produce many of these, but a simple nearest-neighbor expansion
            // makes the cooked result robust around triangle boundaries.
            const auto search_radius_limit = static_cast<int>(std::max(grid.samples_x, grid.samples_z));
            for (std::uint32_t z = 0; z < grid.samples_z; ++z)
            {
                for (std::uint32_t x = 0; x < grid.samples_x; ++x)
                {
                    const std::size_t idx = sample_index(grid.samples_x, x, z);
                    if (filled[idx])
                        continue;

                    bool found = false;
                    for (int radius = 1; radius <= search_radius_limit && !found; ++radius)
                    {
                        for (int dz = -radius; dz <= radius && !found; ++dz)
                        {
                            for (int dx = -radius; dx <= radius; ++dx)
                            {
                                const int sx = static_cast<int>(x) + dx;
                                const int sz = static_cast<int>(z) + dz;
                                if (sx < 0
                                    || sz < 0
                                    || sx >= static_cast<int>(grid.samples_x)
                                    || sz >= static_cast<int>(grid.samples_z))
                                {
                                    continue;
                                }

                                const std::size_t neighbor_idx = sample_index(
                                    grid.samples_x,
                                    static_cast<std::uint32_t>(sx),
                                    static_cast<std::uint32_t>(sz));
                                if (!filled[neighbor_idx])
                                    continue;

                                grid.heights[idx] = grid.heights[neighbor_idx];
                                filled[idx] = true;
                                found = true;
                                break;
                            }
                        }
                    }
                }
            }

            return grid;
        }

        glm::vec3 compute_heightfield_normal(
            const std::vector<float>& heights,
            std::uint32_t samples_x,
            std::uint32_t samples_z,
            std::uint32_t x,
            std::uint32_t z,
            float cell_size_x,
            float cell_size_z)
        {
            const auto clamped_x0 = (x == 0) ? 0u : x - 1u;
            const auto clamped_x1 = std::min(samples_x - 1u, x + 1u);
            const auto clamped_z0 = (z == 0) ? 0u : z - 1u;
            const auto clamped_z1 = std::min(samples_z - 1u, z + 1u);

            const float h_l = heights[sample_index(samples_x, clamped_x0, z)];
            const float h_r = heights[sample_index(samples_x, clamped_x1, z)];
            const float h_d = heights[sample_index(samples_x, x, clamped_z0)];
            const float h_u = heights[sample_index(samples_x, x, clamped_z1)];

            const float ddx = (h_r - h_l) / std::max(cell_size_x * 2.0f, 0.001f);
            const float ddz = (h_u - h_d) / std::max(cell_size_z * 2.0f, 0.001f);
            return glm::normalize(glm::vec3(-ddx, 1.0f, -ddz));
        }

        nlohmann::json build_chunk_batch_entity_json(
            const Guid& entity_guid,
            std::string entity_name,
            const TerrainChunkAsset& chunk_asset,
            const Guid& render_gpu_guid,
            const Guid& terrain_chunk_guid)
        {
            auto registry = std::make_shared<entt::registry>();
            const auto entity = ecs::Entity{ registry->create() };

            ecs::HeaderComponent header{};
            header.name = std::move(entity_name);
            header.guid = entity_guid;
            header.parent_entity = ecs::EntityRef{};
            registry->emplace<ecs::HeaderComponent>(entity, std::move(header));

            ecs::TransformComponent transform{};
            transform.set_position(chunk_asset.world_origin);
            transform.set_rotation(glm::quat{ 1.0f, 0.0f, 0.0f, 0.0f });
            transform.set_scale(glm::vec3{ 1.0f });
            registry->emplace<ecs::TransformComponent>(entity, std::move(transform));

            ecs::ModelComponent model{};
            model.name = std::format("TerrainChunkModel({}, {})", chunk_asset.chunk_x, chunk_asset.chunk_z);
            model.model_ref = AssetRef<GpuModelAsset>{ render_gpu_guid };
            registry->emplace<ecs::ModelComponent>(entity, std::move(model));

            ecs::RigidBodyComponent rigid_body{};
            rigid_body.motion = ecs::PhysicsMotionType::Static;
            registry->emplace<ecs::RigidBodyComponent>(entity, std::move(rigid_body));

            ecs::ColliderDesc collider{};
            collider.id = 1;
            collider.type = ecs::ColliderType::Heightfield;
            collider.terrain_chunk_ref = AssetRef<TerrainChunkAsset>{ terrain_chunk_guid };

            ecs::ColliderComponent collider_component{};
            collider_component.colliders.push_back(std::move(collider));
            registry->emplace<ecs::ColliderComponent>(entity, std::move(collider_component));

            return meta::serialize_entity(
                ecs::EntityRef{ entity_guid, entity },
                registry,
                meta::SerializationPurpose::file);
        }

        TerrainChunkCookOutput build_chunk_output(
            const AssetIndexDataPtr& index_data,
            const HeightGrid& grid,
            const TerrainRecipeAsset& recipe,
            const SubMesh& source_submesh,
            const ChunkAxisRange& range_x,
            const ChunkAxisRange& range_z,
            std::uint32_t chunk_x,
            std::uint32_t chunk_z)
        {
            // Chunks are sliced from the common sampled height grid so that the
            // generated render mesh and generated heightfield collider are
            // derived from the exact same source data.
            const std::uint32_t start_quad_x = range_x.start_quad;
            const std::uint32_t start_quad_z = range_z.start_quad;

            const std::uint32_t local_quads_x = range_x.quad_count;
            const std::uint32_t local_quads_z = range_z.quad_count;
            const std::uint32_t local_samples_x = local_quads_x + 1;
            const std::uint32_t local_samples_z = local_quads_z + 1;

            TerrainChunkCookOutput out{};
            auto& chunk = out.chunk_asset;
            chunk.chunk_x = static_cast<std::int32_t>(chunk_x);
            chunk.chunk_z = static_cast<std::int32_t>(chunk_z);
            chunk.samples_x = local_samples_x;
            chunk.samples_z = local_samples_z;
            chunk.cell_size_x = recipe.sample_spacing_x;
            chunk.cell_size_z = recipe.sample_spacing_z;
            chunk.world_origin = recipe.world_origin + glm::vec3(
                grid.origin_x + static_cast<float>(start_quad_x) * recipe.sample_spacing_x,
                0.0f,
                grid.origin_z + static_cast<float>(start_quad_z) * recipe.sample_spacing_z);
            chunk.heights.resize(
                static_cast<std::size_t>(local_samples_x) * static_cast<std::size_t>(local_samples_z));

            float min_height = std::numeric_limits<float>::max();
            float max_height = std::numeric_limits<float>::lowest();

            for (std::uint32_t z = 0; z < local_samples_z; ++z)
            {
                for (std::uint32_t x = 0; x < local_samples_x; ++x)
                {
                    const float height = grid.heights[sample_index(
                        grid.samples_x,
                        start_quad_x + x,
                        start_quad_z + z)];
                    chunk.heights[sample_index(local_samples_x, x, z)] = height;
                    min_height = std::min(min_height, height);
                    max_height = std::max(max_height, height);
                }
            }

            chunk.min_height = min_height;
            chunk.max_height = max_height;
            chunk.local_bounds_min = glm::vec3(0.0f, min_height, 0.0f);
            chunk.local_bounds_max = glm::vec3(
                static_cast<float>(local_quads_x) * recipe.sample_spacing_x,
                max_height,
                static_cast<float>(local_quads_z) * recipe.sample_spacing_z);

            auto& model = out.render_model;
            model.positions.reserve(chunk.heights.size());
            model.texcoords.reserve(chunk.heights.size());
            model.normals.reserve(chunk.heights.size());
            model.tangents.reserve(chunk.heights.size());
            model.binormals.reserve(chunk.heights.size());

            for (std::uint32_t z = 0; z < local_samples_z; ++z)
            {
                for (std::uint32_t x = 0; x < local_samples_x; ++x)
                {
                    const float px = static_cast<float>(x) * recipe.sample_spacing_x;
                    const float pz = static_cast<float>(z) * recipe.sample_spacing_z;
                    const float py = chunk.heights[sample_index(local_samples_x, x, z)];

                    const glm::vec3 normal = compute_heightfield_normal(
                        chunk.heights,
                        local_samples_x,
                        local_samples_z,
                        x,
                        z,
                        recipe.sample_spacing_x,
                        recipe.sample_spacing_z);
                    const glm::vec3 tangent = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
                    const glm::vec3 binormal = glm::normalize(glm::cross(normal, tangent));

                    model.positions.emplace_back(px, py, pz);
                    model.texcoords.emplace_back(
                        local_quads_x > 0 ? static_cast<float>(x) / static_cast<float>(local_quads_x) : 0.0f,
                        local_quads_z > 0 ? static_cast<float>(z) / static_cast<float>(local_quads_z) : 0.0f);
                    model.normals.push_back(normal);
                    model.tangents.push_back(tangent);
                    model.binormals.push_back(binormal);
                }
            }

            for (std::uint32_t z = 0; z < local_quads_z; ++z)
            {
                for (std::uint32_t x = 0; x < local_quads_x; ++x)
                {
                    const std::uint32_t i0 = z * local_samples_x + x;
                    const std::uint32_t i1 = i0 + 1;
                    const std::uint32_t i2 = i0 + local_samples_x;
                    const std::uint32_t i3 = i2 + 1;

                    model.indices.push_back(i0);
                    model.indices.push_back(i2);
                    model.indices.push_back(i1);

                    model.indices.push_back(i1);
                    model.indices.push_back(i2);
                    model.indices.push_back(i3);
                }
            }

            SubMesh submesh{};
            submesh.base_index = 0;
            submesh.nbr_indices = static_cast<std::uint32_t>(model.indices.size());
            submesh.base_vertex = 0;
            submesh.nbr_vertices = static_cast<std::uint32_t>(model.positions.size());
            // Preserve the source terrain material on the generated chunk mesh
            // so the cooked terrain renders like the authored source by default.
            submesh.material = source_submesh.material;
            model.submeshes.push_back(submesh);

            auto& gpu_model = out.render_gpu_model;
            GpuSubMesh gpu_submesh{};
            gpu_submesh.index_offset = 0;
            gpu_submesh.index_count = static_cast<std::uint32_t>(model.indices.size());
            gpu_submesh.base_vertex = 0;

            // The imported material pipeline stores GpuMaterialAsset as a child
            // of the source MaterialAsset. Terrain cooking reuses that pairing
            // instead of generating duplicate material assets per chunk.
            if (source_submesh.material.guid.valid())
            {
                if (const auto gpu_material_guid = first_child_guid_by_type(
                    index_data,
                    source_submesh.material.guid,
                    meta::get_meta_type_id_string<GpuMaterialAsset>()))
                {
                    gpu_submesh.material = AssetRef<GpuMaterialAsset>{ *gpu_material_guid };
                }
            }

            gpu_model.submeshes.push_back(gpu_submesh);
            gpu_model.vertex_count = static_cast<std::uint32_t>(model.positions.size());
            gpu_model.index_count = static_cast<std::uint32_t>(model.indices.size());

            return out;
        }

        const SubMesh& resolve_source_submesh(
            const ModelDataAsset& model,
            const TerrainRecipeAsset& recipe)
        {
            const int requested_index = recipe.source_submesh_index < 0 ? 0 : recipe.source_submesh_index;
            if (requested_index < 0
                || requested_index >= static_cast<int>(model.submeshes.size()))
            {
                throw std::runtime_error("Terrain cook failed: source_submesh_index out of range.");
            }
            return model.submeshes[static_cast<std::size_t>(requested_index)];
        }
    }

    TerrainCookResult TerrainCooker::cook_recipe(
        ResourceManager& rm,
        const Guid& recipe_guid,
        EngineContext& ctx)
    {
        // Terrain cooking is intentionally explicit: the recipe is an
        // editor-side configuration asset, while the outputs are runtime
        // assets regenerated into a deterministic folder owned by that recipe.
        TerrainCookResult result{};
        if (!recipe_guid.valid())
        {
            result.error_message = "Terrain cook failed: invalid recipe guid.";
            return result;
        }

        const AssetIndexDataPtr index_data = rm.get_index_data();
        if (!index_data)
        {
            result.error_message = "Terrain cook failed: asset index not available.";
            return result;
        }

        bool loaded_recipe_here = false;
        if (!rm.handle_for_guid<TerrainRecipeAsset>(recipe_guid))
        {
            rm.load_asset<TerrainRecipeAsset>(recipe_guid, ctx);
            loaded_recipe_here = true;
        }

        auto recipe_handle_opt = rm.handle_for_guid<TerrainRecipeAsset>(recipe_guid);
        if (!recipe_handle_opt)
        {
            result.error_message = "Terrain cook failed: recipe asset could not be loaded.";
            return result;
        }

        TerrainRecipeAsset recipe{};
        rm.storage().read(*recipe_handle_opt, [&](const TerrainRecipeAsset& src)
            {
                recipe = src;
            });

        if (!recipe.source_model_ref.guid.valid())
        {
            result.error_message = "Terrain cook failed: recipe has no source_model_ref.";
            return result;
        }
        if (recipe.sample_spacing_x <= 0.0f || recipe.sample_spacing_z <= 0.0f)
        {
            result.error_message = "Terrain cook failed: sample spacing must be > 0.";
            return result;
        }
        if (recipe.horizontal_scale_x <= 0.0f
            || recipe.horizontal_scale_z <= 0.0f
            || recipe.height_scale <= 0.0f)
        {
            result.error_message = "Terrain cook failed: terrain scale values must be > 0.";
            return result;
        }
        if (recipe.chunk_count_x == 0u || recipe.chunk_count_z == 0u)
        {
            result.error_message = "Terrain cook failed: chunk counts must be > 0.";
            return result;
        }

        bool loaded_model_here = false;
        if (!rm.handle_for_guid<ModelDataAsset>(recipe.source_model_ref.guid))
        {
            rm.load_asset<ModelDataAsset>(recipe.source_model_ref.guid, ctx);
            loaded_model_here = true;
        }

        auto model_handle_opt = rm.handle_for_guid<ModelDataAsset>(recipe.source_model_ref.guid);
        if (!model_handle_opt)
        {
            result.error_message = "Terrain cook failed: source ModelDataAsset could not be loaded.";
            return result;
        }

        ModelDataAsset source_model{};
        rm.storage().read(*model_handle_opt, [&](const ModelDataAsset& src)
            {
                source_model = src;
            });

        if (source_model.positions.empty() || source_model.indices.empty() || source_model.submeshes.empty())
        {
            result.error_message = "Terrain cook failed: source model does not contain usable mesh data.";
            return result;
        }

        const SubMesh& source_submesh = resolve_source_submesh(source_model, recipe);
        if (source_submesh.nbr_indices < 3)
        {
            result.error_message = "Terrain cook failed: source terrain submesh has no triangles.";
            return result;
        }

        const HeightGrid grid = rasterize_height_grid(source_model, source_submesh, recipe);
        if (grid.samples_x < 2 || grid.samples_z < 2)
        {
            result.error_message = "Terrain cook failed: sampled terrain grid is too small.";
            return result;
        }

        const auto recipe_entry_it = index_data->by_guid.find(recipe_guid);
        if (recipe_entry_it == index_data->by_guid.end() || !recipe_entry_it->second)
        {
            result.error_message = "Terrain cook failed: recipe entry missing from asset index.";
            return result;
        }

        const std::string recipe_name = sanitize_name(recipe_entry_it->second->meta.name);
        const std::filesystem::path assets_root = rm.assets_root();
        const std::filesystem::path cook_root = assets_root / "terrain" / recipe_name;
        const std::filesystem::path chunks_root = cook_root / "chunks";
        const std::filesystem::path render_root = cook_root / "render";
        const std::filesystem::path terrain_asset_path = cook_root / "terrain.json";
        const std::filesystem::path terrain_meta_path = cook_root / "terrain.meta.json";

        auto* batch_registry = dynamic_cast<BatchRegistry*>(ctx.batch_registry.get());
        if (!batch_registry || batch_registry->index_path().empty())
        {
            result.error_message = "Terrain cook failed: concrete BatchRegistry with a valid index is required.";
            return result;
        }

        const std::filesystem::path batches_root = batch_registry->index_path().parent_path();
        const std::vector<Guid> old_batch_ids = read_existing_chunk_batch_ids(terrain_asset_path);

        for (const auto& old_batch_id : old_batch_ids)
        {
            if (!old_batch_id.valid())
                continue;
            if (batch_registry->is_batch_loaded(old_batch_id))
            {
                // Batch unload must happen before entering this RM-strand cook.
                // EditorActions performs that preflight explicitly to avoid a
                // cross-strand deadlock between BatchRegistry and ResourceManager.
                result.error_message = std::format(
                    "Terrain cook failed: terrain chunk batch {} is still loaded. The caller must unload generated terrain batches before cooking.",
                    old_batch_id.to_string());
                return result;
            }
        }

        const Guid terrain_guid = guid_for_asset_path_or_new(index_data, terrain_asset_path);
        result.terrain_guid = terrain_guid;

        const auto chunk_ranges_x = build_chunk_axis_ranges(grid.samples_x - 1u, recipe.chunk_count_x);
        const auto chunk_ranges_z = build_chunk_axis_ranges(grid.samples_z - 1u, recipe.chunk_count_z);

        // The terrain cook owns its folder. Wiping and rewriting it keeps
        // re-cooks deterministic and prevents stale chunk files from surviving
        // after the user changes sample spacing or chunk size.
        std::filesystem::remove_all(cook_root);
        std::filesystem::create_directories(chunks_root);
        std::filesystem::create_directories(render_root);
        std::filesystem::create_directories(batches_root);

        TerrainAsset terrain_asset{};
        terrain_asset.world_origin = recipe.world_origin + glm::vec3(grid.origin_x, 0.0f, grid.origin_z);
        terrain_asset.total_samples_x = grid.samples_x;
        terrain_asset.total_samples_z = grid.samples_z;
        terrain_asset.cell_size_x = recipe.sample_spacing_x;
        terrain_asset.cell_size_z = recipe.sample_spacing_z;
        terrain_asset.chunk_count_x = static_cast<std::uint32_t>(chunk_ranges_x.size());
        terrain_asset.chunk_count_z = static_cast<std::uint32_t>(chunk_ranges_z.size());

        std::set<Guid> new_batch_ids{};

        // Build every chunk from the same sampled grid so collision and render
        // match exactly. That is the key simplification for the MVP.
        for (std::uint32_t chunk_z = 0; chunk_z < terrain_asset.chunk_count_z; ++chunk_z)
        {
            for (std::uint32_t chunk_x = 0; chunk_x < terrain_asset.chunk_count_x; ++chunk_x)
            {
                TerrainChunkCookOutput chunk_output =
                    build_chunk_output(
                        index_data,
                        grid,
                        recipe,
                        source_submesh,
                        chunk_ranges_x[chunk_x],
                        chunk_ranges_z[chunk_z],
                        chunk_x,
                        chunk_z);

                const std::string chunk_stem =
                    std::format("chunk_{}_{}", chunk_x, chunk_z);

                const std::filesystem::path chunk_asset_path = chunks_root / (chunk_stem + ".json");
                const std::filesystem::path chunk_meta_path = chunks_root / (chunk_stem + ".meta.json");
                const std::filesystem::path render_model_path = render_root / (chunk_stem + "_model.json");
                const std::filesystem::path render_model_meta_path = render_root / (chunk_stem + "_model.meta.json");
                const std::filesystem::path render_gpu_path = render_root / (chunk_stem + "_gpu.json");
                const std::filesystem::path render_gpu_meta_path = render_root / (chunk_stem + "_gpu.meta.json");

                chunk_output.chunk_guid = guid_for_asset_path_or_new(index_data, chunk_asset_path);
                chunk_output.render_model_guid = guid_for_asset_path_or_new(index_data, render_model_path);
                chunk_output.render_gpu_guid = guid_for_asset_path_or_new(index_data, render_gpu_path);
                chunk_output.batch_id = deterministic_guid(recipe_guid, "terrain_chunk_batch", static_cast<int>(chunk_x), static_cast<int>(chunk_z));
                chunk_output.batch_name = std::format(
                    "terrain_{}_chunk_{}_{}",
                    recipe_name,
                    chunk_x,
                    chunk_z);

                chunk_output.render_gpu_model.model_ref = AssetRef<ModelDataAsset>{ chunk_output.render_model_guid };

                // The generated terrain chunk batch only needs the chunk-local
                // render assets plus the terrain chunk collision payload.
                // Keep this explicit rather than trying to infer it through a
                // broader asset tree during the cook.
                chunk_output.asset_closure = {
                    chunk_output.render_model_guid,
                    chunk_output.render_gpu_guid,
                    chunk_output.chunk_guid
                };
                if (source_submesh.material.guid.valid())
                    chunk_output.asset_closure.push_back(source_submesh.material.guid);
                if (!chunk_output.render_gpu_model.submeshes.empty()
                    && chunk_output.render_gpu_model.submeshes[0].material.guid.valid())
                {
                    chunk_output.asset_closure.push_back(
                        chunk_output.render_gpu_model.submeshes[0].material.guid);
                }
                std::sort(chunk_output.asset_closure.begin(), chunk_output.asset_closure.end());
                chunk_output.asset_closure.erase(
                    std::unique(chunk_output.asset_closure.begin(), chunk_output.asset_closure.end()),
                    chunk_output.asset_closure.end());

                const AssetMetaData render_model_meta = make_meta(
                    chunk_output.render_model_guid,
                    chunk_output.chunk_guid,
                    chunk_stem + "_model",
                    meta::get_meta_type_id_string<ModelDataAsset>());
                const AssetMetaData render_gpu_meta = make_meta(
                    chunk_output.render_gpu_guid,
                    chunk_output.chunk_guid,
                    chunk_stem + "_gpu",
                    meta::get_meta_type_id_string<GpuModelAsset>());
                const AssetMetaData chunk_meta = make_meta(
                    chunk_output.chunk_guid,
                    terrain_guid,
                    chunk_stem,
                    meta::get_meta_type_id_string<TerrainChunkAsset>());

                rm.import(
                    chunk_output.render_model,
                    render_model_path.string(),
                    render_model_meta,
                    render_model_meta_path.string());
                rm.import(
                    chunk_output.render_gpu_model,
                    render_gpu_path.string(),
                    render_gpu_meta,
                    render_gpu_meta_path.string());
                rm.import(
                    chunk_output.chunk_asset,
                    chunk_asset_path.string(),
                    chunk_meta,
                    chunk_meta_path.string());

                const glm::vec3 world_bounds_min =
                    chunk_output.chunk_asset.world_origin + chunk_output.chunk_asset.local_bounds_min;
                const glm::vec3 world_bounds_max =
                    chunk_output.chunk_asset.world_origin + chunk_output.chunk_asset.local_bounds_max;

                terrain_asset.chunks.push_back(TerrainChunkEntry{
                    .chunk_x = static_cast<std::int32_t>(chunk_x),
                    .chunk_z = static_cast<std::int32_t>(chunk_z),
                    .terrain_chunk_ref = AssetRef<TerrainChunkAsset>{ chunk_output.chunk_guid },
                    .batch_id = chunk_output.batch_id,
                    .batch_name = chunk_output.batch_name,
                    .world_bounds_min = world_bounds_min,
                    .world_bounds_max = world_bounds_max
                    });

                new_batch_ids.insert(chunk_output.batch_id);

                const auto batch_path = batches_root / (chunk_output.batch_id.to_string() + ".json");
                const auto entity_guid = deterministic_guid(recipe_guid, "terrain_chunk_entity", static_cast<int>(chunk_x), static_cast<int>(chunk_z));
                const nlohmann::json entity_json = build_chunk_batch_entity_json(
                    entity_guid,
                    std::format("TerrainChunk({}, {})", chunk_x, chunk_z),
                    chunk_output.chunk_asset,
                    chunk_output.render_gpu_guid,
                    chunk_output.chunk_guid);

                nlohmann::json batch_json{};
                batch_json["header"] = nlohmann::json{
                    { "id", chunk_output.batch_id.to_string() },
                    { "name", chunk_output.batch_name }
                };
                batch_json["header"]["asset_closure"] = nlohmann::json::array();
                for (const auto& guid : chunk_output.asset_closure)
                    batch_json["header"]["asset_closure"].push_back(guid.to_string());
                batch_json["entities"] = nlohmann::json::array({ entity_json });

                std::ofstream batch_file(batch_path);
                if (!batch_file.is_open())
                {
                    result.error_message = std::format(
                        "Terrain cook failed: could not write batch file '{}'.",
                        batch_path.string());
                    return result;
                }
                batch_file << batch_json.dump(2);

                if (!batch_registry->upsert_batch_record(BatchInfo{
                    .id = chunk_output.batch_id,
                    .name = chunk_output.batch_name,
                    .filename = batch_path.filename(),
                    .asset_closure_hdr = chunk_output.asset_closure,
                    .generated = true,
                    .read_only = true,
                    .owner_guid = recipe_guid,
                    .generator_tag = "terrain"
                    }))
                {
                    result.error_message = std::format(
                        "Terrain cook failed: could not register terrain chunk batch {}.",
                        chunk_output.batch_id.to_string());
                    return result;
                }
            }
        }

        for (const auto& old_batch_id : old_batch_ids)
        {
            if (!old_batch_id.valid() || new_batch_ids.contains(old_batch_id))
                continue;

            if (batch_registry->is_batch_loaded(old_batch_id))
            {
                result.error_message = std::format(
                    "Terrain cook failed: stale terrain chunk batch {} is still loaded. The caller must unload generated terrain batches before cooking.",
                    old_batch_id.to_string());
                return result;
            }

            (void)batch_registry->delete_batch(old_batch_id);
        }

        batch_registry->save_index();

        const AssetMetaData terrain_meta = make_meta(
            terrain_guid,
            recipe_guid,
            recipe_name + "_terrain",
            meta::get_meta_type_id_string<TerrainAsset>());
        rm.import(
            terrain_asset,
            terrain_asset_path.string(),
            terrain_meta,
            terrain_meta_path.string());

        if (loaded_model_here)
            rm.unload_asset<ModelDataAsset>(recipe.source_model_ref.guid, ctx);
        if (loaded_recipe_here)
            rm.unload_asset<TerrainRecipeAsset>(recipe_guid, ctx);

        result.success = true;
        return result;
    }
}
