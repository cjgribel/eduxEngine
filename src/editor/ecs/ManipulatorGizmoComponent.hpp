// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "editor/ManipulatorGizmo.hpp"

namespace eeng::editor
{
    struct ManipulatorGizmoComponent
    {
        bool enabled = true;
        ManipulatorGizmo::Mode mode = ManipulatorGizmo::Mode::Translate;
        ManipulatorGizmo::Space space = ManipulatorGizmo::Space::Local;
        ManipulatorGizmo::Settings settings{};

        // Runtime gizmo state (not serialized via meta registration).
        ManipulatorGizmo runtime{};

        void sync_to_runtime()
        {
            runtime.set_mode(mode);
            runtime.set_space(space);
            runtime.settings() = settings;
        }

        void sync_from_runtime()
        {
            mode = runtime.mode();
            space = runtime.space();
            settings = runtime.settings();
        }
    };
} // namespace eeng::editor
