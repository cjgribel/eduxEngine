# Terrain

## Scope
This note collects the current terrain MVP status plus the cleanup items and follow-up work we have identified while implementing cooked chunked terrain with Bullet heightfields.

## Current MVP
Historical note:
- the first terrain MVP used a runtime-spawned chunk flow driven by `TerrainSystem`
- that path is no longer the intended long-term implementation

- Source asset: one artist-authored terrain mesh, imported as a `ModelDataAsset`.
- Editor recipe asset: `TerrainRecipeAsset`.
- Cook output:
  - one cooked `TerrainAsset` manifest
  - one cooked `TerrainChunkAsset` per chunk
  - one chunk-local render `ModelDataAsset`
  - one chunk-local `GpuModelAsset`
- Runtime:
  - one terrain-root scene entity points at a cooked `TerrainAsset`
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
- `TerrainAsset`: cooked runtime terrain collection/manifest.
- `TerrainChunkAsset`: cooked runtime chunk payload for one terrain chunk.
- `TerrainRootComponent`: optional scene-level root hook pointing at a `TerrainAsset`.

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
The current terrain implementation was a valid MVP bring-up path, but the intended architecture is now the Batch-Backed Terrain Flow.

The main design goal going forward is:
- keep terrain data model and runtime ownership clean
- avoid carrying two terrain runtime paths longer than necessary
- prefer replacing the runtime-spawn path rather than letting both paths linger indefinitely

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

## Clean Structure
If we optimize for clarity first, the terrain feature should be shaped like this:

### `TerrainRecipeAsset`
- editor-only cook configuration
- points at the authored source terrain mesh
- exists to make re-cooking easy and deterministic

### `TerrainChunkAsset`
- cooked collision payload for one chunk
- owns the heightfield sample data and chunk bounds needed by Bullet
- should not also act as a render manifest or residency controller

### Chunk render `GpuModelAsset`
- cooked render asset for one chunk
- referenced by the generated terrain chunk entity's `ModelComponent`
- kept separate from `TerrainChunkAsset` so render and collision responsibilities stay cleanly split

### Chunk batch
- generated batch containing the actual runtime terrain chunk entity
- this is the true runtime end product
- contains:
  - `HeaderComponent`
  - `TransformComponent`
  - `ModelComponent`
  - static `RigidBodyComponent`
  - `ColliderComponent` referencing `TerrainChunkAsset`

### `TerrainAsset`
- optional terrain-wide manifest / collection asset
- generated summary/index over all terrain chunks
- useful for metadata, debugging, editor tooling, and future policy/streaming logic
- should stay minimal and should not own extra runtime-only references

### `TerrainRootComponent`
- optional terrain root hook on a scene entity
- useful only if we want terrain-level metadata or future terrain policy anchored in the scene
- should stay minimal and should not become a hidden residency manager by itself

## Upcoming Version: Exact Shape
This section captures the intended shape of the first Batch-Backed Terrain Flow version before we start coding it.

The goal is to keep the current Runtime-Spawn Terrain Flow available for bring-up/testing, while introducing a second path that is more native to the engine's batch model.

### `TerrainAsset`
The cooked terrain manifest should evolve from:
- "coord -> `TerrainChunkAsset` ref"

to:
- terrain-wide metadata
- chunk coord table
- per-chunk `TerrainChunkAsset` ref
- per-chunk batch id
- optionally per-chunk batch name for debugging/tooling
- per-chunk bounds/placement metadata that can be read without loading the chunk batch

Suggested shape:
- keep `world_origin`
- keep total sampled size / sample spacing
- keep chunk layout metadata
- replace the flat `chunks` vector with a chunk-entry struct, for example:
  - `coord`
  - `terrain_chunk_ref`
  - `batch_id`
  - `batch_name`
  - `world_bounds_min`
  - `world_bounds_max`

Rationale:
- the manifest needs to answer both:
  - "what is the chunk payload?"
  - "which batch should be loaded for this chunk?"
- bounds in the manifest allow chunk-selection logic to mature later without forcing the system to touch batch files or chunk assets

This asset is justified as "terrain as a collection", not as the thing that actually renders or collides.

### `TerrainChunkAsset`
`TerrainChunkAsset` remains the cooked payload for one terrain chunk.

It should continue to own:
- heightfield sample data for Bullet
- chunk-local bounds

It should not:
- become responsible for batch residency decisions
- own the chunk render `GpuModelAsset` ref in the clean long-term structure

Rationale:
- chunk asset = payload
- batch = residency unit
- `TerrainSystem` = policy/orchestration

### `TerrainRecipeAsset`
The recipe remains editor-side configuration.

The main user-facing change still wanted here is:
- move from chunk cell entry to chunk count entry

Preferred direction:
- editable:
  - `chunk_count_x`
  - `chunk_count_z`
  - `sample_spacing_x`
  - `sample_spacing_z`
  - cook-time terrain scale
- derived/read-only:
  - chunk world size
  - cells per chunk
  - samples per chunk

This is not specific to the batch-backed flow, but it should be done before the new batch-backed terrain cook UI is considered "clean".

## Cooker Output Layout
The cooker should produce two kinds of generated output:

### Asset-side output
Under the terrain cook folder in imported assets:
- one cooked `TerrainAsset` manifest
- one cooked `TerrainChunkAsset` per chunk
- one chunk-local `ModelDataAsset` per chunk
- one chunk-local `GpuModelAsset` per chunk

Example layout:
- `imported_assets/terrain/<recipe>/terrain.asset`
- `imported_assets/terrain/<recipe>/chunks/...`
- `imported_assets/terrain/<recipe>/render/...`

### Batch-side output
Under the project's batch root:
- one generated batch per terrain chunk
- one batch index entry per terrain chunk

Each generated terrain chunk batch should contain exactly the entity shape we already know works:
- `HeaderComponent`
- `TransformComponent`
- `ModelComponent`
- static `RigidBodyComponent`
- `ColliderComponent` with one `Heightfield` collider pointing at the chunk's `TerrainChunkAsset`

This keeps terrain chunk entities as ordinary loaded world content rather than special transient runtime entities.

### Determinism
Re-cooking the same recipe should overwrite the same generated outputs.

That implies deterministic identities for:
- generated terrain chunk assets
- generated render assets
- generated batch ids or names

This is important for:
- command queue integrity
- avoiding UI clutter
- keeping references stable across re-cooks
- making "tune settings and cook again" a normal workflow

## `TerrainSystem` In The Batch-Backed Terrain Flow
`TerrainSystem` should narrow into a residency policy/orchestration system, if we still need it once chunk batches exist.

It should:
- load or read the cooked `TerrainAsset` manifest
- compute wanted chunk coords
- map wanted coords to generated terrain chunk batch ids
- diff wanted chunk batches against currently requested/resident chunk batches
- request batch load/unload through `BatchRegistry`

It should not:
- directly spawn chunk entities
- directly destroy chunk entities
- directly load chunk asset branches in the long-term path

The terrain root entity should remain the anchor:
- `TerrainRootComponent` points at one cooked `TerrainAsset`
- `TerrainSystem` reads that component and issues residency requests

The current explicit-chunk MVP policy still fits:
- wanted set = one coord from the terrain root component

Later policies can reuse the same shape:
- radius around player
- editor camera interest set
- debug-selected chunk set

### Minimal bring-up variant
There is also a deliberately less magical first version worth considering, and this is likely the best first batch-backed implementation:
- cooker generates chunk batches
- game/editor loads and unloads those batches directly
- `TerrainSystem` does little or nothing in the batch-backed path at first, or is bypassed entirely

In that variant:
- `TerrainRootComponent` mainly points at the manifest for metadata/debug/editor purposes
- chunk load state is owned entirely by existing game/editor batch logic
- terrain-specific runtime selection can be added later only when it is actually needed

This is attractive if we want the first batch-backed terrain version to prove generated chunk batches without also introducing a new residency policy layer.

## Command Queue Integrity In The Batch-Backed Flow
This must stay central.

The preferred model is:
- terrain chunk residency is orchestrated through batch load/unload
- terrain chunk entities are batch-owned generated content
- terrain chunk residency itself does not need to be a normal user-authored edit command
- but it must not violate command queue invariants

This strongly suggests that generated terrain chunk batches should be treated as generated/read-only content in normal editor workflows.

Practical implications to design for:
- user editing should target the terrain source / recipe / root entity, not the loaded generated chunk entities
- commands should not silently mutate generated terrain chunk batches as if they were authored scene content
- snapshot/save behavior must stay explicit and predictable

This is one of the strongest reasons to move away from runtime ad hoc chunk spawning.

## Likely Core Engine Changes
These are the main areas that may need engine-level changes rather than terrain-local code only.

### 1. Broaden `IBatchRegistry`
Right now `IBatchRegistry` only exposes `queue_unload_all_async(...)`.

For a terrain system that is batch-driven rather than spawn-driven, the interface likely needs terrain-usable methods such as:
- `queue_load(batch_id, ctx)`
- `queue_unload(batch_id, ctx)`
- `is_batch_loaded(batch_id)`
- batch lookup by name if ids are not enough for tooling/debug

Rationale:
- `TerrainSystem` should not need to downcast to concrete `BatchRegistry`
- batch-backed terrain should use the same abstraction boundary as other systems

Design note:
- runtime-facing batch operations and editor/tooling batch operations do not need to live in the same interface
- a clean split may be better than continuing to keep `IBatchRegistry` tiny while engine/runtime code reaches for concrete `BatchRegistry`

### 2. Batch metadata for generated/read-only content
Terrain chunk batches are not normal hand-authored scene batches.

The batch model may need metadata such as:
- generated vs authored
- owning/generated-by asset guid
- editable vs read-only
- generator tag (for example terrain cook)

Rationale:
- helps command queue integrity
- helps editor behavior
- helps future Resource Browser / batch UI grouping
- makes re-cook/update/delete semantics safer

### 3. Tool-side batch authoring API
The terrain cooker currently writes assets. The batch-backed flow also needs it to create/update generated batches and batch-index entries deterministically.

That may want a dedicated API rather than ad hoc JSON writing inside the cooker.

Rationale:
- batch index format should stay centrally owned
- generated batch creation/update should be deterministic and safe
- future generators besides terrain may want the same path

### 4. Deterministic generated ids
Today many generated ids are created with fresh random `Guid::generate()`.

For terrain chunk batches and chunk assets, deterministic ids would be valuable for:
- stable recooks
- stable references
- less churn in saved state/UI

If the engine does not yet have a clean deterministic-guid utility, this may be a small but useful core addition.

### 5. Editor behavior for generated loaded content
Once terrain chunks are batch-loaded, the editor needs a clear stance on what the user can do with those entities.

Possible needs:
- show them as generated/read-only
- prevent normal authoring commands from targeting them
- or route edits back to the owning terrain recipe/root instead

This is partly editor code, but it touches core assumptions about command routing and entity ownership.

## Migration Notes
The preferred migration order is now:
- commit the current Runtime-Spawn Terrain Flow as a working milestone
- extend the cooker to emit generated chunk batches
- simplify `TerrainAsset` into a clean collection/manifest asset
- remove render refs from `TerrainChunkAsset`
- move runtime terrain chunk residency to batches
- delete the runtime-spawn path once the batch-backed flow is proven

Keeping both flows side by side for too long is likely to create confusion and forgotten code paths.

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
