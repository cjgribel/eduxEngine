# Architecture Overview

Overview of the architecture of `eduxEngine`, with a recap of the baseline `eduEngine` architecture and a categorized summary of major concepts and systems.

_Last updated: 260407_

## Baseline Version: `eduEngine` (For educational use)

### Overall Architecture

The repository can be described as a compact, modular runtime architecture for interactive 3D applications.

- The code is organized around a small engine core and game-specific code built on top of it.
- The engine layer handles startup, the main loop, window and OpenGL context creation, input and event processing, frame management, and debug GUI support.
- The game layer plugs into the engine through a small abstract interface and implements its own initialization, update logic, rendering, and cleanup.
- Reusable runtime systems are provided for rendering, asset loading, skeletal animation, math utilities, logging, debug drawing, and simple collision and picking.
- A full ECS layer is not part of the compact baseline architecture; ECS-oriented work through EnTT is introduced later as an extension.

### Key Functional Requirements

- Run a real-time interactive application with continuous update and rendering.
- Load and render 3D assets, including shaders, textures, and animated meshes.
- Support input-driven logic, scene/object management, and simple collision or picking.

### Key Quality Attributes

- Modularity through focused runtime components.
- Extensibility for course labs and incremental engine features.
- Learnability through a direct and approachable structure.
- Portability through CMake and cross-platform libraries.

## Extended Version: `eduxEngine`

### Overall Architecture

`eduxEngine` extends that baseline with more explicit editor/runtime separation, more tooling infrastructure, more asset and world-management infrastructure, and clearer architectural boundaries for larger experiments.

The repo can still be described as a layered, modular architecture, but in this larger version the layers are more explicit and more specialized than in the course version.

- At a high level, there is a separation between engine core functionality, editor functionality, game/project-specific code, and asset/content pipelines.
- The engine layer provides reusable runtime services such as ECS/world management, assets, rendering support, threading, events, serialization, and physics integration.
- The editor layer builds on top of the engine rather than replacing it. It adds tools, GUI, inspection, undo/redo, prefab workflows, and edit/play switching.
- The game or project layer then uses the engine through a controlled interface instead of directly depending on every subsystem.
- Several systems are also data-driven: assets, prefabs, batches, animation graphs, and other authored data are loaded and interpreted by generic runtime code.
- The architecture is modular in the sense that many responsibilities are split into focused subsystems or services, but those modules are coordinated through shared context objects, events, commands, and registries.
- Compared with a simple strict layer stack, this is closer to a layered tool-and-runtime architecture: the editor depends on engine services, the runtime can run without most editor features, and play mode reuses the same foundation with a different world state.

A useful distinction is:

- Architectural topics define boundaries, ownership, lifetimes, communication paths, scheduling, or major data flow.
- Feature topics mostly add capabilities within those boundaries.
- Another way to say it: some things define the shape of the engine, while other things live inside that shape.

## Topic Overview by Architectural Level

This section groups the topics by architectural level: architecture-level topics, supporting infrastructure/tooling infrastructure, and feature-level systems.

### Architecture-Level

- [1.1 World State](#11-world-state)
- [1.2 Entity Management / ECS Layer](#12-entity-management--ecs-layer)
- [1.3 Engine Context Object](#13-engine-context-object)
- [1.4 Shared Services vs World State](#14-shared-services-vs-world-state)
- [1.5 `IApp`: Application Host Boundary](#15-iapp-application-host-boundary)
- [1.6 `IGameRuntime`: Game Runtime Boundary](#16-igameruntime-game-runtime-boundary)
- [1.7 GUI Actions Layer](#17-gui-actions-layer)
- [1.8 Custom Asset Storage](#18-custom-asset-storage)
- [1.9 Edit / Play Modes](#19-edit--play-modes)
- [1.10 Threaded Execution](#110-threaded-execution)
- [1.11 Async Loading](#111-async-loading)
- [1.12 Events](#112-events)
- [1.13 GUIDs and Reference Binding](#113-guids-and-reference-binding)
- [1.14 Batch / Scene Partitioning](#114-batch--scene-partitioning)
- [1.15 Main-Thread Boundaries](#115-main-thread-boundaries)
- [1.16 Runtime Pipeline](#116-runtime-pipeline)

### Supporting Infrastructure / Tooling Infrastructure

- [2.1 Runtime Reflection](#21-runtime-reflection)
- [2.2 Serialization](#22-serialization)
- [2.3 Undoable Command Queue](#23-undoable-command-queue)
- [2.4 Prefabs](#24-prefabs)
- [2.5 Asset Import and Cooking](#25-asset-import-and-cooking)
- [2.6 Scripting / Lua (planned)](#26-scripting--lua-planned)

### Feature-Level Systems

- [3.1 Animation and Animation Graphs](#31-animation-and-animation-graphs)
- [3.2 Physics Layer](#32-physics-layer)
- [3.3 Data-Driven Content Generation](#33-data-driven-content-generation)
- [3.4 Particle System](#34-particle-system)

## 1. Architecture-Level Topics

This section contains the topics that most directly define the shape of the engine: boundaries between subsystems, ownership of state, execution order, communication paths, and major lifetime/data-flow decisions.

### 1.1 World State

- What it is: The currently active runtime scene/state of the engine.
- What it is for: Collects the data that belongs to one specific world, so a world can be created, replaced, serialized, or discarded as one unit.
- Library / note: Custom repo architecture. In `eduxEngine`, this is represented by `WorldState`, which holds per-world state such as the entity manager and batch registry.
- Pattern: State container / context-bound runtime state.

### 1.2 Entity Management / ECS Layer

- What it is: The engine's entity/component layer, built around EnTT but wrapped behind engine-facing interfaces such as `IEntityManager` and the concrete `EntityManager`.
- What it is for: Provides the runtime object model for scene data, component storage, controlled entity creation/destruction, scene-graph registration, and GUID-based lookup and rebinding.
- Library / note: Uses `EnTT` as the ECS foundation, with repo-specific management around `EntityManager`, `SceneGraph`, and `EntityRef`. `EntityRef` combines stable GUID identity with an optional live entity handle, which is useful for serialization, prefab spawning, undo/redo, and play-world recreation.
- Pattern: ECS pattern, combined with stable-reference/identity binding.

### 1.3 Engine Context Object

- What it is: A shared access object that bundles the current world together with shared engine services into one interface (`EngineContext`, `EngineServices`, `WorldState`).
- What it is for: Reduces long parameter lists, gives game/editor code one common entry point, and makes world switching easier.
- Library / note: Custom repo design, not a third-party library. This kind of object is often custom; dependency-injection helpers such as `Boost.DI` can support similar service wiring. See also [1.1 World State](#11-world-state), [1.2 Entity Management / ECS Layer](#12-entity-management--ecs-layer), and [1.4 Shared Services vs World State](#14-shared-services-vs-world-state).
- Pattern: Context Object. Also resembles a Service Locator for engine subsystems.

### 1.4 Shared Services vs World State

- What it is: The split between `EngineServices` and `WorldState`, both viewed through `EngineContext`.
- What it is for: Separates long-lived shared services from per-world mutable state, which makes edit/play world switching much cleaner.
- Library / note: Custom repo design decision rather than a library feature. This is the main architectural split behind the distinction between persistent engine facilities and the currently active world.
- Pattern: Separation of concerns, with composition rather than inheritance.

### 1.5 `IApp`: Application Host Boundary

- What it is: The engine-facing application interface. The engine runs exactly one `IApp`, and `EditorApp` and `GameApp` are the main implementations.
- What it is for: Lets the engine stay generic while the app decides whether it is running as an editor host or as a game-only host.
- Library / note: Custom repo contract. This is typically engine-specific rather than something provided by a library.
- Pattern: Application Shell / Host pattern, with polymorphic strategy-style implementations.

### 1.6 `IGameRuntime`: Game Runtime Boundary

- What it is: The runtime/game-facing interface for project-specific gameplay and rendering behavior.
- What it is for: Separates reusable engine code from project-specific game logic. The same runtime concept can be hosted inside either `GameApp` or `EditorApp`.
- Library / note: Custom repo contract. This kind of plugin/runtime boundary is usually engine-defined.
- Pattern: Plugin or Strategy boundary. It also has a Template Method flavor through `render_frame()` calling scene, overlay, and GUI phases in a fixed order.

### 1.7 GUI Actions Layer

- What it is: A thin layer between GUI widgets and engine/editor operations (`SceneActions`, `BatchActions`, `AssetActions`).
- What it is for: Keeps UI code simple, centralizes validation/rules, and routes edits through commands instead of letting widgets mutate data directly.
- Library / note: Built around Dear ImGui, but the actions layer itself is custom. Other established UI options include `Qt` and `Nuklear`.
- Pattern: Facade or Application Service layer in front of lower-level editor/engine logic.

### 1.8 Custom Asset Storage

- What it is: A custom runtime asset management system with GUID lookup, typed handles, versioned handles, indexing, and type-erased storage (`Storage`, `ResourceManager`, `AssetIndex`).
- What it is for: Manages engine-native assets after they exist in the project: tracking identity, lookup, lifetime, loading, binding, unloading, and stable references from gameplay/editor data.
- Library / note: Custom repo system, with some runtime-typed access supported through EnTT meta. Related reusable building blocks include the `EnTT` resource cache. Source-file conversion into engine assets is a separate concern; see [2.5 Asset Import and Cooking](#25-asset-import-and-cooking).
- Pattern: Repository/Manager pattern, plus Handle pattern for safe references to loaded assets.

### 1.9 Edit / Play Modes

- What it is: Two separate worlds: one for editing and one temporary runtime world for play/testing.
- What it is for: Lets the game run without destroying the editor state. Exiting play throws away the play world and restores edit mode unchanged.
- Library / note: Custom repo architecture. The play world is created from serialized snapshots of the loaded edit batches. This is usually an engine-specific system rather than a standalone library. This topic builds directly on [1.1 World State](#11-world-state), [1.2 Entity Management / ECS Layer](#12-entity-management--ecs-layer), and [1.4 Shared Services vs World State](#14-shared-services-vs-world-state).
- Pattern: State pattern at the application level, with snapshot/restore behavior.

### 1.10 Threaded Execution

- What it is: Multiple execution strategies: a thread pool for parallel work, a serial executor for ordered work, and a main-thread queue for thread-affine tasks.
- What it is for: Improves responsiveness and scalability while still respecting constraints such as “some things must happen in order” or “some things must happen on the main thread”.
- Library / note: Custom repo utilities built on top of standard C++ threading/futures. Other established options include `Taskflow` and `oneTBB`.
- Pattern: Executor pattern, plus Producer-Consumer queues.

### 1.11 Async Loading

- What it is: Asset and batch loading started in the background and completed through futures, events, and main-thread handoff when needed.
- What it is for: Keeps the editor responsive, avoids long stalls, and prepares the architecture for loading screens or streaming later.
- Library / note: Uses the repo `ThreadPool`, `SerialExecutor`, `MainThreadQueue`, and `std::future`/`std::shared_future`. Similar infrastructure can also be built with `Taskflow` or `oneTBB`.
- Pattern: Asynchronous Task / Future pattern, with event-driven completion notifications.

### 1.12 Events

- What it is: A messaging mechanism where systems publish and react to events without being tightly coupled.
- What it is for: Lets different engine parts communicate without direct dependencies everywhere.
- Library / note: Covered in Module 4. This repo uses an `EventQueue` for engine/editor/runtime coordination. Other established options include `Boost.Signals2` and Qt signals/slots.
- Pattern: Observer pattern, often implemented with queued event dispatch.

### 1.13 GUIDs and Reference Binding

- What it is: Using globally unique IDs to identify entities, assets, batches, and references between them.
- What it is for: Makes save/load, prefab spawning, undo/redo, and asset references more robust than raw pointers or local entity IDs.
- Library / note: Custom repo infrastructure around `Guid`, `EntityRef`, and binding helpers. This is usually custom, but many serialization/networking stacks also build around UUID libraries such as `Boost.UUID`.
- Pattern: Identity Map / stable identity pattern.

### 1.14 Batch / Scene Partitioning

- What it is: Grouping entities and related assets into named batches that can be loaded, saved, and unloaded as units.
- What it is for: Supports ownership, streaming, editor organization, and clearer boundaries for what content belongs together.
- Library / note: Custom repo system through `BatchRegistry` and related editor commands. This is usually engine-specific rather than a standalone library feature.
- Pattern: Partitioning / Aggregation pattern.

### 1.15 Main-Thread Boundaries

- What it is: Explicit rules for which work may run in worker threads and which work must return to the main thread.
- What it is for: Important for rendering APIs, some ECS mutations, GUI work, and other systems that are not safely parallel.
- Library / note: Managed in the repo with `MainThreadQueue` and thread-pool handoff. This is commonly custom, though frameworks such as Qt also provide main-thread event-loop dispatch patterns.
- Pattern: Thread confinement pattern.

### 1.16 Runtime Pipeline

- What it is: An engine-owned definition of update/render order: which systems run, in what phases, and in what sequence.
- What it is for: Makes dependencies between systems explicit, keeps frame execution deterministic, and creates one place where the engine can decide what “a frame” means in edit mode, play mode, or future runtime variants.
- Library / note: In this repo, `RuntimePipeline` is a custom orchestration layer around engine systems. A notable ECS example with explicit phases/pipelines is `flecs`, which supports custom pipelines and phases.
- Pattern: Pipeline pattern, or more specifically a scheduling/orchestration pattern for frame execution.

## 2. Supporting Infrastructure / Tooling Infrastructure

This section contains things that are not usually the architecture by themselves, but strongly support the architecture. They often enable generic tooling, persistence, reuse, editor workflows, or extensibility.

### 2.1 Runtime Reflection

- What it is: Type information available at runtime instead of only at compile time.
- What it is for: Generic inspector UI, add/remove components by type, field editing, cloning, binding helpers, and reusable serialization support.
- Library / note: `entt::meta`. C++ still has no standard runtime reflection system. Another established option is `RTTR`.
- Pattern: Reflection / Type Metadata pattern.

### 2.2 Serialization

- What it is: Converting engine data to and from a stored representation, mainly JSON.
- What it is for: Save/load to disk, prefab files, undo/redo snapshots, and world/batch snapshots used when entering play mode.
- Library / note: `nlohmann::json`, together with repo-specific serializers and reflection helpers. Other established options include `cereal`, `Protocol Buffers`, `FlatBuffers`, and `yaml-cpp`.
- Pattern: Serializer / Data Mapper style approach.

### 2.3 Undoable Command Queue

- What it is: Editor changes wrapped as command objects with `execute()`, `undo()`, and optional async `update()`.
- What it is for: Consistent undo/redo, safer editor tooling, and one place to sequence long-running edits such as batch or asset operations.
- Library / note: Implemented in the repo. If the editor is built with Qt, `QUndoStack` / Qt's Undo Framework is a well-established alternative.
- Pattern: Command pattern.

### 2.4 Prefabs

- What it is: Reusable serialized entity branches that can be saved to JSON and spawned again later.
- What it is for: Repeated content, tool-generated setups, and faster level building. Spawning can remap GUIDs so each instance becomes unique.
- Library / note: Built on the existing JSON/entity serialization path, not a separate prefab library. In ECS frameworks, `flecs` is a notable example with built-in prefab support.
- Pattern: Prototype-like reuse pattern, where instances are created from saved template data.

### 2.5 Asset Import and Cooking

- What it is: A pipeline that converts external source assets such as models, textures, or terrain inputs into engine-native asset files and derived runtime data.
- What it is for: Separates authoring formats from runtime formats, supports repeatable content workflows, and allows imported data to be validated, transformed, organized, and re-generated before normal runtime loading.
- Library / note: In this repo, model import uses `Assimp`, texture decoding/loading uses `stb_image`, embedded texture export uses `stb_image_write`, and terrain uses a custom `TerrainCooker`. Similar pipelines may also use `meshoptimizer` or custom offline build tools.
- Pattern: Pipeline pattern / import-build pipeline.

### 2.6 Scripting / Lua (planned)

- What it is: A future gameplay scripting layer where entities with `ScriptComponent` can run Lua-based behavior.
- What it is for: Faster gameplay iteration, data-driven behaviors, and less need to recompile C++ for every gameplay change.
- Library / note: Planned with `sol2` + Lua. The repo currently only has placeholders (`ScriptComponent`, `ScriptSystem`). Other established embedded scripting options include `AngelScript` and `Wren`.
- Pattern: Component + embedded scripting runtime pattern.

## 3. Feature-Level Systems

This section contains feature-level systems that extend the engine within an existing architecture. They are important engine functionality, but they usually live inside architectural boundaries rather than defining those boundaries.

### 3.1 Animation and Animation Graphs

- What it is: Systems for moving models over time, often by blending clips through a graph of states and transitions.
- What it is for: Turns static models into characters or mechanisms with reusable, controllable motion.
- Library / note: Covered in Module 3. This repo includes animation graph assets/runtime work on top of that. A well-established animation runtime/toolchain option is `ozz-animation`.
- Pattern: State Machine pattern is often used inside animation graphs.

### 3.2 Physics Layer

- What it is: A subsystem for collision detection, rigid bodies, constraints, and physical simulation.
- What it is for: Gives gameplay objects believable motion and interaction beyond simple manual transform updates.
- Library / note: Example library in this repo: Bullet Physics. Other established choices include `Box2D`, `PhysX`, and `Jolt Physics`.
- Pattern: Subsystem pattern.

### 3.3 Data-Driven Content Generation

- What it is: Building runtime objects from higher-level data descriptions instead of hand-coding every instance.
- What it is for: Makes tools more powerful, supports reuse, and helps scale content creation through prefabs, rig builders, terrain recipes, and similar systems.
- Library / note: Custom repo approach using JSON/data definitions and builder utilities. Depending on the pipeline, formats such as `Protocol Buffers`, `FlatBuffers`, or `yaml-cpp` may also be used for the driving data.
- Pattern: Builder pattern, combined with data-driven design.

### 3.4 Particle System

- What it is: A subsystem for spawning, simulating, and rendering many lightweight visual elements such as smoke, sparks, fire, dust, or magic effects.
- What it is for: Adds rich visual feedback that would be too expensive or awkward to model as normal scene objects. In this repo, the system is CPU-simulated and integrated with ECS, rendering, optional textures/atlases, and some collision/event handling.
- Library / note: Custom repo system (`ParticleEmitterComponent`, `ParticleSystem`, `ParticleRenderSystem`). Established alternatives include `Effekseer` and `PopcornFX`.
- Pattern: Data-oriented simulation system, often combined with object pooling / flyweight-style particles.

## Appendix A: Suggested Gregory Cross-References

This appendix links the topics in this document to broad areas covered in Jason Gregory, *Game Engine Architecture* (3rd ed.). The mapping is approximate: the concepts often align well, but the concrete implementation in `eduxEngine` is specific to this repo.

- Overall runtime architecture, host/runtime boundaries, and frame structure: Chapter 1.6 "Runtime Engine Architecture" and Chapter 8 "The Game Loop and Real-Time Simulation"
- Editor and tooling topics such as GUI actions, undoable commands, in-engine tools, and development support: Chapter 1.7 "Tools and the Asset Pipeline", Chapter 10 "Tools for Debugging and Development", and Chapter 15.4 "The Game World Editor"
- Asset storage, import/cooking, resource management, and data-driven content: Chapter 1.7 "Tools and the Asset Pipeline", Chapter 7 "Resources and the File System", Chapter 15.3 "Data-Driven Game Engines", and Chapter 16.3 "World Chunk Data Formats"
- ECS/world structure, entity management, object references, batching/partitioning, edit/play world handling, and gameplay foundation architecture: Chapter 15 "Introduction to Gameplay Systems" and Chapter 16, especially 16.2 "Runtime Object Model Architectures", 16.4 "Loading and Streaming Game Worlds", 16.5 "Object References and World Queries", 16.6 "Updating Game Objects in Real Time", and 16.10 "High-Level Game Flow"
- Events and messaging: Chapter 16.8 "Events and Message-Passing"
- Threading, async loading, and concurrency: Chapter 4 "Parallelism and Concurrent Programming", Chapter 8.6 "Multiprocessor Game Loops", and Chapter 16.7 "Applying Concurrency to Game Object Updates"
- Scripting: Chapter 16.9 "Scripting"
- Animation and physics: Chapter 12 "Animation Systems" and Chapter 13 "Collision and Rigid Body Dynamics"
