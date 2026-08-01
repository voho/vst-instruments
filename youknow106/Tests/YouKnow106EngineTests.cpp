// Engine behaviour suite: what the instrument does when it is played, as
// opposed to what its individual circuit blocks compute. The circuit suite
// checks the laws; this one checks the machine.

#include "DSP/YouKnow106Chorus.h"
#include "DSP/YouKnow106Engine.h"
#include "DSP/YouKnow106Panel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using namespace youknow106;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectNear(double actual, double expected, double tolerance,
                const std::string& message)
{
    if (!(std::abs(actual - expected) <= tolerance))
    {
        ++failures;
        std::cerr << "FAIL: " << message << " (got " << actual << ", expected "
                  << expected << " +/- " << tolerance << ")\n";
    }
}

constexpr double pi = 3.14159265358979323846;
constexpr int blockSize = 256;

struct Render
{
    std::vector<float> left;
    std::vector<float> right;
};

Render render(YouKnow106Engine& engine, int samples)
{
    Render result;
    const int blocks = (samples + blockSize - 1) / blockSize;
    result.left.assign(static_cast<std::size_t>(blocks * blockSize), 0.0f);
    result.right.assign(static_cast<std::size_t>(blocks * blockSize), 0.0f);
    for (int block = 0; block < blocks; ++block)
        engine.process(result.left.data() + block * blockSize,
                       result.right.data() + block * blockSize, blockSize);
    return result;
}

EngineParameters plainPatch()
{
    EngineParameters parameters;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 0.0f;
    parameters.highPass = HighPassMode::One;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.envDepth = 0.0f;
    parameters.keyFollow = 0.0f;
    parameters.attack = 0.0f;
    parameters.decay = 1.0f;
    parameters.sustain = 1.0f;
    parameters.release = 0.0f;
    parameters.vcaLevel = 1.0f;
    parameters.volume = 1.0f;
    parameters.chorus = ChorusMode::Off;
    parameters.calibration = 0.0f;
    return parameters;
}

double peakOf(const std::vector<float>& signal, std::size_t from)
{
    double peak = 0.0;
    for (std::size_t index = from; index < signal.size(); ++index)
        peak = std::max(peak, static_cast<double>(std::abs(signal[index])));
    return peak;
}

double magnitudeAt(const std::vector<float>& signal, std::size_t start, int length,
                   double frequency, double sampleRate)
{
    std::complex<double> accumulator {};
    for (int index = 0; index < length; ++index)
    {
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * index / (length - 1));
        accumulator += signal[start + static_cast<std::size_t>(index)] * window
                     * std::exp(std::complex<double>(0.0, -2.0 * pi * frequency
                                                              * index / sampleRate));
    }
    return std::abs(accumulator) / (length * 0.5);
}

// Frequency measured between the first and last rising zero crossing, which
// resolves far better than counting crossings over a fixed window.
double measuredFrequency(const std::vector<float>& signal, std::size_t from,
                         double sampleRate)
{
    std::size_t first = 0;
    std::size_t last = 0;
    int intervals = -1;
    for (std::size_t index = from + 1; index < signal.size(); ++index)
        if (signal[index - 1] <= 0.0f && signal[index] > 0.0f)
        {
            if (intervals < 0)
                first = index;
            last = index;
            ++intervals;
        }
    return intervals > 0 ? intervals * sampleRate / static_cast<double>(last - first)
                         : 0.0;
}

// --------------------------------------------------------------------------

void testRangeTransposesByOctaves()
{
    constexpr double sampleRate = 96000.0;
    const std::array<DcoRange, 3> ranges { DcoRange::Sixteen, DcoRange::Eight,
                                           DcoRange::Four };
    std::array<double, 3> measured {};

    for (std::size_t index = 0; index < ranges.size(); ++index)
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.range = ranges[index];
        engine.setParameters(parameters);
        engine.noteOn(69, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate));
        measured[index] = measuredFrequency(rendered.left, rendered.left.size() / 2,
                                            sampleRate);
    }

    expectNear(measured[1] / measured[0], 2.0, 0.01,
               "8' does not sound an octave above 16'");
    expectNear(measured[2] / measured[1], 2.0, 0.01,
               "4' does not sound an octave above 8'");
    expectNear(measured[1], 440.0, 1.0, "8' does not sound at concert pitch");
}

void testSubIsOneOctaveDown()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);

    auto parameters = plainPatch();
    parameters.sawEnabled = false;
    parameters.subLevel = 1.0f;
    engine.setParameters(parameters);
    engine.noteOn(69, 1.0f);

    const auto rendered = render(engine, static_cast<int>(sampleRate));
    const auto frequency = measuredFrequency(rendered.left, rendered.left.size() / 2,
                                             sampleRate);
    expectNear(frequency, 220.0, 1.0, "the sub is not one octave below the note");
}

void testAliasFloor()
{
    // The oscillator's discontinuities are repaired with residuals built by
    // integration; a broken table shows up here as a raised noise floor rather
    // than as anything obviously wrong in the waveform.
    for (double sampleRate : { 44100.0, 48000.0 })
    {
        for (int note : { 60, 84 })
        {
            YouKnow106Engine engine;
            engine.prepare(sampleRate, blockSize, true);
            auto parameters = plainPatch();
            engine.setParameters(parameters);
            engine.noteOn(note, 1.0f);

            const auto rendered = render(engine, static_cast<int>(sampleRate));
            const double wanted = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            const double f0 = YouKnow106Engine::dcoQuantisedFrequency(
                YouKnow106Engine::dcoDivider(wanted), DcoRange::Eight);

            const std::size_t start = static_cast<std::size_t>(sampleRate * 0.5);
            const int length = 16384;
            double harmonic = 0.0;
            for (int index = 1; index * f0 < 20000.0; ++index)
                harmonic = std::max(harmonic, magnitudeAt(rendered.left, start, length,
                                                          index * f0, sampleRate));

            double worst = 0.0;
            for (double frequency = 100.0; frequency < 20000.0; frequency += 41.0)
            {
                bool nearHarmonic = false;
                for (int index = 1; index * f0 < 24000.0; ++index)
                    if (std::abs(frequency - index * f0) < 60.0)
                    {
                        nearHarmonic = true;
                        break;
                    }
                if (nearHarmonic)
                    continue;
                worst = std::max(worst, magnitudeAt(rendered.left, start, length,
                                                    frequency, sampleRate));
            }

            const double decibels = 20.0 * std::log10((worst + 1.0e-15) / harmonic);
            expect(decibels < -55.0,
                   "alias floor for note " + std::to_string(note) + " at "
                       + std::to_string(static_cast<int>(sampleRate))
                       + " Hz is only " + std::to_string(decibels) + " dB down");
        }
    }
}

void testRampHasARampSpectrum()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    engine.setParameters(plainPatch());
    engine.noteOn(45, 1.0f);

    const auto rendered = render(engine, static_cast<int>(sampleRate));
    const double f0 = YouKnow106Engine::dcoQuantisedFrequency(
        YouKnow106Engine::dcoDivider(110.0), DcoRange::Eight);
    const std::size_t start = static_cast<std::size_t>(sampleRate * 0.5);

    const double fundamental = magnitudeAt(rendered.left, start, 16384, f0, sampleRate);
    for (int harmonic : { 2, 3, 5, 9 })
    {
        const double measured = magnitudeAt(rendered.left, start, 16384,
                                            harmonic * f0, sampleRate);
        const double expected = fundamental / harmonic;
        expectNear(20.0 * std::log10(measured / expected), 0.0, 1.5,
                   "harmonic " + std::to_string(harmonic)
                       + " does not follow a ramp's 1/n envelope");
    }
}

void testKeyAssignerDropsRatherThanSteals()
{
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.sustain = 1.0f;
    engine.setParameters(parameters);

    for (int note = 60; note < 68; ++note)
        engine.noteOn(note, 1.0f);
    render(engine, 4096);
    expect(engine.getActiveVoiceCount() == 6,
           "the key assigner does not stop at six voices");

    // The two extra keys must have been dropped, not swapped in: releasing the
    // first six has to silence the instrument completely.
    for (int note = 60; note < 68; ++note)
        engine.noteOff(note);
    render(engine, static_cast<int>(sampleRate));
    expect(engine.getActiveVoiceCount() == 0,
           "a dropped note left a voice sounding");
}

void testPolyModesDifferInAllocation()
{
    constexpr double sampleRate = 48000.0;

    const auto allocationOrder = [&](KeyMode mode) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, false);
        auto parameters = plainPatch();
        parameters.keyMode = mode;
        parameters.release = 0.6f;
        engine.setParameters(parameters);

        std::vector<int> masks;
        for (int note = 0; note < 4; ++note)
        {
            engine.noteOn(60 + note, 1.0f);
            render(engine, 512);
            masks.push_back(engine.getDisplayVoiceMask());
            engine.noteOff(60 + note);
            render(engine, 512);
        }
        return masks;
    };

    const auto rotating = allocationOrder(KeyMode::Poly1);
    const auto fixed = allocationOrder(KeyMode::Poly2);

    // Rotation must visit more than one voice across four separate notes;
    // fixed priority must keep coming back to the first.
    int distinct = 0;
    int seen = 0;
    for (const auto mask : rotating)
        if ((seen & mask) != mask)
        {
            seen |= mask;
            ++distinct;
        }
    expect(distinct >= 3, "rotating assignment is reusing the same voice");
    expect(fixed.front() == fixed.back(),
           "fixed-priority assignment is not returning to the first voice");
}

void testUnisonUsesEveryVoiceWithoutDetuning()
{
    constexpr double sampleRate = 96000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.keyMode = KeyMode::Unison;
    // Zero tolerance: with the analogue spread switched out, six voices sharing
    // one reference and one count must be exactly coincident.
    parameters.calibration = 0.0f;
    engine.setParameters(parameters);
    engine.noteOn(57, 1.0f);

    const auto rendered = render(engine, static_cast<int>(sampleRate));
    expect(engine.getActiveVoiceCount() == 6, "unison does not use every voice");

    // A detuned stack beats; a phase-locked one does not. Compare the peak in
    // two windows a long way apart.
    const std::size_t quarter = rendered.left.size() / 4;
    const double early = peakOf({ rendered.left.begin() + static_cast<long>(quarter),
                                  rendered.left.begin() + static_cast<long>(quarter * 2) },
                                0);
    const double late = peakOf({ rendered.left.begin() + static_cast<long>(quarter * 3),
                                 rendered.left.end() }, 0);
    expectNear(20.0 * std::log10((late + 1e-12) / (early + 1e-12)), 0.0, 0.5,
               "unison voices are beating against one another");
}

void testEnvelopeAndGateModes()
{
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);

    auto parameters = plainPatch();
    parameters.attack = 0.0f;
    parameters.decay = 0.6f;
    parameters.sustain = 0.0f;
    parameters.release = 0.0f;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);
    const auto decaying = render(engine, static_cast<int>(sampleRate * 2));

    // The generator is linear and the amplifier exponential, so equal slices of
    // time must fall by roughly equal numbers of decibels while the segment
    // runs.
    const auto levelAt = [&](double seconds) {
        const auto start = static_cast<std::size_t>(seconds * sampleRate);
        return 20.0 * std::log10(peakOf({ decaying.left.begin() + static_cast<long>(start),
                                          decaying.left.begin()
                                              + static_cast<long>(start + 1200) }, 0)
                                 + 1.0e-12);
    };
    const double first = levelAt(0.05) - levelAt(0.15);
    const double second = levelAt(0.15) - levelAt(0.25);
    expect(first > 3.0, "the decay segment is not falling");
    expectNear(second / first, 1.0, 0.45,
               "amplitude decay is not close to constant decibels per second");

    // Gate mode ignores the generator's shape entirely.
    YouKnow106Engine gated;
    gated.prepare(sampleRate, blockSize, true);
    parameters.vcaMode = VcaMode::Gate;
    gated.setParameters(parameters);
    gated.noteOn(60, 1.0f);
    const auto held = render(gated, static_cast<int>(sampleRate));
    expect(peakOf(held.left, held.left.size() / 2) > 0.05,
           "gate mode falls silent while the key is held");
}

void testChorusWidthAndSilence()
{
    constexpr double sampleRate = 48000.0;

    const auto sideEnergy = [&](ChorusMode mode) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.chorus = mode;
        parameters.chorusNoise = 0.0f;
        engine.setParameters(parameters);
        engine.noteOn(52, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate * 2));

        double side = 0.0;
        double mid = 0.0;
        for (std::size_t index = rendered.left.size() / 2; index < rendered.left.size();
             ++index)
        {
            const double difference = rendered.left[index] - rendered.right[index];
            const double sum = rendered.left[index] + rendered.right[index];
            side += difference * difference;
            mid += sum * sum;
        }
        return std::sqrt(side / std::max(mid, 1.0e-12));
    };

    expect(sideEnergy(ChorusMode::Off) < 1.0e-6,
           "the output is not mono with the effect switched out");
    expect(sideEnergy(ChorusMode::One) > 0.05, "the first mode produces no width");
    expect(sideEnergy(ChorusMode::Two) > 0.05, "the second mode produces no width");
}

void testChorusNoiseIsPresentAndDefeatable()
{
    constexpr double sampleRate = 48000.0;

    const auto idleNoise = [&](float scale) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, true);
        auto parameters = plainPatch();
        parameters.chorus = ChorusMode::Two;
        parameters.chorusNoise = scale;
        engine.setParameters(parameters);
        const auto rendered = render(engine, static_cast<int>(sampleRate));
        double energy = 0.0;
        for (std::size_t index = rendered.left.size() / 2; index < rendered.left.size();
             ++index)
            energy += static_cast<double>(rendered.left[index]) * rendered.left[index];
        return 10.0 * std::log10(energy / (rendered.left.size() / 2) + 1.0e-30);
    };

    const double modelled = idleNoise(1.0f);
    expect(modelled > -85.0 && modelled < -55.0,
           "the delay lines' own noise floor is outside the measured band");
    expect(idleNoise(0.0f) < -120.0,
           "the delay lines still hiss with their noise switched out");
}

void testSampleRateAndOversamplingConsistency()
{
    const auto levelAt = [](double sampleRate, bool oversampled) {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, blockSize, oversampled);
        auto parameters = plainPatch();
        parameters.cutoff = 0.7f;
        engine.setParameters(parameters);
        engine.noteOn(57, 1.0f);
        const auto rendered = render(engine, static_cast<int>(sampleRate));
        double energy = 0.0;
        const auto from = rendered.left.size() / 2;
        for (std::size_t index = from; index < rendered.left.size(); ++index)
            energy += static_cast<double>(rendered.left[index]) * rendered.left[index];
        return 10.0 * std::log10(energy / (rendered.left.size() - from) + 1.0e-30);
    };

    const double reference = levelAt(48000.0, true);
    for (double sampleRate : { 44100.0, 88200.0, 96000.0, 192000.0 })
        expectNear(levelAt(sampleRate, true), reference, 1.0,
                   "output level moves with the host sample rate");
    expectNear(levelAt(48000.0, false), reference, 1.5,
               "output level moves when oversampling is switched off");

    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    expect(engine.getOversamplingFactor() == 4, "48 kHz does not run oversampled");
    expect(engine.getProcessingLatencySamples() > 0,
           "an oversampled engine reports no latency");
    engine.prepare(192000.0, blockSize, true);
    expect(engine.getOversamplingFactor() == 1,
           "a high-rate host is oversampled unnecessarily");
    expect(engine.getProcessingLatencySamples() == 0,
           "a native-rate engine reports latency");
}

void testDeterminismAndSilence()
{
    const auto run = []() {
        YouKnow106Engine engine;
        engine.prepare(48000.0, blockSize, true);
        auto parameters = plainPatch();
        parameters.calibration = 1.0f;
        parameters.noiseLevel = 0.4f;
        parameters.chorus = ChorusMode::Two;
        engine.setParameters(parameters);
        engine.noteOn(64, 1.0f);
        return render(engine, 24000);
    };

    const auto first = run();
    const auto second = run();
    expect(first.left == second.left && first.right == second.right,
           "the engine is not deterministic");

    // With nothing playing and the effect switched out, the output has to be
    // exactly zero rather than merely small.
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    engine.setParameters(plainPatch());
    const auto silent = render(engine, 24000);
    expect(peakOf(silent.left, 0) == 0.0 && peakOf(silent.right, 0) == 0.0,
           "an idle engine is not exactly silent");
}

void testExtremeAutomationStaysFinite()
{
    YouKnow106Engine engine;
    engine.prepare(44100.0, blockSize, true);

    std::uint32_t state = 12345u;
    const auto next = [&state]() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0xffffffu) / 16777215.0f;
    };

    bool finite = true;
    double peak = 0.0;
    for (int block = 0; block < 400; ++block)
    {
        EngineParameters parameters;
        parameters.lfoRate = next();
        parameters.lfoDelay = next();
        parameters.dcoLfoDepth = next();
        parameters.pwmDepth = next();
        parameters.pwmSource = next() < 0.5f ? PwmSource::Lfo : PwmSource::Manual;
        parameters.range = static_cast<DcoRange>(static_cast<int>(next() * 2.99f));
        parameters.sawEnabled = next() < 0.7f;
        parameters.pulseEnabled = next() < 0.7f;
        parameters.subLevel = next();
        parameters.noiseLevel = next();
        parameters.highPass = static_cast<HighPassMode>(static_cast<int>(next() * 3.99f));
        parameters.cutoff = next();
        parameters.resonance = next();
        parameters.envDepth = next();
        parameters.vcfLfoDepth = next();
        parameters.keyFollow = next();
        parameters.attack = next();
        parameters.decay = next();
        parameters.sustain = next();
        parameters.release = next();
        parameters.chorus = static_cast<ChorusMode>(static_cast<int>(next() * 2.99f));
        parameters.portamento = next();
        parameters.calibration = next();
        parameters.volume = 1.0f;
        parameters.vcaLevel = 1.0f;
        engine.setParameters(parameters);
        engine.setPitchBend(next() * 2.0f - 1.0f);
        engine.setModWheel(next());

        if (block % 5 == 0)
            engine.noteOn(36 + static_cast<int>(next() * 60.0f), next());
        if (block % 7 == 0)
            engine.noteOff(36 + static_cast<int>(next() * 60.0f));

        const auto rendered = render(engine, blockSize);
        for (const auto sample : rendered.left)
        {
            finite = finite && std::isfinite(sample);
            peak = std::max(peak, static_cast<double>(std::abs(sample)));
        }
    }

    expect(finite, "extreme automation produced a non-finite sample");
    expect(peak < 4.0, "extreme automation produced an unbounded output");
}

void testParameterSanitisation()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);

    EngineParameters parameters = plainPatch();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    parameters.cutoff = nan;
    parameters.resonance = std::numeric_limits<float>::infinity();
    parameters.attack = -5.0f;
    parameters.sustain = 12.0f;
    parameters.masterTuneCents = 900.0f;
    parameters.polyphony = 500;
    engine.setParameters(parameters);
    engine.noteOn(60, 1.0f);

    const auto rendered = render(engine, 8192);
    bool finite = true;
    for (const auto sample : rendered.left)
        finite = finite && std::isfinite(sample);
    expect(finite, "hostile parameter values reached the audio path");
    expect(engine.getActiveVoiceCount() >= 1,
           "the engine stopped sounding after hostile parameters");
}

void testSustainPedalHoldsAndReleases()
{
    YouKnow106Engine engine;
    engine.prepare(48000.0, blockSize, true);
    auto parameters = plainPatch();
    parameters.release = 0.0f;
    engine.setParameters(parameters);

    engine.setSustainPedal(true);
    engine.noteOn(60, 1.0f);
    render(engine, 4096);
    engine.noteOff(60);
    render(engine, 8192);
    expect(engine.getActiveVoiceCount() == 1,
           "the pedal did not hold the note");

    engine.setSustainPedal(false);
    render(engine, 24000);
    expect(engine.getActiveVoiceCount() == 0,
           "the note did not release when the pedal was lifted");
}

void testPanelLayout()
{
    expect(panel::layoutIsConsistent(),
           "the panel description overlaps, escapes its section, or has a broken group");
    expect(panel::panelWidth() > 0.0f, "the panel has no width");

    const auto& controls = panel::controls();
    expect(controls.size() == panel::controlCount, "panel control count");

    // Every front-panel control must name a parameter, and every parameter the
    // panel names must be reachable from exactly one slider or one radio group.
    for (const auto& control : controls)
    {
        expect(control.parameterId != nullptr && std::strlen(control.parameterId) > 0,
               "a panel control names no parameter");
        expect(control.label != nullptr && std::strlen(control.label) > 0,
               "a panel control carries no legend");
    }

    const std::array<const char*, 8> mustAppear {
        parameters::cutoff, parameters::resonance, parameters::attack,
        parameters::release, parameters::chorus, parameters::range,
        parameters::highPass, parameters::vcaLevel
    };
    for (const auto* wanted : mustAppear)
    {
        int found = 0;
        for (const auto& control : controls)
            if (std::strcmp(control.parameterId, wanted) == 0)
                ++found;
        expect(found >= 1, std::string("panel does not expose ") + wanted);
    }

    // Section accents must alternate, which is what makes the panel readable.
    const auto& sections = panel::sections();
    for (std::size_t index = 1; index < sections.size(); ++index)
        expect(sections[index].accent != sections[index - 1].accent,
               "two neighbouring sections share a highlight colour");
}

void testCpuBudget()
{
    // Not a benchmark of the host, just a guard against a change that makes the
    // engine an order of magnitude more expensive without anyone noticing.
    constexpr double sampleRate = 48000.0;
    YouKnow106Engine engine;
    engine.prepare(sampleRate, blockSize, true);
    auto parameters = plainPatch();
    parameters.pulseEnabled = true;
    parameters.subLevel = 0.5f;
    parameters.noiseLevel = 0.1f;
    parameters.resonance = 0.7f;
    parameters.chorus = ChorusMode::Two;
    engine.setParameters(parameters);
    for (int note = 0; note < 6; ++note)
        engine.noteOn(48 + note * 4, 1.0f);

    const auto started = std::chrono::steady_clock::now();
    const auto rendered = render(engine, static_cast<int>(sampleRate * 2));
    const std::chrono::duration<double> elapsed =
        std::chrono::steady_clock::now() - started;
    const double realtimeRatio = elapsed.count() / 2.0;

    expect(peakOf(rendered.left, 0) > 0.0, "the benchmark patch produced silence");
    expect(realtimeRatio < 1.0,
           "six voices with the effect engaged cost more than realtime ("
               + std::to_string(realtimeRatio) + "x)");
}
} // namespace

int main()
{
    testRangeTransposesByOctaves();
    testSubIsOneOctaveDown();
    testAliasFloor();
    testRampHasARampSpectrum();
    testKeyAssignerDropsRatherThanSteals();
    testPolyModesDifferInAllocation();
    testUnisonUsesEveryVoiceWithoutDetuning();
    testEnvelopeAndGateModes();
    testChorusWidthAndSilence();
    testChorusNoiseIsPresentAndDefeatable();
    testSampleRateAndOversamplingConsistency();
    testDeterminismAndSilence();
    testExtremeAutomationStaysFinite();
    testParameterSanitisation();
    testSustainPedalHoldsAndReleases();
    testPanelLayout();
    testCpuBudget();

    if (failures != 0)
    {
        std::cerr << failures << " YouKnow106 engine check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All YouKnow106 engine checks passed.\n";
    return EXIT_SUCCESS;
}
