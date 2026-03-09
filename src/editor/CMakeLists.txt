add_library(edux_editor STATIC
    ${imgui_file_dialog_SOURCE_DIR}/ImGuiFileDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/EngineFactory.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/GuiCommands.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/GuiCommands_Entities.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/GuiCommands_Components.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/GuiCommands_Assets.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/BatchCommands.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/CommandAsync.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/CommandAssetHelpers.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/CommandBatchHelpers.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/CommandEntityHelpers.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/CommandSnapshot.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorActions.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/content_generators/MannequinGraph.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/content_generators/PistonGraph.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ProjectConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ProjectBootstrap.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorRuntime.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/TransformGizmo.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ecs/EditorBootstrapSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ecs/TransformGizmoSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ecs/ThirdPersonCameraSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ecs/FirstPersonCameraSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ecs/EditorCameraSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/gui/GuiManager.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/gui/ProfilerWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/gui/RigSpawnerWidget.cpp
)

target_link_libraries(edux_editor PUBLIC
    edux_engine
)
