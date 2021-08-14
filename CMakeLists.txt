cmake_minimum_required(VERSION 3.0)

set(CMAKE_BUILD_TYPE Release)

set(EDITOR_NAME gui-gal)
set(SOURCE_PATH "src")

project(${EDITOR_NAME})

find_package(SDL2 REQUIRED)

include_directories("deps")
# include_directories("deps/emscripten/system/include")
include_directories("deps/glad/include/")
include_directories("deps/cglm/include")
include_directories("deps/SDL/include")
include_directories("deps/curl/include")
# include_directories("deps/SDL_image")
include_directories("deps/giflib-5.2.1")
include_directories("deps/nanovg/src")

set(GLAD_GL "deps/glad/include/glad/glad.h"
            "deps/glad/src/glad.c")

file(GLOB_RECURSE NANOVG "deps/nanovg/src/*c")

file(GLOB_RECURSE SOURCES "${SOURCE_PATH}/*.c")

add_subdirectory("deps/flecs")
add_subdirectory("deps/glfw")
add_subdirectory("deps/cglm")
# add_subdirectory("deps/curl")

link_directories("deps/giflib-5.2.1")

# file(GLOB_RECURSE GIFLIB_SOURCES "deps/giflib-5.2.1/*.c")
# add_library(giflib SHARED GIFLIB_SOURCES)
# execute_process(WORKING_DIRECTORY "./deps/giflib-5.2.1/" COMMAND make)


add_executable(${EDITOR_NAME} ${SOURCES} ${GLAD_GL} ${NANOVG})
target_link_libraries(${EDITOR_NAME} glfw SDL2 SDL2_image flecs gif curl)