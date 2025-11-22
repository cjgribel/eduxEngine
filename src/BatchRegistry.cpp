#include "BatchRegistry.hpp"
#include "MainThreadQueue.hpp"
#include "ThreadPool.hpp"
#include "MetaSerialize.hpp"
#include <fstream>

namespace eeng
{
    void BatchRegistry::save_index(const std::filesystem::path& index_path)
    {
        nlohmann::json j;
        j["batches"] = nlohmann::json::array();

        {
            std::lock_guard lk(mtx_);
            for (auto& [_, b] : batches_)
            {
                // serialize asset_closure_hdr to json array
                nlohmann::json closure_json = nlohmann::json::array();
                for (const auto& guid : b.asset_closure_hdr)
                    closure_json.push_back(guid.to_string());

                j["batches"].push_back({
                    { "id", b.id.to_string() },
                    { "name", b.name },
                    { "asset_closure_hdr", closure_json },
                    { "filename", b.filename.string() }
                    });
            }
        }

        std::ofstream f(index_path);
        f << j.dump(2);
    }

    void BatchRegistry::load_or_create_index(const std::filesystem::path& index_path)
    {
        std::ifstream f(index_path);
        if (!f.is_open())
        {
            // Create empty index
            {
                std::lock_guard lk(mtx_);
                batches_.clear();
            }
            save_index(index_path);
            index_path_ = index_path;
            return;
        }

        nlohmann::json j; f >> j;
        std::lock_guard lk(mtx_);
        batches_.clear();
        for (auto& b : j["batches"])
        {
            BatchInfo bi;
            bi.id = Guid::from_string(b["id"].get<std::string>());
            bi.name = b.value("name", bi.id.to_string());
            bi.filename = b["filename"].get<std::string>();

            if (b.contains("asset_closure_hdr"))
            {
                for (auto& gstr : b["asset_closure_hdr"])
                {
                    bi.asset_closure_hdr.push_back(
                        Guid::from_string(gstr.get<std::string>())
                    );
                }
            }

            batches_.emplace(bi.id, std::move(bi));
        }
        index_path_ = index_path;
    }

    Guid BatchRegistry::create_batch(
        // const BatchId& id,
        std::string name
        // const std::filesystem::path& path
    )
    {
        std::lock_guard lk(mtx_);
        if (index_path_.empty()) return Guid::invalid();
        // if (batches_.find(id) != batches_.end()) return false;
        BatchInfo bi;
        bi.id = Guid::generate();
        bi.name = std::move(name);
        bi.filename = bi.id.to_string() + ".json";
        batches_.emplace(bi.id, std::move(bi));
        return bi.id;
    }

    // Later possibly: save batch via task queue
#if 1
    std::shared_future<TaskResult>
        BatchRegistry::queue_save_batch(const BatchId& id, EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, &ctx]() -> TaskResult {
            TaskResult tr{};
            tr.success = this->save_batch(id, ctx);
            return tr;
            });
    }
#endif

    bool BatchRegistry::save_batch(const eeng::BatchId& id, EngineContext& ctx)
    {
        // 1) Snapshot BatchInfo (and enforce "Loaded" state)
        BatchInfo snapshot;
        {
            std::lock_guard lk(mtx_);
            auto it = batches_.find(id);
            if (it == batches_.end())
            {
                // unknown batch – assert/log?
                return false;
            }

            if (it->second.state != BatchInfo::State::Loaded)
            {
                // Batch is not loaded: for now, just refuse to save
                return false;
            }

            snapshot = it->second; // shallow copy (id, name, filename, asset_closure_hdr, live)
        }

        // 2) Main-thread: build entities JSON (later – can be empty for now)
        nlohmann::json entities_json = nlohmann::json::array();

        // Prel. entity data; can skip for now
        entities_json = ctx.main_thread_queue->push_and_wait([&]() {
            nlohmann::json arr = nlohmann::json::array();

            //auto& reg = ctx.entity_manager->registry();
            // If you have a shared_ptr<entt::registry>, adapt this call accordingly
            auto registry_sptr = ctx.entity_manager->registry_wptr().lock();
            if (!registry_sptr) return nlohmann::json::array();

            for (const auto& er : snapshot.live)
            {
                if (!er.has_entity()) continue;

                // existing meta serializer:
                //   nlohmann::json ejson = eeng::meta::serialize_entity(er, ctx.entity_manager->registry_ptr());
                // If only reference, adjust serialize_entity’s signature or wrap it.


                nlohmann::json ejson = eeng::meta::serialize_entity(
                    er,
                    registry_sptr  // <- change to your actual API
                );
                arr.push_back(std::move(ejson));
            }

            return arr;
            });

        // 3) Build final batch JSON
        nlohmann::json j;
        j["header"] = nlohmann::json{
            { "id",   snapshot.id.to_string() },
            { "name", snapshot.name }
        };

        nlohmann::json closure = nlohmann::json::array();
        for (const auto& g : snapshot.asset_closure_hdr)
            closure.push_back(g.to_string());
        j["header"]["asset_closure"] = std::move(closure);

        j["entities"] = std::move(entities_json);

        // 4) Write file
        auto batch_path = index_path_.parent_path() / snapshot.filename;
        std::ofstream f(batch_path);
        if (!f.is_open())
        {
            // TODO: log error
            return false;
        }
        f << j.dump(2);
        return true;
    }

    std::shared_future<TaskResult>
        BatchRegistry::queue_load_all_async(EngineContext& ctx)
    {
        // 1) Snapshot ids under lock
        std::vector<BatchId> ids;
        {
            std::lock_guard lk(mtx_);
            ids.reserve(batches_.size());
            for (auto& [id, _] : batches_)
                ids.push_back(id);
        }

        // 2) Kick a worker that will do the waiting
        auto fut = ctx.thread_pool->queue_task([this, ids = std::move(ids), &ctx]() mutable -> TaskResult
            {
                TaskResult merged{};
                merged.success = true;

                // Optional: collect futures first, then wait in a second loop
                std::vector<std::shared_future<TaskResult>> batch_futs;
                batch_futs.reserve(ids.size());

                for (const auto& id : ids)
                {
                    batch_futs.emplace_back(queue_load(id, ctx));  // schedules on strand
                }

                for (auto& f : batch_futs)
                {
                    TaskResult r = f.get();             // wait for each batch to finish
                    merged.success &= r.success;
                    // Can merge sub-results here as well
                }

                return merged;
            });

        // Wrap in shared_future so caller can copy it around
        return fut.share();
    }

    std::shared_future<TaskResult>
        BatchRegistry::queue_save_all_async(EngineContext& ctx)
    {
        // snapshot ids of Loaded batches
        std::vector<BatchId> ids;
        {
            std::lock_guard lk(mtx_);
            for (auto& [id, b] : batches_)
                if (b.state == BatchInfo::State::Loaded)
                    ids.push_back(id);
        }

        auto fut = ctx.thread_pool->queue_task([this, ids = std::move(ids), &ctx]() mutable -> TaskResult
            {
                TaskResult merged{};
                merged.success = true;

                for (const auto& id : ids)
                {
                    auto f = queue_save_batch(id, ctx);
                    TaskResult r = f.get();
                    merged.success &= r.success;
                }

                return merged;
            });

        return fut.share();
    }


#if 0
    // -----------------------------------------------------------
    // Add one entity to a loaded batch (thread safe)
    // -----------------------------------------------------------
    std::shared_future<ecs::EntityRef>
        BatchRegistry::queue_add_entity(const BatchId& id, EntitySpawnDesc desc, EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, desc = std::move(desc), &ctx]() mutable -> ecs::EntityRef
            {
                BatchInfo* B;
                {
                    std::lock_guard lk(mtx_);
                    auto it = batches_.find(id);
                    if (it == batches_.end() || it->second.state != BatchInfo::State::Bound)
                        return ecs::EntityRef{}; // require loaded batch
                    B = &it->second;
                }

                // Do entt work on main thread
                ecs::EntityRef created = ctx.main_thread_queue->push_and_wait([&]() -> ecs::EntityRef
                    {
                        return instantiate_one_on_main(desc, *B, ctx);
                    });

                // Update asset closure via your visitor
                visit_asset_refs(created.get_entity(), ctx.entity_manager->registry(),
                    [&](const Guid& guid)
                    {
                        std::lock_guard lk(mtx_);
                        auto& closure = B->asset_closure_hdr;
                        if (std::find(closure.begin(), closure.end(), guid) == closure.end())
                            closure.push_back(guid);
                    });

                return created;
            });
}
#endif

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

    // void BatchRegistry::add_entity(const eeng::BatchId& id, const ecs::EntityRef& er)
    // {
    //     // TODO: Compute asset closures
    //     // TODO: Compute entity closure ??? Or, treat entity refs as soft?
    //     // TODO: Dedup entities

    //     std::lock_guard lk(mtx_);
    //     auto& b = batches_[id];
    //     b.id = id;
    //     b.entities.push_back(er);
    // }

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

    // BatchInfo::State BatchRegistry::state(const BatchId& id) const
    // {
    //     std::lock_guard lk(mtx_);
    //     auto it = batches_.find(id);
    //     return it == batches_.end() ? BatchInfo::Idle : it->second.state;
    // }

    // TaskResult BatchRegistry::last_result(const BatchId& id) const
    // {
    //     std::lock_guard lk(mtx_);
    //     auto it = batches_.find(id);
    //     if (it == batches_.end()) return TaskResult{};
    //     return it->second.last_result;
    // }

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
                B->state = BatchInfo::State::Queued;
            }
            auto res = do_load(*B, ctx);
            {
                std::lock_guard lk(mtx_);
                B->last_result = res;
                // B->last_error_count = int(std::count_if(B->last_result.results.begin(),
                //     B->last_result.results.end(),
                //     [](auto& r) { return !r.success; }));
                B->state = (res.success ? BatchInfo::State::Loaded : BatchInfo::State::Error);
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
                B->state = (res.success ? BatchInfo::State::Unloaded : BatchInfo::State::Error);
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
        // -- Mark batch as Loading
        B.state = BatchInfo::State::Loading;

        TaskResult res{};
        res.success = true;

        auto batch_path = index_path_.parent_path() / B.filename;
        std::ifstream f(batch_path);

        nlohmann::json batch_json;

        if (!f.is_open())
        {
            // No file yet → treat as brand new empty batch.
            // No assets, no entities – but this is NOT an error.
            B.asset_closure_hdr.clear();
            B.live.clear();
            res.success = true;
            return res;
        }

        f >> batch_json;

        // parse asset_closure from header if present
        if (batch_json.contains("header") &&
            batch_json["header"].contains("asset_closure"))
        {
            B.asset_closure_hdr.clear();
            for (auto& gstr : batch_json["header"]["asset_closure"])
                B.asset_closure_hdr.push_back(Guid::from_string(gstr.get<std::string>()));
        }

        // parse entities
        std::vector<eeng::meta::EntitySpawnDesc> entity_descs;
        if (batch_json.contains("entities"))
        {
            for (auto& ent_json : batch_json["entities"])
                entity_descs.push_back(eeng::meta::create_entity_spawn_desc(ent_json));
        }

        // 1) load assets (if any)
        if (!B.asset_closure_hdr.empty())
        {
            res = ctx.resource_manager->load_and_bind_async(
                std::deque<Guid>(B.asset_closure_hdr.begin(), B.asset_closure_hdr.end()),
                B.id,
                ctx
            ).get();
            if (!res.success)
                return res;
        }

        // 2) spawn entities on main (if any)
        ctx.main_thread_queue->push_and_wait([&]()
            {
                B.live.clear();
                for (const auto& desc : entity_descs)
                {
                    auto er = eeng::meta::spawn_entity_from_desc(desc, ctx);
                    B.live.push_back(er);
                }
            });

        // ABORT IF ASSET LOAD FAILED ???

        // --- Main thread: Resolve AssetRef fields by looking up handles from RM
        //
        // Resolve AssetRef<T> fields
        // visit_entity_refs(plan, [&](const Guid& guid, ecs::EntityRef& er) {
            //     // lookup entity in global guid→entity map
            //     auto it = global_guid_entity_map.find(guid);
            //     if (it != global_guid_entity_map.end()) {
                //         er.set_entity(it->second);
                //     }
                // });

        // --- Main thread: Resolve EntityRef fields (soft) using guid→entity map in EntityManager
        //
        // 6. MT phase 3: resolve EntityRef fields (soft)
        //    - for each EntityRef (guid, entt::entity):
        //        if guid is in the global guid→entity map → set .entity
        //        else leave .entity invalid
        // 7. MT?: run any "finalize" / "on_loaded" step for components that need GPU/init
        // 8. mark batch as Bound / Loaded and store TaskResult

        // ....

        // -- Main thread: Initialize GPU resources
        // finalize_gpu(B);

        return res;
    }

#endif
#if 1
    TaskResult BatchRegistry::do_unload(BatchInfo& B, EngineContext& ctx)
    {
        B.state = BatchInfo::State::Unloading;

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
        if (!strand_)
        {
            // thread_pool is owned by EngineContext; we just borrow it
            strand_.emplace(*ctx.thread_pool);
    }
        return *strand_;
    }

    // -------- Main-thread helpers (sketches) --------

#if 0
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
        ctx.main_thread_queue->push_and_wait([&, id = B.id]()
            {
                for (auto& er : batches_.at(id).live)
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