// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/TerrainSystem.hpp"

#include "AssetIndexData.hpp"
#include "LogMacros.h"
#include "ResourceManager.hpp"
#include "assets/AssetTreeViews.hpp"
#include "assets/types/TerrainAssets.hpp"
#include "ecs/EntityManager.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/ModelComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TerrainComponent.hpp"
#include "ecs/TransformComponent.hpp"
#include "engineapi/EngineContextHelpers.hpp"
#include "meta/EntityMetaHelpers.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <string>

namespace eeng::ecs::systems
{
    namespace
    {
        constexpr glm::ivec2 kInvalidCoord{
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::min()
        };

        // Terrain roots are ECS entities that currently own a TerrainComponent.
        bool terrain_root_is_live(entt::registry& registry, const ecs::Entity& entity)
        {
            return entity.has_id()
                && registry.valid(entity)
                && registry.all_of<ecs::TerrainComponent>(entity);
        }

        // Runtime-generated terrain chunk entities are transient children under
        // the terrain root. This helper removes stale ones so edit/play mode
        // switching does not accumulate duplicate chunk entities under the same root.
        void prune_generated_chunks_for_root(
            entt::registry& registry,
            EngineContext& ctx,
            const ecs::Entity& root_entity,
            const ecs::Entity& keep_entity = ecs::Entity{})
        {
            auto* em = eeng::try_get_entity_manager_ptr(ctx, "TerrainSystem");
            if (!em || !em->entity_valid(root_entity))
                return;

            auto& scene_graph = em->scene_graph();
            if (!scene_graph.contains(root_entity))
                return;

            std::vector<ecs::Entity> stale_chunks{};
            const auto branch = scene_graph.get_branch_topdown(root_entity);
            for (const auto& entity : branch)
            {
                if (entity == root_entity || !entity.has_id())
                    continue;
                if (!em->entity_valid(entity))
                    continue;
                if (!scene_graph.contains(entity))
                    continue;
                if (scene_graph.get_parent(entity) != root_entity)
                    continue;
                if (keep_entity.has_id() && entity == keep_entity)
                    continue;

                const auto* header = registry.try_get<ecs::HeaderComponent>(entity);
                if (!header || header->chunk_tag != "terrain_chunk")
                    continue;

                stale_chunks.push_back(entity);
            }

            for (const auto& entity : stale_chunks)
            {
                if (em->entity_valid(entity))
                    em->destroy_entity_now(entity);
            }
        }
    }

    std::deque<Guid> TerrainSystem::build_branch_bottomup(
        const Guid& guid,
        const EngineContext& ctx)
    {
        std::deque<Guid> stack{};
        if (!guid.valid() || !ctx.resource_manager)
            return stack;

        auto index_data = ctx.resource_manager->get_index_data();
        if (!index_data || !index_data->trees)
            return stack;

        auto& tree = index_data->trees->content_tree;
        if (!tree.contains(guid))
            return stack;

        tree.traverse_breadthfirst(guid, [&](const Guid& node_guid, size_t)
            {
                // Bottom-up order matches how the existing resource-browser
                // batch load tools prepare dependency closures.
                stack.push_front(node_guid);
            });
        return stack;
    }

    std::optional<Guid> TerrainSystem::lookup_chunk_guid(
        const assets::TerrainAsset& terrain,
        const glm::ivec2& coord)
    {
        if (coord.x < 0 || coord.y < 0)
            return std::nullopt;

        const auto chunk_x = static_cast<std::uint32_t>(coord.x);
        const auto chunk_z = static_cast<std::uint32_t>(coord.y);
        if (chunk_x >= terrain.chunk_count_x || chunk_z >= terrain.chunk_count_z)
            return std::nullopt;

        const std::size_t linear_index =
            static_cast<std::size_t>(chunk_z) * static_cast<std::size_t>(terrain.chunk_count_x)
            + static_cast<std::size_t>(chunk_x);
        if (linear_index >= terrain.chunks.size())
            return std::nullopt;

        const auto& ref = terrain.chunks[linear_index];
        if (!ref.guid.valid())
            return std::nullopt;
        return ref.guid;
    }

    bool TerrainSystem::future_ready(const std::shared_future<TaskResult>& future)
    {
        if (!future.valid())
            return true;
        return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    bool TerrainSystem::root_still_wants_chunk(
        entt::registry& registry,
        const ecs::Entity& root_entity,
        const Guid& terrain_guid,
        const glm::ivec2& coord)
    {
        if (!terrain_root_is_live(registry, root_entity))
            return false;

        const auto* terrain = registry.try_get<ecs::TerrainComponent>(root_entity);
        if (!terrain || !terrain->enabled)
            return false;

        return terrain->terrain_ref.guid == terrain_guid
            && terrain->explicit_chunk_coord == coord;
    }

    bool TerrainSystem::ensure_manifest_loaded(
        EngineContext& ctx,
        ecs::Entity root_entity,
        RootRuntime& runtime,
        Guid terrain_guid)
    {
        auto* rm = eeng::try_get_resource_manager_ptr(ctx, "TerrainSystem");
        if (!rm || !terrain_guid.valid())
            return false;

        const auto* terrain_component =
            ctx.entity_manager->registry().try_get<ecs::TerrainComponent>(root_entity);
        if (terrain_component && terrain_component->terrain_ref.is_bound())
            return true;

        if (runtime.pending_manifest_load.has_value()
            && runtime.pending_manifest_load->terrain_guid == terrain_guid)
        {
            return false;
        }

        runtime.pending_manifest_load = PendingManifestLoad{
            .terrain_guid = terrain_guid,
            .future = ctx.resource_manager->load_and_bind_async(
                std::deque<Guid>{ terrain_guid },
                terrain_batch_id_,
                ctx)
        };
        return false;
    }

    void TerrainSystem::process_pending_manifest_load(
        entt::registry& registry,
        EngineContext& ctx,
        ecs::Entity root_entity,
        RootRuntime& runtime)
    {
        if (!runtime.pending_manifest_load.has_value())
            return;

        auto& pending = *runtime.pending_manifest_load;
        if (!future_ready(pending.future))
            return;

        TaskResult result = pending.future.get();
        const Guid loaded_terrain_guid = pending.terrain_guid;
        runtime.pending_manifest_load.reset();

        if (!result.success)
        {
            EENG_LOG_WARN(&ctx,
                "TerrainSystem: failed to load TerrainAsset %s.",
                loaded_terrain_guid.to_string().c_str());
            return;
        }

        if (!terrain_root_is_live(registry, root_entity))
        {
            runtime.pending_unloads.push_back(PendingUnload{
                .branch_guids = { loaded_terrain_guid },
                .future = ctx.resource_manager->unbind_and_unload_async(
                    std::deque<Guid>{ loaded_terrain_guid },
                    terrain_batch_id_,
                    ctx)
            });
            return;
        }

        const auto* terrain_component = registry.try_get<ecs::TerrainComponent>(root_entity);
        if (!terrain_component || terrain_component->terrain_ref.guid != loaded_terrain_guid)
        {
            runtime.pending_unloads.push_back(PendingUnload{
                .branch_guids = { loaded_terrain_guid },
                .future = ctx.resource_manager->unbind_and_unload_async(
                    std::deque<Guid>{ loaded_terrain_guid },
                    terrain_batch_id_,
                    ctx)
            });
            return;
        }

        meta::bind_asset_refs_for_entity(root_entity, ctx);
    }

    void TerrainSystem::process_pending_load(
        entt::registry& registry,
        EngineContext& ctx,
        ecs::Entity root_entity,
        RootRuntime& runtime)
    {
        if (!runtime.pending_chunk_load.has_value())
            return;

        auto& pending = *runtime.pending_chunk_load;
        if (!future_ready(pending.future))
            return;

        TaskResult result = pending.future.get();
        const auto loaded_chunk_guid = pending.chunk_guid;
        const auto loaded_coord = pending.coord;
        const auto loaded_branch = pending.branch_guids;
        const auto loaded_terrain_guid = pending.terrain_guid;
        runtime.pending_chunk_load.reset();

        if (!result.success)
        {
            EENG_LOG_WARN(&ctx,
                "TerrainSystem: chunk load failed for terrain %s chunk (%d, %d).",
                loaded_terrain_guid.to_string().c_str(),
                loaded_coord.x,
                loaded_coord.y);
            return;
        }

        // Chunk requests can go stale while async load is in flight. In that
        // case we immediately release the branch instead of spawning an entity.
        if (!root_still_wants_chunk(registry, root_entity, loaded_terrain_guid, loaded_coord))
        {
            runtime.pending_unloads.push_back(PendingUnload{
                .branch_guids = loaded_branch,
                .future = ctx.resource_manager->unbind_and_unload_async(
                    loaded_branch,
                    terrain_batch_id_,
                    ctx)
            });
            return;
        }

        spawn_chunk_entity(registry, ctx, root_entity, runtime, loaded_chunk_guid, loaded_coord);
        runtime.active_chunk_guid = loaded_chunk_guid;
        runtime.active_coord = loaded_coord;
        runtime.active_branch_guids = loaded_branch;
    }

    void TerrainSystem::process_pending_unloads(RootRuntime& runtime)
    {
        auto it = runtime.pending_unloads.begin();
        while (it != runtime.pending_unloads.end())
        {
            if (!future_ready(it->future))
            {
                ++it;
                continue;
            }

            // We intentionally only drain the future here. The resource manager
            // already logged task results through its normal event path.
            (void)it->future.get();
            it = runtime.pending_unloads.erase(it);
        }
    }

    void TerrainSystem::start_chunk_load(
        EngineContext& ctx,
        RootRuntime& runtime,
        Guid terrain_guid,
        Guid chunk_guid,
        const glm::ivec2& coord)
    {
        auto branch_guids = build_branch_bottomup(chunk_guid, ctx);
        if (branch_guids.empty())
        {
            EENG_LOG_WARN(&ctx,
                "TerrainSystem: no dependency branch found for TerrainChunkAsset %s.",
                chunk_guid.to_string().c_str());
            return;
        }

        runtime.pending_chunk_load = PendingChunkLoad{
            .terrain_guid = terrain_guid,
            .chunk_guid = chunk_guid,
            .coord = coord,
            .branch_guids = branch_guids,
            .future = ctx.resource_manager->load_and_bind_async(
                branch_guids,
                terrain_batch_id_,
                ctx)
        };
    }

    void TerrainSystem::retire_active_chunk(
        EngineContext& ctx,
        RootRuntime& runtime)
    {
        if (runtime.active_chunk_entity.has_id())
        {
            auto* em = eeng::try_get_entity_manager_ptr(ctx, "TerrainSystem");
            if (em && em->entity_valid(runtime.active_chunk_entity))
                em->destroy_entity_now(runtime.active_chunk_entity);
        }

        if (!runtime.active_branch_guids.empty())
        {
            runtime.pending_unloads.push_back(PendingUnload{
                .branch_guids = runtime.active_branch_guids,
                .future = ctx.resource_manager->unbind_and_unload_async(
                    runtime.active_branch_guids,
                    terrain_batch_id_,
                    ctx)
            });
        }

        runtime.active_chunk_entity = ecs::Entity{};
        runtime.active_chunk_guid = Guid{};
        runtime.active_coord = kInvalidCoord;
        runtime.active_branch_guids.clear();
    }

    void TerrainSystem::spawn_chunk_entity(
        entt::registry& registry,
        EngineContext& ctx,
        ecs::Entity root_entity,
        RootRuntime& runtime,
        Guid chunk_guid,
        const glm::ivec2& coord)
    {
        auto* rm = eeng::try_get_resource_manager_ptr(ctx, "TerrainSystem");
        auto* em = eeng::try_get_entity_manager_ptr(ctx, "TerrainSystem");
        if (!rm || !em)
            return;

        auto chunk_handle_opt = rm->handle_for_guid<assets::TerrainChunkAsset>(chunk_guid);
        if (!chunk_handle_opt)
        {
            EENG_LOG_WARN(&ctx,
                "TerrainSystem: TerrainChunkAsset %s was not loaded after chunk task completed.",
                chunk_guid.to_string().c_str());
            return;
        }

        assets::TerrainChunkAsset chunk{};
        rm->storage().read(*chunk_handle_opt, [&](const assets::TerrainChunkAsset& src)
            {
                chunk = src;
            });

        // Replace the previous live chunk entity before spawning the new one.
        if (runtime.active_chunk_entity.has_id())
            retire_active_chunk(ctx, runtime);

        const auto [chunk_entity_guid, chunk_entity] = em->create_entity_live_parent(
            "terrain_chunk",
            std::format("TerrainChunk({}, {})", coord.x, coord.y),
            root_entity,
            ecs::Entity::EntityNull);

        auto& tfm = registry.emplace<ecs::TransformComponent>(chunk_entity);
        tfm.set_position(chunk.world_origin);
        tfm.set_rotation(glm::quat{ 1.0f, 0.0f, 0.0f, 0.0f });
        tfm.set_scale(glm::vec3{ 1.0f });

        ecs::ModelComponent model{};
        model.name = std::format("TerrainChunkModel({}, {})", coord.x, coord.y);
        model.model_ref = chunk.render_model_ref;
        registry.emplace<ecs::ModelComponent>(chunk_entity, model);

        ecs::RigidBodyComponent rigid_body{};
        rigid_body.motion = ecs::PhysicsMotionType::Static;
        registry.emplace<ecs::RigidBodyComponent>(chunk_entity, rigid_body);

        ecs::ColliderDesc collider{};
        collider.id = 1;
        collider.type = ecs::ColliderType::Heightfield;
        collider.terrain_chunk_ref = AssetRef<assets::TerrainChunkAsset>{ chunk_guid };

        ecs::ColliderComponent collider_component{};
        collider_component.colliders.push_back(collider);
        registry.emplace<ecs::ColliderComponent>(chunk_entity, collider_component);

        // Now that the chunk branch is loaded and bound in the resource manager,
        // bind the entity-side AssetRef handles so render/physics systems can
        // access them directly.
        meta::bind_asset_refs_for_entity(chunk_entity, ctx);

        runtime.active_chunk_entity = chunk_entity;
        (void)chunk_entity_guid;
    }

    void TerrainSystem::update(
        entt::registry& registry,
        EngineContext& ctx,
        float delta_time)
    {
        (void)delta_time;

        // TerrainSystem lives longer than a single world. When the engine swaps
        // edit/play registries we discard cached entity state here instead of
        // trying to carry entity ids across worlds.
        if (active_registry_ && active_registry_ != &registry)
            roots_.clear();
        active_registry_ = &registry;

        auto* rm = eeng::try_get_resource_manager_ptr(ctx, "TerrainSystem");
        auto* em = eeng::try_get_entity_manager_ptr(ctx, "TerrainSystem");
        if (!rm || !em)
            return;

        std::unordered_map<ecs::Entity, bool> seen_roots{};

        auto view = registry.view<ecs::TerrainComponent>();
        for (auto entity_id : view)
        {
            const ecs::Entity entity{ entity_id };
            seen_roots[entity] = true;

            auto& terrain_component = view.get<ecs::TerrainComponent>(entity_id);
            auto& runtime = roots_[entity];

            // Runtime-generated terrain chunks are transient view entities.
            // When we come back from play mode, stale children from the old
            // world/view can still exist under the root. Prune them before we
            // decide what the current root should show.
            prune_generated_chunks_for_root(registry, ctx, entity, runtime.active_chunk_entity);

            if (runtime.active_chunk_entity.has_id() && !em->entity_valid(runtime.active_chunk_entity))
            {
                runtime.active_chunk_entity = ecs::Entity{};
                runtime.active_chunk_guid = Guid{};
                runtime.active_coord = kInvalidCoord;
                runtime.active_branch_guids.clear();
            }

            // Detect manifest swaps so we can retire the old chunk residency and
            // reload the new manifest deterministically.
            if (runtime.terrain_guid.valid() && runtime.terrain_guid != terrain_component.terrain_ref.guid)
            {
                retire_active_chunk(ctx, runtime);
                runtime.pending_unloads.push_back(PendingUnload{
                    .branch_guids = { runtime.terrain_guid },
                    .future = ctx.resource_manager->unbind_and_unload_async(
                        std::deque<Guid>{ runtime.terrain_guid },
                        terrain_batch_id_,
                        ctx)
                });
                runtime.terrain_guid = Guid{};
            }

            process_pending_manifest_load(registry, ctx, entity, runtime);
            process_pending_load(registry, ctx, entity, runtime);
            process_pending_unloads(runtime);

            if (!terrain_component.enabled || !terrain_component.terrain_ref.guid.valid())
            {
                retire_active_chunk(ctx, runtime);
                if (runtime.terrain_guid.valid())
                {
                    runtime.pending_unloads.push_back(PendingUnload{
                        .branch_guids = { runtime.terrain_guid },
                        .future = ctx.resource_manager->unbind_and_unload_async(
                            std::deque<Guid>{ runtime.terrain_guid },
                            terrain_batch_id_,
                            ctx)
                    });
                }
                runtime.retired = false;
                runtime.terrain_guid = Guid{};
                continue;
            }

            if (!ensure_manifest_loaded(ctx, entity, runtime, terrain_component.terrain_ref.guid))
                continue;

            runtime.terrain_guid = terrain_component.terrain_ref.guid;

            auto terrain_handle_opt = rm->handle_for_guid<assets::TerrainAsset>(terrain_component.terrain_ref.guid);
            if (!terrain_handle_opt)
                continue;

            std::optional<Guid> desired_chunk_guid{};
            rm->storage().read(*terrain_handle_opt, [&](const assets::TerrainAsset& terrain)
                {
                    desired_chunk_guid = lookup_chunk_guid(terrain, terrain_component.explicit_chunk_coord);
                });

            const glm::ivec2 desired_coord = terrain_component.explicit_chunk_coord;

            if (!desired_chunk_guid.has_value())
            {
                // If the requested coord does not exist we simply make the root
                // show no chunk. This keeps the system predictable while the
                // user tunes chunk settings in the cooked terrain.
                retire_active_chunk(ctx, runtime);
                continue;
            }

            if (runtime.active_chunk_guid.valid()
                && (runtime.active_chunk_guid != *desired_chunk_guid
                    || runtime.active_coord != desired_coord))
            {
                retire_active_chunk(ctx, runtime);
            }

            if (runtime.pending_chunk_load.has_value())
            {
                const auto& pending = *runtime.pending_chunk_load;
                if (pending.chunk_guid == *desired_chunk_guid && pending.coord == desired_coord)
                    continue;
            }

            if (runtime.active_chunk_guid == *desired_chunk_guid
                && runtime.active_coord == desired_coord
                && runtime.active_chunk_entity.has_id())
            {
                continue;
            }

            start_chunk_load(ctx, runtime, terrain_component.terrain_ref.guid, *desired_chunk_guid, desired_coord);
        }

        // Retire states whose terrain roots disappeared. We keep them around
        // until any outstanding async load/unload work has drained cleanly.
        for (auto& [root_entity, runtime] : roots_)
        {
            if (seen_roots.contains(root_entity))
                continue;

            runtime.retired = true;
            if (runtime.active_chunk_guid.valid())
                retire_active_chunk(ctx, runtime);

            process_pending_manifest_load(registry, ctx, root_entity, runtime);
            process_pending_load(registry, ctx, root_entity, runtime);
            process_pending_unloads(runtime);
        }

        for (auto it = roots_.begin(); it != roots_.end();)
        {
            auto& runtime = it->second;
            const bool root_live = terrain_root_is_live(registry, it->first);
            const bool idle = !runtime.pending_manifest_load.has_value()
                && !runtime.pending_chunk_load.has_value()
                && runtime.pending_unloads.empty()
                && !runtime.active_chunk_guid.valid()
                && !runtime.active_chunk_entity.has_id();

            if ((!root_live || runtime.retired) && idle)
            {
                it = roots_.erase(it);
                continue;
            }

            ++it;
        }
    }

    void TerrainSystem::reset_runtime(EngineContext& ctx)
    {
        auto* em = eeng::try_get_entity_manager_ptr(ctx, "TerrainSystem");
        if (em)
        {
            auto& registry = em->registry();
            for (auto& [root_entity, runtime] : roots_)
            {
                prune_generated_chunks_for_root(registry, ctx, root_entity, ecs::Entity{});
                if (runtime.active_chunk_entity.has_id() && em->entity_valid(runtime.active_chunk_entity))
                    em->destroy_entity_now(runtime.active_chunk_entity);
            }
        }

        roots_.clear();
        active_registry_ = nullptr;
    }
}
