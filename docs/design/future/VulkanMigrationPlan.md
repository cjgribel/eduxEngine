# Vulkan Migration Plan

A low-risk plan is to add Vulkan as a second backend first, not replace OpenGL outright. In this repo, the seam should start above window/context setup and below gameplay/editor code, because the current coupling is concentrated in [Engine.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/Engine.cpp#L546), [ImGuiBackendSDL.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ImGuiBackendSDL.cpp#L1), [RuntimePipeline.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/RuntimePipeline.hpp#L52), [RenderSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/RenderSystem.cpp#L55), [ParticleRenderSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/ParticleRenderSystem.cpp#L98), and [GpuAssetOps.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/gpu/GpuAssetOps.cpp#L126). The rest of the engine can mostly stay as-is.

## Phased Plan

### 1. Backend seam and bring-up

Create a small `IRenderBackend` layer for device/swapchain/frame lifecycle, shader/pipeline creation, buffer/image upload, and draw submission. Keep OpenGL as the default path at first. Replace the GL-specific startup in [Engine.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/Engine.cpp#L546) and the OpenGL-only ImGui bridge in [ImGuiBackendSDL.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ImGuiBackendSDL.cpp#L11) with backend-selected code. In CMake, stop hard-wiring GLEW and `imgui_impl_opengl3` as mandatory in [CMakeLists.txt](/Users/ag1498/GitHub/eduxEngine/CMakeLists.txt#L99) and [CMakeLists.txt](/Users/ag1498/GitHub/eduxEngine/CMakeLists.txt#L258).

### 2. Make GPU assets backend-neutral

Right now your runtime assets literally store GL handles in [ModelAssets.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/assets/types/ModelAssets.hpp#L58). That is the most important structural blocker. Change `GpuModelAsset` and `GpuTextureAsset` to hold backend-owned resource objects or opaque IDs instead of `vao`, `vbo_*`, `ibo`, `gl_id`. Then move the GL upload code in [GpuAssetOps.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/gpu/GpuAssetOps.cpp#L157) behind backend upload functions. This gives you one asset pipeline feeding both GL and Vulkan.

### 3. Port shaders and binding model

Your shaders are currently GLSL 4.10 source strings compiled at runtime in [ShaderLoader.h](/Users/ag1498/GitHub/eduxEngine/src/engine/ShaderLoader.h#L39), and the render code depends on many `glUniform*` calls. For Vulkan, move to GLSL 4.50 + SPIR-V, define descriptor sets for camera/material/texture data, and use push constants or small dynamic uniform buffers for per-draw state. The skinned mesh path in [phong_vert.glsl](/Users/ag1498/GitHub/eduxEngine/shaders/phong_vert.glsl) is a good candidate to move bone matrices into a buffer-backed binding instead of a fixed uniform array.

### 4. Port passes in order of value

First port the main mesh pass in [RenderSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/RenderSystem.cpp#L81). Then do particles in [ParticleRenderSystem.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ecs/systems/ParticleRenderSystem.cpp#L176). Then debug/overlay rendering in [ShapeRenderer.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ShapeRenderer.cpp) and ImGui. Leave [ForwardRenderer.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/ForwardRenderer.cpp#L27) as legacy or remove it later if it is no longer on the active path.

### 5. Add compute/storage work after the graphics path is stable

Once Vulkan rendering is up, add a compute pipeline for whichever system actually needs it first. Good candidates here would be particles, culling, or future simulation. Don’t start the migration by chasing compute; start by getting one Vulkan frame on screen with assets and UI.

## What You’d Gain

Yes, this would unlock the equivalent of SSBOs and compute shaders on Mac, via Vulkan running through MoltenVK. That means storage buffers, compute pipelines, descriptor-based resource binding, and a much more modern GPU model than macOS OpenGL 4.1 gives you. The caveat is that Mac Vulkan is a portability path, not native Vulkan, so you should design for feature checks and avoid assuming every desktop Vulkan feature is available everywhere.

## Rough Estimate

For one experienced C++ graphics engineer, budget about 8-12 weeks for a solid dual-backend migration to “mesh rendering + textures + ImGui + particles + debug draw,” and longer if this is also the team’s first Vulkan backend. A reasonable milestone breakdown is:

- 1-2 weeks: backend seam, Vulkan window/swapchain/device, clear screen
- 2-3 weeks: mesh pass, textures, materials, shader toolchain
- 2-3 weeks: GPU asset refactor and async upload integration
- 1-2 weeks: ImGui, particles, debug shapes
- 1-2 weeks: Mac stabilization, portability-subset fixes, validation/perf

## Direct Vulkan Companion Notes

These are directly tied to a Vulkan migration or to backend work that Vulkan would force.

- `RenderBackendArchitecture.md`
  Define the boundary between engine/game code and GL/Vulkan/Metal backends. This is the most important companion note for a Vulkan migration.

- `ShaderPipelinePlan.md`
  Cover GLSL-to-SPIR-V, shader reflection, descriptor set conventions, hot reload, and whether to keep one shader source strategy for both GL and Vulkan.

- `MacGraphicsStrategy.md`
  Compare these paths explicitly:
  `Keep GL 4.1 on Mac`
  `Vulkan via MoltenVK`
  `Native Metal backend`

- `AssetCookingPipeline.md`
  Describe an offline asset pipeline for precompiled shaders, compressed textures, mesh optimization, tangent generation, baked material layouts, and cached GPU-ready blobs.

- `PerformanceInstrumentationPlan.md`
  Track GPU timestamps, CPU/GPU frame breakdowns, VRAM budgeting, pipeline statistics, and regression benchmarks.

## Adjacent Graphics Roadmap Notes

These are not Vulkan-only, but they become more practical or more valuable once the renderer is modernized.

- `GPUDrivenRoadmap.md`
  Capture ideas like indirect drawing, GPU culling, instance batching, and later GPU-driven submission strategies where supported.

- `ComputeOpportunities.md`
  List systems that could benefit from compute work: particles, boids/flocking, cloth, broad-phase physics, skinning, light culling, terrain updates, and fluid experiments.

- `RenderGraphIdeas.md`
  Note possible render-graph evolution for managing passes, dependencies, and transient render targets as the renderer grows.

- `EditorRuntimeSeparation.md`
  Describe how to better isolate editor overlays, gizmos, and debug rendering from the runtime renderer so backend work stays contained.

- `ExperimentalFeatures.md`
  Keep a scratchpad for ideas that Vulkan or a more modern backend would make easier to explore, such as GPU particles with collisions, procedural destruction, SDF tooling, fluid/smoke experiments, or advanced debug visualizations.
