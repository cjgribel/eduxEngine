function(edux_add_runtime_target target_name)
    set(options)
    set(oneValueArgs OUTPUT_SUBDIR PROJECT_INCLUDE_DIR)
    set(multiValueArgs SOURCES INCLUDE_DIRS COMPILE_DEFINITIONS LINK_LIBRARIES)
    cmake_parse_arguments(EDUX "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT EDUX_OUTPUT_SUBDIR)
        message(FATAL_ERROR "edux_add_runtime_target(${target_name}): OUTPUT_SUBDIR is required")
    endif()

    if(NOT EDUX_PROJECT_INCLUDE_DIR)
        set(EDUX_PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    add_executable(${target_name}
        ${EDUX_SOURCES}
    )

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${EDUX_OUTPUT_SUBDIR}"
    )

    target_include_directories(${target_name} PRIVATE
        ${EDUX_PROJECT_INCLUDE_DIR}
        ${EDUX_INCLUDE_DIRS}
    )

    target_link_libraries(${target_name} PRIVATE
        edux_editor
        ${EDUX_LINK_LIBRARIES}
    )

    target_compile_definitions(${target_name} PRIVATE
        EENG_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        ${EDUX_COMPILE_DEFINITIONS}
    )

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2>
            $<TARGET_FILE_DIR:${target_name}>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_FILE_DIR:assimp>"
            $<TARGET_FILE_DIR:${target_name}>
    )

    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set_property(TARGET ${target_name} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
endfunction()
