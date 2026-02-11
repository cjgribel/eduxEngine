// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "entt/entt.hpp"

namespace eeng
{
    struct EngineContext;
}

namespace ShapeRendering
{
    class ShapeRenderer;
}

namespace eeng::ecs::systems
{
    struct DebugRenderSettings
    {
        bool show_transform_labels = false; //
        std::uint32_t transform_label_bg = 0x80000000u;
        // Distinct label colors make it easier to spot component types.
        std::uint32_t transform_label_text = 0xffffffffu;
        glm::vec3 transform_label_offset{ 0.0f };

        // Collider wireframe + labels.
        bool show_colliders = true;
        bool show_collider_labels = false; //
        // Collider wireframes use the label text color to keep visuals consistent.
        std::uint32_t collider_label_bg = 0x80202020u;
        std::uint32_t collider_label_text = 0xff80ff80u;
        glm::vec3 collider_label_offset{ 0.0f, 0.15f, 0.0f };

        // RigidBody labels.
        bool show_rigidbody_labels = false; //
        std::uint32_t rigidbody_label_bg = 0x80202040u;
        std::uint32_t rigidbody_label_text = 0xffffc080u;
        glm::vec3 rigidbody_label_offset{ 0.0f, 0.3f, 0.0f };
        // RigidBody COM + principal axes.
        bool show_rigidbody_com = true;
        bool show_rigidbody_axes = true;
        bool show_rigidbody_offset = true;
        std::uint32_t rigidbody_com_color = 0xffffffffu;
        std::uint32_t rigidbody_axis_x_color = 0xff0000ffu;
        std::uint32_t rigidbody_axis_y_color = 0xff00ff00u;
        std::uint32_t rigidbody_axis_z_color = 0xffff0000u;
        std::uint32_t rigidbody_offset_color = 0xff808080u;
        float rigidbody_com_radius = 0.05f;
        float rigidbody_axis_length = 0.35f;

        // Raycast debug overlays.
        bool show_raycast_debug = true;
        std::uint32_t raycast_line_color = 0xffff00ffu;
        std::uint32_t raycast_hit_color = 0xff000000u;
        std::uint32_t raycast_normal_color = 0xff00ff00u;
        float raycast_hit_normal_length = 0.25f;
        unsigned raycast_hit_point_size = 8;

        // Spring-damper visualization.
        bool show_springs = true;
        std::uint32_t spring_color = 0xff323232u;
        float spring_radius_outer = 0.1f;
        float spring_radius_inner = 0.02f;
        float spring_revs = 8.0f;

        // Constraint visualization.
        bool show_constraints = true;
        bool show_constraint_hinges = true;
        bool show_constraint_sliders = false; //
        bool show_constraint_6dof = false; //
        bool show_constraint_points = false; //
        std::uint32_t constraint_anchor_a_color = 0xff40ff40u;
        std::uint32_t constraint_anchor_b_color = 0xffff4040u;
        std::uint32_t constraint_line_color = 0xff909090u;
        std::uint32_t constraint_axis_color = 0xff40ffffu;
        std::uint32_t constraint_limit_color = 0xffc0c0c0u;
        float constraint_anchor_radius = 0.04f;
        unsigned constraint_anchor_point_size = 6;
        float constraint_axis_length = 0.4f;
        float constraint_axis_arrow_cone_fraction = 0.35f;
        float constraint_axis_arrow_cone_radius = 0.035f;
        float constraint_axis_arrow_cylinder_radius = 0.014f;
        float constraint_limit_radius = 0.35f;
        float constraint_linear_limit_tick = 0.06f;
        int constraint_limit_segments = 24;
        float constraint_frame_axis_length = 0.25f;

        // Two-anchor alignment visualization.
        bool show_two_anchor_align = true;
        std::uint32_t two_anchor_line_color = 0xff9090ffu;
        std::uint32_t two_anchor_anchor_a_color = 0xff40ff40u;
        std::uint32_t two_anchor_anchor_b_color = 0xffff4040u;
        std::uint32_t two_anchor_up_color = 0xffffff40u;
        float two_anchor_up_length = 0.25f;
        unsigned two_anchor_point_size = 6;

        // ShapeRenderer demo overlay (debug visualization).
        bool show_demo_shapes = true;
        glm::vec3 demo_shape_offset{ 0.0f, 0.0f, -5.0f };

        // Skeleton debug overlays.
        bool show_skeleton = false; // parent-child links
        bool show_skeleton_nodes = false;
        bool show_skeleton_axes = false;
        bool show_skeleton_labels = false;
        bool show_skeleton_bones_only = false;
        std::uint32_t skeleton_line_color = 0xffff8080u;
        std::uint32_t skeleton_bone_line_color = 0xff00ffffu;
        std::uint32_t skeleton_node_point_color = 0xffff8080u;
        std::uint32_t skeleton_bone_point_color = 0xff00ffffu;
        std::uint32_t skeleton_label_text_color = 0xff000000u;
        std::uint32_t skeleton_node_label_bg_color = 0x40ffffffu;
        std::uint32_t skeleton_bone_label_bg_color = 0x4000ffffu;
        unsigned skeleton_point_size = 4;
        float skeleton_axis_length = 0.2f;
    };

    class DebugRenderSystem
    {
    public:
        void render(
            entt::registry& registry,
            EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer,
            const glm::mat4& VP_PROJ_V,
            int window_height) const;

        DebugRenderSettings settings{};
    };
}
