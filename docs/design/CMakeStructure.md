# CMake Structure

## Current Direction

The repository now uses:

- one root `CMakeLists.txt` for project-wide configuration and dependency setup
- one shared helper module in `cmake/EduxTargetHelpers.cmake`
- one `CMakeLists.txt` for `src/engine`
- one `CMakeLists.txt` for `src/editor`
- one `CMakeLists.txt` per app under `projects/...`

This is an intentional intermediate step between:

- the old single-file, single-target root build
- a cleaner long-term engine/editor dependency split

## Why This Layout

The old root `CMakeLists.txt` owned:

- dependency setup
- compiler settings
- include paths
- the complete `LegacyGame` executable definition

That made new app targets expensive to add because every new executable had to
repeat or duplicate a very large common source list.

The new layout improves that by:

- keeping target ownership near each project
- keeping the root file focused on orchestration
- centralizing shared app wiring in one helper
- compiling shared code once into reusable library targets

## Current Target Shape

At the moment, the build is layered like this:

- `edux_engine`
- `edux_editor`
- project executables created through `edux_add_runtime_target(...)`

`edux_engine` currently carries the large shared runtime/core codebase.  
`edux_editor` links `edux_engine` and adds editor-hosting/UI code.  
Project targets then link `edux_editor`.

The helper currently bundles:

- executable creation
- common app-side dependency wiring
- output directory setup
- post-build DLL copying

Project-local `CMakeLists.txt` files only need to provide:

- target name
- output subdirectory
- project include directory
- project-specific source files

## IDE Workflow

The repository now treats VS Code launch/tasks as part of the multi-target
workflow rather than as a one-game convenience layer.

- keep one target-specific build task per runtime/config
- keep explicit Windows/Linux/macOS launch entries per runtime/config
- keep launch `program` paths aligned with the CMake output subdirectory layout
- keep `preLaunchTask` labels stable once a launch entry depends on them

When adding a new app target under `projects/`, add both:

- the CMake target wiring
- matching VS Code build and launch entries

## Recommended Next Step

The next structural improvement should be:

1. Clean up the boundary between `edux_engine` and `edux_editor`.
2. Move editor-dependent runtime/meta pieces out of `edux_engine` where practical.
3. Simplify tests to link `edux_engine` directly where that reduces duplication.

That would further improve:

- build times
- source ownership clarity
- IDE navigation
- scalability for more apps and tools

## Practical Rule

For now:

- new apps should get their own folder under `projects/`
- each app should have its own `CMakeLists.txt`
- shared runtime/editor wiring should go through the helper module and shared libraries

For later:

- shared code should continue moving toward cleaner library boundaries
