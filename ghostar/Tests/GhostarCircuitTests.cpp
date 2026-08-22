// Circuit-block suite: laws the individual modelled circuits must obey, as
// opposed to what the played instrument does. Each check pins a behaviour
// the modelling contract anchors — keyboard law, pulse duties, the dual
// filter's modes, self-oscillation, the brightness pole, the Shaper VCA.

#include "DSP/GhostarEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using ghostar::EngineParameters;
using ghostar::GhostarEngine;

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << "\n";
        ++failures;
    }
}

std::vector<float> renderMono(GhostarEngine& engine, double seconds,
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
    GhostarEngine engine;
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
    GhostarEngine engine;
    engine.prepare(48000.0, 256);
    auto parameters = brightPanel();
    parameters.octave = ghostar::MasterOctave::Sixteen;
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
    const auto measureDuty = [](ghostar::Waveform waveform, bool oscA) {
        GhostarEngine engine;
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

    check(std::abs(measureDuty(ghostar::Waveform::RectWide, true) - 0.50) < 0.04,
          "Osc A's wide rectangle is a 50 % square");
    check(std::abs(measureDuty(ghostar::Waveform::RectThin, true) - 0.06) < 0.03,
          "Osc A's thinnest rectangle sits near 6 %");
    check(std::abs(measureDuty(ghostar::Waveform::RectThin, false) - 0.03) < 0.025,
          "Osc B's thinnest rectangle sits near 3 %");
}

// Hard sync: with SYNC on and B tuned a non-harmonic interval up, B's output
// carries A's fundamental.
void testHardSync()
{
    const auto fundamentalEnergy = [](bool sync) {
        GhostarEngine engine;
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
        GhostarEngine engine;
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
    const auto brightnessOfSlope = [](ghostar::UpperSlope slope) {
        GhostarEngine engine;
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

    check(brightnessOfSlope(ghostar::UpperSlope::TwelveDb)
              > 2.0 * brightnessOfSlope(ghostar::UpperSlope::TwentyFourDb),
          "the 24 dB slope darkens the eighth harmonic more than 12 dB");
}

// The lower filter's BANDPASS is a parametric boost: it must not attenuate
// far below its peak, and it must lift its peak as resonance rises. The
// probe note sits on the lower peak at a moderate Q, wide enough that a few
// hertz of alignment error stays inside the resonance bandwidth.
void testLowerBandPassIsParametricBoost()
{
    const auto response = [](ghostar::LowerFilterMode mode, float resonance,
                             int note) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.oscAWaveform = ghostar::Waveform::Triangle;
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
    const double lowOut = response(ghostar::LowerFilterMode::Out, 0.55f, 45);
    const double lowBoost =
        response(ghostar::LowerFilterMode::BandPass, 0.55f, 45);
    check(lowBoost > 0.6 * lowOut,
          "BANDPASS does not attenuate far below its peak");

    // Resonance lifts the boost peak itself (the probe sits on the peak).
    const double gentle = response(ghostar::LowerFilterMode::BandPass, 0.15f, 69);
    const double sharp = response(ghostar::LowerFilterMode::BandPass, 0.55f, 69);
    check(sharp > 1.4 * gentle, "resonance raises the parametric boost peak");
}

// OVERDRIVE is a saturator: doubling its input must yield clearly less than
// double its output, where the clean boost scales linearly. (A single-
// harmonic check is deliberately avoided — waveshaping a triangle nulls
// individual harmonics at particular drives.)
void testOverdriveCompresses()
{
    const auto level = [](ghostar::LowerFilterMode mode, float slider) {
        GhostarEngine engine;
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
        level(ghostar::LowerFilterMode::BandPass, 0.8f)
        / std::max(1.0e-12, level(ghostar::LowerFilterMode::BandPass, 0.2f));
    const double drivenRatio =
        level(ghostar::LowerFilterMode::Overdrive, 0.8f)
        / std::max(1.0e-12, level(ghostar::LowerFilterMode::Overdrive, 0.2f));
    check(cleanRatio > 3.2, "the clean boost scales linearly with its input");
    check(drivenRatio < 0.8 * cleanRatio,
          "the overdrive stage compresses instead of scaling linearly");
}

// The envelope segments are RC charges on the 4.7 uF cap through the 2 MOhm
// log sliders, so the panel's labelled time is the time *constant*: at full
// travel the decay must fall to 1/e of its span in ~9.4 s, not reach its
// target in that time. Measured on the loudness VCA with the sustain at
// zero, so the audible envelope is the decay itself (SM DWG 3, OQ-04).
void testDecayIsTheLabelledTimeConstant()
{
    GhostarEngine engine;
    engine.prepare(48000.0, 256);
    auto parameters = brightPanel();
    parameters.loudnessAttack = 0.0f;
    parameters.loudnessDecay = 1.0f;      // 2 MOhm: tau = 9.4 s
    parameters.loudnessSustain = 0.0f;
    parameters.filterEnvAmount = 0.5f;
    engine.setParameters(parameters);
    engine.noteOn(57, 1.0f);

    // Peak just after the attack, then the level one time constant later.
    renderMono(engine, 0.05, 48000.0);
    const double atPeak = peak(renderMono(engine, 0.05, 48000.0));
    renderMono(engine, 9.4 - 0.1, 48000.0);
    const double atTau = peak(renderMono(engine, 0.05, 48000.0));

    const double ratio = atTau / std::max(1.0e-12, atPeak);
    check(ratio > 0.30 && ratio < 0.43,
          "the decay falls to 1/e of its span in the labelled time");
}

// The attack charges toward ~1.3x the peak, so it reaches the peak in
// ln(1.3/0.3) = 1.47 time constants — flatter-topped than a segment aiming
// at 1.5 (ln 3 = 1.10) and far from a linear ramp (SM DWG 3, OQ-04).
void testAttackAimsPastItsPeak()
{
    const auto levelAfter = [](double seconds) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.loudnessAttack = 0.75f;   // tau ~ 0.6 s
        parameters.loudnessDecay = 1.0f;
        parameters.loudnessSustain = 1.0f;
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);
        renderMono(engine, seconds, 48000.0);
        return peak(renderMono(engine, 0.02, 48000.0));
    };

    // At travel 0.75 the slider stands at 1 kOhm * 2000^0.75 = 299 kOhm,
    // so tau = 4.7 uF * 299 kOhm = 1.405 s.
    constexpr double tau = 4.7e-6 * 299070.0;
    // One time constant into an aim of 1.3 reaches 1.3*(1-1/e) = 0.822.
    const double atTau = levelAfter(tau);
    const double atPeak = levelAfter(4.0 * tau);
    const double fraction = atTau / std::max(1.0e-12, atPeak);
    check(fraction > 0.76 && fraction < 0.88,
          "the attack reaches ~82 % of its peak in one time constant, as an "
          "RC charge aiming 1.3x past it does");
}

// The travel-to-Q law is derived from the CEM3350's −65 mV/decade Q scale
// and the Spirit's own pot network, anchored by the panel's LOW = Q 0.5.
// Its signature is that resonance stays gentle through mid-travel and then
// climbs steeply: Q ≈ 1.48 at half travel against ≈ 10.9 at nine tenths, a
// ratio near 7.4. A resonant section's peak gain tracks its Q, so the
// measured peak ratio is the law's fingerprint (OQ-12).
void testResonanceFollowsTheDerivedQLaw()
{
    // Probe on the resonant peak itself: the filter is fed a note whose
    // eighth harmonic sits at the cutoff, and that harmonic is measured.
    const auto peakGain = [](float resonance) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.oscAWaveform = ghostar::Waveform::Sawtooth;
        parameters.upperResonance = ghostar::UpperResonanceMode::Variable;
        parameters.slope = ghostar::UpperSlope::TwelveDb;
        parameters.resonance = resonance;
        parameters.cutoff = 0.5f;   // 20 Hz * 800^0.5 = 566 Hz
        engine.setParameters(parameters);
        engine.noteOn(46, 1.0f);    // ~93 Hz: sixth harmonic near 560 Hz
        const auto samples = renderMono(engine, 0.9, 48000.0, 60);
        return goertzelMagnitude(samples, 566.0, 48000.0);
    };

    const double atHalf = peakGain(0.5f);
    const double atNineTenths = peakGain(0.9f);
    const double ratio = atNineTenths / std::max(1.0e-12, atHalf);
    check(ratio > 4.5 && ratio < 11.0,
          "the resonant peak grows by the derived Q ratio between half and "
          "nine-tenths travel");

    // …and the law is gentle where the old voiced one was not: at half
    // travel the section must still be close to critically damped, not
    // ringing. Q = 1.48 puts the peak barely above 3 dB.
    const auto flatness = [](float resonance) {
        GhostarEngine engine;
        engine.prepare(48000.0, 256);
        auto parameters = brightPanel();
        parameters.upperResonance = ghostar::UpperResonanceMode::Variable;
        parameters.slope = ghostar::UpperSlope::TwelveDb;
        parameters.resonance = resonance;
        parameters.cutoff = 0.5f;
        parameters.filterPathA = 0.0f;
        parameters.filterPathNoise = 0.8f;
        engine.setParameters(parameters);
        engine.noteOn(60, 1.0f);
        const auto samples = renderMono(engine, 0.9, 48000.0, 60);
        return goertzelMagnitude(samples, 566.0, 48000.0)
             / std::max(1.0e-12,
                        goertzelMagnitude(samples, 100.0, 48000.0));
    };
    check(flatness(0.5f) < 3.0,
          "half travel is barely resonant, as Q = 1.5 requires");
}

// At full resonance the filter self-oscillates: kicked once, it keeps
// singing after the kick is gone.
void testSelfOscillation()
{
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    auto parameters = brightPanel();
    parameters.filterPathA = 0.0f;
    parameters.filterPathNoise = 0.6f;
    parameters.resonance = 1.0f;
    parameters.upperResonance = ghostar::UpperResonanceMode::Variable;
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
        GhostarEngine engine;
        engine.prepare(44100.0, 256);
        EngineParameters parameters;
        parameters.filterPathA = 0.0f;   // isolate the Shaper path
        parameters.shaperPathA = 0.8f;
        parameters.brightness = brightness;
        parameters.shaperMode = ghostar::ShaperMode::KbdHold;
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
    GhostarEngine engine;
    engine.prepare(44100.0, 256);
    EngineParameters parameters;
    parameters.filterPathA = 0.0f;   // isolate the Shaper path
    parameters.shaperPathA = 0.8f;
    parameters.shaperMode = ghostar::ShaperMode::Free;
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
    testDecayIsTheLabelledTimeConstant();
    testAttackAimsPastItsPeak();
    testResonanceFollowsTheDerivedQLaw();
    testSelfOscillation();
    testBrightnessDarkensShaperPath();
    testShaperFreeModePulsesItsPath();

    if (failures != 0)
    {
        std::cerr << failures << " Ghostar circuit check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Ghostar circuit checks passed.\n";
    return EXIT_SUCCESS;
}
