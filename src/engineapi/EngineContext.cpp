// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EngineContext.hpp"
#include "ThreadPool.hpp"
#include "EventQueue.h"
#include "engineapi/SelectionManager.hpp"

namespace eeng
{
    EngineConfig::EngineConfig(EventQueue& event_queue)
        : event_queue(event_queue)
    {
    }

    void EngineConfig::set_flag(EngineFlag flag, bool enabled)
    {
        if (flags[flag] != enabled)
        {
            flags[flag] = enabled;

            switch (flag)
            {
            case EngineFlag::VSync:
                event_queue.dispatch(SetVsyncEvent{ enabled });
                break;
            case EngineFlag::WireframeRendering:
                event_queue.dispatch(SetWireFrameRenderingEvent{ enabled });
                break;
            }
        }
    }

    bool EngineConfig::get_flag(EngineFlag flag) const
    {
        auto it = flags.find(flag);
        return it != flags.end() ? it->second : false;
    }

    void EngineConfig::set_value(EngineValue key, float new_value)
    {
        float& current = values[key];
        if (current != new_value)
        {
            current = new_value;

            switch (key)
            {
            case EngineValue::MinFrameTime:
                event_queue.dispatch(SetMinFrameTimeEvent{ new_value });
                break;
            case EngineValue::MasterVolume:
                // ...
                break;
            }
        }
    }

    float EngineConfig::get_value(EngineValue key) const
    {
        auto it = values.find(key);
        return it != values.end() ? it->second : 0.0f;
    }

    EngineContext::EngineContext(
        std::unique_ptr<IEntityManager> entity_manager,
        std::unique_ptr<IResourceManager> resource_manager,
        std::unique_ptr<IGuiManager> gui_manager,
        std::unique_ptr<IInputManager> input_manager,
        std::shared_ptr<ILogManager> log_manager)
        : entity_manager(std::move(entity_manager))
        , resource_manager(std::move(resource_manager))
        , gui_manager(std::move(gui_manager))
        , input_manager(std::move(input_manager))
        , log_manager(log_manager)
        , thread_pool(std::make_unique<ThreadPool>())
        , event_queue(std::make_unique<EventQueue>())
        , asset_selection(std::make_unique<editor::SelectionManager<Guid>>())
        , entity_selection(std::make_unique<editor::SelectionManager<Entity>>())
        , engine_config(std::make_unique<EngineConfig>(*event_queue))
    {
    }

    EngineContext::~EngineContext() = default;

} // namespace eeng
