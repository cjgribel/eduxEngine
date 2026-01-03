// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "config.h"
#include "EngineContext.hpp"

#include "ComponentMetaReg.hpp"
#include "EntityMetaHelpers.hpp"

//#include "ResourceTypes.hpp"
#include "ecs/TransformComponent.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/ModelComponent.hpp"
#include "ecs/CoreComponents.hpp"
#include "ecs/MockComponents.hpp"
#include "ecs/systems/TransformSystem.hpp"
#include "mock/MockTypes.hpp"
#include "mock/CopySignaller.hpp"

#include "editor/EntityRefInspect.hpp"
#include "editor/GuidInspect.hpp"

#include "MetaLiterals.h"
#include "meta/GLMMetaReg.hpp"
//#include "Storage.hpp"
#include "MetaInfo.h"
// #include "IResourceManager.hpp" 
//#include "ResourceManager.hpp" // For AssetRef<T>, AssetMetaData, ResourceManager::load<>/unload
#include "LogMacros.h"

// #include <iostream>
#include <entt/entt.hpp>
// #include <entt/meta/pointer.hpp>
#include <nlohmann/json.hpp> // -> TYPE HELPER

    /*
    Note:
        Also note entt::as_cref_t

        TL;DR
        Avoiding (big) data members to be copied in the inspector:
        use a read only trait (disabling editing via ImGui) + entt::as_ref_t
        https://github.com/skypjack/entt/wiki/Runtime-reflection-system#user-defined-data

        Without entt::as_ref_t -> meta_data.get() returns a value meta_any
        With entt::as_ref_t -> meta_data.get() returns a reference meta_any

        If a data member can be edited in the inspector, we should not use entt::as_ref_t
        Why? Theory: the source data will be assigned directly, rather than a copy held by a command

        If a data member is read only, we should be able to use entt::as_ref_t,
        this avoiding copying, since no assignemt is made.
    */

namespace eeng
{
    using namespace eeng::ecs::mock;

    namespace
    {
        template<class T>
        void warm_start_meta_type()
        {
            if (!entt::resolve<T>())
                throw std::runtime_error("entt::resolve() failed for component type");
        }

        template<typename T>
        void assure_type_storage(entt::registry& registry)
        {
            (void)registry.storage<T>();
        }
    }

    namespace
    {
        //
        // Standard component meta functions
        //

#if 0
        template<class T>
        void bind_component(const Guid& guid, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            rm.resolve_asset<T>(guid, ctx);

            // + bind entity references via entity_manager
        }

        template<class T>
        void unbind_component(const Guid& guid, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            rm.unresolve_asset<T>(guid, ctx);

            // + unbind entity references via entity_manager
        }

        // Validates references?
        template<class T>
        bool validate_component(const Guid& guid, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            return rm.validate_asset<T>(guid);
        }

        // Validates references recursively?
        template<class T>
        bool validate_asset_recursive(const Guid& guid, EngineContext& ctx)
        {
            auto& rm = static_cast<ResourceManager&>(*ctx.resource_manager);
            return rm.validate_asset_recursive<T>(guid);
        }
#endif

        template<typename T>
        void register_component()
        {
            entt::meta_factory<T>()

                // Assure entt storage
                .template func<&assure_type_storage<T>, entt::as_void_t>(literals::assure_component_storage_hs)

                // Collect asset references
                .template func<&meta::collect_asset_guids<T>, entt::as_void_t>(literals::collect_asset_guids_hs)

                // Bind referenced assets
                .template func<&meta::bind_asset_refs<T>, entt::as_void_t>(literals::bind_asset_refs_hs)
                .template func<&meta::bind_entity_refs<T>, entt::as_void_t>(literals::bind_entity_refs_hs)

                // TODO -> Unbind referenced assets
                // ...

                ;

            meta::register_type<T>();
            warm_start_meta_type<T>();
        }

        template<typename T>
        void register_helper_type()
        {
            meta::register_type<T>();
            warm_start_meta_type<AssetRef<T>>();
        }
    } // namespace

    namespace
    {
        // TODO -> editor/GuidSerialize.hpp (takes nlohmann::json dependency with it)
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

    void register_component_meta_types(EngineContext& ctx)
    {
        EENG_LOG_INFO(&ctx, "Registering component meta types...");
        meta::register_glm_meta_types();

        // --- Guid ------------------------------------------------------------
        entt::meta_factory<Guid>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.Guid", .name = "Guid", .tooltip = "A globally unique identifier." })
            .traits(MetaFlags::none)

            .func<&serialize_Guid>(eeng::literals::serialize_hs)
            .func<&deserialize_Guid>(eeng::literals::deserialize_hs)

            .func<&eeng::editor::inspect_Guid>(eeng::literals::inspect_hs)
            .template custom<FuncMetaInfo>(FuncMetaInfo{ "inspect_Guid", "Inspect GUID" })
            ;
        register_helper_type<Guid>();
        // warm_start_meta_type<Guid>();
        // meta::type_id_map()["eeng.Guid"] = entt::resolve<Guid>().id();

        // --- EntityRef -------------------------------------------------------
        entt::meta_factory<eeng::ecs::EntityRef>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.EntityRef", .name = "EntityRef", .tooltip = "A reference to an entity." })
            .traits(MetaFlags::none)

            // (Serialize) (Clone)
            // Guid
            .template data<&eeng::ecs::EntityRef::guid>("guid"_hs)
            // .template data<&eeng::ecs::EntityRef::set_guid, &eeng::ecs::EntityRef::get_guid>("guid"_hs)
            .template custom<DataMetaInfo>(DataMetaInfo{ "guid", "Guid", "A globally unique identifier." })
            .traits(MetaFlags::readonly_inspection)
            // Entity
            // (Not serialized) (Not cloned)

            // (Inspect)
            .func<&eeng::editor::inspect_EntityRef>(eeng::literals::inspect_hs)
            .template custom<FuncMetaInfo>(FuncMetaInfo{ "inspect_EntityRef", "Inspect entity reference" })

            //     .template data<&eeng::ecs::EntityRef::entity>("entity"_hs)
            //     .template custom<DataMetaInfo>(DataMetaInfo{ "entity", "Entity", "The referenced entity." })
            //     .traits(MetaFlags::readonly_inspection)   
            ;
        register_helper_type<eeng::ecs::EntityRef>();
        // warm_start_meta_type<eeng::ecs::EntityRef>();
        // meta::type_id_map()["eeng.ecs.EntityRef"] = entt::resolve<eeng::ecs::EntityRef>().id();

        // --- CopySignaller ---------------------------------------------------

        entt::meta_factory<eeng::CopySignaller>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.CopySignaller", .name = "CopySignaller", .tooltip = "Logs copy & move ops." })
            .traits(MetaFlags::none)

            .data<&eeng::CopySignaller::data/*, entt::as_ref_t*/>("data"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "data", "data", "data" })
            .traits(MetaFlags::none) // readonly_inspection -> as_ref_t
            ;
        register_component<eeng::CopySignaller>();

        // --- MockPlayerComponent ---------------------------------------------

        entt::meta_factory<eeng::ecs::mock::MockPlayerComponent>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.mock.MockPlayerComponent", .name = "MockPlayerComponent", .tooltip = "A mock player component for testing." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::mock::MockPlayerComponent::position>("position"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "position", "Position", "Player position." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::mock::MockPlayerComponent::health>("health"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "health", "Health", "Player health." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::mock::MockPlayerComponent::camera_ref>("camera_ref"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "camera_ref", "Camera Reference", "Reference to the player's camera entity." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::mock::MockPlayerComponent::model_ref>("model_ref"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "model_ref", "Model Reference", "Reference to the player's model asset." })
            .traits(MetaFlags::none)
            ;
        register_component<eeng::ecs::mock::MockPlayerComponent>();
        // warm_start_meta_type<eeng::ecs::mock::MockPlayerComponent>();
        // meta::type_id_map()["eeng.ecs.mock.MockPlayerComponent"] = entt::resolve<eeng::ecs::mock::MockPlayerComponent>().id();

        // --- MockCameraComponent ---------------------------------------------
        entt::meta_factory<eeng::ecs::mock::MockCameraComponent>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.mock.MockCameraComponent", .name = "MockCameraComponent", .tooltip = "A mock camera component for testing." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::mock::MockCameraComponent::position>("position"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "position", "Position", "Camera position." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::mock::MockCameraComponent::fov>("fov"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "fov", "Field of View", "Camera field of view." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::mock::MockCameraComponent::target_ref>("target_ref"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "target_ref", "Target Reference", "Reference to the camera target entity." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::mock::MockCameraComponent::model_ref>("model_ref"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "model_ref", "Model Reference", "Reference to the camera's model asset." })
            .traits(MetaFlags::none)
            ;
        register_component<eeng::ecs::mock::MockCameraComponent>();
        // warm_start_meta_type<eeng::ecs::mock::MockCameraComponent>();
        // meta::type_id_map()["eeng.ecs.mock.MockCameraComponent"] = entt::resolve<eeng::ecs::mock::MockCameraComponent>().id();

        // --- TransformComponent ----------------------------------------------

        entt::meta_factory<eeng::ecs::TransformComponent>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.TransformComponent", .name = "TransformComponent", .tooltip = "Entity transform data." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::TransformComponent::position>("position"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "position", "Position", "Local position." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::TransformComponent::rotation>("rotation"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "rotation", "Rotation", "Local rotation." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::TransformComponent::scale>("scale"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "scale", "Scale", "Local scale." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::TransformComponent::local_matrix>("local_matrix"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "local_matrix", "Local Matrix", "Derived local transform matrix." })
            .traits(MetaFlags::readonly_inspection)

            .data<&eeng::ecs::TransformComponent::world_matrix>("world_matrix"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "world_matrix", "World Matrix", "Derived world transform matrix." })
            .traits(MetaFlags::readonly_inspection)

            .data<&eeng::ecs::TransformComponent::world_rotation>("world_rotation"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "world_rotation", "World Rotation", "Derived world rotation." })
            .traits(MetaFlags::readonly_inspection)

            .data<&eeng::ecs::TransformComponent::world_rotation_matrix>("world_rotation_matrix"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "world_rotation_matrix", "World Rotation Matrix", "Derived world rotation matrix." })
            .traits(MetaFlags::readonly_inspection)

            .func<&eeng::ecs::systems::TransformSystem::on_component_post_assign>(eeng::literals::post_assign_hs)
            .template custom<FuncMetaInfo>(FuncMetaInfo{ "post_assign", "Post-assign hook for component edits." })
            ;
        register_component<ecs::TransformComponent>();


        // --- HeaderComponent -------------------------------------------------
#if 0
        // chunk_tag callback
// struct ChunkModifiedEvent { std::string chunk_tag; Entity entity; };
        using TypeModifiedCallbackType = std::function<void(entt::meta_any, const eeng::ecs::Entity&)>;
        const TypeModifiedCallbackType chunk_tag_cb = [context](entt::meta_any any, const Entity& entity)
            {
                const auto& new_tag = any.cast<std::string>();
                // std::cout << new_tag << ", " << entity.to_integral() << std::endl;

                // Dispatch immediately since entity may be in an invalid state
                assert(!context.dispatcher.expired());
                context.dispatcher.lock()->dispatch(ChunkModifiedEvent{ entity, new_tag });
            };
#endif
        entt::meta_factory<eeng::ecs::HeaderComponent>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.HeaderComponent", .name = "HeaderComponent", .tooltip = "Metadata for HeaderComponent." })
            .traits(MetaFlags::none)

            // Name
            .data<&eeng::ecs::HeaderComponent::name>("name"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Entity name." })
            .traits(MetaFlags::none)

            // Guid
            .data<&eeng::ecs::HeaderComponent::guid>("guid"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "guid", "Guid", "A globally unique identifier." })
            .traits(MetaFlags::readonly_inspection)

            // Parent Entity
            .data<&eeng::ecs::HeaderComponent::parent_entity>("parent_entity"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "parent_entity", "Parent Entity", "The parent entity of this entity." })
            .traits(MetaFlags::readonly_inspection)
            ;
        register_component<ecs::HeaderComponent>();
        // warm_start_meta_type<eeng::ecs::HeaderComponent>();
        // meta::type_id_map()["eeng.ecs.HeaderComponent"] = entt::resolve<eeng::ecs::HeaderComponent>().id();

        // --- ModelComponent --------------------------------------------------
        {
            entt::meta_factory<eeng::ecs::ModelComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.ModelComponent", .name = "ModelComponent", .tooltip = "ModelComponent." })
                .traits(MetaFlags::none)

                // Name
                .data<&eeng::ecs::ModelComponent::name>("name"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Entity name." })
                .traits(MetaFlags::none)

                // Model asset ref
                .data<&eeng::ecs::ModelComponent::model_ref>("model_ref"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "model_ref", "Model reference", "Model Reference." })
                // .traits(MetaFlags::readonly_inspection)
                .traits(MetaFlags::none)

                // Anim clip index
                .data<&eeng::ecs::ModelComponent::clip_index>("clip_index"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "clip_index", "Clip Index", "Clip Index." })
                .traits(MetaFlags::none)

                // Anim clip time
                // .data<&eeng::ecs::ModelComponent::clip_time>("clip_time"_hs)
                // .custom<DataMetaInfo>(DataMetaInfo{ "clip_time", "Clip Time", "Clip Time." })
                // .traits(MetaFlags::none)

                // Anim clip speed
                .data<&eeng::ecs::ModelComponent::clip_speed>("clip_speed"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "clip_speed", "Clip Speed", "Clip Speed." })
                .traits(MetaFlags::none)

                // Anim clip loop
                .data<&eeng::ecs::ModelComponent::loop>("loop"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "loop", "Loop", "Loop clip." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::ModelComponent>();
        }

        // --- MockMixComponent + nested types ---------------------------------

        entt::meta_factory<ecs::mock::MockUVcoords>()
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.mock.MockUVcoords", .name = "MockUVcoords", .tooltip = "Metadata for MockUVcoords." })
            .traits(MetaFlags::none)

            .data<&ecs::mock::MockUVcoords::u>("u"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "u", "U", "" }).traits(MetaFlags::none)

            .data<&ecs::mock::MockUVcoords::v>("v"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "v", "V", "" }).traits(MetaFlags::none)

            .func<&ecs::mock::MockUVcoords::to_string>(literals::to_string_hs)
            ;
        register_helper_type<ecs::mock::MockUVcoords>();
        // meta::type_id_map()["ecs::mock::MockUVcoords"] = entt::resolve<ecs::mock::MockUVcoords>().id();

        entt::meta_factory<ecs::mock::MockVec3>()
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.mock.MockVec3", .name = "MockVec3", .tooltip = "Metadata for MockVec3." })
            .traits(MetaFlags::none)

            .data<&ecs::mock::MockVec3::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "" }).traits(MetaFlags::none)

            .data<&ecs::mock::MockVec3::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "" }).traits(MetaFlags::none)

            .data<&ecs::mock::MockVec3::z>("z"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "z", "Z", "" }).traits(MetaFlags::none)

            .data<&ecs::mock::MockVec3::uv_coords>("uv_coords"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "uv_coords", "UV Coords", "" }).traits(MetaFlags::none)

#ifdef JSON
            //.func<&MockVec3_to_json>(to_json_hs)
            .func < [](nlohmann::json& j, const void* ptr) { to_json(j, *static_cast<const MockVec3*>(ptr)); }, entt::as_void_t > (to_json_hs)
            .func < [](const nlohmann::json&& ? j, void* ptr) { from_json(j, *static_cast<MockVec3*>(ptr)); }, entt::as_void_t > (from_json_hs)
#endif
            //        .func<&MockVec3::to_string>(to_string_hs)
            .func<&ecs::mock::MockVec3_to_string>(literals::to_string_hs)
            ;
        register_helper_type<ecs::mock::MockVec3>();
        // meta::type_id_map()["eeng.ecs.mock.MockVec3"] = entt::resolve<ecs::mock::MockVec3>().id();

        entt::meta_factory<ElementType>()
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ElementType", .name = "ElementType", .tooltip = "Metadata for ElementType." })
            .traits(MetaFlags::none)

            .data<&ElementType::m>("m"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "m", "M", "" }).traits(MetaFlags::none)

            //        .func<&debugvec3::to_string>(to_string_hs)
            ;
        register_helper_type<ElementType>();
        // meta::type_id_map()["eeng.ElementType"] = entt::resolve<ElementType>().id();

        auto enum_info = TypeMetaInfo
        {
            .id = "eeng.AnEnum",
            .name = "AnEnum",
            .tooltip = "AnEnum is a test enum with three values.",
            .underlying_type = entt::resolve<std::underlying_type_t<AnEnum>>()
        };
        entt::meta_factory<AnEnum>()
            // .type("AnEnum"_hs)
            .custom<TypeMetaInfo>(enum_info)
            .traits(MetaFlags::none)

            .data<AnEnum::Hello>("Hello"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Hello", "Greeting in English." })
            .traits(MetaFlags::none)

            .data<AnEnum::Bye>("Bye"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Bye", "Farewell in English." })
            .traits(MetaFlags::none)

            .data<AnEnum::Hola>("Hola"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Hola", "Greeting in Spanish." })
            .traits(MetaFlags::none)
            ;
        register_helper_type<AnEnum>();

        // register_component<MockMixComponent>();
        entt::meta_factory<MockMixComponent>()
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.mock.MockMixComponent", .name = "MockMixComponent", .tooltip = "A mock component with mixed data types for testing." })
            .traits(MetaFlags::none)

            .data<&MockMixComponent::bool_flag/*, entt::as_ref_t*/>("bool_flag"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "bool_flag", "Bool Flag", "A boolean flag." }).traits(MetaFlags::none)

            .data<&MockMixComponent::float_scalar>("float_scalar"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "float_scalar", "Float Scalar", "A float value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::int_scalar/*, entt::as_ref_t*/>("int_scalar"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "int_scalar", "Int Scalar", "An integer value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::int_scalar_2>("int_scalar_2"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "int_scalar_2", "Int Scalar 2", "An integer value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::position>("position"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "position", "Position", "A 3D position vector." }).traits(MetaFlags::none)

            .data<&MockMixComponent::string_value>("string_value"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "string_value", "String Value", "A sample string." }).traits(MetaFlags::none)

            .data<&MockMixComponent::int_array3>("int_array3"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "int_array3", "Int Array3", "An array of integers." }).traits(MetaFlags::none)

            .data<&MockMixComponent::element_vector>("element_vector"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "element_vector", "Element Vector", "A vector of ElementType values." }).traits(MetaFlags::none)

            .data<&MockMixComponent::int_float_map>("int_float_map"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "int_float_map", "Int Float Map", "A map of integers to floats." }).traits(MetaFlags::none)

            .data<&MockMixComponent::int_element_map>("int_element_map"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "int_element_map", "Int Element Map", "A map of integers to ElementType values." }).traits(MetaFlags::none)

            .data<&MockMixComponent::element_int_map>("element_int_map"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "element_int_map", "Element Int Map", "A map of ElementType values to integers." }).traits(MetaFlags::none)

            .data<&MockMixComponent::int_set>("int_set"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "int_set", "Int Set", "A set of integers." }).traits(MetaFlags::none)

            .data<&MockMixComponent::enum_value>("enum_value"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "enum_value", "Enum Value", "An example enum value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::nested_int_vectors>("nested_int_vectors"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "nested_int_vectors", "Nested Int Vectors", "A vector of vectors of integers." }).traits(MetaFlags::none)

            .data<&MockMixComponent::enum_vector>("enum_vector"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "enum_vector", "Enum Vector", "A vector of enum values." }).traits(MetaFlags::none)

            .data<&MockMixComponent::enum_int_map>("enum_int_map"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "enum_int_map", "Enum Int Map", "A map of enum values to integers." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_vec2>("glm_vec2"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_vec2", "GLM Vec2", "A glm::vec2 value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_vec3>("glm_vec3"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_vec3", "GLM Vec3", "A glm::vec3 value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_vec4>("glm_vec4"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_vec4", "GLM Vec4", "A glm::vec4 value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_ivec2>("glm_ivec2"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_ivec2", "GLM IVec2", "A glm::ivec2 value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_ivec3>("glm_ivec3"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_ivec3", "GLM IVec3", "A glm::ivec3 value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_ivec4>("glm_ivec4"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_ivec4", "GLM IVec4", "A glm::ivec4 value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_quat>("glm_quat"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_quat", "GLM Quat", "A glm::quat value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_mat2>("glm_mat2"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_mat2", "GLM Mat2", "A glm::mat2 value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_mat3>("glm_mat3"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_mat3", "GLM Mat3", "A glm::mat3 value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_mat4>("glm_mat4"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_mat4", "GLM Mat4", "A glm::mat4 value." }).traits(MetaFlags::none)

            .data<&MockMixComponent::glm_vec3_vector>("glm_vec3_vector"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "glm_vec3_vector", "GLM Vec3 Vector", "A vector of glm::vec3 values." }).traits(MetaFlags::none)

            // to_string, member version
                //.func<&DebugClass::to_string>(to_string_hs)
            // to_string, free function version
                //.func<&to_string_DebugClass>(to_string_hs)
            // clone
                //.func<&cloneDebugClass>(clone_hs)
            ;
        register_component<MockMixComponent>();
        // warm_start_meta_type<MockMixComponent>();
        // meta::type_id_map()["eeng.ecs.mock.MockMixComponent"] = entt::resolve<MockMixComponent>().id();
    }

} // namespace eeng
