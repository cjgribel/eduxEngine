// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/MetaFieldAssign.hpp"
#include <entt/entt.hpp>

namespace
{
    bool is_ref(const entt::meta_any& any)
    {
        return any.base().policy() == entt::any_policy::ref;
    }
}

namespace eeng::editor
{
    bool assign_meta_field_recursive(
        entt::meta_any& obj,
        const eeng::editor::MetaFieldPath& path,
        std::size_t idx,
        const entt::meta_any& leaf_value)
    {
        using Entry = eeng::editor::MetaFieldPath::Entry;

        const auto& entry = path.entries[idx];
        const bool is_leaf = (idx + 1 == path.entries.size());

        // ---- LEAF CASE ----
        if (is_leaf)
        {
            switch (entry.type)
            {
            case Entry::Type::Data:
            {
                entt::meta_type type = obj.type();
                entt::meta_data field = type.data(entry.data_id);
                if (!field)
                    return false;
                return field.set(obj, leaf_value);
            }

            case Entry::Type::Index:
            {
                auto seq = obj.as_sequence_container();
                if (!seq || entry.index < 0 || entry.index >= seq.size())
                    return false;
                seq[entry.index].assign(leaf_value);
                return true;
            }

            case Entry::Type::Key:
            {
                auto assoc = obj.as_associative_container();
                if (!assoc)
                    return false;
                auto it = assoc.find(entry.key_any);
                if (it == assoc.end())
                    return false;
                it->second.assign(leaf_value);
                return true;
            }

            default:
                return false;
            }
        }

        // ---- NON-LEAF CASE ----
        switch (entry.type)
        {
        case Entry::Type::Data:
        {
            entt::meta_type type = obj.type();
            entt::meta_data field = type.data(entry.data_id);
            if (!field)
                return false;

            // Get a VALUE copy of the subobject
            entt::meta_any sub = field.get(obj);
            if (!sub)
                return false;

            // Recurse into subobject
            if (!assign_meta_field_recursive(sub, path, idx + 1, leaf_value))
                return false;

            // Write updated subobject back into obj
            return field.set(obj, sub);
        }

        case Entry::Type::Index:
        {
            auto seq = obj.as_sequence_container();
            if (!seq || entry.index < 0 || entry.index >= seq.size())
                return false;

            // VALUE copy of element
            entt::meta_any element = seq[entry.index];
            if (!element)
                return false;

            if (!assign_meta_field_recursive(element, path, idx + 1, leaf_value))
                return false;

            // Write updated element back into container
            seq[entry.index].assign(element);
            return true;
        }

        case Entry::Type::Key:
        {
            auto assoc = obj.as_associative_container();
            if (!assoc)
                return false;

            auto it = assoc.find(entry.key_any);
            if (it == assoc.end())
                return false;

            // VALUE copy of element
            entt::meta_any element = it->second;
            if (!element)
                return false;

            if (!assign_meta_field_recursive(element, path, idx + 1, leaf_value))
                return false;

            // Write updated element back into container
            it->second.assign(element);
            return true;
        }

        default:
            return false;
        }
    }

    //void ComponentCommand::traverse_and_set_meta_type(entt::meta_any& value_any)
    bool assign_meta_field_stackbased(
        entt::meta_any& meta_any,
        const eeng::editor::MetaFieldPath& meta_path,
        //std::size_t idx,
        const entt::meta_any& value_any)
    {
        using namespace eeng::editor;
        using EntryType = MetaFieldPath::Entry::Type;
        // assert(!registry.expired());
        // auto registry_sp = registry.lock();

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
//        auto type = registry_sp->storage(component_id);
//        assert(type->contains(entity));
        entt::meta_type meta_type = meta_any.type(); // entt::resolve(component_id);
        // from_void returns "a wrapper that references the given instance"
//        entt::meta_any meta_any = meta_type.from_void(type->value(entity));

        // 1.   Use meta path to build a stack with values copied from the specific component
        // 2.   Iterate stack and assign values, from the edited data field up to the component itself 
        struct Property {
            entt::meta_any meta_any; entt::meta_data meta_data; MetaFieldPath::Entry entry;

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
        std::cout << "entry 0: " << meta::get_meta_type_display_name(last_prop.meta_any.type()) << std::endl;
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
            std::cout << "entry " << i << ": " << meta::get_meta_type_display_name(last_prop.meta_any.type())  << std::endl;
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
            std::cout << meta::get_meta_type_display_name(prop.meta_any.type()) << " ";     // object type
            std::cout << any_to_string(prop.meta_any) << std::endl;     // object value
            std::cout << prop.entry.name << " =>" << std::endl;         // property name/index
            std::cout << meta::get_meta_type_display_name(any_new.type()) << " ";           // new property type
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

        return true; // Throws on errors
        }
            } // namespace eeng::editor