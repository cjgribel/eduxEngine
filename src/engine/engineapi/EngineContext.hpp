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
#include "engineapi/PlayModePolicy.hpp"
#include "engineapi/OverlayViewState.hpp"
#include <functional>
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
    struct ProjectConfig;
}

namespace ShapeRendering
{
    class ShapeRenderer;
}

namespace eeng::ecs::systems
{
    struct DebugRenderSettings;
}

namespace eeng::ecs
{
    struct OverlayRenderSettings;
}

namespace eeng
{
    struct EngineContext;
    struct OverlayViewState;
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
    struct SetWindowSizeEvent
    {
        int width = 0;
        int height = 0;
        bool center = true;
        bool restore_if_maximized = true;
    };
    struct ToggleWindowMaximizeEvent { };

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

    // Lightweight stats snapshot shared with editor UI.
    struct PhysicsMonitorStats
    {
        std::size_t body_count = 0;
        int collision_objects = 0;
        int manifolds = 0;
        int contact_points = 0;
        std::size_t dirty_entities = 0;
        std::size_t event_entities = 0;
        std::size_t tracked_contacts = 0;
        bool valid = false;
    };

    struct ParticleMonitorStats
    {
        std::size_t emitter_count = 0;
        std::size_t visible_emitter_count = 0;
        std::size_t live_particles = 0;
        std::size_t rendered_particles = 0;
        std::size_t draw_batches = 0;
        bool collisions_requested = false;
        bool threaded_simulation_enabled = false;
        bool threaded_simulation_used = false;
        bool valid = false;
    };

    using EditorRenderHook =
        std::function<void(EngineContext&, ::ShapeRendering::ShapeRenderer&, const OverlayViewState&)>;

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
        std::atomic<bool>                       play_mode_policy_override_enabled{ false };
        std::atomic<PlayModePolicy>             play_mode_policy_override{ PlayModePolicy::Preview };
        // Shared stats snapshot for editor UI (updated by runtime systems).
        std::shared_ptr<PhysicsMonitorStats>    physics_monitor_stats;
        std::shared_ptr<ParticleMonitorStats>   particle_monitor_stats;
        // Current view state for overlay/debug rendering.
        std::shared_ptr<OverlayViewState>       overlay_view_state;
        // Core debug renderer shared across engine/editor/game.
        std::shared_ptr<::ShapeRendering::ShapeRenderer> shape_renderer;
        // Optional hook to share debug render settings between game runtime and editor UI.
        ecs::systems::DebugRenderSettings* debug_render_settings = nullptr;
        ecs::systems::DebugRenderSettings* debug_render_settings_edit = nullptr;
        ecs::systems::DebugRenderSettings* debug_render_settings_play = nullptr;
        ecs::OverlayRenderSettings* overlay_render_settings = nullptr;
        // Optional editor overlay hook (e.g. gizmo rendering) for shared renderers.
        EditorRenderHook                         editor_render_hook;
        std::unique_ptr<MainThreadQueue>        main_thread_queue;
        std::unique_ptr<ThreadPool>             thread_pool;
        std::unique_ptr<EventQueue>             event_queue;
        std::unique_ptr<editor::CommandQueue>   command_queue;
        std::unique_ptr<GuidSelection>          asset_selection;
        std::unique_ptr<EngineConfig>           engine_config;
        // Active project configuration for resolving asset/batch roots.
        std::shared_ptr<editor::ProjectConfig>  project_config;
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
        // Compatibility view of shared editor stats/config.
        std::shared_ptr<PhysicsMonitorStats> physics_monitor_stats;
        std::shared_ptr<ParticleMonitorStats> particle_monitor_stats;
        std::shared_ptr<editor::ProjectConfig> project_config;
        std::shared_ptr<::ShapeRendering::ShapeRenderer> shape_renderer;
        std::shared_ptr<OverlayViewState> overlay_view_state;
    };

    using EngineContextPtr = std::shared_ptr<EngineContext>;
    using EngineContextWeakPtr = std::weak_ptr<EngineContext>;

} // namespace eeng
