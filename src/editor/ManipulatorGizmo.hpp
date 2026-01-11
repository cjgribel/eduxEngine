// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "ecs/Entity.hpp"

namespace ShapeRendering
{
    class ShapeRenderer;
}

namespace eeng
{
    struct EngineContext;
}

namespace eeng::editor
{
    class ManipulatorGizmo
    {
    public:
        enum class Mode
        {
            Translate,
            Rotate,
            Scale
        };

        enum class Space
        {
            Local,
            World
        };

        enum class Handle
        {
            None,

            TranslateX,
            TranslateY,
            TranslateZ,
            TranslateXY,
            TranslateYZ,
            TranslateZX,

            RotateX,
            RotateY,
            RotateZ,

            ScaleX,
            ScaleY,
            ScaleZ,
            ScaleUniform
        };

        struct Settings
        {
            // Screen-space size for the gizmo (pixels along an axis).
            float screen_size = 90.0f;

            // Base dimensions in world units before screen-space scaling.
            float axis_length = 1.0f;
            float axis_radius = 0.03f;
            float plane_size = 0.25f;
            float plane_offset = 0.10f;
            float rotate_radius = 1.0f;
            float rotate_thickness = 0.06f;
            float scale_box_size = 0.12f;
            float uniform_scale_size = 0.18f;

            // Snapping increments (applied when Ctrl is held).
            float linear_snap = 1.0f;
            float angular_snap_deg = 15.0f;
            float scale_snap = 0.1f;

            // Clamp scale to avoid degenerate transforms.
            float min_scale = 0.001f;

            bool allow_uniform_scale = true;
            bool draw_on_top = true;
        };

        ManipulatorGizmo();

        void update(
            EngineContext& ctx,
            const glm::mat4& view,
            const glm::mat4& proj,
            const glm::mat4& viewport,
            const glm::ivec2& window_size);

        void render(
            EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer,
            const glm::mat4& view,
            const glm::mat4& proj,
            const glm::mat4& viewport,
            const glm::ivec2& window_size) const;

        void set_mode(Mode mode);
        Mode mode() const;

        void set_space(Space space);
        Space space() const;

        Settings& settings();
        const Settings& settings() const;

    private:
        struct DragState
        {
            ecs::Entity entity{};

            // World-space anchor values at drag start.
            glm::vec3 start_world_pos{};
            glm::quat start_world_rot{ 1.0f, 0.0f, 0.0f, 0.0f };

            // Local-space values at drag start.
            glm::vec3 start_local_pos{};
            glm::quat start_local_rot{ 1.0f, 0.0f, 0.0f, 0.0f };
            glm::vec3 start_local_scale{ 1.0f };

            // Parent transform cached at drag start for stable conversion.
            glm::mat4 parent_world_matrix{ 1.0f };
            glm::quat parent_world_rot{ 1.0f, 0.0f, 0.0f, 0.0f };

            // Axis/plane data for the active handle.
            glm::vec3 axis_dir_world{};
            glm::vec3 plane_u_world{};
            glm::vec3 plane_v_world{};
            glm::vec3 plane_normal_world{};
            glm::vec3 plane_hit_start{};

            // Parametric axis offset (used for axis translation/scale).
            float axis_param_start = 0.0f;

            // Rotation drag start vector (on the rotation plane).
            glm::vec3 rotation_start_vec{};
        };

        Settings settings_{};
        Mode mode_ = Mode::Translate;
        Space space_ = Space::Local;

        Handle hovered_handle_ = Handle::None;
        Handle active_handle_ = Handle::None;

        bool dragging_ = false;
        bool was_mouse_down_ = false;
        bool toggle_space_armed_ = false;

        DragState drag_state_{};
    };
} // namespace eeng::editor
