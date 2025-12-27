// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.


#include "config.h"
#include "AssetMetaReg.hpp"

#include "EngineContext.hpp"
#include "MetaInfo.h"
#include "MetaLiterals.h"
#include "meta/EntityMetaHelpers.hpp"

#include "editor/AssetRefInspect.hpp"
#include "editor/GLMInspect.hpp"
#include "assets/types/ModelAssets.hpp"
#include "gpu/GpuAssetOps.hpp"
#include "mock/MockAssetTypes.hpp"
#include "Storage.hpp"
#include "ResourceManager.hpp"
#include "LogMacros.h"

// #include <iostream>
#include <entt/entt.hpp>
// #include <entt/meta/pointer.hpp>
#include <nlohmann/json.hpp> // -> TYPE HELPER

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

        // -> META HELPER, or MERGE WITH GENERAL BIND CODE
        template<class T>
        BindResult bind_asset(const Guid& guid, const Guid& batch_id, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            return rm.bind_asset<T>(guid, batch_id, ctx);
        }

        template<class T>
        BindResult unbind_asset(const Guid& guid, const Guid& batch_id, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            return rm.unbind_asset<T>(guid, batch_id, ctx);
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

        void on_create_gpu_model(const Guid& guid, EngineContext& ctx)
        {
            auto rm = std::dynamic_pointer_cast<ResourceManager>(ctx.resource_manager);
            if (!rm) return;

            auto handle_opt = rm->handle_for_guid<assets::GpuModelAsset>(guid);
            if (!handle_opt) return;

            bool needs_init = false;
            rm->storage().read(*handle_opt, [&](const assets::GpuModelAsset& gpu)
                {
                    needs_init = (gpu.state == assets::GpuModelState::Uninitialized ||
                        gpu.state == assets::GpuModelState::Failed);
                });

            if (!needs_init) return;

            auto r = gl::init_gpu_model(*handle_opt, ctx);
            if (!r.ok)
                EENG_LOG_ERROR(&ctx, "GpuModel init failed: %s", r.error.c_str());
        }

        void on_destroy_gpu_model(const Guid& guid, EngineContext& ctx)
        {
            auto rm = std::dynamic_pointer_cast<ResourceManager>(ctx.resource_manager);
            if (!rm) return;

            auto handle_opt = rm->handle_for_guid<assets::GpuModelAsset>(guid);
            if (!handle_opt) return;

            bool can_destroy = false;
            rm->storage().read(*handle_opt, [&](const assets::GpuModelAsset& gpu)
                {
                    can_destroy = (gpu.state == assets::GpuModelState::Ready);
                });

            if (!can_destroy) return;

            gl::destroy_gpu_model(*handle_opt, ctx);
        }
    }

    namespace
    {
        // template<class T>
        // void register_type_info(
        //     const std::string& type_id,
        //     const std::string& type_name,
        //     const std::string& tooltip,
        //     const entt::meta_type& underlying_type = entt::meta_type{})
        // {
        //     entt::meta_factory<T>{}
        //     .template custom<TypeMetaInfo>(TypeMetaInfo{ .id = type_id, .name = type_name, .tooltip = tooltip, .underlying_type = underlying_type })
        //         ;
        // }

        template<typename T>
        void register_asset()
        {
            // constexpr auto alias = entt::type_name<T>::value();  // e.g. "ResourceTest"
            // constexpr auto id    = entt::type_hash<T>::value();

            meta::register_type<T>();
            warm_start_meta_type<T>();

            entt::meta_factory<T>()

                // Assure type storage
                .template func<&assure_storage<T>, entt::as_void_t>(eeng::literals::assure_storage_hs)

                // Collect asset references
                .template func<&meta::collect_asset_guids<T>, entt::as_void_t>(literals::collect_asset_guids_hs)

                // Type-safe loading
                .template func<&load_asset<T>, entt::as_void_t>(eeng::literals::load_asset_hs)
                .template func<&unload_asset<T>, entt::as_void_t>(eeng::literals::unload_asset_hs)
                // .template func<&reload_asset<T>>(eeng::literals::reload_asset_hs)
                // Type-safe binding
                // .template func<&bind_asset<T>, entt::as_void_t>(eeng::literals::bind_asset_hs)
                // .template func<&unbind_asset<T>, entt::as_void_t>(eeng::literals::unbind_asset_hs)
                .template func<&bind_asset<T>/*, entt::as_void_t*/>(eeng::literals::bind_asset_hs)
                .template func<&unbind_asset<T>/*, entt::as_void_t*/>(eeng::literals::unbind_asset_hs)

                // Asset validation
                .template func<&validate_asset<T>>(eeng::literals::validate_asset_hs)
                .template func<&validate_asset_recursive<T>>(eeng::literals::validate_asset_recursive_hs)
                //
                //.func<&collect_guids<MeshRendererComponent>>("collect_asset_guids"_hs)
                ;

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

            // AssetRef<T> helper
            entt::meta_factory<AssetRef<T>>{}
            .template custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.AssetRef", .name = "AssetRef", .tooltip = "An asset reference." })
                .traits(MetaFlags::none)

                // (Serialize) (Clone)
                // Guid
                .template data<&AssetRef<T>::guid>("guid"_hs)
                .template custom<DataMetaInfo>(DataMetaInfo{ "guid", "Guid", "A globally unique identifier." })
                .traits(MetaFlags::read_only)

                // Handle<T>
                // (Not serialized) (Not cloned)

                // (Inspect)
                .template func<&eeng::editor::inspect_AssetRef<T>>(eeng::literals::inspect_hs)
                .template custom<FuncMetaInfo>(FuncMetaInfo{ "inspect_AssetRef", "Inspect asset reference" })
                ;
            warm_start_meta_type<AssetRef<T>>();
            // NOTE: Generic name for all AssetRef<T> types.
            // meta::type_id_map()["eeng.AssetRef"] = entt::resolve<AssetRef<T>>().id();
        }

        template<typename T>
        void register_helper_type()
        {
            warm_start_meta_type<AssetRef<T>>();
        }
    } // namespace

    void register_asset_meta_types(EngineContext& ctx)
    {
        EENG_LOG_INFO(&ctx, "Registering resource meta types...");

        // === AssetMetaData ===

        entt::meta_factory<AssetMetaData>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.AssetMetaData", .name = "AssetMetaData", .tooltip = "Metadata for an asset." })
            .traits(MetaFlags::none)

            .data<&AssetMetaData::guid>("guid"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "guid", "Guid", "A globally unique identifier." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::guid_parent>("guid_parent"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "parent_guid", "Parent Guid", "The GUID of the parent asset." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::name>("name"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "The name of the asset." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::type_id>("type_id"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "type_id", "Type ID", "The type ID of the asset." })
            .traits(MetaFlags::read_only)

            .data<&AssetMetaData::contained_assets>("contained_assets"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "contained_assets", "Contained Assets", "Contained assets." })
            .traits(MetaFlags::read_only)

            // .data<&AssetMetaData::file_path>("file_path"_hs)
            // .custom<DataMetaInfo>(DataMetaInfo{ "file_path", "File Path", "The file path of the asset." })
            // .traits(MetaFlags::read_only)
            ;
        register_helper_type<AssetMetaData>();
        // warm_start_meta_type<AssetMetaData>();
        // meta::type_id_map()["eeng.AssetMetaData"] = entt::resolve<AssetMetaData>().id();
        /* -> */ //meta::TypeIdRegistry::register_type_from_meta<AssetMetaData>();

        // --- Assets & Asset helpers ------------------------------------------

        // GpuSubMesh (helper)
        {
            entt::meta_factory<assets::GpuSubMesh>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "assets.GpuSubMesh", .name = "GpuSubMesh", .tooltip = "GpuSubMesh." })
                .traits(MetaFlags::none)

                .data<&assets::GpuSubMesh::index_offset>("index_offset"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "index_offset", "Index Offset", "Index Offset." })
                .traits(MetaFlags::read_only)

                .data<&assets::GpuSubMesh::index_count>("index_count"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "index_count", "Index Count", "Index Count." })
                .traits(MetaFlags::read_only)

                .data<&assets::GpuSubMesh::base_vertex>("base_vertex"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "base_vertex", "Base Vertex", "Base Vertex." })
                .traits(MetaFlags::read_only)

                .data<&assets::GpuSubMesh::material>("material"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "material", "Material", "Material." })
                .traits(MetaFlags::read_only)
                ;
            register_helper_type<assets::GpuSubMesh>();
        }

        // GpuModelState (helper)
        {
            auto enum_info = TypeMetaInfo
            {
                .id = "eeng.assets.GpuModelState",
                .name = "GpuModelState",
                .tooltip = "GPU Load States",
                .underlying_type = entt::resolve<std::underlying_type_t<assets::GpuModelState>>()
            };
            entt::meta_factory<assets::GpuModelState>()
                .custom<TypeMetaInfo>(enum_info)
                .traits(MetaFlags::none)

                .data<assets::GpuModelState::Uninitialized>("Uninitialized"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Uninitialized", "Uninitialized." })
                .traits(MetaFlags::none)

                .data<assets::GpuModelState::Queued>("Queued"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Queued", "Queued." })
                .traits(MetaFlags::none)

                .data<assets::GpuModelState::Ready>("Ready"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Ready", "Ready." })
                .traits(MetaFlags::none)

                .data<assets::GpuModelState::Failed>("Failed"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Failed", "Failed." })
                .traits(MetaFlags::none)
                ;
            register_helper_type<assets::GpuModelState>();
        }

        // GpuModelAsset
        {
            entt::meta_factory<assets::GpuModelAsset>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "assets.GpuModelAsset", .name = "GpuModelAsset", .tooltip = "GPU binding for a model asset." })
                .traits(MetaFlags::none)

                .data<&assets::GpuModelAsset::model_ref>("model_ref"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "model_ref", "Model Reference", "Referenced Model asset" })
                .traits(MetaFlags::read_only)

                .data<&assets::GpuModelAsset::state>("state"_hs).custom<DataMetaInfo>(DataMetaInfo{ "state", "Load State", "GPU Load State." }).traits(MetaFlags::read_only)

                .data<&assets::GpuModelAsset::vao>("vao"_hs).custom<DataMetaInfo>(DataMetaInfo{ "vao", "VAO", "VAO." }).traits(MetaFlags::read_only)
                .data<&assets::GpuModelAsset::vbo_uv>("vbo_uv"_hs).custom<DataMetaInfo>(DataMetaInfo{ "vbo_uv", "VBO for texture coordinates", "VBO UV." }).traits(MetaFlags::read_only)
                .data<&assets::GpuModelAsset::vao>("vbo_pos"_hs).custom<DataMetaInfo>(DataMetaInfo{ "vbo_pos", "VAO Positions", "VAO for positions." }).traits(MetaFlags::read_only)
                .data<&assets::GpuModelAsset::vbo_nrm>("vbo_nrm"_hs).custom<DataMetaInfo>(DataMetaInfo{ "vbo_nrm", "VBO Normals", "VBO for normals." }).traits(MetaFlags::read_only)
                .data<&assets::GpuModelAsset::vbo_bnrm>("vbo_binrm"_hs).custom<DataMetaInfo>(DataMetaInfo{ "vbo_bnrm", "VBO Binormals", "VBO for binormals." }).traits(MetaFlags::read_only)
                .data<&assets::GpuModelAsset::vbo_tang>("vbo_tang"_hs).custom<DataMetaInfo>(DataMetaInfo{ "vbo_tang", "VBO Normals", "VBO for tangents." }).traits(MetaFlags::read_only)
                .data<&assets::GpuModelAsset::vbo_bone>("vbo_bone"_hs).custom<DataMetaInfo>(DataMetaInfo{ "vbo_bone", "VBO Bones", "VBO for bone indices and weights." }).traits(MetaFlags::read_only)
                .data<&assets::GpuModelAsset::ibo>("ibo"_hs).custom<DataMetaInfo>(DataMetaInfo{ "ibo", "IBO", "IBO." }).traits(MetaFlags::read_only)
                .data<&assets::GpuModelAsset::submeshes>("submeshes"_hs).custom<DataMetaInfo>(DataMetaInfo{ "submeshes", "SubMeshes", "SubMeshes." }).traits(MetaFlags::read_only)
                .data<&assets::GpuModelAsset::vertex_count>("vertex_count"_hs).custom<DataMetaInfo>(DataMetaInfo{ "vertex_count", "Vertex Count", "Vertex Count." }).traits(MetaFlags::read_only)
                .data<&assets::GpuModelAsset::index_count>("index_count"_hs).custom<DataMetaInfo>(DataMetaInfo{ "index_count", "Index Count", "Index Count." }).traits(MetaFlags::read_only)
                .func<&on_create_gpu_model>(literals::on_create_hs)
                .func<&on_destroy_gpu_model>(literals::on_destroy_hs)
                ;
            register_asset<assets::GpuModelAsset>();
        }

        // TextureAsset
        {
            entt::meta_factory<assets::TextureAsset>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "assets.TextureAsset", .name = "TextureAsset", .tooltip = "TextureAsset." })
                .traits(MetaFlags::none)

                .data<&assets::TextureAsset::source_path>("source_path"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "source_path", "Path", "Path." })
                .traits(MetaFlags::read_only)
                ;
            register_asset<assets::TextureAsset>();
        }

        // glm::vec2 (helper)
        {
            entt::meta_factory<glm::vec2>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.vec2", .name = "Vec2", .tooltip = "Vec2." })
                .traits(MetaFlags::none)

                .data<&glm::vec2::x>("x"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "X." }).traits(MetaFlags::none)

                .data<&glm::vec2::y>("y"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "Y." }).traits(MetaFlags::none)

                .template func<&eeng::editor::inspect_glmvec2>(eeng::literals::inspect_hs)
                ;
            register_asset<assets::TextureAsset>();
        }

        // glm::vec3 (helper)
        {
            entt::meta_factory<glm::vec3>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.vec3", .name = "Vec3", .tooltip = "Vec3." })
                .traits(MetaFlags::none)

                .data<&glm::vec3::x>("x"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "X." }).traits(MetaFlags::none)

                .data<&glm::vec3::y>("y"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "Y." }).traits(MetaFlags::none)

                .data<&glm::vec3::z>("z"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "z", "Z", "Z." }).traits(MetaFlags::none)

                .template func<&eeng::editor::inspect_glmvec3>(eeng::literals::inspect_hs)
                ;
            register_asset<assets::TextureAsset>();
        }

        // MaterialAsset
        {
            entt::meta_factory<assets::MaterialAsset>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "assets.MaterialAsset", .name = "MaterialAsset", .tooltip = "MaterialAsset." })
                .traits(MetaFlags::none)

                .data<&assets::MaterialAsset::Ka>("Ka"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "Ka", "Ambient Color", "Ambient Color." })
                .traits(MetaFlags::read_only)

                .data<&assets::MaterialAsset::Kd>("Kd"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "Kd", "Diffuse Color", "Diffuse Color." })
                .traits(MetaFlags::read_only)

                .data<&assets::MaterialAsset::Ks>("Ks"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "Ks", "Specular Color", "Specular Color." })
                .traits(MetaFlags::read_only)

                .data<&assets::MaterialAsset::shininess>("shininess"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "shininess", "Shininess", "Shininess." })
                .traits(MetaFlags::read_only)

                .data<&assets::MaterialAsset::textures>("textures"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "textures", "Textures", "Textures." })
                .traits(MetaFlags::read_only)
                ;
            register_asset<assets::MaterialAsset>();
        }

        // SubMesh (helper)
        {
            entt::meta_factory<assets::SubMesh>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "ssets.SubMesh", .name = "SubMesh", .tooltip = "SubMesh." })
                .traits(MetaFlags::none)

                .data<&assets::SubMesh::base_index>("base_index"_hs).custom<DataMetaInfo>(DataMetaInfo{ "base_index", "Base Index", "Base Index." }).traits(MetaFlags::read_only)
                .data<&assets::SubMesh::nbr_indices>("nbr_indices"_hs).custom<DataMetaInfo>(DataMetaInfo{ "nbr_indices", "Nbr Indices", "Nbr Indices." }).traits(MetaFlags::read_only)
                .data<&assets::SubMesh::base_vertex>("base_vertex"_hs).custom<DataMetaInfo>(DataMetaInfo{ "base_vertex", "Base Vertex", "Base Vertex." }).traits(MetaFlags::read_only)
                .data<&assets::SubMesh::nbr_vertices>("nbr_vertices"_hs).custom<DataMetaInfo>(DataMetaInfo{ "nbr_vertices", "Nbr Vertices", "Nbr Vertices." }).traits(MetaFlags::read_only)
                .data<&assets::SubMesh::node_index>("node_index"_hs).custom<DataMetaInfo>(DataMetaInfo{ "node_index", "Node Index", "Node Index." }).traits(MetaFlags::read_only)
                .data<&assets::SubMesh::is_skinned>("is_skinned"_hs).custom<DataMetaInfo>(DataMetaInfo{ "is_skinned", "Is Skinned", "Is Skinned." }).traits(MetaFlags::read_only)

                .data<&assets::SubMesh::material>("material"_hs).custom<DataMetaInfo>(DataMetaInfo{ "material", "Material", "Material." }).traits(MetaFlags::read_only)

                // .template func<&eeng::editor::inspect_glmvec3>(eeng::literals::inspect_hs)
                ;
            register_asset<assets::SubMesh>();
        }

        // ModelDataAsset
        {
            entt::meta_factory<assets::ModelDataAsset>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "assets.ModelDataAsset", .name = "ModelDataAsset", .tooltip = "ModelDataAsset." })
                .traits(MetaFlags::none)

                .data<&assets::ModelDataAsset::positions>("positions"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "positions", "Positions", "Positions." })
                .traits(MetaFlags::read_only)

                .data<&assets::ModelDataAsset::normals>("normals"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "normals", "Normals", "Normals." })
                .traits(MetaFlags::read_only)

                .data<&assets::ModelDataAsset::tangents>("tangents"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "tangents", "Tangents", "Tangents." })
                .traits(MetaFlags::read_only)

                .data<&assets::ModelDataAsset::binormals>("binormals"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "binormals", "Binormals", "Binormals." })
                .traits(MetaFlags::read_only)

                .data<&assets::ModelDataAsset::texcoords>("texcoords"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "texcoords", "Texcoords", "Texcoords." })
                .traits(MetaFlags::read_only)

                // std::vector<SkinData> skin;

                .data<&assets::ModelDataAsset::indices>("indices"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "indices", "Indices", "Indices." })
                .traits(MetaFlags::read_only)

                .data<&assets::ModelDataAsset::submeshes>("submeshes"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "submeshes", "Submeshes", "Submeshes." })
                .traits(MetaFlags::read_only)

                // VecTree<SkeletonNode> nodetree;
                // std::vector<Bone> bones;
                // std::vector<AnimClip> animations;

                ;
            register_asset<assets::ModelDataAsset>();
        }

        // === MOCK RESOURCES ===

        // mock::Mesh
        // register_asset<mock::Mesh>();
        entt::meta_factory<mock::Mesh>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.mock.Mesh", .name = "Mesh", .tooltip = "This is a mock mesh type." })
            .traits(MetaFlags::none)

            .data<&mock::Mesh::vertices>("vertices"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "vertices", "Vertices", "A vector of vertex positions." })
            .traits(MetaFlags::none)
            ;
        // warm_start_meta_type<mock::Mesh>();
        // meta::type_id_map()["eeng.mock.Mesh"] = entt::resolve<mock::Mesh>().id();
        register_asset<mock::Mesh>();

        // mock::Texture
        // register_asset<mock::Texture>();
        entt::meta_factory<mock::Texture>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.mock.Texture", .name = "Texture", .tooltip = "This is a mock Texture type." })
            .traits(MetaFlags::none)

            .data<&mock::Texture::name>("name"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "The name of the texture." })
            .traits(MetaFlags::read_only)
            ;
        // warm_start_meta_type<mock::Texture>();
        // meta::type_id_map()["eeng.mock.Texture"] = entt::resolve<mock::Texture>().id();
        register_asset<mock::Texture>();

        // mock::Model
        entt::meta_factory<mock::Model>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.mock.Model", .name = "Model", .tooltip = "This is a mock model type." })
            .traits(MetaFlags::none)

            .data<&mock::Model::meshes>("meshes"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "meshes", "Meshes", "A vector of mesh references." })
            .traits(MetaFlags::none)

            .data<&mock::Model::textures>("textures"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "textures", "Textures", "A vector of texture references." })
            .traits(MetaFlags::none)
            ;
        register_asset<mock::Model>();
        // warm_start_meta_type<mock::Model>();
        // meta::type_id_map()["eeng.mock.Model"] = entt::resolve<mock::Model>().id();

        // entt::meta<Texture>()
        //     .type("Texture"_hs)
        //     .func<&assure_storage<Texture>>("assure_storage"_hs);
        //
        // + forward_as_meta
        // auto assure_fn = entt::resolve<Texture>().func("assure_storage"_hs);
        // assure_fn.invoke({}, registry);

        // mock::MockResource1
        entt::meta_factory<mock::MockResource1>{}
        // .type("MockResource1"_hs)
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.mock.MockResource1", .name = "MockResource1", .tooltip = "This is a mock resource type." })
            .traits(MetaFlags::none)

            // Register member 'x' with DisplayInfo and DataFlags
            .data<&mock::MockResource1::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "An integer member 'x'" })
            .traits(MetaFlags::read_only)

            // Register member 'y' with DisplayInfo and multiple DataFlags
            .data<&mock::MockResource1::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "A float member 'y'" })
            .traits(MetaFlags::hidden | MetaFlags::read_only)

            // Required for all resource types
            // .template func<&assure_storage<mock::MockResource1>>(eeng::literals::assure_storage_hs)

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
        // warm_start_meta_type<mock::MockResource1>();
        // meta::type_id_map()["eeng.mock.MockResource1"] = entt::resolve<mock::MockResource1>().id();
        register_asset<mock::MockResource1>();

        // register_asset<mock::MockResource2>();
    }

} // namespace eeng
