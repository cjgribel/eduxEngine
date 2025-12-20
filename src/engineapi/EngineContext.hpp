// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include "IEntityManager.hpp"
#include "IResourceManager.hpp"
#include "IBatchRegistry.hpp"
#include "IGuiManager.hpp"
#include "IInputManager.hpp"
#include "ILogManager.hpp"
#include "Guid.h"
#include <memory>

class MainThreadQueue;
class ThreadPool;
class EventQueue;
namespace eeng::editor {
    template<typename T> class SelectionManager;
    class CommandQueue;
}

namespace eeng
{
    /*
    Engine context facilities:
    - (TODO?) Main thread queue
    - (TODO?) GPU Resource manager
    - (TODO?) Scene manager
    - Entity manager
    - Resource manager
    - Thread pool
    - Event dispatcher
    - Logger
    - Selection manager for assets & entities
    - CommandQueue
    */

    using GuidSelection = eeng::editor::SelectionManager<Guid>;
    using EntitySelection = eeng::editor::SelectionManager<ecs::Entity>;

    struct SetVsyncEvent { bool enabled; };
    struct SetWireFrameRenderingEvent { bool enabled; };
    struct SetMinFrameTimeEvent { float dt; };
    struct ResourceTaskCompletedEvent { TaskResult result; };

    enum class EngineFlag : uint8_t
    {
        VSync,
        WireframeRendering,
        // ...
    };

    enum class EngineValue : uint8_t
    {
        MinFrameTime,
        MasterVolume,
        // ...
    };

    class EngineConfig
    {
    public:
        explicit EngineConfig(EventQueue& event_queue);

        // --- Flag handling ---
        void set_flag(EngineFlag flag, bool enabled);

        bool get_flag(EngineFlag flag) const;

        // --- Value handling ---
        void set_value(EngineValue key, float new_value);

        float get_value(EngineValue key) const;

    private:
        std::unordered_map<EngineFlag, bool> flags;
        std::unordered_map<EngineValue, float> values;
        EventQueue& event_queue;
    };

    struct EngineContext
    {
        EngineContext(
            std::unique_ptr<IEntityManager>     entity_manager,
            std::shared_ptr<IResourceManager>   resource_manager,
            std::unique_ptr<IBatchRegistry>     batch_registry,
            std::unique_ptr<IGuiManager>        gui_manager,
            std::unique_ptr<IInputManager>      input_manager,
            std::shared_ptr<ILogManager>        log_manager);

        ~EngineContext();

        std::unique_ptr<IEntityManager>         entity_manager;
        std::shared_ptr<IResourceManager>       resource_manager;
        std::unique_ptr<IBatchRegistry>         batch_registry;
        std::unique_ptr<IGuiManager>            gui_manager;
        std::unique_ptr<IInputManager>          input_manager;
        std::shared_ptr<ILogManager>            log_manager;
        std::unique_ptr<MainThreadQueue>        main_thread_queue;
        std::unique_ptr<ThreadPool>             thread_pool;
        std::unique_ptr<EventQueue>             event_queue;
        std::unique_ptr<editor::CommandQueue>   command_queue;
        std::unique_ptr<GuidSelection>          asset_selection;
        std::unique_ptr<EntitySelection>        entity_selection;
        std::unique_ptr<EngineConfig>           engine_config;
    };

    using EngineContextPtr = std::shared_ptr<EngineContext>;
    using EngineContextWeakPtr = std::weak_ptr<EngineContext>;

} // namespace eeng
