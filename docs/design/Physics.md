# Physics

## Overview
- Bullet integration with ECS (RigidBody + Collider + Transform).
- Colliders can be offset; COM/principal axes are computed from colliders while the Transform stays as the authoring pivot.
- Auto mass uses density; auto inertia is diagonalized in the principal-axes frame and cached in the component for inspection.
- SpringDamper anchors support Transform space or Body (COM/principal axes) space.
- Debug overlay draws COM, principal axes, offset line, and RB labels at COM.
- Edit-mode updates rebuild mass properties for authoring feedback only (no physics stepping).

## Mass Properties (Current Setup)
- Per-collider mass properties use analytic formulas (box/sphere/capsule) and convex hull tetrahedralization.
- Aggregate COM/inertia via parallel axis theorem, then diagonalize to get principal axes + diagonal inertia.
- COM/principal axes are applied as a body-local offset; compound child transforms are recentered into that frame.
- Inertia scales linearly with mass; `density` drives auto mass.
- Convex hull falls back to box mass properties if mesh data is unavailable (warns on rebuild).
- Computed values are cached on `RigidBodyComponent` for editor visibility:
  - `mass`, `inertia` (diagonal), `com_local_position`, `com_local_rotation`.

### Mass Properties Data Flow
- Compute per-collider mass properties at unit density (analytic or convex hull tetrahedralization).
- Apply each collider's local offset into entity-pivot space.
- Aggregate total mass/COM/inertia in entity-pivot space (parallel axis theorem).
- Diagonalize inertia to get principal axes; store as `com_local_position` + `com_local_rotation`.
- Build Bullet body in that principal-axes frame; sync to/from Transform via `com_local` and its inverse.
- Cache computed `mass`/`inertia`/`com_local_*` for inspector + debug drawing.

## TODO

### Now
- [x] Forces applied before physics step.
- [x] COM/principal axes computed from colliders; body offset handled in sync.
- [x] Debug overlay for COM/principal axes/offset line and RB labels at COM.
- [x] Edit-mode rebuild of COM/inertia for immediate authoring feedback.
- [x] RigidBody inspector shows computed mass/inertia/COM (read-only when auto).
- [x] SpringDamper anchors support Transform or Body (COM/principal axes) space.
- [ ] Validate convex hull mass properties with real assets (compare COM/inertia against expectations).
- [ ] Consider warn-once policy for convex hull fallback logging.

### Next
- [ ] Ragdoll prototype: mapping component (bone index + body entity + local offsets).
- [ ] Blend window for animation-driven -> physics-driven ragdoll transitions.
- [ ] Per-collider density/weighting (instead of uniform density).

### Later
- [ ] (Optimization) BodyRuntime allocations -> RigidBodyRuntimePool
- [ ] Mass properties for non-convex meshes or convex decomposition.
- [ ] Revisit non-uniform scale handling for spheres/capsules (currently uses uniform scale).

## Ragdolls (Initial Notes)
- One entity per rigid body (per limb); constraints link limbs.
- Animation mode: bones drive kinematic bodies (Transform from pose).
- Ragdoll mode: physics drives bodies; pose is written back to bones.
- Needs a mapping component for bone <-> body space and offsets; likely a policy component to store constraint limits/profiles.
- Anchor-space choice should be explicit (bone/Transform vs Body/COM) to avoid hidden offsets.

# Possible simple games & game systems

- [ ] Simple 3D platformer where multiple players can do fun stuff
- [ ] Spring-picker: Game comp/sys which makes it possible to RMB-pick objects and drag them with a spring force. Draw spring using ShapeRenderer helix.
