cmake_minimum_required(VERSION 3.22)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

option(YOUKNOW_BUILD_UNIVERSAL "Build arm64 and x86_64 slices on macOS" ON)

# These must be established before project() so CMake's compiler checks use the
# same deployment target and architectures as the final plug-in.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()

    if(YOUKNOW_BUILD_UNIVERSAL AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS architectures")
    endif()
endif()

# C is required in addition to C++: JUCE's macOS plug-in wrappers (AU, Standalone)
# compile C/Objective-C sources. Restricting this to CXX breaks non-Xcode
# generators (Ninja/Makefiles) with a missing CMAKE_C_COMPILE_OBJECT error.
project(YouKnow VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The DSP is split across static-library translation units, so the plug-in's
# own LTO flags cannot optimise through that archive unless the library was
# compiled for IPO too.  Release IPO keeps the equations unchanged while
# allowing the linker to optimise the complete audio path.  Respect an
# explicit caller choice, and fall back cleanly on toolchains without IPO.
if(NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION
   AND NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT YOUKNOW_IPO_SUPPORTED
                        OUTPUT YOUKNOW_IPO_ERROR
                        LANGUAGES C CXX)
    if(YOUKNOW_IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    else()
        message(STATUS "Release IPO unavailable: ${YOUKNOW_IPO_ERROR}")
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(YOUKNOW_BUILD_PLUGIN "Build the JUCE plug-in and standalone app" ON)
set(YOUKNOW_JUCE_PATH "" CACHE PATH
    "Optional path to a local JUCE 8.0.14 checkout (avoids downloading JUCE)")

include(CTest)

set(YOUKNOW_DSP_SOURCES
    Source/DSP/YouKnowEngine.cpp
    Source/DSP/YouKnowChorus.cpp
    Source/DSP/YouKnowPanel.cpp
    Source/DSP/YouKnowPresets.cpp
    Source/DSP/YouKnowSysEx.cpp)
set(YOUKNOW_DSP_HEADERS
    Source/DSP/YouKnowEngine.h
    Source/DSP/YouKnowChorus.h
    Source/DSP/YouKnowPanel.h)

add_library(YouKnowDSP STATIC
    ${YOUKNOW_DSP_SOURCES}
    ${YOUKNOW_DSP_HEADERS})

target_include_directories(YouKnowDSP
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_features(YouKnowDSP PUBLIC cxx_std_20)
set_target_properties(YouKnowDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(YouKnowDSP PRIVATE /W4 /permissive-)
else()
    target_compile_options(YouKnowDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(YOUKNOW_BUILD_PLUGIN)
    if(YOUKNOW_JUCE_PATH)
        if(NOT EXISTS "${YOUKNOW_JUCE_PATH}/CMakeLists.txt")
            message(FATAL_ERROR
                "YOUKNOW_JUCE_PATH does not contain a JUCE CMakeLists.txt: ${YOUKNOW_JUCE_PATH}")
        endif()
        add_subdirectory("${YOUKNOW_JUCE_PATH}"
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
        set(YOUKNOW_PLUGIN_FORMATS VST3 AU Standalone)
    else()
        set(YOUKNOW_PLUGIN_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(YouKnow
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "Protocodus"
        COMPANY_WEBSITE "https://protocodus.cz"
        COMPANY_EMAIL "protocodus@proton.me"
        COMPANY_COPYRIGHT "Copyright (c) 2026 Protocodus"
        BUNDLE_ID "cz.protocodus.youknow"
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/youknow-icon.png"
        PLUGIN_MANUFACTURER_CODE Ykno
        PLUGIN_CODE Yk06
        PRODUCT_NAME "YouKnow"
        DESCRIPTION "YouKnow circuit-modelled six-voice DCO polysynth"
        FORMATS ${YOUKNOW_PLUGIN_FORMATS}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        # Patch export is an explicit .syx file operation in v1. The synth does
        # not expose a dormant MIDI-output bus to hosts.
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_SANDBOX_SAFE TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    # JUCE starts every wrapper from the shared BUNDLE_ID above. Give each
    # distinct macOS bundle its own identifier while keeping the shared base ID
    # in JucePlugin_CFBundleIdentifier, so AU/VST3 plug-in identity is unchanged.
    if(APPLE)
        function(youknow_set_wrapper_bundle_identifier target identifier)
            get_target_property(info_plist "${target}" MACOSX_BUNDLE_INFO_PLIST)
            if(NOT info_plist OR info_plist MATCHES "-NOTFOUND$")
                message(FATAL_ERROR "No generated Info.plist for ${target}")
            endif()

            execute_process(
                COMMAND /usr/libexec/PlistBuddy -c
                    "Set :CFBundleIdentifier ${identifier}" "${info_plist}"
                COMMAND_ERROR_IS_FATAL ANY)
            set_target_properties("${target}" PROPERTIES
                XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${identifier}")
        endfunction()

        youknow_set_wrapper_bundle_identifier(YouKnow_Standalone
            cz.protocodus.youknow)
        youknow_set_wrapper_bundle_identifier(YouKnow_AU
            cz.protocodus.youknow.au)
        youknow_set_wrapper_bundle_identifier(YouKnow_VST3
            cz.protocodus.youknow.vst3)
    endif()

    juce_generate_juce_header(YouKnow)

    juce_add_binary_data(YouKnowAssets
        SOURCES Assets/used-charcoal-plastic.png)

    target_sources(YouKnow PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

    target_link_libraries(YouKnow
        PRIVATE
            YouKnowDSP
            YouKnowAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    target_compile_definitions(YouKnow
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# The committed demonstration WAVs under Docs/audio are rendered by this tool
# from the same JUCE-free engine the plug-in uses, so they cannot drift away
# from what YouKnow actually produces. It is built by default, including on
# Linux CI, so it cannot rot unnoticed.
option(YOUKNOW_BUILD_TOOLS "Build the JUCE-free audio render/audit tools" ON)

if(YOUKNOW_BUILD_TOOLS)
    find_package(Threads REQUIRED)

    add_executable(YouKnowRenderDemos Tools/RenderDemos.cpp)
    target_link_libraries(YouKnowRenderDemos PRIVATE YouKnowDSP)
    target_compile_features(YouKnowRenderDemos PRIVATE cxx_std_20)

    add_executable(YouKnowRenderSota Tools/RenderSotaComparisons.cpp)
    target_link_libraries(YouKnowRenderSota PRIVATE YouKnowDSP)
    target_compile_features(YouKnowRenderSota PRIVATE cxx_std_20)

    add_executable(YouKnowRenderTanhAuditions
        Tools/RenderTanhAuditions.cpp
        Tools/RealismComparisonSupport.h)
    target_link_libraries(YouKnowRenderTanhAuditions PRIVATE YouKnowDSP)
    target_compile_features(YouKnowRenderTanhAuditions PRIVATE cxx_std_20)

    add_executable(YouKnowRenderFidelityFixes Tools/RenderFidelityFixes.cpp)
    target_link_libraries(YouKnowRenderFidelityFixes PRIVATE YouKnowDSP)
    target_compile_features(YouKnowRenderFidelityFixes PRIVATE cxx_std_20)

    add_executable(YouKnowRenderRealismFixes Tools/RenderRealismFixes.cpp)
    target_link_libraries(YouKnowRenderRealismFixes PRIVATE YouKnowDSP)
    target_compile_features(YouKnowRenderRealismFixes PRIVATE cxx_std_20)

    # Focused build-to-build comparisons keep their raw float32 baseline and
    # identify the exact DSP source set used to compile each side. Reconfigure
    # when any fingerprinted source changes so a normal `cmake --build` cannot
    # leave stale source metadata in an after manifest.
    set(YOUKNOW_DSP_FINGERPRINT_INPUT "")
    set(YOUKNOW_DSP_FINGERPRINT_FILES
        ${YOUKNOW_DSP_SOURCES}
        ${YOUKNOW_DSP_HEADERS})
    foreach(source IN LISTS YOUKNOW_DSP_FINGERPRINT_FILES)
        file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${source}" source_hash)
        string(APPEND YOUKNOW_DSP_FINGERPRINT_INPUT
            "${source}:${source_hash}\n")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    endforeach()
    string(SHA256 YOUKNOW_DSP_SOURCE_SHA256
        "${YOUKNOW_DSP_FINGERPRINT_INPUT}")

    add_executable(YouKnowRenderRealismComparison
        Tools/RenderRealismComparison.cpp
        Tools/RealismComparisonSupport.h)
    target_link_libraries(YouKnowRenderRealismComparison
        PRIVATE YouKnowDSP)
    target_compile_features(YouKnowRenderRealismComparison PRIVATE cxx_std_20)
    target_compile_definitions(YouKnowRenderRealismComparison PRIVATE
        "YOUKNOW_DSP_SOURCE_SHA256=\"${YOUKNOW_DSP_SOURCE_SHA256}\""
        "YOUKNOW_COMPARISON_COMPILER_ID=\"${CMAKE_CXX_COMPILER_ID}\""
        "YOUKNOW_COMPARISON_COMPILER_VERSION=\"${CMAKE_CXX_COMPILER_VERSION}\"")

    add_executable(YouKnowRenderPresetScore Tools/RenderPresetScore.cpp)
    target_link_libraries(YouKnowRenderPresetScore
        PRIVATE YouKnowDSP Threads::Threads)
    target_compile_features(YouKnowRenderPresetScore PRIVATE cxx_std_20)

    add_executable(YouKnowMakeCalibrationTakes Tools/MakeCalibrationTakes.cpp)
    target_link_libraries(YouKnowMakeCalibrationTakes
        PRIVATE YouKnowDSP Threads::Threads)
    target_compile_features(YouKnowMakeCalibrationTakes PRIVATE cxx_std_20)

    add_executable(YouKnowRenderCalibrationEvents Tools/RenderCalibrationEvents.cpp)
    target_link_libraries(YouKnowRenderCalibrationEvents PRIVATE YouKnowDSP)
    target_compile_features(YouKnowRenderCalibrationEvents PRIVATE cxx_std_20)

    add_executable(YouKnowAuditFactoryPresets Tools/AuditFactoryPresets.cpp)
    target_link_libraries(YouKnowAuditFactoryPresets
        PRIVATE YouKnowDSP Threads::Threads)
    target_compile_features(YouKnowAuditFactoryPresets PRIVATE cxx_std_20)
    target_compile_definitions(YouKnowAuditFactoryPresets PRIVATE
        "YOUKNOW_FACTORY_AUDIT_DEFAULT_OUTPUT=\"${CMAKE_CURRENT_SOURCE_DIR}/Docs/audio/factory-presets\"")

    # Oversampling research has two deliberately separate builds. The normal
    # executable links the exact shipping DSP for uninstrumented CPU timing.
    # The work executable links only the engine/chorus translation units built
    # with semantic counters; the shipping library contains no counter branch.
    add_executable(YouKnowOversamplingAudit
        Tools/AuditOversamplingDomains.cpp
        Tools/OversamplingAuditSupport.h)
    target_link_libraries(YouKnowOversamplingAudit PRIVATE YouKnowDSP)
    target_include_directories(YouKnowOversamplingAudit
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnowOversamplingAudit PRIVATE cxx_std_20)

    add_library(YouKnowDSPWorkAudit STATIC
        Source/DSP/YouKnowEngine.cpp
        Source/DSP/YouKnowChorus.cpp
        Source/DSP/YouKnowEngine.h
        Source/DSP/YouKnowChorus.h
        Tools/OversamplingAuditSupport.h)
    target_include_directories(YouKnowDSPWorkAudit
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnowDSPWorkAudit PUBLIC cxx_std_20)
    target_compile_definitions(YouKnowDSPWorkAudit
        PRIVATE YOUKNOW_WORK_AUDIT=1)
    set_target_properties(YouKnowDSPWorkAudit PROPERTIES
        POSITION_INDEPENDENT_CODE ON)

    add_executable(YouKnowOversamplingWorkAudit
        Tools/AuditOversamplingDomains.cpp
        Tools/OversamplingAuditSupport.h)
    target_link_libraries(YouKnowOversamplingWorkAudit
        PRIVATE YouKnowDSPWorkAudit)
    target_include_directories(YouKnowOversamplingWorkAudit
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnowOversamplingWorkAudit PRIVATE cxx_std_20)
    target_compile_definitions(YouKnowOversamplingWorkAudit
        PRIVATE YOUKNOW_WORK_AUDIT=1)

    # Common-host numerical-quality audits are deliberately separate from the
    # work counters above.  Each executable links the untouched shipping DSP
    # and defines only an executable-local friend probe.  Analytic DCO/scan,
    # RK4 VCF and closed-form component/transfer-pole/zero-order-hold BBD
    # references decide whether 1x/2x/4x domain renders are admissible at the
    # same 44.1/48 kHz output boundary.
    add_executable(YouKnowDcoScanQualityAudit
        Tools/AuditDcoScanQuality.cpp
        Tools/OversamplingQualitySupport.h)
    target_link_libraries(YouKnowDcoScanQualityAudit PRIVATE YouKnowDSP)
    target_include_directories(YouKnowDcoScanQualityAudit
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnowDcoScanQualityAudit PRIVATE cxx_std_20)

    add_executable(YouKnowVcfBbdQualityAudit
        Tools/AuditVcfBbdQuality.cpp
        Tools/OversamplingQualitySupport.h)
    target_link_libraries(YouKnowVcfBbdQualityAudit PRIVATE YouKnowDSP)
    target_include_directories(YouKnowVcfBbdQualityAudit
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnowVcfBbdQualityAudit PRIVATE cxx_std_20)

    # The static, single-BBD-line oracle above deliberately uses a low-drive
    # regime that effectively linearises the fitted transfer, and omits the
    # LFO, second line, stereo mix and seeded line noise.  This companion drives
    # the public Chorus::process path through the actual production rate selector
    # and judges those combined mechanisms against an executable-local
    # continuous/event reference.
    add_executable(YouKnowBbdDynamicQualityAudit
        Tools/AuditBbdDynamicQuality.cpp
        Tools/OversamplingQualitySupport.h)
    target_link_libraries(YouKnowBbdDynamicQualityAudit
        PRIVATE YouKnowDSP)
    target_include_directories(YouKnowBbdDynamicQualityAudit
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tools")
    target_compile_features(YouKnowBbdDynamicQualityAudit PRIVATE cxx_std_20)

    # The static common-host audit above deliberately holds VCF controls and
    # Unit Character nominal.  This companion exercises the physical
    # time-varying equations: the normalized 23-write converter schedule,
    # continuous hold trajectories and all six voice-card profiles are judged
    # against an executable-local double RK64/RK128 oracle.
    add_executable(YouKnowVcfDynamicQualityAudit
        Tools/AuditVcfDynamicQuality.cpp)
    target_link_libraries(YouKnowVcfDynamicQualityAudit
        PRIVATE YouKnowDSP)
    target_compile_features(YouKnowVcfDynamicQualityAudit PRIVATE cxx_std_20)

    # Independent continuous-time qualification for the non-filter passive
    # converter holds.  The executable-local friend probe drives the actual
    # process scheduler; separately written long-double one/two-pole oracles
    # decide the Voice VCA, common VCA, SUB and PWM states.
    add_executable(YouKnowPassiveHoldTimingAudit
        Tools/AuditPassiveHoldTiming.cpp)
    target_link_libraries(YouKnowPassiveHoldTimingAudit
        PRIVATE YouKnowDSP)
    target_compile_features(YouKnowPassiveHoldTimingAudit PRIVATE cxx_std_20)

    # Independent nominal-component MNA qualification for C14 and the shared
    # four-position HPF.  The audit covers fixed-mode transfer and the existing
    # C14 state continuity, but intentionally makes no CMOS switch-click claim.
    add_executable(YouKnowHighPassNetworkAudit
        Tools/AuditHighPassNetwork.cpp)
    target_link_libraries(YouKnowHighPassNetworkAudit
        PRIVATE YouKnowDSP)
    target_compile_features(YouKnowHighPassNetworkAudit PRIVATE cxx_std_20)

    # Independent numerical qualification for the shared main-noise support
    # circuit. The physical C41/R79 corner is re-derived locally, while the
    # actual production updater and hidden filter states are probed across the
    # supported low-rate endpoint and ordinary selector grids.
    add_executable(YouKnowNoiseSourceQualityAudit
        Tools/AuditNoiseSourceQuality.cpp)
    target_link_libraries(YouKnowNoiseSourceQualityAudit
        PRIVATE YouKnowDSP)
    target_compile_features(YouKnowNoiseSourceQualityAudit PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(YouKnowRenderDemos PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowRenderSota PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowRenderTanhAuditions
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowRenderFidelityFixes PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowRenderRealismFixes PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowRenderRealismComparison
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowAuditFactoryPresets PRIVATE
            /W4 /permissive-)
        target_compile_options(YouKnowOversamplingAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowDSPWorkAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowOversamplingWorkAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowDcoScanQualityAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowVcfBbdQualityAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowBbdDynamicQualityAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowVcfDynamicQualityAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowPassiveHoldTimingAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowHighPassNetworkAudit
            PRIVATE /W4 /permissive-)
        target_compile_options(YouKnowNoiseSourceQualityAudit
            PRIVATE /W4 /permissive-)
    else()
        target_compile_options(YouKnowRenderDemos PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowRenderSota PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowRenderTanhAuditions PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowRenderFidelityFixes PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowRenderRealismFixes PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowRenderRealismComparison PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowAuditFactoryPresets PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowOversamplingAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowDSPWorkAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowOversamplingWorkAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowDcoScanQualityAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowVcfBbdQualityAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowBbdDynamicQualityAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowVcfDynamicQualityAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowPassiveHoldTimingAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowHighPassNetworkAudit PRIVATE
            -Wall -Wextra -Wpedantic)
        target_compile_options(YouKnowNoiseSourceQualityAudit PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(YouKnowEngineTests Tests/YouKnowEngineTests.cpp)
    target_link_libraries(YouKnowEngineTests PRIVATE YouKnowDSP)
    target_compile_features(YouKnowEngineTests PRIVATE cxx_std_20)

    add_executable(YouKnowCircuitTests Tests/YouKnowCircuitTests.cpp)
    target_link_libraries(YouKnowCircuitTests PRIVATE YouKnowDSP)
    target_compile_features(YouKnowCircuitTests PRIVATE cxx_std_20)

    add_executable(YouKnowVcfIntegratorTests
        Tests/YouKnowVcfIntegratorTests.cpp)
    target_link_libraries(YouKnowVcfIntegratorTests PRIVATE YouKnowDSP)
    target_compile_features(YouKnowVcfIntegratorTests PRIVATE cxx_std_20)

    add_executable(YouKnowSysExTests Tests/YouKnowSysExTests.cpp)
    target_link_libraries(YouKnowSysExTests PRIVATE YouKnowDSP)
    target_compile_features(YouKnowSysExTests PRIVATE cxx_std_20)

    foreach(target YouKnowEngineTests YouKnowCircuitTests
                   YouKnowVcfIntegratorTests YouKnowSysExTests)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()

    add_test(NAME YouKnow.Engine COMMAND YouKnowEngineTests)
    add_test(NAME YouKnow.Circuit COMMAND YouKnowCircuitTests)
    add_test(NAME YouKnow.VcfIntegrator COMMAND YouKnowVcfIntegratorTests)
    add_test(NAME YouKnow.SysEx COMMAND YouKnowSysExTests)

    if(YOUKNOW_BUILD_TOOLS)
        add_test(NAME YouKnow.CalibrationEvents
                 COMMAND YouKnowRenderCalibrationEvents --self-test)
        # Guards the renderer that produces the committed demonstration WAVs:
        # it renders a short take to a temporary directory and checks that the
        # audio is finite, audible and written as a readable WAV.
        add_test(NAME YouKnow.RenderDemos
                 COMMAND YouKnowRenderDemos --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/demo-smoke")
        set_tests_properties(YouKnow.RenderDemos PROPERTIES TIMEOUT 60)

        # Fast integration coverage for the long-form factory gain auditor. It
        # uses the same mappings, meters, ownership guard and fixed preview
        # path as a full run, with a two-preset shortened score.
        add_test(NAME YouKnow.AuditFactoryPresets
                 COMMAND YouKnowAuditFactoryPresets --smoke
                         "${CMAKE_CURRENT_BINARY_DIR}/factory-audit-smoke")
        set_tests_properties(YouKnow.AuditFactoryPresets PROPERTIES TIMEOUT 60)

        # A same-length WAV is not sufficient evidence for a build-to-build
        # claim. The strict BBD comparison must bind its baseline audio hash,
        # protocol and pre-change DSP fingerprint to the archived manifest.
        add_test(NAME YouKnow.RealismComparisonContract
                 COMMAND YouKnowRenderRealismComparison
                         --self-test-baseline-contract)
        set_tests_properties(YouKnow.RealismComparisonContract
                             PROPERTIES TIMEOUT 30)

        # Wall-clock/CPU measurements are informational and intentionally not
        # gated. CI checks deterministic work algebra and raw-float parity
        # between the shipping and counter-instrumented builds instead.
        add_test(NAME YouKnow.OversamplingAuditContract
                 COMMAND ${CMAKE_COMMAND}
                         "-DNORMAL_AUDIT=$<TARGET_FILE:YouKnowOversamplingAudit>"
                         "-DWORK_AUDIT=$<TARGET_FILE:YouKnowOversamplingWorkAudit>"
                         -P "${CMAKE_CURRENT_SOURCE_DIR}/Tools/CheckOversamplingAudit.cmake")
        set_tests_properties(YouKnow.OversamplingAuditContract
                             PROPERTIES TIMEOUT 300)

        add_test(NAME YouKnow.DcoScanQualityContract
                 COMMAND YouKnowDcoScanQualityAudit --self-test)
        add_test(NAME YouKnow.VcfBbdQualityContract
                 COMMAND YouKnowVcfBbdQualityAudit --self-test)
        # The full continuous-time oracle relies on target-native long-double
        # arithmetic. On x64 the bounded shipping test is the portability gate:
        # it keeps every production boundary and selector ledger, without the
        # architecture-sensitive reference solve that is not used for x64
        # admission.
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x64)$"
           OR CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
            set(YOUKNOW_BBD_DYNAMIC_TEST_ARGUMENT --shipping-self-test)
            set(YOUKNOW_BBD_DYNAMIC_TEST_TIMEOUT 60)
        else()
            set(YOUKNOW_BBD_DYNAMIC_TEST_ARGUMENT --self-test)
            set(YOUKNOW_BBD_DYNAMIC_TEST_TIMEOUT 900)
        endif()
        add_test(NAME YouKnow.BbdDynamicQualityContract
                 COMMAND YouKnowBbdDynamicQualityAudit
                         ${YOUKNOW_BBD_DYNAMIC_TEST_ARGUMENT})
        add_test(NAME YouKnow.VcfDynamicQualityContract
                 COMMAND YouKnowVcfDynamicQualityAudit --self-test)
        add_test(NAME YouKnow.PassiveHoldTimingContract
                 COMMAND YouKnowPassiveHoldTimingAudit --self-test)
        add_test(NAME YouKnow.HighPassNetworkContract
                 COMMAND YouKnowHighPassNetworkAudit --self-test)
        add_test(NAME YouKnow.NoiseSourceQualityContract
                 COMMAND YouKnowNoiseSourceQualityAudit --self-test)
        set_tests_properties(YouKnow.DcoScanQualityContract
                             YouKnow.VcfBbdQualityContract
                             YouKnow.VcfDynamicQualityContract
                             YouKnow.PassiveHoldTimingContract
                             YouKnow.HighPassNetworkContract
                             YouKnow.NoiseSourceQualityContract
                             PROPERTIES TIMEOUT 300)
        set_tests_properties(YouKnow.BbdDynamicQualityContract
                             PROPERTIES TIMEOUT
                                        ${YOUKNOW_BBD_DYNAMIC_TEST_TIMEOUT})
    endif()
    # The engine suite renders the complete factory-balance corpus, while the
    # circuit suite re-solves reference ODE models at high internal rates.
    # Address/undefined sanitizers make both substantially slower; retain
    # runaway guards with enough margin for universal macOS builds and loaded
    # CI workers instead of intermittently timing out valid coverage.
    if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
        set(YOUKNOW_ENGINE_TEST_TIMEOUT 600)
        set(YOUKNOW_CIRCUIT_TEST_TIMEOUT 900)
        # The suites also have to know, because one of them asserts the engine
        # renders faster than realtime and instrumentation makes that false.
        # Compiler macros do not cover the cases this branch does -- GCC with
        # only -fsanitize=undefined defines nothing a test can see -- so the
        # build system tells them directly and the two decisions cannot drift
        # apart.
        foreach(target YouKnowEngineTests YouKnowCircuitTests YouKnowSysExTests)
            target_compile_definitions(${target} PRIVATE YOUKNOW_SANITIZER_BUILD=1)
        endforeach()
    else()
        # The engine bound was 300 s. The warm-up fixture added in the
        # 2026-08-07 pass advances up to 900 s of internal samples for four
        # configurations, which takes the suite to about 297 s run alone and
        # over the bound when it shares a machine with another CTest job —
        # no runner-speed margin at all. Raised to 900 s so the guard catches
        # a hang rather than a slow runner.
        set(YOUKNOW_ENGINE_TEST_TIMEOUT 900)
        set(YOUKNOW_CIRCUIT_TEST_TIMEOUT 90)
    endif()
    set_tests_properties(YouKnow.Engine PROPERTIES
        TIMEOUT ${YOUKNOW_ENGINE_TEST_TIMEOUT})
    set_tests_properties(YouKnow.Circuit PROPERTIES
        TIMEOUT ${YOUKNOW_CIRCUIT_TEST_TIMEOUT})
    # The integrator bound was 60 s. The solver-ladder fixtures added in the
    # 2026-08-23 pass run the independent 96-substep reference solve at two
    # internal grids for three rungs, settle three self-oscillating limit
    # cycles, and repeat the zero-input cap sweep for four more kernel
    # combinations, which takes the suite to about 11 s run alone on the
    # reference x86-64 part. Raised so the guard catches a hang rather than a
    # slow or shared runner.
    set_tests_properties(YouKnow.VcfIntegrator PROPERTIES TIMEOUT 180)

    if(YOUKNOW_BUILD_PLUGIN)
        add_executable(YouKnowPluginProcessorTests Tests/PluginProcessorTests.cpp)
        target_link_libraries(YouKnowPluginProcessorTests PRIVATE YouKnow)
        target_include_directories(YouKnowPluginProcessorTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/Source"
            "$<TARGET_PROPERTY:YouKnow,JUCE_GENERATED_SOURCES_DIRECTORY>"
            "$<TARGET_PROPERTY:YouKnow,INCLUDE_DIRECTORIES>")
        target_compile_features(YouKnowPluginProcessorTests PRIVATE cxx_std_20)

        if(MSVC)
            target_compile_options(YouKnowPluginProcessorTests PRIVATE /W4 /permissive-)
        else()
            target_compile_options(YouKnowPluginProcessorTests PRIVATE
                -Wall -Wextra -Wpedantic)
        endif()

        add_test(NAME YouKnow.PluginProcessor COMMAND YouKnowPluginProcessorTests)
        set_tests_properties(YouKnow.PluginProcessor PROPERTIES TIMEOUT 60)

        if(APPLE)
            # Exercise the product through the same dynamically loaded VST3
            # boundary a DAW uses. This complements the processor tests; it is
            # deliberately small and does not pretend to replace host/vendor
            # validation of the final signed package.
            add_executable(YouKnowVST3BundleSmokeTests
                Tests/VST3BundleSmokeTests.cpp)
            target_link_libraries(YouKnowVST3BundleSmokeTests PRIVATE
                juce::juce_audio_processors_headless
                juce::juce_recommended_config_flags)
            target_compile_definitions(YouKnowVST3BundleSmokeTests PRIVATE
                JUCE_PLUGINHOST_VST3=1
                JUCE_USE_CURL=0
                JUCE_WEB_BROWSER=0)
            target_compile_features(YouKnowVST3BundleSmokeTests PRIVATE
                cxx_std_20)
            target_compile_options(YouKnowVST3BundleSmokeTests PRIVATE
                -Wall -Wextra -Wpedantic)
            add_dependencies(YouKnowVST3BundleSmokeTests YouKnow_VST3)

            add_test(NAME YouKnow.VST3Bundle
                COMMAND YouKnowVST3BundleSmokeTests
                        "$<TARGET_BUNDLE_DIR:YouKnow_VST3>")
            set_tests_properties(YouKnow.VST3Bundle PROPERTIES TIMEOUT 60)
        endif()
    endif()
endif()
