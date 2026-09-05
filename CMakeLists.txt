cmake_minimum_required(VERSION 3.24)

project(map_coloring_dsatur VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(map_core STATIC
    src/bmp.c
    src/graph.c
    src/dsatur.c
)

target_include_directories(map_core PUBLIC include)

if(MSVC)
    target_compile_options(map_core PRIVATE /W4)
else()
    target_compile_options(map_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)

    add_executable(map_coloring src/mapread.c)
    target_include_directories(map_coloring PRIVATE include)
    target_compile_definitions(map_coloring PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(map_coloring PRIVATE
        map_core
        GLEW::GLEW
        glfw
        OpenGL::GL
        gdi32
        user32
        comdlg32
    )

    if(MSVC)
        target_compile_options(map_coloring PRIVATE /W4)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(map_core_tests tests/test_core.c)
    target_link_libraries(map_core_tests PRIVATE map_core)
    add_test(NAME map_core_tests COMMAND map_core_tests)
endif()
