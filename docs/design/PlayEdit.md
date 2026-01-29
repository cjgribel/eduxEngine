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
- Strict Play mode (runtime-only batch load + loading screen) not implemented yet.
- Per-world asset leases for strict isolation not implemented yet.

## Strict Play Mode (Proposed)

### Goal
Ensure play uses only runtime-approved batches/assets, with deterministic loading and clean lifetimes.

### Flow
1) Determine runtime batch set (app-owned; often sourced from the game runtime).
2) Create a fresh play `WorldState` (new `EntityManager`, `BatchRegistry`).
3) Load runtime batches with asset leases:
   - Call `load_and_bind_async` for each batch.
   - Run a loading loop that pumps `main_thread_queue` and `event_queue` until all loads complete.
4) Switch to Play mode and run runtime systems.
5) Exit Play: unload play batches (release leases), destroy play world, and rebind edit world.

### App Hooks
- `IApp::play_policy()` selects Preview vs Strict (EditorApp may override via UI).
- `IApp::on_play_world_created(ctx)` configures the new play world (e.g., load batch index).
- `IApp::play_startup_batches()` returns batch names to load for Strict mode.
- `IApp::on_enter_play(ctx)` and `IApp::on_exit_play(ctx)` bracket the play session.

### Runtime Preferences
- `IGameRuntime::preferred_play_policy()` expresses the runtime default.
- `IGameRuntime::preferred_startup_batches()` provides default Strict-mode batches.

### Edit World Lifetime
Default: keep edit world alive but inactive so returning to Edit is instant.
Optional: unload edit batches during play to free memory (slower exit).

### Differences vs Preview
- No asset borrowing from edit; play acquires its own leases.
- Deterministic runtime loading order (suitable for loading screens).
- Clear separation of asset lifetimes and side effects.

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
- [ ] Add Play mode variants: Preview (reuse edit-loaded assets/batches) vs Strict (load runtime batches only).
- [ ] Add per-world asset lease tracking for Strict Play mode (avoid edit/play stomping).

### Later
- [ ] Split component meta registration: engine vs game.
- [ ] Move game-specific components/systems out of `src/ecs` into game module.
- [ ] Add tests for snapshot + restore integrity.
- [ ] Restructure folders into `engine/`, `editor/`, `game/` roots.
- [ ] Legacy Game cleanup: move editor UI, selection-driven camera focus, debug draw, and bootstrap logic out of `Module1/Game.cpp`.
- [ ] Rename `Module1` to a clearer legacy/samples name and update build files.
- [ ] Add a runtime/plugin system to allow swapping game types in the editor (beyond data reload).
