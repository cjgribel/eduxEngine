// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <unordered_set>

#include "entt/entt.hpp"

namespace eeng
{
    struct EngineContext;
}

namespace eeng::ecs::systems
{
    // Minimal script system placeholder wired into component lifecycle.
    class ScriptSystem
    {
    public:
        void init(EngineContext& ctx);
        void shutdown();

        void update(entt::registry& registry, EngineContext& ctx, float delta_time);

    private:
        // Non-owning context pointer used by lifecycle hooks.
        EngineContext* ctx_ = nullptr;
        // Track active script components for future runtime wiring.
        std::unordered_set<entt::entity> live_scripts_;

        // Scoped connections keep entt signal hooks tied to this system's lifetime.
        entt::scoped_connection script_construct_conn_;
        entt::scoped_connection script_destroy_conn_;

        void on_script_construct(entt::registry& registry, entt::entity entity);
        void on_script_destroy(entt::registry& registry, entt::entity entity);
    };
} // namespace eeng::ecs::systems
