cmake_minimum_required(VERSION 3.16)
project(astroid C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Gebruik de geinstalleerde raylib (brew install raylib), anders binnenhalen.
find_package(raylib 5.0 QUIET)
if(NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW ON
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(astroid
    src/main.c
    src/game.c
)

target_include_directories(astroid PRIVATE src)
target_link_libraries(astroid PRIVATE raylib)
target_compile_options(astroid PRIVATE -Wall -Wextra -Wno-unused-parameter)

if(APPLE)
    target_link_libraries(astroid PRIVATE
        "-framework CoreVideo" "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
