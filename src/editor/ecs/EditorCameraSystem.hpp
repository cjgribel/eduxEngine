// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <memory>

#include "editor/ecs/FirstPersonCameraSystem.hpp"
#include "editor/ecs/ThirdPersonCameraSystem.hpp"

namespace eeng
{
    struct EngineContext;
}

namespace eeng::editor
{
    class EditorCameraSystem
    {
    public:
        // Destructor is out-of-line so unique_ptr sees complete camera system types.
        ~EditorCameraSystem();
        // Drive editor camera input/state each frame (edit mode only).
        void update(EngineContext& ctx, float delta_time);

    private:
        // Ensure exactly one editor camera is active (prefer third-person).
        void normalize_active_camera(entt::registry& registry);

        std::unique_ptr<ThirdPersonCameraSystem> third_person_system_;
        std::unique_ptr<FirstPersonCameraSystem> first_person_system_;
        bool f_was_down_ = false;
    };
} // namespace eeng::editor
