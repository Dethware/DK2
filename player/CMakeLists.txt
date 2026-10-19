cmake_minimum_required(VERSION 3.21)

project(dk2music
    VERSION 0.1.0
    DESCRIPTION "Standalone player for the Dungeon Keeper II dynamic music"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Multimedia Widgets Test)
qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/app)
add_subdirectory(tests)
