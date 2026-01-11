// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ecs/EditorBootstrapSystem.hpp"

#include "BatchRegistry.hpp"
#include "EngineContext.hpp"
#include "EngineContextHelpers.hpp"
#include "MainThreadQueue.hpp"
#include "ThreadPool.hpp"
#include "editor/ecs/ManipulatorGizmoComponent.hpp"

#include <entt/entt.hpp>
#include <memory>
#include <vector>

namespace
{
    bool try_get_editor_batch_id(eeng::BatchRegistry& br, eeng::BatchId& out_id)
    {
        if (!br.try_get_batch_id_by_name(eeng::BatchRegistry::kEditorBatchName, out_id))
            return false;
        return out_id.valid();
    }
}

namespace eeng::editor
{
    void EditorBootstrapSystem::init(EngineContext& ctx)
    {
        if (initialized_)
            return;

        initialized_ = true;

        auto* event_queue = eeng::try_get_event_queue(ctx, "EditorBootstrapSystem");
        if (event_queue)
        {
            std::weak_ptr<EngineContext> ctx_weak = ctx.shared_from_this();
            event_queue->register_callback([this, ctx_weak](const BatchTaskCompletedEvent& event)
                {
                    auto ctx_locked = ctx_weak.lock();
                    if (!ctx_locked)
                        return;
                    on_batch_task_completed(*ctx_locked, event);
                });
        }

        // Attempt to create/attach the gizmo immediately if possible.
        request_ensure_gizmo(ctx);
    }

    void EditorBootstrapSystem::on_batch_task_completed(
        EngineContext& ctx,
        const BatchTaskCompletedEvent& event)
    {
        if (!event.success)
            return;

        if (event.type == BatchTaskType::LoadAll)
        {
            request_ensure_gizmo(ctx);
            return;
        }

        if (event.type != BatchTaskType::Load)
            return;

        if (event.batch_name == BatchRegistry::kEditorBatchName)
        {
            request_ensure_gizmo(ctx);
            return;
        }

        if (!ctx.batch_registry)
            return;

        auto& br = static_cast<BatchRegistry&>(*ctx.batch_registry);
        BatchId editor_id{};
        if (try_get_editor_batch_id(br, editor_id) && editor_id == event.batch_id)
            request_ensure_gizmo(ctx);
    }

    void EditorBootstrapSystem::request_ensure_gizmo(EngineContext& ctx)
    {
        if (ensure_in_flight_.exchange(true))
        {
            ensure_requested_.store(true);
            return;
        }

        if (!ctx.thread_pool)
        {
            ensure_in_flight_.store(false);
            return;
        }

        std::weak_ptr<EngineContext> ctx_weak = ctx.shared_from_this();
        ctx.thread_pool->queue_task([this, ctx_weak]()
            {
                auto ctx_locked = ctx_weak.lock();
                if (ctx_locked)
                    ensure_editor_gizmo_entity(*ctx_locked);

                ensure_in_flight_.store(false);

                if (ensure_requested_.exchange(false) && ctx_locked)
                    request_ensure_gizmo(*ctx_locked);
            });
    }

    void EditorBootstrapSystem::ensure_editor_gizmo_entity(EngineContext& ctx)
    {
        auto* em_ptr = eeng::try_get_entity_manager_ptr(ctx, "EditorBootstrapSystem");
        if (!em_ptr)
            return;

        std::vector<ecs::EntityRef> existing_refs;

        if (ctx.main_thread_queue)
        {
            ctx.main_thread_queue->push_and_wait([&]()
                {
                    auto registry_sp = em_ptr->registry_wptr().lock();
                    if (!registry_sp)
                        return;

                    auto view = registry_sp->view<ManipulatorGizmoComponent>();
                    for (const auto entity : view)
                    {
                        existing_refs.emplace_back(em_ptr->get_entity_ref(ecs::Entity{ entity }));
                    }
                });
        }

        BatchRegistry* br = nullptr;
        if (ctx.batch_registry)
            br = &static_cast<BatchRegistry&>(*ctx.batch_registry);

        const auto try_attach_to_editor_batch = [&](const ecs::EntityRef& ref)
        {
            if (!br || !ref.is_bound())
                return;

            BatchId editor_id{};
            if (!try_get_editor_batch_id(*br, editor_id))
                return;
            if (!br->is_batch_loaded(editor_id))
                return;

            BatchId current_id{};
            if (br->try_get_loaded_batch_for_entity(ref, current_id) && current_id == editor_id)
                return;

            br->queue_attach_entity(editor_id, ref, ctx).get();
        };

        ecs::EntityRef existing{};
        if (!existing_refs.empty())
        {
            if (br)
            {
                BatchId editor_id{};
                if (try_get_editor_batch_id(*br, editor_id) && br->is_batch_loaded(editor_id))
                {
                    for (const auto& ref : existing_refs)
                    {
                        BatchId current_id{};
                        if (ref.is_bound()
                            && br->try_get_loaded_batch_for_entity(ref, current_id)
                            && current_id == editor_id)
                        {
                            existing = ref;
                            break;
                        }
                    }
                }
            }

            if (!existing.is_bound())
                existing = existing_refs.front();
        }

        if (existing.is_bound())
        {
            try_attach_to_editor_batch(existing);
            return;
        }

        ecs::EntityRef created{};

        if (br)
        {
            BatchId editor_id{};
            if (try_get_editor_batch_id(*br, editor_id))
            {
                if (!br->is_batch_loaded(editor_id))
                    return;

                created = br->queue_create_entity(editor_id, "Editor Gizmo", ecs::EntityRef{}, ctx).get();
            }
        }

        if (created.is_bound())
        {
            if (ctx.main_thread_queue)
            {
                ctx.main_thread_queue->push_and_wait([&]()
                    {
                        auto registry_sp = ctx.entity_manager->registry_wptr().lock();
                        if (!registry_sp || !registry_sp->valid(created.entity))
                            return;

                        if (!registry_sp->all_of<ManipulatorGizmoComponent>(created.entity))
                            registry_sp->emplace<ManipulatorGizmoComponent>(created.entity);
                    });
            }
            return;
        }

        if (ctx.main_thread_queue)
        {
            ctx.main_thread_queue->push_and_wait([&]()
                {
                    auto [guid, entity] = em_ptr->create_entity_live_parent(
                        BatchRegistry::kEditorBatchName,
                        "Editor Gizmo",
                        ecs::Entity::EntityNull,
                        ecs::Entity::EntityNull);

                    created = ecs::EntityRef{ guid, entity };

                    auto registry_sp = ctx.entity_manager->registry_wptr().lock();
                    if (!registry_sp || !registry_sp->valid(entity))
                        return;

                    if (!registry_sp->all_of<ManipulatorGizmoComponent>(entity))
                        registry_sp->emplace<ManipulatorGizmoComponent>(entity);
                });
        }

        if (created.is_bound())
            try_attach_to_editor_batch(created);
    }
} // namespace eeng::editor
