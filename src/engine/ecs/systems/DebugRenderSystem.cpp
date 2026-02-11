// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "DebugRenderSystem.hpp"

#include "ImGuiHelpers.hpp"
#include "ShapeRenderer.hpp"
#include "glmcommon.hpp"
#include "EngineContextHelpers.hpp"
#include "hash_combine.h"
#include "assets/types/ModelAssets.hpp"
#include "ecs/HeaderComponent.hpp"
#include "ecs/ModelComponent.hpp"
#include "ecs/PhysicsComponents.hpp"
#include "ecs/TwoAnchorAlignComponent.hpp"
#include "ecs/TransformComponent.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

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

        glm::mat3 basis_from_axis(const glm::vec3& axis)
        {
            const glm::vec3 x = safe_normalize(axis);
            if (glm::dot(x, x) <= 1e-6f)
                return glm::mat3(1.0f);

            const glm::vec3 up = (std::abs(glm::dot(x, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f)
                ? glm::vec3(0.0f, 0.0f, 1.0f)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 z = safe_normalize(glm::cross(x, up));
            const glm::vec3 y = safe_normalize(glm::cross(z, x));
            return glm::mat3(x, y, z);
        }

        void push_constraint_axis_arrow(
            ShapeRendering::ShapeRenderer& renderer,
            const DebugRenderSettings& settings,
            const glm::vec3& from,
            const glm::vec3& to)
        {
            ShapeRendering::ArrowDescriptor arrow_desc{};
            arrow_desc.cone_fraction = settings.constraint_axis_arrow_cone_fraction;
            arrow_desc.cone_radius = settings.constraint_axis_arrow_cone_radius;
            arrow_desc.cylinder_radius = settings.constraint_axis_arrow_cylinder_radius;
            renderer.push_arrow(from, to, arrow_desc);
        }

        void push_arc(ShapeRendering::ShapeRenderer& renderer,
            const glm::vec3& center,
            const glm::vec3& axis_u,
            const glm::vec3& axis_v,
            float radius,
            float angle_min,
            float angle_max,
            int segments)
        {
            if (radius <= 0.0f)
                return;

            const float span = angle_max - angle_min;
            if (std::abs(span) <= 1e-4f)
                return;

            segments = std::max(3, segments);
            std::vector<glm::vec3> points;
            points.reserve(static_cast<size_t>(segments + 1));

            for (int i = 0; i <= segments; ++i)
            {
                const float t = angle_min + span * (static_cast<float>(i) / segments);
                const glm::vec3 dir = axis_u * std::cos(t) + axis_v * std::sin(t);
                points.emplace_back(center + dir * radius);
            }

            renderer.push_lines(points.data(), points.size());
        }

        void push_limit_tick(ShapeRendering::ShapeRenderer& renderer,
            const glm::vec3& pos,
            const glm::vec3& dir,
            float half_length)
        {
            if (half_length <= 0.0f)
                return;
            renderer.push_line(pos - dir * half_length, pos + dir * half_length);
        }

        void push_oriented_box_wire(ShapeRendering::ShapeRenderer& renderer,
            const glm::vec3& center,
            const glm::mat3& basis,
            const glm::vec3& min_local,
            const glm::vec3& max_local)
        {
            const auto to_world = [&](float x, float y, float z)
                {
                    return center + basis * glm::vec3(x, y, z);
                };

            const glm::vec3 c000 = to_world(min_local.x, min_local.y, min_local.z);
            const glm::vec3 c100 = to_world(max_local.x, min_local.y, min_local.z);
            const glm::vec3 c010 = to_world(min_local.x, max_local.y, min_local.z);
            const glm::vec3 c110 = to_world(max_local.x, max_local.y, min_local.z);
            const glm::vec3 c001 = to_world(min_local.x, min_local.y, max_local.z);
            const glm::vec3 c101 = to_world(max_local.x, min_local.y, max_local.z);
            const glm::vec3 c011 = to_world(min_local.x, max_local.y, max_local.z);
            const glm::vec3 c111 = to_world(max_local.x, max_local.y, max_local.z);

            renderer.push_line(c000, c100);
            renderer.push_line(c100, c110);
            renderer.push_line(c110, c010);
            renderer.push_line(c010, c000);

            renderer.push_line(c001, c101);
            renderer.push_line(c101, c111);
            renderer.push_line(c111, c011);
            renderer.push_line(c011, c001);

            renderer.push_line(c000, c001);
            renderer.push_line(c100, c101);
            renderer.push_line(c010, c011);
            renderer.push_line(c110, c111);
        }
    } // namespace

    void DebugRenderSystem::render(
        entt::registry& registry,
        EngineContext& ctx,
        ShapeRendering::ShapeRenderer& renderer,
        const glm::mat4& VP_PROJ_V,
        int window_height) const
    {
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
            if (settings.show_colliders)
                renderer.push_xray();

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

            if (settings.show_colliders)
                renderer.pop_xray();
        }

        // --- RigidBody labels + COM/axes -----------------------------------
        if (settings.show_rigidbody_labels
            || settings.show_rigidbody_com
            || settings.show_rigidbody_axes
            || settings.show_rigidbody_offset)
        {
            const bool show_rigidbody_geom =
                settings.show_rigidbody_com
                || settings.show_rigidbody_axes
                || settings.show_rigidbody_offset;
            if (show_rigidbody_geom)
                renderer.push_xray();

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

            if (show_rigidbody_geom)
                renderer.pop_xray();
        }

        // --- Skeleton debug -------------------------------------------------
        if (settings.show_skeleton
            || settings.show_skeleton_nodes
            || settings.show_skeleton_axes
            || settings.show_skeleton_labels)
        {
            auto rm = eeng::try_get_resource_manager(ctx, "DebugRenderSystem");
            if (rm)
            {
                renderer.push_states(ShapeRendering::DepthTest::False);

                auto view = registry.view<ecs::ModelComponent, ecs::TransformComponent>();
                for (auto [entity, model, transform] : view.each())
                {
                    if (!model.model_ref.is_bound())
                        continue;

                    assets::GpuModelAsset gpu_model{};
                    bool gpu_ready = false;
                    const bool gpu_read = eeng::try_read_asset_ref(
                        *rm,
                        model.model_ref,
                        ctx,
                        "DebugRenderSystem",
                        "Missing GpuModelAsset for skeleton debug:",
                        [&](const assets::GpuModelAsset& gpu)
                        {
                            gpu_model = gpu;
                            gpu_ready = (gpu.state == assets::GpuLoadState::Ready);
                        });
                    if (!gpu_read || !gpu_ready)
                        continue;

                    eeng::try_read_asset(
                        *rm,
                        gpu_model.model_ref.handle,
                        gpu_model.model_ref.guid,
                        ctx,
                        "DebugRenderSystem",
                        "Missing ModelDataAsset for skeleton debug:",
                        [&](const assets::ModelDataAsset& cpu_model)
                        {
                            const size_t node_count = cpu_model.nodetree.size();
                            if (node_count == 0)
                                return;

                            const glm::mat4* node_globals = nullptr;
                            std::vector<glm::mat4> bind_globals;
                            if (model.node_global_matrices.size() == node_count)
                            {
                                node_globals = model.node_global_matrices.data();
                            }
                            else
                            {
                                bind_globals.assign(node_count, glm::mat4(1.0f));
                                cpu_model.nodetree.traverse_depthfirst(
                                    [&](const assets::SkeletonNode* node,
                                        const assets::SkeletonNode* parent,
                                        size_t node_index,
                                        size_t parent_index)
                                    {
                                        if (!node)
                                            return;
                                        glm::mat4 local = node->local_bind_tfm;
                                        if (parent)
                                            local = bind_globals[parent_index] * local;
                                        bind_globals[node_index] = local;
                                    });
                                node_globals = bind_globals.data();
                            }

                            const glm::mat4 world = transform.world_matrix;
                            const auto* header = registry.try_get<ecs::HeaderComponent>(entity);
                            const size_t entity_hash = (header && header->guid.valid())
                                ? std::hash<eeng::Guid>{}(header->guid)
                                : static_cast<size_t>(entt::to_integral(entity));

                            cpu_model.nodetree.traverse_depthfirst(
                                [&](const assets::SkeletonNode* node,
                                    const assets::SkeletonNode* parent,
                                    size_t node_index,
                                    size_t parent_index)
                                {
                                    if (!node)
                                        return;

                                    const bool is_bone = node->bone_index != assets::null_index;
                                    if (settings.show_skeleton_bones_only && !is_bone)
                                        return;

                                    const glm::mat4 node_world = world * node_globals[node_index];
                                    const glm::vec3 node_pos(node_world[3]);

                                    if (parent && settings.show_skeleton)
                                    {
                                        const bool parent_is_bone = parent->bone_index != assets::null_index;
                                        if (!settings.show_skeleton_bones_only || (is_bone && parent_is_bone))
                                        {
                                            const glm::mat4 parent_world = world * node_globals[parent_index];
                                            const glm::vec3 parent_pos(parent_world[3]);
                                            const std::uint32_t line_color = (is_bone && parent_is_bone)
                                                ? settings.skeleton_bone_line_color
                                                : settings.skeleton_line_color;
                                            renderer.push_states(ShapeRendering::Color4u{ line_color });
                                            renderer.push_line(parent_pos, node_pos);
                                            renderer.pop_states<ShapeRendering::Color4u>();
                                        }
                                    }

                                    if (settings.show_skeleton_nodes)
                                    {
                                        const std::uint32_t point_color = is_bone
                                            ? settings.skeleton_bone_point_color
                                            : settings.skeleton_node_point_color;
                                        renderer.push_states(ShapeRendering::Color4u{ point_color });
                                        renderer.push_point(node_pos, settings.skeleton_point_size);
                                        renderer.pop_states<ShapeRendering::Color4u>();
                                    }

                                    if (settings.show_skeleton_axes)
                                    {
                                        renderer.push_basis_basic(node_world, settings.skeleton_axis_length);
                                    }

                                    if (settings.show_skeleton_labels)
                                    {
                                        char label[256];
                                        if (node->bone_index != assets::null_index)
                                        {
                                            std::snprintf(label, sizeof(label), "%zu_%s (bone %d)",
                                                node_index,
                                                node->name.c_str(),
                                                node->bone_index);
                                        }
                                        else
                                        {
                                            std::snprintf(label, sizeof(label), "%zu_%s",
                                                node_index,
                                                node->name.c_str());
                                        }

                                        const std::uint32_t label_bg = (node->bone_index != assets::null_index)
                                            ? settings.skeleton_bone_label_bg_color
                                            : settings.skeleton_node_label_bg_color;
                                        const size_t label_hash = hash_combine(entity_hash, node_index);
                                        const std::string window_name =
                                            "SkeletonNodeLabel##" + std::to_string(label_hash);

                                        eeng::gui::ImGuiPrintTextAt(
                                            node_pos,
                                            VP_PROJ_V,
                                            window_height,
                                            label,
                                            window_name.c_str(),
                                            label_bg,
                                            settings.skeleton_label_text_color);
                                    }
                                });
                        });
                }

                renderer.pop_states<ShapeRendering::DepthTest>();
            }
        }

        // --- Spring-damper debug ------------------------------------------
        if (settings.show_springs)
        {
            renderer.push_xray();

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

            // 6DoF spring visualization (linear springs only).
            {
                auto view = registry.view<ecs::SixDofSpringConstraintComponent>();
                for (const auto entity : view)
                {
                    const auto& constraint = view.get<ecs::SixDofSpringConstraintComponent>(entity);
                    if (!constraint.enabled)
                        continue;

                    const bool linear_active =
                        (glm::dot(constraint.linear_stiffness, constraint.linear_stiffness) > 1e-6f)
                        || (glm::dot(constraint.linear_damping, constraint.linear_damping) > 1e-6f);
                    if (!linear_active)
                        continue;

                    const AnchorInfo anchor_a = resolve_anchor(
                        constraint.entity_a,
                        constraint.local_anchor_a,
                        ecs::SpringAnchorSpace::Transform);
                    const AnchorInfo anchor_b = resolve_anchor(
                        constraint.entity_b,
                        constraint.local_anchor_b,
                        ecs::SpringAnchorSpace::Transform);

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

            renderer.pop_xray();
        }

        // --- Constraint debug ---------------------------------------------
        if (settings.show_constraints)
        {
            auto resolve_anchor = [&](const ecs::EntityRef& entity_ref,
                const glm::vec3& local_anchor,
                glm::vec3& out_anchor,
                const ecs::TransformComponent*& out_tfm) -> bool
                {
                    if (!entity_ref.is_bound())
                        return false;

                    const entt::entity body_entity = static_cast<entt::entity>(entity_ref.entity);
                    if (!registry.valid(body_entity))
                        return false;

                    const auto* tfm = registry.try_get<ecs::TransformComponent>(body_entity);
                    if (!tfm)
                        return false;

                    out_anchor = glm::vec3(tfm->world_matrix * glm::vec4(local_anchor, 1.0f));
                    out_tfm = tfm;
                    return true;
                };

            renderer.push_xray();
            renderer.push_states(ShapeRendering::LineType::Thick, ShapeRendering::LineStyle{ 2.0f });

            // Point constraints.
            if (settings.show_constraint_points)
            {
                auto view = registry.view<ecs::PointConstraintComponent>();
                for (const auto entity : view)
                {
                    const auto& constraint = view.get<ecs::PointConstraintComponent>(entity);
                    if (!constraint.enabled)
                        continue;

                    glm::vec3 anchor_a{};
                    glm::vec3 anchor_b{};
                    const ecs::TransformComponent* tfm_a = nullptr;
                    const ecs::TransformComponent* tfm_b = nullptr;
                    if (!resolve_anchor(constraint.entity_a, constraint.local_anchor_a, anchor_a, tfm_a)
                        || !resolve_anchor(constraint.entity_b, constraint.local_anchor_b, anchor_b, tfm_b))
                        continue;

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_line_color });
                    renderer.push_line(anchor_a, anchor_b);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_anchor_a_color });
                    renderer.push_point(anchor_a, settings.constraint_anchor_point_size);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_anchor_b_color });
                    renderer.push_point(anchor_b, settings.constraint_anchor_point_size);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    const glm::vec3 anchor_mid = 0.5f * (anchor_a + anchor_b);
                    renderer.push_states(
                        ShapeRendering::Color4u{ settings.constraint_line_color },
                        glm::translate(glm::mat4(1.0f), anchor_mid));
                    renderer.push_sphere_wireframe(settings.constraint_anchor_radius, settings.constraint_anchor_radius);
                    renderer.pop_states<glm::mat4, ShapeRendering::Color4u>();
                }
            }

            // Hinge constraints.
            if (settings.show_constraint_hinges)
            {
                auto view = registry.view<ecs::HingeConstraintComponent>();
                for (const auto entity : view)
                {
                    const auto& constraint = view.get<ecs::HingeConstraintComponent>(entity);
                    if (!constraint.enabled)
                        continue;

                    glm::vec3 anchor_a{};
                    glm::vec3 anchor_b{};
                    const ecs::TransformComponent* tfm_a = nullptr;
                    const ecs::TransformComponent* tfm_b = nullptr;
                    if (!resolve_anchor(constraint.entity_a, constraint.local_anchor_a, anchor_a, tfm_a)
                        || !resolve_anchor(constraint.entity_b, constraint.local_anchor_b, anchor_b, tfm_b))
                        continue;

                    const glm::vec3 axis_a = safe_normalize(tfm_a->world_rotation_matrix * constraint.local_axis_a);

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_line_color });
                    renderer.push_line(anchor_a, anchor_b);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_axis_color });
                    push_constraint_axis_arrow(
                        renderer,
                        settings,
                        anchor_a - axis_a * (settings.constraint_axis_length * 0.5f),
                        anchor_a + axis_a * (settings.constraint_axis_length * 0.5f));
                    renderer.pop_states<ShapeRendering::Color4u>();

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_anchor_a_color });
                    renderer.push_point(anchor_a, settings.constraint_anchor_point_size);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_anchor_b_color });
                    renderer.push_point(anchor_b, settings.constraint_anchor_point_size);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    if (constraint.use_limits)
                    {
                        const glm::mat3 basis = basis_from_axis(axis_a);
                        const glm::vec3 axis_u = basis[1];
                        const glm::vec3 axis_v = basis[2];

                        renderer.push_states(ShapeRendering::Color4u{ settings.constraint_limit_color });
                        push_arc(renderer,
                            anchor_a,
                            axis_u,
                            axis_v,
                            settings.constraint_limit_radius,
                            constraint.limit_min,
                            constraint.limit_max,
                            settings.constraint_limit_segments);

                        const glm::vec3 dir_min =
                            axis_u * std::cos(constraint.limit_min) + axis_v * std::sin(constraint.limit_min);
                        const glm::vec3 dir_max =
                            axis_u * std::cos(constraint.limit_max) + axis_v * std::sin(constraint.limit_max);
                        renderer.push_line(anchor_a, anchor_a + dir_min * settings.constraint_limit_radius);
                        renderer.push_line(anchor_a, anchor_a + dir_max * settings.constraint_limit_radius);
                        renderer.pop_states<ShapeRendering::Color4u>();
                    }
                }
            }

            // Slider constraints.
            if (settings.show_constraint_sliders)
            {
                auto view = registry.view<ecs::SliderConstraintComponent>();
                for (const auto entity : view)
                {
                    const auto& constraint = view.get<ecs::SliderConstraintComponent>(entity);
                    if (!constraint.enabled)
                        continue;

                    glm::vec3 anchor_a{};
                    glm::vec3 anchor_b{};
                    const ecs::TransformComponent* tfm_a = nullptr;
                    const ecs::TransformComponent* tfm_b = nullptr;
                    if (!resolve_anchor(constraint.entity_a, constraint.local_anchor_a, anchor_a, tfm_a)
                        || !resolve_anchor(constraint.entity_b, constraint.local_anchor_b, anchor_b, tfm_b))
                        continue;

                    const glm::vec3 axis_a = safe_normalize(tfm_a->world_rotation_matrix * constraint.local_axis_a);

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_line_color });
                    renderer.push_line(anchor_a, anchor_b);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_axis_color });
                    push_constraint_axis_arrow(
                        renderer,
                        settings,
                        anchor_a - axis_a * (settings.constraint_axis_length * 0.5f),
                        anchor_a + axis_a * (settings.constraint_axis_length * 0.5f));
                    renderer.pop_states<ShapeRendering::Color4u>();

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_anchor_a_color });
                    renderer.push_point(anchor_a, settings.constraint_anchor_point_size);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_anchor_b_color });
                    renderer.push_point(anchor_b, settings.constraint_anchor_point_size);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    if (std::abs(constraint.linear_limit_max - constraint.linear_limit_min) > 1e-4f)
                    {
                        const glm::vec3 min_pos = anchor_a + axis_a * constraint.linear_limit_min;
                        const glm::vec3 max_pos = anchor_a + axis_a * constraint.linear_limit_max;
                        const glm::mat3 basis = basis_from_axis(axis_a);
                        const glm::vec3 tick_dir = basis[1];

                        renderer.push_states(ShapeRendering::Color4u{ settings.constraint_limit_color });
                        renderer.push_line(min_pos, max_pos);
                        push_limit_tick(renderer, min_pos, tick_dir, settings.constraint_linear_limit_tick);
                        push_limit_tick(renderer, max_pos, tick_dir, settings.constraint_linear_limit_tick);
                        renderer.pop_states<ShapeRendering::Color4u>();
                    }

                    if (std::abs(constraint.angular_limit_min) > 1e-4f
                        || std::abs(constraint.angular_limit_max) > 1e-4f)
                    {
                        const glm::mat3 basis = basis_from_axis(axis_a);
                        const glm::vec3 axis_u = basis[1];
                        const glm::vec3 axis_v = basis[2];

                        renderer.push_states(ShapeRendering::Color4u{ settings.constraint_limit_color });
                        push_arc(renderer,
                            anchor_a,
                            axis_u,
                            axis_v,
                            settings.constraint_limit_radius,
                            constraint.angular_limit_min,
                            constraint.angular_limit_max,
                            settings.constraint_limit_segments);
                        renderer.pop_states<ShapeRendering::Color4u>();
                    }
                }
            }

            // 6DoF spring constraints.
            if (settings.show_constraint_6dof)
            {
                auto view = registry.view<ecs::SixDofSpringConstraintComponent>();
                for (const auto entity : view)
                {
                    const auto& constraint = view.get<ecs::SixDofSpringConstraintComponent>(entity);
                    if (!constraint.enabled)
                        continue;

                    glm::vec3 anchor_a{};
                    glm::vec3 anchor_b{};
                    const ecs::TransformComponent* tfm_a = nullptr;
                    const ecs::TransformComponent* tfm_b = nullptr;
                    if (!resolve_anchor(constraint.entity_a, constraint.local_anchor_a, anchor_a, tfm_a)
                        || !resolve_anchor(constraint.entity_b, constraint.local_anchor_b, anchor_b, tfm_b))
                        continue;

                    const glm::quat rot_a = tfm_a->world_rotation * constraint.local_rotation_a;
                    const glm::quat rot_b = tfm_b->world_rotation * constraint.local_rotation_b;
                    const glm::mat3 basis_a = glm::mat3_cast(rot_a);
                    const glm::mat3 basis_b = glm::mat3_cast(rot_b);

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_line_color });
                    renderer.push_line(anchor_a, anchor_b);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    {
                        glm::mat4 frame_a(1.0f);
                        frame_a[0] = glm::vec4(basis_a[0], 0.0f);
                        frame_a[1] = glm::vec4(basis_a[1], 0.0f);
                        frame_a[2] = glm::vec4(basis_a[2], 0.0f);
                        frame_a[3] = glm::vec4(anchor_a, 1.0f);
                        renderer.push_basis_basic(frame_a, settings.constraint_frame_axis_length);
                    }

                    {
                        glm::mat4 frame_b(1.0f);
                        frame_b[0] = glm::vec4(basis_b[0], 0.0f);
                        frame_b[1] = glm::vec4(basis_b[1], 0.0f);
                        frame_b[2] = glm::vec4(basis_b[2], 0.0f);
                        frame_b[3] = glm::vec4(anchor_b, 1.0f);
                        renderer.push_basis_basic(frame_b, settings.constraint_frame_axis_length);
                    }

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_anchor_a_color });
                    renderer.push_point(anchor_a, settings.constraint_anchor_point_size);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    renderer.push_states(ShapeRendering::Color4u{ settings.constraint_anchor_b_color });
                    renderer.push_point(anchor_b, settings.constraint_anchor_point_size);
                    renderer.pop_states<ShapeRendering::Color4u>();

                    const glm::vec3 linear_span = constraint.linear_limit_max - constraint.linear_limit_min;
                    if (glm::dot(linear_span, linear_span) > 1e-6f)
                    {
                        const glm::vec3 min_local = glm::min(constraint.linear_limit_min, constraint.linear_limit_max);
                        const glm::vec3 max_local = glm::max(constraint.linear_limit_min, constraint.linear_limit_max);
                        renderer.push_states(ShapeRendering::Color4u{ settings.constraint_limit_color });
                        push_oriented_box_wire(renderer, anchor_a, basis_a, min_local, max_local);
                        renderer.pop_states<ShapeRendering::Color4u>();
                    }

                    const bool angular_active =
                        glm::dot(constraint.angular_limit_min, constraint.angular_limit_min) > 1e-6f
                        || glm::dot(constraint.angular_limit_max, constraint.angular_limit_max) > 1e-6f;
                    if (angular_active)
                    {
                        renderer.push_states(ShapeRendering::Color4u{ settings.constraint_limit_color });

                        if ((constraint.angular_limit_min.x <= constraint.angular_limit_max.x)
                            && (std::abs(constraint.angular_limit_min.x) > 1e-4f
                                || std::abs(constraint.angular_limit_max.x) > 1e-4f))
                        {
                            push_arc(renderer,
                                anchor_a,
                                basis_a[1],
                                basis_a[2],
                                settings.constraint_limit_radius,
                                constraint.angular_limit_min.x,
                                constraint.angular_limit_max.x,
                                settings.constraint_limit_segments);
                        }

                        if ((constraint.angular_limit_min.y <= constraint.angular_limit_max.y)
                            && (std::abs(constraint.angular_limit_min.y) > 1e-4f
                                || std::abs(constraint.angular_limit_max.y) > 1e-4f))
                        {
                            push_arc(renderer,
                                anchor_a,
                                basis_a[2],
                                basis_a[0],
                                settings.constraint_limit_radius,
                                constraint.angular_limit_min.y,
                                constraint.angular_limit_max.y,
                                settings.constraint_limit_segments);
                        }

                        if ((constraint.angular_limit_min.z <= constraint.angular_limit_max.z)
                            && (std::abs(constraint.angular_limit_min.z) > 1e-4f
                                || std::abs(constraint.angular_limit_max.z) > 1e-4f))
                        {
                            push_arc(renderer,
                                anchor_a,
                                basis_a[0],
                                basis_a[1],
                                settings.constraint_limit_radius,
                                constraint.angular_limit_min.z,
                                constraint.angular_limit_max.z,
                                settings.constraint_limit_segments);
                        }

                        renderer.pop_states<ShapeRendering::Color4u>();
                    }
                }
            }

            renderer.pop_states<ShapeRendering::LineType, ShapeRendering::LineStyle>();
            renderer.pop_xray();
        }

        // --- Two-anchor alignment debug -----------------------------------
        if (settings.show_two_anchor_align)
        {
            renderer.push_xray();
            renderer.push_states(ShapeRendering::LineType::Thick, ShapeRendering::LineStyle{ 3.0f });

            auto view = registry.view<ecs::TwoAnchorAlignComponent>();
            for (const auto entity : view)
            {
                const auto& align = view.get<ecs::TwoAnchorAlignComponent>(entity);
                if (!align.enabled)
                    continue;

                if (!align.anchor_a.is_bound() || !align.anchor_b.is_bound())
                    continue;

                const entt::entity anchor_a_entity = static_cast<entt::entity>(align.anchor_a.entity);
                const entt::entity anchor_b_entity = static_cast<entt::entity>(align.anchor_b.entity);
                if (!registry.valid(anchor_a_entity) || !registry.valid(anchor_b_entity))
                    continue;

                const auto* tfm_a = registry.try_get<ecs::TransformComponent>(anchor_a_entity);
                const auto* tfm_b = registry.try_get<ecs::TransformComponent>(anchor_b_entity);
                if (!tfm_a || !tfm_b)
                    continue;

                const glm::vec3 anchor_a = glm::vec3(tfm_a->world_matrix[3]);
                const glm::vec3 anchor_b = glm::vec3(tfm_b->world_matrix[3]);
                const glm::vec3 delta = anchor_b - anchor_a;
                const glm::vec3 forward = safe_normalize(delta);
                if (glm::dot(forward, forward) <= 1e-6f)
                    continue;

                renderer.push_states(ShapeRendering::Color4u{ settings.two_anchor_line_color });
                renderer.push_line(anchor_a, anchor_b);
                renderer.pop_states<ShapeRendering::Color4u>();

                renderer.push_states(ShapeRendering::Color4u{ settings.two_anchor_anchor_a_color });
                renderer.push_point(anchor_a, settings.two_anchor_point_size);
                renderer.pop_states<ShapeRendering::Color4u>();

                renderer.push_states(ShapeRendering::Color4u{ settings.two_anchor_anchor_b_color });
                renderer.push_point(anchor_b, settings.two_anchor_point_size);
                renderer.pop_states<ShapeRendering::Color4u>();

                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                if (align.up_reference.is_bound())
                {
                    const entt::entity up_entity = static_cast<entt::entity>(align.up_reference.entity);
                    if (registry.valid(up_entity))
                    {
                        if (const auto* tfm_up = registry.try_get<ecs::TransformComponent>(up_entity))
                        {
                            const glm::vec3 up_axis = safe_normalize(align.up_axis_ref);
                            if (glm::dot(up_axis, up_axis) > 1e-6f)
                                up = safe_normalize(tfm_up->world_rotation_matrix * up_axis);
                        }
                    }
                }

                if (glm::abs(glm::dot(up, forward)) > 0.99f)
                {
                    up = safe_normalize(glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f)));
                    if (glm::dot(up, up) <= 1e-6f)
                        up = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                const glm::vec3 mid = 0.5f * (anchor_a + anchor_b);
                renderer.push_states(ShapeRendering::Color4u{ settings.two_anchor_up_color });
                renderer.push_line(mid, mid + up * settings.two_anchor_up_length);
                renderer.pop_states<ShapeRendering::Color4u>();
            }

            renderer.pop_states<ShapeRendering::LineType, ShapeRendering::LineStyle>();
            renderer.pop_xray();
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
