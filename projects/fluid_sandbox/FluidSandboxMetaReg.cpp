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

        entt::meta_factory<eeng::fluid_sandbox::ecs::FluidFrameRenderMode>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{
                .id = "eeng.fluid_sandbox.ecs.FluidFrameRenderMode",
                .name = "FluidFrameRenderMode",
                .tooltip = "Visualization mode for a 2D fluid frame." })
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

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::render_mode>("render_mode"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "render_mode", "Render Mode", "Visualization mode for debug rendering." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::debug_draw_frame>("debug_draw_frame"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "debug_draw_frame", "Draw Frame", "Draw frame bounds in the overlay pass." })
            .traits(MetaFlags::none)

            .data<&eeng::fluid_sandbox::ecs::FluidFrameComponent::debug_draw_velocity>("debug_draw_velocity"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "debug_draw_velocity", "Draw Velocity", "Draw velocity glyphs in the overlay pass." })
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
