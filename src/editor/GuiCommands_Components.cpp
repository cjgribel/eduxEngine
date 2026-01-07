// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include <cassert>
#include "GuiCommands.hpp"
#include "MetaSerialize.hpp"
#include "editor/CommandEntityHelpers.hpp"
#include "editor/CommandSnapshot.hpp"
#include "ecs/EntityManager.hpp"
#include "meta/MetaAux.h"
#include "LogMacros.h"

namespace eeng::editor {
    using eeng::EngineContextWeakPtr;
    using eeng::EntityManager;
    using eeng::Guid;
    namespace ecs = eeng::ecs;
    using ecs::Entity;

    AddComponentToEntityCommand::AddComponentToEntityCommand(
        const Entity& entity,
        entt::id_type comp_id,
        EngineContextWeakPtr ctx) :
        entity(entity),
        comp_id(comp_id),
        ctx(std::move(ctx))
    {
        display_name = std::string("Add Component ")
            + std::to_string(comp_id)
            + " to Entity "
            + std::to_string(entity.to_integral());
    }

    CommandStatus AddComponentToEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        if (!entity_guid.valid())
        {
            if (!try_capture_guid(em, entity, entity_guid))
                return CommandStatus::Done;
        }
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        if (storage->contains(entity_current))
        {
            if (auto mt = entt::resolve(comp_id); mt)
            {
                EENG_LOG_INFO(ctx_sp.get(),
                    "AddComponent: %s already present on entity %u",
                    eeng::meta::get_meta_type_display_name(mt).c_str(),
                    entity_current.to_integral());
            }
            return CommandStatus::Done;
        }

        auto comp_type = entt::resolve(comp_id);
        if (!comp_type)
            return CommandStatus::Done;

        auto comp_any = comp_type.construct();
        if (!comp_any)
        {
            EENG_LOG_WARN(ctx_sp.get(),
                "AddComponent: failed to default-construct %s",
                eeng::meta::get_meta_type_display_name(comp_type).c_str());
            return CommandStatus::Done;
        }

        storage->push(entity_current, comp_any.base().data());
        bind_refs_for_entity(entity_current, *ctx_sp);
        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        return CommandStatus::Done;
    }

    CommandStatus AddComponentToEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        if (!storage->contains(entity_current))
            return CommandStatus::Done;

        if (comp_id == header_component_id() && header_component_id() != entt::id_type{})
        {
            if (auto mt = entt::resolve(comp_id); mt)
            {
                EENG_LOG_INFO(ctx_sp.get(),
                    "Undo AddComponent: %s is protected; skipping remove on entity %u",
                    eeng::meta::get_meta_type_display_name(mt).c_str(),
                    entity_current.to_integral());
            }
            return CommandStatus::Done;
        }

        storage->remove(entity_current);
        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        return CommandStatus::Done;
    }

    std::string AddComponentToEntityCommand::get_name() const
    {
        return display_name;
    }

    // --- RemoveComponentFromEntityCommand ----------------------------------------

    RemoveComponentFromEntityCommand::RemoveComponentFromEntityCommand(
        const Entity& entity,
        entt::id_type comp_id,
        EngineContextWeakPtr ctx) :
        entity(entity),
        comp_id(comp_id),
        ctx(std::move(ctx))
    {
        display_name = std::string("Remove Component ")
            + std::to_string(comp_id)
            + " from Entity "
            + std::to_string(entity.to_integral());
    }

    CommandStatus RemoveComponentFromEntityCommand::execute()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        if (!entity_guid.valid())
        {
            if (!try_capture_guid(em, entity, entity_guid))
                return CommandStatus::Done;
        }
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        if (!storage->contains(entity_current))
        {
            if (auto mt = entt::resolve(comp_id); mt)
            {
                EENG_LOG_INFO(ctx_sp.get(),
                    "RemoveComponent: %s not present on entity %u",
                    eeng::meta::get_meta_type_display_name(mt).c_str(),
                    entity_current.to_integral());
            }
            return CommandStatus::Done;
        }

        if (comp_id == header_component_id() && header_component_id() != entt::id_type{})
        {
            if (auto mt = entt::resolve(comp_id); mt)
            {
                EENG_LOG_INFO(ctx_sp.get(),
                    "RemoveComponent: %s is protected; skipping remove on entity %u",
                    eeng::meta::get_meta_type_display_name(mt).c_str(),
                    entity_current.to_integral());
            }
            return CommandStatus::Done;
        }

        // Fetch component type
        auto comp_type = entt::resolve(comp_id);
        // Fetch component
        auto comp_any = comp_type.from_void(storage->value(entity_current));
        // Serialize component
        comp_json = meta::serialize_any(
            comp_type.from_void(storage->value(entity_current)),
            eeng::meta::SerializationPurpose::undo);
        // Remove component from entity
        storage->remove(entity_current);
        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        return CommandStatus::Done;
    }

    CommandStatus RemoveComponentFromEntityCommand::undo()
    {
        auto ctx_sp = ctx.lock();
        if (!ctx_sp || !ctx_sp->entity_manager)
            return CommandStatus::Done;

        auto registry_sp = ctx_sp->entity_manager->registry_wptr().lock();
        if (!registry_sp)
            return CommandStatus::Done;

        auto& em = static_cast<EntityManager&>(*ctx_sp->entity_manager);
        const auto entity_current = resolve_entity_from_guid(em, entity_guid);
        if (!entity_current.has_id())
            return CommandStatus::Done;

        // Fetch component storage
        ensure_storage(*registry_sp, comp_id);
        auto storage = registry_sp->storage(comp_id);
        assert(!storage->contains(entity_current));

        // Fetch component type
        auto comp_type = entt::resolve(comp_id);
        // Default-construct component
        auto comp_any = comp_type.construct();
        // Deserialize component
        meta::deserialize_any(
            comp_json,
            comp_any,
            entity_current,
            *ctx_sp,
            eeng::meta::SerializationPurpose::undo);
        // Add component to entity
        storage->push(entity_current, comp_any.base().data());
        bind_refs_for_entity(entity_current, *ctx_sp);
        mark_batch_dirty_for_entity(entity_current, *ctx_sp);
        return CommandStatus::Done;
    }

    std::string RemoveComponentFromEntityCommand::get_name() const
    {
        return display_name;
    }
}
