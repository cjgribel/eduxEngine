edux_add_runtime_target(ReferenceGame
    OUTPUT_SUBDIR reference_game
    PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ReferenceGame.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/FluidSandboxMetaReg.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ecs/systems/FluidFrameSystem.cpp
)
