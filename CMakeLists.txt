cmake_minimum_required(VERSION 3.24)

project(map_coloring_dsatur VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(WIN32)
    option(MAP_COLORING_BUILD_GUI "Build the Windows OpenGL application" ON)
else()
    option(MAP_COLORING_BUILD_GUI "Build the Windows OpenGL application" OFF)
endif()

option(MAP_COLORING_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(MAP_COLORING_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

function(map_coloring_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(MAP_COLORING_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(MAP_COLORING_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(map_coloring_enable_sanitizers target)
    if(MAP_COLORING_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

add_library(map_core STATIC
    src/bmp.c
    src/graph.c
    src/dsatur.c
)

target_include_directories(map_core PUBLIC include)
map_coloring_enable_warnings(map_core)
map_coloring_enable_sanitizers(map_core)

if(MAP_COLORING_BUILD_GUI)
    if(NOT WIN32)
        message(FATAL_ERROR "MAP_COLORING_BUILD_GUI is supported only on Windows")
    endif()

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

    map_coloring_enable_warnings(map_coloring)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(map_core_tests tests/test_core.c)
    target_link_libraries(map_core_tests PRIVATE map_core)
    map_coloring_enable_warnings(map_core_tests)
    map_coloring_enable_sanitizers(map_core_tests)
    add_test(NAME map_core_tests COMMAND map_core_tests)
endif()
