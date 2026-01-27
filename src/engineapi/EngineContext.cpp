// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "EngineContext.hpp"

#include "MainThreadQueue.hpp"
#include "ThreadPool.hpp"
#include "EventQueue.h"
#include "editor/CommandQueue.hpp"
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
            case EngineFlag::DebugLogging:
                event_queue.dispatch(SetDebugLoggingEvent{ enabled });
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

    EngineServices::EngineServices(
        std::shared_ptr<IResourceManager> resource_manager,
        std::unique_ptr<IGuiManager> gui_manager,
        std::unique_ptr<IInputManager> input_manager,
        std::shared_ptr<ILogManager> log_manager)
        : resource_manager(std::move(resource_manager))
        , gui_manager(std::move(gui_manager))
        , input_manager(std::move(input_manager))
        , log_manager(std::move(log_manager))
        , shutdown_requested(std::make_shared<std::atomic<bool>>(false))
        , main_thread_queue(std::make_unique<MainThreadQueue>(shutdown_requested))
        , thread_pool(std::make_unique<ThreadPool>(std::thread::hardware_concurrency(), shutdown_requested)) // 2+, reload async deadlocks for < 2 threads
        , event_queue(std::make_unique<EventQueue>(shutdown_requested))
        , command_queue(std::make_unique<editor::CommandQueue>())
        , asset_selection(std::make_unique<editor::SelectionManager<Guid>>())
        , engine_config(std::make_unique<EngineConfig>(*event_queue))
    {
    }

    EngineServices::~EngineServices() = default;

    WorldState::WorldState(
        std::unique_ptr<IEntityManager> entity_manager,
        std::unique_ptr<IBatchRegistry> batch_registry)
        : entity_manager(std::move(entity_manager))
        , batch_registry(std::move(batch_registry))
        , entity_selection(std::make_unique<editor::SelectionManager<ecs::Entity>>())
        , batch_selection(std::make_unique<editor::SelectionManager<BatchId>>())
    {
    }

    WorldState::~WorldState() = default;

    EngineContext::EngineContext(
        std::shared_ptr<EngineServices> services,
        std::shared_ptr<WorldState> world)
        : services_owner(std::move(services))
        , world_owner(std::move(world))
    {
        if (services_owner && world_owner)
            bind(*services_owner, *world_owner);
    }

    EngineContext::EngineContext(
        std::unique_ptr<IEntityManager> entity_manager,
        std::shared_ptr<IResourceManager> resource_manager,
        std::unique_ptr<IBatchRegistry> batch_registry,
        std::unique_ptr<IGuiManager> gui_manager,
        std::unique_ptr<IInputManager> input_manager,
        std::shared_ptr<ILogManager> log_manager)
        : EngineContext(
            std::make_shared<EngineServices>(
                std::move(resource_manager),
                std::move(gui_manager),
                std::move(input_manager),
                std::move(log_manager)),
            std::make_shared<WorldState>(
                std::move(entity_manager),
                std::move(batch_registry)))
    {
    }

    EngineContext::~EngineContext() = default;

    void EngineContext::bind(EngineServices& services, WorldState& world)
    {
        this->services = &services;
        this->world = &world;

        entity_manager.reset(world.entity_manager.get());
        batch_registry.reset(world.batch_registry.get());
        entity_selection.reset(world.entity_selection.get());
        batch_selection.reset(world.batch_selection.get());

        resource_manager = services.resource_manager;
        gui_manager.reset(services.gui_manager.get());
        input_manager.reset(services.input_manager.get());
        log_manager = services.log_manager;
        shutdown_requested = services.shutdown_requested;
        main_thread_queue.reset(services.main_thread_queue.get());
        thread_pool.reset(services.thread_pool.get());
        event_queue.reset(services.event_queue.get());
        command_queue.reset(services.command_queue.get());
        asset_selection.reset(services.asset_selection.get());
        engine_config.reset(services.engine_config.get());
    }

} // namespace eeng
