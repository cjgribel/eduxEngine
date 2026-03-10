// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/GLMInspect.hpp"
#include "editor/InspectorEditPolicy.hpp"
#include "meta/MetaInfo.h"

#include <array>
#include <cassert>
#include <type_traits>

#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace eeng::editor
{
    namespace detail
    {
        inline constexpr float kDegToRad = 0.01745329251994329577f;
        inline constexpr float kRadToDeg = 57.2957795130823208768f;

        // Field-level UI hints are pushed into InspectorState by MetaInspect.
        // Vector/quaternion widgets consult those hints to decide representation.
        inline bool use_angle_degrees(const InspectorState& inspector)
        {
            const auto* meta = inspector.current_data_meta_info;
            return meta && eeng::has_ui_hint(meta->ui_hints, eeng::InspectorUiHint::AngleDegrees);
        }

        inline bool use_quat_euler_degrees(const InspectorState& inspector)
        {
            const auto* meta = inspector.current_data_meta_info;
            return meta && eeng::has_ui_hint(meta->ui_hints, eeng::InspectorUiHint::QuaternionEulerDegrees);
        }

        template <class VecT>
        bool inspect_glm_vector_inline(entt::meta_any& any, InspectorState& inspector)
        {
            auto ptr = any.try_cast<VecT>();
            assert(ptr && "inspect_glm_vector_inline: could not cast meta_any to requested glm vector type");

            using scalar_t = typename VecT::value_type;
            constexpr int n = static_cast<int>(VecT::length());

            if constexpr (std::is_same_v<scalar_t, float>)
            {
                float* v = glm::value_ptr(*ptr);
                const auto* meta = inspector.current_data_meta_info;
                const float speed = (meta && meta->ui_speed > 0.0f) ? meta->ui_speed : 0.05f;

                if (use_angle_degrees(inspector))
                {
                    // Storage remains radians; only the UI representation switches to degrees.
                    std::array<float, 4> source_deg{ 0.0f, 0.0f, 0.0f, 0.0f };
                    for (int i = 0; i < n; ++i)
                        source_deg[i] = v[i] * kRadToDeg;

                    std::array<float, 4> committed_deg = source_deg;
                    if (!edit_with_commit_on_end_buffered(
                        "##glm_vec",
                        source_deg,
                        committed_deg,
                        [&](std::array<float, 4>& deg) {
                            bool changed = false;
                            if (meta && meta->ui_has_range)
                            {
                                if constexpr (n == 2) { changed = ImGui::SliderFloat2("##glm_vec", deg.data(), meta->ui_range_min, meta->ui_range_max, "%.2f deg"); }
                                if constexpr (n == 3) { changed = ImGui::SliderFloat3("##glm_vec", deg.data(), meta->ui_range_min, meta->ui_range_max, "%.2f deg"); }
                                if constexpr (n == 4) { changed = ImGui::SliderFloat4("##glm_vec", deg.data(), meta->ui_range_min, meta->ui_range_max, "%.2f deg"); }
                            }
                            else
                            {
                                if constexpr (n == 2) { changed = ImGui::DragFloat2("##glm_vec", deg.data(), speed, 0.0f, 0.0f, "%.2f deg"); }
                                if constexpr (n == 3) { changed = ImGui::DragFloat3("##glm_vec", deg.data(), speed, 0.0f, 0.0f, "%.2f deg"); }
                                if constexpr (n == 4) { changed = ImGui::DragFloat4("##glm_vec", deg.data(), speed, 0.0f, 0.0f, "%.2f deg"); }
                            }

                            if (changed)
                            {
                                // Snap in degrees so inspector steps match what the user sees.
                                apply_snap_from_meta(deg, meta);
                            }
                            return changed;
                        }))
                    {
                        return false;
                    }

                    // Convert back to radians before writing to the component field.
                    for (int i = 0; i < n; ++i)
                        v[i] = committed_deg[i] * kDegToRad;
                    return true;
                }

                std::array<float, 4> source{ 0.0f, 0.0f, 0.0f, 0.0f };
                for (int i = 0; i < n; ++i)
                    source[i] = v[i];

                std::array<float, 4> committed = source;
                if (!edit_with_commit_on_end_buffered(
                    "##glm_vec",
                    source,
                    committed,
                    [&](std::array<float, 4>& p) {
                        bool changed = false;
                        if constexpr (n == 2) { changed = ImGui::DragFloat2("##glm_vec", p.data(), speed); }
                        if constexpr (n == 3) { changed = ImGui::DragFloat3("##glm_vec", p.data(), speed); }
                        if constexpr (n == 4) { changed = ImGui::DragFloat4("##glm_vec", p.data(), speed); }
                        if (changed)
                            apply_snap_from_meta(p, meta);
                        return changed;
                    }))
                {
                    return false;
                }

                for (int i = 0; i < n; ++i)
                    v[i] = committed[i];
                return true;
            }
            else if constexpr (std::is_same_v<scalar_t, int>)
            {
                int* v = glm::value_ptr(*ptr);
                const float speed = 1.0f;

                std::array<int, 4> source{ 0, 0, 0, 0 };
                for (int i = 0; i < n; ++i)
                    source[i] = v[i];

                std::array<int, 4> committed = source;
                if (!edit_with_commit_on_end_buffered(
                    "##glm_vec",
                    source,
                    committed,
                    [&](std::array<int, 4>& p) {
                        if constexpr (n == 2) { return ImGui::DragInt2("##glm_vec", p.data(), speed); }
                        if constexpr (n == 3) { return ImGui::DragInt3("##glm_vec", p.data(), speed); }
                        if constexpr (n == 4) { return ImGui::DragInt4("##glm_vec", p.data(), speed); }
                        return false;
                    }))
                {
                    return false;
                }

                for (int i = 0; i < n; ++i)
                    v[i] = committed[i];
                return true;
            }

            ImGui::TextDisabled("Unsupported glm vector type");
            return false;
        }

        template <class MatT>
        void inspect_glm_matrix_readonly_inline(entt::meta_any& any)
        {
            auto ptr = any.try_cast<MatT>();
            assert(ptr && "inspect_glm_matrix_readonly_inline: could not cast meta_any to requested glm matrix type");

            using scalar_t = typename MatT::value_type;

            const int cols = static_cast<int>(MatT::length());
            const int rows = static_cast<int>(MatT::col_type::length());

            const ImGuiTableFlags flags =
                ImGuiTableFlags_SizingFixedFit |
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_BordersInnerH;

            // The leaf/node already provides a unique ID scope, so this is safe:
            if (ImGui::BeginTable("##glm_mat", cols, flags))
            {
                for (int r = 0; r < rows; ++r)
                {
                    ImGui::TableNextRow();
                    for (int c = 0; c < cols; ++c)
                    {
                        ImGui::TableSetColumnIndex(c);

                        const scalar_t v = (*ptr)[c][r]; // glm is column-major: m[col][row]

                        if constexpr (std::is_floating_point_v<scalar_t>)
                        {
                            ImGui::TextDisabled("%.3f", static_cast<double>(v));
                        }
                        else if constexpr (std::is_integral_v<scalar_t>)
                        {
                            ImGui::TextDisabled("%d", static_cast<int>(v));
                        }
                        else
                        {
                            ImGui::TextDisabled("?");
                        }
                    }
                }

                ImGui::EndTable();
            }
        }
    } // namespace detail

    bool inspect_glmvec2(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)ctx;
        return detail::inspect_glm_vector_inline<glm::vec2>(any, inspector);
    }

    bool inspect_glmvec3(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)ctx;
        return detail::inspect_glm_vector_inline<glm::vec3>(any, inspector);
    }

    bool inspect_glmvec4(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)ctx;
        return detail::inspect_glm_vector_inline<glm::vec4>(any, inspector);
    }

    bool inspect_glmivec2(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)ctx;
        return detail::inspect_glm_vector_inline<glm::ivec2>(any, inspector);
    }

    bool inspect_glmivec3(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)ctx;
        return detail::inspect_glm_vector_inline<glm::ivec3>(any, inspector);
    }

    bool inspect_glmivec4(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)ctx;
        return detail::inspect_glm_vector_inline<glm::ivec4>(any, inspector);
    }

    bool inspect_glmquat(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)ctx;
        auto* ptr = any.try_cast<glm::quat>();
        const auto* cptr = any.try_cast<const glm::quat>();
        if (!ptr && !cptr)
            return false;

        const auto* meta = inspector.current_data_meta_info;
        const float speed = (meta && meta->ui_speed > 0.0f) ? meta->ui_speed : 1.0f;
        const bool use_euler = detail::use_quat_euler_degrees(inspector);
        const glm::quat source = ptr ? *ptr : *cptr;

        if (use_euler)
        {
            // UX mode: present quaternion as Euler degrees in the inspector.
            // This is not a canonical representation and can hit Euler singularities,
            // but it is usually the most intuitive editing mode.
            const glm::vec3 source_euler_deg = glm::degrees(glm::eulerAngles(source));
            std::array<float, 3> source_deg = {
                source_euler_deg.x, source_euler_deg.y, source_euler_deg.z
            };
            std::array<float, 3> committed_deg = source_deg;

            if (!edit_with_commit_on_end_buffered(
                "##glm_quat_euler",
                source_deg,
                committed_deg,
                [&](std::array<float, 3>& p) {
                    if (meta && meta->ui_has_range)
                    {
                        return ImGui::SliderFloat3(
                            "##glm_quat_euler",
                            p.data(),
                            meta->ui_range_min,
                            meta->ui_range_max,
                            "%.2f deg");
                    }
                    return ImGui::DragFloat3(
                        "##glm_quat_euler",
                        p.data(),
                        speed,
                        0.0f,
                        0.0f,
                        "%.2f deg");
                }))
            {
                return false;
            }

            if (!ptr)
                return false;

            // Rebuild and normalize to keep drift from repeated edits under control.
            *ptr = glm::normalize(glm::quat(glm::radians(glm::vec3(
                committed_deg[0], committed_deg[1], committed_deg[2]))));
            return true;
        }

        std::array<float, 4> source_raw = { source.x, source.y, source.z, source.w };
        std::array<float, 4> committed_raw = source_raw;

        if (!edit_with_commit_on_end_buffered(
            "##glm_quat",
            source_raw,
            committed_raw,
            [&](std::array<float, 4>& p) {
                return ImGui::DragFloat4("##glm_quat", p.data(), speed * 0.01f);
            }))
        {
            return false;
        }

        if (!ptr)
            return false;

        // Raw quaternion edit path (x,y,z,w), normalized for numerical safety.
        *ptr = glm::normalize(glm::quat(
            committed_raw[3], committed_raw[0], committed_raw[1], committed_raw[2]));
        return true;
    }

    bool inspect_glmmat2(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        detail::inspect_glm_matrix_readonly_inline<glm::mat2>(any);
        return false;
    }

    bool inspect_glmmat3(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        detail::inspect_glm_matrix_readonly_inline<glm::mat3>(any);
        return false;
    }

    bool inspect_glmmat4(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        detail::inspect_glm_matrix_readonly_inline<glm::mat4>(any);
        return false;
    }
} // namespace eeng::editor
