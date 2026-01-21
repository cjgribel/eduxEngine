// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/PhysicsSystem.hpp"

#include "EngineContext.hpp"
#include "LogMacros.h"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"
#include "hash_combine.h"

#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <cmath>

namespace
{
    // Convert engine-space vectors to Bullet-space (meters).
    btVector3 to_bt_vec3(const glm::vec3& v, float units_per_meter)
    {
        const float meters_per_unit = units_per_meter > 0.0f ? 1.0f / units_per_meter : 1.0f;
        return btVector3(v.x * meters_per_unit, v.y * meters_per_unit, v.z * meters_per_unit);
    }

    // Convert Bullet-space vectors (meters) back to engine units.
    glm::vec3 from_bt_vec3(const btVector3& v, float units_per_meter)
    {
        return glm::vec3(v.x(), v.y(), v.z()) * units_per_meter;
    }

    // Convert quaternion (same numeric layout, different type).
    btQuaternion to_bt_quat(const glm::quat& q)
    {
        return btQuaternion(q.x, q.y, q.z, q.w);
    }

    glm::quat from_bt_quat(const btQuaternion& q)
    {
        return glm::quat(q.w(), q.x(), q.y(), q.z());
    }

    // Helper to get a reasonable uniform scale from a potentially non-uniform scale vector.
    float uniform_scale(const glm::vec3& scale)
    {
        return (scale.x + scale.y + scale.z) / 3.0f;
    }

    // Hash collider content so we can detect edits and rebuild Bullet bodies.
    std::size_t hash_collider_desc(const eeng::ecs::ColliderDesc& collider)
    {
        std::size_t seed = 0;
        ::detail::hash_combine(seed, static_cast<int>(collider.type));
        ::detail::hash_combine(seed, collider.id);
        ::detail::hash_combine(seed, collider.local_position.x);
        ::detail::hash_combine(seed, collider.local_position.y);
        ::detail::hash_combine(seed, collider.local_position.z);
        ::detail::hash_combine(seed, collider.local_rotation.x);
        ::detail::hash_combine(seed, collider.local_rotation.y);
        ::detail::hash_combine(seed, collider.local_rotation.z);
        ::detail::hash_combine(seed, collider.local_rotation.w);
        ::detail::hash_combine(seed, collider.half_extents.x);
        ::detail::hash_combine(seed, collider.half_extents.y);
        ::detail::hash_combine(seed, collider.half_extents.z);
        ::detail::hash_combine(seed, collider.radius);
        ::detail::hash_combine(seed, collider.height);
        ::detail::hash_combine(seed, collider.submesh_index);
        ::detail::hash_combine(seed, collider.mesh_ref.guid);
        ::detail::hash_combine(seed, collider.is_trigger);
        return seed;
    }

    // Hash the entire collider list; this is used to detect edits in the inspector.
    std::size_t hash_colliders(const eeng::ecs::ColliderComponent& colliders)
    {
        std::size_t seed = 0;
        ::detail::hash_combine(seed, colliders.colliders.size());
        for (const auto& collider : colliders.colliders)
            ::detail::hash_combine(seed, hash_collider_desc(collider));
        return seed;
    }

    // Build a Bullet shape from a collider description and entity scale.
    std::unique_ptr<btCollisionShape> build_shape(
        const eeng::ecs::ColliderDesc& collider,
        const glm::vec3& entity_scale,
        float units_per_meter)
    {
        const float meters_per_unit = units_per_meter > 0.0f ? 1.0f / units_per_meter : 1.0f;
        const glm::vec3 scaled_extents = collider.half_extents * entity_scale * meters_per_unit;
        const float scale_u = uniform_scale(entity_scale) * meters_per_unit;

        switch (collider.type)
        {
        case eeng::ecs::ColliderType::Box:
        case eeng::ecs::ColliderType::AABB:
        {
            const btVector3 half_extents(scaled_extents.x, scaled_extents.y, scaled_extents.z);
            return std::make_unique<btBoxShape>(half_extents);
        }
        case eeng::ecs::ColliderType::Sphere:
        {
            const float radius = std::max(0.0f, collider.radius * scale_u);
            return std::make_unique<btSphereShape>(radius);
        }
        case eeng::ecs::ColliderType::Capsule:
        {
            // Our debug capsule uses height as the cylinder length; Bullet does the same.
            const float radius = std::max(0.0f, collider.radius * scale_u);
            const float height = std::max(0.0f, collider.height * scale_u);
            return std::make_unique<btCapsuleShapeZ>(radius, height);
        }
        case eeng::ecs::ColliderType::ConvexHull:
        case eeng::ecs::ColliderType::TriangleMesh:
        default:
        {
            // Placeholder: use a box sized by half_extents when mesh data isn't cooked yet.
            const btVector3 half_extents(scaled_extents.x, scaled_extents.y, scaled_extents.z);
            return std::make_unique<btBoxShape>(half_extents);
        }
        }
    }
} // namespace

namespace eeng::ecs::systems
{
    PhysicsSystem::~PhysicsSystem()
    {
        shutdown();
    }

    void PhysicsSystem::init(EngineContext& ctx)
    {
        if (initialized_)
            return;

        (void)ctx; // Context hook point for future physics settings/asset wiring.

        // Configure and spin up the Bullet world.
        settings_ = physics::PhysicsWorldSettings{};
        world_.init(settings_);
        initialized_ = true;
    }

    void PhysicsSystem::shutdown()
    {
        if (!initialized_)
            return;

        // Remove bodies from the Bullet world before clearing storage.
        if (auto* world = world_.world())
        {
            for (auto& [entity, runtime] : bodies_)
            {
                if (runtime.body)
                    world->removeRigidBody(runtime.body.get());
            }
        }

        bodies_.clear();
        world_.shutdown();
        initialized_ = false;
    }

    void PhysicsSystem::update(entt::registry& registry, EngineContext& ctx, float delta_time)
    {
        if (!initialized_)
            return;

        // Keep the Bullet world in sync with ECS ownership.
        sync_bodies(registry, ctx);

        // Push transforms for static/kinematic bodies into Bullet.
        sync_transforms_to_bullet(registry);

        // Step the simulation.
        world_.step_simulation(delta_time);

        // Pull transforms back for dynamic bodies.
        sync_transforms_from_bullet(registry);
    }

    void PhysicsSystem::sync_bodies(entt::registry& registry, EngineContext& ctx)
    {
        auto* world = world_.world();
        if (!world)
            return;

        // Remove stale bodies (entity destroyed or component removed).
        for (auto it = bodies_.begin(); it != bodies_.end();)
        {
            const entt::entity entity = it->first;
            if (!registry.valid(entity)
                || !registry.all_of<ecs::TransformComponent, ecs::RigidBodyComponent, ecs::ColliderComponent>(entity))
            {
                if (it->second.body)
                    world->removeRigidBody(it->second.body.get());
                it = bodies_.erase(it);
            }
            else
            {
                const auto& tfm = registry.get<ecs::TransformComponent>(entity);
                const auto& rb = registry.get<ecs::RigidBodyComponent>(entity);
                const auto& colliders = registry.get<ecs::ColliderComponent>(entity);

                // Rebuild when motion, scale, or collider data changes in-editor.
                const std::size_t collider_hash = hash_colliders(colliders);
                if (it->second.motion != rb.motion
                    || it->second.collider_hash != collider_hash
                    || it->second.scale != tfm.scale)
                {
                    if (it->second.body)
                        world->removeRigidBody(it->second.body.get());
                    it = bodies_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // Add missing bodies for entities that have the required components.
        auto view = registry.view<ecs::TransformComponent, ecs::RigidBodyComponent, ecs::ColliderComponent>();
        for (const auto entity : view)
        {
            if (bodies_.find(entity) != bodies_.end())
                continue;

            const auto& tfm = view.get<ecs::TransformComponent>(entity);
            const auto& rb = view.get<ecs::RigidBodyComponent>(entity);
            const auto& colliders = view.get<ecs::ColliderComponent>(entity);

            if (colliders.colliders.empty())
            {
                EENG_LOG_WARN(&ctx, "PhysicsSystem: Entity %u has RigidBody but no colliders.",
                    static_cast<unsigned>(entt::to_integral(entity)));
                continue;
            }

            BodyRuntime runtime{};
            runtime.compound_shape = std::make_unique<btCompoundShape>();

            // Build child shapes for every collider (rebuilds are handled via hash checks).
            for (const auto& collider : colliders.colliders)
            {
                auto shape = build_shape(collider, tfm.scale, settings_.units_per_meter);
                if (!shape)
                    continue;

                btTransform local;
                local.setIdentity();
                // Collider offsets should respect entity scale in the same way as size.
                local.setOrigin(to_bt_vec3(collider.local_position * tfm.scale, settings_.units_per_meter));
                local.setRotation(to_bt_quat(collider.local_rotation));

                runtime.compound_shape->addChildShape(local, shape.get());
                runtime.child_shapes.emplace_back(std::move(shape));
            }

            if (runtime.child_shapes.empty())
                continue;

            // Update compound bounds after adding all children.
            runtime.compound_shape->recalculateLocalAabb();

            // Initial world transform uses current ECS local position/rotation.
            // Note: this ignores parent transforms for now; we handle hierarchy later.
            btTransform start_transform;
            start_transform.setIdentity();
            start_transform.setOrigin(to_bt_vec3(tfm.position, settings_.units_per_meter));
            start_transform.setRotation(to_bt_quat(tfm.rotation));

            runtime.motion_state = std::make_unique<btDefaultMotionState>(start_transform);

            const bool is_dynamic = (rb.motion == ecs::PhysicsMotionType::Dynamic);
            const bool is_kinematic = (rb.motion == ecs::PhysicsMotionType::Kinematic);

            float mass = is_dynamic ? rb.mass : 0.0f;
            if (is_dynamic && rb.auto_mass)
            {
                // Auto-mass is not computed yet; keep a reasonable non-zero default.
                mass = rb.mass > 0.0f ? rb.mass : 1.0f;
            }

            btVector3 inertia(0.0f, 0.0f, 0.0f);
            if (is_dynamic && mass > 0.0f)
            {
                // For now, rely on Bullet's shape-based inertia.
                runtime.compound_shape->calculateLocalInertia(mass, inertia);
            }

            btRigidBody::btRigidBodyConstructionInfo info(
                mass,
                runtime.motion_state.get(),
                runtime.compound_shape.get(),
                inertia);

            runtime.body = std::make_unique<btRigidBody>(info);

            runtime.body->setDamping(rb.linear_damping, rb.angular_damping);

            if (!rb.allow_sleep || is_kinematic)
                runtime.body->setActivationState(DISABLE_DEACTIVATION);

            if (is_dynamic)
            {
                // Gravity is set per-body so we can respect gravity_scale.
                const btVector3 gravity(
                    settings_.gravity.x * rb.gravity_scale,
                    settings_.gravity.y * rb.gravity_scale,
                    settings_.gravity.z * rb.gravity_scale);
                runtime.body->setGravity(gravity);
            }

            if (rb.enable_ccd)
            {
                runtime.body->setCcdMotionThreshold(rb.ccd_motion_threshold);
                runtime.body->setCcdSweptSphereRadius(rb.ccd_swept_sphere_radius);
            }

            if (is_kinematic)
            {
                runtime.body->setCollisionFlags(
                    runtime.body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            }

            // Store the entity id on the Bullet body for future contact callbacks.
            runtime.body->setUserIndex(static_cast<int>(entt::to_integral(entity)));

            world->addRigidBody(runtime.body.get());
            // Cache state so we can detect edits later.
            runtime.motion = rb.motion;
            runtime.collider_hash = hash_colliders(colliders);
            runtime.scale = tfm.scale;
            bodies_.emplace(entity, std::move(runtime));
        }
    }

    void PhysicsSystem::sync_transforms_to_bullet(entt::registry& registry)
    {
        auto* world = world_.world();
        if (!world)
            return;

        for (auto& [entity, runtime] : bodies_)
        {
            auto* tfm = registry.try_get<ecs::TransformComponent>(entity);
            auto* rb = registry.try_get<ecs::RigidBodyComponent>(entity);
            if (!tfm || !rb || !runtime.body)
                continue;

            if (rb->motion == ecs::PhysicsMotionType::Dynamic)
                continue;

            btTransform transform;
            transform.setIdentity();
            transform.setOrigin(to_bt_vec3(tfm->position, settings_.units_per_meter));
            transform.setRotation(to_bt_quat(tfm->rotation));

            runtime.body->setWorldTransform(transform);
            runtime.body->getMotionState()->setWorldTransform(transform);
            runtime.body->setInterpolationWorldTransform(transform);
            // Avoid carrying velocities on kinematic/static bodies.
            runtime.body->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
            runtime.body->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
        }
    }

    void PhysicsSystem::sync_transforms_from_bullet(entt::registry& registry)
    {
        auto* world = world_.world();
        if (!world)
            return;

        for (auto& [entity, runtime] : bodies_)
        {
            auto* tfm = registry.try_get<ecs::TransformComponent>(entity);
            auto* rb = registry.try_get<ecs::RigidBodyComponent>(entity);
            if (!tfm || !rb || !runtime.body)
                continue;

            if (rb->motion != ecs::PhysicsMotionType::Dynamic)
                continue;

            btTransform transform;
            runtime.body->getMotionState()->getWorldTransform(transform);

            // Write back into local transforms (hierarchy handling comes later).
            tfm->position = from_bt_vec3(transform.getOrigin(), settings_.units_per_meter);
            tfm->rotation = glm::normalize(from_bt_quat(transform.getRotation()));
            tfm->mark_local_dirty();
        }
    }
} // namespace eeng::ecs::systems
