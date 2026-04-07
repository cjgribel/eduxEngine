// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "EngineContext.hpp"
#include "InspectorState.hpp"

#include "entt/entt.hpp"

namespace eeng::editor
{
    bool inspect_glmvec2(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx);
    bool inspect_glmvec3(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx);
    bool inspect_glmvec4(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx);
    bool inspect_glmivec2(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx);
    bool inspect_glmivec3(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx);
    bool inspect_glmivec4(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx);
    bool inspect_glmquat(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx);
    bool inspect_glmmat2(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx);
    bool inspect_glmmat3(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx);
    bool inspect_glmmat4(entt::meta_any& any, InspectorState& inspector, EngineContext& ctx);
} // namespace eeng::editor
