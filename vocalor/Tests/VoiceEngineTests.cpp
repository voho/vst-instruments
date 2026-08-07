#include "DSP/VocalorMath.h"
#include "DSP/VoiceEngine.h"

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
#include <string_view>
#include <vector>

namespace vocalor
{
struct VoiceEngineTestAccess
{
    static std::array<float, 4> smoothedParameters(const VoiceEngine& engine) noexcept
    {
        return { engine.smoothedRoom_, engine.smoothedGain_,
                 engine.smoothedBreath_, engine.smoothedTension_ };
    }

    static std::array<float, kFormantCount> chunkFormants(const VoiceEngine& engine) noexcept
    {
        std::array<float, kFormantCount> result {};
        for (int i = 0; i < kFormantCount; ++i)
            result[static_cast<std::size_t>(i)] = engine.chunkFormantHz_[static_cast<std::size_t>(i)];
        return result;
    }

    /** Smallest non-zero magnitude anywhere in the recursive state. Anything
        below ~1.2e-38 would be a denormal and would stall unprotected hosts. */
    static float smallestNonZeroState(const VoiceEngine& engine) noexcept
    {
        float smallest = std::numeric_limits<float>::max();
        const auto consider = [&smallest](float value) noexcept
        {
            const float magnitude = std::abs(value);
            if (magnitude > 0.0f)
                smallest = std::min(smallest, magnitude);
        };

        for (const auto& voice : engine.voices_)
        {
            for (const auto& resonator : voice.tract)
            {
                consider(resonator.y1);
                consider(resonator.y2);
            }
            for (const auto& resonator : voice.early)
            {
                consider(resonator.y1);
                consider(resonator.y2);
            }
            consider(voice.sourceTilt);
        }
        for (const auto value : engine.roomLeft_)
            consider(value);
        for (const auto value : engine.roomRight_)
            consider(value);
        consider(engine.roomDampingLeft_);
        consider(engine.roomDampingRight_);
        consider(engine.roomLowCutLeft_);
        consider(engine.roomLowCutRight_);

        return smallest == std::numeric_limits<float>::max() ? 0.0f : smallest;
    }

    static std::array<float, kFormantCount> chunkBandwidths(const VoiceEngine& engine) noexcept
    {
        std::array<float, kFormantCount> result {};
        for (int i = 0; i < kFormantCount; ++i)
            result[static_cast<std::size_t>(i)] = engine.chunkBandwidth_[static_cast<std::size_t>(i)];
        return result;
    }

    static float frequencyForRoot(const VoiceEngine& engine, int rootMidi) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && voice.rootMidi == rootMidi)
                return voice.phaseIncrement * static_cast<float>(engine.sampleRate_);
        return 0.0f;
    }

    static int soundingMidiNote(const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return voice.midiNote;
        return -1;
    }

    static float soundingEnvelope(const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return voice.envelope;
        return 0.0f;
    }

    static int heldNoteCount(const VoiceEngine& engine) noexcept { return engine.heldCount_; }

    /** The two per-sample level scalars the dynamic reaches, as the render loop
        sees them: the voiced drive and the aspiration drive. Their ratio is the
        breath-to-voice balance, which has to rise as the dynamic falls. */
    static std::array<float, 3> dynamicDrives(const VoiceEngine& engine) noexcept
    {
        return { engine.voicedScaleAt_[0], engine.airLevelAt_[0], engine.smoothedDynamics_ };
    }

    /** Peak magnitude of F1 and F2 in the onset stage and in the main tract,
        each evaluated at its own pole angle, read straight out of the live
        coefficients. The onset stage runs 1.75x wider bandwidths, so before
        both stages were peak-normalised on the same basis their levels differed
        and the onset crossfade moved the level as well as the timbre.
        Returned as { tract F1, tract F2, early F1, early F2 }. */
    static std::array<float, 4> onsetStagePeakGains(const VoiceEngine& engine) noexcept
    {
        std::array<float, 4> result {};
        for (const auto& voice : engine.voices_)
        {
            if (! voice.active || voice.releasing || voice.onsetComplete)
                continue;

            const auto peak = [](double a1, double a2, double b0) noexcept
            {
                // The pole angle is what the normalisation targets, and it is
                // recoverable from the coefficients: a2 = -r^2, a1 = 2 r cos w.
                const double radius = std::sqrt(std::max(-a2, 0.0));
                const double omega = std::acos(
                    std::clamp(a1 / std::max(2.0 * radius, 1.0e-12), -1.0, 1.0));
                const double real = 1.0 - a1 * std::cos(omega) - a2 * std::cos(2.0 * omega);
                const double imaginary = a1 * std::sin(omega) + a2 * std::sin(2.0 * omega);
                return static_cast<float>(std::abs(b0)
                                          / std::sqrt(real * real + imaginary * imaginary));
            };

            for (int f = 0; f < 2; ++f)
            {
                const auto index = static_cast<std::size_t>(f);
                result[index] = peak(voice.tract[index].a1, engine.chunkA2_[index],
                                     voice.tract[index].b0);
                result[index + 2] = peak(voice.early[index].a1, engine.earlyA2_[index],
                                         voice.early[index].b0);
            }
            break;
        }
        return result;
    }

    /** The humanisation noise of the sounding voice in the units the render
        loop applies it in: the amplitude modulation depth the voiced drive is
        multiplied by, and the cents of pitch deviation the two nested jitter
        smoothers contribute. Both are read after the engine's own scaling, so
        a smoother whose drive is not renormalised for the sample rate shows up
        directly as a change in these numbers. */
    static std::array<float, 2> humanisationDepth(const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return { engine.shimmerDepth_ * voice.shimmer,
                         engine.blockParameters_.humanize
                             * (1.15f * voice.jitter + 1.35f * voice.jitterSlow) };
        return { 0.0f, 0.0f };
    }
};
} // namespace vocalor

namespace
{
constexpr int blockSize = 256;
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
    double sumOfSquares = 0.0;
    double peak = 0.0;
    std::size_t sampleCount = 0;
    std::size_t nonZeroCount = 0;
    bool finite = true;

    [[nodiscard]] double rms() const noexcept
    {
        return sampleCount > 0
            ? std::sqrt (sumOfSquares / static_cast<double> (sampleCount))
            : 0.0;
    }
};

RenderMetrics render (vocalor::VoiceEngine& engine, int samples)
{
    std::vector<float> left (blockSize, 0.0f);
    std::vector<float> right (blockSize, 0.0f);
    RenderMetrics result;

    for (int rendered = 0; rendered < samples;)
    {
        const auto count = std::min (blockSize, samples - rendered);
        std::fill (left.begin(), left.end(), 0.0f);
        std::fill (right.begin(), right.end(), 0.0f);
        engine.process (left.data(), right.data(), count);

        for (int i = 0; i < count; ++i)
        {
            for (const auto value : { left[static_cast<std::size_t> (i)],
                                      right[static_cast<std::size_t> (i)] })
            {
                result.finite = result.finite && std::isfinite (value);
                if (std::isfinite (value))
                {
                    const auto magnitude = std::abs (static_cast<double> (value));
                    result.peak = std::max (result.peak, magnitude);
                    result.sumOfSquares += static_cast<double> (value)
                                         * static_cast<double> (value);
                    if (magnitude > 1.0e-8)
                        ++result.nonZeroCount;
                }
                ++result.sampleCount;
            }
        }

        rendered += count;
    }

    return result;
}

std::vector<float> renderMono (vocalor::VoiceEngine& engine, int samples)
{
    std::vector<float> result (static_cast<std::size_t> (samples), 0.0f);
    std::vector<float> left (blockSize, 0.0f);
    std::vector<float> right (blockSize, 0.0f);

    for (int rendered = 0; rendered < samples;)
    {
        const auto count = std::min (blockSize, samples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int i = 0; i < count; ++i)
            result[static_cast<std::size_t> (rendered + i)] =
                0.5f * (left[static_cast<std::size_t> (i)] + right[static_cast<std::size_t> (i)]);
        rendered += count;
    }

    return result;
}

std::vector<float> renderInterleaved(vocalor::VoiceEngine& engine, int samples,
                                     int renderBlockSize)
{
    std::vector<float> result(static_cast<std::size_t>(2 * samples), 0.0f);
    std::vector<float> left(static_cast<std::size_t>(renderBlockSize), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(renderBlockSize), 0.0f);
    for (int rendered = 0; rendered < samples;)
    {
        const int count = std::min(renderBlockSize, samples - rendered);
        engine.process(left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto destination = static_cast<std::size_t>(2 * (rendered + sample));
            result[destination] = left[static_cast<std::size_t>(sample)];
            result[destination + 1] = right[static_cast<std::size_t>(sample)];
        }
        rendered += count;
    }
    return result;
}

vocalor::EngineParameters makeParameters (int mode, int chordQuality,
                                         int profile, int vowel)
{
    vocalor::EngineParameters parameters;
    parameters.profile = static_cast<vocalor::VoiceProfile> (profile);
    parameters.mode = static_cast<vocalor::PerformanceMode> (mode);
    parameters.vowel = static_cast<vocalor::Vowel> (vowel);
    parameters.chordQuality = static_cast<vocalor::ChordQuality> (chordQuality);
    parameters.choirSize = 8;
    parameters.breath = 0.34f;
    parameters.resonance = 0.66f;
    parameters.vibrato = 0.42f;
    parameters.humanize = 0.62f;
    parameters.spread = 0.70f;
    parameters.tension = 0.38f;
    parameters.room = 0.22f;
    parameters.outputGain = 0.50f;
    return parameters;
}

struct Scenario
{
    std::string_view name;
    int mode;
    int chordQuality;
};

void testRenderMatrix()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0 };
    constexpr std::array scenarios {
        Scenario { "solo", 0, 0 },
        Scenario { "choir", 1, 0 },
        Scenario { "major chord", 2, 0 },
        Scenario { "minor chord", 2, 1 }
    };

    for (std::size_t rateIndex = 0; rateIndex < sampleRates.size(); ++rateIndex)
    {
        const auto sampleRate = sampleRates[rateIndex];

        for (std::size_t scenarioIndex = 0; scenarioIndex < scenarios.size(); ++scenarioIndex)
        {
            const auto& scenario = scenarios[scenarioIndex];
            vocalor::VoiceEngine engine;
            engine.prepare (sampleRate, blockSize);
            engine.reset();
            auto parameters = makeParameters (
                scenario.mode, scenario.chordQuality,
                static_cast<int> (scenarioIndex % 2),
                static_cast<int> (scenarioIndex % 3));
            // Exercise every 1.1 addition on at least one leg of the matrix.
            parameters.vowelMorph = 0.25f * static_cast<float> (scenarioIndex);
            parameters.vowelX = 0.2f + 0.2f * static_cast<float> (scenarioIndex);
            parameters.vowelY = 0.8f - 0.2f * static_cast<float> (scenarioIndex);
            parameters.formantShift = -6.0f + 4.0f * static_cast<float> (scenarioIndex);
            parameters.glide = 0.35f;
            parameters.legato = scenarioIndex % 2 == 1;
            parameters.roomSize = 0.2f + 0.25f * static_cast<float> (scenarioIndex);
            engine.setParameters (parameters);

            engine.noteOn (60, 0.82f);
            const auto held = render (engine, static_cast<int> (sampleRate * 0.40));

            const auto label = std::string (scenario.name) + " at "
                             + std::to_string (static_cast<int> (sampleRate)) + " Hz";
            expect (held.finite, label + " produced a NaN or infinity");
            expect (held.nonZeroCount > held.sampleCount / 100,
                    label + " was silent or nearly entirely silent");
            expect (held.rms() > 1.0e-6, label + " RMS was effectively silent");
            expect (held.peak < 16.0, label + " exceeded the runaway-amplitude guardrail");
            expect (engine.getActiveVoiceCount() > 0,
                    label + " did not report an active voice after note-on");

            engine.noteOff (60);
            const auto released = render (engine, static_cast<int> (sampleRate * 0.55));
            expect (released.finite, label + " produced invalid audio after note-off");
            expect (released.peak < 16.0,
                    label + " exceeded the amplitude guardrail after note-off");
            engine.allNotesOff();
        }
    }
}

void testReleaseCompletes()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.setParameters (makeParameters (0, 0, 0, 0));
    engine.noteOn (64, 0.8f);

    const auto held = render (engine, static_cast<int> (sampleRate * 0.35));
    engine.noteOff (64);
    const auto earlyRelease = render (engine, static_cast<int> (sampleRate * 0.15));
    const auto releaseBody = render (engine, static_cast<int> (sampleRate * 3.5));
    const auto lateRelease = render (engine, static_cast<int> (sampleRate * 0.15));

    expect (held.finite && earlyRelease.finite && releaseBody.finite && lateRelease.finite,
            "note release produced a NaN or infinity");
    expect (held.rms() > 1.0e-6, "release test note did not produce held audio");
    expect (lateRelease.rms() < std::max (held.rms() * 0.25, 1.0e-7),
            "note-off did not decay to a quiet tail within the advertised tail time");
    expect (engine.getActiveVoiceCount() == 0,
            "voice remained active 3.8 seconds after note-off");
}

void testAllSoundOffIsImmediate()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);

    auto parameters = makeParameters (1, 0, 0, 0);
    parameters.room = 1.0f;
    engine.setParameters (parameters);
    engine.noteOn (60, 0.9f);

    const auto held = render (engine, static_cast<int> (sampleRate * 0.6));
    engine.allNotesOff();
    const auto releaseStart = render (engine, blockSize);

    expect (held.rms() > 1.0e-6, "all-sound-off test note was silent");
    expect (releaseStart.rms() > 1.0e-8,
            "all-notes-off did not preserve the normal release path");
    expect (engine.getActiveVoiceCount() > 0,
            "all-notes-off unexpectedly hard-stopped the active voices");

    engine.allSoundOff();
    const auto stopped = render (engine, static_cast<int> (sampleRate * 0.1));

    expect (engine.getActiveVoiceCount() == 0,
            "all-sound-off left voices active");
    expect (stopped.finite && stopped.peak == 0.0,
            "all-sound-off left audible voice or room-tail samples");
}

void testIdleStateAdvancementAndAutomation()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int idleSamples = 12345;
    constexpr int noteSamples = 4096;
    vocalor::VoiceEngine singleSampleBlocks;
    vocalor::VoiceEngine irregularBlocks;
    for (auto* engine : { &singleSampleBlocks, &irregularBlocks })
    {
        engine->prepare(sampleRate, blockSize);
        auto parameters = makeParameters(1, 0, 0, 0);
        parameters.room = 0.0f;
        // The 1.1 features are all chunk-aligned, so they must not disturb the
        // block-partition invariance either.
        parameters.vowelMorph = 0.65f;
        parameters.vowelX = 0.82f;
        parameters.vowelY = 0.24f;
        parameters.formantShift = 3.5f;
        parameters.glide = 0.4f;
        parameters.roomSize = 0.7f;
        engine->setParameters(parameters);
        engine->reset();
    }

    const auto firstSilence = renderInterleaved(singleSampleBlocks, idleSamples, 1);
    const auto secondSilence = renderInterleaved(irregularBlocks, idleSamples, 383);
    expect(firstSilence == secondSilence
               && std::all_of(firstSilence.begin(), firstSilence.end(),
                              [](float sample) { return sample == 0.0f; }),
           "idle output or ensemble state changed with block partitioning");

    singleSampleBlocks.noteOn(60, 0.82f);
    irregularBlocks.noteOn(60, 0.82f);
    expect(renderInterleaved(singleSampleBlocks, noteSamples, 1)
               == renderInterleaved(irregularBlocks, noteSamples, 383),
           "the first note after idle changed with block partitioning");

    vocalor::VoiceEngine automated;
    automated.prepare(sampleRate, blockSize);
    auto initial = makeParameters(0, 0, 0, 0);
    initial.room = 0.0f;
    initial.outputGain = 0.2f;
    initial.breath = 0.1f;
    initial.tension = 0.2f;
    automated.setParameters(initial);
    automated.reset();

    auto target = initial;
    target.room = 0.8f;
    target.outputGain = 1.3f;
    target.breath = 0.9f;
    target.tension = 0.75f;
    automated.setParameters(target);
    const auto idle = render(automated, static_cast<int>(0.5 * sampleRate));
    const auto smoothed = vocalor::VoiceEngineTestAccess::smoothedParameters(automated);
    expect(idle.peak == 0.0,
           "idle parameter automation produced non-zero output");
    expect(smoothed == std::array<float, 4> {
               target.room, target.outputGain, target.breath, target.tension },
           "idle parameter automation did not advance the smoothers to their targets");
}

void testVowelSpaceModel()
{
    // Every cardinal position must resolve close to its own formant set.
    for (int index = 0; index < vocalor::kCardinalVowelCount; ++index)
    {
        const auto position = vocalor::cardinalVowelPosition (index);
        std::array<float, vocalor::kFormantCount> resolved {};
        vocalor::formantsForVowelPoint (false, position.x, position.y, resolved.data());
        expect (resolved[0] > 200.0f && resolved[0] < 1100.0f,
                "cardinal vowel " + std::string (vocalor::cardinalVowelName (index))
                    + " resolved an implausible F1");
    }

    std::array<float, vocalor::kFormantCount> closeFront {};
    std::array<float, vocalor::kFormantCount> open {};
    std::array<float, vocalor::kFormantCount> closeBack {};
    const auto front = vocalor::cardinalVowelPosition (0);
    const auto centre = vocalor::cardinalVowelPosition (2);
    const auto back = vocalor::cardinalVowelPosition (4);
    vocalor::formantsForVowelPoint (false, front.x, front.y, closeFront.data());
    vocalor::formantsForVowelPoint (false, centre.x, centre.y, open.data());
    vocalor::formantsForVowelPoint (false, back.x, back.y, closeBack.data());

    expect (open[0] > closeFront[0] + 200.0f,
            "the open corner of the vowel space did not raise F1");
    expect (closeFront[1] > open[1] + 700.0f,
            "the front corner of the vowel space did not raise F2");
    expect (closeBack[1] < open[1],
            "the back corner of the vowel space did not lower F2");

    // Moving across the pad has to be continuous, not stepped.
    float previous = 0.0f;
    float largestStep = 0.0f;
    for (int step = 0; step <= 100; ++step)
    {
        std::array<float, vocalor::kFormantCount> point {};
        vocalor::formantsForVowelPoint (false, static_cast<float> (step) / 100.0f, 0.5f,
                                        point.data());
        if (step > 0)
            largestStep = std::max (largestStep, std::abs (point[1] - previous));
        previous = point[1];
    }
    expect (largestStep < 120.0f,
            "the vowel space made a discontinuous jump in F2 across the pad");

    // The preset anchors must keep the exact frequencies the engine shipped with.
    std::array<float, vocalor::kFormantCount> preset {};
    vocalor::formantsForPresetVowel (false, 0, preset.data());
    expect (std::abs (preset[0] - 850.0f) < 0.01f && std::abs (preset[1] - 1220.0f) < 0.01f,
            "the female AAH preset formants changed");
    vocalor::formantsForPresetVowel (true, 1, preset.data());
    expect (std::abs (preset[0] - 300.0f) < 0.01f && std::abs (preset[1] - 700.0f) < 0.01f,
            "the male OOH preset formants changed");
}

void testDisplayMathHelpers()
{
    const std::array<float, vocalor::kFormantCount> hz { 700.0f, 1200.0f, 2600.0f, 3400.0f, 4500.0f };
    const std::array<float, vocalor::kFormantCount> bandwidth { 80.0f, 95.0f, 130.0f, 190.0f, 260.0f };
    const std::array<float, vocalor::kFormantCount> gain { 1.0f, 0.66f, 0.34f, 0.18f, 0.095f };

    const auto onFormant = vocalor::formantResponseDb (700.0f, hz.data(), bandwidth.data(),
                                                       gain.data(), vocalor::kFormantCount, 48000.0f);
    const auto valley = vocalor::formantResponseDb (250.0f, hz.data(), bandwidth.data(),
                                                    gain.data(), vocalor::kFormantCount, 48000.0f);
    const auto top = vocalor::formantResponseDb (9000.0f, hz.data(), bandwidth.data(),
                                                 gain.data(), vocalor::kFormantCount, 48000.0f);
    expect (onFormant > valley + 6.0f, "the formant response did not peak at F1");
    expect (onFormant > top + 6.0f, "the formant response did not roll off above the formants");
    expect (std::isfinite (onFormant) && std::isfinite (valley) && std::isfinite (top),
            "the formant response produced a non-finite value");
    expect (vocalor::formantResponseDb (700.0f, nullptr, nullptr, nullptr, 0, 0.0f) <= -119.0f,
            "the formant response did not fall back safely on missing data");

    // Logarithmic frequency axis round trip.
    for (const float probe : { 100.0f, 440.0f, 3000.0f, 9000.0f })
    {
        const auto normalised = vocalor::normalisedLogFrequency (probe, 80.0f, 11000.0f);
        const auto back = vocalor::logFrequencyForNormalised (normalised, 80.0f, 11000.0f);
        expect (std::abs (back - probe) < probe * 0.001f,
                "the logarithmic frequency mapping did not round trip");
    }

    expect (std::abs (vocalor::linearToDecibels (1.0f, -60.0f)) < 0.001f,
            "unity was not reported as 0 dB");
    expect (vocalor::linearToDecibels (0.0f, -60.0f) == -60.0f,
            "silence was not clamped to the meter floor");
    expect (std::abs (vocalor::decibelsToMeterPosition (-30.0f, -60.0f, 0.0f) - 0.5f) < 0.001f,
            "the meter position mapping is not linear in decibels");

    // Ballistics: instant attack, gradual release.
    const auto attack = vocalor::smoothingCoefficient (0.001f, 0.02f);
    const auto release = vocalor::smoothingCoefficient (0.28f, 0.02f);
    expect (release > 0.0f && release < 0.2f, "the meter release coefficient is out of range");
    float level = 0.0f;
    level = vocalor::meterFollow (level, 1.0f, 1.0f, release);
    expect (std::abs (level - 1.0f) < 1.0e-6f, "the meter did not track a rising peak instantly");
    const auto afterOneStep = vocalor::meterFollow (level, 0.0f, attack, release);
    expect (afterOneStep < level && afterOneStep > 0.85f,
            "the meter release was either frozen or instantaneous");
    expect (vocalor::meterFollow (std::numeric_limits<float>::quiet_NaN(), 0.5f, 1.0f, 0.1f)
                == 0.5f,
            "the meter did not recover from a non-finite state");

    expect (std::abs (vocalor::roomSizeScale (0.5f) - 1.0f) < 1.0e-6f,
            "the default room size did not reproduce the historical geometry");
    expect (vocalor::roomSizeScale (0.0f) < 0.5f && vocalor::roomSizeScale (1.0f) > 2.0f,
            "the room size range is too narrow to be useful");
    expect (std::abs (vocalor::formantShiftRatio (0.0f) - 1.0f) < 1.0e-6f,
            "a zero formant shift was not neutral");
    expect (std::abs (vocalor::formantShiftRatio (12.0f) - 2.0f) < 1.0e-4f,
            "a twelve-semitone formant shift was not an octave");
    expect (vocalor::glideTimeSeconds (0.0f) == 0.0f
                && vocalor::glideTimeSeconds (1.0f) > 0.4f
                && vocalor::glideTimeSeconds (0.5f) < vocalor::glideTimeSeconds (1.0f),
            "the glide time mapping is not monotonic over a useful range");
}

void testVowelMorphAndFormantShift()
{
    constexpr auto sampleRate = 48000.0;

    // With morph at zero the pad position must be inaudible: the engine has to
    // reproduce the preset vowel exactly, which is what protects 1.0 sessions.
    vocalor::VoiceEngine reference;
    vocalor::VoiceEngine moved;
    for (auto* engine : { &reference, &moved })
    {
        engine->prepare (sampleRate, blockSize);
        engine->reset();
    }
    auto neutral = makeParameters (0, 0, 0, 0);
    reference.setParameters (neutral);
    auto shifted = neutral;
    shifted.vowelX = 1.0f;
    shifted.vowelY = 0.0f;
    moved.setParameters (shifted);
    reference.noteOn (60, 0.8f);
    moved.noteOn (60, 0.8f);
    expect (renderMono (reference, 4096) == renderMono (moved, 4096),
            "the vowel pad position changed the sound while morph was at zero");

    // Morphing to the close-front corner must lift F2 far above the AAH anchor.
    vocalor::VoiceEngine morphed;
    morphed.prepare (sampleRate, blockSize);
    morphed.reset();
    auto full = neutral;
    full.vowelMorph = 1.0f;
    full.vowelX = 1.0f;
    full.vowelY = 0.0f;
    morphed.setParameters (full);
    morphed.noteOn (60, 0.8f);
    const auto morphedAudio = render (morphed, 8192);
    const auto anchorFormants = vocalor::VoiceEngineTestAccess::chunkFormants (reference);
    const auto morphFormants = vocalor::VoiceEngineTestAccess::chunkFormants (morphed);
    expect (morphedAudio.finite && morphedAudio.rms() > 1.0e-6,
            "a fully morphed vowel produced no usable audio");
    expect (morphFormants[1] > anchorFormants[1] + 800.0f,
            "morphing to the close-front corner did not raise F2");
    expect (morphFormants[0] < anchorFormants[0] - 200.0f,
            "morphing to the close corner did not lower F1");

    // A partial morph has to land between the two, not snap.
    vocalor::VoiceEngine partial;
    partial.prepare (sampleRate, blockSize);
    partial.reset();
    auto half = full;
    half.vowelMorph = 0.5f;
    partial.setParameters (half);
    partial.noteOn (60, 0.8f);
    render (partial, blockSize);
    const auto halfFormants = vocalor::VoiceEngineTestAccess::chunkFormants (partial);
    const auto expectedF2 = 0.5f * (anchorFormants[1] + morphFormants[1]);
    expect (std::abs (halfFormants[1] - expectedF2) < 1.0f,
            "a half morph did not land halfway between the anchor and the target");

    // Formant shift scales the whole tract without touching the pitch.
    vocalor::VoiceEngine shiftedEngine;
    shiftedEngine.prepare (sampleRate, blockSize);
    shiftedEngine.reset();
    auto up = neutral;
    up.formantShift = 7.0f;
    shiftedEngine.setParameters (up);
    shiftedEngine.noteOn (60, 0.8f);
    const auto shiftedAudio = render (shiftedEngine, 8192);
    const auto shiftedFormants = vocalor::VoiceEngineTestAccess::chunkFormants (shiftedEngine);
    const auto ratio = vocalor::formantShiftRatio (7.0f);
    for (int i = 0; i < vocalor::kFormantCount; ++i)
        expect (std::abs (shiftedFormants[static_cast<std::size_t> (i)]
                          - anchorFormants[static_cast<std::size_t> (i)] * ratio) < 1.0f,
                "formant shift did not scale formant " + std::to_string (i + 1));
    expect (shiftedAudio.finite && shiftedAudio.rms() > 1.0e-6,
            "a shifted tract produced no usable audio");

    // An extreme downward shift must stay stable rather than fold the poles.
    vocalor::VoiceEngine extreme;
    extreme.prepare (sampleRate, blockSize);
    extreme.reset();
    auto down = neutral;
    down.formantShift = -12.0f;
    down.resonance = 1.0f;
    extreme.setParameters (down);
    extreme.noteOn (36, 1.0f);
    const auto extremeAudio = render (extreme, static_cast<int> (sampleRate * 0.5));
    expect (extremeAudio.finite && extremeAudio.peak < 16.0,
            "an extreme downward formant shift destabilised the tract");
}

void testGlideAndLegato()
{
    constexpr auto sampleRate = 48000.0;
    const auto hzFor = [] (int midi) { return 440.0f * std::exp2 ((static_cast<float> (midi) - 69.0f) / 12.0f); };

    // Portamento: the new note starts on the old pitch and arrives on target.
    vocalor::VoiceEngine glided;
    glided.prepare (sampleRate, blockSize);
    glided.reset();
    auto parameters = makeParameters (0, 0, 0, 0);
    parameters.glide = 0.55f;
    parameters.vibrato = 0.0f;
    glided.setParameters (parameters);

    glided.noteOn (48, 0.8f);
    render (glided, static_cast<int> (sampleRate * 0.3));
    glided.noteOff (48);
    glided.noteOn (60, 0.8f);
    render (glided, 512);
    const auto startFrequency = vocalor::VoiceEngineTestAccess::frequencyForRoot (glided, 60);
    render (glided, static_cast<int> (sampleRate * 1.6));
    const auto endFrequency = vocalor::VoiceEngineTestAccess::frequencyForRoot (glided, 60);

    expect (startFrequency > 0.0f && startFrequency < hzFor (54),
            "glide did not start the new note near the previous pitch");
    expect (std::abs (endFrequency - hzFor (60)) < hzFor (60) * 0.06f,
            "glide did not settle on the target pitch");

    // Without glide the new note starts on pitch immediately.
    vocalor::VoiceEngine direct;
    direct.prepare (sampleRate, blockSize);
    direct.reset();
    auto instant = parameters;
    instant.glide = 0.0f;
    direct.setParameters (instant);
    direct.noteOn (48, 0.8f);
    render (direct, static_cast<int> (sampleRate * 0.3));
    direct.noteOff (48);
    direct.noteOn (60, 0.8f);
    render (direct, 512);
    expect (vocalor::VoiceEngineTestAccess::frequencyForRoot (direct, 60) > hzFor (58),
            "a zero glide setting still bent the new note");

    // Legato: an overlapping note bends the sounding voice instead of restarting it.
    vocalor::VoiceEngine legato;
    legato.prepare (sampleRate, blockSize);
    legato.reset();
    auto phrased = parameters;
    phrased.legato = true;
    phrased.glide = 0.0f;
    legato.setParameters (phrased);

    legato.noteOn (60, 0.8f);
    render (legato, static_cast<int> (sampleRate * 0.4));
    const auto envelopeBefore = vocalor::VoiceEngineTestAccess::soundingEnvelope (legato);
    const auto voicesBefore = legato.getActiveVoiceCount();
    expect (envelopeBefore > 0.9f, "the legato test note did not reach a sustained level");

    legato.noteOn (64, 0.8f);
    render (legato, 256);
    expect (legato.getActiveVoiceCount() == voicesBefore,
            "legato started a second voice instead of bending the sounding one");
    expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (legato) == 64,
            "legato did not retune the sounding voice to the new note");
    expect (vocalor::VoiceEngineTestAccess::soundingEnvelope (legato) > 0.9f,
            "legato re-attacked the amplitude envelope");
    expect (vocalor::VoiceEngineTestAccess::heldNoteCount (legato) == 2,
            "the legato note stack did not record both held notes");

    // Repeating the pitch that is already sounding is not a move between
    // pitches, so it must still articulate. Taking the legato path here only
    // retuned the voice to itself, which made overlapping repeats from layered
    // controllers or a loop boundary silent.
    {
        vocalor::VoiceEngine repeated;
        repeated.prepare (sampleRate, blockSize);
        repeated.reset();
        repeated.setParameters (phrased);

        repeated.noteOn (60, 0.8f);
        render (repeated, static_cast<int> (sampleRate * 0.4));
        expect (vocalor::VoiceEngineTestAccess::soundingEnvelope (repeated) > 0.9f,
                "the repeated-root test note did not reach a sustained level");

        repeated.noteOn (60, 0.8f);
        render (repeated, 64);
        expect (vocalor::VoiceEngineTestAccess::soundingEnvelope (repeated) < 0.5f,
                "a repeated root under legato did not re-attack");
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (repeated) == 60,
                "the repeated root did not stay on its own pitch");

        // Both sources are still holding the pitch, so the stack keeps one
        // entry and the first note-off must not release the retriggered voice.
        expect (vocalor::VoiceEngineTestAccess::heldNoteCount (repeated) == 1,
                "an overlapping repeat added a duplicate held-stack entry");

        render (repeated, static_cast<int> (sampleRate * 0.4));
        repeated.noteOff (60);
        render (repeated, 256);
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (repeated) == 60
                    && vocalor::VoiceEngineTestAccess::soundingEnvelope (repeated) > 0.9f,
                "one note-off released a pitch another controller still held");

        repeated.noteOff (60);
        render (repeated, 256);
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (repeated) == -1
                    && vocalor::VoiceEngineTestAccess::heldNoteCount (repeated) == 0,
                "the final note-off did not release the overlapping repeat");
    }

    // Legato can be switched off in the middle of a phrase. The sounding
    // voices were bent to the top note rather than started for it, so
    // releasing it still has to hand them back to the key underneath -- the
    // player is physically holding it and would otherwise hear nothing.
    {
        vocalor::VoiceEngine automated;
        automated.prepare (sampleRate, blockSize);
        automated.reset();
        automated.setParameters (phrased);

        automated.noteOn (60, 0.8f);
        render (automated, static_cast<int> (sampleRate * 0.3));
        automated.noteOn (64, 0.8f);
        render (automated, 256);
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (automated) == 64,
                "the legato phrase did not reach the upper note");

        auto detached = phrased;
        detached.legato = false;
        automated.setParameters (detached);

        automated.noteOff (64);
        render (automated, 256);
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (automated) == 60,
                "disabling legato mid-phrase silenced the key still held");
        expect (vocalor::VoiceEngineTestAccess::soundingEnvelope (automated) > 0.9f,
                "falling back after legato was disabled re-attacked or released");
    }

    legato.noteOff (64);
    render (legato, 256);
    expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (legato) == 60,
            "releasing the top of a legato phrase did not fall back to the held note");
    expect (vocalor::VoiceEngineTestAccess::soundingEnvelope (legato) > 0.9f,
            "falling back to the held note re-attacked the envelope");

    legato.noteOff (60);
    render (legato, 256);
    expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (legato) == -1,
            "releasing the last held note did not start the release");
    expect (vocalor::VoiceEngineTestAccess::heldNoteCount (legato) == 0,
            "the legato note stack was not emptied");

    const auto tail = render (legato, static_cast<int> (sampleRate * 4.0));
    expect (tail.finite && legato.getActiveVoiceCount() == 0,
            "a legato phrase left voices hanging after the final release");
}

/** Renders a note, releases it, and keeps rendering long enough for the room to
    be the only thing left. Shared by the geometry and the tail-length checks. */
std::vector<float> renderRoomProbe (float size, float room)
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto parameters = makeParameters (0, 0, 0, 0);
    parameters.breath = 0.0f;
    parameters.vibrato = 0.0f;
    parameters.humanize = 0.0f;
    parameters.room = room;
    parameters.roomSize = size;
    engine.setParameters (parameters);
    engine.noteOn (60, 0.9f);

    auto result = renderMono (engine, static_cast<int> (sampleRate * 0.5));
    engine.noteOff (60);
    const auto tail = renderMono (engine, static_cast<int> (sampleRate * 1.7));
    result.insert (result.end(), tail.begin(), tail.end());
    return result;
}

void testRoomSizeGeometry()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int probeSamples = 20000;

    const auto probe = [] (float size)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = makeParameters (0, 0, 0, 0);
        parameters.room = 1.0f;
        parameters.roomSize = size;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.9f);
        return renderMono (engine, probeSamples);
    };

    // Two renders that differ only in room size are identical until the first
    // reflection of the smaller room arrives, which measures the tap geometry
    // directly rather than through a decay estimate.
    const auto firstDivergence = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        for (std::size_t i = 0; i < a.size(); ++i)
            if (std::abs (a[i] - b[i]) > 1.0e-9f)
                return static_cast<int> (i);
        return -1;
    };

    const auto tight = probe (0.05f);
    const auto neutral = probe (0.50f);
    const auto wide = probe (0.95f);

    const auto neutralArrival = firstDivergence (neutral, wide);
    const auto tightArrival = firstDivergence (tight, neutral);
    const auto expectedNeutral = 0.0297 * sampleRate * vocalor::roomSizeScale (0.5f);

    expect (neutralArrival > 0 && tightArrival > 0,
            "changing the room size did not change the rendered audio at all");
    expect (std::abs (static_cast<double> (neutralArrival) - expectedNeutral)
                < expectedNeutral * 0.12,
            "the default room size no longer reproduces the historical tap geometry");
    expect (tightArrival < neutralArrival,
            "a smaller room did not bring its first reflection forward");

    // What the size control has to buy musically is a longer tail, so measure
    // the reflected signal on its own: rendering the same note with the room
    // fully wet and fully dry and subtracting leaves nothing but the room.
    const auto reflectedTail = [] (float size)
    {
        const auto withRoom = renderRoomProbe (size, 1.0f);
        const auto withoutRoom = renderRoomProbe (size, 0.0f);
        const auto first = static_cast<std::size_t> (sampleRate * 1.4);
        double sum = 0.0;
        for (std::size_t i = first; i < withRoom.size(); ++i)
        {
            const auto value = static_cast<double> (withRoom[i]) - withoutRoom[i];
            sum += value * value;
        }
        return sum;
    };
    expect (reflectedTail (0.95f) > reflectedTail (0.05f) * 100.0,
            "a larger room did not ring for noticeably longer");

    // The tail still has to end, and the engine has to release its voices.
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto parameters = makeParameters (1, 0, 0, 0);
    parameters.room = 1.0f;
    parameters.roomSize = 1.0f;
    engine.setParameters (parameters);
    engine.noteOn (55, 0.9f);
    render (engine, static_cast<int> (sampleRate * 0.5));
    engine.noteOff (55);
    const auto tail = render (engine, static_cast<int> (sampleRate * 12.0));
    expect (tail.finite, "a maximum-size room tail produced invalid audio");
    expect (engine.getActiveVoiceCount() == 0, "a large room kept voices alive");
    const auto silence = render (engine, static_cast<int> (sampleRate * 0.2));
    expect (silence.peak == 0.0, "the maximum-size room tail never rang out");
}

void testDenormalAndNaNSafety()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto parameters = makeParameters (0, 0, 0, 0);
    parameters.room = 0.6f;
    parameters.breath = 0.0f;
    engine.setParameters (parameters);
    engine.noteOn (60, 0.05f);

    render (engine, static_cast<int> (sampleRate * 0.4));
    engine.noteOff (60);

    // Sample the recursive state repeatedly while the tail decays: this is
    // exactly the window in which an unprotected filter falls into denormals.
    float smallest = std::numeric_limits<float>::max();
    for (int step = 0; step < 40; ++step)
    {
        render (engine, static_cast<int> (sampleRate * 0.1));
        const auto observed = vocalor::VoiceEngineTestAccess::smallestNonZeroState (engine);
        if (observed > 0.0f)
            smallest = std::min (smallest, observed);
    }
    expect (smallest > 1.0e-32f,
            "recursive state reached denormal range during the release tail");

    // Hostile parameters must never leak a NaN into the output.
    vocalor::VoiceEngine hostile;
    hostile.prepare (sampleRate, blockSize);
    hostile.reset();
    auto poisoned = makeParameters (1, 0, 1, 2);
    const auto notANumber = std::numeric_limits<float>::quiet_NaN();
    const auto infinity = std::numeric_limits<float>::infinity();
    poisoned.breath = notANumber;
    poisoned.resonance = infinity;
    poisoned.vibrato = -infinity;
    poisoned.humanize = notANumber;
    poisoned.spread = infinity;
    poisoned.tension = notANumber;
    poisoned.room = infinity;
    poisoned.outputGain = notANumber;
    poisoned.vowelX = notANumber;
    poisoned.vowelY = infinity;
    poisoned.vowelMorph = notANumber;
    poisoned.formantShift = notANumber;
    poisoned.glide = infinity;
    poisoned.roomSize = notANumber;
    hostile.setParameters (poisoned);
    hostile.noteOn (72, 1.0f);
    const auto poisonedRender = render (hostile, static_cast<int> (sampleRate * 0.3));
    hostile.noteOn (72, notANumber);
    const auto afterBadVelocity = render (hostile, blockSize);
    expect (poisonedRender.finite && poisonedRender.peak < 16.0,
            "non-finite parameters produced invalid audio");
    expect (afterBadVelocity.finite, "a non-finite velocity produced invalid audio");
}

void testParameterSmoothingHasNoZipper()
{
    constexpr auto sampleRate = 48000.0;
    constexpr float quietGain = 0.05f;
    constexpr float loudGain = 1.60f;
    const auto settleSamples = static_cast<int> (sampleRate * 0.5);
    const auto observeSamples = static_cast<int> (sampleRate * 0.4);

    // Output gain is the last thing the engine applies and feeds back into
    // nothing, so dividing a jumped render by an otherwise identical constant
    // render recovers the smoother trajectory sample by sample.
    const auto renderWithGain = [&] (bool jump)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = makeParameters (0, 0, 0, 0);
        parameters.outputGain = quietGain;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.85f);
        render (engine, settleSamples);
        if (jump)
        {
            parameters.outputGain = loudGain;
            engine.setParameters (parameters);
        }
        return renderMono (engine, observeSamples);
    };

    const auto steady = renderWithGain (false);
    const auto jumped = renderWithGain (true);

    float previousRatio = 1.0f;
    int previousIndex = -1;
    float worstSlope = 0.0f;
    float finalRatio = 1.0f;
    bool monotonic = true;
    int observations = 0;

    for (std::size_t i = 0; i < steady.size(); ++i)
    {
        if (std::abs (steady[i]) < 1.0e-3f)
            continue;
        const auto ratio = jumped[i] / steady[i];
        if (previousIndex >= 0)
        {
            const auto distance = static_cast<float> (static_cast<int> (i) - previousIndex);
            worstSlope = std::max (worstSlope, (ratio - previousRatio) / distance);
            monotonic = monotonic && ratio > previousRatio - 0.02f;
        }
        previousRatio = ratio;
        previousIndex = static_cast<int> (i);
        finalRatio = ratio;
        ++observations;
    }

    expect (observations > 200, "the smoothing probe did not find enough usable samples");
    expect (monotonic, "the output-gain smoother reversed direction mid-glide");
    // A 25 ms one-pole moving 0.05 -> 1.6 cannot climb faster than about
    // 0.026 gain ratio units per sample. A stepped parameter would be ~31.
    expect (worstSlope < 0.06f,
            "the output gain stepped instead of gliding to its new value");
    expect (std::abs (finalRatio - loudGain / quietGain) < loudGain / quietGain * 0.02f,
            "the output-gain smoother did not reach its new target");

    // The remaining smoothed parameters have to converge too.
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto quiet = makeParameters (0, 0, 0, 0);
    quiet.breath = 0.0f;
    quiet.tension = 0.0f;
    quiet.room = 0.0f;
    quiet.outputGain = 0.05f;
    engine.setParameters (quiet);
    engine.noteOn (60, 0.85f);
    render (engine, static_cast<int> (sampleRate * 0.3));

    auto loud = quiet;
    loud.breath = 1.0f;
    loud.tension = 1.0f;
    loud.room = 1.0f;
    loud.outputGain = 1.6f;
    engine.setParameters (loud);
    const auto glide = render (engine, static_cast<int> (sampleRate * 0.5));
    expect (glide.finite && glide.peak < 16.0,
            "a full-range parameter jump produced invalid audio");

    const auto smoothed = vocalor::VoiceEngineTestAccess::smoothedParameters (engine);
    expect (std::abs (smoothed[0] - loud.room) < 0.01f
                && std::abs (smoothed[1] - loud.outputGain) < 0.01f
                && std::abs (smoothed[2] - loud.breath) < 0.01f
                && std::abs (smoothed[3] - loud.tension) < 0.01f,
            "the parameter smoothers did not converge on their new targets");
}

void testDisplayStateTracksTheEngine()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();

    auto idleState = engine.getDisplayState();
    expect (idleState.formantHz[0] > 100.0f && idleState.formantHz[0] < 2000.0f,
            "the display state had no usable tract before the first note");
    expect (idleState.levelLeft == 0.0f && idleState.levelRight == 0.0f,
            "the display meters were not silent while the engine was idle");
    expect (std::abs (idleState.sampleRate - 48000.0f) < 1.0f,
            "the display state reported the wrong sample rate");

    auto parameters = makeParameters (0, 0, 0, 0);
    parameters.vowelMorph = 1.0f;
    parameters.vowelX = 1.0f;
    parameters.vowelY = 0.0f;
    engine.setParameters (parameters);
    engine.noteOn (62, 0.9f);
    render (engine, static_cast<int> (sampleRate * 0.4));

    const auto state = engine.getDisplayState();
    expect (state.activeVoices > 0, "the display state lost the active voice count");
    expect (state.levelLeft > 0.0f && state.levelRight > 0.0f,
            "the display meters stayed at zero while a note was sounding");
    expect (state.formantHz[1] > 2000.0f,
            "the display did not follow the morphed F2");
    for (int i = 0; i < vocalor::kFormantCount; ++i)
    {
        const auto index = static_cast<std::size_t> (i);
        expect (state.formantBandwidth[index] > 10.0f && state.formantBandwidth[index] < 2000.0f,
                "the published formant bandwidth is out of range");
        expect (state.formantGain[index] > 0.0f && state.formantGain[index] < 4.0f,
                "the published formant gain is out of range");
    }
    const auto corner = vocalor::cardinalVowelPosition (0);
    expect (std::abs (state.vowelX - corner.x) < 0.02f && std::abs (state.vowelY - corner.y) < 0.02f,
            "the published vowel position did not follow a full morph");

    // The response curve of the published tract has to peak on F1.
    const auto atFormant = vocalor::formantResponseDb (
        state.formantHz[0], state.formantHz.data(), state.formantBandwidth.data(),
        state.formantGain.data(), vocalor::kFormantCount, state.sampleRate);
    const auto belowFormant = vocalor::formantResponseDb (
        state.formantHz[0] * 0.35f, state.formantHz.data(), state.formantBandwidth.data(),
        state.formantGain.data(), vocalor::kFormantCount, state.sampleRate);
    expect (atFormant > belowFormant + 3.0f,
            "the published tract does not describe a resonant response");

    engine.allSoundOff();
    const auto stopped = engine.getDisplayState();
    expect (stopped.levelLeft == 0.0f && stopped.levelRight == 0.0f && stopped.activeVoices == 0,
            "all-sound-off did not reset the display meters");
}

/** RMS of a steady note, in dB, with everything stochastic switched off. */
double steadyLevelDb (double sampleRate, const vocalor::EngineParameters& parameters,
                      int midiNote)
{
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.setParameters (parameters);
    engine.noteOn (midiNote, 0.8f);
    render (engine, static_cast<int> (sampleRate * 0.4));
    const auto steady = render (engine, static_cast<int> (sampleRate * 0.6));
    return 20.0 * std::log10 (std::max (steady.rms(), 1.0e-12));
}

/** Magnitude of one harmonic of @c fundamental, by direct evaluation. */
double harmonicMagnitude (const std::vector<float>& samples, double frequency, double sampleRate)
{
    double real = 0.0;
    double imaginary = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        const auto angle = 2.0 * 3.14159265358979323846 * frequency
                         * static_cast<double> (i) / sampleRate;
        real += samples[i] * std::cos (angle);
        imaginary -= samples[i] * std::sin (angle);
    }
    return std::sqrt (real * real + imaginary * imaginary) * 2.0
         / std::max (static_cast<double> (samples.size()), 1.0);
}

vocalor::EngineParameters steadyParameters()
{
    auto parameters = makeParameters (0, 0, 0, 0);
    parameters.vibrato = 0.0f;
    parameters.humanize = 0.0f;
    parameters.room = 0.0f;
    return parameters;
}

/** Nothing about the sound may depend on the sample rate.

    Before the formant bank was peak-normalised, each resonator's gain was
    proportional to the sample rate: the same patch measured 12.7 dB louder at
    192 kHz than at 44.1 kHz, harmonic for harmonic.
*/
void testSampleRateInvariance()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    constexpr double fundamental = 261.6255653;

    double quietest = 1.0e9;
    double loudest = -1.0e9;
    std::array<double, 4> lowest { 1.0e9, 1.0e9, 1.0e9, 1.0e9 };
    std::array<double, 4> highest { -1.0e9, -1.0e9, -1.0e9, -1.0e9 };
    constexpr std::array harmonics { 1, 2, 4, 8 };

    for (const auto sampleRate : sampleRates)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (steadyParameters());
        engine.noteOn (60, 0.8f);
        renderMono (engine, static_cast<int> (sampleRate * 0.6));
        const auto steady = renderMono (engine, static_cast<int> (sampleRate * 0.6));

        double sumOfSquares = 0.0;
        for (const auto value : steady)
            sumOfSquares += static_cast<double> (value) * value;
        const auto levelDb = 20.0 * std::log10 (std::max (
            std::sqrt (sumOfSquares / static_cast<double> (steady.size())), 1.0e-12));
        quietest = std::min (quietest, levelDb);
        loudest = std::max (loudest, levelDb);

        for (std::size_t h = 0; h < harmonics.size(); ++h)
        {
            const auto magnitude = 20.0 * std::log10 (std::max (
                harmonicMagnitude (steady, fundamental * harmonics[h], sampleRate), 1.0e-12));
            lowest[h] = std::min (lowest[h], magnitude);
            highest[h] = std::max (highest[h], magnitude);
        }
    }

    expect (loudest - quietest < 0.6,
            "the rendered level still depends on the sample rate");
    for (std::size_t h = 0; h < harmonics.size(); ++h)
        expect (highest[h] - lowest[h] < 1.5,
                "harmonic " + std::to_string (harmonics[h])
                    + " changed level across the supported sample rates");
}

/** Humanisation must have the same depth at every sample rate, not merely the
    same spectrum.

    The shimmer and the first pitch-jitter smoother are one-poles driven by
    white noise. Deriving their coefficients from a corner frequency and a time
    constant fixed the spectrum, but a noise-driven one-pole settles at output
    variance c / (2 - c), so with c now proportional to 1/sampleRate the depth
    fell as 1/sqrt(sampleRate) unless the drive is renormalised: measured
    shimmer standard deviation 0.0304 / 0.0228 / 0.0158 at 48 / 96 / 192 kHz
    before the compensation went in. testSampleRateInvariance structurally
    cannot see this, because its patch sets humanize = 0, which zeroes both the
    shimmer depth and the jitter's contribution to the pitch.

    Both halves are asserted here: the standard deviation is the depth, and the
    autocorrelation at a fixed lag in seconds is the spectrum.
*/
void testHumanisationDepthIsRateInvariant()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };
    constexpr double windowSeconds = 6.0;
    // The observation stride is a duration, not a sample count, so the lag of
    // the autocorrelation below means the same thing at every rate.
    constexpr double strideSeconds = 0.004;
    constexpr std::array names { "shimmer amplitude modulation",
                                 "pitch jitter" };

    std::array<double, names.size()> quietest {};
    std::array<double, names.size()> loudest {};
    std::array<double, names.size()> slowest {};
    std::array<double, names.size()> fastest {};
    quietest.fill (1.0e9);
    loudest.fill (-1.0e9);
    slowest.fill (-1.0e9);
    fastest.fill (1.0e9);

    for (const auto sampleRate : sampleRates)
    {
        auto parameters = steadyParameters();
        parameters.humanize = 1.0f;

        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (parameters);
        engine.noteOn (60, 0.8f);
        render (engine, static_cast<int> (sampleRate * 0.5));

        const auto stride = std::max (1, static_cast<int> (sampleRate * strideSeconds));
        const auto observations = static_cast<int> (windowSeconds / strideSeconds);
        std::array<std::vector<double>, names.size()> series;
        for (int i = 0; i < observations; ++i)
        {
            render (engine, stride);
            const auto depth = vocalor::VoiceEngineTestAccess::humanisationDepth (engine);
            for (std::size_t k = 0; k < names.size(); ++k)
                series[k].push_back (static_cast<double> (depth[k]));
        }

        for (std::size_t k = 0; k < names.size(); ++k)
        {
            double sumOfSquares = 0.0;
            double lagProduct = 0.0;
            for (std::size_t i = 0; i < series[k].size(); ++i)
            {
                sumOfSquares += series[k][i] * series[k][i];
                if (i > 0)
                    lagProduct += series[k][i] * series[k][i - 1];
            }
            const auto deviation = std::sqrt (sumOfSquares
                                              / static_cast<double> (series[k].size()));
            expect (deviation > 1.0e-5,
                    std::string (names[k]) + " is not running at all");
            quietest[k] = std::min (quietest[k], deviation);
            loudest[k] = std::max (loudest[k], deviation);

            const auto correlation = lagProduct / std::max (sumOfSquares, 1.0e-30);
            slowest[k] = std::max (slowest[k], correlation);
            fastest[k] = std::min (fastest[k], correlation);
        }
    }

    for (std::size_t k = 0; k < names.size(); ++k)
    {
        // Depth. Without the drive compensation this measured 6.5 dB for the
        // shimmer and 6.2 dB for the jitter; with it, under 0.6 dB. Two leaves
        // room for the sampling error of a six-second window and nothing else.
        expect (20.0 * std::log10 (loudest[k] / std::max (quietest[k], 1.0e-12)) < 2.0,
                std::string (names[k]) + " depth still depends on the sample rate");
        // Spectrum. A fixed 4 ms lag: if a smoother's corner moved with the
        // sample rate, so would this. With the old per-sample and per-control-
        // period coefficients the shimmer measured 0.33 / 0.31 / 0.09 / 0.003
        // and the jitter 0.88 / 0.86 / 0.69 / 0.39 across the four rates; both
        // now hold to within 0.025.
        expect (slowest[k] - fastest[k] < 0.06,
                std::string (names[k]) + " spectrum still depends on the sample rate");
    }
}

/** The vowel pad and the formant shift are timbre controls, not faders. */
void testTractLevelStability()
{
    constexpr auto sampleRate = 48000.0;
    constexpr std::array padX { 0.0f, 0.5f, 1.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.5f, 1.0f };
    constexpr std::array padY { 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f };

    double quietest = 1.0e9;
    double loudest = -1.0e9;
    for (std::size_t i = 0; i < padX.size(); ++i)
    {
        auto parameters = steadyParameters();
        parameters.vowelMorph = 1.0f;
        parameters.vowelX = padX[i];
        parameters.vowelY = padY[i];
        const auto level = steadyLevelDb (sampleRate, parameters, 60);
        quietest = std::min (quietest, level);
        loudest = std::max (loudest, level);
    }
    // 15.9 dB before the bank was normalised. What is left is the genuine
    // interaction between the fundamental and where F1 happens to sit.
    expect (loudest - quietest < 10.0,
            "the vowel pad still works as a volume control");

    quietest = 1.0e9;
    loudest = -1.0e9;
    for (int semitones = -12; semitones <= 12; semitones += 3)
    {
        auto parameters = steadyParameters();
        parameters.formantShift = static_cast<float> (semitones);
        const auto level = steadyLevelDb (sampleRate, parameters, 60);
        quietest = std::min (quietest, level);
        loudest = std::max (loudest, level);
    }
    expect (loudest - quietest < 13.0,
            "the formant shift still changes the level more than the timbre");

    // The old bank's gain fell monotonically as the formants rose, so shifting
    // an octave up cost 17.5 dB. Shifting must no longer have a level trend.
    auto down = steadyParameters();
    down.formantShift = -12.0f;
    auto up = steadyParameters();
    up.formantShift = 12.0f;
    expect (std::abs (steadyLevelDb (sampleRate, down, 60)
                      - steadyLevelDb (sampleRate, up, 60)) < 6.0,
            "shifting the formants up an octave still acts as a fader");
}

/** The parallel formant bank has to behave like an all-pole vocal tract. */
void testParallelFormantBank()
{
    // Peak normalisation: the resonator's gain at its own centre frequency must
    // be one for every bandwidth, centre frequency and sample rate.
    for (const float sampleRate : { 44100.0f, 48000.0f, 96000.0f, 192000.0f })
    {
        for (const float centre : { 60.0f, 310.0f, 850.0f, 2790.0f, 0.4f * sampleRate })
        {
            for (const float bandwidth : { 20.0f, 75.0f, 260.0f, 700.0f })
            {
                // The reference is evaluated in double on purpose: at 192 kHz
                // 1 - a1 cos - a2 cos2 is a difference of near-equal ones and a
                // float reference would be noisier than the value under test.
                const auto radius = static_cast<double> (
                    std::exp (-3.14159265358979323846f * bandwidth / sampleRate));
                const auto omega = 2.0 * 3.14159265358979323846
                                 * static_cast<double> (centre) / sampleRate;
                const auto a1 = 2.0 * radius * std::cos (omega);
                const auto a2 = -radius * radius;
                const auto b0 = static_cast<double> (vocalor::formantResonatorGain (
                    static_cast<float> (radius), static_cast<float> (std::sin (omega))));

                const auto real = 1.0 - a1 * std::cos (omega) - a2 * std::cos (2.0 * omega);
                const auto imaginary = a1 * std::sin (omega) + a2 * std::sin (2.0 * omega);
                const auto gain = b0 / std::sqrt (real * real + imaginary * imaginary);
                expect (std::abs (gain - 1.0) < 2.0e-3,
                        "the formant resonator was not normalised to unit peak gain");
            }
        }
    }

    expect (vocalor::formantPolarity (0) > 0.0f && vocalor::formantPolarity (1) < 0.0f
                && vocalor::formantPolarity (2) > 0.0f,
            "the parallel formant bank stopped alternating its polarity");

    // Cascade-derived amplitudes: finite, ordered, and vowel dependent.
    const std::array<float, vocalor::kFormantCount> bandwidth {
        75.0f, 90.0f, 125.0f, 185.0f, 260.0f };
    std::array<float, vocalor::kFormantCount> openGain {};
    std::array<float, vocalor::kFormantCount> closeGain {};
    const std::array<float, vocalor::kFormantCount> openHz {
        850.0f, 1220.0f, 2810.0f, 3650.0f, 4950.0f };
    const std::array<float, vocalor::kFormantCount> closeHz {
        310.0f, 2790.0f, 3310.0f, 3900.0f, 4950.0f };
    vocalor::parallelFormantAmplitudes (openHz.data(), bandwidth.data(), vocalor::kFormantCount,
                                        48000.0f, 0.010f, openGain.data());
    vocalor::parallelFormantAmplitudes (closeHz.data(), bandwidth.data(), vocalor::kFormantCount,
                                        48000.0f, 0.010f, closeGain.data());

    bool finite = true;
    for (int i = 0; i < vocalor::kFormantCount; ++i)
    {
        finite = finite && std::isfinite (openGain[static_cast<std::size_t> (i)])
                        && std::isfinite (closeGain[static_cast<std::size_t> (i)]);
        expect (openGain[static_cast<std::size_t> (i)] > 0.0f,
                "a cascade-derived formant amplitude was not positive");
    }
    expect (finite, "the cascade-derived formant amplitudes were not finite");
    // /a/ concentrates its energy in F1 and F2; /i/ carries F2 and F3 nearly as
    // strongly as F1. If the amplitudes did not track the vowel this would not
    // hold, and a front vowel would not sound front.
    expect (openGain[2] / openGain[0] < closeGain[2] / closeGain[0] * 0.5f,
            "the formant amplitudes no longer follow the vowel");

    // Nothing may starve completely: the bank models nothing above F5.
    const auto largest = *std::max_element (openGain.begin(), openGain.end());
    for (const auto value : openGain)
        expect (value >= largest * 0.0099f,
                "a formant amplitude fell below the modelled floor");

    // And the valley between F1 and F2 must stay within reach of the peak. A
    // bank summed with a common sign digs a 64 dB notch there on a close front
    // vowel, tens of dB deeper than any vocal tract produces.
    // The onset crossfade blends a wide-bandwidth early stage into the full
    // tract. The early stage used the unnormalised b0, so it sat at a different
    // level from the tract it fades into and the crossfade moved the level as
    // well as the timbre. Both stages are now normalised on the same basis, so
    // each resonator delivers the same magnitude at its own centre frequency
    // whatever bandwidth it runs.
    for (const float sampleRate : { 44100.0f, 48000.0f, 96000.0f, 192000.0f })
    {
        vocalor::VoiceEngine engine;
        engine.prepare (static_cast<double> (sampleRate), blockSize);
        engine.reset();
        engine.setParameters (steadyParameters());
        engine.noteOn (60, 0.8f);
        // Short enough that the crossfade is still running: it ends 115-195 ms
        // into the note.
        render (engine, static_cast<int> (sampleRate * 0.05f));

        const auto gains = vocalor::VoiceEngineTestAccess::onsetStagePeakGains (engine);
        for (std::size_t f = 0; f < 2; ++f)
        {
            expect (gains[f] > 1.0e-6f,
                    "the onset crossfade probe found no sounding voice");
            expect (std::abs (gains[f + 2] - gains[f]) <= 2.0e-3f * gains[f],
                    "the onset stage is no longer normalised like the main tract");
        }
    }

    // Limits are the pre-change depths rounded down: /i/-like 64.2, /a/-like
    // 24.9 and /u/-like 29.4 dB. The current bank measures 32.2, 9.6 and
    // 13.7 dB at the same three corners.
    struct Corner { float x; float y; double limit; };
    for (const auto corner : { Corner { 1.0f, 0.0f, 42.0 }, Corner { 0.5f, 1.0f, 16.0 },
                               Corner { 0.0f, 0.0f, 20.0 } })
    {
        vocalor::VoiceEngine engine;
        engine.prepare (48000.0, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.vowelMorph = 1.0f;
        parameters.vowelX = corner.x;
        parameters.vowelY = corner.y;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.8f);
        render (engine, static_cast<int> (48000.0 * 0.5));

        const auto state = engine.getDisplayState();
        const auto response = [&state] (double hz)
        {
            return static_cast<double> (vocalor::formantResponseDb (
                static_cast<float> (hz), state.formantHz.data(), state.formantBandwidth.data(),
                state.formantGain.data(), vocalor::kFormantCount, state.sampleRate));
        };

        double peak = -1.0e9;
        for (double hz = 80.0; hz < 11000.0; hz *= 1.002)
            peak = std::max (peak, response (hz));
        double valley = 1.0e9;
        for (double hz = state.formantHz[0] * 1.08; hz < state.formantHz[1] * 0.92; hz *= 1.002)
            valley = std::min (valley, response (hz));

        expect (peak - valley < corner.limit,
                "the formant bank cancelled itself between F1 and F2");
    }
}

/** Ensemble size has to mean what it says: every value in its range renders
    exactly that many independently humanised singers. */
void testEnsembleSizeIsExact()
{
    constexpr auto sampleRate = 48000.0;
    for (int singers = 2; singers <= 12; ++singers)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = makeParameters (1, 0, 0, 0);
        parameters.choirSize = singers;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.8f);
        render (engine, blockSize);
        expect (engine.getActiveVoiceCount() == singers,
                "an ensemble of " + std::to_string (singers)
                    + " did not render that many singers");
    }

    // Out-of-range requests still land on the nearest usable ensemble.
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto parameters = makeParameters (1, 0, 0, 0);
    parameters.choirSize = 40;
    engine.setParameters (parameters);
    engine.noteOn (60, 0.8f);
    render (engine, blockSize);
    expect (engine.getActiveVoiceCount() == 12,
            "an oversized ensemble request was not clamped to the singers available");
}

/** Resonance and formant shift reach the pole radius, which cannot be smoothed
    downstream, so a jump on either has to be smoothed before it is used. */
void testTractCoefficientSmoothing()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    auto parameters = steadyParameters();
    parameters.resonance = 0.0f;
    parameters.formantShift = 0.0f;
    engine.setParameters (parameters);
    engine.noteOn (60, 0.85f);
    render (engine, static_cast<int> (sampleRate * 0.3));

    const auto before = vocalor::VoiceEngineTestAccess::chunkBandwidths (engine);
    parameters.resonance = 1.0f;
    parameters.formantShift = 12.0f;
    engine.setParameters (parameters);

    // One chunk of a 20 ms smoother may cover only a small part of the jump.
    render (engine, 64);
    const auto afterOneChunk = vocalor::VoiceEngineTestAccess::chunkBandwidths (engine);
    render (engine, static_cast<int> (sampleRate * 0.3));
    const auto settled = vocalor::VoiceEngineTestAccess::chunkBandwidths (engine);

    const auto span = std::abs (settled[0] - before[0]);
    expect (span > 1.0f, "the resonance and shift jump did not move the bandwidths at all");
    expect (std::abs (afterOneChunk[0] - before[0]) < span * 0.25f,
            "the tract bandwidth stepped instead of gliding after a parameter jump");
    expect (std::abs (afterOneChunk[0] - before[0]) > 0.0f,
            "the tract bandwidth froze instead of gliding after a parameter jump");
}

void testRoughPerformance()
{
    constexpr auto sampleRate = 96000.0;
    constexpr auto secondsToRender = 1.0;
    vocalor::VoiceEngine engine;
    engine.prepare (sampleRate, blockSize);
    engine.reset();

    auto parameters = makeParameters (1, 0, 1, 1);
    parameters.choirSize = 12;
    engine.setParameters (parameters);
    engine.noteOn (57, 0.85f);

    // Warm the caches so the measurement reports steady-state cost.
    render (engine, blockSize * 8);

    const auto start = std::chrono::steady_clock::now();
    const auto metrics = render (engine, static_cast<int> (sampleRate * secondsToRender));
    const auto elapsed = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - start).count();
    const auto realTimeRatio = elapsed / secondsToRender;
    const auto nanosecondsPerSample = elapsed * 1.0e9 / (sampleRate * secondsToRender);

    std::cout << std::fixed << std::setprecision (3)
              << "12-singer 96 kHz render: " << elapsed << " s ("
              << realTimeRatio << "x real time, "
              << std::setprecision (1) << nanosecondsPerSample << " ns/sample)\n";

    expect (metrics.finite && metrics.rms() > 1.0e-6,
            "performance render did not produce valid, non-silent audio");
    // Deliberately generous: this is a regression tripwire that also tolerates
    // unoptimised CI builds, not a release-mode real-time certification. The
    // 1.1 engine measures roughly 0.35x here against 0.55x for 1.0.
    expect (realTimeRatio < 20.0,
            "12-singer render exceeded the 20x-real-time regression guardrail");
}

void testSingerFormantAndLipZero()
{
    constexpr auto sampleRate = 48000.0;
    vocalor::VoiceEngine engine;
    engine.prepare(sampleRate, blockSize);
    engine.reset();

    auto parameters = makeParameters(0, 0, 0, 0);
    parameters.tension = 0.90f;
    engine.setParameters(parameters);
    engine.noteOn(60, 0.85f);

    const auto metrics = render(engine, static_cast<int>(sampleRate * 0.4));
    expect(metrics.finite && metrics.rms() > 1.0e-6,
           "Singer's Formant tension render produced no usable audio");
    expect(metrics.peak < 16.0,
           "Singer's Formant tension render exceeded amplitude guardrail");
}

/** Continuous performance expression.

    The 1.1 engine froze the entire dynamic response of a note at note-on and
    had no pitch bend, mod wheel, expression pedal or sustain pedal at all. The
    dynamic added here is not a fader: it has to move the source spectrum and
    the breath-to-voice balance by more than it moves the level, or it is only
    an output trim wearing a different name.
*/
void testPerformanceExpression()
{
    constexpr auto sampleRate = 48000.0;
    constexpr double fundamental = 261.6255653;

    // The response curve is exactly inert at its default, which is what lets
    // every other test in this file keep its 1.1 expectations.
    const auto full = vocalor::dynamicResponse (1.0f);
    expect (full.voicedGain == 1.0f && full.airGain == 1.0f && full.effortScale == 1.0f
                && full.sourceTensionScale == 1.0f && full.vibratoScale == 1.0f,
            "the dynamic response is not inert at its full setting");

    const auto empty = vocalor::dynamicResponse (0.0f);
    expect (std::abs (empty.voicedGain - 0.125f) < 1.0e-6f,
            "an empty dynamic is not 18 dB down on the voiced source");
    expect (empty.airGain > 3.0f * empty.voicedGain,
            "aspiration falls as fast as the voiced source at a low dynamic");

    float previousGain = -1.0f;
    bool monotonic = true;
    for (int step = 0; step <= 10; ++step)
    {
        const auto response = vocalor::dynamicResponse (0.1f * static_cast<float> (step));
        monotonic = monotonic && response.voicedGain > previousGain
                              && response.effortScale > 0.0f;
        previousGain = response.voicedGain;
    }
    expect (monotonic, "the dynamic level is not monotonic in its control");

    // A soft note has to be duller, not merely quieter.
    const auto spectrumAt = [] (float dynamics)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.breath = 0.30f;
        parameters.dynamics = dynamics;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        renderMono (engine, static_cast<int> (sampleRate * 0.5));
        const auto samples = renderMono (engine, static_cast<int> (sampleRate * 0.5));

        double high = 0.0;
        for (int harmonic = 9; harmonic <= 18; ++harmonic)
        {
            const auto magnitude = harmonicMagnitude (
                samples, fundamental * harmonic, sampleRate);
            high += magnitude * magnitude;
        }
        return std::array<double, 2> {
            harmonicMagnitude (samples, fundamental, sampleRate), std::sqrt (high) };
    };

    const auto loudSpectrum = spectrumAt (1.0f);
    const auto softSpectrum = spectrumAt (0.30f);
    const auto toDb = [] (double loud, double soft)
    {
        return 20.0 * std::log10 (std::max (loud, 1.0e-12) / std::max (soft, 1.0e-12));
    };
    const auto fundamentalDrop = toDb (loudSpectrum[0], softSpectrum[0]);
    const auto bandDrop = toDb (loudSpectrum[1], softSpectrum[1]);
    std::cout << "dynamic 1.0 -> 0.3: fundamental " << std::fixed << std::setprecision (2)
              << fundamentalDrop << " dB, 2.4-4.7 kHz " << bandDrop << " dB\n";
    expect (fundamentalDrop > 8.0 && fundamentalDrop < 18.0,
            "the dynamic did not move the level by a plausible amount");
    expect (bandDrop - fundamentalDrop > 2.2,
            "the dynamic is a fader: it did not roll the source spectrum off as it fell");

    // ... and proportionally breathier, measured where the render loop reads it.
    const auto breathBalanceAt = [] (float dynamics)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.breath = 0.40f;
        parameters.dynamics = dynamics;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.5));
        const auto drives = vocalor::VoiceEngineTestAccess::dynamicDrives (engine);
        return static_cast<double> (drives[1])
             / std::max (static_cast<double> (drives[0]), 1.0e-9);
    };
    expect (breathBalanceAt (0.30f) > 2.0 * breathBalanceAt (1.0f),
            "a soft note is not proportionally breathier than a loud one");

    // Pitch bend, on a note that is already sounding.
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (steadyParameters());
        engine.noteOn (60, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.6));
        const auto unbent = static_cast<double> (
            vocalor::VoiceEngineTestAccess::frequencyForRoot (engine, 60));
        expect (unbent > 255.0 && unbent < 268.0,
                "the unbent reference note was not near middle C");

        engine.setPitchBend (2.0f);
        render (engine, static_cast<int> (sampleRate * 0.2));
        const auto up = static_cast<double> (
            vocalor::VoiceEngineTestAccess::frequencyForRoot (engine, 60));
        expect (std::abs (up / unbent - std::exp2 (2.0 / 12.0)) < 0.002,
                "a two-semitone bend did not produce the two-semitone frequency ratio");

        engine.setPitchBend (-12.0f);
        render (engine, static_cast<int> (sampleRate * 0.2));
        const auto down = static_cast<double> (
            vocalor::VoiceEngineTestAccess::frequencyForRoot (engine, 60));
        expect (std::abs (down / unbent - 0.5) < 0.002,
                "an octave bend down did not halve the frequency");

        // A non-finite bend must not reach the oscillator.
        engine.setPitchBend (std::numeric_limits<float>::quiet_NaN());
        const auto recovered = render (engine, static_cast<int> (sampleRate * 0.2));
        expect (recovered.finite, "a non-finite pitch bend produced non-finite audio");
    }

    // Sustain pedal: a note-off arriving under a held pedal is deferred, not
    // dropped, and pedal-up delivers it through the ordinary release path.
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (steadyParameters());
        engine.noteOn (60, 0.80f);
        render (engine, blockSize);
        engine.setSustainPedal (true);
        engine.noteOff (60);
        render (engine, static_cast<int> (sampleRate * 0.3));
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (engine) == 60,
                "the sustain pedal did not hold a note through its note-off");

        engine.setSustainPedal (false);
        render (engine, static_cast<int> (sampleRate * 0.05));
        expect (vocalor::VoiceEngineTestAccess::soundingMidiNote (engine) == -1,
                "releasing the sustain pedal did not release the note it was holding");
        const auto tail = render (engine, static_cast<int> (sampleRate * 3.0));
        expect (tail.finite && engine.getActiveVoiceCount() == 0,
                "the note the pedal released never finished its release");
    }

    // Expression is a level trim and nothing else: half expression has to be
    // the same render at half the amplitude, sample for sample.
    {
        const auto renderAt = [] (float expression)
        {
            vocalor::VoiceEngine engine;
            engine.prepare (sampleRate, blockSize);
            engine.reset();
            engine.setParameters (steadyParameters());
            engine.setExpression (expression);
            engine.noteOn (60, 0.80f);
            renderMono (engine, static_cast<int> (sampleRate * 0.4));
            return renderMono (engine, static_cast<int> (sampleRate * 0.4));
        };

        const auto loud = renderAt (1.0f);
        const auto soft = renderAt (0.5f);
        double peak = 0.0;
        double worst = 0.0;
        for (std::size_t i = 0; i < loud.size(); ++i)
        {
            peak = std::max (peak, std::abs (static_cast<double> (loud[i])));
            worst = std::max (worst, std::abs (static_cast<double> (soft[i])
                                               - 0.5 * static_cast<double> (loud[i])));
        }
        expect (peak > 1.0e-4, "the expression reference render was silent");
        expect (worst < 1.0e-4 * peak,
                "expression changed the sound rather than only its level");
    }

    // The mod wheel takes the dynamic over for good the first time it moves.
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.dynamics = 1.0f;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.2));
        const auto ownedBy = [&engine]
        {
            return vocalor::VoiceEngineTestAccess::dynamicDrives (engine)[2];
        };
        expect (std::abs (ownedBy() - 1.0f) < 1.0e-3f,
                "the host dynamic parameter did not own the level before the wheel moved");

        engine.setModWheel (0.25f);
        render (engine, static_cast<int> (sampleRate * 0.4));
        expect (std::abs (ownedBy() - 0.25f) < 0.01f,
                "the mod wheel did not take over the dynamic level");

        parameters.dynamics = 0.90f;
        engine.setParameters (parameters);
        render (engine, static_cast<int> (sampleRate * 0.4));
        expect (std::abs (ownedBy() - 0.25f) < 0.01f,
                "the host parameter overrode the mod wheel after the wheel had moved");

        engine.resetControllers();
        render (engine, static_cast<int> (sampleRate * 0.4));
        expect (std::abs (ownedBy() - 0.90f) < 0.01f,
                "resetting the controllers did not hand the dynamic back to the host");
    }
}
} // namespace

int main()
{
    testRenderMatrix();
    testReleaseCompletes();
    testAllSoundOffIsImmediate();
    testIdleStateAdvancementAndAutomation();
    testVowelSpaceModel();
    testDisplayMathHelpers();
    testVowelMorphAndFormantShift();
    testGlideAndLegato();
    testRoomSizeGeometry();
    testSampleRateInvariance();
    testHumanisationDepthIsRateInvariant();
    testTractLevelStability();
    testParallelFormantBank();
    testEnsembleSizeIsExact();
    testTractCoefficientSmoothing();
    testSingerFormantAndLipZero();
    testPerformanceExpression();
    testDenormalAndNaNSafety();
    testParameterSmoothingHasNoZipper();
    testDisplayStateTracksTheEngine();
    testRoughPerformance();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Vocalor DSP check(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Vocalor DSP checks passed.\n";
    return EXIT_SUCCESS;
}
