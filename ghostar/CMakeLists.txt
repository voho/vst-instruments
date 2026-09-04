cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(GHOSTAR_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use
# the same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(GHOSTAR_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU,
# Standalone) compile C/Objective-C sources. Restricting this to CXX breaks
# non-Xcode generators (Ninja/Makefiles).
project(Ghostar VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GHOSTAR_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(GHOSTAR_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(GHOSTAR_DSP_SOURCES
    Source/DSP/GhostarEngine.cpp
    Source/DSP/GhostarPresets.cpp
    Source/DSP/SpiritNoise.cpp)
set(GHOSTAR_DSP_HEADERS
    Source/DSP/GhostarEngine.h
    Source/DSP/GhostarPresets.h
    Source/DSP/SpiritNoise.h)

add_library(GhostarDSP STATIC
    ${GHOSTAR_DSP_SOURCES}
    ${GHOSTAR_DSP_HEADERS})

target_include_directories(GhostarDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(GhostarDSP PUBLIC cxx_std_20)
set_target_properties(GhostarDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(GhostarDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(GhostarDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(GHOSTAR_BUILD_PLUGIN)
    if(GHOSTAR_JUCE_PATH)
        if(NOT EXISTS "${GHOSTAR_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "GHOSTAR_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${GHOSTAR_JUCE_PATH}")
        endif()
        add_subdirectory("${GHOSTAR_JUCE_PATH}"
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
        set(GHOSTAR_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(GHOSTAR_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Ghostar
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Protocodus"
        COMPANY_WEBSITE "https://protocodus.cz"
        COMPANY_EMAIL "protocodus@proton.me"
        COMPANY_COPYRIGHT "Copyright (c) 2026 Ghostar contributors"
        BUNDLE_ID "cz.protocodus.ghostar"
        PLUGIN_MANUFACTURER_CODE Gstr
        PLUGIN_CODE Gst1
        PRODUCT_NAME "Ghostar"
        DESCRIPTION "Ghostar circuit-modelled monophonic dual-filter synthesizer"
        FORMATS ${GHOSTAR_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT
            "Ghostar uses audio input for the rear External Audio jack."
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(Ghostar)

    target_sources(Ghostar PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Ghostar
        PRIVATE
            GhostarDSP
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(Ghostar
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# The committed demonstration WAVs under Docs/audio are rendered by this tool
# from the same JUCE-free engine the plug-in runs, so they cannot drift away
# from what Ghostar actually sounds like. It is built by default, including on
# Linux CI, so it cannot rot unnoticed.
option(GHOSTAR_BUILD_TOOLS "Build the JUCE-free audio render tools" ON)

if(GHOSTAR_BUILD_TOOLS)
    add_executable(GhostarRenderDemos Tools/RenderDemos.cpp)
    target_link_libraries(GhostarRenderDemos PRIVATE GhostarDSP)
    target_compile_features(GhostarRenderDemos PRIVATE cxx_std_20)

    # Plan Step 5's measurement: the block-latching (zipper) audit over the
    # published continuous travels, against a sample-accurate reference.
    add_executable(GhostarZipperAudit Tools/ZipperAudit.cpp)
    target_link_libraries(GhostarZipperAudit PRIVATE GhostarDSP)
    target_compile_features(GhostarZipperAudit PRIVATE cxx_std_20)

    # Plan Step 1's measurement: worst-case aliasing strokes against a
    # high-rate ground truth decimated to the shipping rate.
    add_executable(GhostarAliasAudit Tools/AliasAudit.cpp)
    target_link_libraries(GhostarAliasAudit PRIVATE GhostarDSP)
    target_compile_features(GhostarAliasAudit PRIVATE cxx_std_20)

    # Inventory-only preflight for the same-unit hardware capture that will
    # close the remaining voiced circuit seams. The complete acquisition
    # protocol lives at the head of the tool that enforces it.
    add_executable(GhostarValidateCalibrationCapture
        Tools/ValidateCalibrationCapture.cpp)
    target_compile_features(GhostarValidateCalibrationCapture PRIVATE cxx_std_20)

    foreach(target GhostarRenderDemos GhostarZipperAudit GhostarAliasAudit
                   GhostarValidateCalibrationCapture)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${target} PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
endif()

if(BUILD_TESTING)
    add_executable(GhostarEngineTests Tests/GhostarEngineTests.cpp)
    target_link_libraries(GhostarEngineTests PRIVATE GhostarDSP)
    target_compile_features(GhostarEngineTests PRIVATE cxx_std_20)

    add_executable(GhostarCircuitTests Tests/GhostarCircuitTests.cpp)
    target_link_libraries(GhostarCircuitTests PRIVATE GhostarDSP)
    target_compile_features(GhostarCircuitTests PRIVATE cxx_std_20)

    # The alias audit's measuring instrument, on signals whose alias content
    # is known exactly. Links nothing, so it runs whether or not the tools
    # are built: an instrument that cannot see what it claims to measure is
    # worse than no instrument, and that failure is invisible from its output.
    add_executable(GhostarAliasMetricTests Tests/GhostarAliasMetricTests.cpp)
    target_include_directories(GhostarAliasMetricTests
                               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Tools)
    target_compile_features(GhostarAliasMetricTests PRIVATE cxx_std_20)

    foreach(target GhostarEngineTests GhostarCircuitTests
                   GhostarAliasMetricTests)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()

    add_test(NAME Ghostar.Engine COMMAND GhostarEngineTests)
    add_test(NAME Ghostar.Circuit COMMAND GhostarCircuitTests)
    add_test(NAME Ghostar.AliasMetric COMMAND GhostarAliasMetricTests)
    set_tests_properties(Ghostar.Engine PROPERTIES TIMEOUT 300)
    set_tests_properties(Ghostar.Circuit PROPERTIES TIMEOUT 300)
    set_tests_properties(Ghostar.AliasMetric PROPERTIES TIMEOUT 300)

    if(GHOSTAR_BUILD_TOOLS)
        # Guards the renderer that produces the committed demonstration WAVs:
        # it renders a short take to a temporary directory and checks that the
        # audio is finite, audible and written as a readable WAV.
        add_test(NAME Ghostar.RenderDemos
                 COMMAND GhostarRenderDemos --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-smoke")
        set_tests_properties(Ghostar.RenderDemos PROPERTIES TIMEOUT 60)

        # A full render run twice into the same directory proves the
        # atomic-replacement path — a rename over an existing WAV and README —
        # on every CI platform, not only POSIX. std::filesystem::rename is
        # specified with POSIX replace semantics, and this pair keeps that
        # from being a claim CI never exercises on Windows.
        add_test(NAME Ghostar.RenderDemosFullRender
                 COMMAND GhostarRenderDemos
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-full")
        add_test(NAME Ghostar.RenderDemosRerender
                 COMMAND GhostarRenderDemos
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-full")
        # ASan/UBSan makes the complete deterministic corpus roughly six
        # times slower than Release; leave headroom for instrumented CI.
        set_tests_properties(Ghostar.RenderDemosFullRender PROPERTIES TIMEOUT 180)
        set_tests_properties(Ghostar.RenderDemosRerender PROPERTIES TIMEOUT 180
                             DEPENDS Ghostar.RenderDemosFullRender)

        # Keeps the zipper-audit strokes valid (finite, non-silent) on CI;
        # the full audit is run by hand when its table is being updated.
        add_test(NAME Ghostar.ZipperAuditSmoke
                 COMMAND GhostarZipperAudit --smoke)
        set_tests_properties(Ghostar.ZipperAuditSmoke PROPERTIES TIMEOUT 120)

        # Same guard for the alias-audit strokes and reference pipeline.
        add_test(NAME Ghostar.AliasAuditSmoke
                 COMMAND GhostarAliasAudit --smoke)
        set_tests_properties(Ghostar.AliasAuditSmoke PROPERTIES TIMEOUT 300)

        add_test(NAME Ghostar.ValidateCalibrationCapture
                 COMMAND GhostarValidateCalibrationCapture --self-test)
        set_tests_properties(Ghostar.ValidateCalibrationCapture PROPERTIES
            TIMEOUT 30)
    endif()

    if(GHOSTAR_BUILD_PLUGIN)
        add_executable(GhostarPluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(GhostarPluginProcessorTests PRIVATE Ghostar)
        target_include_directories(GhostarPluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:Ghostar,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:juce::juce_audio_utils,INTERFACE_INCLUDE_DIRECTORIES>")
        target_compile_features(GhostarPluginProcessorTests PRIVATE cxx_std_20)

        add_test(NAME Ghostar.PluginProcessor COMMAND GhostarPluginProcessorTests)
        set_tests_properties(Ghostar.PluginProcessor PROPERTIES TIMEOUT 60)
    endif()
endif()
