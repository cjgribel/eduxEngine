// Created by OpenAI Codex 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/FluidFrameSystem.hpp"

#include "EngineContext.hpp"
#include "ShapeRenderer.hpp"
#include "ecs/TransformComponent.hpp"
#include "glmcommon.hpp"
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

        glm::vec4 unpack_abgr(std::uint32_t abgr)
        {
            const float r = static_cast<float>(abgr & 0xffu) / 255.0f;
            const float g = static_cast<float>((abgr >> 8u) & 0xffu) / 255.0f;
            const float b = static_cast<float>((abgr >> 16u) & 0xffu) / 255.0f;
            const float a = static_cast<float>((abgr >> 24u) & 0xffu) / 255.0f;
            return glm::vec4(r, g, b, a);
        }

        std::uint32_t pack_abgr(const glm::vec4& rgba)
        {
            const glm::vec4 clamped = glm::clamp(rgba, glm::vec4(0.0f), glm::vec4(1.0f));
            const std::uint32_t r = static_cast<std::uint32_t>(std::lround(clamped.r * 255.0f));
            const std::uint32_t g = static_cast<std::uint32_t>(std::lround(clamped.g * 255.0f));
            const std::uint32_t b = static_cast<std::uint32_t>(std::lround(clamped.b * 255.0f));
            const std::uint32_t a = static_cast<std::uint32_t>(std::lround(clamped.a * 255.0f));
            return (a << 24u) | (b << 16u) | (g << 8u) | r;
        }

        glm::vec3 frame_local_pos(
            const eeng::fluid_sandbox::ecs::FluidFrameComponent& component,
            float u,
            float v)
        {
            return glm::vec3(
                (u - 0.5f) * component.frame_size.x,
                (v - 0.5f) * component.frame_size.y,
                0.0f);
        }

        std::uint32_t density_color(
            const eeng::fluid_sandbox::ecs::FluidFrameComponent& component,
            float density)
        {
            const float signal = std::max(0.0f, density * component.density_gain);
            const float t = glm::clamp(signal / (1.0f + signal), 0.0f, 1.0f);

            const glm::vec3 c0(0.01f, 0.01f, 0.03f);
            const glm::vec3 c1(0.05f, 0.12f, 0.35f);
            const glm::vec3 c2(0.12f, 0.70f, 0.85f);
            const glm::vec3 c3(0.95f, 0.85f, 0.18f);
            const glm::vec3 c4(0.98f, 0.35f, 0.10f);

            glm::vec3 ramp = c0;
            if (t < 0.25f)
                ramp = glm::mix(c0, c1, t / 0.25f);
            else if (t < 0.50f)
                ramp = glm::mix(c1, c2, (t - 0.25f) / 0.25f);
            else if (t < 0.75f)
                ramp = glm::mix(c2, c3, (t - 0.50f) / 0.25f);
            else
                ramp = glm::mix(c3, c4, (t - 0.75f) / 0.25f);

            const float alpha = glm::mix(0.22f, 0.96f, t);
            return pack_abgr(glm::vec4(ramp, alpha));
        }

        std::uint32_t pressure_color(float pressure, float max_abs_pressure)
        {
            if (max_abs_pressure <= 1.0e-6f)
                return 0u;

            const float normalized = glm::clamp(pressure / max_abs_pressure, -1.0f, 1.0f);
            const float magnitude = std::abs(normalized);
            const glm::vec3 cold(0.20f, 0.55f, 1.00f);
            const glm::vec3 warm(1.00f, 0.45f, 0.20f);
            const glm::vec3 rgb = normalized >= 0.0f ? warm : cold;
            const float alpha = glm::mix(0.05f, 0.80f, magnitude);
            return pack_abgr(glm::vec4(rgb * magnitude, alpha));
        }

        std::uint32_t divergence_color(float divergence, float max_abs_divergence)
        {
            if (max_abs_divergence <= 1.0e-6f)
                return 0u;

            // Divergence is signed:
            // - positive means local outflow / expansion
            // - negative means local inflow / compression
            //
            // For debugging projection, a good image is a signed heat map where
            // both signs remain visible and zero stays near-black.
            const float normalized = glm::clamp(divergence / max_abs_divergence, -1.0f, 1.0f);
            const float magnitude = std::abs(normalized);
            const glm::vec3 inflow(0.16f, 0.54f, 0.98f);
            const glm::vec3 outflow(0.98f, 0.36f, 0.16f);
            const glm::vec3 rgb = normalized >= 0.0f ? outflow : inflow;
            const float alpha = glm::mix(0.10f, 0.88f, magnitude);
            return pack_abgr(glm::vec4(rgb * magnitude, alpha));
        }

        std::uint32_t vorticity_color(float vorticity, float max_abs_vorticity)
        {
            if (max_abs_vorticity <= 1.0e-6f)
                return 0u;

            // In 2D, vorticity is the signed scalar curl:
            //
            //     w = dv/dx - du/dy
            //
            // Positive and negative values correspond to opposite spin
            // directions, so the debug view uses a signed heat map.
            const float normalized = glm::clamp(vorticity / max_abs_vorticity, -1.0f, 1.0f);
            const float magnitude = std::abs(normalized);
            const glm::vec3 clockwise(0.30f, 0.90f, 0.34f);
            const glm::vec3 counter_clockwise(0.96f, 0.20f, 0.74f);
            const glm::vec3 rgb = normalized >= 0.0f ? counter_clockwise : clockwise;
            const float alpha = glm::mix(0.10f, 0.90f, magnitude);
            return pack_abgr(glm::vec4(rgb * magnitude, alpha));
        }

        std::uint32_t face_state_color(FluidFrameSystem::FaceState state)
        {
            switch (state)
            {
            case FluidFrameSystem::FaceState::SolidBoundaryNoSlip:
                return pack_abgr(glm::vec4(0.98f, 0.25f, 0.18f, 0.95f));
            case FluidFrameSystem::FaceState::SolidBoundaryFreeSlip:
                return pack_abgr(glm::vec4(0.18f, 0.86f, 0.96f, 0.95f));
            case FluidFrameSystem::FaceState::SolidInterior:
                return pack_abgr(glm::vec4(0.70f, 0.12f, 0.12f, 0.45f));
            case FluidFrameSystem::FaceState::Fluid:
            default:
                return 0u;
            }
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
        EngineContext& ctx,
        ShapeRendering::ShapeRenderer& renderer)
    {
        (void)ctx;

        auto view = registry.view<FluidFrameComponent>();
        for (auto entity : view)
        {
            const auto& component = view.get<FluidFrameComponent>(entity);
            if (!component.enabled)
                continue;

            const auto runtime_it = runtimes_.find(entity);
            if (runtime_it == runtimes_.end())
                continue;

            const Runtime& runtime = runtime_it->second;
            const int nx = runtime.resolution.x;
            const int ny = runtime.resolution.y;
            if (nx <= 0 || ny <= 0)
                continue;

            glm::mat4 world_matrix(1.0f);
            if (const auto* transform = registry.try_get<eeng::ecs::TransformComponent>(entity))
                world_matrix = transform->world_matrix;

            renderer.push_states(
                ShapeRendering::DepthTest::False,
                ShapeRendering::BackfaceCull::False,
                world_matrix);

            const bool draw_surface = component.render_mode != FluidFrameRenderMode::VelocityGlyphs;
            if (draw_surface)
            {
                float max_abs_pressure = 0.0f;
                float max_abs_divergence = 0.0f;
                float max_abs_vorticity = 0.0f;
                if (component.render_mode == FluidFrameRenderMode::Pressure)
                {
                    for (int y = 0; y < ny; ++y)
                    {
                        for (int x = 0; x < nx; ++x)
                        {
                            const std::size_t idx = center_index(x, y, nx);
                            if (runtime.obstacle_mask[idx] != 0u)
                                continue;
                            max_abs_pressure = std::max(max_abs_pressure, std::abs(runtime.pressure[idx]));
                        }
                    }
                }
                else if (component.render_mode == FluidFrameRenderMode::Divergence)
                {
                    for (int y = 0; y < ny; ++y)
                    {
                        for (int x = 0; x < nx; ++x)
                        {
                            const std::size_t idx = center_index(x, y, nx);
                            if (runtime.obstacle_mask[idx] != 0u)
                                continue;
                            max_abs_divergence = std::max(max_abs_divergence, std::abs(runtime.divergence[idx]));
                        }
                    }
                }
                else if (component.render_mode == FluidFrameRenderMode::Vorticity)
                {
                    for (int y = 0; y < ny; ++y)
                    {
                        for (int x = 0; x < nx; ++x)
                        {
                            const std::size_t idx = center_index(x, y, nx);
                            if (runtime.obstacle_mask[idx] != 0u)
                                continue;
                            max_abs_vorticity = std::max(max_abs_vorticity, std::abs(runtime.vorticity[idx]));
                        }
                    }
                }

                for (int y = 0; y < ny; ++y)
                {
                    const float v0 = static_cast<float>(y) / static_cast<float>(ny);
                    const float v1 = static_cast<float>(y + 1) / static_cast<float>(ny);
                    for (int x = 0; x < nx; ++x)
                    {
                        const std::size_t idx = center_index(x, y, nx);
                        const bool is_obstacle = runtime.obstacle_mask[idx] != 0u;

                        std::uint32_t color = 0u;
                        if (is_obstacle)
                        {
                            color = 0xc43a4450u;
                        }
                        else if (component.render_mode == FluidFrameRenderMode::Pressure)
                        {
                            color = pressure_color(runtime.pressure[idx], max_abs_pressure);
                            if (color == 0u)
                                continue;
                        }
                        else if (component.render_mode == FluidFrameRenderMode::Divergence)
                        {
                            color = divergence_color(runtime.divergence[idx], max_abs_divergence);
                            if (color == 0u)
                                continue;
                        }
                        else if (component.render_mode == FluidFrameRenderMode::Vorticity)
                        {
                            color = vorticity_color(runtime.vorticity[idx], max_abs_vorticity);
                            if (color == 0u)
                                continue;
                        }
                        else
                        {
                            const float density = runtime.density[idx];
                            color = density_color(component, density);
                        }

                        const float u0 = static_cast<float>(x) / static_cast<float>(nx);
                        const float u1 = static_cast<float>(x + 1) / static_cast<float>(nx);
                        const glm::vec3 points[4] = {
                            frame_local_pos(component, u0, v0),
                            frame_local_pos(component, u1, v0),
                            frame_local_pos(component, u1, v1),
                            frame_local_pos(component, u0, v1)
                        };

                        renderer.push_states(ShapeRendering::Color4u{ color });
                        renderer.push_quad(points, glm_aux::vec3_001);
                        renderer.pop_states<ShapeRendering::Color4u>();
                    }
                }
            }

            if (component.debug_draw_frame)
            {
                const glm::vec3 p00 = frame_local_pos(component, 0.0f, 0.0f);
                const glm::vec3 p10 = frame_local_pos(component, 1.0f, 0.0f);
                const glm::vec3 p11 = frame_local_pos(component, 1.0f, 1.0f);
                const glm::vec3 p01 = frame_local_pos(component, 0.0f, 1.0f);

                renderer.push_states(ShapeRendering::Color4u{ component.tint_abgr });
                renderer.push_simple_line(p00, p10);
                renderer.push_simple_line(p10, p11);
                renderer.push_simple_line(p11, p01);
                renderer.push_simple_line(p01, p00);
                renderer.pop_states<ShapeRendering::Color4u>();
            }

            const bool draw_velocity =
                component.debug_draw_velocity
                || component.render_mode == FluidFrameRenderMode::VelocityGlyphs;
            if (draw_velocity)
            {
                const int step_x = std::max(3, nx / 10);
                const int step_y = std::max(3, ny / 8);
                const float cell_width = component.frame_size.x / static_cast<float>(nx);
                const float cell_height = component.frame_size.y / static_cast<float>(ny);
                const float max_glyph_length = 1.75f * std::max(cell_width, cell_height);

                for (int y = step_y / 2; y < ny; y += step_y)
                {
                    for (int x = step_x / 2; x < nx; x += step_x)
                    {
                        const std::size_t idx = center_index(x, y, nx);
                        if (runtime.obstacle_mask[idx] != 0u)
                            continue;

                        const glm::vec2 velocity = sample_velocity_field(
                            runtime,
                            glm::vec2(static_cast<float>(x), static_cast<float>(y)));
                        const float speed_sq = glm::dot(velocity, velocity);
                        if (speed_sq <= 2.5e-3f)
                            continue;

                        const float speed = std::sqrt(speed_sq);
                        const glm::vec2 direction = velocity / speed;
                        const float glyph_length = std::min(
                            speed * component.velocity_glyph_scale,
                            max_glyph_length);
                        const float glyph_alpha = glm::mix(0.35f, 0.95f, glm::clamp(speed * 0.5f, 0.0f, 1.0f));

                        const glm::vec3 start = frame_local_pos(
                            component,
                            (static_cast<float>(x) + 0.5f) / static_cast<float>(nx),
                            (static_cast<float>(y) + 0.5f) / static_cast<float>(ny));
                        const glm::vec3 end = start + glm::vec3(
                            direction.x * glyph_length,
                            direction.y * glyph_length,
                            0.0f);

                        renderer.push_states(ShapeRendering::Color4u{
                            pack_abgr(glm::vec4(0.94f, 0.83f, 0.22f, glyph_alpha))
                        });
                        renderer.push_simple_line(start, end);
                        renderer.pop_states<ShapeRendering::Color4u>();
                    }
                }
            }

            if (component.debug_draw_obstacle_faces)
            {
                for (int y = 0; y < ny; ++y)
                {
                    for (int x = 0; x <= nx; ++x)
                    {
                        const FaceState state = u_face_state_at(runtime, x, y);
                        const std::uint32_t color = face_state_color(state);
                        if (color == 0u || state == FaceState::SolidInterior)
                            continue;

                        const glm::vec3 start = frame_local_pos(
                            component,
                            static_cast<float>(x) / static_cast<float>(nx),
                            static_cast<float>(y) / static_cast<float>(ny));
                        const glm::vec3 end = frame_local_pos(
                            component,
                            static_cast<float>(x) / static_cast<float>(nx),
                            static_cast<float>(y + 1) / static_cast<float>(ny));
                        renderer.push_states(ShapeRendering::Color4u{ color });
                        renderer.push_simple_line(start, end);
                        renderer.pop_states<ShapeRendering::Color4u>();
                    }
                }

                for (int y = 0; y <= ny; ++y)
                {
                    for (int x = 0; x < nx; ++x)
                    {
                        const FaceState state = v_face_state_at(runtime, x, y);
                        const std::uint32_t color = face_state_color(state);
                        if (color == 0u || state == FaceState::SolidInterior)
                            continue;

                        const glm::vec3 start = frame_local_pos(
                            component,
                            static_cast<float>(x) / static_cast<float>(nx),
                            static_cast<float>(y) / static_cast<float>(ny));
                        const glm::vec3 end = frame_local_pos(
                            component,
                            static_cast<float>(x + 1) / static_cast<float>(nx),
                            static_cast<float>(y) / static_cast<float>(ny));
                        renderer.push_states(ShapeRendering::Color4u{ color });
                        renderer.push_simple_line(start, end);
                        renderer.pop_states<ShapeRendering::Color4u>();
                    }
                }
            }

            renderer.pop_states<ShapeRendering::DepthTest, ShapeRendering::BackfaceCull, glm::mat4>();
        }
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

        const bool resized = resize_runtime(runtime, component.resolution);

        bool config_changed = false;
        const std::filesystem::path config_path(component.config_path);
        std::error_code ec;
        const bool path_exists = std::filesystem::exists(config_path, ec);
        const bool path_changed = runtime.loaded_config_path != config_path;
        ec.clear();
        const auto write_time = path_exists
            ? std::filesystem::last_write_time(config_path, ec)
            : std::filesystem::file_time_type{};
        const bool write_time_valid = !ec;
        const bool write_time_changed = write_time_valid && runtime.loaded_write_time != write_time;
        if (path_changed || write_time_changed)
        {
            Config config{};
            if (load_config_from_file(config_path, config))
            {
                runtime.config = std::move(config);
                runtime.cell_size = runtime.config.cell_size;
                runtime.viscosity = runtime.config.viscosity;
                runtime.pressure_iterations = runtime.config.pressure_iterations;
                runtime.loaded_write_time = write_time_valid ? write_time : std::filesystem::file_time_type{};
                config_changed = true;
            }

            runtime.loaded_config_path = config_path;
        }

        runtime.velocity_damping = component.apply_velocity_damping
            ? (component.override_velocity_damping ? component.velocity_damping : runtime.config.velocity_damping)
            : 0.0f;
        runtime.density_damping = component.apply_density_damping
            ? (component.override_density_damping ? component.density_damping : runtime.config.density_damping)
            : 0.0f;

        if (resized || config_changed)
            rebuild_solid_mask(runtime);

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
            // 4) Re-impose wall/obstacle boundary conditions on the intermediate velocity
            // 5) Compute divergence of the intermediate velocity u*
            // 6) Solve pressure Poisson equation
            // 7) Project u* -> u^{n+1} so div(u^{n+1}) = 0
            // 8) Re-apply wall/obstacle boundary conditions after projection
            // 9) Advect density using the projected velocity
            // 10) Re-apply wall/obstacle boundary conditions once more so
            //     density is cleared inside solids and face velocities stay valid
            apply_emitters(component, runtime, step_dt);
            advect_velocity(runtime, step_dt);
            diffuse_velocity(runtime, step_dt);
            apply_boundary_conditions(runtime);
            compute_divergence(runtime, step_dt);
            solve_pressure(runtime, step_dt);
            project_velocity(runtime, step_dt);
            apply_boundary_conditions(runtime);
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
                        runtime.density[idx] = glm::clamp(runtime.density[idx], 0.0f, 2.5f);
                    }
                }
            },
            threaded_used);

        compute_vorticity(runtime);
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

    bool FluidFrameSystem::resize_runtime(Runtime& runtime, glm::ivec2 resolution)
    {
        resolution.x = std::max(1, resolution.x);
        resolution.y = std::max(1, resolution.y);
        if (runtime.resolution == resolution)
            return false;

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
        runtime.vorticity.assign(runtime.pressure.size(), 0.0f);
        runtime.density.assign(runtime.pressure.size(), 0.0f);
        runtime.density_tmp.assign(runtime.pressure.size(), 0.0f);
        runtime.obstacle_mask.assign(runtime.pressure.size(), 0u);
        runtime.obstacle_boundary_modes.assign(runtime.pressure.size(), 0u);
        runtime.u_face_states.assign(runtime.u.size(), static_cast<std::uint8_t>(FaceState::Fluid));
        runtime.v_face_states.assign(runtime.v.size(), static_cast<std::uint8_t>(FaceState::Fluid));
        return true;
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
        std::fill(runtime.obstacle_mask.begin(), runtime.obstacle_mask.end(), 0u);
        std::fill(runtime.obstacle_boundary_modes.begin(), runtime.obstacle_boundary_modes.end(), 0u);
        std::fill(runtime.u_face_states.begin(), runtime.u_face_states.end(), static_cast<std::uint8_t>(FaceState::Fluid));
        std::fill(runtime.v_face_states.begin(), runtime.v_face_states.end(), static_cast<std::uint8_t>(FaceState::Fluid));

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
                        const std::size_t idx = center_index(x, y, nx);
                        runtime.obstacle_mask[idx] = 1u;
                        runtime.obstacle_boundary_modes[idx] = static_cast<std::uint8_t>(obstacle.boundary);
                        break;
                    }
                }
            }
        }

        // Face classification derived from the cell obstacle mask:
        //
        // - Fluid: both adjacent cells are fluid
        // - SolidInterior: both adjacent cells are obstacle cells
        // - SolidBoundary*: exactly one adjacent cell is obstacle
        //
        // This lets the active solver reason about the actual blocked faces
        // rather than only about whole blocked cells.
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x <= nx; ++x)
            {
                const bool left_solid = x > 0 && is_obstacle_cell(runtime, x - 1, y);
                const bool right_solid = x < nx && is_obstacle_cell(runtime, x, y);

                FaceState state = FaceState::Fluid;
                if (left_solid && right_solid)
                {
                    state = FaceState::SolidInterior;
                }
                else if (left_solid || right_solid)
                {
                    const ObstacleBoundaryMode mode = left_solid
                        ? obstacle_boundary_mode_at(runtime, x - 1, y)
                        : obstacle_boundary_mode_at(runtime, x, y);
                    state = mode == ObstacleBoundaryMode::FreeSlip
                        ? FaceState::SolidBoundaryFreeSlip
                        : FaceState::SolidBoundaryNoSlip;
                }

                runtime.u_face_states[u_index(x, y, nx)] = static_cast<std::uint8_t>(state);
            }
        }

        for (int y = 0; y <= ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const bool down_solid = y > 0 && is_obstacle_cell(runtime, x, y - 1);
                const bool up_solid = y < ny && is_obstacle_cell(runtime, x, y);

                FaceState state = FaceState::Fluid;
                if (down_solid && up_solid)
                {
                    state = FaceState::SolidInterior;
                }
                else if (down_solid || up_solid)
                {
                    const ObstacleBoundaryMode mode = down_solid
                        ? obstacle_boundary_mode_at(runtime, x, y - 1)
                        : obstacle_boundary_mode_at(runtime, x, y);
                    state = mode == ObstacleBoundaryMode::FreeSlip
                        ? FaceState::SolidBoundaryFreeSlip
                        : FaceState::SolidBoundaryNoSlip;
                }

                runtime.v_face_states[v_index(x, y, nx)] = static_cast<std::uint8_t>(state);
            }
        }
    }

    bool FluidFrameSystem::is_obstacle_cell(const Runtime& runtime, int x, int y)
    {
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        if (x < 0 || x >= nx || y < 0 || y >= ny)
            return false;
        return runtime.obstacle_mask[center_index(x, y, nx)] != 0u;
    }

    FluidFrameSystem::ObstacleBoundaryMode FluidFrameSystem::obstacle_boundary_mode_at(
        const Runtime& runtime,
        int x,
        int y)
    {
        if (!is_obstacle_cell(runtime, x, y))
            return ObstacleBoundaryMode::NoSlip;

        return static_cast<ObstacleBoundaryMode>(
            runtime.obstacle_boundary_modes[center_index(x, y, runtime.resolution.x)]);
    }

    FluidFrameSystem::FaceState FluidFrameSystem::u_face_state_at(const Runtime& runtime, int x, int y)
    {
        return static_cast<FaceState>(runtime.u_face_states[u_index(x, y, runtime.resolution.x)]);
    }

    FluidFrameSystem::FaceState FluidFrameSystem::v_face_state_at(const Runtime& runtime, int x, int y)
    {
        return static_cast<FaceState>(runtime.v_face_states[v_index(x, y, runtime.resolution.x)]);
    }

    bool FluidFrameSystem::is_fluid_u_face(const Runtime& runtime, int x, int y)
    {
        return u_face_state_at(runtime, x, y) == FaceState::Fluid;
    }

    bool FluidFrameSystem::is_fluid_v_face(const Runtime& runtime, int x, int y)
    {
        return v_face_state_at(runtime, x, y) == FaceState::Fluid;
    }

    bool FluidFrameSystem::point_inside_obstacle_region(const Runtime& runtime, glm::vec2 ij)
    {
        // Shared center-coordinate space:
        //
        // - integer lattice points are cell centers
        // - the cell owning a continuous point is the nearest center cell
        //
        // Rounding gives us that nearest cell-center ownership test directly.
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        const int x = std::clamp(static_cast<int>(std::lround(ij.x)), 0, nx - 1);
        const int y = std::clamp(static_cast<int>(std::lround(ij.y)), 0, ny - 1);
        return is_obstacle_cell(runtime, x, y);
    }

    glm::vec2 FluidFrameSystem::clamp_backtrace_to_fluid(
        const Runtime& runtime,
        glm::vec2 origin_ij,
        glm::vec2 traced_ij)
    {
        const glm::vec2 domain_min(-0.5f, -0.5f);
        const glm::vec2 domain_max(
            static_cast<float>(runtime.resolution.x) - 0.5f,
            static_cast<float>(runtime.resolution.y) - 0.5f);
        traced_ij = glm::clamp(traced_ij, domain_min, domain_max);
        if (!point_inside_obstacle_region(runtime, traced_ij))
            return traced_ij;

        // Semi-Lagrangian backtracing wants the "departure point" to stay on the
        // fluid side of a wall. If the traced point falls inside a solid cell,
        // we binary-search back along the characteristic segment from the origin
        // to the traced point and keep the last fluid-side point we can find.
        float fluid_t = 0.0f;
        float solid_t = 1.0f;
        for (int iter = 0; iter < 10; ++iter)
        {
            const float mid_t = 0.5f * (fluid_t + solid_t);
            const glm::vec2 candidate = glm::mix(origin_ij, traced_ij, mid_t);
            if (point_inside_obstacle_region(runtime, candidate))
                solid_t = mid_t;
            else
                fluid_t = mid_t;
        }

        glm::vec2 safe = glm::mix(origin_ij, traced_ij, fluid_t);
        const glm::vec2 push_dir = origin_ij - safe;
        if (glm::dot(push_dir, push_dir) > 1.0e-8f)
            safe += glm::normalize(push_dir) * 1.0e-3f;
        return glm::clamp(safe, domain_min, domain_max);
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

    float FluidFrameSystem::sample_u_field(
        const std::vector<float>& field,
        int nx,
        int ny,
        glm::vec2 u_ij)
    {
        // u lives on vertical faces, so its logical grid has:
        //
        // - x in [0, nx]
        // - y in [0, ny - 1]
        //
        // We bilinearly interpolate that face-centered field directly.
        const float x = std::clamp(u_ij.x, 0.0f, static_cast<float>(nx));
        const float y = std::clamp(u_ij.y, 0.0f, static_cast<float>(ny - 1));

        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const int x1 = std::min(x0 + 1, nx);
        const int y1 = std::min(y0 + 1, ny - 1);
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);

        const float c00 = field[u_index(x0, y0, nx)];
        const float c10 = field[u_index(x1, y0, nx)];
        const float c01 = field[u_index(x0, y1, nx)];
        const float c11 = field[u_index(x1, y1, nx)];
        const float c0 = glm::mix(c00, c10, tx);
        const float c1 = glm::mix(c01, c11, tx);
        return glm::mix(c0, c1, ty);
    }

    float FluidFrameSystem::sample_v_field(
        const std::vector<float>& field,
        int nx,
        int ny,
        glm::vec2 v_ij)
    {
        // v lives on horizontal faces, so its logical grid has:
        //
        // - x in [0, nx - 1]
        // - y in [0, ny]
        //
        // We again bilinearly interpolate directly on that staggered grid.
        const float x = std::clamp(v_ij.x, 0.0f, static_cast<float>(nx - 1));
        const float y = std::clamp(v_ij.y, 0.0f, static_cast<float>(ny));

        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const int x1 = std::min(x0 + 1, nx - 1);
        const int y1 = std::min(y0 + 1, ny);
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);

        const float c00 = field[v_index(x0, y0, nx)];
        const float c10 = field[v_index(x1, y0, nx)];
        const float c01 = field[v_index(x0, y1, nx)];
        const float c11 = field[v_index(x1, y1, nx)];
        const float c0 = glm::mix(c00, c10, tx);
        const float c1 = glm::mix(c01, c11, tx);
        return glm::mix(c0, c1, ty);
    }

    glm::vec2 FluidFrameSystem::sample_velocity_field_legacy(const Runtime& runtime, glm::vec2 ij)
    {
        // Legacy simplified path:
        //
        // We snap to the nearest cell center and average the four neighboring
        // face velocities. This is easy to read, but it is only a rough
        // approximation of a true MAC-grid sample.
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

    glm::vec2 FluidFrameSystem::sample_velocity_field(const Runtime& runtime, glm::vec2 ij)
    {
        // Active path:
        //
        // Treat ij as a continuous position in cell-center coordinates.
        // If a cell center is at integer (i, j), then:
        //
        // - the matching u sample location is (i + 0.5, j)
        // - the matching v sample location is (i, j + 0.5)
        //
        // This respects the MAC staggering instead of collapsing everything to
        // one nearest cell. It is still lightweight, but much closer to the
        // intended solver math.
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        return glm::vec2(
            sample_u_field(runtime.u, nx, ny, glm::vec2(ij.x + 0.5f, ij.y)),
            sample_v_field(runtime.v, nx, ny, glm::vec2(ij.x, ij.y + 0.5f)));
    }

    void FluidFrameSystem::apply_emitters(Runtime& runtime, float dt)
    {
        apply_emitters(FluidFrameComponent{}, runtime, dt);
    }

    void FluidFrameSystem::apply_emitters(const FluidFrameComponent& component, Runtime& runtime, float dt)
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
                        if (!component.emit_density)
                            continue;
                        if (runtime.obstacle_mask[idx] != 0u)
                            continue;
                        runtime.density[idx] += emitter.amount * component.density_emitter_scale * dt;
                    }
                    else
                    {
                        if (!component.emit_velocity)
                            continue;
                        // Active path:
                        //
                        // Only inject momentum into faces that are actually open.
                        // Otherwise the source keeps pumping into blocked faces,
                        // and the boundary pass immediately deletes that motion.
                        if (is_fluid_u_face(runtime, x, y))
                            runtime.u[u_index(x, y, nx)] += emitter.value.x * component.velocity_emitter_scale * dt;
                        if (is_fluid_u_face(runtime, x + 1, y))
                            runtime.u[u_index(x + 1, y, nx)] += emitter.value.x * component.velocity_emitter_scale * dt;
                        if (is_fluid_v_face(runtime, x, y))
                            runtime.v[v_index(x, y, nx)] += emitter.value.y * component.velocity_emitter_scale * dt;
                        if (is_fluid_v_face(runtime, x, y + 1))
                            runtime.v[v_index(x, y + 1, nx)] += emitter.value.y * component.velocity_emitter_scale * dt;
                    }
                }
            }
        }
    }

    void FluidFrameSystem::advect_velocity_legacy(Runtime& runtime)
    {
        // Legacy simplified path:
        //
        // Keep the previous velocity unchanged. This was useful as a scaffold for
        // bringing up the rest of the solver pipeline, but it means momentum does
        // not actually travel through the domain.
        runtime.u_tmp = runtime.u;
        runtime.v_tmp = runtime.v;
        runtime.u.swap(runtime.u_tmp);
        runtime.v.swap(runtime.v_tmp);
    }

    void FluidFrameSystem::advect_velocity(Runtime& runtime, float dt)
    {
        // Intended equation:
        //
        //     du/dt + (u · grad)u = 0
        //
        // A standard real-time choice is semi-Lagrangian advection:
        //
        //     u^{n+1}(x) = u^n(x - dt * u^n(x))
        //
        // On a MAC grid, u and v live on different staggered face grids, so we
        // advect each face field at its own face positions.
        //
        // We keep the legacy no-op branch in the file for comparison/documentation,
        // but the active branch below performs a real backtrace on the face grids.
        constexpr bool kUseLegacyVelocityAdvection = false;
        if (kUseLegacyVelocityAdvection)
        {
            advect_velocity_legacy(runtime);
            return;
        }
        //
        // Important unit conversion:
        //
        // - face positions are tracked in "cell units"
        // - physical velocity is in world units / second
        // - one cell has width h = cell_size
        //
        // So dt * velocity / h tells us how many cells to move backward.
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        const float inv_h = 1.0f / std::max(1.0e-6f, runtime.cell_size);

        // Start from the old fields; write advected values into the temporary
        // buffers so each sample sees the previous state consistently.
        runtime.u_tmp = runtime.u;
        runtime.v_tmp = runtime.v;

        // Advect u, which lives on vertical faces.
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x <= nx; ++x)
            {
                if (!is_fluid_u_face(runtime, x, y))
                {
                    runtime.u_tmp[u_index(x, y, nx)] = 0.0f;
                    continue;
                }

                // In cell-center coordinates, a u-face at index (x, y) sits at
                // (x - 0.5, y). The x offset is what maps the staggered u grid
                // back into the shared center-coordinate space.
                const glm::vec2 face_pos(
                    static_cast<float>(x) - 0.5f,
                    static_cast<float>(y));

                // Recover the full 2D velocity at that face location. The u
                // component comes from the u field itself; the v component is
                // interpolated from the staggered v field.
                const glm::vec2 velocity = sample_velocity_field(runtime, face_pos);

                // Semi-Lagrangian step: trace backward from the current face
                // position to the location where this fluid parcel came from.
                //
                // Near obstacles, the raw departure point can land inside a
                // blocked cell. Clamp it back to the last fluid-side point along
                // the traced characteristic so momentum gets redirected around
                // the wall instead of disappearing into it.
                constexpr bool kUseObstacleAwareBacktraceClamp = false;
                const glm::vec2 raw_backtraced = face_pos - dt * inv_h * velocity;
                const glm::vec2 backtraced = kUseObstacleAwareBacktraceClamp
                    ? clamp_backtrace_to_fluid(runtime, face_pos, raw_backtraced)
                    : raw_backtraced;

                // Convert the backtraced point from shared center coordinates back
                // into u-face coordinates and sample the previous u field there.
                runtime.u_tmp[u_index(x, y, nx)] = sample_u_field(
                    runtime.u,
                    nx,
                    ny,
                    glm::vec2(backtraced.x + 0.5f, backtraced.y));
            }
        }

        // Advect v, which lives on horizontal faces.
        for (int y = 0; y <= ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                if (!is_fluid_v_face(runtime, x, y))
                {
                    runtime.v_tmp[v_index(x, y, nx)] = 0.0f;
                    continue;
                }

                // In center coordinates, a v-face at index (x, y) sits at
                // (x, y - 0.5). The y offset accounts for the staggered v grid.
                const glm::vec2 face_pos(
                    static_cast<float>(x),
                    static_cast<float>(y) - 0.5f);

                const glm::vec2 velocity = sample_velocity_field(runtime, face_pos);
                constexpr bool kUseObstacleAwareBacktraceClamp = false;
                const glm::vec2 raw_backtraced = face_pos - dt * inv_h * velocity;
                const glm::vec2 backtraced = kUseObstacleAwareBacktraceClamp
                    ? clamp_backtrace_to_fluid(runtime, face_pos, raw_backtraced)
                    : raw_backtraced;

                runtime.v_tmp[v_index(x, y, nx)] = sample_v_field(
                    runtime.v,
                    nx,
                    ny,
                    glm::vec2(backtraced.x, backtraced.y + 0.5f));
            }
        }

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
                if (runtime.obstacle_mask[idx] != 0u)
                {
                    runtime.divergence[idx] = 0.0f;
                    continue;
                }

                // Use the face values directly here. The obstacle cells
                // themselves are still excluded, but this keeps the active path
                // closer to the earlier behavior that produced a more visible
                // interaction around the box.
                const float u_left = runtime.u[u_index(x, y, nx)];
                const float u_right = runtime.u[u_index(x + 1, y, nx)];
                const float v_down = runtime.v[v_index(x, y, nx)];
                const float v_up = runtime.v[v_index(x, y + 1, nx)];
                runtime.divergence[idx] = inv_dt * inv_h * ((u_right - u_left) + (v_up - v_down));
            }
        }
    }

    void FluidFrameSystem::compute_vorticity(Runtime& runtime)
    {
        // In 2D, the curl of the velocity field reduces to a scalar:
        //
        //     w = dv/dx - du/dy
        //
        // We evaluate it at cell centers using centered finite differences.
        // sample_velocity_field() already hides the staggered MAC details, so
        // the math here reads like the textbook cell-centered formula.
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        const float inv_2h = 0.5f / std::max(1.0e-6f, runtime.cell_size);

        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                const std::size_t idx = center_index(x, y, nx);
                if (runtime.obstacle_mask[idx] != 0u)
                {
                    runtime.vorticity[idx] = 0.0f;
                    continue;
                }

                const glm::vec2 velocity_left = sample_velocity_field(
                    runtime,
                    glm::vec2(static_cast<float>(x - 1), static_cast<float>(y)));
                const glm::vec2 velocity_right = sample_velocity_field(
                    runtime,
                    glm::vec2(static_cast<float>(x + 1), static_cast<float>(y)));
                const glm::vec2 velocity_down = sample_velocity_field(
                    runtime,
                    glm::vec2(static_cast<float>(x), static_cast<float>(y - 1)));
                const glm::vec2 velocity_up = sample_velocity_field(
                    runtime,
                    glm::vec2(static_cast<float>(x), static_cast<float>(y + 1)));

                // Centered differences:
                //
                //     dv/dx ~= (v(x + h) - v(x - h)) / (2h)
                //     du/dy ~= (u(y + h) - u(y - h)) / (2h)
                const float dv_dx = (velocity_right.y - velocity_left.y) * inv_2h;
                const float du_dy = (velocity_up.x - velocity_down.x) * inv_2h;
                runtime.vorticity[idx] = dv_dx - du_dy;
            }
        }
    }

    void FluidFrameSystem::solve_pressure_legacy(Runtime& runtime, float dt)
    {
        // Legacy simplified path:
        //
        // Clamp pressure lookups to the domain and treat obstacle cells as hard
        // zero pressure. This is easy to follow, but it gives solid neighbors a
        // less physical role than a proper zero-normal-gradient wall treatment.
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
                    if (runtime.obstacle_mask[idx] != 0u)
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

    void FluidFrameSystem::solve_pressure(Runtime& runtime, float dt)
    {
        constexpr bool kUseLegacyPressureSolve = false;
        if (kUseLegacyPressureSolve)
        {
            solve_pressure_legacy(runtime, dt);
            return;
        }

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
        //
        // Active path:
        //
        // At fluid-solid boundaries we approximate:
        //
        //     dp/dn = 0
        //
        // by reusing the current cell pressure whenever a Jacobi stencil would
        // otherwise sample outside the fluid region. That is the discrete "ghost
        // pressure equals interior pressure" form of a zero normal derivative.
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
                    if (runtime.obstacle_mask[idx] != 0u)
                    {
                        runtime.pressure_tmp[idx] = 0.0f;
                        continue;
                    }

                    float neighbor_sum = 0.0f;
                    int neighbor_count = 0;
                    const float p_center = runtime.pressure[idx];

                    const auto accumulate_neighbor_pressure = [&](int nx_cell, int ny_cell)
                    {
                        if (nx_cell >= 0
                            && nx_cell < nx
                            && ny_cell >= 0
                            && ny_cell < ny
                            && !is_obstacle_cell(runtime, nx_cell, ny_cell))
                        {
                            neighbor_sum += runtime.pressure[center_index(nx_cell, ny_cell, nx)];
                        }
                        else
                        {
                            neighbor_sum += p_center;
                        }
                        ++neighbor_count;
                    };

                    accumulate_neighbor_pressure(x - 1, y);
                    accumulate_neighbor_pressure(x + 1, y);
                    accumulate_neighbor_pressure(x, y - 1);
                    accumulate_neighbor_pressure(x, y + 1);

                    const float rhs = runtime.divergence[idx] * dt;
                    runtime.pressure_tmp[idx] = neighbor_count > 0
                        ? (neighbor_sum - h2 * rhs) / static_cast<float>(neighbor_count)
                        : 0.0f;
                }
            }
            runtime.pressure.swap(runtime.pressure_tmp);
        }
    }

    void FluidFrameSystem::project_velocity_legacy(Runtime& runtime, float dt)
    {
        // Legacy simplified path:
        //
        // Subtract pressure differences on all interior faces without checking
        // whether those faces lie between fluid and solid cells.
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

    void FluidFrameSystem::project_velocity(Runtime& runtime, float dt)
    {
        constexpr bool kUseLegacyProjection = false;
        if (kUseLegacyProjection)
        {
            project_velocity_legacy(runtime, dt);
            return;
        }

        // Velocity projection:
        //
        //     u^{n+1} = u* - dt grad(p)
        //
        // On a MAC grid, each face velocity is corrected by the pressure difference
        // across the two cells that share the face.
        //
        // Active path:
        //
        // If a face separates fluid from solid, that face is a wall-normal
        // velocity component and must be zero rather than pressure-corrected
        // through the obstacle.
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        const float inv_h = 1.0f / std::max(1.0e-6f, runtime.cell_size);

        for (int y = 0; y < ny; ++y)
        {
            for (int x = 1; x < nx; ++x)
            {
                if (is_obstacle_cell(runtime, x - 1, y) || is_obstacle_cell(runtime, x, y))
                {
                    runtime.u[u_index(x, y, nx)] = 0.0f;
                    continue;
                }

                const float p_left = runtime.pressure[center_index(x - 1, y, nx)];
                const float p_right = runtime.pressure[center_index(x, y, nx)];
                runtime.u[u_index(x, y, nx)] -= dt * inv_h * (p_right - p_left);
            }
        }

        for (int y = 1; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                if (is_obstacle_cell(runtime, x, y - 1) || is_obstacle_cell(runtime, x, y))
                {
                    runtime.v[v_index(x, y, nx)] = 0.0f;
                    continue;
                }

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
                if (runtime.obstacle_mask[center_index(x, y, nx)] != 0u)
                {
                    runtime.density_tmp[center_index(x, y, nx)] = 0.0f;
                    continue;
                }

                const glm::vec2 cell_ij(static_cast<float>(x), static_cast<float>(y));
                const glm::vec2 velocity = sample_velocity_field(runtime, cell_ij);
                constexpr bool kUseObstacleAwareBacktraceClamp = false;
                const glm::vec2 raw_backtraced = cell_ij - (dt / std::max(1.0e-6f, runtime.cell_size)) * velocity;
                const glm::vec2 backtraced = kUseObstacleAwareBacktraceClamp
                    ? clamp_backtrace_to_fluid(runtime, cell_ij, raw_backtraced)
                    : raw_backtraced;
                runtime.density_tmp[center_index(x, y, nx)] = sample_center_field(runtime.density, nx, ny, backtraced);
            }
        }
        runtime.density.swap(runtime.density_tmp);
    }

    void FluidFrameSystem::apply_boundary_conditions_legacy(Runtime& runtime)
    {
        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;

        // Domain edges. The legacy path now still honors Dirichlet vs Neumann
        // for the normal velocity components so it remains useful as a reference.
        for (int y = 0; y < ny; ++y)
        {
            runtime.u[u_index(0, y, nx)] = runtime.config.boundaries.left.velocity.type == ScalarBoundaryType::Dirichlet
                ? runtime.config.boundaries.left.velocity.value.x
                : (nx > 0
                    ? runtime.u[u_index(std::min(1, nx), y, nx)] + runtime.cell_size * runtime.config.boundaries.left.velocity.value.x
                    : runtime.config.boundaries.left.velocity.value.x);
            runtime.u[u_index(nx, y, nx)] = runtime.config.boundaries.right.velocity.type == ScalarBoundaryType::Dirichlet
                ? runtime.config.boundaries.right.velocity.value.x
                : (nx > 0
                    ? runtime.u[u_index(std::max(nx - 1, 0), y, nx)] + runtime.cell_size * runtime.config.boundaries.right.velocity.value.x
                    : runtime.config.boundaries.right.velocity.value.x);
        }
        for (int x = 0; x < nx; ++x)
        {
            runtime.v[v_index(x, 0, nx)] = runtime.config.boundaries.bottom.velocity.type == ScalarBoundaryType::Dirichlet
                ? runtime.config.boundaries.bottom.velocity.value.y
                : (ny > 0
                    ? runtime.v[v_index(x, std::min(1, ny), nx)] + runtime.cell_size * runtime.config.boundaries.bottom.velocity.value.y
                    : runtime.config.boundaries.bottom.velocity.value.y);
            runtime.v[v_index(x, ny, nx)] = runtime.config.boundaries.top.velocity.type == ScalarBoundaryType::Dirichlet
                ? runtime.config.boundaries.top.velocity.value.y
                : (ny > 0
                    ? runtime.v[v_index(x, std::max(ny - 1, 0), nx)] + runtime.cell_size * runtime.config.boundaries.top.velocity.value.y
                    : runtime.config.boundaries.top.velocity.value.y);
        }

        // Embedded obstacles. For the sketch we zero the nearby face velocities.
        // A more complete version would distinguish no-slip vs free-slip here.
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                if (runtime.obstacle_mask[center_index(x, y, nx)] == 0u)
                    continue;

                runtime.density[center_index(x, y, nx)] = 0.0f;
                runtime.u[u_index(x, y, nx)] = 0.0f;
                runtime.u[u_index(x + 1, y, nx)] = 0.0f;
                runtime.v[v_index(x, y, nx)] = 0.0f;
                runtime.v[v_index(x, y + 1, nx)] = 0.0f;
            }
        }
    }

    void FluidFrameSystem::apply_boundary_conditions(Runtime& runtime)
    {
        constexpr bool kUseLegacyBoundaryConditions = false;
        if (kUseLegacyBoundaryConditions)
        {
            apply_boundary_conditions_legacy(runtime);
            return;
        }

        const int nx = runtime.resolution.x;
        const int ny = runtime.resolution.y;
        const float h = runtime.cell_size;

        // Domain edges:
        //
        // The MAC grid stores the normal velocity components directly on the
        // boundary faces, so we can enforce those values in place.
        //
        // - Dirichlet: hard-set the boundary face value
        // - Neumann: copy the nearest interior face and add h * (d/dn value)
        //
        // For the current default outflow this means the right edge uses zero
        // normal derivative, so the downstream face simply follows the interior
        // face instead of being pinned to zero.
        for (int y = 0; y < ny; ++y)
        {
            runtime.u[u_index(0, y, nx)] = runtime.config.boundaries.left.velocity.type == ScalarBoundaryType::Dirichlet
                ? runtime.config.boundaries.left.velocity.value.x
                : (nx > 0
                    ? runtime.u[u_index(std::min(1, nx), y, nx)] + h * runtime.config.boundaries.left.velocity.value.x
                    : runtime.config.boundaries.left.velocity.value.x);
            runtime.u[u_index(nx, y, nx)] = runtime.config.boundaries.right.velocity.type == ScalarBoundaryType::Dirichlet
                ? runtime.config.boundaries.right.velocity.value.x
                : (nx > 0
                    ? runtime.u[u_index(std::max(nx - 1, 0), y, nx)] + h * runtime.config.boundaries.right.velocity.value.x
                    : runtime.config.boundaries.right.velocity.value.x);
        }
        for (int x = 0; x < nx; ++x)
        {
            runtime.v[v_index(x, 0, nx)] = runtime.config.boundaries.bottom.velocity.type == ScalarBoundaryType::Dirichlet
                ? runtime.config.boundaries.bottom.velocity.value.y
                : (ny > 0
                    ? runtime.v[v_index(x, std::min(1, ny), nx)] + h * runtime.config.boundaries.bottom.velocity.value.y
                    : runtime.config.boundaries.bottom.velocity.value.y);
            runtime.v[v_index(x, ny, nx)] = runtime.config.boundaries.top.velocity.type == ScalarBoundaryType::Dirichlet
                ? runtime.config.boundaries.top.velocity.value.y
                : (ny > 0
                    ? runtime.v[v_index(x, std::max(ny - 1, 0), nx)] + h * runtime.config.boundaries.top.velocity.value.y
                    : runtime.config.boundaries.top.velocity.value.y);
        }

        // Density edge conditions:
        //
        // - Dirichlet: write the configured boundary value directly
        // - Neumann: copy the nearest interior value plus the requested outward
        //   derivative scaled by h
        if (nx > 1)
        {
            for (int y = 0; y < ny; ++y)
            {
                const std::size_t left_idx = center_index(0, y, nx);
                const std::size_t left_interior_idx = center_index(1, y, nx);
                runtime.density[left_idx] = runtime.config.boundaries.left.density.type == ScalarBoundaryType::Dirichlet
                    ? runtime.config.boundaries.left.density.value
                    : (runtime.density[left_interior_idx] + h * runtime.config.boundaries.left.density.value);

                const std::size_t right_idx = center_index(nx - 1, y, nx);
                const std::size_t right_interior_idx = center_index(nx - 2, y, nx);
                runtime.density[right_idx] = runtime.config.boundaries.right.density.type == ScalarBoundaryType::Dirichlet
                    ? runtime.config.boundaries.right.density.value
                    : (runtime.density[right_interior_idx] + h * runtime.config.boundaries.right.density.value);
            }
        }
        if (ny > 1)
        {
            for (int x = 0; x < nx; ++x)
            {
                const std::size_t bottom_idx = center_index(x, 0, nx);
                const std::size_t bottom_interior_idx = center_index(x, 1, nx);
                runtime.density[bottom_idx] = runtime.config.boundaries.bottom.density.type == ScalarBoundaryType::Dirichlet
                    ? runtime.config.boundaries.bottom.density.value
                    : (runtime.density[bottom_interior_idx] + h * runtime.config.boundaries.bottom.density.value);

                const std::size_t top_idx = center_index(x, ny - 1, nx);
                const std::size_t top_interior_idx = center_index(x, ny - 2, nx);
                runtime.density[top_idx] = runtime.config.boundaries.top.density.type == ScalarBoundaryType::Dirichlet
                    ? runtime.config.boundaries.top.density.value
                    : (runtime.density[top_interior_idx] + h * runtime.config.boundaries.top.density.value);
            }
        }

        // Embedded obstacles:
        //
        // Keep the older cell-centered enforcement for the active path for now.
        // The explicit face masks remain available for visualization/debugging,
        // but this obstacle pass was visually more active before the stronger
        // face-zeroing step was introduced.
        for (int y = 0; y < ny; ++y)
        {
            for (int x = 0; x < nx; ++x)
            {
                if (!is_obstacle_cell(runtime, x, y))
                    continue;

                runtime.density[center_index(x, y, nx)] = 0.0f;
                const ObstacleBoundaryMode mode = obstacle_boundary_mode_at(runtime, x, y);
                if (mode == ObstacleBoundaryMode::NoSlip)
                {
                    runtime.u[u_index(x, y, nx)] = 0.0f;
                    runtime.u[u_index(x + 1, y, nx)] = 0.0f;
                    runtime.v[v_index(x, y, nx)] = 0.0f;
                    runtime.v[v_index(x, y + 1, nx)] = 0.0f;
                    continue;
                }

                if (x == 0 || !is_obstacle_cell(runtime, x - 1, y))
                    runtime.u[u_index(x, y, nx)] = 0.0f;
                if (x == nx - 1 || !is_obstacle_cell(runtime, x + 1, y))
                    runtime.u[u_index(x + 1, y, nx)] = 0.0f;
                if (y == 0 || !is_obstacle_cell(runtime, x, y - 1))
                    runtime.v[v_index(x, y, nx)] = 0.0f;
                if (y == ny - 1 || !is_obstacle_cell(runtime, x, y + 1))
                    runtime.v[v_index(x, y + 1, nx)] = 0.0f;
            }
        }
    }
}
