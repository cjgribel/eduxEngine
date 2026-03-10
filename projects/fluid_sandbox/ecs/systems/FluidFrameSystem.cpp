// Created by OpenAI Codex 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/FluidFrameSystem.hpp"

#include "EngineContext.hpp"
#include "ShapeRenderer.hpp"
#include "util/ThreadPool.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <future>
#include <unordered_set>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

namespace eeng::fluid_sandbox::ecs::systems
{
    namespace
    {
        using Json = nlohmann::json;

        glm::vec2 parse_vec2(const Json& j, const glm::vec2& fallback)
        {
            if (!j.is_array() || j.size() != 2)
                return fallback;
            return glm::vec2(j[0].get<float>(), j[1].get<float>());
        }

        std::string parse_string(const Json& j, const char* key, const std::string& fallback)
        {
            if (!j.contains(key) || !j[key].is_string())
                return fallback;
            return j[key].get<std::string>();
        }

        template<typename Fn>
        void parallel_for_chunks(
            EngineContext& ctx,
            std::size_t item_count,
            std::size_t min_chunk_size,
            Fn&& fn,
            bool& threaded_used)
        {
            threaded_used = false;
            if (!ctx.thread_pool || item_count < min_chunk_size * 2u)
            {
                fn(0u, item_count);
                return;
            }

            const std::size_t worker_count = std::max<std::size_t>(1u, ctx.thread_pool->nbr_threads());
            const std::size_t chunk_size = std::max(min_chunk_size, (item_count + worker_count - 1u) / worker_count);
            std::vector<std::future<void>> futures;
            for (std::size_t begin = 0; begin < item_count; begin += chunk_size)
            {
                const std::size_t end = std::min(begin + chunk_size, item_count);
                futures.push_back(ctx.thread_pool->queue_task([begin, end, &fn]()
                    {
                        fn(begin, end);
                    }));
            }

            for (auto& future : futures)
                future.get();

            threaded_used = futures.size() > 1u;
        }

        bool point_in_box(glm::vec2 uv, glm::vec2 min_uv, glm::vec2 max_uv)
        {
            return uv.x >= min_uv.x && uv.x <= max_uv.x
                && uv.y >= min_uv.y && uv.y <= max_uv.y;
        }

        bool point_in_circle(glm::vec2 uv, glm::vec2 center_uv, float radius_uv)
        {
            const glm::vec2 delta = uv - center_uv;
            return glm::dot(delta, delta) <= radius_uv * radius_uv;
        }

        bool point_in_emitter(
            const FluidFrameSystem::EmitterDesc& emitter,
            glm::vec2 uv)
        {
            if (emitter.shape == FluidFrameSystem::EmitterShape::Box)
                return point_in_box(uv, emitter.min_uv, emitter.max_uv);
            return point_in_circle(uv, emitter.center_uv, emitter.radius_uv);
        }

        bool point_in_obstacle(
            const FluidFrameSystem::ObstacleDesc& obstacle,
            glm::vec2 uv)
        {
            if (obstacle.shape == FluidFrameSystem::ObstacleShape::Circle)
                return point_in_circle(uv, obstacle.center_uv, obstacle.radius_uv);
            return point_in_box(uv, obstacle.min_uv, obstacle.max_uv);
        }
    }

    void FluidFrameSystem::update(entt::registry& registry, EngineContext& ctx, float dt)
    {
        frame_stats_ = FrameStats{};

        auto view = registry.view<FluidFrameComponent>();
        frame_stats_.frame_count = view.size();

        std::vector<entt::entity> entities;
        entities.reserve(view.size());
        std::unordered_set<std::uint32_t> active_ids;
        active_ids.reserve(view.size());
        for (auto entity : view)
        {
            entities.push_back(entity);
            active_ids.insert(static_cast<std::uint32_t>(entt::to_integral(entity)));
        }

        for (auto it = runtimes_.begin(); it != runtimes_.end();)
        {
            const auto key = static_cast<std::uint32_t>(entt::to_integral(it->first));
            if (active_ids.contains(key))
            {
                ++it;
                continue;
            }
            it = runtimes_.erase(it);
        }

        for (auto entity : entities)
        {
            const auto& component = view.get<FluidFrameComponent>(entity);
            auto& runtime = runtimes_[entity];
            update_single_frame(entity, component, runtime, ctx, dt);
            frame_stats_.total_cell_count += static_cast<std::size_t>(std::max(0, runtime.resolution.x))
                * static_cast<std::size_t>(std::max(0, runtime.resolution.y));
            if (component.enabled)
                ++frame_stats_.active_frame_count;
        }
    }

    void FluidFrameSystem::render_overlay(
        entt::registry& registry,
        EngineContext&,
        ShapeRendering::ShapeRenderer& renderer)
    {
        (void)registry;
        (void)renderer;

        // Deliberately left as a no-op for the sketch.
        // The important part here is the ownership boundary:
        // this project-local system should be the only place that knows how
        // to visualize the fluid state, whether that later becomes debug lines,
        // point sprites, or a texture-backed quad.
    }

    void FluidFrameSystem::clear()
    {
        runtimes_.clear();
        frame_stats_ = FrameStats{};
    }

    void FluidFrameSystem::update_single_frame(
        entt::entity,
        const FluidFrameComponent& component,
        Runtime& runtime,
        EngineContext& ctx,
        float dt)
    {
        if (!component.enabled)
            return;

        resize_runtime(runtime, component.resolution);
        runtime.loaded_config_path = component.config_path;

        Config config = runtime.config;
        if (load_config_from_file(component.config_path, config))
        {
            runtime.config = config;
            runtime.cell_size = config.cell_size;
            runtime.viscosity = config.viscosity;
            runtime.velocity_damping = config.velocity_damping;
            runtime.density_damping = config.density_damping;
            runtime.pressure_iterations = config.pressure_iterations;
            rebuild_solid_mask(runtime);
        }

        const int substeps = std::max(1, component.substeps);
        const float max_dt = std::max(1.0e-4f, component.max_step_dt);
        const float clamped_dt = std::min(std::max(0.0f, dt * component.simulation_rate), max_dt);
        const float step_dt = clamped_dt / static_cast<float>(substeps);

        for (int i = 0; i < substeps; ++i)
        {
            // The order below mirrors the standard split used in many real-time
            // incompressible fluid solvers:
            //
            // 1) Add sources/forces
            // 2) Advect velocity
            // 3) Diffuse velocity (optional)
            // 4) Compute divergence of the intermediate velocity u*
            // 5) Solve pressure Poisson equation
            // 6) Project u* -> u^{n+1} so div(u^{n+1}) = 0
            // 7) Advect density using the projected velocity
            // 8) Re-apply wall/obstacle boundary conditions
            apply_emitters(runtime, step_dt);
            advect_velocity(runtime, step_dt);
            diffuse_velocity(runtime, step_dt);
            compute_divergence(runtime, step_dt);
            solve_pressure(runtime, step_dt);
            project_velocity(runtime, step_dt);
            advect_density(runtime, step_dt);
            apply_boundary_conditions(runtime);

            const float vel_damp = std::clamp(1.0f - runtime.velocity_damping * step_dt, 0.0f, 1.0f);
            const float den_damp = std::clamp(1.0f - runtime.density_damping * step_dt, 0.0f, 1.0f);
            for (float& value : runtime.u)
                value *= vel_damp;
            for (float& value : runtime.v)
                value *= vel_damp;
            for (float& value : runtime.density)
                value *= den_damp;
        }

        bool threaded_used = false;
        parallel_for_chunks(ctx, static_cast<std::size_t>(runtime.resolution.y), 16u,
            [&](std::size_t begin, std::size_t end)
            {
                for (std::size_t y = begin; y < end; ++y)
                {
                    for (int x = 0; x < runtime.resolution.x; ++x)
                    {
                        const std::size_t idx = center_index(x, static_cast<int>(y), runtime.resolution.x);
                        runtime.density[idx] = std::max(0.0f, runtime.density[idx]);
                    }
                }
            },
            threaded_used);
        frame_stats_.threaded_update_used = frame_stats_.threaded_update_used || threaded_used;
    }

    std::size_t FluidFrameSystem::center_index(int x, int y, int nx)
    {
        return static_cast<std::size_t>(y * nx + x);
    }

    std::size_t FluidFrameSystem::u_index(int x, int y, int nx)
    {
        return static_cast<std::size_t>(y * (nx + 1) + x);
    }

    std::size_t FluidFrameSystem::v_index(int x, int y, int nx)
    {
        return static_cast<std::size_t>(y * nx + x);
    }

    void FluidFrameSystem::resize_runtime(Runtime& runtime, glm::ivec2 resolution)
    {
        resolution.x = std::max(1, resolution.x);
        resolution.y = std::max(1, resolution.y);
        if (runtime.resolution == resolution)
            return;

        runtime.resolution = resolution;
        const int nx = resolution.x;
        const int ny = resolution.y;

        runtime.u.assign(static_cast<std::size_t>((nx + 1) * ny), 0.0f);
        runtime.u_tmp.assign(runtime.u.size(), 0.0f);
        runtime.v.assign(static_cast<std::size_t>(nx * (ny + 1)), 0.0f);
        runtime.v_tmp.assign(runtime.v.size(), 0.0f);
        runtime.pressure.assign(static_cast<std::size_t>(nx * ny), 0.0f);
        runtime.pressure_tmp.assign(runtime.pressure.size(), 0.0f);
        runtime.divergence.assign(runtime.pressure.size(), 0.0f);
        runtime.density.assign(runtime.pressure.size(), 0.0f);
        runtime.density_tmp.assign(runtime.pressure.size(), 0.0f);
        runtime.solid_mask.assign(runtime.pressure.size(), 0u);
    }

    bool FluidFrameSystem::load_config_from_file(const std::filesystem::path& path, Config& out_config)
    {
        std::ifstream file(path);
        if (!file)
            return false;

        Json root{};
        file >> root;

        Config cfg{};
        if (root.contains("simulation"))
        {
            const auto& simulation = root["simulation"];
            cfg.cell_size = simulation.value("cell_size", cfg.cell_size);
            cfg.viscosity = simulation.value("viscosity", cfg.viscosity);
            cfg.velocity_damping = simulation.value("velocity_damping", cfg.velocity_damping);
            cfg.density_damping = simulation.value("density_damping", cfg.density_damping);
            cfg.pressure_iterations = simulation.value("pressure_iterations", cfg.pressure_iterations);
        }

        const auto parse_boundary = [](const Json& boundary, EdgeBoundaryDesc& out)
        {
            if (boundary.contains("velocity"))
            {
                const auto& velocity = boundary["velocity"];
                out.velocity.type = parse_string(velocity, "type", "dirichlet") == "neumann"
                    ? ScalarBoundaryType::Neumann
                    : ScalarBoundaryType::Dirichlet;
                out.velocity.value = parse_vec2(velocity.value("value", Json::array()), out.velocity.value);
            }
            if (boundary.contains("density"))
            {
                const auto& density = boundary["density"];
                out.density.type = parse_string(density, "type", "dirichlet") == "neumann"
                    ? ScalarBoundaryType::Neumann
                    : ScalarBoundaryType::Dirichlet;
                out.density.value = density.value("value", out.density.value);
            }
        };

        if (root.contains("boundaries"))
        {
            const auto& boundaries = root["boundaries"];
            if (boundaries.contains("left"))
                parse_boundary(boundaries["left"], cfg.boundaries.left);
            if (boundaries.contains("right"))
                parse_boundary(boundaries["right"], cfg.boundaries.right);
            if (boundaries.contains("top"))
                parse_boundary(boundaries["top"], cfg.boundaries.top);
            if (boundaries.contains("bottom"))
                parse_boundary(boundaries["bottom"], cfg.boundaries.bottom);
        }

        if (root.contains("obstacles") && root["obstacles"].is_array())
        {
            for (const auto& obstacle_json : root["obstacles"])
            {
                ObstacleDesc obstacle{};
                obstacle.shape = parse_string(obstacle_json, "shape", "box") == "circle"
                    ? ObstacleShape::Circle
                    : ObstacleShape::Box;
                obstacle.min_uv = parse_vec2(obstacle_json.value("min_uv", Json::array()), obstacle.min_uv);
                obstacle.max_uv = parse_vec2(obstacle_json.value("max_uv", Json::array()), obstacle.max_uv);
                obstacle.center_uv = parse_vec2(obstacle_json.value("center_uv", Json::array()), obstacle.center_uv);
                obstacle.radius_uv = obstacle_json.value("radius_uv", obstacle.radius_uv);
                obstacle.boundary = parse_string(obstacle_json, "boundary", "no_slip") == "free_slip"
                    ? ObstacleBoundaryMode::FreeSlip
                    : ObstacleBoundaryMode::NoSlip;
                cfg.obstacles.push_back(obstacle);
            }
        }

        if (root.contains("emitters") && root["emitters"].is_array())
        {
            for (const auto& emitter_json : root["emitters"])
            {
                EmitterDesc emitter{};
                emitter.kind = parse_string(emitter_json, "kind", "density") == "velocity"
                    ? EmitterKind::Velocity
                    : EmitterKind::Density;
                emitter.shape = parse_string(emitter_json, "shape", "circle") == "box"
                    ? EmitterShape::Box
                    : EmitterShape::Circle;
                emitter.center_uv = parse_vec2(emitter_json.value("center_uv", Json::array()), emitter.center_uv);
                emitter.min_uv = parse_vec2(emitter_json.value("min_uv", Json::array()), emitter.min_uv);
                emitter.max_uv = parse_vec2(emitter_json.value("max_uv", Json::array()), emitter.max_uv);
                emitter.radius_uv = emitter_json.value("radius_uv", emitter.radius_uv);
                emitter.amount = emitter_json.value("amount", emitter.amount);
                emitter.value = parse_vec2(emitter_json.value("value", Json::array()), emitter.value);
                cfg.emitters.push_back(emitter);
            }
        }

        out_config = std::move(cfg);
        return true;
    }

    void FluidFrameSystem::rebuild_solid_mask(Runtime& runtime)
    {
        std::fill(runtime.solid_mask.begin(), runtime.solid_mask.end(), 0u);

        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(nx),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(ny));
                for (const auto& obstacle : runtime.config.obstacles)
                {
                    if (point_in_obstacle(obstacle, uv))
                    {
                        runtime.solid_mask[center_index(x, y, nx)] = 1u;
                        break;
                    }
                }
            }
        }
    }

    float FluidFrameSystem::sample_center_field(
        const std::vector<float>& field,
        int nx,
        int ny,
        glm::vec2 ij)
    {
        const float x = std::clamp(ij.x, 0.0f, static_cast<float>(nx - 1));
        const float y = std::clamp(ij.y, 0.0f, static_cast<float>(ny - 1));

        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const int x1 = std::min(x0 + 1, nx - 1);
        const int y1 = std::min(y0 + 1, ny - 1);
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);

        const float c00 = field[center_index(x0, y0, nx)];
        const float c10 = field[center_index(x1, y0, nx)];
        const float c01 = field[center_index(x0, y1, nx)];
        const float c11 = field[center_index(x1, y1, nx)];
        const float c0 = glm::mix(c00, c10, tx);
        const float c1 = glm::mix(c01, c11, tx);
        return glm::mix(c0, c1, ty);
    }

    glm::vec2 FluidFrameSystem::sample_velocity_field(const Runtime& runtime, glm::vec2 ij)
    {
        // Placeholder for a proper MAC-grid face sampler.
        // For the skeleton we average nearby face values to recover a cell-centered
        // velocity estimate, which is enough to explain the pass structure.
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        const int x = std::clamp(static_cast<int>(std::round(ij.x)), 0, nx - 1);
        const int y = std::clamp(static_cast<int>(std::round(ij.y)), 0, ny - 1);

        const float u_left = runtime.u[u_index(x, y, nx)];
        const float u_right = runtime.u[u_index(x + 1, y, nx)];
        const float v_down = runtime.v[v_index(x, y, nx)];
        const float v_up = runtime.v[v_index(x, y + 1, nx)];
        return glm::vec2(0.5f * (u_left + u_right), 0.5f * (v_down + v_up));
    }

    void FluidFrameSystem::apply_emitters(Runtime& runtime, float dt)
    {
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(nx),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(ny));
                for (const auto& emitter : runtime.config.emitters)
                {
                    if (!point_in_emitter(emitter, uv))
                        continue;

                    const std::size_t idx = center_index(x, y, nx);
                    if (emitter.kind == EmitterKind::Density)
                    {
                        runtime.density[idx] += emitter.amount * dt;
                    }
                    else
                    {
                        runtime.u[u_index(x, y, nx)] += emitter.value.x * dt;
                        runtime.u[u_index(x + 1, y, nx)] += emitter.value.x * dt;
                        runtime.v[v_index(x, y, nx)] += emitter.value.y * dt;
                        runtime.v[v_index(x, y + 1, nx)] += emitter.value.y * dt;
                    }
                }
            }
        }
    }

    void FluidFrameSystem::advect_velocity(Runtime& runtime, float)
    {
        // Intended discretization:
        //
        //     du/dt + (u · grad)u = 0
        //
        // A standard real-time choice is semi-Lagrangian advection:
        //
        //     u^{n+1}(x) = u^n(x - dt * u^n(x))
        //
        // On a MAC grid this means tracing each face velocity backward through the
        // current velocity field, then sampling the previous field at that point.
        //
        // For the sketch we keep this pass as a transparent placeholder rather than
        // sneaking in a half-finished implementation.
        runtime.u_tmp = runtime.u;
        runtime.v_tmp = runtime.v;
        runtime.u.swap(runtime.u_tmp);
        runtime.v.swap(runtime.v_tmp);
    }

    void FluidFrameSystem::diffuse_velocity(Runtime& runtime, float)
    {
        // Intended equation:
        //
        //     du/dt = nu laplacian(u)
        //
        // This is commonly handled implicitly with Jacobi iterations:
        //
        //     (I - dt * nu * laplacian) u^{n+1} = u^*
        //
        // For now we skip the solve when viscosity is effectively zero.
        if (runtime.viscosity <= 1.0e-8f)
            return;
    }

    void FluidFrameSystem::compute_divergence(Runtime& runtime, float dt)
    {
        // Projection step source term:
        //
        //     div(u*) = d/dx u + d/dy v
        //
        // On a MAC grid the divergence in cell (i, j) is:
        //
        //     ((u_{i+1/2,j} - u_{i-1/2,j}) + (v_{i,j+1/2} - v_{i,j-1/2})) / h
        //
        // This produces the right-hand side of the Poisson pressure solve.
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        const float inv_h = 1.0f / std::max(1.0e-6f, runtime.cell_size);
        const float inv_dt = dt > 1.0e-8f ? (1.0f / dt) : 0.0f;

        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const std::size_t idx = center_index(x, y, nx);
                if (runtime.solid_mask[idx] != 0u)
                {
                    runtime.divergence[idx] = 0.0f;
                    continue;
                }

                const float u_left = runtime.u[u_index(x, y, nx)];
                const float u_right = runtime.u[u_index(x + 1, y, nx)];
                const float v_down = runtime.v[v_index(x, y, nx)];
                const float v_up = runtime.v[v_index(x, y + 1, nx)];
                runtime.divergence[idx] = inv_dt * inv_h * ((u_right - u_left) + (v_up - v_down));
            }
        }
    }

    void FluidFrameSystem::solve_pressure(Runtime& runtime, float dt)
    {
        // Pressure projection equation:
        //
        //     laplacian(p) = (rho / dt) * div(u*)
        //
        // With rho absorbed into units, a Jacobi update becomes:
        //
        //     p_new(i,j) = (pL + pR + pB + pT - h^2 * rhs(i,j)) / 4
        //
        // Jacobi is not the fastest solver, but it is easy to read, easy to
        // parallelize, and a very good teaching/reference baseline.
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        const float h2 = runtime.cell_size * runtime.cell_size;
        std::fill(runtime.pressure.begin(), runtime.pressure.end(), 0.0f);

        for (int iter = 0; iter < std::max(1, runtime.pressure_iterations); ++iter)
        {
            for (int y = 0; y < ny; ++y)
            {
                for (int x = 0; x < nx; ++x)
                {
                    const std::size_t idx = center_index(x, y, nx);
                    if (runtime.solid_mask[idx] != 0u)
                    {
                        runtime.pressure_tmp[idx] = 0.0f;
                        continue;
                    }

                    const float p_left = runtime.pressure[center_index(std::max(x - 1, 0), y, nx)];
                    const float p_right = runtime.pressure[center_index(std::min(x + 1, nx - 1), y, nx)];
                    const float p_down = runtime.pressure[center_index(x, std::max(y - 1, 0), nx)];
                    const float p_up = runtime.pressure[center_index(x, std::min(y + 1, ny - 1), nx)];
                    const float rhs = runtime.divergence[idx] * dt;
                    runtime.pressure_tmp[idx] = (p_left + p_right + p_down + p_up - h2 * rhs) * 0.25f;
                }
            }
            runtime.pressure.swap(runtime.pressure_tmp);
        }
    }

    void FluidFrameSystem::project_velocity(Runtime& runtime, float dt)
    {
        // Velocity projection:
        //
        //     u^{n+1} = u* - dt grad(p)
        //
        // On a MAC grid, each face velocity is corrected by the pressure difference
        // across the two cells that share the face.
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        const float inv_h = 1.0f / std::max(1.0e-6f, runtime.cell_size);

        for (int y = 0; y < ny; ++y)
        {
            for (int x = 1; x < nx; ++x)
            {
                const float p_left = runtime.pressure[center_index(x - 1, y, nx)];
                const float p_right = runtime.pressure[center_index(x, y, nx)];
                runtime.u[u_index(x, y, nx)] -= dt * inv_h * (p_right - p_left);
            }
        }

        for (int y = 1; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const float p_down = runtime.pressure[center_index(x, y - 1, nx)];
                const float p_up = runtime.pressure[center_index(x, y, nx)];
                runtime.v[v_index(x, y, nx)] -= dt * inv_h * (p_up - p_down);
            }
        }
    }

    void FluidFrameSystem::advect_density(Runtime& runtime, float dt)
    {
        // Scalar transport equation:
        //
        //     dphi/dt + u · grad(phi) = 0
        //
        // Semi-Lagrangian discretization:
        //
        //     phi^{n+1}(x) = phi^n(x - dt * u(x))
        //
        // This is the first pass where we implement the idea directly, because
        // scalar advection is simpler to read than MAC-face velocity advection.
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const glm::vec2 cell_ij(static_cast<float>(x), static_cast<float>(y));
                const glm::vec2 velocity = sample_velocity_field(runtime, cell_ij);
                const glm::vec2 backtraced = cell_ij - (dt / std::max(1.0e-6f, runtime.cell_size)) * velocity;
                runtime.density_tmp[center_index(x, y, nx)] = sample_center_field(runtime.density, nx, ny, backtraced);
            }
        }
        runtime.density.swap(runtime.density_tmp);
    }

    void FluidFrameSystem::apply_boundary_conditions(Runtime& runtime)
    {
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;

        // Domain edges. The simplest physically reasonable default for a boxed
        // domain is no-through-flow at the outer faces.
        for (int y = 0; y < ny; ++y)
        {
            runtime.u[u_index(0, y, nx)] = runtime.config.boundaries.left.velocity.value.x;
            runtime.u[u_index(nx, y, nx)] = runtime.config.boundaries.right.velocity.value.x;
        }
        for (int x = 0; x < nx; ++x)
        {
            runtime.v[v_index(x, 0, nx)] = runtime.config.boundaries.bottom.velocity.value.y;
            runtime.v[v_index(x, ny, nx)] = runtime.config.boundaries.top.velocity.value.y;
        }

        // Embedded obstacles. For the sketch we zero the nearby face velocities.
        // A more complete version would distinguish no-slip vs free-slip here.
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                if (runtime.solid_mask[center_index(x, y, nx)] == 0u)
                    continue;

                runtime.density[center_index(x, y, nx)] = 0.0f;
                runtime.u[u_index(x, y, nx)] = 0.0f;
                runtime.u[u_index(x + 1, y, nx)] = 0.0f;
                runtime.v[v_index(x, y, nx)] = 0.0f;
                runtime.v[v_index(x, y + 1, nx)] = 0.0f;
            }
        }
    }
}
