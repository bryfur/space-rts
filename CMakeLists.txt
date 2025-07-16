cmake_minimum_required(VERSION 3.16)
project(space_rts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add debug symbols and enable sanitizers for debugging
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address -fsanitize=undefined")
set(CMAKE_BUILD_TYPE Debug)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

# Automatically gather all source files, reconfigure if sources change
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/core/*.cpp
    src/systems/*.cpp
    src/components/*.cpp
    src/rendering/*.cpp
    src/input/*.cpp
    src/gameplay/*.cpp
    src/ui/*.cpp
    src/utils/*.cpp
)

add_executable(space-rts ${SOURCES})

target_link_libraries(space-rts PRIVATE SDL2::SDL2 OpenGL::GL Freetype::Freetype)

