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
- scale terrain at cook time
- control chunking through chunk counts

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
- `TerrainAsset` stores chunk collection metadata, chunk payload refs, batch ids, and chunk bounds
- `TerrainRootComponent` is minimal and does not drive chunk spawning
- the old runtime-spawn terrain path is no longer the intended path

## Limitations
- chunk load state is still manual; no streaming or smart residency logic yet
- generated terrain chunk batches are not fully protected as read-only yet
- generated/read-only batch metadata is still missing
- command filtering for generated terrain chunk entities is still missing
- material transfer from source terrain to cooked chunk render assets is still missing
- generated terrain assets and batches are still somewhat noisy in the Resource Browser
- `HeaderComponent.chunk_tag` is an old remnant and should not be treated as the long-term terrain metadata mechanism

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

## Near-Term Plan
Immediate next steps:
- add batch metadata for generated/read-only terrain chunk batches
- add a `Clear Cooked Terrain` action on `TerrainRecipeAsset`

Short follow-up items:
- expose normal batch delete in the GUI
- decide how generated terrain batches should appear in that GUI
- review and clean up the `IBatchRegistry` / `BatchRegistry` split
- add command filtering so generated terrain chunk entities cannot be edited as normal scene content
- add material transfer from the source terrain submesh to cooked chunk render assets

## `IBatchRegistry` / `BatchRegistry`
Current direction:
- `IBatchRegistry` should hold runtime-facing residency operations
- `BatchRegistry` should keep tooling/editor-oriented batch authoring operations

Likely move into `IBatchRegistry`:
- `queue_load(...)`
- `queue_unload(...)`
- `queue_unload_all_async(...)`
- `is_batch_loaded(...)`
- `try_get_loaded_batch_for_entity(...)`

Likely keep concrete/tooling-only in `BatchRegistry`:
- batch index loading/saving
- batch creation/deletion/restoration
- generated batch upsert
- snapshot/save helpers
- attach/detach/spawn/create/destroy entity helpers

## Notes
- Terrain chunks and terrain chunk batches should be as large as practical.
- Bullet can handle multiple loaded heightfield chunk colliders at once; the limits are practical cost, not a one-heightfield restriction.
