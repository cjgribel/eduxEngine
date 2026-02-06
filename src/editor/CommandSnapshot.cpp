// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/CommandSnapshot.hpp"
#include "meta/MetaAux.h"

namespace eeng::editor
{
    namespace
    {
        // Cache header/parent field names so undo/redo JSON stays in sync with meta.
        HeaderJsonKeys resolve_header_keys()
        {
            HeaderJsonKeys keys{};

            auto header_type = eeng::meta::resolve_by_type_id_string("eeng.ecs.HeaderComponent");
            if (!header_type)
                return keys;

            keys.type_id = eeng::meta::get_meta_type_id_string(header_type);

            for (auto [id, meta_data] : header_type.data())
            {
                if (eeng::DataMetaInfo* info = meta_data.custom(); info)
                {
                    if (info->name == "guid")
                        keys.guid_key = get_meta_data_display_name(id, meta_data);
                    else if (info->name == "parent_entity")
                        keys.parent_key = get_meta_data_display_name(id, meta_data);
                }
            }

            auto entity_ref_type = eeng::meta::resolve_by_type_id_string("eeng.ecs.EntityRef");
            if (entity_ref_type)
            {
                for (auto [id, meta_data] : entity_ref_type.data())
                {
                    if (eeng::DataMetaInfo* info = meta_data.custom(); info && info->name == "guid")
                    {
                        keys.entityref_guid_key = get_meta_data_display_name(id, meta_data);
                        break;
                    }
                }
            }

            return keys;
        }
    }

    const HeaderJsonKeys& header_keys()
    {
        static HeaderJsonKeys keys{};
        static bool initialized = false;

        // Lazy init avoids meta queries until the editor first needs snapshot data.
        if (!initialized || keys.type_id.empty())
        {
            keys = resolve_header_keys();
            initialized = true;
        }

        return keys;
    }

    entt::id_type header_component_id()
    {
        // Cache the header component id for fast comparisons.
        static entt::id_type id = []()
            {
                auto header_type = eeng::meta::resolve_by_type_id_string("eeng.ecs.HeaderComponent");
                return header_type ? header_type.id() : entt::id_type{};
            }();
        return id;
    }

    bool update_entity_guid_in_json(nlohmann::json& entity_json, const Guid& guid)
    {
        // Update the primary entity guid and any component fields that mirror it.
        const auto& keys = header_keys();
        if (!entity_json.contains("components"))
            return false;

        const auto old_guid = entity_json.value("entity_guid", Guid::invalid().raw());
        entity_json["entity_guid"] = guid.raw();

        auto& components = entity_json["components"];
        if (!components.is_object())
            return false;

        auto update_guid_field = [&](nlohmann::json& comp_json) -> bool
            {
                if (!comp_json.is_object())
                    return false;

                if (!keys.guid_key.empty() && comp_json.contains(keys.guid_key))
                {
                    comp_json[keys.guid_key] = guid.raw();
                    return true;
                }

                if (comp_json.contains("Guid"))
                {
                    comp_json["Guid"] = guid.raw();
                    return true;
                }

                if (comp_json.contains("guid"))
                {
                    comp_json["guid"] = guid.raw();
                    return true;
                }

                return false;
            };

        if (!keys.type_id.empty())
        {
            auto it = components.find(keys.type_id);
            if (it != components.end() && update_guid_field(it.value()))
                return true;
        }

        for (auto& [comp_name, comp_json] : components.items())
        {
            if (!comp_json.is_object())
                continue;

            if (comp_json.contains("Parent Entity") || comp_json.contains("parent_entity"))
            {
                if (update_guid_field(comp_json))
                    return true;
            }
        }

        for (auto& [comp_name, comp_json] : components.items())
        {
            if (!comp_json.is_object())
                continue;

            if (comp_json.contains("Guid") && comp_json["Guid"].is_number_unsigned()
                && comp_json["Guid"].get<Guid::underlying_type>() == old_guid)
            {
                comp_json["Guid"] = guid.raw();
                return true;
            }

            if (comp_json.contains("guid") && comp_json["guid"].is_number_unsigned()
                && comp_json["guid"].get<Guid::underlying_type>() == old_guid)
            {
                comp_json["guid"] = guid.raw();
                return true;
            }
        }

        return false;
    }

    bool update_parent_guid_in_json(nlohmann::json& entity_json, const Guid& parent_guid)
    {
        // Update parent guid fields across supported schema variants.
        const auto& keys = header_keys();
        if (!entity_json.contains("components"))
            return false;

        auto& components = entity_json["components"];
        if (!components.is_object())
            return false;

        if (!keys.type_id.empty() && !keys.parent_key.empty())
        {
            auto it = components.find(keys.type_id);
            if (it != components.end() && it.value().is_object())
            {
                auto& header_json = it.value();
                if (header_json.contains(keys.parent_key))
                {
                    auto& parent_json = header_json[keys.parent_key];
                    if (!keys.entityref_guid_key.empty())
                    {
                        parent_json[keys.entityref_guid_key] = parent_guid.raw();
                        return true;
                    }
                    if (parent_json.contains("Guid"))
                    {
                        parent_json["Guid"] = parent_guid.raw();
                        return true;
                    }
                    if (parent_json.contains("guid"))
                    {
                        parent_json["guid"] = parent_guid.raw();
                        return true;
                    }
                }
            }
        }

        for (auto& [comp_name, comp_json] : components.items())
        {
            if (!comp_json.is_object())
                continue;

            if (comp_json.contains("Parent Entity"))
            {
                auto& parent_json = comp_json["Parent Entity"];
                if (parent_json.contains("Guid"))
                {
                    parent_json["Guid"] = parent_guid.raw();
                    return true;
                }
                if (parent_json.contains("guid"))
                {
                    parent_json["guid"] = parent_guid.raw();
                    return true;
                }
            }

            if (comp_json.contains("parent_entity"))
            {
                auto& parent_json = comp_json["parent_entity"];
                if (parent_json.contains("Guid"))
                {
                    parent_json["Guid"] = parent_guid.raw();
                    return true;
                }
                if (parent_json.contains("guid"))
                {
                    parent_json["guid"] = parent_guid.raw();
                    return true;
                }
            }
        }

        return false;
    }

    Guid guid_from_json(const nlohmann::json& entity_json)
    {
        // Read the entity guid from a snapshot payload.
        if (!entity_json.contains("entity_guid"))
            return Guid::invalid();
        return Guid{ entity_json["entity_guid"].get<Guid::underlying_type>() };
    }

    Guid parent_guid_from_json(const nlohmann::json& entity_json)
    {
        const auto& keys = header_keys();
        if (!entity_json.contains("components"))
            return Guid::invalid();

        const auto components_it = entity_json.find("components");
        if (components_it == entity_json.end() || !components_it->is_object())
            return Guid::invalid();

        auto read_parent_guid = [&](const nlohmann::json& parent_json) -> Guid
        {
            if (!parent_json.is_object())
                return Guid::invalid();

            if (!keys.entityref_guid_key.empty() && parent_json.contains(keys.entityref_guid_key))
                return Guid{ parent_json[keys.entityref_guid_key].get<Guid::underlying_type>() };
            if (parent_json.contains("Guid"))
                return Guid{ parent_json["Guid"].get<Guid::underlying_type>() };
            if (parent_json.contains("guid"))
                return Guid{ parent_json["guid"].get<Guid::underlying_type>() };
            return Guid::invalid();
        };

        if (!keys.type_id.empty() && !keys.parent_key.empty())
        {
            auto it = components_it->find(keys.type_id);
            if (it != components_it->end() && it.value().is_object())
            {
                auto& header_json = it.value();
                if (header_json.contains(keys.parent_key))
                    return read_parent_guid(header_json[keys.parent_key]);
            }
        }

        for (auto& [comp_name, comp_json] : components_it->items())
        {
            if (!comp_json.is_object())
                continue;

            if (comp_json.contains("Parent Entity"))
                return read_parent_guid(comp_json["Parent Entity"]);
            if (comp_json.contains("parent_entity"))
                return read_parent_guid(comp_json["parent_entity"]);
        }

        return Guid::invalid();
    }
}
