#include "BatchRegistry.hpp"
#include "MainThreadQueue.hpp"

namespace eeng
{

    // BatchInfo& BatchRegistry::ensure(const BatchId& id, std::string name) {
    //     std::lock_guard lk(mtx_);
    //     auto& bi = batches_[id];
    //     bi.id = id;
    //     if (!name.empty()) bi.name = std::move(name);
    //     return bi;
    // }

    // const BatchInfo* BatchRegistry::get(const BatchId& id) const {
    //     std::lock_guard lk(mtx_);
    //     auto it = batches_.find(id);
    //     return it == batches_.end() ? nullptr : &it->second;
    // }

    // std::vector<const BatchInfo*> BatchRegistry::list() const {
    //     std::lock_guard lk(mtx_);
    //     std::vector<const BatchInfo*> out;
    //     out.reserve(batches_.size());
    //     for (auto& [_, b] : batches_) out.push_back(&b);
    //     return out;
    // }

    // void BatchRegistry::set_closure(const BatchId& id, std::vector<Guid> closure) {
    //     // Dedup (optional) – already guarantee closure?
    //     std::lock_guard lk(mtx_);
    //     auto& b = batches_[id];
    //     b.id = id;
    //     b.closure = std::move(closure);
    // }

    void BatchRegistry::add_entity(const eeng::BatchId& id, const ecs::EntityRef& er)
    {
        // TODO: Compute asset closures
        // TODO: Compute entity closure ??? Or, treat entity refs as soft?
        // TODO: Dedup entities

        std::lock_guard lk(mtx_);
        auto& b = batches_[id];
        b.id = id;
        b.entities.push_back(er);
    }

    // TODO: visit_asset_refs, visit_entity_refs
    //       Note: These can do other stuff, such as adding names or other props (see entt)
    //             entt-style -> move this info to a central registry
#if 0
    void BatchRegistry::update_closures()
    {
        // std::lock_guard lk(mtx_);
        // for (auto& [_, b] : batches_) {
        //     std::unordered_set<Guid> closure_set;
        //     for (const auto& er : b.entities) {
        //         // Compute closure for each entity reference
        //         const auto& entity = ctx->entity_manager->get_entity(er);
        //         if (entity) {
        //             // TODO: Compute asset closure for the entity
        //         }
        //     }
        // }
    }
#endif

    // void BatchRegistry::set_entities(const BatchId& id, std::vector<EntityRef> ents) {
    //     std::lock_guard lk(mtx_);
    //     auto& b = batches_[id];
    //     b.id = id;
    //     b.entities = std::move(ents);
    // }

    BatchInfo::State BatchRegistry::state(const BatchId& id) const
    {
        std::lock_guard lk(mtx_);
        auto it = batches_.find(id);
        return it == batches_.end() ? BatchInfo::Idle : it->second.state;
    }

    TaskResult BatchRegistry::last_result(const BatchId& id) const
    {
        std::lock_guard lk(mtx_);
        auto it = batches_.find(id);
        if (it == batches_.end()) return TaskResult{};
        return it->second.last_result;
    }

    // -------- Orchestration (serialized) --------

#if 1
    std::shared_future<TaskResult> BatchRegistry::queue_load(
        const BatchId& id,
        EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, &ctx]() -> TaskResult {
            // locked only to fetch mut ref
            BatchInfo* B;
            {
                std::lock_guard lk(mtx_);
                B = &batches_.at(id);
                B->state = BatchInfo::Queued;
            }
            auto res = do_load(*B, ctx);
            {
                std::lock_guard lk(mtx_);
                B->last_result = res;
                // B->last_error_count = int(std::count_if(B->last_result.results.begin(),
                //     B->last_result.results.end(),
                //     [](auto& r) { return !r.success; }));
                B->state = (res.success ? BatchInfo::Bound : BatchInfo::Error);
            }
            return res;
            });
    }
#endif
#if 1
    std::shared_future<TaskResult> BatchRegistry::queue_unload(
        const BatchId& id,
        EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, &ctx]() -> TaskResult {
            BatchInfo* B;
            {
                std::lock_guard lk(mtx_);
                B = &batches_.at(id);
            }
            auto res = do_unload(*B, ctx);
            {
                std::lock_guard lk(mtx_);
                B->last_result = res;
                // B->last_error_count = int(std::count_if(B->last_result.results.begin(),
                //     B->last_result.results.end(),
                //     [](auto& r) { return !r.success; }));
                B->state = (res.success ? BatchInfo::Idle : BatchInfo::Error);
            }
            return res;
            });
    }

    // std::shared_future<TaskResult> BatchRegistry::queue_reload(const BatchId& id) {
    //     return strand_.submit([this, id]() -> TaskResult {
    //         BatchInfo* B;
    //         {
    //             std::lock_guard lk(mtx_);
    //             B = &batches_.at(id);
    //         }
    //         // Simple “unload then load” in the strand (RM still does asset-level parallelism)
    //         TaskResult merged; merged.type = TaskResult::TaskType::Reload;
    //         {
    //             auto r1 = do_unload(*B);
    //             merged.results.insert(merged.results.end(), r1.results.begin(), r1.results.end());
    //             merged.success &= r1.success;
    //         }
    //         {
    //             auto r2 = do_load(*B);
    //             merged.results.insert(merged.results.end(), r2.results.begin(), r2.results.end());
    //             merged.success &= r2.success;
    //         }
    //         {
    //             std::lock_guard lk(mtx_);
    //             B->last_result = merged;
    //             B->last_error_count = int(std::count_if(B->last_result.results.begin(),
    //                                                     B->last_result.results.end(),
    //                                                     [](auto& r){ return !r.success; }));
    //             B->state = (merged.success ? BatchInfo::Bound : BatchInfo::Error);
    //         }
    //         return merged;
    //     });
    // }
#endif

// -------- Steps --------

#if 1
    TaskResult BatchRegistry::do_load(BatchInfo& B, EngineContext& ctx)
    {
        B.state = BatchInfo::Loading;

        // 1) (optionally) deserialize JSON for this batch here if not already done
        //    so we have the list of entity descriptions ready

        // 2) load/bind assets for the precomputed closure
        TaskResult res{};
#if 0 // Don't have closures yet
        auto res = ctx.resource_manager
            ->load_and_bind_async(std::deque<Guid>(B.closure.begin(), B.closure.end()),
                B.id, ctx)
            .get(); // ok to be serialized here; the RM fans out internally
#endif

        // 3) main-thread: create entt entities for this batch (phase 1)
        spawn_entities_on_main(B, ctx); // just creates + registers guid→entity

        // 4) main-thread: populate components, bind AssetRef<T>, resolve EntityRef (phase 2)
        //    (can be folded into spawn_entities_on_main)
        //    bind_components_to_handles(B, ctx.resource_manager->storage());
        //    resolve_entity_refs(B, global_guid_entity_map);

        // 5) finalize GPU / post-load hooks if needed
        // finalize_gpu(B);

        return res;
    }

#endif
#if 1
    TaskResult BatchRegistry::do_unload(BatchInfo& B, EngineContext& ctx)
    {
        B.state = BatchInfo::Unloading;

        // 1) Main-thread: unhook components, despawn entities
        despawn_entities_on_main(B, ctx);
        //
        // main.push_and_wait([&]{ unbind_and_despawn(B); });

        // 2) Tell RM to unbind/unload assets (RM will only unload GUIDs no longer held)
        TaskResult res{};
        // auto fut = ctx_.resource_manager->unbind_and_unload_async(
        //     std::deque<Guid>(B.closure.begin(), B.closure.end()),
        //     B.id, ctx_
        // );
        // TaskResult res = fut.get();
        //
        // auto res = rm.unbind_and_unload_async(deque<Guid>(B.closure.begin(), B.closure.end()), B.id, ctx).get();

        return res;
    }
#endif

    SerialExecutor& BatchRegistry::strand(EngineContext& ctx)
    {
        std::scoped_lock lk(strand_mutex_);
        if (!strand_) {
            // thread_pool is owned by EngineContext; we just borrow it
            // --> FIX & USE strand_.emplace(*ctx.thread_pool);
        }
        return *strand_;
    }

    // -------- Main-thread helpers (sketches) --------

#if 1
    void BatchRegistry::spawn_entities_on_main(BatchInfo& B, EngineContext& ctx)
    {
        // If MainThreadQueue runs immediately on the main thread tick,
        // can either push and block until processed, or just call directly if already on main.
        ctx.main_thread_queue->push([&, id = B.id]()
            {
                // Example: for each EntityRef, if entity is null, create and attach components
                // using the batch’s closure-driven data.
                for (auto& er : batches_.at(id).entities)
                {
                    if (!er.has_entity()) // is it an error if entity already exists?
                    {
                        er.set_entity(ctx.entity_manager->create_entity("default_chunk", "entity", ecs::Entity::EntityNull, ecs::Entity::EntityNull));
                        // ... add components ...

                        // populate components from level JSON, etc.
                    }
                }
            });

        // wait until the work is actually done if strict sequencing is needed:
        // (Provide a barrier or future-returning push in your MainThreadQueue.)
    }
#endif
#if 1
    void BatchRegistry::despawn_entities_on_main(BatchInfo& B, EngineContext& ctx)
    {
        ctx.main_thread_queue->push([&, id = B.id]()
            {
                for (auto& er : batches_.at(id).entities)
                {
                    if (er.has_entity()) // <- is it an error if entity is already null?
                    {
                        ctx.entity_manager->queue_entity_for_destruction(er.get_entity());
                        er.clear_entity();
                    }
                }
            });
    }
#endif
} // namespace eeng