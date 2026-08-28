#include "DSP/ElectryEngine.h"
#include "DSP/ElectryFx.h"
#if ELECTRY_MEASURED_MODERN_CABINET
#include "DSP/ModernCabinetIR.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
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

    static HalfbandStage designedHalfband()
    {
        HalfbandStage stage;
        stage.design();
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

    static double internalRate(const ElectryFx& fx) noexcept
    {
        return fx.oversampledRate_;
    }

    // The five control targets exactly as setParameters() sanitised them,
    // before per-sample smoothing would blend the guard's output with the
    // value in effect before the call.
    static FxParameters targetParameters(const ElectryFx& fx) noexcept
    {
        return fx.targetParameters_;
    }

    static float diodeStep(double inputVolts, double rate,
                           double& outputVolts,
                           double& previousDerivative) noexcept
    {
        return ElectryFx::diodePairStep(inputVolts, rate, outputVolts,
                                        previousDerivative);
    }

    static double cathodeCurrent(double plateVoltage,
                                 double gridToCathodeVoltage) noexcept
    {
        return ElectryFx::triodeCathodeCurrent(plateVoltage,
                                               gridToCathodeVoltage);
    }

    static double plateCurrent(double plateVoltage,
                               double gridToCathodeVoltage) noexcept
    {
        return ElectryFx::triodePlateCurrent(plateVoltage,
                                             gridToCathodeVoltage);
    }

    static double gridCurrent(double gridToCathodeVoltage) noexcept
    {
        return ElectryFx::triodeGridCurrent(gridToCathodeVoltage);
    }

    static double solvePlate(double gridToCathodeVoltage,
                             double supplyVoltage,
                             double& warmStart) noexcept
    {
        return ElectryFx::solveTriodePlate(gridToCathodeVoltage,
                                           supplyVoltage, warmStart);
    }

    static float triodeOutput(double gridVoltage,
                              double& plateVoltage) noexcept
    {
        return ElectryFx::triodeStage(gridVoltage, plateVoltage);
    }

    static float triodeLookup(double gridVoltage) noexcept
    {
        return ElectryFx::triodeStageLookup(gridVoltage);
    }

    static double phaseInverterPlateCurrent(
        AmpModel model, double plateToCathodeVoltage,
        double gridToCathodeVoltage) noexcept
    {
        return ElectryFx::phaseInverterPlateCurrent(
            model, plateToCathodeVoltage, gridToCathodeVoltage);
    }

    static std::array<double, 6> phaseInverterDirect(
        AmpModel model, double drive) noexcept
    {
        const auto result = ElectryFx::phaseInverterDirect(model, drive);
        return { result.output, result.plateOne, result.plateTwo,
                 result.cathode, result.tail, result.totalCurrent };
    }

    static double powerGridCurrentDirect(double gridVoltage) noexcept
    {
        return ElectryFx::powerGridCurrentDirect(gridVoltage);
    }

    static double powerGridCurrentLookup(double gridVoltage) noexcept
    {
        return ElectryFx::powerGridCurrentLookup(gridVoltage);
    }

    static std::array<double, 10> phaseInverterCoupledStep(
        AmpModel model, double drive, double sampleRate,
        std::array<double, 5>& state) noexcept
    {
        ElectryFx::AmpChannel channel {};
        channel.phaseCurrentDelta = state[0];
        channel.couplingHistoryOne = state[1];
        channel.couplingHistoryTwo = state[2];
        channel.powerGridOffsetOne = state[3];
        channel.powerGridOffsetTwo = state[4];
        const auto result = ElectryFx::phaseInverterCoupledStep(
            channel, model, drive, sampleRate);
        state = { channel.phaseCurrentDelta,
                  channel.couplingHistoryOne,
                  channel.couplingHistoryTwo,
                  channel.powerGridOffsetOne,
                  channel.powerGridOffsetTwo };
        return { result.gridOne, result.gridTwo,
                 result.plateOne, result.plateTwo,
                 result.capacitorCurrentOne,
                 result.capacitorCurrentTwo,
                 result.gridCurrentOne, result.gridCurrentTwo,
                 result.totalCurrent, result.maximumResidual };
    }

    static double powerTubePlateCurrent(
        AmpModel model, double plateVoltage, double gridVoltage,
        double screenVoltage) noexcept
    {
        return ElectryFx::powerTubePlateCurrent(
            model, plateVoltage, gridVoltage, screenVoltage);
    }

    static double powerTubeScreenCurrent(
        AmpModel model, double plateVoltage, double gridVoltage,
        double screenVoltage) noexcept
    {
        return ElectryFx::powerTubeScreenCurrent(
            model, plateVoltage, gridVoltage, screenVoltage);
    }

    static std::array<double, 5> powerTubePairDirect(
        AmpModel model, double commonDrive, double differentialDrive,
        double railScale) noexcept
    {
        const auto result = ElectryFx::powerTubePairDirect(
            model, commonDrive, differentialDrive, railScale);
        return { result.output, result.supplyDemand,
                 result.screenVoltageOne, result.screenVoltageTwo,
                 result.screenResidual };
    }

    static std::array<double, 2> powerTubePairLookup(
        AmpModel model, float commonDrive, float differentialDrive,
        float railScale) noexcept
    {
        const auto result = ElectryFx::powerTubePairLookup(
            model, commonDrive, differentialDrive, railScale);
        return { result.output, result.supplyDemand };
    }

    static bool pedalAtRest(const ElectryFx& fx) noexcept
    {
        return std::all_of(fx.gain_.begin(), fx.gain_.end(), [] (const auto& channel)
        {
            return ! channel.pedalWasActive
                && channel.diodeVoltage == 0.0
                && channel.diodeDerivative == 0.0
                && channel.pedalHighpass.z1 == 0.0
                && channel.pedalHighpass.z2 == 0.0
                && channel.pedalVoice.z1 == 0.0
                && channel.pedalVoice.z2 == 0.0
                && channel.pedalTilt.z1 == 0.0
                && channel.pedalTilt.z2 == 0.0;
        });
    }

    static bool amplifierAtRest(const ElectryFx::AmpChannel& amplifier) noexcept
    {
        const bool cabinetClear = std::all_of(
            amplifier.cabinet.begin(), amplifier.cabinet.end(),
            [] (const auto& section)
            {
                return section.z1 == 0.0 && section.z2 == 0.0;
            });
        return ! amplifier.wasActive
            && amplifier.bias == 0.0f && amplifier.sag == 0.0f
            && amplifier.interstage.state == 0.0f
            && amplifier.phaseInverterInput.state == 0.0f
            && amplifier.phaseCurrentDelta == 0.0
            && amplifier.couplingHistoryOne == 0.0
            && amplifier.couplingHistoryTwo == 0.0
            && amplifier.powerGridOffsetOne == 0.0
            && amplifier.powerGridOffsetTwo == 0.0
            && amplifier.flux.state == 0.0f
            && amplifier.negativeFeedback.state == 0.0f
            && amplifier.inputHighpass.z1 == 0.0
            && amplifier.inputHighpass.z2 == 0.0
            && amplifier.inputVoice.z1 == 0.0
            && amplifier.inputVoice.z2 == 0.0
            && amplifier.toneStack.z1 == 0.0
            && amplifier.toneStack.z2 == 0.0
            && amplifier.toneStack.z3 == 0.0
            && amplifier.transformerHighpass.z1 == 0.0
            && amplifier.transformerHighpass.z2 == 0.0
            && cabinetClear;
    }

    static bool ampAtRest(const ElectryFx& fx) noexcept
    {
        return std::all_of(fx.gain_.begin(), fx.gain_.end(), [] (const auto& channel)
        {
            const bool ordinaryStateIsClear = ! channel.ampWasActive
                && std::all_of(channel.amplifiers.begin(),
                               channel.amplifiers.end(), [] (const auto& amplifier)
                {
                    return amplifierAtRest(amplifier);
                });
#if ELECTRY_MEASURED_MODERN_CABINET
            return ordinaryStateIsClear
                && (channel.modernCabinet == nullptr
                    || channel.modernCabinet->atRest());
#else
            return ordinaryStateIsClear;
#endif
        });
    }

    static bool ampModelAtRest(const ElectryFx& fx, AmpModel model) noexcept
    {
        const auto index = static_cast<std::size_t>(model);
        return std::all_of(fx.gain_.begin(), fx.gain_.end(), [index] (const auto& channel)
        {
            const bool ordinaryStateIsClear = amplifierAtRest(
                channel.amplifiers[index]);
#if ELECTRY_MEASURED_MODERN_CABINET
            return ordinaryStateIsClear
                && (index != static_cast<std::size_t>(
                        AmpModel::ModernHighGain)
                    || channel.modernCabinet == nullptr
                    || channel.modernCabinet->atRest());
#else
            return ordinaryStateIsClear;
#endif
        });
    }

#if ELECTRY_MEASURED_MODERN_CABINET
    static double cabinetInternalRate(const ElectryFx& fx) noexcept
    {
        return fx.oversampledRate_;
    }

    static int cabinetTapCount(const ElectryFx& fx) noexcept
    {
        return fx.modernCabinetKernel_->tapCount;
    }

    static int cabinetLongPartitionCount(const ElectryFx& fx) noexcept
    {
        return fx.modernCabinetKernel_->longPartitionCount;
    }

    static float cabinetNormalisationGain(const ElectryFx& fx) noexcept
    {
        return fx.modernCabinetKernel_->normalisationGain;
    }

    static double shippingModernCabinetMagnitude(
        const ElectryFx& fx, double frequency) noexcept
    {
        const auto& cabinet = fx.gain_[0].amplifiers[
            static_cast<std::size_t>(AmpModel::ModernHighGain)].cabinet;
        const double omega = 2.0 * 3.14159265358979323846 * frequency
                           / fx.oversampledRate_;
        const std::complex<double> delay = std::polar(1.0, -omega);
        const auto delay2 = delay * delay;
        double magnitude = 1.0;
        for (const auto& section : cabinet)
        {
            const std::complex<double> numerator = section.b0
                + section.b1 * delay + section.b2 * delay2;
            const std::complex<double> denominator = 1.0
                + section.a1 * delay + section.a2 * delay2;
            magnitude *= std::abs(numerator / denominator);
        }
        return magnitude;
    }

    static std::vector<float> cabinetImpulse(const ElectryFx& fx)
    {
        const auto& kernel = *fx.modernCabinetKernel_;
        return { kernel.impulse.begin(),
                 kernel.impulse.begin() + kernel.tapCount };
    }

    static std::vector<float> cabinetConvolve(
        const ElectryFx& fx, const std::vector<float>& input,
        std::size_t outputCount)
    {
        auto convolver = std::make_unique<ElectryFx::CabinetConvolver>();
        std::vector<float> output(outputCount, 0.0f);
        for (std::size_t index = 0; index < outputCount; ++index)
        {
            const float sample = index < input.size() ? input[index] : 0.0f;
            output[index] = convolver->process(
                sample, *fx.modernCabinetKernel_);
        }
        return output;
    }

    static bool cabinetResetClears(const ElectryFx& fx,
                                   int prefixLength) noexcept
    {
        auto convolver = std::make_unique<ElectryFx::CabinetConvolver>();
        for (int index = 0; index < prefixLength; ++index)
        {
            const float input = index == 0 ? 1.0f
                : static_cast<float>((index * 29) % 101 - 50) / 113.0f;
            static_cast<void>(convolver->process(
                input, *fx.modernCabinetKernel_));
        }
        convolver->reset();
        if (! convolver->atRest())
            return false;
        for (int index = 0; index < 1025; ++index)
            if (convolver->process(0.0f, *fx.modernCabinetKernel_) != 0.0f)
                return false;
        convolver->reset();
        return convolver->process(1.0f, *fx.modernCabinetKernel_)
            == fx.modernCabinetKernel_->impulse[0];
    }

    static double cabinetStereoLeak(const ElectryFx& fx,
                                    bool impulseOnLeft) noexcept
    {
        auto left = std::make_unique<ElectryFx::CabinetConvolver>();
        auto right = std::make_unique<ElectryFx::CabinetConvolver>();
        double leak = 0.0;
        const int length = fx.modernCabinetKernel_->tapCount + 512;
        for (int index = 0; index < length; ++index)
        {
            const float impulse = index == 0 ? 1.0f : 0.0f;
            const float leftOutput = left->process(
                impulseOnLeft ? impulse : 0.0f,
                *fx.modernCabinetKernel_);
            const float rightOutput = right->process(
                impulseOnLeft ? 0.0f : impulse,
                *fx.modernCabinetKernel_);
            leak = std::max(leak, std::abs(static_cast<double>(
                impulseOnLeft ? rightOutput : leftOutput)));
        }
        return leak;
    }
#endif

    static std::array<double, 7> toneStackCoefficients(
        const ElectryFx& fx, AmpModel model) noexcept
    {
        const auto& stack = fx.gain_[0].amplifiers[
            static_cast<std::size_t>(model)].toneStack;
        return { stack.b0, stack.b1, stack.b2, stack.b3,
                 stack.a1, stack.a2, stack.a3 };
    }

    static float phaseInverterInputCoefficient(
        const ElectryFx& fx, AmpModel model) noexcept
    {
        return fx.phaseInverterInputCoefficient_[
            static_cast<std::size_t>(model)];
    }

    static std::array<float, 3> ampModelWeights(
        const ElectryFx& fx) noexcept
    {
        return fx.ampModelWeights_;
    }

    static float amplifierSag(const ElectryFx& fx, AmpModel model) noexcept
    {
        return fx.gain_[0].amplifiers[
            static_cast<std::size_t>(model)].sag;
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
using electry::AmpModel;
using electry::EngineParameters;
using electry::FxParameters;
using electry::applyGuitarBuild;
using electry::defaultGuitarBuild;
using electry::PickStyle;
using electry::PickupSelector;
using electry::PlayStyle;
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

#if ELECTRY_MEASURED_MODERN_CABINET
double firMagnitude(const std::vector<float>& impulse,
                    double rate, double frequency)
{
    std::complex<double> response {};
    for (std::size_t index = 0; index < impulse.size(); ++index)
    {
        const double phase = -2.0 * pi * frequency
                           * static_cast<double>(index) / rate;
        response += static_cast<double>(impulse[index])
                  * std::polar(1.0, phase);
    }
    return std::abs(response);
}

#endif

// A sine whose period does not divide the analysis length by a small integer,
// so every folded intermodulation product lands between the harmonics instead
// of hiding on top of one. 431 is prime and the length is a power of two.
constexpr int aliasProbeLength = 16384;
constexpr int aliasProbeCycles = 431;

// Non-harmonic energy in the output of a steady sine, relative to the energy
// that legitimately belongs to the harmonic series. A nonlinearity fed at host
// rate folds its own upper products back into the audio band, and that is
// exactly what this ratio measures.
double aliasFloorDb(float distortion, float amp, double amplitude,
                    AmpModel model = AmpModel::ModernHighGain,
                    double rate = sampleRate,
                    double fixedFrequencyHz = 0.0)
{
    const int cycles = fixedFrequencyHz > 0.0
        ? std::max(1, static_cast<int>(std::lround(
              fixedFrequencyHz * aliasProbeLength / rate)))
        : aliasProbeCycles;
    // Boundary comparisons need equal settling time rather than an equal
    // sample count: otherwise a slow supply follower has only one quarter as
    // long to settle at 192 kHz as it does at 48 kHz.
    const int passes = fixedFrequencyHz > 0.0
        ? std::max(8, static_cast<int>(std::ceil(
              3.0 * rate / aliasProbeLength)))
        : 8;
    ElectryFx fx;
    fx.prepare(rate);
    FxParameters parameters;
    parameters.distortion = distortion;
    parameters.amp = amp;
    parameters.ampModel = model;
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
    for (int pass = 0; pass < passes; ++pass)
    {
        left = sineBlock(aliasProbeLength, cycles, amplitude);
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
        const int nearest = (bin + cycles / 2) / cycles;
        if (nearest >= 1
            && std::abs(bin - nearest * cycles) <= 2)
            harmonic += power;
        else
            alias += power;
    }
    return 10.0 * std::log10(std::max(alias, 1.0e-30)
                             / std::max(harmonic, 1.0e-30));
}

// Small-signal magnitude of either gain path in decibels. At this level the
// nonlinearities stay near their local slope, so ratios between frequencies
// read the physical coupling/voice/cabinet filters rather than clipping.
double gainPathMagnitudeDb(double frequency, float distortion, float amp,
                           AmpModel model = AmpModel::ModernHighGain,
                           double rate = sampleRate,
                           bool startSelected = false)
{
    ElectryFx fx;
    fx.prepare(rate);
    FxParameters parameters;
    parameters.distortion = distortion;
    parameters.amp = amp;
    parameters.ampModel = model;
    fx.setParameters(parameters);
    if (startSelected)
        fx.reset();

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
                               * (pass * blockLength + i) / rate;
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

double ampPathMagnitudeDb(
    double frequency, float amount = 1.0f,
    AmpModel model = AmpModel::ModernHighGain,
    double rate = sampleRate,
    bool startSelected = false)
{
    return gainPathMagnitudeDb(frequency, 0.0f, amount, model, rate,
                               startSelected);
}

// Equal-time, sine-projected small-signal response through the public FX path.
// Unlike the compact fingerprint helper above, this gives a 40 Hz probe the
// same physical settling and measurement windows at every host rate.
double settledAmpPathMagnitude(double frequency, AmpModel model, double rate)
{
    ElectryFx fx;
    fx.prepare(rate);
    FxParameters parameters;
    parameters.amp = 0.01f;
    parameters.ampModel = model;
    fx.setParameters(parameters);
    fx.reset();

    constexpr double amplitude = 0.0008;
    constexpr double settleSeconds = 1.0;
    constexpr double measureSeconds = 0.25;
    constexpr int blockLength = 512;
    const int settleFrames = static_cast<int>(std::lround(
        settleSeconds * rate));
    const int measureFrames = static_cast<int>(std::lround(
        measureSeconds * rate));
    const int totalFrames = settleFrames + measureFrames;
    std::vector<float> left(blockLength);
    std::vector<float> right(blockLength);
    double inPhase = 0.0;
    double quadrature = 0.0;
    for (int offset = 0; offset < totalFrames; offset += blockLength)
    {
        const int count = std::min(blockLength, totalFrames - offset);
        for (int index = 0; index < count; ++index)
        {
            const double phase = 2.0 * pi * frequency
                               * static_cast<double>(offset + index) / rate;
            left[static_cast<std::size_t>(index)] = static_cast<float>(
                amplitude * std::sin(phase));
            right[static_cast<std::size_t>(index)] =
                left[static_cast<std::size_t>(index)];
        }
        fx.process(left.data(), right.data(), count);
        for (int index = 0; index < count; ++index)
        {
            const int frame = offset + index;
            if (frame < settleFrames)
                continue;
            const double phase = 2.0 * pi * frequency
                               * static_cast<double>(frame) / rate;
            const double output = left[static_cast<std::size_t>(index)];
            inPhase += output * std::sin(phase);
            quadrature += output * std::cos(phase);
        }
    }
    return 2.0 * std::hypot(inPhase, quadrature)
         / static_cast<double>(measureFrames);
}

double pedalPathMagnitudeDb(double frequency, float amount = 1.0f)
{
    return gainPathMagnitudeDb(frequency, amount, 0.0f);
}

double toneStackMagnitudeDb(const std::array<double, 7>& coefficients,
                            double frequency, double rate)
{
    const std::complex<double> delay = std::polar(
        1.0, -2.0 * pi * frequency / rate);
    const auto delay2 = delay * delay;
    const auto delay3 = delay2 * delay;
    const std::complex<double> numerator = coefficients[0]
        + coefficients[1] * delay + coefficients[2] * delay2
        + coefficients[3] * delay3;
    const std::complex<double> denominator = 1.0
        + coefficients[4] * delay + coefficients[5] * delay2
        + coefficients[6] * delay3;
    return 20.0 * std::log10(std::max(
        std::abs(numerator / denominator), 1.0e-30));
}

double steadyAmpGainDb(AmpModel model, double amplitude)
{
    ElectryFx fx;
    fx.prepare(sampleRate);
    FxParameters parameters;
    parameters.amp = 0.90f;
    parameters.ampModel = model;
    fx.setParameters(parameters);
    // This fixture measures an already-selected amplifier, not the intentional
    // model-switch crossfade from the default modern state.
    fx.reset();

    constexpr int blockLength = 512;
    constexpr int cycles = 4; // 375 Hz at 48 kHz
    double sum = 0.0;
    int measuredSamples = 0;
    for (int block = 0; block < 96; ++block)
    {
        auto left = sineBlock(blockLength, cycles, amplitude);
        auto right = left;
        fx.process(left.data(), right.data(), blockLength);
        if (block >= 80)
        {
            for (const float value : left)
                sum += static_cast<double>(value) * value;
            measuredSamples += blockLength;
        }
    }
    const double outputRms = std::sqrt(sum / measuredSamples);
    return 20.0 * std::log10(std::max(outputRms, 1.0e-30)
                             / (amplitude / std::sqrt(2.0)));
}

std::vector<float> renderAmpInBlocks(AmpModel model, int blockSize)
{
    constexpr int length = 12288;
    auto source = sineBlock(length, 113, 0.28);
    auto left = source;
    auto right = source;
    ElectryFx fx;
    fx.prepare(sampleRate);
    FxParameters parameters;
    parameters.amp = 0.82f;
    parameters.ampModel = model;
    fx.setParameters(parameters);
    fx.reset();
    for (int offset = 0; offset < length; offset += blockSize)
    {
        const int count = std::min(blockSize, length - offset);
        fx.process(left.data() + offset, right.data() + offset, count);
    }
    return left;
}

std::vector<float> renderModelSwitchesInBlocks(int blockSize)
{
    constexpr int segmentLength = 4096;
    constexpr int length = 3 * segmentLength;
    auto source = sineBlock(length, 79, 0.30);
    auto left = source;
    auto right = source;
    ElectryFx fx;
    fx.prepare(sampleRate);
    FxParameters parameters;
    parameters.amp = 0.90f;
    parameters.ampModel = AmpModel::AmericanClean;
    fx.setParameters(parameters);
    fx.reset();

    const std::array<AmpModel, 3> sequence {
        AmpModel::AmericanClean, AmpModel::BritishCrunch,
        AmpModel::ModernHighGain };
    for (int segment = 0; segment < 3; ++segment)
    {
        parameters.ampModel = sequence[static_cast<std::size_t>(segment)];
        fx.setParameters(parameters);
        const int start = segment * segmentLength;
        const int end = start + segmentLength;
        for (int offset = start; offset < end; offset += blockSize)
        {
            const int count = std::min(blockSize, end - offset);
            fx.process(left.data() + offset, right.data() + offset, count);
        }
    }
    return left;
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

std::vector<float> throughSelectedChain(const FxParameters& parameters,
                                        const std::vector<float>& source,
                                        double rate = sampleRate)
{
    ElectryFx fx;
    fx.prepare(rate);
    fx.setParameters(parameters);
    fx.reset();
    std::vector<float> left = source;
    std::vector<float> right = source;
    fx.process(left.data(), right.data(), static_cast<int>(left.size()));
    return left;
}

// Model-only guard for the 30-80 ms body of the shipping rapid-Palm demo.
// The real reference take is useful evidence that this region needs more upper
// body and less periodic ring, but its amp, cabinet and mastering are unknown,
// so it is deliberately not treated as a numeric target here.
void testRapidPalmBodyDirection()
{
    constexpr double rate = 44100.0;
    constexpr int blockSize = 256;
    constexpr int hitCount = 12;

    EngineParameters engineParameters;
    engineParameters.pickupSelector = PickupSelector::Bridge;
    engineParameters.pickupType = 0.32f;
    engineParameters.toneKnob = 1.0f;
    applyGuitarBuild(engineParameters, defaultGuitarBuild);
    engineParameters.stringAge = 0.10f;
    engineParameters.pickHardness = 0.85f;
    engineParameters.pickPosition = 0.18f;
    engineParameters.velocityAmount = 0.7f;
    engineParameters.sympatheticAmount = 0.25f;
    engineParameters.muteDamping = 0.85f;
    engineParameters.outputGain = 2.0f;
    engineParameters.fingerNoise = 0.55f;
    engineParameters.artifactAmount = 0.15f;

    FxParameters fxParameters;
    fxParameters.distortion = 0.45f;
    fxParameters.amp = 0.95f;
    fxParameters.compressor = 0.60f;

    ElectryEngine engine;
    engine.prepare(rate, blockSize);
    engine.setParameters(engineParameters);
    engine.reset();
    ElectryFx fx;
    fx.prepare(rate);
    fx.setParameters(fxParameters);
    engine.setAcousticReturnLevel(std::min(
        1.0f, fxParameters.amp + 0.6f * fxParameters.distortion));
    fx.reset();

    std::vector<float> left;
    std::vector<float> right;
    const auto render = [&] (int sampleCount)
    {
        while (sampleCount > 0)
        {
            const int samples = std::min(blockSize, sampleCount);
            const auto offset = left.size();
            left.resize(offset + static_cast<std::size_t>(samples));
            right.resize(offset + static_cast<std::size_t>(samples));
            engine.process(left.data() + offset, right.data() + offset, samples);
            fx.process(left.data() + offset, right.data() + offset, samples);
            engine.pushAcousticReturn(left.data() + offset,
                                      right.data() + offset, samples);
            sampleCount -= samples;
        }
    };

    // Take's constructor lead-in, then renderMuteAndDeadMetal's own lead-in.
    render(static_cast<int>(0.25 * rate));
    render(static_cast<int>(0.25 * rate));
    engine.noteOn(ElectryEngine::firstKeyswitchNote
                      + static_cast<int>(PickStyle::Alternate),
                  1.0f);
    engine.noteOn(ElectryEngine::firstPlayStyleKeyswitchNote
                      + static_cast<int>(PlayStyle::PalmMute),
                  1.0f);

    std::array<std::size_t, hitCount> hitStarts {};
    const int holdSamples = static_cast<int>(0.055 * rate);
    const int gapSamples = static_cast<int>(0.028333 * rate);
    for (int hit = 0; hit < hitCount; ++hit)
    {
        hitStarts[static_cast<std::size_t>(hit)] = left.size();
        engine.noteOn(28, hit % 2 == 0 ? 0.95f : 0.82f);
        render(holdSamples);
        engine.noteOff(28);
        render(gapSamples);
    }
    expect(holdSamples + gapSamples == 3674,
           "the rapid-Palm fixture no longer has the demo's 83.31 ms cadence");

    constexpr int fftSize = 4096;
    const int windowStart = static_cast<int>(0.030 * rate);
    const int windowEnd = static_cast<int>(0.080 * rate);
    const int windowLength = windowEnd - windowStart;
    std::array<double, hitCount> upperBody {};
    std::array<double, hitCount> harmonicity {};
    for (int hit = 0; hit < hitCount; ++hit)
    {
        std::vector<double> window(static_cast<std::size_t>(windowLength));
        const auto first = hitStarts[static_cast<std::size_t>(hit)]
                         + static_cast<std::size_t>(windowStart);
        double mean = 0.0;
        for (int i = 0; i < windowLength; ++i)
            mean += left[first + static_cast<std::size_t>(i)];
        mean /= windowLength;
        for (int i = 0; i < windowLength; ++i)
            window[static_cast<std::size_t>(i)] =
                left[first + static_cast<std::size_t>(i)] - mean;

        std::vector<std::complex<double>> spectrum(
            static_cast<std::size_t>(fftSize));
        for (int i = 0; i < windowLength; ++i)
        {
            const double hann = 0.5 - 0.5 * std::cos(
                2.0 * pi * i / static_cast<double>(windowLength - 1));
            spectrum[static_cast<std::size_t>(i)] =
                { window[static_cast<std::size_t>(i)] * hann, 0.0 };
        }
        fft(spectrum);
        double upperPower = 0.0;
        double audiblePower = 0.0;
        for (int bin = 1; bin <= fftSize / 2; ++bin)
        {
            const double frequency = bin * rate / fftSize;
            if (frequency >= 20.0 && frequency <= 8000.0)
            {
                const double power = std::norm(
                    spectrum[static_cast<std::size_t>(bin)]);
                audiblePower += power;
                if (frequency > 500.0)
                    upperPower += power;
            }
        }
        upperBody[static_cast<std::size_t>(hit)] = upperPower
            / std::max(audiblePower, 1.0e-30);

        // Autocorrelation uses the mean-removed body itself. The Hann belongs
        // only to spectral power; applying it here would compare two different
        // window gains one period apart and falsely lower harmonicity.
        double strongestPeriod = -1.0;
        const int firstLag = static_cast<int>(std::ceil(rate / 48.0));
        const int lastLag = static_cast<int>(std::floor(rate / 36.0));
        for (int lag = firstLag; lag <= lastLag; ++lag)
        {
            double product = 0.0;
            double earlyPower = 0.0;
            double latePower = 0.0;
            for (int i = 0; i + lag < windowLength; ++i)
            {
                const double early = window[static_cast<std::size_t>(i)];
                const double late = window[static_cast<std::size_t>(i + lag)];
                product += early * late;
                earlyPower += early * early;
                latePower += late * late;
            }
            strongestPeriod = std::max(
                strongestPeriod,
                product / std::sqrt(std::max(earlyPower * latePower, 1.0e-30)));
        }
        harmonicity[static_cast<std::size_t>(hit)] = strongestPeriod;
    }

    const auto median = [] (auto values)
    {
        std::sort(values.begin(), values.end());
        return 0.5 * (values[values.size() / 2 - 1]
                    + values[values.size() / 2]);
    };
    const double medianUpperBody = median(upperBody);
    const double medianHarmonicity = median(harmonicity);
    const auto previousPrecision = std::cout.precision(9);
    std::cout << "Rapid Palm 30-80 ms upper-body fraction (>500-8k / 20-8k): "
              << medianUpperBody << " ("
              << 10.0 * std::log10(std::max(medianUpperBody, 1.0e-30))
              << " dB), harmonicity: " << medianHarmonicity << '\n';
    std::cout.precision(previousPrecision);

    // Loose one-sided rails: only a material regression toward a darker or
    // more periodic chug should fail. They are not fits to the confounded real
    // reference and intentionally leave room for evidence-led model work.
#if ! ELECTRY_MEASURED_MODERN_CABINET
    expect(medianUpperBody > 0.06,
           "the rapid Palm body became materially darker");
#endif
    expect(medianHarmonicity < 0.97,
           "the rapid Palm body became materially more periodic");
}

// ---------------------------------------------------------------------------

void testHalfbandKernel()
{
    const auto stage = FxAccess::designedHalfband();

    expect(std::abs(FxAccess::halfbandDcGain(stage) - 1.0) < 1.0e-6,
           "the halfband kernel does not have unity DC gain");

    // A halfband is exactly -6 dB at a quarter of its own rate, which is the
    // lower rate's Nyquist; that symmetry is what makes the interpolated and
    // decimated paths complementary.
    const double half = FxAccess::halfbandMagnitude(stage, 0.25);
    expect(std::abs(20.0 * std::log10(half) + 6.0206) < 0.05,
           "the halfband kernel is not -6 dB at a quarter of its rate");

    double worstPassband = 0.0;
    for (int point = 0; point <= 1500; ++point)
    {
        const double frequency = 0.15 * static_cast<double>(point) / 1500.0;
        const double magnitudeDb = 20.0 * std::log10(std::max(
            FxAccess::halfbandMagnitude(stage, frequency), 1.0e-12));
        worstPassband = std::max(worstPassband, std::abs(magnitudeDb));
    }
    expect(worstPassband < 0.01,
           "halfband passband ripple exceeds 0.01 dB ("
               + std::to_string(worstPassband) + " dB)");

    double worstStopband = -300.0;
    for (int point = 0; point <= 1500; ++point)
    {
        const double frequency = 0.35
            + 0.15 * static_cast<double>(point) / 1500.0;
        worstStopband = std::max(worstStopband, 20.0 * std::log10(std::max(
            FxAccess::halfbandMagnitude(stage, frequency), 1.0e-12)));
    }
    std::cout << "Halfband worst passband deviation through 0.15 fs: "
              << worstPassband << " dB\n";
    std::cout << "Halfband stopband rejection from 0.35 fs: "
              << -worstStopband << " dB\n";
    expect(worstStopband < -70.0,
           "halfband stopband rejection is worse than 70 dB");
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

void testCircuitGainStages()
{
    // The diode pair's three DC points come from the independent Shockley KCL
    // equation (Vi - Vo) / 2.2k = 2 Is sinh(Vo / nVt), not from this solver.
    const auto settledDiode = [] (double inputVolts)
    {
        double voltage = 0.0;
        double derivative = 0.0;
        for (int sample = 0; sample < 4096; ++sample)
            FxAccess::diodeStep(inputVolts, 192000.0, voltage, derivative);
        return voltage;
    };
    expect(std::abs(settledDiode(0.1) - 0.099950009) < 2.0e-6,
           "the diode clipper misses its 0.1 V Shockley operating point");
    expect(std::abs(settledDiode(1.0) - 0.514412245) < 2.0e-6,
           "the diode clipper misses its 1 V Shockley operating point");
    expect(std::abs(settledDiode(2.0) - 0.563440008) < 2.0e-6,
           "the diode clipper misses its 2 V Shockley operating point");
    expect(std::abs(settledDiode(1.0) + settledDiode(-1.0)) < 2.0e-7,
           "the antiparallel diode pair is not symmetric");

    double diodeVoltage = 0.0;
    double diodeDerivative = 0.0;
    const double first = FxAccess::diodeStep(
        0.25, 192000.0, diodeVoltage, diodeDerivative);
    const double released = FxAccess::diodeStep(
        0.0, 192000.0, diodeVoltage, diodeDerivative);
    expect(first > 0.0 && first < 0.25,
           "the diode node does not charge through its RC network");
    expect(released > 0.0 && released < 0.25,
           "the diode node has no capacitor memory after the input releases");

    // Independently derived coupled quiescent solution for 250 V / 100 kOhm
    // plate loading and 1 kOhm cathode self-bias. Both KCL equations must hold;
    // checking only the plate solve could conceal a mismatched cathode anchor.
    constexpr double quiescentPlate = 152.15373624396778;
    constexpr double quiescentCathode = 0.978501831350406;
    const double cathode = FxAccess::cathodeCurrent(
        quiescentPlate, -quiescentCathode);
    const double plate = FxAccess::plateCurrent(
        quiescentPlate, -quiescentCathode);
    expect(std::abs(quiescentCathode - 1000.0 * cathode) < 1.0e-9,
           "the measured triode model misses cathode-bias KCL");
    expect(std::abs(quiescentPlate + 100000.0 * plate - 250.0) < 1.0e-8,
           "the measured triode model misses plate-load KCL");

    double arbitraryWarmStart = 10.0;
    const double solvedPlate = FxAccess::solvePlate(
        -quiescentCathode, 250.0, arbitraryWarmStart);
    expect(std::abs(solvedPlate - quiescentPlate) < 1.0e-8,
           "the plate-load Newton solve does not reach the DC operating point");

    // Cross-check the warm-started Newton solver against an independent
    // fixed-iteration bisection of the published current equations across the
    // useful grid range. +2 V is already beyond the load line (the tube asks
    // for more than 2.5 mA even at Vp=0), so both solvers must select the
    // physical 0 V rail there; interior points must satisfy KCL themselves.
    const auto referenceSoftplus = [] (double value)
    {
        return std::max(value, 0.0)
             + std::log1p(std::exp(-std::abs(value)));
    };
    const auto referenceGridCurrent = [&] (double gridToCathode)
    {
        const double conduction = referenceSoftplus(11.99 * gridToCathode)
                                / 11.99;
        return 3.263e-4 * std::pow(conduction, 1.156) + 3.917e-8;
    };
    const auto referencePlateCurrent = [&] (double plateVoltage,
                                             double gridToCathode)
    {
        const double drive = plateVoltage / 86.9 + gridToCathode;
        const double conduction = referenceSoftplus(4.56 * drive) / 4.56;
        const double cathodeCurrent = 1.371e-3
            * std::pow(conduction, 1.349);
        return cathodeCurrent - referenceGridCurrent(gridToCathode);
    };
    const auto referencePlate = [&] (double gridToCathode)
    {
        const auto residual = [&] (double plateVoltage)
        {
            return referencePlateCurrent(plateVoltage, gridToCathode)
                 - (250.0 - plateVoltage) / 100000.0;
        };
        double lower = 0.0;
        double upper = 250.0;
        if (residual(lower) >= 0.0)
            return lower;
        if (residual(upper) <= 0.0)
            return upper;
        for (int iteration = 0; iteration < 64; ++iteration)
        {
            const double middle = 0.5 * (lower + upper);
            if (residual(middle) > 0.0)
                upper = middle;
            else
                lower = middle;
        }
        return 0.5 * (lower + upper);
    };

    double previousPlate = 251.0;
    for (const double gridToCathode
         : { -3.0, -2.0, -1.0, 0.0, 1.0, 1.5, 2.0 })
    {
        double warmStart = gridToCathode < 0.0 ? 5.0 : 245.0;
        const double solved = FxAccess::solvePlate(
            gridToCathode, 250.0, warmStart);
        const double reference = referencePlate(gridToCathode);
        expect(std::abs(solved - reference) < 1.0e-3,
               "the triode Newton solve disagrees with independent bisection at Vgk="
                   + std::to_string(gridToCathode));
        expect(solved < previousPlate,
               "triode plate voltage does not fall strictly as grid voltage rises");
        previousPlate = solved;

        const double currentResidual = referencePlateCurrent(
            solved, gridToCathode) - (250.0 - solved) / 100000.0;
        if (solved > 0.0 && solved < 250.0)
            expect(std::abs(currentResidual) < 1.0e-8,
                   "the triode plate solve misses load-line KCL at Vgk="
                       + std::to_string(gridToCathode));
        else
            expect(gridToCathode == 2.0 && solved == 0.0
                       && currentResidual > 0.0,
                   "the triode solver selected the wrong physical plate rail");
    }

    // A clipped stage can move directly from the conducting 0 V rail to
    // cutoff. That hostile warm start used to exhaust the safeguarded steps
    // and return a point almost four volts away from the load-line root.
    double railWarmStart = 0.0;
    constexpr double cutoffJump = -4.2448;
    const double jumpedPlate = FxAccess::solvePlate(
        cutoffJump, 250.0, railWarmStart);
    const double jumpedReference = referencePlate(cutoffJump);
    const double jumpedResidual = referencePlateCurrent(
        jumpedPlate, cutoffJump) - (250.0 - jumpedPlate) / 100000.0;
    expect(std::abs(jumpedPlate - jumpedReference) < 1.0e-3,
           "the plate solver did not recover from a rail-to-cutoff jump");
    expect(std::abs(jumpedResidual) < 1.0e-8,
           "the rail-to-cutoff fallback returned without satisfying KCL");

    // Runtime uses a dense interpolation of this fixed, memoryless load-line
    // solve. Probe between table knots across the full clamped grid range so
    // the speedup cannot quietly replace the circuit curve with a coarse
    // waveshaper.
    double lookupWarmStart = 250.0;
    double maximumLookupError = 0.0;
    double previousLookup = -std::numeric_limits<double>::infinity();
    bool lookupIsMonotonic = true;
    for (double gridVoltage = -19.0; gridVoltage <= 20.9;
         gridVoltage += 0.0317)
    {
        const double solved = FxAccess::triodeOutput(
            gridVoltage, lookupWarmStart);
        const double lookedUp = FxAccess::triodeLookup(gridVoltage);
        maximumLookupError = std::max(
            maximumLookupError, std::abs(solved - lookedUp));
        lookupIsMonotonic = lookupIsMonotonic
                         && lookedUp >= previousLookup;
        previousLookup = lookedUp;
    }
    std::cout << "Maximum circuit-table transfer error: "
              << maximumLookupError << '\n';
    expect(maximumLookupError < 2.0e-4,
           "the runtime triode table is too coarse ("
               + std::to_string(maximumLookupError) + ")");
    expect(lookupIsMonotonic,
           "the runtime triode table folded back on its load-line curve");

    expect(FxAccess::gridCurrent(1.0) > FxAccess::gridCurrent(-1.0),
           "the measured triode grid current does not rise under positive drive");

    double positivePlate = quiescentPlate;
    double negativePlate = quiescentPlate;
    const double smallPositive = FxAccess::triodeOutput(1.0e-4, positivePlate);
    const double smallNegative = FxAccess::triodeOutput(-1.0e-4, negativePlate);
    const double smallSignalSlope = (smallPositive - smallNegative) / 2.0e-4;
    expect(std::abs(smallSignalSlope - 1.0) < 2.0e-4,
           "the triode transfer is not unity-normalised at quiescence");

    positivePlate = quiescentPlate;
    negativePlate = quiescentPlate;
    const double loudPositive = FxAccess::triodeOutput(2.0, positivePlate);
    const double loudNegative = FxAccess::triodeOutput(-2.0, negativePlate);
    expect(loudPositive > -loudNegative + 0.20,
           "the measured triode transfer lost its plate-load asymmetry");
}

void testPhaseInverterCircuits()
{
    // Independent evaluations of TubeLib's ECC81/ECC83 TriodeK macros. The
    // Vgk=0 anchors exercise the strict no-grid-current boundary as well as
    // the ordinary negative-grid operating region.
    expect(std::abs(FxAccess::phaseInverterPlateCurrent(
                        AmpModel::AmericanClean, 150.0, -2.0)
                    - 0.002302091834234943) < 1.0e-14,
           "the ECC81 phase-inverter current no longer matches TubeLib");
    expect(std::abs(FxAccess::phaseInverterPlateCurrent(
                        AmpModel::AmericanClean, 200.0, 0.0)
                    - 0.015442047889304224) < 1.0e-14,
           "the ECC81 zero-grid current no longer matches TubeLib");
    expect(std::abs(FxAccess::phaseInverterPlateCurrent(
                        AmpModel::BritishCrunch, 200.0, -1.5)
                    - 0.0014299974745466363) < 1.0e-14,
           "the ECC83 phase-inverter current no longer matches TubeLib");
    expect(std::abs(FxAccess::phaseInverterPlateCurrent(
                        AmpModel::BritishCrunch, 200.0, 0.0)
                    - 0.0046774689092718754) < 1.0e-14,
           "the ECC83 zero-grid current no longer matches TubeLib");
    expect(FxAccess::phaseInverterPlateCurrent(
               AmpModel::AmericanClean, 200.0, 1.0)
               == FxAccess::phaseInverterPlateCurrent(
                   AmpModel::AmericanClean, 200.0, 0.0),
           "the phase-inverter plate model crossed its grid-current boundary");

    struct Expected
    {
        AmpModel model;
        double supply;
        double plateResistanceOne;
        double plateResistanceTwo;
        double tailResistance;
        double gridBias;
        double gridStopperResistance;
        double inputVoltsPerDrive;
        std::array<double, 6> idle;
        double negativeOne;
        double positiveOne;
    };
    const std::array<Expected, 2> expected {{
        { AmpModel::AmericanClean, 410.0, 82000.0, 100000.0, 22100.0,
          -37.0, 1500.0, 1.994433668044982,
          { 0.0, 232.249049214088, 225.525768684732,
            90.5606993644173, 88.6748540519992,
            0.00401243683493209 },
          -0.991960053849012, 0.984272776173345 },
        { AmpModel::BritishCrunch, 400.0, 82000.0, 100000.0, 14700.0,
          -36.0, 5600.0, 1.258852687134334,
          { 0.0, 261.464563110962, 250.946889788538,
            48.2404126435508, 46.7458184482661,
            0.00317998764954191 },
          -0.983753983104880, 0.978559763641025 },
    }};

    // The two output coupling capacitors are trapezoidally integrated once
    // per nonlinear frame, so their companion conductance must use that exact
    // frame clock. Compare the same analogue small-signal response at the two
    // internal rates selected by the 48 kHz and 88.2 kHz host families. A
    // host-rate clamp inside phaseInverterCoupledStep() makes the 705.6 kHz
    // path advance 384 kHz coefficients too often and moves this response.
    const auto coupledMagnitude = [] (double frequency, double rate)
    {
        constexpr double amplitude = 0.01;
        constexpr double settleSeconds = 0.25;
        constexpr double measureSeconds = 0.25;
        const int settleFrames = static_cast<int>(std::lround(
            settleSeconds * rate));
        const int measureFrames = static_cast<int>(std::lround(
            measureSeconds * rate));
        const int totalFrames = settleFrames + measureFrames;
        std::array<double, 5> state {};
        double inPhase = 0.0;
        double quadrature = 0.0;
        for (int frame = 0; frame < totalFrames; ++frame)
        {
            const double phase = 2.0 * pi * frequency
                               * static_cast<double>(frame) / rate;
            const double sine = std::sin(phase);
            const auto point = FxAccess::phaseInverterCoupledStep(
                AmpModel::BritishCrunch, amplitude * sine, rate, state);
            if (frame < settleFrames)
                continue;
            const double differential = point[1] - point[0];
            inPhase += differential * sine;
            quadrature += differential * std::cos(phase);
        }
        return 2.0 * std::hypot(inPhase, quadrature)
             / static_cast<double>(measureFrames);
    };
    const double at384 = 20.0 * std::log10(
        coupledMagnitude(40.0, 384000.0)
        / coupledMagnitude(1000.0, 384000.0));
    const double at705k6 = 20.0 * std::log10(
        coupledMagnitude(40.0, 705600.0)
        / coupledMagnitude(1000.0, 705600.0));
    std::cout << "British coupled-PI 40 Hz/1 kHz at 384/705.6 kHz: "
              << at384 << "/" << at705k6 << " dB\n";
    expect(std::abs(at384 - at705k6) < 0.05,
           "the phase-inverter capacitor response moved with its internal "
           "frame rate");

    std::array<double, 2> recoveryAfterOneMillisecond {};
    std::array<double, 2> recoveryAfterTwentyMilliseconds {};
    for (const auto& circuit : expected)
    {
        const auto idle = FxAccess::phaseInverterDirect(circuit.model, 0.0);
        for (std::size_t index = 0; index < idle.size(); ++index)
            expect(std::abs(idle[index] - circuit.idle[index]) < 2.0e-7,
                   "a phase-inverter DC operating point changed");

        const double currentOne = FxAccess::phaseInverterPlateCurrent(
            circuit.model, idle[1] - idle[3], idle[4] - idle[3]);
        const double currentTwo = FxAccess::phaseInverterPlateCurrent(
            circuit.model, idle[2] - idle[3], idle[4] - idle[3]);
        expect(std::abs((circuit.supply - idle[1])
                            / circuit.plateResistanceOne - currentOne)
                       < 2.0e-12
                   && std::abs((circuit.supply - idle[2])
                                   / circuit.plateResistanceTwo - currentTwo)
                       < 2.0e-12
                   && std::abs(currentOne + currentTwo - idle[5]) < 2.0e-12,
               "the phase-inverter idle point does not satisfy KCL");

        const auto negative = FxAccess::phaseInverterDirect(
            circuit.model, -1.0);
        const auto positive = FxAccess::phaseInverterDirect(
            circuit.model, 1.0);
        expect(std::abs(negative[0] - circuit.negativeOne) < 2.0e-7
                   && std::abs(positive[0] - circuit.positiveOne) < 2.0e-7,
               "a loaded phase-inverter transfer anchor changed");
        const auto smallNegative = FxAccess::phaseInverterDirect(
            circuit.model, -1.0e-4);
        const auto smallPositive = FxAccess::phaseInverterDirect(
            circuit.model, 1.0e-4);
        const double localSlope =
            (smallPositive[0] - smallNegative[0]) / 2.0e-4;
        expect(std::abs(localSlope - 1.0) < 2.0e-6,
               "a phase inverter lost its unity small-signal calibration");

        const auto expectLoadedKcl = [&] (
            const std::array<double, 6>& driven, double drive)
        {
            const double currentDriven = FxAccess::phaseInverterPlateCurrent(
                circuit.model, driven[1] - driven[3],
                idle[4] + drive * circuit.inputVoltsPerDrive - driven[3]);
            const double currentReference = FxAccess::phaseInverterPlateCurrent(
                circuit.model, driven[2] - driven[3],
                idle[4] - driven[3]);
            const double residualOne = (circuit.supply - driven[1])
                    / circuit.plateResistanceOne
                + (idle[1] - driven[1]) / 220000.0 - currentDriven;
            const double residualTwo = (circuit.supply - driven[2])
                    / circuit.plateResistanceTwo
                + (idle[2] - driven[2]) / 220000.0 - currentReference;
            expect(std::abs(residualOne) < 2.0e-10
                       && std::abs(residualTwo) < 2.0e-10
                       && std::abs(currentDriven + currentReference
                                       - driven[5]) < 2.0e-10,
                   "a loaded phase-inverter point does not satisfy KCL");
        };
        expectLoadedKcl(negative, -1.0);
        expectLoadedKcl(positive, 1.0);

        // This catches the tempting but wrong formulation that lets both
        // grids follow the instantaneous tail. With their coupling/bypass
        // capacitors holding the idle reference, total current barely moves
        // and the differential projection discards less than 4% common mode.
        for (const auto& driven : { negative, positive })
        {
            const double common = ((driven[1] - idle[1])
                                   + (driven[2] - idle[2]))
                                / (2.0 * std::abs(circuit.gridBias));
            expect(std::abs(common / driven[0]) < 0.04,
                   "the LTP lost its finite-tail common-mode rejection");
            expect(std::abs(driven[5] / idle[5] - 1.0) < 0.04,
                   "the LTP total current moves like a live-grid tail model");
        }

        std::array<double, 5> idleState {};
        const auto coupledIdle = FxAccess::phaseInverterCoupledStep(
            circuit.model, 0.0, 192000.0, idleState);
        expect(std::abs(coupledIdle[0] - circuit.gridBias) < 1.0e-12
                   && std::abs(coupledIdle[1] - circuit.gridBias) < 1.0e-12
                   && std::abs(coupledIdle[2] - idle[1]) < 2.0e-8
                   && std::abs(coupledIdle[3] - idle[2]) < 2.0e-8
                   && std::abs(coupledIdle[4]) < 1.0e-12
                   && std::abs(coupledIdle[5]) < 1.0e-12
                   && std::abs(coupledIdle[6]) < 1.0e-12
                   && std::abs(coupledIdle[7]) < 1.0e-12
                   && coupledIdle[9] < 1.0e-10,
               "the coupled phase inverter is not exactly at DC rest");

        std::array<double, 5> burstState {};
        double maximumResidual = 0.0;
        double maximumGridCurrent = 0.0;
        double minimumCommonOffset = 0.0;
        double maximumCommonOffset = 0.0;
        double maximumDifferential = 0.0;
        double maximumBranchResidual = 0.0;
        for (int frame = 0; frame < 3840; ++frame)
        {
            const double burst = 4.0 * std::sin(
                2.0 * pi * 1000.0 * frame / 192000.0);
            const auto point = FxAccess::phaseInverterCoupledStep(
                circuit.model, burst, 192000.0, burstState);
            maximumResidual = std::max(maximumResidual, point[9]);
            maximumGridCurrent = std::max(
                maximumGridCurrent, std::max(point[6], point[7]));
            minimumCommonOffset = std::min(
                minimumCommonOffset,
                0.5 * (point[0] + point[1]) - circuit.gridBias);
            maximumCommonOffset = std::max(
                maximumCommonOffset,
                0.5 * (point[0] + point[1]) - circuit.gridBias);
            maximumDifferential = std::max(
                maximumDifferential,
                std::abs(point[1] - point[0])
                    / (2.0 * std::abs(circuit.gridBias)));
            const double junctionOne = point[0]
                + circuit.gridStopperResistance * point[6];
            const double junctionTwo = point[1]
                + circuit.gridStopperResistance * point[7];
            maximumBranchResidual = std::max({
                maximumBranchResidual,
                std::abs(point[4]
                    - ((junctionOne - circuit.gridBias) / 220000.0
                       + point[6])),
                std::abs(point[5]
                    - ((junctionTwo - circuit.gridBias) / 220000.0
                       + point[7])) });
        }
        std::array<double, 10> recovery {};
        for (int frame = 0; frame < 192; ++frame)
            recovery = FxAccess::phaseInverterCoupledStep(
                circuit.model, 0.0, 192000.0, burstState);
        const double commonAfterOneMillisecond =
            0.5 * (recovery[0] + recovery[1]) - circuit.gridBias;
        for (int frame = 192; frame < 3840; ++frame)
            recovery = FxAccess::phaseInverterCoupledStep(
                circuit.model, 0.0, 192000.0, burstState);
        const double commonAfterTwentyMilliseconds =
            0.5 * (recovery[0] + recovery[1]) - circuit.gridBias;
        const auto modelIndex = static_cast<std::size_t>(circuit.model);
        recoveryAfterOneMillisecond[modelIndex] =
            commonAfterOneMillisecond;
        recoveryAfterTwentyMilliseconds[modelIndex] =
            commonAfterTwentyMilliseconds;
        std::cout << (circuit.model == AmpModel::AmericanClean
                          ? "American" : "British")
                  << " PI grid current/residual/common 1/20 ms/diff: "
                  << maximumGridCurrent << "/" << maximumResidual << "/"
                  << commonAfterOneMillisecond << "/"
                  << commonAfterTwentyMilliseconds << "/"
                  << maximumDifferential << '\n';
        expect(maximumGridCurrent > 1.0e-5
                   && maximumResidual < 1.0e-7
                   && minimumCommonOffset < -0.1
                   && commonAfterOneMillisecond < -0.1
                   && std::abs(commonAfterTwentyMilliseconds)
                        < std::abs(commonAfterOneMillisecond)
                   && minimumCommonOffset
                        > -2.0 * std::abs(circuit.gridBias)
                   && maximumCommonOffset
                        < 0.5 * std::abs(circuit.gridBias)
                   && maximumBranchResidual < 1.0e-12
                   && maximumDifferential < 4.0,
               "the coupled phase inverter lost grid clamp or recovery");

        std::array<double, 5> edgeState {};
        const auto edge = FxAccess::phaseInverterCoupledStep(
            circuit.model, 4.0, 192000.0, edgeState);
        std::array<double, 5> outsideState {};
        const auto outside = FxAccess::phaseInverterCoupledStep(
            circuit.model, 40.0, 192000.0, outsideState);
        std::array<double, 5> nanState {};
        const auto nonFinite = FxAccess::phaseInverterCoupledStep(
            circuit.model, std::numeric_limits<double>::quiet_NaN(),
            192000.0, nanState);
        expect(edge == outside && std::isfinite(nonFinite[0])
                   && FxAccess::phaseInverterDirect(
                          circuit.model,
                          std::numeric_limits<double>::quiet_NaN())[0]
                      == FxAccess::phaseInverterDirect(
                          circuit.model, 0.0)[0],
               "a non-finite or out-of-range phase drive escaped sanitisation");

        std::array<double, 5> hostileState {};
        for (int frame = 0; frame < 1536; ++frame)
        {
            const double square = (frame / 96) % 2 == 0 ? -4.0 : 4.0;
            const auto point = FxAccess::phaseInverterCoupledStep(
                circuit.model, square, 192000.0, hostileState);
            expect(std::all_of(point.begin(), point.end(), [] (double value)
                   {
                       return std::isfinite(value);
                   }),
                   "a hostile phase-inverter transition became non-finite");
        }
    }
    expect(std::abs(recoveryAfterTwentyMilliseconds[0])
               > 4.0 * std::abs(recoveryAfterTwentyMilliseconds[1])
               && std::abs(recoveryAfterOneMillisecond[0]) > 1.0
               && std::abs(recoveryAfterOneMillisecond[1]) > 1.0,
           "the 100 nF American and 22 nF British blocking recoveries collapsed");

    const std::array<std::array<double, 2>, 4> gridAnchors {{
        { 0.5, 0.0001010041478199964 },
        { 1.0, 0.0003353747286139267 },
        { 2.0, 0.0008235187646659394 },
        { 5.0, 0.0023094468173863484 }
    }};
    double maximumGridLookupError = 0.0;
    for (const auto& anchor : gridAnchors)
    {
        const double direct = FxAccess::powerGridCurrentDirect(anchor[0]);
        const double lookup = FxAccess::powerGridCurrentLookup(anchor[0]);
        expect(std::abs(direct - anchor[1]) < 1.0e-14,
               "a TubeLib power-grid current anchor changed");
        maximumGridLookupError = std::max(
            maximumGridLookupError, std::abs(direct - lookup));
    }
    double previousGridLookup = 0.0;
    bool gridLookupMonotonic = true;
    for (int probe = 0; probe < 1000; ++probe)
    {
        const double voltage = 99.9 * (probe + 0.31) / 1000.0;
        const double lookup = FxAccess::powerGridCurrentLookup(voltage);
        gridLookupMonotonic = gridLookupMonotonic
            && lookup >= previousGridLookup;
        previousGridLookup = lookup;
        maximumGridLookupError = std::max(
            maximumGridLookupError,
            std::abs(FxAccess::powerGridCurrentDirect(voltage)
                     - lookup));
    }
    expect(FxAccess::powerGridCurrentDirect(-1.0) == 0.0
               && FxAccess::powerGridCurrentLookup(-1.0) == 0.0
               && FxAccess::powerGridCurrentLookup(
                    std::numeric_limits<double>::quiet_NaN()) == 0.0
               && maximumGridLookupError < 2.0e-7
               && gridLookupMonotonic,
           "the TubeLib power-grid current lookup is inaccurate or unsafe");

    ElectryFx fx;
    fx.prepare(sampleRate); // 8x, so the circuit runs at 384 kHz
    const float americanCoefficient = FxAccess::phaseInverterInputCoefficient(
        fx, AmpModel::AmericanClean);
    const float britishCoefficient = FxAccess::phaseInverterInputCoefficient(
        fx, AmpModel::BritishCrunch);
    expect(std::abs(americanCoefficient
                    - std::exp(-1.0 / (1.0e-9 * 1.0e6 * 384000.0)))
               < 1.0e-7
               && std::abs(britishCoefficient
                    - std::exp(-1.0 / (22.0e-9 * 1.0e6 * 384000.0)))
               < 1.0e-7,
           "a phase-inverter input no longer follows its capacitor/grid return");
}

void testMeasuredPowerTubes()
{
    // Independent numerical anchors from the distributed uTracer TubeLib
    // BTetrodeDE/BTetrodeD macros. In particular, the screen values reproduce
    // the fitted SPICE implementation, which does not add its secondary-
    // emission E3 term to G2 even though the accompanying paper does.
    const double sixL6Plate = FxAccess::powerTubePlateCurrent(
        AmpModel::AmericanClean, 450.0, -37.0, 400.0);
    const double sixL6Screen = FxAccess::powerTubeScreenCurrent(
        AmpModel::AmericanClean, 450.0, -37.0, 400.0);
    const double el34Plate = FxAccess::powerTubePlateCurrent(
        AmpModel::BritishCrunch, 400.0, -36.0, 400.0);
    const double el34Screen = FxAccess::powerTubeScreenCurrent(
        AmpModel::BritishCrunch, 400.0, -36.0, 400.0);
    expect(std::abs(sixL6Plate - 0.0492344897961332) < 1.0e-10,
           "the fitted 6L6GC plate-current operating point changed");
    expect(std::abs(sixL6Screen - 0.00285458706767009) < 1.0e-10,
           "the fitted 6L6GC screen-current operating point changed");
    expect(std::abs(el34Plate - 0.0284773203679533) < 1.0e-10,
           "the fitted EL34 plate-current operating point changed");
    expect(std::abs(el34Screen - 0.00360430813213864) < 1.0e-10,
           "the fitted EL34 screen-current operating point changed");

    // Low-plate/high-grid anchors exercise the model's knee and secondary-
    // emission subtraction, where a generic pentode waveshaper differs most.
    expect(std::abs(FxAccess::powerTubePlateCurrent(
                        AmpModel::AmericanClean, 50.0, 0.0, 400.0)
                    - 0.265332846126834) < 1.0e-9,
           "the measured 6L6GC knee no longer matches TubeLib");
    expect(std::abs(FxAccess::powerTubePlateCurrent(
                        AmpModel::BritishCrunch, 50.0, 0.0, 400.0)
                    - 0.369486234177639) < 1.0e-9,
           "the measured EL34 knee no longer matches TubeLib");

    const std::array<AmpModel, 2> models {
        AmpModel::AmericanClean, AmpModel::BritishCrunch };
    double maximumOutputError = 0.0;
    double maximumDemandError = 0.0;
    double maximumScreenResidual = 0.0;
    for (const auto model : models)
    {
        const auto idle = FxAccess::powerTubePairDirect(
            model, 0.0, 0.0, 1.0);
        const auto lookupIdle = FxAccess::powerTubePairLookup(
            model, 0.0f, 0.0f, 1.0f);
        expect(std::abs(idle[0]) < 1.0e-12 && idle[1] == 0.0,
               "a quiescent power-tube pair is not zero-centred");
        expect(idle[2] > 0.0 && idle[2] < 400.0
                   && idle[3] > 0.0 && idle[3] < 400.0
                   && idle[4] < 1.0e-9,
               "an idle output screen escaped its sourced resistor KCL");
        const double idleScreenKcl = model == AmpModel::AmericanClean
            ? 470.0 * FxAccess::powerTubeScreenCurrent(
                model, 450.0, -37.0, idle[2])
            : 800.0 * 2.0 * FxAccess::powerTubeScreenCurrent(
                model, 400.0, -36.0, idle[2]);
        expect(std::abs((400.0 - idle[2]) - idleScreenKcl) < 1.0e-9,
               "the sourced output-screen resistance changed");
        expect(lookupIdle[0] == 0.0 && lookupIdle[1] < 1.0e-12,
               "power-table interpolation created demand at digital silence");
        const auto smallPositive = FxAccess::powerTubePairDirect(
            model, 0.0, 1.0e-4, 1.0);
        const auto smallNegative = FxAccess::powerTubePairDirect(
            model, 0.0, -1.0e-4, 1.0);
        const double localSlope =
            (smallPositive[0] - smallNegative[0]) / 2.0e-4;
        expect(std::abs(localSlope - 1.0) < 2.0e-4,
               "a power-tube pair lost its unity small-signal calibration");

        const auto moderate = FxAccess::powerTubePairDirect(
            model, 0.0, 0.5, 1.0);
        const auto opposed = FxAccess::powerTubePairDirect(
            model, 0.0, -0.5, 1.0);
        const auto driven = FxAccess::powerTubePairDirect(
            model, 0.0, 1.0, 1.0);
        expect(std::abs(moderate[0] + opposed[0]) < 1.0e-10
                   && std::abs(moderate[1] - opposed[1]) < 1.0e-10,
               "the ideal push-pull pair is not symmetric");
        expect(moderate[1] > 0.0 && driven[1] > moderate[1],
               "plate-plus-screen supply demand does not rise with drive");
        expect(moderate[2] < idle[2] && driven[2] < moderate[2]
                   && moderate[4] < 1.0e-9 && driven[4] < 1.0e-9,
               "screen loading does not rise smoothly with output drive");
        if (model == AmpModel::AmericanClean)
            expect(moderate[3] > moderate[2],
                   "the American screen branches no longer move independently");
        else
            expect(moderate[2] == moderate[3]
                       && driven[2] == driven[3],
                   "the Mullard common screen node split into two branches");
        const auto blocked = FxAccess::powerTubePairDirect(
            model, -0.5, 0.5, 1.0);
        const auto forwardBiased = FxAccess::powerTubePairDirect(
            model, 0.25, 0.5, 1.0);
        expect(std::abs(blocked[0]) < std::abs(moderate[0])
                   && std::abs(forwardBiased[0]) > std::abs(moderate[0])
                   && forwardBiased[1] > blocked[1],
               "power-tube common mode does not alter transfer and demand");
        const auto drooped = FxAccess::powerTubePairDirect(
            model, 0.0, 0.8, 0.8);
        const auto charged = FxAccess::powerTubePairDirect(
            model, 0.0, 0.8, 1.0);
        expect(std::abs(drooped[0]) < std::abs(charged[0]),
               "lower power-tube rails do not reduce available output swing");

        // Terminal grid current can now cross zero, while the measured
        // plate/screen surface remains explicitly clamped at its validated
        // AB1 boundary. The pair domain itself is bounded independently.
        expect(FxAccess::powerTubePlateCurrent(
                   model, 200.0, 5.0, 400.0)
                   == FxAccess::powerTubePlateCurrent(
                       model, 200.0, 0.0, 400.0),
               "the measured power-tube surface escaped its AB1 boundary");
        for (const double sign : { -1.0, 1.0 })
        {
            const auto boundary = FxAccess::powerTubePairDirect(
                model, 0.0, sign * 4.0, 0.83);
            const auto clamped = FxAccess::powerTubePairDirect(
                model, 0.0, sign * 40.0, 0.83);
            const auto lookupBoundary = FxAccess::powerTubePairLookup(
                model, 0.0f, static_cast<float>(sign * 4.0), 0.83f);
            const auto lookupClamped = FxAccess::powerTubePairLookup(
                model, 0.0f, static_cast<float>(sign * 40.0), 0.83f);
            expect(std::abs(boundary[0] - clamped[0]) < 1.0e-12
                       && std::abs(boundary[1] - clamped[1]) < 1.0e-12,
                   "the direct power-tube solve escaped its solved domain");
            expect(std::abs(lookupBoundary[0] - lookupClamped[0]) < 1.0e-12
                       && std::abs(lookupBoundary[1] - lookupClamped[1])
                           < 1.0e-12,
                   "the power-tube table escaped its solved domain");
        }

        // Off-grid common, differential and rail positions prove that the
        // audio-thread table retains the complete three-dimensional solve.
        for (int probe = 0; probe < 47; ++probe)
        {
            const float common = static_cast<float>(
                -1.97 + 2.44 * (probe + 0.37) / 47.0);
            const float differential = static_cast<float>(
                -3.91 + 7.82 * ((13 * probe + 3) % 47 + 0.41) / 47.0);
            const float rail = static_cast<float>(
                0.751 + 0.247 * ((19 * probe + 7) % 47 + 0.29) / 47.0);
            const auto direct = FxAccess::powerTubePairDirect(
                model, common, differential, rail);
            const auto lookup = FxAccess::powerTubePairLookup(
                model, common, differential, rail);
            maximumOutputError = std::max(
                maximumOutputError, std::abs(direct[0] - lookup[0]));
            maximumDemandError = std::max(
                maximumDemandError, std::abs(direct[1] - lookup[1]));
            maximumScreenResidual = std::max(
                maximumScreenResidual, direct[4]);
        }

        for (const double common : { -2.0, 0.5 })
            for (const double differential : { 0.0, 4.0 })
                for (const double rail : { 0.75, 1.0 })
                {
                    const auto boundary = FxAccess::powerTubePairDirect(
                        model, common, differential, rail);
                    const double screenSupply = 400.0 * rail;
                    expect(boundary[2] >= 0.0
                               && boundary[2] <= screenSupply
                               && boundary[3] >= 0.0
                               && boundary[3] <= screenSupply
                               && boundary[4] < 1.0e-9,
                           "a screen solve failed at its table-domain boundary");
                }
    }

    const auto american = FxAccess::powerTubePairDirect(
        AmpModel::AmericanClean, 0.0, 0.5, 1.0);
    const auto british = FxAccess::powerTubePairDirect(
        AmpModel::BritishCrunch, 0.0, 0.5, 1.0);
    const auto americanIdle = FxAccess::powerTubePairDirect(
        AmpModel::AmericanClean, 0.0, 0.0, 1.0);
    const auto americanDriven = FxAccess::powerTubePairDirect(
        AmpModel::AmericanClean, 0.0, 1.0, 1.0);
    const auto britishIdle = FxAccess::powerTubePairDirect(
        AmpModel::BritishCrunch, 0.0, 0.0, 1.0);
    const auto britishDriven = FxAccess::powerTubePairDirect(
        AmpModel::BritishCrunch, 0.0, 1.0, 1.0);
    std::cout << std::setprecision(15)
              << "Power-tube table max output/demand error: "
              << maximumOutputError << "/" << maximumDemandError
              << ", 6L6GC/EL34 half-drive " << american[0] << "/"
              << british[0] << '\n';
    expect(std::abs(american[0] - 0.481573134952497) < 1.0e-9
               && std::abs(american[1] - 0.355604944502174) < 1.0e-9,
           "the 6L6GC pair load-line anchor changed");
    expect(std::abs(british[0] - 0.607340859201101) < 1.0e-9
               && std::abs(british[1] - 0.336708725602724) < 1.0e-9,
           "the EL34 pair load-line anchor changed");
    const auto droopedAmerican = FxAccess::powerTubePairDirect(
        AmpModel::AmericanClean, 0.0, 1.0, 0.75);
    const auto droopedBritish = FxAccess::powerTubePairDirect(
        AmpModel::BritishCrunch, 0.0, 1.0, 0.75);
    std::cout << std::setprecision(15)
              << "Screen-grid anchors A idle/half/full: "
              << americanIdle[2] << "/" << american[2] << "/"
              << americanDriven[2]
              << ", B idle/half/full: "
              << britishIdle[2] << "/" << british[2] << "/"
              << britishDriven[2]
              << ", half demands " << american[1] << "/" << british[1]
              << ", drooped demands " << droopedAmerican[1] << "/"
              << droopedBritish[1] << ", max residual "
              << maximumScreenResidual << '\n';
    expect(std::abs(americanIdle[2] - 398.678435789881) < 1.0e-9
               && std::abs(american[2] - 395.462170219656) < 1.0e-9
               && std::abs(americanDriven[2] - 390.829021651181) < 1.0e-9,
           "the AB763-derived 470 Ohm screen branches changed");
    expect(std::abs(britishIdle[2] - 394.697030874616) < 1.0e-9
               && std::abs(british[2] - 381.858290392086) < 1.0e-9
               && std::abs(britishDriven[2] - 341.452662312743) < 1.0e-9,
           "the Mullard common 800 Ohm screen branch changed");
    expect(maximumScreenResidual < 1.0e-9,
           "an off-grid output-screen solve lost KCL convergence");
    expect(std::abs(droopedAmerican[1] - 0.523929013622257) < 1.0e-9,
           "the drooped 6L6GC demand lost nominal-idle self-limiting");
    expect(std::abs(droopedBritish[1] - 0.636417890213624) < 1.0e-9,
           "the drooped EL34 demand lost nominal-idle self-limiting");
    expect(maximumOutputError < 3.0e-4
               && maximumDemandError < 4.0e-4,
           "the runtime power-tube table is too coarse");
    expect(british[0] > american[0] + 0.12,
           "the measured 6L6GC and EL34 load lines collapsed to one curve");
}

void testGainStageAliasing()
{
    // The previous host-rate chain measured between -28 and -40 dB on these
    // same probes; the oversampled block has to stay far below that, because
    // folded intermodulation is what makes a high-gain tone read as digital.
    struct Probe
    {
        float distortion;
        float amp;
        double amplitude;
        AmpModel model;
        const char* name;
    };
    static constexpr std::array<Probe, 6> probes {{
        { 1.0f, 0.0f, 0.30, AmpModel::ModernHighGain,
          "distortion at full drive" },
        { 0.0f, 1.0f, 0.30, AmpModel::ModernHighGain,
          "Modern amp at full drive" },
        { 0.0f, 1.0f, 0.30, AmpModel::AmericanClean,
          "American amp at full drive" },
        { 0.0f, 1.0f, 0.30, AmpModel::BritishCrunch,
          "British amp at full drive" },
        { 0.7f, 1.0f, 0.30, AmpModel::ModernHighGain,
          "distortion stacked into the amp" },
        { 0.0f, 1.0f, 0.08, AmpModel::ModernHighGain,
          "amp on a quiet signal" },
    }};

    for (const double rate : { 44100.0, 48000.0 })
    {
        for (const auto& probe : probes)
        {
            const double floorDb = aliasFloorDb(
                probe.distortion, probe.amp, probe.amplitude, probe.model, rate);
            std::cout << "Alias floor at " << rate << " Hz, " << probe.name
                      << ": " << floorDb << " dB\n";
            expect(floorDb < -70.0,
                   std::string("aliasing above -70 dB at ")
                       + std::to_string(rate) + " Hz for " + probe.name + " ("
                       + std::to_string(floorDb) + " dB)");
        }
    }

    // Host sample rate is a prepare-time choice, so continuity between two
    // rates is not an audio-stream contract. What matters at a stage change is
    // that neither side crosses the audible alias rail. Use one fixed physical
    // tone and the actual 8x->4x, 4x->2x and 2x->1x transition pairs.
    constexpr double fixedToneHz = 1262.7;
    static constexpr std::array<std::array<double, 2>, 3> transitionRates {{
        { 95999.0, 96000.0 },
        { 191999.0, 192000.0 },
        { 383999.0, 384000.0 },
    }};
    for (const auto& transition : transitionRates)
    {
        for (const double rate : transition)
        {
            ElectryFx fx;
            fx.prepare(rate);
            expect(FxAccess::internalRate(fx) >= 384000.0,
                   "the nonlinear bandwidth fell below 384 kHz at "
                       + std::to_string(rate) + " Hz");
            for (const auto& probe : probes)
            {
                const double floorDb = aliasFloorDb(
                    probe.distortion, probe.amp, probe.amplitude,
                    probe.model, rate, fixedToneHz);
                std::cout << "Boundary alias floor at " << rate << " Hz, "
                          << probe.name << ": " << floorDb << " dB\n";
                expect(floorDb < -70.0,
                       std::string("aliasing above -70 dB at nonlinear-rate ")
                           + "boundary " + std::to_string(rate) + " Hz for "
                           + probe.name + " (" + std::to_string(floorDb)
                           + " dB)");
            }
        }
    }
}

#if ELECTRY_MEASURED_MODERN_CABINET
void testMeasuredModernCabinet()
{
    using namespace electry::assets;
    expect(sizeof(ElectryFx) < 65536,
           "measured-cabinet buffers returned to the audio-object stack");
    expect(modernCabinetIrSourceSampleRate == 48000
               && modernCabinetIrSourceBitDepth == 24
               && modernCabinetIrSourceFrameCount == 57420
               && modernCabinetIrPeakSample == 7
               && modernCabinetIrSampleCount == 1024,
           "the measured-cabinet source identity changed");
    expect(modernCabinetIrPcm24Fnv1a64 == 0x8b8027ea461742a1ULL,
           "the measured-cabinet packed PCM integrity guard changed");

    std::vector<float> sourceImpulse(modernCabinetIrSampleCount);
    for (std::size_t index = 0; index < sourceImpulse.size(); ++index)
        sourceImpulse[index] = static_cast<float>(
            static_cast<double>(modernCabinetIrPcm24[index]) / 8388608.0);

    struct RateCase
    {
        double hostRate;
        double internalRate;
        int tapCount;
        int longPartitions;
    };
    const std::array<RateCase, 4> rates {{
        { 8000.0, 64000.0, 1366, 2 },
        { 44100.0, 352800.0, 7527, 14 },
        { 48000.0, 384000.0, 8192, 15 },
        { 384000.0, 384000.0, 8192, 15 },
    }};
    static constexpr std::array<double, 7> responseFrequencies {
        80.0, 110.0, 470.0, 3100.0, 8000.0, 12000.0, 18000.0
    };

    double worstResamplingErrorDb = 0.0;
    double worstResamplingRate = 0.0;
    double worstResamplingFrequency = 0.0;
    double worstNormalisedMagnitudeError = 0.0;
    const double sourceMagnitude1k = firMagnitude(
        sourceImpulse, modernCabinetIrSourceSampleRate, 1000.0);
    for (const auto& rateCase : rates)
    {
        auto fx = std::make_unique<ElectryFx>();
        fx->prepare(rateCase.hostRate);
        const double internalRate = FxAccess::cabinetInternalRate(*fx);
        const auto impulse = FxAccess::cabinetImpulse(*fx);
        expect(internalRate == rateCase.internalRate,
               "the cabinet was prepared at the wrong internal rate");
        expect(FxAccess::cabinetTapCount(*fx) == rateCase.tapCount,
               "the cabinet resampler produced the wrong tap count");
        expect(FxAccess::cabinetLongPartitionCount(*fx)
                   == rateCase.longPartitions,
               "the cabinet prepared the wrong number of long partitions");
        expect(std::isfinite(FxAccess::cabinetNormalisationGain(*fx))
                   && FxAccess::cabinetNormalisationGain(*fx) > 0.0f,
               "the cabinet calibration gain is invalid");

        const double expectedFirst = static_cast<double>(
            modernCabinetIrPcm24[0]) / 8388608.0
            * modernCabinetIrSourceSampleRate / internalRate
            * FxAccess::cabinetNormalisationGain(*fx);
        expect(std::abs(static_cast<double>(impulse.front()) - expectedFirst)
                   < 2.0e-9,
               "the cabinet resampler moved or rescaled source sample zero");

        const auto peak = std::max_element(
            impulse.begin(), impulse.end(), [] (float first, float second)
            {
                return std::abs(first) < std::abs(second);
            });
        const int peakIndex = static_cast<int>(peak - impulse.begin());
        const int expectedPeak = static_cast<int>(std::lround(
            modernCabinetIrPeakSample * internalRate
            / modernCabinetIrSourceSampleRate));
        expect(std::abs(peakIndex - expectedPeak) <= 2,
               "the cabinet resampler moved the physical IR peak");

        const double prepared1k = firMagnitude(
            impulse, internalRate, 1000.0);
        const double reference1k = FxAccess::shippingModernCabinetMagnitude(
            *fx, 1000.0);
        expect(std::abs(prepared1k - reference1k)
                   / std::max(reference1k, 1.0e-12) < 2.0e-6,
               "the measured cabinet is not level-matched at 1 kHz");

        for (const double frequency : responseFrequencies)
        {
            // A diagnostic 8 kHz host still prepares the nonlinear path at
            // 64 kHz, but frequencies above its 4 kHz output Nyquist cannot
            // reach a listener and are not a meaningful resampler contract.
            if (frequency >= 0.45 * rateCase.hostRate)
                continue;
            const double sourceRelative = firMagnitude(
                sourceImpulse, modernCabinetIrSourceSampleRate, frequency)
                / sourceMagnitude1k;
            const double preparedRelative = firMagnitude(
                impulse, internalRate, frequency) / prepared1k;
            worstNormalisedMagnitudeError = std::max(
                worstNormalisedMagnitudeError,
                std::abs(preparedRelative - sourceRelative));
            // Relative decibels become unstable in the IR's deep stop-band
            // nulls, so pin those by absolute normalised magnitude below and
            // reserve the tight dB comparison for useful cabinet output.
            if (sourceRelative < 0.01)
                continue;
            const double delta = 20.0 * std::log10(
                std::max(preparedRelative, 1.0e-30)
                / std::max(sourceRelative, 1.0e-30));
            if (std::abs(delta) > worstResamplingErrorDb)
            {
                worstResamplingErrorDb = std::abs(delta);
                worstResamplingRate = rateCase.hostRate;
                worstResamplingFrequency = frequency;
            }
        }
    }
    std::cout << "Measured-cabinet worst preparation response error: "
              << worstResamplingErrorDb << " dB at "
              << worstResamplingFrequency << " Hz / "
              << worstResamplingRate << " Hz host\n";
    std::cout << "Measured-cabinet worst normalised magnitude error: "
              << worstNormalisedMagnitudeError << " ("
              << 20.0 * std::log10(std::max(
                    worstNormalisedMagnitudeError, 1.0e-30))
              << " dB)\n";
    expect(worstResamplingErrorDb < 0.12,
           "cabinet preparation changed the source response by over 0.12 dB");
    expect(worstNormalisedMagnitudeError < 0.001,
           "cabinet preparation introduced over -60 dB normalised error");

    auto fx = std::make_unique<ElectryFx>();
    fx->prepare(sampleRate);
    const auto impulse = FxAccess::cabinetImpulse(*fx);
    const std::vector<float> delta { 1.0f };
    const auto deltaOutput = FxAccess::cabinetConvolve(
        *fx, delta, impulse.size() + 1024);
    double maximumImpulseError = 0.0;
    for (std::size_t index = 0; index < impulse.size(); ++index)
        maximumImpulseError = std::max(maximumImpulseError, std::abs(
            static_cast<double>(deltaOutput[index] - impulse[index])));
    double tailPeak = 0.0;
    for (std::size_t index = impulse.size(); index < deltaOutput.size(); ++index)
        tailPeak = std::max(tailPeak,
                            std::abs(static_cast<double>(deltaOutput[index])));
    expect(deltaOutput.front() == impulse.front(),
           "the cabinet delayed its non-zero first coefficient");
    expect(maximumImpulseError < 2.0e-6,
           "partitioned cabinet impulse output differs from its prepared FIR");
    expect(tailPeak < 2.0e-7,
           "the cabinet emitted a numerical tail after its final coefficient");

    constexpr std::size_t inputLength = 777;
    std::vector<float> input(inputLength);
    for (std::size_t index = 0; index < input.size(); ++index)
        input[index] = static_cast<float>(
            0.17 * std::sin(0.071 * static_cast<double>(index))
            + 0.003 * (static_cast<int>((index * 37) % 101) - 50));
    const std::size_t convolutionLength = input.size() + impulse.size() - 1;
    const auto partitioned = FxAccess::cabinetConvolve(
        *fx, input, convolutionLength + 513);
    double maximumConvolutionError = 0.0;
    for (std::size_t outputIndex = 0;
         outputIndex < convolutionLength; ++outputIndex)
    {
        const std::size_t firstInput = outputIndex >= impulse.size() - 1
            ? outputIndex - (impulse.size() - 1) : 0;
        const std::size_t lastInput = std::min(outputIndex, input.size() - 1);
        double reference = 0.0;
        for (std::size_t inputIndex = firstInput;
             inputIndex <= lastInput; ++inputIndex)
            reference += static_cast<double>(input[inputIndex])
                       * impulse[outputIndex - inputIndex];
        maximumConvolutionError = std::max(
            maximumConvolutionError,
            std::abs(static_cast<double>(partitioned[outputIndex])
                     - reference));
    }
    double flushedPeak = 0.0;
    for (std::size_t index = convolutionLength;
         index < partitioned.size(); ++index)
        flushedPeak = std::max(flushedPeak,
                               std::abs(static_cast<double>(partitioned[index])));
    std::cout << "Measured-cabinet impulse/direct errors: "
              << maximumImpulseError << '/' << maximumConvolutionError
              << ", flushed tail " << flushedPeak << '\n';
    expect(maximumConvolutionError < 1.0e-5,
           "partitioned cabinet differs from direct convolution");
    expect(flushedPeak < 2.0e-7,
           "partitioned cabinet did not flush to numerical silence");

    for (const int boundary : { 63, 64, 511, 512, 513 })
        expect(FxAccess::cabinetResetClears(*fx, boundary),
               "cabinet reset left state at a partition boundary");
    expect(FxAccess::cabinetStereoLeak(*fx, true) == 0.0
               && FxAccess::cabinetStereoLeak(*fx, false) == 0.0,
           "shared cabinet kernels caused stereo state leakage");

    FxParameters parameters;
    parameters.amp = 1.0f;
    parameters.ampModel = AmpModel::ModernHighGain;
    fx->setParameters(parameters);
    fx->reset();
    constexpr int timingBlockSize = 7;
    constexpr int timingSamples = 12000;
    std::array<float, timingBlockSize> left {};
    std::array<float, timingBlockSize> right {};
    const auto render = [&] (int sampleCount)
    {
        int rendered = 0;
        while (rendered < sampleCount)
        {
            const int count = std::min(timingBlockSize,
                                       sampleCount - rendered);
            for (int index = 0; index < count; ++index)
            {
                const float sample = static_cast<float>(0.18 * std::sin(
                    2.0 * pi * 173.0 * (rendered + index) / sampleRate));
                left[static_cast<std::size_t>(index)] = sample;
                right[static_cast<std::size_t>(index)] = sample;
            }
            fx->process(left.data(), right.data(), count);
            rendered += count;
        }
    };
    render(2048);
    const auto start = std::chrono::steady_clock::now();
    render(timingSamples);
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    const double realtimeRatio = elapsed
        / (static_cast<double>(timingSamples) / sampleRate);
    std::cout << "Measured-cabinet full-path 7-sample-block CPU ratio: "
              << realtimeRatio << "x realtime\n";
    expect(realtimeRatio < 4.0,
           "the measured cabinet missed its conservative real-time CPU gate");
}
#endif

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

    std::cout << "Cabinet response relative to 1 kHz: 45 Hz "
              << low - reference << ", 110 Hz " << thump - reference
              << ", 470 Hz " << honk - reference << ", 3.1 kHz "
              << presence - reference << ", 8 kHz " << top - reference
              << ", 12 kHz " << beyond - reference << " dB\n";

    // A sealed cabinet has no useful output below the box, a low-mid thump, a
    // scooped boxy region, a presence peak, and it is essentially gone an
    // octave above five kilohertz. The previous one-pole model had none of it.
    expect(low - reference < -6.0,
           "the cabinet passes too much below the box resonance");
#if ! ELECTRY_MEASURED_MODERN_CABINET
    expect(thump - reference > 1.0, "the cabinet has no low-mid thump");
#endif
    expect(honk - reference < -1.0, "the cabinet's boxy region is not scooped");
#if ! ELECTRY_MEASURED_MODERN_CABINET
    expect(presence - reference > 1.5, "the cabinet has no presence peak");
#endif
    expect(top - reference < -12.0,
           "the cabinet does not roll off above the speaker's range");
    expect(beyond - reference < -25.0,
           "the cabinet's top-end roll-off is not steep enough");

    // Golden small-signal fingerprint of the pre-selector Modern path. These
    // narrow cross-platform tolerances catch a changed coefficient or routing
    // operation while allowing ordinary libm rounding in the sine fixture.
    const std::array<double, 6> measured {
        low - reference, thump - reference, honk - reference,
        presence - reference, top - reference, beyond - reference };
    const std::array<double, 6> golden {
        -14.4410, 1.10749, -6.52522, 3.22390, -21.9261, -38.9643 };
#if ! ELECTRY_MEASURED_MODERN_CABINET
    for (std::size_t index = 0; index < measured.size(); ++index)
        expect(std::abs(measured[index] - golden[index]) < 0.03,
               "the Modern amplifier's shipping response fingerprint changed");
#else
    static_cast<void>(measured);
    static_cast<void>(golden);
#endif
}

void testToneStackCircuits()
{
    ElectryFx fx;
    fx.prepare(sampleRate); // 8x: tone stacks are designed at 384 kHz
    const auto american = FxAccess::toneStackCoefficients(
        fx, AmpModel::AmericanClean);
    const auto british = FxAccess::toneStackCoefficients(
        fx, AmpModel::BritishCrunch);
    const std::array<double, 7> expectedAmerican {
        0.623549669651815, -1.86574174562395, 1.86086175100710,
        -0.618669675034965, -2.95687821217229, 2.91387714851730,
        -0.956998919821471 };
    const std::array<double, 7> expectedBritish {
        0.739479285605914, -2.20759482178058, 2.19681264922900,
        -0.728697113054336, -2.97548772395903, 2.95105968746894,
        -0.975571923083905 };
    for (std::size_t index = 0; index < american.size(); ++index)
    {
        expect(std::abs(american[index] - expectedAmerican[index]) < 2.0e-12,
               "the American passive tone-stack BLT coefficients changed");
        expect(std::abs(british[index] - expectedBritish[index]) < 2.0e-12,
               "the British passive tone-stack BLT coefficients changed");
    }

    const double american1k = toneStackMagnitudeDb(american, 1000.0, 384000.0);
    const double british1k = toneStackMagnitudeDb(british, 1000.0, 384000.0);
    std::cout << "Passive tone-stack insertion at 1 kHz: American "
              << american1k << " dB, British " << british1k << " dB\n";
    expect(std::abs(american1k + 13.00577) < 0.002,
           "the American tone-stack response does not match its RC circuit");
    expect(std::abs(british1k + 5.84512) < 0.002,
           "the British tone-stack response does not match its RC circuit");
}

void testAmpModelVoices()
{
    static constexpr std::array<AmpModel, 3> models {
        AmpModel::AmericanClean, AmpModel::BritishCrunch,
        AmpModel::ModernHighGain };
    static constexpr std::array<const char*, 3> names {
        "American", "British", "Modern" };
    static constexpr std::array<double, 6> frequencies {
        90.0, 120.0, 470.0, 700.0, 2800.0, 8000.0 };
    static constexpr std::array<std::array<double, 6>, 3> goldenResponse {{
        { -6.76776, -4.38230, -7.32158, -3.99830, 9.20223, -13.9867 },
        {  1.44711,  3.41606, -1.52745, -0.69891, 2.92328, -21.1522 },
        {  0.36333,  0.60681, -6.52226, -2.20414, 3.17347, -22.0021 },
    }};
    std::array<std::array<double, frequencies.size()>, models.size()> response {};

    for (std::size_t model = 0; model < models.size(); ++model)
    {
        const double reference = ampPathMagnitudeDb(
            1000.0, 1.0f, models[model]);
        std::cout << names[model] << " response relative to 1 kHz:";
        for (std::size_t band = 0; band < frequencies.size(); ++band)
        {
            response[model][band] = ampPathMagnitudeDb(
                frequencies[band], 1.0f, models[model]) - reference;
            std::cout << ' ' << frequencies[band] << " Hz "
                      << response[model][band] << " dB";
#if ELECTRY_MEASURED_MODERN_CABINET
            if (models[model] != AmpModel::ModernHighGain)
#endif
                expect(std::abs(response[model][band]
                                - goldenResponse[model][band]) < 0.08,
                       std::string(names[model])
                           + " amplifier/cabinet response anchor changed");
        }
        std::cout << '\n';
        expect(response[model].back() < -12.0,
               std::string(names[model])
                   + " cabinet passes implausible 8 kHz energy");
    }

    const auto distance = [&response] (std::size_t first, std::size_t second)
    {
        double sum = 0.0;
        for (std::size_t band = 0; band < frequencies.size(); ++band)
        {
            const double delta = response[first][band] - response[second][band];
            sum += delta * delta;
        }
        return std::sqrt(sum / frequencies.size());
    };
    const double americanBritish = distance(0, 1);
    const double americanModern = distance(0, 2);
    const double britishModern = distance(1, 2);
    std::cout << "Amp spectral RMS separations: A/B " << americanBritish
              << " dB, A/M " << americanModern << " dB, B/M "
              << britishModern << " dB\n";
    expect(americanBritish > 2.0 && americanModern > 2.0
               && britishModern > 2.0,
           "two amplifier models collapse to nearly the same response");

    // The BLT must describe the same analogue network at every internal rate.
    // Compare a musically relevant upper-mid ratio across the 8x and native
    // host-rate paths rather than comparing latency-shifted waveforms.
    for (std::size_t model = 0; model < models.size(); ++model)
    {
        const double at48 = ampPathMagnitudeDb(
            2800.0, 0.01f, models[model], 48000.0, true)
            - ampPathMagnitudeDb(
                1000.0, 0.01f, models[model], 48000.0, true);
        const double at384 = ampPathMagnitudeDb(
            2800.0, 0.01f, models[model], 384000.0, true)
            - ampPathMagnitudeDb(
                1000.0, 0.01f, models[model], 384000.0, true);
        std::cout << names[model] << " 2.8 kHz/1 kHz at 48/384 kHz: "
                  << at48 << "/" << at384 << " dB\n";
        expect(std::abs(at48 - at384) < 0.45,
               std::string(names[model])
                   + " response moved with host sample rate");
    }

    // Exercise the corrected 384/705.6 kHz capacitor clock through the public
    // prepare/process route. Both host rates use 8x staging, so a later caller
    // regression that substitutes the host clock for oversampledRate_ cannot
    // hide behind the direct circuit test above.
    const double britishReference48 = settledAmpPathMagnitude(
        1000.0, AmpModel::BritishCrunch, 48000.0);
    const double britishReference88k2 = settledAmpPathMagnitude(
        1000.0, AmpModel::BritishCrunch, 88200.0);
    const double britishLow48 = 20.0 * std::log10(
        settledAmpPathMagnitude(40.0, AmpModel::BritishCrunch, 48000.0)
        / britishReference48);
    const double britishLow88k2 = 20.0 * std::log10(
        settledAmpPathMagnitude(40.0, AmpModel::BritishCrunch, 88200.0)
        / britishReference88k2);
    std::cout << "British full-path 40 Hz/1 kHz at 48/88.2 kHz: "
              << britishLow48 << "/" << britishLow88k2 << " dB\n";
    expect(std::abs(britishLow48 - britishLow88k2) < 0.05,
           "the shipping phase-inverter capacitor response moved between "
           "384 and 705.6 kHz internal clocks");
}

void testAmpModelDynamics()
{
    static constexpr std::array<AmpModel, 3> models {
        AmpModel::AmericanClean, AmpModel::BritishCrunch,
        AmpModel::ModernHighGain };
    static constexpr std::array<const char*, 3> names {
        "American", "British", "Modern" };
    std::array<double, 3> quietGains {};
    std::array<double, 3> compression {};
    for (std::size_t index = 0; index < models.size(); ++index)
    {
        const double quietGain = steadyAmpGainDb(models[index], 0.002);
        const double loudGain = steadyAmpGainDb(models[index], 0.35);
        quietGains[index] = quietGain;
        compression[index] = loudGain - quietGain;
        std::cout << names[index] << " amp gain: quiet " << quietGain
                  << " dB, loud " << loudGain << " dB, compression "
                  << compression[index] << " dB\n";
        expect(std::isfinite(quietGain) && std::isfinite(loudGain),
               std::string(names[index]) + " dynamics are non-finite");
        expect(loudGain > -18.0 && loudGain < 12.0,
               std::string(names[index]) + " loud output is not playable");
        expect(compression[index] < -0.20,
               std::string(names[index])
                   + " power path has no level-dependent compression");
    }
    expect(compression[0] > compression[1] + 0.25,
           "the American model is not cleaner than the British model");
    expect(compression[1] > compression[2] + 0.25,
           "the British model is not cleaner than the Modern model");
    const auto [quietMinimum, quietMaximum] = std::minmax_element(
        quietGains.begin(), quietGains.end());
#if ! ELECTRY_MEASURED_MODERN_CABINET
    expect(*quietMaximum - *quietMinimum < 5.0,
           "amp selection changes ordinary small-signal level by over 5 dB");
#else
    static_cast<void>(quietMinimum);
    static_cast<void>(quietMaximum);
#endif
}

void testAmpModelSwitchingAndBlockInvariance()
{
    static constexpr std::array<AmpModel, 3> models {
        AmpModel::AmericanClean, AmpModel::BritishCrunch,
        AmpModel::ModernHighGain };
    for (const auto model : models)
    {
        const auto oneBlock = renderAmpInBlocks(model, 12288);
        const auto oddBlocks = renderAmpInBlocks(model, 137);
        expect(std::memcmp(oneBlock.data(), oddBlocks.data(),
                           oneBlock.size() * sizeof(float)) == 0,
               "a stable amplifier model depends on host block size");
    }
    const auto switchedLarge = renderModelSwitchesInBlocks(4096);
    const auto switchedSmall = renderModelSwitchesInBlocks(73);
    expect(std::memcmp(switchedLarge.data(), switchedSmall.data(),
                       switchedLarge.size() * sizeof(float)) == 0,
           "amp-model automation depends on host block size");
    expect(allFinite(switchedLarge) && peakOf(switchedLarge) < 1.5,
           "a model change made mid-crossfade produced unsafe output");

    constexpr int segmentLength = 16384;
    constexpr int length = 3 * segmentLength;
    auto source = sineBlock(length, 173, 0.28);
    auto left = source;
    auto right = source;
    ElectryFx fx;
    fx.prepare(sampleRate);
    FxParameters parameters;
    parameters.amp = 0.90f;
    parameters.ampModel = AmpModel::AmericanClean;
    fx.setParameters(parameters);
    fx.reset();
    for (int segment = 0; segment < 3; ++segment)
    {
        parameters.ampModel = models[static_cast<std::size_t>(segment)];
        fx.setParameters(parameters);
        fx.process(left.data() + segment * segmentLength,
                   right.data() + segment * segmentLength, segmentLength);
    }

    const auto largestStep = [] (const std::vector<float>& buffer,
                                 int begin, int end)
    {
        double result = 0.0;
        for (int sample = std::max(begin, 1); sample < end; ++sample)
            result = std::max(result, std::abs(
                static_cast<double>(buffer[static_cast<std::size_t>(sample)])
                - buffer[static_cast<std::size_t>(sample - 1)]));
        return result;
    };
    const double allSteps = largestStep(left, 1, length);
    const double americanSteps = largestStep(
        left, segmentLength / 2, segmentLength);
    const double britishSteps = largestStep(
        left, segmentLength + segmentLength / 2, 2 * segmentLength);
    const double modernSteps = largestStep(
        left, 2 * segmentLength + segmentLength / 2, length);
    const double settledStep = std::max(
        americanSteps, std::max(britishSteps, modernSteps));
    std::cout << "Amp model switch largest step " << allSteps
              << ", settled model step " << settledStep << '\n';
    expect(allSteps < 1.5 * settledStep + 0.01,
           "switching amplifier model produced an audible discontinuity");
    expect(allFinite(left) && peakOf(left) < 1.5,
           "amp-model switching produced unsafe output");

    const auto weights = FxAccess::ampModelWeights(fx);
    std::cout << "Settled model weights: " << weights[0] << ", "
              << weights[1] << ", " << weights[2] << '\n';
    expect(weights[0] == 0.0f && weights[1] == 0.0f && weights[2] == 1.0f,
           "the amp-model crossfade did not settle to one running circuit");
    expect(FxAccess::ampModelAtRest(fx, AmpModel::AmericanClean)
               && FxAccess::ampModelAtRest(fx, AmpModel::BritishCrunch)
               && ! FxAccess::ampModelAtRest(fx, AmpModel::ModernHighGain),
           "a faded-out amplifier retained recursive circuit state");
}

void testEnabledGainModulesStayInCircuit()
{
    // Distortion and Amp are drive knobs, not parallel blend controls. Once a
    // module is enabled, even the UI's minimum 0.1% detent must retain the
    // same coupling/voice filters and (for the amp) the speaker cabinet. The
    // old setting leaked 99.9% unfiltered DI and measured almost flat.
    constexpr float minimumDrive = 0.001f;
    const double ampReference = ampPathMagnitudeDb(1000.0, minimumDrive);
    const double ampTop = ampPathMagnitudeDb(8000.0, minimumDrive) - ampReference;
    const double fullAmpTop = ampPathMagnitudeDb(8000.0) - ampPathMagnitudeDb(1000.0);
    const double pedalReference = pedalPathMagnitudeDb(1000.0, minimumDrive);
    const double pedalLow = pedalPathMagnitudeDb(40.0, minimumDrive) - pedalReference;
    const double pedalTop = pedalPathMagnitudeDb(12000.0, minimumDrive) - pedalReference;
    const double fullPedalLow = pedalPathMagnitudeDb(40.0) - pedalPathMagnitudeDb(1000.0);
    const double fullPedalTop = pedalPathMagnitudeDb(12000.0) - pedalPathMagnitudeDb(1000.0);

    std::cout << "Enabled 0.1% paths relative to 1 kHz: amp 8 kHz "
              << ampTop << " dB, pedal 40 Hz " << pedalLow
              << " dB, pedal 12 kHz " << pedalTop << " dB\n";
    expect(ampTop < -15.0 && std::abs(ampTop - fullAmpTop) < 0.5,
           "a low Amp setting leaked dry DI around the cabinet");
    expect(pedalLow < -8.0 && pedalTop < -8.0
               && std::abs(pedalLow - fullPedalLow) < 0.5
               && std::abs(pedalTop - fullPedalTop) < 0.5,
           "a low Distortion setting leaked dry DI around the pedal circuit");
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

    // The two measured output-tube paths drive their reservoirs from solved
    // plate-plus-screen current, not from the absolute audio sample used by
    // the legacy Modern branch. Read the private circuit state after matched
    // loud/quiet holds and after a rest so this routing cannot regress while
    // the cabinet happens to conceal it.
    for (const auto model : { AmpModel::AmericanClean,
                              AmpModel::BritishCrunch })
    {
        const auto demandAfter = [model] (double amplitude, bool recover)
        {
            ElectryFx fx;
            fx.prepare(sampleRate);
            FxParameters parameters;
            parameters.amp = 1.0f;
            parameters.ampModel = model;
            fx.setParameters(parameters);
            constexpr int block = 512;
            for (int index = 0; index < 40; ++index)
            {
                std::vector<float> left(block, 0.0f);
                std::vector<float> right(block, 0.0f);
                fx.process(left.data(), right.data(), block);
            }
            for (int index = 0; index < 96; ++index)
            {
                auto left = sineBlock(block, 4, amplitude);
                auto right = left;
                fx.process(left.data(), right.data(), block);
            }
            const float held = FxAccess::amplifierSag(fx, model);
            if (! recover)
                return held;
            for (int index = 0; index < 180; ++index)
            {
                std::vector<float> left(block, 0.0f);
                std::vector<float> right(block, 0.0f);
                fx.process(left.data(), right.data(), block);
            }
            return FxAccess::amplifierSag(fx, model);
        };
        const float loudDemand = demandAfter(0.35, false);
        const float quietDemand = demandAfter(0.002, false);
        const float recoveredDemand = demandAfter(0.35, true);
        std::cout << (model == AmpModel::AmericanClean ? "6L6GC" : "EL34")
                  << " sag state loud/quiet/recovered: " << loudDemand << "/"
                  << quietDemand << "/" << recoveredDemand << '\n';
        expect(loudDemand > quietDemand + 0.05f,
               "a measured power-tube supply does not distinguish loud current demand");
        expect(recoveredDemand < 0.20f * loudDemand,
               "a measured power-tube supply did not recharge during silence");
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
    for (const auto model : { AmpModel::AmericanClean,
                              AmpModel::BritishCrunch,
                              AmpModel::ModernHighGain })
    {
        for (const float mix : { 0.05f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            FxParameters parameters;
            parameters.amp = mix;
            parameters.ampModel = model;
            const double gainDb = 20.0 * std::log10(
                std::max(rmsOf(throughSelectedChain(parameters, riff)), 1.0e-30)
                / dry);
            expect(gainDb > -6.0 && gainDb < 12.0,
                   "amp model " + std::to_string(static_cast<int>(model))
                       + " control travel is not level-consistent at "
                       + std::to_string(mix));
        }
    }

    std::array<double, 3> modelGains {};
    for (const auto model : { AmpModel::AmericanClean,
                              AmpModel::BritishCrunch,
                              AmpModel::ModernHighGain })
    {
        FxParameters parameters;
        parameters.amp = 0.90f;
        parameters.ampModel = model;
        const double gainDb = 20.0 * std::log10(
            std::max(rmsOf(throughSelectedChain(parameters, riff)), 1.0e-30)
            / dry);
        modelGains[static_cast<std::size_t>(model)] = gainDb;
        std::cout << "Selected amp " << static_cast<int>(model)
                  << " Drop-E gain at 90%: " << gainDb << " dB\n";
        expect(gainDb > -6.0 && gainDb < 12.0,
               "an amp model's 90% Drop-E output is not in a playable range");
    }
    const auto [modelMinimum, modelMaximum] = std::minmax_element(
        modelGains.begin(), modelGains.end());
    expect(*modelMaximum - *modelMinimum < 4.0,
           "selecting an amp changes the same Drop-E riff by over 4 dB");
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

    // Turning only the amp off now skips its private tube/filter state while
    // the distortion pedal stays live. Its 15 ms relay ramp must make the
    // later cold-state re-entry just as smooth as an ordinary automation move.
    ElectryFx switched;
    switched.prepare(sampleRate);
    FxParameters stacked;
    stacked.distortion = 1.0f;
    stacked.amp = 1.0f;
    switched.setParameters(stacked);
    for (int pass = 0; pass < 6; ++pass)
    {
        left = source;
        right = source;
        switched.process(left.data(), right.data(), static_cast<int>(left.size()));
    }
    const double stackedStep = largestStep(left);

    FxParameters pedalOnly;
    pedalOnly.distortion = 1.0f;
    switched.setParameters(pedalOnly);
    for (int pass = 0; pass < 6; ++pass)
    {
        left = source;
        right = source;
        switched.process(left.data(), right.data(), static_cast<int>(left.size()));
    }
    const double pedalStep = largestStep(left);
    expect(FxAccess::ampAtRest(switched),
           "an individually bypassed amp retained stale circuit state");

    switched.setParameters(stacked);
    left = source;
    right = source;
    switched.process(left.data(), right.data(), static_cast<int>(left.size()));
    const double ampReentryStep = largestStep(left);
    const double moduleBound = 1.5 * std::max(stackedStep, pedalStep) + 0.01;
    expect(ampReentryStep < moduleBound,
           "re-engaging the skipped amp while distortion stayed live produced a step");

    ElectryFx reciprocal;
    reciprocal.prepare(sampleRate);
    reciprocal.setParameters(stacked);
    for (int pass = 0; pass < 6; ++pass)
    {
        left = source;
        right = source;
        reciprocal.process(left.data(), right.data(), static_cast<int>(left.size()));
    }
    FxParameters ampOnly;
    ampOnly.amp = 1.0f;
    reciprocal.setParameters(ampOnly);
    for (int pass = 0; pass < 6; ++pass)
    {
        left = source;
        right = source;
        reciprocal.process(left.data(), right.data(), static_cast<int>(left.size()));
    }
    expect(FxAccess::pedalAtRest(reciprocal),
           "an individually bypassed pedal retained stale RC/filter state");
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

    for (const double rate : { 8000.0, 22050.0, 44100.0, 48000.0, 88200.0,
                               96000.0, 176400.0, 192000.0, 384000.0 })
    {
        const int length = static_cast<int>(rate * 0.25);
        const auto probe = sineBlock(length, 431, 0.40);
        for (const auto model : { AmpModel::AmericanClean,
                                  AmpModel::BritishCrunch,
                                  AmpModel::ModernHighGain })
        {
            parameters.ampModel = model;
            const auto processed = throughChain(parameters, probe, rate);
            expect(allFinite(processed),
                   "an amp model produced a non-finite sample at "
                       + std::to_string(rate) + " Hz");
            expect(peakOf(processed) < 2.0001,
                   "an amp model exceeded the output clamp at "
                       + std::to_string(rate) + " Hz");
        }

        ElectryFx fx;
        fx.prepare(rate);
        double expectedInternalRate = rate;
        for (int stage = 0;
             stage < 3 && expectedInternalRate < 384000.0; ++stage)
            expectedInternalRate *= 2.0;
        expect(FxAccess::internalRate(fx) == expectedInternalRate,
               "unexpected nonlinear frame rate at " + std::to_string(rate)
                   + " Hz");
        fx.setParameters(parameters);
        std::vector<float> left = probe;
        std::vector<float> right = probe;
        fx.process(left.data(), right.data(), length);
        const float latency = fx.gainStageLatencySamples();
        const float expected = rate < 96000.0
            ? 20.125f : (rate < 192000.0
                ? 17.25f : (rate < 384000.0 ? 11.5f : 0.0f));
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

// setParameters() runs each of the five panel controls through sanitiseMix(): a
// non-finite value falls back to 0.0, then the result is clamped to 0..1. The
// discrete model is separately validated so a corrupt state cannot index past
// the three prepared amplifier circuits.
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
    outOfRange.ampModel = static_cast<AmpModel>(99);
    const auto clamped = sanitised(outOfRange);
    expect(clamped.distortion == 0.0f, "a negative distortion mix was not clamped to 0.0");
    expect(clamped.amp == 1.0f, "an above-range amp mix was not clamped to 1.0");
    expect(clamped.compressor == 0.0f, "a negative compressor mix was not clamped to 0.0");
    expect(clamped.delay == 1.0f, "an above-range delay mix was not clamped to 1.0");
    expect(clamped.room == 1.0f, "an above-range room mix was not clamped to 1.0");
    expect(clamped.ampModel == AmpModel::ModernHighGain,
           "an invalid amp model did not fall back to Modern");

    FxParameters ordinary;
    ordinary.distortion = 0.25f;
    ordinary.amp = 0.5f;
    ordinary.compressor = 0.75f;
    ordinary.delay = 0.1f;
    ordinary.room = 0.9f;
    ordinary.ampModel = AmpModel::BritishCrunch;
    const auto passedThrough = sanitised(ordinary);
    expect(passedThrough.distortion == 0.25f && passedThrough.amp == 0.5f
               && passedThrough.compressor == 0.75f && passedThrough.delay == 0.1f
               && passedThrough.room == 0.9f
               && passedThrough.ampModel == AmpModel::BritishCrunch,
           "ordinary in-range mixes were altered by sanitisation");
}

} // namespace

int main()
{
    testRapidPalmBodyDirection();
    testHalfbandKernel();
    testExactDryBypass();
    testCircuitGainStages();
    testPhaseInverterCircuits();
    testMeasuredPowerTubes();
    testGainStageAliasing();
#if ELECTRY_MEASURED_MODERN_CABINET
    testMeasuredModernCabinet();
#endif
    testCabinetVoicing();
    testToneStackCircuits();
    testAmpModelVoices();
    testAmpModelDynamics();
    testAmpModelSwitchingAndBlockInvariance();
    testEnabledGainModulesStayInCircuit();
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
