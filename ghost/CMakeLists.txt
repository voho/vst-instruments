cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# C is declared alongside C++ for parity with the other subprojects, whose
# JUCE macOS wrappers compile C/Objective-C sources; the DSP-only phase does
# not use it but keeping the declaration means the plug-in phase cannot break
# non-Xcode generators later.
project(Ghost VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ghost is deliberately DSP-first: the engine, its tests and the demo renderer
# exist before any JUCE wrapper does. The option is declared now so the CI and
# nightly lanes that pass -DGHOST_BUILD_PLUGIN=OFF to every subproject work
# unchanged, and so the eventual plug-in lands behind a switch that already
# exists. Asking for the plug-in today is an error rather than a silent no-op.
option(GHOST_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" OFF)
if(GHOST_BUILD_PLUGIN)
    message(FATAL_ERROR
        "The Ghost plug-in wrapper is not implemented yet; this subproject is "
        "in its DSP-first phase. Configure with -DGHOST_BUILD_PLUGIN=OFF.")
endif()

include(CTest)

set(GHOST_DSP_SOURCES
    Source/DSP/GhostEngine.cpp)
set(GHOST_DSP_HEADERS
    Source/DSP/GhostEngine.h)

add_library(GhostDSP STATIC
    ${GHOST_DSP_SOURCES}
    ${GHOST_DSP_HEADERS})

target_include_directories(GhostDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(GhostDSP PUBLIC cxx_std_20)
set_target_properties(GhostDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(GhostDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(GhostDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The committed demonstration WAVs under Docs/audio are rendered by this tool
# from the same JUCE-free engine the eventual plug-in will use, so they cannot
# drift away from what Ghost actually produces. It is built by default,
# including on Linux CI, so it cannot rot unnoticed.
option(GHOST_BUILD_TOOLS "Build the JUCE-free audio render tools" ON)

if(GHOST_BUILD_TOOLS)
    add_executable(GhostRenderDemos Tools/RenderDemos.cpp)
    target_link_libraries(GhostRenderDemos PRIVATE GhostDSP)
    target_compile_features(GhostRenderDemos PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(GhostRenderDemos PRIVATE /W4 /permissive-)
    else()
        target_compile_options(GhostRenderDemos PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(GhostEngineTests Tests/GhostEngineTests.cpp)
    target_link_libraries(GhostEngineTests PRIVATE GhostDSP)
    target_compile_features(GhostEngineTests PRIVATE cxx_std_20)

    add_executable(GhostCircuitTests Tests/GhostCircuitTests.cpp)
    target_link_libraries(GhostCircuitTests PRIVATE GhostDSP)
    target_compile_features(GhostCircuitTests PRIVATE cxx_std_20)

    foreach(target GhostEngineTests GhostCircuitTests)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()

    add_test(NAME Ghost.Engine COMMAND GhostEngineTests)
    add_test(NAME Ghost.Circuit COMMAND GhostCircuitTests)
    set_tests_properties(Ghost.Engine PROPERTIES TIMEOUT 300)
    set_tests_properties(Ghost.Circuit PROPERTIES TIMEOUT 300)

    if(GHOST_BUILD_TOOLS)
        # Guards the renderer that produces the committed demonstration WAVs:
        # it renders a short take to a temporary directory and checks that the
        # audio is finite, audible and written as a readable WAV.
        add_test(NAME Ghost.RenderDemos
                 COMMAND GhostRenderDemos --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-smoke")
        set_tests_properties(Ghost.RenderDemos PROPERTIES TIMEOUT 60)
    endif()
endif()
