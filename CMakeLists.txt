cmake_minimum_required(VERSION 3.0)

# set(CMAKE_BUILD_TYPE Debug)

# set(CMAKE_SYSTEM_NAME Linux)
# set(CMAKE_PLATFORM Linux)

set(EDITOR_NAME gui-gal)
set(SOURCE_PATH "src")
# set(PYTHONHOME "/home/aeri/miniconda3/envs/ldm/bin/python3.8")
set(DCMAKE_VERBOSE_MAKEFILE true)

# Use Anaconda compiler, otherwise .so's like _ssl module will not work properly
# https://conda.io/projects/conda-build/en/latest/resources/compiler-tools.html
# set(CMAKE_C_COMPILER "/home/aeri/miniconda3/envs/ldm/bin/x86_64-conda-linux-gnu-cc")
# set(CMAKE_FIND_ROOT_PATH "/home/aeri/miniconda3/envs/ldm/x86_64-conda-linux-gnu/sysroot/") # CMAKE_SYSROOT

# To see the default compiler header includes
# `gcc -print-prog-name=cpp` -v
# X11 is here!
# include_directories("/usr/include/")

# cross-linux.cmake
# search for programs in the build host directories
# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# god-awful hack because it seems to not run correct tests to determine this:
# set(__CHAR_UNSIGNED___EXITCODE 1)

project(${EDITOR_NAME})

# /home/aeri/miniconda3/envs/ldm/bin/python3.8-config --cflags
# add_compile_options(-I/usr/include -L/usr/include -I/usr/local/include -L/usr/local/include)
# add_compile_options(-I/home/aeri/miniconda3/envs/ldm/include/python3.8 -Wno-unused-result -Wsign-compare -march=nocona -mtune=haswell -ftree-vectorize -fPIC -fstack-protector-strong -fno-plt -O3 -ffunction-sections -pipe -isystem /home/aeri/miniconda3/envs/ldm/include -fdebug-prefix-map=/tmp/build/80754af9/python_1599203911753/work=/usr/local/src/conda/python-3.8.5 -fdebug-prefix-map=/home/aeri/miniconda3/envs/ldm=/usr/local/src/conda-prefix -fuse-linker-plugin -ffat-lto-objects -flto-partition=none -flto -DNDEBUG -fwrapv -O3 -Wall)
# /home/aeri/miniconda3/envs/ldm/bin/python3.8-config --ldflags
# add_link_options(-L/home/aeri/miniconda3/envs/ldm/lib/python3.8/config-3.8-x86_64-linux-gnu -L/home/aeri/miniconda3/envs/ldm/lib -lcrypt -lpthread -ldl -lutil -lrt -lm)

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
# target_compile_options(${EDITOR_NAME} -I/home/aeri/miniconda3/envs/ldm/include/python3.8 -Wno-unused-result -Wsign-compare -march=nocona -mtune=haswell -ftree-vectorize -fPIC -fstack-protector-strong -fno-plt -O3 -ffunction-sections -pipe -isystem /home/aeri/miniconda3/envs/ldm/include -fdebug-prefix-map=/tmp/build/80754af9/python_1599203911753/work=/usr/local/src/conda/python-3.8.5 -fdebug-prefix-map=/home/aeri/miniconda3/envs/ldm=/usr/local/src/conda-prefix -fuse-linker-plugin -ffat-lto-objects -flto-partition=none -flto -DNDEBUG -fwrapv -O3 -Wall)
# target_link_options(${EDITOR_NAME} -L/home/aeri/miniconda3/envs/ldm/lib/python3.8/config-3.8-x86_64-linux-gnu -L/home/aeri/miniconda3/envs/ldm/lib  -lcrypt -lpthread -ldl  -lutil -lrt -lm -lm)

target_link_libraries(${EDITOR_NAME} glfw SDL2 SDL2_image flecs gif curl gaul gaul_util python3.8)