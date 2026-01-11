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
        void init(EngineContext& ctx);

    private:
        void on_batch_task_completed(EngineContext& ctx, const BatchTaskCompletedEvent& event);
        void request_ensure_gizmo(EngineContext& ctx);
        void ensure_editor_gizmo_entity(EngineContext& ctx);

        std::atomic_bool ensure_in_flight_{ false };
        std::atomic_bool ensure_requested_{ false };
        bool initialized_ = false;
    };
} // namespace eeng::editor
