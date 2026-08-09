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

    # Focused build-to-build comparisons keep their raw float32 baseline and
    # identify the exact DSP source set used to compile each side. Reconfigure
    # when any fingerprinted source changes so a normal `cmake --build` cannot
    # leave stale source metadata in an after manifest.
    set(YOUKNOW106_DSP_FINGERPRINT_INPUT "")
    set(YOUKNOW106_DSP_FINGERPRINT_FILES
        ${YOUKNOW106_DSP_SOURCES}
        ${YOUKNOW106_DSP_HEADERS})
    foreach(source IN LISTS YOUKNOW106_DSP_FINGERPRINT_FILES)
        file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${source}" source_hash)
        string(APPEND YOUKNOW106_DSP_FINGERPRINT_INPUT
            "${source}:${source_hash}\n")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    endforeach()
    string(SHA256 YOUKNOW106_DSP_SOURCE_SHA256
        "${YOUKNOW106_DSP_FINGERPRINT_INPUT}")

    add_executable(YouKnow106RenderRealismComparison
        Tools/RenderRealismComparison.cpp
        Tools/RealismComparisonSupport.h)
    target_link_libraries(YouKnow106RenderRealismComparison
        PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106RenderRealismComparison PRIVATE cxx_std_20)
    target_compile_definitions(YouKnow106RenderRealismComparison PRIVATE
        "YOUKNOW106_DSP_SOURCE_SHA256=\"${YOUKNOW106_DSP_SOURCE_SHA256}\""
        "YOUKNOW106_COMPARISON_COMPILER_ID=\"${CMAKE_CXX_COMPILER_ID}\""
        "YOUKNOW106_COMPARISON_COMPILER_VERSION=\"${CMAKE_CXX_COMPILER_VERSION}\"")

    add_executable(YouKnow106AuditFactoryPresets Tools/AuditFactoryPresets.cpp)
    target_link_libraries(YouKnow106AuditFactoryPresets
        PRIVATE YouKnow106DSP Threads::Threads)
    target_compile_features(YouKnow106AuditFactoryPresets PRIVATE cxx_std_20)
    target_compile_definitions(YouKnow106AuditFactoryPresets PRIVATE
        "YOUKNOW106_FACTORY_AUDIT_DEFAULT_OUTPUT=\"${CMAKE_CURRENT_SOURCE_DIR}/Docs/audio/factory-presets\"")

    # Oversampling research has two deliberately separate builds. The normal
    # executable links the exact shipping DSP for uninstrumented CPU timing.
    # The work executable links only the engine/chorus translation units built
    # with semantic counters; the shipping library contains no counter branch.
    add_executable(YouKnow106OversamplingAudit
        Tools/AuditOversamplingDomains.cpp
        Tools/OversamplingAuditSupport.h)
    target_link_libraries(YouKnow106OversamplingAudit PRIVATE YouKnow106DSP)
    target_include_directories(YouKnow106OversamplingAudit
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnow106OversamplingAudit PRIVATE cxx_std_20)

    add_library(YouKnow106DSPWorkAudit STATIC
        Source/DSP/YouKnow106Engine.cpp
        Source/DSP/YouKnow106Chorus.cpp
        Source/DSP/YouKnow106Engine.h
        Source/DSP/YouKnow106Chorus.h
        Tools/OversamplingAuditSupport.h)
    target_include_directories(YouKnow106DSPWorkAudit
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnow106DSPWorkAudit PUBLIC cxx_std_20)
    target_compile_definitions(YouKnow106DSPWorkAudit
        PRIVATE YOUKNOW106_WORK_AUDIT=1)
    set_target_properties(YouKnow106DSPWorkAudit PROPERTIES
        POSITION_INDEPENDENT_CODE ON)

    add_executable(YouKnow106OversamplingWorkAudit
        Tools/AuditOversamplingDomains.cpp
        Tools/OversamplingAuditSupport.h)
    target_link_libraries(YouKnow106OversamplingWorkAudit
        PRIVATE YouKnow106DSPWorkAudit)
    target_include_directories(YouKnow106OversamplingWorkAudit
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnow106OversamplingWorkAudit PRIVATE cxx_std_20)
    target_compile_definitions(YouKnow106OversamplingWorkAudit
        PRIVATE YOUKNOW106_WORK_AUDIT=1)

    # Common-host numerical-quality audits are deliberately separate from the
    # work counters above.  Each executable links the untouched shipping DSP
    # and defines only an executable-local friend probe.  Analytic DCO/scan,
    # RK4 VCF and closed-form component/transfer-pole/zero-order-hold BBD
    # references decide whether 1x/2x/4x domain renders are admissible at the
    # same 44.1/48 kHz output boundary.
    add_executable(YouKnow106DcoScanQualityAudit
        Tools/AuditDcoScanQuality.cpp
        Tools/OversamplingQualitySupport.h)
    target_link_libraries(YouKnow106DcoScanQualityAudit PRIVATE YouKnow106DSP)
    target_include_directories(YouKnow106DcoScanQualityAudit
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnow106DcoScanQualityAudit PRIVATE cxx_std_20)

    add_executable(YouKnow106VcfBbdQualityAudit
        Tools/AuditVcfBbdQuality.cpp
        Tools/OversamplingQualitySupport.h)
    target_link_libraries(YouKnow106VcfBbdQualityAudit PRIVATE YouKnow106DSP)
    target_include_directories(YouKnow106VcfBbdQualityAudit
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnow106VcfBbdQualityAudit PRIVATE cxx_std_20)

    # The static, single-BBD-line oracle above deliberately uses a low-drive
    # regime that effectively linearises the fitted transfer, and omits the
    # LFO, second line, stereo mix and seeded line noise.  This companion drives
    # the public Chorus::process path through the actual production rate selector
    # and judges those combined mechanisms against an executable-local
    # continuous/event reference.
    add_executable(YouKnow106BbdDynamicQualityAudit
        Tools/AuditBbdDynamicQuality.cpp
        Tools/OversamplingQualitySupport.h)
    target_link_libraries(YouKnow106BbdDynamicQualityAudit
        PRIVATE YouKnow106DSP)
    target_include_directories(YouKnow106BbdDynamicQualityAudit
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnow106BbdDynamicQualityAudit PRIVATE cxx_std_20)

    # The static common-host audit above deliberately holds VCF controls and
    # Unit Character nominal.  This companion exercises the physical
    # time-varying equations: the normalized 23-write converter schedule,
    # continuous hold trajectories and all six voice-card profiles are judged
    # against an executable-local double RK64/RK128 oracle.
    add_executable(YouKnow106VcfDynamicQualityAudit
        Tools/AuditVcfDynamicQuality.cpp)
    target_link_libraries(YouKnow106VcfDynamicQualityAudit
        PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106VcfDynamicQualityAudit PRIVATE cxx_std_20)

    # Independent continuous-time qualification for the non-filter passive
    # converter holds.  The executable-local friend probe drives the actual
    # process scheduler; separately written long-double one/two-pole oracles
    # decide the Voice VCA, common VCA, SUB and PWM states.
    add_executable(YouKnow106PassiveHoldTimingAudit
        Tools/AuditPassiveHoldTiming.cpp)
    target_link_libraries(YouKnow106PassiveHoldTimingAudit
        PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106PassiveHoldTimingAudit PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(YouKnow106RenderDemos PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106RenderSota PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106RenderFidelityFixes PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106RenderRealismFixes PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106RenderRealismComparison
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106AuditFactoryPresets PRIVATE
            /W4 /permissive-)
        target_compile_options(YouKnow106OversamplingAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106DSPWorkAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106OversamplingWorkAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106DcoScanQualityAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106VcfBbdQualityAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106BbdDynamicQualityAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106VcfDynamicQualityAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnow106PassiveHoldTimingAudit
            PRIVATE /W4 /permissive-)
    else()
        target_compile_options(YouKnow106RenderDemos PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106RenderSota PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106RenderFidelityFixes PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106RenderRealismFixes PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106RenderRealismComparison PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106AuditFactoryPresets PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106OversamplingAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106DSPWorkAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106OversamplingWorkAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106DcoScanQualityAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106VcfBbdQualityAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106BbdDynamicQualityAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106VcfDynamicQualityAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnow106PassiveHoldTimingAudit PRIVATE
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

    add_executable(YouKnow106VcfIntegratorTests
        Tests/YouKnow106VcfIntegratorTests.cpp)
    target_link_libraries(YouKnow106VcfIntegratorTests PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106VcfIntegratorTests PRIVATE cxx_std_20)

    add_executable(YouKnow106SysExTests Tests/YouKnow106SysExTests.cpp)
    target_link_libraries(YouKnow106SysExTests PRIVATE YouKnow106DSP)
    target_compile_features(YouKnow106SysExTests PRIVATE cxx_std_20)

    foreach(target YouKnow106EngineTests YouKnow106CircuitTests
                   YouKnow106VcfIntegratorTests YouKnow106SysExTests)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()

    add_test(NAME YouKnow106.Engine COMMAND YouKnow106EngineTests)
    add_test(NAME YouKnow106.Circuit COMMAND YouKnow106CircuitTests)
    add_test(NAME YouKnow106.VcfIntegrator COMMAND YouKnow106VcfIntegratorTests)
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

        # A same-length WAV is not sufficient evidence for a build-to-build
        # claim. The strict BBD comparison must bind its baseline audio hash,
        # protocol and pre-change DSP fingerprint to the archived manifest.
        add_test(NAME YouKnow106.RealismComparisonContract
                 COMMAND YouKnow106RenderRealismComparison
                         --self-test-baseline-contract)
        set_tests_properties(YouKnow106.RealismComparisonContract
                             PROPERTIES TIMEOUT 30)

        # Wall-clock/CPU measurements are informational and intentionally not
        # gated. CI checks deterministic work algebra and raw-float parity
        # between the shipping and counter-instrumented builds instead.
        add_test(NAME YouKnow106.OversamplingAuditContract
                 COMMAND ${CMAKE_COMMAND}
                         "-DNORMAL_AUDIT=$<TARGET_FILE:YouKnow106OversamplingAudit>"
                         "-DWORK_AUDIT=$<TARGET_FILE:YouKnow106OversamplingWorkAudit>"
                         -P "${CMAKE_CURRENT_SOURCE_DIR}/Tools/CheckOversamplingAudit.cmake")
        set_tests_properties(YouKnow106.OversamplingAuditContract
                             PROPERTIES TIMEOUT 300)

        add_test(NAME YouKnow106.DcoScanQualityContract
                 COMMAND YouKnow106DcoScanQualityAudit --self-test)
        add_test(NAME YouKnow106.VcfBbdQualityContract
                 COMMAND YouKnow106VcfBbdQualityAudit --self-test)
        add_test(NAME YouKnow106.BbdDynamicQualityContract
                 COMMAND YouKnow106BbdDynamicQualityAudit --self-test)
        add_test(NAME YouKnow106.VcfDynamicQualityContract
                 COMMAND YouKnow106VcfDynamicQualityAudit --self-test)
        add_test(NAME YouKnow106.PassiveHoldTimingContract
                 COMMAND YouKnow106PassiveHoldTimingAudit --self-test)
        set_tests_properties(YouKnow106.DcoScanQualityContract
                             YouKnow106.VcfBbdQualityContract
                             YouKnow106.VcfDynamicQualityContract
                             YouKnow106.PassiveHoldTimingContract
                             PROPERTIES TIMEOUT 300)
        set_tests_properties(YouKnow106.BbdDynamicQualityContract
                             PROPERTIES TIMEOUT 300)
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
        # The engine bound was 300 s. The warm-up fixture added in the
        # 2026-08-07 pass advances up to 900 s of internal samples for four
        # configurations, which takes the suite to about 297 s run alone and
        # over the bound when it shares a machine with another CTest job —
        # no runner-speed margin at all. Raised to 900 s so the guard catches
        # a hang rather than a slow runner.
        set(YOUKNOW106_ENGINE_TEST_TIMEOUT 900)
        set(YOUKNOW106_CIRCUIT_TEST_TIMEOUT 90)
    endif()
    set_tests_properties(YouKnow106.Engine PROPERTIES
        TIMEOUT ${YOUKNOW106_ENGINE_TEST_TIMEOUT})
    set_tests_properties(YouKnow106.Circuit PROPERTIES
        TIMEOUT ${YOUKNOW106_CIRCUIT_TEST_TIMEOUT})
    set_tests_properties(YouKnow106.VcfIntegrator PROPERTIES TIMEOUT 60)

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
