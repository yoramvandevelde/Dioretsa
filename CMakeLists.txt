cmake_minimum_required(VERSION 3.16)

# A self-contained build compiles raylib from source and links it statically,
# so the result runs on a machine that has never heard of Homebrew. It is off by
# default because it makes a cold build minutes instead of seconds.
option(SELF_CONTAINED "Build raylib from source and link it statically" OFF)

# Universal binaries have to be decided before anything is compiled, raylib
# included, so this sits above project().
if(APPLE AND SELF_CONTAINED)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(astroid C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(RAYLIB_VERSION 6.0)     # matches what brew installs, so both paths agree

if(SELF_CONTAINED)
    include(FetchContent)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        ${RAYLIB_VERSION}
        GIT_SHALLOW    ON
    )
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES    OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
else()
    find_package(raylib ${RAYLIB_VERSION} REQUIRED)
endif()

add_executable(astroid
    src/main.c
    src/game.c
    src/fx.c
    src/audio.c
    src/assets.c
)

target_include_directories(astroid PRIVATE src)
target_link_libraries(astroid PRIVATE raylib)
target_compile_options(astroid PRIVATE -Wall -Wextra -Wno-unused-parameter)

if(APPLE)
    target_link_libraries(astroid PRIVATE
        "-framework CoreVideo" "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# Assets are deliberately not copied here. There is one assets directory, the
# one in the repository, and the executable finds it one level up. Copies in a
# build directory would mean three files named ENGINE.wav with only one of them
# counting, which is a good way to lose an edit. Packaging a release is where
# the copy belongs.
