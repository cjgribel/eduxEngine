// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ecs/systems/PhysicsSystem.hpp"

#include "EngineContext.hpp"
#include "LogMacros.h"
#include "ecs/HeaderComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/StickyNoteComponent.hpp"
#include "ecs/TransformComponent.hpp"
#include "engineapi/EngineContextHelpers.hpp"
#include "editor/AssignFieldCommand.hpp"
#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

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

    // Convert Bullet-space direction vectors without unit scaling.
    glm::vec3 from_bt_dir3(const btVector3& v)
    {
        return glm::vec3(v.x(), v.y(), v.z());
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

    // Check if a collider has an identity local transform (used to skip compound shapes).
    bool is_identity_collider_transform(const eeng::ecs::ColliderDesc& collider)
    {
        constexpr float kEpsilon = 1e-4f;
        const float pos_len = std::sqrt(
            collider.local_position.x * collider.local_position.x
            + collider.local_position.y * collider.local_position.y
            + collider.local_position.z * collider.local_position.z);
        if (pos_len > kEpsilon)
            return false;

        if (std::abs(collider.local_rotation.w - 1.0f) > kEpsilon)
            return false;
        if (std::abs(collider.local_rotation.x) > kEpsilon
            || std::abs(collider.local_rotation.y) > kEpsilon
            || std::abs(collider.local_rotation.z) > kEpsilon)
            return false;

        return true;
    }

    // Pick a representative contact point from a manifold (prefer a penetrating one).
    const btManifoldPoint* select_contact_point(const btPersistentManifold& manifold)
    {
        if (manifold.getNumContacts() == 0)
            return nullptr;

        const btManifoldPoint* best = &manifold.getContactPoint(0);
        for (int i = 0; i < manifold.getNumContacts(); ++i)
        {
            const btManifoldPoint& point = manifold.getContactPoint(i);
            if (point.getDistance() < 0.0f)
                return &point;
        }
        return best;
    }

    // Consistent ordering for contact keys so enter/exit works regardless of Bullet body order.
    bool should_swap_contact(
        entt::entity entity_a,
        entt::entity entity_b,
        eeng::ecs::ColliderId collider_a,
        eeng::ecs::ColliderId collider_b)
    {
        const auto id_a = entt::to_integral(entity_a);
        const auto id_b = entt::to_integral(entity_b);
        if (id_a != id_b)
            return id_a > id_b;
        return collider_a > collider_b;
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

        // Cache context for lifecycle hooks and log messages.
        ctx_ = &ctx;

        // Centralized dirtying: listen to field edits and mark entities for rebuild.
        // We intentionally avoid per-component post-assign hooks to keep this policy in one place.
        auto* event_queue = eeng::try_get_event_queue(ctx, "PhysicsSystem");
        if (event_queue)
        {
            event_queue->register_callback([this](const editor::FieldChangedEvent& event)
                {
                    handle_field_changed_event(event);
                });
            // Batch load/unload is a structural change; request a one-shot sync afterwards.
            event_queue->register_callback([this](const BatchTaskCompletedEvent& event)
                {
                    handle_batch_task_event(event);
                });
            event_queue->register_callback([this, &ctx](const SetPlayModeEvent&)
                {
                    reset_for_world(ctx);
                });
            event_queue->register_callback([this, &ctx](const TogglePlayModeEvent&)
                {
                    reset_for_world(ctx);
                });
        }

        // Lifecycle hooks: respond immediately to component add/remove.
        auto* registry = eeng::try_get_registry_ptr(ctx, "PhysicsSystem");
        if (registry)
        {
            // RAII entt signal holders
            rb_construct_conn_ = registry->on_construct<ecs::RigidBodyComponent>()
                .connect<&PhysicsSystem::on_rigidbody_construct>(this);
            rb_destroy_conn_ = registry->on_destroy<ecs::RigidBodyComponent>()
                .connect<&PhysicsSystem::on_rigidbody_destroy>(this);
            collider_construct_conn_ = registry->on_construct<ecs::ColliderComponent>()
                .connect<&PhysicsSystem::on_collider_construct>(this);
            collider_destroy_conn_ = registry->on_destroy<ecs::ColliderComponent>()
                .connect<&PhysicsSystem::on_collider_destroy>(this);
            transform_construct_conn_ = registry->on_construct<ecs::TransformComponent>()
                .connect<&PhysicsSystem::on_transform_construct>(this);
            transform_destroy_conn_ = registry->on_destroy<ecs::TransformComponent>()
                .connect<&PhysicsSystem::on_transform_destroy>(this);
        }

        // Force an initial sync in case entities existed before hooks were connected.
        batch_sync_requested_ = true;

        // Configure and spin up the Bullet world.
        settings_ = physics::PhysicsWorldSettings{};
        world_.init(settings_);
        initialized_ = true;
    }

    void PhysicsSystem::reset_for_world(EngineContext& ctx)
    {
        if (!initialized_)
            return;

        ctx_ = &ctx;

        if (auto* world = world_.world())
        {
            for (auto& [entity, runtime] : bodies_)
            {
                if (runtime.body)
                    world->removeRigidBody(runtime.body.get());
            }
        }

        bodies_.clear();
        dirty_entities_.clear();
        event_entities_.clear();
        current_contacts_.clear();
        previous_contacts_.clear();
        batch_sync_requested_ = true;

        auto* registry = eeng::try_get_registry_ptr(ctx, "PhysicsSystem");
        if (registry)
        {
            rb_construct_conn_ = registry->on_construct<ecs::RigidBodyComponent>()
                .connect<&PhysicsSystem::on_rigidbody_construct>(this);
            rb_destroy_conn_ = registry->on_destroy<ecs::RigidBodyComponent>()
                .connect<&PhysicsSystem::on_rigidbody_destroy>(this);
            collider_construct_conn_ = registry->on_construct<ecs::ColliderComponent>()
                .connect<&PhysicsSystem::on_collider_construct>(this);
            collider_destroy_conn_ = registry->on_destroy<ecs::ColliderComponent>()
                .connect<&PhysicsSystem::on_collider_destroy>(this);
            transform_construct_conn_ = registry->on_construct<ecs::TransformComponent>()
                .connect<&PhysicsSystem::on_transform_construct>(this);
            transform_destroy_conn_ = registry->on_destroy<ecs::TransformComponent>()
                .connect<&PhysicsSystem::on_transform_destroy>(this);
        }
        else
        {
            rb_construct_conn_.release();
            rb_destroy_conn_.release();
            collider_construct_conn_.release();
            collider_destroy_conn_.release();
            transform_construct_conn_.release();
            transform_destroy_conn_.release();
        }
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
        dirty_entities_.clear();
        ctx_ = nullptr;
        world_.shutdown();
        initialized_ = false;
    }

    void PhysicsSystem::update(entt::registry& registry, EngineContext& ctx, float delta_time)
    {
        if (!initialized_)
            return;

        // Clear per-frame physics event buffers before we run the next simulation step.
        clear_contact_events(registry);

        // Keep the Bullet world in sync with ECS ownership.
        // We only scan when there are dirty entities or a batch boundary requests a sync.
        if (batch_sync_requested_ || !dirty_entities_.empty())
        {
            sync_bodies(registry, ctx);
            batch_sync_requested_ = false;
        }

        // Push transforms for static/kinematic bodies into Bullet.
        sync_transforms_to_bullet(registry);

        // Step the simulation.
        world_.step_simulation(delta_time);

        // Gather contact events from Bullet for enter/stay/exit classification.
        emit_contact_events(registry, ctx);

        // Pull transforms back for dynamic bodies.
        sync_transforms_from_bullet(registry);
    }

    PhysicsSystem::PhysicsStats PhysicsSystem::get_stats() const
    {
        PhysicsStats stats{};
        stats.body_count = bodies_.size();
        stats.dirty_entities = dirty_entities_.size();
        stats.event_entities = event_entities_.size();
        stats.tracked_contacts = previous_contacts_.size();

        auto* world = world_.world();
        if (!world)
            return stats;

        stats.collision_objects = world->getNumCollisionObjects();

        auto* dispatcher = world->getDispatcher();
        if (!dispatcher)
            return stats;

        stats.manifolds = dispatcher->getNumManifolds();
        // Count raw Bullet contact points for a quick health check.
        int total_contacts = 0;
        for (int i = 0; i < stats.manifolds; ++i)
        {
            const btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
            if (!manifold)
                continue;
            total_contacts += manifold->getNumContacts();
        }
        stats.contact_points = total_contacts;
        return stats;
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

            if (!create_body_for_entity(registry, ctx, entity, true))
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

    bool PhysicsSystem::create_body_for_entity(
        entt::registry& registry,
        EngineContext& ctx,
        entt::entity entity,
        bool log_missing_colliders)
    {
        if (bodies_.find(entity) != bodies_.end())
            return true;

        if (!registry.all_of<ecs::TransformComponent, ecs::RigidBodyComponent, ecs::ColliderComponent>(entity))
            return false;

        const auto& tfm = registry.get<ecs::TransformComponent>(entity);
        const auto& rb = registry.get<ecs::RigidBodyComponent>(entity);
        const auto& colliders = registry.get<ecs::ColliderComponent>(entity);

        if (colliders.colliders.empty())
        {
            if (log_missing_colliders)
            {
                EENG_LOG_WARN(&ctx, "PhysicsSystem: Entity %u has RigidBody but no colliders.",
                    static_cast<unsigned>(entt::to_integral(entity)));
            }
            return false;
        }

        BodyRuntime runtime{};
        // Optimization: skip btCompoundShape when a single collider has an identity local transform.
        const bool single_collider = (colliders.colliders.size() == 1);
        const bool single_identity =
            single_collider && is_identity_collider_transform(colliders.colliders.front());
        const bool use_compound = !single_identity;

        if (use_compound)
            runtime.compound_shape = std::make_unique<btCompoundShape>();

        bool has_trigger = false;
        bool has_solid = false;

        // Build collision shapes (rebuilds are driven by the dirty set).
        for (const auto& collider : colliders.colliders)
        {
            auto shape = build_shape(collider, tfm.scale, settings_.units_per_meter);
            if (!shape)
                continue;

            if (use_compound)
            {
                btTransform local;
                local.setIdentity();
                // Collider offsets should respect entity scale in the same way as size.
                local.setOrigin(to_bt_vec3(collider.local_position * tfm.scale, settings_.units_per_meter));
                local.setRotation(to_bt_quat(collider.local_rotation));

                runtime.compound_shape->addChildShape(local, shape.get());
                runtime.child_shapes.emplace_back(std::move(shape));
                // Track collider metadata by child index so contact events can resolve ids.
                runtime.collider_info.push_back(
                    BodyRuntime::ColliderRuntimeInfo{ collider.id, collider.is_trigger });
            }
            else
            {
                // Single collider with identity local transform: no compound required.
                runtime.root_shape = std::move(shape);
                runtime.collider_info.push_back(
                    BodyRuntime::ColliderRuntimeInfo{ collider.id, collider.is_trigger });
            }

            if (collider.is_trigger)
                has_trigger = true;
            else
                has_solid = true;
        }

        if (use_compound && runtime.child_shapes.empty())
            return false;
        if (!use_compound && !runtime.root_shape)
            return false;

        // Update compound bounds after adding all children.
        if (runtime.compound_shape)
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

        btCollisionShape* collision_shape = runtime.compound_shape
            ? static_cast<btCollisionShape*>(runtime.compound_shape.get())
            : static_cast<btCollisionShape*>(runtime.root_shape.get());
        if (!collision_shape)
            return false;

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
            collision_shape->calculateLocalInertia(mass, inertia);
        }

        btRigidBody::btRigidBodyConstructionInfo info(
            mass,
            runtime.motion_state.get(),
            collision_shape,
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

        // Trigger rule: any trigger collider makes the entire body non-contact-response.
        // Limitation: Bullet flags are per-body, so mixing trigger + solid colliders on one body
        // will disable physical response for all colliders; use separate entities in that case.
        if (has_trigger)
        {
            runtime.body->setCollisionFlags(
                runtime.body->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            (void)has_solid;
        }

        // Store the entity id on the Bullet body for future contact callbacks.
        runtime.body->setUserIndex(static_cast<int>(entt::to_integral(entity)));

        // Apply collision filtering (layer/mask) when registering the body with Bullet.
        const auto* filter_comp = registry.try_get<ecs::CollisionFilterComponent>(entity);
        const ecs::CollisionFilter filter = filter_comp ? filter_comp->filter : ecs::CollisionFilter{};
        const short group = static_cast<short>(filter.layer & 0xFFFFu);
        const short mask = static_cast<short>(filter.mask & 0xFFFFu);

        if (auto* world = world_.world())
            world->addRigidBody(runtime.body.get(), group, mask);

        // Cache state so we can detect edits later.
        runtime.motion = rb.motion;
        runtime.scale = tfm.scale;
        runtime.local_version = tfm.local_version;
        bodies_.emplace(entity, std::move(runtime));
        // Clear any pending dirty request once we have created the Bullet body.
        dirty_entities_.erase(entity);
        return true;
    }

    void PhysicsSystem::destroy_body_for_entity(entt::entity entity)
    {
        auto it = bodies_.find(entity);
        if (it == bodies_.end())
            return;

        if (auto* world = world_.world())
        {
            if (it->second.body)
                world->removeRigidBody(it->second.body.get());
        }
        bodies_.erase(it);
        dirty_entities_.erase(entity);
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

            // Optimization: only push static/kinematic transforms when the authoring transform changed.
            if (runtime.local_version == tfm->local_version)
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
            runtime.local_version = tfm->local_version;
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

            // Optimization: skip sleeping bodies to avoid unnecessary write-backs.
            if (!runtime.body->isActive())
                continue;

            btTransform transform;
            runtime.body->getMotionState()->getWorldTransform(transform);

            // Write back into local transforms (hierarchy handling comes later).
            tfm->position = from_bt_vec3(transform.getOrigin(), settings_.units_per_meter);
            tfm->rotation = glm::normalize(from_bt_quat(transform.getRotation()));
            tfm->mark_local_dirty();
        }
    }

    struct PhysicsSystem::RaycastCallback final : public btCollisionWorld::ClosestRayResultCallback
    {
        RaycastCallback(
            const btVector3& from,
            const btVector3& to,
            const PhysicsSystem& system,
            const RaycastFilter& filter)
            : btCollisionWorld::ClosestRayResultCallback(from, to),
              system_(system),
              include_triggers_(filter.include_triggers)
        {
        }

        const PhysicsSystem& system_;
        bool include_triggers_ = true;
        entt::entity hit_entity = entt::null;
        ecs::ColliderId hit_collider = 0;
        bool hit_is_trigger = false;

        btScalar addSingleResult(btCollisionWorld::LocalRayResult& ray_result,
            bool normal_in_world_space) override
        {
            const auto* collision_object = ray_result.m_collisionObject;
            if (!collision_object)
                return m_closestHitFraction;

            const int user_index = collision_object->getUserIndex();
            if (user_index < 0)
                return m_closestHitFraction;

            const entt::entity entity = static_cast<entt::entity>(user_index);
            const auto runtime_it = system_.bodies_.find(entity);
            if (runtime_it == system_.bodies_.end())
                return m_closestHitFraction;

            int part_id = -1;
            if (ray_result.m_localShapeInfo)
                part_id = ray_result.m_localShapeInfo->m_shapePart;

            const auto collider = resolve_collider_info(runtime_it->second, part_id);
            if (!include_triggers_ && collider.is_trigger)
                return m_closestHitFraction;

            if (ray_result.m_hitFraction < m_closestHitFraction)
            {
                hit_entity = entity;
                hit_collider = collider.id;
                hit_is_trigger = collider.is_trigger;
            }

            return btCollisionWorld::ClosestRayResultCallback::addSingleResult(
                ray_result, normal_in_world_space);
        }
    };

    bool PhysicsSystem::raycast(const glm::vec3& origin,
        const glm::vec3& direction,
        float max_distance,
        RaycastHit& out_hit,
        const RaycastFilter& filter) const
    {
        out_hit = RaycastHit{};

        auto* world = world_.world();
        if (!world)
            return false;

        glm::vec3 dir = direction;
        const float dir_len = glm::length(dir);
        if (dir_len <= 0.0f)
            return false;

        dir /= dir_len;
        const float distance = max_distance > 0.0f ? max_distance : dir_len;
        if (distance <= 0.0f)
            return false;

        const glm::vec3 end = origin + dir * distance;
        const btVector3 from = to_bt_vec3(origin, settings_.units_per_meter);
        const btVector3 to = to_bt_vec3(end, settings_.units_per_meter);

        RaycastCallback callback(from, to, *this, filter);
        callback.m_collisionFilterGroup = static_cast<short>(filter.layer & 0xFFFFu);
        callback.m_collisionFilterMask = static_cast<short>(filter.mask & 0xFFFFu);

        world->rayTest(from, to, callback);
        if (!callback.hasHit())
            return false;

        out_hit.hit = true;
        out_hit.entity = ecs::Entity(callback.hit_entity);
        out_hit.collider_id = callback.hit_collider;
        out_hit.point = from_bt_vec3(callback.m_hitPointWorld, settings_.units_per_meter);
        out_hit.normal = from_bt_dir3(callback.m_hitNormalWorld);
        out_hit.distance = callback.m_closestHitFraction * distance;
        out_hit.is_trigger = callback.hit_is_trigger;
        return true;
    }

    PhysicsSystem::BodyRuntime::ColliderRuntimeInfo
    PhysicsSystem::resolve_collider_info(
        const BodyRuntime& runtime,
        int part_id)
    {
        if (runtime.collider_info.empty())
            return {};

        int index = part_id;
        if (index < 0 || index >= static_cast<int>(runtime.collider_info.size()))
            index = 0;

        return runtime.collider_info[static_cast<std::size_t>(index)];
    }

    void PhysicsSystem::clear_contact_events(entt::registry& registry)
    {
        if (event_entities_.empty())
            return;

        // Clear only the entities we touched last frame to avoid full registry scans.
        for (const auto entity : event_entities_)
        {
            if (!registry.valid(entity))
                continue;

            if (auto* events = registry.try_get<ecs::PhysicsEventsComponent>(entity))
                events->events.clear();
        }

        event_entities_.clear();
    }

    void PhysicsSystem::emit_contact_events(entt::registry& registry, EngineContext& ctx)
    {
        // Context is reserved for future logging/metrics without threading through globals.
        (void)ctx;
        auto* world = world_.world();
        if (!world)
            return;

        auto* dispatcher = world->getDispatcher();
        if (!dispatcher)
            return;

        current_contacts_.clear();
        const int manifold_count = dispatcher->getNumManifolds();
        if (manifold_count > 0)
            current_contacts_.reserve(static_cast<std::size_t>(manifold_count));

        // Build the current contact set from Bullet manifolds.
        for (int i = 0; i < manifold_count; ++i)
        {
            const btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
            if (!manifold)
                continue;

            const btCollisionObject* body0 = manifold->getBody0();
            const btCollisionObject* body1 = manifold->getBody1();
            if (!body0 || !body1)
                continue;

            const int id0 = body0->getUserIndex();
            const int id1 = body1->getUserIndex();
            if (id0 < 0 || id1 < 0)
                continue;

            const entt::entity entity0 = static_cast<entt::entity>(id0);
            const entt::entity entity1 = static_cast<entt::entity>(id1);

            auto runtime0_it = bodies_.find(entity0);
            auto runtime1_it = bodies_.find(entity1);
            if (runtime0_it == bodies_.end() || runtime1_it == bodies_.end())
                continue;

            const btManifoldPoint* point = select_contact_point(*manifold);
            if (!point)
                continue;

            const auto collider0 = resolve_collider_info(runtime0_it->second, point->m_partId0);
            const auto collider1 = resolve_collider_info(runtime1_it->second, point->m_partId1);
            // Trigger policy: if either collider is marked as trigger, treat the pair as trigger.
            const bool is_trigger = collider0.is_trigger || collider1.is_trigger;

            // Interest gating: only build contact records when at least one side requests events
            // for the current contact type (trigger vs collision).
            const auto* events0 = registry.try_get<ecs::PhysicsEventsComponent>(entity0);
            const auto* events1 = registry.try_get<ecs::PhysicsEventsComponent>(entity1);
            const bool wants0 = events0 && (is_trigger ? events0->emit_triggers : events0->emit_collisions);
            const bool wants1 = events1 && (is_trigger ? events1->emit_triggers : events1->emit_collisions);
            if (!wants0 && !wants1)
                continue;

            const btVector3 point_world_bt =
                (point->m_positionWorldOnA + point->m_positionWorldOnB) * btScalar(0.5f);
            glm::vec3 point_world = from_bt_vec3(point_world_bt, settings_.units_per_meter);
            glm::vec3 normal_world = from_bt_dir3(point->m_normalWorldOnB);
            const float impulse = point->getAppliedImpulse();

            ContactKey key{ entity0, entity1, collider0.id, collider1.id, is_trigger };
            if (should_swap_contact(entity0, entity1, collider0.id, collider1.id))
            {
                std::swap(key.entity_a, key.entity_b);
                std::swap(key.collider_a, key.collider_b);
                normal_world = -normal_world;
            }

            ContactInfo info{ point_world, normal_world, impulse };
            auto [it, inserted] = current_contacts_.emplace(key, info);
            if (!inserted && info.impulse > it->second.impulse)
            {
                // Prefer the strongest contact point for this pair.
                it->second = info;
            }
        }

        // Local helper to push events into components and sticky notes when requested.
        auto emit_event = [&](entt::entity self,
            entt::entity other,
            ecs::ColliderId self_collider,
            ecs::ColliderId other_collider,
            const ContactInfo& info,
            ecs::ContactPhase phase,
            bool is_trigger,
            bool invert_normal)
        {
            if (!registry.valid(self) || !registry.valid(other))
                return;

            auto* events = registry.try_get<ecs::PhysicsEventsComponent>(self);
            if (!events)
                return;

            // Trigger policy: trigger pairs are filtered by emit_triggers.
            if (is_trigger && !events->emit_triggers)
                return;
            // Trigger policy: non-trigger pairs are filtered by emit_collisions.
            if (!is_trigger && !events->emit_collisions)
                return;

            ecs::CollisionEvent event{};
            event.entity_a = ecs::Entity(self);
            event.entity_b = ecs::Entity(other);
            event.collider_id_a = self_collider;
            event.collider_id_b = other_collider;
            event.phase = phase;
            event.is_trigger = is_trigger;
            event.point = info.point;
            event.normal = invert_normal ? -info.normal : info.normal;
            event.impulse = info.impulse;

            events->events.push_back(event);
            event_entities_.insert(self);

            // Optional quick debug output via StickyNoteComponent.
            if (auto* note = registry.try_get<ecs::StickyNoteComponent>(self))
            {
                const char* phase_label = (phase == ecs::ContactPhase::Enter) ? "Enter"
                    : (phase == ecs::ContactPhase::Stay) ? "Stay"
                    : "Exit";
                const char* type_label = is_trigger ? "Trigger" : "Collision";

                std::string other_label;
                if (auto* header = registry.try_get<ecs::HeaderComponent>(other);
                    header && !header->name.empty())
                {
                    other_label = header->name;
                }
                else
                {
                    other_label = "Entity " + std::to_string(entt::to_integral(other));
                }

                std::string message;
                message.reserve(64);
                message.append(type_label);
                message.push_back(' ');
                message.append(phase_label);
                message.append(": ");
                message.append(other_label);

                if (other_collider != 0)
                {
                    message.append(" #");
                    message.append(std::to_string(other_collider));
                }

                if (phase == ecs::ContactPhase::Stay)
                    StickyNoteComponent_AppendStack(*note, message);
                else
                    StickyNoteComponent_Append(*note, message);
            }
        };

        // Emit enter/stay events for current contacts.
        for (const auto& [key, info] : current_contacts_)
        {
            const auto prev_it = previous_contacts_.find(key);
            const ecs::ContactPhase phase =
                (prev_it == previous_contacts_.end())
                ? ecs::ContactPhase::Enter
                : ecs::ContactPhase::Stay;

            emit_event(key.entity_a, key.entity_b, key.collider_a, key.collider_b,
                info, phase, key.is_trigger, false);
            emit_event(key.entity_b, key.entity_a, key.collider_b, key.collider_a,
                info, phase, key.is_trigger, true);
        }

        // Emit exit events for contacts that disappeared.
        for (const auto& [key, info] : previous_contacts_)
        {
            if (current_contacts_.find(key) != current_contacts_.end())
                continue;

            emit_event(key.entity_a, key.entity_b, key.collider_a, key.collider_b,
                info, ecs::ContactPhase::Exit, key.is_trigger, false);
            emit_event(key.entity_b, key.entity_a, key.collider_b, key.collider_a,
                info, ecs::ContactPhase::Exit, key.is_trigger, true);
        }

        // Carry current contacts forward for the next frame's enter/stay/exit tests.
        previous_contacts_.swap(current_contacts_);
    }

    void PhysicsSystem::handle_field_changed_event(const editor::FieldChangedEvent& event)
    {
        if (event.target.kind != editor::FieldTarget::Kind::Component)
            return;

        // We only care about physics-affecting component edits.
        const entt::id_type rb_id = entt::type_hash<ecs::RigidBodyComponent>::value();
        const entt::id_type collider_id = entt::type_hash<ecs::ColliderComponent>::value();
        const entt::id_type transform_id = entt::type_hash<ecs::TransformComponent>::value();
        const entt::id_type filter_id = entt::type_hash<ecs::CollisionFilterComponent>::value();

        const bool is_rb = (event.target.component_id == rb_id);
        const bool is_collider = (event.target.component_id == collider_id);
        const bool is_transform = (event.target.component_id == transform_id);
        const bool is_filter = (event.target.component_id == filter_id);
        if (!is_rb && !is_collider && !is_transform && !is_filter)
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

    void PhysicsSystem::handle_batch_task_event(const BatchTaskCompletedEvent& event)
    {
        // Batch load/unload changes the live entity set; force a sync on the next update.
        switch (event.type)
        {
        case BatchTaskType::Load:
        case BatchTaskType::LoadAll:
        case BatchTaskType::Unload:
        case BatchTaskType::UnloadAll:
            batch_sync_requested_ = true;
            break;
        default:
            break;
        }
    }

    void PhysicsSystem::on_rigidbody_construct(entt::registry& registry, entt::entity entity)
    {
        if (!ctx_)
            return;

        // Structural change: try to create immediately, otherwise mark for next sync.
        if (!create_body_for_entity(registry, *ctx_, entity, false))
            dirty_entities_.insert(entity);
    }

    void PhysicsSystem::on_rigidbody_destroy(entt::registry&, entt::entity entity)
    {
        // Structural change: remove Bullet body immediately if present.
        destroy_body_for_entity(entity);
    }

    void PhysicsSystem::on_collider_construct(entt::registry& registry, entt::entity entity)
    {
        if (!ctx_)
            return;

        // Structural change: try to create immediately, otherwise mark for next sync.
        if (!create_body_for_entity(registry, *ctx_, entity, false))
            dirty_entities_.insert(entity);
    }

    void PhysicsSystem::on_collider_destroy(entt::registry&, entt::entity entity)
    {
        // Structural change: remove Bullet body immediately if present.
        destroy_body_for_entity(entity);
    }

    void PhysicsSystem::on_transform_construct(entt::registry& registry, entt::entity entity)
    {
        if (!ctx_)
            return;

        // Structural change: try to create immediately, otherwise mark for next sync.
        if (!create_body_for_entity(registry, *ctx_, entity, false))
            dirty_entities_.insert(entity);
    }

    void PhysicsSystem::on_transform_destroy(entt::registry&, entt::entity entity)
    {
        // Structural change: remove Bullet body immediately if present.
        destroy_body_for_entity(entity);
    }

} // namespace eeng::ecs::systems
