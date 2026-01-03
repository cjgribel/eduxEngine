//
//  EditComponentCommand.cpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#include "editor/AssignFieldCommand.hpp"
#include "editor/MetaFieldAssign.hpp"
#include "meta/EntityMetaHelpers.hpp"
#include "EngineContextHelpers.hpp"
#include "ResourceManager.hpp"
#include "BatchRegistry.hpp"
#include "MetaLiterals.h"
#include "meta/MetaAux.h"
#include "ecs/EntityManager.hpp"
#include "MetaSerialize.hpp"
#include "LogGlobals.hpp"
#include <iostream>
#include <cassert>
#include <stack>
#include <algorithm>
#include <optional>

// Pick one
//#define ASSIGN_META_FIELD_USE_STACKBASED
#define ASSIGN_META_FIELD_USE_RECURSIVE

namespace
{
    std::optional<eeng::ecs::Entity> resolve_target_entity(const eeng::editor::FieldTarget& target)
    {
        if (target.kind != eeng::editor::FieldTarget::Kind::Component)
            return std::nullopt;

        eeng::ecs::Entity entity = target.entity;
        if (target.entity_guid.valid())
        {
            auto ctx_sp = target.ctx.lock();
            if (ctx_sp && ctx_sp->entity_manager)
            {
                if (auto entity_opt = ctx_sp->entity_manager->get_entity_from_guid(target.entity_guid))
                    entity = *entity_opt;
                if (!ctx_sp->entity_manager->entity_valid(entity))
                    return std::nullopt;
            }
        }

        if (!entity.has_id())
            return std::nullopt;
        return entity;
    }

    std::vector<eeng::Guid> collect_asset_guids_sorted(const eeng::ecs::Entity& entity, entt::registry& registry)
    {
        auto guids = eeng::meta::collect_asset_guids_for_entity(entity, registry);
        std::sort(guids.begin(), guids.end());
        guids.erase(std::unique(guids.begin(), guids.end()), guids.end());
        return guids;
    }

    void mark_batch_dirty_for_entity(eeng::EngineContext& ctx, const eeng::ecs::Entity& entity)
    {
        if (!ctx.batch_registry)
            return;
        auto& br = static_cast<eeng::BatchRegistry&>(*ctx.batch_registry);
        br.mark_closure_dirty_for_entity(entity, ctx);
    }

    bool is_ref(const entt::meta_any& any)
    {
        return any.base().policy() == entt::any_policy::ref;
    }

    struct ComponentTargetContext
    {
        std::shared_ptr<entt::registry> registry;
        eeng::ecs::Entity entity;
    };

    std::optional<ComponentTargetContext> resolve_component_target(const eeng::editor::FieldTarget& target)
    {
        if (target.kind != eeng::editor::FieldTarget::Kind::Component)
            return std::nullopt;

        auto registry_sp = target.registry.lock();
        if (!registry_sp)
            return std::nullopt;

        const auto entity_opt = resolve_target_entity(target);
        if (!entity_opt || !registry_sp->valid(*entity_opt))
            return std::nullopt;

        return ComponentTargetContext{ registry_sp, *entity_opt };
    }

    std::vector<eeng::Guid> collect_asset_guids_if_component_target(
        const std::optional<ComponentTargetContext>& component_ctx)
    {
        if (!component_ctx)
            return {};

        return collect_asset_guids_sorted(component_ctx->entity, *component_ctx->registry);
    }

    void emit_field_changed_event(
        const eeng::editor::FieldTarget& target,
        const eeng::editor::MetaFieldPath& meta_path,
        bool is_undo)
    {
        auto ctx_sp = target.ctx.lock();
        if (!ctx_sp)
            return;

        auto* event_queue = eeng::try_get_event_queue(*ctx_sp, "AssignFieldCommand");
        if (!event_queue)
            return;

        event_queue->enqueue_event(eeng::editor::FieldChangedEvent{
            target,
            meta_path,
            is_undo
            });
    }

    void invoke_component_post_assign_hook(
        eeng::EngineContext& ctx,
        const eeng::editor::FieldTarget& target,
        const eeng::ecs::Entity& entity,
        const eeng::editor::MetaFieldPath& meta_path,
        bool is_undo)
    {
        if (target.kind != eeng::editor::FieldTarget::Kind::Component)
            return;

        entt::meta_type meta_type = entt::resolve(target.component_id);
        if (!meta_type)
            return;

        if (entt::meta_func meta_func = meta_type.func(eeng::literals::post_assign_hs); meta_func)
        {
            meta_func.invoke(
                {},
                entt::forward_as_meta(ctx),
                entt::forward_as_meta(entity),
                entt::forward_as_meta(meta_path),
                entt::forward_as_meta(is_undo));
        }
    }

    bool assign_meta_field(
        const eeng::editor::FieldTarget& target,
        const eeng::editor::MetaFieldPath& meta_path,
        entt::meta_any& value_any)
    {
        // entt::meta_any root;
        assert(!meta_path.entries.empty() && "MetaPath is empty");

        if (target.kind == eeng::editor::FieldTarget::Kind::Component)
        {
            auto ctx_sp = target.ctx.lock();
            if (!ctx_sp)
                return false;

            auto registry_sp = eeng::try_get_registry(*ctx_sp, "AssignFieldCommand");
            if (!registry_sp)
                return false;

            const auto entity_opt = resolve_target_entity(target);
            if (!entity_opt || !registry_sp->valid(*entity_opt))
                return false;
            const auto entity = *entity_opt;

            // Get storage for component type
            auto* storage = registry_sp->storage(target.component_id);
            if (!storage || !storage->contains(entity))
                return false;

            // Get ref meta_any to component
            entt::meta_type meta_type = entt::resolve(target.component_id);
            entt::meta_any root = meta_type.from_void(storage->value(entity));
            if (!root || !is_ref(root))
                return false;

#ifdef ASSIGN_META_FIELD_USE_RECURSIVE
            const bool ok = eeng::editor::assign_meta_field_recursive(root, meta_path, 0, value_any);
#endif
#ifdef ASSIGN_META_FIELD_USE_STACKBASED
            const bool ok = eeng::editor::assign_meta_field_stackbased(root, meta_path, value_any);
#endif
            return ok;
        }
        else if (target.kind == eeng::editor::FieldTarget::Kind::Asset)
        {
            auto ctx_sp = target.ctx.lock();
            if (!ctx_sp)
                return false;

            auto rm = eeng::try_get_resource_manager(*ctx_sp, "AssignFieldCommand");
            if (!rm) return false;

            // Get root meta_any for asset
            if (auto mh_opt = rm->storage().handle_for_guid(target.asset_guid); mh_opt.has_value())
            {
                const bool ok = rm->storage().modify(*mh_opt, [&](entt::meta_any& root)
                    {
                        // Note: no re-entry to storage inside visitor lambda!

                        if (!is_ref(root))
                            return false;
#ifdef ASSIGN_META_FIELD_USE_STACKBASED
                        return eeng::editor::assign_meta_field_stackbased(root, meta_path, value_any);
#else
                        return eeng::editor::assign_meta_field_recursive(root, meta_path, 0, value_any);
#endif
                    });
                return ok;
            }
            return false;
        }
        return false;

        // Apply path and set new value
        // assert(!meta_path.entries.empty() && "MetaPath is empty");


        // Post-assign hooks and field-changed events are emitted by AssignFieldCommand.
    }
}

namespace eeng::editor
{
    CommandStatus AssignFieldCommand::execute()
    {
        const auto component_ctx = resolve_component_target(edit.target);
        const auto asset_guids_before = collect_asset_guids_if_component_target(component_ctx);

        if (!assign_meta_field(edit.target, edit.meta_path, edit.new_value))
            return CommandStatus::Failed;

        if (component_ctx)
        {
            if (auto ctx_sp = edit.target.ctx.lock())
                invoke_component_post_assign_hook(*ctx_sp, edit.target, component_ctx->entity, edit.meta_path, false);
        }

        emit_field_changed_event(edit.target, edit.meta_path, false);

        if (component_ctx)
        {
            // Component edits may change the asset closure for an entity.
            const auto asset_guids_after = collect_asset_guids_if_component_target(component_ctx);
            if (asset_guids_before != asset_guids_after)
            {
                if (auto ctx_sp = edit.target.ctx.lock())
                    mark_batch_dirty_for_entity(*ctx_sp, component_ctx->entity);
            }
        }

        //assign_meta_field(new_value);
        return CommandStatus::Done;
    }

    CommandStatus AssignFieldCommand::undo()
    {
        const auto component_ctx = resolve_component_target(edit.target);
        const auto asset_guids_before = collect_asset_guids_if_component_target(component_ctx);

        if (!assign_meta_field(edit.target, edit.meta_path, edit.prev_value))
            return CommandStatus::Failed;

        if (component_ctx)
        {
            if (auto ctx_sp = edit.target.ctx.lock())
                invoke_component_post_assign_hook(*ctx_sp, edit.target, component_ctx->entity, edit.meta_path, true);
        }

        emit_field_changed_event(edit.target, edit.meta_path, true);

        if (component_ctx)
        {
            // Component edits may change the asset closure for an entity.
            const auto asset_guids_after = collect_asset_guids_if_component_target(component_ctx);
            if (asset_guids_before != asset_guids_after)
            {
                if (auto ctx_sp = edit.target.ctx.lock())
                    mark_batch_dirty_for_entity(*ctx_sp, component_ctx->entity);
            }
        }

        // assign_meta_field(prev_value);
        return CommandStatus::Done;
    }

    std::string AssignFieldCommand::get_name() const
    {
        return edit.display_name;
    }

    AssignFieldCommandBuilder& AssignFieldCommandBuilder::target_component(EngineContext& ctx, const ecs::Entity& entity, entt::id_type component_id)
    {
        command.edit.target.kind = FieldTarget::Kind::Component;
        command.edit.target.ctx = ctx.weak_from_this();
        command.edit.target.registry = ctx.entity_manager->registry_wptr();
        command.edit.target.entity = entity;
        if (ctx.entity_manager && ctx.entity_manager->entity_valid(entity))
        {
            auto& em = static_cast<eeng::EntityManager&>(*ctx.entity_manager);
            command.edit.target.entity_guid = em.get_entity_guid(entity);
        }
        command.edit.target.component_id = component_id;
        return *this;
    }

    AssignFieldCommandBuilder& AssignFieldCommandBuilder::target_asset(EngineContext& ctx, std::weak_ptr<IResourceManager> resource_manager, Guid asset_guid, const std::string& asset_type_id_str)
    {
        command.edit.target.kind = FieldTarget::Kind::Asset;
        command.edit.target.ctx = ctx.weak_from_this();
        command.edit.target.resource_manager = resource_manager;
        command.edit.target.asset_guid = asset_guid;
        command.edit.target.asset_type_id_str = asset_type_id_str;
        return *this;
    }

    AssignFieldCommandBuilder& AssignFieldCommandBuilder::prev_value(const entt::meta_any& value) { command.edit.prev_value = value; return *this; }

    AssignFieldCommandBuilder& AssignFieldCommandBuilder::new_value(const entt::meta_any& value) { command.edit.new_value = value; return *this; }

    AssignFieldCommandBuilder& AssignFieldCommandBuilder::push_path_data(entt::id_type id, const std::string& name)
    {
        command.edit.meta_path.entries.push_back(
            MetaFieldPath::Entry{ .type = MetaFieldPath::Entry::Type::Data, .data_id = id, .name = name }
        );
        return *this;
    }

    AssignFieldCommandBuilder& AssignFieldCommandBuilder::push_path_index(int index, const std::string& name)
    {
        command.edit.meta_path.entries.push_back(
            MetaFieldPath::Entry{ .type = MetaFieldPath::Entry::Type::Index, .index = index, .name = name }
        );
        return *this;
    }

    AssignFieldCommandBuilder& AssignFieldCommandBuilder::push_path_key(const entt::meta_any& key_any, const std::string& name)
    {
        command.edit.meta_path.entries.push_back(
            MetaFieldPath::Entry{ .type = MetaFieldPath::Entry::Type::Key, .key_any = key_any/*.as_ref()*/, .name = name }
        );
        return *this;
    }

    AssignFieldCommandBuilder& AssignFieldCommandBuilder::pop_path()
    {
        command.edit.meta_path.entries.pop_back();
        return *this;
    }

    // AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::reset()
    // {
    //     assert(!command.edit.meta_path.entries.size() && "Meta path not empty when clearing");
    //     command = AssignFieldCommand{};
    //     return *this;
    // }

    bool AssignFieldCommandBuilder::validate_meta_path()
    {
        if (!command.edit.meta_path.entries.size()) return false;

        bool last_was_index_or_key = false;
        for (int i = 0; i < command.edit.meta_path.entries.size(); i++)
        {
            auto& entry = command.edit.meta_path.entries[i];

            // First entry must be Data (enter data member of a component)
            if (i == 0 && entry.type != MetaFieldPath::Entry::Type::Data) return false;
            // assert(i > 0 || entry.type == MetaFieldPath::Entry::Type::Data);

            // Check so relevant values are set for each entry type
            if (entry.type == MetaFieldPath::Entry::Type::None) return false;
            if (entry.type == MetaFieldPath::Entry::Type::Data && !entry.data_id) return false;
            if (entry.type == MetaFieldPath::Entry::Type::Index && entry.index < 0) return false;
            if (entry.type == MetaFieldPath::Entry::Type::Key && !entry.key_any) return false;

            bool this_is_index_or_key =
                entry.type == MetaFieldPath::Entry::Type::Index ||
                entry.type == MetaFieldPath::Entry::Type::Key;
            // Additional sequence checks maybe. Note that index/key entries can be repeated (e.g. nested containers)
            // ...
            last_was_index_or_key = this_is_index_or_key;
        }

        return true;
    }

    std::string AssignFieldCommandBuilder::meta_path_to_string()
    {
        std::string path_str;

        // Gather meta path
        for (auto& entry : command.edit.meta_path.entries)
        {
            if (entry.type == MetaFieldPath::Entry::Type::Data) {
                path_str += "::" + entry.name;
            }
            else if (entry.type == MetaFieldPath::Entry::Type::Index) {
                path_str += "[" + std::to_string(entry.index) + "]";
            }
            else if (entry.type == MetaFieldPath::Entry::Type::Key) {
                if (auto j_new = meta::serialize_any(entry.key_any, meta::SerializationPurpose::display); !j_new.is_null())
                    path_str += "[" + j_new.dump() + "]";
                else
                    path_str += "[]";
            }
        }

        // Serialize new value for display
        if (auto j_new = meta::serialize_any(command.edit.new_value, meta::SerializationPurpose::display); !j_new.is_null())
            path_str += " = " + j_new.dump();

        return path_str;
    }

    AssignFieldCommand AssignFieldCommandBuilder::build_component_command()
    {
        // Validate target (registry, entity, component id)
        assert(!command.edit.target.registry.expired() && "registry pointer expired");
        assert(command.edit.target.entity.has_id() && "entity invalid");
        assert(command.edit.target.component_id != 0 && "component id invalid");

        // Validate values
        assert(command.edit.prev_value);
        assert(command.edit.new_value);

        // Validate meta path
        assert(validate_meta_path() && "meta path validation failed");

        // Display name
        command.edit.display_name =
            meta::get_meta_type_display_name(entt::resolve(command.edit.target.component_id))
            + meta_path_to_string();

        eeng::LogGlobals::log("[build_component_command] %s", command.edit.display_name.c_str());

        return command;
    }

    AssignFieldCommand AssignFieldCommandBuilder::build_asset_command()
    {
        // Validate target (resource manager, asset guid, asset type id)
        assert(!command.edit.target.resource_manager.expired() && "resource manager pointer expired");
        //assert(command.edit.target.asset_guid.is_valid() && "asset guid invalid");
        //assert(!command.edit.target.asset_type_id_str.empty() && "asset type id string invalid");

        // Validate values
        assert(command.edit.prev_value);
        assert(command.edit.new_value);

        // Validate meta path
        assert(validate_meta_path() && "meta path validation failed");

        // Display name
        command.edit.display_name = meta_path_to_string();
        eeng::LogGlobals::log("[build_asset_command] %s", command.edit.display_name.c_str());

        return command;
    }

    AssignFieldCommand AssignFieldCommandBuilder::build()
    {
        if (command.edit.target.kind == FieldTarget::Kind::Component)
        {
            return build_component_command();
        }
        else if (command.edit.target.kind == FieldTarget::Kind::Asset)
        {
            return build_asset_command();
        }
        else
        {
            assert(false && "Unknown FieldTarget kind");
            return command;
        }

    }

} // namespace Editor
