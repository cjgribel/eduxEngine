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
    // Data-only physics components; runtime Bullet objects live elsewhere.
    using ColliderId = std::uint32_t;

    // Motion types and collider shape categories.
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

    // Spring anchor space policy (authoring pivot vs. body COM/principal axes).
    enum class SpringAnchorSpace : std::uint8_t
    {
        Transform,
        Body
    };

    // Material parameters shared across colliders.
    struct PhysicsMaterial
    {
        float friction = 0.5f;
        float restitution = 0.0f;
    };

    // Layer + mask collision filtering.
    struct CollisionFilter
    {
        std::uint32_t layer = 1;
        std::uint32_t mask = 0xFFFFFFFFu;
    };

    // Collider description used by ColliderComponent.
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

        // Mesh source (used by ConvexHull/TriangleMesh).
        AssetRef<assets::ModelDataAsset> mesh_ref;
        int submesh_index = -1;

        // Trigger colliders emit events without physical response (see PhysicsSystem limitation).
        bool is_trigger = false;
    };

    // One entity can have multiple colliders.
    struct ColliderComponent
    {
        std::vector<ColliderDesc> colliders;
    };

    // Rigid body settings (serialized).
    struct RigidBodyComponent
    {
        PhysicsMotionType motion = PhysicsMotionType::Dynamic;

        bool auto_mass = true;
        // Mass in kg. When auto_mass is true, this is recomputed from collider volume * density.
        float mass = 1.0f;
        // Density used for auto_mass (kg per cubic meter in engine units).
        float density = 1.0f;

        bool auto_inertia = true;
        // Inertia diagonal in local body axes, in kg * (units^2) (auto_inertia recomputes it).
        glm::vec3 inertia{ 1.0f };

        // Computed center of mass offset (pivot -> COM) in local authoring units.
        glm::vec3 com_local_position{ 0.0f };
        // Computed principal-axes rotation (body -> pivot). Identity when auto_inertia is off.
        glm::quat com_local_rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

        float linear_damping = 0.0f;
        float angular_damping = 0.0f;

        float gravity_scale = 1.0f;
        bool allow_sleep = true;

        bool enable_ccd = false;
        float ccd_swept_sphere_radius = 0.0f;
        float ccd_motion_threshold = 0.0f;
    };

    // Optional physics material component.
    struct PhysicsMaterialComponent
    {
        PhysicsMaterial material;
    };

    // Optional collision filter component.
    struct CollisionFilterComponent
    {
        CollisionFilter filter;
    };

    // Event payload emitted by the physics system.
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

    // Optional collision/trigger event toggles + runtime event buffer.
    struct PhysicsEventsComponent
    {
        // Collision events are opt-in by default; triggers remain opt-out.
        bool emit_collisions = false;
        bool emit_triggers = true;
        // Runtime-only event buffer populated by PhysicsSystem each frame.
        std::vector<CollisionEvent> events;
    };

    // Spring-damper between two entities (anchors without rigid bodies act as static points).
    struct SpringDamperComponent
    {
        EntityRef entity_a{};
        EntityRef entity_b{};

        // Anchors in local space of each body.
        glm::vec3 local_anchor_a{ 0.0f };
        glm::vec3 local_anchor_b{ 0.0f };
        SpringAnchorSpace anchor_space_a = SpringAnchorSpace::Transform;
        SpringAnchorSpace anchor_space_b = SpringAnchorSpace::Transform;

        // Linear spring parameters.
        float linear_stiffness = 0.0f;
        float linear_damping = 0.0f;
        float rest_length = 0.0f;

        // Optional angular spring parameters.
        bool enable_angular = false;
        float angular_stiffness = 0.0f;
        float angular_damping = 0.0f;
        // When both anchors are bodies: rest rotation from anchor A to anchor B.
        // When only one anchor has a body: target world rotation for that body.
        glm::quat rest_rotation{ 1.0f, 0.0f, 0.0f, 0.0f };

        bool enabled = true;
    };

    struct PointConstraintComponent
    {
        // Optional second body; when use_world_point_b is true, entity_b is ignored.
        EntityRef entity_b{};
        bool use_world_point_b = false;
        glm::vec3 world_point_b{ 0.0f };

        glm::vec3 local_anchor_a{ 0.0f };
        glm::vec3 local_anchor_b{ 0.0f };

        bool disable_collisions = true;
        bool enabled = true;
    };

    struct HingeConstraintComponent
    {
        EntityRef entity_b{};
        bool use_world_point_b = false;
        glm::vec3 world_anchor_b{ 0.0f };
        glm::vec3 world_axis_b{ 0.0f, 1.0f, 0.0f };

        glm::vec3 local_anchor_a{ 0.0f };
        glm::vec3 local_anchor_b{ 0.0f };
        glm::vec3 local_axis_a{ 0.0f, 1.0f, 0.0f };
        glm::vec3 local_axis_b{ 0.0f, 1.0f, 0.0f };

        bool use_limits = false;
        float limit_min = 0.0f;
        float limit_max = 0.0f;

        bool enable_motor = false;
        float motor_target_velocity = 0.0f;
        float motor_max_impulse = 0.0f;

        bool disable_collisions = true;
        bool enabled = true;
    };

    struct SliderConstraintComponent
    {
        EntityRef entity_b{};
        bool use_world_point_b = false;
        glm::vec3 world_anchor_b{ 0.0f };
        glm::vec3 world_axis_b{ 1.0f, 0.0f, 0.0f };

        glm::vec3 local_anchor_a{ 0.0f };
        glm::vec3 local_anchor_b{ 0.0f };
        glm::vec3 local_axis_a{ 1.0f, 0.0f, 0.0f };
        glm::vec3 local_axis_b{ 1.0f, 0.0f, 0.0f };

        float linear_limit_min = 0.0f;
        float linear_limit_max = 0.0f;
        float angular_limit_min = 0.0f;
        float angular_limit_max = 0.0f;

        bool enable_linear_motor = false;
        float linear_motor_target_velocity = 0.0f;
        float linear_motor_max_force = 0.0f;

        bool disable_collisions = true;
        bool enabled = true;
    };

    struct SixDofSpringConstraintComponent
    {
        EntityRef entity_b{};
        bool use_world_point_b = false;
        glm::vec3 world_anchor_b{ 0.0f };
        glm::quat world_rotation_b{ 1.0f, 0.0f, 0.0f, 0.0f };

        glm::vec3 local_anchor_a{ 0.0f };
        glm::quat local_rotation_a{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 local_anchor_b{ 0.0f };
        glm::quat local_rotation_b{ 1.0f, 0.0f, 0.0f, 0.0f };

        glm::vec3 linear_limit_min{ 0.0f };
        glm::vec3 linear_limit_max{ 0.0f };
        glm::vec3 angular_limit_min{ 0.0f };
        glm::vec3 angular_limit_max{ 0.0f };

        glm::vec3 linear_stiffness{ 0.0f };
        glm::vec3 linear_damping{ 0.0f };
        glm::vec3 angular_stiffness{ 0.0f };
        glm::vec3 angular_damping{ 0.0f };

        bool disable_collisions = true;
        bool enabled = true;
    };

    // Debug-only ray record used by PhysicsRaycastDebugComponent.
    struct PhysicsRaycastDebugRay
    {
        glm::vec3 origin{ 0.0f };
        glm::vec3 direction{ 0.0f, -1.0f, 0.0f };
        float length = 0.0f;

        bool hit = false;
        glm::vec3 hit_point{ 0.0f };
        glm::vec3 hit_normal{ 0.0f, 1.0f, 0.0f };
        Entity hit_entity{};
        ColliderId hit_collider = 0;
        bool hit_is_trigger = false;
    };

    // Debug-only component to store recent raycasts for visualization.
    // Callers should clear rays each frame if they want a single-frame view.
    struct PhysicsRaycastDebugComponent
    {
        std::vector<PhysicsRaycastDebugRay> rays;
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

    inline std::string to_string(const SpringDamperComponent&)
    {
        return "SpringDamperComponent";
    }

    inline std::string to_string(const PointConstraintComponent&)
    {
        return "PointConstraintComponent";
    }

    inline std::string to_string(const HingeConstraintComponent&)
    {
        return "HingeConstraintComponent";
    }

    inline std::string to_string(const SliderConstraintComponent&)
    {
        return "SliderConstraintComponent";
    }

    inline std::string to_string(const SixDofSpringConstraintComponent&)
    {
        return "SixDofSpringConstraintComponent";
    }

    inline std::string to_string(const PhysicsRaycastDebugComponent&)
    {
        return "PhysicsRaycastDebugComponent";
    }

    // Asset reference traversal hooks used by the ResourceManager.
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

    template<typename Visitor>
    void visit_asset_refs(SpringDamperComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(SpringDamperComponent& spring, Visitor&& visitor)
    {
        visitor(spring.entity_a);
        visitor(spring.entity_b);
    }

    template<typename Visitor>
    void visit_asset_refs(PointConstraintComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(PointConstraintComponent& constraint, Visitor&& visitor)
    {
        visitor(constraint.entity_b);
    }

    template<typename Visitor>
    void visit_asset_refs(HingeConstraintComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(HingeConstraintComponent& constraint, Visitor&& visitor)
    {
        visitor(constraint.entity_b);
    }

    template<typename Visitor>
    void visit_asset_refs(SliderConstraintComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(SliderConstraintComponent& constraint, Visitor&& visitor)
    {
        visitor(constraint.entity_b);
    }

    template<typename Visitor>
    void visit_asset_refs(SixDofSpringConstraintComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(SixDofSpringConstraintComponent& constraint, Visitor&& visitor)
    {
        visitor(constraint.entity_b);
    }

    template<typename Visitor>
    void visit_asset_refs(PhysicsRaycastDebugComponent&, Visitor&&) {}

    template<typename Visitor>
    void visit_entity_refs(PhysicsRaycastDebugComponent&, Visitor&&) {}
}
