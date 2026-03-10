edux_add_runtime_target(FluidSandbox
    OUTPUT_SUBDIR fluid_sandbox
    PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/FluidSandboxGame.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/FluidSandboxMetaReg.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ecs/systems/FluidFrameSystem.cpp
)
