edux_add_runtime_target(LegacyGame
    OUTPUT_SUBDIR legacy_game
    PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Game.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/LegacyGameMetaReg.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ecs/systems/VehicleRig1ControlSystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ecs/systems/PistonInputSystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ecs/systems/MannequinPlayerControllerSystem.cpp
)
