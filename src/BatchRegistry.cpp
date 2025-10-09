#include "BatchRegistry.hpp"

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
    //     // Dedup (optional) – you said you already guarantee closure
    //     std::lock_guard lk(mtx_);
    //     auto& b = batches_[id];
    //     b.id = id;
    //     b.closure = std::move(closure);
    // }

    void BatchRegistry::add_entity(const eeng::BatchId& id, const ecs::EntityRef& er) {
        std::lock_guard lk(mtx_);
        auto& b = batches_[id];
        b.id = id;
        b.entities.push_back(er);
    }

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

#if 0
    std::shared_future<TaskResult> BatchRegistry::queue_load(
        const BatchId& id,
        EngineContext& ctx)
    {
        return strand(ctx).post([this, id]() -> TaskResult {
            // locked only to fetch mut ref
            BatchInfo* B;
            {
                std::lock_guard lk(mtx_);
                B = &batches_.at(id);
                B->state = BatchInfo::Queued;
            }
            auto res = do_load(*B);
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

    std::shared_future<TaskResult> BatchRegistry::queue_unload(
        const BatchId& id,
        EngineContext& ctx)
    {
        return strand(ctx).post([this, id]() -> TaskResult {
            BatchInfo* B;
            {
                std::lock_guard lk(mtx_);
                B = &batches_.at(id);
            }
            auto res = do_unload(*B);
            {
                std::lock_guard lk(mtx_);
                B->last_result = res;
                B->last_error_count = int(std::count_if(B->last_result.results.begin(),
                    B->last_result.results.end(),
                    [](auto& r) { return !r.success; }));
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

#if 0
    TaskResult BatchRegistry::do_load(BatchInfo& B)
    {
        B.state = BatchInfo::Loading;

        // 1) Main-thread: spawn/populate entities for this batch
        // NOTE: do this last, once assets are loaded: entities should be "born completed"
        spawn_entities_on_main(B);
        //
        // main.push_and_wait([&]{ spawn_entities(B); });

        // 2) Ask RM to load/bind assets for the closure
        auto fut = ctx_.resource_manager->load_and_bind_async(
            std::deque<Guid>(B.closure.begin(), B.closure.end()),
            B.id, ctx_
        );
        TaskResult res = fut.get(); // serialized here; assets load in parallel inside RM
        //
        //auto loadRes = rm.load_and_bind_async(deque<Guid>(B.closure.begin(), B.closure.end()), B.id, ctx).get();

        // 3) MT: component GUID→Handle binding
        //main.push_and_wait([&] { bind_components_to_handles(B, rm.storage()); });
        // i.e.
        // main.push_and_wait([&] {
        //     for (auto& er : B.entities) {
        //         // For each component with AssetRef<T>:
        //         // ref.handle = storage.handle_for_guid<T>(ref.guid).value_or(Handle<T>{});
        //         // If missing: leave handle empty; set component.ready=false.
        //     }
        //     });

        // 4) Main-thread: finalize components / upload GPU if needed
        //    (You can skip here if you bind handles during component creation.)
        // ctx_.main_thread_queue->push([]{ ... });
        //
        //main.push_and_wait([&]{ finalize_gpu(B); });

        return res;
    }

    TaskResult BatchRegistry::do_unload(BatchInfo& B) {
        B.state = BatchInfo::Unloading;

        // 1) Main-thread: unhook components, despawn entities
        despawn_entities_on_main(B);
        //
        // main.push_and_wait([&]{ unbind_and_despawn(B); });

        // 2) Tell RM to unbind/unload assets (RM will only unload GUIDs no longer held)
        auto fut = ctx_.resource_manager->unbind_and_unload_async(
            std::deque<Guid>(B.closure.begin(), B.closure.end()),
            B.id, ctx_
        );
        TaskResult res = fut.get();
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

#if 0
    void BatchRegistry::spawn_entities_on_main(BatchInfo& B, EngineContext& ctx) {
        // If your MainThreadQueue runs immediately on the main thread tick,
        // you can either push and block until processed, or just call directly if you’re already on main.
        ctx.main_thread_queue->push([&, id = B.id]() {
            // Example: for each EntityRef, if entity is null, create and attach components
            // using the batch’s closure-driven data (your own format).
            for (auto& er : batches_.at(id).entities) {
                if (!er.entity.valid()) {
                    er.entity = ctx.entity_manager->create();
                    // populate components from your level JSON, etc.
                }
            }
            });

        // wait until the work is actually done if you need strict sequencing:
        // (Provide a barrier or future-returning push in your MainThreadQueue.)
    }

    void BatchRegistry::despawn_entities_on_main(BatchInfo& B, EngineContext& ctx) {
        ctx.main_thread_queue->push([&, id = B.id]() {
            for (auto& er : batches_.at(id).entities) {
                if (er.entity.valid()) {
                    ctx_.entity_manager->destroy(er.entity);
                    er.entity = {}; // null it
                }
            }
            });
    }
#endif
} // namespace eeng