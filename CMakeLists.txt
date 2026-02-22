cmake_minimum_required(VERSION 3.12)

#
# Project
#
project(DA376B_Labs
  VERSION 1.0
  LANGUAGES CXX
)

message(STATUS "Starting CMake configuration for DA376B_Labs project")

#
# CPP
#
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

#
# Compiler
#
if(APPLE)
    add_compile_definitions(GL_SILENCE_DEPRECATION=1)
    message(STATUS "Apple platform detected, setting GL_SILENCE_DEPRECATION=1")
else()
    add_compile_definitions(GL_SILENCE_DEPRECATION=0)
    message(STATUS "Non-Apple platform detected")
endif()
add_compile_definitions(ENTT_USE_ATOMIC)
# add_compile_definitions(EENG_ENTITY_DESTROY_IDEMPOTENT)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
# ^ consider target_include_directories
# skip all these ->
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/util)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/engine)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/engine/engineapi)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/engine/util)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/engine/meta)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/engine/serializers)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/engine/assets)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/engine/assets/importers)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/engine/input)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/editor)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/editor/gui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/app)

#
# OpenGL
#
if(APPLE)
    # On macOS, just use the framework directly
    set(OPENGL_LIBRARIES "-framework OpenGL")
    message(STATUS "Using OpenGL framework on macOS: ${OPENGL_LIBRARIES}")
else()
    find_package(OpenGL REQUIRED)
    message(STATUS "Found OpenGL")
    message(STATUS "OpenGL include dir: ${OPENGL_INCLUDE_DIR}")
    message(STATUS "OpenGL libraries: ${OPENGL_LIBRARIES}")
endif()

#
# Lua
#
# find_package(Lua REQUIRED)
# message(STATUS "Lua found")
# message(STATUS "Found Lua: ${LUA_VERSION_STRING}")
# message(STATUS "Lua include dir: ${LUA_INCLUDE_DIR}")
# message(STATUS "Lua library: ${LUA_LIBRARY}")

#
# Git
#
find_package (Git REQUIRED)
message(STATUS "Found Git: ${GIT_EXECUTABLE}")

include(FetchContent)

#
# Fetch SDL
#
message(STATUS "Fetching SDL2")
FetchContent_Declare(
    sdl2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.2 # April 2nd 2024
)
FetchContent_GetProperties(sdl2)
if(NOT sdl2_POPULATED)
    FetchContent_Populate(sdl2)
    add_subdirectory(${sdl2_SOURCE_DIR} ${sdl2_BINARY_DIR})
    message(STATUS "SDL2 populated and added to the build")
endif()
include_directories(${sdl2_SOURCE_DIR}/include)

#
# Fetch glew
# https://github.com/Perlmint/glew-cmake/blob/master/glew-cmake/fetch-content.cmake
#
message(STATUS "Fetching GLEW")
include(FetchContent)
FetchContent_Declare(
  glew
  GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
  GIT_TAG        origin/master
)
FetchContent_GetProperties(glew)
if(NOT glew_POPULATED)
  FetchContent_Populate(glew)
  add_subdirectory(${glew_SOURCE_DIR} ${glew_BINARY_DIR} EXCLUDE_FROM_ALL)
  message(STATUS "GLEW populated and added to the build")
endif()

#
# Fetch Assimp
#
message(STATUS "Fetching Assimp")
if (APPLE)
    set(ASSIMP_BUILD_ZLIB OFF CACHE BOOL "Disable Assimp's zlib on macOS" FORCE)
    find_package(ZLIB REQUIRED) # Use macOS system zlib
else()
    set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "Use bundled zlib on Windows" FORCE)
endif()
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Build Assimp tools" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Build Assimp tests" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "Install Assimp" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "Inject debug postfix" FORCE)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assimp/CMakeLists.txt")
    add_subdirectory("assimp")
    message(STATUS "Assimp added from local source")
else()
    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v6.0.1
        GIT_SHALLOW ON
    )
    FetchContent_MakeAvailable(assimp)
    message(STATUS "Assimp fetched and made available")
endif()
include_directories(${assimp_SOURCE_DIR})

#
# Fetch STB
#
message(STATUS "Fetching STB")
FetchContent_Declare(
	stb
	GIT_REPOSITORY https://github.com/nothings/stb.git
)
FetchContent_MakeAvailable(stb)
include_directories(${stb_SOURCE_DIR})
message(STATUS "STB fetched")

#
# Fetch GLM 
# https://github.com/g-truc/glm
#
message(STATUS "Fetching GLM")
# set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
# set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 	bf71a834948186f4097caa076cd2663c69a10e1e #refs/tags/1.0.1
)
FetchContent_MakeAvailable(glm)
message(STATUS "GLM fetched and made available")

#
# Fetch enTT (header-only)
#
message(STATUS "Fetching enTT")
FetchContent_Declare(
  entt
  GIT_REPOSITORY https://github.com/skypjack/entt.git
  GIT_TAG        v3.15.0
)
FetchContent_MakeAvailable(entt)
include_directories(${entt_SOURCE_DIR}/src) 
message(STATUS "enTT fetched and included")

#
# Fetch Bullet Physics
#
message(STATUS "Fetching Bullet Physics")
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "Disable Bullet demos" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "Disable Bullet CPU demos" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "Disable Bullet extras" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "Disable Bullet unit tests" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "Disable Bullet OpenGL demos" FORCE)
set(USE_DOUBLE_PRECISION OFF CACHE BOOL "Use single-precision Bullet math" FORCE)
FetchContent_Declare(
    bullet
    GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
    GIT_TAG        3.25 # Apr 25, 2022
)
FetchContent_MakeAvailable(bullet)
include_directories(${bullet_SOURCE_DIR}/src)
message(STATUS "Bullet Physics fetched and included")

#
# Fetch Dear ImGui
#
message(STATUS "Fetching Dear ImGui")
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.89.9-docking)
FetchContent_MakeAvailable(imgui)
include_directories(${imgui_SOURCE_DIR})
include_directories(${imgui_SOURCE_DIR}/backends)
message(STATUS "Dear ImGui fetched and included")

#
# Fetch ImGuiFileDialog
#
message(STATUS "Fetching ImGuiFileDialog")
FetchContent_Declare(imgui_file_dialog
    GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
    GIT_TAG master
)
FetchContent_GetProperties(imgui_file_dialog)
if(NOT imgui_file_dialog_POPULATED)
    FetchContent_Populate(imgui_file_dialog)
    message(STATUS "ImGuiFileDialog populated")
endif()
include_directories(${imgui_file_dialog_SOURCE_DIR})

#
# Fetch nlohmann
#
# Fetch nlohmann/json library
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

#
# Targets
#

# Legacy game
message(STATUS "Creating executable target for LegacyGame")
add_executable(LegacyGame
    projects/legacy_game/main.cpp
    projects/legacy_game/Game.cpp
    projects/legacy_game/LegacyGameMetaReg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/ecs/ThirdPersonCameraSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/ecs/FirstPersonCameraSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/ecs/EditorCameraSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/projects/legacy_game/ecs/systems/VehicleRig1ControlSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/projects/legacy_game/ecs/systems/PistonInputSystem.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp # ImGui widgets for std::string
    ${imgui_file_dialog_SOURCE_DIR}/ImGuiFileDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/EngineOverlayGui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/EngineFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/GLDebugMessageCallback.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/glmcommon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/input/InputManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/RenderableMesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ForwardRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ShapeRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ImGuiBackendSDL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/BatchRegistry.cpp # where?
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/engineapi/EngineContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/assets/Storage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/assets/AssetIndex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/assets/ResourceManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/assets/AnimationGraphRuntime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/assets/AnimationGraphValidation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/assets/importers/AssimpImporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/assets/importers/MockImporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/gpu/GpuAssetOps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/Entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/EntityManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/EntityBatchPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/SceneGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/AnimationGraphComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/TransformComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/HeaderComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/StickyNoteComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/VehicleRig1Builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/PistonRigBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/CoreComponents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/RenderSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/projects/legacy_game/ecs/systems/MannequinPlayerControllerSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/AnimationSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/AnimationGraphSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/TransformSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/TransformSocketSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/ConstraintSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/PistonConstraintDriveSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/PistonAnimSyncSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/TwoAnchorAlignSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/MousePointConstraintSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/DebugRenderSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/PhysicsSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/SpringDamperSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/ScriptSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/ecs/systems/StickyNoteSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/GuiCommands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/GuiCommands_Entities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/GuiCommands_Components.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/GuiCommands_Assets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/BatchCommands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/AssignFieldCommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/CommandAsync.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/CommandAssetHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/CommandBatchHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/CommandContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/CommandEntityHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/CommandSnapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/CommandSanityChecks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/EditorActions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/content_generators/MannequinGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/content_generators/PistonGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/MetaFieldAssign.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/GLMInspect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/ProjectConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/ProjectBootstrap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/EditorRuntime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/TransformGizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/ecs/EditorBootstrapSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/ecs/TransformGizmoSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/util/Profiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/util/ThreadPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/meta/AssetMetaReg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/meta/ComponentMetaReg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/meta/GLMMetaReg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/meta/MetaSerialize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/meta/MetaInspect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/meta/MetaClone.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/serializers/GuidSerialize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/serializers/GLMSerialize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/serializers/ModelDataAssetSerialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/serializers/AnimationGraphSerialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/gui/GuiManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/gui/ProfilerWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/gui/RigSpawnerWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/LogManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/LogGlobals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/physics/MassProperties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/physics/PhysicsWorld.cpp
    )

set_target_properties(LegacyGame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/legacy_game"
)

# GLEW headers and library
# We need to prevent glew from using '-framework AGL', which was removed 
# in macOS Tahoe. The current fix is to 1) explicitly include glew headers,
# and 2) link to the glew library file directly, rather than the glew target.
# Should work equally well on all platforms.
# https://developer.apple.com/documentation/macos-release-notes/macos-26-release-notes
target_include_directories(LegacyGame PRIVATE
    ${glew_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/projects/legacy_game
)

target_link_libraries(LegacyGame PRIVATE 
    SDL2 
    assimp  
    $<TARGET_FILE:libglew_static> # link to library file directly to avoid AGL framework # libglew_static
    glm::glm 
    nlohmann_json::nlohmann_json
    BulletDynamics BulletCollision LinearMath # Bullet Physics libs
    ${OPENGL_LIBRARIES}
    )
if (TARGET ZLIB::ZLIB)
    target_link_libraries(LegacyGame PRIVATE ZLIB::ZLIB)
endif()

target_compile_definitions(LegacyGame PRIVATE
    META_OUTPUT_DIR="${CMAKE_BINARY_DIR}/legacy_game/meta"
    EENG_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
# target_compile_definitions(LegacyGame PRIVATE VERBOSE_LOGGING)

# Post-build commands
add_custom_command(TARGET LegacyGame POST_BUILD
    # Copy SDL2 DLL to the build directory (for Windows)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2>
        $<TARGET_FILE_DIR:LegacyGame>
    # Copy assimp DLL to the build directory (for Windows)
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "$<TARGET_FILE_DIR:assimp>"
        $<TARGET_FILE_DIR:LegacyGame>
)
message(STATUS "Post-build commands to copy SDL2 and Assimp DLLs to LegacyGame folder")

# Set the working directory for the Visual Studio debugger
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set_property(TARGET LegacyGame PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    message(STATUS "Set Visual Studio debugger working directory")
endif()

# Module2 ...

# Set the startup project for Visual Studio
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT LegacyGame)
    message(STATUS "Set Visual Studio startup project to LegacyGame")
endif()

# For 'make install'
#install(TARGETS LegacyGame Module2)
# message(STATUS "Install targets

enable_testing()
add_subdirectory(tests)
