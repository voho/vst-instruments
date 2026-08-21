cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(YOUKNOW201_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(YOUKNOW201_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU and
# Standalone) compile C/Objective-C sources with non-Xcode generators too.
project(YouKnow201 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(YOUKNOW201_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(YOUKNOW201_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(YOUKNOW201_DSP_SOURCES
    Source/DSP/YouKnow201Engine.cpp
    Source/DSP/YouKnow201Presets.cpp)
set(YOUKNOW201_DSP_HEADERS
    Source/DSP/YouKnow201Engine.h
    Source/DSP/YouKnow201Patch.h
    Source/DSP/YouKnow201Presets.h)

add_library(YouKnow201DSP STATIC
    ${YOUKNOW201_DSP_SOURCES}
    ${YOUKNOW201_DSP_HEADERS})

target_include_directories(YouKnow201DSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(YouKnow201DSP PUBLIC cxx_std_20)
set_target_properties(YouKnow201DSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(YouKnow201DSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(YouKnow201DSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(YOUKNOW201_BUILD_PLUGIN)
    if(YOUKNOW201_JUCE_PATH)
        if(NOT EXISTS "${YOUKNOW201_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "YOUKNOW201_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${YOUKNOW201_JUCE_PATH}")
        endif()
        add_subdirectory("${YOUKNOW201_JUCE_PATH}"
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
        set(YOUKNOW201_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(YOUKNOW201_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(YouKnow201
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Protocodus"
        COMPANY_WEBSITE "https://protocodus.cz"
        COMPANY_EMAIL "protocodus@proton.me"
        COMPANY_COPYRIGHT "Copyright (c) 2026 Protocodus"
        BUNDLE_ID "cz.protocodus.youknow201"
        PLUGIN_MANUFACTURER_CODE Ykno
        PLUGIN_CODE Yk21
        PRODUCT_NAME "YouKnow201"
        DESCRIPTION "YouKnow201 ten-voice virtual-analog synthesizer"
        FORMATS ${YOUKNOW201_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(YouKnow201)

    target_sources(YouKnow201 PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(YouKnow201
        PRIVATE
            YouKnow201DSP
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(YouKnow201
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# The committed demonstration WAVs under Docs/audio are rendered by this tool
# from the same JUCE-free engine the plug-in uses, so they cannot drift away
# from what YouKnow201 actually produces. It is built by default, including on
# Linux CI, so it cannot rot unnoticed.
option(YOUKNOW201_BUILD_TOOLS "Build offline YouKnow201 tools" ON)

if(YOUKNOW201_BUILD_TOOLS)
    add_executable(YouKnow201RenderDemos Tools/RenderDemos.cpp)
    target_link_libraries(YouKnow201RenderDemos PRIVATE YouKnow201DSP)
    target_compile_features(YouKnow201RenderDemos PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(YouKnow201RenderDemos PRIVATE /W4 /permissive-)
    else()
        target_compile_options(YouKnow201RenderDemos PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(YouKnow201EngineTests Tests/YouKnow201EngineTests.cpp)
    target_link_libraries(YouKnow201EngineTests PRIVATE YouKnow201DSP)
    target_compile_features(YouKnow201EngineTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(YouKnow201EngineTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(YouKnow201EngineTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME YouKnow201.Engine COMMAND YouKnow201EngineTests)
    # The suite renders every factory patch at two sample rates; allow slack
    # for slower CI workers.
    set_tests_properties(YouKnow201.Engine PROPERTIES TIMEOUT 240)

    if(YOUKNOW201_BUILD_TOOLS)
        # Guards the renderer that produces the committed demonstration WAVs:
        # it renders a short take to a temporary directory and checks that the
        # audio is finite, audible and written as a readable WAV.
        add_test(NAME YouKnow201.RenderDemos
                 COMMAND YouKnow201RenderDemos --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-smoke")
        set_tests_properties(YouKnow201.RenderDemos PROPERTIES TIMEOUT 60)
    endif()

    if(YOUKNOW201_BUILD_PLUGIN)
        add_executable(YouKnow201PluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(YouKnow201PluginProcessorTests PRIVATE YouKnow201)
        target_include_directories(YouKnow201PluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:YouKnow201,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:YouKnow201,INCLUDE_DIRECTORIES>")
        target_compile_features(YouKnow201PluginProcessorTests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(YouKnow201PluginProcessorTests PRIVATE
                /W4 /permissive-)
        else()
            target_compile_options(YouKnow201PluginProcessorTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME YouKnow201.PluginProcessor
                 COMMAND YouKnow201PluginProcessorTests)
        set_tests_properties(YouKnow201.PluginProcessor PROPERTIES TIMEOUT 120)
    endif()
endif()
