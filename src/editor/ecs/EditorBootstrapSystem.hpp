// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <atomic>

namespace eeng
{
    struct EngineContext;
    struct BatchTaskCompletedEvent;
}

namespace eeng::editor
{
    class EditorBootstrapSystem
    {
    public:
        // Register batch-load callbacks and schedule an initial ensure pass.
        void init(EngineContext& ctx);

    private:
        // React to batch-load events and trigger a gizmo ensure pass when needed.
        void on_batch_task_completed(EngineContext& ctx, const BatchTaskCompletedEvent& event);
        // Coalesce multiple requests and run the ensure work on a background thread.
        void request_ensure_transform_gizmo(EngineContext& ctx);
        // Ensure a TransformGizmoComponent exists and is attached to the editor batch.
        void ensure_editor_transform_gizmo_entity(EngineContext& ctx);

        // Coalescing state so repeated load events do not spawn duplicate work.
        std::atomic_bool ensure_transform_gizmo_in_flight_{ false };
        std::atomic_bool ensure_transform_gizmo_requested_{ false };
        bool initialized_ = false;
    };
} // namespace eeng::editor
