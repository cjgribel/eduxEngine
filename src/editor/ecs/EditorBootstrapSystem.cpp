// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ecs/EditorBootstrapSystem.hpp"

#include "BatchRegistry.hpp"
#include "EngineContext.hpp"
#include "EngineContextHelpers.hpp"
#include "MainThreadQueue.hpp"
#include "ThreadPool.hpp"
#include "ecs/HeaderComponent.hpp"
#include "FirstPersonCameraComponent.hpp"
#include "editor/ecs/TransformGizmoComponent.hpp"
#include "ThirdPersonCameraComponent.hpp"

#include <algorithm>
#include <entt/entt.hpp>
#include <memory>
#include <vector>

namespace
{
    // Look up the editor batch id by name; returns false if missing/invalid.
    bool try_get_editor_batch_id(eeng::BatchRegistry& br, eeng::BatchId& out_id)
    {
        if (!br.try_get_batch_id_by_name(eeng::BatchRegistry::kEditorBatchName, out_id))
            return false;
        return out_id.valid();
    }

    bool is_entity_in_batch_live(
        eeng::BatchRegistry& br,
        const eeng::BatchId& batch_id,
        const eeng::ecs::EntityRef& ref)
    {
        if (!batch_id.valid() || !ref.guid.valid())
            return false;

        // Verify membership against the batch's live list to avoid stale id->batch mappings.
        const auto batches = br.list();
        for (const auto* batch : batches)
        {
            if (!batch || batch->id != batch_id)
                continue;
            if (batch->state != eeng::BatchInfo::State::Loaded)
                return false;

            for (const auto& er : batch->live)
            {
                if (er.guid == ref.guid)
                    return true;
            }
            return false;
        }
        return false;
    }
}

namespace eeng::editor
{
    void EditorBootstrapSystem::init(EngineContext& ctx)
    {
        if (initialized_)
            return;

        initialized_ = true;

        // Subscribe once during initialization; event queue callbacks are read-only after init.
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

        // Attempt to create/attach editor helpers immediately if possible.
        request_ensure_editor_entities(ctx);
    }

    void EditorBootstrapSystem::on_batch_task_completed(
        EngineContext& ctx,
        const BatchTaskCompletedEvent& event)
    {
        if (!event.success)
            return;

        // LoadAll can include the editor batch; schedule a single ensure pass.
        if (event.type == BatchTaskType::LoadAll)
        {
            request_ensure_editor_entities(ctx);
            return;
        }

        if (event.type != BatchTaskType::Load)
            return;

        // Fast path: batch name is known for this event.
        if (event.batch_name == BatchRegistry::kEditorBatchName)
        {
            request_ensure_editor_entities(ctx);
            return;
        }

        if (!ctx.batch_registry)
            return;

        auto& br = static_cast<BatchRegistry&>(*ctx.batch_registry);
        BatchId editor_id{};
        // Fallback: compare ids in case name was not populated.
        if (try_get_editor_batch_id(br, editor_id) && editor_id == event.batch_id)
            request_ensure_editor_entities(ctx);
    }

    void EditorBootstrapSystem::request_ensure_editor_entities(EngineContext& ctx)
    {
        // Coalesce back-to-back requests into a single background job.
        if (ensure_editor_entities_in_flight_.exchange(true))
        {
            ensure_editor_entities_requested_.store(true);
            return;
        }

        if (!ctx.thread_pool)
        {
            ensure_editor_entities_in_flight_.store(false);
            return;
        }

        std::weak_ptr<EngineContext> ctx_weak = ctx.shared_from_this();
        ctx.thread_pool->queue_task([this, ctx_weak]()
            {
                auto ctx_locked = ctx_weak.lock();
                if (ctx_locked)
                    ensure_editor_entities(*ctx_locked);

                ensure_editor_entities_in_flight_.store(false);

                // If something triggered another request while we were running, schedule again.
                if (ensure_editor_entities_requested_.exchange(false) && ctx_locked)
                    request_ensure_editor_entities(*ctx_locked);
            });
    }

    void EditorBootstrapSystem::ensure_editor_entities(EngineContext& ctx)
    {
        auto* em_ptr = eeng::try_get_entity_manager_ptr(ctx, "EditorBootstrapSystem");
        if (!em_ptr)
            return;

        BatchRegistry* br = nullptr;
        if (ctx.batch_registry)
            br = &static_cast<BatchRegistry&>(*ctx.batch_registry);

        BatchId editor_id{};
        const bool editor_loaded =
            br && try_get_editor_batch_id(*br, editor_id) && br->is_batch_loaded(editor_id);

        // Attach to the editor batch if loaded and not already attached.
        const auto try_attach_to_editor_batch = [&](const ecs::EntityRef& ref)
        {
            if (!editor_loaded || !ref.is_bound())
                return;

            // Use the batch live list to detect missing attachments after reloads.
            const bool in_live = is_entity_in_batch_live(*br, editor_id, ref);
            BatchId current_id{};
            if (br->try_get_loaded_batch_for_entity(ref, current_id))
            {
                if (current_id == editor_id && in_live)
                    return;

                br->queue_detach_entity(current_id, ref, ctx).get();
            }

            if (!in_live)
                br->queue_attach_entity(editor_id, ref, ctx).get();
        };

        const auto ensure_editor_entity = [&]<typename Component>(
            const char* name,
            const auto& init_component)
        {
            std::vector<ecs::EntityRef> existing_refs;

            if (ctx.main_thread_queue)
            {
                ctx.main_thread_queue->push_and_wait([&]()
                    {
                        auto registry_sp = em_ptr->registry_wptr().lock();
                        if (!registry_sp)
                            return;

                        auto& registry = *registry_sp;
                        // Collect any entities that already have the component.
                        auto view = registry.view<Component>();
                        for (const auto entity : view)
                        {
                            existing_refs.emplace_back(em_ptr->get_entity_ref(ecs::Entity{ entity }));
                        }

                        // Also match by header name in case components were removed or renamed.
                        auto header_view = registry.view<ecs::HeaderComponent>();
                        for (const auto entity : header_view)
                        {
                            const auto& header = header_view.get<ecs::HeaderComponent>(entity);
                            if (header.name != name)
                                continue;

                            ecs::EntityRef ref = em_ptr->get_entity_ref(ecs::Entity{ entity });
                            if (!ref.is_bound())
                                continue;

                            const bool already_listed = std::any_of(
                                existing_refs.begin(),
                                existing_refs.end(),
                                [&](const ecs::EntityRef& existing)
                                {
                                    return existing.guid == ref.guid;
                                });

                            if (!already_listed)
                                existing_refs.push_back(ref);
                        }
                    });
            }

            // Prefer entities already attached to the editor batch; otherwise keep the first.
            ecs::EntityRef selected{};
            if (!existing_refs.empty())
            {
                if (editor_loaded)
                {
                    for (const auto& ref : existing_refs)
                    {
                        BatchId current_id{};
                        if (ref.is_bound()
                            && br->try_get_loaded_batch_for_entity(ref, current_id)
                            && current_id == editor_id)
                        {
                            selected = ref;
                            break;
                        }
                    }
                }

                if (!selected.is_bound())
                    selected = existing_refs.front();
            }

            if (selected.is_bound())
            {
                // Ensure the selected entity stays with the editor batch if it is loaded.
                try_attach_to_editor_batch(selected);

                if (ctx.main_thread_queue)
                {
                    ctx.main_thread_queue->push_and_wait([&]()
                        {
                            auto registry_sp = ctx.entity_manager->registry_wptr().lock();
                            if (!registry_sp || !registry_sp->valid(selected.entity))
                                return;

                            // Reapply the component if it was missing on the selected entity.
                            if (!registry_sp->all_of<Component>(selected.entity))
                            {
                                auto& comp = registry_sp->emplace<Component>(selected.entity);
                                init_component(comp);
                            }
                        });
                }
                return;
            }

            if (!editor_loaded)
                return;

            // No existing entity found: create a new one in the editor batch.
            ecs::EntityRef created = br->queue_create_entity(editor_id, name, ecs::EntityRef{}, ctx).get();
            if (!created.is_bound())
                return;

            if (ctx.main_thread_queue)
            {
                ctx.main_thread_queue->push_and_wait([&]()
                    {
                        auto registry_sp = ctx.entity_manager->registry_wptr().lock();
                        if (!registry_sp || !registry_sp->valid(created.entity))
                            return;

                        // Make sure the new entity receives the component data.
                        if (!registry_sp->all_of<Component>(created.entity))
                        {
                            auto& comp = registry_sp->emplace<Component>(created.entity);
                            init_component(comp);
                        }
                    });
            }
        };

        ensure_editor_entity.operator()<TransformGizmoComponent>(
            "Transform Gizmo",
            [](TransformGizmoComponent&) {});

        ensure_editor_entity.operator()<eeng::module1::ThirdPersonCameraComponent>(
            "Third Person Camera",
            [](eeng::module1::ThirdPersonCameraComponent& camera)
            {
                camera.active = true;
            });

        ensure_editor_entity.operator()<eeng::module1::FirstPersonCameraComponent>(
            "First Person Camera",
            [](eeng::module1::FirstPersonCameraComponent& camera)
            {
                camera.active = false;
            });
    }
} // namespace eeng::editor
