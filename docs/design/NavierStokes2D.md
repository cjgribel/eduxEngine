# 2D Navier-Stokes Frame Sketch

## Intent

This is a "fun sandbox" feature, so the safest shape is:

- keep it out of `src/engine`
- keep it project-owned
- make it opt-in per entity
- allow the frame contents to come from JSON

The engine already supports project-specific components and systems layered on top of the shared runtime pipeline. That makes this a good fit for a small project module rather than a new core engine feature.

The companion math note is in `docs/design/NavierStokesMath.md`.

## Recommended MVP

Use a project-local component and a project-local system first.

- Component: describes where the simulation frame lives in the world and how it should render.
- System: owns the runtime grid buffers and advances the solver each frame.
- Config: start with a plain JSON file path in the component, not a first-class engine asset.

That last point is important: a raw JSON path keeps the first version isolated from the shared asset registry. If this turns out to be genuinely fun and sticky, it can later graduate to a proper `FluidFrameAsset`.

## Suggested Location

For a minimal, non-core version:

- `projects/reference_game/ecs/FluidFrameComponent.hpp`
- `projects/reference_game/ecs/systems/FluidFrameSystem.hpp`
- `projects/reference_game/ecs/systems/FluidFrameSystem.cpp`
- `projects/reference_game/FluidSandboxMetaReg.hpp`
- `projects/reference_game/FluidSandboxMetaReg.cpp`
- `projects/reference_game/data/fluids/*.json`

If you want it available in the current sample game instead, swap `reference_game` for `legacy_game`.

## Component Sketch

The component should stay small and serializable. The heavy runtime data should live in the system.

```cpp
namespace eeng::reference_game
{
    struct FluidFrameComponent
    {
        std::string name = "Fluid Frame";
        bool enabled = true;

        // Local 2D frame inside the entity's XY plane.
        glm::vec2 frame_size = { 4.0f, 2.0f };
        glm::ivec2 resolution = { 128, 64 };

        float simulation_rate = 1.0f;
        int substeps = 1;

        // MVP: direct JSON file path, project-relative.
        std::string config_path = "projects/reference_game/data/fluids/default_frame.json";

        // Rendering/debug
        bool render_density = true;
        bool render_velocity = false;
        float density_gain = 1.0f;
        float velocity_glyph_scale = 0.25f;
        std::uint32_t tint_abgr = 0xffffffffu;
    };
}
```

Notes:

- `TransformComponent` defines placement in 3D.
- The frame is still 2D; the entity transform just tells you where that plane sits in world space.
- Runtime arrays do not belong in the component because they would bloat serialization and the inspector.

## System Sketch

The system should own per-entity runtime state keyed by `entt::entity`, similar to how `ParticleSystem` owns emitter runtime.

```cpp
namespace eeng::reference_game
{
    class FluidFrameSystem
    {
    public:
        void update(entt::registry& registry, eeng::EngineContext& ctx, float dt);
        void render_overlay(
            entt::registry& registry,
            eeng::EngineContext& ctx,
            ShapeRendering::ShapeRenderer& renderer,
            const glm::mat4& proj_view) const;
        void clear();

    private:
        struct Runtime
        {
            glm::ivec2 resolution{ 0, 0 };
            float cell_size = 1.0f;

            std::vector<float> vel_x;
            std::vector<float> vel_y;
            std::vector<float> vel_x_tmp;
            std::vector<float> vel_y_tmp;
            std::vector<float> pressure;
            std::vector<float> divergence;
            std::vector<float> density;
            std::vector<float> density_tmp;
            std::vector<std::uint8_t> solid_mask;

            std::filesystem::path loaded_config_path;
            std::filesystem::file_time_type loaded_write_time{};
        };

        std::unordered_map<entt::entity, Runtime> runtimes_;
    };
}
```

This keeps the component clean while letting the system cache grids, masks, and hot-reloaded config state.

## Solver Sketch

For a toy feature, do not start with "full Navier-Stokes" in the CFD sense. Start with a stable-fluids style loop:

1. Apply emitters and boundary-driven injections.
2. Advect velocity with semi-Lagrangian backtracing.
3. Diffuse velocity optionally.
4. Solve pressure with Jacobi or Gauss-Seidel iterations.
5. Subtract pressure gradient to project to divergence-free velocity.
6. Advect dye/density for visualization.
7. Re-apply solid and edge boundary conditions.

That gives you the right feel without turning this into a research project.

## JSON Sketch

This is a reasonable first-pass JSON shape:

```json
{
  "version": 1,
  "simulation": {
    "cell_size": 0.05,
    "viscosity": 0.0005,
    "velocity_damping": 0.001,
    "density_damping": 0.02,
    "pressure_iterations": 32
  },
  "boundaries": {
    "left":   { "velocity": { "type": "dirichlet", "value": [1.0, 0.0] } },
    "right":  { "velocity": { "type": "neumann",   "value": [0.0, 0.0] } },
    "top":    { "velocity": { "type": "dirichlet", "value": [0.0, 0.0] } },
    "bottom": { "velocity": { "type": "dirichlet", "value": [0.0, 0.0] } }
  },
  "obstacles": [
    {
      "shape": "box",
      "min_uv": [0.35, 0.35],
      "max_uv": [0.45, 0.65],
      "boundary": "no_slip"
    },
    {
      "shape": "circle",
      "center_uv": [0.70, 0.45],
      "radius_uv": 0.08,
      "boundary": "no_slip"
    }
  ],
  "emitters": [
    {
      "kind": "density",
      "shape": "circle",
      "center_uv": [0.12, 0.50],
      "radius_uv": 0.04,
      "amount": 5.0
    },
    {
      "kind": "velocity",
      "shape": "circle",
      "center_uv": [0.12, 0.50],
      "radius_uv": 0.06,
      "value": [2.0, 0.0]
    }
  ]
}
```

## Boundary Model

Keep the boundary vocabulary intentionally small:

- `dirichlet`: hard-set the field value
- `neumann`: hard-set the outward normal derivative
- `no_slip`: obstacle velocity becomes zero
- `free_slip`: obstacle normal velocity becomes zero, tangential preserved

That is enough to make the frame edges and embedded geometry interesting.

## Runtime Behavior

Each `FluidFrameComponent` entity becomes an independent little simulation box.

- Different entities can run different resolutions.
- Different entities can point at different JSON configs.
- The system rebuilds masks when resolution or config changes.
- The system hot-reloads the JSON when the file timestamp changes.

This keeps the feature playful: you can drop several fluid paintings into a scene without teaching the engine about "global fluid worlds".

## Rendering Options

### MVP rendering

Render it as an overlay/debug surface:

- sample density at cell centers
- draw a small tinted quad per cell, or
- draw a wireframe box plus velocity glyphs with `ShapeRenderer`

This is ugly but fast to build and great for debugging.

### Better looking pass

If the toy survives first contact:

- upload density to a texture each frame
- draw one camera-facing or transform-aligned quad
- optionally derive normal-ish lighting from density gradients

That still does not need core engine changes; it can remain a project-owned renderer.

## Integration Points

Minimal integration in a game/runtime looks like this:

1. Register project-local meta for `FluidFrameComponent`.
2. Create a `FluidFrameSystem` member in the game runtime.
3. Call `fluid_frame_system_.update(...)` in `update_edit()` and `update_play()`.
4. Call `fluid_frame_system_.render_overlay(...)` from the project's overlay/render path.

That is the same layering idea already used for project-specific gameplay systems.

## If You Want a Real Asset Later

If the JSON path approach becomes annoying, promote it to a project asset type:

```cpp
struct FluidFrameAsset
{
    int version = 1;
    FluidSimDesc simulation{};
    std::vector<FluidObstacleDesc> obstacles;
    std::vector<FluidEmitterDesc> emitters;
    FluidBoundaryDesc boundaries{};
};
```

Then:

- add meta registration in the project
- add serialization/deserialization in the project
- switch the component from `std::string config_path` to `AssetRef<FluidFrameAsset>`

I would not do that first.

## Why This Stays Out Of Core

This feature does not need:

- core ECS changes
- core runtime pipeline changes
- core renderer changes
- core physics changes

It only needs:

- one project component
- one project system
- one project meta registration file
- one call site in the chosen game runtime

That matches your goal of "component + system, no effect on the core engine".

## Nice Extra Bits

If you want the toy to feel more alive later, these are the best upgrades:

- mouse poke force in editor
- animated emitters
- temperature/buoyancy term
- vorticity confinement
- obstacle entities that stamp themselves into the mask
- a color ramp texture for dye visualization

## Recommended First Slice

If I were building this in this repo, I would do it in this order:

1. Project-local `FluidFrameComponent` with `config_path`.
2. Project-local `FluidFrameSystem` with CPU grids and density rendering.
3. JSON parsing plus hot reload.
4. Edge boundaries plus box/circle obstacles.
5. Velocity glyph debug draw.
6. Optional texture-backed rendering.

That gives you the fun part quickly, while keeping the blast radius tiny.
