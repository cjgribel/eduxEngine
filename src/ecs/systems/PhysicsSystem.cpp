// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/PhysicsSystem.hpp"

#include "EngineContext.hpp"
#include "LogMacros.h"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"
#include "engineapi/EngineContextHelpers.hpp"
#include "editor/AssignFieldCommand.hpp"
#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace
{
    // Helper to resolve an event target into a live entity.
    std::optional<eeng::ecs::Entity> resolve_event_entity(
        const eeng::editor::FieldTarget& target,
        eeng::EngineContext& ctx)
    {
        eeng::ecs::Entity entity = target.entity;
        if (target.entity_guid.valid())
        {
            if (ctx.entity_manager)
            {
                if (auto entity_opt = ctx.entity_manager->get_entity_from_guid(target.entity_guid))
                    entity = *entity_opt;
                if (!ctx.entity_manager->entity_valid(entity))
                    return std::nullopt;
            }
        }

        if (!entity.has_id())
            return std::nullopt;
        return entity;
    }

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

        // Centralized dirtying: listen to field edits and mark entities for rebuild.
        // We intentionally avoid per-component post-assign hooks to keep this policy in one place.
        auto* event_queue = eeng::try_get_event_queue(ctx, "PhysicsSystem");
        if (event_queue)
        {
            event_queue->register_callback([this](const editor::FieldChangedEvent& event)
                {
                    handle_field_changed_event(event);
                });
        }

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

                // Rebuild when marked dirty, when motion changes, when scale changes, or when colliders are cleared.
                // The extra checks are safety nets for non-editor changes (runtime code, deserialization).
                // Consume the dirty request so it does not rebuild every frame.
                const bool dirty = (dirty_entities_.erase(entity) > 0);
                const bool motion_changed = (it->second.motion != rb.motion);
                const bool scale_changed = (it->second.scale != tfm.scale);
                const bool colliders_cleared = colliders.colliders.empty();
                if (dirty || motion_changed || scale_changed || colliders_cleared)
                {
                    if (!dirty && (motion_changed || scale_changed || colliders_cleared))
                    {
                        // Safety-net log: rebuild triggered without an explicit dirty event.
                        EENG_LOG_WARN(&ctx,
                            "PhysicsSystem: Rebuilding entity %u via safety check (motion=%d scale=%d colliders=%d).",
                            static_cast<unsigned>(entt::to_integral(entity)),
                            motion_changed ? 1 : 0,
                            scale_changed ? 1 : 0,
                            colliders_cleared ? 1 : 0);
                    }
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

            // Build child shapes for every collider (rebuilds are driven by the dirty set).
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
            runtime.scale = tfm.scale;
            bodies_.emplace(entity, std::move(runtime));

            // Clear any pending dirty request once we have rebuilt the Bullet body.
            dirty_entities_.erase(entity);
        }

        // Drop dirty flags for entities that are no longer valid or lack required components.
        for (auto it = dirty_entities_.begin(); it != dirty_entities_.end();)
        {
            if (!registry.valid(*it)
                || !registry.all_of<ecs::TransformComponent, ecs::RigidBodyComponent, ecs::ColliderComponent>(*it))
                it = dirty_entities_.erase(it);
            else
                ++it;
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

    void PhysicsSystem::handle_field_changed_event(const editor::FieldChangedEvent& event)
    {
        if (event.target.kind != editor::FieldTarget::Kind::Component)
            return;

        // We only care about physics-affecting component edits.
        const entt::id_type rb_id = entt::type_hash<ecs::RigidBodyComponent>::value();
        const entt::id_type collider_id = entt::type_hash<ecs::ColliderComponent>::value();
        const entt::id_type transform_id = entt::type_hash<ecs::TransformComponent>::value();

        const bool is_rb = (event.target.component_id == rb_id);
        const bool is_collider = (event.target.component_id == collider_id);
        const bool is_transform = (event.target.component_id == transform_id);
        if (!is_rb && !is_collider && !is_transform)
            return;

        // Only scale edits require collider rebuilds; position/rotation do not.
        if (is_transform)
        {
            const editor::MetaFieldPath::Entry* first_data = nullptr;
            for (const auto& entry : event.meta_path.entries)
            {
                if (entry.type == editor::MetaFieldPath::Entry::Type::Data)
                {
                    first_data = &entry;
                    break;
                }
            }

            if (!first_data || first_data->name != "scale")
                return;
        }

        auto ctx_sp = event.target.ctx.lock();
        if (!ctx_sp)
            return;

        auto entity_opt = resolve_event_entity(event.target, *ctx_sp);
        if (!entity_opt)
            return;

        auto registry_sp = event.target.registry.lock();
        entt::registry* registry = registry_sp ? registry_sp.get()
            : eeng::try_get_registry_ptr(*ctx_sp, "PhysicsSystem");

        if (!registry || !registry->valid(*entity_opt))
            return;

        // Store in the local dirty set; rebuild happens on the next PhysicsSystem update.
        // RigidBody/Collider edits always dirty; Transform edits are filtered above.
        dirty_entities_.insert(*entity_opt);
    }

} // namespace eeng::ecs::systems
