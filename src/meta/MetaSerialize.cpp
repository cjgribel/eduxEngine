// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "MetaSerialize.hpp"
#include "config.h"
#include "MetaLiterals.h"
#include "MetaAux.h"
#include "EngineContext.hpp"
#include <iostream>
#include <sstream>
#include <cassert>
#include <nlohmann/json.hpp>

namespace eeng::meta
{
#if 0
    /*
        NOTE
        Instead of using this meta registered function (which requires a
        temporary entity):

            registry.get_or_emplace<Component>(entity)

        register this function instead:

            template<typename T>
            void assure_component_storage(entt::registry& registry)
            {
                if (!registry.storage<T>()) registry.storage<T>();
            }
    */
    void ensure_storage(entt::registry& registry, entt::id_type component_id)
    {
        // Check if the storage for the component exists
        if (!registry.storage(component_id))
        {
            auto meta_type = entt::resolve(component_id);
            assert(meta_type); // Component type must be registered with entt::meta

            // Temporary entity
            entt::entity temp_entity = registry.create();

            // Use meta function to emplace a default instance of the component
            auto emplace_func = meta_type.func("emplace_component"_hs);
            assert(emplace_func);
            emplace_func.invoke({}, entt::forward_as_meta(registry), temp_entity);

            // Destroy the temporary entity
            registry.destroy(temp_entity);

            // std::cout << "Ceeated storage for " << get_meta_type_name(meta_type) << std::endl;
        }
    }
#endif
    nlohmann::json serialize_any(const entt::meta_any& any)
    {
        assert(any);
        nlohmann::json json;

        if (entt::meta_type meta_type = entt::resolve(any.type().id()); meta_type)
        {
            if (entt::meta_func meta_func = meta_type.func(literals::serialize_hs); meta_func)
            {
                auto res = meta_func.invoke(
                    {},
                    entt::forward_as_meta(json),
                    entt::forward_as_meta(any)
                    // any.base().data()
                );
                assert(res && "Failed to invoke serialize");
#ifdef SERIALIZATION_DEBUG_PRINTS
                std::cout << "serialize invoked: " << json.dump() << std::endl;
#endif
            }
            else if (meta_type.is_enum())
            {
                // std::cout << " [is_enum]";
#if 0
                // Cast to underlying meta type
                auto any_conv = cast_to_underlying_type(meta_type, any);

                auto enum_entries = gather_meta_enum_entries(any);
                // Look for entry with current value
                auto entry = std::find_if(enum_entries.begin(), enum_entries.end(), [&any_conv](auto& e) {
                    return e.second == any_conv;
                    });
                assert(entry != enum_entries.end());
                // Push entry name to json
                json = entry->first;
#endif
                json = enum_value_name(any);
            }
            else
            {
                // to_json() not available: traverse data members
                for (auto&& [id, meta_data] : meta_type.data())
                {
                    // JSON key name is the display name if present, or the id type
                    std::string key_name = get_meta_data_nice_name(id, meta_data);

                    auto field_any = meta_data.get(any);
                    json[key_name] = serialize_any(field_any);
                }
            }

            return json;
        }

        // any is not a meta type

        if (any.type().is_sequence_container())
        {
            auto view = any.as_sequence_container();
            assert(view && "as_sequence_container() failed");

            auto json_array = nlohmann::json::array();
            for (auto&& v : view)
            {
                json_array.push_back(serialize_any(v));
            }
            json = json_array;
        }
        else if (any.type().is_associative_container())
        {
            auto view = any.as_associative_container();
            assert(view && "as_associative_container() failed");

            // JSON structure,
            // mapped container:    [[key1, val1], [key2, val2], ...]
            // set type:            [key1, key2, ...]
            auto json_array = nlohmann::json::array();

            for (auto&& [key_any, mapped_any] : view)
            {
                if (view.mapped_type())
                {
                    // Store key & value as a sub-array in the container array
                    nlohmann::json json_elem{
                        serialize_any(key_any), serialize_any(mapped_any)
                    };
                    json_array.push_back(json_elem);
                }
                else
                    // Store key in the container array
                    json_array.push_back(serialize_any(key_any));
            }
            json = json_array;
        }
        else
        {
            bool res = try_apply(any, [&json](auto& value)
                {
                    json = value;
                });
            if (!res)
                throw std::runtime_error(std::string("Unable to cast ") + get_meta_type_name(any.type()));
        }

        return json;
    }
#if 0
    nlohmann::json serialize_entity(
        const Entity& entity,
        std::shared_ptr<entt::registry>& registry)
    {
        std::cout << "Serializing entity "
            << entity.to_integral() << std::endl;

        nlohmann::json entity_json;
        entity_json["entity"] = entity.to_integral();

        // For all component types
        for (auto&& [id, type] : registry->storage())
        {
            if (!type.contains(entity)) continue;

            if (entt::meta_type meta_type = entt::resolve(id); meta_type)
            {
                auto key_name = std::string{ meta_type.info().name() }; // Better for serialization?
                // auto type_name = get_meta_type_name(meta_type); // Display name or mangled name

                entity_json["components"][key_name] = serialize_any(meta_type.from_void(type.value(entity)));
            }
            else
            {
                assert(false && "No meta type for component");
            }
        }
        return entity_json;
    }

#if 0
    std::optional<ResourceHandle> deserialize_resource(const nlohmann::json& json, AssetDatabase& db, ResourceRegistry& registry) {
        auto type_name = json["type"].get<std::string>();
        auto guid_str = json["guid"].get<std::string>();
        auto type_id = entt::hashed_string::value(type_name.c_str());

        if (auto meta_type = entt::resolve(type_id)) {
            Guid guid = Guid::from_string(guid_str);

            // Load resource from AssetDatabase using templated dispatch via meta-type
            entt::meta_any resource_any = meta_type.construct();

            if (deserialize_any(json["data"], resource_any)) {
                // Register to ResourceRegistry
                return registry.add_resource(guid, resource_any);
            }
        }

        return std::nullopt;
    }
#endif

    nlohmann::json serialize_entities(
        Entity* entity_first,
        int count,
        std::shared_ptr<entt::registry>& registry)
    {
        nlohmann::json json = nlohmann::json::array();

        for (int i = 0; i < count; i++)
            json.push_back(serialize_entity(*(entity_first + i), registry));

        return json;
    }

    nlohmann::json serialize_registry(std::shared_ptr<entt::registry>& registry)
    {
#if 1
        std::vector<Entity> entities;

        auto view = registry->view<entt::entity>();
        entities.reserve(view.size_hint());
        for (auto entity : view)
            entities.push_back(Entity{ entity });

        return serialize_entities(entities.data(), entities.size(), registry);
#else
        nlohmann::json json = nlohmann::json::array();

        auto view = registry->template view<entt::entity>();
        for (auto entity : view)
        {
#if 1
            json.push_back(serialize_entity(entity, registry));
#else
            std::cout << "Serializing entity "
                << entt::to_integral(entity) << std::endl;

            nlohmann::json entity_json;
            entity_json["entity"] = entt::to_integral(entity);

            // For all component types
            for (auto&& [id, type] : registry->storage())
            {
                if (!type.contains(entity)) continue;

                if (entt::meta_type meta_type = entt::resolve(id); meta_type)
                {
                    auto key_name = std::string{ meta_type.info().name() }; // Better for serialization?
                    // auto type_name = get_meta_type_name(meta_type); // Inspector-friendly version

                    entity_json["components"][key_name] = serialize_any(meta_type.from_void(type.value(entity)));
                }
                else
                {
                    assert(false && "Meta-type required");
                }
            }
            json.push_back(entity_json);
#endif
        }

        return json;
#endif
    }
#endif
    void deserialize_any(
        const nlohmann::json& json,
        entt::meta_any& any,
        const ecs::Entity& entity, // SKIP THIS SOMEHOW?
        EngineContext& context)
    {
        assert(any);

        if (entt::meta_type meta_type = entt::resolve(any.type().id()); meta_type)
        {
            if (entt::meta_func meta_func = meta_type.func(literals::deserialize_hs); meta_func)
            {
                // 1) Try signature (json, any)
                entt::meta_any res = meta_func.invoke(
                    {},
                    entt::forward_as_meta(json),
                    entt::forward_as_meta(any)
                );

                if (!res) {
                    // 2) Try signature (json, any, context)
                    res = meta_func.invoke(
                        {},
                        entt::forward_as_meta(json),
                        entt::forward_as_meta(any),
                        entt::forward_as_meta(context)
                    );
                }

                if (!res) {
                    // 3) Try signature (json, any, entity, context)
                    res = meta_func.invoke(
                        {},
                        entt::forward_as_meta(json),
                        entt::forward_as_meta(any),
                        entt::forward_as_meta(entity),
                        entt::forward_as_meta(context)
                    );
                }

                // If none succeeded, fail
                assert(res && "Failed to invoke deserialize");
            }
            else if (meta_type.is_enum())
            {
                // std::cout << " [is_enum]";

                assert(json.is_string());
                auto entry_name = json.get<std::string>();

                auto enum_entries = gather_meta_enum_entries(any);
                //            // Look for entry with a matching name
                const auto entry = std::find_if(enum_entries.begin(), enum_entries.end(), [&entry_name](auto& e) {
                    return e.first == entry_name;
                    });
                assert(entry != enum_entries.end());

                // Update any value
#if 1
                auto any_conv = entry->second.allow_cast(meta_type);
                bool r = any.assign(any_conv);
#else
                bool r = any.assign(entry->second);
#endif
                assert(r);
            }
            else
            {
                // from_json() not available: traverse data members
                for (auto&& [id, meta_data] : meta_type.data())
                {
                    // JSON key name is the display name if present, or the id type
                    std::string key_name = get_meta_data_nice_name(id, meta_data);

                    entt::meta_any field_any = meta_data.get(any);
                    deserialize_any(json[key_name], field_any, entity, context);
                    meta_data.set(any, field_any);
                }
            }
            return;
        }

        // any is not a meta type

        if (any.type().is_sequence_container())
        {
            auto view = any.as_sequence_container();
            assert(view && "as_sequence_container() failed");

            // Resize and then deserialize in-place
            // Note: This approach is necessary to make fixed-size containers without insertion to work (std::array)
#define SEQDESER_ALT

#ifndef SEQDESER_ALT
            // Clear before adding deserialized elements one by one
            view.clear();
#else
            // Resize meta container to fit json array
            // As expected, seems to do nothing for fixed-size containers such as std::array
            view.resize(json.size());
#endif
            assert(json.is_array());

            for (int i = 0; i < json.size(); i++)
            {
#ifndef SEQDESER_ALT
                entt::meta_any elem_any = view.value_type().construct();
                deserialize_any(json[i], elem_any, entity, context);
                view.insert(view.end(), elem_any);
#else
                entt::meta_any elem_any = view[i]; // view[i].as_ref() works too
                deserialize_any(json[i], elem_any, entity, context); // TODO: view[i] here if &&
#endif
            }
        }

        else if (any.type().is_associative_container())
        {
            auto view = any.as_associative_container();
            assert(view && "as_associative_container() failed");

            // Clear meta container before populating it with json array
            view.clear();

            // JSON structure,
            // mapped container:    [[key1, val1], [key2, val2], ...]
            // set type:            [key1, key2, ...]
            assert(json.is_array());

            for (int i = 0; i < json.size(); i++)
            {
                auto json_elem = json[i];
                if (json_elem.is_array()) // Key & mapped value
                {
                    entt::meta_any key_any = view.key_type().construct();
                    entt::meta_any mapped_any = view.mapped_type().construct();
                    deserialize_any(json_elem[0], key_any, entity, context);
                    deserialize_any(json_elem[1], mapped_any, entity, context);
                    view.insert(key_any, mapped_any);
                }
                else // Just key
                {
                    entt::meta_any key_any = view.key_type().construct();
                    deserialize_any(json_elem, key_any, entity, context);
                    view.insert(key_any);
                }
            }
        }

        else
        {
            // Try casting the meta_any to a primitive type.
            bool res = try_apply(any, [&json, &any](auto& value)
                {
                    // Stript away any constness
                    using Type = std::decay_t<decltype(value)>;

                    // Assign a new value to the stored object
                    // Note: any = json.get<Type>() *replaces* the stored object
                    any.assign(json.get<Type>());
                });
            if (!res)
                throw std::runtime_error(std::string("Unable to cast ") + get_meta_type_name(any.type()));
        }
    }
#if 0
    // deserialize_component

    Entity deserialize_entity(
        const nlohmann::json& json,
        Editor::Context& context
    )
    {
        Entity entity_hint{ json["entity"].get<entt::entity>() };
        auto entity = context.entity_remap.at(entity_hint);

        // assert(json.contains("entity"));
        // entt::entity entity_hint = json["entity"].get<entt::entity>();
        // assert(!context.registry->valid(entity_hint));

        // auto entity = Entity{ context.registry->create(entity_hint) };
        // assert(entity_hint == entity);

        std::cout << "Deserializing entity " << entity.to_integral() << std::endl;

        assert(json.contains("components"));
        for (const auto& component_json : json["components"].items())
        {
            auto key_str = component_json.key().c_str();
            auto id = entt::hashed_string::value(key_str);

            if (entt::meta_type meta_type = entt::resolve(id); meta_type)
            {
                // Default-construct component
                entt::meta_any any = meta_type.construct();
                // Deserialize
                deserialize_any(component_json.value(), any, entity, context);
                // Ensure storage and add component to entity
                ensure_storage(*context.registry, id);
                // assert(context.registry->storage(id));
                context.registry->storage(id)->push(entity, any.data());
            }
            else
            {
                assert(false && "Deserialized component is not a meta-type");
            }
        }

        // Register to scene graph and chunk
        // assert(context.register_entity_func);
        // context.register_entity_func(entity);

        return entity;
    }

    void deserialize_entities(
        const nlohmann::json& json,
        Editor::Context& context)
    {
        assert(json.is_array());
        std::vector<Entity> entity_buffer; // For deferred scene graph registration
        entity_buffer.reserve(json.size());

        // Map serialized entities to new or reused values depending on availability
        context.entity_remap.clear();
        for (const auto& entity_json : json)
        {
            assert(entity_json.contains("entity"));
            Entity entity_hint{ entity_json["entity"].get<entt::entity>() };

            Entity entity;
            if (context.registry->valid(entity_hint))
                entity = Entity{ context.registry->create() };
            else
                entity = Entity{ context.registry->create(entity_hint) };
            context.entity_remap.insert({ entity_hint, entity });
        }

        // Deserialize entities
        for (const auto& entity_json : json)
        {
            auto entity = deserialize_entity(entity_json, context);
            entity_buffer.push_back(entity);
        }

        // Deferred scene graph registration since we're not expecting
        // a certain entity deserialization order
#if 1
        int pivot = 0; // First unregistered entity index
        while (pivot < entity_buffer.size())
        {
            bool swap_made = false;
            for (int i = pivot; i < entity_buffer.size(); i++)
            {
                auto& entity = entity_buffer.at(i);
                if (!context.can_register_entity(entity)) continue;

                context.register_entity(entity);
                std::swap(entity, entity_buffer[pivot++]);
                swap_made = true;
            }
            if (!swap_made)
            {
                std::cerr << "Corrupt hierarchy: Unable to register entities due to missing or circular dependencies.\n";
                for (int i = pivot; i < entity_buffer.size(); ++i)
                    std::cerr << "Unregistered entity: " << entity_buffer[i].to_integral() << "\n";
                throw std::runtime_error("Entity parent-child relationships corrupt");
            }
        }
#else
        int max_size = entity_buffer.size();
        int i = 0;
        while (entity_buffer.size())
        {
            int j = i % entity_buffer.size();
            auto& entity = entity_buffer.at(j);
            if (context.entity_parent_registered_func(entity))
            {
                context.register_entity_func(entity);
                entity_buffer.erase(entity_buffer.begin() + j);
            }
            else
            {
                i++;
                assert(i < max_size * max_size && "Entity parent-child relationships corrupt");
            }
        }
#endif
    }
#endif
} // namespace eeng::meta