// Circuit-block suite: laws the individual modelled circuits must obey, as
// opposed to what the played instrument does. Each check pins a behaviour
// the modelling contract anchors — keyboard law, pulse duties, the dual
// filter's modes, self-oscillation, the brightness pole, the Shaper VCA.

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

std::vector<float> renderMono(GhostEngine& engine, double seconds,
                              double sampleRate, int discardBlocks = 0)
{
    constexpr int blockSize = 256;
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    std::vector<float> samples;
    auto blocks = static_cast<int>(
        std::lround(seconds * sampleRate / blockSize));
    for (int block = 0; block < blocks; ++block)
    {
        engine.process(left.data(), right.data(), blockSize);
        if (block >= discardBlocks)
            samples.insert(samples.end(), left.begin(), left.end());
    }
    return samples;
}

double meanAbs(const std::vector<float>& samples)
{
    double sum = 0.0;
    for (const float value : samples)
        sum += std::abs(static_cast<double>(value));
    return samples.empty() ? 0.0 : sum / static_cast<double>(samples.size());
}

double peak(const std::vector<float>& samples)
{
    double result = 0.0;
    for (const float value : samples)
        result = std::max(result, std::abs(static_cast<double>(value)));
    return result;
}

// Signal energy at one frequency, for fundamental checks that zero
// crossings cannot decide (synced and multi-peak waveforms).
double goertzelMagnitude(const std::vector<float>& samples, double hz,
                         double sampleRate)
{
    const double w = 2.0 * 3.14159265358979323846 * hz / sampleRate;
    const double coefficient = 2.0 * std::cos(w);
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;
    for (const float value : samples)
    {
        s0 = static_cast<double>(value) + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power =
        s1 * s1 + s2 * s2 - coefficient * s1 * s2;
    return std::sqrt(std::max(power, 0.0))
         / std::max(1.0, static_cast<double>(samples.size()));
}

EngineParameters brightPanel()
{
    EngineParameters parameters;
    parameters.filterPathA = 0.8f;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.kbAmount = 0.0f;
    parameters.filterEnvAmount = 0.5f;
    parameters.loudnessAttack = 0.0f;
    parameters.loudnessSustain = 1.0f;
    return parameters;
}

double dominantFrequency(int midiNote)
{
    GhostEngine engine;
    engine.prepare(48000.0, 256);
    engine.setParameters(brightPanel());
    engine.noteOn(midiNote, 1.0f);
    const auto samples = renderMono(engine, 0.6, 48000.0, 20);

    int crossings = 0;
    for (std::size_t index = 1; index < samples.size(); ++index)
        if (samples[index - 1] <= 0.0f && samples[index] > 0.0f)
            ++crossings;
    const double seconds = static_cast<double>(samples.size()) / 48000.0;
    return static_cast<double>(crossings) / seconds;
}

// The keyboard law: MIDI 69 sounds 440 Hz at 8', one octave doubles.
void testKeyboardLaw()
{
    const double lower = dominantFrequency(57);
    const double upper = dominantFrequency(69);
    check(std::abs(lower - 220.0) < 6.0, "A3 sounds near 220 Hz");
    check(std::abs(upper - 440.0) < 9.0, "A4 sounds near 440 Hz");
    check(std::abs(upper / std::max(lower, 1.0) - 2.0) < 0.05,
          "one octave doubles the sounding frequency");
}

// MASTER OCTAVE transposes in exact octaves.
void testMasterOctave()
{
    GhostEngine engine;
    engine.prepare(48000.0, 256);
    auto parameters = brightPanel();
    parameters.octave = ghost::MasterOctave::Sixteen;
    engine.setParameters(parameters);
    engine.noteOn(69, 1.0f);
    const auto samples = renderMono(engine, 0.6, 48000.0, 20);
    int crossings = 0;
    for (std::size_t index = 1; index < samples.size(); ++index)
        if (samples[index - 1] <= 0.0f && samples[index] > 0.0f)
            ++crossings;
    const double hz = static_cast<double>(crossings)
                    / (static_cast<double>(samples.size()) / 48000.0);
    check(std::abs(hz - 220.0) < 6.0, "16' sounds one octave below 8'");
}

// The panel duty sets: Osc A's thinnest rectangle is 6 %, Osc B's is 3 %.
void testPulseDuties()
{
    const auto measureDuty = [](ghost::Waveform waveform, bool oscA) {
        GhostEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        if (oscA)
        {
            parameters.oscAWaveform = waveform;
        }
        else
        {
            parameters.filterPathA = 0.0f;
            parameters.filterPathB = 0.8f;
            parameters.oscBWaveform = waveform;
        }
        engine.setParameters(parameters);
        engine.noteOn(45, 1.0f);
        const auto samples = renderMono(engine, 0.8, 48000.0, 30);
        int high = 0;
        for (const float value : samples)
            if (value > 0.0f)
                ++high;
        return static_cast<double>(high)
             / static_cast<double>(samples.size());
    };

    check(std::abs(measureDuty(ghost::Waveform::RectWide, true) - 0.50) < 0.04,
          "Osc A's wide rectangle is a 50 % square");
    check(std::abs(measureDuty(ghost::Waveform::RectThin, true) - 0.06) < 0.03,
          "Osc A's thinnest rectangle sits near 6 %");
    check(std::abs(measureDuty(ghost::Waveform::RectThin, false) - 0.03) < 0.025,
          "Osc B's thinnest rectangle sits near 3 %");
}

// Hard sync: with SYNC on and B tuned a non-harmonic interval up, B's output
// carries A's fundamental.
void testHardSync()
{
    const auto fundamentalEnergy = [](bool sync) {
        GhostEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.filterPathA = 0.0f;
        parameters.filterPathB = 0.8f;
        parameters.sync = sync;
        parameters.interval = 0.93f;   // most of a fifth up: non-harmonic
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);       // A = 220 Hz
        const auto samples = renderMono(engine, 0.8, 48000.0, 30);
        return goertzelMagnitude(samples, 220.0, 48000.0)
             / std::max(1.0e-9, meanAbs(samples));
    };

    const double synced = fundamentalEnergy(true);
    const double unsynced = fundamentalEnergy(false);
    check(synced > 2.0 * unsynced,
          "sync locks Osc B's fundamental to Osc A");
}

// A lowpass must pass less of the same source as its cutoff falls.
void testLowpassAttenuationIsMonotonic()
{
    const auto steadyLevel = [](float cutoff) {
        GhostEngine engine;
        engine.prepare(44100.0, 256);
        auto parameters = brightPanel();
        parameters.cutoff = cutoff;
        engine.setParameters(parameters);
        engine.noteOn(69, 1.0f);
        return meanAbs(renderMono(engine, 0.6, 44100.0, 40));
    };

    const double open = steadyLevel(0.9f);
    const double mid = steadyLevel(0.5f);
    const double closed = steadyLevel(0.2f);
    check(open > mid, "half-closing the filter reduces the steady level");
    check(mid > closed, "closing the filter further keeps reducing the level");
    check(closed > 0.0, "a nearly closed filter still leaks a fundamental");
}

// 24 dB attenuates far-above-cutoff content more than 12 dB.
void testSlopeSwitch()
{
    const auto brightnessOfSlope = [](ghost::UpperSlope slope) {
        GhostEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.cutoff = 0.35f;
        parameters.slope = slope;
        engine.setParameters(parameters);
        engine.noteOn(69, 1.0f);
        const auto samples = renderMono(engine, 0.6, 48000.0, 30);
        // Compare a high harmonic against the fundamental.
        return goertzelMagnitude(samples, 440.0 * 8.0, 48000.0)
             / std::max(1.0e-12,
                        goertzelMagnitude(samples, 440.0, 48000.0));
    };

    check(brightnessOfSlope(ghost::UpperSlope::TwelveDb)
              > 2.0 * brightnessOfSlope(ghost::UpperSlope::TwentyFourDb),
          "the 24 dB slope darkens the eighth harmonic more than 12 dB");
}

// The lower filter's BANDPASS is a parametric boost: it must not attenuate
// far below its peak, and it must lift its peak as resonance rises. The
// probe note sits on the lower peak at a moderate Q, wide enough that a few
// hertz of alignment error stays inside the resonance bandwidth.
void testLowerBandPassIsParametricBoost()
{
    const auto response = [](ghost::LowerFilterMode mode, float resonance,
                             int note) {
        GhostEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.oscAWaveform = ghost::Waveform::Triangle;
        parameters.lowerMode = mode;
        parameters.resonance = resonance;
        parameters.lowerOnly = 0.663f;
        parameters.cutoff = 0.55f;
        engine.setParameters(parameters);
        engine.noteOn(note, 1.0f);
        const auto samples = renderMono(engine, 0.6, 48000.0, 30);
        return goertzelMagnitude(samples, 440.0 * std::exp2((note - 69) / 12.0),
                                 48000.0);
    };

    // A fundamental far below the peak passes without attenuation.
    const double lowOut = response(ghost::LowerFilterMode::Out, 0.55f, 45);
    const double lowBoost =
        response(ghost::LowerFilterMode::BandPass, 0.55f, 45);
    check(lowBoost > 0.6 * lowOut,
          "BANDPASS does not attenuate far below its peak");

    // Resonance lifts the boost peak itself (the probe sits on the peak).
    const double gentle = response(ghost::LowerFilterMode::BandPass, 0.15f, 69);
    const double sharp = response(ghost::LowerFilterMode::BandPass, 0.55f, 69);
    check(sharp > 1.4 * gentle, "resonance raises the parametric boost peak");
}

// OVERDRIVE is a saturator: doubling its input must yield clearly less than
// double its output, where the clean boost scales linearly. (A single-
// harmonic check is deliberately avoided — waveshaping a triangle nulls
// individual harmonics at particular drives.)
void testOverdriveCompresses()
{
    const auto level = [](ghost::LowerFilterMode mode, float slider) {
        GhostEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.filterPathA = slider;
        parameters.lowerMode = mode;
        parameters.resonance = 0.3f;
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);
        return meanAbs(renderMono(engine, 0.5, 48000.0, 30));
    };

    const double cleanRatio =
        level(ghost::LowerFilterMode::BandPass, 0.8f)
        / std::max(1.0e-12, level(ghost::LowerFilterMode::BandPass, 0.2f));
    const double drivenRatio =
        level(ghost::LowerFilterMode::Overdrive, 0.8f)
        / std::max(1.0e-12, level(ghost::LowerFilterMode::Overdrive, 0.2f));
    check(cleanRatio > 3.2, "the clean boost scales linearly with its input");
    check(drivenRatio < 0.8 * cleanRatio,
          "the overdrive stage compresses instead of scaling linearly");
}

// At full resonance the filter self-oscillates: kicked once, it keeps
// singing after the kick is gone.
void testSelfOscillation()
{
    GhostEngine engine;
    engine.prepare(44100.0, 256);
    auto parameters = brightPanel();
    parameters.filterPathA = 0.0f;
    parameters.filterPathNoise = 0.6f;
    parameters.resonance = 1.0f;
    parameters.upperResonance = ghost::UpperResonanceMode::Variable;
    parameters.cutoff = 0.6f;
    parameters.vcaBypass = true;
    engine.setParameters(parameters);
    renderMono(engine, 0.2, 44100.0);

    parameters.filterPathNoise = 0.0f;
    engine.setParameters(parameters);
    renderMono(engine, 1.0, 44100.0);
    const auto ringing = renderMono(engine, 0.5, 44100.0);
    check(peak(ringing) > 0.02,
          "the filter keeps singing after its excitation is removed");
    check(peak(ringing) < 4.0, "self-oscillation stays bounded");
}

// BRIGHTNESS is the Shaper path's one-pole tone control.
void testBrightnessDarkensShaperPath()
{
    const auto shaperLevel = [](float brightness) {
        GhostEngine engine;
        engine.prepare(44100.0, 256);
        EngineParameters parameters;
        parameters.filterPathA = 0.0f;   // isolate the Shaper path
        parameters.shaperPathA = 0.8f;
        parameters.brightness = brightness;
        parameters.shaperMode = ghost::ShaperMode::KbdHold;
        parameters.shaperRate = 1.0f;
        engine.setParameters(parameters);
        engine.noteOn(69, 1.0f);
        return meanAbs(renderMono(engine, 0.6, 44100.0, 40));
    };

    const double bright = shaperLevel(1.0f);
    const double dark = shaperLevel(0.15f);
    check(bright > 1.0e-4, "the Shaper path sounds with brightness open");
    check(dark < 0.5 * bright, "closing BRIGHTNESS darkens the Shaper path");
}

// In FREE mode the Shaper VCA gates its path: the output has loud and
// silent stretches within each cycle.
void testShaperFreeModePulsesItsPath()
{
    GhostEngine engine;
    engine.prepare(44100.0, 256);
    EngineParameters parameters;
    parameters.filterPathA = 0.0f;   // isolate the Shaper path
    parameters.shaperPathA = 0.8f;
    parameters.shaperMode = ghost::ShaperMode::Free;
    parameters.shaperRate = 0.85f;
    engine.setParameters(parameters);
    engine.noteOn(57, 1.0f);
    renderMono(engine, 0.5, 44100.0);
    const auto samples = renderMono(engine, 2.0, 44100.0);

    // Split into short windows; some must be silent, some loud.
    const std::size_t window = 2048;
    double quietest = 1.0e9;
    double loudest = 0.0;
    for (std::size_t start = 0; start + window <= samples.size();
         start += window)
    {
        double sum = 0.0;
        for (std::size_t index = start; index < start + window; ++index)
            sum += std::abs(static_cast<double>(samples[index]));
        const double level = sum / static_cast<double>(window);
        quietest = std::min(quietest, level);
        loudest = std::max(loudest, level);
    }
    check(loudest > 1.0e-3, "the free-running Shaper opens its VCA");
    check(quietest < 0.05 * loudest,
          "the free-running Shaper closes its VCA in the negative half");
}
} // namespace

int main()
{
    testKeyboardLaw();
    testMasterOctave();
    testPulseDuties();
    testHardSync();
    testLowpassAttenuationIsMonotonic();
    testSlopeSwitch();
    testLowerBandPassIsParametricBoost();
    testOverdriveCompresses();
    testSelfOscillation();
    testBrightnessDarkensShaperPath();
    testShaperFreeModePulsesItsPath();

    if (failures != 0)
    {
        std::cerr << failures << " Ghost circuit check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Ghost circuit checks passed.\n";
    return EXIT_SUCCESS;
}
