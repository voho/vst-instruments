// A strict, build-to-build realism comparison renderer.
//
// Usage:
//   YouKnow106RenderRealismComparison retrigger-release-tail before <output-dir>
//   YouKnow106RenderRealismComparison common-vca-level before <output-dir>
//   YouKnow106RenderRealismComparison bbd-transfer-clock-law before <output-dir>
//   YouKnow106RenderRealismComparison voice-vca-feedthrough before <output-dir>
//   YouKnow106RenderRealismComparison bbd-host-grid-alias before <output-dir>
//   ... make and rebuild one DSP change ...
//   YouKnow106RenderRealismComparison <scenario> after <output-dir>
//
// The `before` raw float32 file is an immutable baseline: rerunning `before`
// succeeds only if the new render is bit-identical. The `after` pass reads that
// archive, writes an unscaled signed difference, and applies one common
// listening gain to both sides and their difference.

#include "RealismComparisonSupport.h"

#include "DSP/YouKnow106Engine.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef YOUKNOW106_DSP_SOURCE_SHA256
#define YOUKNOW106_DSP_SOURCE_SHA256 "unavailable"
#endif

#ifndef YOUKNOW106_COMPARISON_COMPILER_ID
#define YOUKNOW106_COMPARISON_COMPILER_ID "unknown"
#endif

#ifndef YOUKNOW106_COMPARISON_COMPILER_VERSION
#define YOUKNOW106_COMPARISON_COMPILER_VERSION "unknown"
#endif

namespace youknow106
{
// Both DSP classes deliberately expose this one narrow friend name to
// deterministic regression fixtures.  The comparison renderer uses it only to
// isolate one complete BBD line and to apply the engine's shipping 4x-to-1x
// decimator.  No production API is widened for an offline diagnostic.
struct YouKnow106TestAccess
{
    template <typename Producer>
    static tools::realism::StereoBuffer captureAt48k(
        bool highQuality, std::uint64_t warmupFrames,
        std::uint64_t captureFrames, Producer&& producer)
    {
        const int factor = highQuality ? 4 : 1;
        const float internalRate = static_cast<float>(
            tools::realism::comparisonSampleRate * factor);
        YouKnow106Engine decimator;
        tools::realism::StereoBuffer output;
        output.left.reserve(static_cast<std::size_t>(captureFrames));
        output.right.reserve(static_cast<std::size_t>(captureFrames));

        for (std::uint64_t hostFrame = 0;
             hostFrame < warmupFrames + captureFrames; ++hostFrame)
        {
            std::array<float, 4> stageLeft {};
            std::array<float, 4> stageRight {};
            for (int step = 0; step < factor; ++step)
            {
                const auto sample = producer(internalRate);
                stageLeft[static_cast<std::size_t>(step)] = sample[0];
                stageRight[static_cast<std::size_t>(step)] = sample[1];
            }

            float left = stageLeft[0];
            float right = stageRight[0];
            if (factor == 4)
            {
                float firstLeft = 0.0f;
                float firstRight = 0.0f;
                float secondLeft = 0.0f;
                float secondRight = 0.0f;
                decimator.downsamplePair(
                    decimator.firstDecimator_, stageLeft[0], stageRight[0],
                    stageLeft[1], stageRight[1], firstLeft, firstRight);
                decimator.downsamplePair(
                    decimator.firstDecimator_, stageLeft[2], stageRight[2],
                    stageLeft[3], stageRight[3], secondLeft, secondRight);
                decimator.downsamplePair(
                    decimator.secondDecimator_, firstLeft, firstRight,
                    secondLeft, secondRight, left, right);
            }

            if (hostFrame >= warmupFrames)
            {
                output.left.push_back(left);
                output.right.push_back(right);
            }
        }
        return output;
    }

    static tools::realism::StereoBuffer renderFixedBbdProbe(
        bool highQuality, std::uint64_t warmupFrames,
        std::uint64_t captureFrames, float inputFrequencyHz,
        float inputAmplitude, float clockHz)
    {
        const float internalRate = static_cast<float>(
            tools::realism::comparisonSampleRate * (highQuality ? 4 : 1));
        auto support = Chorus::supportChainFor(internalRate);
        Chorus::Line lineA;
        Chorus::Line lineB;
        lineA.reset(0x9e3779b9u);
        lineB.reset(0x85ebca6bu);
        double phase = 0.0;
        return captureAt48k(
            highQuality, warmupFrames, captureFrames,
            [&](float rate) {
                const float input = inputAmplitude * static_cast<float>(
                    std::sin(2.0 * 3.14159265358979323846 * phase));
                phase += static_cast<double>(inputFrequencyHz)
                       / static_cast<double>(rate);
                phase -= std::floor(phase);
                return std::array<float, 2> {
                    lineA.process(input, clockHz, rate, support,
                                  support.wetOutputCouplingConnectedG, 0.0f),
                    lineB.process(input, clockHz, rate, support,
                                  support.wetOutputCouplingConnectedG, 0.0f)
                };
            });
    }

    static tools::realism::StereoBuffer renderSweptBbdProbe(
        bool highQuality, ChorusMode mode, std::uint64_t warmupFrames,
        std::uint64_t captureFrames, float inputFrequencyHz,
        float inputAmplitude)
    {
        const float internalRate = static_cast<float>(
            tools::realism::comparisonSampleRate * (highQuality ? 4 : 1));
        Chorus chorus;
        chorus.prepare(internalRate);
        double phase = 0.0;
        return captureAt48k(
            highQuality, warmupFrames, captureFrames,
            [&](float rate) {
                const float input = inputAmplitude * static_cast<float>(
                    std::sin(2.0 * 3.14159265358979323846 * phase));
                phase += static_cast<double>(inputFrequencyHz)
                       / static_cast<double>(rate);
                phase -= std::floor(phase);
                float left = 0.0f;
                float right = 0.0f;
                chorus.process(input, mode, 0.0f, left, right,
                               false, true, 1.0f);
                // Public Chorus output includes the dry IC6 leg.  Remove it
                // with the same float expression to leave only the modeled
                // BBD/support-chain contribution.
                const float dry = Chorus::dryMixGain * input;
                return std::array<float, 2> { left - dry, right - dry };
            });
    }
};
} // namespace youknow106

namespace
{
using youknow106::Chorus;
using youknow106::ChorusMode;
using youknow106::DcoRange;
using youknow106::EngineParameters;
using youknow106::HighPassMode;
using youknow106::KeyMode;
using youknow106::VcaMode;
using youknow106::YouKnow106Engine;
using youknow106::YouKnow106TestAccess;
using youknow106::tools::realism::Level;
using youknow106::tools::realism::StereoBuffer;
using youknow106::tools::realism::applyGain;
using youknow106::tools::realism::comparisonBlockSize;
using youknow106::tools::realism::comparisonSampleRate;
using youknow106::tools::realism::decibels;
using youknow106::tools::realism::difference;
using youknow106::tools::realism::jsonNumber;
using youknow106::tools::realism::listeningTargetPeak;
using youknow106::tools::realism::measure;
using youknow106::tools::realism::readFloatWav;
using youknow106::tools::realism::updateGeneratedSection;
using youknow106::tools::realism::validate;
using youknow106::tools::realism::writeFloatWav;
using youknow106::tools::realism::writeText;

constexpr std::string_view scenarioSlug = "retrigger-release-tail";
constexpr std::string_view commonVcaScenarioSlug = "common-vca-level";
constexpr std::string_view bbdTransferScenarioSlug = "bbd-transfer-clock-law";
constexpr std::string_view voiceVcaFeedthroughScenarioSlug =
    "voice-vca-feedthrough";
constexpr std::string_view bbdHostGridAliasScenarioSlug =
    "bbd-host-grid-alias";
constexpr std::uint32_t eventScheduleSeed = 0x1061065du;
constexpr std::uint32_t commonVcaEventScheduleSeed = 0x106c0a11u;
constexpr std::uint32_t bbdTransferEventScheduleSeed = 0x106bbd31u;
constexpr double voiceVcaFeedthroughListeningGain = 31.622776601683793;
constexpr double voiceVcaFeedthroughListeningCeiling = 0.95;
constexpr std::uint64_t voiceVcaNoteOnSample = 20160u;
constexpr std::uint64_t voiceVcaNoteOffSample = 100800u;
constexpr std::uint64_t voiceVcaEndSample = 181440u;
constexpr float bbdAliasProbeFrequencyHz = 2093.0f;
constexpr float bbdAliasProbeAmplitude = 0.20f;
constexpr std::uint64_t bbdAliasWarmupFrames = 48000u;
constexpr std::uint64_t bbdAliasFixedProbeFrames = 144000u;
constexpr std::uint64_t bbdAliasSeparatorFrames = 4800u;
constexpr double bbdAliasListeningGain = 1.0;
constexpr double bbdAliasListeningCeiling = 0.95;
constexpr int scenarioProtocolVersion = 1;
constexpr std::string_view generatedBegin =
    "<!-- BEGIN GENERATED REALISM COMPARISON -->";
constexpr std::string_view generatedEnd =
    "<!-- END GENERATED REALISM COMPARISON -->";

struct Event
{
    std::uint64_t sample { 0 };
    const char* action { "" };
    int note { 0 };
};

struct VcaAutomationEvent
{
    std::uint64_t sample { 0 };
    std::uint8_t storedByte { 0 };
    const char* section { "" };
    const char* purpose { "" };
};

struct SectionEvent
{
    std::uint64_t sample { 0 };
    const char* name { "" };
    const char* chorus { "" };
};

struct AnalysisWindow
{
    const char* name { "" };
    std::uint64_t startSample { 0 };
    std::uint64_t endSample { 0 };
    const char* purpose { "" };
};

struct BbdAliasSection
{
    const char* name { "" };
    const char* kind { "" };
    const char* quality { "" };
    const char* chorus { "" };
    std::uint64_t startSample { 0 };
    std::uint64_t endSample { 0 };
    double requestedLfoCycles { 0.0 };
    double realisedLfoCycles { 0.0 };
};

constexpr std::array<AnalysisWindow, 5> voiceVcaAnalysisWindows {{
    { "pre-event-floor", 10080u, 20160u,
      "settled floor immediately before gate open" },
    { "gate-open-transient", 20160u, 40320u,
      "gate-open event and subsequent control settling" },
    { "held-settled-floor", 90720u, 100800u,
      "settled six-voice gate-open floor" },
    { "gate-close-transient", 100800u, 120960u,
      "gate-close event and subsequent control settling" },
    { "released-settled-floor", 171360u, 181440u,
      "settled floor at the end of the released tail" }
}};

struct RenderedScenario
{
    StereoBuffer audio;
    std::vector<Event> events;
    std::vector<int> releaseGapSamples;
    std::vector<VcaAutomationEvent> vcaAutomation;
    std::vector<SectionEvent> sections;
    std::vector<BbdAliasSection> bbdAliasSections;
};

class FixedRng
{
public:
    explicit FixedRng(std::uint32_t seed) : state_(seed) {}

    std::uint32_t next() noexcept
    {
        // xorshift32: this exact state transition is part of protocol v1.
        state_ ^= state_ << 13u;
        state_ ^= state_ >> 17u;
        state_ ^= state_ << 5u;
        return state_;
    }

private:
    std::uint32_t state_;
};

class Performance
{
public:
    explicit Performance(const EngineParameters& parameters,
                         bool highQuality = true)
    {
        engine_.prepare(static_cast<double>(comparisonSampleRate),
                        comparisonBlockSize, highQuality);
        engine_.setParameters(parameters);
    }

    void noteOn(int note)
    {
        events_.push_back({ cursor_, "note_on", note });
        engine_.noteOn(note, 1.0f);
    }

    void noteOff(int note)
    {
        events_.push_back({ cursor_, "note_off", note });
        engine_.noteOff(note);
    }

    void setParameters(const EngineParameters& parameters)
    {
        engine_.setParameters(parameters);
    }

    [[nodiscard]] std::uint64_t cursor() const noexcept { return cursor_; }

    void render(int samples)
    {
        std::array<float, comparisonBlockSize> left {};
        std::array<float, comparisonBlockSize> right {};
        int remaining = samples;
        while (remaining > 0)
        {
            const int count = std::min(comparisonBlockSize, remaining);
            std::fill(left.begin(), left.end(), 0.0f);
            std::fill(right.begin(), right.end(), 0.0f);
            engine_.process(left.data(), right.data(), count);
            audio_.left.insert(audio_.left.end(), left.begin(), left.begin() + count);
            audio_.right.insert(audio_.right.end(), right.begin(), right.begin() + count);
            cursor_ += static_cast<std::uint64_t>(count);
            remaining -= count;
        }
    }

    StereoBuffer takeAudio() { return std::move(audio_); }
    std::vector<Event> takeEvents() { return std::move(events_); }

    void resetCaptureClock()
    {
        audio_.left.clear();
        audio_.right.clear();
        events_.clear();
        cursor_ = 0;
    }

private:
    YouKnow106Engine engine_;
    StereoBuffer audio_;
    std::vector<Event> events_;
    std::uint64_t cursor_ { 0 };
};

EngineParameters retriggerPatch()
{
    EngineParameters parameters;
    parameters.keyMode = KeyMode::Poly1;
    parameters.polyphony = 1;
    parameters.range = DcoRange::Eight;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 0.72f;
    parameters.resonance = 0.18f;
    parameters.envDepth = 0.18f;
    parameters.keyFollow = 0.5f;
    parameters.attack = 0.0f;
    parameters.decay = 0.30f;
    parameters.sustain = 0.78f;
    parameters.release = 0.92f;
    parameters.vcaLevel = 1.0f;
    parameters.chorus = ChorusMode::Off;
    parameters.chorusNoise = 0.0f;
    parameters.velocityDepth = 0.0f;
    parameters.volume = 0.50f;
    parameters.calibration = 1.0f;
    return parameters;
}

RenderedScenario renderRetriggerReleaseTail()
{
    Performance performance(retriggerPatch());
    FixedRng rng(eventScheduleSeed);
    std::vector<int> gaps;

    constexpr int note = 60;
    constexpr int scanSamples = 202; // nearest host sample to the 4.2 ms scan
    performance.render(4800);         // 100 ms deterministic warm-up

    // A same-pitch release-tail reassignment avoids a DCO restart or pitch
    // discontinuity. The varying 2 ms + scan-phase gaps expose whether a note
    // assignment itself creates a VCA-control pulse before a converter write.
    for (int repetition = 0; repetition < 8; ++repetition)
    {
        performance.noteOn(note);
        performance.render(6720); // 140 ms: high, repeatable envelope level
        performance.noteOff(note);

        const int releaseGap = 96 + static_cast<int>(rng.next() % scanSamples);
        gaps.push_back(releaseGap);
        performance.render(releaseGap);

        performance.noteOn(note);
        performance.render(3840); // 80 ms observation window after retrigger
        performance.noteOff(note);
        performance.render(4320); // 90 ms release, still sounding next cycle
    }
    performance.render(24000); // retain the audible final release tail

    return { performance.takeAudio(), performance.takeEvents(), std::move(gaps),
             {}, {}, {} };
}

EngineParameters commonVcaPatch()
{
    EngineParameters parameters;
    parameters.keyMode = KeyMode::Poly1;
    parameters.polyphony = 6;
    parameters.range = DcoRange::Eight;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = true;
    parameters.pwmDepth = 0.36f;
    parameters.subLevel = 0.28f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 0.82f;
    parameters.resonance = 0.08f;
    parameters.envDepth = 0.0f;
    parameters.keyFollow = 0.5f;
    parameters.vcaMode = VcaMode::Gate;
    parameters.attack = 0.0f;
    parameters.decay = 0.0f;
    parameters.sustain = 1.0f;
    parameters.release = 0.25f;
    parameters.vcaLevel = 64.0f / 127.0f;
    parameters.chorus = ChorusMode::Off;
    parameters.chorusNoise = 0.0f;
    parameters.velocityDepth = 0.0f;
    parameters.volume = 0.35f;
    parameters.calibration = 1.0f;
    return parameters;
}

RenderedScenario renderCommonVcaLevel()
{
    auto parameters = commonVcaPatch();
    Performance performance(parameters);
    FixedRng rng(commonVcaEventScheduleSeed);
    std::vector<VcaAutomationEvent> automation {
        { 0u, 64u, "setup", "initial stored level" }
    };
    std::vector<SectionEvent> sections;

    const auto setVcaByte = [&](std::uint8_t storedByte, const char* section,
                                const char* purpose) {
        parameters.vcaLevel = static_cast<float>(storedByte) / 127.0f;
        // Guard the fixture's claim that it addresses exact hardware bytes.
        if (YouKnow106Engine::storedControlDacCode(parameters.vcaLevel)
            != static_cast<std::uint16_t>(storedByte) << 5u)
            std::abort();
        automation.push_back(
            { performance.cursor(), storedByte, section, purpose });
        performance.setParameters(parameters);
    };

    const auto chordOn = [&performance] {
        for (const int note : { 48, 55, 60 })
            performance.noteOn(note);
    };
    const auto chordOff = [&performance] {
        for (const int note : { 48, 55, 60 })
            performance.noteOff(note);
    };

    const auto runLevelSequence = [&](const char* section) {
        // Long plateaus expose the byte-to-steady-gain law without per-take
        // normalization. Every requested byte receives exactly 250 ms.
        for (const std::uint8_t storedByte : { 0u, 32u, 64u, 96u, 127u })
        {
            setVcaByte(storedByte, section, "steady plateau");
            performance.render(12000);
        }

        // Each 6.7--9.3 ms dwell is longer than one 4.2 ms converter pass plus
        // several current 687 us hold constants, but still fast enough to make
        // the scanned transition and analogue settling directly visible.
        for (const std::uint8_t storedByte :
             { 0u, 127u, 32u, 96u, 0u, 64u, 127u, 32u })
        {
            setVcaByte(storedByte, section, "rapid transition");
            performance.render(320 + static_cast<int>(rng.next() % 128u));
        }
        setVcaByte(64u, section, "section landing level");
        performance.render(2400);
    };

    performance.render(4800); // deterministic engine/control-scan warm-up
    sections.push_back({ performance.cursor(), "dry", "off" });
    chordOn();
    performance.render(7200); // settle gate envelope and the initial level
    runLevelSequence("dry");
    chordOff();
    performance.render(24000);

    parameters.chorus = ChorusMode::Two;
    performance.setParameters(parameters);
    sections.push_back({ performance.cursor(), "chorus-ii", "II" });
    performance.render(4800); // settle the bypass crossfade before excitation
    chordOn();
    performance.render(7200);
    runLevelSequence("chorus-ii");
    chordOff();
    performance.render(48000); // retain the envelope and BBD release tails

    return { performance.takeAudio(), performance.takeEvents(), {},
             std::move(automation), std::move(sections), {} };
}

EngineParameters bbdTransferPatch()
{
    EngineParameters parameters;
    parameters.keyMode = KeyMode::Poly1;
    parameters.polyphony = 6;
    parameters.range = DcoRange::Four;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = true;
    parameters.pwmDepth = 0.37f;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.envDepth = 0.0f;
    parameters.vcfLfoDepth = 0.0f;
    parameters.keyFollow = 0.5f;
    parameters.vcaMode = VcaMode::Gate;
    parameters.attack = 0.0f;
    parameters.decay = 0.0f;
    parameters.sustain = 1.0f;
    parameters.release = 0.20f;
    parameters.vcaLevel = 96.0f / 127.0f;
    parameters.chorus = ChorusMode::Off;
    parameters.chorusNoise = 0.0f;
    parameters.velocityDepth = 0.0f;
    parameters.volume = 0.28f;
    parameters.calibration = 1.0f;
    parameters.enableChorusClockBleed = false;
    parameters.enableChorusHyperbolicSweep = true;
    return parameters;
}

RenderedScenario renderBbdTransferClockLaw()
{
    auto parameters = bbdTransferPatch();
    Performance performance(parameters);
    FixedRng rng(bbdTransferEventScheduleSeed);
    std::vector<SectionEvent> sections;

    const auto dyadOn = [&performance] {
        for (const int note : { 60, 67 })
            performance.noteOn(note);
    };
    const auto dyadOff = [&performance] {
        for (const int note : { 60, 67 })
            performance.noteOff(note);
    };
    const auto samplesForCycles = [](ChorusMode mode, double cycles) {
        const double rate = Chorus::settingsFor(mode).rateHz;
        return static_cast<int>(std::lround(
            cycles * static_cast<double>(comparisonSampleRate) / rate));
    };

    // The BBDs and modulation oscillator free-run while muted. A dry reference
    // establishes the exact bright engine material without changing gain or
    // rendering a separate, independently normalised take.
    sections.push_back({ performance.cursor(), "silent-clock-warmup", "off" });
    performance.render(48000 + static_cast<int>(rng.next() % 4096u));
    sections.push_back({ performance.cursor(), "dry-reference", "off" });
    dyadOn();
    performance.render(72000);
    dyadOff();
    performance.render(12000 + static_cast<int>(rng.next() % 4096u));

    // Each sounding mode begins only after the 5 ms wet-mute glide and all
    // millisecond-scale support filters have settled. The held dyad then spans
    // 3.5 complete triangle cycles, so both anti-phase lines repeatedly reach
    // the minimum and maximum clock excursions rather than sampling one LFO
    // phase and calling it representative.
    parameters.chorus = ChorusMode::One;
    performance.setParameters(parameters);
    sections.push_back({ performance.cursor(), "chorus-i-settle", "I" });
    performance.render(24000 + static_cast<int>(rng.next() % 4096u));
    sections.push_back({ performance.cursor(), "chorus-i-clock-excursions", "I" });
    dyadOn();
    performance.render(samplesForCycles(ChorusMode::One, 3.5));
    dyadOff();
    performance.render(12000 + static_cast<int>(rng.next() % 4096u));

    parameters.chorus = ChorusMode::Two;
    performance.setParameters(parameters);
    sections.push_back({ performance.cursor(), "chorus-ii-settle", "II" });
    performance.render(24000 + static_cast<int>(rng.next() % 4096u));
    sections.push_back({ performance.cursor(), "chorus-ii-clock-excursions", "II" });
    dyadOn();
    performance.render(samplesForCycles(ChorusMode::Two, 3.5));
    dyadOff();
    performance.render(48000); // retain the envelope and BBD output tails

    return { performance.takeAudio(), performance.takeEvents(), {}, {},
             std::move(sections), {} };
}

EngineParameters voiceVcaFeedthroughPatch()
{
    EngineParameters parameters;
    parameters.keyMode = KeyMode::Unison;
    parameters.polyphony = 6;
    parameters.range = DcoRange::Eight;
    parameters.sawEnabled = false;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 0.0f;
    parameters.resonance = 0.0f;
    parameters.envDepth = 0.0f;
    parameters.vcfLfoDepth = 0.0f;
    parameters.keyFollow = 0.0f;
    parameters.vcaMode = VcaMode::Gate;
    parameters.attack = 0.0f;
    parameters.decay = 0.0f;
    parameters.sustain = 1.0f;
    parameters.release = 0.0f;
    parameters.vcaLevel = 1.0f;
    parameters.chorus = ChorusMode::Off;
    parameters.chorusNoise = 0.0f;
    parameters.velocityDepth = 0.0f;
    parameters.volume = 1.0f;
    parameters.calibration = 1.0f;
    parameters.enableVcfStageOffsets = false;
    parameters.enableOpAmpSlewLimiting = false;
    parameters.enableVcfEarlyEffect = false;
    parameters.enableSpatialThermalGradient = false;
    parameters.enableChorusClockBleed = false;
    parameters.enableChorusHyperbolicSweep = false;
    parameters.enableElectrolyticC14Nonlinearity = false;
    return parameters;
}

RenderedScenario renderVoiceVcaFeedthrough()
{
    Performance performance(voiceVcaFeedthroughPatch());
    std::vector<SectionEvent> sections {
        { 0u, "silent-pre-roll", "off" }
    };

    performance.render(static_cast<int>(voiceVcaNoteOnSample));
    sections.push_back({ performance.cursor(), "six-voice-gate-open", "off" });
    performance.noteOn(60);
    performance.render(static_cast<int>(voiceVcaNoteOffSample
                                        - voiceVcaNoteOnSample));
    sections.push_back({ performance.cursor(), "six-voice-gate-close", "off" });
    performance.noteOff(60);
    performance.render(static_cast<int>(voiceVcaEndSample
                                        - voiceVcaNoteOffSample));

    if (performance.cursor() != voiceVcaEndSample)
        std::abort();
    return { performance.takeAudio(), performance.takeEvents(), {}, {},
             std::move(sections), {} };
}

void appendAudio(StereoBuffer& destination, StereoBuffer source)
{
    destination.left.insert(destination.left.end(), source.left.begin(),
                            source.left.end());
    destination.right.insert(destination.right.end(), source.right.begin(),
                             source.right.end());
}

void appendSilence(StereoBuffer& destination, std::uint64_t frames)
{
    destination.left.insert(destination.left.end(),
                            static_cast<std::size_t>(frames), 0.0f);
    destination.right.insert(destination.right.end(),
                             static_cast<std::size_t>(frames), 0.0f);
}

EngineParameters bbdHostGridAliasPatch(ChorusMode mode)
{
    EngineParameters parameters;
    parameters.keyMode = KeyMode::Poly1;
    parameters.polyphony = 6;
    parameters.range = DcoRange::Four;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = true;
    parameters.pwmDepth = 0.31f;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.envDepth = 0.0f;
    parameters.vcfLfoDepth = 0.0f;
    parameters.keyFollow = 0.5f;
    parameters.vcaMode = VcaMode::Gate;
    parameters.attack = 0.0f;
    parameters.decay = 0.0f;
    parameters.sustain = 1.0f;
    parameters.release = 0.12f;
    parameters.vcaLevel = 96.0f / 127.0f;
    parameters.chorus = mode;
    parameters.chorusNoise = 0.0f;
    parameters.velocityDepth = 0.0f;
    parameters.volume = 0.14f;
    parameters.calibration = 1.0f;
    parameters.enableVcfStageOffsets = false;
    parameters.enableOpAmpSlewLimiting = false;
    parameters.enableVcfEarlyEffect = false;
    parameters.enableSpatialThermalGradient = false;
    parameters.enableChorusClockBleed = false;
    parameters.enableChorusHyperbolicSweep = true;
    parameters.enableElectrolyticC14Nonlinearity = false;
    return parameters;
}

RenderedScenario renderBbdHostGridAlias()
{
    RenderedScenario result;
    const auto settingsOne = Chorus::settingsFor(ChorusMode::One);
    const float slowestClockHz = Chorus::clockForDelaySeconds(
        settingsOne.centreDelaySeconds + settingsOne.sweepSeconds);

    const auto appendSection = [&](const char* name, const char* kind,
                                   const char* quality, const char* chorus,
                                   double requestedCycles,
                                   double realisedCycles,
                                   StereoBuffer audio,
                                   std::vector<Event> events = {}) {
        if (!result.audio.left.empty())
            appendSilence(result.audio, bbdAliasSeparatorFrames);
        const auto start = static_cast<std::uint64_t>(result.audio.left.size());
        for (auto& event : events)
            event.sample += start;
        result.events.insert(result.events.end(), events.begin(), events.end());
        appendAudio(result.audio, std::move(audio));
        const auto end = static_cast<std::uint64_t>(result.audio.left.size());
        result.sections.push_back({ start, name, chorus });
        result.bbdAliasSections.push_back(
            { name, kind, quality, chorus, start, end,
              requestedCycles, realisedCycles });
    };

    // A fixed endpoint clock makes the physical image at f_clock - f_input and
    // the host-folded images of out-of-band k*f_clock +/- f_input stationary.
    // This is the stationary section in which exact-frequency projections are
    // meaningful without a moving clock trajectory smearing each target.
    for (const bool highQuality : { true, false })
    {
        appendSection(
            highQuality ? "fixed-min-clock-tone-hq"
                        : "fixed-min-clock-tone-lq",
            "direct-fixed-clock-analysis",
            highQuality ? "HQ / 192 kHz internal"
                        : "LQ / 48 kHz internal",
            "fixed minimum endpoint clock", 0.0, 0.0,
            YouKnow106TestAccess::renderFixedBbdProbe(
                highQuality, bbdAliasWarmupFrames, bbdAliasFixedProbeFrames,
                bbdAliasProbeFrequencyHz, bbdAliasProbeAmplitude,
                slowestClockHz));
    }

    struct SweepCase
    {
        bool highQuality;
        ChorusMode mode;
        const char* name;
        const char* quality;
        const char* chorus;
    };
    constexpr std::array<SweepCase, 4> sweepCases {{
        { true, ChorusMode::One, "swept-tone-hq-chorus-i",
          "HQ / 192 kHz internal", "I" },
        { true, ChorusMode::Two, "swept-tone-hq-chorus-ii",
          "HQ / 192 kHz internal", "II" },
        { false, ChorusMode::One, "swept-tone-lq-chorus-i",
          "LQ / 48 kHz internal", "I" },
        { false, ChorusMode::Two, "swept-tone-lq-chorus-ii",
          "LQ / 48 kHz internal", "II" }
    }};

    for (const auto& item : sweepCases)
    {
        const double rate = Chorus::settingsFor(item.mode).rateHz;
        const auto frames = static_cast<std::uint64_t>(std::ceil(
            static_cast<double>(comparisonSampleRate) / rate));
        const double realisedCycles = static_cast<double>(frames) * rate
                                    / comparisonSampleRate;
        appendSection(
            item.name, "direct-swept-wet-tone", item.quality, item.chorus,
            1.0, realisedCycles,
            YouKnow106TestAccess::renderSweptBbdProbe(
                item.highQuality, item.mode, bbdAliasWarmupFrames, frames,
                bbdAliasProbeFrequencyHz, bbdAliasProbeAmplitude));
    }

    // Repeat the same four complete sweeps through the shipping engine. Short
    // bright stabs distribute broadband musical energy over every LFO phase
    // without changing gain between quality settings or modes.
    for (const auto& item : sweepCases)
    {
        const double rate = Chorus::settingsFor(item.mode).rateHz;
        const auto frames = static_cast<std::uint64_t>(std::ceil(
            static_cast<double>(comparisonSampleRate) / rate));
        const double realisedCycles = static_cast<double>(frames) * rate
                                    / comparisonSampleRate;
        Performance performance(
            bbdHostGridAliasPatch(item.mode), item.highQuality);
        performance.render(static_cast<int>(bbdAliasWarmupFrames));
        performance.resetCaptureClock();

        std::uint64_t remaining = frames;
        while (remaining > 0u)
        {
            for (const int note : { 60, 67, 72 })
                performance.noteOn(note);
            const auto onFrames = std::min<std::uint64_t>(7200u, remaining);
            performance.render(static_cast<int>(onFrames));
            remaining -= onFrames;
            for (const int note : { 60, 67, 72 })
                performance.noteOff(note);
            if (remaining == 0u)
                break;
            const auto offFrames = std::min<std::uint64_t>(4800u, remaining);
            performance.render(static_cast<int>(offFrames));
            remaining -= offFrames;
        }

        // The section metadata stores pointers, so select stable literals.
        const char* stableName = item.highQuality
            ? (item.mode == ChorusMode::One ? "musical-stabs-hq-chorus-i"
                                            : "musical-stabs-hq-chorus-ii")
            : (item.mode == ChorusMode::One ? "musical-stabs-lq-chorus-i"
                                            : "musical-stabs-lq-chorus-ii");
        appendSection(stableName, "full-engine-musical-stabs", item.quality,
                      item.chorus, 1.0, realisedCycles,
                      performance.takeAudio(), performance.takeEvents());
    }

    return result;
}

std::string jsonEscape(std::string_view input)
{
    std::string output;
    for (const char value : input)
    {
        switch (value)
        {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default: output += value; break;
        }
    }
    return output;
}

std::string levelJson(const Level& level, int indent)
{
    const std::string spaces(static_cast<std::size_t>(indent), ' ');
    std::ostringstream output;
    output << "{\n"
           << spaces << "  \"peak_linear\": " << jsonNumber(level.peak) << ",\n"
           << spaces << "  \"peak_dbfs\": " << jsonNumber(decibels(level.peak)) << ",\n"
           << spaces << "  \"rms_linear\": " << jsonNumber(level.rms) << ",\n"
           << spaces << "  \"rms_dbfs\": " << jsonNumber(decibels(level.rms)) << "\n"
           << spaces << "}";
    return output.str();
}

std::string eventJson(const std::vector<Event>& events)
{
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < events.size(); ++index)
    {
        const auto& event = events[index];
        output << "    { \"sample\": " << event.sample
               << ", \"action\": \"" << event.action
               << "\", \"note\": " << event.note << " }";
        if (index + 1 < events.size())
            output << ',';
        output << '\n';
    }
    output << "  ]";
    return output.str();
}

std::string gapsJson(const std::vector<int>& gaps)
{
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < gaps.size(); ++index)
    {
        if (index > 0)
            output << ", ";
        output << gaps[index];
    }
    output << ']';
    return output.str();
}

std::string vcaAutomationJson(
    const std::vector<VcaAutomationEvent>& automation)
{
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < automation.size(); ++index)
    {
        const auto& event = automation[index];
        const double normalised = static_cast<double>(event.storedByte) / 127.0;
        output << "    { \"host_parameter_set_sample\": " << event.sample
               << ", \"stored_byte\": " << static_cast<unsigned>(event.storedByte)
               << ", \"normalised_panel\": " << jsonNumber(normalised)
               << ", \"dac12_code\": "
               << (static_cast<unsigned>(event.storedByte) << 5u)
               << ", \"section\": \"" << event.section
               << "\", \"purpose\": \"" << event.purpose << "\" }";
        if (index + 1 < automation.size())
            output << ',';
        output << '\n';
    }
    output << "  ]";
    return output.str();
}

std::string sectionJson(const std::vector<SectionEvent>& sections)
{
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < sections.size(); ++index)
    {
        const auto& section = sections[index];
        output << "    { \"start_sample\": " << section.sample
               << ", \"name\": \"" << section.name
               << "\", \"chorus\": \"" << section.chorus << "\" }";
        if (index + 1 < sections.size())
            output << ',';
        output << '\n';
    }
    output << "  ]";
    return output.str();
}

struct WindowMetrics
{
    double positivePeak { 0.0 };
    double negativePeak { 0.0 };
    double absolutePeak { 0.0 };
    double rms { 0.0 };
    double mean { 0.0 };
    double lowBandRms20To200Hz { 0.0 };
    double maximumStereoDifference { 0.0 };
    std::uint64_t peakAbsoluteSample { 0 };
    std::uint64_t peakWindowOffset { 0 };
};

WindowMetrics measureWindow(const StereoBuffer& audio,
                            const AnalysisWindow& window)
{
    WindowMetrics metrics;
    if (window.endSample <= window.startSample
        || window.endSample > audio.left.size()
        || audio.left.size() != audio.right.size())
        return metrics;

    const double highPassCoefficient = 1.0 - std::exp(
        -2.0 * 3.14159265358979323846 * 20.0
        / static_cast<double>(comparisonSampleRate));
    const double lowPassCoefficient = 1.0 - std::exp(
        -2.0 * 3.14159265358979323846 * 200.0
        / static_cast<double>(comparisonSampleRate));
    double low20Left = 0.0;
    double low20Right = 0.0;
    double band200Left = 0.0;
    double band200Right = 0.0;
    long double sum = 0.0;
    long double sumSquares = 0.0;
    long double lowBandSquares = 0.0;
    // Prime the diagnostic filters from sample zero so each window observes
    // the same continuous 20--200 Hz analysis signal rather than a new filter
    // startup transient at its left edge.
    for (std::uint64_t frame = 0; frame < window.endSample; ++frame)
    {
        const double left = audio.left[static_cast<std::size_t>(frame)];
        const double right = audio.right[static_cast<std::size_t>(frame)];
        low20Left += highPassCoefficient * (left - low20Left);
        low20Right += highPassCoefficient * (right - low20Right);
        const double highLeft = left - low20Left;
        const double highRight = right - low20Right;
        band200Left += lowPassCoefficient * (highLeft - band200Left);
        band200Right += lowPassCoefficient * (highRight - band200Right);

        if (frame < window.startSample)
            continue;

        metrics.positivePeak = std::max({ metrics.positivePeak, left, right });
        metrics.negativePeak = std::min({ metrics.negativePeak, left, right });
        const double framePeak = std::max(std::abs(left), std::abs(right));
        if (framePeak > metrics.absolutePeak)
        {
            metrics.absolutePeak = framePeak;
            metrics.peakAbsoluteSample = frame;
            metrics.peakWindowOffset = frame - window.startSample;
        }
        metrics.maximumStereoDifference = std::max(
            metrics.maximumStereoDifference, std::abs(left - right));
        sum += left + right;
        sumSquares += static_cast<long double>(left) * left;
        sumSquares += static_cast<long double>(right) * right;
        lowBandSquares += static_cast<long double>(band200Left) * band200Left;
        lowBandSquares += static_cast<long double>(band200Right) * band200Right;
    }

    const auto samples = static_cast<long double>(
        (window.endSample - window.startSample) * 2u);
    metrics.rms = std::sqrt(static_cast<double>(sumSquares / samples));
    metrics.mean = static_cast<double>(sum / samples);
    metrics.lowBandRms20To200Hz =
        std::sqrt(static_cast<double>(lowBandSquares / samples));
    return metrics;
}

std::string voiceVcaWindowsJson()
{
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < voiceVcaAnalysisWindows.size(); ++index)
    {
        const auto& window = voiceVcaAnalysisWindows[index];
        output << "    { \"name\": \"" << window.name
               << "\", \"start_sample\": " << window.startSample
               << ", \"end_sample_exclusive\": " << window.endSample
               << ", \"frames\": " << (window.endSample - window.startSample)
               << ", \"purpose\": \"" << window.purpose << "\" }";
        if (index + 1 < voiceVcaAnalysisWindows.size())
            output << ',';
        output << '\n';
    }
    output << "  ]";
    return output.str();
}

std::string voiceVcaWindowMetricsJson(const StereoBuffer& audio, int indent)
{
    const std::string spaces(static_cast<std::size_t>(indent), ' ');
    std::ostringstream output;
    output << "{\n";
    for (std::size_t index = 0; index < voiceVcaAnalysisWindows.size(); ++index)
    {
        const auto& window = voiceVcaAnalysisWindows[index];
        const auto metrics = measureWindow(audio, window);
        output << spaces << "  \"" << window.name << "\": {\n"
               << spaces << "    \"positive_peak\": "
               << jsonNumber(metrics.positivePeak) << ",\n"
               << spaces << "    \"negative_peak\": "
               << jsonNumber(metrics.negativePeak) << ",\n"
               << spaces << "    \"absolute_peak\": "
               << jsonNumber(metrics.absolutePeak) << ",\n"
               << spaces << "    \"rms\": " << jsonNumber(metrics.rms) << ",\n"
               << spaces << "    \"mean\": " << jsonNumber(metrics.mean) << ",\n"
               << spaces << "    \"rms_20_to_200_hz\": "
               << jsonNumber(metrics.lowBandRms20To200Hz) << ",\n"
               << spaces << "    \"maximum_left_right_difference\": "
               << jsonNumber(metrics.maximumStereoDifference) << ",\n"
               << spaces << "    \"absolute_peak_sample\": "
               << metrics.peakAbsoluteSample << ",\n"
               << spaces << "    \"absolute_peak_window_offset\": "
               << metrics.peakWindowOffset << "\n"
               << spaces << "  }";
        if (index + 1 < voiceVcaAnalysisWindows.size())
            output << ',';
        output << '\n';
    }
    output << spaces << '}';
    return output.str();
}

std::string bbdAliasSectionsJson(
    const std::vector<BbdAliasSection>& sections)
{
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0; index < sections.size(); ++index)
    {
        const auto& section = sections[index];
        output << "    { \"name\": \"" << section.name
               << "\", \"kind\": \"" << section.kind
               << "\", \"quality\": \"" << section.quality
               << "\", \"chorus\": \"" << section.chorus
               << "\", \"start_sample\": " << section.startSample
               << ", \"end_sample_exclusive\": " << section.endSample
               << ", \"frames\": "
               << (section.endSample - section.startSample)
               << ", \"requested_lfo_cycles\": "
               << jsonNumber(section.requestedLfoCycles)
               << ", \"realised_lfo_cycles\": "
               << jsonNumber(section.realisedLfoCycles) << " }";
        if (index + 1 < sections.size())
            output << ',';
        output << '\n';
    }
    output << "  ]";
    return output.str();
}

const BbdAliasSection* findBbdAliasSection(
    const RenderedScenario& rendered, std::string_view name)
{
    for (const auto& section : rendered.bbdAliasSections)
        if (name == section.name)
            return &section;
    return nullptr;
}

double foldedToHostNyquist(double frequencyHz) noexcept
{
    const double sampleRate = static_cast<double>(comparisonSampleRate);
    const double wrapped = frequencyHz
                         - std::floor(frequencyHz / sampleRate) * sampleRate;
    return wrapped <= 0.5 * sampleRate ? wrapped : sampleRate - wrapped;
}

double hannToneAmplitude(const StereoBuffer& audio,
                         const BbdAliasSection& section,
                         double frequencyHz) noexcept
{
    if (section.endSample <= section.startSample
        || section.endSample > audio.left.size())
        return 0.0;
    const auto frames = section.endSample - section.startSample;
    if (frames < 2u)
        return 0.0;

    long double cosineLeft = 0.0;
    long double sineLeft = 0.0;
    long double cosineRight = 0.0;
    long double sineRight = 0.0;
    long double windowSum = 0.0;
    for (std::uint64_t offset = 0; offset < frames; ++offset)
    {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * 3.14159265358979323846 * static_cast<double>(offset)
            / static_cast<double>(frames - 1u));
        const double phase = 2.0 * 3.14159265358979323846 * frequencyHz
                           * static_cast<double>(offset)
                           / static_cast<double>(comparisonSampleRate);
        const double cosine = std::cos(phase);
        const double sine = std::sin(phase);
        const auto frame = static_cast<std::size_t>(
            section.startSample + offset);
        cosineLeft += audio.left[frame] * window * cosine;
        sineLeft -= audio.left[frame] * window * sine;
        cosineRight += audio.right[frame] * window * cosine;
        sineRight -= audio.right[frame] * window * sine;
        windowSum += window;
    }
    if (windowSum <= 0.0)
        return 0.0;
    const double left = 2.0 * std::hypot(
        static_cast<double>(cosineLeft), static_cast<double>(sineLeft))
        / static_cast<double>(windowSum);
    const double right = 2.0 * std::hypot(
        static_cast<double>(cosineRight), static_cast<double>(sineRight))
        / static_cast<double>(windowSum);
    return std::sqrt(0.5 * (left * left + right * right));
}

Level measureBbdAliasSection(const StereoBuffer& audio,
                             const BbdAliasSection& section) noexcept
{
    Level result;
    if (section.endSample <= section.startSample
        || section.endSample > audio.left.size()
        || section.endSample > audio.right.size())
        return result;

    long double sumSquares = 0.0;
    for (auto frame = section.startSample; frame < section.endSample; ++frame)
    {
        const double left = audio.left[static_cast<std::size_t>(frame)];
        const double right = audio.right[static_cast<std::size_t>(frame)];
        result.peak = std::max(
            { result.peak, std::abs(left), std::abs(right) });
        sumSquares += static_cast<long double>(left) * left;
        sumSquares += static_cast<long double>(right) * right;
    }

    const auto samples = static_cast<long double>(
        section.endSample - section.startSample) * 2.0L;
    result.rms = std::sqrt(static_cast<double>(sumSquares / samples));
    return result;
}

struct BbdAliasComparisonTarget
{
    const char* name;
    const char* displayName;
    double sourceFrequencyHz;
    double observedFrequencyHz;
};

std::array<BbdAliasComparisonTarget, 4> bbdAliasComparisonTargets()
{
    const auto settings = Chorus::settingsFor(ChorusMode::One);
    const double clock = Chorus::clockForDelaySeconds(
        settings.centreDelaySeconds + settings.sweepSeconds);
    const double input = bbdAliasProbeFrequencyHz;
    return {{
        { "physical-in-band-k1-minus", "wanted physical k1-minus",
          clock - input, clock - input },
        { "host-fold-k1-plus", "unwanted folded k1-plus",
          clock + input, foldedToHostNyquist(clock + input) },
        { "host-fold-k2-minus", "unwanted folded k2-minus",
          2.0 * clock - input, foldedToHostNyquist(2.0 * clock - input) },
        { "host-fold-k2-plus", "unwanted folded k2-plus",
          2.0 * clock + input, foldedToHostNyquist(2.0 * clock + input) }
    }};
}

std::uint64_t interleavedFloatHash(const StereoBuffer& audio) noexcept
{
    constexpr std::uint64_t basis = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = basis;
    const auto mix = [&](std::uint32_t bits) {
        for (int byte = 0; byte < 4; ++byte)
        {
            hash ^= (bits >> (8 * byte)) & 0xffu;
            hash *= prime;
        }
    };
    for (std::size_t frame = 0; frame < audio.left.size(); ++frame)
    {
        mix(std::bit_cast<std::uint32_t>(audio.left[frame]));
        mix(std::bit_cast<std::uint32_t>(audio.right[frame]));
    }
    return hash;
}

std::string hex64(std::uint64_t value)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << value;
    return output.str();
}

struct BbdAliasBaselineIdentity
{
    std::string dspSourceSha256;
    std::string rawSampleHashFnv1a64;
};

bool isLowerHex(std::string_view value, std::size_t digits) noexcept
{
    if (value.size() != digits)
        return false;
    for (const char character : value)
    {
        if (!((character >= '0' && character <= '9')
              || (character >= 'a' && character <= 'f')))
            return false;
    }
    return true;
}

bool requireUniqueManifestFragment(std::string_view manifest,
                                   std::string_view fragment,
                                   std::string& error)
{
    const auto first = manifest.find(fragment);
    if (first == std::string_view::npos
        || manifest.find(fragment, first + fragment.size())
               != std::string_view::npos)
    {
        error = "baseline manifest is missing or duplicates contract field: "
              + std::string(fragment);
        return false;
    }
    return true;
}

bool extractUniqueManifestString(std::string_view manifest,
                                 std::string_view field,
                                 std::string& value,
                                 std::string& error)
{
    const std::string prefix = "\"" + std::string(field) + "\": \"";
    const auto first = manifest.find(prefix);
    if (first == std::string_view::npos
        || manifest.find(prefix, first + prefix.size())
               != std::string_view::npos)
    {
        error = "baseline manifest is missing or duplicates string field: "
              + std::string(field);
        return false;
    }
    const auto valueBegin = first + prefix.size();
    const auto valueEnd = manifest.find('"', valueBegin);
    if (valueEnd == std::string_view::npos)
    {
        error = "baseline manifest has an unterminated string field: "
              + std::string(field);
        return false;
    }
    value.assign(manifest.substr(valueBegin, valueEnd - valueBegin));
    return true;
}

bool validateBbdAliasBaselineManifestText(
    std::string_view manifest, const StereoBuffer& beforeAudio,
    std::size_t expectedFrames, BbdAliasBaselineIdentity& identity,
    std::string& error)
{
    const std::array<std::string, 8> required {{
        "  \"schema_version\": 1,",
        "  \"tool\": \"YouKnow106RenderRealismComparison\",",
        "  \"scenario\": \"bbd-host-grid-alias\",",
        "  \"scenario_protocol_version\": 1,",
        "  \"stage\": \"before\",",
        "  \"sample_rate_hz\": 48000,",
        "  \"channels\": 2,",
        "  \"frames\": " + std::to_string(expectedFrames) + ","
    }};
    for (const auto& fragment : required)
        if (!requireUniqueManifestFragment(manifest, fragment, error))
            return false;

    if (!extractUniqueManifestString(
            manifest, "dsp_source_sha256", identity.dspSourceSha256, error)
        || !isLowerHex(identity.dspSourceSha256, 64u))
    {
        if (error.empty())
            error = "baseline manifest has no valid 64-digit DSP source SHA-256";
        return false;
    }

    const std::string_view currentFingerprint = YOUKNOW106_DSP_SOURCE_SHA256;
    if (!isLowerHex(currentFingerprint, 64u))
    {
        error = "this build has no valid DSP source SHA-256 fingerprint";
        return false;
    }
    if (identity.dspSourceSha256 == currentFingerprint)
    {
        error = "baseline and after build have the same DSP source fingerprint; "
                "refusing a same-build comparison";
        return false;
    }

    identity.rawSampleHashFnv1a64 = hex64(interleavedFloatHash(beforeAudio));
    if (!requireUniqueManifestFragment(
            manifest,
            "\"before_raw\": \"" + identity.rawSampleHashFnv1a64 + "\"",
            error))
    {
        error = "baseline WAV sample hash does not match its before manifest";
        return false;
    }
    return true;
}

bool readAndValidateBbdAliasBaselineManifest(
    const std::filesystem::path& path, const StereoBuffer& beforeAudio,
    std::size_t expectedFrames, BbdAliasBaselineIdentity& identity,
    std::string& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "cannot open baseline manifest " + path.string();
        return false;
    }
    std::ostringstream loaded;
    loaded << stream.rdbuf();
    if (!stream.good() && !stream.eof())
    {
        error = "failed while reading baseline manifest " + path.string();
        return false;
    }
    return validateBbdAliasBaselineManifestText(
        loaded.str(), beforeAudio, expectedFrames, identity, error);
}

std::string bbdAliasFixedProbeMetricsJson(
    const RenderedScenario& rendered)
{
    const auto settings = Chorus::settingsFor(ChorusMode::One);
    const double clock = Chorus::clockForDelaySeconds(
        settings.centreDelaySeconds + settings.sweepSeconds);
    const double input = bbdAliasProbeFrequencyHz;
    struct Target
    {
        const char* name;
        const char* classification;
        double sourceFrequencyHz;
        double observedFrequencyHz;
    };
    const std::array<Target, 5> targets {{
        { "input-fundamental", "wanted input tone", input, input },
        { "physical-in-band-k1-minus",
          "wanted physical BBD image below host Nyquist",
          clock - input, clock - input },
        { "host-fold-k1-plus",
          "unwanted host-grid fold of out-of-band physical image",
          clock + input, foldedToHostNyquist(clock + input) },
        { "host-fold-k2-minus",
          "unwanted host-grid fold of out-of-band physical image",
          2.0 * clock - input,
          foldedToHostNyquist(2.0 * clock - input) },
        { "host-fold-k2-plus",
          "unwanted host-grid fold of out-of-band physical image",
          2.0 * clock + input,
          foldedToHostNyquist(2.0 * clock + input) }
    }};

    std::ostringstream output;
    output << "{\n";
    constexpr std::array<std::string_view, 2> names {
        "fixed-min-clock-tone-hq", "fixed-min-clock-tone-lq"
    };
    constexpr std::array<std::string_view, 2> labels {
        "hq_192khz_internal", "lq_48khz_internal"
    };
    for (std::size_t quality = 0; quality < names.size(); ++quality)
    {
        const auto* section = findBbdAliasSection(rendered, names[quality]);
        output << "    \"" << labels[quality] << "\": {\n";
        if (section == nullptr)
        {
            output << "      \"error\": \"section missing\"\n    }";
        }
        else
        {
            const double fundamental = hannToneAmplitude(
                rendered.audio, *section, input);
            output << "      \"window\": \"Hann over complete fixed-clock section\",\n"
                   << "      \"measurement\": \"single exact-frequency complex projection; not an integrated spectral mask\",\n"
                   << "      \"targets\": {\n";
            for (std::size_t index = 0; index < targets.size(); ++index)
            {
                const auto& target = targets[index];
                const double amplitude = hannToneAmplitude(
                    rendered.audio, *section, target.observedFrequencyHz);
                output << "        \"" << target.name << "\": {\n"
                       << "          \"classification\": \""
                       << target.classification << "\",\n"
                       << "          \"source_frequency_hz\": "
                       << jsonNumber(target.sourceFrequencyHz) << ",\n"
                       << "          \"observed_frequency_hz\": "
                       << jsonNumber(target.observedFrequencyHz) << ",\n"
                       << "          \"hann_amplitude_linear\": "
                       << jsonNumber(amplitude) << ",\n"
                       << "          \"hann_amplitude_dbfs\": "
                       << jsonNumber(decibels(amplitude)) << ",\n"
                       << "          \"relative_to_fundamental_dbc\": "
                       << jsonNumber(decibels(amplitude / std::max(
                              fundamental, 1.0e-18))) << "\n"
                       << "        }";
                if (index + 1 < targets.size())
                    output << ',';
                output << '\n';
            }
            output << "      }\n    }";
        }
        if (quality + 1 < names.size())
            output << ',';
        output << '\n';
    }
    output << "  }";
    return output.str();
}

std::string commonManifestPrefix(std::string_view stage,
                                 const RenderedScenario& rendered)
{
    std::ostringstream output;
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"tool\": \"YouKnow106RenderRealismComparison\",\n"
           << "  \"scenario\": \"" << scenarioSlug << "\",\n"
           << "  \"scenario_protocol_version\": " << scenarioProtocolVersion << ",\n"
           << "  \"stage\": \"" << stage << "\",\n"
           << "  \"dsp_source_sha256\": \"" << YOUKNOW106_DSP_SOURCE_SHA256 << "\",\n"
           << "  \"compiler_id\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_ID) << "\",\n"
           << "  \"compiler_version\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_VERSION) << "\",\n"
           << "  \"sample_rate_hz\": " << comparisonSampleRate << ",\n"
           << "  \"block_size_samples\": " << comparisonBlockSize << ",\n"
           << "  \"high_quality\": true,\n"
           << "  \"channels\": 2,\n"
           << "  \"frames\": " << rendered.audio.left.size() << ",\n"
           << "  \"wav_encoding\": \"IEEE-754 float32 little-endian\",\n"
           << "  \"event_schedule_seed_hex\": \"0x1061065d\",\n"
           << "  \"engine_seed_policy\": "
              "\"fixed per-card hash seeds rooted at 17; chorus noise disabled\",\n"
           << "  \"patch\": {\n"
           << "    \"key_mode\": \"Poly 1\",\n"
           << "    \"polyphony\": 1,\n"
           << "    \"range\": \"8 foot\",\n"
           << "    \"saw_enabled\": true,\n"
           << "    \"pulse_enabled\": false,\n"
           << "    \"sub_level\": 0.0,\n"
           << "    \"noise_level\": 0.0,\n"
           << "    \"high_pass\": \"1\",\n"
           << "    \"cutoff\": 0.72,\n"
           << "    \"resonance\": 0.18,\n"
           << "    \"envelope_depth\": 0.18,\n"
           << "    \"key_follow\": 0.5,\n"
           << "    \"attack\": 0.0,\n"
           << "    \"decay\": 0.30,\n"
           << "    \"sustain\": 0.78,\n"
           << "    \"release\": 0.92,\n"
           << "    \"vca_level\": 1.0,\n"
           << "    \"chorus\": \"off\",\n"
           << "    \"chorus_noise\": 0.0,\n"
           << "    \"velocity_depth\": 0.0,\n"
           << "    \"volume\": 0.50,\n"
           << "    \"unit_character\": 1.0\n"
           << "  },\n"
           << "  \"release_gap_samples\": " << gapsJson(rendered.releaseGapSamples)
           << ",\n"
           << "  \"midi_events\": " << eventJson(rendered.events) << ",\n";
    return output.str();
}

std::string commonVcaManifestPrefix(std::string_view stage,
                                    const RenderedScenario& rendered)
{
    std::ostringstream output;
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"tool\": \"YouKnow106RenderRealismComparison\",\n"
           << "  \"scenario\": \"" << commonVcaScenarioSlug << "\",\n"
           << "  \"scenario_protocol_version\": " << scenarioProtocolVersion << ",\n"
           << "  \"stage\": \"" << stage << "\",\n"
           << "  \"dsp_source_sha256\": \"" << YOUKNOW106_DSP_SOURCE_SHA256 << "\",\n"
           << "  \"compiler_id\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_ID) << "\",\n"
           << "  \"compiler_version\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_VERSION) << "\",\n"
           << "  \"sample_rate_hz\": " << comparisonSampleRate << ",\n"
           << "  \"block_size_samples\": " << comparisonBlockSize << ",\n"
           << "  \"high_quality\": true,\n"
           << "  \"channels\": 2,\n"
           << "  \"frames\": " << rendered.audio.left.size() << ",\n"
           << "  \"wav_encoding\": \"IEEE-754 float32 little-endian\",\n"
           << "  \"event_schedule_seed_hex\": \"0x106c0a11\",\n"
           << "  \"engine_seed_policy\": "
              "\"fixed per-card hash seeds rooted at 17; chorus noise disabled\",\n"
           << "  \"automation_sample_semantics\": "
              "\"host parameter is set immediately before this host sample; the modelled converter consumes the byte at its next scheduled CommonVca write\",\n"
           << "  \"requested_steady_bytes\": [0, 32, 64, 96, 127],\n"
           << "  \"patch\": {\n"
           << "    \"key_mode\": \"Poly 1\",\n"
           << "    \"polyphony\": 6,\n"
           << "    \"notes\": [48, 55, 60],\n"
           << "    \"range\": \"8 foot\",\n"
           << "    \"saw_enabled\": true,\n"
           << "    \"pulse_enabled\": true,\n"
           << "    \"pwm_depth\": 0.36,\n"
           << "    \"sub_level\": 0.28,\n"
           << "    \"noise_level\": 0.0,\n"
           << "    \"high_pass\": \"1\",\n"
           << "    \"cutoff\": 0.82,\n"
           << "    \"resonance\": 0.08,\n"
           << "    \"envelope_depth\": 0.0,\n"
           << "    \"key_follow\": 0.5,\n"
           << "    \"vca_mode\": \"gate\",\n"
           << "    \"initial_vca_level_byte\": 64,\n"
           << "    \"chorus_noise\": 0.0,\n"
           << "    \"velocity_depth\": 0.0,\n"
           << "    \"volume\": 0.35,\n"
           << "    \"unit_character\": 1.0\n"
           << "  },\n"
           << "  \"sections\": " << sectionJson(rendered.sections) << ",\n"
           << "  \"vca_level_automation\": "
           << vcaAutomationJson(rendered.vcaAutomation) << ",\n"
           << "  \"midi_events\": " << eventJson(rendered.events) << ",\n";
    return output.str();
}

std::string bbdTransferManifestPrefix(std::string_view stage,
                                      const RenderedScenario& rendered)
{
    const auto modeOne = Chorus::settingsFor(ChorusMode::One);
    const auto modeTwo = Chorus::settingsFor(ChorusMode::Two);
    const double minimumDelay = modeOne.centreDelaySeconds - modeOne.sweepSeconds;
    const double maximumDelay = modeOne.centreDelaySeconds + modeOne.sweepSeconds;
    const double maximumClock = Chorus::clockForDelaySeconds(
        static_cast<float>(minimumDelay));
    const double minimumClock = Chorus::clockForDelaySeconds(
        static_cast<float>(maximumDelay));

    std::ostringstream output;
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"tool\": \"YouKnow106RenderRealismComparison\",\n"
           << "  \"scenario\": \"" << bbdTransferScenarioSlug << "\",\n"
           << "  \"scenario_protocol_version\": " << scenarioProtocolVersion << ",\n"
           << "  \"stage\": \"" << stage << "\",\n"
           << "  \"dsp_source_sha256\": \"" << YOUKNOW106_DSP_SOURCE_SHA256 << "\",\n"
           << "  \"compiler_id\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_ID) << "\",\n"
           << "  \"compiler_version\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_VERSION) << "\",\n"
           << "  \"sample_rate_hz\": " << comparisonSampleRate << ",\n"
           << "  \"engine_internal_rate_hz\": 192000,\n"
           << "  \"block_size_samples\": " << comparisonBlockSize << ",\n"
           << "  \"high_quality\": true,\n"
           << "  \"channels\": 2,\n"
           << "  \"frames\": " << rendered.audio.left.size() << ",\n"
           << "  \"wav_encoding\": \"IEEE-754 float32 little-endian\",\n"
           << "  \"event_schedule_seed_hex\": \"0x106bbd31\",\n"
           << "  \"seed_role\": \"xorshift32 fixes silent settle lengths and therefore exact free-running LFO/clock phases\",\n"
           << "  \"engine_seed_policy\": "
              "\"fixed per-card hash seeds rooted at 17; chorus noise disabled\",\n"
           << "  \"mechanism_under_test\": {\n"
           << "    \"name\": \"MN3009 aggregate charge-transfer loss versus clock\",\n"
           << "    \"baseline_nominal_transfer_step\": 0.8654743,\n"
           << "    \"baseline_clock_pivot_hz\": 26000.0,\n"
           << "    \"baseline_unsupported_clock_slope_per_hz\": 0.0000015,\n"
           << "    \"sounding_cycles_per_mode\": 3.5,\n"
           << "    \"mode_i_rate_hz\": " << jsonNumber(modeOne.rateHz) << ",\n"
           << "    \"mode_ii_rate_hz\": " << jsonNumber(modeTwo.rateHz) << ",\n"
           << "    \"minimum_delay_seconds\": " << jsonNumber(minimumDelay) << ",\n"
           << "    \"maximum_delay_seconds\": " << jsonNumber(maximumDelay) << ",\n"
           << "    \"minimum_clock_hz\": " << jsonNumber(minimumClock) << ",\n"
           << "    \"maximum_clock_hz\": " << jsonNumber(maximumClock) << "\n"
           << "  },\n"
           << "  \"patch\": {\n"
           << "    \"key_mode\": \"Poly 1\",\n"
           << "    \"polyphony\": 6,\n"
           << "    \"notes\": [60, 67],\n"
           << "    \"range\": \"4 foot\",\n"
           << "    \"saw_enabled\": true,\n"
           << "    \"pulse_enabled\": true,\n"
           << "    \"pwm_source\": \"manual\",\n"
           << "    \"pwm_depth\": 0.37,\n"
           << "    \"sub_level\": 0.0,\n"
           << "    \"noise_level\": 0.0,\n"
           << "    \"high_pass\": \"1\",\n"
           << "    \"cutoff\": 1.0,\n"
           << "    \"resonance\": 0.0,\n"
           << "    \"envelope_depth\": 0.0,\n"
           << "    \"vcf_lfo_depth\": 0.0,\n"
           << "    \"key_follow\": 0.5,\n"
           << "    \"vca_mode\": \"gate\",\n"
           << "    \"vca_level_byte\": 96,\n"
           << "    \"attack\": 0.0,\n"
           << "    \"decay\": 0.0,\n"
           << "    \"sustain\": 1.0,\n"
           << "    \"release\": 0.20,\n"
           << "    \"chorus_noise\": 0.0,\n"
           << "    \"chorus_clock_bleed\": false,\n"
           << "    \"chorus_hyperbolic_sweep\": true,\n"
           << "    \"velocity_depth\": 0.0,\n"
           << "    \"volume\": 0.28,\n"
           << "    \"unit_character\": 1.0\n"
           << "  },\n"
           << "  \"sections\": " << sectionJson(rendered.sections) << ",\n"
           << "  \"midi_events\": " << eventJson(rendered.events) << ",\n";
    return output.str();
}

std::string voiceVcaFeedthroughManifestPrefix(
    std::string_view stage, const RenderedScenario& rendered)
{
    std::ostringstream output;
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"tool\": \"YouKnow106RenderRealismComparison\",\n"
           << "  \"scenario\": \"" << voiceVcaFeedthroughScenarioSlug << "\",\n"
           << "  \"scenario_protocol_version\": " << scenarioProtocolVersion << ",\n"
           << "  \"stage\": \"" << stage << "\",\n"
           << "  \"dsp_source_sha256\": \"" << YOUKNOW106_DSP_SOURCE_SHA256 << "\",\n"
           << "  \"compiler_id\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_ID) << "\",\n"
           << "  \"compiler_version\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_VERSION) << "\",\n"
           << "  \"sample_rate_hz\": " << comparisonSampleRate << ",\n"
           << "  \"engine_internal_rate_hz\": 192000,\n"
           << "  \"block_size_samples\": " << comparisonBlockSize << ",\n"
           << "  \"high_quality\": true,\n"
           << "  \"channels\": 2,\n"
           << "  \"frames\": " << rendered.audio.left.size() << ",\n"
           << "  \"wav_encoding\": \"IEEE-754 float32 little-endian\",\n"
           << "  \"event_schedule_seed\": null,\n"
           << "  \"schedule_policy\": \"protocol-exact sample positions; no random timing\",\n"
           << "  \"engine_seed_policy\": "
              "\"fixed per-card hash seeds rooted at 17; all intentional audio and chorus noise disabled\",\n"
           << "  \"schedule\": {\n"
           << "    \"start_sample\": 0,\n"
           << "    \"note_on_sample\": " << voiceVcaNoteOnSample << ",\n"
           << "    \"note_off_sample\": " << voiceVcaNoteOffSample << ",\n"
           << "    \"end_sample_exclusive\": " << voiceVcaEndSample << ",\n"
           << "    \"pre_roll_nominal_control_scans\": 100,\n"
           << "    \"gate_open_nominal_control_scans\": 400,\n"
           << "    \"released_tail_nominal_control_scans\": 400,\n"
           << "    \"nominal_control_scan_seconds\": 0.0042\n"
           << "  },\n"
           << "  \"listening_protocol\": {\n"
           << "    \"fixed_gain_linear\": "
           << jsonNumber(voiceVcaFeedthroughListeningGain) << ",\n"
           << "    \"fixed_gain_db\": 30.0,\n"
           << "    \"adaptive_normalization\": false,\n"
           << "    \"diagnostic_magnification\": true,\n"
           << "    \"amplified_peak_ceiling\": "
           << jsonNumber(voiceVcaFeedthroughListeningCeiling) << "\n"
           << "  },\n"
           << "  \"patch\": {\n"
           << "    \"key_mode\": \"Unison\",\n"
           << "    \"polyphony\": 6,\n"
           << "    \"note\": 60,\n"
           << "    \"velocity\": 1.0,\n"
           << "    \"range\": \"8 foot\",\n"
           << "    \"saw_enabled\": false,\n"
           << "    \"pulse_enabled\": false,\n"
           << "    \"sub_level\": 0.0,\n"
           << "    \"noise_level\": 0.0,\n"
           << "    \"high_pass\": \"1\",\n"
           << "    \"cutoff\": 0.0,\n"
           << "    \"resonance\": 0.0,\n"
           << "    \"envelope_depth\": 0.0,\n"
           << "    \"vcf_lfo_depth\": 0.0,\n"
           << "    \"key_follow\": 0.0,\n"
           << "    \"vca_mode\": \"gate\",\n"
           << "    \"attack\": 0.0,\n"
           << "    \"decay\": 0.0,\n"
           << "    \"sustain\": 1.0,\n"
           << "    \"release\": 0.0,\n"
           << "    \"vca_level\": 1.0,\n"
           << "    \"volume\": 1.0,\n"
           << "    \"chorus\": \"off\",\n"
           << "    \"chorus_noise\": 0.0,\n"
           << "    \"velocity_depth\": 0.0,\n"
           << "    \"unit_character\": 1.0,\n"
           << "    \"enable_vcf_stage_offsets\": false,\n"
           << "    \"enable_op_amp_slew_limiting\": false,\n"
           << "    \"enable_vcf_early_effect\": false,\n"
           << "    \"enable_spatial_thermal_gradient\": false,\n"
           << "    \"enable_chorus_clock_bleed\": false,\n"
           << "    \"enable_chorus_hyperbolic_sweep\": false,\n"
           << "    \"enable_electrolytic_c14_nonlinearity\": false\n"
           << "  },\n"
           << "  \"sections\": " << sectionJson(rendered.sections) << ",\n"
           << "  \"midi_events\": " << eventJson(rendered.events) << ",\n"
           << "  \"analysis_windows\": " << voiceVcaWindowsJson() << ",\n"
           << "  \"analysis_filter\": "
              "\"diagnostic first-order 20 Hz high-pass followed by 200 Hz low-pass, continuously primed from sample zero\",\n"
           << "  \"limitations\": [\n"
           << "    \"The full-engine fixture retains deterministic microscopic filter excitation; raw silence is not expected.\",\n"
           << "    \"No calibrated hardware capture establishes the residual feedthrough magnitude for a trimmed unit.\",\n"
           << "    \"The +30 dB files are diagnostic magnifications, not the original listening level; raw files carry actual amplitude.\"\n"
           << "  ],\n";
    return output.str();
}

std::string bbdHostGridAliasManifestPrefix(
    std::string_view stage, const RenderedScenario& rendered)
{
    const auto modeOne = Chorus::settingsFor(ChorusMode::One);
    const auto modeTwo = Chorus::settingsFor(ChorusMode::Two);
    const double minimumDelay = modeOne.centreDelaySeconds
                              - modeOne.sweepSeconds;
    const double maximumDelay = modeOne.centreDelaySeconds
                              + modeOne.sweepSeconds;
    const double minimumClock = Chorus::clockForDelaySeconds(
        static_cast<float>(maximumDelay));
    const double maximumClock = Chorus::clockForDelaySeconds(
        static_cast<float>(minimumDelay));

    std::ostringstream output;
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"tool\": \"YouKnow106RenderRealismComparison\",\n"
           << "  \"scenario\": \"" << bbdHostGridAliasScenarioSlug << "\",\n"
           << "  \"scenario_protocol_version\": "
           << scenarioProtocolVersion << ",\n"
           << "  \"stage\": \"" << stage << "\",\n"
           << "  \"dsp_source_sha256\": \""
           << YOUKNOW106_DSP_SOURCE_SHA256 << "\",\n"
           << "  \"compiler_id\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_ID) << "\",\n"
           << "  \"compiler_version\": \""
           << jsonEscape(YOUKNOW106_COMPARISON_COMPILER_VERSION) << "\",\n"
           << "  \"sample_rate_hz\": " << comparisonSampleRate << ",\n"
           << "  \"internal_rate_variants_hz\": [192000, 48000],\n"
           << "  \"block_size_samples\": " << comparisonBlockSize << ",\n"
           << "  \"channels\": 2,\n"
           << "  \"frames\": " << rendered.audio.left.size() << ",\n"
           << "  \"wav_encoding\": \"IEEE-754 float32 little-endian\",\n"
           << "  \"separator_frames\": " << bbdAliasSeparatorFrames << ",\n"
           << "  \"schedule_policy\": \"literal protocol-v1 frame counts; no random timing\",\n"
           << "  \"seed_policy\": \"fixed BBD line seeds 0x9e3779b9/0x85ebca6b and fixed engine card/noise seeds; every chorus-noise component disabled\",\n"
           << "  \"gain_policy\": {\n"
           << "    \"raw_gain_linear\": 1.0,\n"
           << "    \"listening_gain_linear\": "
           << jsonNumber(bbdAliasListeningGain) << ",\n"
           << "    \"listening_gain_db\": "
           << jsonNumber(decibels(bbdAliasListeningGain)) << ",\n"
           << "    \"adaptive_normalization\": false,\n"
           << "    \"per_section_normalization\": false,\n"
           << "    \"peak_ceiling\": "
           << jsonNumber(bbdAliasListeningCeiling) << "\n"
           << "  },\n"
           << "  \"direct_probe\": {\n"
           << "    \"input_frequency_hz\": "
           << jsonNumber(bbdAliasProbeFrequencyHz) << ",\n"
           << "    \"input_amplitude_linear\": "
           << jsonNumber(bbdAliasProbeAmplitude) << ",\n"
           << "    \"warmup_frames_not_written\": "
           << bbdAliasWarmupFrames << ",\n"
           << "    \"fixed_probe_frames\": "
           << bbdAliasFixedProbeFrames << ",\n"
           << "    \"fixed_clock_hz\": " << jsonNumber(minimumClock) << ",\n"
           << "    \"path\": \"one complete modeled Line per channel: input coupling, five input poles, asynchronous 128-cell-pair BBD, transfer loss, held output, tap pole, four output poles and output coupling; noise zero\",\n"
           << "    \"hq_downsampling\": \"the engine's two shipping 63-tap half-band stages; no ad-hoc renderer resampler\",\n"
           << "    \"swept_output\": \"wet-only Chorus output after subtracting its mathematically identical dry IC6 leg\"\n"
           << "  },\n"
           << "  \"clock_program\": {\n"
           << "    \"mode_i_rate_hz\": " << jsonNumber(modeOne.rateHz) << ",\n"
           << "    \"mode_ii_rate_hz\": " << jsonNumber(modeTwo.rateHz) << ",\n"
           << "    \"minimum_delay_seconds\": "
           << jsonNumber(minimumDelay) << ",\n"
           << "    \"maximum_delay_seconds\": "
           << jsonNumber(maximumDelay) << ",\n"
           << "    \"minimum_clock_hz\": " << jsonNumber(minimumClock) << ",\n"
           << "    \"maximum_clock_hz\": " << jsonNumber(maximumClock) << ",\n"
           << "    \"sweep_law\": \"linear clock trajectory between endpoint frequencies, rendered through the shipping hyperbolic-delay option\"\n"
           << "  },\n"
           << "  \"spectral_target_contract\": {\n"
           << "    \"physical_image_formula_hz\": \"k*f_clock +/- f_input\",\n"
           << "    \"host_fold_formula_hz\": \"abs(f_source - round(f_source/48000)*48000)\",\n"
           << "    \"wanted_physical_target\": \"an unfurled physical image whose source frequency is <= 24 kHz; the fixed-clock k=1 minus image is the sentinel\",\n"
           << "    \"unwanted_numerical_target\": \"an in-band host fold whose unfurled k*f_clock +/- f_input source is > 24 kHz; suppress the fold, not the underlying physical BBD image\",\n"
           << "    \"scalar_measurement\": \"exact-frequency Hann projection over the complete 3 s fixed-clock section; no FFT-bin snapping, so the incommensurate clock does not turn leakage skirts into an alias estimate\",\n"
           << "    \"swept_section_policy\": \"retain complete-cycle raw audio for audition or future trajectory-aware analysis; no static swept-band metric is claimed because wanted and folded trajectories can cross\"\n"
           << "  },\n"
           << "  \"anchor_scope\": {\n"
           << "    \"existing_40khz_12khz_anchor\": \"the circuit test's raw BBD ZOH-aperture plus transfer-loss anchor remains scoped upstream of reconstruction and host-rate emission\",\n"
           << "    \"fixture_scope\": \"this scenario measures emitted wet response after support filters and, for HQ, production decimation; a future bandlimiting change may alter that emitted kernel without changing the raw transfer anchor\",\n"
           << "    \"after_review_guidance\": \"report fundamental and wanted in-band physical-image deltas separately; absolute changes <= 0.5 dB are preferred preservation targets, not hardware tolerances\"\n"
           << "  },\n"
           << "  \"musical_patch\": {\n"
           << "    \"notes\": [60, 67, 72],\n"
           << "    \"stabbing_pattern_frames\": { \"on\": 7200, \"off\": 4800 },\n"
           << "    \"range\": \"4 foot\",\n"
           << "    \"saw_enabled\": true,\n"
           << "    \"pulse_enabled\": true,\n"
           << "    \"pwm_depth\": 0.31,\n"
           << "    \"sub_level\": 0.0,\n"
           << "    \"noise_level\": 0.0,\n"
           << "    \"cutoff\": 1.0,\n"
           << "    \"resonance\": 0.0,\n"
           << "    \"vca_mode\": \"gate\",\n"
           << "    \"vca_level_byte\": 96,\n"
           << "    \"volume\": 0.14,\n"
           << "    \"chorus_noise\": 0.0,\n"
           << "    \"chorus_clock_bleed\": false,\n"
           << "    \"chorus_hyperbolic_sweep\": true,\n"
           << "    \"unit_character\": 1.0\n"
           << "  },\n"
           << "  \"sections\": "
           << bbdAliasSectionsJson(rendered.bbdAliasSections) << ",\n"
           << "  \"midi_events\": " << eventJson(rendered.events) << ",\n"
           << "  \"limitations\": [\n"
           << "    \"The fixed-clock exact-frequency projections diagnose the model's numerical host-grid behavior; they are not calibrated measurements of a hardware unit.\",\n"
           << "    \"The LFO rates are derived from the Juno-106 timing network, while the clock endpoints retain a calibrated sibling measurement of the shared driver; spectral trajectories are fixture coordinates rather than new installed-unit evidence.\",\n"
           << "    \"A 48 kHz output cannot contain an unfurled image above 24 kHz; the manifest preserves its physical source frequency and separately labels its in-band numerical fold.\",\n"
           << "    \"Swept images and modulation sidebands cross, so only the stationary fixed-clock sections receive scalar target amplitudes; complete-cycle sections remain raw audition/analysis evidence.\"\n"
           << "  ],\n";
    return output.str();
}

std::string readmeProse()
{
    return
        "# Retrigger/release-tail realism comparison\n\n"
        "This focused fixture reassigns one still-releasing voice to the same note at\n"
        "several deterministic control-scan phases. Keeping the pitch unchanged isolates\n"
        "assignment/VCA behaviour from oscillator restart and portamento effects.\n\n"
        "The raw float32 baseline is archival evidence and must be rendered before the DSP\n"
        "change. Listening files use one shared gain; no side is independently normalized.\n"
        "Only the text between the generated markers below is renderer-owned.\n\n"
        "From the project directory, render the two stages around exactly one rebuilt DSP\n"
        "change:\n\n"
        "```bash\n"
        "cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison\n"
        "./build-dsp/YouKnow106RenderRealismComparison retrigger-release-tail before \\\n"
        "  Docs/audio/realism-comparisons/retrigger-release-tail\n"
        "# Apply and rebuild the DSP change, then:\n"
        "./build-dsp/YouKnow106RenderRealismComparison retrigger-release-tail after \\\n"
        "  Docs/audio/realism-comparisons/retrigger-release-tail\n"
        "```";
}

std::string commonVcaReadmeProse()
{
    return
        "# Common VCA LEVEL realism comparison\n\n"
        "This fixture holds a three-note tone through stored VCA LEVEL bytes 0, 32,\n"
        "64, 96 and 127, followed by rapid transitions that expose the shared hold's\n"
        "scan latency and settling. It repeats the sequence dry and through Chorus II\n"
        "because the common VCA precedes the chorus and therefore changes its drive.\n\n"
        "The raw float32 baseline is archival evidence and must be rendered before the DSP\n"
        "change. Listening files use one shared gain; no side is independently normalized.\n"
        "Only the text between the generated markers below is renderer-owned.\n\n"
        "From the project directory, render the two stages around exactly one rebuilt DSP\n"
        "change:\n\n"
        "```bash\n"
        "cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison\n"
        "./build-dsp/YouKnow106RenderRealismComparison common-vca-level before \\\n"
        "  Docs/audio/realism-comparisons/common-vca-level\n"
        "# Apply and rebuild the DSP change, then:\n"
        "./build-dsp/YouKnow106RenderRealismComparison common-vca-level after \\\n"
        "  Docs/audio/realism-comparisons/common-vca-level\n"
        "```";
}

std::string bbdTransferReadmeProse()
{
    return
        "# BBD transfer/clock-law realism comparison\n\n"
        "This full-engine fixture holds the same bright 4-foot saw/pulse dyad dry,\n"
        "through Chorus I, and through Chorus II. Each wet section begins after its\n"
        "support network settles and spans 3.5 complete modulation cycles, repeatedly\n"
        "visiting both BBD clock extremes. Chorus noise and speculative clock bleed are\n"
        "off, so the comparison concentrates on deterministic wet-path transfer loss.\n\n"
        "The raw float32 baseline is archival evidence and must be rendered before the DSP\n"
        "change. Listening files use one shared gain; no side is independently normalized.\n"
        "Only the text between the generated markers below is renderer-owned.\n\n"
        "```bash\n"
        "cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison\n"
        "./build-dsp/YouKnow106RenderRealismComparison bbd-transfer-clock-law before \\\n"
        "  Docs/audio/realism-comparisons/bbd-transfer-clock-law\n"
        "# Apply and rebuild the single Chorus change, then:\n"
        "./build-dsp/YouKnow106RenderRealismComparison bbd-transfer-clock-law after \\\n"
        "  Docs/audio/realism-comparisons/bbd-transfer-clock-law\n"
        "```";
}

std::string voiceVcaFeedthroughReadmeProse()
{
    return
        "# Voice-VCA feedthrough realism comparison\n\n"
        "This fixture opens all six physical voices in Unison while every intentional\n"
        "oscillator, sub, and noise source is off. Gate-open and gate-close events occur\n"
        "at the same nominal converter-scan phase, isolating any signal invented by the\n"
        "voice-VCA path from musical masking.\n\n"
        "The `-listen` files apply a protocol-fixed **+30 dB diagnostic magnification**.\n"
        "They do not represent the defect at its original loudness and are never\n"
        "adaptively normalized. The raw float32 files carry the actual rendered level.\n"
        "Before, after, and signed difference always receive the identical fixed gain.\n\n"
        "The engine intentionally retains deterministic microscopic filter excitation,\n"
        "so the full render is not expected to become exact digital silence. No calibrated\n"
        "hardware capture yet establishes the residual feedthrough of a trimmed unit.\n\n"
        "Only the text between the generated markers below is renderer-owned.\n\n"
        "```bash\n"
        "cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison\n"
        "./build-dsp/YouKnow106RenderRealismComparison voice-vca-feedthrough before \\\n"
        "  Docs/audio/realism-comparisons/voice-vca-feedthrough\n"
        "# Apply and rebuild the single DSP change, then:\n"
        "./build-dsp/YouKnow106RenderRealismComparison voice-vca-feedthrough after \\\n"
        "  Docs/audio/realism-comparisons/voice-vca-feedthrough\n"
        "```";
}

std::string bbdHostGridAliasReadmeProse()
{
    return
        "# BBD host-grid alias realism comparison\n\n"
        "This deterministic fixture separates an implementation artifact from the\n"
        "physical images of an asynchronously clocked bucket-brigade line. A clean\n"
        "2.093 kHz sine first drives one complete modeled BBD/support chain at the\n"
        "minimum sweep clock. That stationary probe places the wanted in-band\n"
        "`fClock - fInput` image near, but not on, the host-folded `fClock + fInput`\n"
        "image. The next four sections run the same wet-only tone through complete\n"
        "Chorus I and II LFO cycles at HQ (192 kHz internal) and LQ (48 kHz\n"
        "internal). Four final sections repeat those complete cycles with bright\n"
        "full-engine chord stabs. Sections are separated by 100 ms of digital zero.\n\n"
        "A physical image is always identified at its unfurled source frequency\n"
        "`k*fClock +/- fInput`. If that source is above 24 kHz, its in-band 48 kHz\n"
        "fold is labeled an unwanted numerical alias; suppressing that fold must not\n"
        "erase the physical image when it genuinely lies below host Nyquist. The\n"
        "manifest records both coordinates and the exact-frequency Hann measurement\n"
        "contract.\n\n"
        "Raw and listening WAVs are stereo 48 kHz float32. Protocol v1 uses one fixed\n"
        "0 dB gain for the whole concatenation and for both comparison stages: there\n"
        "is no section, quality, mode, or stage normalization. Chorus noise and clock\n"
        "bleed are disabled. The HQ direct probe uses the engine's shipping two-stage\n"
        "half-band decimator, not a renderer-only resampler. The after pass refuses\n"
        "a baseline unless its manifest binds the scenario, protocol, frame count,\n"
        "raw sample hash and a distinct pre-change DSP source fingerprint.\n\n"
        "Only the text between the generated markers below is renderer-owned.\n\n"
        "```bash\n"
        "cmake --build build-dsp --parallel --target YouKnow106RenderRealismComparison\n"
        "./build-dsp/YouKnow106RenderRealismComparison bbd-host-grid-alias before \\\n"
        "  Docs/audio/realism-comparisons/bbd-host-grid-alias\n"
        "# Apply and rebuild exactly one future BBD host-grid change, then:\n"
        "./build-dsp/YouKnow106RenderRealismComparison bbd-host-grid-alias after \\\n"
        "  Docs/audio/realism-comparisons/bbd-host-grid-alias\n"
        "```";
}

std::string beforeGenerated(const Level& level, double listeningGain)
{
    std::ostringstream output;
    output << "Generated baseline for scenario protocol " << scenarioProtocolVersion
           << ". The `after` pass has not supplied a comparison yet.\n\n"
           << "| Metric | Value |\n"
           << "| --- | ---: |\n"
           << "| Raw peak | " << std::fixed << std::setprecision(6) << level.peak
           << " (" << std::setprecision(2) << decibels(level.peak) << " dBFS) |\n"
           << "| Raw RMS | " << std::setprecision(6) << level.rms
           << " (" << std::setprecision(2) << decibels(level.rms) << " dBFS) |\n"
           << "| Baseline listening gain | " << std::setprecision(9) << listeningGain
           << " (" << std::setprecision(3) << decibels(listeningGain) << " dB) |\n\n"
           << "See `retrigger-release-tail-before-manifest.json` for the exact event\n"
           << "schedule, build fingerprint, render settings, and raw values.";
    return output.str();
}

std::string commonVcaBeforeGenerated(const Level& level, double listeningGain)
{
    std::ostringstream output;
    output << "Generated baseline for scenario protocol " << scenarioProtocolVersion
           << ". The `after` pass has not supplied a comparison yet.\n\n"
           << "| Metric | Value |\n"
           << "| --- | ---: |\n"
           << "| Raw peak | " << std::fixed << std::setprecision(6) << level.peak
           << " (" << std::setprecision(2) << decibels(level.peak) << " dBFS) |\n"
           << "| Raw RMS | " << std::setprecision(6) << level.rms
           << " (" << std::setprecision(2) << decibels(level.rms) << " dBFS) |\n"
           << "| Baseline listening gain | " << std::setprecision(9) << listeningGain
           << " (" << std::setprecision(3) << decibels(listeningGain) << " dB) |\n\n"
           << "See `common-vca-level-before-manifest.json` for exact byte/sample\n"
           << "automation, section boundaries, build fingerprint, and raw values.";
    return output.str();
}

std::string bbdTransferBeforeGenerated(const Level& level,
                                       double listeningGain)
{
    std::ostringstream output;
    output << "Generated baseline for scenario protocol " << scenarioProtocolVersion
           << ". The `after` pass has not supplied a comparison yet.\n\n"
           << "| Metric | Value |\n"
           << "| --- | ---: |\n"
           << "| Raw peak | " << std::fixed << std::setprecision(6) << level.peak
           << " (" << std::setprecision(2) << decibels(level.peak) << " dBFS) |\n"
           << "| Raw RMS | " << std::setprecision(6) << level.rms
           << " (" << std::setprecision(2) << decibels(level.rms) << " dBFS) |\n"
           << "| Baseline listening gain | " << std::setprecision(9) << listeningGain
           << " (" << std::setprecision(3) << decibels(listeningGain) << " dB) |\n\n"
           << "See `bbd-transfer-clock-law-before-manifest.json` for exact section and\n"
           << "MIDI sample positions, clock endpoints, seed, patch, and build fingerprint.";
    return output.str();
}

std::string voiceVcaFeedthroughBeforeGenerated(const StereoBuffer& audio,
                                               const Level& level)
{
    std::ostringstream output;
    output << "Generated baseline for scenario protocol " << scenarioProtocolVersion
           << ". The `after` pass has not supplied a comparison yet.\n\n"
           << "| Whole-file metric | Raw value |\n"
           << "| --- | ---: |\n"
           << "| Peak | " << std::scientific << std::setprecision(9) << level.peak
           << " (" << std::fixed << std::setprecision(2)
           << decibels(level.peak) << " dBFS) |\n"
           << "| RMS | " << std::scientific << std::setprecision(9) << level.rms
           << " (" << std::fixed << std::setprecision(2)
           << decibels(level.rms) << " dBFS) |\n"
           << "| Listening gain | 31.622776602 (+30.000 dB, fixed) |\n\n"
           << "| Analysis window | Raw absolute peak | Raw RMS | 20--200 Hz RMS |\n"
           << "| --- | ---: | ---: | ---: |\n";
    for (const auto& window : voiceVcaAnalysisWindows)
    {
        const auto metrics = measureWindow(audio, window);
        output << "| " << window.name << " | " << std::scientific
               << std::setprecision(6) << metrics.absolutePeak << " | "
               << metrics.rms << " | " << metrics.lowBandRms20To200Hz << " |\n";
    }
    output << "\nSee `voice-vca-feedthrough-before-manifest.json` for exact sample\n"
           << "boundaries, signed peaks, means, peak offsets, stereo equality, patch,\n"
           << "limitations, build fingerprint, and unrounded values.";
    return output.str();
}

std::string bbdHostGridAliasBeforeGenerated(
    const RenderedScenario& rendered, const Level& level,
    const StereoBuffer& listening)
{
    const auto settings = Chorus::settingsFor(ChorusMode::One);
    const double clock = Chorus::clockForDelaySeconds(
        settings.centreDelaySeconds + settings.sweepSeconds);
    const double input = bbdAliasProbeFrequencyHz;
    struct DisplayTarget
    {
        const char* name;
        double frequency;
    };
    const std::array<DisplayTarget, 4> targets {{
        { "wanted physical k1-minus",
          clock - input },
        { "folded k1-plus",
          foldedToHostNyquist(clock + input) },
        { "folded k2-minus",
          foldedToHostNyquist(2.0 * clock - input) },
        { "folded k2-plus",
          foldedToHostNyquist(2.0 * clock + input) }
    }};

    std::ostringstream output;
    output << "Generated immutable baseline for scenario protocol "
           << scenarioProtocolVersion
           << ". No `after` render is present.\n\n"
           << "| Whole-file metric | Value |\n"
           << "| --- | ---: |\n"
           << std::scientific << std::setprecision(9)
           << "| Raw peak | " << level.peak << " (" << std::fixed
           << std::setprecision(2) << decibels(level.peak) << " dBFS) |\n"
           << std::scientific << std::setprecision(9)
           << "| Raw RMS | " << level.rms << " (" << std::fixed
           << std::setprecision(2) << decibels(level.rms) << " dBFS) |\n"
           << "| Listening gain | +0.000 dB fixed; no normalization |\n"
           << "| Raw sample hash | `fnv1a64:"
           << hex64(interleavedFloatHash(rendered.audio)) << "` |\n"
           << "| Listening sample hash | `fnv1a64:"
           << hex64(interleavedFloatHash(listening)) << "` |\n\n"
           << "Stationary 2.093 kHz / minimum-clock probe, Hann amplitude relative\n"
           << "to the input fundamental:\n\n"
           << "| Mask centre | HQ / 192 kHz | LQ / 48 kHz |\n"
           << "| --- | ---: | ---: |\n";

    const auto* hq = findBbdAliasSection(
        rendered, "fixed-min-clock-tone-hq");
    const auto* lq = findBbdAliasSection(
        rendered, "fixed-min-clock-tone-lq");
    const double hqFundamental = hq != nullptr
        ? hannToneAmplitude(rendered.audio, *hq, input) : 0.0;
    const double lqFundamental = lq != nullptr
        ? hannToneAmplitude(rendered.audio, *lq, input) : 0.0;
    for (const auto& target : targets)
    {
        const double hqAmplitude = hq != nullptr
            ? hannToneAmplitude(rendered.audio, *hq, target.frequency) : 0.0;
        const double lqAmplitude = lq != nullptr
            ? hannToneAmplitude(rendered.audio, *lq, target.frequency) : 0.0;
        output << "| " << target.name << " (" << std::fixed
               << std::setprecision(2) << target.frequency << " Hz) | "
               << std::setprecision(2)
               << decibels(hqAmplitude / std::max(hqFundamental, 1.0e-18))
               << " dBc | "
               << decibels(lqAmplitude / std::max(lqFundamental, 1.0e-18))
               << " dBc |\n";
    }

    output << "\n| Section | Kind | Quality | Chorus | Duration | LFO cycles |\n"
           << "| --- | --- | --- | --- | ---: | ---: |\n";
    for (const auto& section : rendered.bbdAliasSections)
    {
        output << "| " << section.name << " | " << section.kind << " | "
               << section.quality << " | " << section.chorus << " | "
               << std::fixed << std::setprecision(3)
               << static_cast<double>(section.endSample - section.startSample)
                    / comparisonSampleRate
               << " s | " << std::setprecision(6)
               << section.realisedLfoCycles << " |\n";
    }
    output << "\nSee `bbd-host-grid-alias-before-manifest.json` for source and\n"
           << "observed image frequencies, exact targets, sample boundaries, MIDI\n"
           << "events, patch, fixed gains, hashes, build fingerprint, and limitations.";
    return output.str();
}

std::string afterGenerated(const Level& before, const Level& after,
                           const Level& diff, double listeningGain,
                           double peakDbc, double rmsDbc)
{
    std::ostringstream output;
    output << "Generated comparison for scenario protocol " << scenarioProtocolVersion
           << ". `difference` is signed `after - before`.\n\n"
           << "| Signal | Raw peak | Raw RMS |\n"
           << "| --- | ---: | ---: |\n"
           << std::fixed << std::setprecision(6)
           << "| Before | " << before.peak << " (" << std::setprecision(2)
           << decibels(before.peak) << " dBFS) | " << std::setprecision(6)
           << before.rms << " (" << std::setprecision(2)
           << decibels(before.rms) << " dBFS) |\n"
           << std::setprecision(6) << "| After | " << after.peak << " ("
           << std::setprecision(2) << decibels(after.peak) << " dBFS) | "
           << std::setprecision(6) << after.rms << " (" << std::setprecision(2)
           << decibels(after.rms) << " dBFS) |\n"
           << std::setprecision(6) << "| Signed difference | " << diff.peak << " ("
           << std::setprecision(2) << decibels(diff.peak) << " dBFS) | "
           << std::setprecision(6) << diff.rms << " (" << std::setprecision(2)
           << decibels(diff.rms) << " dBFS) |\n\n"
           << "| Correct relative metric | Value |\n"
           << "| --- | ---: |\n"
           << "| Difference peak / before peak | " << std::setprecision(2)
           << peakDbc << " dBc |\n"
           << "| Difference RMS / before RMS | " << rmsDbc << " dBc |\n"
           << "| Shared listening gain | " << std::setprecision(9) << listeningGain
           << " (" << std::setprecision(3) << decibels(listeningGain) << " dB) |\n\n"
           << "See `retrigger-release-tail-comparison-manifest.json` for machine-readable\n"
           << "metadata and exact values.";
    return output.str();
}

std::string commonVcaAfterGenerated(const Level& before, const Level& after,
                                    const Level& diff, double listeningGain,
                                    double peakDbc, double rmsDbc)
{
    std::ostringstream output;
    output << "Generated comparison for scenario protocol " << scenarioProtocolVersion
           << ". `difference` is signed `after - before`.\n\n"
           << "| Signal | Raw peak | Raw RMS |\n"
           << "| --- | ---: | ---: |\n"
           << std::fixed << std::setprecision(6)
           << "| Before | " << before.peak << " (" << std::setprecision(2)
           << decibels(before.peak) << " dBFS) | " << std::setprecision(6)
           << before.rms << " (" << std::setprecision(2)
           << decibels(before.rms) << " dBFS) |\n"
           << std::setprecision(6) << "| After | " << after.peak << " ("
           << std::setprecision(2) << decibels(after.peak) << " dBFS) | "
           << std::setprecision(6) << after.rms << " (" << std::setprecision(2)
           << decibels(after.rms) << " dBFS) |\n"
           << std::setprecision(6) << "| Signed difference | " << diff.peak << " ("
           << std::setprecision(2) << decibels(diff.peak) << " dBFS) | "
           << std::setprecision(6) << diff.rms << " (" << std::setprecision(2)
           << decibels(diff.rms) << " dBFS) |\n\n"
           << "| Correct relative metric | Value |\n"
           << "| --- | ---: |\n"
           << "| Difference peak / before peak | " << std::setprecision(2)
           << peakDbc << " dBc |\n"
           << "| Difference RMS / before RMS | " << rmsDbc << " dBc |\n"
           << "| Shared listening gain | " << std::setprecision(9) << listeningGain
           << " (" << std::setprecision(3) << decibels(listeningGain) << " dB) |\n\n"
           << "See `common-vca-level-comparison-manifest.json` for machine-readable\n"
           << "metadata and exact values.";
    return output.str();
}

std::string bbdTransferAfterGenerated(const Level& before, const Level& after,
                                      const Level& diff, double listeningGain,
                                      double peakDbc, double rmsDbc)
{
    std::ostringstream output;
    output << "Generated comparison for scenario protocol " << scenarioProtocolVersion
           << ". `difference` is signed `after - before`.\n\n"
           << "| Signal | Raw peak | Raw RMS |\n"
           << "| --- | ---: | ---: |\n"
           << std::fixed << std::setprecision(6)
           << "| Before | " << before.peak << " (" << std::setprecision(2)
           << decibels(before.peak) << " dBFS) | " << std::setprecision(6)
           << before.rms << " (" << std::setprecision(2)
           << decibels(before.rms) << " dBFS) |\n"
           << std::setprecision(6) << "| After | " << after.peak << " ("
           << std::setprecision(2) << decibels(after.peak) << " dBFS) | "
           << std::setprecision(6) << after.rms << " (" << std::setprecision(2)
           << decibels(after.rms) << " dBFS) |\n"
           << std::setprecision(6) << "| Signed difference | " << diff.peak << " ("
           << std::setprecision(2) << decibels(diff.peak) << " dBFS) | "
           << std::setprecision(6) << diff.rms << " (" << std::setprecision(2)
           << decibels(diff.rms) << " dBFS) |\n\n"
           << "| Correct relative metric | Value |\n"
           << "| --- | ---: |\n"
           << "| Difference peak / before peak | " << std::setprecision(2)
           << peakDbc << " dBc |\n"
           << "| Difference RMS / before RMS | " << rmsDbc << " dBc |\n"
           << "| Shared listening gain | " << std::setprecision(9) << listeningGain
           << " (" << std::setprecision(3) << decibels(listeningGain) << " dB) |\n\n"
           << "See `bbd-transfer-clock-law-comparison-manifest.json` for machine-readable\n"
           << "metadata and exact values.";
    return output.str();
}

std::string voiceVcaFeedthroughAfterGenerated(
    const StereoBuffer& beforeAudio, const StereoBuffer& afterAudio,
    const StereoBuffer& diffAudio, const Level& before, const Level& after,
    const Level& diff, double peakDbc, double rmsDbc)
{
    std::ostringstream output;
    output << "Generated comparison for scenario protocol " << scenarioProtocolVersion
           << ". `difference` is signed `after - before`.\n\n"
           << "| Signal | Raw peak | Raw RMS |\n"
           << "| --- | ---: | ---: |\n"
           << std::scientific << std::setprecision(9)
           << "| Before | " << before.peak << " | " << before.rms << " |\n"
           << "| After | " << after.peak << " | " << after.rms << " |\n"
           << "| Signed difference | " << diff.peak << " | " << diff.rms << " |\n\n"
           << "| Relative metric | Value |\n"
           << "| --- | ---: |\n"
           << std::fixed << std::setprecision(2)
           << "| Difference peak / before peak | " << peakDbc << " dBc |\n"
           << "| Difference RMS / before RMS | " << rmsDbc << " dBc |\n"
           << "| Listening gain | +30.000 dB fixed diagnostic magnification |\n\n"
           << "| Window | Before RMS | After RMS | Difference RMS |\n"
           << "| --- | ---: | ---: | ---: |\n";
    for (const auto& window : voiceVcaAnalysisWindows)
    {
        const auto beforeWindow = measureWindow(beforeAudio, window);
        const auto afterWindow = measureWindow(afterAudio, window);
        const auto diffWindow = measureWindow(diffAudio, window);
        output << "| " << window.name << " | " << std::scientific
               << std::setprecision(6) << beforeWindow.rms << " | "
               << afterWindow.rms << " | " << diffWindow.rms << " |\n";
    }
    output << "\nSee `voice-vca-feedthrough-comparison-manifest.json` for complete\n"
           << "machine-readable window metrics and unrounded values.";
    return output.str();
}

std::string bbdHostGridAliasAfterGenerated(
    const RenderedScenario& beforeRendered,
    const RenderedScenario& afterRendered,
    const StereoBuffer& diffAudio,
    const Level& before, const Level& after, const Level& diff,
    double peakDbc, double rmsDbc)
{
    std::ostringstream output;
    output << "Generated comparison for scenario protocol "
           << scenarioProtocolVersion
           << ". `difference` is signed `after - before`.\n\n"
           << "| Signal | Raw peak | Raw RMS |\n"
           << "| --- | ---: | ---: |\n"
           << std::scientific << std::setprecision(9)
           << "| Before | " << before.peak << " | " << before.rms << " |\n"
           << "| After | " << after.peak << " | " << after.rms << " |\n"
           << "| Signed difference | " << diff.peak << " | " << diff.rms
           << " |\n\n"
           << "| Relative metric | Value |\n"
           << "| --- | ---: |\n"
           << std::fixed << std::setprecision(2)
           << "| Difference peak / before peak | " << peakDbc << " dBc |\n"
           << "| Difference RMS / before RMS | " << rmsDbc << " dBc |\n"
           << "| Listening gain | +0.000 dB fixed; no normalization |\n\n";

    constexpr std::array<std::string_view, 2> sectionNames {
        "fixed-min-clock-tone-hq", "fixed-min-clock-tone-lq"
    };
    constexpr std::array<std::string_view, 2> qualityLabels {
        "HQ / 192 kHz", "LQ / 48 kHz"
    };
    output << "Stationary 2.093 kHz / minimum-clock Hann amplitudes, each\n"
           << "relative to that render's own input fundamental:\n\n"
           << "| Target | Quality | Observed Hz | Before | After | Change |\n"
           << "| --- | --- | ---: | ---: | ---: | ---: |\n";
    for (std::size_t quality = 0; quality < sectionNames.size(); ++quality)
    {
        const auto* beforeSection = findBbdAliasSection(
            beforeRendered, sectionNames[quality]);
        const auto* afterSection = findBbdAliasSection(
            afterRendered, sectionNames[quality]);
        if (beforeSection == nullptr || afterSection == nullptr)
            continue;
        const double beforeFundamental = hannToneAmplitude(
            beforeRendered.audio, *beforeSection, bbdAliasProbeFrequencyHz);
        const double afterFundamental = hannToneAmplitude(
            afterRendered.audio, *afterSection, bbdAliasProbeFrequencyHz);
        for (const auto& target : bbdAliasComparisonTargets())
        {
            const double beforeDbc = decibels(hannToneAmplitude(
                beforeRendered.audio, *beforeSection,
                target.observedFrequencyHz)
                / std::max(beforeFundamental, 1.0e-18));
            const double afterDbc = decibels(hannToneAmplitude(
                afterRendered.audio, *afterSection,
                target.observedFrequencyHz)
                / std::max(afterFundamental, 1.0e-18));
            output << "| " << target.displayName << " | "
                   << qualityLabels[quality] << " | " << std::fixed
                   << std::setprecision(2) << target.observedFrequencyHz
                   << " | " << beforeDbc << " dBc | " << afterDbc
                   << " dBc | " << std::showpos << (afterDbc - beforeDbc)
                   << " dB |\n" << std::noshowpos;
        }
    }

    output << "\nPer-section raw levels; relative columns use the matching before\n"
           << "section as their denominator:\n\n"
           << "| Section | Before RMS | After RMS | Difference RMS | Diff peak | Diff RMS |\n"
           << "| --- | ---: | ---: | ---: | ---: | ---: |\n";
    for (const auto& section : afterRendered.bbdAliasSections)
    {
        const auto* beforeSection = findBbdAliasSection(
            beforeRendered, section.name);
        if (beforeSection == nullptr)
            continue;
        const auto beforeSectionLevel = measureBbdAliasSection(
            beforeRendered.audio, *beforeSection);
        const auto afterSectionLevel = measureBbdAliasSection(
            afterRendered.audio, section);
        const auto diffSectionLevel = measureBbdAliasSection(
            diffAudio, section);
        output << "| " << section.name << " | " << std::fixed
               << std::setprecision(2)
               << decibels(beforeSectionLevel.rms) << " dBFS | "
               << decibels(afterSectionLevel.rms) << " dBFS | "
               << decibels(diffSectionLevel.rms) << " dBFS | "
               << decibels(diffSectionLevel.peak / std::max(
                      beforeSectionLevel.peak, 1.0e-18)) << " dBc | "
               << decibels(diffSectionLevel.rms / std::max(
                      beforeSectionLevel.rms, 1.0e-18)) << " dBc |\n";
    }

    output << "\n"
           << "See `bbd-host-grid-alias-comparison-manifest.json` for complete\n"
           << "machine-readable targets, per-section metrics, hashes and source "
              "fingerprints.";
    return output.str();
}

bool updateReadme(const std::filesystem::path& outputDirectory,
                  const std::string& generated, std::string& error)
{
    return updateGeneratedSection(outputDirectory / "README.md", readmeProse(),
                                  std::string(generatedBegin),
                                  std::string(generatedEnd), generated, error);
}

bool updateCommonVcaReadme(const std::filesystem::path& outputDirectory,
                           const std::string& generated, std::string& error)
{
    return updateGeneratedSection(outputDirectory / "README.md",
                                  commonVcaReadmeProse(),
                                  std::string(generatedBegin),
                                  std::string(generatedEnd), generated, error);
}

bool updateBbdTransferReadme(const std::filesystem::path& outputDirectory,
                             const std::string& generated, std::string& error)
{
    return updateGeneratedSection(outputDirectory / "README.md",
                                  bbdTransferReadmeProse(),
                                  std::string(generatedBegin),
                                  std::string(generatedEnd), generated, error);
}

bool updateVoiceVcaFeedthroughReadme(
    const std::filesystem::path& outputDirectory,
    const std::string& generated, std::string& error)
{
    return updateGeneratedSection(outputDirectory / "README.md",
                                  voiceVcaFeedthroughReadmeProse(),
                                  std::string(generatedBegin),
                                  std::string(generatedEnd), generated, error);
}

bool updateBbdHostGridAliasReadme(
    const std::filesystem::path& outputDirectory,
    const std::string& generated, std::string& error)
{
    return updateGeneratedSection(outputDirectory / "README.md",
                                  bbdHostGridAliasReadmeProse(),
                                  std::string(generatedBegin),
                                  std::string(generatedEnd), generated, error);
}

bool writeBefore(const std::filesystem::path& outputDirectory,
                 const RenderedScenario& rendered, std::string& error)
{
    const auto rawPath = outputDirectory
                       / "retrigger-release-tail-before-raw-f32.wav";
    if (std::filesystem::exists(rawPath))
    {
        StereoBuffer archived;
        if (!readFloatWav(rawPath, archived, error))
            return false;
        if (!youknow106::tools::realism::buffersEqual(archived, rendered.audio))
        {
            error = "refusing to overwrite a non-identical raw before archive: "
                  + rawPath.string();
            return false;
        }
    }
    else if (!writeFloatWav(rawPath, rendered.audio, error))
    {
        return false;
    }

    const Level rawLevel = measure(rendered.audio);
    if (rawLevel.peak <= 1.0e-12 || rawLevel.rms <= 1.0e-12)
    {
        error = "scenario rendered silence";
        return false;
    }
    const double listeningGain = listeningTargetPeak / rawLevel.peak;
    const StereoBuffer listening = applyGain(rendered.audio, listeningGain);
    if (!writeFloatWav(outputDirectory
                       / "retrigger-release-tail-before-listen-f32.wav",
                       listening, error))
        return false;

    std::ostringstream manifest;
    manifest << commonManifestPrefix("before", rendered)
             << "  \"raw_level\": " << levelJson(rawLevel, 2) << ",\n"
             << "  \"listening\": {\n"
             << "    \"target_peak_dbfs\": -6.0,\n"
             << "    \"gain_linear\": " << jsonNumber(listeningGain) << ",\n"
             << "    \"gain_db\": " << jsonNumber(decibels(listeningGain)) << ",\n"
             << "    \"policy\": \"baseline-derived provisional gain; after pass applies one shared pair gain\"\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory
                   / "retrigger-release-tail-before-manifest.json",
                   manifest.str(), error))
        return false;
    return updateReadme(outputDirectory,
                        beforeGenerated(rawLevel, listeningGain), error);
}

bool writeAfter(const std::filesystem::path& outputDirectory,
                const RenderedScenario& rendered, std::string& error)
{
    const auto beforeRawPath = outputDirectory
                             / "retrigger-release-tail-before-raw-f32.wav";
    StereoBuffer before;
    if (!readFloatWav(beforeRawPath, before, error))
        return false;
    if (before.left.size() != rendered.audio.left.size())
    {
        error = "baseline frame count does not match scenario protocol v1";
        return false;
    }

    StereoBuffer diff;
    if (!difference(before, rendered.audio, diff, error))
        return false;

    const Level beforeLevel = measure(before);
    const Level afterLevel = measure(rendered.audio);
    const Level diffLevel = measure(diff);
    if (beforeLevel.peak <= 1.0e-12 || beforeLevel.rms <= 1.0e-12)
    {
        error = "raw before archive is silent and cannot be a dBc reference";
        return false;
    }

    const double peakDbc = decibels(diffLevel.peak / beforeLevel.peak);
    const double rmsDbc = decibels(diffLevel.rms / beforeLevel.rms);
    const double pairPeak = std::max(beforeLevel.peak, afterLevel.peak);
    const double listeningGain = listeningTargetPeak / pairPeak;

    if (!writeFloatWav(outputDirectory
                       / "retrigger-release-tail-after-raw-f32.wav",
                       rendered.audio, error)
        || !writeFloatWav(outputDirectory
                          / "retrigger-release-tail-difference-raw-f32.wav",
                          diff, error)
        || !writeFloatWav(outputDirectory
                          / "retrigger-release-tail-before-listen-f32.wav",
                          applyGain(before, listeningGain), error)
        || !writeFloatWav(outputDirectory
                          / "retrigger-release-tail-after-listen-f32.wav",
                          applyGain(rendered.audio, listeningGain), error)
        || !writeFloatWav(outputDirectory
                          / "retrigger-release-tail-difference-listen-f32.wav",
                          applyGain(diff, listeningGain), error))
        return false;

    std::ostringstream manifest;
    manifest << commonManifestPrefix("after", rendered)
             << "  \"baseline_manifest\": \"retrigger-release-tail-before-manifest.json\",\n"
             << "  \"raw_levels\": {\n"
             << "    \"before\": " << levelJson(beforeLevel, 4) << ",\n"
             << "    \"after\": " << levelJson(afterLevel, 4) << ",\n"
             << "    \"signed_difference_after_minus_before\": "
             << levelJson(diffLevel, 4) << "\n"
             << "  },\n"
             << "  \"relative_metrics\": {\n"
             << "    \"difference_peak_over_before_peak_dbc\": "
             << jsonNumber(peakDbc) << ",\n"
             << "    \"difference_rms_over_before_rms_dbc\": "
             << jsonNumber(rmsDbc) << "\n"
             << "  },\n"
             << "  \"listening\": {\n"
             << "    \"target_peak_dbfs\": -6.0,\n"
             << "    \"shared_gain_linear\": " << jsonNumber(listeningGain) << ",\n"
             << "    \"shared_gain_db\": " << jsonNumber(decibels(listeningGain)) << ",\n"
             << "    \"policy\": \"one gain derived from max(before peak, after peak), identically applied to before, after, and signed difference\"\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory
                   / "retrigger-release-tail-comparison-manifest.json",
                   manifest.str(), error))
        return false;

    if (!updateReadme(outputDirectory,
                      afterGenerated(beforeLevel, afterLevel, diffLevel,
                                     listeningGain, peakDbc, rmsDbc), error))
        return false;

    std::printf("Difference peak / before peak: %+.2f dBc\n", peakDbc);
    std::printf("Difference RMS  / before RMS:  %+.2f dBc\n", rmsDbc);
    return true;
}

bool writeCommonVcaBefore(const std::filesystem::path& outputDirectory,
                          const RenderedScenario& rendered, std::string& error)
{
    const auto rawPath = outputDirectory / "common-vca-level-before-raw-f32.wav";
    if (std::filesystem::exists(rawPath))
    {
        StereoBuffer archived;
        if (!readFloatWav(rawPath, archived, error))
            return false;
        if (!youknow106::tools::realism::buffersEqual(archived, rendered.audio))
        {
            error = "refusing to overwrite a non-identical raw before archive: "
                  + rawPath.string();
            return false;
        }
    }
    else if (!writeFloatWav(rawPath, rendered.audio, error))
    {
        return false;
    }

    const Level rawLevel = measure(rendered.audio);
    if (rawLevel.peak <= 1.0e-12 || rawLevel.rms <= 1.0e-12)
    {
        error = "scenario rendered silence";
        return false;
    }
    const double listeningGain = listeningTargetPeak / rawLevel.peak;
    if (!writeFloatWav(outputDirectory / "common-vca-level-before-listen-f32.wav",
                       applyGain(rendered.audio, listeningGain), error))
        return false;

    std::ostringstream manifest;
    manifest << commonVcaManifestPrefix("before", rendered)
             << "  \"raw_level\": " << levelJson(rawLevel, 2) << ",\n"
             << "  \"listening\": {\n"
             << "    \"target_peak_dbfs\": -6.0,\n"
             << "    \"gain_linear\": " << jsonNumber(listeningGain) << ",\n"
             << "    \"gain_db\": " << jsonNumber(decibels(listeningGain)) << ",\n"
             << "    \"policy\": \"baseline-derived provisional gain; after pass applies one shared pair gain\"\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory / "common-vca-level-before-manifest.json",
                   manifest.str(), error))
        return false;
    return updateCommonVcaReadme(
        outputDirectory, commonVcaBeforeGenerated(rawLevel, listeningGain), error);
}

bool writeCommonVcaAfter(const std::filesystem::path& outputDirectory,
                         const RenderedScenario& rendered, std::string& error)
{
    StereoBuffer before;
    if (!readFloatWav(outputDirectory / "common-vca-level-before-raw-f32.wav",
                      before, error))
        return false;
    if (before.left.size() != rendered.audio.left.size())
    {
        error = "baseline frame count does not match common-vca-level protocol v1";
        return false;
    }

    StereoBuffer diff;
    if (!difference(before, rendered.audio, diff, error))
        return false;

    const Level beforeLevel = measure(before);
    const Level afterLevel = measure(rendered.audio);
    const Level diffLevel = measure(diff);
    if (beforeLevel.peak <= 1.0e-12 || beforeLevel.rms <= 1.0e-12)
    {
        error = "raw before archive is silent and cannot be a dBc reference";
        return false;
    }

    const double peakDbc = decibels(diffLevel.peak / beforeLevel.peak);
    const double rmsDbc = decibels(diffLevel.rms / beforeLevel.rms);
    const double pairPeak = std::max(beforeLevel.peak, afterLevel.peak);
    const double listeningGain = listeningTargetPeak / pairPeak;

    if (!writeFloatWav(outputDirectory / "common-vca-level-after-raw-f32.wav",
                       rendered.audio, error)
        || !writeFloatWav(outputDirectory
                          / "common-vca-level-difference-raw-f32.wav",
                          diff, error)
        || !writeFloatWav(outputDirectory
                          / "common-vca-level-before-listen-f32.wav",
                          applyGain(before, listeningGain), error)
        || !writeFloatWav(outputDirectory
                          / "common-vca-level-after-listen-f32.wav",
                          applyGain(rendered.audio, listeningGain), error)
        || !writeFloatWav(outputDirectory
                          / "common-vca-level-difference-listen-f32.wav",
                          applyGain(diff, listeningGain), error))
        return false;

    std::ostringstream manifest;
    manifest << commonVcaManifestPrefix("after", rendered)
             << "  \"baseline_manifest\": \"common-vca-level-before-manifest.json\",\n"
             << "  \"raw_levels\": {\n"
             << "    \"before\": " << levelJson(beforeLevel, 4) << ",\n"
             << "    \"after\": " << levelJson(afterLevel, 4) << ",\n"
             << "    \"signed_difference_after_minus_before\": "
             << levelJson(diffLevel, 4) << "\n"
             << "  },\n"
             << "  \"relative_metrics\": {\n"
             << "    \"difference_peak_over_before_peak_dbc\": "
             << jsonNumber(peakDbc) << ",\n"
             << "    \"difference_rms_over_before_rms_dbc\": "
             << jsonNumber(rmsDbc) << "\n"
             << "  },\n"
             << "  \"listening\": {\n"
             << "    \"target_peak_dbfs\": -6.0,\n"
             << "    \"shared_gain_linear\": " << jsonNumber(listeningGain) << ",\n"
             << "    \"shared_gain_db\": " << jsonNumber(decibels(listeningGain)) << ",\n"
             << "    \"policy\": \"one gain derived from max(before peak, after peak), identically applied to before, after, and signed difference\"\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory / "common-vca-level-comparison-manifest.json",
                   manifest.str(), error))
        return false;

    if (!updateCommonVcaReadme(
            outputDirectory,
            commonVcaAfterGenerated(beforeLevel, afterLevel, diffLevel,
                                    listeningGain, peakDbc, rmsDbc), error))
        return false;

    std::printf("Difference peak / before peak: %+.2f dBc\n", peakDbc);
    std::printf("Difference RMS  / before RMS:  %+.2f dBc\n", rmsDbc);
    return true;
}

bool writeBbdTransferBefore(const std::filesystem::path& outputDirectory,
                            const RenderedScenario& rendered,
                            std::string& error)
{
    const auto rawPath = outputDirectory
                       / "bbd-transfer-clock-law-before-raw-f32.wav";
    if (std::filesystem::exists(rawPath))
    {
        StereoBuffer archived;
        if (!readFloatWav(rawPath, archived, error))
            return false;
        if (!youknow106::tools::realism::buffersEqual(archived, rendered.audio))
        {
            error = "refusing to overwrite a non-identical raw before archive: "
                  + rawPath.string();
            return false;
        }
    }
    else if (!writeFloatWav(rawPath, rendered.audio, error))
    {
        return false;
    }

    const Level rawLevel = measure(rendered.audio);
    if (rawLevel.peak <= 1.0e-12 || rawLevel.rms <= 1.0e-12)
    {
        error = "scenario rendered silence";
        return false;
    }
    const double listeningGain = listeningTargetPeak / rawLevel.peak;
    if (!writeFloatWav(outputDirectory
                       / "bbd-transfer-clock-law-before-listen-f32.wav",
                       applyGain(rendered.audio, listeningGain), error))
        return false;

    std::ostringstream manifest;
    manifest << bbdTransferManifestPrefix("before", rendered)
             << "  \"raw_level\": " << levelJson(rawLevel, 2) << ",\n"
             << "  \"listening\": {\n"
             << "    \"target_peak_dbfs\": -6.0,\n"
             << "    \"gain_linear\": " << jsonNumber(listeningGain) << ",\n"
             << "    \"gain_db\": " << jsonNumber(decibels(listeningGain)) << ",\n"
             << "    \"policy\": \"baseline-derived provisional gain; after pass applies one shared pair gain\"\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory
                   / "bbd-transfer-clock-law-before-manifest.json",
                   manifest.str(), error))
        return false;
    return updateBbdTransferReadme(
        outputDirectory, bbdTransferBeforeGenerated(rawLevel, listeningGain), error);
}

bool writeBbdTransferAfter(const std::filesystem::path& outputDirectory,
                           const RenderedScenario& rendered,
                           std::string& error)
{
    StereoBuffer before;
    if (!readFloatWav(outputDirectory
                      / "bbd-transfer-clock-law-before-raw-f32.wav",
                      before, error))
        return false;
    if (before.left.size() != rendered.audio.left.size())
    {
        error = "baseline frame count does not match bbd-transfer-clock-law protocol v1";
        return false;
    }

    StereoBuffer diff;
    if (!difference(before, rendered.audio, diff, error))
        return false;

    const Level beforeLevel = measure(before);
    const Level afterLevel = measure(rendered.audio);
    const Level diffLevel = measure(diff);
    if (beforeLevel.peak <= 1.0e-12 || beforeLevel.rms <= 1.0e-12)
    {
        error = "raw before archive is silent and cannot be a dBc reference";
        return false;
    }

    const double peakDbc = decibels(diffLevel.peak / beforeLevel.peak);
    const double rmsDbc = decibels(diffLevel.rms / beforeLevel.rms);
    const double pairPeak = std::max(beforeLevel.peak, afterLevel.peak);
    const double listeningGain = listeningTargetPeak / pairPeak;

    if (!writeFloatWav(outputDirectory
                       / "bbd-transfer-clock-law-after-raw-f32.wav",
                       rendered.audio, error)
        || !writeFloatWav(outputDirectory
                          / "bbd-transfer-clock-law-difference-raw-f32.wav",
                          diff, error)
        || !writeFloatWav(outputDirectory
                          / "bbd-transfer-clock-law-before-listen-f32.wav",
                          applyGain(before, listeningGain), error)
        || !writeFloatWav(outputDirectory
                          / "bbd-transfer-clock-law-after-listen-f32.wav",
                          applyGain(rendered.audio, listeningGain), error)
        || !writeFloatWav(outputDirectory
                          / "bbd-transfer-clock-law-difference-listen-f32.wav",
                          applyGain(diff, listeningGain), error))
        return false;

    std::ostringstream manifest;
    manifest << bbdTransferManifestPrefix("after", rendered)
             << "  \"baseline_manifest\": \"bbd-transfer-clock-law-before-manifest.json\",\n"
             << "  \"raw_levels\": {\n"
             << "    \"before\": " << levelJson(beforeLevel, 4) << ",\n"
             << "    \"after\": " << levelJson(afterLevel, 4) << ",\n"
             << "    \"signed_difference_after_minus_before\": "
             << levelJson(diffLevel, 4) << "\n"
             << "  },\n"
             << "  \"relative_metrics\": {\n"
             << "    \"difference_peak_over_before_peak_dbc\": "
             << jsonNumber(peakDbc) << ",\n"
             << "    \"difference_rms_over_before_rms_dbc\": "
             << jsonNumber(rmsDbc) << "\n"
             << "  },\n"
             << "  \"listening\": {\n"
             << "    \"target_peak_dbfs\": -6.0,\n"
             << "    \"shared_gain_linear\": " << jsonNumber(listeningGain) << ",\n"
             << "    \"shared_gain_db\": " << jsonNumber(decibels(listeningGain)) << ",\n"
             << "    \"policy\": \"one gain derived from max(before peak, after peak), identically applied to before, after, and signed difference\"\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory
                   / "bbd-transfer-clock-law-comparison-manifest.json",
                   manifest.str(), error))
        return false;

    if (!updateBbdTransferReadme(
            outputDirectory,
            bbdTransferAfterGenerated(beforeLevel, afterLevel, diffLevel,
                                      listeningGain, peakDbc, rmsDbc), error))
        return false;

    std::printf("Difference peak / before peak: %+.2f dBc\n", peakDbc);
    std::printf("Difference RMS  / before RMS:  %+.2f dBc\n", rmsDbc);
    return true;
}

bool checkVoiceVcaDiagnosticLevel(const StereoBuffer& audio,
                                  std::string_view label,
                                  std::string& error)
{
    const auto level = measure(audio);
    if (level.peak > voiceVcaFeedthroughListeningCeiling)
    {
        error = std::string(label) + " fixed +30 dB diagnostic peak "
              + jsonNumber(level.peak) + " exceeds protocol ceiling "
              + jsonNumber(voiceVcaFeedthroughListeningCeiling)
              + "; refusing to adapt the gain";
        return false;
    }
    return true;
}

bool writeVoiceVcaFeedthroughBefore(
    const std::filesystem::path& outputDirectory,
    const RenderedScenario& rendered, std::string& error)
{
    if (rendered.audio.left.size() != voiceVcaEndSample)
    {
        error = "voice-vca-feedthrough render did not end at sample 181440";
        return false;
    }

    const auto rawPath = outputDirectory
                       / "voice-vca-feedthrough-before-raw-f32.wav";
    if (std::filesystem::exists(rawPath))
    {
        StereoBuffer archived;
        if (!readFloatWav(rawPath, archived, error))
            return false;
        if (!youknow106::tools::realism::buffersEqual(archived, rendered.audio))
        {
            error = "refusing to overwrite a non-identical raw before archive: "
                  + rawPath.string();
            return false;
        }
    }
    else if (!writeFloatWav(rawPath, rendered.audio, error))
    {
        return false;
    }

    const Level rawLevel = measure(rendered.audio);
    if (rawLevel.peak <= 0.0 || rawLevel.rms <= 0.0)
    {
        error = "scenario rendered exact silence; expected deterministic full-engine floor";
        return false;
    }
    const StereoBuffer listening = applyGain(
        rendered.audio, voiceVcaFeedthroughListeningGain);
    if (!checkVoiceVcaDiagnosticLevel(listening, "before", error)
        || !writeFloatWav(outputDirectory
                          / "voice-vca-feedthrough-before-listen-f32.wav",
                          listening, error))
        return false;

    std::ostringstream manifest;
    manifest << voiceVcaFeedthroughManifestPrefix("before", rendered)
             << "  \"raw_level\": " << levelJson(rawLevel, 2) << ",\n"
             << "  \"raw_window_metrics\": "
             << voiceVcaWindowMetricsJson(rendered.audio, 2) << ",\n"
             << "  \"listening\": {\n"
             << "    \"gain_linear\": "
             << jsonNumber(voiceVcaFeedthroughListeningGain) << ",\n"
             << "    \"gain_db\": 30.0,\n"
             << "    \"adaptive_normalization\": false,\n"
             << "    \"label\": \"diagnostic magnification; raw file carries actual level\",\n"
             << "    \"peak_linear\": " << jsonNumber(measure(listening).peak) << "\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory
                   / "voice-vca-feedthrough-before-manifest.json",
                   manifest.str(), error))
        return false;
    return updateVoiceVcaFeedthroughReadme(
        outputDirectory,
        voiceVcaFeedthroughBeforeGenerated(rendered.audio, rawLevel), error);
}

bool writeVoiceVcaFeedthroughAfter(
    const std::filesystem::path& outputDirectory,
    const RenderedScenario& rendered, std::string& error)
{
    StereoBuffer before;
    if (!readFloatWav(outputDirectory
                      / "voice-vca-feedthrough-before-raw-f32.wav",
                      before, error))
        return false;
    if (before.left.size() != voiceVcaEndSample
        || rendered.audio.left.size() != voiceVcaEndSample)
    {
        error = "baseline or after frame count does not match voice-vca-feedthrough protocol v1";
        return false;
    }

    StereoBuffer diff;
    if (!difference(before, rendered.audio, diff, error))
        return false;

    const Level beforeLevel = measure(before);
    const Level afterLevel = measure(rendered.audio);
    const Level diffLevel = measure(diff);
    if (beforeLevel.peak <= 0.0 || beforeLevel.rms <= 0.0)
    {
        error = "raw before archive is silent and cannot be a dBc reference";
        return false;
    }

    const double peakDbc = decibels(diffLevel.peak / beforeLevel.peak);
    const double rmsDbc = decibels(diffLevel.rms / beforeLevel.rms);
    const StereoBuffer beforeListening = applyGain(
        before, voiceVcaFeedthroughListeningGain);
    const StereoBuffer afterListening = applyGain(
        rendered.audio, voiceVcaFeedthroughListeningGain);
    const StereoBuffer diffListening = applyGain(
        diff, voiceVcaFeedthroughListeningGain);
    if (!checkVoiceVcaDiagnosticLevel(beforeListening, "before", error)
        || !checkVoiceVcaDiagnosticLevel(afterListening, "after", error)
        || !checkVoiceVcaDiagnosticLevel(diffListening, "difference", error))
        return false;

    if (!writeFloatWav(outputDirectory
                       / "voice-vca-feedthrough-after-raw-f32.wav",
                       rendered.audio, error)
        || !writeFloatWav(outputDirectory
                          / "voice-vca-feedthrough-difference-raw-f32.wav",
                          diff, error)
        || !writeFloatWav(outputDirectory
                          / "voice-vca-feedthrough-before-listen-f32.wav",
                          beforeListening, error)
        || !writeFloatWav(outputDirectory
                          / "voice-vca-feedthrough-after-listen-f32.wav",
                          afterListening, error)
        || !writeFloatWav(outputDirectory
                          / "voice-vca-feedthrough-difference-listen-f32.wav",
                          diffListening, error))
        return false;

    std::ostringstream manifest;
    manifest << voiceVcaFeedthroughManifestPrefix("after", rendered)
             << "  \"baseline_manifest\": \"voice-vca-feedthrough-before-manifest.json\",\n"
             << "  \"raw_levels\": {\n"
             << "    \"before\": " << levelJson(beforeLevel, 4) << ",\n"
             << "    \"after\": " << levelJson(afterLevel, 4) << ",\n"
             << "    \"signed_difference_after_minus_before\": "
             << levelJson(diffLevel, 4) << "\n"
             << "  },\n"
             << "  \"raw_window_metrics\": {\n"
             << "    \"before\": " << voiceVcaWindowMetricsJson(before, 4) << ",\n"
             << "    \"after\": "
             << voiceVcaWindowMetricsJson(rendered.audio, 4) << ",\n"
             << "    \"signed_difference_after_minus_before\": "
             << voiceVcaWindowMetricsJson(diff, 4) << "\n"
             << "  },\n"
             << "  \"relative_metrics\": {\n"
             << "    \"difference_peak_over_before_peak_dbc\": "
             << jsonNumber(peakDbc) << ",\n"
             << "    \"difference_rms_over_before_rms_dbc\": "
             << jsonNumber(rmsDbc) << "\n"
             << "  },\n"
             << "  \"listening\": {\n"
             << "    \"shared_fixed_gain_linear\": "
             << jsonNumber(voiceVcaFeedthroughListeningGain) << ",\n"
             << "    \"shared_fixed_gain_db\": 30.0,\n"
             << "    \"adaptive_normalization\": false,\n"
             << "    \"label\": \"diagnostic magnification; raw files carry actual levels\",\n"
             << "    \"before_peak_linear\": "
             << jsonNumber(measure(beforeListening).peak) << ",\n"
             << "    \"after_peak_linear\": "
             << jsonNumber(measure(afterListening).peak) << ",\n"
             << "    \"difference_peak_linear\": "
             << jsonNumber(measure(diffListening).peak) << "\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory
                   / "voice-vca-feedthrough-comparison-manifest.json",
                   manifest.str(), error))
        return false;

    if (!updateVoiceVcaFeedthroughReadme(
            outputDirectory,
            voiceVcaFeedthroughAfterGenerated(
                before, rendered.audio, diff, beforeLevel, afterLevel, diffLevel,
                peakDbc, rmsDbc), error))
        return false;

    std::printf("Difference peak / before peak: %+.2f dBc\n", peakDbc);
    std::printf("Difference RMS  / before RMS:  %+.2f dBc\n", rmsDbc);
    return true;
}

bool checkBbdAliasListeningLevel(const StereoBuffer& audio,
                                 std::string_view label,
                                 std::string& error)
{
    const auto level = measure(audio);
    if (level.peak > bbdAliasListeningCeiling)
    {
        error = std::string(label) + " fixed-gain peak "
              + jsonNumber(level.peak) + " exceeds protocol ceiling "
              + jsonNumber(bbdAliasListeningCeiling)
              + "; refusing to normalize or change the fixed gain";
        return false;
    }
    return true;
}

bool writeBbdHostGridAliasBefore(
    const std::filesystem::path& outputDirectory,
    const RenderedScenario& rendered, std::string& error)
{
    const auto rawPath = outputDirectory
                       / "bbd-host-grid-alias-before-raw-f32.wav";
    if (std::filesystem::exists(rawPath))
    {
        StereoBuffer archived;
        if (!readFloatWav(rawPath, archived, error))
            return false;
        if (!youknow106::tools::realism::buffersEqual(archived, rendered.audio))
        {
            error = "refusing to overwrite a non-identical raw before archive: "
                  + rawPath.string();
            return false;
        }
    }
    else if (!writeFloatWav(rawPath, rendered.audio, error))
    {
        return false;
    }

    const Level rawLevel = measure(rendered.audio);
    if (rawLevel.peak <= 1.0e-12 || rawLevel.rms <= 1.0e-12)
    {
        error = "bbd-host-grid-alias scenario rendered silence";
        return false;
    }
    const StereoBuffer listening = applyGain(
        rendered.audio, bbdAliasListeningGain);
    if (!checkBbdAliasListeningLevel(listening, "before", error)
        || !writeFloatWav(
            outputDirectory / "bbd-host-grid-alias-before-listen-f32.wav",
            listening, error))
        return false;

    std::ostringstream manifest;
    manifest << bbdHostGridAliasManifestPrefix("before", rendered)
             << "  \"artifact_sample_hashes\": {\n"
             << "    \"algorithm\": \"FNV-1a-64 over canonical interleaved little-endian float32 sample bits; RIFF header excluded\",\n"
             << "    \"before_raw\": \""
             << hex64(interleavedFloatHash(rendered.audio)) << "\",\n"
             << "    \"before_listen\": \""
             << hex64(interleavedFloatHash(listening)) << "\"\n"
             << "  },\n"
             << "  \"raw_level\": " << levelJson(rawLevel, 2) << ",\n"
             << "  \"fixed_clock_probe_metrics\": "
             << bbdAliasFixedProbeMetricsJson(rendered) << ",\n"
             << "  \"listening\": {\n"
             << "    \"gain_linear\": "
             << jsonNumber(bbdAliasListeningGain) << ",\n"
             << "    \"gain_db\": "
             << jsonNumber(decibels(bbdAliasListeningGain)) << ",\n"
             << "    \"adaptive_normalization\": false,\n"
             << "    \"per_section_normalization\": false,\n"
             << "    \"peak_linear\": "
             << jsonNumber(measure(listening).peak) << "\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory
                   / "bbd-host-grid-alias-before-manifest.json",
                   manifest.str(), error))
        return false;
    return updateBbdHostGridAliasReadme(
        outputDirectory,
        bbdHostGridAliasBeforeGenerated(rendered, rawLevel, listening), error);
}

std::string bbdAliasPreservationDeltasJson(
    const RenderedScenario& before, const RenderedScenario& after)
{
    const auto settings = Chorus::settingsFor(ChorusMode::One);
    const double clock = Chorus::clockForDelaySeconds(
        settings.centreDelaySeconds + settings.sweepSeconds);
    const double input = bbdAliasProbeFrequencyHz;
    const double physicalImage = clock - input;

    std::ostringstream output;
    output << "{\n";
    constexpr std::array<std::string_view, 2> names {
        "fixed-min-clock-tone-hq", "fixed-min-clock-tone-lq"
    };
    constexpr std::array<std::string_view, 2> labels {
        "hq_192khz_internal", "lq_48khz_internal"
    };
    for (std::size_t quality = 0; quality < names.size(); ++quality)
    {
        const auto* beforeSection = findBbdAliasSection(before, names[quality]);
        const auto* afterSection = findBbdAliasSection(after, names[quality]);
        const double beforeFundamental = beforeSection != nullptr
            ? hannToneAmplitude(before.audio, *beforeSection, input) : 0.0;
        const double afterFundamental = afterSection != nullptr
            ? hannToneAmplitude(after.audio, *afterSection, input) : 0.0;
        const double beforePhysical = beforeSection != nullptr
            ? hannToneAmplitude(before.audio, *beforeSection, physicalImage) : 0.0;
        const double afterPhysical = afterSection != nullptr
            ? hannToneAmplitude(after.audio, *afterSection, physicalImage) : 0.0;
        output << "    \"" << labels[quality] << "\": {\n"
               << "      \"fundamental_after_over_before_db\": "
               << jsonNumber(decibels(afterFundamental / std::max(
                      beforeFundamental, 1.0e-18))) << ",\n"
               << "      \"wanted_physical_k1_minus_after_over_before_db\": "
               << jsonNumber(decibels(afterPhysical / std::max(
                      beforePhysical, 1.0e-18))) << ",\n"
               << "      \"preservation_guidance_absolute_db\": 0.5,\n"
               << "      \"guidance_role\": \"review target, not an invented hardware tolerance\"\n"
               << "    }";
        if (quality + 1 < names.size())
            output << ',';
        output << '\n';
    }
    output << "  }";
    return output.str();
}

std::string bbdAliasFixedProbeComparisonJson(
    const RenderedScenario& before, const RenderedScenario& after)
{
    constexpr std::array<std::string_view, 2> names {
        "fixed-min-clock-tone-hq", "fixed-min-clock-tone-lq"
    };
    constexpr std::array<std::string_view, 2> labels {
        "hq_192khz_internal", "lq_48khz_internal"
    };

    std::ostringstream output;
    output << "{\n";
    for (std::size_t quality = 0; quality < names.size(); ++quality)
    {
        const auto* beforeSection = findBbdAliasSection(before, names[quality]);
        const auto* afterSection = findBbdAliasSection(after, names[quality]);
        output << "    \"" << labels[quality] << "\": {\n";
        if (beforeSection == nullptr || afterSection == nullptr)
        {
            output << "      \"error\": \"section missing\"\n";
        }
        else
        {
            const double beforeFundamental = hannToneAmplitude(
                before.audio, *beforeSection, bbdAliasProbeFrequencyHz);
            const double afterFundamental = hannToneAmplitude(
                after.audio, *afterSection, bbdAliasProbeFrequencyHz);
            output << "      \"fundamental_after_over_before_db\": "
                   << jsonNumber(decibels(afterFundamental / std::max(
                          beforeFundamental, 1.0e-18))) << ",\n"
                   << "      \"targets\": {\n";
            const auto targets = bbdAliasComparisonTargets();
            for (std::size_t index = 0; index < targets.size(); ++index)
            {
                const auto& target = targets[index];
                const double beforeDbc = decibels(hannToneAmplitude(
                    before.audio, *beforeSection, target.observedFrequencyHz)
                    / std::max(beforeFundamental, 1.0e-18));
                const double afterDbc = decibels(hannToneAmplitude(
                    after.audio, *afterSection, target.observedFrequencyHz)
                    / std::max(afterFundamental, 1.0e-18));
                output << "        \"" << target.name << "\": {\n"
                       << "          \"classification\": \""
                       << (index == 0u
                               ? "wanted physical BBD image below host Nyquist"
                               : "unwanted host-grid fold of out-of-band physical image")
                       << "\",\n"
                       << "          \"source_frequency_hz\": "
                       << jsonNumber(target.sourceFrequencyHz) << ",\n"
                       << "          \"observed_frequency_hz\": "
                       << jsonNumber(target.observedFrequencyHz) << ",\n"
                       << "          \"before_relative_to_fundamental_dbc\": "
                       << jsonNumber(beforeDbc) << ",\n"
                       << "          \"after_relative_to_fundamental_dbc\": "
                       << jsonNumber(afterDbc) << ",\n"
                       << "          \"after_minus_before_db\": "
                       << jsonNumber(afterDbc - beforeDbc) << "\n"
                       << "        }";
                if (index + 1u < targets.size())
                    output << ',';
                output << '\n';
            }
            output << "      }\n";
        }
        output << "    }";
        if (quality + 1u < names.size())
            output << ',';
        output << '\n';
    }
    output << "  }";
    return output.str();
}

std::string bbdAliasSectionComparisonJson(
    const RenderedScenario& before, const RenderedScenario& after,
    const StereoBuffer& diff)
{
    std::ostringstream output;
    output << "[\n";
    for (std::size_t index = 0;
         index < after.bbdAliasSections.size(); ++index)
    {
        const auto& section = after.bbdAliasSections[index];
        const auto* beforeSection = findBbdAliasSection(before, section.name);
        output << "    {\n"
               << "      \"name\": \"" << section.name << "\",\n"
               << "      \"kind\": \"" << section.kind << "\",\n"
               << "      \"quality\": \"" << section.quality << "\",\n"
               << "      \"chorus\": \"" << section.chorus << "\",\n";
        if (beforeSection == nullptr)
        {
            output << "      \"error\": \"matching before section missing\"\n";
        }
        else
        {
            const auto beforeLevel = measureBbdAliasSection(
                before.audio, *beforeSection);
            const auto afterLevel = measureBbdAliasSection(after.audio, section);
            const auto diffLevel = measureBbdAliasSection(diff, section);
            output << "      \"before\": " << levelJson(beforeLevel, 6)
                   << ",\n"
                   << "      \"after\": " << levelJson(afterLevel, 6)
                   << ",\n"
                   << "      \"signed_difference_after_minus_before\": "
                   << levelJson(diffLevel, 6) << ",\n"
                   << "      \"difference_peak_over_before_peak_dbc\": "
                   << jsonNumber(decibels(diffLevel.peak / std::max(
                          beforeLevel.peak, 1.0e-18))) << ",\n"
                   << "      \"difference_rms_over_before_rms_dbc\": "
                   << jsonNumber(decibels(diffLevel.rms / std::max(
                          beforeLevel.rms, 1.0e-18))) << "\n";
        }
        output << "    }";
        if (index + 1u < after.bbdAliasSections.size())
            output << ',';
        output << '\n';
    }
    output << "  ]";
    return output.str();
}

bool writeBbdHostGridAliasAfter(
    const std::filesystem::path& outputDirectory,
    const RenderedScenario& rendered, std::string& error)
{
    StereoBuffer beforeAudio;
    if (!readFloatWav(outputDirectory
                      / "bbd-host-grid-alias-before-raw-f32.wav",
                      beforeAudio, error))
        return false;
    if (beforeAudio.left.size() != rendered.audio.left.size())
    {
        error = "baseline frame count does not match bbd-host-grid-alias protocol v1";
        return false;
    }
    BbdAliasBaselineIdentity baselineIdentity;
    if (!readAndValidateBbdAliasBaselineManifest(
            outputDirectory / "bbd-host-grid-alias-before-manifest.json",
            beforeAudio, rendered.audio.left.size(), baselineIdentity, error))
        return false;

    StereoBuffer diff;
    if (!difference(beforeAudio, rendered.audio, diff, error))
        return false;
    const Level beforeLevel = measure(beforeAudio);
    const Level afterLevel = measure(rendered.audio);
    const Level diffLevel = measure(diff);
    if (beforeLevel.peak <= 1.0e-12 || beforeLevel.rms <= 1.0e-12)
    {
        error = "raw before archive is silent and cannot be a dBc reference";
        return false;
    }
    const double peakDbc = decibels(diffLevel.peak / beforeLevel.peak);
    const double rmsDbc = decibels(diffLevel.rms / beforeLevel.rms);
    const StereoBuffer beforeListening = applyGain(
        beforeAudio, bbdAliasListeningGain);
    const StereoBuffer afterListening = applyGain(
        rendered.audio, bbdAliasListeningGain);
    const StereoBuffer diffListening = applyGain(
        diff, bbdAliasListeningGain);
    if (!checkBbdAliasListeningLevel(beforeListening, "before", error)
        || !checkBbdAliasListeningLevel(afterListening, "after", error)
        || !checkBbdAliasListeningLevel(diffListening, "difference", error))
        return false;

    if (!writeFloatWav(outputDirectory
                       / "bbd-host-grid-alias-after-raw-f32.wav",
                       rendered.audio, error)
        || !writeFloatWav(outputDirectory
                          / "bbd-host-grid-alias-difference-raw-f32.wav",
                          diff, error)
        || !writeFloatWav(outputDirectory
                          / "bbd-host-grid-alias-before-listen-f32.wav",
                          beforeListening, error)
        || !writeFloatWav(outputDirectory
                          / "bbd-host-grid-alias-after-listen-f32.wav",
                          afterListening, error)
        || !writeFloatWav(outputDirectory
                          / "bbd-host-grid-alias-difference-listen-f32.wav",
                          diffListening, error))
        return false;

    RenderedScenario beforeRendered = rendered;
    beforeRendered.audio = beforeAudio;
    std::ostringstream manifest;
    manifest << bbdHostGridAliasManifestPrefix("after", rendered)
             << "  \"baseline_manifest\": \"bbd-host-grid-alias-before-manifest.json\",\n"
             << "  \"baseline_dsp_source_sha256\": \""
             << baselineIdentity.dspSourceSha256 << "\",\n"
             << "  \"baseline_raw_sample_hash_fnv1a64\": \""
             << baselineIdentity.rawSampleHashFnv1a64 << "\",\n"
             << "  \"artifact_sample_hashes\": {\n"
             << "    \"algorithm\": \"FNV-1a-64 over canonical interleaved little-endian float32 sample bits; RIFF header excluded\",\n"
             << "    \"before_raw\": \""
             << hex64(interleavedFloatHash(beforeAudio)) << "\",\n"
             << "    \"after_raw\": \""
             << hex64(interleavedFloatHash(rendered.audio)) << "\",\n"
             << "    \"difference_raw\": \""
             << hex64(interleavedFloatHash(diff)) << "\"\n"
             << "  },\n"
             << "  \"raw_levels\": {\n"
             << "    \"before\": " << levelJson(beforeLevel, 4) << ",\n"
             << "    \"after\": " << levelJson(afterLevel, 4) << ",\n"
             << "    \"signed_difference_after_minus_before\": "
             << levelJson(diffLevel, 4) << "\n"
             << "  },\n"
             << "  \"relative_metrics\": {\n"
             << "    \"difference_peak_over_before_peak_dbc\": "
             << jsonNumber(peakDbc) << ",\n"
             << "    \"difference_rms_over_before_rms_dbc\": "
             << jsonNumber(rmsDbc) << "\n"
             << "  },\n"
             << "  \"fixed_clock_probe_metrics_before\": "
             << bbdAliasFixedProbeMetricsJson(beforeRendered) << ",\n"
             << "  \"fixed_clock_probe_metrics_after\": "
             << bbdAliasFixedProbeMetricsJson(rendered) << ",\n"
             << "  \"fixed_clock_probe_comparison\": "
             << bbdAliasFixedProbeComparisonJson(beforeRendered, rendered)
             << ",\n"
             << "  \"emitted_response_preservation\": "
             << bbdAliasPreservationDeltasJson(beforeRendered, rendered) << ",\n"
             << "  \"section_comparison_metrics\": "
             << bbdAliasSectionComparisonJson(beforeRendered, rendered, diff)
             << ",\n"
             << "  \"listening\": {\n"
             << "    \"shared_fixed_gain_linear\": "
             << jsonNumber(bbdAliasListeningGain) << ",\n"
             << "    \"shared_fixed_gain_db\": "
             << jsonNumber(decibels(bbdAliasListeningGain)) << ",\n"
             << "    \"adaptive_normalization\": false,\n"
             << "    \"per_stage_normalization\": false\n"
             << "  }\n"
             << "}\n";
    if (!writeText(outputDirectory
                   / "bbd-host-grid-alias-comparison-manifest.json",
                   manifest.str(), error))
        return false;
    if (!updateBbdHostGridAliasReadme(
            outputDirectory,
            bbdHostGridAliasAfterGenerated(
                beforeRendered, rendered, diff, beforeLevel, afterLevel,
                diffLevel, peakDbc, rmsDbc), error))
        return false;

    std::printf("Difference peak / before peak: %+.2f dBc\n", peakDbc);
    std::printf("Difference RMS  / before RMS:  %+.2f dBc\n", rmsDbc);
    return true;
}

bool runBbdAliasBaselineContractSelfTest(std::string& error)
{
    StereoBuffer archived;
    archived.left = { 0.125f, -0.25f, 0.375f };
    archived.right = { -0.5f, 0.625f, -0.75f };

    const std::string currentFingerprint = YOUKNOW106_DSP_SOURCE_SHA256;
    if (!isLowerHex(currentFingerprint, 64u))
    {
        error = "self-test build has no valid DSP source fingerprint";
        return false;
    }
    std::string priorFingerprint(64u, '0');
    if (priorFingerprint == currentFingerprint)
        priorFingerprint.assign(64u, '1');
    const std::string rawHash = hex64(interleavedFloatHash(archived));

    const auto makeManifest = [&](std::string_view stage,
                                  std::string_view fingerprint) {
        std::ostringstream manifest;
        manifest << "{\n"
                 << "  \"schema_version\": 1,\n"
                 << "  \"tool\": \"YouKnow106RenderRealismComparison\",\n"
                 << "  \"scenario\": \"bbd-host-grid-alias\",\n"
                 << "  \"scenario_protocol_version\": 1,\n"
                 << "  \"stage\": \"" << stage << "\",\n"
                 << "  \"dsp_source_sha256\": \"" << fingerprint << "\",\n"
                 << "  \"sample_rate_hz\": 48000,\n"
                 << "  \"channels\": 2,\n"
                 << "  \"frames\": " << archived.left.size() << ",\n"
                 << "  \"artifact_sample_hashes\": {\n"
                 << "    \"before_raw\": \"" << rawHash << "\"\n"
                 << "  }\n"
                 << "}\n";
        return manifest.str();
    };

    BbdAliasBaselineIdentity identity;
    std::string validationError;
    const std::string valid = makeManifest("before", priorFingerprint);
    if (!validateBbdAliasBaselineManifestText(
            valid, archived, archived.left.size(), identity, validationError))
    {
        error = "valid baseline contract rejected: " + validationError;
        return false;
    }

    StereoBuffer wrongSameLength = archived;
    wrongSameLength.left[1] = 0.25f;
    validationError.clear();
    if (validateBbdAliasBaselineManifestText(
            valid, wrongSameLength, wrongSameLength.left.size(), identity,
            validationError))
    {
        error = "same-length wrong baseline WAV escaped manifest hash binding";
        return false;
    }

    validationError.clear();
    if (validateBbdAliasBaselineManifestText(
            makeManifest("after", priorFingerprint), archived,
            archived.left.size(), identity, validationError))
    {
        error = "non-before baseline manifest escaped stage validation";
        return false;
    }

    validationError.clear();
    if (validateBbdAliasBaselineManifestText(
            makeManifest("before", currentFingerprint), archived,
            archived.left.size(), identity, validationError))
    {
        error = "same-build baseline escaped DSP fingerprint validation";
        return false;
    }
    return true;
}

void printUsage(const char* executable)
{
    std::fprintf(stderr,
                 "usage: %s <scenario> <before|after> <output-dir>\n"
                 "scenarios: retrigger-release-tail, common-vca-level, "
                 "bbd-transfer-clock-law, voice-vca-feedthrough, "
                 "bbd-host-grid-alias\n",
                 executable);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 2
        && std::string_view(argv[1]) == "--self-test-baseline-contract")
    {
        std::string error;
        if (!runBbdAliasBaselineContractSelfTest(error))
        {
            std::fprintf(stderr, "baseline-contract self-test failed: %s\n",
                         error.c_str());
            return 1;
        }
        std::printf("BBD baseline manifest/hash contract self-test passed.\n");
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--help")
    {
        printUsage(argv[0]);
        return 0;
    }
    if (argc != 4)
    {
        printUsage(argv[0]);
        return 2;
    }

    const std::string_view scenario = argv[1];
    const std::string_view stage = argv[2];
    const std::filesystem::path outputDirectory = argv[3];
    const bool retriggerScenario = scenario == scenarioSlug;
    const bool commonVcaScenario = scenario == commonVcaScenarioSlug;
    const bool bbdTransferScenario = scenario == bbdTransferScenarioSlug;
    const bool voiceVcaFeedthroughScenario =
        scenario == voiceVcaFeedthroughScenarioSlug;
    const bool bbdHostGridAliasScenario =
        scenario == bbdHostGridAliasScenarioSlug;
    if ((!retriggerScenario && !commonVcaScenario && !bbdTransferScenario
         && !voiceVcaFeedthroughScenario && !bbdHostGridAliasScenario)
        || (stage != "before" && stage != "after")
        || outputDirectory.empty())
    {
        printUsage(argv[0]);
        return 2;
    }

    RenderedScenario rendered = retriggerScenario
        ? renderRetriggerReleaseTail()
        : (commonVcaScenario
            ? renderCommonVcaLevel()
            : (bbdTransferScenario
                ? renderBbdTransferClockLaw()
                : (voiceVcaFeedthroughScenario
                    ? renderVoiceVcaFeedthrough()
                    : renderBbdHostGridAlias())));
    std::string error;
    if (!validate(rendered.audio, error))
    {
        std::fprintf(stderr, "render failed: %s\n", error.c_str());
        return 1;
    }

    bool written = false;
    if (retriggerScenario)
        written = stage == "before" ? writeBefore(outputDirectory, rendered, error)
                                     : writeAfter(outputDirectory, rendered, error);
    else if (commonVcaScenario)
        written = stage == "before"
            ? writeCommonVcaBefore(outputDirectory, rendered, error)
            : writeCommonVcaAfter(outputDirectory, rendered, error);
    else if (bbdTransferScenario)
        written = stage == "before"
            ? writeBbdTransferBefore(outputDirectory, rendered, error)
            : writeBbdTransferAfter(outputDirectory, rendered, error);
    else if (voiceVcaFeedthroughScenario)
        written = stage == "before"
            ? writeVoiceVcaFeedthroughBefore(outputDirectory, rendered, error)
            : writeVoiceVcaFeedthroughAfter(outputDirectory, rendered, error);
    else
        written = stage == "before"
            ? writeBbdHostGridAliasBefore(outputDirectory, rendered, error)
            : writeBbdHostGridAliasAfter(outputDirectory, rendered, error);
    if (!written)
    {
        std::fprintf(stderr, "comparison failed: %s\n", error.c_str());
        return 1;
    }

    const auto level = measure(rendered.audio);
    std::printf("Rendered %s %s: %zu frames, peak %.6f, RMS %.6f\n",
                std::string(scenario).c_str(), std::string(stage).c_str(),
                rendered.audio.left.size(), level.peak, level.rms);
    std::printf("Output: %s\n", outputDirectory.string().c_str());
    return 0;
}
