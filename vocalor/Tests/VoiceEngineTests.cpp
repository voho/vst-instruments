#include "DSP/Presets.h"
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
#include <set>
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
            consider(voice.sourceTilt);
            consider(voice.sourceSlow);
            consider(voice.sourceSlower);
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

    /** Sounding frequency of every held voice, in Hz. An ensemble puts twelve
        of them on one note, and how far apart they sit is what separates a
        section from one thick voice. */
    static std::vector<float> soundingFrequencies(const VoiceEngine& engine)
    {
        std::vector<float> result;
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                result.push_back(voice.phaseIncrement * static_cast<float>(engine.sampleRate_));
        return result;
    }

    /** Sounding frequency of the first held voice on @c midiNote, in Hz, or 0
        if that note is not sounding. Chord mode puts every member of the triad
        on a different sounding note behind one root, so this is what the
        intonation of an interval has to be measured from. */
    static float frequencyForNote(const VoiceEngine& engine, int midiNote) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing && voice.midiNote == midiNote)
                return voice.phaseIncrement * static_cast<float>(engine.sampleRate_);
        return 0.0f;
    }

    /** The formant targets of the first sounding voice, after the per-singer
        anatomy and the pitch-dependent tuning the chunk targets know nothing
        about. Zeroes if nothing is sounding. */
    static std::array<float, kFormantCount> voiceFormants(const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
        {
            if (! voice.active || voice.releasing)
                continue;
            std::array<float, kFormantCount> result {};
            for (int i = 0; i < kFormantCount; ++i)
                result[static_cast<std::size_t>(i)] = voice.formantHz[static_cast<std::size_t>(i)];
            return result;
        }
        return {};
    }

    /** The two per-sample level scalars the dynamic reaches, as the render loop
        sees them: the voiced drive and the aspiration drive. Their ratio is the
        breath-to-voice balance, which has to rise as the dynamic falls. */
    static std::array<float, 3> dynamicDrives(const VoiceEngine& engine) noexcept
    {
        return { engine.voicedScaleAt_[0], engine.airLevelAt_[0], engine.smoothedDynamics_ };
    }

    /** Forces the source-tension ramp depth. A note-on starts the glottal
        source this far below the block's tension and firms up onto it; passing
        0 renders the same note with the ramp switched out, which is how the
        tract half of the onset and the source half are told apart inside one
        binary. Applied on every control update, so a note already sounding
        follows it. */
    static void setSourceTensionRampDepth(VoiceEngine& engine, float depth) noexcept
    {
        engine.sourceTensionRampDepth_ = depth;
    }

    /** How far below the block's tension the sounding voice's glottal source
        actually started, after the note's own velocity has scaled the engine's
        depth. Read from the voice rather than from a render because the two
        glottal prototypes are crossfaded in the time domain: their harmonics
        cancel non-monotonically at intermediate tensions, so no band share
        moves monotonically with this. Measured on the shipping engine, the
        2-5 kHz share at 10-35 ms with the ramp in minus the same share with it
        forced out reads 3.06, 1.53, 2.14 and 2.17 dB at velocity 0.10, 0.40,
        0.80 and 1.00, which is the notch rather than the depth. */
    static float sourceTensionSag(const VoiceEngine& engine) noexcept
    {
        for (const auto& voice : engine.voices_)
            if (voice.active && ! voice.releasing)
                return voice.tensionSag;
        return 0.0f;
    }

    /** The engine's own ramp depth, which is the value the reference velocity
        0.80 resolves to. */
    static float sourceTensionRampDepth(const VoiceEngine& engine) noexcept
    {
        return engine.sourceTensionRampDepth_;
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
    // The epilarynx cluster reads the tension smoother, so the tract is only
    // comparable between engines once that smoother has settled in both.
    render (reference, static_cast<int> (sampleRate * 0.3));
    render (morphed, static_cast<int> (sampleRate * 0.3));
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
    const auto shiftedAudio = render (shiftedEngine, static_cast<int> (sampleRate * 0.4));
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
    // The two-formant onset stage this block used to check for a matching
    // normalisation no longer exists: every voice renders the whole bank from
    // its first sample. testOnsetSpectrum is what holds that now.

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

/** Reference settings for the level and articulation measurements below: the
    factory defaults with everything that randomises a take switched off, so the
    render is deterministic and the output gain is out of the way. */
vocalor::EngineParameters makeReferenceParameters()
{
    vocalor::EngineParameters parameters;
    parameters.humanize = 0.0f;
    parameters.vibrato = 0.0f;
    parameters.spread = 0.0f;
    parameters.room = 0.0f;
    parameters.outputGain = 1.0f;
    return parameters;
}

/** The tract level is calibrated so a solo AAH at the default settings lands at
    a known level, and the whole voice is expressed in time constants and corner
    frequencies so that level does not depend on the sample rate.

    A broadband gain quietly inserted into the excitation path shows up here as
    an equal shift at every rate. That is what a lip-radiation zero would be:
    the wavetable is already a glottal flow *derivative*, so radiation is
    modelled once, and applying it again costs level without changing timbre. */
void testSourceLevelCalibration()
{
    // Measured from the calibrated engine. The window is loose enough for
    // platform floating-point differences and far tighter than the ~1.9 dB a
    // second radiation stage across the excitation path costs.
    // Moved from -15.53 when the source slope started following the note's own
    // loudness: the reference render is at velocity 0.85, which is 1.4 dB below
    // full voice, so its partials above the 850 Hz shelf corner sit 2.8 dB down
    // and the broadband RMS with them. Nothing about the source's absolute
    // calibration or its single radiation accounting moved -- at velocity 1.00
    // and full dynamic the shelf is exactly transparent.
    constexpr double referenceRmsDb = -17.03;
    constexpr double toleranceDb = 0.9;

    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (makeReferenceParameters());
        engine.noteOn (60, 0.85f);

        render (engine, static_cast<int> (sampleRate * 0.6));   // past the onset
        const auto held = render (engine, static_cast<int> (sampleRate * 1.0));
        const auto levelDb = 20.0 * std::log10 (held.rms() + 1.0e-30);

        const auto label = std::to_string (static_cast<int> (sampleRate)) + " Hz";
        expect (held.finite, "reference AAH at " + label + " produced a NaN or infinity");
        expect (std::abs (levelDb - referenceRmsDb) < toleranceDb,
                "reference AAH at " + label + " rendered at "
                    + std::to_string (levelDb) + " dB, outside the calibrated "
                    + std::to_string (referenceRmsDb) + " +/- "
                    + std::to_string (toleranceDb) + " dB");
    }
}

/** Every factory preset has to be playable.

    The table lives in the JUCE-free core precisely so this can be checked here:
    a preset that renders silence, clips, or carries a value the engine clamps
    away is a defect the suite finds rather than one a player does.
*/
void testFactoryPresets()
{
    constexpr auto sampleRate = 48000.0;
    const auto count = vocalor::factoryPresetCount();
    expect (count >= 8, "the factory bank is too small to open a session on");

    std::set<std::string> names;
    for (int index = 0; index < count; ++index)
    {
        const auto& preset = vocalor::factoryPreset (index);
        const std::string name = preset.name != nullptr ? preset.name : "";
        expect (! name.empty(), "factory preset " + std::to_string (index) + " has no name");
        expect (names.insert (name).second, "two factory presets share the name " + name);

        // Nothing may be silently clamped away: what the table says is what the
        // engine gets.
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (preset.parameters);

        const auto& wanted = preset.parameters;
        expect (wanted.choirSize >= 2 && wanted.choirSize <= 12,
                name + " asks for a singer count the engine cannot render exactly");
        expect (wanted.formantShift >= -12.0f && wanted.formantShift <= 12.0f,
                name + " asks for a formant shift outside the published range");
        expect (wanted.outputGain > 0.0f && wanted.outputGain <= 2.0f,
                name + " asks for an output gain outside the published range");
        for (const float unit : { wanted.breath, wanted.resonance, wanted.vibrato,
                                  wanted.humanize, wanted.spread, wanted.tension,
                                  wanted.room, wanted.vowelX, wanted.vowelY,
                                  wanted.vowelMorph, wanted.glide, wanted.roomSize,
                                  wanted.dynamics, wanted.intonation, wanted.nasal })
            expect (unit >= 0.0f && unit <= 1.0f,
                    name + " carries a normalised value outside 0..1");

        // A held note and a held triad, because chord and choir presets take
        // different paths through the allocator.
        engine.noteOn (57, 0.85f);
        engine.noteOn (64, 0.85f);
        const auto held = render (engine, static_cast<int> (sampleRate * 0.6));
        expect (held.finite, name + " produced a NaN or an infinity");
        expect (held.rms() > 1.0e-5, name + " rendered silence");
        expect (held.peak < 4.0, name + " exceeded the amplitude guardrail");

        engine.noteOff (57);
        engine.noteOff (64);
        const auto tail = render (engine, static_cast<int> (sampleRate * 4.0));
        expect (tail.finite, name + " produced invalid audio during its release");
        expect (engine.getActiveVoiceCount() == 0, name + " never finished releasing");
    }

    // ... and it has to be playable at the level it was voiced at. Nothing else
    // in the suite makes a re-trim mandatory: the checks above pass a preset
    // left 10 dB quiet, and testSourceLevelCalibration renders at Dynamics 1.00,
    // where the voiced gain does not move at all. These are the levels the bank
    // shipped at before the dynamic grew to 30 dB and the source slope started
    // following the note's own loudness, both of which lower any preset not sung
    // at full velocity and full dynamic. Measured on the shipping engine: two
    // notes held at 57 and 64 at velocity 0.85, 1 s of stereo RMS from t = 0.6 s
    // at 48 kHz.
    struct Level { const char* name; double db; };
    constexpr std::array<Level, 12> shipped { {
        { "Init Soprano", -19.04 }, { "Intimate Alto", -25.06 },
        { "Pressed Tenor", -20.28 }, { "Legato Soloist", -25.41 },
        { "Breath And Air", -29.81 }, { "Warm Bass Choir", -20.98 },
        { "Cathedral Ensemble", -22.85 }, { "Closed Mouth Hum", -34.88 },
        { "Small Voices", -24.72 }, { "Vowel Morph Pad", -27.79 },
        { "Locked Major Chorale", -26.34 }, { "Airy Minor Pad", -29.62 } } };

    for (const auto& entry : shipped)
    {
        int index = -1;
        for (int i = 0; i < count; ++i)
            if (std::string (vocalor::factoryPreset (i).name) == entry.name)
                index = i;
        expect (index >= 0, std::string (entry.name) + " is no longer in the factory bank");
        if (index < 0)
            continue;

        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (vocalor::factoryPreset (index).parameters);
        engine.noteOn (57, 0.85f);
        engine.noteOn (64, 0.85f);
        render (engine, static_cast<int> (sampleRate * 0.6));
        const auto held = render (engine, static_cast<int> (sampleRate * 1.0));
        const auto levelDb = 20.0 * std::log10 (held.rms() + 1.0e-30);
        std::cout << "preset level: " << std::left << std::setw (22) << entry.name
                  << std::right << std::fixed << std::setprecision (2) << levelDb
                  << " dB against " << entry.db << " dB\n";
        expect (std::abs (levelDb - entry.db) <= 1.0,
                std::string (entry.name) + " renders at " + std::to_string (levelDb)
                    + " dB against the " + std::to_string (entry.db)
                    + " dB it shipped at: the preset bank was not re-trimmed");
    }
}

/** The singer's formant is a cluster, not a boost.

    The 1.1 engine raised F3 by 12 % and F4 by 6 % of Tension and left them
    where they were. Three formants 700 Hz apart with slightly more gain each
    are still three formants; the peak that lets an unamplified voice carry over
    an orchestra comes from narrowing the epilaryngeal tube until F3, F4 and F5
    collapse into one.
*/
void testSingersFormantCluster()
{
    constexpr auto sampleRate = 48000.0;
    constexpr double fundamental = 146.832;   // D3, so the comb resolves the peak

    const auto probe = [] (float tension, std::array<float, vocalor::kFormantCount>& formants)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.profile = vocalor::VoiceProfile::Male;
        parameters.tension = tension;
        engine.setParameters (parameters);
        engine.noteOn (50, 0.80f);
        renderMono (engine, static_cast<int> (sampleRate * 0.6));
        const auto samples = renderMono (engine, static_cast<int> (sampleRate * 0.6));
        formants = vocalor::VoiceEngineTestAccess::chunkFormants (engine);

        const auto energy = [&samples] (int first, int last)
        {
            double total = 0.0;
            for (int harmonic = first; harmonic <= last; ++harmonic)
            {
                const auto magnitude = harmonicMagnitude (
                    samples, fundamental * harmonic, sampleRate);
                total += magnitude * magnitude;
            }
            return total;
        };
        // 2.05-4.0 kHz against everything from the fundamental to 6 kHz.
        return energy (14, 27) / std::max (energy (1, 41), 1.0e-18);
    };

    std::array<float, vocalor::kFormantCount> relaxed {};
    std::array<float, vocalor::kFormantCount> pressed {};
    const auto relaxedRatio = probe (0.0f, relaxed);
    const auto pressedRatio = probe (0.95f, pressed);

    const auto relaxedSpan = relaxed[4] - relaxed[2];
    const auto pressedSpan = pressed[4] - pressed[2];
    const auto ringDb = 10.0 * std::log10 (std::max (pressedRatio, 1.0e-18)
                                           / std::max (relaxedRatio, 1.0e-18));
    std::cout << "epilarynx: F3-F5 span " << std::fixed << std::setprecision (0)
              << relaxedSpan << " -> " << pressedSpan << " Hz, 2.05-4 kHz share "
              << std::setprecision (1) << ringDb << " dB\n";

    // At rest the tract has to be exactly the vowel it always was: the male
    // open anchor is F3 2440, F4 3250, F5 4300.
    expect (std::abs (relaxed[2] - 2440.0f) < 1.0f && std::abs (relaxed[3] - 3250.0f) < 1.0f
                && std::abs (relaxed[4] - 4300.0f) < 1.0f,
            "a relaxed larynx no longer leaves the vowel's own upper formants alone");

    expect (pressedSpan < 0.72f * relaxedSpan,
            "the upper formants did not cluster: this is still an amplitude boost");
    expect (pressed[2] > relaxed[2] && pressed[4] < relaxed[4],
            "the cluster did not close from both sides toward the epilarynx resonance");
    expect (ringDb > 3.0,
            "clustering the upper formants did not put energy where a voice carries");

    // Narrowing the epilarynx is a phonation mode, not a vowel change. F1 and
    // F2 carry the vowel's identity and belong to the vowel, so the epilarynx
    // must not touch them however hard the glottis is pressed.
    for (const int formant : { 0, 1 })
    {
        const auto index = static_cast<std::size_t> (formant);
        expect (std::abs (pressed[index] - relaxed[index]) <= 1.0e-3f * relaxed[index],
                "tension moved F" + std::to_string (formant + 1)
                    + ", which carries the vowel rather than the singer's formant");
    }
}

/** Energy in [lowHz, highHz) of a Blackman-Harris window of @c length samples
    starting at @c start, in dB. Per-bin DFT: the windows are 25 ms, which is
    1200 bins at 48 kHz, and the bands are narrow, so this costs less than the
    renders it measures. */
double windowedBandDb (const std::vector<float>& samples, int start, int length,
                       double sampleRate, double lowHz, double highHz)
{
    constexpr double pi = 3.14159265358979323846;
    std::vector<double> window (static_cast<std::size_t> (length));
    for (int i = 0; i < length; ++i)
    {
        const auto angle = 2.0 * pi * i / (length - 1.0);
        window[static_cast<std::size_t> (i)] = 0.35875 - 0.48829 * std::cos (angle)
                                             + 0.14128 * std::cos (2.0 * angle)
                                             - 0.01168 * std::cos (3.0 * angle);
    }

    const auto binHz = sampleRate / length;
    const auto firstBin = std::max (1, static_cast<int> (std::ceil (lowHz / binHz)));
    const auto lastBin = std::min (length / 2 - 1, static_cast<int> (std::floor (highHz / binHz)));
    double total = 0.0;
    for (int bin = firstBin; bin <= lastBin; ++bin)
    {
        double real = 0.0;
        double imaginary = 0.0;
        for (int i = 0; i < length; ++i)
        {
            const auto value = samples[static_cast<std::size_t> (start + i)]
                             * window[static_cast<std::size_t> (i)];
            const auto angle = 2.0 * pi * bin * i / length;
            real += value * std::cos (angle);
            imaginary -= value * std::sin (angle);
        }
        total += real * real + imaginary * imaginary;
    }
    return 10.0 * std::log10 (std::max (total, 1.0e-300));
}

/** A vocal tract is a fixed geometry with fixed poles, so it is fully formed
    before the first glottal pulse reaches it and the singer's-formant cluster
    is present in the first cycle. The engine used to render the first 50-70 ms
    through a two-formant stage and lerp the full bank in over the following
    65-125 ms, which left the 2000-3300 Hz band 17.68 dB below its sustain share
    at 10-35 ms on a male D3.

    What does develop at an onset is the source: the folds start abducted and
    lax and adduct over the first tens of milliseconds. That is the second half
    of this test, and it is measured separately, because a ramp that does
    nothing is otherwise indistinguishable from a ramp that works.
*/
void testOnsetSpectrum()
{
    constexpr auto sampleRate = 48000.0;
    constexpr int window = 1200;              // 25 ms
    const auto at = [] (double milliseconds)
    {
        return static_cast<int> (sampleRate * milliseconds * 0.001);
    };

    // The published baseline for gap 9 was measured on the engine's own shipped
    // defaults rather than on steadyParameters(), which runs a narrower
    // resonance and a wider spread; keeping to the defaults is what makes the
    // numbers quoted below comparable with the ones in the plan.
    const auto onsetParameters = [] (vocalor::VoiceProfile profile, float tension)
    {
        vocalor::EngineParameters parameters;
        parameters.profile = profile;
        parameters.mode = vocalor::PerformanceMode::Solo;
        parameters.vowel = vocalor::Vowel::Aah;
        parameters.tension = tension;
        parameters.breath = 0.10f;
        parameters.humanize = 0.0f;
        parameters.vibrato = 0.0f;
        parameters.room = 0.0f;
        return parameters;
    };

    /** A male AAH on D3 at Tension 0.90, Breath 0.10, Humanize 0, Vibrato 0,
        either at the shipping source-tension ramp depth or with that ramp
        forced out. The ramp-on leg does not name a depth, so a depth shipped at
        zero fails here rather than passing quietly. */
    const auto probe = [&] (bool rampOff)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (onsetParameters (vocalor::VoiceProfile::Male, 0.90f));
        if (rampOff)
            vocalor::VoiceEngineTestAccess::setSourceTensionRampDepth (engine, 0.0f);
        engine.noteOn (50, 0.80f);
        return renderMono (engine, static_cast<int> (sampleRate * 1.2));
    };

    const auto rampOff = probe (true);
    const auto rampOn = probe (false);

    const auto share = [&] (const std::vector<float>& samples, double milliseconds,
                            double lowHz, double highHz)
    {
        return windowedBandDb (samples, at (milliseconds), window, sampleRate, lowHz, highHz)
             - windowedBandDb (samples, at (milliseconds), window, sampleRate, 100.0, 900.0);
    };
    /** How far the singer's-formant band sits below the share it reaches in the
        sustain, in dB. This is gap 9's own measurement. */
    const auto deficit = [&] (const std::vector<float>& samples, double milliseconds)
    {
        return share (samples, 1000.0, 2000.0, 3300.0)
             - share (samples, milliseconds, 2000.0, 3300.0);
    };

    const auto offDeficit = deficit (rampOff, 10.0);
    const auto onDeficit = deficit (rampOn, 10.0);
    const auto offAir = share (rampOff, 10.0, 5000.0, 18000.0);
    const auto onAir = share (rampOn, 10.0, 5000.0, 18000.0);
    std::cout << "onset: 2-3.3 kHz deficit at 10-35 ms " << std::fixed << std::setprecision (2)
              << offDeficit << " dB with the source ramp off, " << onDeficit
              << " dB with it on; 5-18 kHz share " << offAir << " -> " << onAir << " dB\n";

    // The tract is present from sample zero. With the source ramp switched out
    // nothing is left to develop but the envelope, so the band the singer's
    // formant sits in has to arrive with the note. Measured 2.79 dB, against
    // 17.68 dB for the two-formant onset stage this replaces.
    expect (offDeficit <= 3.5,
            "the upper formants are still missing from the attack");

    // And the gap stays closed once the source ramp is doing its work.
    // Measured 0.57 dB.
    expect (onDeficit <= 8.0,
            "the source-tension ramp reopened the singer's-formant gap at the onset");

    // The ramp is doing the remaining work. A lax fold configuration is an
    // abducted one, so the note has to speak with more aspiration per unit of
    // voiced output than the same note started at the block's tension.
    // Measured 5.62 dB.
    expect (onAir - offAir >= 3.0,
            "the source-tension ramp does not make the onset breathier than a note "
            "started at the block's tension");

    // It is an onset gesture, so by the sustain the two renders have to be the
    // same instrument. Measured within 0.001 dB in all three bands.
    for (const auto& band : { std::pair { 100.0, 900.0 }, std::pair { 2000.0, 3300.0 },
                              std::pair { 5000.0, 18000.0 } })
    {
        const auto offBand = windowedBandDb (rampOff, at (1000.0), window, sampleRate,
                                             band.first, band.second);
        const auto onBand = windowedBandDb (rampOn, at (1000.0), window, sampleRate,
                                            band.first, band.second);
        expect (std::abs (onBand - offBand) <= 0.5,
                "the source-tension ramp is still moving the sustain a second into the note");
    }

    // Direction test only: the aspiration-to-voiced ratio falls across the
    // onset rather than rising. Measured 26.20 -> 11.43 -> 3.59 dB above the
    // sustain share.
    const auto airEarly = share (rampOn, 10.0, 5000.0, 18000.0);
    const auto airMiddle = share (rampOn, 70.0, 5000.0, 18000.0);
    const auto airLate = share (rampOn, 180.0, 5000.0, 18000.0);
    expect (airEarly > airMiddle && airMiddle > airLate,
            "the aspiration no longer falls away across the onset");

    // And it must not click. What a click is, is energy in the note's first
    // instants that the envelope did not put there -- the tract's own transient
    // response to a source that starts from silence. The window has to be
    // shorter than the fastest attack the engine produces or it measures the
    // attack instead: velocity now sets the envelope time constant and an
    // accented note reaches amplitude on 4.5 ms, so the 2 ms window this test
    // shipped with is a third of the way up the intended envelope rather than
    // ahead of it. 1 ms is under a quarter of that time constant and under a
    // third of a glottal period at C4. Measured, the narrowest of these
    // eighteen is 28.44 dB.
    struct Entry { int midi; vocalor::VoiceProfile profile; const char* name; };
    for (const auto entry : { Entry { 50, vocalor::VoiceProfile::Male, "male D3" },
                              Entry { 60, vocalor::VoiceProfile::Female, "female C4" },
                              Entry { 84, vocalor::VoiceProfile::Female, "female C6" } })
    {
        for (const float tension : { 0.30f, 0.90f })
        {
            // Across the velocity range, because velocity is what sets the
            // attack: the loudest, fastest onset is the one that can click.
            for (const float velocity : { 0.30f, 0.80f, 1.00f })
            {
                vocalor::VoiceEngine engine;
                engine.prepare (sampleRate, blockSize);
                engine.reset();
                engine.setParameters (onsetParameters (entry.profile, tension));
                engine.noteOn (entry.midi, velocity);
                const auto samples = renderMono (engine, static_cast<int> (sampleRate * 1.2));

                double firstPeak = 0.0;
                for (int i = 0; i < at (1.0); ++i)
                    firstPeak = std::max (firstPeak,
                                          std::abs (static_cast<double> (samples[static_cast<std::size_t> (i)])));
                double sustainPeak = 0.0;
                for (int i = at (900.0); i < at (1100.0); ++i)
                    sustainPeak = std::max (sustainPeak,
                                            std::abs (static_cast<double> (samples[static_cast<std::size_t> (i)])));
                const auto below = 20.0 * std::log10 (sustainPeak / std::max (firstPeak, 1.0e-30));
                std::cout << "onset: " << entry.name << " at Tension " << std::setprecision (2)
                          << tension << ", velocity " << velocity << " enters " << below
                          << " dB below its sustain peak\n";
                expect (below >= 24.0,
                        std::string (entry.name) + " starts with a click: its first 1 ms peak "
                            "is within 24 dB of the sustain peak");
            }
        }
    }
}

/** Solo, one note, nothing moving but the thing under test. The published
    baselines for gaps 11 and 21 were measured on the engine's own shipped
    defaults rather than on steadyParameters(), so these two tests build from
    EngineParameters directly, as testOnsetSpectrum does. */
vocalor::EngineParameters dynamicProtocolParameters()
{
    vocalor::EngineParameters parameters;
    parameters.mode = vocalor::PerformanceMode::Solo;
    parameters.vowel = vocalor::Vowel::Aah;
    parameters.humanize = 0.0f;
    parameters.vibrato = 0.0f;
    parameters.room = 0.0f;
    parameters.dynamics = 1.0f;
    return parameters;
}

/** 10-90 % rise of a four-period sliding-peak envelope, in milliseconds.

    Four periods rather than a fixed window: a 2 ms window at C4 is shorter than
    the 3.82 ms glottal period, so it does not smooth the waveform out at all
    and the crossings land on within-period ripple -- the same rise reads 7.8 ms
    at 2 ms, 13.2 at 8 ms and 27.8 at 30 ms, which is a metric whose answer is
    set by its own window length. The envelope is the peak of the four periods
    ending at each sample, sampled on a 0.5 ms grid, and the two crossings are
    interpolated linearly between grid points. The steady value is the envelope
    averaged over 0.8-1.0 s, which is well past any attack this engine produces.
*/
double onsetRiseMs (const std::vector<float>& samples, double fundamentalHz,
                    double sampleRate)
{
    const auto window = static_cast<int> (std::lround (4.0 * sampleRate / fundamentalHz));
    const auto count = static_cast<int> (samples.size());
    std::vector<double> envelope (static_cast<std::size_t> (count), 0.0);
    // A running maximum over a sliding window, kept O(n) by rescanning only when
    // the sample that held the maximum falls out of it.
    double running = 0.0;
    int runningAt = -1;
    for (int i = 0; i < count; ++i)
    {
        const auto magnitude = std::abs (static_cast<double> (samples[static_cast<std::size_t> (i)]));
        if (magnitude >= running)
        {
            running = magnitude;
            runningAt = i;
        }
        else if (runningAt <= i - window)
        {
            running = 0.0;
            for (int k = std::max (0, i - window + 1); k <= i; ++k)
            {
                const auto value = std::abs (static_cast<double> (samples[static_cast<std::size_t> (k)]));
                if (value >= running)
                {
                    running = value;
                    runningAt = k;
                }
            }
        }
        envelope[static_cast<std::size_t> (i)] = running;
    }

    const auto from = static_cast<int> (0.8 * sampleRate);
    const auto to = std::min (count, static_cast<int> (1.0 * sampleRate));
    double steady = 0.0;
    for (int i = from; i < to; ++i)
        steady += envelope[static_cast<std::size_t> (i)];
    steady /= std::max (1, to - from);

    const auto step = static_cast<int> (0.0005 * sampleRate);
    const auto crossing = [&] (double fraction)
    {
        const auto target = fraction * steady;
        for (int i = step; i < count; i += step)
        {
            const auto before = envelope[static_cast<std::size_t> (i - step)];
            const auto after = envelope[static_cast<std::size_t> (i)];
            if (before < target && after >= target)
            {
                const auto within = (target - before) / std::max (after - before, 1.0e-30);
                return 1000.0 * (static_cast<double> (i - step) + within * step) / sampleRate;
            }
        }
        return -1.0;
    };
    return crossing (0.9) - crossing (0.1);
}

/** Energy in [lowHz, highHz) of a held note's sustain, in dB. Two 100 ms
    windows, because at Humanize 0 and Vibrato 0 the sustain is stationary. */
double sustainBandDb (const std::vector<float>& samples, double sampleRate,
                      double lowHz, double highHz)
{
    const auto window = static_cast<int> (0.100 * sampleRate);
    double total = 0.0;
    for (const double at : { 0.80, 1.00 })
        total += windowedBandDb (samples, static_cast<int> (at * sampleRate), window,
                                 sampleRate, lowHz, highHz);
    return 0.5 * total;
}

/** How many decibels the 2-5 kHz band moves per decibel of 150-800 Hz between
    two renders. Sundberg's measurement is that partials above 1 kHz rise about
    twice as fast in dB as overall sound pressure level, so this is 2 in a real
    singer and 1 in a fader. Read at Breath 0.00: at any breath setting the
    aspiration, which loses only 7.2 dB across the dynamic, floors the 2-5 kHz
    band once the voiced component has fallen 30 dB, and the ratio then measures
    the noise rather than the source. */
double presenceRatio (const std::vector<float>& soft, const std::vector<float>& loud,
                      double sampleRate)
{
    const auto low = sustainBandDb (loud, sampleRate, 150.0, 800.0)
                   - sustainBandDb (soft, sampleRate, 150.0, 800.0);
    const auto high = sustainBandDb (loud, sampleRate, 2000.0, 5000.0)
                    - sustainBandDb (soft, sampleRate, 2000.0, 5000.0);
    std::cout << "  150-800 Hz " << std::fixed << std::setprecision (2) << low
              << " dB, 2-5 kHz " << high << " dB, ratio " << high / low << '\n';
    return high / low;
}

/** Velocity has to shape the note, not fade it.

    Velocity used to reach a gain and the corner of a one-pole source tilt and
    nothing else. In particular it never reached the envelope: attackCoefficient_
    was a per-block constant derived from Humanize alone, so a held C4 rose in
    the same 16.6 ms at velocity 0.05, 0.40 and 1.00 alike, across a 22 dB level
    span. A soft onset has to approach phonation threshold pressure slowly and
    an accented one arrives above it, so the time constant is now the note's own.
*/
void testVelocityShapesOnset()
{
    constexpr auto sampleRate = 48000.0;
    constexpr double fundamental = 261.6255653;   // C4

    const auto riseAt = [&] (float velocity, float humanize)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = dynamicProtocolParameters();
        parameters.humanize = humanize;
        engine.setParameters (parameters);
        engine.noteOn (60, velocity);
        const auto samples = renderMono (engine, static_cast<int> (sampleRate * 1.5));
        return onsetRiseMs (samples, fundamental, sampleRate);
    };

    const auto accented = riseAt (1.00f, 0.0f);
    const auto soft = riseAt (0.10f, 0.0f);
    std::cout << "onset rise at Humanize 0: velocity 1.00 " << std::fixed
              << std::setprecision (2) << accented << " ms, velocity 0.10 " << soft
              << " ms, ratio " << soft / accented << "x\n";

    // Two-sided, because both ends carry a defect. Below about 6 ms an accented
    // attack stops being an attack and becomes a click; above about 120 ms a
    // soft one stops being an onset and becomes a pad swell. Today's engine
    // fails both: 16.62 ms is over the accented ceiling and under the soft
    // floor. Measured 8.91 ms and 92.16 ms.
    expect (accented >= 6.0 && accented <= 14.0,
            "an accented attack does not reach amplitude in the 6-14 ms a hard "
            "onset takes");
    expect (soft >= 45.0 && soft <= 120.0,
            "a soft attack does not take the 45-120 ms an onset near phonation "
            "threshold takes");
    expect (soft / accented >= 2.5,
            "velocity is still not the envelope's time constant");

    // Humanize published as the dial that loosens a take, and the attack time is
    // one of the things it loosens. It survives as a multiplier on the time
    // constant rather than as its source. Measured 8.91 -> 23.53 ms.
    const auto loose = riseAt (1.00f, 1.0f);
    std::cout << "onset rise at velocity 1.00: Humanize 0 " << accented
              << " ms, Humanize 1 " << loose << " ms\n";
    expect (loose >= 2.0 * accented,
            "Humanize no longer stretches the attack it is published as loosening");

    // Velocity also reaches the source-tension ramp, so a soft attack is lax as
    // well as slow: the folds of a hard onset start close to the adducted
    // configuration they settle on, a soft one starts further from it. The
    // reference is velocity 0.80, which is what leaves the onset the first pass
    // measured exactly where it was.
    const auto sagAt = [&] (float velocity)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        engine.setParameters (dynamicProtocolParameters());
        engine.noteOn (60, velocity);
        render (engine, blockSize);
        return vocalor::VoiceEngineTestAccess::sourceTensionSag (engine);
    };
    const auto softSag = sagAt (0.10f);
    const auto referenceSag = sagAt (0.80f);
    const auto hardSag = sagAt (1.00f);
    std::cout << "source-tension ramp depth: velocity 0.10 " << softSag
              << ", 0.80 " << referenceSag << ", 1.00 " << hardSag << '\n';
    const vocalor::VoiceEngine untouched;
    expect (std::abs (referenceSag
                          - vocalor::VoiceEngineTestAccess::sourceTensionRampDepth (untouched))
                <= 0.01f,
            "the reference velocity no longer resolves to the engine's own ramp depth");
    expect (softSag >= 1.3f * hardSag,
            "velocity does not reach the source-tension ramp: a soft attack is "
            "slow but not lax");

    // And velocity has to reach the spectrum, not only the level.
    const auto renderAt = [&] (float velocity)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = dynamicProtocolParameters();
        parameters.breath = 0.0f;
        engine.setParameters (parameters);
        engine.noteOn (60, velocity);
        return renderMono (engine, static_cast<int> (sampleRate * 1.3));
    };
    std::cout << "velocity 0.05 -> 1.00:\n";
    const auto ratio = presenceRatio (renderAt (0.05f), renderAt (1.00f), sampleRate);
    // 1.066 before this step: velocity was a fader with a very slight tilt on
    // it. Measured 1.53. Two cascaded first-order shelves cannot reach the 2.0
    // Sundberg measures across these two bands -- see the plan's note on the
    // 6 dB per octave per stage ceiling -- so the bound is set where the
    // mechanism actually lands with margin rather than at the physiology.
    expect (ratio >= 1.40,
            "velocity still moves the presence band no faster than the level");
}

/** The dynamic control has to cover a singer's range, and stay a spectrum
    control at the bottom of it.

    dynamicResponse's voiced gain was exp2(-3.00 * below), which is exactly
    18.06 dB by construction, against the 30-40 dB a singer covers between
    pianissimo and fortissimo. The aspiration only loses 7.2 dB over the same
    span, so once the voiced gain moves past about 35 dB the noise floors the
    result and the control stops working at its own bottom end -- which is why
    the span is measured at three breath settings rather than one.
*/
void testDynamicRange()
{
    constexpr auto sampleRate = 48000.0;

    const auto spanAt = [&] (float breath)
    {
        std::array<double, 2> level {};
        for (int end = 0; end < 2; ++end)
        {
            vocalor::VoiceEngine engine;
            engine.prepare (sampleRate, blockSize);
            engine.reset();
            auto parameters = dynamicProtocolParameters();
            parameters.breath = breath;
            parameters.outputGain = 1.0f;
            parameters.dynamics = end == 0 ? 0.0f : 1.0f;
            engine.setParameters (parameters);
            engine.noteOn (60, 0.80f);
            render (engine, static_cast<int> (sampleRate * 0.8));
            const auto held = render (engine, static_cast<int> (sampleRate * 1.0));
            level[static_cast<std::size_t> (end)] = 20.0 * std::log10 (held.rms() + 1.0e-30);
        }
        const auto span = level[1] - level[0];
        std::cout << "dynamic span at Breath " << std::fixed << std::setprecision (2)
                  << breath << ": " << span << " dB\n";
        return span;
    };

    const auto dry = spanAt (0.00f);
    const auto shipped = spanAt (0.28f);
    const auto breathy = spanAt (0.60f);

    // Measured 33.19, 33.04 and 32.41 dB, against 18.10 / 18.10 / 18.09 before.
    expect (shipped >= 28.0,
            "the dynamic control does not cover a singer's range");
    // Pinned at the shipping breath, because the aspiration is what would take
    // the bottom of the range away without any of it showing up at Breath 0.
    expect (std::abs (breathy - dry) <= 2.0,
            "the aspiration is flooring the bottom of the dynamic range");

    // And the dynamic has to be a spectrum control across that whole span, not
    // a 30 dB fader. 1.195 before this step; measured 1.53.
    const auto renderAt = [&] (float dynamics)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = dynamicProtocolParameters();
        parameters.breath = 0.0f;
        parameters.dynamics = dynamics;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        return renderMono (engine, static_cast<int> (sampleRate * 1.3));
    };
    std::cout << "dynamics 0.00 -> 1.00:\n";
    expect (presenceRatio (renderAt (0.00f), renderAt (1.00f), sampleRate) >= 1.40,
            "the dynamic still moves the presence band no faster than the level");
}

/** A vowel change is a jaw and a tongue moving, not a de-zipper.

    The 1.1 formant glides ran on 16, 9, 5, 4 and 3 ms time constants, so a
    vowel switch was over in about 50 ms; a sung vowel-to-vowel transition runs
    100-200. The engine had the mechanism for coarticulation and used it to stop
    clicks.
*/
void testCoarticulationTiming()
{
    constexpr auto sampleRate = 48000.0;

    /** 10-90 % rise time of each formant, in milliseconds, after the pad is
        stepped from @c fromX,@c fromY to @c toX,@c toY on a held note. */
    const auto riseTimes = [] (float fromX, float fromY, float toX, float toY)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.vowelMorph = 1.0f;
        parameters.vowelX = fromX;
        parameters.vowelY = fromY;
        engine.setParameters (parameters);
        engine.noteOn (57, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.4));

        const auto start = vocalor::VoiceEngineTestAccess::voiceFormants (engine);
        parameters.vowelX = toX;
        parameters.vowelY = toY;
        engine.setParameters (parameters);

        // Settle first so the destination is measured, not guessed.
        std::array<std::array<float, vocalor::kFormantCount>, 400> trace {};
        constexpr int step = 64;
        for (std::size_t frame = 0; frame < trace.size(); ++frame)
        {
            render (engine, step);
            trace[frame] = vocalor::VoiceEngineTestAccess::voiceFormants (engine);
        }
        const auto finish = trace.back();

        std::array<double, vocalor::kFormantCount> milliseconds {};
        for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
        {
            const auto index = static_cast<std::size_t> (formant);
            const auto span = finish[index] - start[index];
            if (std::abs (span) < 1.0f)
                continue;

            double tenth = -1.0;
            double ninetieth = -1.0;
            for (std::size_t frame = 0; frame < trace.size(); ++frame)
            {
                const auto progress = (trace[frame][index] - start[index]) / span;
                const auto at = 1000.0 * static_cast<double> ((frame + 1) * step) / sampleRate;
                if (tenth < 0.0 && progress >= 0.10)
                    tenth = at;
                if (ninetieth < 0.0 && progress >= 0.90)
                {
                    ninetieth = at;
                    break;
                }
            }
            milliseconds[index] = (tenth >= 0.0 && ninetieth >= 0.0) ? ninetieth - tenth : -1.0;
        }
        return milliseconds;
    };

    // Close front to open: the largest move the pad offers.
    const auto large = riseTimes (1.0f, 0.0f, 0.5f, 1.0f);
    std::cout << "vowel step /i/ -> /a/, 10-90 % rise: " << std::fixed << std::setprecision (0);
    for (int formant = 0; formant < vocalor::kFormantCount; ++formant)
        std::cout << ' ' << (formant + 1) << ':' << large[static_cast<std::size_t> (formant)] << "ms";
    std::cout << '\n';

    // F1 and F2 make the large moves here: 310 -> 850 Hz and 2790 -> 1220. F3
    // moves 500 Hz and F4 only 250, and F5 is the same 4950 Hz for both these
    // vowels, so only the first three carry a timing claim at all.
    expect (large[0] > 80.0 && large[0] < 260.0,
            "F1 did not move on a jaw's timescale after a full vowel step");
    expect (large[1] > 55.0 && large[1] < 220.0,
            "F2 did not move on a tongue's timescale after a full vowel step");
    expect (large[0] > large[1] && large[1] > large[2],
            "the formants no longer move in order of the cavity that carries them");
    expect (large[2] > 12.0,
            "F3 still switches as fast as a de-zipper on a 500 Hz move");
    // The jaw is a heavier articulator than the larynx by enough that the two
    // must not read as one shared glide. Collapsing every formant back onto a
    // single time constant is the regression this separation guards.
    expect (large[0] >= 3.0 * large[3],
            "the jaw and the larynx are gliding at nearly the same speed");

    // A small move is a small adjustment, not the same journey done slowly.
    const auto small = riseTimes (0.5f, 1.0f, 0.56f, 0.94f);
    std::cout << "small vowel step, F1 10-90 % rise: " << small[0] << "ms\n";
    expect (small[0] > 0.0 && small[0] < 0.6 * large[0],
            "a small vowel move took as long as a full one: the transition has a "
            "deadline rather than a speed");
}

/** A parallel bank of poles cannot be asked for an /m/.

    Its transfer function does have zeros, but they land wherever the sections
    happen to cancel; there is no way to place one. That rules out the nasal
    branch, and a hum is the most common choir colour after "ah". The branch
    adds a murmur pole at the nasal cavity's own resonance and a placed
    anti-resonance where the closed mouth loads it.
*/
void testNasalBranch()
{
    constexpr auto sampleRate = 48000.0;
    // A low note so the harmonic comb resolves both bands. A2 puts harmonics at
    // 220 and 330 Hz across the murmur pole, and at 880, 990 and 1100 across
    // the anti-resonance.
    constexpr double fundamental = 110.0;

    struct Bands { double low; double notch; double peak; double total; };
    const auto bandsAt = [] (float nasal)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.profile = vocalor::VoiceProfile::Male;
        parameters.nasal = nasal;
        engine.setParameters (parameters);
        engine.noteOn (45, 0.85f);
        renderMono (engine, static_cast<int> (sampleRate * 0.5));
        const auto samples = renderMono (engine, static_cast<int> (sampleRate * 0.5));

        const auto energy = [&samples] (int first, int last)
        {
            double total = 0.0;
            for (int harmonic = first; harmonic <= last; ++harmonic)
            {
                const auto magnitude = harmonicMagnitude (
                    samples, fundamental * harmonic, sampleRate);
                total += magnitude * magnitude;
            }
            return std::sqrt (total);
        };
        // 220-330 Hz across the murmur, 880-1100 across the zero, and
        // 1650-2200 well above it as a control that the branch is not simply a
        // low-pass filter with a nicer name.
        double sumOfSquares = 0.0;
        for (const auto value : samples)
            sumOfSquares += static_cast<double> (value) * value;
        return Bands { energy (2, 3), energy (8, 10), energy (15, 20),
                       std::sqrt (sumOfSquares / std::max<std::size_t> (samples.size(), 1)) };
    };

    const auto oral = bandsAt (0.0f);
    const auto hummed = bandsAt (1.0f);
    const auto half = bandsAt (0.5f);
    const auto drop = [] (double before, double after)
    {
        return 20.0 * std::log10 (std::max (before, 1.0e-12) / std::max (after, 1.0e-12));
    };

    const auto notchDrop = drop (oral.notch, hummed.notch);
    const auto lowDrop = drop (oral.low, hummed.low);
    const auto highDrop = drop (oral.peak, hummed.peak);
    const auto totalDrop = drop (oral.total, hummed.total);
    std::cout << "velum fully open: 880-1100 Hz " << std::fixed << std::setprecision (1)
              << notchDrop << " dB, 220-330 Hz " << lowDrop << " dB, 1.65-2.2 kHz "
              << highDrop << " dB, overall " << totalDrop << " dB\n";

    expect (notchDrop > 20.0,
            "the nasal branch did not place an anti-resonance where the mouth loads it");
    expect (lowDrop < -3.0,
            "the murmur pole did not carry the band an /m/ radiates in");
    expect (highDrop > 4.0 && highDrop < notchDrop - 8.0,
            "a hum has to be duller than the vowel without being a low-pass filter");
    // The velum is a timbre control. An /m/ radiates from a smaller and more
    // damped aperture than an open mouth, and the branch has to account for
    // that rather than arriving as the loudest thing the instrument does.
    expect (std::abs (totalDrop) < 4.0,
            "opening the velum worked as a volume control");

    // A half-open velum has to land between the two, not snap to one of them.
    const auto halfDrop = drop (oral.notch, half.notch);
    expect (halfDrop > 1.0 && halfDrop < notchDrop - 1.0,
            "the velum coupling is a switch rather than a continuous control");

    // Every coupling has to stay finite and bounded, including through a jump.
    for (const float amount : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = makeParameters (1, 0, 0, 0);
        parameters.choirSize = 6;
        parameters.nasal = amount;
        engine.setParameters (parameters);
        engine.noteOn (52, 0.9f);
        const auto held = render (engine, static_cast<int> (sampleRate * 0.3));
        parameters.nasal = 1.0f - amount;
        engine.setParameters (parameters);
        const auto moved = render (engine, static_cast<int> (sampleRate * 0.3));
        expect (held.finite && moved.finite && moved.peak < 16.0,
                "the nasal branch destabilised at coupling "
                    + std::to_string (static_cast<int> (amount * 100.0f)) + " %");
    }
}

/** Twelve singers have to disperse like twelve singers.

    The 1.1 engine gave each singer a uniform +/-5.6 cents of static detune,
    about 3.2 cents of standard deviation at full Humanize. Jers and Ternstrom
    measured 25-30 cents of dispersion between real choir singers, and listeners
    were reported to tolerate 14 cents of standard deviation; a quarter of the
    tolerance is why twelve voices could still read as one thick one.
*/
void testEnsembleDispersion()
{
    constexpr auto sampleRate = 48000.0;

    const auto dispersionCents = [] (float humanize, int renderSamples,
                                     std::vector<float>& offsets)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.mode = vocalor::PerformanceMode::Choir;
        parameters.choirSize = 12;
        parameters.humanize = humanize;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        render (engine, renderSamples);

        const auto sounding = vocalor::VoiceEngineTestAccess::soundingFrequencies (engine);
        offsets.clear();
        if (sounding.size() < 2)
            return 0.0;

        double mean = 0.0;
        for (const auto frequency : sounding)
            mean += 1200.0 * std::log2 (std::max (static_cast<double> (frequency), 1.0e-9));
        mean /= static_cast<double> (sounding.size());

        double sumOfSquares = 0.0;
        for (const auto frequency : sounding)
        {
            const auto cents = 1200.0 * std::log2 (std::max (static_cast<double> (frequency), 1.0e-9))
                             - mean;
            offsets.push_back (static_cast<float> (cents));
            sumOfSquares += cents * cents;
        }
        return std::sqrt (sumOfSquares / static_cast<double> (sounding.size()));
    };

    std::vector<float> early;
    std::vector<float> late;
    std::vector<float> tight;

    const auto spread = dispersionCents (1.0f, static_cast<int> (sampleRate * 1.0), early);
    std::cout << "12-singer pitch dispersion at full Humanize: " << std::fixed
              << std::setprecision (1) << spread << " cents\n";
    expect (early.size() == 12, "the dispersion probe did not find twelve singers");
    // The measured band is 10-15 cents of standard deviation between section
    // colleagues; anything under 6 is a studio overdub, anything over 20 is out
    // of tune rather than human.
    expect (spread > 8.0 && spread < 18.0,
            "the ensemble dispersion is not in the range measured for real choirs");

    // Humanize is the dial from a studio unison to a rehearsal room, so at zero
    // the section has to be perfectly locked.
    expect (dispersionCents (0.0f, static_cast<int> (sampleRate * 1.0), tight) < 0.05,
            "the ensemble was not perfectly in tune with Humanize at zero");

    // The offsets are not fixed: a section drifts and recovers.
    dispersionCents (1.0f, static_cast<int> (sampleRate * 9.0), late);
    expect (late.size() == early.size(), "the late dispersion probe lost singers");
    double largestMove = 0.0;
    for (std::size_t i = 0; i < late.size(); ++i)
        largestMove = std::max (largestMove,
                                std::abs (static_cast<double> (late[i] - early[i])));
    std::cout << "largest singer drift over nine seconds: " << largestMove << " cents\n";
    expect (largestMove > 1.5,
            "the per-singer offsets are frozen: the section does not drift at all");
    expect (largestMove < 40.0,
            "the per-singer drift is unbounded rather than a wander around the target");
}

/** An a cappella ensemble tunes its chord to the bass, not to a keyboard.

    Chord mode stacked equal-tempered semitones, so a one-finger triad beat
    where a real section locks. The intonation control blends toward five-limit
    just intervals referred to the lowest sounding root, and has to reach them
    to within a cent or it is only a detune.
*/
void testJustIntonation()
{
    constexpr auto sampleRate = 48000.0;

    // The table itself, against the interval ratios it claims to realise.
    const auto centsForRatio = [] (double ratio, int equalSemitones)
    {
        return 1200.0 * std::log2 (ratio) - 100.0 * equalSemitones;
    };
    const std::array<std::pair<int, double>, 6> ratios {{
        { 3, 6.0 / 5.0 }, { 4, 5.0 / 4.0 }, { 5, 4.0 / 3.0 },
        { 7, 3.0 / 2.0 }, { 8, 8.0 / 5.0 }, { 9, 5.0 / 3.0 }
    }};
    for (const auto& entry : ratios)
        expect (std::abs (vocalor::justIntonationOffsetCents (entry.first)
                          - centsForRatio (entry.second, entry.first)) < 0.01,
                "the just offset for " + std::to_string (entry.first)
                    + " semitones does not match its ratio");
    expect (vocalor::justIntonationOffsetCents (0) == 0.0f
                && vocalor::justIntonationOffsetCents (12) == 0.0f
                && vocalor::justIntonationOffsetCents (-12) == 0.0f,
            "the octave is not left alone by the intonation table");

    const auto intervalCents = [] (float lower, float upper)
    {
        return 1200.0 * std::log2 (static_cast<double> (upper)
                                   / std::max (static_cast<double> (lower), 1.0e-9));
    };

    // Chord mode, both qualities, measured from the rendered oscillators.
    for (const int quality : { 0, 1 })
    {
        const int third = quality == 0 ? 4 : 3;
        for (const float amount : { 0.0f, 1.0f })
        {
            vocalor::VoiceEngine engine;
            engine.prepare (sampleRate, blockSize);
            engine.reset();
            auto parameters = steadyParameters();
            parameters.mode = vocalor::PerformanceMode::Chord;
            parameters.chordQuality = static_cast<vocalor::ChordQuality> (quality);
            parameters.intonation = amount;
            engine.setParameters (parameters);
            engine.noteOn (60, 0.80f);
            render (engine, static_cast<int> (sampleRate * 0.8));

            const auto root = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 60);
            const auto thirdHz = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 60 + third);
            const auto fifthHz = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 67);
            expect (root > 0.0f && thirdHz > 0.0f && fifthHz > 0.0f,
                    "chord mode did not sound the root, the third and the fifth");

            const auto expectedThird = 100.0 * third
                + amount * vocalor::justIntonationOffsetCents (third);
            const auto expectedFifth = 700.0
                + amount * vocalor::justIntonationOffsetCents (7);
            expect (std::abs (intervalCents (root, thirdHz) - expectedThird) < 1.0,
                    "the chord third did not land on its target interval");
            expect (std::abs (intervalCents (root, fifthHz) - expectedFifth) < 1.0,
                    "the chord fifth did not land on its target interval");
        }
    }

    // Played polyphonically the reference is the bass, so the same triad locks
    // whether it is one key in chord mode or three keys in solo mode.
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.intonation = 1.0f;
        engine.setParameters (parameters);
        // Deliberately out of order: the bass arrives last and the section has
        // to re-tune to it rather than to whatever was pressed first.
        engine.noteOn (64, 0.80f);
        engine.noteOn (67, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.2));
        engine.noteOn (60, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.8));

        const auto root = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 60);
        const auto third = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 64);
        const auto fifth = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 67);
        expect (std::abs (intervalCents (root, third) - 386.314) < 1.0,
                "a polyphonic major third did not re-tune to the bass that arrived after it");
        expect (std::abs (intervalCents (root, fifth) - 701.955) < 1.0,
                "a polyphonic fifth did not re-tune to the bass that arrived after it");
    }

    // The adjustment is a singer moving onto the interval, not a step.
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.intonation = 0.0f;
        engine.setParameters (parameters);
        engine.noteOn (60, 0.80f);
        engine.noteOn (64, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.6));
        const auto equal = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 64);

        parameters.intonation = 1.0f;
        engine.setParameters (parameters);
        render (engine, 512);
        const auto partway = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 64);
        render (engine, static_cast<int> (sampleRate * 0.8));
        const auto settled = vocalor::VoiceEngineTestAccess::frequencyForNote (engine, 64);

        const auto total = std::abs (intervalCents (equal, settled));
        expect (total > 12.0, "the intonation control did not move the third at all");
        expect (std::abs (intervalCents (equal, partway)) < 0.5 * total,
                "the intonation adjustment stepped instead of gliding");
        expect (std::abs (intervalCents (equal, partway)) > 0.0,
                "the intonation adjustment never started");
    }
}

/** A singer does not keep a speech tract when the fundamental climbs past its
    lowest resonance; she opens the jaw and takes F1 up with the pitch.

    The 1.1 engine raised F1 by a flat 0.32 % per semitone above A4, so a female
    /u/ at C6 kept F1 at 367 Hz with the fundamental at 1047 Hz — the whole
    spectrum above the lowest resonance, which is the configuration the soprano
    literature describes as a remarkable loss of acoustic energy.
*/
void testFormantTuningAtHighPitch()
{
    constexpr auto sampleRate = 48000.0;

    const auto formantsFor = [] (int midiNote, int vowel)
    {
        vocalor::VoiceEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.reset();
        auto parameters = steadyParameters();
        parameters.vowel = static_cast<vocalor::Vowel> (vowel);
        engine.setParameters (parameters);
        engine.noteOn (midiNote, 0.80f);
        render (engine, static_cast<int> (sampleRate * 0.5));
        return vocalor::VoiceEngineTestAccess::voiceFormants (engine);
    };

    const auto levelFor = [] (int midiNote, int vowel)
    {
        auto parameters = steadyParameters();
        parameters.vowel = static_cast<vocalor::Vowel> (vowel);
        return steadyLevelDb (sampleRate, parameters, midiNote);
    };

    // Middle C on the open anchor is a long way below F1: nothing may move.
    const auto low = formantsFor (60, 0);
    expect (std::abs (low[0] - 850.0f) < 1.0f,
            "the open vowel's F1 moved at a pitch nowhere near it");

    // C6 on the close-back anchor is the case the strategy exists for.
    constexpr float c6 = 1046.502f;
    const auto high = formantsFor (84, 1);
    std::cout << "C6 /u/: F1 " << std::fixed << std::setprecision (1) << high[0]
              << " Hz against f0 " << c6 << " Hz, F2 " << high[1] << " Hz\n";
    expect (high[0] > 0.95f * c6,
            "F1 did not follow the fundamental once the fundamental passed it");
    expect (high[0] < 1.45f * c6,
            "F1 overshot the fundamental instead of tuning to it");
    expect (high[1] > 1.20f * high[0],
            "F2 was not kept clear of the tracked F1");

    // The ceiling is physiological: the jaw runs out before the pitch does.
    const auto extreme = formantsFor (96, 1);
    expect (extreme[0] < 1400.0f,
            "F1 tracked past any jaw opening a singer actually has");

    // What it buys. With a fixed tract the vowel decides how much of the top
    // octave survives; with the strategy in place both vowels put a resonance
    // on the fundamental, so the choice of vowel costs far less.
    const auto openHigh = levelFor (84, 0);
    const auto closeHigh = levelFor (84, 1);
    std::cout << "C6 open vs close vowel level: " << std::setprecision (2)
              << openHigh << " dB vs " << closeHigh << " dB\n";
    // 25.1 dB before. The residual is the cascade amplitude weighting, which is
    // still resolved once per chunk for the untuned tract and so still knows
    // the close vowel by its speech formants; that is a real, stated limit.
    expect (std::abs (openHigh - closeHigh) < 8.0,
            "the vowel choice still decides whether the top octave is audible");

    // ... and the top octave must not simply collapse against the middle.
    const auto openMid = levelFor (60, 0);
    const auto closeMid = levelFor (60, 1);
    std::cout << "C4 open vs close vowel level: " << openMid << " dB vs "
              << closeMid << " dB\n";
    // -20.1 dB before: the top octave of a close vowel was simply gone.
    expect (closeHigh - closeMid > -3.0,
            "a close vowel still loses the top octave against the middle");
    // ... and the resonance it wins must not make the top octave shout either.
    expect (closeHigh - closeMid < 6.0,
            "formant tuning spent its resonance gain on volume instead of effort");
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
    // 30.00 dB, which is what a singer covers between pianissimo and
    // fortissimo. It was 18.06 dB until the dynamic stopped being a fader.
    expect (std::abs (20.0f * std::log10 (empty.voicedGain) + 30.00f) < 0.01f,
            "an empty dynamic is not 30 dB down on the voiced source");
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
    // Seven tenths of the control, so seven tenths of the 30 dB span plus what
    // the source slope takes out of the fundamental's own neighbourhood.
    expect (fundamentalDrop > 15.0 && fundamentalDrop < 30.0,
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
    testSourceLevelCalibration();
    testPerformanceExpression();
    testFormantTuningAtHighPitch();
    testJustIntonation();
    testEnsembleDispersion();
    testNasalBranch();
    testOnsetSpectrum();
    testVelocityShapesOnset();
    testDynamicRange();
    testCoarticulationTiming();
    testSingersFormantCluster();
    testFactoryPresets();
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
