// Created by Codex.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "Guid.h"
#include "engineapi/IResourceManager.hpp"
#include "ecs/Entity.hpp"

#include <deque>
#include <future>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "entt/fwd.hpp"

namespace eeng
{
    struct EngineContext;
}

namespace eeng::assets
{
    struct TerrainAsset;
}

namespace eeng::ecs::systems
{
    /**
     * @brief Runtime terrain residency system for cooked chunked terrain.
     *
     * `TerrainSystem` reads `TerrainComponent`, ensures the referenced cooked
     * `TerrainAsset` manifest is available, selects the chunk coords that are
     * currently wanted, and spawns transient runtime chunk entities for the
     * chosen chunks.
     *
     * The current MVP policy is intentionally simple: one explicitly selected
     * chunk per terrain root. The system is structured so that more advanced
     * chunk-selection logic can be inserted later without changing the overall
     * runtime shape.
     */
    class TerrainSystem
    {
    public:
        /**
         * @brief Advance terrain residency for the currently bound world.
         *
         * The same system instance is used in edit and play mode. Runtime
         * state is therefore explicitly guarded against world/registry swaps.
         */
        void update(
            entt::registry& registry,
            EngineContext& ctx,
            float delta_time);

        /**
         * @brief Clear cached terrain residency state for the currently bound world.
         *
         * This is primarily intended for explicit play/edit lifecycle cleanup.
         * The system also contains defensive self-healing for registry swaps,
         * but this function is the clearer ownership boundary.
         */
        void reset_runtime(EngineContext& ctx);

    private:
        struct PendingChunkLoad
        {
            Guid terrain_guid{};
            Guid chunk_guid{};
            glm::ivec2 coord{ 0, 0 };
            std::deque<Guid> branch_guids{};
            std::shared_future<TaskResult> future{};
        };

        struct PendingManifestLoad
        {
            Guid terrain_guid{};
            std::shared_future<TaskResult> future{};
        };

        struct PendingUnload
        {
            std::deque<Guid> branch_guids{};
            std::shared_future<TaskResult> future{};
        };

        struct RootRuntime
        {
            // Track which manifest the root is currently associated with so we
            // can cleanly react when the user swaps the TerrainAsset.
            Guid terrain_guid{};

            // Exactly one active chunk for the MVP explicit-chunk policy.
            glm::ivec2 active_coord{ std::numeric_limits<int>::min(), std::numeric_limits<int>::min() };
            Guid active_chunk_guid{};
            ecs::Entity active_chunk_entity{};
            std::deque<Guid> active_branch_guids{};

            // Chunk load is async because GpuModelAsset init can enqueue
            // main-thread GL work. We poll this future in later updates.
            std::optional<PendingManifestLoad> pending_manifest_load{};
            std::optional<PendingChunkLoad> pending_chunk_load{};

            // Unloads are also async so we do not block the main thread while
            // the RM tears down references and GPU objects.
            std::vector<PendingUnload> pending_unloads{};

            // When the terrain root disappears we keep its runtime state just
            // long enough to drain pending loads/unloads and release assets.
            bool retired = false;
        };

        static std::deque<Guid> build_branch_bottomup(
            const Guid& guid,
            const EngineContext& ctx);

        static std::optional<Guid> lookup_chunk_guid(
            const assets::TerrainAsset& terrain,
            const glm::ivec2& coord);

        static bool future_ready(const std::shared_future<TaskResult>& future);

        static bool root_still_wants_chunk(
            entt::registry& registry,
            const ecs::Entity& root_entity,
            const Guid& terrain_guid,
            const glm::ivec2& coord);

        bool ensure_manifest_loaded(
            EngineContext& ctx,
            ecs::Entity root_entity,
            RootRuntime& runtime,
            Guid terrain_guid);

        void process_pending_manifest_load(
            entt::registry& registry,
            EngineContext& ctx,
            ecs::Entity root_entity,
            RootRuntime& runtime);

        void process_pending_load(
            entt::registry& registry,
            EngineContext& ctx,
            ecs::Entity root_entity,
            RootRuntime& runtime);

        void process_pending_unloads(RootRuntime& runtime);

        void start_chunk_load(
            EngineContext& ctx,
            RootRuntime& runtime,
            Guid terrain_guid,
            Guid chunk_guid,
            const glm::ivec2& coord);

        void retire_active_chunk(
            EngineContext& ctx,
            RootRuntime& runtime);

        void spawn_chunk_entity(
            entt::registry& registry,
            EngineContext& ctx,
            ecs::Entity root_entity,
            RootRuntime& runtime,
            Guid chunk_guid,
            const glm::ivec2& coord);

        BatchId terrain_batch_id_ = Guid::generate();
        // TerrainSystem is longer-lived than a single world. This pointer is a
        // defensive guard so cached world-local entity ids are not reused
        // across edit/play registry swaps.
        const entt::registry* active_registry_ = nullptr;
        std::unordered_map<ecs::Entity, RootRuntime> roots_{};
    };
}
