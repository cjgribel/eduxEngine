// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

// #include <iostream>
#include <entt/entt.hpp>
#include <entt/meta/pointer.hpp>
#include <nlohmann/json.hpp>
#include "config.h"
#include "AssetMetaReg.hpp"
#include "ResourceTypes.h"
#include "MetaLiterals.h"
#include "Storage.hpp"
#include "MetaInfo.h"
// #include "IResourceManager.hpp" 
#include "ResourceManager.hpp" // For AssetRef<T>, AssetMetaData, ResourceManager::load<>/unload

#include "MockImporter.hpp" // For mock types

namespace eeng {

    // + EngineContext for serialization & deserialization

#if 0
    void BehaviorScript_to_json(nlohmann::json& j, const void* ptr)
    {
        std::cout << "BehaviorScript_to_json\n";

        auto script = static_cast<const BehaviorScript*>(ptr);

        // self sol::table
        nlohmann::json self_json;
        table_to_json(self_json, script->self);
        j["self"] = self_json;

        j["identifier"] = script->identifier;
        j["path"] = script->path;
    }

    void BehaviorScript_from_json(
        const nlohmann::json& j,
        void* ptr,
        const Entity& entity,
        Editor::Context& context)
    {
        std::cout << "BehaviorScript_from_json\n";

        auto script = static_cast<BehaviorScript*>(ptr);
        std::string identifier = j["identifier"];
        std::string path = j["path"];

        // self sol::table
        // LOAD + copy Lua meta fields
        *script = BehaviorScriptFactory::create_from_file(
            *context.registry,
            entity,
            *context.lua, // self.lua_state() <- nothing here
            path,
            identifier);

        table_from_json(script->self, j["self"]);
    }
#endif

#if 0
    entt::meta<BehaviorScript>()
        .type("BehaviorScript"_hs).prop(display_name_hs, "BehaviorScript")

        .data<&BehaviorScript::identifier>("identifier"_hs)
        .prop(display_name_hs, "identifier")
        .prop(readonly_hs, true)

        .data<&BehaviorScript::path>("path"_hs)
        .prop(display_name_hs, "path")
        .prop(readonly_hs, true)

        // sol stuff
        .data<&BehaviorScript::self/*, entt::as_ref_t*/>("self"_hs).prop(display_name_hs, "self")
        .data<&BehaviorScript::update/*, entt::as_ref_t*/>("update"_hs).prop(display_name_hs, "update")
        .data<&BehaviorScript::on_collision/*, entt::as_ref_t*/>("on_collision"_hs).prop(display_name_hs, "on_collision")

        // inspect_hs
        // We have this, and not one for sol::table, so that when a sol::table if edited,
        // a deep copy of BehaviorScript is made, and not just the sol::table
        .func < [](void* ptr, Editor::InspectorState& inspector) {return Editor::inspect_type(*static_cast<BehaviorScript*>(ptr), inspector); } > (inspect_hs)

        // clone
        .func<&copy_BehaviorScript>(clone_hs)

        // to_json
        .func<&BehaviorScript_to_json>(to_json_hs)

        // from_json
        .func<&BehaviorScript_from_json>(from_json_hs)
        ;
#endif

    namespace
    {
        //
        // Standard asset meta functions
        //

        template<class T>
        void assure_storage(eeng::Storage& storage)
        {
            storage.assure_storage<T>();
        }

#if 0
        // Collect referenced GUIDs for Assets or Components
        template<typename T>
        void collect_guids(T& t, std::unordered_set<Guid>& out_guids)
        {
            visit_assets(t, [&](const auto& asset_ref) {
                out_guids.insert(asset_ref.guid);
                });
        }
#endif

        template<class T>
        void load_asset(const Guid& guid, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            rm.load_asset<T>(guid, ctx);
        }

        template<class T>
        void unload_asset(const Guid& guid, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            rm.unload_asset<T>(guid, ctx);
        }

        // template<class T>
        // void reload_asset(const Guid& guid, EngineContext& ctx)
        // {
        //     unload_asset<T>(guid, ctx);
        //     load_asset<T>(guid, ctx);
        // }

        template<class T>
        void resolve_asset(const Guid& guid, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            rm.resolve_asset<T>(guid, ctx);
        }

        template<class T>
        void unresolve_asset(const Guid& guid, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            rm.unresolve_asset<T>(guid, ctx);
        }

        template<class T>
        bool validate_asset(const Guid& guid, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            return rm.validate_asset<T>(guid);
        }

        template<class T>
        bool validate_asset_recursive(const Guid& guid, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            return rm.validate_asset_recursive<T>(guid);
        }

        template<typename T>
        void register_resource()
        {
            // constexpr auto alias = entt::type_name<T>::value();  // e.g. "ResourceTest"
            // constexpr auto id    = entt::type_hash<T>::value();

            entt::meta_factory<T>()
                // Assuring type storage
                .template func<&assure_storage<T>, entt::as_void_t>(eeng::literals::assure_storage_hs)
                // Type-safe loading
                .template func<&load_asset<T>, entt::as_void_t>(eeng::literals::load_asset_hs)
                .template func<&unload_asset<T>, entt::as_void_t>(eeng::literals::unload_asset_hs)
                // .template func<&reload_asset<T>>(eeng::literals::reload_asset_hs)
                // Type-safe binding
                .template func<&resolve_asset<T>, entt::as_void_t>(eeng::literals::resolve_asset_hs)
                .template func<&unresolve_asset<T>, entt::as_void_t>(eeng::literals::unresolve_asset_hs)
                // Asset validation
                .template func<&validate_asset<T>>(eeng::literals::validate_asset_hs)
                .template func<&validate_asset_recursive<T>>(eeng::literals::validate_asset_recursive_hs)
                //
                //.func<&collect_guids<MeshRendererComponent>>("collect_asset_guids"_hs)
                ;

            // Caution: this name will include namespaces
            auto name = std::string{ entt::resolve<T>().info().name() };

            // Handle<T>
            // NOTE: Is not exposed via AssetRef<T>
#if 0
            entt::meta_factory<Handle<T>>{}
            .data<&Handle<T>::idx>("idx"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "idx", "Index", "The index of the handle in storage." })
                .traits(MetaFlags::read_only)
                .data<&Handle<T>::ver>("ver"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "ver", "Version", "The version of the handle." })
                .traits(MetaFlags::read_only)
                ;
#endif

            // AssetRef<T>
            entt::meta_factory<AssetRef<T>>{}
            .template data<&AssetRef<T>::guid>("guid"_hs)
                .template custom<DataMetaInfo>(DataMetaInfo{ "guid", "Guid", "A globally unique identifier." })
                .traits(MetaFlags::read_only)
                ;
        }
    } // namespace

    namespace
    {
        // Guid to and from json
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
    } // namespace

    void register_component_meta_types()
    {
        
    }

} // namespace eeng