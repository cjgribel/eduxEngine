// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/Entity.hpp"
#include "IBatchRegistry.hpp"
//#include "ResourceManager.hpp"
#include "engineapi/IResourceManager.hpp"
#include "SerialExecutor.hpp"
#include "MetaSerialize.hpp"
#include "EngineContext.hpp"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <future>
#include <string>
#include <vector>

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
        bool                        generated = false;
        bool                        read_only = false;
        Guid                        owner_guid{};
        std::string                 generator_tag{};

        State       state{ State::Unloaded };
        TaskResult  last_result{};
    };

    struct BatchSnapshot
    {
        BatchInfo info{};
        nlohmann::json entities = nlohmann::json::array();
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

        static constexpr const char* kEditorBatchName = "editor";
        static constexpr const char* kDefaultBatchName = "default";

        void save_index(const std::filesystem::path& index_path);
        void load_or_create_index(const std::filesystem::path& index_path);
        void save_index();
        const std::filesystem::path& index_path() const noexcept { return index_path_; }

        Guid create_batch(
            // const BatchId& id,
            std::string name
            // const std::filesystem::path& path
        );
        Guid create_batch_with_id(const BatchId& id, std::string name);
        /// @brief Insert or update an unloaded batch record in the registry/index model.
        ///
        /// This is intended for tool-generated content such as cooked terrain
        /// chunk batches, where the batch JSON and the batch index entry are
        /// produced offline rather than by saving a currently loaded scene batch.
        bool upsert_batch_record(BatchInfo info);
        bool delete_batch(const BatchId& id, BatchInfo* out_info = nullptr);
        bool restore_batch(BatchInfo info);


        std::shared_future<TaskResult> queue_save_batch(const BatchId& id, EngineContext& ctx);

        bool snapshot_batch(const BatchId& id, EngineContext& ctx, BatchSnapshot& out_snapshot);
        std::vector<BatchSnapshot> snapshot_loaded_batches(EngineContext& ctx);
        TaskResult load_batch_from_snapshot(const BatchSnapshot& snapshot, EngineContext& ctx, bool skip_asset_load = false);
        TaskResult load_batches_from_snapshots(const std::vector<BatchSnapshot>& snapshots, EngineContext& ctx, bool skip_asset_load = false);

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

        std::shared_future<TaskResult> queue_unload_all_async(EngineContext& ctx) override;

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
        std::shared_future<ecs::EntityRef> queue_create_entity(const BatchId& id, const std::string& name, const ecs::EntityRef& parent, EngineContext& ctx);

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

// Returns a future that completes when the full sequence is done.
        std::shared_future<TaskResult>
            queue_load(const eeng::BatchId& id, EngineContext& ctx) override;
        std::shared_future<TaskResult>
            queue_unload(const eeng::BatchId& id, EngineContext& ctx) override;

        /// Recompute the asset closure for a loaded batch from its live entities,
        /// and adjust RM leases accordingly (load/bind new GUIDs, unbind/unload
        /// GUIDs that are no longer referenced). Runs on the BatchRegistry strand.
        std::shared_future<TaskResult>
            queue_rebuild_closure(const BatchId& id, EngineContext& ctx);

        /// Mark a loaded batch as needing asset-closure rebuild (coalesced).
        void mark_closure_dirty(const BatchId& id);

        /// Mark the owning batch (if any) for this entity as needing a rebuild.
        void mark_closure_dirty_for_entity(const ecs::Entity& entity, EngineContext& ctx);

        /// Queue rebuilds for dirty loaded batches (call from main loop).
        void process_dirty_batches(EngineContext& ctx);

        std::vector<const BatchInfo*> list() const;
        bool try_get_batch_info(const BatchId& id, BatchInfo& out_info) const;
        bool try_get_loaded_batch_for_entity(const ecs::EntityRef& entity_ref, BatchId& out_id) const override;
        bool try_get_batch_id_by_name(const std::string& name, BatchId& out_id) const override;
        bool is_batch_loaded(const BatchId& id) const override;
        bool is_batch_read_only(const BatchId& id) const override;

    private:

        // Steps (run by strand)
        TaskResult do_load(BatchInfo& B, EngineContext& ctx);
        TaskResult do_unload(BatchInfo& B, EngineContext& ctx);

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
        std::unordered_set<eeng::BatchId>           dirty_batches_;
        std::unordered_map<Guid, BatchId>           entity_to_batch_;
        std::filesystem::path                        index_path_;
    };
} // namespace eeng
