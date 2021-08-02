cmake_minimum_required(VERSION 3.0)

set(EDITOR_NAME gui-gal)
set(SOURCE_PATH "src")

project(${EDITOR_NAME})

find_package(SDL2 REQUIRED)

include_directories("deps")
# include_directories("deps/emscripten/system/include")#include <GLFW/glfw3.h>
include_directories("deps/glad/include/")
include_directories("deps/cglm/include")
include_directories("deps/SDL/include")

set(GLAD_GL "deps/glad/include/glad/glad.h"
            "deps/glad/src/glad.c")

file(GLOB_RECURSE SOURCES "${SOURCE_PATH}/*.c")

# add_subdirectory("deps/SDL")
add_subdirectory("deps/flecs")
add_subdirectory("deps/glfw")
add_subdirectory("deps/cglm")

add_executable(${EDITOR_NAME} ${SOURCES} ${GLAD_GL})
target_link_libraries(${EDITOR_NAME} glfw SDL2 SDL2_image flecs)