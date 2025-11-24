// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/Entity.hpp"
#include "IBatchRegistry.hpp"
//#include "ResourceManager.hpp"
#include "engineapi/IResourceManager.hpp"
#include "SerialExecutor.hpp"
#include "MetaSerialize.hpp"
#include "EngineContext.hpp"
#include <unordered_map>
#include <mutex>
#include <future>
#include <string>

#pragma once

// Possible Comamnds
//      CreateBatchCommand
//      DeleteBatchCommand
//      LoadBatchCommand
//      UnloadBatchCommand
//      SpawnEntityInBatchCommand (queue_spawn_entity)
//      AttachEntityToBatchCommand
//      DetachEntityFromBatchCommand

/*
UI ideas

Batches panel:
- name, state (Idle/Queued/Loading/Bound/Unloading/Error)
- entity count
- asset closure size
- last errors (expandable)
- “Load / Unload / Reload” buttons

Asset ownership (optional):
- asset → “held by batches: [A, C]”
- from RM’s lease map (informational only)
*/

/*
{
  "batches": [
    {
      "id": "batch-level-1",
      "name": "Level 1",
      "entities": [
        { "guid": "e-1001", "archetype": "Player", "data": { ... } },
        { "guid": "e-1002", "archetype" : "Room",   "data" : { ... } }
      ],
      "assets": [
          "a-mesh-hero", "a-tex-hero", "a-model-room", "a-tex-wall"
      ]
    }
  ]
}
*/

namespace eeng {

    struct BatchInfo
    {
        enum class State { Unloaded, Queued, Loading, Loaded, Unloading, Error };

        BatchId                     id{};
        std::string                 name{};
        std::filesystem::path       filename{};               // from index
        std::vector<Guid>           asset_closure_hdr;    // what header says (or recomputed)
        std::vector<ecs::EntityRef> live;                 // only while loaded

        State       state{ State::Unloaded };
        TaskResult  last_result{};
    };
    // struct BatchInfo
    // {
    //     using State = enum { Idle, Queued, Loading, Bound, Unloading, Error };

    //     BatchId id{};
    //     std::string name{};
    //     // std::vector<Guid> closure;          // deduped closure of all asset GUIDs in the batch
    //     std::vector<ecs::EntityRef> entities;    // entities associated with this batch

    //     State state{ State::Idle };
    //     // int last_error_count = 0;
    //     TaskResult last_result{};
    // };

    class BatchRegistry : public IBatchRegistry
    {
    public:
        BatchRegistry() = default;
        ~BatchRegistry() = default;

        void save_index(const std::filesystem::path& index_path);
        void load_or_create_index(const std::filesystem::path& index_path);

        Guid create_batch(
            // const BatchId& id,
            std::string name
            // const std::filesystem::path& path
        );


        std::shared_future<TaskResult> queue_save_batch(const BatchId& id, EngineContext& ctx);

        /**
         * @brief Enqueue loading of all batches listed in the batch index.
         *
         * Schedules each batch load as an individual task via queue_load(), and waits
         * for all loads to complete inside a worker thread (not the strand).
         *
         * @note The returned future becomes ready only when *all* batches have fully
         *       completed their load sequence (asset load/bind + entity instantiation).
         *
         * @param ctx Engine context (resource manager, entity manager, thread pool).
         * @return Shared future resolving to a TaskResult summarizing the overall load.
         */
        std::shared_future<TaskResult> queue_load_all_async(EngineContext& ctx);

        std::shared_future<TaskResult> queue_unload_all_async(EngineContext& ctx);

        /**
         * @brief Enqueue saving of all currently loaded batches.
         *
         * Schedules a per-batch save operation (via queue_save_batch()), waits for each
         * to finish inside a worker thread, and aggregates the results.
         *
         * @note Only batches in the Loaded state are saved; unloaded batches are skipped.
         *
         * @param ctx Engine context.
         * @return Shared future resolving to a TaskResult summarizing the save results.
         */
        std::shared_future<TaskResult> queue_save_all_async(EngineContext& ctx);

        /// Create and spawn a new entity in the batch (thread safe).
        std::shared_future<ecs::EntityRef> queue_create_entity(const BatchId& id, const std::string& name, EngineContext& ctx);

        /// Destroy and remove an entity from the batch (thread safe).
        std::shared_future<bool> queue_destroy_entity(const BatchId& id, ecs::EntityRef entity_ref, EngineContext& ctx);

        /// Attach an entity to a batch (thread safe). Does NOT spawn the entity.
        std::shared_future<bool> queue_attach_entity(const BatchId& id, ecs::EntityRef entity_ref, EngineContext& ctx);

        /// Detach an entity from a batch (thread safe). Does NOT destroy the entity.
        std::shared_future<bool> queue_detach_entity(const BatchId& id, ecs::EntityRef entity_ref, EngineContext& ctx);

        /// Spawn entity from description and add to batch (thread safe).
        std::shared_future<ecs::EntityRef> queue_spawn_entity(const BatchId& id, meta::EntitySpawnDesc desc, EngineContext& ctx);


        // (private)
        bool save_batch(const eeng::BatchId& id, EngineContext& ctx);
        // void save_all_batches(EngineContext& ctx);

        // explicit BatchRegistry(eeng::EngineContext& ctx)
        //     : ctx_(ctx), strand_(eeng::SerialExecutor::make(ctx.thread_pool.get())) {
        // }

        // --- CRUD / access --- CRUD means Create, Read, Update, Delete
        // Ensure a BatchInfo exists (creates empty if needed)
        // BatchInfo& ensure(const eeng::BatchId& id, std::string name = {});
        // const BatchInfo* get(const eeng::BatchId& id) const;
        // std::vector<const BatchInfo*> list() const;

        // Called by tooling: set closure & entities (builder side)
        // void set_closure(const eeng::BatchId&, std::vector<eeng::Guid> closure);
        // void add_entity(const eeng::BatchId& id, const ecs::EntityRef& er);
        // void set_entities(const eeng::BatchId&, std::vector<ecs::EntityRef> ents);

        // For UI
        // BatchInfo::State state(const eeng::BatchId&) const;
        // TaskResult last_result(const eeng::BatchId&) const;

        // --- Orchestration (serialized via strand) ---
#if 1
// Returns a future that completes when the full sequence is done.
        std::shared_future<TaskResult> queue_load(const eeng::BatchId& id, EngineContext& ctx);
#endif
#if 1
        std::shared_future<TaskResult> queue_unload(const eeng::BatchId& id, EngineContext& ctx);
        // std::shared_future<TaskResult> queue_reload(const eeng::BatchId& id);
#endif

        std::vector<const BatchInfo*> list() const;

    private:
#if 1
        // Steps (run by strand)
        TaskResult do_load(BatchInfo& B, EngineContext& ctx);
#endif
#if 1
        TaskResult do_unload(BatchInfo& B, EngineContext& ctx);
#endif

        // Helpers (main-thread work)
#if 0
        void spawn_entities_on_main(BatchInfo& B, EngineContext& ctx);   // Step 1 (create/populate)
#endif
#if 0
        void despawn_entities_on_main(BatchInfo& B, EngineContext& ctx); // Step last (cleanup)
#endif

    private:
        // eeng::EngineContext& ctx_;
        // eeng::SerialExecutor strand_;
        std::optional<SerialExecutor>   strand_;   // lazy initialization
        mutable std::mutex              strand_mutex_;
        SerialExecutor& strand(EngineContext& ctx);   // helper

        // registry storage
        mutable std::mutex                           mtx_;
        std::unordered_map<eeng::BatchId, BatchInfo> batches_;
        std::filesystem::path                        index_path_;
    };
} // namespace eeng