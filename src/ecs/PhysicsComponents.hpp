// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "assets/AssetRef.hpp"
#include "ecs/Entity.hpp"

#include <cstdint>
#include <format>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eeng::assets
{
    struct ModelDataAsset;
}

namespace eeng::ecs
{
    using ColliderId = std::uint32_t;

    enum class PhysicsMotionType : std::uint8_t
    {
        Static,
        Dynamic,
        Kinematic
    };

    enum class ColliderType : std::uint8_t
    {
        Box,
        Sphere,
        Capsule,
        ConvexHull,
        TriangleMesh,
        AABB
    };

    enum class ContactPhase : std::uint8_t
    {
        Enter,
        Stay,
        Exit
    };

    struct PhysicsMaterial
    {
        float friction = 0.5f;
        float restitution = 0.0f;
    };

    struct CollisionFilter
    {
        std::uint32_t layer = 1;
        std::uint32_t mask = 0xFFFFFFFFu;
    };

    struct ColliderDesc
    {
        ColliderId id = 0;
        ColliderType type = ColliderType::Box;

        glm::vec3 local_position{ 0.0f };
        glm::quat local_rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

        // Shape data (interpret per type).
        glm::vec3 half_extents{ 0.5f };
        float radius = 0.5f;
        float height = 1.0f;

        AssetRef<assets::ModelDataAsset> mesh_ref;
        int submesh_index = -1;

        bool is_trigger = false;
    };

    struct ColliderComponent
    {
        std::vector<ColliderDesc> colliders;
    };

    struct RigidBodyComponent
    {
        PhysicsMotionType motion = PhysicsMotionType::Dynamic;

        bool auto_mass = true;
        float mass = 1.0f;

        bool auto_inertia = true;
        glm::vec3 inertia{ 1.0f };

        float linear_damping = 0.0f;
        float angular_damping = 0.0f;

        float gravity_scale = 1.0f;
        bool allow_sleep = true;

        bool enable_ccd = false;
        float ccd_swept_sphere_radius = 0.0f;
        float ccd_motion_threshold = 0.0f;
    };

    struct PhysicsMaterialComponent
    {
        PhysicsMaterial material;
    };

    struct CollisionFilterComponent
    {
        CollisionFilter filter;
    };

    struct PhysicsEventsComponent
    {
        bool emit_collisions = true;
        bool emit_triggers = true;
    };

    struct CollisionEvent
    {
        Entity entity_a;
        Entity entity_b;
        ColliderId collider_id_a = 0;
        ColliderId collider_id_b = 0;
        ContactPhase phase = ContactPhase::Enter;
        bool is_trigger = false;

        glm::vec3 point{ 0.0f };
        glm::vec3 normal{ 0.0f };
        float impulse = 0.0f;
    };

    inline std::string to_string(const RigidBodyComponent& t)
    {
        return std::format("RigidBodyComponent(motion = {})",
            static_cast<int>(t.motion));
    }

    inline std::string to_string(const ColliderComponent& t)
    {
        return std::format("ColliderComponent(count = {})", t.colliders.size());
    }

    inline std::string to_string(const PhysicsMaterialComponent&)
    {
        return "PhysicsMaterialComponent";
    }

    inline std::string to_string(const CollisionFilterComponent&)
    {
        return "CollisionFilterComponent";
    }

    inline std::string to_string(const PhysicsEventsComponent&)
    {
        return "PhysicsEventsComponent";
    }

    template<typename Visitor>
    void visit_asset_refs(ColliderComponent& c, Visitor&& visitor)
    {
        for (auto& desc : c.colliders)
        {
            visitor(desc.mesh_ref);
        }
    }

    template<typename Visitor>
    void visit_entity_refs(ColliderComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_asset_refs(RigidBodyComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(RigidBodyComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_asset_refs(PhysicsMaterialComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(PhysicsMaterialComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_asset_refs(CollisionFilterComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(CollisionFilterComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_asset_refs(PhysicsEventsComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(PhysicsEventsComponent&, Visitor&&) {}
}
