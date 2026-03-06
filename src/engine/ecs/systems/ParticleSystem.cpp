// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/ParticleSystem.hpp"

#include "EngineContext.hpp"
#include "ecs/ParticleEmitterComponent.hpp"
#include "ecs/TransformComponent.hpp"
#include "ecs/systems/PhysicsSystem.hpp"
#include "util/ThreadPool.hpp"

#include <algorithm>
#include <cmath>
#include <future>
#include <unordered_set>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

namespace eeng::ecs::systems
{
    namespace
    {
        struct EmitterUpdateItem
        {
            ecs::ParticleEmitterComponent emitter{};
            glm::mat4 world_matrix{ 1.0f };
            std::uint32_t requested_burst = 0u;
            ParticleSystem::EmitterRuntime* runtime = nullptr;
        };

        std::uint32_t seed_from_entity(entt::entity entity)
        {
            const auto raw = static_cast<std::uint32_t>(entt::to_integral(entity));
            return raw != 0u ? (raw ^ 0x9e3779b9u) : 0x9e3779b9u;
        }

        std::uint32_t xorshift32(std::uint32_t& state)
        {
            if (state == 0u)
                state = 0x9e3779b9u;
            state ^= state << 13u;
            state ^= state >> 17u;
            state ^= state << 5u;
            return state;
        }

        float random_unit(std::uint32_t& state)
        {
            constexpr float inv = 1.0f / static_cast<float>(0xFFFFFFFFu);
            return static_cast<float>(xorshift32(state)) * inv;
        }

        float random_range(std::uint32_t& state, float min_v, float max_v)
        {
            if (max_v < min_v)
                std::swap(min_v, max_v);
            return min_v + (max_v - min_v) * random_unit(state);
        }

        std::uint8_t color_channel(std::uint32_t color, std::uint32_t shift)
        {
            return static_cast<std::uint8_t>((color >> shift) & 0xffu);
        }

        std::uint32_t pack_color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
        {
            return static_cast<std::uint32_t>(r)
                | (static_cast<std::uint32_t>(g) << 8u)
                | (static_cast<std::uint32_t>(b) << 16u)
                | (static_cast<std::uint32_t>(a) << 24u);
        }

        std::uint32_t lerp_color(std::uint32_t a, std::uint32_t b, float t)
        {
            t = std::clamp(t, 0.0f, 1.0f);
            const auto lerp_channel = [t](std::uint8_t c0, std::uint8_t c1)
            {
                const float value = static_cast<float>(c0) + (static_cast<float>(c1) - static_cast<float>(c0)) * t;
                return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
            };

            const std::uint8_t r = lerp_channel(color_channel(a, 0u), color_channel(b, 0u));
            const std::uint8_t g = lerp_channel(color_channel(a, 8u), color_channel(b, 8u));
            const std::uint8_t bl = lerp_channel(color_channel(a, 16u), color_channel(b, 16u));
            const std::uint8_t al = lerp_channel(color_channel(a, 24u), color_channel(b, 24u));
            return pack_color(r, g, bl, al);
        }

        glm::vec3 safe_normalize(glm::vec3 v, const glm::vec3& fallback)
        {
            const float len_sq = glm::dot(v, v);
            if (len_sq <= 1.0e-10f)
                return fallback;
            return v * glm::inversesqrt(len_sq);
        }

        glm::vec3 sample_cone_direction(glm::vec3 axis, float max_angle_rad, std::uint32_t& rng_state)
        {
            axis = safe_normalize(axis, glm::vec3(0.0f, 1.0f, 0.0f));
            const float clamped_angle = std::clamp(max_angle_rad, 0.0f, glm::pi<float>());
            const float cos_max = std::cos(clamped_angle);
            const float z = glm::mix(cos_max, 1.0f, random_unit(rng_state));
            const float phi = random_unit(rng_state) * glm::two_pi<float>();
            const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));

            const glm::vec3 helper = std::abs(axis.y) > 0.98f
                ? glm::vec3(1.0f, 0.0f, 0.0f)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 tangent = safe_normalize(glm::cross(helper, axis), glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::vec3 bitangent = safe_normalize(glm::cross(axis, tangent), glm::vec3(0.0f, 0.0f, 1.0f));

            const glm::vec3 local =
                tangent * (r * std::cos(phi))
                + bitangent * (r * std::sin(phi))
                + axis * z;
            return safe_normalize(local, axis);
        }

        glm::vec3 emitter_world_position(const glm::mat4& world_matrix, const glm::vec3& local_offset)
        {
            return glm::vec3(world_matrix * glm::vec4(local_offset, 1.0f));
        }

        glm::vec3 emitter_world_direction(const glm::mat4& world_matrix, const glm::vec3& local_direction)
        {
            const glm::vec3 world_dir = glm::vec3(world_matrix * glm::vec4(local_direction, 0.0f));
            return safe_normalize(world_dir, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        std::uint32_t next_particle_capacity(std::uint32_t max_particles)
        {
            return std::clamp(max_particles, 1u, 200000u);
        }

        bool can_emit_now(
            const ecs::ParticleEmitterComponent& emitter,
            ParticleSystem::EmitterRuntime& runtime,
            float delta_time)
        {
            if (!emitter.enabled || !emitter.emitting)
                return false;

            if (emitter.duration <= 0.0f)
                return true;

            runtime.duration_elapsed += std::max(0.0f, delta_time);
            if (emitter.looping)
            {
                if (runtime.duration_elapsed > emitter.duration)
                    runtime.duration_elapsed = std::fmod(runtime.duration_elapsed, emitter.duration);
                return true;
            }

            return runtime.duration_elapsed <= emitter.duration;
        }

        void update_emitter_particles(
            const EmitterUpdateItem& item,
            float delta_time,
            const PhysicsSystem* physics_system)
        {
            auto& runtime = *item.runtime;
            runtime.render_particles.clear();

            const auto& emitter = item.emitter;
            if (!emitter.enabled)
                return;

            const std::uint32_t capacity = next_particle_capacity(emitter.max_particles);
            if (runtime.particles.size() > capacity)
                runtime.particles.resize(static_cast<std::size_t>(capacity));
            if (runtime.particles.capacity() < capacity)
                runtime.particles.reserve(static_cast<std::size_t>(capacity));

            const bool should_emit = can_emit_now(emitter, runtime, delta_time);
            if (should_emit)
            {
                runtime.spawn_accumulator += std::max(0.0f, emitter.spawn_rate) * std::max(0.0f, delta_time);
                int spawn_count = static_cast<int>(runtime.spawn_accumulator);
                runtime.spawn_accumulator -= static_cast<float>(spawn_count);
                spawn_count += static_cast<int>(item.requested_burst);

                const glm::vec3 spawn_position = emitter_world_position(item.world_matrix, emitter.local_offset);
                const glm::vec3 direction = emitter_world_direction(item.world_matrix, emitter.local_direction);
                const float spread_rad = glm::radians(std::max(0.0f, emitter.spread_angle_deg));
                const float lifetime_min = std::max(0.01f, emitter.lifetime_min);
                const float lifetime_max = std::max(lifetime_min, emitter.lifetime_max);
                const float speed_min = std::max(0.0f, emitter.speed_min);
                const float speed_max = std::max(speed_min, emitter.speed_max);

                while (spawn_count-- > 0
                    && runtime.particles.size() < static_cast<std::size_t>(capacity))
                {
                    ParticleSystem::ParticleRuntime particle{};
                    particle.position = spawn_position;
                    const glm::vec3 dir = sample_cone_direction(direction, spread_rad, runtime.rng_state);
                    const float speed = random_range(runtime.rng_state, speed_min, speed_max);
                    particle.velocity = dir * speed;
                    particle.lifetime = random_range(runtime.rng_state, lifetime_min, lifetime_max);
                    particle.size_begin = emitter.size_begin;
                    particle.size_end = emitter.size_end;
                    particle.color_begin = emitter.color_begin;
                    particle.color_end = emitter.color_end;
                    runtime.particles.push_back(particle);
                }
            }

            runtime.render_particles.reserve(runtime.particles.size());
            const float dt = std::max(0.0f, delta_time);
            for (std::size_t i = 0; i < runtime.particles.size();)
            {
                auto& particle = runtime.particles[i];
                particle.age += dt;
                if (particle.age >= particle.lifetime)
                {
                    runtime.particles[i] = runtime.particles.back();
                    runtime.particles.pop_back();
                    continue;
                }

                particle.velocity += emitter.acceleration * dt;
                const float drag_factor = std::max(0.0f, 1.0f - std::max(0.0f, emitter.drag) * dt);
                particle.velocity *= drag_factor;
                const glm::vec3 prev_position = particle.position;
                glm::vec3 next_position = particle.position + particle.velocity * dt;

                if (physics_system
                    && emitter.collision_mode != ecs::ParticleCollisionMode::None)
                {
                    const glm::vec3 segment = next_position - prev_position;
                    const float segment_len = glm::length(segment);
                    if (segment_len > 1.0e-6f)
                    {
                        PhysicsSystem::RaycastFilter filter{};
                        filter.layer = emitter.collision_layer;
                        filter.mask = emitter.collision_mask;
                        filter.include_triggers = false;

                        PhysicsSystem::RaycastHit hit{};
                        const bool did_hit = physics_system->raycast(
                            prev_position,
                            segment / segment_len,
                            segment_len + std::max(0.0f, emitter.collision_radius),
                            hit,
                            filter);
                        if (did_hit && hit.hit)
                        {
                            if (emitter.collision_mode == ecs::ParticleCollisionMode::Kill)
                            {
                                runtime.particles[i] = runtime.particles.back();
                                runtime.particles.pop_back();
                                continue;
                            }
                            if (emitter.collision_mode == ecs::ParticleCollisionMode::Bounce)
                            {
                                const float push_out = std::max(0.0f, emitter.collision_radius);
                                next_position = hit.point + hit.normal * push_out;
                                const float vn = glm::dot(particle.velocity, hit.normal);
                                if (vn < 0.0f)
                                {
                                    const float bounce = std::clamp(emitter.collision_bounce, 0.0f, 1.0f);
                                    particle.velocity -= (1.0f + bounce) * vn * hit.normal;
                                }
                            }
                        }
                    }
                }

                particle.position = next_position;

                const float age01 = std::clamp(particle.age / particle.lifetime, 0.0f, 1.0f);
                const float size = glm::mix(particle.size_begin, particle.size_end, age01);
                runtime.render_particles.push_back(ParticleSystem::RenderParticle{
                    particle.position,
                    std::max(0.0f, size),
                    lerp_color(particle.color_begin, particle.color_end, age01)
                    });
                ++i;
            }
        }
    }

    void ParticleSystem::update(
        entt::registry& registry,
        EngineContext& ctx,
        float delta_time,
        const PhysicsSystem* physics_system)
    {
        auto view = registry.view<ecs::ParticleEmitterComponent>();

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
            auto [it, inserted] = runtimes_.try_emplace(entity);
            if (inserted || it->second.rng_state == 0u)
                it->second.rng_state = seed_from_entity(entity);
        }

        std::vector<EmitterUpdateItem> work_items;
        work_items.reserve(entities.size());
        bool collisions_requested = false;
        for (auto entity : entities)
        {
            const auto runtime_it = runtimes_.find(entity);
            if (runtime_it == runtimes_.end())
                continue;

            EmitterUpdateItem item{};
            item.emitter = view.get<ecs::ParticleEmitterComponent>(entity);
            if (const auto* transform = registry.try_get<ecs::TransformComponent>(entity))
                item.world_matrix = transform->world_matrix;
            if (const auto burst_it = pending_bursts_.find(entity); burst_it != pending_bursts_.end())
                item.requested_burst = burst_it->second;
            item.runtime = &runtime_it->second;
            work_items.push_back(item);
            collisions_requested = collisions_requested
                || item.emitter.collision_mode != ecs::ParticleCollisionMode::None;
        }
        pending_bursts_.clear();

        const bool can_thread =
            threaded_simulation_
            && !collisions_requested
            && ctx.thread_pool
            && work_items.size() >= 16;

        if (!can_thread)
        {
            for (const auto& item : work_items)
                update_emitter_particles(item, delta_time, physics_system);
            return;
        }

        std::vector<std::future<void>> futures;
        const std::size_t worker_count = std::max<std::size_t>(1u, ctx.thread_pool->nbr_threads());
        const std::size_t chunk_size = std::max<std::size_t>(
            8u,
            (work_items.size() + worker_count - 1u) / worker_count);

        for (std::size_t start = 0; start < work_items.size(); start += chunk_size)
        {
            const std::size_t end = std::min(start + chunk_size, work_items.size());
            futures.push_back(ctx.thread_pool->queue_task(
                [start, end, delta_time, &work_items]()
                {
                    for (std::size_t i = start; i < end; ++i)
                        update_emitter_particles(work_items[i], delta_time, nullptr);
                }));
        }

        for (auto& future : futures)
            future.get();
    }

    void ParticleSystem::clear()
    {
        runtimes_.clear();
        pending_bursts_.clear();
    }

    void ParticleSystem::request_burst(entt::entity entity, std::uint32_t count)
    {
        if (entity == entt::null || count == 0u)
            return;
        auto& pending = pending_bursts_[entity];
        const std::uint64_t sum = static_cast<std::uint64_t>(pending) + static_cast<std::uint64_t>(count);
        pending = static_cast<std::uint32_t>(std::min<std::uint64_t>(sum, 100000u));
    }
}
