// Created by OpenAI Codex 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace eeng::reference_game::ecs
{
    enum class FluidFrameRenderMode : std::uint8_t
    {
        Density,
        VelocityGlyphs,
        Pressure
    };

    struct FluidFrameComponent
    {
        std::string name = "Fluid Frame";
        bool enabled = true;

        // The simulation lives in the entity's local XY plane.
        glm::vec2 frame_size = { 4.0f, 2.0f };
        glm::ivec2 resolution = { 128, 64 };

        float simulation_rate = 1.0f;
        float max_step_dt = 1.0f / 30.0f;
        int substeps = 1;

        // Keep v1 project-local and low-friction: point directly at a JSON file.
        std::string config_path = "projects/reference_game/data/fluids/default_frame.json";

        FluidFrameRenderMode render_mode = FluidFrameRenderMode::Density;
        bool debug_draw_frame = true;
        bool debug_draw_velocity = false;
        float density_gain = 1.0f;
        float velocity_glyph_scale = 0.20f;
        std::uint32_t tint_abgr = 0xffffffffu;
    };

    inline std::string to_string(const FluidFrameComponent&)
    {
        return "FluidFrameComponent(...)";
    }

    template<typename Visitor>
    void visit_asset_refs(FluidFrameComponent&, Visitor&&)
    {
    }

    template<typename Visitor>
    void visit_entity_refs(FluidFrameComponent&, Visitor&&)
    {
    }
}
