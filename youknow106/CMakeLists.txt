cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(YOUKNOW106_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(YOUKNOW106_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU, Standalone)
# compile C/Objective-C sources. Restricting this to CXX breaks non-Xcode
# generators (Ninja/Makefiles) with a missing CMAKE_C_COMPILE_OBJECT error.
project(YouKnow106 VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(YOUKNOW106_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(YOUKNOW106_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(YOUKNOW106_DSP_SOURCES
    Source/DSP/YouKnow106Engine.cpp
    Source/DSP/YouKnow106Chorus.cpp
    Source/DSP/YouKnow106Panel.cpp
    Source/DSP/YouKnow106Presets.cpp
    Source/DSP/YouKnow106SysEx.cpp)
set(YOUKNOW106_DSP_HEADERS
    Source/DSP/YouKnow106Engine.h
    Source/DSP/YouKnow106Chorus.h
    Source/DSP/YouKnow106Panel.h)

add_library(YouKnow106DSP STATIC
    ${YOUKNOW106_DSP_SOURCES}
    ${YOUKNOW106_DSP_HEADERS})

target_include_directories(YouKnow106DSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(YouKnow106DSP PUBLIC cxx_std_20)
set_target_properties(YouKnow106DSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(YouKnow106DSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(YouKnow106DSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(YOUKNOW106_BUILD_PLUGIN)
    if(YOUKNOW106_JUCE_PATH)
        if(NOT EXISTS "${YOUKNOW106_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "YOUKNOW106_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${YOUKNOW106_JUCE_PATH}")
        endif()
        add_subdirectory("${YOUKNOW106_JUCE_PATH}"
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
        set(YOUKNOW106_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(YOUKNOW106_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(YouKnow106
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Pluto Audio"
        BUNDLE_ID "audio.youknow106.synth"
        PLUGIN_MANUFACTURER_CODE Ykno
        PLUGIN_CODE Yk06
        PRODUCT_NAME "YouKnow106"
        DESCRIPTION "YouKnow106 circuit-modelled six-voice DCO polysynth"
        FORMATS ${YOUKNOW106_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        # The instrument can send its panel to hardware as a patch dump, so it
        # needs somewhere to send it.
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_generate_juce_header(YouKnow106)

    # Keep the photographed surface treatment inside every plug-in format and
    # the standalone app; hosts must never need to locate a loose UI asset.
    juce_add_binary_data(YouKnow106Assets
        SOURCES
            Assets/used-charcoal-plastic.png)

    target_sources(YouKnow106 PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(YouKnow106
        PRIVATE
            YouKnow106DSP
            YouKnow106Assets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(YouKnow106
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# The committed demonstration WAVs under Docs/audio are rendered by this tool
# from the same JUCE-free engine the plug-in uses, so they cannot drift away
# from what YouKnow106 actually produces. It is built by default, including on
# Linux CI, so it cannot rot unnoticed.
option(YOUKNOW106_BUILD_TOOLS "Build the JUCE-free audio render/audit tools" ON)

if(YOUKNOW106_BUILD_TOOLS)
    find_package(Threads REQUIRED)

    add_executable(YouKnow106RenderDemos Tools/RenderDemos.cpp)
    target_link_libraries(YouKnow106RenderDemos PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106RenderDemos PRIVATE cxx_std_20)

    add_executable(YouKnow106RenderSota Tools/RenderSotaComparisons.cpp)
    target_link_libraries(YouKnow106RenderSota PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106RenderSota PRIVATE cxx_std_20)

    add_executable(YouKnow106RenderFidelityFixes Tools/RenderFidelityFixes.cpp)
    target_link_libraries(YouKnow106RenderFidelityFixes PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106RenderFidelityFixes PRIVATE cxx_std_20)

    add_executable(YouKnow106RenderRealismFixes Tools/RenderRealismFixes.cpp)
    target_link_libraries(YouKnow106RenderRealismFixes PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106RenderRealismFixes PRIVATE cxx_std_20)

    add_executable(YouKnow106AuditFactoryPresets Tools/AuditFactoryPresets.cpp)
    target_link_libraries(YouKnow106AuditFactoryPresets
        PRIVATE YouKnow106DSP Threads::Threads)
    target_compile_features(YouKnow106AuditFactoryPresets PRIVATE cxx_std_20)
    target_compile_definitions(YouKnow106AuditFactoryPresets PRIVATE
        "YOUKNOW106_FACTORY_AUDIT_DEFAULT_OUTPUT=\"${CMAKE_CURRENT_SOURCE_DIR}/Docs/audio/factory-presets\"")

    if(MSVC)
        target_compile_options(YouKnow106RenderDemos PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106RenderSota PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106RenderFidelityFixes PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106RenderRealismFixes PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106AuditFactoryPresets PRIVATE
            /W4 /permissive-)
    else()
        target_compile_options(YouKnow106RenderDemos PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106RenderSota PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106RenderFidelityFixes PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106RenderRealismFixes PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106AuditFactoryPresets PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(YouKnow106EngineTests Tests/YouKnow106EngineTests.cpp)
    target_link_libraries(YouKnow106EngineTests PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106EngineTests PRIVATE cxx_std_20)

    add_executable(YouKnow106CircuitTests Tests/YouKnow106CircuitTests.cpp)
    target_link_libraries(YouKnow106CircuitTests PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106CircuitTests PRIVATE cxx_std_20)

    add_executable(YouKnow106SysExTests Tests/YouKnow106SysExTests.cpp)
    target_link_libraries(YouKnow106SysExTests PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106SysExTests PRIVATE cxx_std_20)

    foreach(target YouKnow106EngineTests YouKnow106CircuitTests YouKnow106SysExTests)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()

    add_test(NAME YouKnow106.Engine COMMAND YouKnow106EngineTests)
    add_test(NAME YouKnow106.Circuit COMMAND YouKnow106CircuitTests)
    add_test(NAME YouKnow106.SysEx COMMAND YouKnow106SysExTests)

    if(YOUKNOW106_BUILD_TOOLS)
        # Guards the renderer that produces the committed demonstration WAVs:
        # it renders a short take to a temporary directory and checks that the
        # audio is finite, audible and written as a readable WAV.
        add_test(NAME YouKnow106.RenderDemos
                 COMMAND YouKnow106RenderDemos --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-smoke")
        set_tests_properties(YouKnow106.RenderDemos PROPERTIES TIMEOUT 60)

        # Fast integration coverage for the long-form factory gain auditor. It
        # uses the same mappings, meters, ownership guard and fixed preview
        # path as a full run, with a two-preset shortened score.
        add_test(NAME YouKnow106.AuditFactoryPresets
                 COMMAND YouKnow106AuditFactoryPresets --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/factory-audit-smoke")
        set_tests_properties(YouKnow106.AuditFactoryPresets PROPERTIES TIMEOUT 60)
    endif()
    # The engine suite renders the complete factory-balance corpus, while the
    # circuit suite re-solves reference ODE models at high internal rates.
    # Address/undefined sanitizers make both substantially slower; retain
    # runaway guards with enough margin for universal macOS builds and loaded
    # CI workers instead of intermittently timing out valid coverage.
    if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
        set(YOUKNOW106_ENGINE_TEST_TIMEOUT 600)
        set(YOUKNOW106_CIRCUIT_TEST_TIMEOUT 900)
        # The suites also have to know, because one of them asserts the engine
        # renders faster than realtime and instrumentation makes that false.
        # Compiler macros do not cover the cases this branch does -- GCC with
        # only -fsanitize=undefined defines nothing a test can see -- so the
        # build system tells them directly and the two decisions cannot drift
        # apart.
        foreach(target YouKnow106EngineTests YouKnow106CircuitTests YouKnow106SysExTests)
            target_compile_definitions(${target} PRIVATE YOUKNOW106_SANITIZER_BUILD=1)
        endforeach()
    else()
        set(YOUKNOW106_ENGINE_TEST_TIMEOUT 300)
        set(YOUKNOW106_CIRCUIT_TEST_TIMEOUT 90)
    endif()
    set_tests_properties(YouKnow106.Engine PROPERTIES
        TIMEOUT ${YOUKNOW106_ENGINE_TEST_TIMEOUT})
    set_tests_properties(YouKnow106.Circuit PROPERTIES
        TIMEOUT ${YOUKNOW106_CIRCUIT_TEST_TIMEOUT})

    if(YOUKNOW106_BUILD_PLUGIN)
        add_executable(YouKnow106PluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(YouKnow106PluginProcessorTests PRIVATE YouKnow106)
        target_include_directories(YouKnow106PluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:YouKnow106,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:YouKnow106,INCLUDE_DIRECTORIES>")
        target_compile_features(YouKnow106PluginProcessorTests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(YouKnow106PluginProcessorTests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(YouKnow106PluginProcessorTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME YouKnow106.PluginProcessor COMMAND YouKnow106PluginProcessorTests)
        set_tests_properties(YouKnow106.PluginProcessor PROPERTIES TIMEOUT 60)
    endif()
endif()
