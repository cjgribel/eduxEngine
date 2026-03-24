# Code Style

## Goal
New code in this repository should be understandable, maintainable, and consistent.

This document starts with comment/style expectations, and can later be extended with broader code-style rules.

## Comments
### Goal
Comments should explain intent, invariants, ownership, and non-obvious behavior.

### Rules
1. Comment all new non-trivial code.
This especially applies to:
- asset cooking/import paths
- async loading/unloading
- edit/play boundary handling
- runtime-generated entities
- physics integration
- serialization/versioning behavior

2. Use Doxygen for major public types and functions.
At minimum, document:
- what the type/function is for
- important behavioral constraints
- important parameters/return values when they are not obvious

3. Prefer intent comments over narration comments.
Good comments explain:
- why a path exists
- what invariant is being protected
- what assumption the code relies on

Avoid comments that only restate the next line mechanically.

4. Comment transitional or defensive code explicitly.
If code is a workaround, safety valve, fallback, or compatibility path, say so directly.

5. Keep ownership and lifetime visible.
Whenever code stores raw pointers, handles, async futures, or transient entities, document:
- who owns them
- what must outlive what
- when cleanup occurs

6. Comments explain the code; they do not address the user.
Never write comments that talk to the user or refer to the user in first, second, or third person.

Examples to avoid:
- "so you can ..."
- "this lets the user ..."

Prefer:
- "Keeps render and collision aligned."
- "Ensures stale chunk entities are removed on world switch."
- "Stores runtime-owned height samples so Bullet pointers remain valid."

7. Keep comments maintained with code.
If code changes enough that a comment is no longer accurate, update or remove the comment in the same change.

8. Call out notable risks or design debt directly.
If code contains a compromise, limitation, fragile assumption, workaround, or other maintainability risk, comment it clearly and concisely.

Prefer neutral, specific wording over vague "code smell" language.

Examples:
- "Temporary compatibility path until chunk-count-based recipes replace chunk-cell settings."
- "Defensive registry-swap guard because this system instance spans both edit and play worlds."
- "Fallback lookup by GUID to tolerate unbound `AssetRef` handles in lightweight runtime/editor flows."

## Terrain-Specific Expectations
Terrain code should clearly document:
- difference between `TerrainRecipeAsset` and `TerrainAsset`
- what is editor-only vs runtime
- how chunk residency is decided
- how cooked render and cooked collision are kept in sync
- any assumptions about heightfield-like source meshes
