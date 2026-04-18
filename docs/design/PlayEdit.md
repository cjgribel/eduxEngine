# Play/Edit Separation

## Overview

Play/Edit is implemented as two distinct worlds that share engine services. Edit mode runs editor-only systems against the edit world. Entering Play snapshots the loaded edit batches (excluding the editor batch) into a fresh play world and switches the active `EngineContext` binding to it. Exiting Play discards the play world and rebinds the edit world unchanged.

## Contract

### Mode Definitions
- **Edit Mode**: The editor is authoritative. Commands, selection, and editor-only systems operate on the edit world.
- **Play Mode (Preview)**: The game runtime is authoritative. Editor batch entities are excluded, and the play world is ephemeral.

### World Ownership and Persistence
- `EngineServices` are shared across modes (resource manager, event queue, command queue, etc).
- `WorldState` is per-mode (entity manager, batch registry, entity/batch selection).
- The edit `WorldState` is created once and preserved across play toggles.
- The play `WorldState` is created on enter Play and destroyed on exit.
- `EngineContext` is a compatibility view that binds to the active world.

### Batch and Entity Rules
- Enter Play: snapshot all **loaded** batches from edit, **excluding** the editor batch by name.
- Play world entities are reconstructed from snapshot JSON.
- Exit Play: play world is discarded; edit world state is restored as-is.

### System Gating
- Edit Mode: editor systems run (gizmos, inspector actions, command queue, batch dirty processing).
- Play Mode: runtime systems run (physics, scripts, gameplay). Editor actions are disabled.
- Physics resets on mode change to avoid stale Bullet state across world swaps.

### Asset and Resource Behavior (Preview)
- Assets are reused from edit mode; play snapshot loading **skips asset loading** for now.
- No strict unloading/loading cycle is enforced in Preview mode.

### UI and Control
- F5 or the toolbar button toggles play mode via `TogglePlayModeEvent`.
- GUI remains visible in Play, but edit actions are disabled.

### Known Limits (Planned)
- Warm Play is implemented, but Cold Play is not implemented yet.
- Per-world asset leases for strict isolation not implemented yet.

## Warm Play / Cold Play (Planned)

### Goal
Ensure play uses only runtime-approved content/assets, with deterministic loading and clean lifetimes.

Play should not be defined only as "unload editor batches, then load some preferred batches."

That batch swap is still the core mechanism, but the longer-term direction is:

- Preview mode reuses the editor-owned working set.
- Warm Play starts a game-owned play session in a fresh play world while the edit world remains resident.
- Cold Play should later start the same kind of game-owned play session, but after edit-owned content has been released.
- The game/runtime should be able to describe how that session boots, not only which batches happen to load first.

In practice, that means Warm/Cold Play should become the editor-hosted version of the same boot flow a standalone game target would eventually use.

### Long-Term Direction
The long-term goal is not necessarily "one new C++ runtime per game."

The engine should be able to support a future generic runtime host, e.g. `ScriptedGame`, where:

- the engine remains native C++
- the project/game is defined by data, assets, prefabs/batches, and scripts
- new games can be created without adding new C++ gameplay code

From that perspective, Strict Play should evolve toward:

- an engine-owned and data-driven boot path
- a runtime boot contract that can work for both custom C++ runtimes and a future generic scripted runtime
- a clear handoff point where project-defined gameplay logic takes over after engine-owned boot completes

This means Stage 1 should avoid pushing too much startup ownership into bespoke per-game C++ code, even if C++ runtimes are still the short-term host model.

### Flow
1) Determine the Warm/Cold Play boot configuration for the active project/runtime.
2) Create a fresh play `WorldState` (new `EntityManager`, `BatchRegistry`).
3) Let the app/runtime prepare the play world and content roots.
4) Load runtime-approved startup content with asset leases:
   - usually startup batches first
   - later possibly a richer boot path such as entry scene/level/save/menu
   - run a loading loop that pumps `main_thread_queue` and `event_queue` until all loads complete
5) Enter Play and run runtime systems.
6) Exit Play: unload play-owned batches/assets, destroy play world, and rebind edit world.

### Current Warm Play Behavior
Current Warm Play keeps the edit world alive while play runs.

- edit-loaded batches remain loaded in the edit world during Warm Play
- overlapping assets can therefore remain resident while both edit and play hold leases
- on exit, play-owned batches/assets are unloaded before the engine rebinds the edit world

This makes Warm Play fast to enter and leave, but it does not model a cold startup cost yet.

### Cold Play Direction
Cold Play should later use the same runtime boot contract as Warm Play, but with stronger residency isolation.

- release/unload edit-owned content before or during play entry
- force runtime startup content to pay the true load cost
- make loading screens and startup regressions visible during development

### Why This Is More Than Startup Batches
Returning batch names is a good minimal contract, but a playable session often needs more information:

- which batch index to use
- which startup batches to load
- whether the runtime should start in a menu, hub, level, or test map
- runtime-only spawn/setup work after content load
- loading-screen behavior

The design should therefore evolve from "Warm/Cold Play loads these batches" to "Warm/Cold Play boots the game/runtime using a project-defined boot contract."

### Configuration Direction
Warm/Cold Play should resolve boot inputs from project/runtime configuration rather than hardcoded strings in runtime code.

Examples of boot inputs that belong in config:

- batch index path
- startup batch list
- optional entry level/scene identifier
- optional loading-screen or boot mode flags

Why:

- different projects can define their own runtime startup behavior cleanly
- editor-hosted Warm/Cold Play and standalone game launch can share the same boot description
- content/startup changes do not require recompiling game code
- runtime code stays focused on behavior instead of path wiring

### App Hooks
- `IApp::play_policy()` selects Preview vs Warm Play (EditorApp may override via UI).
- `IApp::on_play_world_created(ctx)` configures the new play world before play-load work begins.
- `IApp::play_startup_batches()` remains the minimal Stage 1 contract for startup batch names.
- `IApp::on_enter_play(ctx)` and `IApp::on_exit_play(ctx)` bracket the play session.

### Runtime Preferences
- `IGameRuntime::preferred_play_policy()` expresses the runtime default.
- `IGameRuntime::preferred_startup_batches()` is a temporary standalone fallback until `GameApp` loads project boot config too.

### Staged Plan

#### Stage 1: Config-Driven Warm Play
- Keep Warm Play simple and useful.
- Boot a fresh play world using runtime/project-configured startup batches.
- Resolve the batch index and startup batch list from config rather than hardcoded runtime strings.
- Treat missing or malformed Warm-Play boot config as a project error; do not silently invent defaults.
- On exit, explicitly unload the play-owned batches/assets before destroying the play world.

This stage is enough to make a new game target feel real rather than just an editor preview.

#### Stage 2: Game-Owned Session Boot
- Keep startup batches as the first content-loading mechanism.
- Add a richer boot contract so the runtime can describe how a session starts.
- Allow the runtime to choose entry scene/level/menu/test map and perform post-load runtime-only setup such as spawning the player or initializing session state.

This stage is where Warm/Cold Play becomes the editor-hosted form of real game startup, not just a batch swap.

#### Stage 3: Cold Play And UX
- Add Cold Play as a residency-isolated version of the same boot contract.
- Add loading-screen/state support for play startup.
- Support richer config-driven boot descriptors beyond startup batch names.
- Make Preview, Warm Play, and Cold Play easy to switch in editor UI and easy to understand.
- Keep the standalone game target and editor-hosted play aligned on the same boot path.

### Edit World Lifetime
Default Warm Play behavior: keep edit world alive but inactive so returning to Edit is instant.
Cold Play option: unload edit batches during play to free memory and expose true startup cost (slower enter/exit).

### Differences vs Preview
- No asset borrowing from edit; play acquires its own leases.
- Deterministic runtime loading order (suitable for loading screens).
- Clear separation of asset lifetimes and side effects.
- Project-driven Warm/Cold Play should fail loudly on missing boot config rather than fall back implicitly.

### Relationship To Scripting
Warm/Cold Play boot and Lua gameplay scripting are related, but they should be treated as mostly orthogonal tracks.

- Warm/Cold Play defines how a play session starts and owns its world/content lifetime.
- Scripting should later plug into that runtime once the play world is alive.
- Boot descriptors/config should stay serializable and engine-owned; they should not depend on Lua state existing.

Expected later scripting roles:

- post-load session setup hooks
- gameplay behaviors after play begins
- optional menu/mission/game-mode logic
- eventual project-defined entrypoint logic in a generic scripted runtime

Expected non-scripting responsibilities that still belong in Warm/Cold Play:

- create/destroy the play world
- resolve project/runtime boot config
- load and unload startup content
- maintain deterministic ownership of assets and runtime lifetimes

So scripting is worth keeping in mind, but it should not block Stage 1 Warm Play work.
The Stage 1 design should instead aim to remain valid when a future `ScriptedGame` runtime replaces bespoke C++ gameplay classes for some projects.

## TODO

### Now
- [x] Split ownership into EngineServices + WorldState, keep EngineContext compatibility view.
- [x] Update EngineFactory to construct services + an edit world.

### Next
- [x] Add in-memory batch snapshot helpers (serialize edit world without disk).
- [x] Add play world creation + EngineContext bind switch.
- [x] Gate runtime systems vs editor systems by mode.
- [ ] Remove the temporary EngineContext 6-arg compatibility constructor.
- [ ] Define GameContext API surface (what game code can access vs editor-only).
- [ ] Add a physics queries interface (raycasts/overlaps) for game code.
- [ ] Stage 1: move Warm Play startup inputs out of hardcoded runtime strings and into project/runtime config.
- [ ] Stage 1: make Warm Play explicitly unload play-owned batches/assets on exit before destroying the play world.
- [ ] Stage 1: finish Preview vs Warm Play editor UI/policy controls and expose the active policy clearly.
- [ ] Stage 1: add tests for Warm Play enter/exit ownership and asset-lifetime behavior.
- [ ] Stage 2: add a richer play boot contract beyond startup batch names.
- [ ] Stage 2: support runtime-owned post-load session boot work such as player spawn/session initialization.
- [ ] Stage 2: restore parity between editor-hosted play and standalone `run_game<T>()` so project-config boot data can drive both paths.
- [ ] Stage 3: add Cold Play entry behavior and a loading screen/state for play startup.

### Later
- [ ] Split component meta registration: engine vs game.
- [ ] Move game-specific components/systems out of `src/engine/ecs` into game module.
- [ ] Add tests for snapshot + restore integrity.
- [ ] Restructure folders into `engine/`, `editor/`, `game/` roots.
- [ ] Legacy Game cleanup: move editor UI, selection-driven camera focus, debug draw, and bootstrap logic out of `projects/legacy_game/Game.cpp`.
- [x] Rename `Module1` to `projects/legacy_game` and update build files.
- [ ] Add a runtime/plugin system to allow swapping game types in the editor (beyond data reload).
- [ ] Audit global caches and runtime state for strict play reset (physics/script state, etc).
- [ ] Add a standalone game-app bootstrap path that can load project config/content roots without depending on `EditorApp`.
- [ ] Keep future Lua/script runtime integrated through runtime hooks, without making Warm/Cold Play depend on script state or script-defined ownership.
