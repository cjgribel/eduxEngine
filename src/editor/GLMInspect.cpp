// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "editor/GLMInspect.hpp"
#include "meta/MetaInfo.h"

#include <cassert>
#include <type_traits>

#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace eeng::editor
{
    namespace detail
    {
        inline constexpr float kDegToRad = 0.01745329251994329577f;
        inline constexpr float kRadToDeg = 57.2957795130823208768f;

        inline bool use_angle_degrees(const InspectorState& inspector)
        {
            const auto* meta = inspector.current_data_meta_info;
            return meta && eeng::has_ui_hint(meta->ui_hints, eeng::InspectorUiHint::AngleDegrees);
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
                    float deg[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    for (int i = 0; i < n; ++i)
                        deg[i] = v[i] * kRadToDeg;

                    bool changed = false;
                    if (meta && meta->ui_has_range)
                    {
                        if constexpr (n == 2) { changed = ImGui::SliderFloat2("##glm_vec", deg, meta->ui_range_min, meta->ui_range_max, "%.2f deg"); }
                        if constexpr (n == 3) { changed = ImGui::SliderFloat3("##glm_vec", deg, meta->ui_range_min, meta->ui_range_max, "%.2f deg"); }
                        if constexpr (n == 4) { changed = ImGui::SliderFloat4("##glm_vec", deg, meta->ui_range_min, meta->ui_range_max, "%.2f deg"); }
                    }
                    else
                    {
                        if constexpr (n == 2) { changed = ImGui::DragFloat2("##glm_vec", deg, speed, 0.0f, 0.0f, "%.2f deg"); }
                        if constexpr (n == 3) { changed = ImGui::DragFloat3("##glm_vec", deg, speed, 0.0f, 0.0f, "%.2f deg"); }
                        if constexpr (n == 4) { changed = ImGui::DragFloat4("##glm_vec", deg, speed, 0.0f, 0.0f, "%.2f deg"); }
                    }

                    if (changed)
                    {
                        for (int i = 0; i < n; ++i)
                            v[i] = deg[i] * kDegToRad;
                    }
                    return changed;
                }

                if constexpr (n == 2) { return ImGui::DragFloat2("##glm_vec", v, speed); }
                if constexpr (n == 3) { return ImGui::DragFloat3("##glm_vec", v, speed); }
                if constexpr (n == 4) { return ImGui::DragFloat4("##glm_vec", v, speed); }
            }
            else if constexpr (std::is_same_v<scalar_t, int>)
            {
                int* v = glm::value_ptr(*ptr);
                const float speed = 1.0f;

                if constexpr (n == 2) { return ImGui::DragInt2("##glm_vec", v, speed); }
                if constexpr (n == 3) { return ImGui::DragInt3("##glm_vec", v, speed); }
                if constexpr (n == 4) { return ImGui::DragInt4("##glm_vec", v, speed); }
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
