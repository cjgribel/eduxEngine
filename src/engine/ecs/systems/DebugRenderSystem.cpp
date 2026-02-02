// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "DebugRenderSystem.hpp"

#include "ImGuiHelpers.hpp"
#include "ShapeRenderer.hpp"
#include "glmcommon.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TransformComponent.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
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

        // World transform for the rigid body COM/principal axes frame.
        glm::mat4 rigidbody_com_world_transform(
            const eeng::ecs::TransformComponent& transform,
            const eeng::ecs::RigidBodyComponent& rb)
        {
            const glm::mat4 local =
                glm::translate(glm::mat4(1.0f), rb.com_local_position) *
                glm::mat4_cast(rb.com_local_rotation);
            return transform.world_matrix * local;
        }

        glm::vec3 safe_normalize(const glm::vec3& v)
        {
            const float len2 = glm::dot(v, v);
            if (len2 <= 1e-6f)
                return glm::vec3(0.0f);
            return v / std::sqrt(len2);
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

        // --- RigidBody labels + COM/axes -----------------------------------
        if (settings.show_rigidbody_labels
            || settings.show_rigidbody_com
            || settings.show_rigidbody_axes
            || settings.show_rigidbody_offset)
        {
            auto view = registry.view<ecs::TransformComponent, ecs::RigidBodyComponent>();
            for (const auto entity : view)
            {
                const auto& transform = view.get<ecs::TransformComponent>(entity);
                const auto& rb = view.get<ecs::RigidBodyComponent>(entity);
                const glm::vec3 pivot_pos = glm::vec3(transform.world_matrix[3]);
                const glm::mat4 com_world = rigidbody_com_world_transform(transform, rb);
                const glm::vec3 com_pos = glm::vec3(com_world[3]);

                if (settings.show_rigidbody_offset)
                {
                    renderer.push_states(
                        ShapeRendering::Color4u{ settings.rigidbody_offset_color },
                        glm::mat4(1.0f));
                    renderer.push_line(pivot_pos, com_pos);
                    renderer.pop_states<glm::mat4, ShapeRendering::Color4u>();
                }

                if (settings.show_rigidbody_com)
                {
                    renderer.push_states(
                        ShapeRendering::Color4u{ settings.rigidbody_com_color },
                        glm::translate(glm::mat4(1.0f), com_pos));
                    renderer.push_sphere_wireframe(settings.rigidbody_com_radius, settings.rigidbody_com_radius);
                    renderer.pop_states<glm::mat4, ShapeRendering::Color4u>();
                }

                if (settings.show_rigidbody_axes)
                {
                    const glm::vec3 axis_x = safe_normalize(glm::vec3(com_world[0]));
                    const glm::vec3 axis_y = safe_normalize(glm::vec3(com_world[1]));
                    const glm::vec3 axis_z = safe_normalize(glm::vec3(com_world[2]));

                    renderer.push_states(
                        ShapeRendering::Color4u{ settings.rigidbody_axis_x_color },
                        glm::mat4(1.0f));
                    if (glm::dot(axis_x, axis_x) > 0.0f)
                        renderer.push_line(com_pos, com_pos + axis_x * settings.rigidbody_axis_length);
                    renderer.pop_states<glm::mat4, ShapeRendering::Color4u>();

                    renderer.push_states(
                        ShapeRendering::Color4u{ settings.rigidbody_axis_y_color },
                        glm::mat4(1.0f));
                    if (glm::dot(axis_y, axis_y) > 0.0f)
                        renderer.push_line(com_pos, com_pos + axis_y * settings.rigidbody_axis_length);
                    renderer.pop_states<glm::mat4, ShapeRendering::Color4u>();

                    renderer.push_states(
                        ShapeRendering::Color4u{ settings.rigidbody_axis_z_color },
                        glm::mat4(1.0f));
                    if (glm::dot(axis_z, axis_z) > 0.0f)
                        renderer.push_line(com_pos, com_pos + axis_z * settings.rigidbody_axis_length);
                    renderer.pop_states<glm::mat4, ShapeRendering::Color4u>();
                }

                const glm::vec3 label_pos = com_pos + settings.rigidbody_label_offset;

                if (settings.show_rigidbody_labels)
                {
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
        }

        // --- Spring-damper debug ------------------------------------------
        if (settings.show_springs)
        {
            auto view = registry.view<ecs::SpringDamperComponent>();
            for (const auto entity : view)
            {
                const auto& spring = view.get<ecs::SpringDamperComponent>(entity);

                if (!spring.enabled)
                    continue;

                const bool linear_active =
                    (spring.linear_stiffness != 0.0f || spring.linear_damping != 0.0f);
                if (!linear_active)
                    continue;

                struct AnchorInfo
                {
                    bool valid = false;
                    bool has_body = false;
                    entt::entity entity = entt::null;
                    glm::vec3 anchor_world{ 0.0f };
                };

                auto resolve_anchor = [&](const ecs::EntityRef& entity_ref,
                                          const glm::vec3& local_anchor,
                                          ecs::SpringAnchorSpace anchor_space) -> AnchorInfo
                {
                    AnchorInfo info{};
                    if (!entity_ref.is_bound())
                        return info;

                    const entt::entity body_entity = static_cast<entt::entity>(entity_ref.entity);
                    info.entity = body_entity;

                    if (!registry.valid(body_entity))
                        return info;

                    const auto* tfm = registry.try_get<ecs::TransformComponent>(body_entity);
                    if (!tfm)
                        return info;

                    const auto* rb = registry.try_get<ecs::RigidBodyComponent>(body_entity);
                    info.has_body = (rb != nullptr);

                    if (rb && anchor_space == ecs::SpringAnchorSpace::Body)
                    {
                        const glm::mat4 com_world = rigidbody_com_world_transform(*tfm, *rb);
                        info.anchor_world = glm::vec3(com_world * glm::vec4(local_anchor * tfm->scale, 1.0f));
                    }
                    else
                    {
                        info.anchor_world = glm::vec3(tfm->world_matrix * glm::vec4(local_anchor, 1.0f));
                    }

                    info.valid = true;
                    return info;
                };

                const AnchorInfo anchor_a = resolve_anchor(
                    spring.entity_a,
                    spring.local_anchor_a,
                    spring.anchor_space_a);
                const AnchorInfo anchor_b = resolve_anchor(
                    spring.entity_b,
                    spring.local_anchor_b,
                    spring.anchor_space_b);

                if (!anchor_a.valid || !anchor_b.valid)
                    continue;

                const bool same_body = anchor_a.has_body && anchor_b.has_body && anchor_a.entity == anchor_b.entity;
                if ((!anchor_a.has_body && !anchor_b.has_body) || same_body)
                    continue;

                renderer.push_states(ShapeRendering::LineType::Thick, ShapeRendering::LineStyle{ 3.0f });
                renderer.push_states(ShapeRendering::Color4u{ settings.spring_color });
                renderer.push_helix(
                    anchor_a.anchor_world,
                    anchor_b.anchor_world,
                    settings.spring_radius_outer,
                    settings.spring_radius_inner,
                    settings.spring_revs);
                renderer.pop_states<ShapeRendering::Color4u>();
                renderer.pop_states<ShapeRendering::LineType, ShapeRendering::LineStyle>();
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

        if (settings.show_demo_shapes)
        {
            // Use a shared_ptr alias to reuse the existing renderer API.
            renderer.push_states(glm_aux::T(settings.demo_shape_offset));
            ShapeRendererPtr renderer_ptr(
                &renderer,
                [](ShapeRendering::ShapeRenderer*) {});
            ShapeRendering::DemoDraw(renderer_ptr);
            renderer.pop_states<glm::mat4>();
        }
    }
}
