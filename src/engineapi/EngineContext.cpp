// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EngineContext.hpp"
#include "ThreadPool.hpp"
#include "EventQueue.h"

namespace eeng
{
    EngineConfig::EngineConfig(EventQueue& event_queue)
        : event_queue(event_queue)
    {
    }

    void EngineConfig::set_vsync(bool enabled)
    {
        if (vsync != enabled)
        {
            vsync = enabled;
            event_queue.dispatch(SetVsyncEvent{ enabled });
        }
    }

    void EngineConfig::set_wireframe(bool enabled)
    {
        if (wireframe_mode != enabled)
        {
            wireframe_mode = enabled;
            event_queue.dispatch(SetWireFrameRenderingEvent{ enabled });
        }
    }

    bool EngineConfig::is_vsync_enabled() const { return vsync; }
    bool EngineConfig::is_wireframe_enabled() const { return wireframe_mode; }

    EngineContext::EngineContext(
        std::unique_ptr<IEntityManager> entity_manager,
        std::unique_ptr<IResourceManager> resource_manager,
        std::unique_ptr<IGuiManager> gui_manager)
        : entity_manager(std::move(entity_manager))
        , resource_manager(std::move(resource_manager))
        , gui_manager(std::move(gui_manager))
        , thread_pool(std::make_unique<ThreadPool>())
        , event_queue(std::make_unique<EventQueue>())
        , engine_config(std::make_unique<EngineConfig>(*event_queue))
    {
    }

    EngineContext::~EngineContext() = default;

} // namespace eeng
