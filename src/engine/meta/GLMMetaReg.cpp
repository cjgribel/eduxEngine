// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "meta/GLMMetaReg.hpp"

#include <stdexcept>

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "MetaAux.h"
#include "MetaInfo.h"
#include "MetaLiterals.h"
#include "editor/GLMInspect.hpp"
#include "serializers/GLMSerialize.hpp"

namespace eeng::meta
{
    namespace
    {
        template<class T>
        void warm_start_meta_type()
        {
            if (!entt::resolve<T>())
                throw std::runtime_error("entt::resolve() failed for GLM meta type");
        }

        template<typename T>
        void register_helper_type()
        {
            warm_start_meta_type<T>();
        }
    } // namespace

    void register_glm_meta_types()
    {
        entt::meta_factory<glm::vec2>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.vec2", .name = "Vec2", .tooltip = "Vec2." })
            .traits(MetaFlags::none)

            .data<&glm::vec2::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "X." }).traits(MetaFlags::none)

            .data<&glm::vec2::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "Y." }).traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_glmvec2>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_glmvec2>(eeng::literals::deserialize_hs)
            .template func<&eeng::editor::inspect_glmvec2>(eeng::literals::inspect_hs)
            ;
        register_type<glm::vec2>();
        register_helper_type<glm::vec2>();

        entt::meta_factory<glm::vec3>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.vec3", .name = "Vec3", .tooltip = "Vec3." })
            .traits(MetaFlags::none)

            .data<&glm::vec3::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "X." }).traits(MetaFlags::none)

            .data<&glm::vec3::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "Y." }).traits(MetaFlags::none)

            .data<&glm::vec3::z>("z"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "z", "Z", "Z." }).traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_glmvec3>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_glmvec3>(eeng::literals::deserialize_hs)
            .template func<&eeng::editor::inspect_glmvec3>(eeng::literals::inspect_hs)
            ;
        register_type<glm::vec3>();
        register_helper_type<glm::vec3>();

        entt::meta_factory<glm::vec4>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.vec4", .name = "Vec4", .tooltip = "Vec4." })
            .traits(MetaFlags::none)

            .data<&glm::vec4::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "X." }).traits(MetaFlags::none)

            .data<&glm::vec4::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "Y." }).traits(MetaFlags::none)

            .data<&glm::vec4::z>("z"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "z", "Z", "Z." }).traits(MetaFlags::none)

            .data<&glm::vec4::w>("w"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "w", "W", "W." }).traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_glmvec4>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_glmvec4>(eeng::literals::deserialize_hs)
            .template func<&eeng::editor::inspect_glmvec4>(eeng::literals::inspect_hs)
            ;
        register_type<glm::vec4>();
        register_helper_type<glm::vec4>();

        entt::meta_factory<glm::ivec2>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.ivec2", .name = "IVec2", .tooltip = "IVec2." })
            .traits(MetaFlags::none)

            .data<&glm::ivec2::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "X." }).traits(MetaFlags::none)

            .data<&glm::ivec2::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "Y." }).traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_glmivec2>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_glmivec2>(eeng::literals::deserialize_hs)
            .template func<&eeng::editor::inspect_glmivec2>(eeng::literals::inspect_hs)
            ;
        register_type<glm::ivec2>();
        register_helper_type<glm::ivec2>();

        entt::meta_factory<glm::ivec3>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.ivec3", .name = "IVec3", .tooltip = "IVec3." })
            .traits(MetaFlags::none)

            .data<&glm::ivec3::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "X." }).traits(MetaFlags::none)

            .data<&glm::ivec3::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "Y." }).traits(MetaFlags::none)

            .data<&glm::ivec3::z>("z"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "z", "Z", "Z." }).traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_glmivec3>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_glmivec3>(eeng::literals::deserialize_hs)
            .template func<&eeng::editor::inspect_glmivec3>(eeng::literals::inspect_hs)
            ;
        register_type<glm::ivec3>();
        register_helper_type<glm::ivec3>();

        entt::meta_factory<glm::ivec4>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.ivec4", .name = "IVec4", .tooltip = "IVec4." })
            .traits(MetaFlags::none)

            .data<&glm::ivec4::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "X." }).traits(MetaFlags::none)

            .data<&glm::ivec4::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "Y." }).traits(MetaFlags::none)

            .data<&glm::ivec4::z>("z"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "z", "Z", "Z." }).traits(MetaFlags::none)

            .data<&glm::ivec4::w>("w"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "w", "W", "W." }).traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_glmivec4>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_glmivec4>(eeng::literals::deserialize_hs)
            .template func<&eeng::editor::inspect_glmivec4>(eeng::literals::inspect_hs)
            ;
        register_type<glm::ivec4>();
        register_helper_type<glm::ivec4>();

        entt::meta_factory<glm::quat>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.quat", .name = "Quat", .tooltip = "Quat." })
            .traits(MetaFlags::none)

            .data<&glm::quat::x>("x"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "x", "X", "X." }).traits(MetaFlags::none)

            .data<&glm::quat::y>("y"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "y", "Y", "Y." }).traits(MetaFlags::none)

            .data<&glm::quat::z>("z"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "z", "Z", "Z." }).traits(MetaFlags::none)

            .data<&glm::quat::w>("w"_hs)
            .custom<DataMetaInfo>(DataMetaInfo{ "w", "W", "W." }).traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_glmquat>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_glmquat>(eeng::literals::deserialize_hs)
            ;
        register_type<glm::quat>();
        register_helper_type<glm::quat>();

        entt::meta_factory<glm::mat2>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.mat2", .name = "Mat2", .tooltip = "Mat2." })
            .traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_glmmat2>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_glmmat2>(eeng::literals::deserialize_hs)
            .template func<&eeng::editor::inspect_glmmat2>(eeng::literals::inspect_hs)
            ;
        register_type<glm::mat2>();
        register_helper_type<glm::mat2>();

        entt::meta_factory<glm::mat3>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.mat3", .name = "Mat3", .tooltip = "Mat3." })
            .traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_glmmat3>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_glmmat3>(eeng::literals::deserialize_hs)
            .template func<&eeng::editor::inspect_glmmat3>(eeng::literals::inspect_hs)
            ;
        register_type<glm::mat3>();
        register_helper_type<glm::mat3>();

        entt::meta_factory<glm::mat4>{}
            .custom<TypeMetaInfo>(TypeMetaInfo{ .id = "glm.mat4", .name = "Mat4", .tooltip = "Mat4." })
            .traits(MetaFlags::none)

            .func<&eeng::serializers::serialize_glmmat4>(eeng::literals::serialize_hs)
            .func<&eeng::serializers::deserialize_glmmat4>(eeng::literals::deserialize_hs)
            .template func<&eeng::editor::inspect_glmmat4>(eeng::literals::inspect_hs)
            ;
        register_type<glm::mat4>();
        register_helper_type<glm::mat4>();
    }
} // namespace eeng::meta
