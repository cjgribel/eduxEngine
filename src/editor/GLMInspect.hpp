// InspectGlm.hpp
#pragma once

#include <cassert>
#include <type_traits>

#include "EngineContext.hpp"
#include "InspectorState.hpp"

#include "entt/entt.hpp"
#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace eeng::editor
{
    namespace detail
    {
        template <class VecT>
        bool inspect_glm_vector_inline(entt::meta_any& any)
        {
            auto ptr = any.try_cast<VecT>();
            assert(ptr && "inspect_glm_vector_inline: could not cast meta_any to requested glm vector type");

            using scalar_t = typename VecT::value_type;
            constexpr int n = static_cast<int>(VecT::length());

            if constexpr (std::is_same_v<scalar_t, float>)
            {
                float* v = glm::value_ptr(*ptr);
                const float speed = 0.05f;

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

    // ---- EXACT signatures for EnTT meta func registration -------------------

    inline bool inspect_glmvec2(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        return detail::inspect_glm_vector_inline<glm::vec2>(any);
    }

    inline bool inspect_glmvec3(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        return detail::inspect_glm_vector_inline<glm::vec3>(any);
    }

    inline bool inspect_glmvec4(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        return detail::inspect_glm_vector_inline<glm::vec4>(any);
    }

    inline bool inspect_glmivec2(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        return detail::inspect_glm_vector_inline<glm::ivec2>(any);
    }

    inline bool inspect_glmivec3(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        return detail::inspect_glm_vector_inline<glm::ivec3>(any);
    }

    inline bool inspect_glmivec4(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        return detail::inspect_glm_vector_inline<glm::ivec4>(any);
    }

    inline bool inspect_glmmat2(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        detail::inspect_glm_matrix_readonly_inline<glm::mat2>(any);
        return false;
    }

    inline bool inspect_glmmat3(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        detail::inspect_glm_matrix_readonly_inline<glm::mat3>(any);
        return false;
    }

    inline bool inspect_glmmat4(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx)
    {
        (void)inspector; (void)ctx;
        detail::inspect_glm_matrix_readonly_inline<glm::mat4>(any);
        return false;
    }
} // namespace eeng::editor
