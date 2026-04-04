# Terrain

## Purpose
Terrain is now built around generated chunk batches.

The runtime end product is:
- one generated batch per terrain chunk
- one entity in that batch with:
  - `ModelComponent` referencing the cooked chunk render `GpuModelAsset`
  - `ColliderComponent` referencing the cooked heightfield `TerrainChunkAsset`
  - static `RigidBodyComponent`
  - `TransformComponent`

The terrain data model is:
- `TerrainRecipeAsset`: editor-only cook configuration
- `TerrainChunkAsset`: cooked collision payload for one chunk
- chunk render assets: cooked render assets for one chunk
- `TerrainAsset`: terrain-as-a-collection manifest
- `TerrainRootComponent`: optional scene-level reference to a `TerrainAsset`

## Functional Requirements
Current terrain behavior should support:
- author one clean source terrain mesh
- create and edit a `TerrainRecipeAsset`
- cook terrain into chunk assets and chunk batches
- manually load chunk batches through the existing batch UI
- see loaded chunk render and collision in edit and play mode
- re-cook deterministically without stale generated terrain artifacts lingering
- clear cooked terrain artifacts deterministically
- scale terrain at cook time
- control chunking through chunk counts
- preserve source terrain material and texture dependencies on cooked chunk render assets

## Current Shape
What we have now:
- terrain cooking generates:
  - one `TerrainAsset`
  - one `TerrainChunkAsset` per chunk
  - one chunk-local `ModelDataAsset`
  - one chunk-local `GpuModelAsset`
  - one generated batch per chunk
- `TerrainChunkAsset` is collision/bounds data only
- render asset references live on the generated chunk entity, not in `TerrainChunkAsset`
- cooked chunk render assets reuse the source terrain material/texture package instead of duplicating those assets per chunk
- `TerrainAsset` stores chunk collection metadata, chunk payload refs, batch ids, and chunk bounds
- `TerrainRootComponent` is minimal and does not drive chunk spawning
- the old runtime-spawn terrain path is no longer the intended path

## Limitations
- chunk load state is still manual; no streaming or smart residency logic yet
- generated terrain assets and batches are still somewhat noisy in the Resource Browser
- `HeaderComponent.chunk_tag` is an old remnant and should not be treated as the long-term terrain metadata mechanism
- some lower-level engine APIs can still mutate entities without going through editor command policy; terrain safety currently focuses on the editor-facing mutation paths
- terrain material transfer currently assumes the source terrain material follows the normal imported `MaterialAsset -> GpuMaterialAsset` child relationship
- cooked terrain render assets currently depend on the source terrain import package remaining present, because materials/textures are shared rather than duplicated
- terrain chunk UV generation is currently chunk-local and normalized to `0..1`, which breaks authored tiled UV density across cooked chunks
- Assimp/Maya UV transform or placed-texture repeat settings are not part of the current terrain cook contract

## Command Integrity
Generated terrain batches can still be loadable through commands without threatening command queue integrity, but only if we keep the semantics strict.

Safe model:
- load/unload of generated terrain chunk batches is a residency action, not an authoring action
- load/unload commands may remain undoable
- generated terrain chunk entities and generated terrain chunk batches must be treated as read-only
- normal edit commands must not target generated terrain chunk entities
- normal batch save/edit/delete flows must be filtered or specialized for generated terrain batches
- cook and clear-cook should remain explicit build actions, not ordinary scene-edit commands

Main risk:
- integrity is threatened when a generated batch is not only loaded, but also treated like normal authored editable content

So the key distinction is:
- loadable: yes
- editable like authored content: no
- cook / clear-cook pre-unload generated terrain batches off the RM strand before entering the RM job, to avoid BatchRegistry <-> ResourceManager deadlocks

## Near-Term Plan
Immediate next steps:
- expose normal batch delete in the GUI
- improve generated terrain asset/batch grouping in the Resource Browser
- continue reviewing lower-level mutation paths that bypass editor command policy
- fix cooked terrain chunk UV generation so source tiling works across chunk boundaries
- decide whether terrain should continue to rely on authored mesh UVs only, or whether material/texture UV transforms should become a first-class rendering feature

## `IBatchRegistry` / `BatchRegistry`
Current direction:
- `IBatchRegistry` should hold runtime-facing residency operations
- `BatchRegistry` should keep tooling/editor-oriented batch authoring operations

Now on `IBatchRegistry`:
- `queue_load(...)`
- `queue_unload(...)`
- `queue_unload_all_async(...)`
- `is_batch_loaded(...)`
- `is_batch_read_only(...)`
- `try_get_loaded_batch_for_entity(...)`
- `try_get_batch_id_by_name(...)`

Likely keep concrete/tooling-only in `BatchRegistry`:
- batch index loading/saving
- batch creation/deletion/restoration
- generated batch upsert
- snapshot/save helpers
- attach/detach/spawn/create/destroy entity helpers

## Notes
- Terrain chunks and terrain chunk batches should be as large as practical.
- Bullet can handle multiple loaded heightfield chunk colliders at once; the limits are practical cost, not a one-heightfield restriction.
- The current recommended workflow for terrain texturing is to author the desired tiling in mesh UVs, then have the terrain cooker preserve that UV density. Per-texture UV transforms can be added later, but they are not required for the terrain MVP.
- Resource Browser note: the current tree color reflects an asset's own load state, while terrain cases can have unloaded package roots with loaded contained assets below them. That is technically correct but visually ambiguous. A better UI model is to keep text color for self state and add a separate contained-subtree activity badge or tooltip, rather than overloading one color to mean both self residency and contained asset activity.
