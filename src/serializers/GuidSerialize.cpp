// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "serializers/GuidSerialize.hpp"

#include <cassert>

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

#include "Guid.h"

namespace eeng::serializers
{
    void serialize_Guid(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<Guid>();
        assert(ptr && "serialize_Guid: could not cast meta_any to Guid");
        j = ptr->raw();
    }

    void deserialize_Guid(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<Guid>();
        assert(ptr && "deserialize_Guid: could not cast meta_any to Guid");
        *ptr = Guid{ j.get<uint64_t>() };
    }
}
