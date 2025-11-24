// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

// #include <iostream>
#include <entt/entt.hpp>
// #include <entt/meta/pointer.hpp>
#include <nlohmann/json.hpp>
#include "config.h"
#include "AssetMetaReg.hpp"
#include "EngineContext.hpp"

#include "ResourceTypes.hpp"
#include "MetaLiterals.h"
#include "Storage.hpp"
#include "MetaInfo.h"
#include "ResourceManager.hpp"

namespace eeng {

    namespace
    {
        template<class T>
        void warm_start_meta_type()
        {
            if (!entt::resolve<T>())
                throw std::runtime_error("entt::resolve() failed for asset type");
        }
    }

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
            visit_asset_refs(t, [&](const auto& asset_ref) {
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

        template<class T>
        void bind_asset(const Guid& guid, const Guid& batch_id, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            rm.bind_asset<T>(guid, batch_id, ctx);
        }

        template<class T>
        void unbind_asset(const Guid& guid, const Guid& batch_id, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            rm.unbind_asset<T>(guid, batch_id, ctx);
        }

        // template<class T>
        // void bind_asset_with_leases(const Guid& guid, EngineContext& ctx, const BatchId& batch)
        // {
        //     auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
        //     rm.bind_asset_with_leases<T>(guid, ctx, batch);
        // }

        // template<class T>
        // void unbind_asset_with_leases(const Guid& guid, EngineContext& ctx, const BatchId& batch)
        // {
        //     auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
        //     rm.unbind_asset_with_leases<T>(guid, ctx, batch);
        // }

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
        void register_asset()
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
                // .template func<&bind_asset<T>, entt::as_void_t>(eeng::literals::bind_asset_hs)
                // .template func<&unbind_asset<T>, entt::as_void_t>(eeng::literals::unbind_asset_hs)
                .template func<&bind_asset<T>, entt::as_void_t>(eeng::literals::bind_asset_hs)
                .template func<&unbind_asset<T>, entt::as_void_t>(eeng::literals::unbind_asset_hs)

                // Asset validation
                .template func<&validate_asset<T>>(eeng::literals::validate_asset_hs)
                .template func<&validate_asset_recursive<T>>(eeng::literals::validate_asset_recursive_hs)
                //
                //.func<&collect_guids<MeshRendererComponent>>("collect_asset_guids"_hs)
                ;
            warm_start_meta_type<T>();

            // Caution: this name will include namespaces
            // auto name = std::string{ entt::resolve<T>().info().name() };

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
            warm_start_meta_type<AssetRef<T>>();
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

    void register_asset_meta_types(EngineContext& ctx)
    {
        EENG_LOG_INFO(&ctx, "Registering resource meta types...");

        // === Guid ===

        entt::meta_factory<Guid>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ "Guid", "A globally unique identifier." })
            .func<&serialize_Guid>(eeng::literals::serialize_hs)
            .func<&deserialize_Guid>(eeng::literals::deserialize_hs)
            ;
        warm_start_meta_type<Guid>();

        // === AssetMetaData ===

        entt::meta_factory<AssetMetaData>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ "AssetMetaData", "Metadata for an asset." })

            .data<&AssetMetaData::guid>("guid"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "guid", "Guid", "A globally unique identifier." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::guid_parent>("guid_parent"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "parent_guid", "Parent Guid", "The GUID of the parent asset." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::name>("name"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "The name of the asset." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::type_name>("type_name"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "type_name", "Type Name", "The type name of the asset." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::contained_assets>("contained_assets"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "contained_assets", "Contained Assets", "Contained assets." })
            .traits(MetaFlags::read_only)

            // .data<&AssetMetaData::file_path>("file_path"_hs)
            // .custom<DataMetaInfo>(DataMetaInfo{ "file_path", "File Path", "The file path of the asset." })
            // .traits(MetaFlags::read_only)
            ;
        warm_start_meta_type<AssetMetaData>();

        // === RESOURCES ===

        // mock::Mesh
        register_asset<mock::Mesh>();
        entt::meta_factory<mock::Mesh>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ "Mesh", "This is a mock mesh type." })
            .data<&mock::Mesh::vertices>("vertices"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "vertices", "Vertices", "A vector of vertex positions." })
            .traits(MetaFlags::read_only)
            ;

        // mock::Texture
        register_asset<mock::Texture>();
        entt::meta_factory<mock::Texture>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ "Texture", "This is a mock Texture type." })
            .data<&mock::Texture::name>("name"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "The name of the texture." })
            .traits(MetaFlags::read_only)
            ;

        // mock::Model
        register_asset<mock::Model>();
        entt::meta_factory<mock::Model>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ "Model", "This is a mock model type." })

            .data<&mock::Model::meshes>("meshes"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "meshes", "Meshes", "A vector of mesh references." })
            .traits(MetaFlags::read_only)

            .data<&mock::Model::textures>("textures"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "textures", "Textures", "A vector of texture references." })
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
        register_asset<mock::MockResource1>();
        entt::meta_factory<mock::MockResource1>{}
        // .type("MockResource1"_hs)
        .custom<TypeMetaInfo>(TypeMetaInfo{ "MockResource1", "This is a mock resource type." })

            // Register member 'x' with DisplayInfo and DataFlags
            .data<&mock::MockResource1::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "An integer member 'x'" })
            .traits(MetaFlags::read_only)

            // Register member 'y' with DisplayInfo and multiple DataFlags
            .data<&mock::MockResource1::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "A float member 'y'" })
            .traits(MetaFlags::hidden | MetaFlags::read_only)

            // Required for all resource types
            .template func<&assure_storage<mock::MockResource1>>(eeng::literals::assure_storage_hs)

            // Required only if resource has recursive references
            //.func<&visit_assets<MockResource1>>("visit_refs"_hs);
                // Usage:
                //entt::meta_type type = entt::resolve<MockResource1>();
                // auto visit_fn = type.func("visit_refs"_hs);
                // if (visit_fn) {
                //     visit_fn.invoke({}, entt::forward_as_meta(model), visitor_fn);
                // }

        //.func < [](eeng::Storage& storage) { } > (inspect_hs)
        //.data<&BehaviorScript::on_collision/*, entt::as_ref_t*/>("on_collision"_hs).prop(display_name_hs, "on_collision")

            ;

        register_asset<mock::MockResource2>();
    }

} // namespace eeng