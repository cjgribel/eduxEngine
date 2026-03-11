// Created by OpenAI Codex 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/FluidFrameComponent.hpp"
#include "entt/entt.hpp"
#include "ShapeRenderer.hpp"

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace ShapeRendering
{
    class ShapeRenderer;
}

namespace eeng
{
    struct EngineContext;
}

namespace eeng::fluid_sandbox::ecs::systems
{
    class FluidFrameSystem
    {
    public:
        struct FrameStats
        {
            std::size_t frame_count = 0;
            std::size_t active_frame_count = 0;
            std::size_t total_cell_count = 0;
            bool threaded_update_used = false;
        };

        // These types are public so parsing helpers and tests can live out of line
        // without dragging implementation details into unrelated headers.
        enum class ScalarBoundaryType : std::uint8_t
        {
            Dirichlet,
            Neumann
        };

        enum class ObstacleShape : std::uint8_t
        {
            Box,
            Circle
        };

        enum class ObstacleBoundaryMode : std::uint8_t
        {
            NoSlip,
            FreeSlip
        };

        enum class FaceState : std::uint8_t
        {
            Fluid,
            SolidInterior,
            SolidBoundaryNoSlip,
            SolidBoundaryFreeSlip
        };

        enum class EmitterKind : std::uint8_t
        {
            Density,
            Velocity
        };

        enum class EmitterShape : std::uint8_t
        {
            Circle,
            Box
        };

        struct ScalarBoundaryDesc
        {
            ScalarBoundaryType type = ScalarBoundaryType::Dirichlet;
            float value = 0.0f;
        };

        struct VelocityBoundaryDesc
        {
            ScalarBoundaryType type = ScalarBoundaryType::Dirichlet;
            glm::vec2 value{ 0.0f, 0.0f };
        };

        struct EdgeBoundaryDesc
        {
            VelocityBoundaryDesc velocity{};
            ScalarBoundaryDesc density{};
        };

        struct BoundaryDesc
        {
            EdgeBoundaryDesc left{};
            EdgeBoundaryDesc right{};
            EdgeBoundaryDesc top{};
            EdgeBoundaryDesc bottom{};
        };

        struct ObstacleDesc
        {
            ObstacleShape shape = ObstacleShape::Box;
            glm::vec2 min_uv{ 0.0f, 0.0f };
            glm::vec2 max_uv{ 1.0f, 1.0f };
            glm::vec2 center_uv{ 0.5f, 0.5f };
            float radius_uv = 0.1f;
            ObstacleBoundaryMode boundary = ObstacleBoundaryMode::NoSlip;
        };

        struct EmitterDesc
        {
            EmitterKind kind = EmitterKind::Density;
            EmitterShape shape = EmitterShape::Circle;
            glm::vec2 center_uv{ 0.5f, 0.5f };
            glm::vec2 min_uv{ 0.0f, 0.0f };
            glm::vec2 max_uv{ 1.0f, 1.0f };
            float radius_uv = 0.05f;
            float amount = 1.0f;
            glm::vec2 value{ 0.0f, 0.0f };
        };

        struct Config
        {
            float cell_size = 0.05f;
            float viscosity = 0.0f;
            float velocity_damping = 0.0f;
            float density_damping = 0.0f;
            int pressure_iterations = 24;
            BoundaryDesc boundaries{};
            std::vector<ObstacleDesc> obstacles;
            std::vector<EmitterDesc> emitters;
        };

        void update(entt::registry& registry, EngineContext& ctx, float dt);
        void render_overlay(
            entt::registry& registry,
            EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer);
        void clear();

        const FrameStats& frame_stats() const { return frame_stats_; }

    private:
        // Governing equations for the intended solver:
        //
        //     du/dt + (u · grad)u = -(1/rho) grad p + nu laplacian(u) + f
        //     div(u) = 0
        //
        // We use the standard "advection + diffusion + projection" split:
        //
        // 1. Advect velocity
        // 2. Diffuse velocity (optional)
        // 3. Solve pressure from div(u*)
        // 4. Project velocity to make div(u^{n+1}) = 0
        // 5. Advect density/dye
        //
        // For references, see docs/design/NavierStokesMath.md.

        struct Runtime
        {
            glm::ivec2 resolution{ 0, 0 };
            float cell_size = 0.05f;
            float viscosity = 0.0f;
            float velocity_damping = 0.0f;
            float density_damping = 0.0f;
            int pressure_iterations = 24;

            Config config{};

            // MAC-style layout:
            // - u on vertical faces: (nx + 1) * ny
            // - v on horizontal faces: nx * (ny + 1)
            // - pressure / divergence / density at cell centers: nx * ny
            std::vector<float> u;
            std::vector<float> u_tmp;
            std::vector<float> v;
            std::vector<float> v_tmp;
            std::vector<float> pressure;
            std::vector<float> pressure_tmp;
            std::vector<float> divergence;
            std::vector<float> vorticity;
            std::vector<float> density;
            std::vector<float> density_tmp;
            std::vector<std::uint8_t> obstacle_mask;
            std::vector<std::uint8_t> obstacle_boundary_modes;
            std::vector<std::uint8_t> u_face_states;
            std::vector<std::uint8_t> v_face_states;

            std::filesystem::path loaded_config_path;
            std::filesystem::file_time_type loaded_write_time{};
        };

        void update_single_frame(
            entt::entity entity,
            const FluidFrameComponent& component,
            Runtime& runtime,
            EngineContext& ctx,
            float dt);

        static std::size_t center_index(int x, int y, int nx);
        static std::size_t u_index(int x, int y, int nx);
        static std::size_t v_index(int x, int y, int nx);

        static bool resize_runtime(Runtime& runtime, glm::ivec2 resolution);
        static bool load_config_from_file(const std::filesystem::path& path, Config& out_config);
        static void rebuild_solid_mask(Runtime& runtime);

        static bool is_obstacle_cell(const Runtime& runtime, int x, int y);
        static ObstacleBoundaryMode obstacle_boundary_mode_at(const Runtime& runtime, int x, int y);
        static FaceState u_face_state_at(const Runtime& runtime, int x, int y);
        static FaceState v_face_state_at(const Runtime& runtime, int x, int y);
        static bool is_fluid_u_face(const Runtime& runtime, int x, int y);
        static bool is_fluid_v_face(const Runtime& runtime, int x, int y);
        static bool point_inside_obstacle_region(const Runtime& runtime, glm::vec2 ij);
        static glm::vec2 clamp_backtrace_to_fluid(const Runtime& runtime, glm::vec2 origin_ij, glm::vec2 traced_ij);

        static float sample_center_field(const std::vector<float>& field, int nx, int ny, glm::vec2 ij);
        static float sample_u_field(const std::vector<float>& field, int nx, int ny, glm::vec2 u_ij);
        static float sample_v_field(const std::vector<float>& field, int nx, int ny, glm::vec2 v_ij);
        static glm::vec2 sample_velocity_field_legacy(const Runtime& runtime, glm::vec2 ij);
        static glm::vec2 sample_velocity_field(const Runtime& runtime, glm::vec2 ij);

        static void apply_emitters(Runtime& runtime, float dt);
        static void apply_emitters(const FluidFrameComponent& component, Runtime& runtime, float dt);
        static void advect_velocity_legacy(Runtime& runtime);
        static void advect_velocity(Runtime& runtime, float dt);
        static void diffuse_velocity(Runtime& runtime, float dt);
        static void compute_divergence(Runtime& runtime, float dt);
        static void compute_vorticity(Runtime& runtime);
        static void solve_pressure_legacy(Runtime& runtime, float dt);
        static void solve_pressure(Runtime& runtime, float dt);
        static void project_velocity_legacy(Runtime& runtime, float dt);
        static void project_velocity(Runtime& runtime, float dt);
        static void advect_density(Runtime& runtime, float dt);
        static void apply_boundary_conditions_legacy(Runtime& runtime);
        static void apply_boundary_conditions(Runtime& runtime);

        std::unordered_map<entt::entity, Runtime> runtimes_;
        FrameStats frame_stats_{};
    };
}
