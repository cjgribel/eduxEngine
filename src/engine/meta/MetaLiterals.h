// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#ifndef MetaLiterals_h
#define MetaLiterals_h

#include <entt/entt.hpp>
using namespace entt::literals;

namespace eeng::literals
{
    // Meta object utilities (inspect/serialize).
    constexpr entt::hashed_string to_string_hs = "to_string"_hs;
    constexpr entt::hashed_string clone_hs = "clone"_hs;
    constexpr entt::hashed_string serialize_hs = "serialize"_hs;
    constexpr entt::hashed_string deserialize_hs = "deserialize"_hs;
    // Inspector entry point; invoked from MetaInspect to draw/edit.
    constexpr entt::hashed_string inspect_hs = "inspect"_hs;

    // Component edit hook; invoked after AssignFieldCommand commits a change.
    constexpr entt::hashed_string post_assign_hs = "post_assign"_hs;
    // Component ref hook; invoked after bind_asset_refs_for_entity finishes binding AssetRef<>.
    constexpr entt::hashed_string post_bind_hs = "post_bind"_hs;
    // Storage hook for assets; invoked when storage needs to be created/reserved.
    constexpr entt::hashed_string assure_storage_hs = "assure_storage"_hs;

    // Asset lifecycle (ResourceManager load/bind/unbind/unload).
    constexpr entt::hashed_string load_asset_hs = "load_asset"_hs;
    constexpr entt::hashed_string unload_asset_hs = "unload_asset"_hs;
    constexpr entt::hashed_string bind_asset_hs = "bind_asset"_hs;
    constexpr entt::hashed_string unbind_asset_hs = "unbind_asset"_hs;
    // Asset persistence; invoked by UI save or tooling.
    constexpr entt::hashed_string save_asset_hs = "save_asset"_hs;
    // constexpr entt::hashed_string bind_asset_with_leases_hs = "bind_asset_with_leases"_hs;
    // constexpr entt::hashed_string unbind_asset_with_leases_hs = "unbind_asset_with_leases"_hs;

    // Asset validation hooks.
    constexpr entt::hashed_string validate_asset_hs = "validate_asset"_hs;
    constexpr entt::hashed_string validate_asset_recursive_hs = "validate_asset_recursive"_hs;

    // Optional asset lifecycle hooks (after load/bind and before unload).
    constexpr entt::hashed_string on_create_hs = "on_create"_hs;
    constexpr entt::hashed_string on_destroy_hs = "on_destroy"_hs;

    // Component types (EntityMetaHelpers / MetaSerialize helpers).
    constexpr entt::hashed_string assure_component_storage_hs = "assure_component_storage"_hs;
    constexpr entt::hashed_string collect_asset_guids_hs = "collect_asset_guids"_hs;
    // Component AssetRef<> binding; invoked by bind_asset_refs_for_entity.
    constexpr entt::hashed_string bind_asset_refs_hs = "bind_asset_refs"_hs;
    // Component EntityRef binding; invoked by bind_entity_refs_for_entity.
    constexpr entt::hashed_string bind_entity_refs_hs = "bind_entity_refs"_hs;
}
#endif // MetaLiterals_h
