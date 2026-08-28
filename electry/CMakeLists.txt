cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(ELECTRY_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(ELECTRY_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU, Standalone)
# compile C/Objective-C sources. Restricting this to CXX breaks non-Xcode
# generators (Ninja/Makefiles) with a missing CMAKE_C_COMPILE_OBJECT error.
project(Electry VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ELECTRY_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
option(ELECTRY_MEASURED_BODY_RESPONSE
    "Use the shipping matched-material modal body response" ON)
option(ELECTRY_DECOUPLED_PICK_RELEASE
    "Use the shipping force-decoupled plectrum release response" ON)
option(ELECTRY_MEASURED_MODERN_CABINET
    "Build the default-off measured-IR Modern cabinet candidate" OFF)
set(ELECTRY_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(ELECTRY_DSP_SOURCES
    Source/DSP/ElectryEngine.cpp
    Source/DSP/ElectryFx.cpp
    Source/DSP/ElectryVisuals.cpp)
set(ELECTRY_DSP_HEADERS
    Source/DSP/DspMath.h
    Source/DSP/ElectryEngine.h
    Source/DSP/ElectryFx.h
    Source/DSP/ModernCabinetIR.h
    Source/DSP/ElectryVisuals.h)

add_library(ElectryDSP STATIC
    ${ELECTRY_DSP_SOURCES}
    ${ELECTRY_DSP_HEADERS})

target_include_directories(ElectryDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(ElectryDSP PUBLIC cxx_std_20)
set_target_properties(ElectryDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(ELECTRY_MEASURED_MODERN_CABINET)
    target_compile_definitions(ElectryDSP PUBLIC
        ELECTRY_MEASURED_MODERN_CABINET=1)
endif()

if(ELECTRY_MEASURED_BODY_RESPONSE)
    target_compile_definitions(ElectryDSP PUBLIC
        ELECTRY_MEASURED_BODY_RESPONSE=1)
endif()

if(ELECTRY_DECOUPLED_PICK_RELEASE)
    target_compile_definitions(ElectryDSP PUBLIC
        ELECTRY_DECOUPLED_PICK_RELEASE=1)
endif()

if(MSVC)
    target_compile_options(ElectryDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(ElectryDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(ELECTRY_BUILD_PLUGIN)
    if(ELECTRY_JUCE_PATH)
        if(NOT EXISTS "${ELECTRY_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "ELECTRY_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${ELECTRY_JUCE_PATH}")
        endif()
        add_subdirectory("${ELECTRY_JUCE_PATH}"
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
        set(ELECTRY_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(ELECTRY_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Electry
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Electry Audio"
        BUNDLE_ID "audio.electry.synth"
        PLUGIN_MANUFACTURER_CODE Eltr
        PLUGIN_CODE Elc1
        PRODUCT_NAME "Electry"
        DESCRIPTION "Electry physically modeled dry electric guitar"
        FORMATS ${ELECTRY_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(Electry)

    juce_add_binary_data(ElectryAssets
        SOURCES Assets/electry-mahogany-satin-v2.png)

    target_sources(Electry PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Electry
        PRIVATE
            ElectryDSP
            ElectryAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(Electry
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# The committed demonstration WAVs under Docs/audio are rendered by this tool
# from the same JUCE-free path the plug-in uses, so they cannot drift away from
# what Electry actually produces. It is built by default, including on Linux CI,
# so it cannot rot unnoticed.
option(ELECTRY_BUILD_TOOLS "Build the demo and evaluation WAV renderers" ON)

if(ELECTRY_BUILD_TOOLS)
    add_executable(ElectryRenderDemos Tools/RenderDemos.cpp)
    target_link_libraries(ElectryRenderDemos PRIVATE ElectryDSP)
    target_compile_features(ElectryRenderDemos PRIVATE cxx_std_20)

    add_executable(ElectryRenderEvaluation Tools/RenderEvaluation.cpp)
    target_link_libraries(ElectryRenderEvaluation PRIVATE ElectryDSP)
    target_compile_features(ElectryRenderEvaluation PRIVATE cxx_std_20)
    target_compile_definitions(ElectryRenderEvaluation PRIVATE
        ELECTRY_EVALUATION_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/evaluation"
        ELECTRY_EVALUATION_PROJECT_VERSION="${PROJECT_VERSION}")

    if(MSVC)
        target_compile_options(ElectryRenderDemos PRIVATE /W4 /permissive-)
        target_compile_options(ElectryRenderEvaluation PRIVATE /W4 /permissive-)
    else()
        target_compile_options(ElectryRenderDemos PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(ElectryRenderEvaluation PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(ElectryEngineTests Tests/ElectryEngineTests.cpp)
    target_link_libraries(ElectryEngineTests PRIVATE ElectryDSP)
    target_compile_features(ElectryEngineTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(ElectryEngineTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(ElectryEngineTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME Electry.ElectryEngine COMMAND ElectryEngineTests)
    # Sanitizer instrumentation slows the multi-rate render matrix by roughly
    # an order of magnitude; keep the normal runaway guard tight while
    # retaining full coverage under instrumentation.
    # The unsanitized bound was 60 s. The picking-hand variation fixture added
    # in the 2026-08-07 pass renders two sets of twelve strokes separated by
    # twelve-second gaps, plus a rapid set, which put the suite at 54 s on a
    # loaded four-core Linux box and past 60 s on slower runners — so the guard
    # had started firing on ordinary growth rather than on the runaway it is
    # for. Raised to 300 s, keeping roughly the same margin over the real
    # runtime that the 60 s bound gave when it was chosen.
    if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
        set(ELECTRY_ENGINE_TEST_TIMEOUT 600)
    else()
        set(ELECTRY_ENGINE_TEST_TIMEOUT 300)
    endif()
    set_tests_properties(Electry.ElectryEngine PROPERTIES
        TIMEOUT ${ELECTRY_ENGINE_TEST_TIMEOUT})

    add_executable(ElectryFxTests Tests/ElectryFxTests.cpp)
    target_link_libraries(ElectryFxTests PRIVATE ElectryDSP)
    target_compile_features(ElectryFxTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(ElectryFxTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(ElectryFxTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME Electry.ElectryFx COMMAND ElectryFxTests)
    # This suite renders about 140 seconds of stereo audio across the normal
    # rate matrix and every nonlinear oversampling boundary. Debug and
    # instrumented builds need the wider guard; optimised builds still retain
    # a three-minute runaway bound.
    if(CMAKE_CXX_FLAGS MATCHES "fsanitize"
       OR CMAKE_BUILD_TYPE STREQUAL "Debug"
       OR NOT CMAKE_BUILD_TYPE)
        set(ELECTRY_FX_TEST_TIMEOUT 600)
    else()
        set(ELECTRY_FX_TEST_TIMEOUT 300)
    endif()
    set_tests_properties(Electry.ElectryFx PROPERTIES
        TIMEOUT ${ELECTRY_FX_TEST_TIMEOUT})

    if(ELECTRY_BUILD_TOOLS)
        # Guards the renderer that produces the committed demonstration WAVs:
        # it renders a short take to a temporary directory and checks that the
        # audio is finite, audible and written as a readable WAV.
        add_test(NAME Electry.RenderDemos
                 COMMAND ElectryRenderDemos --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-smoke")
        set_tests_properties(Electry.RenderDemos PROPERTIES TIMEOUT 60)

        # Render, exercise mutation-tested signal oracles, read every written
        # float payload back, parse the JSON manifest with CMake's own reader,
        # and inspect every WAV header and length.
        add_test(NAME Electry.RenderEvaluation
                 COMMAND ${CMAKE_COMMAND}
                         -DELECTRY_EVALUATION_RENDERER=$<TARGET_FILE:ElectryRenderEvaluation>
                         -DELECTRY_EVALUATION_DIR=${CMAKE_CURRENT_BINARY_DIR}/evaluation-test
                         -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ValidateEvaluation.cmake)
        set_tests_properties(Electry.RenderEvaluation PROPERTIES TIMEOUT 60)
    endif()

    if(ELECTRY_BUILD_PLUGIN)
        add_executable(ElectryPluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(ElectryPluginProcessorTests PRIVATE Electry)
        target_include_directories(ElectryPluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:Electry,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:Electry,INCLUDE_DIRECTORIES>")
        target_compile_features(ElectryPluginProcessorTests PRIVATE cxx_std_20)
        if(ELECTRY_MEASURED_BODY_RESPONSE)
            target_compile_definitions(ElectryPluginProcessorTests PRIVATE
                ELECTRY_MEASURED_BODY_RESPONSE=1)
        endif()

        if(MSVC)
            target_compile_options(ElectryPluginProcessorTests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(ElectryPluginProcessorTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME Electry.PluginProcessor COMMAND ElectryPluginProcessorTests)
        set_tests_properties(Electry.PluginProcessor PROPERTIES TIMEOUT 60)

        # Black-box the built product rather than linking the processor class:
        # this catches a broken VST3 bundle, wrapper or exported host contract.
        add_executable(ElectryVST3ArtifactSmokeTests
            Tests/VST3ArtifactSmokeTests.cpp)
        target_link_libraries(ElectryVST3ArtifactSmokeTests PRIVATE
            juce::juce_audio_processors_headless)
        target_compile_definitions(ElectryVST3ArtifactSmokeTests PRIVATE
            JUCE_PLUGINHOST_VST3=1
            ELECTRY_EXPECTED_VERSION="${PROJECT_VERSION}")
        target_compile_features(ElectryVST3ArtifactSmokeTests PRIVATE cxx_std_20)
        add_dependencies(ElectryVST3ArtifactSmokeTests Electry_VST3)

        if(MSVC)
            target_compile_options(ElectryVST3ArtifactSmokeTests PRIVATE
                /W4 /permissive-)
        else()
            target_compile_options(ElectryVST3ArtifactSmokeTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME Electry.VST3Artifact
                 COMMAND ElectryVST3ArtifactSmokeTests
                    "$<TARGET_GENEX_EVAL:Electry_VST3,$<TARGET_PROPERTY:Electry_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
        set_tests_properties(Electry.VST3Artifact PROPERTIES
            TIMEOUT 60)

        if(APPLE)
            # Load the exact AU binary through its exported factory and Core
            # Audio's process-local registration API. This avoids validating a
            # stale component discovered in the user's global AU directories.
            add_executable(ElectryAUArtifactSmokeTests
                Tests/ValidateAUArtifact.cpp)
            target_link_libraries(ElectryAUArtifactSmokeTests PRIVATE
                "-framework AudioToolbox"
                "-framework CoreFoundation")
            target_compile_features(ElectryAUArtifactSmokeTests PRIVATE cxx_std_20)
            target_compile_options(ElectryAUArtifactSmokeTests PRIVATE
                -Wall -Wextra -Wpedantic)
            add_dependencies(ElectryAUArtifactSmokeTests Electry_AU)

            add_test(NAME Electry.AUArtifact
                     COMMAND ElectryAUArtifactSmokeTests
                        "$<TARGET_FILE:Electry_AU>")
            set_tests_properties(Electry.AUArtifact PROPERTIES
                TIMEOUT 60)
        endif()
    endif()

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME Electry.PrepareBlindListening
                 COMMAND "${Python3_EXECUTABLE}"
                         "${CMAKE_CURRENT_SOURCE_DIR}/Tests/PrepareBlindListeningTests.py")
        add_test(NAME Electry.ScoreBlindListening
                 COMMAND "${Python3_EXECUTABLE}"
                         "${CMAKE_CURRENT_SOURCE_DIR}/Tests/ScoreBlindListeningTests.py")
        set_tests_properties(
            Electry.PrepareBlindListening Electry.ScoreBlindListening
            PROPERTIES TIMEOUT 60)
    endif()
endif()
