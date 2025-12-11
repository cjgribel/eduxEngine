//
//  EditComponentCommand.cpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#include <iostream>
#include <cassert>
#include <stack>
//#include <nlohmann/json.hpp>
#include "MetaLiterals.h"
#include "MetaSerialize.hpp"
#include "EditComponentCommand.hpp"

// TODO -> IEditComponentCommand ???
namespace
{
    bool apply_path_and_set(entt::meta_any& root,
        const eeng::editor::MetaPath& path,
        const entt::meta_any& value)
    {
        using namespace eeng::editor;
        using Entry = MetaPath::Entry;

        // Start from a reference view of the root component
        entt::meta_any current = root.as_ref();

#ifdef COMMAND_DEBUG_PRINTS
        std::cout << "apply_path_and_set: root.policy = "
            << policy_to_string(root.base().policy()) << std::endl;
        std::cout << "apply_path_and_set: current.policy = "
            << policy_to_string(current.base().policy()) << std::endl;
#endif

        if (path.entries.empty())
            return false;

        // Traverse all but last
        for (size_t i = 0; i + 1 < path.entries.size(); ++i)
        {
            const auto& entry = path.entries[i];

            switch (entry.type)
            {
            case Entry::Type::Data:
            {
                auto field = current.type().data(entry.data_id);
                if (!field) return false;

                entt::meta_any next = field.get(current);
                if (!next) return false;

                // Keep aliasing as we descend
                current = next.as_ref();
                break;
            }

            case Entry::Type::Index:
            {
                auto seq = current.as_sequence_container();
                if (!seq || entry.index < 0 || entry.index >= seq.size()) return false;

                entt::meta_any next = seq[entry.index];
                if (!next) return false;

                current = next.as_ref();
                break;
            }

            case Entry::Type::Key:
            {
                auto assoc = current.as_associative_container();
                if (!assoc) return false;

                auto it = assoc.find(entry.key_any);
                if (it == assoc.end()) return false;

                entt::meta_any next = it->second;
                if (!next) return false;

                current = next.as_ref();
                break;
            }

            default:
                return false;
            }
        }

        const auto& leaf = path.entries.back();

        switch (leaf.type)
        {
        case Entry::Type::Data:
        {
            auto field = current.type().data(leaf.data_id);
            if (!field) return false;
            return field.set(current, value);
        }

        case Entry::Type::Index:
        {
            auto seq = current.as_sequence_container();
            if (!seq || leaf.index < 0 || leaf.index >= seq.size()) return false;
            seq[leaf.index].assign(value);
            return true;
        }

        case Entry::Type::Key:
        {
            auto assoc = current.as_associative_container();
            if (!assoc) return false;
            auto it = assoc.find(leaf.key_any);
            if (it == assoc.end()) return false;
            it->second.assign(value);
            return true;
        }

        default:
            return false;
        }
    }
}

namespace
{
    // bool is_ref(const entt::meta_any& any)

    bool is_ref(const entt::meta_any& any)
    {
        return any.base().policy() == entt::any_policy::ref; // as_ref_t::value(); // entt::meta_any::policy::reference;

        // // DOOES NOT WORK
        // // Create a reference-based meta_any from the original
        // auto ref_any = any.as_ref();
        // // Compare addresses: if the original `any` was already a reference, 
        // // `ref_any` should match it in content (same underlying object)
        // return ref_any == any;
    }
}

namespace eeng::editor
{

#if 0
    void ComponentCommand::traverse_and_set_meta_type(entt::meta_any& value_any)
    {
        //         auto registry_sp = registry.lock();
        //         assert(registry_sp && "Registry expired");

        //         // Get a handle to the entity
        //         entt::handle handle{ *registry_sp, entity };
        //         // Get the component as a meta_any (this should be by-ref)
        //         entt::meta_any root = handle.get(component_id);
        //         assert(root && "Failed to get component as meta_any");

        // #ifdef COMMAND_DEBUG_PRINTS
        //         std::cout << "root.policy = "
        //             << policy_to_string(root.base().policy()) << std::endl;
        // #endif

        //         const bool ok = apply_path_and_set(root, meta_path, value_any);
        //         assert(ok && "Failed to set field");

        auto registry_sp = registry.lock();
        assert(registry_sp && "Registry expired");

        auto* storage = registry_sp->storage(component_id);
        assert(storage && storage->contains(entity));

        entt::meta_type meta_type = entt::resolve(component_id);
        entt::meta_any root = meta_type.from_void(storage->value(entity));
        assert(root && "Failed to get component as meta_any");

        const bool ok = apply_path_and_set(root, meta_path, value_any);
        assert(ok && "Failed to set field");

        // After this, the component in the registry is actually modified.
        // Now we can run per-field callbacks or emit events.
    }
#else
    void ComponentCommand::traverse_and_set_meta_type(entt::meta_any& value_any)
    {
        using EntryType = MetaPath::Entry::Type;
        assert(!registry.expired());
        auto registry_sp = registry.lock();

#ifdef COMMAND_DEBUG_PRINTS
        auto any_to_string = [](const entt::meta_any any) -> std::string {
            if (auto j = Meta::serialize_any(any); !j.is_null())
                return j.dump();
            return "n/a";
            };

        std::cout << "Executing command" << std::endl;
        std::cout << "\t" << get_name() << std::endl;
        std::cout << "\tentity " << entt::to_integral(entity);
        std::cout << ", component type " << component_id << std::endl;
        std::cout << "\tprev value: " << any_to_string(prev_value) << std::endl;
        std::cout << "\tnew value: " << any_to_string(new_value) << std::endl;
#endif   

        // Component
        auto type = registry_sp->storage(component_id);
        assert(type->contains(entity));
        entt::meta_type meta_type = entt::resolve(component_id);
        // from_void returns "a wrapper that references the given instance"
        entt::meta_any meta_any = meta_type.from_void(type->value(entity));

        // 1.   Use meta path to build a stack with values copied from the specific component
        // 2.   Iterate stack and assign values, from the edited data field up to the component itself 
        struct Property {
            entt::meta_any meta_any; entt::meta_data meta_data; MetaPath::Entry entry;

            // explicit Property(entt::meta_any& meta_any, const entt::meta_data& meta_data, const MetaPath::Entry& entry)
            //     : meta_any{ is_ref(meta_any) ? meta_any : meta_any.as_ref() },
            //     meta_data(meta_data),
            //     entry(entry) { }
        };
        std::stack<Property> prop_stack;

        // Push first data path entry manually (meta_any is the component itself)
        auto& entry0 = meta_path.entries[0];
        const entt::meta_data meta_data0 = meta_type.data(entry0.data_id);
        Property last_prop{ meta_any, meta_data0, entry0 };
        prop_stack.push(last_prop);
        prop_stack.top().meta_any = meta_any.as_ref(); // TODO
        assert(is_ref(prop_stack.top().meta_any)); // TODO

        //entt::as_ref_t
        //meta_any.policy
        // entt::meta_any any = meta_any;
        // //assert(any == any.as_ref());
        // entt::meta_any cpy2 = meta_any.as_ref();
        // std::cout << "is_ref meta_any " << is_ref(meta_any) << std::endl;
        // std::cout << "is_ref meta_any.as_ref() " << is_ref(meta_any.as_ref()) << std::endl;
        // std::cout << "is_ref cpy1 " << is_ref(any) << std::endl;
        // std::cout << "is_ref cpy2 " << is_ref(cpy2) << std::endl;

#ifdef COMMAND_DEBUG_PRINTS
        std::cout << "building property stack..." << std::endl;
        std::cout << "entry 0: " << last_prop.meta_any.type().info().name() << std::endl;
#endif

        // Push the remaining path entries
        int i = 1;
        for (;i < meta_path.entries.size(); i++)
        {
            auto& e = meta_path.entries[i];
            if (e.type == EntryType::Data)
            {
                entt::meta_any meta_any;
                if (last_prop.entry.type == EntryType::Index)
                {
                    meta_any = last_prop.meta_any.as_sequence_container()[last_prop.entry.index];
                }
                else if (last_prop.entry.type == EntryType::Key)
                {
                    meta_any = last_prop.meta_any.as_associative_container().find(last_prop.entry.key_any)->second;
                }
                else if (last_prop.entry.type == EntryType::Data)
                {
                    meta_any = last_prop.meta_data.get(last_prop.meta_any);
                }
                else { assert(0); }
                assert(meta_any);
                // auto meta_any = last_prop.meta_data.get(last_prop.meta_any); assert(meta_any);
                auto meta_type = entt::resolve(meta_any.type().id());  assert(meta_type);
                auto meta_data = meta_type.data(e.data_id); assert(meta_data);

                last_prop = Property{ meta_any, meta_data, e };
                prop_stack.push(last_prop);
            }
            else if (e.type == EntryType::Index)
            {
                assert(last_prop.entry.type == EntryType::Data);
                auto meta_any = last_prop.meta_data.get(last_prop.meta_any); assert(meta_any); // = container

                // Validate index
                assert(meta_any.type().is_sequence_container());
                assert(e.index < meta_any.as_sequence_container().size());

                last_prop = Property{ meta_any, entt::meta_data{}, e };
                prop_stack.push(last_prop);
            }
            else if (e.type == EntryType::Key)
            {
                assert(last_prop.entry.type == EntryType::Data);
                auto meta_any = last_prop.meta_data.get(last_prop.meta_any); assert(meta_any); // = container

                // Validate key
                assert(meta_any.type().is_associative_container());
                assert(meta_any.as_associative_container().find(e.key_any));

                last_prop = Property{ meta_any, entt::meta_data{}, e };
                prop_stack.push(last_prop);
            }
            else { assert(0); }
#ifdef COMMAND_DEBUG_PRINTS
            std::cout << "entry " << i << ": " << last_prop.meta_any.type().info().name() << std::endl;
#endif
        }

#ifdef COMMAND_DEBUG_PRINTS
        std::cout << "assigning new values..." << std::endl;
#endif
        entt::meta_any any_new = value_any;

        // Iterate stack and assign new values
        while (!prop_stack.empty())
        {
            auto& prop = prop_stack.top();
#ifdef COMMAND_DEBUG_PRINTS
            std::cout << "Property " << prop_stack.size() << std::endl;
            std::cout << "object:" << std::endl;
            std::cout << prop.meta_any.type().info().name() << " ";     // object type
            std::cout << any_to_string(prop.meta_any) << std::endl;     // object value
            std::cout << prop.entry.name << " =>" << std::endl;         // property name/index
            std::cout << any_new.type().info().name() << " ";           // new property type
            std::cout << any_to_string(any_new) << std::endl;           // new property value
#endif
            if (prop.entry.type == EntryType::Data)
            {
                bool res = prop.meta_data.set(prop.meta_any, any_new); assert(res); //break;
            }
            else if (prop.entry.type == EntryType::Index)
            {
                auto view = prop.meta_any.as_sequence_container();
                // Index already validated
                view[prop.entry.index].assign(any_new);
            }
            else if (prop.entry.type == EntryType::Key)
            {
                auto view = prop.meta_any.as_associative_container();
                // Key already validated
                auto pair = view.find(prop.entry.key_any);
                pair->second.assign(any_new);
            }
            else { assert(0); }

            any_new = prop.meta_any;
            prop_stack.pop();
        }

#ifdef COMMAND_DEBUG_PRINTS
        std::cout << "Object before:" << std::endl;
        std::cout << any_to_string(meta_any) << std::endl;
#endif

        // At this point, any_new is an updated COPY of the component:
        // assign it to the in-memory component
        // This works since
        //      * meta_type.from_void(type.value(entity)) is a REFERENCE
        //      * We do this above: prop_stack.top().meta_any = meta_any.as_ref();
        // meta_any.assign(any_new);

#ifdef COMMAND_DEBUG_PRINTS
        std::cout << "Object after:" << std::endl;
        std::cout << any_to_string(meta_any) << std::endl;
        std::cout << "Done executing command" << std::endl;
#endif

#if 0
        // Call data field callback if present
        {
            // meta_data is the actual data field that was edited
            using TypeModifiedCallbackType = std::function<void(entt::meta_any, const ecs::Entity&)>;
            if (auto prop = meta_data0.prop("callback"_hs); prop)
            {
                if (auto ptr = prop.value().try_cast<TypeModifiedCallbackType>(); ptr)
                    ptr->operator()(value_any, entity);
                else { assert(0); }
            }
        }
#endif
    }
#endif

    void ComponentCommand::execute()
    {
        traverse_and_set_meta_type(new_value);
    }

    void ComponentCommand::undo()
    {
        traverse_and_set_meta_type(prev_value);
    }

    std::string ComponentCommand::get_name() const
    {
        return display_name;
    }

    ComponentCommandBuilder& ComponentCommandBuilder::registry(std::weak_ptr<entt::registry> registry) { command.registry = registry; return *this; }

    ComponentCommandBuilder& ComponentCommandBuilder::entity(const ecs::Entity& entity) { command.entity = entity; return *this; }

    ComponentCommandBuilder& ComponentCommandBuilder::component(entt::id_type id) { command.component_id = id; return *this; }

    ComponentCommandBuilder& ComponentCommandBuilder::prev_value(const entt::meta_any& value) { command.prev_value = value; return *this; }

    ComponentCommandBuilder& ComponentCommandBuilder::new_value(const entt::meta_any& value) { command.new_value = value; return *this; }

    ComponentCommandBuilder& ComponentCommandBuilder::push_path_data(entt::id_type id, const std::string& name)
    {
        command.meta_path.entries.push_back(
            MetaPath::Entry{ .type = MetaPath::Entry::Type::Data, .data_id = id, .name = name }
        );
        return *this;
    }

    ComponentCommandBuilder& ComponentCommandBuilder::push_path_index(int index, const std::string& name)
    {
        command.meta_path.entries.push_back(
            MetaPath::Entry{ .type = MetaPath::Entry::Type::Index, .index = index, .name = name }
        );
        return *this;
    }

    ComponentCommandBuilder& ComponentCommandBuilder::push_path_key(const entt::meta_any& key_any, const std::string& name)
    {
        command.meta_path.entries.push_back(
            MetaPath::Entry{ .type = MetaPath::Entry::Type::Key, .key_any = key_any/*.as_ref()*/, .name = name }
        );
        return *this;
    }

    ComponentCommandBuilder& ComponentCommandBuilder::pop_path()
    {
        command.meta_path.entries.pop_back();
        return *this;
    }

    ComponentCommandBuilder& ComponentCommandBuilder::reset()
    {
        assert(!command.meta_path.entries.size() && "Meta path not empty when clearing");
        command = ComponentCommand{};
        return *this;
    }

    ComponentCommand ComponentCommandBuilder::build()
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
                assert(i > 0 || entry.type == MetaPath::Entry::Type::Data);

                // Check so relevant values are set for each entry type
                assert(entry.type != MetaPath::Entry::Type::None);
                assert(entry.type != MetaPath::Entry::Type::Data || entry.data_id);
                assert(entry.type != MetaPath::Entry::Type::Index || entry.index > -1);
                assert(entry.type != MetaPath::Entry::Type::Key || entry.key_any);

                last_was_index_or_key =
                    entry.type == MetaPath::Entry::Type::Index ||
                    entry.type == MetaPath::Entry::Type::Key;
            }
        }

        // Build a display name for the command
        {
            // Gather meta path
            command.display_name = entt::resolve(command.component_id).info().name();
            for (auto& entry : command.meta_path.entries)
            {
                if (entry.type == MetaPath::Entry::Type::Data) {
                    command.display_name += "::" + entry.name;
                }
                else if (entry.type == MetaPath::Entry::Type::Index) {
                    command.display_name += "[" + std::to_string(entry.index) + "]";
                }
                else if (entry.type == MetaPath::Entry::Type::Key) {
                    if (auto j_new = meta::serialize_any(entry.key_any); !j_new.is_null())
                        command.display_name += "[" + j_new.dump() + "]";
                    else
                        command.display_name += "[]";
                }
            }
            // Serialize new value
            if (auto j_new = meta::serialize_any(command.new_value); !j_new.is_null())
                command.display_name += " = " + j_new.dump();
        }

        return command;
    }

} // namespace Editor