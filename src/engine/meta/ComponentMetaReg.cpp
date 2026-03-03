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
#include "ecs/AnimationGraphComponent.hpp"
#include "ecs/ScriptComponent.hpp"
#include "ecs/StickyNoteComponent.hpp"
#include "ecs/TrailComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/PistonConstraintDriveComponent.hpp"
#include "ecs/PistonAnimSyncComponent.hpp"
#include "ecs/TransformSocketComponent.hpp"
#include "ecs/TwoAnchorAlignComponent.hpp"
#include "ecs/CoreComponents.hpp"
#include "ecs/MockComponents.hpp"
#include "ecs/VehicleRig1Component.hpp"
#include "ecs/systems/TransformSystem.hpp"
#include "mock/MockTypes.hpp"
#include "mock/CopySignaller.hpp"
#include "editor/ecs/ThirdPersonCameraComponent.hpp"
#include "editor/ecs/FirstPersonCameraComponent.hpp"
#include "editor/ecs/TransformGizmoComponent.hpp"

#include "editor/EntityRefInspect.hpp"
#include "editor/GuidInspect.hpp"
#include "editor/AnimationGraphComponentInspect.hpp"
#include "editor/PhysicsComponentsInspect.hpp"

#include "MetaLiterals.h"
#include "meta/GLMMetaReg.hpp"
//#include "Storage.hpp"
#include "MetaInfo.h"
#include "serializers/GuidSerialize.hpp"
// #include "IResourceManager.hpp" 
//#include "ResourceManager.hpp" // For AssetRef<T>, AssetMetaData, ResourceManager::load<>/unload
#include "LogMacros.h"

// #include <iostream>
#include <entt/entt.hpp>
// #include <entt/meta/pointer.hpp>
#ifdef JSON
#include <nlohmann/json.hpp> // -> TYPE HELPER
#endif
#include <type_traits>

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
            static_assert(std::is_default_constructible_v<T>,
                "Component must be default constructible for editor add/remove.");

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

    void register_component_meta_types(EngineContext& ctx)
    {
        EENG_LOG_INFO(&ctx, "Registering component meta types...");
        meta::register_glm_meta_types();

        // --- Guid ------------------------------------------------------------
        entt::meta_factory<Guid>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.Guid", .name = "Guid", .tooltip = "A globally unique identifier." })
            .traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_Guid>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_Guid>(eeng::literals::deserialize_hs)

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

        // --- ThirdPersonCameraComponent ------------------------------------

        entt::meta_factory<eeng::editor::ThirdPersonCameraComponent>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{
            .id = "eeng.editor.ThirdPersonCameraComponent",
            .name = "ThirdPersonCameraComponent",
            .tooltip = "Pivot/third-person camera settings."
            })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::active>("active"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "active", "Active", "Responds to input when true." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::target>("target"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "target", "Target", "Entity to follow and orbit around." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::target_offset>("target_offset"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "target_offset", "Target Offset", "Offset added to the target position." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::distance>("distance"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "distance", "Distance", "Orbit radius from the pivot." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::mouse_sensitivity>("mouse_sensitivity"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "mouse_sensitivity", "Mouse Sensitivity", "Radians per pixel." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::controller_look_speed>("controller_look_speed"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "controller_look_speed", "Controller Look Speed", "Radians per second." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::move_speed>("move_speed"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "move_speed", "Move Speed", "Units per second." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::near_plane>("near_plane"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "near_plane", "Near Plane", "Camera near clip plane." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::far_plane>("far_plane"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "far_plane", "Far Plane", "Camera far clip plane." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::yaw>("yaw"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "yaw", "Yaw", "Horizontal orbit angle (radians)." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::pitch>("pitch"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "pitch", "Pitch", "Vertical orbit angle (radians)." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::ThirdPersonCameraComponent::look_at>("look_at"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "look_at", "Look At", "Cached look-at position." })
            .traits(MetaFlags::readonly_inspection)

            .data<&eeng::editor::ThirdPersonCameraComponent::position>("position"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "position", "Position", "Cached camera position." })
            .traits(MetaFlags::readonly_inspection)

            .data<&eeng::editor::ThirdPersonCameraComponent::forward>("forward"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "forward", "Forward", "Cached forward direction." })
            .traits(MetaFlags::readonly_inspection)

            .data<&eeng::editor::ThirdPersonCameraComponent::model_to_view>("model_to_view"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "model_to_view", "Model To View", "Cached view matrix." })
            .traits(MetaFlags::readonly_inspection)

            .data<&eeng::editor::ThirdPersonCameraComponent::view_to_world>("view_to_world"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "view_to_world", "View To World", "Cached inverse view matrix." })
            .traits(MetaFlags::readonly_inspection)
            ;
        register_component<eeng::editor::ThirdPersonCameraComponent>();

        // --- FirstPersonCameraComponent ------------------------------------

        entt::meta_factory<eeng::editor::FirstPersonCameraComponent>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{
            .id = "eeng.editor.FirstPersonCameraComponent",
            .name = "FirstPersonCameraComponent",
            .tooltip = "First-person/free-look camera settings."
            })
            .traits(MetaFlags::none)

            .data<&eeng::editor::FirstPersonCameraComponent::active>("active"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "active", "Active", "Responds to input when true." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::FirstPersonCameraComponent::position>("position"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "position", "Position", "Camera position." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::FirstPersonCameraComponent::move_speed>("move_speed"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "move_speed", "Move Speed", "Units per second." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::FirstPersonCameraComponent::mouse_sensitivity>("mouse_sensitivity"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "mouse_sensitivity", "Mouse Sensitivity", "Radians per pixel." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::FirstPersonCameraComponent::controller_look_speed>("controller_look_speed"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "controller_look_speed", "Controller Look Speed", "Radians per second." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::FirstPersonCameraComponent::near_plane>("near_plane"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "near_plane", "Near Plane", "Camera near clip plane." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::FirstPersonCameraComponent::far_plane>("far_plane"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "far_plane", "Far Plane", "Camera far clip plane." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::FirstPersonCameraComponent::yaw>("yaw"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "yaw", "Yaw", "Horizontal view angle (radians)." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::FirstPersonCameraComponent::pitch>("pitch"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "pitch", "Pitch", "Vertical view angle (radians)." })
            .traits(MetaFlags::none)

            .data<&eeng::editor::FirstPersonCameraComponent::forward>("forward"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "forward", "Forward", "Cached forward direction." })
            .traits(MetaFlags::readonly_inspection)

            .data<&eeng::editor::FirstPersonCameraComponent::model_to_view>("model_to_view"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "model_to_view", "Model To View", "Cached view matrix." })
            .traits(MetaFlags::readonly_inspection)

            .data<&eeng::editor::FirstPersonCameraComponent::view_to_world>("view_to_world"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "view_to_world", "View To World", "Cached inverse view matrix." })
            .traits(MetaFlags::readonly_inspection)
            ;
        register_component<eeng::editor::FirstPersonCameraComponent>();

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


        // --- TransformGizmoComponent --------------------------------------

        using GizmoMode = eeng::editor::TransformGizmo::Mode;
        using GizmoSpace = eeng::editor::TransformGizmo::Space;
        using GizmoSettings = eeng::editor::TransformGizmo::Settings;
        using GizmoComponent = eeng::editor::TransformGizmoComponent;

        auto gizmo_mode_info = TypeMetaInfo
        {
            .id = "eeng.editor.TransformGizmoMode",
            .name = "TransformGizmoMode",
            .tooltip = "Gizmo operation mode (translate/rotate/scale).",
            .underlying_type = entt::resolve<std::underlying_type_t<GizmoMode>>()
        };
        entt::meta_factory<GizmoMode>()
            .custom<TypeMetaInfo>(gizmo_mode_info)
            .traits(MetaFlags::none)

            .data<GizmoMode::Translate>("Translate"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Translate", "Translate gizmo." })
            .traits(MetaFlags::none)

            .data<GizmoMode::Rotate>("Rotate"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Rotate", "Rotate gizmo." })
            .traits(MetaFlags::none)

            .data<GizmoMode::Scale>("Scale"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Scale", "Scale gizmo." })
            .traits(MetaFlags::none)
            ;
        meta::register_type<GizmoMode>();
        warm_start_meta_type<GizmoMode>();

        auto gizmo_space_info = TypeMetaInfo
        {
            .id = "eeng.editor.TransformGizmoSpace",
            .name = "TransformGizmoSpace",
            .tooltip = "Gizmo orientation space (local/world).",
            .underlying_type = entt::resolve<std::underlying_type_t<GizmoSpace>>()
        };
        entt::meta_factory<GizmoSpace>()
            .custom<TypeMetaInfo>(gizmo_space_info)
            .traits(MetaFlags::none)

            .data<GizmoSpace::Local>("Local"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Local", "Align gizmo to local axes." })
            .traits(MetaFlags::none)

            .data<GizmoSpace::World>("World"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "World", "Align gizmo to world axes." })
            .traits(MetaFlags::none)
            ;
        meta::register_type<GizmoSpace>();
        warm_start_meta_type<GizmoSpace>();

        entt::meta_factory<GizmoSettings>()
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.editor.TransformGizmoSettings", .name = "TransformGizmoSettings", .tooltip = "Gizmo visual and snapping settings." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::screen_size>("screen_size"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "screen_size", "Screen Size", "Target gizmo size in pixels." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::axis_length>("axis_length"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "axis_length", "Axis Length", "Base axis length in world units." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::axis_radius>("axis_radius"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "axis_radius", "Axis Radius", "Axis cylinder radius in world units." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::plane_size>("plane_size"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "plane_size", "Plane Size", "Plane handle size in world units." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::plane_offset>("plane_offset"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "plane_offset", "Plane Offset", "Plane handle offset from origin." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::rotate_radius>("rotate_radius"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "rotate_radius", "Rotate Radius", "Rotation ring radius." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::rotate_thickness>("rotate_thickness"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "rotate_thickness", "Rotate Thickness", "Rotation ring thickness for picking." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::scale_box_size>("scale_box_size"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "scale_box_size", "Scale Box Size", "Axis scale handle size." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::uniform_scale_size>("uniform_scale_size"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "uniform_scale_size", "Uniform Scale Size", "Uniform scale handle size." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::linear_snap>("linear_snap"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "linear_snap", "Linear Snap", "Translation snap increment." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::angular_snap_deg>("angular_snap_deg"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "angular_snap_deg", "Angular Snap (deg)", "Rotation snap increment in degrees." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::scale_snap>("scale_snap"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "scale_snap", "Scale Snap", "Scale snap increment." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::min_scale>("min_scale"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "min_scale", "Min Scale", "Clamp to avoid degenerate scales." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::allow_uniform_scale>("allow_uniform_scale"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "allow_uniform_scale", "Allow Uniform Scale", "Enable center uniform scale handle." })
            .traits(MetaFlags::none)

            .data<&GizmoSettings::draw_on_top>("draw_on_top"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "draw_on_top", "Draw On Top", "Disable depth test for gizmo rendering." })
            .traits(MetaFlags::none)
            ;
        meta::register_type<GizmoSettings>();
        warm_start_meta_type<GizmoSettings>();

        entt::meta_factory<GizmoComponent>()
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.editor.TransformGizmoComponent", .name = "TransformGizmoComponent", .tooltip = "Editor gizmo settings and runtime state." })
            .traits(MetaFlags::none)

            .data<&GizmoComponent::enabled>("enabled"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable or disable the gizmo." })
            .traits(MetaFlags::none)

            .data<&GizmoComponent::mode>("mode"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "mode", "Mode", "Translate/rotate/scale mode." })
            .traits(MetaFlags::none)

            .data<&GizmoComponent::space>("space"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "space", "Space", "Local or world alignment." })
            .traits(MetaFlags::none)

            .data<&GizmoComponent::settings>("settings"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "settings", "Settings", "Gizmo rendering and snapping settings." })
            .traits(MetaFlags::none)
            ;
        register_component<GizmoComponent>();


        // --- StickyNoteComponent -------------------------------------------

        entt::meta_factory<eeng::ecs::StickyNoteComponent>{}
        .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.StickyNoteComponent", .name = "StickyNoteComponent", .tooltip = "World-space debug notes." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::StickyNoteComponent::max_age>("max_age"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "max_age", "Max Age", "Seconds before a line expires. <= 0 disables aging." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::StickyNoteComponent::enabled>("enabled"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Whether the sticky note is active." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::StickyNoteComponent::world_offset>("world_offset"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "world_offset", "World Offset", "Offset applied to the world position." })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::StickyNoteComponent::color_bg>("color_bg"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "color_bg", "Background Color", "ImGui background color (ABGR).", InspectorUiHint::ColorABGR })
            .traits(MetaFlags::none)

            .data<&eeng::ecs::StickyNoteComponent::color_text>("color_text"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "color_text", "Text Color", "ImGui text color (ABGR).", InspectorUiHint::ColorABGR })
            .traits(MetaFlags::none)

            // Line struct not registered
            // .data<&eeng::ecs::StickyNoteComponent::lines>("lines"_hs)
            // .custom<DataMetaInfo>(DataMetaInfo{ "lines", "Lines", "Lines of text in the sticky note." })
            // .traits(MetaFlags::none)

            ;
        register_component<ecs::StickyNoteComponent>();

        // --- Trail types/components ----------------------------------------
        {
            using TrailFadeMode = eeng::ecs::TrailFadeMode;
            auto trail_fade_mode_info = TypeMetaInfo
            {
                .id = "eeng.ecs.TrailFadeMode",
                .name = "TrailFadeMode",
                .tooltip = "Trail alpha fade mode.",
                .underlying_type = entt::resolve<std::underlying_type_t<TrailFadeMode>>()
            };
            entt::meta_factory<TrailFadeMode>()
                .custom<TypeMetaInfo>(trail_fade_mode_info)
                .traits(MetaFlags::none)
                .data<TrailFadeMode::None>("None"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "None", "Do not age-fade trail alpha." })
                .traits(MetaFlags::none)
                .data<TrailFadeMode::Linear>("Linear"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Linear", "Linearly fade alpha over trail lifetime." })
                .traits(MetaFlags::none)
                ;
            meta::register_type<TrailFadeMode>();
            warm_start_meta_type<TrailFadeMode>();

            entt::meta_factory<eeng::ecs::TrailLineStyle>{}
                .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.TrailLineStyle", .name = "TrailLineStyle", .tooltip = "Thick line style settings." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TrailLineStyle::thickness>("thickness"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "thickness", "Thickness", "Line thickness in pixels." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TrailLineStyle::dash_period_px>("dash_period_px"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "dash_period_px", "Dash Period", "Dash period in pixels. <= 0 disables dashes." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TrailLineStyle::dash_ratio>("dash_ratio"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "dash_ratio", "Dash Ratio", "Fraction of dash period that is visible [0..1]." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TrailLineStyle::dash_offset_px>("dash_offset_px"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "dash_offset_px", "Dash Offset", "Dash offset in pixels." })
                .traits(MetaFlags::none)
                ;
            meta::register_type<eeng::ecs::TrailLineStyle>();
            warm_start_meta_type<eeng::ecs::TrailLineStyle>();

            using Trail = eeng::ecs::TrailComponent::Trail;
            entt::meta_factory<Trail>{}
                .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.TrailComponent.Trail", .name = "Trail", .tooltip = "Config for one trail slot." })
                .traits(MetaFlags::none)
                .data<&Trail::active>("active"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "active", "Active", "Enable this trail slot." })
                .traits(MetaFlags::none)
                .data<&Trail::emitting>("emitting"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "emitting", "Emitting", "Emit new trail points." })
                .traits(MetaFlags::none)
                .data<&Trail::lifetime>("lifetime"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "lifetime", "Lifetime", "Point lifetime in seconds. <= 0 disables age culling." })
                .traits(MetaFlags::none)
                .data<&Trail::min_emit_distance>("min_emit_distance"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "min_emit_distance", "Emit Threshold", "Minimum movement before adding a point." })
                .traits(MetaFlags::none)
                .data<&Trail::color>("color"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "color", "Color", "Base trail color (ABGR).", InspectorUiHint::ColorABGR })
                .traits(MetaFlags::none)
                .data<&Trail::use_color_over_age>("use_color_over_age"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "use_color_over_age", "Color Over Age", "Interpolate from start color to end color over lifetime." })
                .traits(MetaFlags::none)
                .data<&Trail::color_start>("color_start"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "color_start", "Start Color", "Color at age 0 (ABGR).", InspectorUiHint::ColorABGR })
                .traits(MetaFlags::none)
                .data<&Trail::color_end>("color_end"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "color_end", "End Color", "Color at end of lifetime (ABGR).", InspectorUiHint::ColorABGR })
                .traits(MetaFlags::none)
                .data<&Trail::style>("style"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "style", "Style", "Line style settings." })
                .traits(MetaFlags::none)
                .data<&Trail::local_offset>("local_offset"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_offset", "Local Offset", "Emitter offset in local transform space." })
                .traits(MetaFlags::none)
                .data<&Trail::fade_mode>("fade_mode"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "fade_mode", "Fade Mode", "Trail alpha fading behavior." })
                .traits(MetaFlags::none)
                .data<&Trail::clear_on_teleport>("clear_on_teleport"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "clear_on_teleport", "Clear On Teleport", "Clear trail when movement exceeds teleport threshold." })
                .traits(MetaFlags::none)
                .data<&Trail::clear_teleport_distance>("clear_teleport_distance"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "clear_teleport_distance", "Teleport Distance", "Distance threshold for teleport clear." })
                .traits(MetaFlags::none)
                ;
            meta::register_type<Trail>();
            warm_start_meta_type<Trail>();
        }

        entt::meta_factory<eeng::ecs::TrailComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.TrailComponent", .name = "TrailComponent", .tooltip = "Motion trails rendered as overlay lines." })
            .traits(MetaFlags::none)
            .data<&eeng::ecs::TrailComponent::active_trail_count>("active_trail_count"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "active_trail_count", "Active Trail Count", "Number of trail slots to process (up to 8)." })
            .traits(MetaFlags::none)
            .data<&eeng::ecs::TrailComponent::trails>("trails"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "trails", "Trails", "Trail slot settings." })
            .traits(MetaFlags::none)
            ;
        register_component<ecs::TrailComponent>();


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

                // Blend clip index
                .data<&eeng::ecs::ModelComponent::blend_clip_index>("blend_clip_index"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "blend_clip_index", "Blend Clip Index", "Blend clip index." })
                .traits(MetaFlags::none)

                // Anim clip time
                // .data<&eeng::ecs::ModelComponent::clip_time>("clip_time"_hs)
                // .custom<DataMetaInfo>(DataMetaInfo{ "clip_time", "Clip Time", "Clip Time." })
                // .traits(MetaFlags::none)

                // Anim clip speed
                .data<&eeng::ecs::ModelComponent::clip_speed>("clip_speed"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "clip_speed", "Clip Speed", "Clip Speed." })
                .traits(MetaFlags::none)

                // Blend clip speed
                .data<&eeng::ecs::ModelComponent::blend_clip_speed>("blend_clip_speed"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "blend_clip_speed", "Blend Clip Speed", "Blend clip speed." })
                .traits(MetaFlags::none)

                // Anim clip loop
                .data<&eeng::ecs::ModelComponent::loop>("loop"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "loop", "Loop", "Loop clip." })
                .traits(MetaFlags::none)

                // Blend clip loop
                .data<&eeng::ecs::ModelComponent::blend_loop>("blend_loop"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "blend_loop", "Blend Loop", "Loop blend clip." })
                .traits(MetaFlags::none)

                // Blend factor
                .data<&eeng::ecs::ModelComponent::blend_factor>("blend_factor"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "blend_factor", "Blend Factor", "Blend factor between clips." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::ModelComponent>();
        }

        // --- Script component ----------------------------------------------
        {
            entt::meta_factory<eeng::ecs::ScriptComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.ScriptComponent", .name = "ScriptComponent", .tooltip = "Script binding placeholder." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ScriptComponent::script_id>("script_id"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "script_id", "Script Id", "Script identifier or path." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ScriptComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable script execution." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::ScriptComponent>();
        }

        // --- Physics enums --------------------------------------------------
        {
            using PhysicsMotionType = eeng::ecs::PhysicsMotionType;
            using ColliderType = eeng::ecs::ColliderType;
            using ContactPhase = eeng::ecs::ContactPhase;
            using SpringAnchorSpace = eeng::ecs::SpringAnchorSpace;

            auto motion_type_info = TypeMetaInfo
            {
                .id = "eeng.ecs.PhysicsMotionType",
                .name = "PhysicsMotionType",
                .tooltip = "Rigid body motion type (static/dynamic/kinematic).",
                .underlying_type = entt::resolve<std::underlying_type_t<PhysicsMotionType>>()
            };
            entt::meta_factory<PhysicsMotionType>()
                .custom<TypeMetaInfo>(motion_type_info)
                .traits(MetaFlags::none)
                .data<PhysicsMotionType::Static>("Static"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Static", "Static body." })
                .traits(MetaFlags::none)
                .data<PhysicsMotionType::Dynamic>("Dynamic"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Dynamic", "Dynamic body." })
                .traits(MetaFlags::none)
                .data<PhysicsMotionType::Kinematic>("Kinematic"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Kinematic", "Kinematic body." })
                .traits(MetaFlags::none)
                ;
            meta::register_type<PhysicsMotionType>();
            warm_start_meta_type<PhysicsMotionType>();

            auto collider_type_info = TypeMetaInfo
            {
                .id = "eeng.ecs.ColliderType",
                .name = "ColliderType",
                .tooltip = "Collider shape type.",
                .underlying_type = entt::resolve<std::underlying_type_t<ColliderType>>()
            };
            entt::meta_factory<ColliderType>()
                .custom<TypeMetaInfo>(collider_type_info)
                .traits(MetaFlags::none)
                .data<ColliderType::Box>("Box"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Box", "Box collider." })
                .traits(MetaFlags::none)
                .data<ColliderType::Sphere>("Sphere"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Sphere", "Sphere collider." })
                .traits(MetaFlags::none)
                .data<ColliderType::Capsule>("Capsule"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Capsule", "Capsule collider." })
                .traits(MetaFlags::none)
                .data<ColliderType::ConvexHull>("ConvexHull"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Convex Hull", "Convex hull collider." })
                .traits(MetaFlags::none)
                .data<ColliderType::TriangleMesh>("TriangleMesh"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Triangle Mesh", "Triangle mesh collider." })
                .traits(MetaFlags::none)
                .data<ColliderType::AABB>("AABB"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "AABB", "Axis-aligned bounding box collider." })
                .traits(MetaFlags::none)
                ;
            meta::register_type<ColliderType>();
            warm_start_meta_type<ColliderType>();

            auto contact_phase_info = TypeMetaInfo
            {
                .id = "eeng.ecs.ContactPhase",
                .name = "ContactPhase",
                .tooltip = "Collision phase (enter/stay/exit).",
                .underlying_type = entt::resolve<std::underlying_type_t<ContactPhase>>()
            };
            entt::meta_factory<ContactPhase>()
                .custom<TypeMetaInfo>(contact_phase_info)
                .traits(MetaFlags::none)
                .data<ContactPhase::Enter>("Enter"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Enter", "Contact entered this frame." })
                .traits(MetaFlags::none)
                .data<ContactPhase::Stay>("Stay"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Stay", "Contact continues this frame." })
                .traits(MetaFlags::none)
                .data<ContactPhase::Exit>("Exit"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Exit", "Contact exited this frame." })
                .traits(MetaFlags::none)
                ;
            meta::register_type<ContactPhase>();
            warm_start_meta_type<ContactPhase>();

            auto anchor_space_info = TypeMetaInfo
            {
                .id = "eeng.ecs.SpringAnchorSpace",
                .name = "SpringAnchorSpace",
                .tooltip = "Anchor space (Transform vs Body COM).",
                .underlying_type = entt::resolve<std::underlying_type_t<SpringAnchorSpace>>()
            };
            entt::meta_factory<SpringAnchorSpace>()
                .custom<TypeMetaInfo>(anchor_space_info)
                .traits(MetaFlags::none)
                .data<SpringAnchorSpace::Transform>("Transform"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Transform", "Anchor in authoring transform space." })
                .traits(MetaFlags::none)
                .data<SpringAnchorSpace::Body>("Body"_hs)
                .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Body", "Anchor in body COM/principal-axes space." })
                .traits(MetaFlags::none)
                ;
            meta::register_type<SpringAnchorSpace>();
            warm_start_meta_type<SpringAnchorSpace>();

        }

        // --- Physics helper types ------------------------------------------
        {
            entt::meta_factory<eeng::ecs::PhysicsMaterial>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.PhysicsMaterial", .name = "PhysicsMaterial", .tooltip = "Physics material settings." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PhysicsMaterial::friction>("friction"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "friction", "Friction", "Surface friction." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PhysicsMaterial::restitution>("restitution"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "restitution", "Restitution", "Bounciness." })
                .traits(MetaFlags::none)
                ;
            meta::register_type<eeng::ecs::PhysicsMaterial>();
            warm_start_meta_type<eeng::ecs::PhysicsMaterial>();

            entt::meta_factory<eeng::ecs::CollisionFilter>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.CollisionFilter", .name = "CollisionFilter", .tooltip = "Collision filtering (layer/mask)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::CollisionFilter::layer>("layer"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "layer", "Layer", "Collision layer." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::CollisionFilter::mask>("mask"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "mask", "Mask", "Collision mask." })
                .traits(MetaFlags::none)
                ;
            meta::register_type<eeng::ecs::CollisionFilter>();
            warm_start_meta_type<eeng::ecs::CollisionFilter>();

            entt::meta_factory<eeng::ecs::ColliderDesc>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.ColliderDesc", .name = "ColliderDesc", .tooltip = "Collider descriptor." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ColliderDesc::id>("id"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "id", "Id", "Collider identifier." })
                .traits(MetaFlags::readonly_inspection)
                .data<&eeng::ecs::ColliderDesc::type>("type"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "type", "Type", "Collider type." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ColliderDesc::local_position>("local_position"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_position", "Local Position", "Local collider offset." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ColliderDesc::local_rotation>("local_rotation"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_rotation", "Local Rotation", "Local collider rotation." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ColliderDesc::half_extents>("half_extents"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "half_extents", "Half Extents", "Half extents for box/AABB." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ColliderDesc::radius>("radius"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "radius", "Radius", "Radius for sphere/capsule." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ColliderDesc::height>("height"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "height", "Height", "Height for capsule." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ColliderDesc::mesh_ref>("mesh_ref"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "mesh_ref", "Mesh Ref", "Mesh source for mesh colliders." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ColliderDesc::submesh_index>("submesh_index"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "submesh_index", "Submesh Index", "Submesh index for mesh colliders." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ColliderDesc::is_trigger>("is_trigger"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "is_trigger", "Is Trigger", "Trigger-only collider." })
                .traits(MetaFlags::none)
                .func<&eeng::editor::inspect_ColliderDesc>(eeng::literals::inspect_hs)
                .template custom<FuncMetaInfo>(FuncMetaInfo{ "inspect_ColliderDesc", "Inspect collider descriptor" })
                ;
            meta::register_type<eeng::ecs::ColliderDesc>();
            warm_start_meta_type<eeng::ecs::ColliderDesc>();
        }

        // --- Physics components --------------------------------------------
        {
            entt::meta_factory<eeng::ecs::RigidBodyComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.RigidBodyComponent", .name = "RigidBodyComponent", .tooltip = "Rigid body settings." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::motion>("motion"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "motion", "Motion", "Motion type." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::auto_mass>("auto_mass"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "auto_mass", "Auto Mass", "Compute mass from colliders." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::mass>("mass"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "mass", "Mass", "Mass (computed when Auto Mass is enabled)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::density>("density"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "density", "Density", "Density used for auto-mass (kg/m^3)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::auto_inertia>("auto_inertia"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "auto_inertia", "Auto Inertia", "Compute inertia from colliders." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::inertia>("inertia"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "inertia", "Inertia", "Inertia override." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::linear_damping>("linear_damping"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_damping", "Linear Damping", "Linear damping." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::angular_damping>("angular_damping"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_damping", "Angular Damping", "Angular damping." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::gravity_scale>("gravity_scale"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "gravity_scale", "Gravity Scale", "Gravity multiplier." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::allow_sleep>("allow_sleep"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "allow_sleep", "Allow Sleep", "Allow Bullet to sleep body." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::enable_ccd>("enable_ccd"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enable_ccd", "Enable CCD", "Enable continuous collision detection." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::ccd_swept_sphere_radius>("ccd_swept_sphere_radius"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "ccd_swept_sphere_radius", "CCD Radius", "CCD swept sphere radius." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::RigidBodyComponent::ccd_motion_threshold>("ccd_motion_threshold"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "ccd_motion_threshold", "CCD Threshold", "CCD motion threshold." })
                .traits(MetaFlags::none)
                .func<&eeng::editor::inspect_RigidBodyComponent>(eeng::literals::inspect_hs)
                .template custom<FuncMetaInfo>(FuncMetaInfo{ "inspect_RigidBodyComponent", "Inspect rigid body component" })

                ;
            register_component<ecs::RigidBodyComponent>();

            entt::meta_factory<eeng::ecs::ColliderComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.ColliderComponent", .name = "ColliderComponent", .tooltip = "Collider list." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::ColliderComponent::colliders>("colliders"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "colliders", "Colliders", "Collider list." })
                .traits(MetaFlags::none)
                .func<&eeng::editor::inspect_ColliderComponent>(eeng::literals::inspect_hs)
                .template custom<FuncMetaInfo>(FuncMetaInfo{ "inspect_ColliderComponent", "Inspect collider list." })
                
                ;
            register_component<ecs::ColliderComponent>();

            entt::meta_factory<eeng::ecs::PhysicsMaterialComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.PhysicsMaterialComponent", .name = "PhysicsMaterialComponent", .tooltip = "Physics material component." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PhysicsMaterialComponent::material>("material"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "material", "Material", "Physics material." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::PhysicsMaterialComponent>();

            entt::meta_factory<eeng::ecs::CollisionFilterComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.CollisionFilterComponent", .name = "CollisionFilterComponent", .tooltip = "Collision filter component." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::CollisionFilterComponent::filter>("filter"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "filter", "Filter", "Collision filter." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::CollisionFilterComponent>();

            entt::meta_factory<eeng::ecs::PhysicsEventsComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.PhysicsEventsComponent", .name = "PhysicsEventsComponent", .tooltip = "Physics event toggles." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PhysicsEventsComponent::emit_collisions>("emit_collisions"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "emit_collisions", "Emit Collisions", "Emit collision events." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PhysicsEventsComponent::emit_triggers>("emit_triggers"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "emit_triggers", "Emit Triggers", "Emit trigger events." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::PhysicsEventsComponent>();

            entt::meta_factory<eeng::ecs::SpringDamperComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.SpringDamperComponent", .name = "SpringDamperComponent", .tooltip = "Spring-damper force component." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::entity_a>("entity_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "entity_a", "Entity A", "Anchor A body." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::entity_b>("entity_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "entity_b", "Entity B", "Anchor B body." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::local_anchor_a>("local_anchor_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_anchor_a", "Local Anchor A", "Anchor in body A local space." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::local_anchor_b>("local_anchor_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_anchor_b", "Local Anchor B", "Anchor in body B local space." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::anchor_space_a>("anchor_space_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "anchor_space_a", "Anchor Space A", "Local anchor space for body A." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::anchor_space_b>("anchor_space_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "anchor_space_b", "Anchor Space B", "Local anchor space for body B." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::linear_stiffness>("linear_stiffness"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_stiffness", "Linear Stiffness", "Linear spring stiffness." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::linear_damping>("linear_damping"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_damping", "Linear Damping", "Linear damping constant." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::rest_length>("rest_length"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "rest_length", "Rest Length", "Resting spring length." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::enable_angular>("enable_angular"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enable_angular", "Enable Angular", "Enable angular spring torque." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::angular_stiffness>("angular_stiffness"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_stiffness", "Angular Stiffness", "Angular spring stiffness." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::angular_damping>("angular_damping"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_damping", "Angular Damping", "Angular damping constant." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::rest_rotation>("rest_rotation"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "rest_rotation", "Rest Rotation", "Rest rotation from A to B; when only one anchor has a body, this is the target world rotation for that body." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SpringDamperComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable spring-damper." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::SpringDamperComponent>();

            entt::meta_factory<eeng::ecs::PointConstraintComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.PointConstraintComponent", .name = "PointConstraintComponent", .tooltip = "Point-to-point constraint." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PointConstraintComponent::entity_a>("entity_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "entity_a", "Entity A", "First rigid body entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PointConstraintComponent::entity_b>("entity_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "entity_b", "Entity B", "Second rigid body entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PointConstraintComponent::local_anchor_a>("local_anchor_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_anchor_a", "Local Anchor A", "Anchor in body A local space (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PointConstraintComponent::local_anchor_b>("local_anchor_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_anchor_b", "Local Anchor B", "Anchor in body B local space (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PointConstraintComponent::disable_collisions>("disable_collisions"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "disable_collisions", "Disable Collisions", "Disable collisions between the two bodies." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PointConstraintComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable constraint." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::PointConstraintComponent>();

            entt::meta_factory<eeng::ecs::HingeConstraintComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.HingeConstraintComponent", .name = "HingeConstraintComponent", .tooltip = "Hinge constraint." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::entity_a>("entity_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "entity_a", "Entity A", "First rigid body entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::entity_b>("entity_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "entity_b", "Entity B", "Second rigid body entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::local_anchor_a>("local_anchor_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_anchor_a", "Local Anchor A", "Anchor in body A local space (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::local_anchor_b>("local_anchor_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_anchor_b", "Local Anchor B", "Anchor in body B local space (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::local_axis_a>("local_axis_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_axis_a", "Local Axis A", "Hinge axis in body A local space (normalized)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::local_axis_b>("local_axis_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_axis_b", "Local Axis B", "Hinge axis in body B local space (normalized)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::use_limits>("use_limits"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "use_limits", "Use Limits", "Enable angular limits around the hinge axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::limit_min>("limit_min"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "limit_min", "Limit Min", "Lower hinge angle limit (radians)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::limit_max>("limit_max"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "limit_max", "Limit Max", "Upper hinge angle limit (radians)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::enable_motor>("enable_motor"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enable_motor", "Enable Motor", "Enable hinge motor (angular velocity)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::motor_target_velocity>("motor_target_velocity"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "motor_target_velocity", "Motor Target Velocity", "Target angular velocity (rad/s)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::motor_max_impulse>("motor_max_impulse"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "motor_max_impulse", "Motor Max Impulse", "Maximum motor impulse (limits motor torque)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::disable_collisions>("disable_collisions"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "disable_collisions", "Disable Collisions", "Disable collisions between the two bodies." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::HingeConstraintComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable constraint." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::HingeConstraintComponent>();

            entt::meta_factory<eeng::ecs::SliderConstraintComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.SliderConstraintComponent", .name = "SliderConstraintComponent", .tooltip = "Slider constraint." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::entity_a>("entity_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "entity_a", "Entity A", "First rigid body entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::entity_b>("entity_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "entity_b", "Entity B", "Second rigid body entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::local_anchor_a>("local_anchor_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_anchor_a", "Local Anchor A", "Anchor in body A local space (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::local_anchor_b>("local_anchor_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_anchor_b", "Local Anchor B", "Anchor in body B local space (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::local_axis_a>("local_axis_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_axis_a", "Local Axis A", "Slider axis in body A local space (normalized)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::local_axis_b>("local_axis_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_axis_b", "Local Axis B", "Slider axis in body B local space (normalized)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::linear_limit_min>("linear_limit_min"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_limit_min", "Linear Limit Min", "Lower linear limit along the slider axis (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::linear_limit_max>("linear_limit_max"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_limit_max", "Linear Limit Max", "Upper linear limit along the slider axis (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::angular_limit_min>("angular_limit_min"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_limit_min", "Angular Limit Min", "Lower angular limit around the slider axis (radians)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::angular_limit_max>("angular_limit_max"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_limit_max", "Angular Limit Max", "Upper angular limit around the slider axis (radians)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::enable_linear_motor>("enable_linear_motor"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enable_linear_motor", "Enable Motor", "Enable linear motor along the slider axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::linear_motor_target_velocity>("linear_motor_target_velocity"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_motor_target_velocity", "Motor Target Velocity", "Target linear velocity along the axis (units/s)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::linear_motor_max_force>("linear_motor_max_force"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_motor_max_force", "Motor Max Force", "Maximum motor force along the axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::disable_collisions>("disable_collisions"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "disable_collisions", "Disable Collisions", "Disable collisions between the two bodies." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SliderConstraintComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable constraint." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::SliderConstraintComponent>();

            entt::meta_factory<eeng::ecs::SixDofSpringConstraintComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.SixDofSpringConstraintComponent", .name = "SixDofSpringConstraintComponent", .tooltip = "6DoF spring constraint." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::entity_a>("entity_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "entity_a", "Entity A", "First rigid body entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::entity_b>("entity_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "entity_b", "Entity B", "Second rigid body entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::local_anchor_a>("local_anchor_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_anchor_a", "Local Anchor A", "Frame origin in body A local space (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::local_rotation_a>("local_rotation_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_rotation_a", "Local Rotation A", "Frame rotation in body A local space." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::local_anchor_b>("local_anchor_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_anchor_b", "Local Anchor B", "Frame origin in body B local space (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::local_rotation_b>("local_rotation_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_rotation_b", "Local Rotation B", "Frame rotation in body B local space." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_limit_min>("linear_limit_min"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_limit_min", "Linear Limit Min", "Lower linear limits in the local constraint frame (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_limit_max>("linear_limit_max"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_limit_max", "Linear Limit Max", "Upper linear limits in the local constraint frame (authoring units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::angular_limit_min>("angular_limit_min"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_limit_min", "Angular Limit Min", "Lower angular limits in the local constraint frame (radians)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::angular_limit_max>("angular_limit_max"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_limit_max", "Angular Limit Max", "Upper angular limits in the local constraint frame (radians)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_stiffness>("linear_stiffness"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_stiffness", "Linear Stiffness", "Linear spring stiffness per axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_damping>("linear_damping"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_damping", "Linear Damping", "Linear damping per axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::angular_stiffness>("angular_stiffness"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_stiffness", "Angular Stiffness", "Angular spring stiffness per axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::angular_damping>("angular_damping"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_damping", "Angular Damping", "Angular damping per axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_equilibrium_enabled>("linear_equilibrium_enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_equilibrium_enabled", "Linear Equilibrium Enabled", "Enable linear equilibrium override per axis (0/1)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_equilibrium_target>("linear_equilibrium_target"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_equilibrium_target", "Linear Equilibrium Target", "Linear equilibrium target per axis (units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_motor_enabled>("linear_motor_enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_motor_enabled", "Linear Motor Enabled", "Enable linear motors per axis (0/1)."})
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_motor_target_velocity>("linear_motor_target_velocity"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_motor_target_velocity", "Linear Motor Velocity", "Target linear motor velocity per axis (units/s)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_motor_max_force>("linear_motor_max_force"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_motor_max_force", "Linear Motor Max Force", "Max linear motor force per axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_servo_enabled>("linear_servo_enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_servo_enabled", "Linear Servo Enabled", "Enable linear servos per axis (0/1)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::linear_servo_target>("linear_servo_target"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "linear_servo_target", "Linear Servo Target", "Linear servo target per axis (units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::angular_motor_enabled>("angular_motor_enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_motor_enabled", "Angular Motor Enabled", "Enable angular motors per axis (0/1)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::angular_motor_target_velocity>("angular_motor_target_velocity"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_motor_target_velocity", "Angular Motor Velocity", "Target angular motor velocity per axis (rad/s)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::angular_motor_max_force>("angular_motor_max_force"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_motor_max_force", "Angular Motor Max Force", "Max angular motor force per axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::angular_servo_enabled>("angular_servo_enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_servo_enabled", "Angular Servo Enabled", "Enable angular servos per axis (0/1)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::angular_servo_target>("angular_servo_target"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "angular_servo_target", "Angular Servo Target", "Angular servo target per axis (radians)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::disable_collisions>("disable_collisions"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "disable_collisions", "Disable Collisions", "Disable collisions between the two bodies." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::SixDofSpringConstraintComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable constraint." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::SixDofSpringConstraintComponent>();

            entt::meta_factory<eeng::ecs::PistonConstraintDriveComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.PistonConstraintDriveComponent", .name = "PistonConstraintDriveComponent", .tooltip = "Drive a linear constraint like a hydraulic piston." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::name>("name"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Component name." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable piston constraint drive." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::constraint>("constraint"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "constraint", "Constraint", "6DoF constraint entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::anchor_a>("anchor_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "anchor_a", "Anchor A", "Anchor entity used to drive the constraint frame." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::anchor_b>("anchor_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "anchor_b", "Anchor B", "Anchor entity used to drive the constraint frame." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::axis_local>("axis_local"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "axis_local", "Axis Local", "Local axis for extension measurement." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::stroke_min>("stroke_min"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "stroke_min", "Stroke Min", "Minimum extension (units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::stroke_max>("stroke_max"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "stroke_max", "Stroke Max", "Maximum extension (units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::max_force>("max_force"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "max_force", "Max Force", "Max motor force." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::max_velocity>("max_velocity"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "max_velocity", "Max Velocity", "Max extension speed (units/s)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::mode>("mode"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "mode", "Mode", "0=Hold, 1=Extend, 2=Contract, 3=Position." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::target_extension>("target_extension"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "target_extension", "Target Extension", "Target extension (0..1) in Position mode." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::lock_when_idle>("lock_when_idle"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "lock_when_idle", "Lock When Idle", "Lock piston position when holding." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonConstraintDriveComponent::current_position>("current_position"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "current_position", "Current Position", "Measured extension along axis (units)." })
                .traits(MetaFlags::readonly_inspection)
                .data<&eeng::ecs::PistonConstraintDriveComponent::current_extension>("current_extension"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "current_extension", "Current Extension", "Measured extension normalized (0..1)." })
                .traits(MetaFlags::readonly_inspection)
                .data<&eeng::ecs::PistonConstraintDriveComponent::command_position>("command_position"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "command_position", "Command Position", "Rate-limited target position (units)." })
                .traits(MetaFlags::readonly_inspection)
                ;
            register_component<ecs::PistonConstraintDriveComponent>();

            entt::meta_factory<eeng::ecs::PistonAnimSyncComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.PistonAnimSyncComponent", .name = "PistonAnimSyncComponent", .tooltip = "Drive animation pose time from piston extension." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonAnimSyncComponent::name>("name"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Component name." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonAnimSyncComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable piston animation sync." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonAnimSyncComponent::target>("target"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "target", "Target", "Entity with AnimationGraphComponent + ModelComponent." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonAnimSyncComponent::clip_name>("clip_name"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "clip_name", "Clip Name", "Clip to scrub. Leave empty to use the graph's clip." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::PistonAnimSyncComponent::auto_find_target>("auto_find_target"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "auto_find_target", "Auto Find Target", "Search descendants for an AnimationGraphComponent when target is unbound." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::PistonAnimSyncComponent>();

            entt::meta_factory<eeng::ecs::TransformSocketComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.TransformSocketComponent", .name = "TransformSocketComponent", .tooltip = "Socket a transform to a target entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::name>("name"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Component name." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable socket updates." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::target>("target"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "target", "Target", "Target entity to follow." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::local_offset>("local_offset"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_offset", "Local Offset", "Offset in target local space." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::local_rotation>("local_rotation"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_rotation", "Local Rotation", "Rotation in target local space." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::follow_position>("follow_position"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "follow_position", "Follow Position", "Update position from target." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::follow_rotation>("follow_rotation"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "follow_rotation", "Follow Rotation", "Update rotation from target." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::preserve_scale>("preserve_scale"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "preserve_scale", "Preserve Scale", "Keep current scale." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::update_in_edit>("update_in_edit"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "update_in_edit", "Update In Edit", "Update sockets in edit mode." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::capture_offset_in_edit>("capture_offset_in_edit"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "capture_offset_in_edit", "Capture Offset In Edit", "Update offsets when edited with gizmo." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TransformSocketComponent::last_local_version>("last_local_version"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "last_local_version", "Last Local Version", "Runtime local version cache." })
                .traits(MetaFlags::readonly_inspection)
                ;
            register_component<ecs::TransformSocketComponent>();

            entt::meta_factory<eeng::ecs::TwoAnchorAlignComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.TwoAnchorAlignComponent", .name = "TwoAnchorAlignComponent", .tooltip = "Align a target transform between two anchors." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::name>("name"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Component name." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable alignment." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::anchor_a>("anchor_a"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "anchor_a", "Anchor A", "First anchor entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::anchor_b>("anchor_b"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "anchor_b", "Anchor B", "Second anchor entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::target>("target"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "target", "Target", "Target entity (defaults to owner)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::up_reference>("up_reference"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "up_reference", "Up Reference", "Reference entity for the up vector." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::up_axis_ref>("up_axis_ref"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "up_axis_ref", "Up Axis Ref", "Local up axis on the reference entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::local_forward_axis>("local_forward_axis"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_forward_axis", "Local Forward", "Target-local forward axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::local_up_axis>("local_up_axis"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "local_up_axis", "Local Up", "Target-local up axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::position_mode>("position_mode"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "position_mode", "Position Mode", "0=Anchor A, 1=Anchor B, 2=Midpoint." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::position_offset>("position_offset"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "position_offset", "Position Offset", "Offset along forward axis (units)." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::align_position>("align_position"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "align_position", "Align Position", "Update position from anchors." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::align_rotation>("align_rotation"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "align_rotation", "Align Rotation", "Update rotation from anchors." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::preserve_scale>("preserve_scale"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "preserve_scale", "Preserve Scale", "Keep current scale when aligning." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::TwoAnchorAlignComponent::current_length>("current_length"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "current_length", "Current Length", "Measured anchor distance." })
                .traits(MetaFlags::readonly_inspection)
                ;
            register_component<ecs::TwoAnchorAlignComponent>();

            entt::meta_factory<eeng::ecs::VehicleRig1WheelLink>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.VehicleRig1WheelLink", .name = "VehicleRig1WheelLink", .tooltip = "VehicleRig1 wheel linkage state." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::knuckle>("knuckle"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "knuckle", "Knuckle", "Knuckle entity reference." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::wheel>("wheel"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "wheel", "Wheel", "Wheel entity reference." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::suspension_6dof>("suspension_6dof"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "suspension_6dof", "Suspension 6DoF", "6DoF suspension constraint entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::axle_hinge>("axle_hinge"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "axle_hinge", "Axle Hinge", "Axle hinge constraint entity." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::steerable>("steerable"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "steerable", "Steerable", "Steerable wheel flag." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::driven>("driven"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "driven", "Driven", "Driven wheel flag." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::drive_direction>("drive_direction"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "drive_direction", "Drive Direction", "Sign to flip drive motor direction." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::steer_direction>("steer_direction"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "steer_direction", "Steer Direction", "Sign to flip steer motor direction." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::mount_local>("mount_local"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "mount_local", "Mount Local", "Chassis-local mount position." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::suspension_axis>("suspension_axis"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "suspension_axis", "Suspension Axis", "Chassis-local suspension axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::axle_axis>("axle_axis"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "axle_axis", "Axle Axis", "Local axle axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::wheel_local_anchor>("wheel_local_anchor"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "wheel_local_anchor", "Wheel Anchor", "Wheel-local anchor position." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::suspension_rest_length>("suspension_rest_length"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "suspension_rest_length", "Rest Length", "Suspension rest length." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1WheelLink::suspension_travel>("suspension_travel"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "suspension_travel", "Travel", "Suspension travel distance." })
                .traits(MetaFlags::none)
                ;
            meta::register_type<eeng::ecs::VehicleRig1WheelLink>();
            warm_start_meta_type<eeng::ecs::VehicleRig1WheelLink>();

            entt::meta_factory<eeng::ecs::VehicleRig1RigComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.VehicleRig1RigComponent", .name = "VehicleRig1RigComponent", .tooltip = "VehicleRig1 rig linkage component." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1RigComponent::chassis>("chassis"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "chassis", "Chassis", "Chassis entity reference." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1RigComponent::steer_axis>("steer_axis"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "steer_axis", "Steer Axis", "Chassis-local steering axis." })
                .traits(MetaFlags::none)
                .data<&eeng::ecs::VehicleRig1RigComponent::wheels>("wheels"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "wheels", "Wheels", "Wheel linkage array." })
                .traits(MetaFlags::none)
                ;
            register_component<ecs::VehicleRig1RigComponent>();

            entt::meta_factory<eeng::ecs::PhysicsRaycastDebugComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.PhysicsRaycastDebugComponent", .name = "PhysicsRaycastDebugComponent", .tooltip = "Debug-only raycast cache." })
                .traits(MetaFlags::no_inspection | MetaFlags::no_serialize)
                ;
            register_component<ecs::PhysicsRaycastDebugComponent>();
        }

        // --- AnimationGraphComponent ----------------------------------------
        {
            entt::meta_factory<eeng::ecs::AnimationGraphComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "eeng.ecs.AnimationGraphComponent", .name = "AnimationGraphComponent", .tooltip = "Animation graph runtime component." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::AnimationGraphComponent::name>("name"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Component name." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::AnimationGraphComponent::graph_ref>("graph_ref"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "graph_ref", "Graph Reference", "Animation graph asset reference." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::AnimationGraphComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable animation graph evaluation." })
                .traits(MetaFlags::none)

                .func<&eeng::editor::inspect_AnimationGraphComponent>(eeng::literals::inspect_hs)
                .template custom<FuncMetaInfo>(FuncMetaInfo{ "inspect", "Inspect animation graph component." })

                // Needed to sync runtime when AssetRef bindings are refreshed after batch rebuilds.
                .func<&eeng::ecs::AnimationGraphComponent::on_component_post_bind>(eeng::literals::post_bind_hs)
                .template custom<FuncMetaInfo>(FuncMetaInfo{ "post_bind", "Post-bind hook for graph initialization." })

                // Needed so inspector edits can reset/reinit runtime immediately after field changes.
                .func<&eeng::ecs::AnimationGraphComponent::on_component_post_assign>(eeng::literals::post_assign_hs)
                .template custom<FuncMetaInfo>(FuncMetaInfo{ "post_assign", "Post-assign hook for graph edits." })
                ;
            register_component<ecs::AnimationGraphComponent>();
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

            // With entt::as_cref_t to avoid copies on get()
            // Should be readonly_inspection
            .data<&MockMixComponent::copy_signaller, entt::as_cref_t>("copy_signaller"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "copy_signaller", "Copy Signaller", "A read-only CopySignaller instance." })
            .traits(MetaFlags::readonly_inspection)

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
