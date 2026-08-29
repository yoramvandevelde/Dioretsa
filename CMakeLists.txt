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

project(dioretsa C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(RAYLIB_VERSION 6.0)     # matches what brew installs, so both paths agree

# Which revision a from-source build clones. The release everywhere except
# Android, where 6.0 drops every gamepad face button: AOSP stamps the keyboard
# source bit on a controller's key events, and 6.0 reads that as proof the event
# did not come from a gamepad. Upstream fixed it three weeks after the release,
# in a commit that no release has reached yet, so Android pins the commit.
# Back to ${RAYLIB_VERSION} the moment a release contains it.
# https://github.com/raysan5/raylib/pull/5824
set(RAYLIB_GIT_TAG ${RAYLIB_VERSION})
set(RAYLIB_SHALLOW ON)
if(ANDROID)
    set(RAYLIB_GIT_TAG a005a044dac0d55d6e7b0eb783541e4fa88138d3)
    # A shallow clone can only be asked for a name the remote advertises, and a
    # bare commit is not one of them.
    set(RAYLIB_SHALLOW OFF)
endif()

# There is no Homebrew on a television, so Android always compiles raylib from
# source. The option only settles the question for the desktop builds. This has
# to sit below project(), which is where the NDK toolchain defines ANDROID.
if(ANDROID)
    set(SELF_CONTAINED ON)
endif()

if(SELF_CONTAINED)
    include(FetchContent)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        ${RAYLIB_GIT_TAG}
        GIT_SHALLOW    ${RAYLIB_SHALLOW}
    )
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES    OFF CACHE BOOL "" FORCE)
    if(ANDROID)
        # raylib chooses its backend from this, and brings native_app_glue, EGL
        # and OpenSLES along with it.
        set(PLATFORM Android CACHE STRING "" FORCE)
    endif()
    FetchContent_MakeAvailable(raylib)

    if(ANDROID)
        # Everything drawn between two state changes goes into one vertex
        # buffer, and whatever will not fit forces raylib to upload and draw
        # what it has so far, mid-picture. That buffer defaults to 2048 quads
        # on OpenGL ES 2 against 8192 on the desktop, which is why the same
        # scene batches cleanly on a laptop and comes apart on a television.
        # A megabyte of vertex buffer is far cheaper than the flushes.
        target_compile_definitions(raylib PRIVATE RL_DEFAULT_BATCH_BUFFER_ELEMENTS=8192)
    endif()
else()
    find_package(raylib ${RAYLIB_VERSION} REQUIRED)
endif()

set(DIORETSA_SOURCES
    src/main.c
    src/game.c
    src/fx.c
    src/audio.c
    src/assets.c
    src/input.c
)

# Android has nothing to launch an executable with. The activity loads a shared
# library instead, raylib's android_main() runs inside it, and that calls the
# main() in src/main.c the way a shell would.
if(ANDROID)
    add_library(dioretsa SHARED ${DIORETSA_SOURCES})
    # Nothing in this code refers to the entry point the manifest names, so
    # without this the linker drops it and the activity never starts.
    target_link_options(dioretsa PRIVATE -u ANativeActivity_onCreate)
else()
    add_executable(dioretsa ${DIORETSA_SOURCES})
endif()

target_include_directories(dioretsa PRIVATE src)
target_link_libraries(dioretsa PRIVATE raylib)
# MSVC does not speak GCC's warning flags, and the Windows runner is MSVC.
if(MSVC)
    target_compile_options(dioretsa PRIVATE /W3)
else()
    target_compile_options(dioretsa PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(APPLE)
    target_link_libraries(dioretsa PRIVATE
        "-framework CoreVideo" "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# Assets are deliberately not copied here. There is one assets directory, the
# one in the repository, and the executable finds it one level up. Copies in a
# build directory would mean three files named ENGINE.wav with only one of them
# counting, which is a good way to lose an edit. Packaging a release is where
# the copy belongs. An APK is packaging, and android/app/build.gradle points
# Gradle straight at that same directory.
