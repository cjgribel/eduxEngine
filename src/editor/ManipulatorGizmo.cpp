// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/ManipulatorGizmo.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "EngineContext.hpp"
#include "EngineContextHelpers.hpp"
#include "ecs/EntityManager.hpp"
#include "ecs/TransformComponent.hpp"
#include "editor/AssignFieldCommand.hpp"
#include "editor/CommandQueue.hpp"
#include "engineapi/SelectionManager.hpp"
#include "glmcommon.hpp"
#include "ShapeRenderer.hpp"
#include "entt/entt.hpp"

namespace
{
    using eeng::editor::ManipulatorGizmo;
    using namespace entt::literals;

    constexpr float kEpsilon = 1e-6f;

    struct RayLineResult
    {
        float ray_t = 0.0f;  // Parameter on ray: P = ray.origin + ray.dir * ray_t
        float line_t = 0.0f; // Parameter on line: Q = line.origin + line.dir * line_t
        float dist2 = 0.0f;  // Squared distance between closest points
        glm::vec3 ray_p{};
        glm::vec3 line_p{};
    };

    struct Plane
    {
        glm::vec3 point{};
        glm::vec3 normal{};
    };

    glm::vec3 axis_color_base(ManipulatorGizmo::Handle handle)
    {
        switch (handle)
        {
        case ManipulatorGizmo::Handle::TranslateX:
        case ManipulatorGizmo::Handle::RotateX:
        case ManipulatorGizmo::Handle::ScaleX:
            return glm::vec3(1.0f, 0.1f, 0.1f);
        case ManipulatorGizmo::Handle::TranslateY:
        case ManipulatorGizmo::Handle::RotateY:
        case ManipulatorGizmo::Handle::ScaleY:
            return glm::vec3(0.2f, 1.0f, 0.2f);
        case ManipulatorGizmo::Handle::TranslateZ:
        case ManipulatorGizmo::Handle::RotateZ:
        case ManipulatorGizmo::Handle::ScaleZ:
            return glm::vec3(0.2f, 0.4f, 1.0f);
        case ManipulatorGizmo::Handle::TranslateXY:
            return glm::vec3(1.0f, 1.0f, 0.2f);
        case ManipulatorGizmo::Handle::TranslateYZ:
            return glm::vec3(0.2f, 1.0f, 1.0f);
        case ManipulatorGizmo::Handle::TranslateZX:
            return glm::vec3(1.0f, 0.2f, 1.0f);
        case ManipulatorGizmo::Handle::ScaleUniform:
            return glm::vec3(1.0f);
        default:
            break;
        }

        return glm::vec3(1.0f);
    }

    ShapeRendering::Color4u to_color_u32(const glm::vec3& rgb, float alpha = 1.0f)
    {
        auto clamp_to_byte = [](float v)
        {
            v = std::max(0.0f, std::min(1.0f, v));
            return static_cast<unsigned>(std::round(v * 255.0f));
        };

        const unsigned r = clamp_to_byte(rgb.r);
        const unsigned g = clamp_to_byte(rgb.g);
        const unsigned b = clamp_to_byte(rgb.b);
        const unsigned a = clamp_to_byte(alpha);

        return ShapeRendering::Color4u(
            static_cast<unsigned char>(r),
            static_cast<unsigned char>(g),
            static_cast<unsigned char>(b),
            static_cast<unsigned char>(a));
    }

    ShapeRendering::Color4u resolve_handle_color(
        ManipulatorGizmo::Handle handle,
        ManipulatorGizmo::Handle hovered,
        ManipulatorGizmo::Handle active)
    {
        // If any handle is active, dim the rest to reduce visual noise.
        if (active != ManipulatorGizmo::Handle::None && handle != active)
            return ShapeRendering::Color4u::Gray;

        // Highlight hovered/active handles.
        if (handle == active || handle == hovered)
            return ShapeRendering::Color4u::Yellow;

        // Default axis colors.
        const glm::vec3 rgb = axis_color_base(handle);
        return to_color_u32(rgb, 1.0f);
    }

    bool intersect_ray_plane(
        const glm_aux::Ray& ray,
        const Plane& plane,
        float& out_t)
    {
        // Compute denominator of the plane intersection formula:
        //   denom = dot(ray_dir, plane_normal)
        const float denom = glm::dot(ray.dir, plane.normal);

        // If denom is ~0, the ray is parallel to the plane (no hit).
        if (std::fabs(denom) < kEpsilon)
            return false;

        // Solve for t in:
        //   ray.origin + ray.dir * t = plane.point + plane.normal * 0
        // -> dot(plane.point - ray.origin, plane.normal) = t * denom
        const float t = glm::dot(plane.point - ray.origin, plane.normal) / denom;

        // Reject intersections behind the ray origin.
        if (t < 0.0f)
            return false;

        out_t = t;
        return true;
    }

    RayLineResult closest_points_ray_line(
        const glm_aux::Ray& ray,
        const glm::vec3& line_origin,
        const glm::vec3& line_dir)
    {
        RayLineResult result{};

        // We solve for the closest points between:
        //   Ray:  P(s) = O + D * s,  s >= 0
        //   Line: Q(t) = L + A * t,  t in R
        // by minimizing |P(s) - Q(t)|^2.

        const glm::vec3 w0 = ray.origin - line_origin;
        const float a = glm::dot(ray.dir, ray.dir);   // Ray direction length squared.
        const float b = glm::dot(ray.dir, line_dir);  // Dot between ray and line directions.
        const float c = glm::dot(line_dir, line_dir); // Line direction length squared.
        const float d = glm::dot(ray.dir, w0);
        const float e = glm::dot(line_dir, w0);

        const float denom = a * c - b * b;

        float s = 0.0f;
        float t = 0.0f;

        if (std::fabs(denom) > kEpsilon)
        {
            // Non-parallel case:
            //   s = (b*e - c*d) / denom
            //   t = (a*e - b*d) / denom
            s = (b * e - c * d) / denom;
            t = (a * e - b * d) / denom;
        }
        else
        {
            // Parallel case: choose s = 0 (closest point on ray origin).
            s = 0.0f;
            t = e / c;
        }

        // Clamp s to the ray domain (s >= 0).
        if (s < 0.0f)
        {
            s = 0.0f;
            t = e / c;
        }

        result.ray_t = s;
        result.line_t = t;
        result.ray_p = ray.origin + ray.dir * s;
        result.line_p = line_origin + line_dir * t;

        const glm::vec3 diff = result.ray_p - result.line_p;
        result.dist2 = glm::dot(diff, diff);

        return result;
    }

    RayLineResult closest_points_ray_segment(
        const glm_aux::Ray& ray,
        const glm::vec3& seg_a,
        const glm::vec3& seg_b)
    {
        RayLineResult result{};

        // We minimize distance between:
        //   Ray:  P(s) = O + D*s, s >= 0
        //   Seg:  Q(t) = A + (B-A)*t, t in [0, 1]
        // using the Ericson closest-segment formulation with clamping.

        const glm::vec3 seg_dir = seg_b - seg_a;
        const glm::vec3 r = ray.origin - seg_a;

        const float a = glm::dot(ray.dir, ray.dir);   // Ray direction length squared.
        const float e = glm::dot(seg_dir, seg_dir);   // Segment direction length squared.
        const float f = glm::dot(seg_dir, r);

        float s = 0.0f;
        float t = 0.0f;

        if (a <= kEpsilon && e <= kEpsilon)
        {
            // Both ray and segment degenerate to points.
            s = 0.0f;
            t = 0.0f;
        }
        else if (a <= kEpsilon)
        {
            // Ray degenerates to a point; clamp projection onto segment.
            s = 0.0f;
            t = glm::clamp(f / e, 0.0f, 1.0f);
        }
        else
        {
            const float c = glm::dot(ray.dir, r);

            if (e <= kEpsilon)
            {
                // Segment degenerates to a point.
                t = 0.0f;
                s = std::max(0.0f, -c / a);
            }
            else
            {
                const float b = glm::dot(ray.dir, seg_dir);
                const float denom = a * e - b * b;

                // Compute s for the infinite lines and clamp to ray domain (s >= 0).
                if (std::fabs(denom) > kEpsilon)
                    s = std::max(0.0f, (b * f - c * e) / denom);
                else
                    s = 0.0f;

                // Compute t on the segment from the (possibly clamped) s.
                t = (b * s + f) / e;

                // Clamp t into [0, 1] and recompute s for the new t.
                if (t < 0.0f)
                {
                    t = 0.0f;
                    s = std::max(0.0f, -c / a);
                }
                else if (t > 1.0f)
                {
                    t = 1.0f;
                    s = std::max(0.0f, (b - c) / a);
                }
            }
        }

        result.ray_t = s;
        result.line_t = t;
        result.ray_p = ray.origin + ray.dir * s;
        result.line_p = seg_a + seg_dir * t;

        const glm::vec3 diff = result.ray_p - result.line_p;
        result.dist2 = glm::dot(diff, diff);
        return result;
    }

    bool intersect_ray_aabb(
        const glm_aux::Ray& ray,
        const glm::vec3& aabb_min,
        const glm::vec3& aabb_max,
        float& out_t)
    {
        // Slab-based ray/AABB test.
        // For each axis, we find the range of t where the ray intersects that slab.
        float t_min = 0.0f;
        float t_max = std::numeric_limits<float>::max();

        for (int i = 0; i < 3; ++i)
        {
            const float origin = ray.origin[i];
            const float dir = ray.dir[i];

            if (std::fabs(dir) < kEpsilon)
            {
                // Ray is parallel to slab: must be inside the slab to intersect.
                if (origin < aabb_min[i] || origin > aabb_max[i])
                    return false;
                continue;
            }

            // Compute intersection range with the slab along this axis.
            const float inv_dir = 1.0f / dir;
            float t0 = (aabb_min[i] - origin) * inv_dir;
            float t1 = (aabb_max[i] - origin) * inv_dir;
            if (t0 > t1) std::swap(t0, t1);

            t_min = std::max(t_min, t0);
            t_max = std::min(t_max, t1);

            if (t_min > t_max)
                return false;
        }

        out_t = t_min;
        return true;
    }

    glm::vec3 extract_translation(const glm::mat4& m)
    {
        return glm::vec3(m[3]);
    }

    struct GizmoBasis
    {
        glm::vec3 x{ 1.0f, 0.0f, 0.0f };
        glm::vec3 y{ 0.0f, 1.0f, 0.0f };
        glm::vec3 z{ 0.0f, 0.0f, 1.0f };
    };

    glm::vec3 extract_axis(const glm::mat3& m, int column_index)
    {
        return glm::normalize(glm::vec3(m[column_index]));
    }

    bool is_finite_vec3(const glm::vec3& v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    glm::vec3 safe_normalize(const glm::vec3& v, const glm::vec3& fallback)
    {
        const float len = glm::length(v);
        if (!is_finite_vec3(v) || len < kEpsilon)
            return fallback;
        return v / len;
    }

    void orthonormalize_basis(GizmoBasis& basis)
    {
        // Gram-Schmidt orthonormalization with safe fallbacks.
        basis.x = safe_normalize(basis.x, glm::vec3(1.0f, 0.0f, 0.0f));

        basis.y = basis.y - basis.x * glm::dot(basis.y, basis.x);
        basis.y = safe_normalize(basis.y, glm::vec3(0.0f, 1.0f, 0.0f));

        basis.z = glm::cross(basis.x, basis.y);
        basis.z = safe_normalize(basis.z, glm::vec3(0.0f, 0.0f, 1.0f));

        basis.y = glm::cross(basis.z, basis.x);
    }

    GizmoBasis compute_gizmo_basis(
        const eeng::ecs::TransformComponent& tfm,
        ManipulatorGizmo::Mode mode,
        ManipulatorGizmo::Space space)
    {
        GizmoBasis basis{};

        // Scale handles are always drawn in local space for predictable scaling.
        const bool use_local = (mode == ManipulatorGizmo::Mode::Scale)
            ? true
            : (space == ManipulatorGizmo::Space::Local);

        if (use_local)
        {
            basis.x = safe_normalize(glm::vec3(tfm.world_rotation_matrix[0]), glm::vec3(1.0f, 0.0f, 0.0f));
            basis.y = safe_normalize(glm::vec3(tfm.world_rotation_matrix[1]), glm::vec3(0.0f, 1.0f, 0.0f));
            basis.z = safe_normalize(glm::vec3(tfm.world_rotation_matrix[2]), glm::vec3(0.0f, 0.0f, 1.0f));
            orthonormalize_basis(basis);
        }

        return basis;
    }

    glm::mat4 make_basis_matrix(
        const glm::vec3& x,
        const glm::vec3& y,
        const glm::vec3& z,
        const glm::vec3& origin,
        float scale)
    {
        // Build a transform matrix from basis vectors:
        //   [ x*scale  y*scale  z*scale  origin ]
        // This maps unit circles (XY plane) into world-space circles.
        return glm::mat4(
            glm::vec4(x * scale, 0.0f),
            glm::vec4(y * scale, 0.0f),
            glm::vec4(z * scale, 0.0f),
            glm::vec4(origin, 1.0f));
    }

    float compute_screen_scale(
        const glm::vec3& world_pos,
        float desired_pixels,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& viewport)
    {
        // Project a 1-unit step along camera right into screen space to measure
        // how many pixels one world unit occupies at the gizmo depth.

        // Inverse view gives the camera basis in world-space.
        const glm::mat4 view_inv = glm::inverse(view);
        const glm::vec3 cam_right = glm::normalize(glm::vec3(view_inv[0]));

        const glm::mat4 vp_proj_view = viewport * proj * view;

        const glm::vec4 p0_clip = vp_proj_view * glm::vec4(world_pos, 1.0f);
        const glm::vec4 p1_clip = vp_proj_view * glm::vec4(world_pos + cam_right, 1.0f);

        // Perspective divide: Clip -> Screen (viewport already applied).
        const glm::vec2 p0 = glm::vec2(p0_clip) / p0_clip.w;
        const glm::vec2 p1 = glm::vec2(p1_clip) / p1_clip.w;

        const float pixels_per_unit = glm::length(p1 - p0);

        if (pixels_per_unit < kEpsilon)
            return 1.0f;

        // Scale factor that makes 1 world unit map to desired_pixels.
        return desired_pixels / pixels_per_unit;
    }

    glm::vec3 clamp_scale(const glm::vec3& scale, float min_scale)
    {
        return glm::vec3(
            std::max(min_scale, scale.x),
            std::max(min_scale, scale.y),
            std::max(min_scale, scale.z));
    }

    bool nearly_equal_vec3(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f)
    {
        return glm::length(a - b) <= eps;
    }

    bool nearly_equal_quat(const glm::quat& a, const glm::quat& b, float eps = 1e-4f)
    {
        // Quaternions can represent the same rotation with opposite signs.
        // Use dot to compare orientation equivalence.
        return std::fabs(glm::dot(a, b)) >= (1.0f - eps);
    }

    bool is_axis_handle(ManipulatorGizmo::Handle handle)
    {
        return handle == ManipulatorGizmo::Handle::TranslateX
            || handle == ManipulatorGizmo::Handle::TranslateY
            || handle == ManipulatorGizmo::Handle::TranslateZ
            || handle == ManipulatorGizmo::Handle::RotateX
            || handle == ManipulatorGizmo::Handle::RotateY
            || handle == ManipulatorGizmo::Handle::RotateZ
            || handle == ManipulatorGizmo::Handle::ScaleX
            || handle == ManipulatorGizmo::Handle::ScaleY
            || handle == ManipulatorGizmo::Handle::ScaleZ;
    }

    glm::vec3 axis_dir_from_handle(
        ManipulatorGizmo::Handle handle,
        const GizmoBasis& basis)
    {
        switch (handle)
        {
        case ManipulatorGizmo::Handle::TranslateX:
        case ManipulatorGizmo::Handle::RotateX:
        case ManipulatorGizmo::Handle::ScaleX:
            return basis.x;
        case ManipulatorGizmo::Handle::TranslateY:
        case ManipulatorGizmo::Handle::RotateY:
        case ManipulatorGizmo::Handle::ScaleY:
            return basis.y;
        case ManipulatorGizmo::Handle::TranslateZ:
        case ManipulatorGizmo::Handle::RotateZ:
        case ManipulatorGizmo::Handle::ScaleZ:
            return basis.z;
        default:
            break;
        }
        return glm::vec3(0.0f);
    }

    glm::vec3 fallback_axis_from_handle(ManipulatorGizmo::Handle handle)
    {
        switch (handle)
        {
        case ManipulatorGizmo::Handle::TranslateX:
        case ManipulatorGizmo::Handle::RotateX:
        case ManipulatorGizmo::Handle::ScaleX:
            return glm::vec3(1.0f, 0.0f, 0.0f);
        case ManipulatorGizmo::Handle::TranslateY:
        case ManipulatorGizmo::Handle::RotateY:
        case ManipulatorGizmo::Handle::ScaleY:
            return glm::vec3(0.0f, 1.0f, 0.0f);
        case ManipulatorGizmo::Handle::TranslateZ:
        case ManipulatorGizmo::Handle::RotateZ:
        case ManipulatorGizmo::Handle::ScaleZ:
            return glm::vec3(0.0f, 0.0f, 1.0f);
        default:
            break;
        }

        return glm::vec3(0.0f, 1.0f, 0.0f);
    }

    glm::vec3 safe_axis_dir_from_handle(
        ManipulatorGizmo::Handle handle,
        const GizmoBasis& basis)
    {
        const glm::vec3 axis = axis_dir_from_handle(handle, basis);
        return safe_normalize(axis, fallback_axis_from_handle(handle));
    }

    void plane_axes_from_handle(
        ManipulatorGizmo::Handle handle,
        const GizmoBasis& basis,
        glm::vec3& out_u,
        glm::vec3& out_v)
    {
        switch (handle)
        {
        case ManipulatorGizmo::Handle::TranslateXY:
            out_u = basis.x;
            out_v = basis.y;
            break;
        case ManipulatorGizmo::Handle::TranslateYZ:
            out_u = basis.y;
            out_v = basis.z;
            break;
        case ManipulatorGizmo::Handle::TranslateZX:
            out_u = basis.z;
            out_v = basis.x;
            break;
        default:
            out_u = glm::vec3(0.0f);
            out_v = glm::vec3(0.0f);
            break;
        }
    }

    bool is_translate_plane(ManipulatorGizmo::Handle handle)
    {
        return handle == ManipulatorGizmo::Handle::TranslateXY
            || handle == ManipulatorGizmo::Handle::TranslateYZ
            || handle == ManipulatorGizmo::Handle::TranslateZX;
    }

    bool is_translate_handle(ManipulatorGizmo::Handle handle)
    {
        return handle == ManipulatorGizmo::Handle::TranslateX
            || handle == ManipulatorGizmo::Handle::TranslateY
            || handle == ManipulatorGizmo::Handle::TranslateZ
            || is_translate_plane(handle);
    }

    bool is_rotate_handle(ManipulatorGizmo::Handle handle)
    {
        return handle == ManipulatorGizmo::Handle::RotateX
            || handle == ManipulatorGizmo::Handle::RotateY
            || handle == ManipulatorGizmo::Handle::RotateZ;
    }

    bool is_scale_handle(ManipulatorGizmo::Handle handle)
    {
        return handle == ManipulatorGizmo::Handle::ScaleX
            || handle == ManipulatorGizmo::Handle::ScaleY
            || handle == ManipulatorGizmo::Handle::ScaleZ
            || handle == ManipulatorGizmo::Handle::ScaleUniform;
    }

    glm::vec3 snap_vector_to_grid(
        const glm::vec3& v,
        float snap_step)
    {
        return glm::vec3(
            std::round(v.x / snap_step) * snap_step,
            std::round(v.y / snap_step) * snap_step,
            std::round(v.z / snap_step) * snap_step);
    }

    glm::vec3 project_onto_axis(const glm::vec3& v, const glm::vec3& axis)
    {
        // Projection of v onto axis:
        //   proj = axis * dot(v, axis)
        return axis * glm::dot(v, axis);
    }

    glm::vec3 project_onto_plane(
        const glm::vec3& v,
        const glm::vec3& plane_u,
        const glm::vec3& plane_v)
    {
        // Express v in the plane basis (u, v), then reconstruct in world.
        const float u = glm::dot(v, plane_u);
        const float v_comp = glm::dot(v, plane_v);
        return plane_u * u + plane_v * v_comp;
    }

    bool resolve_target(
        eeng::EngineContext& ctx,
        eeng::ecs::Entity& out_entity,
        eeng::ecs::TransformComponent*& out_tfm,
        eeng::ecs::TransformComponent*& out_parent_tfm)
    {
        auto* em_ptr = eeng::try_get_entity_manager_ptr(ctx, "ManipulatorGizmo");
        if (!em_ptr || !ctx.entity_selection)
            return false;

        auto& selection = *ctx.entity_selection;
        if (selection.empty())
            return false;

        const eeng::ecs::Entity entity = selection.last();
        if (!em_ptr->entity_valid(entity))
            return false;

        auto registry = em_ptr->registry_wptr().lock();
        if (!registry || !registry->valid(entity))
            return false;

        auto* tfm = registry->try_get<eeng::ecs::TransformComponent>(entity);
        if (!tfm)
            return false;

        out_entity = entity;
        out_tfm = tfm;
        out_parent_tfm = nullptr;

        const auto parent_ref = em_ptr->get_entity_parent(entity);
        if (parent_ref.entity.has_id() && registry->valid(parent_ref.entity))
        {
            out_parent_tfm = registry->try_get<eeng::ecs::TransformComponent>(parent_ref.entity);
        }

        return true;
    }

    void build_assign_command(
        eeng::EngineContext& ctx,
        const eeng::ecs::Entity& entity,
        const char* field_name,
        entt::id_type field_id,
        const entt::meta_any& prev_value,
        const entt::meta_any& new_value)
    {
        if (!ctx.command_queue)
            return;

        eeng::editor::AssignFieldCommandBuilder builder;
        builder.target_component(ctx, entity, entt::type_hash<eeng::ecs::TransformComponent>::value());
        builder.push_path_data(field_id, field_name);
        builder.prev_value(prev_value);
        builder.new_value(new_value);

        ctx.command_queue->add(std::make_unique<eeng::editor::AssignFieldCommand>(builder.build()));
    }
}

namespace eeng::editor
{
    ManipulatorGizmo::ManipulatorGizmo() = default;

    void ManipulatorGizmo::set_mode(Mode mode)
    {
        mode_ = mode;
    }

    ManipulatorGizmo::Mode ManipulatorGizmo::mode() const
    {
        return mode_;
    }

    void ManipulatorGizmo::set_space(Space space)
    {
        space_ = space;
    }

    ManipulatorGizmo::Space ManipulatorGizmo::space() const
    {
        return space_;
    }

    ManipulatorGizmo::Settings& ManipulatorGizmo::settings()
    {
        return settings_;
    }

    const ManipulatorGizmo::Settings& ManipulatorGizmo::settings() const
    {
        return settings_;
    }

    void ManipulatorGizmo::update(
        EngineContext& ctx,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& viewport,
        const glm::ivec2& window_size)
    {
        if (!ctx.input_manager)
            return;

        if (window_size.x <= 0 || window_size.y <= 0)
            return;

        // Resolve current target.
        ecs::Entity entity{};
        ecs::TransformComponent* tfm = nullptr;
        ecs::TransformComponent* parent_tfm = nullptr;
        if (!resolve_target(ctx, entity, tfm, parent_tfm))
        {
            hovered_handle_ = Handle::None;
            active_handle_ = Handle::None;
            dragging_ = false;
            return;
        }

        if (dragging_ && drag_state_.entity.has_id() && drag_state_.entity != entity)
        {
            // Selection changed mid-drag; cancel to avoid editing the wrong entity.
            hovered_handle_ = Handle::None;
            active_handle_ = Handle::None;
            dragging_ = false;
            return;
        }

        // Pull input state.
        const auto& mouse = ctx.input_manager->GetMouseState();
        const bool mouse_down = mouse.leftButton;
        const bool mouse_pressed = mouse_down && !was_mouse_down_;
        const bool mouse_released = !mouse_down && was_mouse_down_;
        was_mouse_down_ = mouse_down;

        const bool ctrl_pressed =
            ctx.input_manager->IsKeyPressed(eeng::IInputManager::Key::LeftCtrl)
            || ctx.input_manager->IsKeyPressed(eeng::IInputManager::Key::RightCtrl);

        // Hotkeys for gizmo modes (hold or tap).
        if (ctx.input_manager->IsKeyPressed(eeng::IInputManager::Key::W))
            mode_ = Mode::Translate;
        if (ctx.input_manager->IsKeyPressed(eeng::IInputManager::Key::E))
            mode_ = Mode::Rotate;
        if (ctx.input_manager->IsKeyPressed(eeng::IInputManager::Key::R))
            mode_ = Mode::Scale;

        // Toggle space on Q press (edge-triggered).
        const bool q_down = ctx.input_manager->IsKeyPressed(eeng::IInputManager::Key::Q);
        if (q_down && !toggle_space_armed_)
        {
            toggle_space_armed_ = true;
            space_ = (space_ == Space::Local) ? Space::World : Space::Local;
        }
        if (!q_down)
            toggle_space_armed_ = false;

        // Build world ray from window coordinates.
        const glm::ivec2 window_coords(mouse.x, window_size.y - mouse.y);
        glm_aux::Ray ray = glm_aux::world_ray_from_window_coords(window_coords, view, proj, viewport);
        ray.z_near = std::numeric_limits<float>::max();

        const glm::vec3 world_pos = extract_translation(tfm->world_matrix);
        const GizmoBasis basis = compute_gizmo_basis(*tfm, mode_, space_);

        // Screen-space scaling for consistent gizmo size.
        const float gizmo_scale = compute_screen_scale(
            world_pos,
            settings_.screen_size,
            view,
            proj,
            viewport);

        // Reset hover state if we are not dragging.
        if (!dragging_)
            hovered_handle_ = Handle::None;

        // ------------------------------------------------------------------
        // Hover detection (ray tests) when idle.
        // ------------------------------------------------------------------
        const auto update_translate_hover = [&]()
        {
            const float axis_len = settings_.axis_length * gizmo_scale;
            const float axis_radius = settings_.axis_radius * gizmo_scale;

            // Axis handles (X/Y/Z).
            for (Handle handle : { Handle::TranslateX, Handle::TranslateY, Handle::TranslateZ })
            {
                const glm::vec3 axis_dir = safe_axis_dir_from_handle(handle, basis);
                const glm::vec3 seg_a = world_pos;
                const glm::vec3 seg_b = world_pos + axis_dir * axis_len;

                const RayLineResult hit = closest_points_ray_segment(ray, seg_a, seg_b);

                if (hit.dist2 <= axis_radius * axis_radius && hit.ray_t < ray.z_near)
                {
                    ray.z_near = hit.ray_t;
                    hovered_handle_ = handle;
                }
            }

            // Plane handles (XY/YZ/ZX).
            for (Handle handle : { Handle::TranslateXY, Handle::TranslateYZ, Handle::TranslateZX })
            {
                glm::vec3 u, v;
                plane_axes_from_handle(handle, basis, u, v);
                if (glm::length(u) < kEpsilon || glm::length(v) < kEpsilon)
                    continue;

                const Plane plane{ world_pos, glm::normalize(glm::cross(u, v)) };
                float t = 0.0f;
                if (!intersect_ray_plane(ray, plane, t))
                    continue;

                const glm::vec3 hit_point = ray.origin + ray.dir * t;
                const glm::vec3 local = hit_point - world_pos;

                // Compute plane coordinates in the (u, v) basis.
                const float u_coord = glm::dot(local, u);
                const float v_coord = glm::dot(local, v);

                const float offset = settings_.plane_offset * gizmo_scale;
                const float size = settings_.plane_size * gizmo_scale;

                if (u_coord >= offset && u_coord <= offset + size
                    && v_coord >= offset && v_coord <= offset + size)
                {
                    if (t < ray.z_near)
                    {
                        ray.z_near = t;
                        hovered_handle_ = handle;
                    }
                }
            }
        };

        const auto update_rotate_hover = [&]()
        {
            const float ring_radius = settings_.rotate_radius * gizmo_scale;
            const float ring_thickness = settings_.rotate_thickness * gizmo_scale;

            for (Handle handle : { Handle::RotateX, Handle::RotateY, Handle::RotateZ })
            {
                const glm::vec3 axis_dir = safe_axis_dir_from_handle(handle, basis);

                // Rotation plane is orthogonal to the axis and passes through the gizmo origin.
                const Plane plane{ world_pos, axis_dir };
                float t = 0.0f;
                if (!intersect_ray_plane(ray, plane, t))
                    continue;

                const glm::vec3 hit_point = ray.origin + ray.dir * t;
                const float dist = glm::length(hit_point - world_pos);

                // Accept hits within the ring thickness.
                if (std::fabs(dist - ring_radius) <= ring_thickness)
                {
                    if (t < ray.z_near)
                    {
                        ray.z_near = t;
                        hovered_handle_ = handle;
                    }
                }
            }
        };

        const auto update_scale_hover = [&]()
        {
            const float axis_len = settings_.axis_length * gizmo_scale;
            const float box_size = settings_.scale_box_size * gizmo_scale;

            // Axis scale handles at the axis tips.
            for (Handle handle : { Handle::ScaleX, Handle::ScaleY, Handle::ScaleZ })
            {
                const glm::vec3 axis_dir = safe_axis_dir_from_handle(handle, basis);
                const glm::vec3 center = world_pos + axis_dir * axis_len;

                const glm::vec3 half = glm::vec3(box_size * 0.5f);
                const glm::vec3 aabb_min = center - half;
                const glm::vec3 aabb_max = center + half;

                float t = 0.0f;
                if (intersect_ray_aabb(ray, aabb_min, aabb_max, t) && t < ray.z_near)
                {
                    ray.z_near = t;
                    hovered_handle_ = handle;
                }
            }

            // Uniform scale handle at the origin (optional).
            if (settings_.allow_uniform_scale)
            {
                const float uniform_size = settings_.uniform_scale_size * gizmo_scale;
                const glm::vec3 half = glm::vec3(uniform_size * 0.5f);
                const glm::vec3 aabb_min = world_pos - half;
                const glm::vec3 aabb_max = world_pos + half;

                float t = 0.0f;
                if (intersect_ray_aabb(ray, aabb_min, aabb_max, t) && t < ray.z_near)
                {
                    ray.z_near = t;
                    hovered_handle_ = Handle::ScaleUniform;
                }
            }
        };

        if (!dragging_)
        {
            switch (mode_)
            {
            case Mode::Translate:
                update_translate_hover();
                break;
            case Mode::Rotate:
                update_rotate_hover();
                break;
            case Mode::Scale:
                update_scale_hover();
                break;
            }
        }

        // ------------------------------------------------------------------
        // Drag start (engage).
        // ------------------------------------------------------------------
        if (!dragging_ && hovered_handle_ != Handle::None && mouse_pressed)
        {
            dragging_ = true;
            active_handle_ = hovered_handle_;

            drag_state_.entity = entity;
            drag_state_.start_world_pos = world_pos;
            drag_state_.start_world_rot = tfm->world_rotation;
            drag_state_.start_local_pos = tfm->position;
            drag_state_.start_local_rot = tfm->rotation;
            drag_state_.start_local_scale = tfm->scale;

            if (parent_tfm)
            {
                drag_state_.parent_world_matrix = parent_tfm->world_matrix;
                drag_state_.parent_world_rot = parent_tfm->world_rotation;
            }
            else
            {
                drag_state_.parent_world_matrix = glm::mat4(1.0f);
                drag_state_.parent_world_rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            }

            if (is_axis_handle(active_handle_))
            {
                drag_state_.axis_dir_world = safe_axis_dir_from_handle(active_handle_, basis);

                // Compute axis parameter at drag start using the infinite axis line.
                // The line parameter is in world units because axis_dir is normalized.
                const RayLineResult hit = closest_points_ray_line(
                    ray,
                    world_pos,
                    drag_state_.axis_dir_world);
                drag_state_.axis_param_start = hit.line_t;
            }

            if (is_translate_plane(active_handle_))
            {
                plane_axes_from_handle(active_handle_, basis,
                    drag_state_.plane_u_world,
                    drag_state_.plane_v_world);

                drag_state_.plane_normal_world = glm::normalize(
                    glm::cross(drag_state_.plane_u_world, drag_state_.plane_v_world));

                const Plane plane{ world_pos, drag_state_.plane_normal_world };
                float t = 0.0f;
                if (intersect_ray_plane(ray, plane, t))
                {
                    drag_state_.plane_hit_start = ray.origin + ray.dir * t;
                }
            }

            if (is_rotate_handle(active_handle_))
            {
                drag_state_.axis_dir_world = safe_axis_dir_from_handle(active_handle_, basis);

                // Store the start vector on the rotation plane.
                const Plane plane{ world_pos, drag_state_.axis_dir_world };
                float t = 0.0f;
                if (intersect_ray_plane(ray, plane, t))
                {
                    const glm::vec3 hit_point = ray.origin + ray.dir * t;
                    drag_state_.rotation_start_vec = glm::normalize(hit_point - world_pos);
                }
            }

            if (active_handle_ == Handle::ScaleUniform)
            {
                // Use a plane facing the camera so uniform scaling feels stable.
                const glm::vec3 cam_forward = glm::normalize(glm::vec3(glm::inverse(view)[2]));
                drag_state_.plane_normal_world = cam_forward;

                const Plane plane{ world_pos, cam_forward };
                float t = 0.0f;
                if (intersect_ray_plane(ray, plane, t))
                {
                    const glm::vec3 hit_point = ray.origin + ray.dir * t;
                    drag_state_.axis_param_start = glm::length(hit_point - world_pos);
                }
            }
        }

        const auto update_translate_drag = [&](const glm::mat4& parent_inv) -> bool
        {
            if (active_handle_ == Handle::TranslateX
                || active_handle_ == Handle::TranslateY
                || active_handle_ == Handle::TranslateZ)
            {
                const glm::vec3 axis_dir = drag_state_.axis_dir_world;

                // Parametric axis t for the current ray on the infinite axis line.
                const RayLineResult hit = closest_points_ray_line(
                    ray,
                    drag_state_.start_world_pos,
                    axis_dir);

                float t = hit.line_t;
                float delta = t - drag_state_.axis_param_start;

                if (ctrl_pressed && settings_.linear_snap > 0.0f)
                    delta = std::round(delta / settings_.linear_snap) * settings_.linear_snap;

                // Move along the selected axis in world space.
                const glm::vec3 world_delta = axis_dir * delta;
                const glm::vec3 new_world_pos = drag_state_.start_world_pos + world_delta;

                // Convert world position back into local space:
                //   local_pos = parent_inv * [new_world_pos, 1]
                const glm::vec3 new_local = glm::vec3(parent_inv * glm::vec4(new_world_pos, 1.0f));
                tfm->set_position(new_local);
                return true;
            }

            if (is_translate_plane(active_handle_))
            {
                const Plane plane{ drag_state_.start_world_pos, drag_state_.plane_normal_world };
                float t = 0.0f;
                if (!intersect_ray_plane(ray, plane, t))
                    return false;

                // Compute hit point in world space.
                const glm::vec3 hit_point = ray.origin + ray.dir * t;

                // Drag delta relative to the initial hit point on the plane.
                glm::vec3 delta = hit_point - drag_state_.plane_hit_start;
                delta = project_onto_plane(delta,
                    drag_state_.plane_u_world,
                    drag_state_.plane_v_world);

                if (ctrl_pressed && settings_.linear_snap > 0.0f)
                {
                    // Snap within the plane basis so movement respects plane axes.
                    const float u = glm::dot(delta, drag_state_.plane_u_world);
                    const float v = glm::dot(delta, drag_state_.plane_v_world);

                    const float u_snap = std::round(u / settings_.linear_snap) * settings_.linear_snap;
                    const float v_snap = std::round(v / settings_.linear_snap) * settings_.linear_snap;

                    delta = drag_state_.plane_u_world * u_snap + drag_state_.plane_v_world * v_snap;
                }

                const glm::vec3 new_world_pos = drag_state_.start_world_pos + delta;
                const glm::vec3 new_local = glm::vec3(parent_inv * glm::vec4(new_world_pos, 1.0f));
                tfm->set_position(new_local);
            }

            return true;
        };

        const auto update_rotate_drag = [&]() -> bool
        {
            const Plane plane{ drag_state_.start_world_pos, drag_state_.axis_dir_world };
            float t = 0.0f;
            if (!intersect_ray_plane(ray, plane, t))
                return false;

            const glm::vec3 hit_point = ray.origin + ray.dir * t;
            const glm::vec3 current_vec = glm::normalize(hit_point - drag_state_.start_world_pos);

            // Signed angle between start and current vectors around axis:
            //   angle = atan2( dot(axis, cross(v0, v1)), dot(v0, v1) )
            const glm::vec3 v0 = drag_state_.rotation_start_vec;
            const glm::vec3 v1 = current_vec;
            if (glm::length(v0) < kEpsilon || glm::length(v1) < kEpsilon)
                return false;
            const float det = glm::dot(drag_state_.axis_dir_world, glm::cross(v0, v1));
            const float dotp = glm::dot(v0, v1);
            float angle = std::atan2(det, dotp);

            if (ctrl_pressed && settings_.angular_snap_deg > 0.0f)
            {
                const float snap_rad = glm::radians(settings_.angular_snap_deg);
                angle = std::round(angle / snap_rad) * snap_rad;
            }

            // Apply world-space rotation:
            //   new_world = delta_world * start_world
            const glm::quat delta_world = glm::angleAxis(angle, drag_state_.axis_dir_world);
            const glm::quat new_world = glm::normalize(delta_world * drag_state_.start_world_rot);

            // Convert to local rotation using parent world rotation:
            //   local = inverse(parent_world) * world
            const glm::quat new_local = glm::normalize(glm::inverse(drag_state_.parent_world_rot) * new_world);
            tfm->set_rotation(new_local);
            return true;
        };

        const auto update_scale_drag = [&]() -> bool
        {
            if (active_handle_ == Handle::ScaleX
                || active_handle_ == Handle::ScaleY
                || active_handle_ == Handle::ScaleZ)
            {
                const glm::vec3 axis_dir = drag_state_.axis_dir_world;

                const RayLineResult hit = closest_points_ray_line(
                    ray,
                    drag_state_.start_world_pos,
                    axis_dir);

                float t = hit.line_t;
                float delta = t - drag_state_.axis_param_start;

                glm::vec3 new_scale = drag_state_.start_local_scale;

                if (active_handle_ == Handle::ScaleX)
                    new_scale.x = drag_state_.start_local_scale.x * (1.0f + delta);
                if (active_handle_ == Handle::ScaleY)
                    new_scale.y = drag_state_.start_local_scale.y * (1.0f + delta);
                if (active_handle_ == Handle::ScaleZ)
                    new_scale.z = drag_state_.start_local_scale.z * (1.0f + delta);

                if (ctrl_pressed && settings_.scale_snap > 0.0f)
                    new_scale = snap_vector_to_grid(new_scale, settings_.scale_snap);

                tfm->set_scale(clamp_scale(new_scale, settings_.min_scale));
                return true;
            }

            if (active_handle_ == Handle::ScaleUniform)
            {
                const Plane plane{ drag_state_.start_world_pos, drag_state_.plane_normal_world };
                float t = 0.0f;
                if (!intersect_ray_plane(ray, plane, t))
                    return false;

                // Uniform scale factor = current radius / start radius.
                const glm::vec3 hit_point = ray.origin + ray.dir * t;
                const float current_radius = glm::length(hit_point - drag_state_.start_world_pos);

                if (drag_state_.axis_param_start <= kEpsilon)
                    return false;

                float factor = current_radius / drag_state_.axis_param_start;

                if (ctrl_pressed && settings_.scale_snap > 0.0f)
                    factor = std::round(factor / settings_.scale_snap) * settings_.scale_snap;

                glm::vec3 new_scale = drag_state_.start_local_scale * factor;
                tfm->set_scale(clamp_scale(new_scale, settings_.min_scale));
            }

            return true;
        };

        const auto commit_translate_drag = [&]()
        {
            if (!nearly_equal_vec3(drag_state_.start_local_pos, tfm->position))
            {
                build_assign_command(
                    ctx,
                    entity,
                    "position",
                    "position"_hs,
                    entt::meta_any{ drag_state_.start_local_pos },
                    entt::meta_any{ tfm->position });
            }
        };

        const auto commit_rotate_drag = [&]()
        {
            if (!nearly_equal_quat(drag_state_.start_local_rot, tfm->rotation))
            {
                build_assign_command(
                    ctx,
                    entity,
                    "rotation",
                    "rotation"_hs,
                    entt::meta_any{ drag_state_.start_local_rot },
                    entt::meta_any{ tfm->rotation });
            }
        };

        const auto commit_scale_drag = [&]()
        {
            if (!nearly_equal_vec3(drag_state_.start_local_scale, tfm->scale))
            {
                build_assign_command(
                    ctx,
                    entity,
                    "scale",
                    "scale"_hs,
                    entt::meta_any{ drag_state_.start_local_scale },
                    entt::meta_any{ tfm->scale });
            }
        };

        // ------------------------------------------------------------------
        // Drag update.
        // ------------------------------------------------------------------
        if (dragging_ && active_handle_ != Handle::None)
        {
            // Ensure the entity is still valid during drag.
            if (!drag_state_.entity.has_id())
            {
                dragging_ = false;
                active_handle_ = Handle::None;
                return;
            }

            // Cached parent inverse for world->local conversion.
            const glm::mat4 parent_inv = glm::inverse(drag_state_.parent_world_matrix);

            if (is_translate_handle(active_handle_))
            {
                if (!update_translate_drag(parent_inv))
                    return;
            }
            else if (is_rotate_handle(active_handle_))
            {
                if (!update_rotate_drag())
                    return;
            }
            else if (is_scale_handle(active_handle_))
            {
                if (!update_scale_drag())
                    return;
            }
        }

        // ------------------------------------------------------------------
        // Drag end (commit command for undo).
        // ------------------------------------------------------------------
        if (dragging_ && mouse_released)
        {
            dragging_ = false;

            if (active_handle_ != Handle::None)
            {
                if (is_translate_handle(active_handle_))
                {
                    commit_translate_drag();
                }
                else if (is_rotate_handle(active_handle_))
                {
                    commit_rotate_drag();
                }
                else if (is_scale_handle(active_handle_))
                {
                    commit_scale_drag();
                }
            }

            active_handle_ = Handle::None;
        }
    }

    void ManipulatorGizmo::render(
        EngineContext& ctx,
        ShapeRendering::ShapeRenderer& renderer,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::mat4& viewport,
        const glm::ivec2& window_size) const
    {
        if (window_size.x <= 0 || window_size.y <= 0)
            return;

        ecs::Entity entity{};
        ecs::TransformComponent* tfm = nullptr;
        ecs::TransformComponent* parent_tfm = nullptr;
        if (!resolve_target(ctx, entity, tfm, parent_tfm))
            return;

        const glm::vec3 world_pos = extract_translation(tfm->world_matrix);
        const GizmoBasis basis = compute_gizmo_basis(*tfm, mode_, space_);

        const float gizmo_scale = compute_screen_scale(
            world_pos,
            settings_.screen_size,
            view,
            proj,
            viewport);

        const ShapeRendering::DepthTest depth = settings_.draw_on_top
            ? ShapeRendering::DepthTest::False
            : ShapeRendering::DepthTest::True;

        const ShapeRendering::BackfaceCull cull = ShapeRendering::BackfaceCull::False;

        if (mode_ == Mode::Translate)
        {
            const float axis_len = settings_.axis_length * gizmo_scale;
            const float axis_radius = settings_.axis_radius * gizmo_scale;

            const ShapeRendering::ArrowDescriptor arrow_desc{
                .cone_fraction = 0.2f,
                .cone_radius = axis_radius * 2.0f,
                .cylinder_radius = axis_radius
            };

            for (Handle handle : { Handle::TranslateX, Handle::TranslateY, Handle::TranslateZ })
            {
                const glm::vec3 axis_dir = safe_axis_dir_from_handle(handle, basis);
                const glm::vec3 end = world_pos + axis_dir * axis_len;
                const auto color = resolve_handle_color(handle, hovered_handle_, active_handle_);

                renderer.push_states(depth, cull, color);
                renderer.push_arrow(world_pos, end, arrow_desc);
                renderer.pop_states<ShapeRendering::DepthTest, ShapeRendering::BackfaceCull, ShapeRendering::Color4u>();
            }

            for (Handle handle : { Handle::TranslateXY, Handle::TranslateYZ, Handle::TranslateZX })
            {
                glm::vec3 u, v;
                plane_axes_from_handle(handle, basis, u, v);
                if (glm::length(u) < kEpsilon || glm::length(v) < kEpsilon)
                    continue;

                const float offset = settings_.plane_offset * gizmo_scale;
                const float size = settings_.plane_size * gizmo_scale;

                const glm::vec3 p0 = world_pos + u * offset + v * offset;
                const glm::vec3 p1 = p0 + u * size;
                const glm::vec3 p2 = p1 + v * size;
                const glm::vec3 p3 = p0 + v * size;
                const glm::vec3 points[4] = { p0, p1, p2, p3 };

                const glm::vec3 n = glm::normalize(glm::cross(u, v));
                const auto color = resolve_handle_color(handle, hovered_handle_, active_handle_);

                renderer.push_states(depth, cull, color);
                renderer.push_quad(points, n);
                renderer.pop_states<ShapeRendering::DepthTest, ShapeRendering::BackfaceCull, ShapeRendering::Color4u>();
            }
        }
        else if (mode_ == Mode::Rotate)
        {
            const float ring_radius = settings_.rotate_radius * gizmo_scale;

            for (Handle handle : { Handle::RotateX, Handle::RotateY, Handle::RotateZ })
            {
                const glm::vec3 axis_dir = safe_axis_dir_from_handle(handle, basis);

                // Build a ring basis (u,v) perpendicular to the axis by crossing with
                // a helper vector that is *least* aligned with the axis (numerically stable).
                const glm::vec3 helper =
                    (std::fabs(axis_dir.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                   : glm::vec3(1.0f, 0.0f, 0.0f);
                glm::vec3 u = glm::cross(axis_dir, helper);
                u = safe_normalize(u, glm::vec3(0.0f, 0.0f, 1.0f));
                glm::vec3 v = glm::cross(axis_dir, u);
                v = safe_normalize(v, glm::vec3(1.0f, 0.0f, 0.0f));

                const glm::mat4 ring_transform = make_basis_matrix(u, v, axis_dir, world_pos, ring_radius);
                const auto color = resolve_handle_color(handle, hovered_handle_, active_handle_);

                renderer.push_states(depth, cull, color, ring_transform);
                renderer.push_circle_ring<64>();
                renderer.pop_states<ShapeRendering::DepthTest, ShapeRendering::BackfaceCull, ShapeRendering::Color4u, glm::mat4>();
            }
        }
        else if (mode_ == Mode::Scale)
        {
            const float axis_len = settings_.axis_length * gizmo_scale;
            const float box_size = settings_.scale_box_size * gizmo_scale;

            for (Handle handle : { Handle::ScaleX, Handle::ScaleY, Handle::ScaleZ })
            {
                const glm::vec3 axis_dir = safe_axis_dir_from_handle(handle, basis);
                const glm::vec3 end = world_pos + axis_dir * axis_len;
                const auto color = resolve_handle_color(handle, hovered_handle_, active_handle_);

                renderer.push_states(depth, cull, color);
                renderer.push_line(world_pos, end);
                renderer.pop_states<ShapeRendering::DepthTest, ShapeRendering::BackfaceCull, ShapeRendering::Color4u>();

                const glm::mat4 cube_transform =
                    glm::translate(glm::mat4(1.0f), end)
                    * glm::scale(glm::mat4(1.0f), glm::vec3(box_size));

                renderer.push_states(depth, cull, color, cube_transform);
                renderer.push_cube();
                renderer.pop_states<ShapeRendering::DepthTest, ShapeRendering::BackfaceCull, ShapeRendering::Color4u, glm::mat4>();
            }

            if (settings_.allow_uniform_scale)
            {
                const float size = settings_.uniform_scale_size * gizmo_scale;
                const glm::mat4 cube_transform =
                    glm::translate(glm::mat4(1.0f), world_pos)
                    * glm::scale(glm::mat4(1.0f), glm::vec3(size));

                const auto color = resolve_handle_color(Handle::ScaleUniform, hovered_handle_, active_handle_);
                renderer.push_states(depth, cull, color, cube_transform);
                renderer.push_cube();
                renderer.pop_states<ShapeRendering::DepthTest, ShapeRendering::BackfaceCull, ShapeRendering::Color4u, glm::mat4>();
            }
        }
    }
} // namespace eeng::editor
