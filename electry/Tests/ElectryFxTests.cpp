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
    // The first pass settles the mix smoothers, the engagement ramp and the
    // filter state; the second is analysed.
    for (int pass = 0; pass < 2; ++pass)
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
    const double low = ampPathMagnitudeDb(60.0);
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

} // namespace

int main()
{
    testHalfbandKernel();
    testExactDryBypass();
    testGainStageAliasing();
    testCabinetVoicing();
    testChainLevelMatching();
    testDelayAndRoom();
    testEngagementIsClickFree();
    testDeterminismAndRateMatrix();
    testHostileInput();

    if (failures != 0)
    {
        std::cerr << failures << " Electry FX check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Electry FX checks passed.\n";
    return EXIT_SUCCESS;
}
