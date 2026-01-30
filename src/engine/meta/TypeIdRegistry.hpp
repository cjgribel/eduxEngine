// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <unordered_map>
#include <string>
#include <string_view>

#include <entt/entt.hpp>
#include "MetaInfo.h"   // for TypeMetaInfo

namespace eeng::meta
{
    struct TypeIdRegistry
    {
        using map_type = std::unordered_map<std::string, entt::id_type>;

        static map_type &map()
        {
            static map_type m;
            return m;
        }

        // Low-level: explicit string → id
        static entt::id_type register_type(std::string_view name, entt::meta_type mt)
        {
            assert(mt && "TypeIdRegistry::register_type: meta_type is invalid");

            const entt::id_type id = mt.id();
            auto key = std::string{name};

            auto [it, inserted] = map().emplace(key, id);
            if (!inserted)
            {
                // same name reused, must be same id
                assert(it->second == id && "TypeIdRegistry: name already used for a different meta_type");
            }

            return id;
        }

        // Higher-level: use TypeMetaInfo::id
        template<class T>
        static entt::id_type register_type_from_meta()
        {
            // Use resolve(type_id) so we only accept explicitly registered types.
            entt::meta_type mt = entt::resolve(entt::type_id<T>());
            assert(mt && "TypeIdRegistry::register_type_from_meta<T>: T not registered with entt::meta");

            TypeMetaInfo *info = mt.custom();
            assert(info && !info->id.empty() && "TypeMetaInfo with non-empty id is required");

            return register_type(info->id, mt);
        }

        static entt::meta_type resolve(std::string_view name)
        {
            auto it = map().find(std::string{name});
            if (it == map().end())
                return {};

            return entt::resolve(it->second);
        }

        // For unit tests only
        static void clear_for_tests()
        {
            map().clear();
        }
    };
}
