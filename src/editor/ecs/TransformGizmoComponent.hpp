// Created by Carl Johan Gribel 2026.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "editor/TransformGizmo.hpp"

namespace eeng::editor
{
    struct TransformGizmoComponent
    {
        bool enabled = true;
        TransformGizmo::Mode mode = TransformGizmo::Mode::Translate;
        TransformGizmo::Space space = TransformGizmo::Space::Local;
        TransformGizmo::Settings settings{};

        // Runtime gizmo state (not serialized via meta registration).
        TransformGizmo runtime_gizmo{};

        void sync_to_runtime()
        {
            runtime_gizmo.set_mode(mode);
            runtime_gizmo.set_space(space);
            runtime_gizmo.settings() = settings;
        }

        void sync_from_runtime()
        {
            mode = runtime_gizmo.mode();
            space = runtime_gizmo.space();
            settings = runtime_gizmo.settings();
        }
    };
} // namespace eeng::editor
