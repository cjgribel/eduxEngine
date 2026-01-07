#pragma once

#include <string>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include "Guid.h"

namespace eeng::editor
{
    struct HeaderJsonKeys
    {
        std::string type_id;
        std::string guid_key;
        std::string parent_key;
        std::string entityref_guid_key;
    };

    const HeaderJsonKeys& header_keys();
    entt::id_type header_component_id();
    bool update_entity_guid_in_json(nlohmann::json& entity_json, const Guid& guid);
    bool update_parent_guid_in_json(nlohmann::json& entity_json, const Guid& parent_guid);
    Guid guid_from_json(const nlohmann::json& entity_json);
}
