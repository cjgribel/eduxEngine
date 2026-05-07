# Camera View Flow

This note documents how camera transforms currently move through the engine/editor/runtime boundary, why that works, and why it is still more fragmented than we probably want long term.

## Status

The first cleanup step from this note is now in place:

- [RenderContext.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/RenderContext.hpp) carries an explicit [CameraView.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/CameraView.hpp)
- [EditorApp.hpp](/Users/ag1498/GitHub/eduxEngine/src/app/EditorApp.hpp) chooses the active camera view for editor-hosted rendering
- [GameApp.hpp](/Users/ag1498/GitHub/eduxEngine/src/app/GameApp.hpp) stays editor-free and only asks the runtime for a gameplay camera
- [IGameRuntime.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/IGameRuntime.hpp) now exposes `build_play_camera_view(...)` for game-owned play cameras

`OverlayViewState` still exists as a bridge for debug rendering, picking, and gizmos. So the architecture is cleaner than before, but not yet at the final “one view type everywhere” end state.

## Current Flow

### 1. Editor cameras live in ECS

The editor owns camera components in the main registry:

- [ThirdPersonCameraComponent.hpp](/Users/ag1498/GitHub/eduxEngine/src/editor/ecs/ThirdPersonCameraComponent.hpp)
- [FirstPersonCameraComponent.hpp](/Users/ag1498/GitHub/eduxEngine/src/editor/ecs/FirstPersonCameraComponent.hpp)

Those components store camera state plus cached matrices:

- `model_to_view`
- `view_to_world`

The editor camera systems update them directly every edit frame:

- [EditorApp.hpp](/Users/ag1498/GitHub/eduxEngine/src/app/EditorApp.hpp#L124)
- [EditorRuntime.cpp](/Users/ag1498/GitHub/eduxEngine/src/editor/EditorRuntime.cpp#L26)
- [EditorCameraSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/editor/ecs/EditorCameraSystem.cpp#L51)
- [ThirdPersonCameraSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/editor/ecs/ThirdPersonCameraSystem.cpp#L140)
- [FirstPersonCameraSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/editor/ecs/FirstPersonCameraSystem.cpp#L97)

So the first important observation is:

- editor camera state is not sent through a dedicated transport layer
- it is just shared ECS state in the same world

### 2. The runtime must pull a camera out of that state

`IGameRuntime` does not receive a camera in `update_*()` or `render_scene()`.

See:

- [IGameRuntime.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/IGameRuntime.hpp#L65)

That means each runtime is responsible for deciding:

- which camera is active
- how to build projection
- how to expose the current view to editor tools

Examples:

- `legacy_game` reads the active editor camera into a local `active_camera` cache and builds `P`, `V`, and viewport matrices itself in [Game.cpp](/Users/ag1498/GitHub/eduxEngine/projects/legacy_game/Game.cpp#L218)
- `fluid_sandbox` now reads the active editor camera and builds an `OverlayViewState` directly in [FluidSandboxGame.cpp](/Users/ag1498/GitHub/eduxEngine/projects/fluid_sandbox/FluidSandboxGame.cpp#L196)

### 3. Scene rendering uses game-provided matrices

The engine runtime pipeline does not choose a camera. It expects the game/runtime to provide the matrices already assembled.

See:

- [RuntimePipeline.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/RuntimePipeline.hpp#L206)
- [Game.cpp](/Users/ag1498/GitHub/eduxEngine/projects/legacy_game/Game.cpp#L417)

For example, `legacy_game` computes:

- `proj_view`
- `eye_pos`
- particle-facing basis vectors

and passes them into:

- `render_entities(...)`
- `render_particles(...)`

### 4. Overlay tools use `OverlayViewState`

There is one shared per-frame view object for overlay/debug rendering:

- [OverlayViewState.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/OverlayViewState.hpp)

It currently contains:

- `view`
- `proj`
- `viewport`
- `window_size`
- `valid`

This is used for:

- shape-renderer overlay flush in [Engine.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/Engine.cpp#L421)
- editor gizmo rendering via the editor render hook in [Engine.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/Engine.cpp#L423)
- editor-side consumption through `IGameRuntime::get_editor_view(...)` in [IGameRuntime.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/IGameRuntime.hpp#L118)
- mouse picking / mouse-point constraints in [MousePointConstraintSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/MousePointConstraintSystem.cpp#L22)

So the second important observation is:

- scene rendering camera flow is game-local
- overlay and picking camera flow is shared through `OverlayViewState`

## What This Means In Practice

Today the camera path is split across three levels:

1. Editor camera ECS components hold the raw camera state and cached view matrices.
2. Each runtime decides how to turn that into its own render data.
3. `OverlayViewState` acts as a second, shared camera snapshot for tools and debug rendering.

This is flexible, but it also means a target can accidentally break editor camera behavior simply by forgetting one handoff step.

That is exactly what happened in `fluid_sandbox` before its camera-view fix:

- editor camera input was updating correctly
- but the runtime published identity view/projection matrices
- so the sandbox looked like the editor camera was disconnected

## What Is Good About The Current Shape

- Editor cameras are plain ECS data, which is easy to inspect and serialize.
- Runtimes can choose their own camera policy.
- Overlay and picking already have a shared camera snapshot.
- The engine stays relatively agnostic about gameplay camera rules.

## What Is Weak About The Current Shape

- Camera state is duplicated in multiple forms:
  - editor camera components
  - game-local camera caches
  - `OverlayViewState`
  - runtime-local projection rebuilds
- `render_scene()` has no explicit camera/view contract.
- A runtime must remember to keep scene rendering, overlay rendering, and editor view export in sync.
- Bugs are easy when one path is updated but another is not.

## Recommendation

Move toward one canonical per-frame camera snapshot that is produced once and consumed everywhere.

Suggested direction:

```cpp
struct CameraView
{
    glm::mat4 view{ 1.0f };
    glm::mat4 proj{ 1.0f };
    glm::mat4 view_proj{ 1.0f };
    glm::mat4 viewport{ 1.0f };

    glm::vec3 position{ 0.0f };
    glm::vec3 forward{ 0.0f, 0.0f, -1.0f };
    glm::vec3 up{ 0.0f, 1.0f, 0.0f };
    glm::vec3 right{ 1.0f, 0.0f, 0.0f };

    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    glm::ivec2 window_size{ 0, 0 };
    bool valid = false;
};
```

Then:

- editor camera systems still own the source camera components
- one place resolves the active camera and builds a `CameraView`
- that `CameraView` is published once per frame
- scene rendering, overlays, gizmos, and picking all consume the same snapshot

## Minimal Refactor Path

If we want to improve this incrementally rather than redesigning it all at once:

1. Introduce a dedicated `CameraView` type instead of reusing `OverlayViewState` for multiple purposes.
2. Add the active camera snapshot to `RenderContext` or `EngineContext`.
3. Let `render_scene()` consume that shared camera snapshot instead of having every runtime rediscover it.
4. Keep `OverlayViewState` only if we still need a separate tool-facing wrapper; otherwise replace it with `CameraView`.

## Later: Debug Render Passes

One related idea that looks promising, but is not urgent enough to do right now, is to formalize debug/overlay rendering as explicit passes rather than one shared shape queue plus one shared overlay view.

Suggested direction:

- keep `ShapeRenderer` as the low-level line/point/solid batcher
- add a small `DebugRenderView` type that is explicit about the view being flushed
- add named debug render passes, each with:
  - one `DebugRenderView`
  - one shape queue
  - optional per-pass settings
- let editor-owned code publish edit views
- let game/runtime code publish play views
- let the engine flush passes explicitly in order

Example pass names:

- `editor_overlay`
- `play_overlay`
- later `split_left_overlay`
- later `split_right_overlay`

This is motivated less by `ShapeRenderer::render(proj_view)` itself and more by the hidden policy around who owns the matrices that are passed into that flush.

What we would gain:

- clearer ownership of edit-view vs play-view debug rendering
- less need for game targets to know editor camera details
- a cleaner path to multiple simultaneous views such as split screen
- easier debugging when overlays are wrong, because the target pass/view is explicit
- better separation between low-level drawing and higher-level camera/view policy

Tradeoffs:

- more concepts to carry around (`DebugRenderView`, pass ids, routing/flushing)
- more plumbing for systems that currently just push into one shared queue
- migration cost for existing gizmo/debug producers
- some risk of overdesign if the engine stayed single-view forever

Why defer it:

- the current architecture can be improved meaningfully first by clarifying shared camera/view ownership
- the engine does not need multi-view debug rendering immediately
- the pass-based model is more valuable once split-screen or additional runtime/editor views become real product needs

So the likely longer-term target is:

- `ShapeRenderer` stays simple
- camera/view ownership becomes explicit
- debug rendering moves toward explicit passes/views rather than one global implicit overlay flush

If split screen becomes active work, this idea should move up in priority.

## Short Version

The current system is workable, but not clean:

- camera source of truth: editor camera ECS components
- scene render handoff: ad hoc and runtime-specific
- overlay/picking handoff: shared through `OverlayViewState`

That split is the main reason camera bugs can appear in one target even while editor camera input itself is working correctly.
