// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "ecs/Entity.hpp"
#include "glmcommon.hpp"

#include <entt/entt.hpp>

namespace eeng
{
    struct EngineContext;
}

namespace eeng::ecs::systems
{
    class PhysicsSystem;

    // RMB-driven point-constraint picker for play mode.
    class MousePointConstraintSystem
    {
    public:
        void update(entt::registry& registry, EngineContext& ctx, PhysicsSystem& physics_system);
        void shutdown(EngineContext& ctx);

    private:
        bool compute_mouse_ray(const EngineContext& ctx, glm_aux::Ray& out_ray) const;
        bool create_picker_entities(entt::registry& registry, EngineContext& ctx);
        void disable_picker(entt::registry& registry, EngineContext& ctx);

        ecs::Entity anchor_entity_{};
        ecs::Entity constraint_entity_{};
        ecs::Entity target_entity_{};
        float distance_ = 0.0f;
        bool active_ = false;
        bool rmb_was_down_ = false;
    };
} // namespace eeng::ecs::systems
