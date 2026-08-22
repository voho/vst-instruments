cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(GHOST_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use
# the same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(GHOST_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU,
# Standalone) compile C/Objective-C sources. Restricting this to CXX breaks
# non-Xcode generators (Ninja/Makefiles).
project(Ghost VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GHOST_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(GHOST_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(GHOST_DSP_SOURCES
    Source/DSP/GhostEngine.cpp
    Source/DSP/GhostPresets.cpp)
set(GHOST_DSP_HEADERS
    Source/DSP/GhostEngine.h
    Source/DSP/GhostPresets.h)

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

if(GHOST_BUILD_PLUGIN)
    if(GHOST_JUCE_PATH)
        if(NOT EXISTS "${GHOST_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "GHOST_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${GHOST_JUCE_PATH}")
        endif()
        add_subdirectory("${GHOST_JUCE_PATH}"
                         "${CMAKE_CURRENT_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            # JUCE 8.0.14, pinned to its immutable release commit and archive
            # checksum so automated release builds cannot follow a moved tag.
            URL https://github.com/juce-framework/JUCE/archive/2cdfca8feb300fb424002ba2c2751569e5bacb64.tar.gz
            URL_HASH SHA256=814019716960a16f8f850a1593eeb56e2f21910aff2d673f7d3513b4ebaf251d)
        FetchContent_MakeAvailable(JUCE)
    endif()

    if(APPLE)
        set(GHOST_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(GHOST_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Ghost
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Protocodus"
        COMPANY_WEBSITE "https://protocodus.cz"
        COMPANY_EMAIL "protocodus@proton.me"
        COMPANY_COPYRIGHT "Copyright (c) 2026 Ghost contributors"
        BUNDLE_ID "cz.protocodus.ghost"
        PLUGIN_MANUFACTURER_CODE Ghst
        PLUGIN_CODE Gst1
        PRODUCT_NAME "Ghost"
        DESCRIPTION "Ghost circuit-modelled monophonic dual-filter synthesizer"
        FORMATS ${GHOST_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(Ghost)

    target_sources(Ghost PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Ghost
        PRIVATE
            GhostDSP
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(Ghost
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# The committed demonstration WAVs under Docs/audio are rendered by this tool
# from the same JUCE-free engine the plug-in runs, so they cannot drift away
# from what Ghost actually sounds like. It is built by default, including on
# Linux CI, so it cannot rot unnoticed.
option(GHOST_BUILD_TOOLS "Build the JUCE-free audio render tools" ON)

if(GHOST_BUILD_TOOLS)
    add_executable(GhostRenderDemos Tools/RenderDemos.cpp)
    target_link_libraries(GhostRenderDemos PRIVATE GhostDSP)
    target_compile_features(GhostRenderDemos PRIVATE cxx_std_20)

    # Plan Step 5's measurement: the block-latching (zipper) audit over the
    # published continuous travels, against a sample-accurate reference.
    add_executable(GhostZipperAudit Tools/ZipperAudit.cpp)
    target_link_libraries(GhostZipperAudit PRIVATE GhostDSP)
    target_compile_features(GhostZipperAudit PRIVATE cxx_std_20)

    foreach(target GhostRenderDemos GhostZipperAudit)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${target} PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
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

        # A full render run twice into the same directory proves the
        # atomic-replacement path — a rename over an existing WAV and README —
        # on every CI platform, not only POSIX. std::filesystem::rename is
        # specified with POSIX replace semantics, and this pair keeps that
        # from being a claim CI never exercises on Windows.
        add_test(NAME Ghost.RenderDemosFullRender
                 COMMAND GhostRenderDemos
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-full")
        add_test(NAME Ghost.RenderDemosRerender
                 COMMAND GhostRenderDemos
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-full")
        set_tests_properties(Ghost.RenderDemosFullRender PROPERTIES TIMEOUT 60)
        set_tests_properties(Ghost.RenderDemosRerender PROPERTIES TIMEOUT 60
                             DEPENDS Ghost.RenderDemosFullRender)

        # Keeps the zipper-audit strokes valid (finite, non-silent) on CI;
        # the full audit is run by hand when its table is being updated.
        add_test(NAME Ghost.ZipperAuditSmoke
                 COMMAND GhostZipperAudit --smoke)
        set_tests_properties(Ghost.ZipperAuditSmoke PROPERTIES TIMEOUT 120)
    endif()

    if(GHOST_BUILD_PLUGIN)
        add_executable(GhostPluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(GhostPluginProcessorTests PRIVATE Ghost)
        target_include_directories(GhostPluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:Ghost,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:juce::juce_audio_utils,INTERFACE_INCLUDE_DIRECTORIES>")
        target_compile_features(GhostPluginProcessorTests PRIVATE cxx_std_20)

        add_test(NAME Ghost.PluginProcessor COMMAND GhostPluginProcessorTests)
        set_tests_properties(Ghost.PluginProcessor PROPERTIES TIMEOUT 60)
    endif()
endif()
