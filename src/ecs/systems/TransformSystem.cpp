// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/TransformSystem.hpp"

#include "EngineContextHelpers.hpp"
#include "editor/AssignFieldCommand.hpp"
#include "ecs/TransformComponent.hpp"

#include <entt/entt.hpp>
#include <optional>

namespace
{
    std::optional<eeng::ecs::Entity> resolve_event_entity(
        const eeng::editor::FieldTarget& target,
        eeng::EngineContext& ctx)
    {
        eeng::ecs::Entity entity = target.entity;
        if (target.entity_guid.valid())
        {
            if (ctx.entity_manager)
            {
                if (auto entity_opt = ctx.entity_manager->get_entity_from_guid(target.entity_guid))
                    entity = *entity_opt;
                if (!ctx.entity_manager->entity_valid(entity))
                    return std::nullopt;
            }
        }

        if (!entity.has_id())
            return std::nullopt;
        return entity;
    }
}

namespace eeng::ecs::systems
{
    void TransformSystem::init(EngineContext& ctx)
    {
        auto* event_queue = eeng::try_get_event_queue(ctx, "TransformSystem");
        if (!event_queue)
            return;

        event_queue->register_callback([this](const editor::FieldChangedEvent& event)
            {
                handle_field_changed_event(event);
            });
    }

    void TransformSystem::update(EngineContext& ctx, float)
    {
        auto registry_sp = eeng::try_get_registry(ctx, "TransformSystem");
        if (!registry_sp)
            return;

        auto* entity_manager = eeng::try_get_entity_manager(ctx, "TransformSystem");
        if (!entity_manager)
            return;

        entity_manager->scene_graph().traverse(registry_sp);
    }

    void TransformSystem::on_component_post_assign(
        EngineContext& ctx,
        const ecs::Entity& entity,
        const editor::MetaFieldPath& meta_path,
        bool is_undo)
    {
        (void)meta_path;
        (void)is_undo;
        if (!kDirtyViaMetaHook)
            return;

        auto* registry = eeng::try_get_registry_ptr(ctx, "TransformSystem");
        if (!registry || !registry->valid(entity))
            return;

        if (auto* tfm = registry->try_get<ecs::TransformComponent>(entity))
            tfm->mark_local_dirty();
    }

    void TransformSystem::handle_field_changed_event(const editor::FieldChangedEvent& event)
    {
        if (!kDirtyViaEvent)
            return;
        if (event.target.kind != editor::FieldTarget::Kind::Component)
            return;
        if (event.target.component_id != entt::type_hash<ecs::TransformComponent>::value())
            return;

        auto ctx_sp = event.target.ctx.lock();
        if (!ctx_sp)
            return;

        auto entity_opt = resolve_event_entity(event.target, *ctx_sp);
        if (!entity_opt)
            return;

        auto registry_sp = event.target.registry.lock();
        entt::registry* registry = registry_sp ? registry_sp.get()
            : eeng::try_get_registry_ptr(*ctx_sp, "TransformSystem");

        if (!registry || !registry->valid(*entity_opt))
            return;

        if (auto* tfm = registry->try_get<ecs::TransformComponent>(*entity_opt))
            tfm->mark_local_dirty();
    }
}
