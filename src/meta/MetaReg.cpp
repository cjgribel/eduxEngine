// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

// #include <iostream>
#include <entt/entt.hpp>
#include <entt/meta/pointer.hpp>
#include <nlohmann/json.hpp>
#include "config.h"
#include "MetaReg.hpp"
#include "ResourceTypes.h"
#include "MetaLiterals.h"
#include "Storage.hpp"
#include "MetaInfo.h"
#include "IResourceManager.hpp" // For AssetRef<T>, AssetMetaData

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
        // class AssetDatabase;

        // template<typename T>
        // void serialize_handle(
        //     const Handle<T>& handle,
        //     nlohmann::json& j,
        //     AssetDatabase& adb)
        // {
        //     Guid guid = adb.get_guid(handle);
        //     j = guid.to_string();
        // }

        // template<typename T>
        // void deserialize_handle(
        //     Handle<T>& handle,
        //     const nlohmann::json& j,
        //     AssetDatabase& registry)
        // {
        //     Guid guid = Guid::from_string(j.get<std::string>());
        //     handle = registry.get_or_load<T>(guid);
        // }

        template<class T>
        void assure_storage(eeng::Storage& storage)
        {
            storage.assure_storage<T>();
        }

        // template<typename T>
        // void register_handle(const std::string& name)
        // {
        //     using handle_type = Handle<T>;

        //     entt::meta_factory<handle_type>()
        //         .type(entt::hashed_string{ name.c_str() })
        //         //.type(entt::hashed_string::value(name.c_str()))
        //         // .template func<&serialize_handle<T>>("serialize"_hs)
        //         // .template func<&deserialize_handle<T>>("deserialize"_hs)
        //         ;
        // }

        template<typename T>
        void register_resource(/*const std::string& name*/)
        {
            // constexpr auto alias = entt::type_name<T>::value();  // e.g. "ResourceTest"
            // constexpr auto id    = entt::type_hash<T>::value();

            entt::meta_factory<T>()
                .template func<&assure_storage<T>>(eeng::literals::assure_storage_hs)
                ;

            // Caution: this name will include namespaces
            auto name = std::string{ entt::resolve<T>().info().name() };

            // Handle<T>
            // NOTE: Is not exposed via AssetRef<T>
#if 0
            entt::meta_factory<Handle<T>>{}
            .data<&Handle<T>::idx>("idx"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "idx", "The index of the handle in storage." })
                .traits(MetaFlags::read_only)
                .data<&Handle<T>::ver>("ver"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "ver", "The version of the handle." })
                .traits(MetaFlags::read_only)
                ;
#endif

            // AssetRef<T>
            entt::meta_factory<AssetRef<T>>{}
            .template data<&AssetRef<T>::guid>("guid"_hs)
                .template custom<DataMetaInfo>(DataMetaInfo{ "guid", "A globally unique identifier." })
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
        // void serialize_Guid(nlohmann::json& j, const void* ptr)
        // {
        //     j = static_cast<const Guid*>(ptr)->raw();
        // }

        // void deserialize_Guid(const nlohmann::json& j, void* ptr)
        // {
        //     *static_cast<Guid*>(ptr) = Guid{ j.get<uint64_t>() };
        // }

    } // namespace

    void register_meta_types()
    {
        // === Guid ===

        entt::meta_factory<Guid>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ "Guid", "A globally unique identifier." })
            .func<&serialize_Guid>(eeng::literals::serialize_hs)
            .func<&deserialize_Guid>(eeng::literals::deserialize_hs)
            ;

        // === AssetMetaData ===

        entt::meta_factory<AssetMetaData>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ "AssetMetaData", "Metadata for an asset." })

            .data<&AssetMetaData::guid>("guid"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Guid", "A globally unique identifier." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::guid_parent>("guid_parent"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Parent Guid", "The GUID of the parent asset." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::name>("name"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Name", "The name of the asset." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::type_name>("type_name"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "Type name", "The type name of the asset." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::file_path>("file_path"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "File path", "The file path of the asset." })
            .traits(MetaFlags::read_only);

        // === RESOURCES ===

        // mock::Mesh
        register_resource<mock::Mesh>();
        entt::meta_factory<mock::Mesh>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ "Mesh", "This is a mock mesh type." })
            .data<&mock::Mesh::vertices>("vertices"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "vertices", "A vector of vertex positions." })
            .traits(MetaFlags::read_only)
            ;

        // mock::Model
        register_resource<mock::Model>();
        entt::meta_factory<mock::Model>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ "Model", "This is a mock model type." })
            .data<&mock::Model::meshes>("meshes"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "meshes", "A vector of mesh references." })
            .traits(MetaFlags::read_only)
            ;

        // entt::meta<Texture>()
        //     .type("Texture"_hs)
        //     .func<&assure_storage<Texture>>("assure_storage"_hs);
        //
        // + forward_as_meta
        // auto assure_fn = entt::resolve<Texture>().func("assure_storage"_hs);
        // assure_fn.invoke({}, registry);

        // mock::MockResource1
        register_resource<mock::MockResource1>();
        entt::meta_factory<mock::MockResource1>{}
        // .type("MockResource1"_hs)
        .custom<TypeMetaInfo>(TypeMetaInfo{ "MockResource1", "This is a mock resource type." })

            // Register member 'x' with DisplayInfo and DataFlags
            .data<&mock::MockResource1::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "An integer member 'x'" })
            .traits(MetaFlags::read_only)

            // Register member 'y' with DisplayInfo and multiple DataFlags
            .data<&mock::MockResource1::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "A float member 'y'" })
            .traits(MetaFlags::hidden | MetaFlags::read_only)

            // Required for all resource types
            .template func<&assure_storage<mock::MockResource1>>(eeng::literals::assure_storage_hs)

            // Required only if resource has recursive references
            //.func<&visit_asset_refs<MockResource1>>("visit_refs"_hs);
                // Usage:
                //entt::meta_type type = entt::resolve<MockResource1>();
                // auto visit_fn = type.func("visit_refs"_hs);
                // if (visit_fn) {
                //     visit_fn.invoke({}, entt::forward_as_meta(model), visitor_fn);
                // }

        //.func < [](eeng::Storage& storage) { } > (inspect_hs)
        //.data<&BehaviorScript::on_collision/*, entt::as_ref_t*/>("on_collision"_hs).prop(display_name_hs, "on_collision")

            ;

        register_resource<mock::MockResource2>();
    }

} // namespace eeng