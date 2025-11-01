// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/Entity.hpp"
#include "IBatchRegistry.hpp"
//#include "ResourceManager.hpp"
#include "engineapi/IResourceManager.hpp"
#include "SerialExecutor.hpp"
#include "EngineContext.hpp"
#include <unordered_map>
#include <mutex>
#include <future>
#include <string>

#pragma once

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
        using State = enum { Idle, Queued, Loading, Bound, Unloading, Error };

        BatchId id{};
        std::string name{};
        // std::vector<Guid> closure;          // deduped closure of all asset GUIDs in the batch
        std::vector<ecs::EntityRef> entities;    // entities associated with this batch

        State state{ State::Idle };
        // int last_error_count = 0;
        TaskResult last_result{};
    };

    class BatchRegistry : public IBatchRegistry
    {
    public:
        BatchRegistry() = default;
        ~BatchRegistry() = default;

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
        void add_entity(const eeng::BatchId& id, const ecs::EntityRef& er);
        // void set_entities(const eeng::BatchId&, std::vector<ecs::EntityRef> ents);

        // For UI
        BatchInfo::State state(const eeng::BatchId&) const;
        TaskResult last_result(const eeng::BatchId&) const;

        // --- Orchestration (serialized via strand) ---
#if 1
// Returns a future that completes when the full sequence is done.
        std::shared_future<TaskResult> queue_load(const eeng::BatchId& id, EngineContext& ctx);
#endif
#if 1
        std::shared_future<TaskResult> queue_unload(const eeng::BatchId& id, EngineContext& ctx);
        // std::shared_future<TaskResult> queue_reload(const eeng::BatchId& id);
#endif

    private:
#if 1
        // Steps (run by strand)
        TaskResult do_load(BatchInfo& B, EngineContext& ctx);
#endif
#if 1
        TaskResult do_unload(BatchInfo& B, EngineContext& ctx);
#endif

        // Helpers (main-thread work)
#if 1
        void spawn_entities_on_main(BatchInfo& B, EngineContext& ctx);   // Step 1 (create/populate)
#endif
#if 1
        void despawn_entities_on_main(BatchInfo& B, EngineContext& ctx); // Step last (cleanup)
#endif

    private:
        // eeng::EngineContext& ctx_;
        // eeng::SerialExecutor strand_;
        std::optional<SerialExecutor>   strand_;   // lazy initialization
        mutable std::mutex              strand_mutex_;
        SerialExecutor& strand(EngineContext& ctx);   // helper

        // registry storage
        mutable std::mutex mtx_;
        std::unordered_map<eeng::BatchId, BatchInfo> batches_;
    };
} // namespace eeng