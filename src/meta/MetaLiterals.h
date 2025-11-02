// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MetaLiterals_h
#define MetaLiterals_h

#include <entt/entt.hpp>
using namespace entt::literals;

namespace eeng::literals
{
    constexpr entt::hashed_string to_string_hs = "to_string"_hs;
    constexpr entt::hashed_string clone_hs = "clone"_hs;
    constexpr entt::hashed_string serialize_hs = "serialize"_hs;
    constexpr entt::hashed_string deserialize_hs = "deserialize"_hs;
    constexpr entt::hashed_string inspect_hs = "inspect"_hs;
    constexpr entt::hashed_string assure_storage_hs = "assure_storage"_hs;

    constexpr entt::hashed_string load_asset_hs = "load_asset"_hs;
    constexpr entt::hashed_string unload_asset_hs = "unload_asset"_hs;
    constexpr entt::hashed_string bind_asset_hs = "bind_asset"_hs;
    constexpr entt::hashed_string unbind_asset_hs = "unbind_asset"_hs;
    // constexpr entt::hashed_string bind_asset_with_leases_hs = "bind_asset_with_leases"_hs;
    // constexpr entt::hashed_string unbind_asset_with_leases_hs = "unbind_asset_with_leases"_hs;

    constexpr entt::hashed_string validate_asset_hs = "validate_asset"_hs;
    constexpr entt::hashed_string validate_asset_recursive_hs = "validate_asset_recursive"_hs;

    constexpr entt::hashed_string assure_component_storage_hs = "assure_component_storage"_hs;
}
#endif // MetaLiterals_h
