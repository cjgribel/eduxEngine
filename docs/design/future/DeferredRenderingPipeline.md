# Deferred Rendering Pipeline Proposal

This note outlines a plausible deferred renderer for the current engine as an alternative to the active forward path. The goal is not to replace everything at once, but to introduce a second scene-rendering path that fits the engine we already have and creates room for post effects such as bloom, SSAO, and deferred decals.

It should also be read alongside the Vulkan note in [VulkanMigrationPlan.md](/Users/ag1498/GitHub/eduxEngine/docs/design/future/VulkanMigrationPlan.md). If Vulkan lands before deferred rendering, the pass structure proposed here should still hold. What should change is mostly the backend seam, resource ownership model, and shader/binding strategy.

## Current Shape

Today the engine starts each frame by clearing the default framebuffer in [Engine.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/Engine.cpp#L861), then hands scene rendering to the hosted runtime through `render_frame()` in [IGameRuntime.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/IGameRuntime.hpp#L86). The active ECS mesh path lives in [RuntimePipeline.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/RuntimePipeline.hpp#L206), which delegates opaque scene draws to [RenderSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/RenderSystem.cpp#L81) and particles to [ParticleRenderSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/ParticleRenderSystem.cpp#L185). Debug shapes are flushed later by the engine in [Engine.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/Engine.cpp#L421).

That render path is simple and workable, but it has three structural limits:

- lighting is evaluated per draw call with a small uniform set rather than from a shared scene-light buffer
- the scene renders directly to the default framebuffer, so there is no natural place for HDR composition or post-processing
- GPU assets are still explicitly OpenGL-flavored, exposing `vao`, `ibo`, and `gl_id` in [ModelAssets.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/assets/types/ModelAssets.hpp#L63) and populated in [GpuAssetOps.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/gpu/GpuAssetOps.cpp#L126)

Those constraints matter because bloom, SSAO, decals, light volumes, and similar effects all work best when the renderer can:

- separate geometry capture from lighting
- preserve depth and material buffers between passes
- light into an HDR off-screen target
- run a chain of full-screen post passes before presenting

## Design Goals

- Keep the current forward path as a valid fallback.
- Fit the current OpenGL 4.1 renderer and SDL/ImGui frame loop.
- Reuse the existing ECS-driven runtime flow instead of inventing a new game-facing rendering API all at once.
- Preserve `ShapeRenderer`, editor gizmos, and ImGui with minimal disruption.
- Support opaque deferred shading first, while keeping particles and transparent objects on a forward path.
- Make bloom, SSAO, and decals natural additions rather than one-off hacks.
- Avoid baking new GL-only concepts into the deferred design if they would fight a later Vulkan backend.

## Non-Goals For The First Version

- Full render-graph infrastructure.
- A backend-neutral graphics layer.
- Clustered or compute-driven lighting.
- Full PBR material migration.
- Solving transparency with order-independent techniques.

Those are all reasonable future steps, but they should not be prerequisites for getting a useful deferred renderer running in the current engine.

## Proposed High-Level Frame

The deferred path should be a hybrid pipeline:

1. Build per-frame view/light data from the runtime.
2. Render opaque and alpha-tested meshes into a G-buffer FBO.
3. Run screen-space effects that depend on depth and normals.
4. Accumulate lighting into an HDR scene-color target.
5. Render transparent meshes and particles forward on top of the lit HDR scene.
6. Run post-processing on the HDR scene.
7. Composite the tone-mapped result to the default framebuffer.
8. Flush debug shapes and let the existing ImGui backend render UI.

That keeps the engine-friendly parts of the current flow:

- runtimes still decide which camera/view to publish
- ECS systems still produce the draw data
- particles stay in their own renderer
- overlays still land after the main scene

## Recommended Render Order

### 1. Frame setup

Introduce a per-frame scene view struct owned by the runtime-facing render code:

```cpp
struct SceneView
{
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::mat4 view_proj{1.0f};
    glm::mat4 inv_view{1.0f};
    glm::mat4 inv_proj{1.0f};
    glm::vec3 eye_pos{0.0f};
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    glm::ivec2 window_size{0, 0};
};
```

`RuntimePipeline::render_entities()` currently receives `proj_view`, `light_pos`, `light_color`, and `eye_pos` separately in [RuntimePipeline.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/RuntimePipeline.hpp#L206). A deferred path will need a slightly richer frame description so later passes can reconstruct positions from depth, sample normals, and run SSAO.

### 2. Geometry pass

Bind a dedicated G-buffer framebuffer instead of the default framebuffer and draw all opaque plus alpha-tested meshes once.

Semantic attachments:

- `SceneAlbedo`
- `SceneNormal`
- `SceneMaterial`
- `SceneDepth`
- optional `SceneAux`

Possible OpenGL 4.1 formats for a first implementation:

- `g_albedo`: `GL_RGBA8` or `GL_SRGB8_ALPHA8`
  Stores base color in RGB. Alpha can carry a small material flag field or cutout state if needed.
- `g_normal`: `GL_RGBA16F`
  Stores view-space or world-space normal in RGB. `A` can store a packed smoothness/shininess term.
- `g_material`: `GL_RGBA8` or `GL_RGBA16F`
  Stores specular color and a scalar gloss/shininess remap for the current Phong material model.
- `g_depth`: `GL_DEPTH24_STENCIL8`
  Stores the authoritative scene depth.
- Optional `g_aux`: `GL_R32UI` or `GL_RGBA8`
  Can be used for editor entity IDs, emissive strength, decal mask, or material feature bits.

Important detail: do not store world position in the G-buffer. Reconstruct it from depth using `inv_proj` and `inv_view`. That saves bandwidth and keeps the layout small enough for OpenGL 4.1.

This geometry pass can reuse most of the draw traversal logic already present in [RenderSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/RenderSystem.cpp#L98). The main change is that the shader no longer computes final lighting. It only writes surface data into multiple render targets.

If Vulkan comes first, these attachments should be defined by semantic role rather than hard-wired GL texture state. In other words, name the pass outputs by what they mean, then let OpenGL or Vulkan choose the concrete image format and attachment setup.

### 3. Deferred decal pass

After geometry, but before lighting, render decals against the G-buffer.

A good first implementation for this engine is box-projected deferred decals:

- render a cube volume for each decal
- use depth reconstruction to find the shaded world position
- project that position into decal local space
- blend albedo, normal, and material properties into selected G-buffer targets

This pass should not touch the depth buffer. It only modifies material data for already-visible opaque surfaces. That makes it a natural fit for deferred rendering and much cheaper than re-rendering all affected meshes.

### 4. SSAO pass

Run SSAO from `g_depth` and `g_normal` into a single-channel AO texture, then blur it.

This is straightforward in the current engine because it only requires:

- a small kernel/noise texture
- camera projection data
- full-screen passes

No compute path is required. OpenGL 4.1 fragment passes are enough.

### 5. Lighting pass

Render lighting into an HDR scene-color target, for example `scene_hdr` with format `GL_RGBA16F`.

The initial lighting pass can be:

- one full-screen pass for ambient plus directional lights
- optional light-volume passes for point and spot lights when light counts grow

For the first implementation, a CPU-built light buffer in a UBO is sufficient. The current engine already sends a single light through uniforms in [RuntimePipeline.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/RuntimePipeline.hpp#L217). The deferred version should generalize that to a small `FrameLightingData` block with a fixed-cap light count.

If Vulkan is likely to happen first, do not design this around scattered `glUniform*` calls. Define one explicit frame-lighting payload and one explicit per-material payload now, even if the first OpenGL implementation still uploads them through UBOs. That way the same data model can map cleanly to descriptor-backed buffers later.

Lighting inputs:

- `g_albedo`
- `g_normal`
- `g_material`
- `g_depth`
- `ssao`
- shadow maps later, if and when they exist

Outputs:

- lit HDR scene color
- optional bright-pass mask for bloom, if convenient to emit here

### 6. Forward transparent pass

Keep transparent rendering forward, using the deferred depth buffer.

This pass should cover:

- alpha-blended meshes
- particles from [ParticleRenderSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/ParticleRenderSystem.cpp#L185)
- special materials that are awkward to express in the G-buffer

This is also where soft particles become practical. The particle shader can sample the copied or shared scene depth and fade particles near geometry intersections.

Opaque alpha-cutout materials can stay in the geometry pass, matching the current discard-based behavior in the existing material model. True translucency stays forward.

### 7. Post-processing chain

Once the scene exists in HDR, post effects become a normal stack instead of a special case.

Recommended first post chain:

1. Bright-pass extract from `scene_hdr`
2. Bloom downsample and blur/upsample chain
3. Composite bloom back into the HDR scene
4. Tone mapping and gamma correction
5. Optional FXAA or SMAA later

This should run through a small ping-pong target pair, for example:

- `post_a`
- `post_b`

That is enough for bloom, color grading, vignette, and similar full-screen effects without needing a full render graph yet.

### 8. Final composite, debug shapes, and UI

After tone mapping, draw the final LDR image to the default framebuffer, then keep the existing overlay/UI flow largely intact.

That fits the current frame loop well:

- the engine already owns the final default-framebuffer presentation in [Engine.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/Engine.cpp#L440)
- debug shapes are already flushed after app scene rendering in [Engine.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/Engine.cpp#L421)
- ImGui draw commands are only rasterized at `end_frame()`, so GUI can still end up on top

In other words, the deferred renderer should hand the engine a final tone-mapped scene on the default framebuffer, and `ShapeRenderer` plus ImGui can continue to work with minimal sequencing changes.

## Where This Fits In The Current Architecture

The safest place to introduce this is inside the runtime scene-rendering layer, not in the game code and not directly in the outer engine loop.

Recommended shape:

- keep `IGameRuntime::render_frame()` and `render_scene()` as they are in [IGameRuntime.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/IGameRuntime.hpp#L65)
- keep `RuntimePipeline` as the engine-owned ECS façade in [RuntimePipeline.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/RuntimePipeline.hpp#L57)
- introduce a scene-renderer abstraction underneath `RuntimePipeline`
- if Vulkan migration is plausible in the near term, put a small backend seam under that scene renderer, as described in [VulkanMigrationPlan.md](/Users/ag1498/GitHub/eduxEngine/docs/design/future/VulkanMigrationPlan.md)

For example:

```cpp
enum class SceneRenderPath
{
    Forward,
    Deferred
};

class ISceneRenderer
{
public:
    virtual ~ISceneRenderer() = default;
    virtual void resize(glm::ivec2 size) = 0;
    virtual void render_scene(
        entt::registry& registry,
        EngineContext& ctx,
        const SceneView& view,
        const SceneLighting& lighting) = 0;
};
```

Then:

- `ForwardSceneRenderer` can wrap the existing `RenderSystem`
- `DeferredSceneRenderer` can own G-buffer resources, lighting passes, and post-processing
- `RuntimePipeline` becomes the stable caller-facing façade

That keeps the runtime-facing API reasonably small while allowing the implementation to diverge heavily under the hood.

If Vulkan is likely before deferred work starts, one small adjustment would simplify a lot:

- `RuntimePipeline`
  remains the ECS/game-facing coordinator
- `DeferredRenderer`
  owns pass order, draw-list consumption, and render-feature toggles
- `IRenderBackend`
  owns images, framebuffers/render passes, shader modules, pipelines, and draw submission

That split lets the deferred pass graph survive a backend swap instead of being rewritten with it.

## Suggested Internal Components

### `DeferredRenderer`

Owns:

- semantic pass layout and execution order
- G-buffer, HDR, and post-process frame resources
- full-screen quad helpers
- pass shaders
- resize and destroy logic

If Vulkan comes first, `DeferredRenderer` should describe what passes need, not directly own GL object names. The backend should own the concrete images/views/framebuffers or their Vulkan equivalents.

### `SceneDrawList`

Built once per frame from ECS traversal and consumed by multiple passes.

Each entry should contain:

- mesh handle / geometry resource reference
- world matrix
- submesh range
- material handle
- skinning data pointer or bone range
- render classification:
  `opaque`
  `masked`
  `transparent`
  `decal_receiver`

This is important because the current renderer traverses the registry and draws immediately in [RenderSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/RenderSystem.cpp#L98). Deferred rendering wants traversal and pass execution to be more separate.

If Vulkan is on the horizon, avoid letting `SceneDrawList` hard-code VAOs, sampler bindings, or other GL submission concepts. Treat it as backend-neutral scene data plus resource references.

### `PostProcessStack`

Owns small reusable full-screen passes:

- bloom extract
- blur/downsample/upsample
- SSAO
- tone map
- optional color grading later

This keeps the deferred renderer from turning into one giant source file.

## Material Strategy

The current material model is still Phong-like:

- `Kd`
- `Ks`
- `shininess`
- diffuse / normal / specular / opacity textures in [ModelAssets.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/assets/types/ModelAssets.hpp#L146)

That is enough for a first deferred path.

Suggested first-pass mapping:

- diffuse texture or `Kd` -> `g_albedo`
- normal map or geometric normal -> `g_normal`
- specular texture or `Ks` -> `g_material.rgb`
- `shininess` -> `g_material.a`

Later, if the engine moves toward PBR, the same deferred structure can evolve to:

- albedo
- normal
- roughness
- metallic
- emissive
- AO

without changing the overall pipeline shape.

## GPU Asset Implications

The deferred renderer can be introduced before a full backend-neutral asset refactor, but two changes will make it much easier to live with.

### 1. Separate draw data from backend handles

Right now `GpuModelAsset` and `GpuTextureAsset` expose raw GL object names in [ModelAssets.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/assets/types/ModelAssets.hpp#L63), and those are created directly in [GpuAssetOps.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/gpu/GpuAssetOps.cpp#L157). That is acceptable for a first OpenGL deferred renderer, but the deferred-specific render targets should not be pushed into those asset types. They should live in renderer-owned frame resources instead.

### 2. Preserve the current upload path initially

Do not block deferred rendering on a full asset-system redesign if the work stays OpenGL-first. Reuse the existing GL upload path for meshes and textures first. The render-path swap is already a large enough change on its own.

If Vulkan migration is likely first, flip this priority: do the backend-neutral GPU asset step from [VulkanMigrationPlan.md](/Users/ag1498/GitHub/eduxEngine/docs/design/future/VulkanMigrationPlan.md) before building the deferred renderer. That avoids writing a new deferred path directly on top of `vao` and `gl_id`, then having to peel it back apart immediately.

## Lighting Data Strategy

The current render code effectively assumes a tiny light set passed through uniform calls. Deferred rendering should move toward a per-frame light buffer with a capped count, for example:

```cpp
struct GpuPointLight
{
    glm::vec4 position_radius;
    glm::vec4 color_intensity;
};
```

For OpenGL 4.1, a UBO is a reasonable first home for this data. If that becomes too tight, a texture buffer is the next likely step. There is no need to wait for compute or SSBO work just to support a useful number of lights.

## Transparency, Particles, And Other Hybrid Cases

Deferred rendering should not try to absorb every renderable type.

Recommended split:

- opaque meshes: deferred geometry pass
- alpha-tested meshes: deferred geometry pass with discard
- transparent meshes: forward pass after lighting
- particles: current forward particle path after lighting
- debug shapes and gizmos: after tone mapping on the default framebuffer
- ImGui: unchanged, still rendered at the end of the frame

That hybrid approach is standard for a reason: it gets the main value of deferred rendering without forcing the hardest material classes into the wrong pipeline.

## Resize And Lifetime Rules

All deferred framebuffers and textures should be renderer-owned transient resources, recreated on window resize.

The renderer should watch `window_width` and `window_height` from the same render context currently passed into runtimes by [EditorApp.hpp](/Users/ag1498/GitHub/eduxEngine/src/app/EditorApp.hpp#L150).

Recommended lifecycle:

- `init()`: create shaders, full-screen quad, empty frame resource container
- `resize()`: recreate G-buffer, HDR, and post-process targets
- `render_scene()`: lazily resize if needed, execute passes
- `shutdown()`: delete framebuffer and texture objects

## Important OpenGL 4.1 Tradeoffs

### MSAA

Deferred shading and MSAA are not a great fit in a simple OpenGL 4.1 implementation. The most practical first choice is:

- keep the deferred path non-MSAA
- use post AA later if needed

If the project must keep MSAA, that should be treated as a second-phase enhancement, not a requirement for the first deferred milestone.

### Bandwidth

G-buffer size matters. Keep the first layout lean. Avoid a position buffer and avoid adding attachments until they have a clear use.

### State ownership

Because [Engine.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/Engine.cpp#L861) currently assumes it owns global GL setup for the frame, the deferred renderer should restore any unusual state it changes before handing control back to overlays and ImGui.

## If Vulkan Comes First

If the engine migrates to Vulkan before deferred rendering lands, the recommendation changes from "implement this directly in OpenGL" to "preserve this pass layout, but implement it through a backend seam."

The main design choices that become more important are:

- define pass inputs and outputs semantically
  `SceneAlbedo`, `SceneNormal`, `SceneDepth`, `SceneHDR`, `BloomMipN`
- move new renderer code away from raw GL resource names
  no new public APIs should expose FBO IDs, texture IDs, or VAOs
- describe material, frame, and light bindings as explicit data blocks
  not scattered per-draw uniform calls
- keep draw-list building separate from draw submission
  that split helps both deferred rendering and Vulkan command recording
- prefer a small pass graph now, even if it is hand-authored
  Vulkan benefits from explicit dependencies and resource lifetimes much more than the current GL path does

Concretely, if Vulkan happens first, a better delivery order would be:

1. introduce `IRenderBackend`
2. make GPU mesh and texture assets backend-neutral
3. move scene rendering onto a `ForwardSceneRenderer` plus backend seam
4. add `DeferredSceneRenderer` on top of that seam
5. then add bloom, SSAO, and decals as backend-agnostic passes

That path is probably less total work than building a GL-only deferred pipeline first and then immediately refactoring it around Vulkan concepts.

## Incremental Adoption Plan

### Phase 1: Introduce the alternative path

- Add `SceneRenderPath`.
- Add `DeferredSceneRenderer`.
- Keep the current forward renderer as default.
- Add a runtime or engine flag to switch between paths.

If Vulkan migration happens first, Phase 1 should instead be:

- add `IRenderBackend`
- move `ForwardSceneRenderer` and later `DeferredSceneRenderer` above it
- keep the pass model in this document, but delay concrete deferred implementation until the backend seam exists

### Phase 2: Opaque deferred scene

- Implement G-buffer pass for current `ModelComponent` meshes.
- Implement a basic lighting pass with one directional or point light.
- Present the lit scene to the default framebuffer.

At this point, the deferred path is useful even before SSAO or bloom exist.

### Phase 3: Hybrid completion

- Reattach particles after lighting.
- Reattach transparent materials after lighting.
- Verify `ShapeRenderer`, editor gizmos, and ImGui still layer correctly.

### Phase 4: Post stack

- Add tone mapping.
- Add bloom.
- Add SSAO plus blur.

### Phase 5: Decals and quality passes

- Add deferred decals.
- Add soft particles from depth.
- Add optional entity ID buffer for editor outline/picking integration.

## Recommended First Deliverable

The best first milestone is not “all deferred features.” It is:

- opaque skinned and static meshes render into a G-buffer
- one lighting pass shades them into an HDR scene target
- particles still render forward afterward
- the final scene is tone-mapped back to the default framebuffer
- `ShapeRenderer` and ImGui still work

Once that exists, bloom, SSAO, decals, outlines, and similar effects become straightforward extensions instead of a redesign trigger.

## Short Version

The current engine already has most of the staging points needed for a deferred renderer:

- a stable frame loop in [Engine.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/Engine.cpp#L283)
- a runtime scene/overlay/gui split in [IGameRuntime.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/IGameRuntime.hpp#L86)
- an ECS render façade in [RuntimePipeline.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/RuntimePipeline.hpp#L57)
- separate particle and debug overlay systems

So the plausible design is a hybrid deferred path under `RuntimePipeline`:

- deferred for opaque geometry
- forward for transparent and particles
- HDR scene color plus post-processing
- final composite back to the default framebuffer
- overlays and GUI preserved

That is a natural next step for this engine and a solid foundation for bloom, SSAO, and decals later.
