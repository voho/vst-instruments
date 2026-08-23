cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(SEPTUM_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(SEPTUM_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU and
# Standalone) compile C/Objective-C sources with non-Xcode generators too.
project(Septum VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SEPTUM_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(SEPTUM_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(SEPTUM_DSP_SOURCES
    Source/DSP/SeptumEngine.cpp
    Source/DSP/SeptumPresets.cpp
    Source/DSP/SeptumSysEx.cpp)
set(SEPTUM_DSP_HEADERS
    Source/DSP/SeptumEngine.h
    Source/DSP/SeptumPatch.h
    Source/DSP/SeptumPresets.h
    Source/DSP/SeptumSysEx.h)

add_library(SeptumDSP STATIC
    ${SEPTUM_DSP_SOURCES}
    ${SEPTUM_DSP_HEADERS})

target_include_directories(SeptumDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(SeptumDSP PUBLIC cxx_std_20)
set_target_properties(SeptumDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(SeptumDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(SeptumDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SEPTUM_BUILD_PLUGIN)
    if(SEPTUM_JUCE_PATH)
        if(NOT EXISTS "${SEPTUM_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "SEPTUM_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${SEPTUM_JUCE_PATH}")
        endif()
        add_subdirectory("${SEPTUM_JUCE_PATH}"
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
        set(SEPTUM_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(SEPTUM_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Septum
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Protocodus"
        COMPANY_WEBSITE "https://protocodus.cz"
        COMPANY_EMAIL "protocodus@proton.me"
        COMPANY_COPYRIGHT "Copyright (c) 2026 Protocodus"
        BUNDLE_ID "cz.protocodus.septum"
        PLUGIN_MANUFACTURER_CODE Sptm
        PLUGIN_CODE Spt1
        PRODUCT_NAME "Septum"
        DESCRIPTION "Septum ten-voice virtual-analog synthesizer"
        FORMATS ${SEPTUM_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(Septum)

    target_sources(Septum PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Septum
        PRIVATE
            SeptumDSP
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(Septum
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# The committed demonstration WAVs under Docs/audio are rendered by this tool
# from the same JUCE-free engine the plug-in uses, so they cannot drift away
# from what Septum actually produces. It is built by default, including on
# Linux CI, so it cannot rot unnoticed.
option(SEPTUM_BUILD_TOOLS "Build offline Septum tools" ON)

if(SEPTUM_BUILD_TOOLS)
    add_executable(SeptumRenderDemos Tools/RenderDemos.cpp)
    target_link_libraries(SeptumRenderDemos PRIVATE SeptumDSP)
    target_compile_features(SeptumRenderDemos PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(SeptumRenderDemos PRIVATE /W4 /permissive-)
    else()
        target_compile_options(SeptumRenderDemos PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(SeptumEngineTests Tests/SeptumEngineTests.cpp)
    target_link_libraries(SeptumEngineTests PRIVATE SeptumDSP)
    target_compile_features(SeptumEngineTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(SeptumEngineTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(SeptumEngineTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME Septum.Engine COMMAND SeptumEngineTests)
    # The suite renders every factory patch at two sample rates; allow slack
    # for slower CI workers.
    set_tests_properties(Septum.Engine PROPERTIES TIMEOUT 240)

    if(SEPTUM_BUILD_TOOLS)
        # Guards the renderer that produces the committed demonstration WAVs:
        # it renders a short take to a temporary directory and checks that the
        # audio is finite, audible and written as a readable WAV.
        add_test(NAME Septum.RenderDemos
                 COMMAND SeptumRenderDemos --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-smoke")
        set_tests_properties(Septum.RenderDemos PROPERTIES TIMEOUT 60)
    endif()

    if(SEPTUM_BUILD_PLUGIN)
        add_executable(SeptumPluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(SeptumPluginProcessorTests PRIVATE Septum)
        target_include_directories(SeptumPluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:Septum,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:Septum,INCLUDE_DIRECTORIES>")
        target_compile_features(SeptumPluginProcessorTests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(SeptumPluginProcessorTests PRIVATE
                /W4 /permissive-)
        else()
            target_compile_options(SeptumPluginProcessorTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME Septum.PluginProcessor
                 COMMAND SeptumPluginProcessorTests)
        set_tests_properties(Septum.PluginProcessor PROPERTIES TIMEOUT 120)
    endif()
endif()
