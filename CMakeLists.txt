cmake_minimum_required(VERSION 3.0)

set(EDITOR_NAME gui-gal)
set(SOURCE_PATH "src")

project(${EDITOR_NAME})

include_directories("deps")
include_directories("deps/emscripten/system/include")
include_directories("deps/glad/include/")

set(GLAD_GL "deps/glad/include/glad/glad.h"
            "deps/glad/src/glad.c")

file(GLOB_RECURSE SOURCES "${SOURCE_PATH}/*.c")

add_subdirectory("deps/flecs")
add_subdirectory("deps/glfw")

add_executable(${EDITOR_NAME} ${SOURCES} ${GLAD_GL})
target_link_libraries(${EDITOR_NAME} flecs glfw)