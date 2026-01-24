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
    struct BatchTaskCompletedEvent;
}

namespace eeng::editor
{
    struct FieldChangedEvent;
}

namespace eeng::ecs::systems
{
    // Physics system that bridges ECS components to a Bullet world.
    // Notes:
    // - Uses a private dirty set fed by FieldChangedEvent + lifecycle hooks.
    // - Uses a batch sync request flag to avoid per-frame structural scans.
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
            struct ColliderRuntimeInfo
            {
                ecs::ColliderId id = 0;
                bool is_trigger = false;
            };

            // Compound shape acts as the root to support multiple colliders per entity.
            std::unique_ptr<btCompoundShape> compound_shape;
            std::vector<std::unique_ptr<btCollisionShape>> child_shapes;
            // Per-child collider metadata for contact event lookup.
            std::vector<ColliderRuntimeInfo> collider_info;
            std::unique_ptr<btDefaultMotionState> motion_state;
            std::unique_ptr<btRigidBody> body;
            // Cached component state so we can rebuild when key properties change.
            ecs::PhysicsMotionType motion = ecs::PhysicsMotionType::Dynamic;
            glm::vec3 scale{ 1.0f };
        };

        // Contact key used to track enter/stay/exit across frames.
        struct ContactKey
        {
            entt::entity entity_a{ entt::null };
            entt::entity entity_b{ entt::null };
            ecs::ColliderId collider_a = 0;
            ecs::ColliderId collider_b = 0;
            bool is_trigger = false;

            bool operator==(const ContactKey& other) const
            {
                return entity_a == other.entity_a
                    && entity_b == other.entity_b
                    && collider_a == other.collider_a
                    && collider_b == other.collider_b
                    && is_trigger == other.is_trigger;
            }
        };

        // Contact details captured from the current simulation step.
        struct ContactInfo
        {
            glm::vec3 point{ 0.0f };
            glm::vec3 normal{ 0.0f };
            float impulse = 0.0f;
        };

        // Hash functor for ContactKey
        struct ContactKeyHash
        {
            std::size_t operator()(const ContactKey& key) const noexcept
            {
                std::size_t seed = 0;
                auto hash_combine = [&seed](std::size_t value)
                {
                    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
                };

                hash_combine(std::hash<std::uint32_t>{}(
                    static_cast<std::uint32_t>(entt::to_integral(key.entity_a))));
                hash_combine(std::hash<std::uint32_t>{}(
                    static_cast<std::uint32_t>(entt::to_integral(key.entity_b))));
                hash_combine(std::hash<std::uint32_t>{}(key.collider_a));
                hash_combine(std::hash<std::uint32_t>{}(key.collider_b));
                hash_combine(std::hash<std::uint8_t>{}(key.is_trigger ? 1u : 0u));
                return seed;
            }
        };

        physics::PhysicsWorld world_;
        physics::PhysicsWorldSettings settings_{};
        std::unordered_map<entt::entity, BodyRuntime> bodies_;
        // Central dirty set populated from editor field edit events + construct hooks.
        std::unordered_set<entt::entity> dirty_entities_;
        // Event buffers cleared each frame for entities that received physics events.
        std::unordered_set<entt::entity> event_entities_;
        // Contact history for enter/stay/exit classification.
        std::unordered_map<ContactKey, ContactInfo, ContactKeyHash> previous_contacts_;
        // Set when batch load/unload completes to force a structural sync on the next update.
        bool batch_sync_requested_ = false;
        bool initialized_ = false;

        // Non-owning context pointer used by lifecycle hooks (valid while system is active).
        EngineContext* ctx_ = nullptr;

        // Scoped connections keep entt signal hooks tied to this system's lifetime.
        entt::scoped_connection rb_construct_conn_;
        entt::scoped_connection rb_destroy_conn_;
        entt::scoped_connection collider_construct_conn_;
        entt::scoped_connection collider_destroy_conn_;
        entt::scoped_connection transform_construct_conn_;
        entt::scoped_connection transform_destroy_conn_;

        void sync_bodies(entt::registry& registry, EngineContext& ctx);
        void sync_transforms_to_bullet(entt::registry& registry);
        void sync_transforms_from_bullet(entt::registry& registry);
        void clear_contact_events(entt::registry& registry);
        void emit_contact_events(entt::registry& registry, EngineContext& ctx);
        // Resolve collider metadata from a compound part id (defaults to index 0).
        static BodyRuntime::ColliderRuntimeInfo resolve_collider_info(
            const BodyRuntime& runtime,
            int part_id);

        // Callback for field edit events; filters for physics-affecting component changes.
        void handle_field_changed_event(const editor::FieldChangedEvent& event);
        // Callback for batch load/unload completion; triggers a one-shot structural sync.
        void handle_batch_task_event(const BatchTaskCompletedEvent& event);

        // Lifecycle hooks to create/destroy Bullet bodies as components appear/disappear.
        void on_rigidbody_construct(entt::registry& registry, entt::entity entity);
        void on_rigidbody_destroy(entt::registry& registry, entt::entity entity);
        void on_collider_construct(entt::registry& registry, entt::entity entity);
        void on_collider_destroy(entt::registry& registry, entt::entity entity);
        void on_transform_construct(entt::registry& registry, entt::entity entity);
        void on_transform_destroy(entt::registry& registry, entt::entity entity);

        // Helpers for creating/removing bodies without duplicating sync_bodies logic.
        bool create_body_for_entity(entt::registry& registry,
            EngineContext& ctx,
            entt::entity entity,
            bool log_missing_colliders);
        void destroy_body_for_entity(entt::entity entity);
    };
} // namespace eeng::ecs::systems
