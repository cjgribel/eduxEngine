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
#include "physics/MassProperties.hpp"
#include "assets/types/ModelAssets.hpp"
#include "assets/types/TerrainAssets.hpp"
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace
{
    struct BuiltShape
    {
        std::unique_ptr<btCollisionShape> shape;
        btTransform local_shape_transform = btTransform::getIdentity();
        std::vector<float> owned_height_samples;
    };

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

    btVector3 to_bt_force(const glm::vec3& v, float units_per_meter)
    {
        const float meters_per_unit = units_per_meter > 0.0f ? 1.0f / units_per_meter : 1.0f;
        return btVector3(v.x * meters_per_unit, v.y * meters_per_unit, v.z * meters_per_unit);
    }

    btVector3 to_bt_torque(const glm::vec3& v, float units_per_meter)
    {
        const float meters_per_unit = units_per_meter > 0.0f ? 1.0f / units_per_meter : 1.0f;
        const float scale = meters_per_unit * meters_per_unit;
        return btVector3(v.x * scale, v.y * scale, v.z * scale);
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

    // Convert glm vectors directly into Bullet without unit scaling.
    btVector3 to_bt_vec3_raw(const glm::vec3& v)
    {
        return btVector3(v.x, v.y, v.z);
    }

    btVector3 to_bt_dir(const glm::vec3& v)
    {
        return btVector3(v.x, v.y, v.z);
    }

    btVector3 normalize_or_default(const btVector3& v, const btVector3& fallback)
    {
        const btScalar len2 = v.length2();
        if (len2 <= btScalar(1e-12))
            return fallback;
        return v / btSqrt(len2);
    }

    btMatrix3x3 basis_from_axis(const btVector3& axis)
    {
        const btVector3 x = normalize_or_default(axis, btVector3(1.0f, 0.0f, 0.0f));
        const btVector3 up = (btFabs(x.dot(btVector3(0.0f, 1.0f, 0.0f))) > btScalar(0.99))
            ? btVector3(0.0f, 0.0f, 1.0f)
            : btVector3(0.0f, 1.0f, 0.0f);
        const btVector3 z = normalize_or_default(x.cross(up), btVector3(0.0f, 0.0f, 1.0f));
        const btVector3 y = normalize_or_default(z.cross(x), btVector3(0.0f, 1.0f, 0.0f));

        btMatrix3x3 basis;
        basis[0] = x;
        basis[1] = y;
        basis[2] = z;
        return basis;
    }

    // Check if a Bullet quaternion is close to identity.
    bool is_identity_quat(const btQuaternion& q)
    {
        constexpr float kEpsilon = 1e-4f;
        if (std::abs(q.w() - 1.0f) > kEpsilon)
            return false;
        return std::abs(q.x()) <= kEpsilon
            && std::abs(q.y()) <= kEpsilon
            && std::abs(q.z()) <= kEpsilon;
    }

    // Check if a COM transform is effectively identity (no offset/rotation).
    bool is_identity_com(const glm::vec3& position, const btQuaternion& rotation)
    {
        constexpr float kEpsilon = 1e-4f;
        const float pos_len = std::sqrt(
            position.x * position.x
            + position.y * position.y
            + position.z * position.z);
        if (pos_len > kEpsilon)
            return false;
        return is_identity_quat(rotation);
    }

    bool is_identity_transform(const btTransform& transform)
    {
        return transform.getOrigin().fuzzyZero() && is_identity_quat(transform.getRotation());
    }

    // Diagonalize a symmetric inertia tensor.
    // Returns a rotation matrix (body->pivot) and the diagonal inertia in that frame.
    bool diagonalize_inertia(const glm::mat3& inertia,
        btMatrix3x3& out_rotation,
        btVector3& out_diagonal)
    {
        const float trace = inertia[0][0] + inertia[1][1] + inertia[2][2];
        if (!std::isfinite(trace) || std::abs(trace) <= 1e-8f)
            return false;

        // glm is column-major; btMatrix3x3 expects row-major.
        btMatrix3x3 inertia_bt(
            inertia[0][0], inertia[1][0], inertia[2][0],
            inertia[0][1], inertia[1][1], inertia[2][1],
            inertia[0][2], inertia[1][2], inertia[2][2]);

        btMatrix3x3 rotation;
        inertia_bt.diagonalize(rotation, btScalar(1e-6f), 20);
        const btMatrix3x3 diag = rotation.transpose() * inertia_bt * rotation;

        out_rotation = rotation;
        out_diagonal = btVector3(diag[0][0], diag[1][1], diag[2][2]);
        return true;
    }

    std::optional<eeng::physics::MassProperties3d> compute_collider_mass_properties(
        const eeng::ecs::ColliderDesc& collider,
        const glm::vec3& entity_scale,
        float units_per_meter,
        eeng::EngineContext& ctx,
        float density)
    {
        const float meters_per_unit = units_per_meter > 0.0f ? 1.0f / units_per_meter : 1.0f;
        const glm::vec3 scale_m = entity_scale * meters_per_unit;
        const float scale_u = uniform_scale(entity_scale) * meters_per_unit;

        eeng::physics::MassProperties3d props{};
        bool has_props = false;

        switch (collider.type)
        {
        case eeng::ecs::ColliderType::Box:
        case eeng::ecs::ColliderType::AABB:
        {
            const glm::vec3 half_extents = collider.half_extents * scale_m;
            props = eeng::physics::mass_properties_box(half_extents, density);
            has_props = true;
            break;
        }
        case eeng::ecs::ColliderType::Sphere:
        {
            const float radius = std::max(0.0f, collider.radius * scale_u);
            props = eeng::physics::mass_properties_sphere(radius, density);
            has_props = true;
            break;
        }
        case eeng::ecs::ColliderType::Capsule:
        {
            const float radius = std::max(0.0f, collider.radius * scale_u);
            const float height = std::max(0.0f, collider.height * scale_u);
            props = eeng::physics::mass_properties_capsule_z(radius, height, density);
            has_props = true;
            break;
        }
        case eeng::ecs::ColliderType::ConvexHull:
        {
            auto* rm = eeng::try_get_resource_manager_ptr(ctx, "PhysicsSystem");
            if (!rm)
                break;

            bool used_fallback = false;
            bool read_ok = eeng::try_read_asset_ref(
                *rm,
                collider.mesh_ref,
                ctx,
                "PhysicsSystem",
                "Missing ModelDataAsset for convex hull collider.",
                [&](const eeng::assets::ModelDataAsset& model)
                {
                    const int sm_index = collider.submesh_index >= 0
                        ? collider.submesh_index
                        : 0;
                    if (sm_index < 0
                        || sm_index >= static_cast<int>(model.submeshes.size()))
                        return;

                    const auto& sm = model.submeshes[static_cast<std::size_t>(sm_index)];
                    if (sm.nbr_vertices == 0 || sm.nbr_indices == 0)
                        return;
                    if (sm.base_vertex + sm.nbr_vertices > model.positions.size())
                        return;
                    if (sm.base_index + sm.nbr_indices > model.indices.size())
                        return;

                    std::vector<glm::vec3> vertices;
                    vertices.reserve(sm.nbr_vertices);
                    for (std::size_t i = 0; i < sm.nbr_vertices; ++i)
                    {
                        vertices.push_back(model.positions[sm.base_vertex + i] * scale_m);
                    }

                    std::vector<std::uint32_t> indices;
                    indices.reserve(sm.nbr_indices);
                    for (std::size_t i = 0; i < sm.nbr_indices; ++i)
                    {
                        indices.push_back(model.indices[sm.base_index + i]);
                    }

                    props = eeng::physics::mass_properties_convex_mesh(
                        vertices.data(),
                        vertices.size(),
                        indices.data(),
                        indices.size(),
                        density);
                    has_props = (props.mass > 0.0f);
                });
            (void)read_ok;

            // If mesh data is unavailable, fall back to a simple box.
            if (!has_props)
            {
                const glm::vec3 half_extents = collider.half_extents * scale_m;
                props = eeng::physics::mass_properties_box(half_extents, density);
                has_props = true;
                used_fallback = true;
            }

            if (used_fallback)
            {
                EENG_LOG_WARN(&ctx,
                    "PhysicsSystem: ConvexHull collider %u using box mass properties (mesh data unavailable).",
                    collider.id);
            }
            break;
        }
        case eeng::ecs::ColliderType::Heightfield:
        {
            auto* rm = eeng::try_get_resource_manager_ptr(ctx, "PhysicsSystem");
            if (!rm)
                break;

            bool read_ok = eeng::try_read_asset_ref(
                *rm,
                collider.terrain_chunk_ref,
                ctx,
                "PhysicsSystem",
                "Missing TerrainChunkAsset for heightfield collider.",
                [&](const eeng::assets::TerrainChunkAsset& chunk)
                {
                    const glm::vec3 chunk_size = chunk.local_bounds_max - chunk.local_bounds_min;
                    const glm::vec3 half_extents = glm::max(chunk_size * 0.5f * scale_m, glm::vec3(0.01f));
                    props = eeng::physics::mass_properties_box(half_extents, density);
                    has_props = true;
                });
            (void)read_ok;

            if (!has_props)
            {
                const glm::vec3 half_extents = collider.half_extents * scale_m;
                props = eeng::physics::mass_properties_box(half_extents, density);
                has_props = true;
            }
            break;
        }
        case eeng::ecs::ColliderType::TriangleMesh:
        default:
        {
            // Triangle meshes may be non-convex; use a conservative box approximation for now.
            const glm::vec3 half_extents = collider.half_extents * scale_m;
            props = eeng::physics::mass_properties_box(half_extents, density);
            has_props = true;
            break;
        }
        }

        if (!has_props)
            return std::nullopt;

        // Move inertia/COM into the entity pivot frame.
        const glm::vec3 local_pos = collider.local_position * entity_scale * meters_per_unit;
        props = eeng::physics::transform_mass_properties(props, local_pos, collider.local_rotation);
        return props;
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
    BuiltShape build_shape(
        const eeng::ecs::ColliderDesc& collider,
        const glm::vec3& entity_scale,
        float units_per_meter,
        eeng::EngineContext& ctx)
    {
        BuiltShape out{};
        const float meters_per_unit = units_per_meter > 0.0f ? 1.0f / units_per_meter : 1.0f;
        const glm::vec3 scaled_extents = collider.half_extents * entity_scale * meters_per_unit;
        const float scale_u = uniform_scale(entity_scale) * meters_per_unit;

        switch (collider.type)
        {
        case eeng::ecs::ColliderType::Box:
        case eeng::ecs::ColliderType::AABB:
        {
            const btVector3 half_extents(scaled_extents.x, scaled_extents.y, scaled_extents.z);
            out.shape = std::make_unique<btBoxShape>(half_extents);
            return out;
        }
        case eeng::ecs::ColliderType::Sphere:
        {
            const float radius = std::max(0.0f, collider.radius * scale_u);
            out.shape = std::make_unique<btSphereShape>(radius);
            return out;
        }
        case eeng::ecs::ColliderType::Capsule:
        {
            // Our debug capsule uses height as the cylinder length; Bullet does the same.
            const float radius = std::max(0.0f, collider.radius * scale_u);
            const float height = std::max(0.0f, collider.height * scale_u);
            out.shape = std::make_unique<btCapsuleShapeZ>(radius, height);
            return out;
        }
        case eeng::ecs::ColliderType::Heightfield:
        {
            auto* rm = eeng::try_get_resource_manager_ptr(ctx, "PhysicsSystem");
            if (!rm)
                break;

            bool built_heightfield = false;
            eeng::try_read_asset_ref(
                *rm,
                collider.terrain_chunk_ref,
                ctx,
                "PhysicsSystem",
                "Missing TerrainChunkAsset for heightfield collider.",
                [&](const eeng::assets::TerrainChunkAsset& chunk)
                {
                    const std::size_t sample_count =
                        static_cast<std::size_t>(chunk.samples_x)
                        * static_cast<std::size_t>(chunk.samples_z);
                    if (chunk.samples_x < 2
                        || chunk.samples_z < 2
                        || sample_count == 0
                        || chunk.heights.size() != sample_count)
                    {
                        EENG_LOG_WARN(&ctx,
                            "PhysicsSystem: Heightfield collider %u has invalid TerrainChunkAsset sample data; using box fallback.",
                            collider.id);
                        return;
                    }

                    // Bullet keeps a raw pointer to the sample memory, so we
                    // move an owned copy into BodyRuntime after shape creation.
                    out.owned_height_samples = chunk.heights;

                    const float scale_x = entity_scale.x * meters_per_unit;
                    const float scale_y = entity_scale.y * meters_per_unit;
                    const float scale_z = entity_scale.z * meters_per_unit;

                    const float min_height = chunk.min_height;
                    const float max_height = chunk.max_height;

                    auto heightfield = std::make_unique<btHeightfieldTerrainShape>(
                        static_cast<int>(chunk.samples_x),
                        static_cast<int>(chunk.samples_z),
                        out.owned_height_samples.data(),
                        min_height,
                        max_height,
                        1,
                        false);

                    heightfield->setUseDiamondSubdivision(true);
                    heightfield->setLocalScaling(btVector3(
                        chunk.cell_size_x * scale_x,
                        scale_y,
                        chunk.cell_size_z * scale_z));

                    const float width = static_cast<float>(chunk.samples_x - 1) * chunk.cell_size_x * scale_x;
                    const float length = static_cast<float>(chunk.samples_z - 1) * chunk.cell_size_z * scale_z;
                    const float center_height = 0.5f * (min_height + max_height) * scale_y;

                    // Bullet heightfields are centered on their internal AABB.
                    // We shift them back into our chunk-corner convention here
                    // so chunk.world_origin/local_position can refer to the
                    // chunk's minimum X/Z corner instead of Bullet's center.
                    out.local_shape_transform.setIdentity();
                    out.local_shape_transform.setOrigin(btVector3(
                        chunk.local_bounds_min.x * scale_x + width * 0.5f,
                        center_height,
                        chunk.local_bounds_min.z * scale_z + length * 0.5f));

                    out.shape = std::move(heightfield);
                    built_heightfield = true;
                });

            if (built_heightfield)
                return out;

            EENG_LOG_WARN(&ctx,
                "PhysicsSystem: Heightfield collider %u using box fallback.",
                collider.id);
            [[fallthrough]];
        }
        case eeng::ecs::ColliderType::ConvexHull:
        case eeng::ecs::ColliderType::TriangleMesh:
        default:
        {
            // Placeholder: use a box sized by half_extents when mesh data isn't cooked yet.
            const btVector3 half_extents(scaled_extents.x, scaled_extents.y, scaled_extents.z);
            out.shape = std::make_unique<btBoxShape>(half_extents);
            return out;
        }
        }

        return out;
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

        // Bullet asserts if a rigid body is deleted while constraints still reference it.
        destroy_all_constraints();

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
        force_requests_.clear();
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

        // Bullet asserts if a rigid body is deleted while constraints still reference it.
        destroy_all_constraints();

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
        force_requests_.clear();
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

        // Apply cached forces before stepping the simulation.
        apply_force_requests();

        // Step the simulation.
        world_.step_simulation(delta_time);

        // Gather contact events from Bullet for enter/stay/exit classification.
        emit_contact_events(registry, ctx);

        // Pull transforms back for dynamic bodies.
        sync_transforms_from_bullet(registry);
    }

    void PhysicsSystem::update_edit(entt::registry& registry, EngineContext& ctx)
    {
        if (!initialized_)
            return;

        // In edit mode we only rebuild bodies/COM/inertia; no stepping or force application.
        if (batch_sync_requested_ || !dirty_entities_.empty())
        {
            sync_bodies(registry, ctx);
            batch_sync_requested_ = false;
        }
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
                remove_constraints_for_entity(entity);
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
                    remove_constraints_for_entity(entity);
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
        auto& rb = registry.get<ecs::RigidBodyComponent>(entity);
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

        // Compute unit-density mass properties for COM and inertia.
        // We prefer analytic formulas and aggregate them with the parallel axis theorem.
        physics::MassProperties3d unit_props{};
        auto accumulate_props = [&](bool include_triggers)
        {
            bool has_props = false;
            for (const auto& collider : colliders.colliders)
            {
                if (!include_triggers && collider.is_trigger)
                    continue;

                auto props_opt = compute_collider_mass_properties(
                    collider,
                    tfm.scale,
                    settings_.units_per_meter,
                    ctx,
                    1.0f);
                if (!props_opt)
                    continue;

                if (!has_props)
                {
                    unit_props = *props_opt;
                    has_props = true;
                }
                else
                {
                    unit_props = physics::combine(unit_props, *props_opt);
                }
            }
            return has_props;
        };

        // Prefer solid colliders for mass; fall back to triggers if needed.
        if (!accumulate_props(false))
            accumulate_props(true);

        const bool is_dynamic = (rb.motion == ecs::PhysicsMotionType::Dynamic);
        const bool is_kinematic = (rb.motion == ecs::PhysicsMotionType::Kinematic);

        const float unit_mass = unit_props.mass;
        // COM is already in Bullet space (meters) because inputs were scaled before aggregation.
        const glm::vec3 com_local_pos = unit_props.center_of_mass;

        float mass = is_dynamic ? rb.mass : 0.0f;
        if (is_dynamic)
        {
            if (rb.auto_mass)
            {
                // Unit-density mass scales linearly with density.
                mass = unit_mass * rb.density;
            }

            if (mass <= 0.0f)
            {
                // Safety fallback to keep Bullet stable if the mass is invalid.
                mass = rb.mass > 0.0f ? rb.mass : 1.0f;
            }

            if (rb.auto_mass)
                rb.mass = mass;
        }

        const float mass_scale = (unit_mass > 0.0f && mass > 0.0f)
            ? (mass / unit_mass)
            : 0.0f;

        // Principal axes rotation for the inertia tensor (body->pivot).
        btMatrix3x3 principal_rotation;
        principal_rotation.setIdentity();
        btVector3 inertia_diag_unit(0.0f, 0.0f, 0.0f);
        const bool has_principal = rb.auto_inertia
            && diagonalize_inertia(unit_props.inertia, principal_rotation, inertia_diag_unit);

        btQuaternion com_rotation(0.0f, 0.0f, 0.0f, 1.0f);
        if (has_principal)
            principal_rotation.getRotation(com_rotation);

        // Cache COM/principal axes in component space (engine units) for debug/inspection.
        rb.com_local_position = com_local_pos * settings_.units_per_meter;
        rb.com_local_rotation = glm::normalize(from_bt_quat(com_rotation));

        btVector3 inertia_diag(0.0f, 0.0f, 0.0f);
        if (is_dynamic && mass > 0.0f)
        {
            if (rb.auto_inertia && has_principal && mass_scale > 0.0f)
            {
                // Inertia scales linearly with mass for a fixed geometry.
                inertia_diag = inertia_diag_unit * mass_scale;
                // Editor-friendly: store the computed diagonal in engine units for inspection.
                const float units_per_meter = settings_.units_per_meter > 0.0f
                    ? settings_.units_per_meter
                    : 1.0f;
                const float scale = units_per_meter * units_per_meter;
                rb.inertia = glm::vec3(
                    inertia_diag.x() * scale,
                    inertia_diag.y() * scale,
                    inertia_diag.z() * scale);
            }
            else if (!rb.auto_inertia)
            {
                // Manual inertia is stored in engine units; convert to meters^2.
                const float meters_per_unit = settings_.units_per_meter > 0.0f
                    ? 1.0f / settings_.units_per_meter
                    : 1.0f;
                const float scale = meters_per_unit * meters_per_unit;
                inertia_diag = btVector3(
                    rb.inertia.x * scale,
                    rb.inertia.y * scale,
                    rb.inertia.z * scale);
            }
        }
        // Guard against small negative diagonals from numerical noise.
        inertia_diag = btVector3(
            std::max(0.0f, inertia_diag.x()),
            std::max(0.0f, inertia_diag.y()),
            std::max(0.0f, inertia_diag.z()));

        BodyRuntime runtime{};
        runtime.com_local.setIdentity();
        runtime.com_local.setOrigin(to_bt_vec3_raw(com_local_pos));
        runtime.com_local.setRotation(com_rotation);
        runtime.com_local_inverse = runtime.com_local.inverse();

        // Optimization: skip btCompoundShape only when both collider and COM are identity.
        // Heightfields used to force the compound path solely to carry an
        // internal shape offset. We now keep that offset explicitly on the
        // BodyRuntime so single heightfields can stay as root shapes.
        const bool single_collider = (colliders.colliders.size() == 1);
        const bool single_identity =
            single_collider && is_identity_collider_transform(colliders.colliders.front());
        const bool com_identity = is_identity_com(com_local_pos, com_rotation);
        const bool use_compound =
            !(single_collider && single_identity && com_identity);

        if (use_compound)
            runtime.compound_shape = std::make_unique<btCompoundShape>();

        bool has_trigger = false;
        bool has_solid = false;

        // Build collision shapes (rebuilds are driven by the dirty set).
        for (const auto& collider : colliders.colliders)
        {
            auto built_shape = build_shape(collider, tfm.scale, settings_.units_per_meter, ctx);
            if (!built_shape.shape)
                continue;

            if (!built_shape.owned_height_samples.empty())
                runtime.heightfield_samples.push_back(std::move(built_shape.owned_height_samples));

            if (use_compound)
            {
                btTransform local;
                local.setIdentity();
                // Collider offsets should respect entity scale in the same way as size.
                local.setOrigin(to_bt_vec3(collider.local_position * tfm.scale, settings_.units_per_meter));
                local.setRotation(to_bt_quat(collider.local_rotation));

                // Some Bullet shapes, such as heightfields, are internally
                // centered differently than our chunk authoring convention. The
                // shape-local transform lets the builder correct that without
                // leaking Bullet-specific offsets into serialized collider data.
                local = local * built_shape.local_shape_transform;

                // Shift the child into COM/principal-axis space for a stable body frame.
                const btTransform shifted = runtime.com_local_inverse * local;
                runtime.compound_shape->addChildShape(shifted, built_shape.shape.get());
                runtime.child_shapes.emplace_back(std::move(built_shape.shape));
                // Track collider metadata by child index so contact events can resolve ids.
                runtime.collider_info.push_back(
                    BodyRuntime::ColliderRuntimeInfo{ collider.id, collider.is_trigger });
            }
            else
            {
                // Single collider with identity serialized local transform: no
                // compound required. Keep any builder-provided root-shape
                // offset so centered Bullet shapes, such as heightfields, land
                // at the same place as the authored render chunk.
                runtime.root_shape = std::move(built_shape.shape);
                runtime.root_shape_local = built_shape.local_shape_transform;
                runtime.root_shape_local_inverse = runtime.root_shape_local.inverse();
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
        btTransform pivot_transform;
        pivot_transform.setIdentity();
        pivot_transform.setOrigin(to_bt_vec3(tfm.position, settings_.units_per_meter));
        pivot_transform.setRotation(to_bt_quat(tfm.rotation));

        const btTransform start_transform =
            pivot_transform * runtime.com_local * runtime.root_shape_local;
        runtime.motion_state = std::make_unique<btDefaultMotionState>(start_transform);

        btCollisionShape* collision_shape = runtime.compound_shape
            ? static_cast<btCollisionShape*>(runtime.compound_shape.get())
            : static_cast<btCollisionShape*>(runtime.root_shape.get());
        if (!collision_shape)
            return false;

        btRigidBody::btRigidBodyConstructionInfo info(
            mass,
            runtime.motion_state.get(),
            collision_shape,
            inertia_diag);

        runtime.body = std::make_unique<btRigidBody>(info);

        if (const auto* material = registry.try_get<ecs::PhysicsMaterialComponent>(entity))
        {
            runtime.body->setFriction(material->material.friction);
            runtime.body->setRestitution(material->material.restitution);
        }

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

        remove_constraints_for_entity(entity);

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

            // Body lives at COM/principal axes; pivot is the authoring transform.
            const btTransform body_transform =
                transform * runtime.com_local * runtime.root_shape_local;
            runtime.body->setWorldTransform(body_transform);
            runtime.body->getMotionState()->setWorldTransform(body_transform);
            runtime.body->setInterpolationWorldTransform(body_transform);
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

            btTransform body_transform;
            runtime.body->getMotionState()->getWorldTransform(body_transform);

            // Convert from COM/principal-axes frame back to the authoring pivot.
            const btTransform pivot_transform =
                body_transform * runtime.root_shape_local_inverse * runtime.com_local_inverse;

            // Write back into local transforms (hierarchy handling comes later).
            tfm->position = from_bt_vec3(pivot_transform.getOrigin(), settings_.units_per_meter);
            tfm->rotation = glm::normalize(from_bt_quat(pivot_transform.getRotation()));
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

    void PhysicsSystem::submit_force(
        entt::entity entity,
        const glm::vec3& force,
        const glm::vec3& point_world)
    {
        force_requests_.push_back(ForceRequest{
            entity,
            force,
            point_world,
            ForceRequest::Type::ForceAtPoint
        });
    }

    void PhysicsSystem::submit_torque(
        entt::entity entity,
        const glm::vec3& torque)
    {
        force_requests_.push_back(ForceRequest{
            entity,
            torque,
            glm::vec3(0.0f),
            ForceRequest::Type::Torque
        });
    }

    void PhysicsSystem::submit_impulse(
        entt::entity entity,
        const glm::vec3& impulse,
        const glm::vec3& point_world)
    {
        force_requests_.push_back(ForceRequest{
            entity,
            impulse,
            point_world,
            ForceRequest::Type::ImpulseAtPoint
        });
    }

    void PhysicsSystem::wake_body(entt::entity entity)
    {
        auto it = bodies_.find(entity);
        if (it == bodies_.end() || !it->second.body)
            return;

        it->second.body->activate(true);
    }

    bool PhysicsSystem::get_body_state(entt::entity entity, BodyState& out_state) const
    {
        auto it = bodies_.find(entity);
        if (it == bodies_.end() || !it->second.body)
            return false;

        const btRigidBody* body = it->second.body.get();
        const btTransform& tfm = body->getWorldTransform();

        out_state.position = from_bt_vec3(tfm.getOrigin(), settings_.units_per_meter);
        out_state.rotation = glm::normalize(from_bt_quat(tfm.getRotation()));
        out_state.linear_velocity = from_bt_vec3(body->getLinearVelocity(), settings_.units_per_meter);
        out_state.angular_velocity = from_bt_dir3(body->getAngularVelocity());
        return true;
    }

    PhysicsSystem::ConstraintHandle PhysicsSystem::create_point_constraint(const PointConstraintDesc& desc)
    {
        auto* world = world_.world();
        if (!world)
            return 0;

        auto it_a = bodies_.find(desc.entity_a);
        if (it_a == bodies_.end() || !it_a->second.body)
            return 0;

        btRigidBody* body_a = it_a->second.body.get();
        const btVector3 pivot_a =
            it_a->second.com_local_inverse * to_bt_vec3(desc.local_anchor_a, settings_.units_per_meter);

        if (desc.entity_b == entt::null)
            return 0;
        auto it_b = bodies_.find(desc.entity_b);
        if (it_b == bodies_.end() || !it_b->second.body)
            return 0;
        btRigidBody* body_b = it_b->second.body.get();
        const btVector3 pivot_b =
            it_b->second.com_local_inverse * to_bt_vec3(desc.local_anchor_b, settings_.units_per_meter);

        auto constraint = std::make_unique<btPoint2PointConstraint>(
            *body_a, *body_b, pivot_a, pivot_b);

        world->addConstraint(constraint.get(), desc.disable_collisions);

        const ConstraintHandle handle = next_constraint_handle_++;
        constraints_.emplace(handle, ConstraintRuntime{
            ConstraintKind::Point,
            desc.entity_a,
            desc.entity_b,
            desc.disable_collisions,
            std::move(constraint)
        });
        return handle;
    }

    PhysicsSystem::ConstraintHandle PhysicsSystem::create_hinge_constraint(const HingeConstraintDesc& desc)
    {
        auto* world = world_.world();
        if (!world)
            return 0;

        auto it_a = bodies_.find(desc.entity_a);
        if (it_a == bodies_.end() || !it_a->second.body)
            return 0;

        btRigidBody* body_a = it_a->second.body.get();
        const btVector3 pivot_a =
            it_a->second.com_local_inverse * to_bt_vec3(desc.local_anchor_a, settings_.units_per_meter);
        const btVector3 axis_a = normalize_or_default(
            it_a->second.com_local_inverse.getBasis() * to_bt_dir(desc.local_axis_a),
            btVector3(0.0f, 1.0f, 0.0f));

        if (desc.entity_b == entt::null)
            return 0;
        auto it_b = bodies_.find(desc.entity_b);
        if (it_b == bodies_.end() || !it_b->second.body)
            return 0;
        btRigidBody* body_b = it_b->second.body.get();
        const btVector3 pivot_b =
            it_b->second.com_local_inverse * to_bt_vec3(desc.local_anchor_b, settings_.units_per_meter);
        const btVector3 axis_b = normalize_or_default(
            it_b->second.com_local_inverse.getBasis() * to_bt_dir(desc.local_axis_b),
            btVector3(0.0f, 1.0f, 0.0f));

        auto constraint = std::make_unique<btHingeConstraint>(
            *body_a, *body_b, pivot_a, pivot_b, axis_a, axis_b, true);

        if (desc.use_limits)
            constraint->setLimit(desc.limit_min, desc.limit_max);
        constraint->enableMotor(desc.enable_motor);
        constraint->setMotorTargetVelocity(desc.motor_target_velocity);
        constraint->setMaxMotorImpulse(desc.motor_max_impulse);

        world->addConstraint(constraint.get(), desc.disable_collisions);

        const ConstraintHandle handle = next_constraint_handle_++;
        constraints_.emplace(handle, ConstraintRuntime{
            ConstraintKind::Hinge,
            desc.entity_a,
            desc.entity_b,
            desc.disable_collisions,
            std::move(constraint)
        });
        return handle;
    }

    PhysicsSystem::ConstraintHandle PhysicsSystem::create_slider_constraint(const SliderConstraintDesc& desc)
    {
        auto* world = world_.world();
        if (!world)
            return 0;

        auto it_a = bodies_.find(desc.entity_a);
        if (it_a == bodies_.end() || !it_a->second.body)
            return 0;

        btRigidBody* body_a = it_a->second.body.get();

        const btVector3 pivot_a = to_bt_vec3(desc.local_anchor_a, settings_.units_per_meter);
        const btMatrix3x3 basis_a = basis_from_axis(to_bt_dir(desc.local_axis_a));
        btTransform frame_a;
        frame_a.setIdentity();
        frame_a.setOrigin(pivot_a);
        frame_a.setBasis(basis_a);
        frame_a = it_a->second.com_local_inverse * frame_a;

        if (desc.entity_b == entt::null)
            return 0;
        auto it_b = bodies_.find(desc.entity_b);
        if (it_b == bodies_.end() || !it_b->second.body)
            return 0;
        btRigidBody* body_b = it_b->second.body.get();
        btTransform frame_b;
        frame_b.setIdentity();
        const btVector3 pivot_b = to_bt_vec3(desc.local_anchor_b, settings_.units_per_meter);
        const btMatrix3x3 basis_b = basis_from_axis(to_bt_dir(desc.local_axis_b));
        frame_b.setOrigin(pivot_b);
        frame_b.setBasis(basis_b);
        frame_b = it_b->second.com_local_inverse * frame_b;

        auto constraint = std::make_unique<btSliderConstraint>(
            *body_a, *body_b, frame_a, frame_b, true);

        constraint->setLowerLinLimit(desc.linear_limit_min * world_.meters_per_unit());
        constraint->setUpperLinLimit(desc.linear_limit_max * world_.meters_per_unit());
        constraint->setLowerAngLimit(desc.angular_limit_min);
        constraint->setUpperAngLimit(desc.angular_limit_max);
        constraint->setPoweredLinMotor(desc.enable_linear_motor);
        constraint->setTargetLinMotorVelocity(desc.linear_motor_target_velocity * world_.meters_per_unit());
        constraint->setMaxLinMotorForce(desc.linear_motor_max_force);

        world->addConstraint(constraint.get(), desc.disable_collisions);

        const ConstraintHandle handle = next_constraint_handle_++;
        constraints_.emplace(handle, ConstraintRuntime{
            ConstraintKind::Slider,
            desc.entity_a,
            desc.entity_b,
            desc.disable_collisions,
            std::move(constraint)
        });
        return handle;
    }

    PhysicsSystem::ConstraintHandle PhysicsSystem::create_sixdof_spring_constraint(const SixDofSpringConstraintDesc& desc)
    {
        auto* world = world_.world();
        if (!world)
            return 0;

        auto it_a = bodies_.find(desc.entity_a);
        if (it_a == bodies_.end() || !it_a->second.body)
            return 0;

        btRigidBody* body_a = it_a->second.body.get();

        btTransform frame_a;
        frame_a.setIdentity();
        frame_a.setOrigin(to_bt_vec3(desc.local_anchor_a, settings_.units_per_meter));
        frame_a.setRotation(to_bt_quat(desc.local_rotation_a));
        frame_a = it_a->second.com_local_inverse * frame_a;

        if (desc.entity_b == entt::null)
            return 0;
        auto it_b = bodies_.find(desc.entity_b);
        if (it_b == bodies_.end() || !it_b->second.body)
            return 0;
        btRigidBody* body_b = it_b->second.body.get();
        btTransform frame_b;
        frame_b.setIdentity();
        frame_b.setOrigin(to_bt_vec3(desc.local_anchor_b, settings_.units_per_meter));
        frame_b.setRotation(to_bt_quat(desc.local_rotation_b));
        frame_b = it_b->second.com_local_inverse * frame_b;

        auto constraint = std::make_unique<btGeneric6DofSpring2Constraint>(
            *body_a, *body_b, frame_a, frame_b);

        const btVector3 linear_lower = to_bt_vec3(desc.linear_limit_min, settings_.units_per_meter);
        const btVector3 linear_upper = to_bt_vec3(desc.linear_limit_max, settings_.units_per_meter);
        constraint->setLinearLowerLimit(linear_lower);
        constraint->setLinearUpperLimit(linear_upper);
        constraint->setAngularLowerLimit(to_bt_vec3_raw(desc.angular_limit_min));
        constraint->setAngularUpperLimit(to_bt_vec3_raw(desc.angular_limit_max));

        for (int axis = 0; axis < 3; ++axis)
        {
            const float stiff = desc.linear_stiffness[axis];
            const float damp = desc.linear_damping[axis];
            const bool enable = (stiff != 0.0f || damp != 0.0f);
            constraint->enableSpring(axis, enable);
            if (enable)
            {
                constraint->setStiffness(axis, stiff);
                constraint->setDamping(axis, damp);
            }
        }
        for (int axis = 0; axis < 3; ++axis)
        {
            const float stiff = desc.angular_stiffness[axis];
            const float damp = desc.angular_damping[axis];
            const bool enable = (stiff != 0.0f || damp != 0.0f);
            constraint->enableSpring(axis + 3, enable);
            if (enable)
            {
                constraint->setStiffness(axis + 3, stiff);
                constraint->setDamping(axis + 3, damp);
            }
        }
        bool any_equilibrium = false;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (desc.linear_equilibrium_enabled[axis] > 0.5f)
            {
                any_equilibrium = true;
                break;
            }
        }
        if (any_equilibrium)
        {
            const float meters_per_unit = world_.meters_per_unit();
            for (int axis = 0; axis < 3; ++axis)
            {
                if (desc.linear_equilibrium_enabled[axis] > 0.5f)
                {
                    constraint->setEquilibriumPoint(
                        axis,
                        desc.linear_equilibrium_target[axis] * meters_per_unit);
                }
            }
        }
        else
        {
            constraint->setEquilibriumPoint();
        }

        const float meters_per_unit = world_.meters_per_unit();
        for (int axis = 0; axis < 3; ++axis)
        {
            const bool enable = desc.linear_motor_enabled[axis] > 0.5f;
            const bool servo = desc.linear_servo_enabled[axis] > 0.5f;
            constraint->enableMotor(axis, enable);
            constraint->setServo(axis, servo);
            constraint->setTargetVelocity(axis, desc.linear_motor_target_velocity[axis] * meters_per_unit);
            constraint->setMaxMotorForce(axis, desc.linear_motor_max_force[axis]);
            if (servo)
                constraint->setServoTarget(axis, desc.linear_servo_target[axis] * meters_per_unit);
        }
        for (int axis = 0; axis < 3; ++axis)
        {
            const int motor_axis = axis + 3;
            const bool enable = desc.angular_motor_enabled[axis] > 0.5f;
            const bool servo = desc.angular_servo_enabled[axis] > 0.5f;
            constraint->enableMotor(motor_axis, enable);
            constraint->setServo(motor_axis, servo);
            constraint->setTargetVelocity(motor_axis, desc.angular_motor_target_velocity[axis]);
            constraint->setMaxMotorForce(motor_axis, desc.angular_motor_max_force[axis]);
            if (servo)
                constraint->setServoTarget(motor_axis, desc.angular_servo_target[axis]);
        }

        world->addConstraint(constraint.get(), desc.disable_collisions);

        const ConstraintHandle handle = next_constraint_handle_++;
        constraints_.emplace(handle, ConstraintRuntime{
            ConstraintKind::SixDofSpring,
            desc.entity_a,
            desc.entity_b,
            desc.disable_collisions,
            std::move(constraint)
        });
        return handle;
    }

    bool PhysicsSystem::update_point_constraint(ConstraintHandle handle, const PointConstraintDesc& desc)
    {
        auto it = constraints_.find(handle);
        if (it == constraints_.end() || it->second.kind != ConstraintKind::Point)
            return false;

        if (it->second.entity_a != desc.entity_a || it->second.entity_b != desc.entity_b)
            return false;
        if (it->second.disable_collisions != desc.disable_collisions)
            return false;

        auto* constraint = static_cast<btPoint2PointConstraint*>(it->second.constraint.get());
        if (!constraint)
            return false;

        auto it_a = bodies_.find(desc.entity_a);
        if (it_a == bodies_.end() || !it_a->second.body)
            return false;

        const btVector3 pivot_a =
            it_a->second.com_local_inverse * to_bt_vec3(desc.local_anchor_a, settings_.units_per_meter);
        constraint->setPivotA(pivot_a);

        auto it_b = bodies_.find(desc.entity_b);
        if (it_b == bodies_.end() || !it_b->second.body)
            return false;
        const btVector3 pivot_b =
            it_b->second.com_local_inverse * to_bt_vec3(desc.local_anchor_b, settings_.units_per_meter);
        constraint->setPivotB(pivot_b);

        return true;
    }

    bool PhysicsSystem::update_hinge_constraint(ConstraintHandle handle, const HingeConstraintDesc& desc)
    {
        auto it = constraints_.find(handle);
        if (it == constraints_.end() || it->second.kind != ConstraintKind::Hinge)
            return false;

        if (it->second.entity_a != desc.entity_a || it->second.entity_b != desc.entity_b)
            return false;
        if (it->second.disable_collisions != desc.disable_collisions)
            return false;

        auto* constraint = static_cast<btHingeConstraint*>(it->second.constraint.get());
        if (!constraint)
            return false;

        if (desc.use_limits)
            constraint->setLimit(desc.limit_min, desc.limit_max);
        constraint->enableMotor(desc.enable_motor);
        constraint->setMotorTargetVelocity(desc.motor_target_velocity);
        constraint->setMaxMotorImpulse(desc.motor_max_impulse);
        return true;
    }

    bool PhysicsSystem::update_slider_constraint(ConstraintHandle handle, const SliderConstraintDesc& desc)
    {
        auto it = constraints_.find(handle);
        if (it == constraints_.end() || it->second.kind != ConstraintKind::Slider)
            return false;

        if (it->second.entity_a != desc.entity_a || it->second.entity_b != desc.entity_b)
            return false;
        if (it->second.disable_collisions != desc.disable_collisions)
            return false;

        auto* constraint = static_cast<btSliderConstraint*>(it->second.constraint.get());
        if (!constraint)
            return false;

        auto it_a = bodies_.find(desc.entity_a);
        if (it_a == bodies_.end() || !it_a->second.body)
            return false;
        auto it_b = bodies_.find(desc.entity_b);
        if (it_b == bodies_.end() || !it_b->second.body)
            return false;

        {
            btTransform frame_a;
            frame_a.setIdentity();
            frame_a.setOrigin(to_bt_vec3(desc.local_anchor_a, settings_.units_per_meter));
            frame_a.setBasis(basis_from_axis(to_bt_dir(desc.local_axis_a)));
            frame_a = it_a->second.com_local_inverse * frame_a;

            btTransform frame_b;
            frame_b.setIdentity();
            frame_b.setOrigin(to_bt_vec3(desc.local_anchor_b, settings_.units_per_meter));
            frame_b.setBasis(basis_from_axis(to_bt_dir(desc.local_axis_b)));
            frame_b = it_b->second.com_local_inverse * frame_b;

            constraint->setFrames(frame_a, frame_b);
        }

        constraint->setLowerLinLimit(desc.linear_limit_min * world_.meters_per_unit());
        constraint->setUpperLinLimit(desc.linear_limit_max * world_.meters_per_unit());
        constraint->setLowerAngLimit(desc.angular_limit_min);
        constraint->setUpperAngLimit(desc.angular_limit_max);
        constraint->setPoweredLinMotor(desc.enable_linear_motor);
        constraint->setTargetLinMotorVelocity(desc.linear_motor_target_velocity * world_.meters_per_unit());
        constraint->setMaxLinMotorForce(desc.linear_motor_max_force);

        if (desc.enable_linear_motor && desc.linear_motor_max_force > 0.0f)
        {
            it_a->second.body->activate(true);
            it_b->second.body->activate(true);
        }
        return true;
    }

    bool PhysicsSystem::update_sixdof_spring_constraint(ConstraintHandle handle, const SixDofSpringConstraintDesc& desc)
    {
        auto it = constraints_.find(handle);
        if (it == constraints_.end() || it->second.kind != ConstraintKind::SixDofSpring)
            return false;

        if (it->second.entity_a != desc.entity_a || it->second.entity_b != desc.entity_b)
            return false;
        if (it->second.disable_collisions != desc.disable_collisions)
            return false;

        auto* constraint = static_cast<btGeneric6DofSpring2Constraint*>(it->second.constraint.get());
        if (!constraint)
            return false;

        auto it_a = bodies_.find(desc.entity_a);
        if (it_a == bodies_.end() || !it_a->second.body)
            return false;
        auto it_b = bodies_.find(desc.entity_b);
        if (it_b == bodies_.end() || !it_b->second.body)
            return false;

        {
            btTransform frame_a;
            frame_a.setIdentity();
            frame_a.setOrigin(to_bt_vec3(desc.local_anchor_a, settings_.units_per_meter));
            frame_a.setRotation(to_bt_quat(desc.local_rotation_a));
            frame_a = it_a->second.com_local_inverse * frame_a;

            btTransform frame_b;
            frame_b.setIdentity();
            frame_b.setOrigin(to_bt_vec3(desc.local_anchor_b, settings_.units_per_meter));
            frame_b.setRotation(to_bt_quat(desc.local_rotation_b));
            frame_b = it_b->second.com_local_inverse * frame_b;

            constraint->setFrames(frame_a, frame_b);
        }

        constraint->setLinearLowerLimit(to_bt_vec3(desc.linear_limit_min, settings_.units_per_meter));
        constraint->setLinearUpperLimit(to_bt_vec3(desc.linear_limit_max, settings_.units_per_meter));
        constraint->setAngularLowerLimit(to_bt_vec3_raw(desc.angular_limit_min));
        constraint->setAngularUpperLimit(to_bt_vec3_raw(desc.angular_limit_max));

        for (int axis = 0; axis < 3; ++axis)
        {
            const float stiff = desc.linear_stiffness[axis];
            const float damp = desc.linear_damping[axis];
            const bool enable = (stiff != 0.0f || damp != 0.0f);
            constraint->enableSpring(axis, enable);
            if (enable)
            {
                constraint->setStiffness(axis, stiff);
                constraint->setDamping(axis, damp);
            }
        }
        for (int axis = 0; axis < 3; ++axis)
        {
            const float stiff = desc.angular_stiffness[axis];
            const float damp = desc.angular_damping[axis];
            const bool enable = (stiff != 0.0f || damp != 0.0f);
            constraint->enableSpring(axis + 3, enable);
            if (enable)
            {
                constraint->setStiffness(axis + 3, stiff);
                constraint->setDamping(axis + 3, damp);
            }
        }

        const float meters_per_unit = world_.meters_per_unit();
        for (int axis = 0; axis < 3; ++axis)
        {
            if (desc.linear_equilibrium_enabled[axis] > 0.5f)
            {
                constraint->setEquilibriumPoint(
                    axis,
                    desc.linear_equilibrium_target[axis] * meters_per_unit);
            }
        }
        for (int axis = 0; axis < 3; ++axis)
        {
            const bool enable = desc.linear_motor_enabled[axis] > 0.5f;
            const bool servo = desc.linear_servo_enabled[axis] > 0.5f;
            constraint->enableMotor(axis, enable);
            constraint->setServo(axis, servo);
            constraint->setTargetVelocity(axis, desc.linear_motor_target_velocity[axis] * meters_per_unit);
            constraint->setMaxMotorForce(axis, desc.linear_motor_max_force[axis]);
            if (servo)
                constraint->setServoTarget(axis, desc.linear_servo_target[axis] * meters_per_unit);
        }
        for (int axis = 0; axis < 3; ++axis)
        {
            const int motor_axis = axis + 3;
            const bool enable = desc.angular_motor_enabled[axis] > 0.5f;
            const bool servo = desc.angular_servo_enabled[axis] > 0.5f;
            constraint->enableMotor(motor_axis, enable);
            constraint->setServo(motor_axis, servo);
            constraint->setTargetVelocity(motor_axis, desc.angular_motor_target_velocity[axis]);
            constraint->setMaxMotorForce(motor_axis, desc.angular_motor_max_force[axis]);
            if (servo)
                constraint->setServoTarget(motor_axis, desc.angular_servo_target[axis]);
        }

        bool wake_bodies = false;
        for (int axis = 0; axis < 3; ++axis)
        {
            const bool enable = desc.linear_motor_enabled[axis] > 0.5f
                || desc.linear_servo_enabled[axis] > 0.5f
                || desc.angular_motor_enabled[axis] > 0.5f
                || desc.angular_servo_enabled[axis] > 0.5f;
            const bool has_force = desc.linear_motor_max_force[axis] > 0.0f
                || desc.angular_motor_max_force[axis] > 0.0f;
            if (enable && has_force)
            {
                wake_bodies = true;
                break;
            }
        }
        if (wake_bodies)
        {
            it_a->second.body->activate(true);
            it_b->second.body->activate(true);
        }
        return true;
    }

    void PhysicsSystem::destroy_constraint(ConstraintHandle handle)
    {
        auto it = constraints_.find(handle);
        if (it == constraints_.end())
            return;

        if (auto* world = world_.world())
        {
            if (it->second.constraint)
                world->removeConstraint(it->second.constraint.get());
        }
        constraints_.erase(it);
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

    void PhysicsSystem::apply_force_requests()
    {
        if (force_requests_.empty())
            return;

        auto* world = world_.world();
        if (!world)
        {
            force_requests_.clear();
            return;
        }

        for (const auto& request : force_requests_)
        {
            auto it = bodies_.find(request.entity);
            if (it == bodies_.end() || !it->second.body)
                continue;

            btRigidBody* body = it->second.body.get();
            if (!body || body->isStaticOrKinematicObject())
                continue;

            body->activate(true);

            switch (request.type)
            {
            case ForceRequest::Type::ForceAtPoint:
            {
                const btVector3 force = to_bt_force(request.vector, settings_.units_per_meter);
                const btVector3 point = to_bt_vec3(request.point, settings_.units_per_meter);
                const btVector3 rel = point - body->getWorldTransform().getOrigin();
                body->applyForce(force, rel);
                break;
            }
            case ForceRequest::Type::Torque:
            {
                const btVector3 torque = to_bt_torque(request.vector, settings_.units_per_meter);
                body->applyTorque(torque);
                break;
            }
            case ForceRequest::Type::ImpulseAtPoint:
            {
                const btVector3 impulse = to_bt_force(request.vector, settings_.units_per_meter);
                const btVector3 point = to_bt_vec3(request.point, settings_.units_per_meter);
                const btVector3 rel = point - body->getWorldTransform().getOrigin();
                body->applyImpulse(impulse, rel);
                break;
            }
            }
        }

        force_requests_.clear();
    }

    void PhysicsSystem::destroy_all_constraints()
    {
        auto* world = world_.world();
        if (world)
        {
            for (auto& [handle, runtime] : constraints_)
            {
                if (runtime.constraint)
                    world->removeConstraint(runtime.constraint.get());
            }
        }
        constraints_.clear();
        next_constraint_handle_ = 1;
    }

    void PhysicsSystem::remove_constraints_for_entity(entt::entity entity)
    {
        if (constraints_.empty())
            return;

        auto* world = world_.world();
        for (auto it = constraints_.begin(); it != constraints_.end();)
        {
            if (it->second.entity_a == entity || it->second.entity_b == entity)
            {
                if (world && it->second.constraint)
                    world->removeConstraint(it->second.constraint.get());
                it = constraints_.erase(it);
            }
            else
            {
                ++it;
            }
        }
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
        const entt::id_type material_id = entt::type_hash<ecs::PhysicsMaterialComponent>::value();

        const bool is_rb = (event.target.component_id == rb_id);
        const bool is_collider = (event.target.component_id == collider_id);
        const bool is_transform = (event.target.component_id == transform_id);
        const bool is_filter = (event.target.component_id == filter_id);
        const bool is_material = (event.target.component_id == material_id);
        if (!is_rb && !is_collider && !is_transform && !is_filter && !is_material)
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

        if (is_material)
        {
            auto body_it = bodies_.find(*entity_opt);
            if (body_it != bodies_.end() && body_it->second.body)
            {
                if (const auto* material = registry->try_get<ecs::PhysicsMaterialComponent>(*entity_opt))
                {
                    body_it->second.body->setFriction(material->material.friction);
                    body_it->second.body->setRestitution(material->material.restitution);
                }
                return;
            }
        }

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
