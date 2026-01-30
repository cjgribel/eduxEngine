// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "serializers/GLMSerialize.hpp"

#include <algorithm>
#include <cassert>

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

namespace eeng::serializers
{
    namespace
    {
        template<typename VecT>
        nlohmann::json serialize_vec(const VecT& v)
        {
            nlohmann::json j = nlohmann::json::array();
            auto& arr = j.get_ref<nlohmann::json::array_t&>();
            const int n = static_cast<int>(VecT::length());
            arr.reserve(n);
            for (int i = 0; i < n; ++i)
                arr.emplace_back(v[i]);
            return j;
        }

        template<typename VecT>
        VecT deserialize_vec(const nlohmann::json& j)
        {
            using scalar_t = typename VecT::value_type;
            VecT v{};

            if (j.is_array())
            {
                const size_t n = std::min<size_t>(j.size(), VecT::length());
                for (size_t i = 0; i < n; ++i)
                    v[static_cast<int>(i)] = j[i].get<scalar_t>();
            }
            else if (j.is_object())
            {
                if constexpr (VecT::length() >= 1) v.x = j.value("x", scalar_t{});
                if constexpr (VecT::length() >= 2) v.y = j.value("y", scalar_t{});
                if constexpr (VecT::length() >= 3) v.z = j.value("z", scalar_t{});
                if constexpr (VecT::length() >= 4) v.w = j.value("w", scalar_t{});
            }
            return v;
        }

        template<typename MatT>
        nlohmann::json serialize_mat(const MatT& m)
        {
            nlohmann::json j = nlohmann::json::array();
            auto& arr = j.get_ref<nlohmann::json::array_t&>();
            const int cols = static_cast<int>(MatT::length());
            const int rows = static_cast<int>(MatT::col_type::length());
            arr.reserve(static_cast<size_t>(cols * rows));
            for (int c = 0; c < cols; ++c)
            {
                for (int r = 0; r < rows; ++r)
                    arr.emplace_back(m[c][r]);
            }
            return j;
        }

        template<typename MatT>
        MatT deserialize_mat(const nlohmann::json& j)
        {
            using scalar_t = typename MatT::value_type;
            MatT m{ scalar_t{1} };
            const int cols = static_cast<int>(MatT::length());
            const int rows = static_cast<int>(MatT::col_type::length());
            const size_t count = static_cast<size_t>(cols * rows);
            if (j.is_array() && j.size() >= count)
            {
                size_t idx = 0;
                for (int c = 0; c < cols; ++c)
                {
                    for (int r = 0; r < rows; ++r)
                        m[c][r] = j[idx++].get<scalar_t>();
                }
            }
            return m;
        }

        template<typename VecT, typename SerializeFn>
        nlohmann::json serialize_vec_array(const std::vector<VecT>& values, SerializeFn serialize_fn)
        {
            nlohmann::json j = nlohmann::json::array();
            auto& arr = j.get_ref<nlohmann::json::array_t&>();
            arr.reserve(values.size());
            for (const auto& v : values)
                arr.emplace_back(serialize_fn(v));
            return j;
        }

        template<typename VecT, typename DeserializeFn>
        void deserialize_vec_array(const nlohmann::json& j, std::vector<VecT>& values, DeserializeFn deserialize_fn)
        {
            values.clear();
            if (!j.is_array())
                return;
            values.resize(j.size());
            for (size_t i = 0; i < j.size(); i++)
                values[i] = deserialize_fn(j[i]);
        }
    } // namespace

    nlohmann::json serialize_vec2(const glm::vec2& v) { return serialize_vec(v); }
    nlohmann::json serialize_vec3(const glm::vec3& v) { return serialize_vec(v); }
    nlohmann::json serialize_vec4(const glm::vec4& v) { return serialize_vec(v); }
    nlohmann::json serialize_ivec2(const glm::ivec2& v) { return serialize_vec(v); }
    nlohmann::json serialize_ivec3(const glm::ivec3& v) { return serialize_vec(v); }
    nlohmann::json serialize_ivec4(const glm::ivec4& v) { return serialize_vec(v); }

    nlohmann::json serialize_quat(const glm::quat& q)
    {
        return nlohmann::json::array({ q.x, q.y, q.z, q.w });
    }

    nlohmann::json serialize_mat2(const glm::mat2& m) { return serialize_mat(m); }
    nlohmann::json serialize_mat3(const glm::mat3& m) { return serialize_mat(m); }
    nlohmann::json serialize_mat4(const glm::mat4& m) { return serialize_mat(m); }

    glm::vec2 deserialize_vec2(const nlohmann::json& j) { return deserialize_vec<glm::vec2>(j); }
    glm::vec3 deserialize_vec3(const nlohmann::json& j) { return deserialize_vec<glm::vec3>(j); }
    glm::vec4 deserialize_vec4(const nlohmann::json& j) { return deserialize_vec<glm::vec4>(j); }
    glm::ivec2 deserialize_ivec2(const nlohmann::json& j) { return deserialize_vec<glm::ivec2>(j); }
    glm::ivec3 deserialize_ivec3(const nlohmann::json& j) { return deserialize_vec<glm::ivec3>(j); }
    glm::ivec4 deserialize_ivec4(const nlohmann::json& j) { return deserialize_vec<glm::ivec4>(j); }

    glm::quat deserialize_quat(const nlohmann::json& j)
    {
        if (j.is_array() && j.size() >= 4)
            return glm::quat(j[3].get<float>(), j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
        if (j.is_object())
            return glm::quat(j.value("w", 1.0f), j.value("x", 0.0f), j.value("y", 0.0f), j.value("z", 0.0f));
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    glm::mat2 deserialize_mat2(const nlohmann::json& j) { return deserialize_mat<glm::mat2>(j); }
    glm::mat3 deserialize_mat3(const nlohmann::json& j) { return deserialize_mat<glm::mat3>(j); }
    glm::mat4 deserialize_mat4(const nlohmann::json& j) { return deserialize_mat<glm::mat4>(j); }

    nlohmann::json serialize_vec2_array(const std::vector<glm::vec2>& values)
    {
        return serialize_vec_array(values, serialize_vec2);
    }

    nlohmann::json serialize_vec3_array(const std::vector<glm::vec3>& values)
    {
        return serialize_vec_array(values, serialize_vec3);
    }

    nlohmann::json serialize_quat_array(const std::vector<glm::quat>& values)
    {
        return serialize_vec_array(values, serialize_quat);
    }

    void deserialize_vec2_array(const nlohmann::json& j, std::vector<glm::vec2>& values)
    {
        deserialize_vec_array(j, values, deserialize_vec2);
    }

    void deserialize_vec3_array(const nlohmann::json& j, std::vector<glm::vec3>& values)
    {
        deserialize_vec_array(j, values, deserialize_vec3);
    }

    void deserialize_quat_array(const nlohmann::json& j, std::vector<glm::quat>& values)
    {
        deserialize_vec_array(j, values, deserialize_quat);
    }

    void serialize_glmvec2(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::vec2>();
        assert(ptr && "serialize_glmvec2: bad meta_any");
        j = serialize_vec2(*ptr);
    }

    void deserialize_glmvec2(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::vec2>();
        assert(ptr && "deserialize_glmvec2: bad meta_any");
        *ptr = deserialize_vec2(j);
    }

    void serialize_glmvec3(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::vec3>();
        assert(ptr && "serialize_glmvec3: bad meta_any");
        j = serialize_vec3(*ptr);
    }

    void deserialize_glmvec3(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::vec3>();
        assert(ptr && "deserialize_glmvec3: bad meta_any");
        *ptr = deserialize_vec3(j);
    }

    void serialize_glmvec4(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::vec4>();
        assert(ptr && "serialize_glmvec4: bad meta_any");
        j = serialize_vec4(*ptr);
    }

    void deserialize_glmvec4(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::vec4>();
        assert(ptr && "deserialize_glmvec4: bad meta_any");
        *ptr = deserialize_vec4(j);
    }

    void serialize_glmivec2(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::ivec2>();
        assert(ptr && "serialize_glmivec2: bad meta_any");
        j = serialize_ivec2(*ptr);
    }

    void deserialize_glmivec2(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::ivec2>();
        assert(ptr && "deserialize_glmivec2: bad meta_any");
        *ptr = deserialize_ivec2(j);
    }

    void serialize_glmivec3(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::ivec3>();
        assert(ptr && "serialize_glmivec3: bad meta_any");
        j = serialize_ivec3(*ptr);
    }

    void deserialize_glmivec3(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::ivec3>();
        assert(ptr && "deserialize_glmivec3: bad meta_any");
        *ptr = deserialize_ivec3(j);
    }

    void serialize_glmivec4(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::ivec4>();
        assert(ptr && "serialize_glmivec4: bad meta_any");
        j = serialize_ivec4(*ptr);
    }

    void deserialize_glmivec4(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::ivec4>();
        assert(ptr && "deserialize_glmivec4: bad meta_any");
        *ptr = deserialize_ivec4(j);
    }

    void serialize_glmquat(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::quat>();
        assert(ptr && "serialize_glmquat: bad meta_any");
        j = serialize_quat(*ptr);
    }

    void deserialize_glmquat(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::quat>();
        assert(ptr && "deserialize_glmquat: bad meta_any");
        *ptr = deserialize_quat(j);
    }

    void serialize_glmmat2(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::mat2>();
        assert(ptr && "serialize_glmmat2: bad meta_any");
        j = serialize_mat2(*ptr);
    }

    void deserialize_glmmat2(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::mat2>();
        assert(ptr && "deserialize_glmmat2: bad meta_any");
        *ptr = deserialize_mat2(j);
    }

    void serialize_glmmat3(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::mat3>();
        assert(ptr && "serialize_glmmat3: bad meta_any");
        j = serialize_mat3(*ptr);
    }

    void deserialize_glmmat3(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::mat3>();
        assert(ptr && "deserialize_glmmat3: bad meta_any");
        *ptr = deserialize_mat3(j);
    }

    void serialize_glmmat4(nlohmann::json& j, const entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::mat4>();
        assert(ptr && "serialize_glmmat4: bad meta_any");
        j = serialize_mat4(*ptr);
    }

    void deserialize_glmmat4(const nlohmann::json& j, entt::meta_any& any)
    {
        auto ptr = any.try_cast<glm::mat4>();
        assert(ptr && "deserialize_glmmat4: bad meta_any");
        *ptr = deserialize_mat4(j);
    }
} // namespace eeng::serializers
