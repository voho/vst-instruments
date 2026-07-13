cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(MARS_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(MARS_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU, Standalone)
# compile C/Objective-C sources. Restricting this to CXX breaks non-Xcode
# generators (Ninja/Makefiles) with a missing CMAKE_C_COMPILE_OBJECT error.
project(Mars VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MARS_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(MARS_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(MARS_DSP_SOURCES Source/DSP/MarsEngine.cpp)
set(MARS_DSP_HEADERS Source/DSP/MarsEngine.h)

add_library(MarsDSP STATIC
    ${MARS_DSP_SOURCES}
    ${MARS_DSP_HEADERS})

target_include_directories(MarsDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(MarsDSP PUBLIC cxx_std_20)
set_target_properties(MarsDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(MarsDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(MarsDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MARS_BUILD_PLUGIN)
    if(MARS_JUCE_PATH)
        if(NOT EXISTS "${MARS_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "MARS_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${MARS_JUCE_PATH}")
        endif()
        add_subdirectory("${MARS_JUCE_PATH}"
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
        set(MARS_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(MARS_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_binary_data(MarsAssets
        HEADER_NAME MarsAssets.h
        NAMESPACE MarsAssets
        SOURCES Assets/mars-panel-texture.png)

    juce_add_plugin(Mars
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Mars Audio"
        BUNDLE_ID "audio.mars.synth"
        PLUGIN_MANUFACTURER_CODE Mars
        PLUGIN_CODE Mar1
        PRODUCT_NAME "Mars"
        DESCRIPTION "Mars nonlinear virtual-analog polysynth"
        FORMATS ${MARS_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(Mars)

    target_sources(Mars PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Mars
        PRIVATE
            MarsDSP
            MarsAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(Mars
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

if(BUILD_TESTING)
    add_executable(MarsEngineTests Tests/MarsEngineTests.cpp)
    target_link_libraries(MarsEngineTests PRIVATE MarsDSP)
    target_compile_features(MarsEngineTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(MarsEngineTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(MarsEngineTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME Mars.MarsEngine COMMAND MarsEngineTests)
    set_tests_properties(Mars.MarsEngine PROPERTIES TIMEOUT 30)

    if(MARS_BUILD_PLUGIN)
        add_executable(MarsPluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(MarsPluginProcessorTests PRIVATE Mars)
        target_include_directories(MarsPluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:Mars,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:Mars,INCLUDE_DIRECTORIES>")
        target_compile_features(MarsPluginProcessorTests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(MarsPluginProcessorTests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(MarsPluginProcessorTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME Mars.PluginProcessor COMMAND MarsPluginProcessorTests)
        set_tests_properties(Mars.PluginProcessor PROPERTIES TIMEOUT 30)
    endif()
endif()
