cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(TAIKOR_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(TAIKOR_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU and
# Standalone) compile C/Objective-C sources with non-Xcode generators too.
project(Taikor VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TAIKOR_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(TAIKOR_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(TAIKOR_DSP_SOURCES
    Source/DSP/TaikoEngine.cpp
    Source/DSP/UiMath.cpp)
set(TAIKOR_DSP_HEADERS
    Source/DSP/TaikoEngine.h
    Source/DSP/UiMath.h)

add_library(TaikorDSP STATIC
    ${TAIKOR_DSP_SOURCES}
    ${TAIKOR_DSP_HEADERS})

target_include_directories(TaikorDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(TaikorDSP PUBLIC cxx_std_20)
set_target_properties(TaikorDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(TaikorDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(TaikorDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(TAIKOR_BUILD_PLUGIN)
    if(TAIKOR_JUCE_PATH)
        if(NOT EXISTS "${TAIKOR_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "TAIKOR_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${TAIKOR_JUCE_PATH}")
        endif()
        add_subdirectory("${TAIKOR_JUCE_PATH}"
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
        set(TAIKOR_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(TAIKOR_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Taikor
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Taikor Audio"
        BUNDLE_ID "audio.taikor.synth"
        PLUGIN_MANUFACTURER_CODE Tkor
        PLUGIN_CODE Tko1
        PRODUCT_NAME "Taikor"
        DESCRIPTION "Physically modeled taiko drum"
        FORMATS ${TAIKOR_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Drum
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(Taikor)

    target_sources(Taikor PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Taikor
        PRIVATE
            TaikorDSP
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(Taikor
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# The committed demonstration WAVs under Docs/audio are rendered by this tool
# from the same JUCE-free engine the plug-in uses, so they cannot drift away
# from what Taikor actually produces. It is built by default, including on
# Linux CI, so it cannot rot unnoticed.
option(TAIKOR_BUILD_TOOLS "Build the demo WAV renderer" ON)

if(TAIKOR_BUILD_TOOLS)
    add_executable(TaikorRenderDemos Tools/RenderDemos.cpp)
    target_link_libraries(TaikorRenderDemos PRIVATE TaikorDSP)
    target_compile_features(TaikorRenderDemos PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(TaikorRenderDemos PRIVATE /W4 /permissive-)
    else()
        target_compile_options(TaikorRenderDemos PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(TaikorTaikoEngineTests Tests/TaikoEngineTests.cpp)
    target_link_libraries(TaikorTaikoEngineTests PRIVATE TaikorDSP)
    target_compile_features(TaikorTaikoEngineTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(TaikorTaikoEngineTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(TaikorTaikoEngineTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME Taikor.TaikoEngine COMMAND TaikorTaikoEngineTests)
    # 75 s on the reference machine, up from 69 when the Octave Body handover
    # and the readout's Nyquist clauses landed: those sweep the control in
    # ten-thousandths and build a voice bank per drum at four sample rates.
    set_tests_properties(Taikor.TaikoEngine PROPERTIES TIMEOUT 240)

    if(TAIKOR_BUILD_TOOLS)
        # Guards the renderer that produces the committed demonstration WAVs:
        # it renders a short take to a temporary directory and checks that the
        # audio is finite, audible and written as a readable WAV.
        add_test(NAME Taikor.RenderDemos
                 COMMAND TaikorRenderDemos --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-smoke")
        set_tests_properties(Taikor.RenderDemos PROPERTIES TIMEOUT 60)
    endif()

    if(TAIKOR_BUILD_PLUGIN)
        add_executable(TaikorPluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(TaikorPluginProcessorTests PRIVATE Taikor)
        target_include_directories(TaikorPluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:Taikor,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:Taikor,INCLUDE_DIRECTORIES>")
        target_compile_features(TaikorPluginProcessorTests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(TaikorPluginProcessorTests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(TaikorPluginProcessorTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME Taikor.PluginProcessor COMMAND TaikorPluginProcessorTests)
        set_tests_properties(Taikor.PluginProcessor PROPERTIES TIMEOUT 60)
    endif()
endif()
