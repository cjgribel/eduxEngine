// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "entt/entt.hpp"
#include "ecs/ParticleEmitterComponent.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace eeng
{
    struct EngineContext;
}

namespace eeng::ecs::systems
{
    class PhysicsSystem;

    class ParticleSystem
    {
    public:
        struct RenderParticle
        {
            glm::vec3 position{ 0.0f };
            float size = 0.0f;
            std::uint32_t color_abgr = 0xffffffffu;
        };

        // Internal runtime structs are public so update helpers can be implemented
        // out-of-line without exposing extra headers.
        struct ParticleRuntime
        {
            glm::vec3 position{ 0.0f };
            glm::vec3 velocity{ 0.0f };
            float age = 0.0f;
            float lifetime = 1.0f;
            float size_begin = 0.0f;
            float size_end = 0.0f;
            std::uint32_t color_begin = 0xffffffffu;
            std::uint32_t color_end = 0xffffffffu;
        };

        struct EmitterRuntime
        {
            float spawn_accumulator = 0.0f;
            float duration_elapsed = 0.0f;
            std::uint32_t rng_state = 0u;
            std::vector<ParticleRuntime> particles;
            std::vector<RenderParticle> render_particles;
        };

        void set_threaded_simulation(bool enabled) { threaded_simulation_ = enabled; }
        bool threaded_simulation() const { return threaded_simulation_; }

        void update(
            entt::registry& registry,
            EngineContext& ctx,
            float delta_time,
            const PhysicsSystem* physics_system);
        void request_burst(entt::entity entity, std::uint32_t count);

        void clear();

        template<typename Fn>
        void for_each_render_emitter(entt::registry& registry, Fn&& fn) const
        {
            auto view = registry.view<ecs::ParticleEmitterComponent>();
            for (auto entity : view)
            {
                const auto runtime_it = runtimes_.find(entity);
                if (runtime_it == runtimes_.end())
                    continue;

                const auto& runtime = runtime_it->second;
                if (runtime.render_particles.empty())
                    continue;

                fn(entity, view.get<ecs::ParticleEmitterComponent>(entity), runtime.render_particles);
            }
        }

    private:
        std::unordered_map<entt::entity, EmitterRuntime> runtimes_;
        std::unordered_map<entt::entity, std::uint32_t> pending_bursts_;
        bool threaded_simulation_ = false;
    };
}
