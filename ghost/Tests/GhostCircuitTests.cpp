// Circuit-block suite: laws the individual modelled circuits must obey, as
// opposed to what the played instrument does. While the voice is the
// pre-research skeleton this suite is deliberately thin — it holds the
// executable, harness and CTest wiring open for the researched blocks, and
// asserts only block laws the skeleton already promises.

#include "DSP/GhostEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
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

// Renders one steady held note and measures the mean absolute level of the
// last portion, once every transient has settled.
double steadyLevel(float cutoff)
{
    GhostEngine engine;
    engine.prepare(44100.0, 256);
    EngineParameters parameters;
    parameters.cutoff = cutoff;
    parameters.resonance = 0.0f;
    parameters.envToCutoff = 0.0f;
    parameters.attack = 0.0f;
    parameters.sustain = 1.0f;
    engine.setParameters(parameters);
    engine.noteOn(69, 1.0f);

    constexpr int blockSize = 256;
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    std::vector<float> tail;
    const int totalBlocks = 200;
    for (int block = 0; block < totalBlocks; ++block)
    {
        engine.process(left.data(), right.data(), blockSize);
        if (block >= totalBlocks / 2)
            tail.insert(tail.end(), left.begin(), left.end());
    }

    double sum = 0.0;
    for (const float value : tail)
        sum += std::abs(static_cast<double>(value));
    return sum / static_cast<double>(tail.size());
}

// A lowpass must pass less of the same source as its cutoff falls. The exact
// attenuation law belongs to the researched filter; monotonicity belongs to
// any lowpass.
void testLowpassAttenuationIsMonotonic()
{
    const double open = steadyLevel(0.9f);
    const double mid = steadyLevel(0.45f);
    const double closed = steadyLevel(0.12f);
    check(open > mid, "half-closing the filter reduces the steady level");
    check(mid > closed, "closing the filter further keeps reducing the level");
    check(closed > 0.0, "a nearly closed filter still leaks a fundamental");
}

// The keyboard law: one octave up must double the oscillator frequency. The
// measurement counts zero crossings of a bright, unfiltered render.
double dominantFrequency(int midiNote)
{
    GhostEngine engine;
    engine.prepare(48000.0, 256);
    EngineParameters parameters;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.envToCutoff = 0.0f;
    parameters.attack = 0.0f;
    parameters.sustain = 1.0f;
    engine.setParameters(parameters);
    engine.noteOn(midiNote, 1.0f);

    constexpr int blockSize = 256;
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    std::vector<float> samples;
    for (int block = 0; block < 100; ++block)
    {
        engine.process(left.data(), right.data(), blockSize);
        if (block >= 20)
            samples.insert(samples.end(), left.begin(), left.end());
    }

    // A sawtooth has exactly one rising zero crossing per period once the
    // filter transient has settled.
    int crossings = 0;
    for (std::size_t index = 1; index < samples.size(); ++index)
        if (samples[index - 1] <= 0.0f && samples[index] > 0.0f)
            ++crossings;
    const double seconds = static_cast<double>(samples.size()) / 48000.0;
    return static_cast<double>(crossings) / seconds;
}

void testOctaveDoublesFrequency()
{
    const double lower = dominantFrequency(57);
    const double upper = dominantFrequency(69);
    check(std::abs(lower - 220.0) < 6.0, "A3 sounds near 220 Hz");
    check(std::abs(upper - 440.0) < 9.0, "A4 sounds near 440 Hz");
    check(std::abs(upper / std::max(lower, 1.0) - 2.0) < 0.05,
          "one octave doubles the sounding frequency");
}
} // namespace

int main()
{
    testLowpassAttenuationIsMonotonic();
    testOctaveDoublesFrequency();

    if (failures != 0)
    {
        std::cerr << failures << " Ghost circuit check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Ghost circuit checks passed.\n";
    return EXIT_SUCCESS;
}
