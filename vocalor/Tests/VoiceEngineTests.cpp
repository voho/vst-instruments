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
            engine.setParameters (makeParameters (
                scenario.mode, scenario.chordQuality,
                static_cast<int> (scenarioIndex % 2),
                static_cast<int> (scenarioIndex % 3)));

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

    const auto start = std::chrono::steady_clock::now();
    const auto metrics = render (engine, static_cast<int> (sampleRate * secondsToRender));
    const auto elapsed = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - start).count();
    const auto realTimeRatio = elapsed / secondsToRender;

    std::cout << std::fixed << std::setprecision (3)
              << "12-singer 96 kHz render: " << elapsed << " s ("
              << realTimeRatio << "x real time)\n";

    expect (metrics.finite && metrics.rms() > 1.0e-6,
            "performance render did not produce valid, non-silent audio");
    // Deliberately generous: this is a regression tripwire that also tolerates
    // unoptimised CI builds, not a release-mode real-time certification.
    expect (realTimeRatio < 20.0,
            "12-singer render exceeded the 20x-real-time regression guardrail");
}
} // namespace

int main()
{
    testRenderMatrix();
    testReleaseCompletes();
    testAllSoundOffIsImmediate();
    testRoughPerformance();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Vocalor DSP check(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "All Vocalor DSP checks passed.\n";
    return EXIT_SUCCESS;
}
