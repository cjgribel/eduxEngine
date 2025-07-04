// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#pragma once

#include <memory>
#include "IEntityManager.hpp"
#include "IResourceManager.hpp"
#include "IGuiManager.hpp"
class ThreadPool;
class EventQueue;

namespace eeng
{
    /*
    Engine context facilities:
    - Entity manager
    - Resource manager
    - Thread pool
    - Event dispatcher
    - (TODO) CommandQueue
    - (TODO?) Scene manager
    - (TODO?) Logger
    */

    struct SetVsyncEvent { bool enabled; };
    struct SetWireFrameRenderingEvent { bool enabled; };

    class EngineConfig
    {
    public:
        explicit EngineConfig(EventQueue& event_queue);

        void set_vsync(bool enabled);
        void set_wireframe(bool enabled);

        bool is_vsync_enabled() const;
        bool is_wireframe_enabled() const;

    private:
        bool vsync = true;
        bool wireframe_mode = false;
        EventQueue& event_queue;
    };

    struct EngineContext
    {
        EngineContext(
            std::unique_ptr<IEntityManager> entity_manager,
            std::unique_ptr<IResourceManager> resource_manager,
            std::unique_ptr<IGuiManager> gui_manager);

        ~EngineContext();

        std::unique_ptr<IEntityManager>     entity_manager;
        std::unique_ptr<IResourceManager>   resource_manager; // NOTE: is cast to ResourceManager by client
        std::unique_ptr<IGuiManager>        gui_manager;
        std::unique_ptr<ThreadPool>         thread_pool;
        std::unique_ptr<EventQueue>         event_queue;
        std::unique_ptr<EngineConfig>       engine_config;
    };

    using EngineContextPtr = std::shared_ptr<EngineContext>;
    using EngineContextWeakPtr = std::weak_ptr<EngineContext>;

} // namespace eeng
