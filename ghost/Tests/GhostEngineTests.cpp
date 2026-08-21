// Engine behaviour suite: what the instrument does when it is played. While
// the voice is the pre-research skeleton, these tests assert only the engine
// contracts that survive the researched model — finiteness, gating, silence
// after release — never the placeholder laws themselves.

#include "DSP/GhostEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using ghost::EngineParameters;
using ghost::GhostEngine;

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << "\n";
        ++failures;
    }
}

struct Rendered
{
    std::vector<float> left;
    std::vector<float> right;
};

Rendered render(GhostEngine& engine, double seconds, double sampleRate)
{
    constexpr int blockSize = 256;
    Rendered rendered;
    auto remaining = static_cast<int>(std::lround(seconds * sampleRate));
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    while (remaining > 0)
    {
        const int count = std::min(blockSize, remaining);
        engine.process(left.data(), right.data(), count);
        rendered.left.insert(rendered.left.end(), left.begin(),
                             left.begin() + count);
        rendered.right.insert(rendered.right.end(), right.begin(),
                              right.begin() + count);
        remaining -= count;
    }
    return rendered;
}

double peak(const Rendered& rendered)
{
    double result = 0.0;
    for (std::size_t index = 0; index < rendered.left.size(); ++index)
        result = std::max({ result,
                            std::abs(static_cast<double>(rendered.left[index])),
                            std::abs(static_cast<double>(rendered.right[index])) });
    return result;
}

bool finite(const Rendered& rendered)
{
    for (std::size_t index = 0; index < rendered.left.size(); ++index)
        if (!std::isfinite(rendered.left[index])
            || !std::isfinite(rendered.right[index]))
            return false;
    return true;
}

void testSilentBeforeFirstNote()
{
    GhostEngine engine;
    engine.prepare(44100.0, 256);
    engine.setParameters(EngineParameters {});
    const auto rendered = render(engine, 0.25, 44100.0);
    check(finite(rendered), "idle render is finite");
    check(peak(rendered) == 0.0, "engine is silent before the first note");
}

void testNoteProducesAudibleFiniteAudio()
{
    GhostEngine engine;
    engine.prepare(44100.0, 256);
    engine.setParameters(EngineParameters {});
    engine.noteOn(48, 0.9f);
    const auto rendered = render(engine, 0.5, 44100.0);
    check(finite(rendered), "held note renders finite audio");
    check(peak(rendered) > 1.0e-3, "held note is audible");
}

void testReleaseDecaysToSilence()
{
    GhostEngine engine;
    engine.prepare(44100.0, 256);
    EngineParameters parameters;
    parameters.release = 0.1f;
    engine.setParameters(parameters);
    engine.noteOn(60, 0.9f);
    render(engine, 0.4, 44100.0);
    engine.noteOff(60);
    render(engine, 2.0, 44100.0);
    const auto tail = render(engine, 0.25, 44100.0);
    check(finite(tail), "release tail is finite");
    check(peak(tail) < 1.0e-4, "released note decays to silence");
    check(!engine.isGateOpen(), "gate is closed after the last key is released");
}

void testHeldKeyMemoryReturnsToOlderKey()
{
    GhostEngine engine;
    engine.prepare(44100.0, 256);
    engine.setParameters(EngineParameters {});
    engine.noteOn(48, 0.9f);
    render(engine, 0.1, 44100.0);
    engine.noteOn(60, 0.9f);
    render(engine, 0.1, 44100.0);
    engine.noteOff(60);
    check(engine.isGateOpen(),
          "releasing the newer key with an older key held keeps the gate open");
    engine.noteOff(48);
    check(!engine.isGateOpen(), "releasing the last held key closes the gate");
}

void testHostileRatesAreClamped()
{
    GhostEngine engine;
    engine.prepare(0.0, 256);
    check(engine.getSampleRate() == GhostEngine::minimumSupportedSampleRate,
          "a zero host rate clamps to the minimum supported rate");
    engine.prepare(1.0e9, 256);
    check(engine.getSampleRate() == GhostEngine::maximumSupportedSampleRate,
          "an absurd host rate clamps to the maximum supported rate");

    // std::clamp passes NaN through, so the engine must sanitise before it.
    engine.prepare(std::numeric_limits<double>::quiet_NaN(), 256);
    check(std::isfinite(engine.getSampleRate()),
          "a NaN host rate resolves to a finite rate");
    engine.noteOn(60, 1.0f);
    const auto nanRateRender = render(engine, 0.25, engine.getSampleRate());
    check(finite(nanRateRender), "post-NaN-rate render is finite");
    engine.noteOff(60);

    engine.prepare(48000.0, 256);
    engine.noteOn(60, 1.0f);
    const auto rendered = render(engine, 0.25, 48000.0);
    check(finite(rendered), "post-clamp render is finite");
}

// Note 127 is ~12.5 kHz, above an 8 kHz host's Nyquist; the oscillator must
// bound its frequency rather than let the phase step exceed a whole cycle and
// settle into DC.
void testTopNoteAtLowestRateStaysAnOscillation()
{
    GhostEngine engine;
    engine.prepare(8000.0, 256);
    EngineParameters parameters;
    parameters.cutoff = 1.0f;
    parameters.attack = 0.0f;
    parameters.sustain = 1.0f;
    engine.setParameters(parameters);
    engine.noteOn(127, 1.0f);
    render(engine, 0.5, 8000.0);
    const auto settled = render(engine, 0.5, 8000.0);
    check(finite(settled), "top note at 8 kHz renders finite audio");
    check(peak(settled) < 4.0, "top note at 8 kHz stays bounded");

    double mean = 0.0;
    for (const float value : settled.left)
        mean += static_cast<double>(value);
    mean /= static_cast<double>(settled.left.size());
    double deviation = 0.0;
    for (const float value : settled.left)
        deviation = std::max(deviation,
                             std::abs(static_cast<double>(value) - mean));
    check(deviation > 1.0e-3,
          "top note at 8 kHz keeps oscillating instead of settling to DC");
}

// Every distinct MIDI note held at once must survive in the held-key memory:
// releasing every newer key has to fall back to the very first one.
void testKeyMemorySpansTheWholeMidiDomain()
{
    GhostEngine engine;
    engine.prepare(44100.0, 256);
    engine.setParameters(EngineParameters {});
    for (int note = 0; note < 128; ++note)
        engine.noteOn(note, 0.9f);
    for (int note = 127; note >= 1; --note)
        engine.noteOff(note);
    check(engine.isGateOpen(),
          "the oldest of 128 held keys still holds the gate open");
    engine.noteOff(0);
    check(!engine.isGateOpen(), "releasing the final held key closes the gate");
}

void testFullResonanceStaysBounded()
{
    GhostEngine engine;
    engine.prepare(44100.0, 256);
    EngineParameters parameters;
    parameters.resonance = 1.0f;
    parameters.cutoff = 0.8f;
    engine.setParameters(parameters);
    engine.noteOn(36, 1.0f);
    const auto rendered = render(engine, 1.0, 44100.0);
    check(finite(rendered), "full-resonance render is finite");
    check(peak(rendered) < 4.0, "full-resonance render stays bounded");
}
} // namespace

int main()
{
    testSilentBeforeFirstNote();
    testNoteProducesAudibleFiniteAudio();
    testReleaseDecaysToSilence();
    testHeldKeyMemoryReturnsToOlderKey();
    testHostileRatesAreClamped();
    testTopNoteAtLowestRateStaysAnOscillation();
    testKeyMemorySpansTheWholeMidiDomain();
    testFullResonanceStaysBounded();

    if (failures != 0)
    {
        std::cerr << failures << " Ghost engine check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Ghost engine checks passed.\n";
    return EXIT_SUCCESS;
}
