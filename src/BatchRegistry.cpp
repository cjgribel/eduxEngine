// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "BatchRegistry.hpp"
#include "meta/EntityMetaHelpers.hpp"
#include "MainThreadQueue.hpp"
#include "EventQueue.h"
#include "ThreadPool.hpp"
#include <fstream>

namespace eeng::detail
{
    struct ClosureBuildResult
    {
        TaskResult tr{};
        std::vector<Guid> closure{};
        std::vector<Guid> loaded_now{}; // for rollback on failure
    };

    static ClosureBuildResult build_asset_closure_recursive(
        const std::vector<Guid>& roots,
        const std::vector<Guid>& already_in_closure,
        const BatchId& batch_id,
        EngineContext& ctx)
    {
        ClosureBuildResult out{};
        out.tr.success = true;

        auto& rm = *ctx.resource_manager;

        std::unordered_set<Guid> seen;
        seen.reserve(roots.size() * 2);

        std::unordered_set<Guid> already;
        already.reserve(already_in_closure.size() * 2);
        for (const auto& g : already_in_closure)
            already.insert(g);

        std::vector<Guid> frontier;
        frontier.reserve(roots.size());

        for (const auto& g : roots)
        {
            if (!g.valid())
                continue;

            if (seen.insert(g).second)
            {
                out.closure.push_back(g);
                frontier.push_back(g);
            }
        }

        while (!frontier.empty() && out.tr.success)
        {
            // 1) Load/bind everything in this frontier that is not already in the batch closure.
            std::vector<Guid> to_load;
            to_load.reserve(frontier.size());

            for (const auto& g : frontier)
            {
                if (already.find(g) == already.end())
                    to_load.push_back(g);
            }

            if (!to_load.empty())
            {
                std::deque<Guid> dq(to_load.begin(), to_load.end());
                auto r = rm.load_and_bind_async(dq, batch_id, ctx).get();
                out.tr.success = out.tr.success && r.success;

                if (!r.success)
                {
                    // Track what was attempted/added, so rollback can be selective if desired.
                    out.loaded_now.insert(out.loaded_now.end(), to_load.begin(), to_load.end());
                    break;
                }

                out.loaded_now.insert(out.loaded_now.end(), to_load.begin(), to_load.end());
                for (const auto& g : to_load)
                    already.insert(g);
            }

            // 2) Expand: for each GUID in this frontier, add its referenced GUIDs.
            std::vector<Guid> next;
            for (const auto& g : frontier)
            {
                // ResourceManager helper from section (2)
                // auto refs = static_cast<eeng::ResourceManager&>(rm).collect_referenced_asset_guids(g);
                auto refs = meta::collect_referenced_asset_guids(g, ctx);

                for (const auto& child : refs)
                {
                    if (!child.valid())
                        continue;

                    if (seen.insert(child).second)
                    {
                        out.closure.push_back(child);
                        next.push_back(child);
                    }
                }
            }

            frontier.swap(next);
        }

        std::sort(out.closure.begin(), out.closure.end());
        out.closure.erase(std::unique(out.closure.begin(), out.closure.end()), out.closure.end());
        return out;
    }
}

namespace eeng::detail
{
    template<typename T>
    void append_new_elements(
        std::vector<T>& closure,
        const std::vector<T>& candidates,
        std::vector<T>& out_added)
    {
        for (const auto& g : candidates)
        {
            if (std::find(closure.begin(), closure.end(), g) == closure.end())
            {
                closure.push_back(g);
                out_added.push_back(g);
            }
        }
    }

    template<typename T>
    void diff_sets(
        const std::vector<T>& old_closure,
        const std::vector<T>& new_closure,
        std::vector<T>& out_to_add,
        std::vector<T>& out_to_remove)
    {
        auto sorted_old = old_closure;
        auto sorted_new = new_closure;

        std::sort(sorted_old.begin(), sorted_old.end());
        sorted_old.erase(std::unique(sorted_old.begin(), sorted_old.end()), sorted_old.end());

        std::sort(sorted_new.begin(), sorted_new.end());
        sorted_new.erase(std::unique(sorted_new.begin(), sorted_new.end()), sorted_new.end());

        std::set_difference(
            sorted_new.begin(), sorted_new.end(),
            sorted_old.begin(), sorted_old.end(),
            std::back_inserter(out_to_add));

        std::set_difference(
            sorted_old.begin(), sorted_old.end(),
            sorted_new.begin(), sorted_new.end(),
            std::back_inserter(out_to_remove));
    }
}

namespace
{
    void bind_refs_for_entities_on_main(
        const std::vector<eeng::ecs::EntityRef>& entities,
        eeng::EngineContext& ctx)
    {
        if (!ctx.entity_manager)
            return;
        auto reg_sp = ctx.entity_manager->registry_wptr().lock();
        if (!reg_sp)
            return;

        ctx.main_thread_queue->push_and_wait([&]()
            {
                for (const auto& er : entities)
                {
                    if (!er.is_bound())
                        continue;
                    eeng::meta::bind_asset_refs_for_entity(er.entity, ctx);
                    eeng::meta::bind_entity_refs_for_entity(er.entity, ctx);
                }
            });
    }

    void fill_batch_event_from_info(
        eeng::BatchTaskCompletedEvent& event,
        const eeng::BatchInfo& info)
    {
        event.batch_name = info.name;
        event.live_entities = info.live.size();
        event.asset_closure_size = info.asset_closure_hdr.size();
        if (event.batch_count == 0)
            event.batch_count = 1;
    }

    eeng::BatchTaskCompletedEvent make_batch_event(
        eeng::BatchTaskType type,
        bool success,
        const eeng::BatchId& batch_id = eeng::BatchId{})
    {
        eeng::BatchTaskCompletedEvent event{};
        event.type = type;
        event.batch_id = batch_id;
        event.success = success;
        return event;
    }

    void enqueue_batch_event(
        eeng::EngineContext& ctx,
        const eeng::BatchTaskCompletedEvent& event)
    {
        if (ctx.event_queue)
        {
            ctx.event_queue->enqueue_event(event);
        }
    }

    void enqueue_batch_event(
        eeng::EngineContext& ctx,
        eeng::BatchTaskType type,
        bool success,
        const eeng::BatchId& batch_id = eeng::BatchId{},
        size_t batch_count = 0)
    {
        auto event = make_batch_event(type, success, batch_id);
        event.batch_count = batch_count;
        enqueue_batch_event(ctx, event);
    }

    bool rebind_assets_in_closure(
        const std::vector<eeng::Guid>& closure,
        const eeng::BatchId& batch_id,
        eeng::EngineContext& ctx)
    {
        if (closure.empty())
            return true;

        std::deque<eeng::Guid> dq(closure.begin(), closure.end());
        auto tr = ctx.resource_manager->load_and_bind_async(
            std::move(dq),
            batch_id,
            ctx
        ).get();

        return tr.success;
    }
}

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

    std::shared_future<TaskResult>
        BatchRegistry::queue_save_batch(const BatchId& id, EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, &ctx]() -> TaskResult {
            TaskResult tr{};
            tr.success = this->save_batch(id, ctx);
            auto event = make_batch_event(BatchTaskType::Save, tr.success, id);
            {
                std::lock_guard lk(mtx_);
                auto it = batches_.find(id);
                if (it != batches_.end())
                    fill_batch_event_from_info(event, it->second);
            }
            enqueue_batch_event(ctx, event);
            return tr;
            });
    }

    // TODO -> update index file as well?
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

            auto registry_sptr = ctx.entity_manager->registry_wptr().lock();
            if (!registry_sptr) return nlohmann::json::array();

            for (const auto& er : snapshot.live)
            {
                if (!er.is_bound()) continue;

                // existing meta serializer:
                //   nlohmann::json ejson = eeng::meta::serialize_entity(er, ctx.entity_manager->registry_ptr());
                // If only reference, adjust serialize_entity’s signature or wrap it.


                nlohmann::json ejson = eeng::meta::serialize_entity(
                    er,
                    registry_sptr,  // <- change to your actual API
                    eeng::meta::SerializationPurpose::file
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

                enqueue_batch_event(ctx, BatchTaskType::LoadAll, merged.success, BatchId{}, ids.size());

                return merged;
            });

        // Wrap in shared_future so caller can copy it around
        return fut.share();
    }

    std::shared_future<TaskResult>
        BatchRegistry::queue_unload_all_async(EngineContext& ctx)
    {
        // Snapshot ids of batches that are not Unloaded
        std::vector<BatchId> ids;
        {
            std::lock_guard lk(mtx_);
            for (auto& [id, b] : batches_)
            {
                if (b.state != BatchInfo::State::Unloaded)
                    ids.push_back(id);
            }
        }

        auto fut = ctx.thread_pool->queue_task(
            [this, ids = std::move(ids), &ctx]() mutable -> TaskResult
            {
                TaskResult merged{};
                merged.success = true;

                std::vector<std::shared_future<TaskResult>> futs;
                futs.reserve(ids.size());

                for (const auto& id : ids)
                    futs.emplace_back(queue_unload(id, ctx));

                for (auto& f : futs)
                {
                    TaskResult r = f.get();
                    merged.success &= r.success;
                }

                enqueue_batch_event(ctx, BatchTaskType::UnloadAll, merged.success, BatchId{}, ids.size());

                return merged;
            });

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

                enqueue_batch_event(ctx, BatchTaskType::SaveAll, merged.success, BatchId{}, ids.size());

                return merged;
            });

        return fut.share();
    }

    std::shared_future<ecs::EntityRef>
        BatchRegistry::queue_create_entity(
            const BatchId& id,
            const std::string& name,
            const ecs::EntityRef& parent, // <- TODO just Entity?
            EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, name, parent, &ctx]() -> ecs::EntityRef
            {
                BatchTaskCompletedEvent event{};
                event.type = BatchTaskType::CreateEntity;
                event.batch_id = id;

                auto emit_and_return = [&](ecs::EntityRef er, const BatchInfo* info) -> ecs::EntityRef
                    {
                        event.success = er.is_bound();
                        if (info)
                            fill_batch_event_from_info(event, *info);
                        enqueue_batch_event(ctx, event);
                        return er;
                    };

                BatchInfo* B = nullptr;
                {
                    std::lock_guard lk(mtx_);
                    auto it = batches_.find(id);
                    if (it == batches_.end() || it->second.state != BatchInfo::State::Loaded)
                        return emit_and_return(ecs::EntityRef{ Guid::invalid() }, (it != batches_.end()) ? &it->second : nullptr);      // require loaded batch
                    B = &it->second;
                }

                // ecs::Entity parent_entity = parent.get_entity(); // may be null

                // MT: create entity with a HeaderComponent and proper registration
                ecs::EntityRef created = ctx.main_thread_queue->push_and_wait([&]() -> ecs::EntityRef
                    {
                        // Chunk tag = batch id string (or B->name)
                        std::string batch_tag = id.to_string();

                        auto& em = ctx.entity_manager;
                        auto [guid, entity] = em->create_entity(
                            batch_tag,          // REMOVE ?
                            name,
                            parent.entity,
                            ecs::Entity::EntityNull
                        );

                        return ecs::EntityRef{ guid, entity };
                    });

                // Register in batch live list
                {
                    std::lock_guard lk(mtx_);
                    B->live.push_back(created);
                }

                // No components yet → no asset refs yet → no closure update now

                return emit_and_return(created, B);
            });
    }

    std::shared_future<bool>
        BatchRegistry::queue_destroy_entity(
            const BatchId& id,
            ecs::EntityRef entity_ref,
            EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, entity_ref, &ctx]() -> bool
            {
                BatchTaskCompletedEvent event{};
                event.type = BatchTaskType::DestroyEntity;
                event.batch_id = id;

                auto emit_and_return = [&](bool ok, const BatchInfo* info) -> bool
                    {
                        event.success = ok;
                        if (info)
                            fill_batch_event_from_info(event, *info);
                        enqueue_batch_event(ctx, event);
                        return ok;
                    };

                BatchInfo* B = nullptr;
                {
                    std::lock_guard lk(mtx_);
                    auto it = batches_.find(id);
                    if (it == batches_.end() || it->second.state != BatchInfo::State::Loaded)
                        return emit_and_return(false, (it != batches_.end()) ? &it->second : nullptr);
                    B = &it->second;

                    // Remove from live list
                    auto& live = B->live;
                    live.erase(std::remove_if(live.begin(), live.end(),
                        [&](const ecs::EntityRef& er)
                        {
                            return er.guid == entity_ref.guid;
                        }),
                        live.end());
                }

                // MT: queue destroy
                ctx.main_thread_queue->push_and_wait([&]()
                    {
                        if (entity_ref.is_bound())
                        {
                            ctx.entity_manager->queue_entity_for_destruction(entity_ref.entity);
                        }
                    });

                // NOTE: we do NOT shrink asset_closure_hdr here.
                // Over-approx closure is fine; RM will ref-count correctly.
                return emit_and_return(true, B);
            });
    }

    std::shared_future<bool>
        BatchRegistry::queue_attach_entity(
            const BatchId& id,
            ecs::EntityRef entity_ref,
            EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, entity_ref, &ctx]() -> bool
            {
                BatchTaskCompletedEvent event{};
                event.type = BatchTaskType::AttachEntity;
                event.batch_id = id;

                auto emit_and_return = [&](bool ok, const BatchInfo* info) -> bool
                    {
                        event.success = ok;
                        if (info)
                            fill_batch_event_from_info(event, *info);
                        enqueue_batch_event(ctx, event);
                        return ok;
                    };

                BatchInfo* B = nullptr;
                {
                    std::lock_guard lk(mtx_);
                    auto it = batches_.find(id);
                    if (it == batches_.end() || it->second.state != BatchInfo::State::Loaded)
                        return emit_and_return(false, (it != batches_.end()) ? &it->second : nullptr);

                    B = &it->second;
                    B->live.push_back(entity_ref);
                }

                // If entity isn't live or registry is gone, nothing more to do.
                auto registry_sp = ctx.entity_manager->registry_wptr().lock();
                if (!registry_sp || !entity_ref.is_bound())
                    return emit_and_return(true, B);

                auto& reg = *registry_sp;

                // 1) MT: collect asset GUIDs from this entity's components
                std::vector<Guid> guids = ctx.main_thread_queue->push_and_wait([&]()
                    {
                        return eeng::meta::collect_asset_guids_for_entity(
                            entity_ref.entity,
                            reg
                        );
                    });

                if (!guids.empty())
                {
                    // 2) Compute which GUIDs are new to this batch’s closure
                    std::vector<Guid> to_add;
                    {
                        std::lock_guard lk(mtx_);
                        auto& closure = B->asset_closure_hdr;
                        eeng::detail::append_new_elements(closure, guids, to_add);
                    }

                    // 3) Ask RM to load/bind newly added assets (idempotent at RM level)
                    if (!to_add.empty())
                    {
                        std::deque<Guid> dq(to_add.begin(), to_add.end());
                        auto tr = ctx.resource_manager->load_and_bind_async(
                            std::move(dq),
                            B->id,
                            ctx
                        ).get();
                        (void)tr; // TODO -> fold into TaskResult
                    }
                }

                // 4) MT: bind AssetRef<> and EntityRef<> inside this entity’s components
                bind_refs_for_entities_on_main({ entity_ref }, ctx);

                return emit_and_return(true, B);
            });
    }

    std::shared_future<bool>
        BatchRegistry::queue_detach_entity(
            const BatchId& id,
            ecs::EntityRef entity_ref,
            EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, entity_ref, &ctx]() -> bool
            {
                BatchTaskCompletedEvent event{};
                event.type = BatchTaskType::DetachEntity;
                event.batch_id = id;

                auto emit_and_return = [&](bool ok, const BatchInfo* info) -> bool
                    {
                        event.success = ok;
                        if (info)
                            fill_batch_event_from_info(event, *info);
                        enqueue_batch_event(ctx, event);
                        return ok;
                    };

                std::lock_guard lk(mtx_);
                auto it = batches_.find(id);
                if (it == batches_.end() || it->second.state != BatchInfo::State::Loaded)
                    return emit_and_return(false, (it != batches_.end()) ? &it->second : nullptr);

                auto& live = it->second.live;
                auto old_size = live.size();
                live.erase(std::remove_if(live.begin(), live.end(),
                    [&](const ecs::EntityRef& er)
                    {
                        return er.guid == entity_ref.guid;
                    }),
                    live.end());
                return emit_and_return(live.size() != old_size, &it->second);
            });
    }

    std::shared_future<ecs::EntityRef>
        BatchRegistry::queue_spawn_entity(
            const BatchId& id,
            meta::EntitySpawnDesc desc,
            EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, desc = std::move(desc), &ctx]() mutable -> ecs::EntityRef
            {
                BatchTaskCompletedEvent event{};
                event.type = BatchTaskType::SpawnEntity;
                event.batch_id = id;

                auto emit_and_return = [&](ecs::EntityRef er, const BatchInfo* info) -> ecs::EntityRef
                    {
                        event.success = er.is_bound();
                        if (info)
                            fill_batch_event_from_info(event, *info);
                        enqueue_batch_event(ctx, event);
                        return er;
                    };

                BatchInfo* B = nullptr;
                {
                    std::lock_guard lk(mtx_);
                    auto it = batches_.find(id);
                    if (it == batches_.end() || it->second.state != BatchInfo::State::Loaded)
                        return emit_and_return(ecs::EntityRef{}, (it != batches_.end()) ? &it->second : nullptr); // require loaded batch
                    B = &it->second;
                }

                // 1) MT: spawn + register entity from desc
                ecs::EntityRef created = ctx.main_thread_queue->push_and_wait([&]() -> ecs::EntityRef
                    {
                        auto er = eeng::meta::spawn_entity_from_desc(
                            desc,
                            ctx,
                            eeng::meta::SerializationPurpose::file);
                        ctx.entity_manager->register_entity(er.entity);
                        return er;
                    });

                if (!created.is_bound())
                    return emit_and_return(created, B);

                {
                    std::lock_guard lk(mtx_);
                    B->live.push_back(created);
                }

                auto registry_sp = ctx.entity_manager->registry_wptr().lock();
                if (!registry_sp)
                    return emit_and_return(created, B);

                auto& reg = *registry_sp;

                // 2) MT: collect asset GUIDs from the spawned entity
                std::vector<Guid> guids = ctx.main_thread_queue->push_and_wait([&]()
                    {
                        return eeng::meta::collect_asset_guids_for_entity(
                            created.entity,
                            reg
                        );
                    });

                if (!guids.empty())
                {
                    std::vector<Guid> to_add;
                    {
                        std::lock_guard lk(mtx_);
                        auto& closure = B->asset_closure_hdr;
                        eeng::detail::append_new_elements(closure, guids, to_add);
                    }

                    if (!to_add.empty())
                    {
                        std::deque<Guid> dq(to_add.begin(), to_add.end());
                        auto tr = ctx.resource_manager->load_and_bind_async(
                            std::move(dq),
                            B->id,
                            ctx
                        ).get();
                        (void)tr;
                    }
                }

                // 3) MT: bind refs in the spawned entity’s components
                bind_refs_for_entities_on_main({ created }, ctx);

                return emit_and_return(created, B);
            });
    }


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
            BatchTaskCompletedEvent event{};
            event.type = BatchTaskType::Load;
            event.batch_id = id;
            {
                std::lock_guard lk(mtx_);
                B->last_result = res;
                // B->last_error_count = int(std::count_if(B->last_result.results.begin(),
                //     B->last_result.results.end(),
                //     [](auto& r) { return !r.success; }));
                B->state = (res.success ? BatchInfo::State::Loaded : BatchInfo::State::Error);
                fill_batch_event_from_info(event, *B);
            }
            event.success = res.success;
            enqueue_batch_event(ctx, event);
            return res;
            });
    }

    std::shared_future<TaskResult>
        BatchRegistry::queue_unload(const BatchId& id, EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, &ctx]() -> TaskResult
            {
                BatchInfo* B = nullptr;
                BatchTaskCompletedEvent event{};
                event.type = BatchTaskType::Unload;
                event.batch_id = id;
                {
                    std::lock_guard lk(mtx_);
                    auto it = batches_.find(id);
                    if (it == batches_.end())
                    {
                        TaskResult tr{};
                        tr.success = false;
                        event.success = false;
                        enqueue_batch_event(ctx, event);
                        return tr; // unknown batch
                    }
                    B = &it->second;

                    if (B->state == BatchInfo::State::Unloaded)
                    {
                        TaskResult tr{};
                        tr.success = true; // nothing to do
                        fill_batch_event_from_info(event, *B);
                        event.success = true;
                        enqueue_batch_event(ctx, event);
                        return tr;
                    }

                    B->state = BatchInfo::State::Queued;
                }

                auto res = do_unload(*B, ctx);

                {
                    std::lock_guard lk(mtx_);
                    B->last_result = res;
                    B->state = (res.success ? BatchInfo::State::Unloaded
                        : BatchInfo::State::Error);
                    fill_batch_event_from_info(event, *B);
                }

                event.success = res.success;
                enqueue_batch_event(ctx, event);
                return res;
            });
    }

    std::vector<const BatchInfo*> BatchRegistry::list() const
    {
        std::lock_guard lk(mtx_);
        std::vector<const BatchInfo*> out;
        out.reserve(batches_.size());
        for (auto& [_, b] : batches_)
        {
            out.push_back(&b);
        }
        return out;
    }

    std::shared_future<TaskResult>
        BatchRegistry::queue_rebuild_closure(const BatchId& id, EngineContext& ctx)
    {
        return strand(ctx).submit([this, id, &ctx]() -> TaskResult
            {
                TaskResult result{};
                result.success = true;
                BatchTaskCompletedEvent event{};
                event.type = BatchTaskType::RebuildClosure;
                event.batch_id = id;

                auto emit_and_return = [&]() -> TaskResult
                    {
                        event.success = result.success;
                        enqueue_batch_event(ctx, event);
                        return result;
                    };

                // 1) Snapshot batch data needed for this job
                std::vector<ecs::EntityRef> live_snapshot;
                std::vector<Guid> old_closure;

                {
                    std::lock_guard lk(mtx_);
                    auto it = batches_.find(id);
                    if (it == batches_.end())
                    {
                        result.success = false;
                        return emit_and_return();
                    }

                    if (it->second.state != BatchInfo::State::Loaded)
                    {
                        result.success = false;
                        return emit_and_return();
                    }

                    live_snapshot = it->second.live;
                    old_closure = it->second.asset_closure_hdr;
                    event.batch_name = it->second.name;
                    event.batch_count = 1;
                }
                event.live_entities = live_snapshot.size();
                event.asset_closure_size = old_closure.size();

                // 2) Grab registry once
                auto registry_sp = ctx.entity_manager->registry_wptr().lock();
                if (!registry_sp)
                {
                    result.success = false;
                    return emit_and_return();
                }
                auto& reg = *registry_sp;

                auto erase_invalid_guids = [](std::vector<Guid>& v)
                    {
                        v.erase(
                            std::remove_if(v.begin(), v.end(), [](const Guid& g) { return !g.valid(); }),
                            v.end());
                    };

                auto sort_unique = [](std::vector<Guid>& v)
                    {
                        std::sort(v.begin(), v.end());
                        v.erase(std::unique(v.begin(), v.end()), v.end());
                    };

                // 3) Main-thread: collect direct roots from live entities
                std::vector<Guid> roots = ctx.main_thread_queue->push_and_wait([&]()
                    {
                        std::vector<Guid> guids;

                        for (const auto& er : live_snapshot)
                        {
                            if (!er.is_bound())
                            {
                                continue;
                            }

                            auto per_entity = eeng::meta::collect_asset_guids_for_entity(er.entity, reg);
                            guids.insert(guids.end(), per_entity.begin(), per_entity.end());
                        }

                        // Null/invalid GUIDs can exist (unassigned refs). Keep them for now,
                        // but they must be removed before commit/unload.
                        sort_unique(guids);
                        return guids;
                    });

                // 4) Build transitive closure (entity roots + asset->asset refs recursively)
                //    Helper is expected to call rm.load_and_bind_async for newly discovered assets.
                auto built = eeng::detail::build_asset_closure_recursive(roots, old_closure, id, ctx);

                result.success = result.success && built.tr.success;
                if (!result.success)
                {
                    // Roll back assets that were newly bound during closure building.
                    std::vector<Guid> rollback = std::move(built.loaded_now);
                    erase_invalid_guids(rollback);
                    sort_unique(rollback);

                    if (!rollback.empty())
                    {
                        std::deque<Guid> dq(rollback.begin(), rollback.end());
                        ctx.resource_manager->unbind_and_unload_async(dq, id, ctx).get();
                    }

                    return emit_and_return();
                }

                // 5) Late cleanup: remove invalid GUIDs and dedup
                std::vector<Guid> new_closure = std::move(built.closure);

                erase_invalid_guids(old_closure);
                sort_unique(old_closure);

                erase_invalid_guids(new_closure);
                sort_unique(new_closure);

                // 6) Diff vs old header closure
                std::vector<Guid> to_add;
                std::vector<Guid> to_remove;
                eeng::detail::diff_sets(old_closure, new_closure, to_add, to_remove);

                event.has_closure_delta = true;
                event.closure_roots = roots.size();
                event.closure_old = old_closure.size();
                event.closure_new = new_closure.size();
                event.closure_added = to_add.size();
                event.closure_removed = to_remove.size();
                event.asset_closure_size = event.closure_new;

                // Debug logs (optional)
                // for (auto& g : old_closure)  { EENG_LOG(&ctx, "[queue_rebuild_closure] old %s", g.to_string().c_str()); }
                // for (auto& g : new_closure)  { EENG_LOG(&ctx, "[queue_rebuild_closure] new %s", g.to_string().c_str()); }

                // for (auto& g : to_add) { EENG_LOG(&ctx, "[queue_rebuild_closure] + %s", g.to_string().c_str()); }
                // for (auto& g : to_remove) { EENG_LOG(&ctx, "[queue_rebuild_closure] - %s", g.to_string().c_str()); }

                // 7) Adjust RM leases
                //
                // Everything in to_add is already loaded/bound by build_asset_closure_recursive().
                // Only unbind/unload removals here.
                erase_invalid_guids(to_remove);
                sort_unique(to_remove);

                if (!to_remove.empty())
                {
                    std::deque<Guid> dq(to_remove.begin(), to_remove.end());
                    auto r = ctx.resource_manager->unbind_and_unload_async(dq, id, ctx).get();
                    result.success = result.success && r.success;
                }

                if (!result.success)
                {
                    // Do not commit if unload failed; keeping the old closure allows a later retry.
                    return emit_and_return();
                }

                // 8) Rebind asset refs now that the full closure is loaded
                const bool asset_rebind_ok = rebind_assets_in_closure(new_closure, id, ctx);
                result.success = result.success && asset_rebind_ok;

                // 9) Commit new closure to BatchInfo
                {
                    std::lock_guard lk(mtx_);
                    auto it = batches_.find(id);
                    if (it == batches_.end())
                    {
                        result.success = false;
                        return emit_and_return();
                    }

                    if (it->second.state != BatchInfo::State::Loaded)
                    {
                        result.success = false;
                        return emit_and_return();
                    }

                    it->second.asset_closure_hdr = std::move(new_closure);
                }

                // 10) MT: rebind refs inside entities now that assets are loaded/bound
                bind_refs_for_entities_on_main(live_snapshot, ctx);

                event.assets_rebound = asset_rebind_ok;
                return emit_and_return();
            });
    }

    // -------- Load / Unload implementations --------

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

        // 1) Load assets
        if (!B.asset_closure_hdr.empty())
        {
            res = ctx.resource_manager->load_and_bind_async(
                std::deque<Guid>(B.asset_closure_hdr.begin(), B.asset_closure_hdr.end()),
                B.id,
                ctx
            ).get();
            if (!res.success)
                return res; // TODO -> assets failed -> go on and spawn entities anyway?
        }

        // 2) spawn entities on main (if any)
        ctx.main_thread_queue->push_and_wait([&]()
            {
                B.live.clear();
                std::vector<ecs::Entity> new_entities;
                new_entities.reserve(entity_descs.size());

                for (const auto& desc : entity_descs)
                {
                    auto er = eeng::meta::spawn_entity_from_desc(
                        desc,
                        ctx,
                        eeng::meta::SerializationPurpose::file);
                    // ctx.entity_manager->register_entity(er.get_entity());
                    B.live.push_back(er);
                    new_entities.push_back(er.entity);
                }
                ctx.entity_manager->register_entities(new_entities);
            });

        // ABORT IF ASSET LOAD FAILED ???

        // 3) Bind AssetRef<> + EntityRef<> inside components
        bind_refs_for_entities_on_main(B.live, ctx);

        return res;
    }

    TaskResult BatchRegistry::do_unload(BatchInfo& B, EngineContext& ctx)
    {
        B.state = BatchInfo::State::Unloading;

        TaskResult res{};
        res.success = true;

        // 1) Main-thread: despawn / queue-destroy entities
        ctx.main_thread_queue->push_and_wait([&]()
            {
                for (auto& er : B.live)
                {
                    if (er.is_bound())
                    {
                        ctx.entity_manager->queue_entity_for_destruction(er.entity);
                        er.unbind();
                    }
                }
            });

        B.live.clear();

        // 3) RM: unbind/unload assets for this batch (if any)
        if (!B.asset_closure_hdr.empty())
        {
            res = ctx.resource_manager->unbind_and_unload_async(
                std::deque<Guid>(B.asset_closure_hdr.begin(), B.asset_closure_hdr.end()),
                B.id,
                ctx
            ).get();
            if (!res.success)
            {
                // If unload failed, keep GPU resources intact to avoid dropping shared assets.
                return res;
            }
        }

        return res;
    }

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


} // namespace eeng
