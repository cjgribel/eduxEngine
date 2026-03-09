# Lua Integration Plan

This note outlines a practical path for integrating Lua using `sol2` into the current engine.

It is based on the current state of the repo:

- `ScriptComponent` exists as a minimal placeholder in [ScriptComponent.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/ScriptComponent.hpp).
- `ScriptSystem` exists, but currently only tracks component lifetime and does not execute scripts in [ScriptSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/ScriptSystem.cpp).
- The editor currently shows script buttons as disabled in [EntityInspectorWidget.hpp](/Users/ag1498/GitHub/eduxEngine/src/editor/gui/EntityInspectorWidget.hpp).
- There is older commented-out `sol` experimentation in [CoreComponents.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/CoreComponents.cpp), but it should be treated as reference material rather than the basis of the new runtime.

## Goals

- Add a real gameplay scripting path without destabilizing the ECS/runtime architecture.
- Keep the first version narrow, debuggable, and editor-friendly.
- Use `sol2` as a binding layer, but avoid exposing the entire engine surface at once.
- Make script failures recoverable and visible in logs/editor UI.

## Suggested Architecture

### 1. Add a dedicated script runtime service

Do not let Lua state management live ad hoc inside `ScriptSystem`.

Add a `ScriptRuntime` service owned from engine context/services that is responsible for:

- owning one or more `sol::state` instances
- opening approved standard libraries
- loading script files
- caching compiled chunks or script environments
- logging script errors
- handling hot reload hooks later

`ScriptSystem` should focus on ECS lifecycle and per-frame dispatch, while `ScriptRuntime` owns the Lua VM policy.

### 2. Keep one shared Lua state first

For the first implementation, prefer one shared `sol::state` for the engine rather than one state per entity or per script.

Why:

- simpler lifetime model
- easier shared bindings
- easier editor inspection/logging
- fewer memory and startup costs

Use isolated per-script environments or per-instance tables inside that state so script instances do not trample each other.

### 3. Evolve `ScriptComponent`

The current component is a good placeholder, but it will need a little more structure.

Suggested next shape:

- `script_id` or `script_path`
- `enabled`
- optional `run_in_edit_mode`
- optional serialized parameter blob
- optional runtime instance handle or script instance id

Do not store raw `sol::table` or `sol::function` directly in the serialized component. Keep Lua runtime state outside serialized ECS data.

## Suggested Script Model

Start with one script file creating one behavior table. For example:

```lua
return {
    on_init = function(self) end,
    on_update = function(self, dt) end,
    on_destroy = function(self) end
}
```

Each entity with a `ScriptComponent` gets its own instance table such as:

- script functions copied or referenced from the module
- `entity`
- `world`
- `transform`
- user fields / parameters

Recommended initial callbacks:

- `on_init(self)`
- `on_play_begin(self)`
- `on_update(self, dt)`
- `on_play_end(self)`
- `on_destroy(self)`

Avoid a large callback matrix initially. It is better to ship a tiny, reliable lifecycle first.

## ECS Binding Strategy

### 1. Do not expose `entt::registry` directly at first

That would make scripts powerful quickly, but it also creates a fragile API and makes safety/debugging harder.

Instead, expose narrow wrappers such as:

- `EntityHandle`
- `TransformAPI`
- `InputAPI`
- `LogAPI`
- `SpawnAPI`
- `TimeAPI`

Example script-facing operations:

- `entity:is_valid()`
- `entity:get_transform()`
- `entity:set_position(x, y, z)`
- `input:key_down("Space")`
- `log:info("hello")`

This keeps the scripting ABI stable even if the ECS internals change.

### 2. Bind only the components scripts really need

Good first bindings:

- transform
- tag/header/name
- basic input access
- time
- logging
- prefab/entity spawning hooks if they already exist cleanly

Delay physics mutation, asset loading, and arbitrary component reflection until the first scripting loop is stable.

## System Flow

### Phase 1 runtime flow

On `ScriptComponent` construct:

- resolve the script asset/path
- create a script instance
- attach engine/entity API objects
- queue `on_init`

On update:

- iterate enabled `ScriptComponent`
- call `on_update(self, dt)` through `sol::protected_function`
- catch and log failures
- disable or mark failed scripts after repeated errors

On destroy:

- call `on_destroy`
- release runtime instance data

This fits naturally into the existing `ScriptSystem` lifecycle hooks in [ScriptSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/ScriptSystem.cpp).

## Editor Integration

The editor already hints at future script workflows, but the buttons are disabled in [EntityInspectorWidget.hpp](/Users/ag1498/GitHub/eduxEngine/src/editor/gui/EntityInspectorWidget.hpp).

Recommended editor scope for version 1:

- assign script path/id to `ScriptComponent`
- toggle enabled state
- show last runtime error
- reload one script or all scripts

Recommended version 2:

- inspect serialized script parameters
- expose documented callbacks/signature help
- hot reload the selected script from disk

## Asset and Path Design

The old hardcoded script path idea in [EditorTypes.hpp](/Users/ag1498/GitHub/eduxEngine/src/editor/EditorTypes.hpp) should not become the new long-term design.

Preferred direction:

- treat scripts as assets
- give them GUID-backed references eventually
- keep disk paths as an editor/import concern

Short-term compromise:

- keep `script_id` as a path string
- resolve it relative to project content roots
- centralize that resolution in one place

## Dependency Plan

### 1. Add `sol2`

Prefer vendoring or `FetchContent` for `sol2`, similar to the rest of the repo.

### 2. Add Lua itself explicitly

The current `find_package(Lua REQUIRED)` line in [CMakeLists.txt](/Users/ag1498/GitHub/eduxEngine/CMakeLists.txt#L68) is commented out, so Lua is not part of the current build.

Choose one of:

- `FetchContent` Lua
- system Lua package
- checked-in third-party dependency

For consistency with the current project, `FetchContent` or a vendored third-party folder is the cleanest path.

### 3. Turn on safer `sol2` defaults

For development builds, use safer call paths and protected functions. Stability and error messages matter more than absolute script-call speed early on.

## Safety Rules

Recommended guardrails for the first version:

- all script calls go through `sol::protected_function`
- all errors go to the engine log and visible editor UI
- scripts are not allowed to crash the frame loop
- avoid unrestricted file IO from scripts initially
- avoid exposing raw pointers broadly
- do not allow scripts to mutate registry structure while iterating sensitive views unless routed through command queues or deferred actions

## Serialization Design

Keep runtime Lua objects out of serialized scene data.

Serialize only:

- script reference/path/id
- enabled state
- explicit parameter values

Runtime-only data should live in `ScriptRuntime` or `ScriptSystem` side tables keyed by entity.

## Hot Reload Plan

Do not make hot reload a prerequisite for the first scripting milestone, but design for it.

Good future behavior:

- detect file change
- reload module
- preserve serialized fields
- rebuild instance table
- call optional `on_reload(self)`

The simplest first version can be manual reload only.

## Recommended First Milestone

The smallest useful Lua milestone for this engine would be:

1. Add `sol2` and Lua to the build.
2. Add `ScriptRuntime` service to engine context/services.
3. Extend `ScriptSystem` so it loads one script per `ScriptComponent`.
4. Support `on_init`, `on_update`, and `on_destroy`.
5. Expose a tiny API: entity id, transform get/set, logging, and input query.
6. Enable script assignment in the editor inspector.
7. Surface script errors in logs and inspector UI.

That would turn scripting from placeholder to genuinely usable without overcommitting to a huge reflective API.

## Design Recommendation

The old `sol` experiments in [CoreComponents.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/CoreComponents.cpp) suggest a very broad binding style. I would avoid starting there.

A better design for this engine is:

- narrow curated API
- runtime state stored outside ECS serialization
- one shared Lua state
- per-entity script instances
- protected calls everywhere
- editor-visible error reporting

That gives you a stable base that can later grow into richer scripting, prefab logic, data-driven behaviors, or tool scripting.
