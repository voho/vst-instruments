cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(NEURAMAR_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(NEURAMAR_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU and
# Standalone) compile C/Objective-C sources with non-Xcode generators too.
project(Neuramar VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NEURAMAR_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(NEURAMAR_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(NEURAMAR_DSP_SOURCES
    Source/DSP/NeuralModel.cpp
    Source/DSP/SampleLearner.cpp
    Source/DSP/ModelVisualisation.cpp
    Source/DSP/NeuramarEngine.cpp)
set(NEURAMAR_DSP_HEADERS
    Source/DSP/AirFilterbank.h
    Source/DSP/NeuralModel.h
    Source/DSP/SampleLearner.h
    Source/DSP/SpectralEnvelope.h
    Source/DSP/ModelVisualisation.h
    Source/DSP/NeuramarEngine.h)

add_library(NeuramarDSP STATIC
    ${NEURAMAR_DSP_SOURCES}
    ${NEURAMAR_DSP_HEADERS})

target_include_directories(NeuramarDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(NeuramarDSP PUBLIC cxx_std_20)
set_target_properties(NeuramarDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(NeuramarDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(NeuramarDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NEURAMAR_BUILD_PLUGIN)
    if(NEURAMAR_JUCE_PATH)
        if(NOT EXISTS "${NEURAMAR_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "NEURAMAR_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${NEURAMAR_JUCE_PATH}")
        endif()
        add_subdirectory("${NEURAMAR_JUCE_PATH}"
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
        set(NEURAMAR_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(NEURAMAR_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Neuramar
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Neuramar Audio"
        BUNDLE_ID "audio.neuramar.synth"
        PLUGIN_MANUFACTURER_CODE Nram
        PLUGIN_CODE Nrm1
        PRODUCT_NAME "Neuramar"
        DESCRIPTION "Sample-learned neural synthesizer"
        FORMATS ${NEURAMAR_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(Neuramar)

    target_sources(Neuramar PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(Neuramar
        PRIVATE
            NeuramarDSP
            juce::juce_audio_formats
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(Neuramar
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

if(BUILD_TESTING)
    add_executable(NeuramarEngineTests Tests/NeuramarEngineTests.cpp)
    target_link_libraries(NeuramarEngineTests PRIVATE NeuramarDSP)
    target_compile_features(NeuramarEngineTests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(NeuramarEngineTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(NeuramarEngineTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME Neuramar.NeuralEngine COMMAND NeuramarEngineTests)
    # Sanitizers make training and high-rate render fixtures substantially
    # slower. The universal macOS ASan/UBSan configuration has been observed to
    # take roughly 18 minutes after the full 128-frame quality suite, while a
    # native arm64 sanitizer build remains much faster.
    if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
        set(NEURAMAR_ENGINE_TEST_TIMEOUT 1800)
    else()
        set(NEURAMAR_ENGINE_TEST_TIMEOUT 30)
    endif()
    set_tests_properties(Neuramar.NeuralEngine PROPERTIES
        TIMEOUT ${NEURAMAR_ENGINE_TEST_TIMEOUT})

    add_executable(NeuramarResynthesisQualityTests
        Tests/ResynthesisQualityTests.cpp)
    target_link_libraries(NeuramarResynthesisQualityTests PRIVATE NeuramarDSP)
    target_compile_features(NeuramarResynthesisQualityTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(NeuramarResynthesisQualityTests PRIVATE
            /W4 /permissive-)
    else()
        target_compile_options(NeuramarResynthesisQualityTests PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME Neuramar.ResynthesisQuality
        COMMAND NeuramarResynthesisQualityTests)
    set_tests_properties(Neuramar.ResynthesisQuality PROPERTIES TIMEOUT 60)

    if(NEURAMAR_BUILD_PLUGIN)
        add_executable(NeuramarPluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(NeuramarPluginProcessorTests PRIVATE Neuramar)
        target_include_directories(NeuramarPluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:Neuramar,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:Neuramar,INCLUDE_DIRECTORIES>")
        target_compile_features(NeuramarPluginProcessorTests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(NeuramarPluginProcessorTests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(NeuramarPluginProcessorTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME Neuramar.PluginProcessor COMMAND NeuramarPluginProcessorTests)
        set_tests_properties(Neuramar.PluginProcessor PROPERTIES TIMEOUT 60)

        if(APPLE)
            add_executable(NeuramarBundleFactorySmoke
                Tests/BundleFactorySmoke.cpp)
            target_compile_features(NeuramarBundleFactorySmoke PRIVATE cxx_std_20)
            target_link_libraries(NeuramarBundleFactorySmoke PRIVATE
                ${CMAKE_DL_LIBS})
            add_dependencies(NeuramarBundleFactorySmoke
                Neuramar_VST3 Neuramar_AU)

            if(MSVC)
                target_compile_options(NeuramarBundleFactorySmoke PRIVATE
                    /W4 /permissive-)
            else()
                target_compile_options(NeuramarBundleFactorySmoke PRIVATE
                    -Wall -Wextra -Wpedantic)
            endif()

            add_test(NAME Neuramar.VST3Factory
                COMMAND NeuramarBundleFactorySmoke
                    "$<TARGET_FILE:Neuramar_VST3>" GetPluginFactory invoke)
            add_test(NAME Neuramar.AUFactory
                COMMAND NeuramarBundleFactorySmoke
                    "$<TARGET_FILE:Neuramar_AU>" NeuramarAUFactory lookup)
            set_tests_properties(
                Neuramar.VST3Factory Neuramar.AUFactory
                PROPERTIES TIMEOUT 15)
        endif()
    endif()
endif()
