cmake_minimum_required(VERSION 3.22)

option(VOCALOR_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(VOCALOR_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

project(Vocalor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(VOCALOR_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(VOCALOR_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(VOCALOR_DSP_SOURCES Source/DSP/VoiceEngine.cpp)
set(VOCALOR_DSP_HEADERS Source/DSP/VoiceEngine.h)

add_library(VocalorDSP STATIC
    ${VOCALOR_DSP_SOURCES}
    ${VOCALOR_DSP_HEADERS})

target_include_directories(VocalorDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(VocalorDSP PUBLIC cxx_std_20)
set_target_properties(VocalorDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(VocalorDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(VocalorDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(VOCALOR_BUILD_PLUGIN)
    if(VOCALOR_JUCE_PATH)
        if(NOT EXISTS "${VOCALOR_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "VOCALOR_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${VOCALOR_JUCE_PATH}")
        endif()
        add_subdirectory("${VOCALOR_JUCE_PATH}"
                         "${CMAKE_CURRENT_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.14
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    if(APPLE)
        set(VOCALOR_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(VOCALOR_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Vocalor
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Vocalor Audio"
        BUNDLE_ID "audio.vocalor.synth"
        PLUGIN_MANUFACTURER_CODE Vclr
        PLUGIN_CODE Vcl1
        PRODUCT_NAME "Vocalor"
        DESCRIPTION "Expressive human voice and choir synthesizer"
        FORMATS ${VOCALOR_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(Vocalor)

    target_sources(Vocalor PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Vocalor
        PRIVATE
            VocalorDSP
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(Vocalor
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

if(BUILD_TESTING)
    add_executable(VocalorVoiceEngineTests Tests/VoiceEngineTests.cpp)
    target_link_libraries(VocalorVoiceEngineTests PRIVATE VocalorDSP)
    target_compile_features(VocalorVoiceEngineTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(VocalorVoiceEngineTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(VocalorVoiceEngineTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME Vocalor.VoiceEngine COMMAND VocalorVoiceEngineTests)
    set_tests_properties(Vocalor.VoiceEngine PROPERTIES TIMEOUT 30)
endif()
