// Common-host VCF/BBD numerical-quality audit.
//
// This executable deliberately does not expose a production quality control.
// It drives the shipping private kernels through their existing friend seam,
// reconstructs every candidate through the shipping decimator, and compares
// 1x/2x/4x at the same 44.1/48 kHz host boundaries.  Four-times processing is
// one candidate in the matrix, never the reference.

#include "DSP/YouKnow106Chorus.h"
#include "DSP/YouKnow106Engine.h"
#include "OversamplingQualitySupport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace youknow106
{
// Executable-local access only.  Nothing here is part of the plug-in API.
struct YouKnow106TestAccess
{
    using Cascade = YouKnow106Engine::OtaCascade;

    struct BbdState
    {
        int writeIndex {};
        double clockPhase {};
    };

    static constexpr float otaHeadroom() noexcept
    {
        return YouKnow106Engine::otaHeadroomVolts;
    }

    static constexpr float feedbackHeadroom() noexcept
    {
        return YouKnow106Engine::VoicedResonanceCompatibilityProfile::
            loopHeadroomVolts;
    }

    static float inputCompensation(float feedback) noexcept
    {
        return YouKnow106Engine::VoicedResonanceCompatibilityProfile::
            inputCompensation(feedback);
    }

    static std::vector<float> decimate(const std::vector<float>& internal,
                                       int factor)
    {
        if (factor != 1 && factor != 2 && factor != 4)
            throw std::runtime_error("factor must be 1, 2 or 4");
        if (internal.size() % static_cast<std::size_t>(factor) != 0u)
            throw std::runtime_error("internal render is not host-frame aligned");
        if (factor == 1)
            return internal;

        YouKnow106Engine engine;
        std::vector<float> output(
            internal.size() / static_cast<std::size_t>(factor));
        for (std::size_t host = 0; host < output.size(); ++host)
        {
            const auto base = host * static_cast<std::size_t>(factor);
            float left = 0.0f;
            float right = 0.0f;
            if (factor == 2)
            {
                engine.downsamplePair(
                    engine.firstDecimator_, internal[base], internal[base],
                    internal[base + 1u], internal[base + 1u], left, right);
            }
            else
            {
                float firstLeft = 0.0f;
                float firstRight = 0.0f;
                float secondLeft = 0.0f;
                float secondRight = 0.0f;
                engine.downsamplePair(
                    engine.firstDecimator_, internal[base], internal[base],
                    internal[base + 1u], internal[base + 1u],
                    firstLeft, firstRight);
                engine.downsamplePair(
                    engine.firstDecimator_, internal[base + 2u],
                    internal[base + 2u], internal[base + 3u],
                    internal[base + 3u], secondLeft, secondRight);
                engine.downsamplePair(
                    engine.secondDecimator_, firstLeft, firstRight,
                    secondLeft, secondRight, left, right);
            }
            output[host] = left;
        }
        return output;
    }

    static float processBbdLine(Chorus& chorus, float input, float clockHz,
                                float sampleRate) noexcept
    {
        return chorus.lineA_.process(
            input, clockHz, sampleRate, chorus.support_,
            chorus.support_.wetOutputCouplingConnectedG, 0.0f);
    }

    static constexpr double shippingDecimatorBoundaryDelayHostFrames(
        int factor) noexcept
    {
        constexpr double half =
            (YouKnow106Engine::halfbandTaps - 1) / 2.0;
        if (factor == 2)
            return half / 2.0;
        if (factor == 4)
            return half / 4.0 + half / 2.0;
        return 0.0;
    }

    static BbdState bbdState(const Chorus& chorus) noexcept
    {
        return { chorus.lineA_.writeIndex, chorus.lineA_.clockPhase };
    }
};
} // namespace youknow106

namespace
{
using youknow106::Chorus;
using youknow106::YouKnow106TestAccess;
namespace quality = youknow106::oversampling_quality;

constexpr double pi = 3.14159265358979323846;
constexpr std::array<double, 2> hostRates { 44100.0, 48000.0 };
constexpr std::array<int, 3> factors { 1, 2, 4 };
constexpr std::array<double, 3> drivenFeedbackCases { 0.0, 2.0, 3.6 };
constexpr std::size_t oracleFactor = 16u;
constexpr std::size_t oracleFilterTaps = 4097u;
constexpr double auditPassbandHz = 20000.0;

constexpr double vcfRkRelativeRmsGate = 0.01;       // -40 dB
constexpr double vcfReferenceConvergenceGate = 1.0e-4; // -80 dB
constexpr double vcfOscillationPitchConvergenceGateCents = 0.01;
constexpr double vcfOscillationLevelConvergenceGateDb = 0.001;
constexpr double vcfOscillationPitchGateCents = 1.0;
constexpr double vcfOscillationLevelGateDb = 0.10;
constexpr double vcfHotRelativeRmsGate = 0.01; // -40 dB
constexpr double vcfHotReferenceConvergenceGate = 1.0e-4; // -80 dB
constexpr double vcfHotResidualOffMaskGateDb = -60.0;
constexpr double vcfHotOracleOffMaskGateDb = -85.0;
constexpr double bbdAnalyticRelativeRmsGate = 0.01; // -40 dB
constexpr double bbdLinearizationRelativeGate = 1.0e-6;
constexpr double bbdOracleProjectionGainGateDb = 0.025;
constexpr double bbdOracleOffMaskGateDb = -85.0;
constexpr double bbdOracleImageTailGateDb = -120.0;
constexpr double bbdPhysicalImageGateDb = 0.75;
constexpr double bbdSgaGateDb = -60.0;
constexpr double bbdPhaseGate = 5.0e-10;

double decibels(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-30));
}

template <typename Sample>
double rootMeanSquare(std::span<const Sample> signal)
{
    if (signal.empty())
        throw std::runtime_error("invalid RMS interval");
    long double squared = 0.0;
    for (const auto sample : signal)
        squared += static_cast<long double>(sample) * sample;
    return std::sqrt(static_cast<double>(
        squared / static_cast<long double>(signal.size())));
}

template <typename Sample>
bool allFiniteSamples(std::span<const Sample> signal)
{
    return std::all_of(signal.begin(), signal.end(), [](const auto sample) {
        return std::isfinite(static_cast<double>(sample));
    });
}

template <typename Sample, typename Allocator>
bool allFiniteSamples(const std::vector<Sample, Allocator>& signal)
{
    return allFiniteSamples(
        std::span<const Sample>(signal.data(), signal.size()));
}

bool finiteProjection(const quality::ToneProjection& projection)
{
    return std::isfinite(projection.complexAmplitude.real())
        && std::isfinite(projection.complexAmplitude.imag())
        && std::isfinite(projection.amplitude)
        && std::isfinite(projection.rms)
        && std::isfinite(projection.phaseRadians);
}

bool finiteComplex(std::complex<double> value)
{
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

bool finiteComparison(const quality::RmsComparison& comparison)
{
    return std::isfinite(comparison.referenceRms)
        && std::isfinite(comparison.candidateRms)
        && std::isfinite(comparison.errorRms)
        && std::isfinite(comparison.relativeError)
        && std::isfinite(comparison.relativeErrorDb);
}

// A four-term Blackman-Harris line's first null is within four bins of its
// centre even when the tone lies between bins. Six bins masks that complete
// main lobe plus explicit margin; the exhaustive oracle-only control below
// proves the remaining leakage stays beneath its -85 dBc gate.
constexpr std::size_t spectrumMaskHalfWidthBins = 6u;

struct BlackmanHarrisSpectrum
{
    std::vector<double> amplitudes;
    double binWidthHz {};
    std::size_t frameCount {};
    bool allFinite {};
};

BlackmanHarrisSpectrum blackmanHarrisSpectrum(
    std::span<const double> signal, double sampleRate)
{
    if (signal.empty() || (signal.size() & (signal.size() - 1u)) != 0u
        || !std::isfinite(sampleRate) || sampleRate <= 0.0)
        throw std::runtime_error(
            "quality spectrum requires finite-rate power-of-two audio");

    std::vector<std::complex<double>> bins(signal.size());
    long double windowSum = 0.0L;
    for (std::size_t frame = 0u; frame < signal.size(); ++frame)
    {
        const double window = quality::projectionWindowValue(
            quality::ProjectionWindow::BlackmanHarris92Db,
            frame, signal.size());
        bins[frame] = signal[frame] * window;
        windowSum += static_cast<long double>(window);
    }
    if (!(windowSum > 0.0L) || !allFiniteSamples(signal))
        throw std::runtime_error("quality spectrum input/window is invalid");

    for (std::size_t index = 1u, reverse = 0u;
         index < bins.size(); ++index)
    {
        std::size_t bit = bins.size() >> 1u;
        while ((reverse & bit) != 0u)
        {
            reverse ^= bit;
            bit >>= 1u;
        }
        reverse ^= bit;
        if (index < reverse)
            std::swap(bins[index], bins[reverse]);
    }
    for (std::size_t length = 2u; length <= bins.size(); length <<= 1u)
    {
        const auto step = std::polar(
            1.0, -2.0 * pi / static_cast<double>(length));
        for (std::size_t base = 0u; base < bins.size(); base += length)
        {
            std::complex<double> twiddle(1.0, 0.0);
            const std::size_t half = length >> 1u;
            for (std::size_t offset = 0u; offset < half; ++offset)
            {
                const auto even = bins[base + offset];
                const auto odd = bins[base + offset + half] * twiddle;
                bins[base + offset] = even + odd;
                bins[base + offset + half] = even - odd;
                twiddle *= step;
            }
        }
    }

    BlackmanHarrisSpectrum result;
    result.frameCount = signal.size();
    result.binWidthHz = sampleRate / static_cast<double>(signal.size());
    result.amplitudes.resize(signal.size() / 2u + 1u);
    const double twoSidedScale = 2.0 / static_cast<double>(windowSum);
    for (std::size_t bin = 0u; bin < result.amplitudes.size(); ++bin)
    {
        const bool edge = bin == 0u || 2u * bin == signal.size();
        result.amplitudes[bin] = std::abs(bins[bin])
            * (edge ? 0.5 * twoSidedScale : twoSidedScale);
    }
    result.allFinite = std::isfinite(result.binWidthHz)
        && allFiniteSamples(result.amplitudes);
    return result;
}

bool spectrumBinIsMasked(double frequencyHz,
                         std::span<const double> referenceLines,
                         double binWidthHz)
{
    const double maskHz = static_cast<double>(spectrumMaskHalfWidthBins)
                        * binWidthHz;
    return std::any_of(
        referenceLines.begin(), referenceLines.end(),
        [=](double lineHz) {
            return std::abs(frequencyHz - lineHz) <= maskHz;
        });
}

template <typename Sample>
double risingCrossingFrequency(std::span<const Sample> signal,
                               double sampleRate)
{
    if (signal.size() < 3u)
        throw std::runtime_error("invalid crossing interval");
    std::vector<double> crossings;
    for (std::size_t index = 1u; index < signal.size(); ++index)
    {
        const double before = signal[index - 1u];
        const double after = signal[index];
        if (before <= 0.0 && after > 0.0)
        {
            const double fraction = -before / (after - before);
            crossings.push_back(static_cast<double>(index - 1u) + fraction);
        }
    }
    if (crossings.size() < 3u)
        throw std::runtime_error("self-oscillation produced too few crossings");
    return static_cast<double>(crossings.size() - 1u) * sampleRate
         / (crossings.back() - crossings.front());
}

quality::KaiserLowPass makeOracleFilter(double hostRate)
{
    return quality::designKaiserLowPass({
        hostRate * static_cast<double>(oracleFactor),
        20000.0,
        0.5 * hostRate,
        quality::referenceFilterAttenuationDb,
        oracleFilterTaps
    });
}

template <typename Sample>
std::vector<Sample> oracleHostEndpointPhase(
    const std::vector<Sample>& highRate)
{
    // Each model call returns the state at its interval endpoint.  Host frame
    // n therefore corresponds to 16x source index 16*n+15, not 16*n.  Shift
    // that fixed phase before the zero-phase independent FIR so its published
    // integer host timeline matches the shipping block endpoint.
    constexpr std::size_t phase = oracleFactor - 1u;
    if (highRate.size() <= phase)
        throw std::runtime_error("oracle render is too short to phase-align");
    return { highRate.begin() + static_cast<std::ptrdiff_t>(phase),
             highRate.end() };
}

struct AlignedInterval
{
    std::int64_t firstHostFrame {};
    std::size_t frameCount {};
    std::span<const double> reference;
    std::span<const double> candidate;
};

AlignedInterval alignedInterval(
    const quality::HostAlignedSignal& reference,
    const quality::FractionalDelayAlignedSignal& candidate,
    std::int64_t wantedFirst, std::int64_t wantedEnd)
{
    const auto first = std::max({ wantedFirst, reference.firstHostFrame,
                                  candidate.firstHostFrame });
    const auto end = std::min({ wantedEnd,
                                reference.endHostFrameExclusive(),
                                candidate.endHostFrameExclusive() });
    if (end <= first)
        throw std::runtime_error("aligned audit interval is empty");
    const auto count = static_cast<std::size_t>(end - first);
    return { first, count,
             quality::hostFrameSpan(reference, first, count),
             quality::hostFrameSpan(candidate, first, count) };
}

quality::RmsComparison compareInterval(
    const quality::HostAlignedSignal& reference,
    const quality::FractionalDelayAlignedSignal& candidate,
    std::int64_t wantedFirst, std::int64_t wantedEnd)
{
    const auto interval = alignedInterval(
        reference, candidate, wantedFirst, wantedEnd);
    return quality::compareRms(interval.reference, interval.candidate);
}

// Independent double-precision RK4 integration of the declared four-stage
// continuous-time ODE.  It calls no production cascade helper and consumes the
// piecewise-linear path between the same endpoint samples as the production
// input boundary at each RK abscissa.
class ReferenceCascade
{
public:
    ReferenceCascade(double cutoffHz, double feedback)
        : omega_(2.0 * pi * cutoffHz), feedback_(feedback)
    {
    }

    template <typename Input>
    double advance(double startSeconds, double intervalSeconds, int subdivisions,
                   Input&& input)
    {
        const double step = intervalSeconds / subdivisions;
        for (int sub = 0; sub < subdivisions; ++sub)
        {
            const double time = startSeconds + static_cast<double>(sub) * step;
            const auto k1 = derivative(state_, input(time));
            const auto k2 = derivative(add(state_, k1, 0.5 * step),
                                       input(time + 0.5 * step));
            const auto k3 = derivative(add(state_, k2, 0.5 * step),
                                       input(time + 0.5 * step));
            const auto k4 = derivative(add(state_, k3, step),
                                       input(time + step));
            for (std::size_t stage = 0; stage < state_.size(); ++stage)
                state_[stage] += step * (k1[stage] + 2.0 * k2[stage]
                                      + 2.0 * k3[stage] + k4[stage]) / 6.0;
        }
        return state_[3];
    }

private:
    static std::array<double, 4> add(const std::array<double, 4>& state,
                                     const std::array<double, 4>& slope,
                                     double scale)
    {
        std::array<double, 4> result {};
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = state[stage] + scale * slope[stage];
        return result;
    }

    std::array<double, 4> derivative(const std::array<double, 4>& state,
                                     double input) const
    {
        const double headroom = YouKnow106TestAccess::otaHeadroom();
        const double feedbackHeadroom =
            YouKnow106TestAccess::feedbackHeadroom();
        std::array<double, 4> slope {};
        double previous = input - feedback_ * feedbackHeadroom
            * std::tanh(state[3] / feedbackHeadroom);
        for (std::size_t stage = 0; stage < slope.size(); ++stage)
        {
            slope[stage] = omega_ * headroom
                         * std::tanh((previous - state[stage]) / headroom);
            previous = state[stage];
        }
        return slope;
    }

    std::array<double, 4> state_ {};
    double omega_ {};
    double feedback_ {};
};

constexpr double vcfToneHz = 220.0;
constexpr double vcfToneAmplitude = 3.0;
constexpr double vcfCutoffHz = 1500.0;
constexpr std::size_t vcfHostFrames = 4096u;
constexpr std::size_t vcfWarmupHostFrames = 1024u;
constexpr double vcfRingCutoffHz = 500.0;
constexpr float vcfRingFeedback = 4.4f;
constexpr std::size_t vcfRingWarmupHostFrames = 8192u;
constexpr std::size_t vcfRingHostFrames = 16384u;
constexpr double vcfHotFundamentalHz = 1046.502;
constexpr double vcfHotAmplitude = 2.4;
constexpr double vcfHotCutoffHz = 16000.0;
constexpr float vcfHotFeedback = 3.8f;
constexpr std::size_t vcfHotWarmupHostFrames = 8192u;
constexpr std::size_t vcfHotCaptureHostFrames = 32768u;
constexpr std::size_t vcfHotMeasurementEndHostFrame =
    vcfHotWarmupHostFrames + vcfHotCaptureHostFrames;
constexpr std::size_t vcfHotRenderTailHostFrames = 256u;
constexpr std::size_t vcfHotHostFrames =
    vcfHotMeasurementEndHostFrame + vcfHotRenderTailHostFrames;

double vcfHotDriveAmplitude()
{
    return vcfHotAmplitude
         * YouKnow106TestAccess::inputCompensation(vcfHotFeedback);
}

struct VcfDrivenOracle
{
    double feedback {};
    quality::HostAlignedSignal rk64;
    quality::HostAlignedSignal rk128;
};

struct VcfOracle
{
    quality::KaiserLowPass filter;
    quality::ReferenceFilterCheck filterCheck;
    std::array<VcfDrivenOracle, drivenFeedbackCases.size()> driven;
    quality::HostAlignedSignal ringRk64;
    quality::HostAlignedSignal ringRk128;
    quality::HostAlignedSignal hotRk64;
    quality::HostAlignedSignal hotRk128;
    double worstReferenceConvergence {};
    double hotReferenceConvergence {};
    double ringPitchConvergenceCents {};
    double ringLevelConvergenceDb {};
    std::size_t validDrivenComparisons {};
    bool ringTakeValid {};
    bool hotTakeValid {};
    bool allFinite { true };

    [[nodiscard]] bool valid() const noexcept
    {
        return allFinite && filterCheck.passed()
            && validDrivenComparisons == drivenFeedbackCases.size()
            && ringTakeValid && hotTakeValid;
    }
};

std::vector<double> renderDrivenRkGrid(double hostRate, double feedback,
                                       int substeps)
{
    const double rate = hostRate * static_cast<double>(oracleFactor);
    const double interval = 1.0 / rate;
    const std::size_t frames = vcfHostFrames * oracleFactor;
    const auto inputAt = [](double time) {
        return vcfToneAmplitude * std::sin(2.0 * pi * vcfToneHz * time);
    };
    ReferenceCascade reference(vcfCutoffHz, feedback);
    std::vector<double> output(frames);
    for (std::size_t index = 0u; index < frames; ++index)
    {
        const double start = static_cast<double>(index) * interval;
        output[index] = reference.advance(
            start, interval, substeps, inputAt);
    }
    return output;
}

std::vector<double> renderRingRkGrid(double hostRate, int substeps)
{
    const double rate = hostRate * static_cast<double>(oracleFactor);
    const double interval = 1.0 / rate;
    const std::size_t frames = vcfRingHostFrames * oracleFactor;
    ReferenceCascade reference(vcfRingCutoffHz, vcfRingFeedback);
    std::vector<double> output(frames);
    double previousInput = 0.0;
    for (std::size_t index = 0u; index < frames; ++index)
    {
        const double targetInput = index == 0u ? 0.5 : 0.0;
        const double start = static_cast<double>(index) * interval;
        const auto inputAt = [=](double time) {
            const double fraction = std::clamp(
                (time - start) / interval, 0.0, 1.0);
            return previousInput
                 + fraction * (targetInput - previousInput);
        };
        output[index] = reference.advance(
            start, interval, substeps, inputAt);
        previousInput = targetInput;
    }
    return output;
}

std::vector<double> renderHotRkGrid(double hostRate, int substeps)
{
    const double rate = hostRate * static_cast<double>(oracleFactor);
    const double interval = 1.0 / rate;
    const std::size_t frames = vcfHotHostFrames * oracleFactor;
    const int inputHarmonics = static_cast<int>(
        auditPassbandHz / vcfHotFundamentalHz);
    const auto inputAt = [](double time) {
        const double phase = 2.0 * pi * vcfHotFundamentalHz * time;
        double sum = 0.0;
        for (int harmonic = 1; harmonic <= inputHarmonics; ++harmonic)
            sum += std::sin(static_cast<double>(harmonic) * phase) / harmonic;
        return vcfHotDriveAmplitude() * 2.0 * sum / pi;
    };
    ReferenceCascade reference(vcfHotCutoffHz, vcfHotFeedback);
    std::vector<double> output(frames);
    for (std::size_t index = 0u; index < frames; ++index)
    {
        const double start = static_cast<double>(index) * interval;
        output[index] = reference.advance(
            start, interval, substeps, inputAt);
    }
    return output;
}

std::span<const double> oracleInterval(
    const quality::HostAlignedSignal& signal,
    std::int64_t wantedFirst, std::int64_t wantedEnd)
{
    const auto first = std::max(wantedFirst, signal.firstHostFrame);
    const auto end = std::min(wantedEnd, signal.endHostFrameExclusive());
    if (end <= first)
        throw std::runtime_error("oracle interval is empty");
    return quality::hostFrameSpan(
        signal, first, static_cast<std::size_t>(end - first));
}

VcfOracle buildVcfOracle(double hostRate)
{
    VcfOracle oracle;
    oracle.filter = makeOracleFilter(hostRate);
    oracle.filterCheck = quality::checkReferenceFilter(oracle.filter);
    for (std::size_t index = 0u; index < drivenFeedbackCases.size(); ++index)
    {
        const double feedback = drivenFeedbackCases[index];
        oracle.driven[index].feedback = feedback;
        const auto rk64 = oracleHostEndpointPhase(
            renderDrivenRkGrid(hostRate, feedback, 4));
        const auto rk128 = oracleHostEndpointPhase(
            renderDrivenRkGrid(hostRate, feedback, 8));
        oracle.driven[index].rk64 = quality::decimateToHostBoundary(
            rk64, oracleFactor, oracle.filter);
        oracle.driven[index].rk128 = quality::decimateToHostBoundary(
            rk128, oracleFactor, oracle.filter);
        const auto first = std::max<std::int64_t>({
            static_cast<std::int64_t>(vcfWarmupHostFrames),
            oracle.driven[index].rk64.firstHostFrame,
            oracle.driven[index].rk128.firstHostFrame });
        const auto end = std::min(
            oracle.driven[index].rk64.endHostFrameExclusive(),
            oracle.driven[index].rk128.endHostFrameExclusive());
        const auto count = static_cast<std::size_t>(end - first);
        const auto convergence = quality::compareRms(
            quality::hostFrameSpan(oracle.driven[index].rk128, first, count),
            quality::hostFrameSpan(oracle.driven[index].rk64, first, count));
        const bool takeFinite = allFiniteSamples(rk64)
            && allFiniteSamples(rk128)
            && allFiniteSamples(oracle.driven[index].rk64.samples)
            && allFiniteSamples(oracle.driven[index].rk128.samples)
            && finiteComparison(convergence);
        oracle.allFinite = oracle.allFinite && takeFinite;
        if (takeFinite)
        {
            oracle.worstReferenceConvergence = std::max(
                oracle.worstReferenceConvergence, convergence.relativeError);
            ++oracle.validDrivenComparisons;
        }
    }

    const auto ring64 = oracleHostEndpointPhase(
        renderRingRkGrid(hostRate, 4));
    const auto ring128 = oracleHostEndpointPhase(
        renderRingRkGrid(hostRate, 8));
    oracle.ringRk64 = quality::decimateToHostBoundary(
        ring64, oracleFactor, oracle.filter);
    oracle.ringRk128 = quality::decimateToHostBoundary(
        ring128, oracleFactor, oracle.filter);
    const auto ring64Span = oracleInterval(
        oracle.ringRk64, vcfRingWarmupHostFrames, vcfRingHostFrames);
    const auto ring128Span = oracleInterval(
        oracle.ringRk128, vcfRingWarmupHostFrames, vcfRingHostFrames);
    const double ring64Frequency = risingCrossingFrequency(
        ring64Span, hostRate);
    const double ring128Frequency = risingCrossingFrequency(
        ring128Span, hostRate);
    oracle.ringPitchConvergenceCents = std::abs(
        1200.0 * std::log2(ring64Frequency / ring128Frequency));
    oracle.ringLevelConvergenceDb = std::abs(decibels(
        rootMeanSquare(ring64Span) / rootMeanSquare(ring128Span)));
    oracle.ringTakeValid = allFiniteSamples(ring64)
        && allFiniteSamples(ring128)
        && allFiniteSamples(oracle.ringRk64.samples)
        && allFiniteSamples(oracle.ringRk128.samples)
        && std::isfinite(ring64Frequency)
        && std::isfinite(ring128Frequency)
        && std::isfinite(oracle.ringPitchConvergenceCents)
        && std::isfinite(oracle.ringLevelConvergenceDb);
    oracle.allFinite = oracle.allFinite && oracle.ringTakeValid;

    const auto hot64 = oracleHostEndpointPhase(
        renderHotRkGrid(hostRate, 4));
    const auto hot128 = oracleHostEndpointPhase(
        renderHotRkGrid(hostRate, 8));
    oracle.hotRk64 = quality::decimateToHostBoundary(
        hot64, oracleFactor, oracle.filter);
    oracle.hotRk128 = quality::decimateToHostBoundary(
        hot128, oracleFactor, oracle.filter);
    const auto hotFirst = std::max<std::int64_t>({
        static_cast<std::int64_t>(vcfHotWarmupHostFrames),
        oracle.hotRk64.firstHostFrame, oracle.hotRk128.firstHostFrame });
    const auto hotEnd = std::min(
        oracle.hotRk64.endHostFrameExclusive(),
        oracle.hotRk128.endHostFrameExclusive());
    const auto hotCount = static_cast<std::size_t>(hotEnd - hotFirst);
    const auto hotConvergence = quality::compareRms(
        quality::hostFrameSpan(oracle.hotRk128, hotFirst, hotCount),
        quality::hostFrameSpan(oracle.hotRk64, hotFirst, hotCount));
    oracle.hotReferenceConvergence = hotConvergence.relativeError;
    oracle.hotTakeValid = allFiniteSamples(hot64)
        && allFiniteSamples(hot128)
        && allFiniteSamples(oracle.hotRk64.samples)
        && allFiniteSamples(oracle.hotRk128.samples)
        && finiteComparison(hotConvergence);
    oracle.allFinite = oracle.allFinite && oracle.hotTakeValid;
    return oracle;
}

struct VcfMetrics
{
    double worstRkRelativeRms {};
    double worstReferenceConvergence {};
    double oscillationReferencePitchConvergenceCents {};
    double oscillationReferenceLevelConvergenceDb {};
    double oscillationPitchErrorCents {};
    double oscillationLevelErrorDb {};
    double hotRkRelativeRms {};
    double hotReferenceConvergence {};
    double worstHotResidualOffMaskDb { -300.0 };
    double hotOracleOffMaskDb { -300.0 };
    double hotFundamentalAmplitude {};
    std::size_t validHotResidualBins {};
    std::size_t validHotOracleBins {};
    std::size_t hotSpectrumFrames {};
    double appliedDelayHostFrames {};
    std::size_t validDrivenTakes {};
    bool oracleFilterPassed {};
    bool oracleFinite {};
    bool ringTakeValid {};
    bool hotTakeValid {};
    bool allFinite { true };
    bool pass {};
};

VcfMetrics auditVcf(double hostRate, int factor, const VcfOracle& oracle)
{
    const double internalRate = hostRate * factor;
    const std::size_t internalFrames = vcfHostFrames
                                     * static_cast<std::size_t>(factor);
    const auto tone = [](double time) {
        return vcfToneAmplitude * std::sin(2.0 * pi * vcfToneHz * time);
    };
    const double delay =
        YouKnow106TestAccess::shippingDecimatorBoundaryDelayHostFrames(factor);

    VcfMetrics metrics;
    metrics.worstReferenceConvergence = oracle.worstReferenceConvergence;
    metrics.oscillationReferencePitchConvergenceCents =
        oracle.ringPitchConvergenceCents;
    metrics.oscillationReferenceLevelConvergenceDb =
        oracle.ringLevelConvergenceDb;
    metrics.hotReferenceConvergence = oracle.hotReferenceConvergence;
    metrics.oracleFilterPassed = oracle.filterCheck.passed();
    metrics.oracleFinite = oracle.valid();
    metrics.appliedDelayHostFrames = delay;
    for (const auto& reference : oracle.driven)
    {
        YouKnow106TestAccess::Cascade production;
        production.reset();
        std::vector<float> actualInternal(internalFrames);
        const float g = static_cast<float>(
            std::tan(pi * vcfCutoffHz / internalRate));
        for (std::size_t index = 0u; index < internalFrames; ++index)
        {
            const double time = static_cast<double>(index + 1u) / internalRate;
            actualInternal[index] = production.process(
                static_cast<float>(tone(time)), g,
                static_cast<float>(reference.feedback),
                YouKnow106TestAccess::otaHeadroom(), false, 0.0f);
        }
        const auto boundary = YouKnow106TestAccess::decimate(
            actualInternal, factor);
        const auto aligned = quality::compensateFractionalDelay(
            boundary, hostRate, delay);
        const auto comparison = compareInterval(
            reference.rk128, aligned, vcfWarmupHostFrames, vcfHostFrames);
        const bool takeFinite = allFiniteSamples(actualInternal)
            && allFiniteSamples(boundary)
            && allFiniteSamples(aligned.samples)
            && finiteComparison(comparison);
        metrics.allFinite = metrics.allFinite && takeFinite;
        if (takeFinite)
        {
            metrics.worstRkRelativeRms = std::max(
                metrics.worstRkRelativeRms, comparison.relativeError);
            ++metrics.validDrivenTakes;
        }
    }

    // Above k=4, waveform subtraction turns a tiny rate error into arbitrary
    // phase error.  Compare the independently converged limit cycle's pitch
    // and RMS level instead.
    const std::size_t ringInternalFrames = vcfRingHostFrames
        * static_cast<std::size_t>(factor);
    YouKnow106TestAccess::Cascade ringProduction;
    ringProduction.reset();
    std::vector<float> ringInternal(ringInternalFrames);
    const float ringG = static_cast<float>(
        std::tan(pi * vcfRingCutoffHz / internalRate));
    for (std::size_t index = 0u; index < ringInternalFrames; ++index)
    {
        const float input = index == 0u ? 0.5f : 0.0f;
        ringInternal[index] = ringProduction.process(
            input, ringG, vcfRingFeedback,
            YouKnow106TestAccess::otaHeadroom(), false, 0.0f);
    }
    const auto ringBoundary = YouKnow106TestAccess::decimate(
        ringInternal, factor);
    const auto ringAligned = quality::compensateFractionalDelay(
        ringBoundary, hostRate, delay);
    const auto ringInterval = alignedInterval(
        oracle.ringRk128, ringAligned,
        vcfRingWarmupHostFrames, vcfRingHostFrames);
    const double actualFrequency = risingCrossingFrequency(
        ringInterval.candidate, hostRate);
    const double expectedFrequency = risingCrossingFrequency(
        ringInterval.reference, hostRate);
    metrics.oscillationPitchErrorCents = std::abs(
        1200.0 * std::log2(actualFrequency / expectedFrequency));
    metrics.oscillationLevelErrorDb = std::abs(decibels(
        rootMeanSquare(ringInterval.candidate)
        / rootMeanSquare(ringInterval.reference)));
    metrics.ringTakeValid = allFiniteSamples(ringInternal)
        && allFiniteSamples(ringBoundary)
        && allFiniteSamples(ringAligned.samples)
        && allFiniteSamples(ringInterval.reference)
        && allFiniteSamples(ringInterval.candidate)
        && std::isfinite(actualFrequency)
        && std::isfinite(expectedFrequency)
        && std::isfinite(metrics.oscillationPitchErrorCents)
        && std::isfinite(metrics.oscillationLevelErrorDb);
    metrics.allFinite = metrics.allFinite && metrics.ringTakeValid;

    // A converged, factor-independent hot-saw RK render makes every candidate
    // answer at the same host boundary.  The residual therefore includes both
    // pre-grid nonlinear foldback and shipping-boundary stopband leakage without
    // assigning either to one guessed image family.
    const int inputHarmonics = static_cast<int>(
        auditPassbandHz / vcfHotFundamentalHz);
    YouKnow106TestAccess::Cascade hotCascade;
    hotCascade.reset();
    std::vector<float> hotInternal(
        vcfHotHostFrames * static_cast<std::size_t>(factor));
    const float hotG = static_cast<float>(
        std::tan(pi * vcfHotCutoffHz / internalRate));
    for (std::size_t index = 0; index < hotInternal.size(); ++index)
    {
        const double time = static_cast<double>(index + 1u) / internalRate;
        const double phase = 2.0 * pi * vcfHotFundamentalHz * time;
        double sum = 0.0;
        for (int harmonic = 1; harmonic <= inputHarmonics; ++harmonic)
            sum += std::sin(static_cast<double>(harmonic) * phase) / harmonic;
        const float input = static_cast<float>(vcfHotDriveAmplitude())
                          * static_cast<float>(2.0 * sum / pi);
        hotInternal[index] = hotCascade.process(
            input, hotG, vcfHotFeedback,
            YouKnow106TestAccess::otaHeadroom(), false, 0.0f);
    }
    const auto hotBoundary = YouKnow106TestAccess::decimate(
        hotInternal, factor);
    const auto hotAligned = quality::compensateFractionalDelay(
        hotBoundary, hostRate, delay);
    const auto hotInterval = alignedInterval(
        oracle.hotRk128, hotAligned,
        vcfHotWarmupHostFrames, vcfHotMeasurementEndHostFrame);
    const auto hotComparison = quality::compareRms(
        hotInterval.reference, hotInterval.candidate);
    metrics.hotRkRelativeRms = hotComparison.relativeError;
    const auto fundamentalProjection = quality::projectTone(
        hotInterval.reference, hostRate, vcfHotFundamentalHz,
        quality::ProjectionWindow::BlackmanHarris92Db,
        hotInterval.firstHostFrame);
    metrics.hotFundamentalAmplitude = fundamentalProjection.amplitude;
    metrics.hotTakeValid = allFiniteSamples(hotInternal)
        && allFiniteSamples(hotBoundary)
        && allFiniteSamples(hotAligned.samples)
        && allFiniteSamples(hotInterval.reference)
        && allFiniteSamples(hotInterval.candidate)
        && finiteComparison(hotComparison)
        && finiteProjection(fundamentalProjection)
        && metrics.hotFundamentalAmplitude > 1.0e-8;
    metrics.allFinite = metrics.allFinite && metrics.hotTakeValid;
    std::vector<double> protectedFrequencies;
    for (int harmonic = 1; harmonic <= inputHarmonics; ++harmonic)
        protectedFrequencies.push_back(
            harmonic * vcfHotFundamentalHz);
    std::vector<double> residual(hotInterval.frameCount);
    for (std::size_t index = 0u; index < residual.size(); ++index)
        residual[index] = hotInterval.candidate[index]
                        - hotInterval.reference[index];
    metrics.allFinite = metrics.allFinite && allFiniteSamples(residual);
    const auto referenceSpectrum = blackmanHarrisSpectrum(
        hotInterval.reference, hostRate);
    const auto residualSpectrum = blackmanHarrisSpectrum(residual, hostRate);
    const bool spectraFinite = referenceSpectrum.allFinite
        && residualSpectrum.allFinite
        && referenceSpectrum.frameCount == vcfHotCaptureHostFrames
        && residualSpectrum.frameCount == vcfHotCaptureHostFrames
        && referenceSpectrum.frameCount == residualSpectrum.frameCount
        && referenceSpectrum.binWidthHz == residualSpectrum.binWidthHz;
    metrics.allFinite = metrics.allFinite && spectraFinite;
    if (spectraFinite)
    {
        metrics.hotSpectrumFrames = residualSpectrum.frameCount;
        for (std::size_t bin = 1u;
             bin < residualSpectrum.amplitudes.size(); ++bin)
        {
            const double frequencyHz = residualSpectrum.binWidthHz
                                     * static_cast<double>(bin);
            if (frequencyHz < 20.0 || frequencyHz > auditPassbandHz
                || spectrumBinIsMasked(
                    frequencyHz, protectedFrequencies,
                    residualSpectrum.binWidthHz))
                continue;
            metrics.worstHotResidualOffMaskDb = std::max(
                metrics.worstHotResidualOffMaskDb,
                decibels(residualSpectrum.amplitudes[bin]
                    / metrics.hotFundamentalAmplitude));
            metrics.hotOracleOffMaskDb = std::max(
                metrics.hotOracleOffMaskDb,
                decibels(referenceSpectrum.amplitudes[bin]
                    / metrics.hotFundamentalAmplitude));
            ++metrics.validHotResidualBins;
            ++metrics.validHotOracleBins;
        }
    }

    metrics.pass = std::isfinite(metrics.worstRkRelativeRms)
        && std::isfinite(metrics.worstReferenceConvergence)
        && metrics.oracleFilterPassed
        && metrics.oracleFinite
        && metrics.allFinite
        && metrics.validDrivenTakes == drivenFeedbackCases.size()
        && metrics.ringTakeValid
        && metrics.hotTakeValid
        && metrics.worstRkRelativeRms <= vcfRkRelativeRmsGate
        && metrics.worstReferenceConvergence
               <= vcfReferenceConvergenceGate
        && metrics.oscillationReferencePitchConvergenceCents
               <= vcfOscillationPitchConvergenceGateCents
        && metrics.oscillationReferenceLevelConvergenceDb
               <= vcfOscillationLevelConvergenceGateDb
        && metrics.oscillationPitchErrorCents
               <= vcfOscillationPitchGateCents
        && metrics.oscillationLevelErrorDb <= vcfOscillationLevelGateDb
        && std::isfinite(metrics.hotRkRelativeRms)
        && std::isfinite(metrics.hotReferenceConvergence)
        && std::isfinite(metrics.worstHotResidualOffMaskDb)
        && std::isfinite(metrics.hotOracleOffMaskDb)
        && std::isfinite(metrics.hotFundamentalAmplitude)
        && metrics.hotFundamentalAmplitude > 1.0e-8
        && metrics.validHotResidualBins > 0u
        && metrics.validHotOracleBins == metrics.validHotResidualBins
        && metrics.hotSpectrumFrames == vcfHotCaptureHostFrames
        && metrics.hotRkRelativeRms <= vcfHotRelativeRmsGate
        && metrics.hotReferenceConvergence
               <= vcfHotReferenceConvergenceGate
        && metrics.hotOracleOffMaskDb <= vcfHotOracleOffMaskGateDb
        && metrics.worstHotResidualOffMaskDb
               < vcfHotResidualOffMaskGateDb;
    return metrics;
}

// The BBD oracle is closed form and contains no numerical support grid.  Its
// constants are an independent transcription of the documented component
// values and fitted per-transfer pole.  At this deliberately low drive, the
// production soft clip differs from its linear tangent by less than 1e-6.
constexpr float bbdAmplitude = 0.02f;
constexpr double bbdTransferSmear = 0.8654743;
constexpr double bbdSaturationLevel = 1.1246614;
constexpr double bbdSaturationExponent = 3.4541951;
constexpr double bbdInputCouplingHz = 15.9155;
constexpr double bbdInputPassiveHz = 7234.316;
constexpr double bbdFirstSallenKeyHz = 9688.043;
constexpr double bbdFirstSallenKeyQ = 0.549063;
constexpr double bbdSecondSallenKeyHz = 10377.179;
constexpr double bbdSecondSallenKeyQ = 1.290994;
constexpr double bbdOutputTapHz = 23461.385;
constexpr double bbdOutputCouplingHz = 11.315211571801418;
constexpr double bbdOraclePassbandHz = auditPassbandHz;
constexpr int bbdMaximumImageOrder = 1024;
constexpr std::size_t bbdWarmupHostFrames = 8192u;
constexpr std::size_t bbdCaptureHostFrames = 4096u;
constexpr std::size_t bbdMeasurementEndHostFrame =
    bbdWarmupHostFrames + bbdCaptureHostFrames;
constexpr std::size_t bbdRenderTailHostFrames = 256u;
constexpr std::size_t bbdHostFrames =
    bbdMeasurementEndHostFrame + bbdRenderTailHostFrames;

static_assert(Chorus::cellPairs == 128);

std::complex<double> onePoleLowPass(double frequencyHz, double cutoffHz)
{
    const std::complex<double> s(0.0, 2.0 * pi * frequencyHz);
    const double omega = 2.0 * pi * cutoffHz;
    return omega / (s + omega);
}

std::complex<double> onePoleHighPass(double frequencyHz, double cutoffHz)
{
    const std::complex<double> s(0.0, 2.0 * pi * frequencyHz);
    const double omega = 2.0 * pi * cutoffHz;
    return s / (s + omega);
}

std::complex<double> sallenKeyLowPass(double frequencyHz, double cutoffHz,
                                      double q)
{
    const std::complex<double> s(0.0, 2.0 * pi * frequencyHz);
    const double omega = 2.0 * pi * cutoffHz;
    return (omega * omega)
         / (s * s + (omega / q) * s + omega * omega);
}

std::complex<double> bbdInputSupportResponse(double frequencyHz)
{
    return sallenKeyLowPass(
               frequencyHz, bbdFirstSallenKeyHz, bbdFirstSallenKeyQ)
         * sallenKeyLowPass(
               frequencyHz, bbdSecondSallenKeyHz, bbdSecondSallenKeyQ)
         * onePoleHighPass(frequencyHz, bbdInputCouplingHz)
         * onePoleLowPass(frequencyHz, bbdInputPassiveHz);
}

std::complex<double> bbdOutputSupportResponse(double frequencyHz)
{
    return onePoleLowPass(frequencyHz, bbdOutputTapHz)
         * sallenKeyLowPass(
               frequencyHz, bbdFirstSallenKeyHz, bbdFirstSallenKeyQ)
         * sallenKeyLowPass(
               frequencyHz, bbdSecondSallenKeyHz, bbdSecondSallenKeyQ)
         * onePoleHighPass(frequencyHz, bbdOutputCouplingHz);
}

double normalizedSinc(double value)
{
    if (std::abs(value) < 1.0e-12)
        return 1.0;
    return std::sin(pi * value) / (pi * value);
}

struct BbdCaseDefinition
{
    float clockHz {};
    double toneHz {};
    bool highTone {};
};

std::array<BbdCaseDefinition, 4> bbdCases(double hostRate)
{
    const double lowTone = hostRate * 137.0 / 4096.0;
    return {{ { 20000.0f, lowTone, false },
              { 50000.0f, lowTone, false },
              { 128.0f / 0.0014f, lowTone, false },
              { 50000.0f, 12000.0, true } }};
}

struct BbdOracleCase
{
    BbdCaseDefinition definition;
    quality::HostAlignedSignal output;
    std::vector<int> synthesizedImageOrders;
    std::vector<double> hostWantedReferenceFrequencies;
    int enumeratedImageOrder {};
    double remainingImageTailDb {};
    int writeIndex {};
    double clockPhase {};
    std::uint64_t edgeCount {};
};

struct BbdOracleCheck
{
    double hostRate {};
    double linearizationRelativeBound {};
    double maximumProjectionGainErrorDb {};
    double worstOffMaskSpurDb { -300.0 };
    double worstTerminalImageDb { -300.0 };
    double worstConjugacyError {};
    int maximumEnumeratedImageOrder {};
    int verifiedImageOrder {};
    std::size_t validSyntheticProjections {};
    std::size_t validOffMaskBins {};
    std::size_t validOffMaskFfts {};
    std::size_t offMaskFftFrames {};
    bool everyImageTailTerminated { true };
    bool independentFilterPassed {};
    bool allFinite { true };

    [[nodiscard]] bool passed() const noexcept
    {
        return std::isfinite(linearizationRelativeBound)
            && std::isfinite(maximumProjectionGainErrorDb)
            && std::isfinite(worstOffMaskSpurDb)
            && std::isfinite(worstTerminalImageDb)
            && std::isfinite(worstConjugacyError)
            && linearizationRelativeBound <= bbdLinearizationRelativeGate
            && maximumProjectionGainErrorDb
                   <= bbdOracleProjectionGainGateDb
            && worstOffMaskSpurDb < bbdOracleOffMaskGateDb
            && worstTerminalImageDb <= bbdOracleImageTailGateDb
            && worstConjugacyError <= 1.0e-12
            && validSyntheticProjections > 0u
            && validOffMaskBins > 0u
            && validOffMaskFfts == 4u
            && offMaskFftFrames == bbdCaptureHostFrames
            && verifiedImageOrder == bbdMaximumImageOrder
            && everyImageTailTerminated
            && independentFilterPassed
            && allFinite;
    }
};

struct BbdOracle
{
    std::array<BbdOracleCase, 4> cases;
    BbdOracleCheck check;
};

std::complex<double> bbdImagePhasor(
    const BbdCaseDefinition& definition, int imageOrder)
{
    const double toneHz = definition.toneHz;
    const double clockHz = definition.clockHz;
    const double omega = 2.0 * pi * toneHz / clockHz;
    const auto zDelay = std::exp(std::complex<double>(
        0.0, -omega * static_cast<double>(Chorus::cellPairs)));
    const auto transferLoss = bbdTransferSmear * zDelay
        / (1.0 - (1.0 - bbdTransferSmear)
             * std::exp(std::complex<double>(0.0, -omega)));
    const double imageRatio = toneHz / clockHz
                            + static_cast<double>(imageOrder);
    const auto hold = std::exp(std::complex<double>(
        0.0, -pi * imageRatio)) * normalizedSinc(imageRatio);
    const double imageHz = toneHz
                         + static_cast<double>(imageOrder) * clockHz;
    const std::complex<double> sinePhasor(
        0.0, -static_cast<double>(bbdAmplitude));
    return sinePhasor * bbdInputSupportResponse(toneHz)
         * transferLoss * hold * bbdOutputSupportResponse(imageHz);
}

double bbdObservedImagePhasorFrequency(double frequencyHz,
                                       double sourceRateHz)
{
    double folded = std::fmod(std::abs(frequencyHz), sourceRateHz);
    if (folded > 0.5 * sourceRateHz)
        folded = sourceRateHz - folded;
    return folded;
}

std::complex<double> bbdObservedImagePhasor(
    const BbdCaseDefinition& definition, int imageOrder,
    const quality::KaiserLowPass& filter)
{
    const double imageHz = definition.toneHz
                         + static_cast<double>(imageOrder)
                         * definition.clockHz;
    const double responseFrequency = bbdObservedImagePhasorFrequency(
        imageHz, filter.specification.sampleRateHz);
    return bbdImagePhasor(definition, imageOrder)
         * quality::zeroPhaseFrequencyResponse(filter, responseFrequency);
}

double bbdLinearizationRelativeBound(double driveAmplitude)
{
    const double normalised = driveAmplitude / bbdSaturationLevel;
    const double gain = 1.0 / std::pow(
        1.0 + std::pow(normalised, bbdSaturationExponent),
        1.0 / bbdSaturationExponent);
    return 1.0 - gain;
}

double bbdBeyondMaximumImageTailBound(
    const BbdCaseDefinition& definition,
    const quality::KaiserLowPass& filter,
    double observedFundamentalAmplitude)
{
    constexpr int firstOmittedOrder = bbdMaximumImageOrder + 1;
    const double ratio = definition.toneHz / definition.clockHz;
    const double minimumOrder = static_cast<double>(firstOmittedOrder) - ratio;
    const double minimumFrequency = minimumOrder * definition.clockHz;
    if (minimumFrequency <= std::max({ bbdOutputTapHz,
                                      bbdFirstSallenKeyHz,
                                      bbdSecondSallenKeyHz }))
        throw std::runtime_error("BBD image envelope starts below support poles");

    const double omega = 2.0 * pi * definition.toneHz
                       / definition.clockHz;
    const auto transferLoss = bbdTransferSmear
        * std::exp(std::complex<double>(
            0.0, -omega * static_cast<double>(Chorus::cellPairs)))
        / (1.0 - (1.0 - bbdTransferSmear)
             * std::exp(std::complex<double>(0.0, -omega)));
    const double baseMagnitude = static_cast<double>(bbdAmplitude)
        * std::abs(bbdInputSupportResponse(definition.toneHz))
        * std::abs(transferLoss);
    const auto skEnvelopeConstant = [minimumFrequency](double cutoffHz) {
        const double ratioAtStart = cutoffHz / minimumFrequency;
        return cutoffHz * cutoffHz
             / (1.0 - ratioAtStart * ratioAtStart);
    };
    const double frequencyPowerConstant = baseMagnitude
        * (definition.clockHz / pi) * bbdOutputTapHz
        * skEnvelopeConstant(bbdFirstSallenKeyHz)
        * skEnvelopeConstant(bbdSecondSallenKeyHz);
    double filterL1Norm = 0.0;
    for (double coefficient : filter.coefficients)
        filterL1Norm += std::abs(coefficient);
    const double dimensionlessConstant = frequencyPowerConstant * filterL1Norm
        / std::pow(static_cast<double>(definition.clockHz), 6.0);
    const double infinitePairSum = 2.0 * (
        1.0 / std::pow(minimumOrder, 6.0)
        + 1.0 / (5.0 * std::pow(minimumOrder, 5.0)));
    return dimensionlessConstant * infinitePairSum
         / observedFundamentalAmplitude;
}

struct HostPhasorGroup
{
    double frequencyHz {};
    std::complex<double> phasor {};
};

std::pair<double, std::complex<double>> hostCanonicalPhasor(
    double sourceFrequencyHz, std::complex<double> phasor, double hostRate)
{
    double remainder = std::remainder(sourceFrequencyHz, hostRate);
    if (remainder < 0.0)
    {
        remainder = -remainder;
        phasor = std::conj(phasor);
    }
    return { remainder, phasor };
}

BbdOracle buildBbdOracle(double hostRate,
                         const quality::KaiserLowPass& filter)
{
    BbdOracle oracle;
    oracle.check.hostRate = hostRate;
    oracle.check.verifiedImageOrder = bbdMaximumImageOrder;
    oracle.check.independentFilterPassed =
        quality::checkReferenceFilter(filter).passed();
    const auto definitions = bbdCases(hostRate);
    for (std::size_t caseIndex = 0u;
         caseIndex < definitions.size(); ++caseIndex)
    {
        const auto definition = definitions[caseIndex];
        auto& result = oracle.cases[caseIndex];
        result.definition = definition;
        const double limitedDrive = static_cast<double>(bbdAmplitude)
            * std::abs(bbdInputSupportResponse(definition.toneHz));
        const double linearizationBound =
            bbdLinearizationRelativeBound(limitedDrive);
        oracle.check.allFinite = oracle.check.allFinite
            && std::isfinite(limitedDrive)
            && std::isfinite(linearizationBound);
        if (std::isfinite(linearizationBound))
            oracle.check.linearizationRelativeBound = std::max(
                oracle.check.linearizationRelativeBound,
                linearizationBound);
        const auto physicalFundamental = bbdImagePhasor(definition, 0);
        const auto fundamental = bbdObservedImagePhasor(definition, 0, filter);
        const double physicalFundamentalAmplitude =
            std::abs(physicalFundamental);
        const double fundamentalAmplitude = std::abs(fundamental);
        oracle.check.allFinite = oracle.check.allFinite
            && finiteComplex(physicalFundamental)
            && finiteComplex(fundamental)
            && std::isfinite(physicalFundamentalAmplitude)
            && std::isfinite(fundamentalAmplitude);
        if (!(physicalFundamentalAmplitude > 0.0)
            || !(fundamentalAmplitude > 0.0))
            throw std::runtime_error("BBD analytic fundamental vanished");
        std::array<double, bbdMaximumImageOrder + 1> pairRelativeSums {};
        for (int image = 1; image <= bbdMaximumImageOrder; ++image)
        {
            for (int order : { -image, image })
            {
                const auto phasor = bbdObservedImagePhasor(
                    definition, order, filter);
                const double imageHz = definition.toneHz
                    + static_cast<double>(order) * definition.clockHz;
                pairRelativeSums[static_cast<std::size_t>(image)] +=
                    std::abs(phasor) / fundamentalAmplitude;
                const auto inputConjugacy = std::abs(
                    bbdInputSupportResponse(-definition.toneHz)
                    - std::conj(bbdInputSupportResponse(definition.toneHz)));
                const auto outputConjugacy = std::abs(
                    bbdOutputSupportResponse(-imageHz)
                    - std::conj(bbdOutputSupportResponse(imageHz)));
                const bool imageFinite = finiteComplex(phasor)
                    && std::isfinite(inputConjugacy)
                    && std::isfinite(outputConjugacy);
                oracle.check.allFinite =
                    oracle.check.allFinite && imageFinite;
                if (imageFinite)
                    oracle.check.worstConjugacyError = std::max({
                        oracle.check.worstConjugacyError,
                        inputConjugacy, outputConjugacy });
            }
        }
        const double beyondMaximum = bbdBeyondMaximumImageTailBound(
            definition, filter, fundamentalAmplitude);
        oracle.check.allFinite = oracle.check.allFinite
            && std::isfinite(beyondMaximum);
        double finiteTail = 0.0;
        int firstNegligibleOrder = 0;
        double remainingTail = std::numeric_limits<double>::infinity();
        for (int image = bbdMaximumImageOrder; image >= 1; --image)
        {
            finiteTail += pairRelativeSums[static_cast<std::size_t>(image)];
            const double candidateTail = finiteTail + beyondMaximum;
            if (decibels(candidateTail) <= bbdOracleImageTailGateDb)
            {
                firstNegligibleOrder = image;
                remainingTail = candidateTail;
            }
        }
        const bool tailTerminated = firstNegligibleOrder > 0;
        result.enumeratedImageOrder = firstNegligibleOrder - 1;
        result.remainingImageTailDb = decibels(remainingTail);
        for (int image = 0; image <= result.enumeratedImageOrder; ++image)
        {
            if (image == 0)
                result.synthesizedImageOrders.push_back(0);
            else
            {
                result.synthesizedImageOrders.push_back(-image);
                result.synthesizedImageOrders.push_back(image);
            }
        }
        oracle.check.worstTerminalImageDb = std::max(
            oracle.check.worstTerminalImageDb,
            result.remainingImageTailDb);
        oracle.check.maximumEnumeratedImageOrder = std::max(
            oracle.check.maximumEnumeratedImageOrder,
            result.enumeratedImageOrder);
        oracle.check.everyImageTailTerminated =
            oracle.check.everyImageTailTerminated && tailTerminated;
        if (!tailTerminated)
            throw std::runtime_error("BBD analytic image tail did not terminate");

        result.output.sourceSampleRateHz = hostRate;
        result.output.hostSampleRateHz = hostRate;
        result.output.decimationFactor = 1u;
        result.output.firstHostFrame = 0;
        result.output.samples.resize(bbdHostFrames);
        for (std::size_t frame = 0u; frame < bbdHostFrames; ++frame)
        {
            const double seconds = static_cast<double>(frame + 1u) / hostRate;
            double sample = 0.0;
            for (int order : result.synthesizedImageOrders)
            {
                const double imageHz = definition.toneHz
                    + static_cast<double>(order) * definition.clockHz;
                sample += std::real(bbdObservedImagePhasor(
                    definition, order, filter)
                    * std::exp(std::complex<double>(
                        0.0, 2.0 * pi * imageHz * seconds)));
            }
            result.output.samples[frame] = sample;
        }
        oracle.check.allFinite = oracle.check.allFinite
            && allFiniteSamples(result.output.samples);

        const long double elapsedEdges =
            static_cast<long double>(definition.clockHz)
            * static_cast<long double>(bbdHostFrames)
            / static_cast<long double>(hostRate);
        result.edgeCount = static_cast<std::uint64_t>(std::floor(elapsedEdges));
        result.writeIndex = static_cast<int>(
            result.edgeCount % static_cast<std::uint64_t>(Chorus::cellPairs));
        result.clockPhase = static_cast<double>(
            elapsedEdges - std::floor(elapsedEdges));

        const auto capture = std::span<const double>(
            result.output.samples.data() + bbdWarmupHostFrames,
            bbdCaptureHostFrames);
        std::vector<HostPhasorGroup> groups;
        std::vector<HostPhasorGroup> wantedGroups;
        for (int order : result.synthesizedImageOrders)
        {
            const double imageHz = definition.toneHz
                + static_cast<double>(order) * definition.clockHz;
            const auto canonical = hostCanonicalPhasor(
                imageHz, bbdObservedImagePhasor(definition, order, filter),
                hostRate);
            auto group = std::find_if(
                groups.begin(), groups.end(), [&](const auto& candidate) {
                    return std::abs(candidate.frequencyHz - canonical.first)
                         < 1.0e-9;
                });
            if (group == groups.end())
                groups.push_back({ canonical.first, canonical.second });
            else
                group->phasor += canonical.second;

            // Validate every synthesized group above, but protect only lines
            // whose physical source belongs to the declared <=20 kHz BGA
            // reference set.  Out-of-band sources remain visible after they
            // fold into the host-band SGA spectrum.
            if (std::abs(imageHz) <= bbdOraclePassbandHz)
            {
                auto wantedGroup = std::find_if(
                    wantedGroups.begin(), wantedGroups.end(),
                    [&](const auto& candidate) {
                        return std::abs(
                            candidate.frequencyHz - canonical.first) < 1.0e-9;
                    });
                if (wantedGroup == wantedGroups.end())
                    wantedGroups.push_back(
                        { canonical.first, canonical.second });
                else
                    wantedGroup->phasor += canonical.second;
            }
        }
        for (const auto& group : groups)
        {
            const double imageHz = group.frequencyHz;
            const auto projection = quality::projectTone(
                capture, hostRate, imageHz,
                quality::ProjectionWindow::BlackmanHarris92Db,
                static_cast<std::int64_t>(bbdWarmupHostFrames));
            const double expected = std::abs(group.phasor);
            if (expected > 1.0e-8 * fundamentalAmplitude
                && finiteProjection(projection)
                && std::isfinite(expected))
            {
                oracle.check.maximumProjectionGainErrorDb = std::max(
                    oracle.check.maximumProjectionGainErrorDb,
                    std::abs(decibels(projection.amplitude / expected)));
                ++oracle.check.validSyntheticProjections;
            }
            else if (!finiteProjection(projection) || !std::isfinite(expected))
                oracle.check.allFinite = false;
        }
        std::vector<double> protectedFrequencies;
        protectedFrequencies.reserve(wantedGroups.size());
        for (const auto& group : wantedGroups)
            protectedFrequencies.push_back(group.frequencyHz);
        result.hostWantedReferenceFrequencies = protectedFrequencies;
        const auto oracleFundamental = quality::projectTone(
            capture, hostRate, definition.toneHz,
            quality::ProjectionWindow::BlackmanHarris92Db,
            static_cast<std::int64_t>(bbdWarmupHostFrames));
        const auto spectrum = blackmanHarrisSpectrum(capture, hostRate);
        const bool spectrumFinite = spectrum.allFinite
            && finiteProjection(oracleFundamental)
            && oracleFundamental.amplitude > 1.0e-10
            && spectrum.frameCount == bbdCaptureHostFrames;
        oracle.check.allFinite = oracle.check.allFinite && spectrumFinite;
        if (spectrumFinite)
        {
            if (oracle.check.offMaskFftFrames == 0u)
                oracle.check.offMaskFftFrames = spectrum.frameCount;
            else if (oracle.check.offMaskFftFrames != spectrum.frameCount)
                oracle.check.allFinite = false;
            ++oracle.check.validOffMaskFfts;
            for (std::size_t bin = 1u;
                 bin < spectrum.amplitudes.size(); ++bin)
            {
                const double frequencyHz = spectrum.binWidthHz
                                         * static_cast<double>(bin);
                if (frequencyHz < 20.0
                    || frequencyHz > bbdOraclePassbandHz
                    || spectrumBinIsMasked(
                        frequencyHz, protectedFrequencies,
                        spectrum.binWidthHz))
                    continue;
                oracle.check.worstOffMaskSpurDb = std::max(
                    oracle.check.worstOffMaskSpurDb,
                    decibels(spectrum.amplitudes[bin]
                        / oracleFundamental.amplitude));
                ++oracle.check.validOffMaskBins;
            }
        }
    }
    return oracle;
}

struct BbdMetrics
{
    double worstAnalyticRelativeRms {};
    double highToneRelativeRms {};
    double worstPhysicalImageErrorDb {};
    double worstSgaDb { -300.0 };
    double worstClockPhaseError {};
    double appliedDelayHostFrames {};
    double minimumFundamentalAmplitude {
        std::numeric_limits<double>::infinity() };
    std::size_t validCases {};
    std::size_t validBgaLines {};
    std::size_t validSgaBins {};
    std::size_t validSgaFfts {};
    std::size_t sgaFftFrames {};
    bool edgeStateMatches { true };
    bool analyticOraclePassed {};
    bool allFinite { true };
    bool pass {};
};

BbdMetrics auditBbd(double hostRate, int factor, const BbdOracle& oracle)
{
    const float internalRate = static_cast<float>(hostRate * factor);
    const std::size_t internalFrames =
        bbdHostFrames * static_cast<std::size_t>(factor);
    const double delay =
        YouKnow106TestAccess::shippingDecimatorBoundaryDelayHostFrames(factor);

    BbdMetrics metrics;
    metrics.appliedDelayHostFrames = delay;
    metrics.analyticOraclePassed = oracle.check.passed();
    for (const auto& reference : oracle.cases)
    {
        Chorus production;
        production.prepare(internalRate);
        std::vector<float> actualInternal(internalFrames);
        for (std::size_t index = 0u; index < internalFrames; ++index)
        {
            const double time = static_cast<double>(index + 1u)
                              / static_cast<double>(internalRate);
            const float input = bbdAmplitude * static_cast<float>(
                std::sin(2.0 * pi * reference.definition.toneHz * time));
            actualInternal[index] = YouKnow106TestAccess::processBbdLine(
                production, input, reference.definition.clockHz, internalRate);
        }

        const auto boundary = YouKnow106TestAccess::decimate(
            actualInternal, factor);
        const auto aligned = quality::compensateFractionalDelay(
            boundary, hostRate, delay);
        const auto interval = alignedInterval(
            reference.output, aligned,
            bbdWarmupHostFrames, bbdMeasurementEndHostFrame);
        const auto comparison = quality::compareRms(
            interval.reference, interval.candidate);
        const bool caseFinite = allFiniteSamples(actualInternal)
            && allFiniteSamples(boundary)
            && allFiniteSamples(aligned.samples)
            && allFiniteSamples(interval.reference)
            && allFiniteSamples(interval.candidate)
            && finiteComparison(comparison);
        metrics.allFinite = metrics.allFinite && caseFinite;
        if (caseFinite)
        {
            metrics.worstAnalyticRelativeRms = std::max(
                metrics.worstAnalyticRelativeRms,
                comparison.relativeError);
            ++metrics.validCases;
        }
        if (reference.definition.highTone)
            metrics.highToneRelativeRms = comparison.relativeError;

        const auto state = YouKnow106TestAccess::bbdState(production);
        metrics.edgeStateMatches = metrics.edgeStateMatches
            && state.writeIndex == reference.writeIndex
            && reference.edgeCount % Chorus::cellPairs
                   == static_cast<std::uint64_t>(state.writeIndex);
        const double phaseError =
            std::abs(state.clockPhase - reference.clockPhase);
        metrics.allFinite = metrics.allFinite
            && std::isfinite(state.clockPhase)
            && std::isfinite(reference.clockPhase)
            && std::isfinite(phaseError);
        if (std::isfinite(phaseError))
            metrics.worstClockPhaseError = std::max(
                metrics.worstClockPhaseError, phaseError);

        const auto referenceFundamental = quality::projectTone(
            interval.reference, hostRate, reference.definition.toneHz,
            quality::ProjectionWindow::BlackmanHarris92Db,
            interval.firstHostFrame);
        metrics.allFinite = metrics.allFinite
            && finiteProjection(referenceFundamental)
            && referenceFundamental.amplitude > 1.0e-10;
        if (finiteProjection(referenceFundamental))
            metrics.minimumFundamentalAmplitude = std::min(
                metrics.minimumFundamentalAmplitude,
                referenceFundamental.amplitude);
        std::vector<double> difference(interval.frameCount);
        for (std::size_t index = 0u; index < difference.size(); ++index)
            difference[index] = interval.candidate[index]
                              - interval.reference[index];
        metrics.allFinite = metrics.allFinite && allFiniteSamples(difference);
        for (int image = 1; image <= reference.enumeratedImageOrder; ++image)
        {
            for (int order : { -image, image })
            {
                const double source = std::abs(reference.definition.toneHz
                    + static_cast<double>(order)
                    * reference.definition.clockHz);
                if (!(source > 20.0))
                    continue;
                if (source <= bbdOraclePassbandHz)
                {
                    const auto wanted = quality::projectTone(
                        interval.reference, hostRate, source,
                        quality::ProjectionWindow::BlackmanHarris92Db,
                        interval.firstHostFrame);
                    const auto actual = quality::projectTone(
                        interval.candidate, hostRate, source,
                        quality::ProjectionWindow::BlackmanHarris92Db,
                        interval.firstHostFrame);
                    metrics.allFinite = metrics.allFinite
                        && finiteProjection(wanted)
                        && finiteProjection(actual);
                    if (wanted.amplitude
                            > 1.0e-8 * referenceFundamental.amplitude
                        && finiteProjection(wanted)
                        && finiteProjection(actual))
                    {
                        const double errorDb = std::abs(decibels(
                            actual.amplitude / wanted.amplitude));
                        metrics.worstPhysicalImageErrorDb = std::max(
                            metrics.worstPhysicalImageErrorDb, errorDb);
                        ++metrics.validBgaLines;
                    }
                }

            }
        }
        const auto residualSpectrum = blackmanHarrisSpectrum(
            difference, hostRate);
        const bool spectrumFinite = residualSpectrum.allFinite
            && referenceFundamental.amplitude > 1.0e-10
            && residualSpectrum.frameCount == bbdCaptureHostFrames;
        metrics.allFinite = metrics.allFinite && spectrumFinite;
        if (spectrumFinite)
        {
            if (metrics.sgaFftFrames == 0u)
                metrics.sgaFftFrames = residualSpectrum.frameCount;
            else if (metrics.sgaFftFrames != residualSpectrum.frameCount)
                metrics.allFinite = false;
            ++metrics.validSgaFfts;
            for (std::size_t bin = 1u;
                 bin < residualSpectrum.amplitudes.size(); ++bin)
            {
                const double frequencyHz = residualSpectrum.binWidthHz
                                         * static_cast<double>(bin);
                if (frequencyHz < 20.0
                    || frequencyHz > bbdOraclePassbandHz
                    || spectrumBinIsMasked(
                        frequencyHz,
                        reference.hostWantedReferenceFrequencies,
                        residualSpectrum.binWidthHz))
                    continue;
                metrics.worstSgaDb = std::max(
                    metrics.worstSgaDb,
                    decibels(residualSpectrum.amplitudes[bin]
                        / referenceFundamental.amplitude));
                ++metrics.validSgaBins;
            }
        }
    }

    metrics.pass = std::isfinite(metrics.worstAnalyticRelativeRms)
        && std::isfinite(metrics.highToneRelativeRms)
        && std::isfinite(metrics.worstPhysicalImageErrorDb)
        && std::isfinite(metrics.worstSgaDb)
        && metrics.analyticOraclePassed
        && metrics.allFinite
        && metrics.validCases == oracle.cases.size()
        && metrics.validBgaLines > 0u
        && metrics.validSgaBins > 0u
        && metrics.validSgaFfts == oracle.cases.size()
        && metrics.sgaFftFrames == bbdCaptureHostFrames
        && std::isfinite(metrics.minimumFundamentalAmplitude)
        && metrics.minimumFundamentalAmplitude > 1.0e-10
        && metrics.edgeStateMatches
        && metrics.worstClockPhaseError <= bbdPhaseGate
        && metrics.worstAnalyticRelativeRms
               <= bbdAnalyticRelativeRmsGate
        && metrics.worstPhysicalImageErrorDb <= bbdPhysicalImageGateDb
        && metrics.worstSgaDb < bbdSgaGateDb;
    return metrics;
}

struct CellResult
{
    double hostRate {};
    int factor {};
    VcfMetrics vcf;
    BbdMetrics bbd;
};

struct AuditResult
{
    std::vector<CellResult> cells;
    std::array<quality::ReferencePathSelfCheck, hostRates.size()>
        referencePathChecks;
    std::array<quality::FractionalDelaySelfCheck, 2>
        fractionalDelayChecks;
    std::array<BbdOracleCheck, hostRates.size()> bbdOracleChecks;
};

AuditResult runMatrix()
{
    AuditResult result;
    result.referencePathChecks =
        quality::runCanonicalReferencePathSelfChecks();
    result.fractionalDelayChecks =
        quality::runCanonicalFractionalDelaySelfChecks();
    result.cells.reserve(hostRates.size() * factors.size());
    for (std::size_t hostIndex = 0u;
         hostIndex < hostRates.size(); ++hostIndex)
    {
        const double hostRate = hostRates[hostIndex];
        const auto vcfOracle = buildVcfOracle(hostRate);
        const auto bbdOracle = buildBbdOracle(hostRate, vcfOracle.filter);
        result.bbdOracleChecks[hostIndex] = bbdOracle.check;
        for (int factor : factors)
            result.cells.push_back({
                hostRate, factor,
                auditVcf(hostRate, factor, vcfOracle),
                auditBbd(hostRate, factor, bbdOracle) });
    }
    return result;
}

const char* verdict(bool pass) noexcept
{
    return pass ? "PASS" : "REJECT";
}

void printReport(const AuditResult& audit)
{
    std::cout << "VCF/BBD common-host quality matrix\n"
              << "reference policy: VCF uses one factor-independent fixed-q16 "
                 "oracle per host/case, RK4 with 4/8 substeps (effective "
                 "64x/128x), and an independent 4097-tap host-boundary FIR; "
                 "BBD uses exact continuous H_in/H_out, a 128-edge delay, "
                 "edge-rate loss pole and full-period ZOH image phasors, then "
                 "the same independent fixed-q16 FIR host boundary. 4x is a "
                 "candidate, not truth\n"
              << "VCF gates: RK NRMS <= " << vcfRkRelativeRmsGate
              << ", RK64/RK128 NRMS <= " << vcfReferenceConvergenceGate
              << ", oracle self-osc convergence <= "
              << vcfOscillationPitchConvergenceGateCents << " cent/"
              << vcfOscillationLevelConvergenceGateDb << " dB"
              << ", self-osc pitch <= " << vcfOscillationPitchGateCents
              << " cent, self-osc level <= " << vcfOscillationLevelGateDb
              << " dB, hot RK NRMS <= " << vcfHotRelativeRmsGate
              << ", hot RK64/RK128 NRMS <= "
              << vcfHotReferenceConvergenceGate
              << ", exhaustive 20Hz-20k hot-residual off-mask < "
              << vcfHotResidualOffMaskGateDb
              << " dBc, oracle off-mask <= "
              << vcfHotOracleOffMaskGateDb << " dBc; FFT masks +/-"
              << spectrumMaskHalfWidthBins
              << " bins per physical output harmonic\n"
              << "BBD gates: analytic-component NRMS <= "
              << bbdAnalyticRelativeRmsGate
              << ", physical-image |error| <= " << bbdPhysicalImageGateDb
              << " dB, exhaustive 20Hz-20k unmasked BH-FFT SGA < "
              << bbdSgaGateDb
              << " dBc, edge state exact; oracle linearization <= "
              << bbdLinearizationRelativeGate << ", projection <= "
              << bbdOracleProjectionGainGateDb
              << " dB, exhaustive 20Hz-20k oracle off-mask < "
              << bbdOracleOffMaskGateDb << " dBc, image tail <= "
              << bbdOracleImageTailGateDb << " dBc after FIR; FFT masks +/-"
              << spectrumMaskHalfWidthBins
              << " bins only per <=20 kHz physical/reference line\n\n";

    std::cout << std::fixed << std::setprecision(6);
    for (const auto& check : audit.referencePathChecks)
    {
        std::cout << "oracle_support host=" << check.hostSampleRateHz
                  << " factor=" << check.decimationFactor
                  << " taps=" << check.tapCount
                  << " ripple=" << check.measuredPassbandRippleDb << " dB"
                  << " stop=" << check.measuredWorstStopbandDb << " dB"
                  << " tone_gain=" << check.projectedToneGainDb << " dB"
                  << " projection_vs_response="
                  << check.projectionVersusResponseDb << " dB"
                  << " phase_error=" << std::scientific << std::setprecision(3)
                  << check.projectionVersusResponsePhaseRadians << " rad"
                  << std::fixed << std::setprecision(6)
                  << " stop_path=" << check.projectedStopAliasGainDb << " dB"
                  << " stop_vs_response="
                  << check.stopProjectionVersusResponseDb << " dB"
                  << " verdict=" << verdict(check.passed()) << '\n';
    }
    for (const auto& check : audit.fractionalDelayChecks)
    {
        std::cout << "fractional_support delay="
                  << check.declaredDelayHostFrames
                  << " applied=" << check.appliedAdvanceHostFrames
                  << " nrms=" << check.alignedRms.relativeErrorDb << " dB"
                  << " amplitude_delta="
                  << check.maximumToneAmplitudeDeltaDb << " dB"
                  << " phase_delta=" << std::scientific
                  << std::setprecision(3)
                  << check.maximumTonePhaseDeltaRadians << " rad"
                  << std::fixed << std::setprecision(6)
                  << " verdict=" << verdict(check.passed()) << '\n';
    }
    for (const auto& check : audit.bbdOracleChecks)
    {
        std::cout << "bbd_oracle host=" << check.hostRate
                  << " linearization=" << std::scientific
                  << check.linearizationRelativeBound
                  << " projection=" << std::fixed
                  << check.maximumProjectionGainErrorDb << " dB"
                  << " off_mask=" << check.worstOffMaskSpurDb << " dBc"
                  << " post_fir_tail=" << check.worstTerminalImageDb << " dBc"
                  << " synth_order=" << check.maximumEnumeratedImageOrder
                  << " verified_through=" << check.verifiedImageOrder
                  << " conjugacy=" << std::scientific
                  << check.worstConjugacyError << std::fixed
                  << " projections=" << check.validSyntheticProjections
                  << " offmask_fft=" << check.validOffMaskFfts << "x"
                  << check.offMaskFftFrames
                  << " bins=" << check.validOffMaskBins
                  << " finite=" << verdict(check.allFinite)
                  << " filter="
                  << verdict(check.independentFilterPassed)
                  << " verdict=" << verdict(check.passed()) << '\n';
    }
    std::cout << '\n';
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& cell : audit.cells)
    {
        std::cout << "host=" << cell.hostRate
                  << " factor=" << cell.factor
                  << " internal=" << cell.hostRate * cell.factor << '\n'
                  << "  VCF " << verdict(cell.vcf.pass)
                  << " rk=" << decibels(cell.vcf.worstRkRelativeRms) << " dB"
                  << " rk_convergence="
                  << decibels(cell.vcf.worstReferenceConvergence) << " dB"
                  << " osc_ref=" << std::scientific
                  << cell.vcf.oscillationReferencePitchConvergenceCents
                  << " cents/"
                  << cell.vcf.oscillationReferenceLevelConvergenceDb
                  << " dB" << std::fixed
                  << " osc_pitch="
                  << cell.vcf.oscillationPitchErrorCents << " cents"
                  << " osc_level="
                  << cell.vcf.oscillationLevelErrorDb << " dB"
                  << " hot=" << decibels(cell.vcf.hotRkRelativeRms) << " dB"
                  << " hot_ref="
                  << decibels(cell.vcf.hotReferenceConvergence) << " dB"
                  << " hot_offmask="
                  << cell.vcf.worstHotResidualOffMaskDb << " dBc"
                  << " hot_oracle_offmask="
                  << cell.vcf.hotOracleOffMaskDb << " dBc"
                  << " hot_fund=" << std::scientific
                  << cell.vcf.hotFundamentalAmplitude << std::fixed
                  << " offmask_bins=" << cell.vcf.validHotResidualBins
                  << " oracle_bins=" << cell.vcf.validHotOracleBins
                  << " spectrum_frames=" << cell.vcf.hotSpectrumFrames
                  << " takes=" << cell.vcf.validDrivenTakes << "/"
                  << (cell.vcf.ringTakeValid ? 1 : 0) << "/"
                  << (cell.vcf.hotTakeValid ? 1 : 0)
                  << " finite=" << verdict(
                         cell.vcf.allFinite && cell.vcf.oracleFinite)
                  << " delay=" << cell.vcf.appliedDelayHostFrames << '\n'
                  << "  BBD " << verdict(cell.bbd.pass)
                  << " analytic="
                  << decibels(cell.bbd.worstAnalyticRelativeRms) << " dB"
                  << " high12k="
                  << decibels(cell.bbd.highToneRelativeRms) << " dB"
                  << " bga_error=" << cell.bbd.worstPhysicalImageErrorDb
                  << " dB sga_max=" << cell.bbd.worstSgaDb << " dBc"
                  << " phase_error=" << std::scientific
                  << cell.bbd.worstClockPhaseError << std::fixed
                  << " edge_state="
                  << (cell.bbd.edgeStateMatches ? "exact" : "mismatch")
                  << " takes=" << cell.bbd.validCases
                  << " bga_lines=" << cell.bbd.validBgaLines
                  << " sga_fft=" << cell.bbd.validSgaFfts << "x"
                  << cell.bbd.sgaFftFrames
                  << " bins=" << cell.bbd.validSgaBins
                  << " fund=" << std::scientific
                  << cell.bbd.minimumFundamentalAmplitude << std::fixed
                  << " finite=" << verdict(cell.bbd.allFinite)
                  << " delay=" << cell.bbd.appliedDelayHostFrames << '\n';
    }
    std::cout << "\nScope: the VCF cell owns the nominal OtaCascade "
                 "input-to-fourth-pole boundary (calibration/offsets zero, "
                 "gScale 1, nominal fixed headroom, Early off); this is a "
                 "declared fixture, not VCF-domain or Unit Character admission. "
                 "Its hot saw is 2.4 V at the mixer coordinate and "
              << vcfHotDriveAmplitude()
              << " V after the shipping k=3.8 input compensation. "
              << "The BBD cell owns one deterministic line from its "
                 "five-pole/coupling input support through buckets, transfer, "
                 "BLEP and output support/coupling. The steady-state BBD "
                 "capture starts after 8192 host frames (>12 output-coupling "
                 "time constants). Candidates alone use the shipping TPT "
                 "support, polyBLEP, half-bands and the declared no-search "
                 "0/23.5/35.25 host-frame advance. Scan/holds, DCO, VCA, "
                 "LFO trajectory, "
                 "stochastic noise, stereo/IC6 mix, output stages, latency "
                 "padding and a split-domain interpolator are outside this audit.\n"
              << "Evidence caveat: these are numerical-consistency/product "
                 "admission tests for the declared circuit equations and fitted "
                 "BBD transfer. The BBD oracle is independent closed-form math "
                 "over the same declared component/model anchors and linearizes "
                 "the fitted saturation at A=0.02 under the printed bound; "
                 "it is not a second hardware fit, "
                 "not measurement of an original JUNO-106, and not authority "
                 "to change production rate selection.\n";
}

void selfTest(const AuditResult& audit)
{
    const auto requireNear = [](double actual, double expected,
                                double tolerance, std::string_view label) {
        if (!std::isfinite(actual)
            || std::abs(actual - expected) > tolerance)
            throw std::runtime_error(
                std::string(label) + " changed; inspect the full matrix");
    };
    const auto& results = audit.cells;
    if (results.size() != hostRates.size() * factors.size())
        throw std::runtime_error("quality matrix has the wrong number of cells");

    for (std::size_t index = 0u;
         index < audit.referencePathChecks.size(); ++index)
    {
        const auto& check = audit.referencePathChecks[index];
        if (!check.passed() || check.hostSampleRateHz != hostRates[index]
            || check.decimationFactor != oracleFactor
            || check.tapCount != oracleFilterTaps)
            throw std::runtime_error(
                "independent 16x reference-path self-check failed");
        constexpr std::array expectedResponseStopDb {
            -141.945285, -147.116854
        };
        constexpr std::array expectedRenderedStopDb {
            -163.709752, -193.979072
        };
        requireNear(check.measuredWorstStopbandDb,
                    expectedResponseStopDb[index], 0.75,
                    "reference FIR response stopband");
        requireNear(check.projectedStopAliasGainDb,
                    expectedRenderedStopDb[index], 3.0,
                    "reference FIR rendered stop path");
        requireNear(check.projectedToneGainDb, 0.0, 0.001,
                    "reference FIR tone gain");
        requireNear(check.projectionVersusResponsePhaseRadians,
                    0.0, check.maximumTonePhaseErrorRadians,
                    "reference FIR timeline phase");
        if (!std::isfinite(check.measuredPassbandRippleDb)
            || check.measuredPassbandRippleDb > 2.0e-6
            || !std::isfinite(check.projectionVersusResponseDb)
            || std::abs(check.projectionVersusResponseDb) > 0.001
            || !std::isfinite(check.stopProjectionVersusResponseDb)
            || std::abs(check.stopProjectionVersusResponseDb) > 0.01)
            throw std::runtime_error(
                "reference FIR published validity metric changed");
    }
    constexpr std::array<double, 2> canonicalDelays { 23.5, 35.25 };
    for (std::size_t index = 0u;
         index < audit.fractionalDelayChecks.size(); ++index)
    {
        const auto& check = audit.fractionalDelayChecks[index];
        if (!check.passed()
            || check.declaredDelayHostFrames != canonicalDelays[index]
            || check.appliedAdvanceHostFrames != canonicalDelays[index])
            throw std::runtime_error(
                "fractional-delay support self-check failed");
        constexpr std::array expectedFractionalRmsDb {
            -152.346051, -149.483201
        };
        requireNear(check.alignedRms.relativeErrorDb,
                    expectedFractionalRmsDb[index], 2.0,
                    "fractional-delay recovery RMS");
    }
    for (std::size_t index = 0u;
         index < audit.bbdOracleChecks.size(); ++index)
    {
        const auto& check = audit.bbdOracleChecks[index];
        if (!check.passed() || check.hostRate != hostRates[index])
            throw std::runtime_error(
                "closed-form BBD oracle self-check failed");
        constexpr std::array expectedLinearization {
            2.423593e-7, 2.391281e-7
        };
        constexpr std::array expectedProjectionDb {
            0.016142, 0.000012
        };
        constexpr std::array expectedOffMaskDb {
            -93.045895, -135.607093
        };
        constexpr std::array expectedTailDb {
            -198.029642, -202.098403
        };
        constexpr std::array<std::size_t, 2> expectedOffMaskCounts {
            7361u, 6756u
        };
        requireNear(check.linearizationRelativeBound,
                    expectedLinearization[index], 2.0e-10,
                    "BBD oracle saturation linearization");
        requireNear(check.maximumProjectionGainErrorDb,
                    expectedProjectionDb[index], 0.005,
                    "BBD oracle synthetic projection");
        requireNear(check.worstOffMaskSpurDb,
                    expectedOffMaskDb[index], 5.0,
                    "BBD oracle off-mask spur");
        requireNear(check.worstTerminalImageDb,
                    expectedTailDb[index], 1.0,
                    "BBD oracle post-FIR remaining image tail");
        if (check.maximumEnumeratedImageOrder != 1
            || check.verifiedImageOrder != bbdMaximumImageOrder
            || check.validSyntheticProjections != 6u
            || check.validOffMaskBins != expectedOffMaskCounts[index]
            || check.validOffMaskFfts != 4u
            || check.offMaskFftFrames != bbdCaptureHostFrames
            || check.worstConjugacyError > 1.0e-12
            || !check.allFinite || !check.independentFilterPassed)
            throw std::runtime_error(
                "BBD oracle published validity class changed");
    }

    // Filled from the first reviewed deterministic run.  These are the current
    // admission classifications under the fixed gates above, not assertions
    // that a larger factor must win or that 4x is the reference.
    constexpr std::array<bool, 6> expectedVcf {
        false, false, false, false, false, false
    };
    constexpr std::array<bool, 6> expectedBbd {
        false, false, false, false, false, false
    };
    constexpr std::array<bool, 6> expectedVcfRkGate {
        true, true, true, true, true, true
    };
    constexpr std::array<bool, 6> expectedVcfHotRmsGate {
        false, false, false, false, false, false
    };
    constexpr std::array<bool, 6> expectedVcfHotOffMaskGate {
        false, true, true, false, true, true
    };
    constexpr std::array<bool, 6> expectedBbdAnalyticGate {
        false, false, false, false, false, false
    };
    constexpr std::array<bool, 6> expectedBbdBgaGate {
        false, false, false, false, false, true
    };
    constexpr std::array<bool, 6> expectedBbdSgaGate {
        false, false, false, false, false, false
    };
    constexpr std::array<double, 6> expectedVcfRkDb {
        -53.243, -65.282, -77.301, -54.730, -66.783, -78.803
    };
    constexpr std::array<double, 6> expectedVcfHotRmsDb {
        -1.110, -12.233, -24.348, -1.062, -13.752, -25.810
    };
    constexpr std::array<double, 6> expectedVcfHotOffMaskDb {
        -27.063, -67.589, -99.040, -19.658, -67.128, -99.620
    };
    constexpr std::array<double, 6> expectedVcfHotOracleOffMaskDb {
        -93.242, -93.242, -93.242, -93.163, -93.163, -93.163
    };
    constexpr std::array<double, 6> expectedVcfPitchCents {
        0.232, 0.059, 0.015, 0.195, 0.047, 0.010
    };
    constexpr std::array<double, 6> expectedVcfLevelDb {
        0.004, 0.003, 0.003, 0.008, 0.007, 0.005
    };
    constexpr std::array<double, 6> expectedBbdAnalyticDb {
        -3.099, -14.910, -27.045, -4.640, -16.426, -28.181
    };
    constexpr std::array<double, 6> expectedBbdHighToneDb {
        -3.099, -14.910, -27.045, -4.640, -16.426, -28.181
    };
    constexpr std::array<double, 6> expectedBbdBgaDb {
        34.389, 4.088, 0.867, 22.893, 3.257, 0.708
    };
    constexpr std::array<double, 6> expectedBbdSgaDb {
        -24.854, -28.762, -47.635, -28.871, -31.329, -38.189
    };
    constexpr std::array<double, 6> expectedBbdPhaseError {
        5.810e-13, 3.537e-13, 7.834e-13,
        1.535e-12, 1.535e-12, 1.651e-12
    };
    constexpr std::array<std::size_t, 6> expectedVcfOffMaskBinCounts {
        14618u, 14618u, 14618u, 13412u, 13412u, 13412u
    };
    constexpr std::array<std::size_t, 6> expectedBbdSgaCounts {
        7361u, 7361u, 7361u, 6756u, 6756u, 6756u
    };
    for (std::size_t index = 0; index < results.size(); ++index)
    {
        const auto& cell = results[index];
        const auto expectedHost = hostRates[index / factors.size()];
        const auto expectedFactor = factors[index % factors.size()];
        if (cell.hostRate != expectedHost || cell.factor != expectedFactor)
            throw std::runtime_error("quality matrix ordering changed");
        if (cell.vcf.pass != expectedVcf[index]
            || cell.bbd.pass != expectedBbd[index])
            throw std::runtime_error(
                "quality classification changed; inspect the full matrix");
        const bool vcfRkPass =
            cell.vcf.worstRkRelativeRms <= vcfRkRelativeRmsGate;
        const bool vcfReferencePass =
            cell.vcf.worstReferenceConvergence
                <= vcfReferenceConvergenceGate;
        const bool vcfOscReferencePass =
            cell.vcf.oscillationReferencePitchConvergenceCents
                <= vcfOscillationPitchConvergenceGateCents
            && cell.vcf.oscillationReferenceLevelConvergenceDb
                <= vcfOscillationLevelConvergenceGateDb;
        const bool vcfPitchPass =
            cell.vcf.oscillationPitchErrorCents
                <= vcfOscillationPitchGateCents;
        const bool vcfLevelPass =
            cell.vcf.oscillationLevelErrorDb
                <= vcfOscillationLevelGateDb;
        const bool vcfHotRmsPass =
            cell.vcf.hotRkRelativeRms <= vcfHotRelativeRmsGate;
        const bool vcfHotReferencePass =
            cell.vcf.hotReferenceConvergence
                <= vcfHotReferenceConvergenceGate;
        const bool vcfHotOffMaskPass =
            cell.vcf.worstHotResidualOffMaskDb
                < vcfHotResidualOffMaskGateDb;
        if (vcfRkPass != expectedVcfRkGate[index]
            || !vcfReferencePass || !vcfOscReferencePass
            || !vcfPitchPass || !vcfLevelPass
            || vcfHotRmsPass != expectedVcfHotRmsGate[index]
            || !vcfHotReferencePass
            || vcfHotOffMaskPass != expectedVcfHotOffMaskGate[index]
            || !cell.vcf.oracleFinite || !cell.vcf.allFinite
            || cell.vcf.validDrivenTakes != drivenFeedbackCases.size()
            || !cell.vcf.ringTakeValid || !cell.vcf.hotTakeValid
            || cell.vcf.validHotResidualBins
                   != expectedVcfOffMaskBinCounts[index]
            || cell.vcf.validHotOracleBins
                   != expectedVcfOffMaskBinCounts[index]
            || cell.vcf.hotSpectrumFrames != vcfHotCaptureHostFrames
            || !(cell.vcf.hotOracleOffMaskDb
                     <= vcfHotOracleOffMaskGateDb)
            || !(cell.vcf.hotFundamentalAmplitude > 1.0e-8))
            throw std::runtime_error(
                "a VCF metric class changed; inspect the full matrix");

        const bool bbdAnalyticPass =
            cell.bbd.worstAnalyticRelativeRms
                <= bbdAnalyticRelativeRmsGate;
        const bool bbdBgaPass =
            cell.bbd.worstPhysicalImageErrorDb <= bbdPhysicalImageGateDb;
        const bool bbdSgaPass = cell.bbd.worstSgaDb < bbdSgaGateDb;
        const bool bbdPhasePass =
            cell.bbd.worstClockPhaseError <= bbdPhaseGate;
        if (bbdAnalyticPass != expectedBbdAnalyticGate[index]
            || bbdBgaPass != expectedBbdBgaGate[index]
            || bbdSgaPass != expectedBbdSgaGate[index]
            || !bbdPhasePass || !cell.bbd.edgeStateMatches
            || !cell.bbd.analyticOraclePassed || !cell.bbd.allFinite
            || cell.bbd.validCases != 4u
            || cell.bbd.validBgaLines != 1u
            || cell.bbd.validSgaBins != expectedBbdSgaCounts[index]
            || cell.bbd.validSgaFfts != 4u
            || cell.bbd.sgaFftFrames != bbdCaptureHostFrames
            || !(cell.bbd.minimumFundamentalAmplitude > 1.0e-10))
            throw std::runtime_error(
                "a BBD metric class changed; inspect the full matrix");

        requireNear(decibels(cell.vcf.worstRkRelativeRms),
                    expectedVcfRkDb[index], 0.75, "VCF RK error");
        requireNear(decibels(cell.vcf.hotRkRelativeRms),
                    expectedVcfHotRmsDb[index], 1.0,
                    "VCF hot RK waveform");
        requireNear(cell.vcf.worstHotResidualOffMaskDb,
                    expectedVcfHotOffMaskDb[index], 2.0,
                    "VCF hot residual off-mask");
        requireNear(cell.vcf.hotOracleOffMaskDb,
                    expectedVcfHotOracleOffMaskDb[index], 1.0,
                    "VCF hot oracle off-mask");
        requireNear(cell.vcf.hotFundamentalAmplitude,
                    0.6158, 0.005,
                    "VCF hot reference fundamental");
        requireNear(cell.vcf.oscillationPitchErrorCents,
                    expectedVcfPitchCents[index], 0.03,
                    "VCF self-oscillation pitch");
        requireNear(cell.vcf.oscillationLevelErrorDb,
                    expectedVcfLevelDb[index], 0.01,
                    "VCF self-oscillation level");
        requireNear(decibels(cell.bbd.worstAnalyticRelativeRms),
                    expectedBbdAnalyticDb[index], 0.75,
                    "BBD analytic-component waveform");
        requireNear(decibels(cell.bbd.highToneRelativeRms),
                    expectedBbdHighToneDb[index], 0.75,
                    "BBD 12 kHz waveform");
        requireNear(cell.bbd.worstPhysicalImageErrorDb,
                    expectedBbdBgaDb[index], 0.25,
                    "BBD physical-image error");
        requireNear(cell.bbd.worstSgaDb,
                    expectedBbdSgaDb[index], 1.5,
                    "BBD simulation-generated alias");
        requireNear(cell.bbd.worstClockPhaseError,
                    expectedBbdPhaseError[index], 5.0e-11,
                    "BBD clock phase");
        requireNear(cell.bbd.minimumFundamentalAmplitude,
                    1.466e-3, 5.0e-5,
                    "BBD analytic fundamental");
        if (!std::isfinite(cell.vcf.worstRkRelativeRms)
            || !std::isfinite(cell.vcf.worstReferenceConvergence)
            || !std::isfinite(
                cell.vcf.oscillationReferencePitchConvergenceCents)
            || !std::isfinite(
                cell.vcf.oscillationReferenceLevelConvergenceDb)
            || !std::isfinite(cell.vcf.oscillationPitchErrorCents)
            || !std::isfinite(cell.vcf.oscillationLevelErrorDb)
            || !std::isfinite(cell.vcf.hotRkRelativeRms)
            || !std::isfinite(cell.vcf.hotReferenceConvergence)
            || !std::isfinite(cell.vcf.worstHotResidualOffMaskDb)
            || !std::isfinite(cell.vcf.hotOracleOffMaskDb)
            || !std::isfinite(cell.vcf.hotFundamentalAmplitude)
            || !std::isfinite(cell.bbd.worstAnalyticRelativeRms)
            || !std::isfinite(cell.bbd.highToneRelativeRms)
            || !std::isfinite(cell.bbd.worstPhysicalImageErrorDb)
            || !std::isfinite(cell.bbd.worstSgaDb)
            || !std::isfinite(cell.bbd.minimumFundamentalAmplitude))
            throw std::runtime_error("quality matrix contains a non-finite metric");
        if (!cell.vcf.oracleFilterPassed
            || !cell.bbd.analyticOraclePassed)
            throw std::runtime_error(
                "an independent oracle failed its validity fence");
        const double declaredDelay =
            YouKnow106TestAccess::shippingDecimatorBoundaryDelayHostFrames(
                cell.factor);
        if (cell.vcf.appliedDelayHostFrames != declaredDelay
            || cell.bbd.appliedDelayHostFrames != declaredDelay)
            throw std::runtime_error(
                "candidate boundary used an undeclared delay alignment");
    }
    std::cout << "VCF/BBD quality classification self-test: PASS\n";
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        bool selfTestRequested = false;
        if (argc == 2 && std::string_view(argv[1]) == "--self-test")
            selfTestRequested = true;
        else if (argc == 2 && (std::string_view(argv[1]) == "--help"
                              || std::string_view(argv[1]) == "-h"))
        {
            std::cout << "Usage: YouKnow106VcfBbdQualityAudit [--self-test]\n";
            return 0;
        }
        else if (argc != 1)
        {
            std::cerr << "unknown argument; use --help\n";
            return 2;
        }

        const auto results = runMatrix();
        printReport(results);
        if (selfTestRequested)
            selfTest(results);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "VCF/BBD quality audit failed: " << error.what() << '\n';
        return 1;
    }
}
