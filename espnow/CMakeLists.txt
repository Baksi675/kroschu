# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS modules)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(espnow_ack_comm)

if(EXISTS "${CMAKE_SOURCE_DIR}/fix_compile_commands.py")
    add_custom_target(
        fix_clangd ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Running fix_compile_commands.py..."
        COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/fix_compile_commands.py
        COMMENT "Cleaning compile_commands.json for clangd"
        VERBATIM
    )
endif()
