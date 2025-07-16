cmake_minimum_required(VERSION 3.16)
project(space_rts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

# Automatically gather all source files, reconfigure if sources change
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(space-rts ${SOURCES})

target_link_libraries(space-rts PRIVATE SDL2::SDL2 OpenGL::GL Freetype::Freetype)

