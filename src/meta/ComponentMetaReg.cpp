// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "config.h"
#include "EngineContext.hpp"

#include "ComponentMetaReg.hpp"
#include "EntityMetaHelpers.hpp"

//#include "ResourceTypes.hpp"
#include "ecs/TransformComponent.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/CoreComponents.hpp"
#include "ecs/MockComponents.hpp"
#include "mock/MockTypes.hpp"

#include "editor/EntityRefInspect.hpp"
#include "editor/GuidInspect.hpp"

#include "MetaLiterals.h"
//#include "Storage.hpp"
#include "MetaInfo.h"
// #include "IResourceManager.hpp" 
//#include "ResourceManager.hpp" // For AssetRef<T>, AssetMetaData, ResourceManager::load<>/unload
#include "LogMacros.h"

// #include <iostream>
#include <entt/entt.hpp>
// #include <entt/meta/pointer.hpp>
#include <nlohmann/json.hpp> // -> TYPE HELPER

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

            // if (!registry.storage<T>()) registry.storage<T>();
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
                .template func<&assure_type_storage<T>, entt::as_void_t>(literals::assure_component_storage_hs)

                .template func<&meta::collect_asset_guids<T>, entt::as_void_t>(literals::collect_asset_guids_hs)

                .template func<&meta::bind_asset_refs<T>, entt::as_void_t>(literals::bind_asset_refs_hs)
                .template func<&meta::bind_entity_refs<T>, entt::as_void_t>(literals::bind_entity_refs_hs)
                // + unbind?

                ;

            warm_start_meta_type<T>();
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

        // --- Guid ------------------------------------------------------------
        entt::meta_factory<Guid>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.Guid", .name = "Guid", .tooltip = "A globally unique identifier." })

            .func<&serialize_Guid>(eeng::literals::serialize_hs)
            .func<&deserialize_Guid>(eeng::literals::deserialize_hs)

            .func<&eeng::editor::inspect_Guid>(eeng::literals::inspect_hs)
            .template custom<FuncMetaInfo>(FuncMetaInfo{ "inspect_Guid", "Inspect GUID" })
            ;
        warm_start_meta_type<Guid>();

        // --- EntityRef -------------------------------------------------------
        entt::meta_factory<eeng::ecs::EntityRef>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.EntityRef", .name = "EntityRef", .tooltip = "A reference to an entity." })

            // (Serialize) (Clone)
            // Guid
            .template data<&eeng::ecs::EntityRef::guid>("guid"_hs)
            // .template data<&eeng::ecs::EntityRef::set_guid, &eeng::ecs::EntityRef::get_guid>("guid"_hs)
            .template custom<DataMetaInfo>(DataMetaInfo{ "guid", "Guid", "A globally unique identifier." })
            .traits(MetaFlags::read_only)
            // Entity
            // (Not serialized) (Not cloned)

            // (Inspect)
            .func<&eeng::editor::inspect_EntityRef>(eeng::literals::inspect_hs)
            .template custom<FuncMetaInfo>(FuncMetaInfo{ "inspect_EntityRef", "Inspect entity reference" })

            //     .template data<&eeng::ecs::EntityRef::entity>("entity"_hs)
            //     .template custom<DataMetaInfo>(DataMetaInfo{ "entity", "Entity", "The referenced entity." })
            //     .traits(MetaFlags::read_only)   
            ;
        warm_start_meta_type<eeng::ecs::EntityRef>();

        // --- MockPlayerComponent ---------------------------------------------
        register_component<eeng::ecs::mock::MockPlayerComponent>();
        entt::meta_factory<eeng::ecs::mock::MockPlayerComponent>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.mock.MockPlayerComponent", .name = "MockPlayerComponent", .tooltip = "A mock player component for testing." })

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

        // --- MockCameraComponent ---------------------------------------------
        register_component<eeng::ecs::mock::MockCameraComponent>();
        entt::meta_factory<eeng::ecs::mock::MockCameraComponent>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.mock.MockCameraComponent", .name = "MockCameraComponent", .tooltip = "A mock camera component for testing." })

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

        // --- TransformComponent ----------------------------------------------
        register_component<ecs::TransformComponent>();
#if 0
        entt::meta<ecs::Transform>()
            //.type("Transform"_hs)                 // <- this hashed string is used implicitly
            //.prop(display_name_hs, "Transform")  // <- Can be used without .type()

            .data<&Transform::x>("x"_hs).prop(display_name_hs, "x")
            .data<&Transform::y>("y"_hs).prop(display_name_hs, "y")
            .data<&Transform::angle>("angle"_hs).prop(display_name_hs, "angle")
            .data<&Transform::x_global>("x_global"_hs).prop(display_name_hs, "x_global").prop(readonly_hs, true)
            .data<&Transform::y_global>("y_global"_hs).prop(display_name_hs, "y_global").prop(readonly_hs, true)
            .data<&Transform::angle_global>("angle_global"_hs).prop(display_name_hs, "angle_global").prop(readonly_hs, true)
            ;
#endif

        // --- HeaderComponent -------------------------------------------------
        register_component<ecs::HeaderComponent>();
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

            // Name
            .data<&eeng::ecs::HeaderComponent::name>("name"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Entity name." })
            .traits(MetaFlags::none)

            // Guid
            .data<&eeng::ecs::HeaderComponent::guid>("guid"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "guid", "Guid", "A globally unique identifier." })
            .traits(MetaFlags::read_only)

            // Parent Entity
            .data<&eeng::ecs::HeaderComponent::parent_entity>("parent_entity"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "parent_entity", "Parent Entity", "The parent entity of this entity." })
            .traits(MetaFlags::none)
            ;
#if 0
        entt::meta<HeaderComponent>()
            .type("HeaderComponent"_hs).prop(display_name_hs, "Header")

            .data<&HeaderComponent::name>("name"_hs).prop(display_name_hs, "name")
            .data<&HeaderComponent::chunk_tag>("chunk_tag"_hs).prop(display_name_hs, "chunk_tag") //.prop(readonly_hs, true)
            .prop("callback"_hs, chunk_tag_cb)

            .data<&HeaderComponent::guid>("guid"_hs).prop(display_name_hs, "guid").prop(readonly_hs, true)
            .data<&HeaderComponent::entity_parent>("entity_parent"_hs).prop(display_name_hs, "entity_parent").prop(readonly_hs, true)

            // Optional meta functions

            // to_string, member version
                //.func<&DebugClass::to_string>(to_string_hs)
            // to_string, lambda version
            .func < [](const void* ptr) {
            return static_cast<const HeaderComponent*>(ptr)->name;
            } > (to_string_hs)
                // inspect
                    // .func<&inspect_Transform>(inspect_hs)
                // clone
                    //.func<&cloneDebugClass>(clone_hs)
                ;
#endif

            // --- MockMixComponent + nested types -----------------------------

            entt::meta_factory<ecs::mock::MockUVcoords>()
                .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.mock.MockUVcoords", .name = "MockUVcoords", .tooltip = "Metadata for MockUVcoords." })
                // .type("UVcoords"_hs).prop(display_name_hs, "UVcoords")

                .data<&ecs::mock::MockUVcoords::u>("u"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "u", "U", "" }).traits(MetaFlags::none)

                .data<&ecs::mock::MockUVcoords::v>("v"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "v", "V", "" }).traits(MetaFlags::none)

                .func<&ecs::mock::MockUVcoords::to_string>(literals::to_string_hs)
                ;

            entt::meta_factory<ecs::mock::MockVec3>()
                .custom<TypeMetaInfo>(TypeMetaInfo{ "MockVec3", "Metadata for MockVec3." })
                // .type("MockVec3"_hs).prop(display_name_hs, "MockVec3")

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

            entt::meta_factory<ElementType>()
                .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ElementType", .name = "ElementType", .tooltip = "Metadata for ElementType." })
                // .type("ElementType"_hs).prop(display_name_hs, "ElementType")

                .data<&ElementType::m>("m"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "m", "M", "" }).traits(MetaFlags::none)

                //        .func<&debugvec3::to_string>(to_string_hs)
                ;

            auto enum_info = EnumTypeMetaInfo
            {
                .name = "AnEnum",
                .tooltip = "AnEnum is a test enum with three values.",
                .underlying_type = entt::resolve<std::underlying_type_t<AnEnum>>()
            };
            entt::meta_factory<AnEnum>()
                // .type("AnEnum"_hs)
                .custom<EnumTypeMetaInfo>(enum_info)

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

            register_component<MockMixComponent>();
            entt::meta_factory<MockMixComponent>()
                .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.mock.MockMixComponent", .name = "MockMixComponent", .tooltip = "A mock component with mixed data types for testing." })
                // .type("DebugClass"_hs).prop(display_name_hs, "DebugClass")

                .data<&MockMixComponent::flag/*, entt::as_ref_t*/>("flag"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "flag", "Flag", "A boolean flag." }).traits(MetaFlags::none)

                .data<&MockMixComponent::a>("a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "a", "A", "A float value." }).traits(MetaFlags::none)

                .data<&MockMixComponent::b/*, entt::as_ref_t*/>("b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "b", "B", "An integer value." }).traits(MetaFlags::none)

                .data<&MockMixComponent::c>("c"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "c", "C", "An integer value." }).traits(MetaFlags::none)

                .data<&MockMixComponent::position>("position"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "position", "Position", "A 3D position vector." }).traits(MetaFlags::none)

                .data<&MockMixComponent::somestring>("somestring"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "somestring", "Some String", "A sample string." }).traits(MetaFlags::none)

                .data<&MockMixComponent::vector1>("vector1"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "vector1", "Vector1", "An array of integers." }).traits(MetaFlags::none)

                .data<&MockMixComponent::vector2>("vector2"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "vector2", "Vector2", "An array of integers." }).traits(MetaFlags::none)

                .data<&MockMixComponent::map1>("map1"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "map1", "Map1", "A map of strings to integers." }).traits(MetaFlags::none)

                .data<&MockMixComponent::map2>("map2"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "map2", "Map2", "A map of strings to floats." }).traits(MetaFlags::none)

                .data<&MockMixComponent::map3>("map3"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "map3", "Map3", "A map of strings to strings." }).traits(MetaFlags::none)

                .data<&MockMixComponent::set1>("set1"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "set1", "Set1", "A set of strings." }).traits(MetaFlags::none)

                .data<&MockMixComponent::anEnum>("anEnum"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "anEnum", "An Enum", "An example enum value." }).traits(MetaFlags::none)

                .data<&MockMixComponent::nested_int_vectors>("nested_int_vectors"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "nested_int_vectors", "Nested Int Vectors", "A vector of vectors of integers." }).traits(MetaFlags::none)

                .data<&MockMixComponent::enum_vector>("enum_vector"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enum_vector", "Enum Vector", "A vector of enum values." }).traits(MetaFlags::none)

                .data<&MockMixComponent::enum_map>("enum_map"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enum_map", "Enum Map", "A map of enum values to integers." }).traits(MetaFlags::none)

                // to_string, member version
                    //.func<&DebugClass::to_string>(to_string_hs)
                // to_string, free function version
                    //.func<&to_string_DebugClass>(to_string_hs)
                // clone
                    //.func<&cloneDebugClass>(clone_hs)
                ;
    }

} // namespace eeng