// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace entt
{
    class meta_any;
}

namespace eeng::serializers
{
    nlohmann::json serialize_vec2(const glm::vec2& v);
    nlohmann::json serialize_vec3(const glm::vec3& v);
    nlohmann::json serialize_vec4(const glm::vec4& v);
    nlohmann::json serialize_ivec2(const glm::ivec2& v);
    nlohmann::json serialize_ivec3(const glm::ivec3& v);
    nlohmann::json serialize_ivec4(const glm::ivec4& v);
    nlohmann::json serialize_quat(const glm::quat& q);
    nlohmann::json serialize_mat2(const glm::mat2& m);
    nlohmann::json serialize_mat3(const glm::mat3& m);
    nlohmann::json serialize_mat4(const glm::mat4& m);

    glm::vec2 deserialize_vec2(const nlohmann::json& j);
    glm::vec3 deserialize_vec3(const nlohmann::json& j);
    glm::vec4 deserialize_vec4(const nlohmann::json& j);
    glm::ivec2 deserialize_ivec2(const nlohmann::json& j);
    glm::ivec3 deserialize_ivec3(const nlohmann::json& j);
    glm::ivec4 deserialize_ivec4(const nlohmann::json& j);
    glm::quat deserialize_quat(const nlohmann::json& j);
    glm::mat2 deserialize_mat2(const nlohmann::json& j);
    glm::mat3 deserialize_mat3(const nlohmann::json& j);
    glm::mat4 deserialize_mat4(const nlohmann::json& j);

    nlohmann::json serialize_vec2_array(const std::vector<glm::vec2>& values);
    nlohmann::json serialize_vec3_array(const std::vector<glm::vec3>& values);
    nlohmann::json serialize_quat_array(const std::vector<glm::quat>& values);
    void deserialize_vec2_array(const nlohmann::json& j, std::vector<glm::vec2>& values);
    void deserialize_vec3_array(const nlohmann::json& j, std::vector<glm::vec3>& values);
    void deserialize_quat_array(const nlohmann::json& j, std::vector<glm::quat>& values);

    void serialize_glmvec2(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_glmvec2(const nlohmann::json& j, entt::meta_any& any);
    void serialize_glmvec3(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_glmvec3(const nlohmann::json& j, entt::meta_any& any);
    void serialize_glmvec4(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_glmvec4(const nlohmann::json& j, entt::meta_any& any);
    void serialize_glmivec2(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_glmivec2(const nlohmann::json& j, entt::meta_any& any);
    void serialize_glmivec3(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_glmivec3(const nlohmann::json& j, entt::meta_any& any);
    void serialize_glmivec4(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_glmivec4(const nlohmann::json& j, entt::meta_any& any);
    void serialize_glmquat(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_glmquat(const nlohmann::json& j, entt::meta_any& any);
    void serialize_glmmat2(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_glmmat2(const nlohmann::json& j, entt::meta_any& any);
    void serialize_glmmat3(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_glmmat3(const nlohmann::json& j, entt::meta_any& any);
    void serialize_glmmat4(nlohmann::json& j, const entt::meta_any& any);
    void deserialize_glmmat4(const nlohmann::json& j, entt::meta_any& any);
}
