// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <memory>
#include "IEntityManager.hpp"
#include "IResourceManager.hpp"
#include "IGuiManager.hpp"
#include "IInputManager.hpp"
#include "ILogManager.hpp"
#include "Guid.h"
class ThreadPool;
class EventQueue;
namespace eeng:: editor {
    template<typename T> class SelectionManager;
}

namespace eeng
{
    /*
    Engine context facilities:
    - Entity manager
    - Resource manager
    - Thread pool
    - Event dispatcher
    - Logger
    - Selection manager for assets & entities
    - (TODO) CommandQueue
    - (TODO?) Scene manager
    */

    struct SetVsyncEvent { bool enabled; };
    struct SetWireFrameRenderingEvent { bool enabled; };
    struct SetMinFrameTimeEvent { float dt; };

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
            std::unique_ptr<IEntityManager> entity_manager,
            std::unique_ptr<IResourceManager> resource_manager,
            std::unique_ptr<IGuiManager> gui_manager,
            std::unique_ptr<IInputManager> input_manager,
            std::shared_ptr<ILogManager> log_manager);

        ~EngineContext();

        std::unique_ptr<IEntityManager>     entity_manager;
        std::unique_ptr<IResourceManager>   resource_manager; // NOTE: is cast to ResourceManager by client
        std::unique_ptr<IGuiManager>        gui_manager;
        std::unique_ptr<IInputManager>      input_manager;
        std::shared_ptr<ILogManager>        log_manager;
        std::unique_ptr<ThreadPool>         thread_pool;
        std::unique_ptr<EventQueue>         event_queue;
        std::unique_ptr<editor::SelectionManager<Guid>>     asset_selection;
        std::unique_ptr<editor::SelectionManager<Entity>>   entity_selection;
        std::unique_ptr<EngineConfig>       engine_config;
    };

    using EngineContextPtr = std::shared_ptr<EngineContext>;
    using EngineContextWeakPtr = std::weak_ptr<EngineContext>;

} // namespace eeng
