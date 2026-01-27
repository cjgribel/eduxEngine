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
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
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
    using BatchSelection = eeng::editor::SelectionManager<BatchId>;

    struct SetVsyncEvent { bool enabled; };
    struct SetWireFrameRenderingEvent { bool enabled; };
    struct SetDebugLoggingEvent { bool enabled; };
    struct SetMinFrameTimeEvent { float dt; };
    struct ResourceTaskCompletedEvent { TaskResult result; };
    struct SetPlayModeEvent { bool enabled; };
    struct TogglePlayModeEvent { };

    enum class BatchTaskType : uint8_t
    {
        Load,
        LoadAll,
        Unload,
        UnloadAll,
        Save,
        SaveAll,
        RebuildClosure,
        CreateEntity,
        DestroyEntity,
        AttachEntity,
        DetachEntity,
        SpawnEntity,
        Unknown
    };

    struct BatchTaskCompletedEvent
    {
        BatchTaskType type{ BatchTaskType::Unknown };
        BatchId batch_id{};
        std::string batch_name{};
        bool success = false;
        size_t batch_count = 0;

        size_t live_entities = 0;
        size_t asset_closure_size = 0;

        bool has_closure_delta = false;
        size_t closure_roots = 0;
        size_t closure_old = 0;
        size_t closure_new = 0;
        size_t closure_added = 0;
        size_t closure_removed = 0;
        bool assets_rebound = false;
    };

    enum class EngineFlag : uint8_t
    {
        VSync,
        WireframeRendering,
        DebugLogging,
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

    template<typename T>
    class PtrView
    {
    public:
        PtrView() = default;
        explicit PtrView(T* ptr) : ptr_(ptr) {}

        void reset(T* ptr = nullptr) { ptr_ = ptr; }
        T* get() const { return ptr_; }
        T& operator*() const { return *ptr_; }
        T* operator->() const { return ptr_; }
        explicit operator bool() const { return ptr_ != nullptr; }

    private:
        T* ptr_ = nullptr;
    };

    struct EngineServices
    {
        EngineServices(
            std::shared_ptr<IResourceManager> resource_manager,
            std::unique_ptr<IGuiManager> gui_manager,
            std::unique_ptr<IInputManager> input_manager,
            std::shared_ptr<ILogManager> log_manager);

        ~EngineServices();

        std::shared_ptr<IResourceManager>       resource_manager;
        std::unique_ptr<IGuiManager>            gui_manager;
        std::unique_ptr<IInputManager>          input_manager;
        std::shared_ptr<ILogManager>            log_manager;
        std::shared_ptr<std::atomic<bool>>      shutdown_requested;
        std::atomic<bool>                       play_mode_active{ false };
        std::unique_ptr<MainThreadQueue>        main_thread_queue;
        std::unique_ptr<ThreadPool>             thread_pool;
        std::unique_ptr<EventQueue>             event_queue;
        std::unique_ptr<editor::CommandQueue>   command_queue;
        std::unique_ptr<GuidSelection>          asset_selection;
        std::unique_ptr<EngineConfig>           engine_config;
    };

    struct WorldState
    {
        WorldState(
            std::unique_ptr<IEntityManager> entity_manager,
            std::unique_ptr<IBatchRegistry> batch_registry);

        ~WorldState();

        std::unique_ptr<IEntityManager>         entity_manager;
        std::unique_ptr<IBatchRegistry>         batch_registry;
        std::unique_ptr<EntitySelection>        entity_selection;
        std::unique_ptr<BatchSelection>         batch_selection;
    };

    struct EngineContext : public std::enable_shared_from_this<EngineContext>
    {
        // Compatibility constructor for existing call sites (builds services + world).
        EngineContext(
            std::unique_ptr<IEntityManager>     entity_manager,
            std::shared_ptr<IResourceManager>   resource_manager,
            std::unique_ptr<IBatchRegistry>     batch_registry,
            std::unique_ptr<IGuiManager>        gui_manager,
            std::unique_ptr<IInputManager>      input_manager,
            std::shared_ptr<ILogManager>        log_manager);

        EngineContext(
            std::shared_ptr<EngineServices> services,
            std::shared_ptr<WorldState> world);

        ~EngineContext();

        void bind(EngineServices& services, WorldState& world);

        std::shared_ptr<EngineServices> services_owner;
        std::shared_ptr<WorldState> world_owner;
        EngineServices* services = nullptr;
        WorldState* world = nullptr;

        // Compatibility view (non-owning, mirrors current world/services).
        PtrView<IEntityManager>         entity_manager;
        std::shared_ptr<IResourceManager> resource_manager;
        PtrView<IBatchRegistry>         batch_registry;
        PtrView<IGuiManager>            gui_manager;
        PtrView<IInputManager>          input_manager;
        std::shared_ptr<ILogManager>    log_manager;
        std::shared_ptr<std::atomic<bool>> shutdown_requested;
        PtrView<MainThreadQueue>        main_thread_queue;
        PtrView<ThreadPool>             thread_pool;
        PtrView<EventQueue>             event_queue;
        PtrView<editor::CommandQueue>   command_queue;
        PtrView<GuidSelection>          asset_selection;
        PtrView<EntitySelection>        entity_selection;
        PtrView<BatchSelection>         batch_selection;
        PtrView<EngineConfig>           engine_config;
    };

    using EngineContextPtr = std::shared_ptr<EngineContext>;
    using EngineContextWeakPtr = std::weak_ptr<EngineContext>;

} // namespace eeng
