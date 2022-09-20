cmake_minimum_required(VERSION 3.0)

set(CMAKE_BUILD_TYPE Debug)

set(EDITOR_NAME gui-gal)
set(SOURCE_PATH "src")
set(PYTHONHOME "/home/aeri/miniconda3/envs/ldm/bin/python3.8")

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
include_directories("deps/flecs/include")
include_directories("deps/gaul/src")
include_directories("deps/gaul/util")

# Embed Python for Stable Diffusion
# https://docs.python.org/3/extending/embedding.html
include_directories("/home/aeri/miniconda3/envs/ldm/include/python3.8/")
# find_library(python3.8 PATHS "/home/aeri/miniconda3/envs/ldm/bin/python3.8" NO_DEFAULT_PATH)

set(GLAD_GL "deps/glad/include/glad/glad.h"
            "deps/glad/src/glad.c")

file(GLOB_RECURSE NANOVG "deps/nanovg/src/*c")

file(GLOB_RECURSE SOURCES "${SOURCE_PATH}/*.c")

find_package(Vulkan)

add_subdirectory("deps/flecs")
add_subdirectory("deps/glfw")
add_subdirectory("deps/cglm")
# add_subdirectory("deps/curl")

link_directories("deps/giflib-5.2.1")
link_directories("deps/gaul/util/.libs")
link_directories("deps/gaul/src/.libs")

# file(GLOB_RECURSE GIFLIB_SOURCES "deps/giflib-5.2.1/*.c")
# add_library(giflib SHARED GIFLIB_SOURCES)
# execute_process(WORKING_DIRECTORY "./deps/giflib-5.2.1/" COMMAND make)


add_executable(${EDITOR_NAME} ${SOURCES} ${GLAD_GL} ${NANOVG})
target_link_libraries(${EDITOR_NAME} glfw SDL2 SDL2_image flecs gif curl gaul gaul_util python3.8)