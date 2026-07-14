cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(DRUMALOR_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(DRUMALOR_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU and
# Standalone) compile C/Objective-C sources with non-Xcode generators too.
project(Drumalor VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DRUMALOR_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(DRUMALOR_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(DRUMALOR_DSP_SOURCES Source/DSP/DrumEngine.cpp)
set(DRUMALOR_DSP_HEADERS Source/DSP/DrumEngine.h)

add_library(DrumalorDSP STATIC
    ${DRUMALOR_DSP_SOURCES}
    ${DRUMALOR_DSP_HEADERS})

target_include_directories(DrumalorDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(DrumalorDSP PUBLIC cxx_std_20)
set_target_properties(DrumalorDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(DrumalorDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(DrumalorDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DRUMALOR_BUILD_PLUGIN)
    if(DRUMALOR_JUCE_PATH)
        if(NOT EXISTS "${DRUMALOR_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "DRUMALOR_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${DRUMALOR_JUCE_PATH}")
        endif()
        add_subdirectory("${DRUMALOR_JUCE_PATH}"
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
        set(DRUMALOR_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(DRUMALOR_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_binary_data(DrumalorAssets
        HEADER_NAME DrumalorAssets.h
        NAMESPACE DrumalorAssets
        SOURCES Assets/vintage-panel.jpg)

    juce_add_plugin(Drumalor
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Drumalor Audio"
        BUNDLE_ID "audio.drumalor.synth"
        PLUGIN_MANUFACTURER_CODE Dral
        PLUGIN_CODE Drm1
        PRODUCT_NAME "Drumalor"
        DESCRIPTION "Procedural thirteen-voice drum synthesizer"
        FORMATS ${DRUMALOR_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Drum
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(Drumalor)

    target_sources(Drumalor PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Drumalor
        PRIVATE
            DrumalorDSP
            DrumalorAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(Drumalor
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

if(BUILD_TESTING)
    add_executable(DrumalorDrumEngineTests Tests/DrumEngineTests.cpp)
    target_link_libraries(DrumalorDrumEngineTests PRIVATE DrumalorDSP)
    target_compile_features(DrumalorDrumEngineTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(DrumalorDrumEngineTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(DrumalorDrumEngineTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME Drumalor.DrumEngine COMMAND DrumalorDrumEngineTests)
    set_tests_properties(Drumalor.DrumEngine PROPERTIES TIMEOUT 30)

    if(DRUMALOR_BUILD_PLUGIN)
        add_executable(DrumalorPluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(DrumalorPluginProcessorTests PRIVATE Drumalor)
        target_include_directories(DrumalorPluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:Drumalor,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:Drumalor,INCLUDE_DIRECTORIES>")
        target_compile_features(DrumalorPluginProcessorTests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(DrumalorPluginProcessorTests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(DrumalorPluginProcessorTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME Drumalor.PluginProcessor COMMAND DrumalorPluginProcessorTests)
        set_tests_properties(Drumalor.PluginProcessor PROPERTIES TIMEOUT 30)
    endif()
endif()
