# Terrain

## Scope
This note collects the current terrain MVP status plus the cleanup items and follow-up work we have identified while implementing cooked chunked terrain with Bullet heightfields.

## Current MVP
- Source asset: one artist-authored terrain mesh, imported as a `ModelDataAsset`.
- Editor recipe asset: `TerrainRecipeAsset`.
- Cook output:
  - one cooked `TerrainAsset` manifest
  - one cooked `TerrainChunkAsset` per chunk
  - one chunk-local render `ModelDataAsset`
  - one chunk-local `GpuModelAsset`
- Runtime:
  - one `TerrainComponent` points at a cooked `TerrainAsset`
  - `TerrainSystem` picks chunk coords and spawns runtime chunk entities
  - each chunk entity renders through `ModelComponent`
  - each chunk entity collides through a Bullet `btHeightfieldTerrainShape`

## Working Behavior
- Cooked terrain chunk render is visible in edit and play mode.
- Cooked terrain chunk collision works with dynamic rigid bodies.
- Edit/play switching no longer accumulates duplicate terrain chunk entities in the common case.
- Terrain debug rendering now shows cooked heightfield bounds instead of a meaningless placeholder box.
- Terrain recipes now support cook-time scaling:
  - `horizontal_scale_x`
  - `horizontal_scale_z`
  - `height_scale`

## Terminology
- `TerrainRecipeAsset`: editor/tool-side configuration asset.
- `Terrain Recipe`: user-facing shorthand for one `TerrainRecipeAsset` instance.
- `TerrainAsset`: cooked runtime terrain manifest.
- `TerrainChunkAsset`: cooked runtime chunk payload for one terrain chunk.

## Terrain Flows
### Runtime-Spawn Terrain Flow
This is the current MVP path.

Shape:
- `TerrainSystem` reads `TerrainComponent`
- `TerrainSystem` chooses desired chunk coords
- `TerrainSystem` loads chunk asset branches directly
- `TerrainSystem` spawns transient chunk entities directly

Pros:
- simple bring-up path
- easy to debug
- kept the first terrain implementation small enough to get render and collision working quickly

Cons:
- runtime-generated chunk entities sit outside the normal batch-loading model
- command/snapshot/save interactions need extra care
- terrain residency policy and terrain entity spawning are coupled in one system

### Batch-Backed Terrain Flow
This is the preferred direction going forward.

Shape:
- cooker generates one batch per terrain chunk
- each chunk batch contains the terrain chunk entity for that chunk
- `TerrainSystem` becomes a residency/policy system
- `TerrainSystem` requests chunk batch load/unload from `BatchRegistry`

Pros:
- aligns terrain residency with the engine's intended async world-part loading model
- keeps chunk entities batch-owned rather than ad hoc runtime-spawned
- makes future streaming logic fit naturally on top of batches
- reduces special-case command/snapshot/save concerns

Cons:
- adds one more generated artifact kind during terrain cook
- needs a clean mapping from terrain chunk coord to batch id/guid

## Direction
The current plan is:
- keep the Runtime-Spawn Terrain Flow while testing
- design and add the Batch-Backed Terrain Flow alongside it
- move terrain chunk residency toward batches once the design is proven

This means the current terrain implementation is still valid as an MVP, but it is not assumed to be the final architecture.

## Batch / Chunk Granularity
Guiding preference:
- terrain chunks should be as large as is practical
- terrain chunk batches should also be as large as is practical

Rationale:
- each batch load is linearized at the batch level
- asset work inside a batch can still be threaded
- larger batches/chunks should therefore amortize overhead better than many tiny ones

Practical limiter:
- terrain chunk size is still constrained by what Bullet can handle comfortably for one static heightfield collider
- if Bullet or memory/query cost sets the effective upper bound, that upper bound should influence terrain chunk batch size as well

Open question to calibrate later:
- what terrain chunk size is still comfortable for Bullet in this engine at the expected world scale and sample density?

## Bullet Note
Multiple terrain chunks can be loaded at the same time.

In practice, this means:
- multiple `btHeightfieldTerrainShape`s can exist simultaneously
- multiple terrain chunk batches can therefore be loaded simultaneously
- neighboring terrain chunk colliders are not a conceptual problem for Bullet

The real limits are practical rather than conceptual:
- total memory
- broadphase/collision-query cost
- sample density per chunk
- total number of loaded chunks

## Batch-Backed Terrain Sketch
Preferred long-term responsibilities:

Cooker:
- generate one `TerrainChunkAsset` per chunk
- generate one chunk-local render `ModelDataAsset` / `GpuModelAsset`
- generate one batch per terrain chunk containing the corresponding terrain chunk entity
- generate one top-level `TerrainAsset` manifest that maps chunk coords to runtime metadata and batch ids/guids

`TerrainAsset`:
- terrain-wide metadata
- chunk coord table
- per-chunk `TerrainChunkAsset` ref
- per-chunk batch id/guid

`TerrainSystem`:
- decide which chunk coords are wanted
- translate coords into terrain chunk batch ids/guids
- request batch load/unload through `BatchRegistry`
- avoid directly spawning/despawning chunk entities in the long-term path

BatchRegistry:
- own terrain chunk entity residency
- load and unload terrain chunk entities using the same mechanism used for other world parts

## Why This Matters
Terrain chunks are not just render/collision data; they are natural world residency units.

That matches the original motivation for batches:
- load/unload world parts asynchronously
- support larger worlds and future streaming
- keep persistent content and streamed content under a common runtime model

This strongly suggests terrain chunk residency should converge on batches.

## Immediate Cleanup / Follow-up
### 1. Replace chunk cell input with chunk count input
The current recipe still exposes chunk sizing in cooked terrain cells. This is engine-friendly, but not designer-friendly.

Preferred direction:
- editable:
  - `chunk_count_x`
  - `chunk_count_z`
  - `sample_spacing_x`
  - `sample_spacing_z`
- derived/read-only:
  - chunk world size
  - cells per chunk
  - samples per chunk

Rationale:
- Designers think more naturally in terms of "split this terrain into 2x2 or 4x4 chunks" than in terms of cooked grid cell counts.

### 2. Material transfer from source terrain to cooked chunk render assets
The terrain cooker currently generates chunk-local render meshes, but material propagation is still missing.

Desired behavior:
- copy the source terrain submesh material to each generated chunk submesh
- propagate that into the generated `GpuModelAsset`

Rationale:
- This is the most obvious remaining gap between the cooked terrain render path and normal imported render assets.

### 3. Make terrain runtime lifecycle more explicit
The current terrain runtime fix is intentionally small and pragmatic:
- `TerrainSystem` clears cached world-local runtime state when the active registry changes
- `TerrainSystem` prunes stale generated `terrain_chunk` child entities under terrain roots

This works, but a cleaner follow-up is still wanted.

Preferred direction:
- add an explicit runtime reset hook on play/edit transitions
- consider keying terrain root runtime by root GUID or `EntityRef` instead of raw `ecs::Entity`

Note:
- `EntityRef` helps with stable root identity, but it does not remove the need for explicit world-boundary cleanup because spawned chunk entities and pending runtime work are still world-local.
- `EntityRef` is still worth revisiting for terrain root identity, especially while the Runtime-Spawn Terrain Flow exists.

### 4. Auto-load lightweight editor assets for inspection
`TerrainRecipeAsset` is a configuration asset, so requiring manual load (`VLoad A`) before editing is awkward.

Preferred direction:
- auto-load lightweight editor assets when selected in the Asset Inspector

Candidates:
- `TerrainRecipeAsset`
- `TerrainAsset`
- other small editor/config assets

Alternative:
- explicit `Load For Editing` for unloaded data assets

Rationale:
- Terrain recipes are not naturally referenced by scene/runtime assets, so they otherwise tend to sit unloaded.

### 5. Bundle generated chunk assets more cleanly in the Resource Browser
Disk layout is already acceptable:
- one folder per recipe under the cooked terrain output root

UI layout is still the problem when many chunks exist.

Preferred direction:
- solve this first at the Resource Browser/view layer
- do not rush a "root asset" unless asset-graph nesting semantics are truly wanted

Possible approaches:
- file/folder view in the Resource Browser
- hide generated chunk assets by default
- collapse generated terrain chunk assets behind their terrain folder

Rationale:
- This is primarily a presentation problem, not yet a runtime asset model problem.

### 6. Improve heightfield fallback diagnostics
We already found one real issue because the warning path was useful.

Desired improvement:
- when heightfield build falls back, log the exact reason:
  - invalid GUID
  - asset not loaded
  - invalid sample data
  - other Bullet shape build failure

Rationale:
- Terrain chunk render and debug draw can look plausible even when Bullet is silently using a fallback shape.

### 7. Clean up terrain recipe / cook output naming
We already changed the cooked manifest display name so it no longer looks like a second recipe asset.

Still worth reviewing:
- default recipe naming
- output naming conventions for cooked terrain folders and generated assets

Rationale:
- The user should be able to tell at a glance:
  - this is a recipe
  - this is a cooked runtime terrain
  - these are generated chunk assets

### 8. Investigate command queue integrity around terrain-system-generated entities
This needs explicit follow-up.

This is not just a side concern; it is a central design constraint for the next terrain architecture step.

Requirement:
- the Batch-Backed Terrain Flow must preserve command queue integrity
- this is required whether terrain chunk residency actions are undoable or not
- terrain/runtime residency operations must not break command, snapshot, save/load, or undo/redo invariants

Problem statement:
- `TerrainSystem` spawns and destroys runtime chunk entities outside the normal editor command flow
- those entities exist in edit mode as well as play mode
- command queue assumptions may be violated if editor commands, snapshots, or validation logic interact with these generated entities as if they were ordinary user-authored scene entities

Questions to answer:
- Are terrain-system-generated chunk entities ever captured by editor commands or snapshots?
- Can they interfere with undo/redo assumptions?
- Can they interfere with batch save/load assumptions?
- Should they be tagged as transient/runtime-only in a more explicit way?

Possible directions:
- mark generated terrain chunk entities as transient/editor-hidden/runtime-owned
- exclude them from command/snapshot/save paths by policy
- move more terrain chunk residency behavior behind an explicit runtime layer

Questions that the batch-backed design must answer explicitly:
- Are terrain chunk batch load/unload requests represented as commands, or explicitly outside the command queue?
- If outside the command queue, how are undo/redo invariants protected?
- Can editor commands ever target terrain chunk entities directly?
- If not, how is that prevented or filtered?
- Are loaded terrain chunk entities excluded from normal scene snapshot/save flows?
- What happens if terrain residency changes while a command is in flight?

Rationale:
- This is a correctness/integrity issue, not just a UX issue.

### 9. Design and add the Batch-Backed Terrain Flow
This is now a core follow-up rather than an optional later idea.

Design goals:
- cooker can emit one batch per terrain chunk
- terrain manifest maps chunk coords to batch ids/guids
- `TerrainSystem` becomes a chunk residency policy system
- terrain chunk residency moves under `BatchRegistry`

Migration strategy:
- keep the current Runtime-Spawn Terrain Flow in place while the batch-backed path is designed and tested
- allow the old path and new path to coexist for a while if needed
- rename current terrain runtime code if that helps keep the two flows conceptually separate during the transition

## Secondary Follow-up
### 10. Revisit terrain scaling UX
Cook-time scaling now exists and is the preferred way to resize terrain without relying on runtime transform scaling.

Possible follow-up:
- clarify how cook-time terrain scale interacts with:
  - sample spacing
  - chunk count
  - derived chunk world size

Rationale:
- The user should not have to go back to Maya just to resize the terrain in-engine.

### 11. Add a terrain smoke/regression setup
Even if not a full automated test yet, keep one known-good terrain recipe plus one scene setup around.

Suggested checks:
- cook succeeds
- one chunk renders
- one chunk collides
- edit/play switching stays stable
- no heightfield fallback warning is emitted

## Suggested Order
1. Replace chunk cell input with chunk count input
2. Transfer materials into cooked chunk render assets
3. Design the Batch-Backed Terrain Flow
4. Make terrain runtime lifecycle explicit
5. Auto-load lightweight assets for inspection
6. Improve Resource Browser grouping for generated terrain assets
7. Investigate command queue integrity for terrain-generated entities
