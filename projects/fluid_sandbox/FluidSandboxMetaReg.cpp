// Created by OpenAI Codex 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "FluidSandboxMetaReg.hpp"

#include "LogMacros.h"
#include "MetaInfo.h"
#include "ecs/FluidFrameComponent.hpp"
#include "meta/EntityMetaHelpers.hpp"
#include "meta/MetaLiterals.h"

#include <entt/entt.hpp>
#include <stdexcept>
#include <type_traits>

namespace eeng::fluid_sandbox
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

    void register_fluid_sandbox_meta_types(EngineContext& ctx)
    {
        EENG_LOG_INFO(&ctx, "Registering fluid_sandbox meta types...");

        const TypeMetaInfo render_mode_info{
            .id = "eeng.fluid_sandbox.ecs.FluidFrameRenderMode",
            .name = "FluidFrameRenderMode",
            .tooltip = "Visualization mode for a 2D fluid frame.",
            .underlying_type = entt::resolve<std::underlying_type_t<eeng::fluid_sandbox::ecs::FluidFrameRenderMode>>()
        };
        entt::meta_factory<eeng::fluid_sandbox::ecs::FluidFrameRenderMode>{}
            .custom<TypeMetaInfo>(render_mode_info)
            .traits(MetaFlags::none)
            .data<eeng::fluid_sandbox::ecs::FluidFrameRenderMode::Density>("Density"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Density", "Render the dye/density scalar field." })
            .traits(MetaFlags::none)
            .data<eeng::fluid_sandbox::ecs::FluidFrameRenderMode::VelocityGlyphs>("VelocityGlyphs"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Velocity Glyphs", "Render sampled velocity vectors as debug lines." })
            .traits(MetaFlags::none)
            .data<eeng::fluid_sandbox::ecs::FluidFrameRenderMode::Pressure>("Pressure"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Pressure", "Render the scalar pressure solve result." })
            .traits(MetaFlags::none)
            .data<eeng::fluid_sandbox::ecs::FluidFrameRenderMode::Divergence>("Divergence"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Divergence", "Render div(u), useful for debugging the projection step." })
            .traits(MetaFlags::none)
            .data<eeng::fluid_sandbox::ecs::FluidFrameRenderMode::Vorticity>("Vorticity"_hs)
            .custom<EnumDataMetaInfo>(EnumDataMetaInfo{ "Vorticity", "Render curl(u), useful for debugging swirl and shear." })
            .traits(MetaFlags::none);
        eeng::meta::register_type<eeng::fluid_sandbox::ecs::FluidFrameRenderMode>();
        warm_start_meta_type<eeng::fluid_sandbox::ecs::FluidFrameRenderMode>();

        entt::meta_factory<eeng::fluid_sandbox::ecs::FluidFrameComponent>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{
                .id = "eeng.fluid_sandbox.ecs.FluidFrameComponent",
                .name = "FluidFrameComponent",
                .tooltip = "Project-local 2D incompressible fluid frame." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::name>("name"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "name", "Name", "Debug/display name." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::enabled>("enabled"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "enabled", "Enabled", "Enable simulation and rendering." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::frame_size>("frame_size"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "frame_size", "Frame Size", "Frame extents in the local XY plane." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::resolution>("resolution"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "resolution", "Resolution", "Grid resolution (nx, ny)." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::simulation_rate>("simulation_rate"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "simulation_rate", "Simulation Rate", "Scale factor applied to frame dt." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::max_step_dt>("max_step_dt"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "max_step_dt", "Max Step Dt", "Safety clamp for the internal solver dt." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::substeps>("substeps"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "substeps", "Substeps", "Number of internal solver substeps." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::config_path>("config_path"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "config_path", "Config Path", "Project-relative JSON file for boundaries, obstacles, and emitters." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::apply_velocity_damping>("apply_velocity_damping"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "apply_velocity_damping", "Apply Velocity Damping", "If disabled, effective velocity damping is forced to zero." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::apply_density_damping>("apply_density_damping"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "apply_density_damping", "Apply Density Damping", "If disabled, effective density damping is forced to zero." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::override_velocity_damping>("override_velocity_damping"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "override_velocity_damping", "Override Velocity Damping", "Use the inspector value instead of the JSON config value." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::velocity_damping>("velocity_damping"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{
                "velocity_damping",
                "Velocity Damping",
                "Per-second damping applied to the velocity field.",
                .ui_has_range = true,
                .ui_range_min = 0.0f,
                .ui_range_max = 2.0f,
                .ui_speed = 0.001f
            })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::override_density_damping>("override_density_damping"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "override_density_damping", "Override Density Damping", "Use the inspector value instead of the JSON config value." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::density_damping>("density_damping"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{
                "density_damping",
                "Density Damping",
                "Per-second damping applied to the density field.",
                .ui_has_range = true,
                .ui_range_min = 0.0f,
                .ui_range_max = 2.0f,
                .ui_speed = 0.001f
            })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::emit_density>("emit_density"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "emit_density", "Emit Density", "Enable density emitters from the fluid config." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::emit_velocity>("emit_velocity"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "emit_velocity", "Emit Velocity", "Enable velocity emitters from the fluid config." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::density_emitter_scale>("density_emitter_scale"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{
                "density_emitter_scale",
                "Density Emitter Scale",
                "Multiplier applied to density emitter strength.",
                .ui_has_range = true,
                .ui_range_min = 0.0f,
                .ui_range_max = 5.0f,
                .ui_speed = 0.01f
            })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::velocity_emitter_scale>("velocity_emitter_scale"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{
                "velocity_emitter_scale",
                "Velocity Emitter Scale",
                "Multiplier applied to velocity emitter strength.",
                .ui_has_range = true,
                .ui_range_min = 0.0f,
                .ui_range_max = 5.0f,
                .ui_speed = 0.01f
            })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::render_mode>("render_mode"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "render_mode", "Render Mode", "Visualization mode for debug rendering." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::debug_draw_frame>("debug_draw_frame"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "debug_draw_frame", "Draw Frame", "Draw frame bounds in the overlay pass." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::debug_draw_velocity>("debug_draw_velocity"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "debug_draw_velocity", "Draw Velocity", "Draw velocity glyphs in the overlay pass." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::debug_draw_obstacle_faces>("debug_draw_obstacle_faces"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "debug_draw_obstacle_faces", "Draw Obstacle Faces", "Draw blocked obstacle/interface faces in the overlay pass." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::density_gain>("density_gain"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "density_gain", "Density Gain", "Display gain applied to density visualization." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::velocity_glyph_scale>("velocity_glyph_scale"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "velocity_glyph_scale", "Velocity Glyph Scale", "Scale used when drawing velocity arrows." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::tint_abgr>("tint_abgr"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "tint_abgr", "Tint", "Base tint used by the visualization." })
            .traits(MetaFlags::none);

        register_component<eeng::fluid_sandbox::ecs::FluidFrameComponent>();
    }
}
