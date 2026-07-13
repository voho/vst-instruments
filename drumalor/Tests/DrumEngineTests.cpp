#include "DSP/DrumEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace
{
constexpr int defaultBlockSize = 257;
int failureCount = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct RenderMetrics
{
    double sumOfSquares { 0.0 };
    double peak { 0.0 };
    std::size_t sampleCount { 0 };
    std::size_t nonZeroCount { 0 };
    bool finite { true };

    [[nodiscard]] double rms() const noexcept
    {
        return sampleCount == 0 ? 0.0
                                : std::sqrt (sumOfSquares / static_cast<double> (sampleCount));
    }
};

RenderMetrics renderMetrics (drumalor::DrumEngine& engine, int numSamples,
                             int blockSize = defaultBlockSize)
{
    std::vector<float> left (static_cast<std::size_t> (blockSize));
    std::vector<float> right (static_cast<std::size_t> (blockSize));
    RenderMetrics metrics;
    for (int rendered = 0; rendered < numSamples;)
    {
        const int count = std::min (blockSize, numSamples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            for (const float value : { left[static_cast<std::size_t> (sample)],
                                       right[static_cast<std::size_t> (sample)] })
            {
                metrics.finite = metrics.finite && std::isfinite (value);
                if (std::isfinite (value))
                {
                    const double magnitude = std::abs (static_cast<double> (value));
                    metrics.peak = std::max (metrics.peak, magnitude);
                    metrics.sumOfSquares += static_cast<double> (value) * value;
                    if (magnitude > 1.0e-9)
                        ++metrics.nonZeroCount;
                }
                ++metrics.sampleCount;
            }
        }
        rendered += count;
    }
    return metrics;
}

std::vector<float> renderInterleaved (drumalor::DrumEngine& engine, int numSamples,
                                      int blockSize)
{
    std::vector<float> result (static_cast<std::size_t> (numSamples) * 2u);
    std::vector<float> left (static_cast<std::size_t> (blockSize));
    std::vector<float> right (static_cast<std::size_t> (blockSize));
    for (int rendered = 0; rendered < numSamples;)
    {
        const int count = std::min (blockSize, numSamples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto target = static_cast<std::size_t> (rendered + sample) * 2u;
            result[target] = left[static_cast<std::size_t> (sample)];
            result[target + 1u] = right[static_cast<std::size_t> (sample)];
        }
        rendered += count;
    }
    return result;
}

double meanAbsoluteDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    const auto count = std::min (a.size(), b.size());
    double difference = 0.0;
    for (std::size_t index = 0; index < count; ++index)
        difference += std::abs (static_cast<double> (a[index]) - b[index]);
    return count == 0 ? 0.0 : difference / static_cast<double> (count);
}

double meanAbsoluteMagnitude (const std::vector<float>& values)
{
    double magnitude = 0.0;
    for (const float value : values)
        magnitude += std::abs (static_cast<double> (value));
    return values.empty() ? 0.0 : magnitude / static_cast<double> (values.size());
}

double decibelSpread (const std::array<double, 4>& values)
{
    const auto [minimum, maximum] = std::minmax_element (values.begin(), values.end());
    return 20.0 * std::log10 (*maximum / std::max (1.0e-12, *minimum));
}

int renderUntilInactive (drumalor::DrumEngine& engine, int maximumSamples,
                         int blockSize = defaultBlockSize)
{
    int rendered = 0;
    while (engine.getActiveVoiceCount() > 0 && rendered < maximumSamples)
    {
        const int count = std::min (blockSize, maximumSamples - rendered);
        renderMetrics (engine, count, blockSize);
        rendered += count;
    }
    return rendered;
}

void testMetadataAndMidiMapping()
{
    struct MidiMapping
    {
        int note;
        drumalor::Instrument instrument;
    };
    constexpr std::array expectedMappings {
        MidiMapping { 35, drumalor::Instrument::Kick },
        MidiMapping { 36, drumalor::Instrument::Kick },
        MidiMapping { 37, drumalor::Instrument::Perc2 },
        MidiMapping { 38, drumalor::Instrument::Snare },
        MidiMapping { 39, drumalor::Instrument::Clap },
        MidiMapping { 40, drumalor::Instrument::Snare },
        MidiMapping { 41, drumalor::Instrument::LowTom },
        MidiMapping { 42, drumalor::Instrument::ClosedHat },
        MidiMapping { 43, drumalor::Instrument::LowTom },
        MidiMapping { 44, drumalor::Instrument::ClosedHat },
        MidiMapping { 45, drumalor::Instrument::LowTom },
        MidiMapping { 46, drumalor::Instrument::OpenHat },
        MidiMapping { 47, drumalor::Instrument::MidTom },
        MidiMapping { 48, drumalor::Instrument::MidTom },
        MidiMapping { 49, drumalor::Instrument::Crash },
        MidiMapping { 50, drumalor::Instrument::HighTom },
        MidiMapping { 51, drumalor::Instrument::Ride },
        MidiMapping { 53, drumalor::Instrument::Ride },
        MidiMapping { 56, drumalor::Instrument::Perc1 },
        MidiMapping { 57, drumalor::Instrument::Crash },
        MidiMapping { 59, drumalor::Instrument::Ride },
        MidiMapping { 70, drumalor::Instrument::Shaker },
        MidiMapping { 75, drumalor::Instrument::Perc2 },
        MidiMapping { 76, drumalor::Instrument::Perc2 },
        MidiMapping { 77, drumalor::Instrument::Perc2 },
        MidiMapping { 82, drumalor::Instrument::Shaker },
    };

    std::set<std::string> slugs;
    std::set<int> primaryNotes;
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto& item = drumalor::getInstrumentMetadata (instrument);
        expect (item.instrument == instrument, "metadata instrument order changed");
        expect (! drumalor::getInstrumentDisplayName (instrument).empty(), "empty display name");
        expect (! drumalor::getInstrumentSlug (instrument).empty(), "empty instrument slug");
        expect (! drumalor::getCharacterALabel (instrument).empty(), "empty character A label");
        expect (! drumalor::getCharacterBLabel (instrument).empty(), "empty character B label");
        expect (slugs.insert (std::string (item.slug)).second, "duplicate instrument slug");
        expect (primaryNotes.insert (item.standardMidiNote).second, "duplicate primary MIDI note");
        const auto mapped = drumalor::instrumentForMidiNote (item.standardMidiNote);
        expect (mapped.has_value() && *mapped == instrument,
                std::string (item.displayName) + " primary MIDI note maps incorrectly");
    }

    for (int midiNote = 0; midiNote <= 127; ++midiNote)
    {
        const auto expected = std::find_if (
            expectedMappings.begin(), expectedMappings.end(),
            [midiNote] (const MidiMapping& mapping) { return mapping.note == midiNote; });
        const auto actual = drumalor::instrumentForMidiNote (midiNote);
        if (expected == expectedMappings.end())
        {
            expect (! actual.has_value(),
                    "unexpected MIDI mapping for note " + std::to_string (midiNote));
        }
        else
        {
            expect (actual.has_value() && *actual == expected->instrument,
                    "incorrect MIDI mapping for note " + std::to_string (midiNote));
        }
    }
    expect (! drumalor::instrumentForMidiNote (-1).has_value(), "negative MIDI note was accepted");
    expect (! drumalor::instrumentForMidiNote (128).has_value(), "out-of-range MIDI note was accepted");
}

void testEveryInstrumentAndSampleRate()
{
    constexpr std::array sampleRates { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 };
    for (const double sampleRate : sampleRates)
    {
        for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
        {
            const auto instrument = static_cast<drumalor::Instrument> (index);
            drumalor::DrumEngine engine;
            engine.prepare (sampleRate, defaultBlockSize);
            engine.setInstrumentParameters (instrument,
                                             drumalor::getInstrumentMetadata (instrument).defaultParameters);
            engine.trigger (instrument, 0.82f);
            const auto metrics = renderMetrics (engine, static_cast<int> (0.16 * sampleRate));
            const std::string label = std::string (drumalor::getInstrumentDisplayName (instrument))
                + " at " + std::to_string (static_cast<int> (sampleRate)) + " Hz";
            expect (metrics.finite, label + " produced NaN or infinity");
            expect (metrics.nonZeroCount > 10, label + " was silent");
            expect (metrics.rms() > 1.0e-7, label + " RMS was effectively silent");
            expect (metrics.peak <= 1.001, label + " exceeded the output safety bound");
        }
    }
}

void testModalSampleRateConsistency()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };
    constexpr std::array modalInstruments {
        drumalor::Instrument::Ride,
        drumalor::Instrument::Crash,
        drumalor::Instrument::Perc2
    };

    for (const auto instrument : modalInstruments)
    {
        std::array<double, sampleRates.size()> levels {};
        for (std::size_t rateIndex = 0; rateIndex < sampleRates.size(); ++rateIndex)
        {
            const auto rate = sampleRates[rateIndex];
            drumalor::DrumEngine engine;
            engine.prepare (rate, defaultBlockSize);
            engine.setInstrumentParameters (
                instrument, drumalor::getInstrumentMetadata (instrument).defaultParameters);

            // Multiple deterministic strikes exercise several PRNG streams,
            // while the low velocity keeps the output stage effectively linear.
            for (int hit = 0; hit < 4; ++hit)
                engine.trigger (instrument, 0.20f);

            const auto metrics = renderMetrics (
                engine, static_cast<int> (0.5 * rate), defaultBlockSize);
            expect (metrics.finite && metrics.rms() > 1.0e-7,
                    std::string (drumalor::getInstrumentDisplayName (instrument))
                        + " modal consistency probe was invalid or silent");
            levels[rateIndex] = metrics.rms();
        }

        expect (decibelSpread (levels) < 1.25,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " modal level changed by more than 1.25 dB across sample rates");
    }

    // Perc 2 can excite its modes with a single impulse. With the stochastic
    // click removed, this directly guards the resonator normalization itself.
    std::array<double, sampleRates.size()> impulseLevels {};
    for (std::size_t rateIndex = 0; rateIndex < sampleRates.size(); ++rateIndex)
    {
        const auto rate = sampleRates[rateIndex];
        drumalor::DrumEngine engine;
        engine.prepare (rate, defaultBlockSize);
        auto parameters = drumalor::getInstrumentMetadata (
            drumalor::Instrument::Perc2).defaultParameters;
        parameters.characterB = 0.0f;
        engine.setInstrumentParameters (drumalor::Instrument::Perc2, parameters);
        engine.trigger (drumalor::Instrument::Perc2, 0.85f);
        impulseLevels[rateIndex] = renderMetrics (
            engine, static_cast<int> (0.5 * rate), defaultBlockSize).rms();
    }
    expect (decibelSpread (impulseLevels) < 0.25,
            "Perc 2 resonator impulse changed by more than 0.25 dB across sample rates");
}

void testTailsTerminate()
{
    constexpr double sampleRate = 48000.0;
    const int limit = static_cast<int> (sampleRate * drumalor::maximumTailSeconds);
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        drumalor::DrumEngine engine;
        engine.prepare (sampleRate, defaultBlockSize);
        auto parameters = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        parameters.decay = 1.0f;
        engine.setInstrumentParameters (instrument, parameters);
        engine.trigger (instrument, 0.8f);
        const auto boundaryAudio = renderMetrics (engine, limit, 389);
        expect (engine.getActiveVoiceCount() == 0,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " remained active at the advertised tail boundary");
        expect (boundaryAudio.finite,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " produced invalid audio while approaching the tail boundary");

        const auto afterTail = renderMetrics (engine, 4096, 251);
        expect (afterTail.finite && afterTail.peak < 1.0e-7 && afterTail.rms() < 1.0e-8,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " emitted non-silent audio after the advertised tail boundary");
    }
}

void testHatChokeAndPanic()
{
    constexpr double sampleRate = 48000.0;
    drumalor::DrumEngine engine;
    engine.prepare (sampleRate, defaultBlockSize);
    auto openParameters = drumalor::getInstrumentMetadata (drumalor::Instrument::OpenHat).defaultParameters;
    openParameters.decay = 1.0f;
    engine.setInstrumentParameters (drumalor::Instrument::OpenHat, openParameters);
    engine.trigger (drumalor::Instrument::OpenHat, 1.0f);
    renderMetrics (engine, static_cast<int> (0.025 * sampleRate));
    expect (engine.getActiveVoiceCount() == 1, "open hat stopped before choke test");

    engine.trigger (drumalor::Instrument::ClosedHat, 0.8f);
    expect (engine.getActiveVoiceCount() == 2, "hat retrigger did not create the new voice");
    renderMetrics (engine, static_cast<int> (0.010 * sampleRate));
    expect (engine.getActiveVoiceCount() == 1, "closed hat did not choke open hat quickly");

    engine.trigger (drumalor::Instrument::Crash, 0.9f);
    engine.trigger (drumalor::Instrument::Ride, 0.9f);
    engine.allSoundsOff();
    const auto panicTail = renderMetrics (engine, static_cast<int> (0.015 * sampleRate));
    expect (panicTail.finite, "allSoundsOff produced invalid audio");
    expect (engine.getActiveVoiceCount() == 0, "allSoundsOff did not clear voices");
}

void testDeterminismAndBlockPartitioning()
{
    constexpr int samples = 24000;
    drumalor::DrumEngine first;
    drumalor::DrumEngine second;
    first.prepare (48000.0, 512);
    second.prepare (48000.0, 512);
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        first.trigger (instrument, 0.35f + 0.04f * static_cast<float> (index));
        second.trigger (instrument, 0.35f + 0.04f * static_cast<float> (index));
    }
    const auto oneSampleBlocks = renderInterleaved (first, samples, 1);
    const auto irregularBlocks = renderInterleaved (second, samples, 383);
    expect (oneSampleBlocks == irregularBlocks,
            "render changed when the same stream was partitioned into different blocks");

    drumalor::DrumEngine resetEngine;
    resetEngine.prepare (48000.0, 512);
    auto shakerParameters = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Shaker).defaultParameters;
    shakerParameters.decay = 0.0f;
    resetEngine.setInstrumentParameters (drumalor::Instrument::Shaker, shakerParameters);

    resetEngine.trigger (drumalor::Instrument::Shaker, 0.81f);
    const auto firstHit = renderInterleaved (resetEngine, 4096, 127);
    renderUntilInactive (resetEngine, static_cast<int> (48000.0 * drumalor::maximumTailSeconds));
    renderMetrics (resetEngine, 4096);

    resetEngine.trigger (drumalor::Instrument::Shaker, 0.81f);
    const auto successiveHit = renderInterleaved (resetEngine, 4096, 127);
    const double successiveDifference = meanAbsoluteDifference (firstHit, successiveHit);
    expect (successiveDifference > 0.02 * meanAbsoluteMagnitude (firstHit),
            "successive same-instrument hits reused the same noise/phase sequence");

    resetEngine.reset();
    resetEngine.trigger (drumalor::Instrument::Shaker, 0.81f);
    const auto postResetHit = renderInterleaved (resetEngine, 4096, 127);
    expect (firstHit == postResetHit,
            "reset did not restore the initial deterministic noise and phase state");
}

void testLowFrequencyTailAndVoiceStealing()
{
    constexpr double sampleRate = 48000.0;
    constexpr int recentWindowSamples = 2400; // 50 ms, longer than a 13 Hz peak interval.
    drumalor::DrumEngine lowKick;
    lowKick.prepare (sampleRate, 1);
    auto kickParameters = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Kick).defaultParameters;
    kickParameters.pitch = -24.0f;
    kickParameters.decay = 1.0f;
    lowKick.setInstrumentParameters (drumalor::Instrument::Kick, kickParameters);
    lowKick.trigger (drumalor::Instrument::Kick, 1.0f);

    std::array<float, recentWindowSamples> recentMagnitudes {};
    std::size_t recentIndex = 0;
    float finalLeft = 0.0f;
    float finalRight = 0.0f;
    int rendered = 0;
    const int hardLimit = static_cast<int> (sampleRate * drumalor::maximumTailSeconds);
    while (lowKick.getActiveVoiceCount() > 0 && rendered < hardLimit)
    {
        lowKick.process (&finalLeft, &finalRight, 1);
        recentMagnitudes[recentIndex] = std::max (std::abs (finalLeft), std::abs (finalRight));
        recentIndex = (recentIndex + 1u) % recentMagnitudes.size();
        ++rendered;
    }

    const auto recentPeak = *std::max_element (
        recentMagnitudes.begin(), recentMagnitudes.end());
    float afterLeft = 0.0f;
    float afterRight = 0.0f;
    lowKick.process (&afterLeft, &afterRight, 1);
    expect (recentPeak < 1.2e-5f,
            "minimum-pitch Kick was cleared while an audible low-frequency peak remained");
    expect (std::max (std::abs (afterLeft - finalLeft), std::abs (afterRight - finalRight))
                < 1.0e-5f,
            "minimum-pitch Kick tail ended with a discontinuity");

    // Twelve same-sample replacements exceed the old four-slot retirement
    // edge case. At very low levels the output stage is effectively linear,
    // so old + new renders should equal the saturated render on its first sample.
    drumalor::DrumEngine oldOnly;
    drumalor::DrumEngine saturated;
    drumalor::DrumEngine newOnly;
    for (auto* engine : { &oldOnly, &saturated, &newOnly })
        engine->prepare (sampleRate, 256);

    auto crashParameters = drumalor::getInstrumentMetadata (
        drumalor::Instrument::Crash).defaultParameters;
    crashParameters.decay = 1.0f;
    oldOnly.setInstrumentParameters (drumalor::Instrument::Crash, crashParameters);
    saturated.setInstrumentParameters (drumalor::Instrument::Crash, crashParameters);
    for (int voice = 0; voice < 64; ++voice)
    {
        oldOnly.trigger (drumalor::Instrument::Crash, 0.005f);
        saturated.trigger (drumalor::Instrument::Crash, 0.005f);
    }

    std::array<float, 256> scratchLeft {};
    std::array<float, 256> scratchRight {};
    oldOnly.process (scratchLeft.data(), scratchRight.data(), 256);
    saturated.process (scratchLeft.data(), scratchRight.data(), 256);
    newOnly.process (scratchLeft.data(), scratchRight.data(), 256);

    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        if (instrument == drumalor::Instrument::Crash)
            continue;
        saturated.trigger (instrument, 0.010f);
        newOnly.trigger (instrument, 0.010f);
    }

    float oldLeft = 0.0f;
    float oldRight = 0.0f;
    float saturatedLeft = 0.0f;
    float saturatedRight = 0.0f;
    float newLeft = 0.0f;
    float newRight = 0.0f;
    oldOnly.process (&oldLeft, &oldRight, 1);
    saturated.process (&saturatedLeft, &saturatedRight, 1);
    newOnly.process (&newLeft, &newRight, 1);
    expect (std::max (std::abs (saturatedLeft - oldLeft - newLeft),
                      std::abs (saturatedRight - oldRight - newRight)) < 1.0e-6f,
            "same-sample voice stealing dropped an audible tail without a fade");

    for (int trigger = 0; trigger < 128; ++trigger)
        saturated.trigger (static_cast<drumalor::Instrument> (
                               static_cast<std::size_t> (trigger) % drumalor::instrumentCount),
                           0.5f);
    expect (saturated.getActiveVoiceCount() == 64,
            "same-sample trigger burst exceeded the fixed primary voice capacity");
    const auto burstMetrics = renderMetrics (saturated, 1024, 127);
    expect (burstMetrics.finite && burstMetrics.peak <= 1.001,
            "same-sample trigger burst produced unsafe audio");
}

std::vector<float> renderWithParameters (drumalor::Instrument instrument,
                                         drumalor::InstrumentParameters parameters,
                                         int samples = 7200)
{
    drumalor::DrumEngine engine;
    engine.prepare (48000.0, defaultBlockSize);
    engine.setInstrumentParameters (instrument, parameters);
    engine.trigger (instrument, 0.85f);
    return renderInterleaved (engine, samples, defaultBlockSize);
}

void testParameterInfluence()
{
    for (std::size_t index = 0; index < drumalor::instrumentCount; ++index)
    {
        const auto instrument = static_cast<drumalor::Instrument> (index);
        const auto defaults = drumalor::getInstrumentMetadata (instrument).defaultParameters;
        auto lowA = defaults;
        auto highA = defaults;
        lowA.characterA = 0.0f;
        highA.characterA = 1.0f;
        const double characterADifference = meanAbsoluteDifference (
            renderWithParameters (instrument, lowA), renderWithParameters (instrument, highA));
        expect (characterADifference > 1.0e-7,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " character A did not affect its sound");

        auto lowB = defaults;
        auto highB = defaults;
        lowB.characterB = 0.0f;
        highB.characterB = 1.0f;
        const double characterBDifference = meanAbsoluteDifference (
            renderWithParameters (instrument, lowB), renderWithParameters (instrument, highB));
        expect (characterBDifference > 1.0e-7,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " character B did not affect its sound");

        auto lowPitch = defaults;
        auto highPitch = defaults;
        lowPitch.pitch = -12.0f;
        highPitch.pitch = 12.0f;
        const auto lowPitchRender = renderWithParameters (instrument, lowPitch, 14400);
        const auto highPitchRender = renderWithParameters (instrument, highPitch, 14400);
        const double pitchDifference = meanAbsoluteDifference (lowPitchRender, highPitchRender);
        const double pitchReference = std::max (meanAbsoluteMagnitude (lowPitchRender),
                                                meanAbsoluteMagnitude (highPitchRender));
        expect (pitchDifference > std::max (1.0e-7, 0.02 * pitchReference),
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " pitch endpoints produced indistinguishable renders");

        auto shortDecay = defaults;
        auto longDecay = defaults;
        shortDecay.decay = 0.0f;
        longDecay.decay = 1.0f;
        drumalor::DrumEngine shortEngine;
        drumalor::DrumEngine longEngine;
        shortEngine.prepare (48000.0, defaultBlockSize);
        longEngine.prepare (48000.0, defaultBlockSize);
        shortEngine.setInstrumentParameters (instrument, shortDecay);
        longEngine.setInstrumentParameters (instrument, longDecay);
        shortEngine.trigger (instrument, 0.9f);
        longEngine.trigger (instrument, 0.9f);
        constexpr int tailLimit = static_cast<int> (
            48000.0 * drumalor::maximumTailSeconds);
        const int shortTailSamples = renderUntilInactive (shortEngine, tailLimit, 127);
        const int longTailSamples = renderUntilInactive (longEngine, tailLimit, 127);
        expect (longTailSamples > shortTailSamples + 4800
                    && longTailSamples > shortTailSamples * 3 / 2,
                std::string (drumalor::getInstrumentDisplayName (instrument))
                    + " decay did not materially extend its active tail");
    }
}

void testInvalidValuesAndStressPerformance()
{
    drumalor::DrumEngine engine;
    engine.prepare (96000.0, defaultBlockSize);
    drumalor::InstrumentParameters invalid;
    invalid.characterA = std::numeric_limits<float>::quiet_NaN();
    invalid.characterB = std::numeric_limits<float>::infinity();
    invalid.pitch = -std::numeric_limits<float>::infinity();
    invalid.decay = std::numeric_limits<float>::quiet_NaN();
    engine.setInstrumentParameters (drumalor::Instrument::Snare, invalid);
    engine.setOutputGain (std::numeric_limits<float>::quiet_NaN());
    expect (! engine.triggerMidi (127, 1.0f), "unmapped MIDI note reported as handled");
    expect (engine.triggerMidi (38, 1.0f), "mapped snare note was rejected");
    const auto invalidMetrics = renderMetrics (engine, 4096);
    expect (invalidMetrics.finite && invalidMetrics.nonZeroCount > 0,
            "invalid parameter sanitization did not produce safe audio");

    engine.reset();
    constexpr int samples = 96000;
    constexpr std::array instruments {
        drumalor::Instrument::Kick, drumalor::Instrument::Snare,
        drumalor::Instrument::Clap, drumalor::Instrument::ClosedHat,
        drumalor::Instrument::OpenHat, drumalor::Instrument::Ride,
        drumalor::Instrument::Crash, drumalor::Instrument::LowTom,
        drumalor::Instrument::MidTom, drumalor::Instrument::HighTom,
        drumalor::Instrument::Shaker, drumalor::Instrument::Perc1,
        drumalor::Instrument::Perc2
    };
    std::array<float, defaultBlockSize> left {};
    std::array<float, defaultBlockSize> right {};
    bool finite = true;
    double peak = 0.0;
    const auto start = std::chrono::steady_clock::now();
    for (int rendered = 0, triggerIndex = 0; rendered < samples;)
    {
        engine.trigger (instruments[static_cast<std::size_t> (triggerIndex) % instruments.size()],
                        0.35f + 0.05f * static_cast<float> (triggerIndex % 12));
        ++triggerIndex;
        const int count = std::min (defaultBlockSize, samples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            finite = finite && std::isfinite (left[static_cast<std::size_t> (sample)])
                            && std::isfinite (right[static_cast<std::size_t> (sample)]);
            peak = std::max ({ peak,
                              std::abs (static_cast<double> (left[static_cast<std::size_t> (sample)])),
                              std::abs (static_cast<double> (right[static_cast<std::size_t> (sample)])) });
        }
        rendered += count;
    }
    const double elapsed = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - start).count();
    expect (finite && peak <= 1.001, "dense stress render produced unsafe audio");
    expect (engine.getActiveVoiceCount() <= 64, "voice allocation exceeded its fixed capacity");
    expect (elapsed < 20.0, "one-second stress render exceeded the generous performance guardrail");
}
} // namespace

int main()
{
    testMetadataAndMidiMapping();
    testEveryInstrumentAndSampleRate();
    testModalSampleRateConsistency();
    testTailsTerminate();
    testHatChokeAndPanic();
    testDeterminismAndBlockPartitioning();
    testLowFrequencyTailAndVoiceStealing();
    testParameterInfluence();
    testInvalidValuesAndStressPerformance();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Drumalor DSP test(s) failed\n";
        return 1;
    }
    std::cout << "All Drumalor DSP tests passed\n";
    return 0;
}
