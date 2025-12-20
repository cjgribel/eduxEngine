//
//  EditComponentCommand.cpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#include "MetaLiterals.h"
#include "meta/MetaAux.h"
#include "MetaSerialize.hpp"
#include "editor/MetaFieldAssign.hpp"
#include "editor/AssignFieldCommand.hpp"
#include "LogGlobals.hpp"
#include <iostream>
#include <cassert>
#include <stack>

// Pick one
//#define ASSIGN_META_FIELD_USE_STACKBASED
#define ASSIGN_META_FIELD_USE_RECURSIVE

namespace
{
    bool is_ref(const entt::meta_any& any)
    {
        return any.base().policy() == entt::any_policy::ref;
    }

    void assign_meta_field(
        const eeng::editor::FieldTarget& target,
        const eeng::editor::MetaFieldPath& meta_path,
        entt::meta_any& value_any)
    {
        entt::meta_any root;

        if (target.kind == eeng::editor::FieldTarget::Kind::Component)
        {
            // Get registry
            auto registry_sp = target.registry.lock();
            assert(registry_sp && "Registry expired");

            // Get storage for component type
            auto* storage = registry_sp->storage(target.component_id);
            assert(storage && storage->contains(target.entity));

            // Get ref meta_any to component
            entt::meta_type meta_type = entt::resolve(target.component_id);
            root = meta_type.from_void(storage->value(target.entity));
            assert(root && "Failed to get component as meta_any");
            assert(is_ref(root) && "Component meta_any is not a reference");
        }
        else if (target.kind == eeng::editor::FieldTarget::Kind::Asset)
        {
            // TODO
#if 0
            auto& resource_manager = static_cast<ResourceManager&>(*ctx.resource_manager);
            if (auto metah_opt = resource_manager.storage().handle_for_guid(edit.target.asset_guid); metah_opt.has_value())
            {
                root /*auto any*/ = resource_manager.storage().get(*metah_opt);
                // TODO -> check is_ref?
                // TODO -> do inside modify scope?
            }
#endif
            //assert(false && "Asset field assignment not implemented yet");
            return;
        }
        else
        {
            assert(false && "Unknown FieldTarget kind");
        }

        // Apply path and set new value
        assert(!meta_path.entries.empty() && "MetaPath is empty");
#ifdef ASSIGN_META_FIELD_USE_RECURSIVE
        const bool ok = eeng::editor::assign_meta_field_recursive(root, meta_path, 0, value_any);
#endif
#ifdef ASSIGN_META_FIELD_USE_STACKBASED
        const bool ok = eeng::editor::assign_meta_field_stackbased(root, meta_path, value_any);
#endif
        assert(ok && "Failed to set field");

        // TODO ->
        // Component is now modified - run per-field callbacks or emit events.
    }
}

namespace eeng::editor
{
    //     void AssignFieldCommand::assign_meta_field(entt::meta_any& value_any)
    //     {
    //         // Get registry
    //         auto registry_sp = registry.lock();
    //         assert(registry_sp && "Registry expired");

    //         // Get storage for component type
    //         auto* storage = registry_sp->storage(component_id);
    //         assert(storage && storage->contains(entity));

    //         // Get ref meta_any to component
    //         entt::meta_type meta_type = entt::resolve(component_id);
    //         entt::meta_any root = meta_type.from_void(storage->value(entity));
    //         assert(root && "Failed to get component as meta_any");
    //         assert(is_ref(root) && "Component meta_any is not a reference");

    //         // Apply path and set new value
    //         assert(!meta_path.entries.empty() && "MetaPath is empty");
    // #ifdef ASSIGN_META_FIELD_USE_RECURSIVE
    //         const bool ok = assign_meta_field_recursive(root, meta_path, 0, value_any);
    // #endif
    // #ifdef ASSIGN_META_FIELD_USE_STACKBASED
    //         const bool ok = assign_meta_field_stackbased(root, meta_path, value_any);
    // #endif
    //         assert(ok && "Failed to set field");

    //         // TODO ->
    //         // Component is now modified - run per-field callbacks or emit events.
    //     }

    void AssignFieldCommand::execute()
    {
        assign_meta_field(edit.target, edit.meta_path, edit.new_value);

        //assign_meta_field(new_value);
    }

    void AssignFieldCommand::undo()
    {
        assign_meta_field(edit.target, edit.meta_path, edit.prev_value);

        // assign_meta_field(prev_value);
    }

    std::string AssignFieldCommand::get_name() const
    {
        return edit.display_name;
    }

    AssignFieldCommandBuilder& AssignFieldCommandBuilder::target_component(std::weak_ptr<entt::registry> registry, const ecs::Entity& entity, entt::id_type component_id)
    {
        command.edit.target.kind = FieldTarget::Kind::Component;
        command.edit.target.registry = registry;
        command.edit.target.entity = entity;
        command.edit.target.component_id = component_id;
        return *this;
    }

    AssignFieldCommandBuilder& AssignFieldCommandBuilder::target_asset(std::weak_ptr<IResourceManager> resource_manager, Guid asset_guid, const std::string& asset_type_id_str)
    {
        command.edit.target.kind = FieldTarget::Kind::Asset;
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

            // Index/Key cannot be followed by another Index/Key
            bool this_is_index_or_key =
                entry.type == MetaFieldPath::Entry::Type::Index ||
                entry.type == MetaFieldPath::Entry::Type::Key;
            // if (last_was_index_or_key && this_is_index_or_key) return false;
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
                if (auto j_new = meta::serialize_any(entry.key_any); !j_new.is_null())
                    path_str += "[" + j_new.dump() + "]";
                else
                    path_str += "[]";
            }
        }

        // Serialize new value for display
        if (auto j_new = meta::serialize_any(command.edit.new_value); !j_new.is_null())
            path_str += " = " + j_new.dump();

        return path_str;
    }

    AssignFieldCommand AssignFieldCommandBuilder::build_component_command()
    {
        // Validate target (registry, entity, component id)
        assert(!command.edit.target.registry.expired() && "registry pointer expired");
        assert(command.edit.target.entity.valid() && "entity invalid");
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

        eeng::LogGlobals::log("[AssignFieldCommand] %s", command.edit.display_name.c_str());

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
        eeng::LogGlobals::log("[AssignFieldCommand] Command not implemented: %s", command.edit.display_name.c_str());

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