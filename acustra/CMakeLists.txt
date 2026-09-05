cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(ACUSTRA_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# Establish the deployment target and architectures before project(), so
# CMake's compiler probes use the same settings as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(ACUSTRA_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# JUCE's macOS AU and Standalone wrappers compile C/Objective-C translation
# units, so non-Xcode generators require both languages here.
project(Acustra VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ACUSTRA_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
option(ACUSTRA_BUILD_TOOLS "Build offline Acustra tools" ON)
option(ACUSTRA_BUILD_PLUGIN_REFERENCE_HOST
    "Build the optional JUCE VST3 reference renderer/editor" OFF)
set(ACUSTRA_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

file(GLOB ACUSTRA_BANK_PARTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP/SampleBank/BankData-*.cpp")

add_library(AcustraDSP STATIC
    Source/DSP/AcustraEngine.cpp
    Source/DSP/AcustraEngine.h)

add_library(AcustraReferenceBank STATIC EXCLUDE_FROM_ALL
    Source/DSP/SampleBank/EmbeddedGuitarBank.cpp
    Source/DSP/SampleBank/EmbeddedGuitarBank.h
    Source/DSP/SampleBank/GeneratedBankData.h
    Source/DSP/SampleBank/BankData.cpp
    ${ACUSTRA_BANK_PARTS})

target_include_directories(AcustraDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(AcustraDSP PUBLIC cxx_std_20)
set_target_properties(AcustraDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(AcustraReferenceBank
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(AcustraReferenceBank PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(AcustraDSP PRIVATE /W4 /permissive-)
    target_compile_options(AcustraReferenceBank PRIVATE /W4 /permissive-)
else()
    target_compile_options(AcustraDSP PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(AcustraReferenceBank PRIVATE
        -Wall -Wextra -Wpedantic)
endif()

if(ACUSTRA_BUILD_PLUGIN)
    if(ACUSTRA_JUCE_PATH)
        if(NOT EXISTS "${ACUSTRA_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "ACUSTRA_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${ACUSTRA_JUCE_PATH}")
        endif()
        add_subdirectory("${ACUSTRA_JUCE_PATH}"
                         "${CMAKE_CURRENT_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            # JUCE 8.0.14, pinned to its immutable release commit and archive
            # checksum so automated builds cannot follow a moved tag.
            URL https://github.com/juce-framework/JUCE/archive/2cdfca8feb300fb424002ba2c2751569e5bacb64.tar.gz
            URL_HASH SHA256=814019716960a16f8f850a1593eeb56e2f21910aff2d673f7d3513b4ebaf251d)
        FetchContent_MakeAvailable(JUCE)
    endif()

    if(APPLE)
        set(ACUSTRA_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(ACUSTRA_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Acustra
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Protocodus"
        COMPANY_WEBSITE "https://protocodus.cz"
        COMPANY_EMAIL "protocodus@proton.me"
        COMPANY_COPYRIGHT "Copyright (c) 2026 Acustra contributors"
        BUNDLE_ID "cz.protocodus.acustra"
        PLUGIN_MANUFACTURER_CODE Acst
        PLUGIN_CODE Acu1
        PRODUCT_NAME "Acustra"
        DESCRIPTION "Acustra acoustic guitar instrument"
        FORMATS ${ACUSTRA_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_add_binary_data(AcustraUIAssets
        NAMESPACE AcustraUIAssets
        HEADER_NAME AcustraUIAssets.h
        SOURCES Assets/UI/cedar-background.png)

    juce_generate_juce_header(Acustra)

    target_sources(Acustra PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Acustra
        PRIVATE
            AcustraDSP
            AcustraUIAssets
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(Acustra
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    if(ACUSTRA_BUILD_PLUGIN_REFERENCE_HOST)
        juce_add_console_app(AcustraPluginReferenceHost
            PRODUCT_NAME "Acustra Plugin Reference Host")
        target_sources(AcustraPluginReferenceHost
            PRIVATE Tools/PluginReferenceHost.cpp)
        target_compile_definitions(AcustraPluginReferenceHost PRIVATE
            JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=1)
        target_link_libraries(AcustraPluginReferenceHost PRIVATE
            juce::juce_audio_processors juce::juce_audio_formats
            juce::juce_recommended_config_flags)
        if(BUILD_TESTING)
            add_test(NAME Acustra.PluginReferenceHost
                COMMAND AcustraPluginReferenceHost --self-test)
        endif()
    endif()
elseif(ACUSTRA_BUILD_PLUGIN_REFERENCE_HOST)
    message(FATAL_ERROR
        "ACUSTRA_BUILD_PLUGIN_REFERENCE_HOST requires ACUSTRA_BUILD_PLUGIN=ON")
endif()

# The committed demonstrations are rendered by the same JUCE-free engine used
# by the plug-in. Build the renderer in CI so its source cannot silently rot.
if(ACUSTRA_BUILD_TOOLS)
    add_executable(AcustraRenderDemos Tools/RenderDemos.cpp)
    target_link_libraries(AcustraRenderDemos PRIVATE AcustraDSP)
    target_compile_features(AcustraRenderDemos PRIVATE cxx_std_20)

    add_executable(AcustraPhysicalFitRenderer Tools/PhysicalFitRenderer.cpp)
    target_link_libraries(AcustraPhysicalFitRenderer
        PRIVATE AcustraDSP AcustraReferenceBank)
    target_compile_features(AcustraPhysicalFitRenderer PRIVATE cxx_std_20)

    add_executable(AcustraBridgeProbe Tools/BridgeProbe.cpp)
    target_link_libraries(AcustraBridgeProbe PRIVATE AcustraDSP)
    target_compile_features(AcustraBridgeProbe PRIVATE cxx_std_20)

    add_executable(AcustraPerformanceRenderer Tools/PerformanceRenderer.cpp)
    target_link_libraries(AcustraPerformanceRenderer PRIVATE AcustraDSP)

    if(MSVC)
        target_compile_options(AcustraRenderDemos PRIVATE /W4 /permissive-)
        target_compile_options(AcustraPhysicalFitRenderer
            PRIVATE /W4 /permissive-)
        target_compile_options(AcustraBridgeProbe PRIVATE /W4 /permissive-)
    else()
        target_compile_options(AcustraRenderDemos PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(AcustraPhysicalFitRenderer PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(AcustraBridgeProbe PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(AcustraSampleBankTests Tests/SampleBankTests.cpp)
    target_link_libraries(AcustraSampleBankTests PRIVATE AcustraReferenceBank)
    target_compile_features(AcustraSampleBankTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(AcustraSampleBankTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(AcustraSampleBankTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME Acustra.SampleBank COMMAND AcustraSampleBankTests)
    set_tests_properties(Acustra.SampleBank PROPERTIES TIMEOUT 90)

    add_executable(AcustraEngineTests Tests/AcustraEngineTests.cpp)
    target_link_libraries(AcustraEngineTests PRIVATE AcustraDSP)
    target_compile_features(AcustraEngineTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(AcustraEngineTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(AcustraEngineTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME Acustra.Engine COMMAND AcustraEngineTests)
    set_tests_properties(Acustra.Engine PROPERTIES TIMEOUT 300)

    add_executable(AcustraCaptureTests Tests/CaptureTests.cpp)
    target_link_libraries(AcustraCaptureTests PRIVATE AcustraDSP)
    add_test(NAME Acustra.Capture COMMAND AcustraCaptureTests)
    set_tests_properties(Acustra.Capture PROPERTIES TIMEOUT 90)

    add_executable(AcustraMeasuredSteelBridgeTests Tests/MeasuredSteelBridgeTests.cpp)
    target_link_libraries(AcustraMeasuredSteelBridgeTests PRIVATE AcustraDSP)
    add_test(NAME Acustra.MeasuredSteelBridge COMMAND AcustraMeasuredSteelBridgeTests)
    set_tests_properties(Acustra.MeasuredSteelBridge PROPERTIES TIMEOUT 90)

    if(ACUSTRA_BUILD_TOOLS)
        add_test(NAME Acustra.RenderDemos
                 COMMAND AcustraRenderDemos --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-smoke")
        set_tests_properties(Acustra.RenderDemos PROPERTIES TIMEOUT 60)

        add_test(NAME Acustra.PhysicalFitRenderer
                 COMMAND AcustraPhysicalFitRenderer --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/physical-fit-smoke"
                         1 1 1 0 .03
                         1 1 1 1 1 0
                         1 1 1 1 1 1 0
                         1 1 .0061 -.030 1.3 .001 1000 .02 0 80 .0008)
        set_tests_properties(Acustra.PhysicalFitRenderer PROPERTIES TIMEOUT 120)

        add_test(NAME Acustra.BridgeProbe
                 COMMAND AcustraBridgeProbe
                         "${CMAKE_CURRENT_BINARY_DIR}/bridge-probe-smoke.f32"
                         nylon on 0.5 auditorium 40)
        set_tests_properties(Acustra.BridgeProbe PROPERTIES TIMEOUT 30)

        # ExtractExcitation.py's self-test fails only on a synthesised note
        # whose excitation is known exactly, so what this pins is the
        # estimator, not the engine; the probe renders beside it are reported,
        # not gated (the engine's own released shape departs from the ideal
        # comb, which the tool's docstring records). Registered only where the
        # interpreter and the two libraries it imports are present, so a build
        # without them still configures.
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            execute_process(COMMAND "${Python3_EXECUTABLE}" -c
                                    "import numpy, scipy.io.wavfile"
                            RESULT_VARIABLE ACUSTRA_EXCITATION_PY_DEPS
                            OUTPUT_QUIET ERROR_QUIET)
            if(ACUSTRA_EXCITATION_PY_DEPS EQUAL 0)
                add_test(NAME Acustra.PhysicalScorer
                         COMMAND "${Python3_EXECUTABLE}"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/Tools/FitPhysicalModel.py"
                                 --self-test)
                set_tests_properties(Acustra.PhysicalScorer PROPERTIES TIMEOUT 180)
                add_test(NAME Acustra.PerformanceBenchmark
                         COMMAND "${Python3_EXECUTABLE}"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/Tools/BenchmarkPerformances.py"
                                 --self-test --renderer "$<TARGET_FILE:AcustraPerformanceRenderer>")
                set_tests_properties(Acustra.PerformanceBenchmark PROPERTIES TIMEOUT 180)
                add_test(NAME Acustra.TechniqueBenchmark
                         COMMAND "${Python3_EXECUTABLE}"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/Tools/BenchmarkTechniqueNotes.py"
                                 --self-test)
                set_tests_properties(Acustra.TechniqueBenchmark PROPERTIES TIMEOUT 180)
                add_test(NAME Acustra.HammerOnBenchmark
                         COMMAND "${Python3_EXECUTABLE}"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/Tools/BenchmarkHammerOnNotes.py"
                                 --self-test)
                set_tests_properties(Acustra.HammerOnBenchmark PROPERTIES TIMEOUT 180)
                add_test(NAME Acustra.BodyForcePair
                         COMMAND "${Python3_EXECUTABLE}"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/Tools/AuditBodyForcePair.py"
                                 --self-test)
                set_tests_properties(Acustra.BodyForcePair PROPERTIES TIMEOUT 180)
                add_test(NAME Acustra.GenerateBodyForcePair
                         COMMAND "${Python3_EXECUTABLE}"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/Tools/GenerateBodyForcePair.py"
                                 --self-test)
                set_tests_properties(Acustra.GenerateBodyForcePair PROPERTIES TIMEOUT 180)
                add_test(NAME Acustra.MeasuredSteelFit
                         COMMAND "${Python3_EXECUTABLE}"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/Tools/GenerateMeasuredSteelBridge.py"
                                 --self-test)
                set_tests_properties(Acustra.MeasuredSteelFit PROPERTIES TIMEOUT 180)
                add_test(NAME Acustra.ExtractExcitation
                         COMMAND "${Python3_EXECUTABLE}"
                                 "${CMAKE_CURRENT_SOURCE_DIR}/Tools/ExtractExcitation.py"
                                 --self-test
                                 --probe "$<TARGET_FILE:AcustraBridgeProbe>"
                                 --work "${CMAKE_CURRENT_BINARY_DIR}/excitation-selftest")
                set_tests_properties(Acustra.ExtractExcitation PROPERTIES TIMEOUT 180)
            endif()
        endif()
    endif()

    if(ACUSTRA_BUILD_PLUGIN)
        add_executable(AcustraPluginProcessorTests
            Tests/PluginProcessorTests.cpp)
        target_link_libraries(AcustraPluginProcessorTests PRIVATE Acustra)
        target_include_directories(AcustraPluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:Acustra,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:Acustra,INCLUDE_DIRECTORIES>")
        target_compile_features(AcustraPluginProcessorTests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(AcustraPluginProcessorTests PRIVATE
                /W4 /permissive-)
        else()
            target_compile_options(AcustraPluginProcessorTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME Acustra.PluginProcessor
                 COMMAND AcustraPluginProcessorTests)
        set_tests_properties(Acustra.PluginProcessor PROPERTIES TIMEOUT 90)
    endif()
endif()
