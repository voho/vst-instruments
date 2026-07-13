#include "DSP/MarsEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr int blockSize = 128;
int failures = 0;

#if defined(__has_feature)
constexpr bool sanitizerBuild = __has_feature(address_sanitizer)
                             || __has_feature(undefined_behavior_sanitizer);
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__)
constexpr bool sanitizerBuild = true;
#else
constexpr bool sanitizerBuild = false;
#endif

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct Metrics
{
    double sum { 0.0 };
    double sumSquares { 0.0 };
    double peak { 0.0 };
    double minimum { std::numeric_limits<double>::infinity() };
    double maximum { -std::numeric_limits<double>::infinity() };
    std::size_t samples { 0 };
    std::size_t nonZero { 0 };
    bool finite { true };

    [[nodiscard]] double rms() const noexcept
    {
        return samples == 0 ? 0.0 : std::sqrt(sumSquares / static_cast<double>(samples));
    }

    [[nodiscard]] double mean() const noexcept
    {
        return samples == 0 ? 0.0 : sum / static_cast<double>(samples);
    }

    void add(float value) noexcept
    {
        finite = finite && std::isfinite(value);
        if (!std::isfinite(value))
            return;
        const double magnitude = std::abs(static_cast<double>(value));
        peak = std::max(peak, magnitude);
        minimum = std::min(minimum, static_cast<double>(value));
        maximum = std::max(maximum, static_cast<double>(value));
        sum += static_cast<double>(value);
        sumSquares += static_cast<double>(value) * static_cast<double>(value);
        nonZero += magnitude > 1.0e-9 ? 1u : 0u;
        ++samples;
    }
};

Metrics render(mars::MarsEngine& engine, int sampleCount, int discardSamples = 0)
{
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    Metrics metrics;
    int position = 0;
    while (position < sampleCount)
    {
        const int count = std::min(blockSize, sampleCount - position);
        engine.process(left.data(), right.data(), count);
        for (int i = 0; i < count; ++i)
        {
            if (position + i >= discardSamples)
            {
                metrics.add(left[static_cast<std::size_t>(i)]);
                metrics.add(right[static_cast<std::size_t>(i)]);
            }
        }
        position += count;
    }
    return metrics;
}

mars::EngineParameters basicParameters()
{
    mars::EngineParameters p;
    p.osc1Wave = mars::OscillatorWave::Saw;
    p.osc2Wave = mars::OscillatorWave::Pulse;
    p.filterModel = mars::FilterModel::Ladder;
    p.voiceMode = mars::VoiceMode::Poly;
    p.lfoWave = mars::LfoWaveform::Triangle;
    p.osc1Octave = 0;
    p.osc2Octave = 0;
    p.osc2Semitones = 0;
    p.unisonVoices = 4;
    p.oscMix = 0.42f;
    p.osc2FineCents = 4.0f;
    p.pulseWidth = 0.47f;
    p.subLevel = 0.12f;
    p.noiseLevel = 0.01f;
    p.crossMod = 0.04f;
    p.cutoffHz = 5200.0f;
    p.resonance = 0.32f;
    p.filterDrive = 0.22f;
    p.filterShape = 0.18f;
    p.filterEnvAmount = 0.32f;
    p.filterKeyTrack = 0.45f;
    p.filterAttack = 0.006f;
    p.filterDecay = 0.18f;
    p.filterSustain = 0.42f;
    p.filterRelease = 0.11f;
    p.ampAttack = 0.004f;
    p.ampDecay = 0.12f;
    p.ampSustain = 0.82f;
    p.ampRelease = 0.10f;
    p.lfoRateHz = 4.7f;
    p.lfoPitchCents = 6.0f;
    p.lfoFilterOctaves = 0.08f;
    p.lfoPwm = 0.10f;
    p.drift = 0.24f;
    p.spread = 0.55f;
    p.glideSeconds = 0.0f;
    p.velocityAmount = 0.70f;
    p.chorusMix = 0.18f;
    p.chorusRateHz = 0.54f;
    p.outputGain = 0.68f;
    return p;
}

double estimateFrequency(double sampleRate, float pitchBend = 0.0f)
{
    mars::MarsEngine engine;
    engine.prepare(sampleRate, blockSize);
    auto p = basicParameters();
    p.osc1Wave = mars::OscillatorWave::Saw;
    p.oscMix = 0.0f;
    p.subLevel = 0.0f;
    p.noiseLevel = 0.0f;
    p.crossMod = 0.0f;
    p.cutoffHz = 18000.0f;
    p.resonance = 0.0f;
    p.filterDrive = 0.0f;
    p.filterEnvAmount = 0.0f;
    p.filterKeyTrack = 0.0f;
    p.ampAttack = 0.001f;
    p.ampDecay = 0.01f;
    p.ampSustain = 1.0f;
    p.lfoPitchCents = 0.0f;
    p.lfoFilterOctaves = 0.0f;
    p.drift = 0.0f;
    p.chorusMix = 0.0f;
    engine.setParameters(p);
    engine.setPitchBend(pitchBend);
    engine.noteOn(57, 0.8f); // A3, nominally 220 Hz.

    render(engine, static_cast<int>(0.20 * sampleRate));
    const int measurementSamples = static_cast<int>(0.45 * sampleRate);
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    float previous = 0.0f;
    int positiveCrossings = 0;
    int position = 0;
    while (position < measurementSamples)
    {
        const int count = std::min(blockSize, measurementSamples - position);
        engine.process(left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const float current = left[static_cast<std::size_t>(sample)];
            positiveCrossings += previous <= 0.0f && current > 0.0f ? 1 : 0;
            previous = current;
        }
        position += count;
    }
    return static_cast<double>(positiveCrossings) * sampleRate
         / static_cast<double>(measurementSamples);
}

mars::EngineParameters isolatedOscillatorParameters()
{
    auto p = basicParameters();
    p.osc1Wave = mars::OscillatorWave::Triangle;
    p.oscMix = 0.0f;
    p.subLevel = 0.0f;
    p.noiseLevel = 0.0f;
    p.crossMod = 0.0f;
    p.cutoffHz = 16000.0f;
    p.resonance = 0.0f;
    p.filterDrive = 0.0f;
    p.filterEnvAmount = 0.0f;
    p.filterKeyTrack = 0.0f;
    p.ampAttack = 0.001f;
    p.ampDecay = 0.01f;
    p.ampSustain = 1.0f;
    p.ampRelease = 0.03f;
    p.lfoPitchCents = 0.0f;
    p.lfoFilterOctaves = 0.0f;
    p.lfoPwm = 0.0f;
    p.drift = 0.0f;
    p.spread = 0.0f;
    p.velocityAmount = 0.0f;
    p.chorusMix = 0.0f;
    p.outputGain = 0.72f;
    return p;
}

double measureReleaseDuration(double sampleRate)
{
    mars::MarsEngine engine;
    engine.prepare(sampleRate, 32);
    auto p = isolatedOscillatorParameters();
    p.ampRelease = 0.080f;
    engine.setParameters(p);
    engine.noteOn(60, 0.8f);
    render(engine, static_cast<int>(0.10 * sampleRate));
    engine.noteOff(60);

    std::array<float, 32> left {};
    std::array<float, 32> right {};
    int elapsed = 0;
    const int maximum = static_cast<int>(sampleRate);
    while (engine.getActiveVoiceCount() > 0 && elapsed < maximum)
    {
        engine.process(left.data(), right.data(), static_cast<int>(left.size()));
        elapsed += static_cast<int>(left.size());
    }
    return static_cast<double>(elapsed) / sampleRate;
}

double averageRenderDifference(mars::MarsEngine& first, mars::MarsEngine& second,
                               int sampleCount)
{
    std::array<float, blockSize> firstLeft {};
    std::array<float, blockSize> firstRight {};
    std::array<float, blockSize> secondLeft {};
    std::array<float, blockSize> secondRight {};
    double difference = 0.0;
    int measured = 0;
    while (measured < sampleCount)
    {
        const int count = std::min(blockSize, sampleCount - measured);
        first.process(firstLeft.data(), firstRight.data(), count);
        second.process(secondLeft.data(), secondRight.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            difference += std::abs(static_cast<double>(
                firstLeft[static_cast<std::size_t>(sample)]
                - secondLeft[static_cast<std::size_t>(sample)]));
        }
        measured += count;
    }
    return difference / static_cast<double>(std::max(measured, 1));
}

float processLeftSample(mars::MarsEngine& engine)
{
    float left = 0.0f;
    float right = 0.0f;
    engine.process(&left, &right, 1);
    return left;
}

bool findFlatAudibleSample(mars::MarsEngine& engine, float minimumMagnitude,
                           float maximumStep, float& sample)
{
    float previous = processLeftSample(engine);
    for (int attempt = 0; attempt < 96000; ++attempt)
    {
        const float current = processLeftSample(engine);
        if (std::abs(current) >= minimumMagnitude
            && std::abs(current - previous) <= maximumStep)
        {
            sample = current;
            return true;
        }
        previous = current;
    }
    sample = previous;
    return false;
}

double maximumStepAfter(mars::MarsEngine& engine, float previous, int sampleCount)
{
    double maximum = 0.0;
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const float current = processLeftSample(engine);
        maximum = std::max(maximum,
                           std::abs(static_cast<double>(current - previous)));
        previous = current;
    }
    return maximum;
}

double orbitShapeDistance(float firstShape, float secondShape)
{
    constexpr double sampleRate = 48000.0;
    mars::MarsEngine first;
    mars::MarsEngine second;
    first.prepare(sampleRate, blockSize);
    second.prepare(sampleRate, blockSize);
    auto firstParameters = basicParameters();
    firstParameters.filterModel = mars::FilterModel::Orbit;
    firstParameters.filterShape = firstShape;
    firstParameters.cutoffHz = 1200.0f;
    firstParameters.filterEnvAmount = 0.0f;
    firstParameters.filterKeyTrack = 0.0f;
    firstParameters.resonance = 0.42f;
    firstParameters.chorusMix = 0.0f;
    auto secondParameters = firstParameters;
    secondParameters.filterShape = secondShape;
    first.setParameters(firstParameters);
    second.setParameters(secondParameters);
    first.noteOn(52, 0.82f);
    second.noteOn(52, 0.82f);
    render(first, static_cast<int>(0.20 * sampleRate));
    render(second, static_cast<int>(0.20 * sampleRate));

    std::array<float, blockSize> firstLeft {};
    std::array<float, blockSize> firstRight {};
    std::array<float, blockSize> secondLeft {};
    std::array<float, blockSize> secondRight {};
    double difference = 0.0;
    int samples = 0;
    for (int block = 0; block < 80; ++block)
    {
        first.process(firstLeft.data(), firstRight.data(), blockSize);
        second.process(secondLeft.data(), secondRight.data(), blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            difference += std::abs(static_cast<double>(
                firstLeft[static_cast<std::size_t>(sample)]
                - secondLeft[static_cast<std::size_t>(sample)]));
            ++samples;
        }
    }
    return difference / static_cast<double>(samples);
}

void testRenderMatrix()
{
    struct Scenario
    {
        mars::OscillatorWave wave;
        mars::FilterModel filter;
        mars::VoiceMode mode;
        int expectedVoices;
        const char* name;
    };
    constexpr std::array scenarios {
        Scenario { mars::OscillatorWave::Saw, mars::FilterModel::Ladder,
                   mars::VoiceMode::Poly, 1, "saw ladder" },
        Scenario { mars::OscillatorWave::Pulse, mars::FilterModel::Orbit,
                   mars::VoiceMode::Unison, 4, "pulse orbit unison" },
        Scenario { mars::OscillatorWave::Triangle, mars::FilterModel::Orbit,
                   mars::VoiceMode::Fifth, 2, "triangle fifth" }
    };
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0, 192000.0, 384000.0 };

    for (const double rate : sampleRates)
    {
        for (const auto& scenario : scenarios)
        {
            mars::MarsEngine engine;
            engine.prepare(rate, blockSize);
            auto p = basicParameters();
            p.osc1Wave = scenario.wave;
            p.osc2Wave = scenario.wave;
            p.filterModel = scenario.filter;
            p.voiceMode = scenario.mode;
            engine.setParameters(p);
            engine.noteOn(60, 0.82f);
            const auto held = render(engine, static_cast<int>(0.24 * rate));
            const std::string label = std::string(scenario.name) + " at "
                + std::to_string(static_cast<int>(rate)) + " Hz";
            expect(held.finite, label + " produced a NaN or infinity");
            expect(held.rms() > 1.0e-5, label + " was silent");
            expect(held.peak < 3.0, label + " escaped the output guardrail");
            expect(engine.getActiveVoiceCount() == scenario.expectedVoices,
                   label + " allocated an unexpected voice count");
            engine.noteOff(60);
            const auto released = render(engine, static_cast<int>(0.65 * rate));
            expect(released.finite && released.peak < 3.0,
                   label + " became unstable during release");
            expect(engine.getActiveVoiceCount() == 0,
                   label + " did not finish its release");
        }
    }
}

void testSustainAndRelease()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine engine;
    engine.prepare(rate, blockSize);
    auto p = basicParameters();
    p.ampRelease = 0.045f;
    p.chorusMix = 0.0f;
    engine.setParameters(p);
    engine.setSustainPedal(true);
    engine.noteOn(64, 0.8f);
    render(engine, static_cast<int>(0.12 * rate));
    engine.noteOff(64);
    const auto sustained = render(engine, static_cast<int>(0.22 * rate));
    expect(sustained.finite && sustained.rms() > 1.0e-5,
           "sustain pedal did not hold audible audio");
    expect(engine.getActiveVoiceCount() == 1,
           "sustain pedal released the voice early");
    engine.setSustainPedal(false);
    render(engine, static_cast<int>(0.35 * rate));
    expect(engine.getActiveVoiceCount() == 0,
           "lifting sustain did not release the held voice");

    engine.setSustainPedal(true);
    engine.noteOn(67, 0.8f);
    render(engine, static_cast<int>(0.08 * rate));
    engine.allNotesOff();
    const auto heldByAllNotesOff = render(engine, static_cast<int>(0.22 * rate));
    expect(heldByAllNotesOff.finite && heldByAllNotesOff.rms() > 1.0e-5,
           "allNotesOff did not leave a sustained held key audible");
    expect(engine.getActiveVoiceCount() == 1,
           "allNotesOff incorrectly overrode the sustain pedal");
    engine.setSustainPedal(false);
    render(engine, static_cast<int>(0.35 * rate));
    expect(engine.getActiveVoiceCount() == 0,
           "pedal-up did not finish an allNotesOff-sustained voice");
}

void testVoiceAllocation()
{
    mars::MarsEngine engine;
    engine.prepare(48000.0, blockSize);
    auto p = basicParameters();
    p.voiceMode = mars::VoiceMode::Poly;
    engine.setParameters(p);
    for (int note = 36; note < 56; ++note)
        engine.noteOn(note, 0.7f);
    render(engine, blockSize);
    expect(engine.getActiveVoiceCount() == 16,
           "poly mode did not enforce the 16-note logical limit");

    engine.reset();
    p.voiceMode = mars::VoiceMode::Unison;
    p.unisonVoices = 8;
    engine.setParameters(p);
    for (int note = 48; note < 53; ++note)
        engine.noteOn(note, 0.7f);
    render(engine, blockSize);
    expect(engine.getActiveVoiceCount() == 32,
           "8-layer unison did not use the 32-slot dynamic limit");
}

void testSampleRateLevelConsistency()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0, 192000.0, 384000.0 };
    std::array<double, sampleRates.size()> levels {};
    for (std::size_t index = 0; index < sampleRates.size(); ++index)
    {
        const double rate = sampleRates[index];
        mars::MarsEngine engine;
        engine.prepare(rate, blockSize);
        auto p = basicParameters();
        p.voiceMode = mars::VoiceMode::Poly;
        p.osc1Wave = mars::OscillatorWave::Saw;
        p.oscMix = 0.0f;
        p.subLevel = 0.0f;
        p.noiseLevel = 0.0f;
        p.crossMod = 0.0f;
        p.cutoffHz = 12000.0f;
        p.resonance = 0.05f;
        p.filterDrive = 0.0f;
        p.filterEnvAmount = 0.0f;
        p.filterKeyTrack = 0.0f;
        p.ampAttack = 0.001f;
        p.ampDecay = 0.01f;
        p.ampSustain = 1.0f;
        p.chorusMix = 0.0f;
        p.outputGain = 0.65f;
        engine.setParameters(p);
        engine.noteOn(57, 0.8f);
        const auto metrics = render(engine, static_cast<int>(0.55 * rate),
                                    static_cast<int>(0.15 * rate));
        expect(metrics.finite && metrics.rms() > 1.0e-5,
               "sample-rate consistency render failed");
        levels[index] = metrics.rms();
    }
    const auto [minimum, maximum] = std::minmax_element(levels.begin(), levels.end());
    expect(*maximum / std::max(*minimum, 1.0e-12) < 1.65,
           "held-note level changed by more than 4.35 dB across sample rates");
}

void testOversamplingPitchAndPitchBend()
{
    const double pitch48 = estimateFrequency(48000.0);
    const double pitch192 = estimateFrequency(192000.0);
    const double pitch384 = estimateFrequency(384000.0);
    expect(pitch48 > 205.0 && pitch48 < 235.0,
           "48 kHz oscillator frequency was outside the expected A3 range");
    expect(pitch192 > 205.0 && pitch192 < 235.0,
           "192 kHz 1x path changed oscillator pitch or timing");
    expect(std::abs(pitch192 / pitch48 - 1.0) < 0.025,
           "1x and 2x paths disagreed on oscillator pitch");
    expect(pitch384 > 205.0 && pitch384 < 235.0,
           "384 kHz host timebase changed oscillator pitch");
    expect(std::abs(pitch384 / pitch48 - 1.0) < 0.025,
           "384 kHz and 48 kHz paths disagreed on oscillator pitch");

    const double release48 = measureReleaseDuration(48000.0);
    const double release384 = measureReleaseDuration(384000.0);
    expect(release48 > 0.09 && release48 < 0.20,
           "48 kHz release timing was outside its expected range");
    expect(std::abs(release384 / release48 - 1.0) < 0.035,
           "384 kHz host timebase changed envelope timing");

    const double bent = estimateFrequency(48000.0, 1.0f);
    const double bendRatio = bent / pitch48;
    expect(bendRatio > 1.08 && bendRatio < 1.17,
           "full positive pitch bend was not close to +2 semitones");
}

void testParameterSanitisation()
{
    mars::MarsEngine engine;
    engine.prepare(48000.0, blockSize);
    auto p = basicParameters();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    p.cutoffHz = nan;
    p.resonance = std::numeric_limits<float>::infinity();
    p.filterDrive = -100.0f;
    p.pulseWidth = 100.0f;
    p.osc1Octave = -99;
    p.osc2Octave = 99;
    p.unisonVoices = 99;
    p.voiceMode = mars::VoiceMode::Unison;
    engine.setParameters(p);
    engine.setPitchBend(nan);
    engine.setModWheel(std::numeric_limits<float>::infinity());
    engine.noteOn(60, 0.8f);
    const auto metrics = render(engine, 24000);
    expect(metrics.finite && metrics.peak < 3.0,
           "non-finite or out-of-range parameters escaped sanitisation");
    expect(engine.getActiveVoiceCount() == 8,
           "unison voice count was not sanitised to the 2..8 contract");
}

void testOrbitShapeEndpoints()
{
    const double lowToBand = orbitShapeDistance(0.0f, 0.5f);
    const double bandToHigh = orbitShapeDistance(0.5f, 1.0f);
    const double lowToHigh = orbitShapeDistance(0.0f, 1.0f);
    expect(lowToBand > 1.0e-4,
           "Orbit low-pass and band-pass anchors produced the same response");
    expect(bandToHigh > 1.0e-4,
           "Orbit band-pass and high-pass anchors produced the same response");
    expect(lowToHigh > 1.0e-4,
           "Orbit shape endpoints produced the same response");
}

void testDeterminismAndOscillatorResponses()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine first;
    mars::MarsEngine second;
    first.prepare(rate, blockSize);
    second.prepare(rate, blockSize);
    auto p = basicParameters();
    p.lfoWave = mars::LfoWaveform::SampleHold;
    first.setParameters(p);
    second.setParameters(p);
    for (const int note : { 48, 55, 62 })
    {
        first.noteOn(note, 0.73f);
        second.noteOn(note, 0.73f);
    }
    const double deterministicDifference = averageRenderDifference(
        first, second, static_cast<int>(0.75 * rate));
    expect(deterministicDifference < 1.0e-9,
           "identical engines and MIDI did not render deterministically");

    const auto compareWaves = [](mars::OscillatorWave firstWave,
                                 mars::OscillatorWave secondWave)
    {
        mars::MarsEngine a;
        mars::MarsEngine b;
        a.prepare(rate, blockSize);
        b.prepare(rate, blockSize);
        auto firstParameters = isolatedOscillatorParameters();
        auto secondParameters = firstParameters;
        firstParameters.osc1Wave = firstWave;
        secondParameters.osc1Wave = secondWave;
        a.setParameters(firstParameters);
        b.setParameters(secondParameters);
        a.noteOn(60, 0.8f);
        b.noteOn(60, 0.8f);
        render(a, static_cast<int>(0.15 * rate));
        render(b, static_cast<int>(0.15 * rate));
        return averageRenderDifference(a, b, static_cast<int>(0.20 * rate));
    };

    expect(compareWaves(mars::OscillatorWave::Saw, mars::OscillatorWave::Pulse) > 1.0e-3,
           "saw and pulse oscillator selections produced the same response");
    expect(compareWaves(mars::OscillatorWave::Pulse, mars::OscillatorWave::Triangle) > 1.0e-3,
           "pulse and triangle oscillator selections produced the same response");
}

void testLongHeldTriangleStability()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine engine;
    engine.prepare(rate, blockSize);
    auto p = isolatedOscillatorParameters();
    p.filterModel = mars::FilterModel::Orbit;
    p.filterShape = 0.0f;
    engine.setParameters(p);
    engine.noteOn(60, 0.8f);
    render(engine, static_cast<int>(0.75 * rate));
    const auto early = render(engine, static_cast<int>(0.50 * rate));
    render(engine, static_cast<int>(30.0 * rate));
    const auto late = render(engine, static_cast<int>(0.50 * rate));

    expect(early.finite && late.finite && early.rms() > 1.0e-4,
           "long triangle stability render was invalid or silent");
    const double levelRatio = late.rms() / std::max(early.rms(), 1.0e-12);
    expect(levelRatio > 0.96 && levelRatio < 1.04,
           "long-held triangle level drifted as its integrator accumulated error");
    expect(late.peak < 0.95,
           "long-held triangle reached a rail or the output safety limiter");
    expect(std::abs(late.mean()) < 1.0e-3,
           "long-held triangle developed a DC offset");
}

void testClicklessRetriggerAndVoiceSteal()
{
    constexpr double rate = 48000.0;
    const auto runBoundary = [](bool forceAllocationSteal)
    {
        mars::MarsEngine engine;
        engine.prepare(rate, 64);
        auto p = isolatedOscillatorParameters();
        p.cutoffHz = 7000.0f;
        p.velocityAmount = 1.0f;
        engine.setParameters(p);
        engine.noteOn(60, 1.0f);
        if (forceAllocationSteal)
            for (int note = 61; note <= 75; ++note)
                engine.noteOn(note, 1.0e-6f);
        render(engine, static_cast<int>(0.20 * rate));

        float boundarySample = 0.0f;
        const bool found = findFlatAudibleSample(engine, 0.10f, 0.0025f,
                                                 boundarySample);
        if (forceAllocationSteal)
            engine.noteOn(76, 1.0e-6f);
        else
            engine.noteOn(60, 1.0f);
        return std::pair<bool, double> {
            found, maximumStepAfter(engine, boundarySample, 128)
        };
    };

    const auto retrigger = runBoundary(false);
    const auto steal = runBoundary(true);
    expect(retrigger.first, "could not locate an audible retrigger boundary");
    expect(steal.first, "could not locate an audible voice-steal boundary");
    expect(retrigger.second < 0.045,
           "same-note retrigger introduced an audible one-sample discontinuity");
    expect(steal.second < 0.045,
           "oldest-group voice stealing introduced an audible discontinuity");
}

void testClicklessFilterModelSwitch()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine engine;
    engine.prepare(rate, 64);
    auto p = isolatedOscillatorParameters();
    p.filterModel = mars::FilterModel::Ladder;
    p.cutoffHz = 900.0f;
    p.resonance = 0.48f;
    engine.setParameters(p);
    engine.noteOn(52, 0.9f);
    render(engine, static_cast<int>(0.25 * rate));

    float boundarySample = 0.0f;
    const bool found = findFlatAudibleSample(engine, 0.025f, 0.0015f,
                                             boundarySample);
    p.filterModel = mars::FilterModel::Orbit;
    p.filterShape = 1.0f;
    engine.setParameters(p);
    const double maximumStep = maximumStepAfter(engine, boundarySample, 384);
    expect(found, "could not locate an audible filter-switch boundary");
    expect(maximumStep < 0.025,
           "Ladder/Orbit switching introduced an audible discontinuity");
}

void testGlideAndModulationMeaningfulness()
{
    constexpr double rate = 48000.0;
    const auto earlyPitch = [](float glideSeconds)
    {
        mars::MarsEngine engine;
        engine.prepare(rate, blockSize);
        auto p = isolatedOscillatorParameters();
        p.osc1Wave = mars::OscillatorWave::Saw;
        p.glideSeconds = glideSeconds;
        p.ampRelease = 0.01f;
        engine.setParameters(p);
        engine.noteOn(48, 0.8f);
        render(engine, static_cast<int>(0.12 * rate));
        engine.noteOff(48);
        render(engine, static_cast<int>(0.08 * rate));
        engine.noteOn(72, 0.8f);

        constexpr double windowSeconds = 0.050;
        const int samples = static_cast<int>(windowSeconds * rate);
        float previous = 0.0f;
        int crossings = 0;
        for (int sample = 0; sample < samples; ++sample)
        {
            const float current = processLeftSample(engine);
            crossings += previous <= 0.0f && current > 0.0f ? 1 : 0;
            previous = current;
        }
        return static_cast<double>(crossings) / windowSeconds;
    };

    const double immediatePitch = earlyPitch(0.0f);
    const double glidingPitch = earlyPitch(0.35f);
    expect(immediatePitch > 430.0,
           "zero-glide note did not reach its target pitch immediately");
    expect(glidingPitch < 0.78 * immediatePitch,
           "glide control did not materially slow the pitch transition");

    mars::MarsEngine plain;
    mars::MarsEngine modulated;
    plain.prepare(rate, blockSize);
    modulated.prepare(rate, blockSize);
    auto p = isolatedOscillatorParameters();
    p.osc1Wave = mars::OscillatorWave::Saw;
    p.lfoWave = mars::LfoWaveform::Sine;
    p.lfoRateHz = 5.0f;
    plain.setParameters(p);
    modulated.setParameters(p);
    modulated.setModWheel(1.0f);
    plain.noteOn(60, 0.8f);
    modulated.noteOn(60, 0.8f);
    render(plain, static_cast<int>(0.25 * rate));
    render(modulated, static_cast<int>(0.25 * rate));
    const double modulationDifference = averageRenderDifference(
        plain, modulated, static_cast<int>(0.25 * rate));
    expect(modulationDifference > 1.0e-3,
           "mod wheel did not materially affect its fixed LFO routes");
}

void testExtremeAutomationStability()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine engine;
    engine.prepare(rate, 64);
    auto p = basicParameters();
    p.voiceMode = mars::VoiceMode::Fifth;
    p.ampRelease = 0.04f;
    engine.setParameters(p);
    for (int note = 40; note < 52; ++note)
        engine.noteOn(note, 0.92f);

    std::array<float, 64> left {};
    std::array<float, 64> right {};
    Metrics metrics;
    for (int block = 0; block < 360; ++block)
    {
        const bool high = (block & 1) != 0;
        p.osc1Wave = static_cast<mars::OscillatorWave>(block % 3);
        p.osc2Wave = static_cast<mars::OscillatorWave>((block + 1) % 3);
        p.filterModel = high ? mars::FilterModel::Orbit : mars::FilterModel::Ladder;
        p.lfoWave = static_cast<mars::LfoWaveform>(block % 3);
        p.cutoffHz = high ? 20000.0f : 20.0f;
        p.resonance = high ? 1.0f : 0.0f;
        p.filterDrive = high ? 1.0f : 0.0f;
        p.filterShape = high ? 1.0f : 0.0f;
        p.filterEnvAmount = high ? 1.0f : -1.0f;
        p.crossMod = high ? 1.0f : 0.0f;
        p.pulseWidth = high ? 0.97f : 0.03f;
        p.lfoFilterOctaves = high ? 8.0f : -8.0f;
        p.chorusMix = high ? 1.0f : 0.0f;
        engine.setParameters(p);
        engine.setPitchBend(high ? 1.0f : -1.0f);
        engine.setModWheel(high ? 1.0f : 0.0f);
        engine.process(left.data(), right.data(), 64);
        for (int i = 0; i < 64; ++i)
        {
            metrics.add(left[static_cast<std::size_t>(i)]);
            metrics.add(right[static_cast<std::size_t>(i)]);
        }
    }
    expect(metrics.finite, "extreme automation produced a NaN or infinity");
    expect(metrics.peak < 4.0, "extreme automation escaped the bounded output stage");
    expect(engine.getActiveVoiceCount() <= 32,
           "extreme automation exceeded the render-slot limit");
    engine.allNotesOff();
    render(engine, static_cast<int>(0.8 * rate));
    expect(engine.getActiveVoiceCount() == 0,
           "automated voices did not finish releasing");
}

void testCpuRegression()
{
    constexpr double rate = 96000.0;
    constexpr double renderSeconds = 0.50;
    mars::MarsEngine engine;
    engine.prepare(rate, blockSize);
    auto p = basicParameters();
    p.voiceMode = mars::VoiceMode::Fifth;
    p.filterModel = mars::FilterModel::Orbit;
    p.filterDrive = 0.9f;
    p.resonance = 0.85f;
    p.chorusMix = 0.7f;
    engine.setParameters(p);
    for (int note = 36; note < 52; ++note)
        engine.noteOn(note, 0.85f);
    expect(engine.getActiveVoiceCount() == 32,
           "CPU regression did not fill all 32 render slots");

    const auto start = std::chrono::steady_clock::now();
    const auto metrics = render(engine, static_cast<int>(renderSeconds * rate));
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    const double ratio = elapsed / renderSeconds;
    std::cout << std::fixed << std::setprecision(3)
              << "32-slot 96 kHz/2x render: " << elapsed << " s ("
              << ratio << "x real time)\n";
    expect(metrics.finite && metrics.rms() > 1.0e-6,
           "CPU regression render was invalid or silent");
    const double maximumRatio = sanitizerBuild ? 40.0 : 8.0;
    expect(ratio < maximumRatio,
           sanitizerBuild
               ? "sanitized 32-slot nonlinear render exceeded its diagnostic guardrail"
               : "Release 32-slot nonlinear render exceeded the 8x real-time guardrail");
}
} // namespace

int main()
{
    testRenderMatrix();
    testSustainAndRelease();
    testVoiceAllocation();
    testSampleRateLevelConsistency();
    testOversamplingPitchAndPitchBend();
    testParameterSanitisation();
    testOrbitShapeEndpoints();
    testDeterminismAndOscillatorResponses();
    testLongHeldTriangleStability();
    testClicklessRetriggerAndVoiceSteal();
    testClicklessFilterModelSwitch();
    testGlideAndModulationMeaningfulness();
    testExtremeAutomationStability();
    testCpuRegression();

    if (failures != 0)
    {
        std::cerr << failures << " Mars DSP check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Mars DSP checks passed.\n";
    return EXIT_SUCCESS;
}
