// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "LegacyGameMetaReg.hpp"

#include "meta/EntityMetaHelpers.hpp"
#include "meta/MetaAux.h"
#include "meta/MetaLiterals.h"
#include "MetaInfo.h"
#include "LogMacros.h"
#include "ecs/MannequinPlayerControllerComponent.hpp"
#include "ecs/VehicleRig1ControlComponent.hpp"

#include <entt/entt.hpp>
#include <type_traits>
#include <stdexcept>

namespace eeng::legacy_game
{
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

        template<typename T>
        void register_component()
        {
            static_assert(std::is_default_constructible_v<T>,
                "Component must be default constructible for editor add/remove.");

            entt::meta_factory<T>()
                .template func<&assure_type_storage<T>, entt::as_void_t>(eeng::literals::assure_component_storage_hs)
                .template func<&eeng::meta::collect_asset_guids<T>, entt::as_void_t>(eeng::literals::collect_asset_guids_hs)
                .template func<&eeng::meta::bind_asset_refs<T>, entt::as_void_t>(eeng::literals::bind_asset_refs_hs)
                .template func<&eeng::meta::bind_entity_refs<T>, entt::as_void_t>(eeng::literals::bind_entity_refs_hs);

            eeng::meta::register_type<T>();
            warm_start_meta_type<T>();
        }
    }

    void register_legacy_game_meta_types(EngineContext& ctx)
    {
        EENG_LOG_INFO(&ctx, "Registering legacy game component meta types...");

        // --- MannequinPlayerControllerComponent --------------------------
        {
            entt::meta_factory<eeng::ecs::MannequinPlayerControllerComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{
                .id = "eeng.legacy_game.MannequinPlayerControllerComponent",
                .name = "MannequinPlayerControllerComponent",
                .tooltip = "Mannequin-specific input-driven animation graph parameter control." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::name>("name"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Component name." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable input-driven animation control." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::controller_id>("controller_id"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "controller_id", "Controller Id", "Controller instance id (-1 = first connected)." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::use_keyboard_fallback>("use_keyboard_fallback"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "use_keyboard_fallback", "Keyboard Fallback", "Use keyboard/mouse when no controller is active." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::use_mouse_aim>("use_mouse_aim"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "use_mouse_aim", "Mouse Aim", "Drive aim parameters from mouse delta when using keyboard." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::stick_deadzone>("stick_deadzone"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "stick_deadzone", "Stick Deadzone", "Deadzone for controller sticks." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::trigger_deadzone>("trigger_deadzone"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "trigger_deadzone", "Trigger Deadzone", "Deadzone threshold for controller triggers." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::mouse_sensitivity>("mouse_sensitivity"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "mouse_sensitivity", "Mouse Sensitivity", "Sensitivity for mouse-driven aim parameters." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::move_x_param>("move_x_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "move_x_param", "Move X Param", "Graph parameter for locomotion X." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::move_y_param>("move_y_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "move_y_param", "Move Y Param", "Graph parameter for locomotion Y." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::aim_x_param>("aim_x_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "aim_x_param", "Aim X Param", "Graph parameter for aim X." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::aim_y_param>("aim_y_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "aim_y_param", "Aim Y Param", "Graph parameter for aim Y." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::jump_param>("jump_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "jump_param", "Jump Param", "Graph parameter for jump trigger." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::fire_param>("fire_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "fire_param", "Fire Param", "Graph parameter for fire trigger." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::reload_param>("reload_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "reload_param", "Reload Param", "Graph parameter for reload trigger." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::hit0_param>("hit0_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "hit0_param", "Hit0 Param", "Graph parameter for hit reaction 0." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::hit1_param>("hit1_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "hit1_param", "Hit1 Param", "Graph parameter for hit reaction 1." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::hit2_param>("hit2_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "hit2_param", "Hit2 Param", "Graph parameter for hit reaction 2." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::MannequinPlayerControllerComponent::hit3_param>("hit3_param"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "hit3_param", "Hit3 Param", "Graph parameter for hit reaction 3." })
                .traits(MetaFlags::none)
                ;
            register_component<eeng::ecs::MannequinPlayerControllerComponent>();
        }

        // --- VehicleRig1ControlComponent -------------------------------------
        {
            entt::meta_factory<eeng::ecs::VehicleRig1ControlComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{
                .id = "eeng.legacy_game.VehicleRig1ControlComponent",
                .name = "VehicleRig1ControlComponent",
                .tooltip = "Input-driven steering and drive control for VehicleRig1." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::name>("name"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Component name." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::enabled>("enabled"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable vehicle input control." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::controller_id>("controller_id"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "controller_id", "Controller Id", "Controller instance id (-1 = first connected)." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::use_keyboard_fallback>("use_keyboard_fallback"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "use_keyboard_fallback", "Keyboard Fallback", "Use keyboard when no controller is active." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::stick_deadzone>("stick_deadzone"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "stick_deadzone", "Stick Deadzone", "Deadzone for steering input." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::trigger_deadzone>("trigger_deadzone"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "trigger_deadzone", "Trigger Deadzone", "Deadzone for controller triggers." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::steer_limit>("steer_limit"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "steer_limit", "Steer Limit", "Max steering angle (radians)." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::steer_speed>("steer_speed"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "steer_speed", "Steer Speed", "Steering speed (rad/s)." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::steer_max_impulse>("steer_max_impulse"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "steer_max_impulse", "Steer Max Impulse", "Max motor impulse for steering hinge." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::drive_velocity>("drive_velocity"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "drive_velocity", "Drive Velocity", "Target wheel angular velocity." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::drive_max_impulse>("drive_max_impulse"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "drive_max_impulse", "Drive Max Impulse", "Max motor impulse for drive wheels." })
                .traits(MetaFlags::none)

                .data<&eeng::ecs::VehicleRig1ControlComponent::brake_max_impulse>("brake_max_impulse"_hs)
                .custom<DataMetaInfo>(DataMetaInfo{ "brake_max_impulse", "Brake Max Impulse", "Max motor impulse when braking/coasting." })
                .traits(MetaFlags::none)
                ;
            register_component<eeng::ecs::VehicleRig1ControlComponent>();
        }
    }
}
