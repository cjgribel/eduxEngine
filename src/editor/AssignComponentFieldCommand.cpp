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
#include "editor/AssignComponentFieldCommand.hpp"
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
}

namespace eeng::editor
{
    void AssignComponentFieldCommand::assign_meta_field(entt::meta_any& value_any)
    {
        // Get registry
        auto registry_sp = registry.lock();
        assert(registry_sp && "Registry expired");

        // Get storage for component type
        auto* storage = registry_sp->storage(component_id);
        assert(storage && storage->contains(entity));

        // Get ref meta_any to component
        entt::meta_type meta_type = entt::resolve(component_id);
        entt::meta_any root = meta_type.from_void(storage->value(entity));
        assert(root && "Failed to get component as meta_any");
        assert(is_ref(root) && "Component meta_any is not a reference");

        // Apply path and set new value
        assert(!meta_path.entries.empty() && "MetaPath is empty");
#ifdef ASSIGN_META_FIELD_USE_RECURSIVE
        const bool ok = assign_meta_field_recursive(root, meta_path, 0, value_any);
#endif
#ifdef ASSIGN_META_FIELD_USE_STACKBASED
        const bool ok = assign_meta_field_stackbased(root, meta_path, value_any);
#endif
        assert(ok && "Failed to set field");

        // TODO ->
        // Component is now modified - run per-field callbacks or emit events.
    }

    void AssignComponentFieldCommand::execute()
    {
        assign_meta_field(new_value);
    }

    void AssignComponentFieldCommand::undo()
    {
        assign_meta_field(prev_value);
    }

    std::string AssignComponentFieldCommand::get_name() const
    {
        return display_name;
    }

    AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::registry(std::weak_ptr<entt::registry> registry) { command.registry = registry; return *this; }

    AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::entity(const ecs::Entity& entity) { command.entity = entity; return *this; }

    AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::component(entt::id_type id) { command.component_id = id; return *this; }

    AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::prev_value(const entt::meta_any& value) { command.prev_value = value; return *this; }

    AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::new_value(const entt::meta_any& value) { command.new_value = value; return *this; }

    AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::push_path_data(entt::id_type id, const std::string& name)
    {
        command.meta_path.entries.push_back(
            MetaFieldPath::Entry{ .type = MetaFieldPath::Entry::Type::Data, .data_id = id, .name = name }
        );
        return *this;
    }

    AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::push_path_index(int index, const std::string& name)
    {
        command.meta_path.entries.push_back(
            MetaFieldPath::Entry{ .type = MetaFieldPath::Entry::Type::Index, .index = index, .name = name }
        );
        return *this;
    }

    AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::push_path_key(const entt::meta_any& key_any, const std::string& name)
    {
        command.meta_path.entries.push_back(
            MetaFieldPath::Entry{ .type = MetaFieldPath::Entry::Type::Key, .key_any = key_any/*.as_ref()*/, .name = name }
        );
        return *this;
    }

    AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::pop_path()
    {
        command.meta_path.entries.pop_back();
        return *this;
    }

    AssignComponentFieldCommandBuilder& AssignComponentFieldCommandBuilder::reset()
    {
        assert(!command.meta_path.entries.size() && "Meta path not empty when clearing");
        command = AssignComponentFieldCommand{};
        return *this;
    }

    AssignComponentFieldCommand AssignComponentFieldCommandBuilder::build()
    {
        // Valdiate before returning command instance 

        assert(!command.registry.expired() && "registry pointer expired");
        assert(command.entity.valid() && "entity invalid");
        assert(command.component_id != 0 && "component id invalid");

        assert(command.prev_value);
        assert(command.new_value);

        assert(command.meta_path.entries.size() && "Meta path empty");
        {
            bool last_was_index_or_key = false;
            for (int i = 0; i < command.meta_path.entries.size(); i++)
            {
                auto& entry = command.meta_path.entries[i];

                // First entry must be Data (enter data member of a component)
                assert(i > 0 || entry.type == MetaFieldPath::Entry::Type::Data);

                // Check so relevant values are set for each entry type
                assert(entry.type != MetaFieldPath::Entry::Type::None);
                assert(entry.type != MetaFieldPath::Entry::Type::Data || entry.data_id);
                assert(entry.type != MetaFieldPath::Entry::Type::Index || entry.index > -1);
                assert(entry.type != MetaFieldPath::Entry::Type::Key || entry.key_any);

                last_was_index_or_key =
                    entry.type == MetaFieldPath::Entry::Type::Index ||
                    entry.type == MetaFieldPath::Entry::Type::Key;
            }
        }

        // Build a display name for the command
        {
            // Gather meta path
            command.display_name = meta::get_meta_type_display_name(entt::resolve(command.component_id));
            for (auto& entry : command.meta_path.entries)
            {
                if (entry.type == MetaFieldPath::Entry::Type::Data) {
                    command.display_name += "::" + entry.name;
                }
                else if (entry.type == MetaFieldPath::Entry::Type::Index) {
                    command.display_name += "[" + std::to_string(entry.index) + "]";
                }
                else if (entry.type == MetaFieldPath::Entry::Type::Key) {
                    if (auto j_new = meta::serialize_any(entry.key_any); !j_new.is_null())
                        command.display_name += "[" + j_new.dump() + "]";
                    else
                        command.display_name += "[]";
                }
            }
            // Serialize new value for display
            if (auto j_new = meta::serialize_any(command.new_value); !j_new.is_null())
                command.display_name += " = " + j_new.dump();
        }

        return command;
    }

} // namespace Editor