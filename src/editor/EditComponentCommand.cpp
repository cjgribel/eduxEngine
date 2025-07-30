//
//  EditComponentCommand.cpp
//
//  Created by Carl Johan Gribel on 2024-10-14.
//  Copyright © 2024 Carl Johan Gribel. All rights reserved.
//

#include <iostream>
#include <cassert>
//#include <nlohmann/json.hpp>
#include "meta_literals.h"
#include "MetaSerialize.hpp"
#include "EditComponentCommand.hpp"

namespace
{
    // bool is_ref(const entt::meta_any& any)

    bool is_ref(const entt::meta_any& any)
    {
        return any.policy() == entt::meta_any_policy::ref; // as_ref_t::value(); // entt::meta_any::policy::reference;

        // // DOOES NOT WORK
        // // Create a reference-based meta_any from the original
        // auto ref_any = any.as_ref();
        // // Compare addresses: if the original `any` was already a reference, 
        // // `ref_any` should match it in content (same underlying object)
        // return ref_any == any;
    }
}

namespace Editor {

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

        // Call data field callback if present
        {
            // meta_data is the actual data field that was edited
            using TypeModifiedCallbackType = std::function<void(entt::meta_any, const Entity&)>;
            if (auto prop = meta_data0.prop("callback"_hs); prop)
            {
                if (auto ptr = prop.value().try_cast<TypeModifiedCallbackType>(); ptr)
                    ptr->operator()(value_any, entity);
                else { assert(0); }
            }
        }
    }

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

    ComponentCommandBuilder& ComponentCommandBuilder::entity(const Entity& entity) { command.entity = entity; return *this; }

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
        assert(!command.entity.is_null() && "entity invalid");
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
                    if (auto j_new = Meta::serialize_any(entry.key_any); !j_new.is_null())
                        command.display_name += "[" + j_new.dump() + "]";
                    else
                        command.display_name += "[]";
                }
            }
            // Serialize new value
            if (auto j_new = Meta::serialize_any(command.new_value); !j_new.is_null())
                command.display_name += " = " + j_new.dump();
        }

        return command;
    }

} // namespace Editor