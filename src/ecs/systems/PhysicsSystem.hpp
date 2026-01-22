// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "entt/entt.hpp"

#include "physics/PhysicsWorld.hpp"
#include "ecs/PhysicsComponents.hpp"

// Bullet headers are required here because BodyRuntime owns Bullet objects.
#include <btBulletDynamicsCommon.h>

namespace eeng
{
    struct EngineContext;
}

namespace eeng::editor
{
    struct FieldChangedEvent;
}

namespace eeng::ecs::systems
{
    // Physics system that bridges ECS components to a Bullet world.
    // Notes:
    // - Uses a private dirty set fed by FieldChangedEvent for editor edits.
    // - Keeps a small cache of motion/scale so non-editor changes are also caught safely.
    class PhysicsSystem
    {
    public:
        PhysicsSystem() = default;
        ~PhysicsSystem();

        PhysicsSystem(const PhysicsSystem&) = delete;
        PhysicsSystem& operator=(const PhysicsSystem&) = delete;

        void init(EngineContext& ctx);
        void shutdown();

        void update(entt::registry& registry, EngineContext& ctx, float delta_time);

    private:
        struct BodyRuntime
        {
            // Compound shape acts as the root to support multiple colliders per entity.
            std::unique_ptr<btCompoundShape> compound_shape;
            std::vector<std::unique_ptr<btCollisionShape>> child_shapes;
            std::unique_ptr<btDefaultMotionState> motion_state;
            std::unique_ptr<btRigidBody> body;
            // Cached component state so we can rebuild when key properties change.
            ecs::PhysicsMotionType motion = ecs::PhysicsMotionType::Dynamic;
            glm::vec3 scale{ 1.0f };
        };

        physics::PhysicsWorld world_;
        physics::PhysicsWorldSettings settings_{};
        std::unordered_map<entt::entity, BodyRuntime> bodies_;
        // Central dirty set populated from editor field edit events.
        std::unordered_set<entt::entity> dirty_entities_;
        bool initialized_ = false;

        void sync_bodies(entt::registry& registry, EngineContext& ctx);
        void sync_transforms_to_bullet(entt::registry& registry);
        void sync_transforms_from_bullet(entt::registry& registry);

        // Callback for field edit events; filters for physics-affecting component changes.
        void handle_field_changed_event(const editor::FieldChangedEvent& event);
    };
} // namespace eeng::ecs::systems
