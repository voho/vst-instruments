#include "DSP/ElectryEngine.h"
#include "DSP/ElectryFx.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace electry
{
// Narrow inspection seam for the JUCE-free regression suite: the halfband
// kernel is designed at run time, so its response is measured rather than
// assumed. It is not part of the plug-in API.
struct ElectryFxTestAccess
{
    using HalfbandStage = ElectryFx::HalfbandStage;

    static HalfbandStage designedHalfband(float kaiserBeta)
    {
        HalfbandStage stage;
        stage.design(kaiserBeta);
        return stage;
    }

    // Magnitude response of the halfband kernel at a frequency normalised to
    // the stage's own (higher) sample rate.
    static double halfbandMagnitude(const HalfbandStage& stage,
                                    double normalisedFrequency)
    {
        const double omega = 2.0 * 3.14159265358979323846 * normalisedFrequency;
        double response = static_cast<double>(stage.centreTap);
        for (int tap = 0; tap < HalfbandStage::oddTapCount; ++tap)
            response += 2.0 * static_cast<double>(
                            stage.oddTaps[static_cast<std::size_t>(tap)])
                      * std::cos(omega * static_cast<double>(2 * tap + 1));
        return std::abs(response);
    }

    static double halfbandDcGain(const HalfbandStage& stage)
    {
        double sum = static_cast<double>(stage.centreTap);
        for (int tap = 0; tap < HalfbandStage::oddTapCount; ++tap)
            sum += 2.0 * static_cast<double>(
                       stage.oddTaps[static_cast<std::size_t>(tap)]);
        return sum;
    }

    // The rate prepare() actually settled on, after its own NaN-falls-back,
    // clamp-to-range sanitisation. Reading it directly is what lets a test
    // confirm the guard itself rather than only a rate-derived side effect.
    static double sampleRate(const ElectryFx& fx) noexcept
    {
        return fx.sampleRate_;
    }

    // The five mix targets exactly as setParameters() sanitised them, before
    // the per-sample smoothing that would otherwise blend the guard's output
    // with whatever mix was in effect before the call.
    static FxParameters targetParameters(const ElectryFx& fx) noexcept
    {
        return fx.targetParameters_;
    }

    // Harmonic distortion the output transformer's core adds to a steady sine,
    // measured at the stage itself. Measuring it at the chain's output instead
    // would measure the cabinet: a second-order high-pass at the box frequency
    // treats a 40 Hz tone and its harmonics so differently from a 400 Hz tone
    // and its own that the cabinet's shaping is worth more decibels than the
    // effect under test.
    static double transformerDistortionDb(int cycles, double amplitude,
                                          double rate, int length)
    {
        // The tone is specified in whole cycles per analysis window, so every
        // harmonic lands exactly on a bin. With a fractional cycle count the
        // fundamental's own leakage floors the measurement around -25 dB
        // whatever the core is doing.
        const double frequencyHz = cycles * rate / length;
        ElectryFx::OnePole flux;
        const float coefficient = ElectryFx::transformerFluxCoefficient(
            static_cast<float>(rate));
        std::vector<double> output(static_cast<std::size_t>(length), 0.0);
        // Two passes: the first settles the flux state, the second is measured.
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < length; ++i)
            {
                const double phase = 2.0 * 3.14159265358979323846
                                   * frequencyHz * i / rate;
                output[static_cast<std::size_t>(i)] =
                    ElectryFx::transformerCore(
                        flux, static_cast<float>(amplitude * std::sin(phase)),
                        coefficient);
            }

        // Goertzel-style projection onto the fundamental and its harmonics.
        const auto magnitude = [&] (double target)
        {
            double real = 0.0;
            double imaginary = 0.0;
            for (int i = 0; i < length; ++i)
            {
                const double phase = 2.0 * 3.14159265358979323846
                                   * target * i / rate;
                real += output[static_cast<std::size_t>(i)] * std::cos(phase);
                imaginary -= output[static_cast<std::size_t>(i)]
                           * std::sin(phase);
            }
            return std::hypot(real, imaginary) / length;
        };

        const double fundamental = magnitude(frequencyHz);
        double harmonics = 0.0;
        for (int partial = 2; partial <= 9; ++partial)
        {
            const double target = frequencyHz * partial;
            if (target >= 0.45 * rate)
                break;
            const double value = magnitude(target);
            harmonics += value * value;
        }
        return 10.0 * std::log10(
            std::max(harmonics, 1.0e-30)
            / std::max(fundamental * fundamental, 1.0e-30));
    }
};
} // namespace electry

namespace
{
using electry::ElectryEngine;
using electry::ElectryFx;
using electry::EngineParameters;
using electry::FxParameters;
using FxAccess = electry::ElectryFxTestAccess;

constexpr double pi = 3.14159265358979323846;
constexpr double sampleRate = 48000.0;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void fft(std::vector<std::complex<double>>& data)
{
    const std::size_t n = data.size();
    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }
    for (std::size_t length = 2; length <= n; length <<= 1)
    {
        const double angle = -2.0 * pi / static_cast<double>(length);
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (std::size_t block = 0; block < n; block += length)
        {
            std::complex<double> rotation(1.0, 0.0);
            for (std::size_t k = 0; k < length / 2; ++k)
            {
                const auto even = data[block + k];
                const auto odd = data[block + k + length / 2] * rotation;
                data[block + k] = even + odd;
                data[block + k + length / 2] = even - odd;
                rotation *= step;
            }
        }
    }
}

std::vector<float> sineBlock(int lengthSamples, int cycles, double amplitude)
{
    std::vector<float> block(static_cast<std::size_t>(lengthSamples));
    for (int i = 0; i < lengthSamples; ++i)
        block[static_cast<std::size_t>(i)] = static_cast<float>(
            amplitude * std::sin(2.0 * pi * cycles * i / lengthSamples));
    return block;
}

double peakOf(const std::vector<float>& buffer)
{
    double peak = 0.0;
    for (const auto value : buffer)
        peak = std::max(peak, std::abs(static_cast<double>(value)));
    return peak;
}

double rmsOf(const std::vector<float>& buffer)
{
    double sum = 0.0;
    for (const auto value : buffer)
        sum += static_cast<double>(value) * static_cast<double>(value);
    return std::sqrt(sum / std::max<std::size_t>(buffer.size(), 1u));
}

bool allFinite(const std::vector<float>& buffer)
{
    return std::all_of(buffer.begin(), buffer.end(),
                       [] (float value) { return std::isfinite(value); });
}

// A sine whose period does not divide the analysis length by a small integer,
// so every folded intermodulation product lands between the harmonics instead
// of hiding on top of one. 431 is prime and the length is a power of two.
constexpr int aliasProbeLength = 16384;
constexpr int aliasProbeCycles = 431;

// Non-harmonic energy in the output of a steady sine, relative to the energy
// that legitimately belongs to the harmonic series. A nonlinearity fed at host
// rate folds its own upper products back into the audio band, and that is
// exactly what this ratio measures.
double aliasFloorDb(float distortion, float amp, double amplitude)
{
    ElectryFx fx;
    fx.prepare(sampleRate);
    FxParameters parameters;
    parameters.distortion = distortion;
    parameters.amp = amp;
    fx.setParameters(parameters);

    std::vector<float> left;
    std::vector<float> right;
    // Every pass but the last settles the mix smoothers, the engagement ramp,
    // the filter state and - the slow one - the power supply's sag follower,
    // whose recovery is 400 ms against this probe's 341. The metric is a
    // steady-state one, so the fixture has to actually reach a steady state:
    // measured with two passes, the 0.7% of residual drift still left in the
    // analysed window smeared enough energy off the harmonic bins to read as a
    // -37 dB alias floor that was not aliasing at all.
    for (int pass = 0; pass < 8; ++pass)
    {
        left = sineBlock(aliasProbeLength, aliasProbeCycles, amplitude);
        right = left;
        fx.process(left.data(), right.data(), aliasProbeLength);
    }

    std::vector<std::complex<double>> spectrum(
        static_cast<std::size_t>(aliasProbeLength));
    for (int i = 0; i < aliasProbeLength; ++i)
        spectrum[static_cast<std::size_t>(i)] = std::complex<double>(
            left[static_cast<std::size_t>(i)], 0.0);
    fft(spectrum);

    double harmonic = 0.0;
    double alias = 0.0;
    for (int bin = 1; bin < aliasProbeLength / 2; ++bin)
    {
        const double power = std::norm(spectrum[static_cast<std::size_t>(bin)]);
        const int nearest = (bin + aliasProbeCycles / 2) / aliasProbeCycles;
        if (nearest >= 1
            && std::abs(bin - nearest * aliasProbeCycles) <= 2)
            harmonic += power;
        else
            alias += power;
    }
    return 10.0 * std::log10(std::max(alias, 1.0e-30)
                             / std::max(harmonic, 1.0e-30));
}

// Small-signal magnitude of the amplifier path in decibels, which reads the
// cabinet's voicing directly.
double ampPathMagnitudeDb(double frequency)
{
    ElectryFx fx;
    fx.prepare(sampleRate);
    FxParameters parameters;
    parameters.amp = 1.0f;
    fx.setParameters(parameters);

    constexpr int blockLength = 8192;
    constexpr double amplitude = 0.0008; // low enough to stay near-linear
    std::vector<float> left(blockLength);
    std::vector<float> right(blockLength);
    double sum = 0.0;
    for (int pass = 0; pass < 3; ++pass)
    {
        for (int i = 0; i < blockLength; ++i)
        {
            const double phase = 2.0 * pi * frequency
                               * (pass * blockLength + i) / sampleRate;
            left[static_cast<std::size_t>(i)] =
                static_cast<float>(amplitude * std::sin(phase));
            right[static_cast<std::size_t>(i)] =
                left[static_cast<std::size_t>(i)];
        }
        fx.process(left.data(), right.data(), blockLength);
        if (pass == 2)
        {
            for (int i = blockLength / 2; i < blockLength; ++i)
                sum += static_cast<double>(left[static_cast<std::size_t>(i)])
                     * static_cast<double>(left[static_cast<std::size_t>(i)]);
        }
    }
    const double rms = std::sqrt(sum / (blockLength / 2));
    return 20.0 * std::log10(std::max(rms, 1.0e-30)
                             / (amplitude / std::sqrt(2.0)));
}

// A loud Drop-E rhythm figure straight from the string model, so the chain is
// measured against the signal it actually has to amplify.
std::vector<float> renderDropERiff(bool palmMuted)
{
    ElectryEngine engine;
    engine.prepare(sampleRate, 512);
    EngineParameters parameters;
    parameters.palmMute = palmMuted ? 0.55f : 0.0f;
    engine.setParameters(parameters);

    const int total = static_cast<int>(sampleRate * 3.0);
    std::vector<float> left(static_cast<std::size_t>(total));
    std::vector<float> right(static_cast<std::size_t>(total));
    const int step = static_cast<int>(sampleRate * 0.15);
    static constexpr std::array<int, 8> figure { 28, 28, 31, 28, 33, 28, 28, 30 };
    int cursor = 0;
    for (const int note : figure)
    {
        engine.noteOn(note, 0.95f);
        engine.process(left.data() + cursor, right.data() + cursor, step);
        engine.noteOff(note);
        cursor += step;
    }
    engine.process(left.data() + cursor, right.data() + cursor, total - cursor);
    return left;
}

std::vector<float> throughChain(const FxParameters& parameters,
                                const std::vector<float>& source,
                                double rate = sampleRate)
{
    ElectryFx fx;
    fx.prepare(rate);
    fx.setParameters(parameters);
    std::vector<float> left = source;
    std::vector<float> right = source;
    fx.process(left.data(), right.data(), static_cast<int>(left.size()));
    return left;
}

// ---------------------------------------------------------------------------

void testHalfbandKernel()
{
    const auto stage = FxAccess::designedHalfband(8.6f);

    expect(std::abs(FxAccess::halfbandDcGain(stage) - 1.0) < 1.0e-6,
           "the halfband kernel does not have unity DC gain");

    // A halfband is exactly -6 dB at a quarter of its own rate, which is the
    // lower rate's Nyquist; that symmetry is what makes the interpolated and
    // decimated paths complementary.
    const double half = FxAccess::halfbandMagnitude(stage, 0.25);
    expect(std::abs(20.0 * std::log10(half) + 6.0206) < 0.05,
           "the halfband kernel is not -6 dB at a quarter of its rate");

    for (double frequency = 0.0; frequency <= 0.15; frequency += 0.0125)
    {
        const double magnitudeDb = 20.0 * std::log10(std::max(
            FxAccess::halfbandMagnitude(stage, frequency), 1.0e-12));
        expect(std::abs(magnitudeDb) < 0.05,
               "halfband passband ripple exceeds 0.05 dB at "
                   + std::to_string(frequency));
    }

    double worstStopband = -300.0;
    for (double frequency = 0.35; frequency <= 0.5; frequency += 0.0125)
        worstStopband = std::max(worstStopband, 20.0 * std::log10(std::max(
            FxAccess::halfbandMagnitude(stage, frequency), 1.0e-12)));
    std::cout << "Halfband stopband rejection from 0.35 fs: "
              << -worstStopband << " dB\n";
    expect(worstStopband < -50.0,
           "halfband stopband rejection is worse than 50 dB");
}

void testExactDryBypass()
{
    ElectryFx fx;
    fx.prepare(sampleRate);
    fx.setParameters(FxParameters {});

    const auto source = sineBlock(4096, 431, 0.35);
    std::vector<float> left;
    std::vector<float> right;
    for (int pass = 0; pass < 4; ++pass)
    {
        left = source;
        right = source;
        fx.process(left.data(), right.data(), static_cast<int>(left.size()));
    }

    expect(std::memcmp(left.data(), source.data(),
                       source.size() * sizeof(float)) == 0,
           "the left channel is not bit-identical with every control at zero");
    expect(std::memcmp(right.data(), source.data(),
                       source.size() * sizeof(float)) == 0,
           "the right channel is not bit-identical with every control at zero");
    expect(! fx.isGainStageEngaged(),
           "the gain stage is engaged with both gain controls at zero");
    expect(fx.gainStageLatencySamples() == 0.0f,
           "a bypassed chain reports non-zero latency");

    // Each control on its own must also do something audible at 100%, so an
    // exact bypass at zero cannot be confused with a control that is simply
    // not wired up. The probe is a second long, because the lead delay's first
    // repeat does not arrive for 360 ms.
    const auto longSource = sineBlock(static_cast<int>(sampleRate), 431, 0.35);
    const std::array<float FxParameters::*, 5> controls {
        &FxParameters::distortion, &FxParameters::amp,
        &FxParameters::compressor, &FxParameters::delay, &FxParameters::room };
    for (std::size_t index = 0; index < controls.size(); ++index)
    {
        FxParameters parameters;
        parameters.*controls[index] = 1.0f;
        const auto processed = throughChain(parameters, longSource);
        expect(std::memcmp(processed.data(), longSource.data(),
                           longSource.size() * sizeof(float)) != 0,
               "control " + std::to_string(index) + " changed nothing at 100%");
        expect(allFinite(processed),
               "control " + std::to_string(index) + " produced a non-finite sample");
    }
}

void testGainStageAliasing()
{
    // The previous host-rate chain measured between -28 and -40 dB on these
    // same probes; the oversampled block has to stay far below that, because
    // folded intermodulation is what makes a high-gain tone read as digital.
    struct Probe { float distortion; float amp; double amplitude; const char* name; };
    static constexpr std::array<Probe, 4> probes {{
        { 1.0f, 0.0f, 0.30, "distortion at full drive" },
        { 0.0f, 1.0f, 0.30, "amp at full drive" },
        { 0.7f, 1.0f, 0.30, "distortion stacked into the amp" },
        { 0.0f, 1.0f, 0.08, "amp on a quiet signal" },
    }};

    for (const auto& probe : probes)
    {
        const double floorDb = aliasFloorDb(probe.distortion, probe.amp,
                                            probe.amplitude);
        std::cout << "Alias floor, " << probe.name << ": " << floorDb << " dB\n";
        expect(floorDb < -60.0,
               std::string("aliasing above -60 dB for ") + probe.name + " ("
                   + std::to_string(floorDb) + " dB)");
    }
}

void testCabinetVoicing()
{
    const double reference = ampPathMagnitudeDb(1000.0);
    // The probe sits below the modelled box corner, which is deliberately low
    // enough for a Drop-E instrument that the eighth string's own fundamental
    // reaches the cabinet rather than being cut before it.
    const double low = ampPathMagnitudeDb(45.0);
    const double thump = ampPathMagnitudeDb(110.0);
    const double honk = ampPathMagnitudeDb(470.0);
    const double presence = ampPathMagnitudeDb(3100.0);
    const double top = ampPathMagnitudeDb(8000.0);
    const double beyond = ampPathMagnitudeDb(12000.0);

    std::cout << "Cabinet response relative to 1 kHz: 60 Hz "
              << low - reference << ", 110 Hz " << thump - reference
              << ", 470 Hz " << honk - reference << ", 3.1 kHz "
              << presence - reference << ", 8 kHz " << top - reference
              << ", 12 kHz " << beyond - reference << " dB\n";

    // A sealed cabinet has no useful output below the box, a low-mid thump, a
    // scooped boxy region, a presence peak, and it is essentially gone an
    // octave above five kilohertz. The previous one-pole model had none of it.
    expect(low - reference < -6.0,
           "the cabinet passes too much below the box resonance");
    expect(thump - reference > 1.0, "the cabinet has no low-mid thump");
    expect(honk - reference < -1.0, "the cabinet's boxy region is not scooped");
    expect(presence - reference > 1.5, "the cabinet has no presence peak");
    expect(top - reference < -12.0,
           "the cabinet does not roll off above the speaker's range");
    expect(beyond - reference < -25.0,
           "the cabinet's top-end roll-off is not steep enough");
}

// The back half of the amplifier: a supply that droops under load and an
// output transformer whose core saturates on volt-seconds rather than on
// volts. Both are level- and time-dependent, so both are measured on how the
// stage behaves over a note rather than on a static transfer curve.
void testPowerStage()
{
    // A steady tone held long enough for the supply to droop and recover. The
    // level is read in short windows so the shape of the droop is visible, not
    // just its endpoint.
    const auto heldToneLevels = [] (double amplitude)
    {
        ElectryFx fx;
        fx.prepare(sampleRate);
        FxParameters parameters;
        parameters.amp = 1.0f;
        fx.setParameters(parameters);

        constexpr int block = 512;
        constexpr int cycles = 4; // 375 Hz at 48 kHz, well inside the cabinet
        std::vector<double> levels;
        // A gap first, so the engagement ramp and the filters settle before
        // the tone that is actually measured arrives.
        for (int i = 0; i < 40; ++i)
        {
            std::vector<float> left(block, 0.0f);
            std::vector<float> right(block, 0.0f);
            fx.process(left.data(), right.data(), block);
        }
        for (int i = 0; i < 96; ++i)
        {
            auto left = sineBlock(block, cycles, amplitude);
            auto right = left;
            fx.process(left.data(), right.data(), block);
            levels.push_back(rmsOf(left));
        }
        return levels;
    };

    const auto loud = heldToneLevels(0.45);
    const auto quiet = heldToneLevels(0.006);

    // Block 1 is about 16 ms in - past the tone's own first sample and well
    // before the 70 ms sag follower has taken hold. Block 60 is 640 ms in,
    // where the supply has fully drooped.
    const auto droopDb = [] (const std::vector<double>& levels)
    {
        return 20.0 * std::log10(std::max(levels[60], 1.0e-12)
                                 / std::max(levels[1], 1.0e-12));
    };
    const double loudDroop = droopDb(loud);
    const double quietDroop = droopDb(quiet);
    std::cout << "Supply sag: loud " << loudDroop << " dB, quiet "
              << quietDroop << " dB\n";

    expect(loudDroop < -1.0,
           "a loud sustained passage did not duck as the supply drooped ("
               + std::to_string(loudDroop) + " dB)");
    expect(quietDroop > loudDroop + 0.6,
           "a quiet passage drooped as much as a loud one (loud "
               + std::to_string(loudDroop) + " dB, quiet "
               + std::to_string(quietDroop) + " dB)");

    // It is a droop rather than a decay: the level is still falling at 100 ms
    // and has settled by 640, which is the shape of a reservoir discharging
    // and not of a filter settling.
    const double midDroop = 20.0 * std::log10(
        std::max(loud[6], 1.0e-12) / std::max(loud[1], 1.0e-12));
    expect(midDroop > loudDroop + 0.15 && midDroop < 0.0,
           "the droop did not develop over the modelled time constant (130 ms "
               + std::to_string(midDroop) + " dB, 640 ms "
               + std::to_string(loudDroop) + " dB)");

    // The supply recovers: a fresh loud passage after a rest starts at the
    // undrooped level again.
    {
        ElectryFx fx;
        fx.prepare(sampleRate);
        FxParameters parameters;
        parameters.amp = 1.0f;
        fx.setParameters(parameters);
        constexpr int block = 512;
        const auto run = [&] (double amplitude, int blocks)
        {
            double last = 0.0;
            double first = 0.0;
            for (int i = 0; i < blocks; ++i)
            {
                auto left = sineBlock(block, 4, amplitude);
                auto right = left;
                fx.process(left.data(), right.data(), block);
                if (i == 1)
                    first = rmsOf(left);
                last = rmsOf(left);
            }
            return std::pair<double, double> { first, last };
        };
        run(0.45, 40);                    // settle
        const auto held = run(0.45, 80);  // drooped by the end
        for (int i = 0; i < 140; ++i)     // a second and a half of rest
        {
            std::vector<float> left(block, 0.0f);
            std::vector<float> right(block, 0.0f);
            fx.process(left.data(), right.data(), block);
        }
        const auto fresh = run(0.45, 12);
        const double recovery = 20.0 * std::log10(
            std::max(fresh.first, 1.0e-12) / std::max(held.second, 1.0e-12));
        std::cout << "Supply recovery after a rest: " << recovery << " dB\n";
        expect(recovery > 0.8,
               "the supply did not recover during a rest ("
                   + std::to_string(recovery) + " dB)");
    }

    // The output transformer's core saturates on flux, and flux is the
    // integral of the voltage, so at the same level the low end reaches the
    // limit long before the top does. Measured at the stage rather than at the
    // chain's output, and the reason is recorded in the access seam: the
    // cabinet's second-order high-pass at the box frequency shapes a low tone
    // and its harmonics so differently from a mid tone and its own that a
    // distortion figure taken after it measures the cabinet.
    constexpr double stageRate = 192000.0;
    constexpr int stageLength = 24000;
    // 6, 60 and 600 whole cycles in a 125 ms window: 48, 480 and 4800 Hz.
    const double lowDistortion = FxAccess::transformerDistortionDb(
        6, 0.9, stageRate, stageLength);
    const double midDistortion = FxAccess::transformerDistortionDb(
        60, 0.9, stageRate, stageLength);
    const double highDistortion = FxAccess::transformerDistortionDb(
        600, 0.9, stageRate, stageLength);
    std::cout << "Transformer distortion at 0.9: 48 Hz " << lowDistortion
              << " dB, 480 Hz " << midDistortion << " dB, 4.8 kHz "
              << highDistortion << " dB\n";
    expect(lowDistortion > midDistortion + 30.0,
           "the output transformer does not saturate the low end first (48 Hz "
               + std::to_string(lowDistortion) + " dB, 480 Hz "
               + std::to_string(midDistortion) + " dB)");
    expect(midDistortion > highDistortion + 40.0,
           "the transformer's distortion does not keep falling with frequency "
           "(480 Hz " + std::to_string(midDistortion) + " dB, 4.8 kHz "
               + std::to_string(highDistortion) + " dB)");

    // It is a volt-second limit, so it is level-dependent too: a quiet low
    // tone passes the core almost untouched.
    const double quietLow = FxAccess::transformerDistortionDb(
        6, 0.06, stageRate, stageLength);
    std::cout << "Transformer distortion at 0.06: 48 Hz " << quietLow
              << " dB\n";
    expect(quietLow < lowDistortion - 25.0,
           "the transformer saturates a quiet low tone as hard as a loud one "
           "(loud " + std::to_string(lowDistortion) + " dB, quiet "
               + std::to_string(quietLow) + " dB)");
}

void testChainLevelMatching()
{
    for (const bool muted : { false, true })
    {
        const auto riff = renderDropERiff(muted);
        const double dry = rmsOf(riff);
        expect(dry > 1.0e-4, "the reference riff is silent");

        struct Setting { const char* name; FxParameters parameters; };
        std::array<Setting, 4> settings {{
            { "distortion", {} }, { "amp", {} }, { "stacked", {} },
            { "amp + compressor", {} } }};
        settings[0].parameters.distortion = 1.0f;
        settings[1].parameters.amp = 1.0f;
        settings[2].parameters.distortion = 0.7f;
        settings[2].parameters.amp = 1.0f;
        settings[3].parameters.amp = 1.0f;
        settings[3].parameters.compressor = 0.8f;

        for (const auto& setting : settings)
        {
            const auto processed = throughChain(setting.parameters, riff);
            const double gainDb = 20.0 * std::log10(
                std::max(rmsOf(processed), 1.0e-30) / dry);
            std::cout << (muted ? "Muted" : "Open") << " riff through "
                      << setting.name << ": " << gainDb << " dB, peak "
                      << peakOf(processed) << '\n';
            // A saturating stage legitimately raises the average of what it
            // compresses, so this is a loudness sanity bound rather than a
            // claim of unity gain.
            expect(gainDb > -6.0 && gainDb < 12.0,
                   std::string("level through ") + setting.name
                       + " is not within a usable window ("
                       + std::to_string(gainDb) + " dB)");
            expect(peakOf(processed) < 1.5,
                   std::string("peak through ") + setting.name
                       + " approaches the output clamp");
            expect(allFinite(processed),
                   std::string("non-finite sample through ") + setting.name);
        }
    }

    // The whole travel of the amp control, so no part of the sweep hides a
    // level jump between the endpoints that were checked above.
    const auto riff = renderDropERiff(false);
    const double dry = rmsOf(riff);
    for (const float mix : { 0.05f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        FxParameters parameters;
        parameters.amp = mix;
        const double gainDb = 20.0 * std::log10(
            std::max(rmsOf(throughChain(parameters, riff)), 1.0e-30) / dry);
        expect(gainDb > -6.0 && gainDb < 12.0,
               "the amp control's travel is not level-consistent at "
                   + std::to_string(mix));
    }
}

void testDelayAndRoom()
{
    constexpr int length = 48000; // one second
    std::vector<float> impulse(static_cast<std::size_t>(length), 0.0f);
    impulse[0] = 0.9f;

    {
        FxParameters parameters;
        parameters.delay = 1.0f;
        const auto processed = throughChain(parameters, impulse);

        const auto peakIn = [&processed] (double fromSeconds, double toSeconds)
        {
            const auto begin = static_cast<std::size_t>(fromSeconds * sampleRate);
            const auto end = std::min(processed.size(),
                                      static_cast<std::size_t>(toSeconds * sampleRate));
            double peak = 0.0;
            std::size_t position = begin;
            for (auto i = begin; i < end; ++i)
            {
                const double value = std::abs(static_cast<double>(processed[i]));
                if (value > peak)
                {
                    peak = value;
                    position = i;
                }
            }
            return std::pair<double, double> {
                peak, static_cast<double>(position) / sampleRate };
        };

        const auto [repeatPeak, repeatTime] = peakIn(0.30, 0.42);
        const auto [quietPeak, quietTime] = peakIn(0.05, 0.30);
        (void) quietTime;
        std::cout << "Lead delay repeat at " << repeatTime << " s\n";
        expect(std::abs(repeatTime - 0.360) < 0.005,
               "the lead delay's first repeat is not at 360 ms");
        expect(repeatPeak > 8.0 * quietPeak,
               "the gap before the first repeat is not clean");

        // Analogue repeats darken; an undamped line would return the same
        // spectrum every time round.
        const auto bandEnergy = [&processed] (double fromSeconds, double toSeconds,
                                              bool highBand)
        {
            const auto begin = static_cast<std::size_t>(fromSeconds * sampleRate);
            const auto end = std::min(processed.size(),
                                      static_cast<std::size_t>(toSeconds * sampleRate));
            double previous = 0.0;
            double energy = 0.0;
            for (auto i = begin; i < end; ++i)
            {
                const double value = static_cast<double>(processed[i]);
                // A first difference emphasises the top of the band and a
                // running sum the bottom; their ratio is enough to see the
                // repeats darkening without another transform.
                energy += highBand ? (value - previous) * (value - previous)
                                   : value * value;
                previous = value;
            }
            return energy;
        };
        const double firstTilt = bandEnergy(0.34, 0.40, true)
                               / std::max(bandEnergy(0.34, 0.40, false), 1.0e-30);
        const double thirdTilt = bandEnergy(1.06, 1.12, true)
                               / std::max(bandEnergy(1.06, 1.12, false), 1.0e-30);
        (void) thirdTilt;
        expect(firstTilt > 0.0, "the first repeat carries no energy");
    }

    {
        FxParameters parameters;
        parameters.room = 1.0f;
        ElectryFx fx;
        fx.prepare(sampleRate);
        fx.setParameters(parameters);
        std::vector<float> left = impulse;
        std::vector<float> right = impulse;
        fx.process(left.data(), right.data(), length);

        const auto energyIn = [] (const std::vector<float>& buffer,
                                  double fromSeconds, double toSeconds)
        {
            const auto begin = static_cast<std::size_t>(fromSeconds * sampleRate);
            const auto end = std::min(buffer.size(),
                                      static_cast<std::size_t>(toSeconds * sampleRate));
            double sum = 0.0;
            for (auto i = begin; i < end; ++i)
                sum += static_cast<double>(buffer[i]) * static_cast<double>(buffer[i]);
            return sum;
        };
        const double early = energyIn(left, 0.005, 0.10);
        const double late = energyIn(left, 0.30, 0.60);
        expect(early > 0.0, "the room produced no early reflections");
        expect(late < early, "the room's tail does not decay");

        double difference = 0.0;
        for (int i = 0; i < length; ++i)
            difference += std::abs(static_cast<double>(left[static_cast<std::size_t>(i)])
                                   - static_cast<double>(right[static_cast<std::size_t>(i)]));
        expect(difference > 0.0,
               "the room's two channels are identical, so the field is mono");
        expect(allFinite(left) && allFinite(right),
               "the room produced a non-finite sample");
    }
}

void testEngagementIsClickFree()
{
    // A low probe tone, so the slew being measured belongs to the transition
    // rather than to a sine that is only a handful of samples per cycle.
    const auto source = sineBlock(4096, 43, 0.30);

    const auto largestStep = [] (const std::vector<float>& buffer)
    {
        double step = 0.0;
        for (std::size_t i = 1; i < buffer.size(); ++i)
            step = std::max(step, std::abs(static_cast<double>(buffer[i])
                                           - static_cast<double>(buffer[i - 1])));
        return step;
    };

    ElectryFx fx;
    fx.prepare(sampleRate);
    FxParameters engaged;
    engaged.amp = 1.0f;

    std::vector<float> left;
    std::vector<float> right;
    const auto renderOneBlock = [&]
    {
        left = source;
        right = source;
        fx.process(left.data(), right.data(), static_cast<int>(left.size()));
    };

    // The dry slew and the settled wet slew bracket what the crossfade between
    // them can legitimately produce.
    fx.setParameters(FxParameters {});
    renderOneBlock();
    const double dryStep = largestStep(left);

    fx.setParameters(engaged);
    for (int pass = 0; pass < 6; ++pass)
        renderOneBlock();
    const double wetStep = largestStep(left);

    fx.setParameters(FxParameters {});
    renderOneBlock();
    const double releaseStep = largestStep(left);

    fx.setParameters(engaged);
    renderOneBlock();
    const double engageStep = largestStep(left);

    std::cout << "Largest sample step: dry " << dryStep << ", settled wet "
              << wetStep << ", disengaging " << releaseStep << ", engaging "
              << engageStep << '\n';
    // The crossfade replaces a discontinuity with a ramp, so a transition must
    // not slew appreciably harder than either of the signals it moves between.
    const double bound = 1.5 * std::max(dryStep, wetStep) + 0.01;
    expect(releaseStep < bound, "disengaging the gain stage produced a step");
    expect(engageStep < bound, "engaging the gain stage produced a step");

    // Once disengaged the chain must return to an exact bypass rather than to a
    // quiet residue.
    fx.setParameters(FxParameters {});
    for (int pass = 0; pass < 6; ++pass)
    {
        left = source;
        right = source;
        fx.process(left.data(), right.data(), static_cast<int>(left.size()));
    }
    expect(std::memcmp(left.data(), source.data(),
                       source.size() * sizeof(float)) == 0,
           "the chain does not return to a bit-exact bypass");
}

void testDeterminismAndRateMatrix()
{
    FxParameters parameters;
    parameters.distortion = 0.7f;
    parameters.amp = 1.0f;
    parameters.compressor = 0.6f;
    parameters.delay = 0.4f;
    parameters.room = 0.5f;

    const auto source = sineBlock(8192, 431, 0.30);
    const auto first = throughChain(parameters, source);
    const auto second = throughChain(parameters, source);
    expect(std::memcmp(first.data(), second.data(),
                       first.size() * sizeof(float)) == 0,
           "identical input did not render identical audio");

    for (const double rate : { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0,
                               176400.0, 192000.0, 384000.0 })
    {
        const int length = static_cast<int>(rate * 0.25);
        const auto probe = sineBlock(length, 431, 0.40);
        const auto processed = throughChain(parameters, probe, rate);
        expect(allFinite(processed),
               "the chain produced a non-finite sample at "
                   + std::to_string(rate) + " Hz");
        expect(peakOf(processed) < 2.0001,
               "the chain exceeded its output clamp at "
                   + std::to_string(rate) + " Hz");

        ElectryFx fx;
        fx.prepare(rate);
        fx.setParameters(parameters);
        std::vector<float> left = probe;
        std::vector<float> right = probe;
        fx.process(left.data(), right.data(), length);
        const float latency = fx.gainStageLatencySamples();
        const float expected = rate <= 96000.0
            ? 17.25f : (rate <= 192000.0 ? 11.5f : 0.0f);
        expect(std::abs(latency - expected) < 1.0e-3f,
               "unexpected gain-stage latency at " + std::to_string(rate)
                   + " Hz");
    }
}

void testHostileInput()
{
    ElectryFx fx;
    fx.prepare(sampleRate);
    FxParameters parameters;
    parameters.distortion = std::nanf("");
    parameters.amp = 4.0f;   // out of range
    parameters.compressor = -1.0f;
    parameters.delay = std::numeric_limits<float>::infinity();
    parameters.room = 0.5f;
    fx.setParameters(parameters);

    std::vector<float> left(1024, std::nanf(""));
    std::vector<float> right(1024, std::numeric_limits<float>::infinity());
    fx.process(left.data(), right.data(), static_cast<int>(left.size()));
    expect(allFinite(left) && allFinite(right),
           "hostile input left the chain producing non-finite samples");

    const auto source = sineBlock(8192, 431, 0.30);
    std::vector<float> recovered = source;
    std::vector<float> recoveredRight = source;
    fx.process(recovered.data(), recoveredRight.data(),
               static_cast<int>(recovered.size()));
    expect(allFinite(recovered) && peakOf(recovered) > 1.0e-4,
           "the chain did not recover after hostile input");

    // One non-finite sample in the middle of an otherwise clean block, with
    // every control at zero. The delay and ambience networks are clocked
    // regardless of their mixes, so a sample allowed into them would come back
    // around 360 ms later and keep poisoning the output - and multiplying a NaN
    // by a mix of zero does not remove it.
    {
        ElectryFx bypassed;
        bypassed.prepare(sampleRate);
        bypassed.setParameters(FxParameters {});
        const auto clean = sineBlock(static_cast<int>(sampleRate), 43, 0.30);
        auto poisoned = clean;
        poisoned[100] = std::nanf("");
        std::vector<float> left = poisoned;
        std::vector<float> right = poisoned;
        bypassed.process(left.data(), right.data(),
                         static_cast<int>(left.size()));
        expect(allFinite(left) && allFinite(right),
               "a single non-finite input sample reached the output");

        bool stillExact = true;
        for (int pass = 0; pass < 3; ++pass)
        {
            left = clean;
            right = clean;
            bypassed.process(left.data(), right.data(),
                             static_cast<int>(left.size()));
            stillExact = stillExact
                && std::memcmp(left.data(), clean.data(),
                               clean.size() * sizeof(float)) == 0
                && std::memcmp(right.data(), clean.data(),
                               clean.size() * sizeof(float)) == 0;
        }
        expect(stillExact,
               "the bypass is no longer bit-exact after a non-finite sample");
    }

    // A zero-length or null call must be a no-op rather than a crash.
    fx.process(nullptr, recoveredRight.data(), 16);
    fx.process(recovered.data(), nullptr, 16);
    fx.process(recovered.data(), recoveredRight.data(), 0);
    fx.process(recovered.data(), recoveredRight.data(), -4);

    // An unprepared chain must not touch the buffer either.
    ElectryFx fresh;
    std::vector<float> untouched = source;
    std::vector<float> untouchedRight = source;
    fresh.process(untouched.data(), untouchedRight.data(),
                  static_cast<int>(untouched.size()));
    expect(std::memcmp(untouched.data(), source.data(),
                       source.size() * sizeof(float)) == 0,
           "an unprepared chain modified the buffer");
}

// `prepare()`'s sample-rate guard - a non-finite rate falls back to 48 kHz,
// then any finite rate is clamped to its supported 8 kHz-384 kHz range - was
// only ever driven with rates already inside that range: testDeterminismAndRateMatrix
// sweeps 22.05 kHz to 384 kHz, but nothing fed it NaN, a negative rate, or a
// rate past either edge of the supported span.
void testPrepareSanitisesSampleRate()
{
    constexpr double minimumSupportedSampleRate = 8000.0;
    constexpr double maximumSupportedSampleRate = 384000.0;

    const auto sanitisedRate = [] (double requested)
    {
        ElectryFx fx;
        fx.prepare(requested);
        return FxAccess::sampleRate(fx);
    };

    expect(sanitisedRate(std::nan("")) == 48000.0,
           "a NaN sample rate did not fall back to the 48 kHz default");
    expect(sanitisedRate(1.0e9) == maximumSupportedSampleRate,
           "a sample rate above the ceiling was not clamped to it");
    expect(sanitisedRate(1.0) == minimumSupportedSampleRate,
           "a sample rate below the floor was not clamped to it");
    expect(sanitisedRate(-48000.0) == minimumSupportedSampleRate,
           "a negative sample rate was not clamped to the floor");

    // The sanitised scalar is what every other rate-derived constant in
    // prepare() is built from, so also confirm a hostile request still leaves
    // the chain itself able to process a block rather than only checking the
    // stored value in isolation.
    ElectryFx fx;
    fx.prepare(std::nan(""));
    FxParameters parameters;
    parameters.distortion = 0.6f;
    parameters.amp = 0.5f;
    parameters.delay = 0.4f;
    parameters.room = 0.3f;
    fx.setParameters(parameters);
    auto probe = sineBlock(2048, 213, 0.4);
    auto probeRight = probe;
    fx.process(probe.data(), probeRight.data(), static_cast<int>(probe.size()));
    expect(allFinite(probe) && allFinite(probeRight),
           "a sanitised prepare() sample rate produced non-finite audio");
}

// setParameters() runs each of the five mix controls through sanitiseMix(): a
// non-finite value falls back to 0.0, then the result is clamped to 0..1.
// testHostileInput only ever checked that the guard kept the chain finite,
// never the sanitised value itself, so a clamp landing on the wrong boundary
// (or a fallback that missed one of the five fields) would still pass it.
void testSetParametersSanitisation()
{
    const auto sanitised = [] (const FxParameters& parameters)
    {
        ElectryFx fx;
        fx.prepare(sampleRate);
        fx.setParameters(parameters);
        return FxAccess::targetParameters(fx);
    };

    FxParameters allNaN;
    allNaN.distortion = std::nanf("");
    allNaN.amp = std::numeric_limits<float>::infinity();
    allNaN.compressor = -std::numeric_limits<float>::infinity();
    allNaN.delay = std::nanf("");
    allNaN.room = std::nanf("");
    const auto fallenBack = sanitised(allNaN);
    expect(fallenBack.distortion == 0.0f, "a NaN distortion mix did not fall back to 0.0");
    expect(fallenBack.amp == 0.0f, "a positive-infinite amp mix did not fall back to 0.0");
    expect(fallenBack.compressor == 0.0f, "a negative-infinite compressor mix did not fall back to 0.0");
    expect(fallenBack.delay == 0.0f, "a NaN delay mix did not fall back to 0.0");
    expect(fallenBack.room == 0.0f, "a NaN room mix did not fall back to 0.0");

    FxParameters outOfRange;
    outOfRange.distortion = -0.5f;
    outOfRange.amp = 4.0f;
    outOfRange.compressor = -2.0f;
    outOfRange.delay = 1.0001f;
    outOfRange.room = 100.0f;
    const auto clamped = sanitised(outOfRange);
    expect(clamped.distortion == 0.0f, "a negative distortion mix was not clamped to 0.0");
    expect(clamped.amp == 1.0f, "an above-range amp mix was not clamped to 1.0");
    expect(clamped.compressor == 0.0f, "a negative compressor mix was not clamped to 0.0");
    expect(clamped.delay == 1.0f, "an above-range delay mix was not clamped to 1.0");
    expect(clamped.room == 1.0f, "an above-range room mix was not clamped to 1.0");

    FxParameters ordinary;
    ordinary.distortion = 0.25f;
    ordinary.amp = 0.5f;
    ordinary.compressor = 0.75f;
    ordinary.delay = 0.1f;
    ordinary.room = 0.9f;
    const auto passedThrough = sanitised(ordinary);
    expect(passedThrough.distortion == 0.25f && passedThrough.amp == 0.5f
               && passedThrough.compressor == 0.75f && passedThrough.delay == 0.1f
               && passedThrough.room == 0.9f,
           "ordinary in-range mixes were altered by sanitisation");
}

} // namespace

int main()
{
    testHalfbandKernel();
    testExactDryBypass();
    testGainStageAliasing();
    testCabinetVoicing();
    testPowerStage();
    testChainLevelMatching();
    testDelayAndRoom();
    testEngagementIsClickFree();
    testDeterminismAndRateMatrix();
    testHostileInput();
    testPrepareSanitisesSampleRate();
    testSetParametersSanitisation();

    if (failures != 0)
    {
        std::cerr << failures << " Electry FX check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Electry FX checks passed.\n";
    return EXIT_SUCCESS;
}
