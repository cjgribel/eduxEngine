// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "DebugRenderSystem.hpp"

#include "ImGuiHelpers.hpp"
#include "ShapeRenderer.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <string>

namespace eeng::ecs::systems
{
    namespace
    {
        // Label helper for collider types.
        const char* collider_type_label(eeng::ecs::ColliderType type)
        {
            switch (type)
            {
            case eeng::ecs::ColliderType::Box: return "Box";
            case eeng::ecs::ColliderType::Sphere: return "Sphere";
            case eeng::ecs::ColliderType::Capsule: return "Capsule";
            case eeng::ecs::ColliderType::ConvexHull: return "ConvexHull";
            case eeng::ecs::ColliderType::TriangleMesh: return "TriMesh";
            case eeng::ecs::ColliderType::AABB: return "AABB";
            default: return "Unknown";
            }
        }

        // Label helper for rigidbody motion types.
        const char* motion_type_label(eeng::ecs::PhysicsMotionType type)
        {
            switch (type)
            {
            case eeng::ecs::PhysicsMotionType::Static: return "Static";
            case eeng::ecs::PhysicsMotionType::Dynamic: return "Dynamic";
            case eeng::ecs::PhysicsMotionType::Kinematic: return "Kinematic";
            default: return "Unknown";
            }
        }

        // World transform for a collider from entity transform + local collider transform.
        glm::mat4 collider_world_transform(
            const eeng::ecs::TransformComponent& transform,
            const eeng::ecs::ColliderDesc& collider)
        {
            const glm::mat4 local =
                glm::translate(glm::mat4(1.0f), collider.local_position) *
                glm::mat4_cast(collider.local_rotation);
            return transform.world_matrix * local;
        }
    } // namespace

    void DebugRenderSystem::render(
        entt::registry& registry,
        EngineContext& ctx,
        ShapeRendering::ShapeRenderer& renderer,
        const glm::mat4& VP_PROJ_V,
        int window_height) const
    {
        (void)ctx; // Reserved for future debug render hooks.
        // --- Transform labels ------------------------------------------------
        if (settings.show_transform_labels)
        {
            auto view = registry.view<ecs::TransformComponent>();
            for (const auto entity : view)
            {
                const auto& transform = view.get<ecs::TransformComponent>(entity);
                const auto* header = registry.try_get<ecs::HeaderComponent>(entity);
                const auto world_pos = glm::vec3(transform.world_matrix[3]) + settings.transform_label_offset;

                char label[128];
                if (header && !header->name.empty())
                {
                    std::snprintf(label, sizeof(label), "%s", header->name.c_str());
                }
                else
                {
                    std::snprintf(label, sizeof(label), "Entity %u",
                        static_cast<unsigned>(entt::to_integral(entity)));
                }

                const std::string window_name =
                    "TransformLabel##" + std::to_string(entt::to_integral(entity));

                eeng::gui::ImGuiPrintTextAt(
                    world_pos,
                    VP_PROJ_V,
                    window_height,
                    label,
                    window_name.c_str(),
                    settings.transform_label_bg,
                    settings.transform_label_text);
            }
        }

        // --- Collider wireframes + labels ----------------------------------
        if (settings.show_colliders || settings.show_collider_labels)
        {
            auto view = registry.view<ecs::TransformComponent, ecs::ColliderComponent>();
            for (const auto entity : view)
            {
                const auto& transform = view.get<ecs::TransformComponent>(entity);
                const auto& colliders = view.get<ecs::ColliderComponent>(entity);

                for (const auto& collider : colliders.colliders)
                {
                    const glm::mat4 world = collider_world_transform(transform, collider);
                    const glm::vec3 label_pos = glm::vec3(world[3]) + settings.collider_label_offset;

                    if (settings.show_colliders)
                    {
                        renderer.push_states(ShapeRendering::Color4u{ settings.collider_label_text }, world);

                        switch (collider.type)
                        {
                        case ecs::ColliderType::Sphere:
                            // ShapeRenderer uses (y radius, xz radius) for spheres.
                            renderer.push_sphere_wireframe(collider.radius, collider.radius);
                            break;
                        case ecs::ColliderType::Capsule:
                        {
                            // ShapeRenderer cylinders run along +Z in [0..h]; keep capsule axis along local +Z.
                            const float half_height = collider.height * 0.5f;

                            renderer.push_states(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -half_height)));
                            renderer.push_cylinder_wireframe(collider.height, collider.radius);
                            renderer.pop_states<glm::mat4>();

                            renderer.push_states(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, half_height)));
                            // ShapeRenderer uses (y radius, xz radius) for spheres.
                            renderer.push_sphere_wireframe(collider.radius, collider.radius);
                            renderer.pop_states<glm::mat4>();
                            renderer.push_states(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -half_height)));
                            // ShapeRenderer uses (y radius, xz radius) for spheres.
                            renderer.push_sphere_wireframe(collider.radius, collider.radius);
                            renderer.pop_states<glm::mat4>();
                            break;
                        }
                            break;
                        case ecs::ColliderType::Box:
                        case ecs::ColliderType::AABB:
                            renderer.push_AABB(-collider.half_extents, collider.half_extents);
                            break;
                        case ecs::ColliderType::ConvexHull:
                        case ecs::ColliderType::TriangleMesh:
                            renderer.push_AABB(-collider.half_extents, collider.half_extents);
                            break;
                        default:
                            renderer.push_AABB(-collider.half_extents, collider.half_extents);
                            break;
                        }

                        renderer.pop_states<glm::mat4, ShapeRendering::Color4u>();
                    }

                    if (settings.show_collider_labels)
                    {
                        char label[128];
                        std::snprintf(label, sizeof(label), "Col %u %s%s",
                            collider.id,
                            collider_type_label(collider.type),
                            collider.is_trigger ? " (Trigger)" : "");

                        const std::string window_name =
                            "ColliderLabel##" + std::to_string(entt::to_integral(entity)) + "_" + std::to_string(collider.id);

                        eeng::gui::ImGuiPrintTextAt(
                            label_pos,
                            VP_PROJ_V,
                            window_height,
                            label,
                            window_name.c_str(),
                            settings.collider_label_bg,
                            settings.collider_label_text);
                    }
                }
            }
        }

        // --- RigidBody labels ----------------------------------------------
        if (settings.show_rigidbody_labels)
        {
            auto view = registry.view<ecs::TransformComponent, ecs::RigidBodyComponent>();
            for (const auto entity : view)
            {
                const auto& transform = view.get<ecs::TransformComponent>(entity);
                const auto& rb = view.get<ecs::RigidBodyComponent>(entity);
                const glm::vec3 label_pos = glm::vec3(transform.world_matrix[3]) + settings.rigidbody_label_offset;

                char label[128];
                std::snprintf(label, sizeof(label), "RB %s", motion_type_label(rb.motion));

                const std::string window_name =
                    "RigidBodyLabel##" + std::to_string(entt::to_integral(entity));

                eeng::gui::ImGuiPrintTextAt(
                    label_pos,
                    VP_PROJ_V,
                    window_height,
                    label,
                    window_name.c_str(),
                    settings.rigidbody_label_bg,
                    settings.rigidbody_label_text);
            }
        }

        // --- Raycast debug -------------------------------------------------
        if (settings.show_raycast_debug)
        {
            auto view = registry.view<ecs::PhysicsRaycastDebugComponent>();
            for (const auto entity : view)
            {
                (void)entity;
                auto& debug = view.get<ecs::PhysicsRaycastDebugComponent>(entity);

                for (const auto& ray : debug.rays)
                {
                    const glm::vec3 ray_end = ray.origin + ray.direction * ray.length;

                    // Draw ray line.
                    renderer.push_states(ShapeRendering::Color4u{ settings.raycast_line_color });
                    renderer.push_line(ray.origin, ray_end);
                    renderer.pop_states<ShapeRendering::Color4u>();

                if (!ray.hit)
                    continue;

                // Draw hit point and normal line when we have a hit.
                renderer.push_states(ShapeRendering::Color4u{ settings.raycast_hit_color });
                renderer.push_point(ray.hit_point, settings.raycast_hit_point_size);
                renderer.pop_states<ShapeRendering::Color4u>();

                const float normal_len = settings.raycast_hit_normal_length;
                renderer.push_states(ShapeRendering::Color4u{ settings.raycast_normal_color });
                renderer.push_line(ray.hit_point,
                    ray.hit_point + ray.hit_normal * normal_len);
                renderer.pop_states<ShapeRendering::Color4u>();
            }

                // Clear cached rays after drawing so they only last one frame by default.
                debug.rays.clear();
            }
        }
    }
}
