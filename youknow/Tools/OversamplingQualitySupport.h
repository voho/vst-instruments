// Independent numerical support for the oversampling quality audit.
//
// This header deliberately contains no engine hooks, friend declarations or
// production filter constants.  The long double-accumulated, double-precision
// reference path is intended to put renders from different internal grids on
// one declared host-time boundary before they are compared.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace youknow::oversampling_quality
{

inline constexpr std::size_t referenceFilterTapCount = 4097u;
inline constexpr double referenceFilterAttenuationDb = 140.0;
inline constexpr double defaultDecibelFloor = -300.0;

template <typename Sample>
concept RealSample = std::is_arithmetic_v<std::remove_cv_t<Sample>>
                  && (!std::is_same_v<std::remove_cv_t<Sample>, bool>);

enum class Classification
{
    Pass,
    Review,
    Fail
};

[[nodiscard]] inline constexpr std::string_view
classificationName(Classification classification) noexcept
{
    switch (classification)
    {
        case Classification::Pass: return "pass";
        case Classification::Review: return "review";
        case Classification::Fail: return "fail";
    }
    return "fail";
}

[[nodiscard]] inline constexpr Classification
worstClassification(Classification first, Classification second) noexcept
{
    return static_cast<int>(first) >= static_cast<int>(second) ? first : second;
}

// Classify a metric for which smaller values are better.  The middle region is
// intentionally named Review rather than warning: an audit must decide how to
// treat it instead of silently accepting it.
[[nodiscard]] inline Classification classifyUpperBound(
    double value, double passMaximum, double reviewMaximum) noexcept
{
    if (!std::isfinite(value) || !std::isfinite(passMaximum)
        || !std::isfinite(reviewMaximum) || reviewMaximum < passMaximum)
        return Classification::Fail;
    if (value <= passMaximum)
        return Classification::Pass;
    if (value <= reviewMaximum)
        return Classification::Review;
    return Classification::Fail;
}

// Classify a metric for which larger values are better.
[[nodiscard]] inline Classification classifyLowerBound(
    double value, double passMinimum, double reviewMinimum) noexcept
{
    if (!std::isfinite(value) || !std::isfinite(passMinimum)
        || !std::isfinite(reviewMinimum) || reviewMinimum > passMinimum)
        return Classification::Fail;
    if (value >= passMinimum)
        return Classification::Pass;
    if (value >= reviewMinimum)
        return Classification::Review;
    return Classification::Fail;
}

[[nodiscard]] inline double decibelsToAmplitude(double decibels) noexcept
{
    return std::pow(10.0, decibels / 20.0);
}

[[nodiscard]] inline double amplitudeToDecibels(
    double amplitude, double floorDb = defaultDecibelFloor) noexcept
{
    if (std::isnan(amplitude) || std::isnan(floorDb))
        return std::numeric_limits<double>::quiet_NaN();
    if (!std::isfinite(amplitude))
        return std::numeric_limits<double>::infinity();
    const double magnitude = std::abs(amplitude);
    if (magnitude == 0.0)
        return floorDb;
    return std::max(20.0 * std::log10(magnitude), floorDb);
}

[[nodiscard]] inline double powerToDecibels(
    double power, double floorDb = defaultDecibelFloor) noexcept
{
    if (std::isnan(power) || std::isnan(floorDb) || power < 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    if (!std::isfinite(power))
        return std::numeric_limits<double>::infinity();
    if (power == 0.0)
        return floorDb;
    return std::max(10.0 * std::log10(power), floorDb);
}

[[nodiscard]] inline double relativeAmplitudeToDecibels(
    double numerator, double denominator,
    double floorDb = defaultDecibelFloor) noexcept
{
    const double numeratorMagnitude = std::abs(numerator);
    const double denominatorMagnitude = std::abs(denominator);
    if (std::isnan(numeratorMagnitude) || std::isnan(denominatorMagnitude))
        return std::numeric_limits<double>::quiet_NaN();
    if (denominatorMagnitude == 0.0)
        return numeratorMagnitude == 0.0
            ? floorDb : std::numeric_limits<double>::infinity();
    return amplitudeToDecibels(numeratorMagnitude / denominatorMagnitude,
                               floorDb);
}

template <RealSample Sample>
[[nodiscard]] inline double rootMeanSquare(
    std::span<const Sample> samples) noexcept
{
    if (samples.empty())
        return 0.0;
    long double sumSquares = 0.0L;
    for (const Sample sample : samples)
    {
        const long double value = static_cast<long double>(sample);
        sumSquares += value * value;
    }
    return std::sqrt(static_cast<double>(
        sumSquares / static_cast<long double>(samples.size())));
}

struct RmsComparison
{
    double referenceRms {};
    double candidateRms {};
    double errorRms {};
    double relativeError {};
    double relativeErrorDb { defaultDecibelFloor };
    double maximumAbsoluteError {};
};

template <RealSample ReferenceSample, RealSample CandidateSample>
[[nodiscard]] inline RmsComparison compareRms(
    std::span<const ReferenceSample> reference,
    std::span<const CandidateSample> candidate,
    double floorDb = defaultDecibelFloor)
{
    if (reference.empty() || reference.size() != candidate.size())
        throw std::invalid_argument(
            "RMS comparison requires equal, non-empty signals");

    long double referenceSquares = 0.0L;
    long double candidateSquares = 0.0L;
    long double errorSquares = 0.0L;
    double maximumError = 0.0;
    for (std::size_t frame = 0; frame < reference.size(); ++frame)
    {
        const long double referenceValue =
            static_cast<long double>(reference[frame]);
        const long double candidateValue =
            static_cast<long double>(candidate[frame]);
        const long double error = candidateValue - referenceValue;
        referenceSquares += referenceValue * referenceValue;
        candidateSquares += candidateValue * candidateValue;
        errorSquares += error * error;
        maximumError = std::max(maximumError,
            std::abs(static_cast<double>(error)));
    }

    const long double count = static_cast<long double>(reference.size());
    RmsComparison result;
    result.referenceRms = std::sqrt(
        static_cast<double>(referenceSquares / count));
    result.candidateRms = std::sqrt(
        static_cast<double>(candidateSquares / count));
    result.errorRms = std::sqrt(static_cast<double>(errorSquares / count));
    result.maximumAbsoluteError = maximumError;
    if (result.referenceRms == 0.0)
    {
        result.relativeError = result.errorRms == 0.0
            ? 0.0 : std::numeric_limits<double>::infinity();
    }
    else
    {
        result.relativeError = result.errorRms / result.referenceRms;
    }
    result.relativeErrorDb = amplitudeToDecibels(result.relativeError, floorDb);
    return result;
}

struct LowPassSpecification
{
    double sampleRateHz {};
    double passbandEdgeHz {};
    double stopbandEdgeHz {};
    double stopbandAttenuationDb { referenceFilterAttenuationDb };
    std::size_t tapCount { referenceFilterTapCount };
};

struct KaiserLowPass
{
    LowPassSpecification specification;
    double idealCutoffHz {};
    double kaiserBeta {};
    std::size_t groupDelayInputFrames {};
    std::vector<double> coefficients;
};

namespace detail
{

inline void validateLowPassSpecification(const LowPassSpecification& spec)
{
    const double nyquist = 0.5 * spec.sampleRateHz;
    if (!std::isfinite(spec.sampleRateHz) || spec.sampleRateHz <= 0.0
        || !std::isfinite(spec.passbandEdgeHz)
        || spec.passbandEdgeHz < 0.0
        || !std::isfinite(spec.stopbandEdgeHz)
        || spec.stopbandEdgeHz <= spec.passbandEdgeHz
        || spec.stopbandEdgeHz > nyquist
        || !std::isfinite(spec.stopbandAttenuationDb)
        || spec.stopbandAttenuationDb < 0.0
        || spec.stopbandAttenuationDb > 300.0
        || spec.tapCount < 3u || spec.tapCount % 2u == 0u)
    {
        throw std::invalid_argument(
            "low-pass design requires finite ordered band edges, an odd "
            "tap count and a stop edge no higher than Nyquist");
    }
}

[[nodiscard]] inline double modifiedBesselI0(double value)
{
    // The positive power series avoids a dependency on std::cyl_bessel_i,
    // which is not uniformly available on every supported standard library.
    const long double argument = static_cast<long double>(value);
    const long double scaledSquare = 0.25L * argument * argument;
    long double term = 1.0L;
    long double sum = 1.0L;
    for (unsigned int order = 1u; order <= 256u; ++order)
    {
        const long double divisor = static_cast<long double>(order)
                                  * static_cast<long double>(order);
        term *= scaledSquare / divisor;
        sum += term;
        if (term <= sum * std::numeric_limits<long double>::epsilon())
            return static_cast<double>(sum);
    }
    throw std::runtime_error("Kaiser I0 series did not converge");
}

[[nodiscard]] inline double kaiserBetaForAttenuation(
    double attenuationDb) noexcept
{
    if (attenuationDb > 50.0)
        return 0.1102 * (attenuationDb - 8.7);
    if (attenuationDb >= 21.0)
    {
        const double excess = attenuationDb - 21.0;
        return 0.5842 * std::pow(excess, 0.4) + 0.07886 * excess;
    }
    return 0.0;
}

[[nodiscard]] inline std::complex<double> zeroPhaseResponseUnchecked(
    const KaiserLowPass& filter, double frequencyHz) noexcept
{
    const std::size_t centre = filter.groupDelayInputFrames;
    const long double angularFrequency =
        2.0L * std::numbers::pi_v<long double>
        * static_cast<long double>(frequencyHz)
        / static_cast<long double>(filter.specification.sampleRateHz);
    long double real = static_cast<long double>(filter.coefficients[centre]);
    long double imaginary = 0.0L;
    const long double stepCosine = std::cos(angularFrequency);
    const long double stepSine = std::sin(angularFrequency);
    long double phaseCosine = 1.0L;
    long double phaseSine = 0.0L;
    for (std::size_t offset = 1u; offset <= centre; ++offset)
    {
        const long double nextCosine = phaseCosine * stepCosine
                                     - phaseSine * stepSine;
        const long double nextSine = phaseSine * stepCosine
                                   + phaseCosine * stepSine;
        phaseCosine = nextCosine;
        phaseSine = nextSine;
        const long double before = static_cast<long double>(
            filter.coefficients[centre - offset]);
        const long double after = static_cast<long double>(
            filter.coefficients[centre + offset]);
        real += (before + after) * phaseCosine;
        imaginary += (before - after) * phaseSine;
    }
    return { static_cast<double>(real), static_cast<double>(imaginary) };
}

} // namespace detail

[[nodiscard]] inline KaiserLowPass designKaiserLowPass(
    const LowPassSpecification& specification)
{
    detail::validateLowPassSpecification(specification);
    KaiserLowPass result;
    result.specification = specification;
    result.idealCutoffHz = 0.5 * (specification.passbandEdgeHz
                                + specification.stopbandEdgeHz);
    result.kaiserBeta = detail::kaiserBetaForAttenuation(
        specification.stopbandAttenuationDb);
    result.groupDelayInputFrames = (specification.tapCount - 1u) / 2u;
    result.coefficients.resize(specification.tapCount);

    const double denominator = detail::modifiedBesselI0(result.kaiserBeta);
    const double normalisedCutoff = result.idealCutoffHz
                                  / specification.sampleRateHz;
    const double radius = static_cast<double>(result.groupDelayInputFrames);
    long double coefficientSum = 0.0L;
    for (std::size_t tap = 0u; tap < specification.tapCount; ++tap)
    {
        const double displacement = static_cast<double>(tap)
                                  - static_cast<double>(
                                      result.groupDelayInputFrames);
        const double ideal = displacement == 0.0
            ? 2.0 * normalisedCutoff
            : std::sin(2.0 * std::numbers::pi_v<double>
                     * normalisedCutoff * displacement)
              / (std::numbers::pi_v<double> * displacement);
        const double position = displacement / radius;
        const double windowArgument = result.kaiserBeta
            * std::sqrt(std::max(0.0, 1.0 - position * position));
        const double window = detail::modifiedBesselI0(windowArgument)
                            / denominator;
        result.coefficients[tap] = ideal * window;
        coefficientSum += static_cast<long double>(result.coefficients[tap]);
    }
    if (!std::isfinite(static_cast<double>(coefficientSum))
        || coefficientSum == 0.0L)
        throw std::runtime_error("Kaiser low-pass normalization failed");
    for (double& coefficient : result.coefficients)
        coefficient = static_cast<double>(
            static_cast<long double>(coefficient) / coefficientSum);
    return result;
}

// Standard Kaiser order estimate, rounded up to an odd tap count whose group
// delay is an integer multiple of groupDelayMultiple.  The audit's default
// 4097-tap filter intentionally need not equal this lower-bound estimate.
[[nodiscard]] inline std::size_t estimatedKaiserTapCount(
    const LowPassSpecification& specification,
    std::size_t groupDelayMultiple = 4u)
{
    detail::validateLowPassSpecification(specification);
    if (groupDelayMultiple == 0u)
        throw std::invalid_argument("group-delay multiple must be positive");
    const double transitionRadians =
        2.0 * std::numbers::pi_v<double>
        * (specification.stopbandEdgeHz - specification.passbandEdgeHz)
        / specification.sampleRateHz;
    const double estimatedOrder = std::max(
        2.0, (specification.stopbandAttenuationDb - 8.0)
             / (2.285 * transitionRadians));
    const auto requiredDelay = static_cast<std::size_t>(
        std::ceil(0.5 * std::ceil(estimatedOrder)));
    const std::size_t roundedDelay =
        ((requiredDelay + groupDelayMultiple - 1u) / groupDelayMultiple)
        * groupDelayMultiple;
    return 2u * roundedDelay + 1u;
}

[[nodiscard]] inline std::complex<double> zeroPhaseFrequencyResponse(
    const KaiserLowPass& filter, double frequencyHz)
{
    if (!std::isfinite(frequencyHz) || frequencyHz < 0.0
        || frequencyHz > 0.5 * filter.specification.sampleRateHz)
        throw std::invalid_argument("response frequency lies outside Nyquist");
    return detail::zeroPhaseResponseUnchecked(filter, frequencyHz);
}

[[nodiscard]] inline std::complex<double> frequencyResponse(
    const KaiserLowPass& filter, double frequencyHz)
{
    const auto zeroPhase = zeroPhaseFrequencyResponse(filter, frequencyHz);
    const double angularFrequency = 2.0 * std::numbers::pi_v<double>
                                  * frequencyHz
                                  / filter.specification.sampleRateHz;
    return zeroPhase * std::polar(
        1.0, -angularFrequency
             * static_cast<double>(filter.groupDelayInputFrames));
}

struct LowPassResponseMetrics
{
    std::size_t gridIntervals {};
    double dcGain {};
    double passbandMinimumDb {};
    double passbandMinimumFrequencyHz {};
    double passbandMaximumDb {};
    double passbandMaximumFrequencyHz {};
    double passbandRippleDb {};
    double stopbandMaximumDb {};
    double stopbandMaximumFrequencyHz {};
    double maximumSymmetryError {};
    bool finite {};
};

[[nodiscard]] inline LowPassResponseMetrics measureLowPassResponse(
    const KaiserLowPass& filter, std::size_t gridIntervals = 16384u,
    double floorDb = defaultDecibelFloor)
{
    if (gridIntervals < 2u)
        throw std::invalid_argument("response grid needs at least two intervals");
    LowPassResponseMetrics result;
    result.gridIntervals = gridIntervals;
    const double nyquist = 0.5 * filter.specification.sampleRateHz;
    const double gridSpacing = nyquist / static_cast<double>(gridIntervals);
    double passbandMinimum = std::numeric_limits<double>::infinity();
    double passbandMaximum = -std::numeric_limits<double>::infinity();
    double stopbandMaximum = -std::numeric_limits<double>::infinity();
    double passbandMinimumFrequency = 0.0;
    double passbandMaximumFrequency = 0.0;
    double stopbandMaximumFrequency = filter.specification.stopbandEdgeHz;
    const auto magnitudeAt = [&](double frequencyHz) {
        return std::abs(detail::zeroPhaseResponseUnchecked(
            filter, frequencyHz));
    };
    const auto observe = [&](double frequencyHz) {
        const double magnitude = magnitudeAt(frequencyHz);
        if (frequencyHz <= filter.specification.passbandEdgeHz)
        {
            if (magnitude < passbandMinimum)
            {
                passbandMinimum = magnitude;
                passbandMinimumFrequency = frequencyHz;
            }
            if (magnitude > passbandMaximum)
            {
                passbandMaximum = magnitude;
                passbandMaximumFrequency = frequencyHz;
            }
        }
        if (frequencyHz >= filter.specification.stopbandEdgeHz)
        {
            if (magnitude > stopbandMaximum)
            {
                stopbandMaximum = magnitude;
                stopbandMaximumFrequency = frequencyHz;
            }
        }
    };

    for (std::size_t point = 0u; point <= gridIntervals; ++point)
    {
        observe(nyquist * static_cast<double>(point)
                / static_cast<double>(gridIntervals));
    }
    // Always include the contractual edges even when neither lies on a grid
    // point.  Duplicate observations are harmless extrema updates.
    observe(filter.specification.passbandEdgeHz);
    observe(filter.specification.stopbandEdgeHz);

    const auto refine = [&](double approximateFrequency, double bandMinimum,
                            double bandMaximum, bool findMaximum) {
        double lower = std::max(bandMinimum,
                                approximateFrequency - gridSpacing);
        double upper = std::min(bandMaximum,
                                approximateFrequency + gridSpacing);
        constexpr double inverseGoldenRatio = 0.6180339887498948482;
        double left = upper - inverseGoldenRatio * (upper - lower);
        double right = lower + inverseGoldenRatio * (upper - lower);
        double leftValue = magnitudeAt(left);
        double rightValue = magnitudeAt(right);
        const auto leftIsBetter = [&](double first, double second) {
            return findMaximum ? first > second : first < second;
        };
        for (int iteration = 0; iteration < 56; ++iteration)
        {
            if (leftIsBetter(leftValue, rightValue))
            {
                upper = right;
                right = left;
                rightValue = leftValue;
                left = upper - inverseGoldenRatio * (upper - lower);
                leftValue = magnitudeAt(left);
            }
            else
            {
                lower = left;
                left = right;
                leftValue = rightValue;
                right = lower + inverseGoldenRatio * (upper - lower);
                rightValue = magnitudeAt(right);
            }
        }
        double bestFrequency = approximateFrequency;
        double bestValue = magnitudeAt(approximateFrequency);
        for (const double frequency : { lower, left, right, upper })
        {
            const double value = magnitudeAt(frequency);
            if (leftIsBetter(value, bestValue))
            {
                bestFrequency = frequency;
                bestValue = value;
            }
        }
        return std::pair { bestFrequency, bestValue };
    };

    const auto refinedPassbandMinimum = refine(
        passbandMinimumFrequency, 0.0,
        filter.specification.passbandEdgeHz, false);
    const auto refinedPassbandMaximum = refine(
        passbandMaximumFrequency, 0.0,
        filter.specification.passbandEdgeHz, true);
    const auto refinedStopbandMaximum = refine(
        stopbandMaximumFrequency, filter.specification.stopbandEdgeHz,
        nyquist, true);
    passbandMinimumFrequency = refinedPassbandMinimum.first;
    passbandMinimum = refinedPassbandMinimum.second;
    passbandMaximumFrequency = refinedPassbandMaximum.first;
    passbandMaximum = refinedPassbandMaximum.second;
    stopbandMaximumFrequency = refinedStopbandMaximum.first;
    stopbandMaximum = refinedStopbandMaximum.second;

    result.passbandMinimumDb = amplitudeToDecibels(passbandMinimum, floorDb);
    result.passbandMinimumFrequencyHz = passbandMinimumFrequency;
    result.passbandMaximumDb = amplitudeToDecibels(passbandMaximum, floorDb);
    result.passbandMaximumFrequencyHz = passbandMaximumFrequency;
    result.stopbandMaximumDb = amplitudeToDecibels(stopbandMaximum, floorDb);
    result.stopbandMaximumFrequencyHz = stopbandMaximumFrequency;

    result.dcGain = std::abs(detail::zeroPhaseResponseUnchecked(filter, 0.0));
    for (std::size_t offset = 0u;
         offset <= filter.groupDelayInputFrames; ++offset)
    {
        result.maximumSymmetryError = std::max(
            result.maximumSymmetryError,
            std::abs(filter.coefficients[offset]
                   - filter.coefficients[filter.coefficients.size()
                                       - 1u - offset]));
    }
    result.passbandRippleDb = result.passbandMaximumDb
                            - result.passbandMinimumDb;
    result.finite = std::isfinite(result.dcGain)
                 && std::isfinite(result.passbandMinimumDb)
                 && std::isfinite(result.passbandMaximumDb)
                 && std::isfinite(result.passbandRippleDb)
                 && std::isfinite(result.stopbandMaximumDb)
                 && std::isfinite(result.maximumSymmetryError);
    return result;
}

struct ResponseGridConvergence
{
    LowPassResponseMetrics coarse;
    LowPassResponseMetrics fine;
    double passbandMinimumDeltaDb {};
    double passbandMaximumDeltaDb {};
    double stopbandMaximumDeltaDb {};
    double maximumExtremumDeltaDb {};
    bool finite {};
};

[[nodiscard]] inline ResponseGridConvergence checkResponseGridConvergence(
    const KaiserLowPass& filter, std::size_t coarseGridIntervals = 8192u,
    double floorDb = defaultDecibelFloor)
{
    if (coarseGridIntervals > std::numeric_limits<std::size_t>::max() / 2u)
        throw std::invalid_argument("response grid is too large to refine");
    ResponseGridConvergence result;
    result.coarse = measureLowPassResponse(
        filter, coarseGridIntervals, floorDb);
    result.fine = measureLowPassResponse(
        filter, 2u * coarseGridIntervals, floorDb);
    result.passbandMinimumDeltaDb = std::abs(
        result.fine.passbandMinimumDb - result.coarse.passbandMinimumDb);
    result.passbandMaximumDeltaDb = std::abs(
        result.fine.passbandMaximumDb - result.coarse.passbandMaximumDb);
    result.stopbandMaximumDeltaDb = std::abs(
        result.fine.stopbandMaximumDb - result.coarse.stopbandMaximumDb);
    result.maximumExtremumDeltaDb = std::max({
        result.passbandMinimumDeltaDb,
        result.passbandMaximumDeltaDb,
        result.stopbandMaximumDeltaDb });
    result.finite = result.coarse.finite && result.fine.finite
                 && std::isfinite(result.maximumExtremumDeltaDb);
    return result;
}

struct ReferenceFilterRequirements
{
    double maximumPassbandRippleDb { 0.001 };
    double minimumStopbandAttenuationDb { 110.0 };
    double maximumDcGainError { 1.0e-12 };
    double maximumSymmetryError { 1.0e-15 };
    double maximumGridConvergenceDb { 0.01 };
    std::size_t coarseGridIntervals { 8192u };
};

struct ReferenceFilterCheck
{
    ResponseGridConvergence convergence;
    bool responsePassed {};
    bool convergencePassed {};

    [[nodiscard]] bool passed() const noexcept
    {
        return responsePassed && convergencePassed;
    }
};

[[nodiscard]] inline ReferenceFilterCheck checkReferenceFilter(
    const KaiserLowPass& filter,
    const ReferenceFilterRequirements& requirements = {})
{
    if (!std::isfinite(requirements.maximumPassbandRippleDb)
        || requirements.maximumPassbandRippleDb < 0.0
        || !std::isfinite(requirements.minimumStopbandAttenuationDb)
        || requirements.minimumStopbandAttenuationDb < 0.0
        || !std::isfinite(requirements.maximumDcGainError)
        || requirements.maximumDcGainError < 0.0
        || !std::isfinite(requirements.maximumSymmetryError)
        || requirements.maximumSymmetryError < 0.0
        || !std::isfinite(requirements.maximumGridConvergenceDb)
        || requirements.maximumGridConvergenceDb < 0.0)
        throw std::invalid_argument("reference-filter requirements are invalid");

    ReferenceFilterCheck result;
    result.convergence = checkResponseGridConvergence(
        filter, requirements.coarseGridIntervals);
    const auto& response = result.convergence.fine;
    result.responsePassed = response.finite
        && std::abs(response.dcGain - 1.0)
               <= requirements.maximumDcGainError
        && response.passbandRippleDb
               <= requirements.maximumPassbandRippleDb
        && response.stopbandMaximumDb
               <= -requirements.minimumStopbandAttenuationDb
        && response.maximumSymmetryError
               <= requirements.maximumSymmetryError;
    result.convergencePassed = result.convergence.finite
        && result.convergence.maximumExtremumDeltaDb
               <= requirements.maximumGridConvergenceDb;
    return result;
}

struct HostAlignedSignal
{
    std::vector<double> samples;
    double sourceSampleRateHz {};
    double hostSampleRateHz {};
    std::size_t decimationFactor {};
    std::size_t declaredGroupDelayInputFrames {};
    std::size_t declaredGroupDelayHostFrames {};
    std::int64_t firstHostFrame {};

    [[nodiscard]] std::int64_t endHostFrameExclusive() const noexcept
    {
        return firstHostFrame + static_cast<std::int64_t>(samples.size());
    }
};

// Apply the symmetric reference FIR and select centres at source frame k*factor.
// Only samples with full FIR support are returned.  firstHostFrame preserves
// their position on the original host timeline; the causal delay is declared
// in both source and host frames and has been compensated in the samples.
template <RealSample Sample>
[[nodiscard]] inline HostAlignedSignal decimateToHostBoundary(
    std::span<const Sample> input, std::size_t factor,
    const KaiserLowPass& filter)
{
    if (factor == 0u)
        throw std::invalid_argument("decimation factor must be positive");
    if (input.size() < filter.coefficients.size())
        throw std::invalid_argument(
            "decimation input is shorter than the reference filter");
    const std::size_t delay = filter.groupDelayInputFrames;
    if (delay % factor != 0u)
        throw std::invalid_argument(
            "reference-filter delay must be divisible by decimation factor");

    const std::size_t firstHostFrame = delay / factor;
    const std::size_t lastSupportedCentre = input.size() - 1u - delay;
    const std::size_t lastHostFrame = lastSupportedCentre / factor;
    if (lastHostFrame < firstHostFrame)
        throw std::invalid_argument("decimation input has no supported host frame");

    HostAlignedSignal result;
    result.sourceSampleRateHz = filter.specification.sampleRateHz;
    result.hostSampleRateHz = filter.specification.sampleRateHz
                            / static_cast<double>(factor);
    result.decimationFactor = factor;
    result.declaredGroupDelayInputFrames = delay;
    result.declaredGroupDelayHostFrames = delay / factor;
    result.firstHostFrame = static_cast<std::int64_t>(firstHostFrame);
    result.samples.reserve(lastHostFrame - firstHostFrame + 1u);

    for (std::size_t hostFrame = firstHostFrame;
         hostFrame <= lastHostFrame; ++hostFrame)
    {
        const std::size_t centre = hostFrame * factor;
        const std::size_t firstInput = centre - delay;
        long double sum = 0.0L;
        for (std::size_t tap = 0u; tap < filter.coefficients.size(); ++tap)
        {
            sum += static_cast<long double>(filter.coefficients[tap])
                 * static_cast<long double>(input[firstInput + tap]);
        }
        result.samples.push_back(static_cast<double>(sum));
    }
    return result;
}

template <RealSample Sample, typename Allocator>
[[nodiscard]] inline HostAlignedSignal decimateToHostBoundary(
    const std::vector<Sample, Allocator>& input, std::size_t factor,
    const KaiserLowPass& filter)
{
    return decimateToHostBoundary(
        std::span<const Sample>(input.data(), input.size()), factor, filter);
}

struct AlignedRmsComparison
{
    std::int64_t firstHostFrame {};
    std::size_t frameCount {};
    RmsComparison rms;
};

[[nodiscard]] inline AlignedRmsComparison compareAlignedRms(
    const HostAlignedSignal& reference, const HostAlignedSignal& candidate,
    double floorDb = defaultDecibelFloor)
{
    const double rateScale = std::max({ 1.0,
        std::abs(reference.hostSampleRateHz),
        std::abs(candidate.hostSampleRateHz) });
    if (!std::isfinite(reference.hostSampleRateHz)
        || !std::isfinite(candidate.hostSampleRateHz)
        || std::abs(reference.hostSampleRateHz - candidate.hostSampleRateHz)
               > 16.0 * std::numeric_limits<double>::epsilon() * rateScale)
        throw std::invalid_argument("aligned signals have different host rates");

    const std::int64_t first = std::max(reference.firstHostFrame,
                                        candidate.firstHostFrame);
    const std::int64_t end = std::min(reference.endHostFrameExclusive(),
                                      candidate.endHostFrameExclusive());
    if (end <= first)
        throw std::invalid_argument("aligned signals have no host-time overlap");
    const std::size_t count = static_cast<std::size_t>(end - first);
    const std::size_t referenceOffset = static_cast<std::size_t>(
        first - reference.firstHostFrame);
    const std::size_t candidateOffset = static_cast<std::size_t>(
        first - candidate.firstHostFrame);
    AlignedRmsComparison result;
    result.firstHostFrame = first;
    result.frameCount = count;
    result.rms = compareRms(
        std::span<const double>(reference.samples.data() + referenceOffset,
                                count),
        std::span<const double>(candidate.samples.data() + candidateOffset,
                                count),
        floorDb);
    return result;
}

[[nodiscard]] inline std::span<const double> hostFrameSpan(
    const HostAlignedSignal& signal, std::int64_t firstHostFrame,
    std::size_t frameCount)
{
    if (firstHostFrame < signal.firstHostFrame
        || firstHostFrame > signal.endHostFrameExclusive())
        throw std::out_of_range("requested host span starts outside the signal");
    const auto offset = static_cast<std::size_t>(
        firstHostFrame - signal.firstHostFrame);
    if (frameCount > signal.samples.size() - offset)
        throw std::out_of_range("requested host span ends outside the signal");
    return { signal.samples.data() + offset, frameCount };
}

struct FractionalDelaySpecification
{
    std::size_t interpolationHalfWidth { 64u };
    double kaiserBeta { 12.0 };
};

struct FractionalDelayAlignedSignal
{
    std::vector<double> samples;
    double hostSampleRateHz {};
    double declaredDelayHostFrames {};
    double appliedAdvanceHostFrames {};
    std::size_t interpolationHalfWidth {};
    std::int64_t inputFirstHostFrame {};
    std::size_t croppedInputFramesBefore {};
    std::size_t croppedInputFramesAfter {};
    std::int64_t firstHostFrame {};

    [[nodiscard]] std::int64_t endHostFrameExclusive() const noexcept
    {
        return firstHostFrame + static_cast<std::int64_t>(samples.size());
    }
};

// Remove one explicitly supplied positive delay.  This is not a lag search:
// output host frame k is evaluated at observed input position k + delay.  A
// Kaiser-windowed sinc supplies the fractional sample, and the result records
// both the applied advance and the full-support crop.
template <RealSample Sample>
[[nodiscard]] inline FractionalDelayAlignedSignal compensateFractionalDelay(
    std::span<const Sample> input, double hostSampleRateHz,
    double declaredDelayHostFrames, std::int64_t inputFirstHostFrame = 0,
    const FractionalDelaySpecification& specification = {})
{
    if (input.empty() || !std::isfinite(hostSampleRateHz)
        || hostSampleRateHz <= 0.0
        || !std::isfinite(declaredDelayHostFrames)
        || declaredDelayHostFrames < 0.0
        || specification.interpolationHalfWidth < 2u
        || !std::isfinite(specification.kaiserBeta)
        || specification.kaiserBeta < 0.0)
        throw std::invalid_argument("fractional-delay specification is invalid");
    if (input.size() > static_cast<std::size_t>(
            std::numeric_limits<std::int64_t>::max()))
        throw std::invalid_argument("fractional-delay input is too long");

    const double halfWidth = static_cast<double>(
        specification.interpolationHalfWidth);
    const double lowerHostFrame =
        static_cast<double>(inputFirstHostFrame) + halfWidth
        - declaredDelayHostFrames;
    const double upperHostFrame =
        static_cast<double>(inputFirstHostFrame)
        + static_cast<double>(input.size() - 1u) - halfWidth
        - declaredDelayHostFrames;
    const auto firstHostFrame = static_cast<std::int64_t>(
        std::ceil(lowerHostFrame));
    const auto lastHostFrame = static_cast<std::int64_t>(
        std::floor(upperHostFrame));
    if (lastHostFrame < firstHostFrame)
        throw std::invalid_argument(
            "fractional-delay input has no full-support output frame");

    FractionalDelayAlignedSignal result;
    result.hostSampleRateHz = hostSampleRateHz;
    result.declaredDelayHostFrames = declaredDelayHostFrames;
    result.appliedAdvanceHostFrames = declaredDelayHostFrames;
    result.interpolationHalfWidth = specification.interpolationHalfWidth;
    result.inputFirstHostFrame = inputFirstHostFrame;
    result.firstHostFrame = firstHostFrame;
    result.samples.reserve(static_cast<std::size_t>(
        lastHostFrame - firstHostFrame + 1));

    const double denominator = detail::modifiedBesselI0(
        specification.kaiserBeta);
    for (std::int64_t hostFrame = firstHostFrame;
         hostFrame <= lastHostFrame; ++hostFrame)
    {
        const double inputPosition = static_cast<double>(hostFrame)
            + declaredDelayHostFrames
            - static_cast<double>(inputFirstHostFrame);
        const auto firstInput = static_cast<std::int64_t>(
            std::ceil(inputPosition - halfWidth));
        const auto lastInput = static_cast<std::int64_t>(
            std::floor(inputPosition + halfWidth));
        long double weightedSum = 0.0L;
        long double weightSum = 0.0L;
        for (std::int64_t inputFrame = firstInput;
             inputFrame <= lastInput; ++inputFrame)
        {
            const double distance = inputPosition
                                  - static_cast<double>(inputFrame);
            const double normalisedDistance = distance / halfWidth;
            const double windowArgument = specification.kaiserBeta
                * std::sqrt(std::max(0.0,
                    1.0 - normalisedDistance * normalisedDistance));
            const double window = detail::modifiedBesselI0(windowArgument)
                                / denominator;
            const double sinc = distance == 0.0
                ? 1.0
                : std::sin(std::numbers::pi_v<double> * distance)
                  / (std::numbers::pi_v<double> * distance);
            const long double weight = static_cast<long double>(window * sinc);
            weightedSum += weight * static_cast<long double>(
                input[static_cast<std::size_t>(inputFrame)]);
            weightSum += weight;
        }
        if (std::abs(weightSum)
            <= std::numeric_limits<long double>::epsilon())
            throw std::runtime_error(
                "fractional-delay interpolator has zero DC gain");
        result.samples.push_back(static_cast<double>(weightedSum / weightSum));
    }

    const double firstInputPosition = static_cast<double>(firstHostFrame)
        + declaredDelayHostFrames
        - static_cast<double>(inputFirstHostFrame);
    const double lastInputPosition = static_cast<double>(lastHostFrame)
        + declaredDelayHostFrames
        - static_cast<double>(inputFirstHostFrame);
    result.croppedInputFramesBefore = static_cast<std::size_t>(
        std::floor(firstInputPosition));
    result.croppedInputFramesAfter = input.size() - 1u
        - static_cast<std::size_t>(std::ceil(lastInputPosition));
    return result;
}

template <RealSample Sample, typename Allocator>
[[nodiscard]] inline FractionalDelayAlignedSignal compensateFractionalDelay(
    const std::vector<Sample, Allocator>& input, double hostSampleRateHz,
    double declaredDelayHostFrames, std::int64_t inputFirstHostFrame = 0,
    const FractionalDelaySpecification& specification = {})
{
    return compensateFractionalDelay(
        std::span<const Sample>(input.data(), input.size()), hostSampleRateHz,
        declaredDelayHostFrames, inputFirstHostFrame, specification);
}

[[nodiscard]] inline std::span<const double> hostFrameSpan(
    const FractionalDelayAlignedSignal& signal,
    std::int64_t firstHostFrame, std::size_t frameCount)
{
    if (firstHostFrame < signal.firstHostFrame
        || firstHostFrame > signal.endHostFrameExclusive())
        throw std::out_of_range("requested host span starts outside the signal");
    const auto offset = static_cast<std::size_t>(
        firstHostFrame - signal.firstHostFrame);
    if (frameCount > signal.samples.size() - offset)
        throw std::out_of_range("requested host span ends outside the signal");
    return { signal.samples.data() + offset, frameCount };
}

[[nodiscard]] inline AlignedRmsComparison compareAlignedRms(
    const HostAlignedSignal& reference,
    const FractionalDelayAlignedSignal& candidate,
    double floorDb = defaultDecibelFloor)
{
    const double rateScale = std::max({ 1.0,
        std::abs(reference.hostSampleRateHz),
        std::abs(candidate.hostSampleRateHz) });
    if (!std::isfinite(reference.hostSampleRateHz)
        || !std::isfinite(candidate.hostSampleRateHz)
        || std::abs(reference.hostSampleRateHz - candidate.hostSampleRateHz)
               > 16.0 * std::numeric_limits<double>::epsilon() * rateScale)
        throw std::invalid_argument("aligned signals have different host rates");
    const std::int64_t first = std::max(reference.firstHostFrame,
                                        candidate.firstHostFrame);
    const std::int64_t end = std::min(reference.endHostFrameExclusive(),
                                      candidate.endHostFrameExclusive());
    if (end <= first)
        throw std::invalid_argument("aligned signals have no host-time overlap");

    AlignedRmsComparison result;
    result.firstHostFrame = first;
    result.frameCount = static_cast<std::size_t>(end - first);
    result.rms = compareRms(hostFrameSpan(reference, first, result.frameCount),
                            hostFrameSpan(candidate, first, result.frameCount),
                            floorDb);
    return result;
}

enum class ProjectionWindow
{
    Rectangular,
    Hann,
    BlackmanHarris92Db
};

[[nodiscard]] inline double projectionWindowValue(
    ProjectionWindow window, std::size_t frame, std::size_t frameCount) noexcept
{
    if (frameCount <= 1u || window == ProjectionWindow::Rectangular)
        return 1.0;
    const double phase = 2.0 * std::numbers::pi_v<double>
                       * static_cast<double>(frame)
                       / static_cast<double>(frameCount - 1u);
    if (window == ProjectionWindow::Hann)
        return 0.5 - 0.5 * std::cos(phase);

    // Four-term minimum-sidelobe Blackman-Harris window (about -92 dB first
    // sidelobe), useful when an exact-frequency projection is near a strong
    // non-coherent neighbour.
    constexpr double a0 = 0.35875;
    constexpr double a1 = 0.48829;
    constexpr double a2 = 0.14128;
    constexpr double a3 = 0.01168;
    return a0 - a1 * std::cos(phase)
              + a2 * std::cos(2.0 * phase)
              - a3 * std::cos(3.0 * phase);
}

struct ToneProjection
{
    std::complex<double> complexAmplitude {};
    double amplitude {};
    double rms {};
    double phaseRadians {};
    double coherentGain {};
    double windowEnergyGain {};
    std::size_t frameCount {};
};

// Project directly at frequencyHz; no FFT bin snapping is involved.  The
// optional firstFrameOnTimeline makes phase comparable between cropped signals.
template <RealSample Sample>
[[nodiscard]] inline ToneProjection projectTone(
    std::span<const Sample> samples, double sampleRateHz, double frequencyHz,
    ProjectionWindow window = ProjectionWindow::Hann,
    std::int64_t firstFrameOnTimeline = 0)
{
    const double nyquist = 0.5 * sampleRateHz;
    if (samples.empty() || !std::isfinite(sampleRateHz)
        || sampleRateHz <= 0.0 || !std::isfinite(frequencyHz)
        || std::abs(frequencyHz) > nyquist)
        throw std::invalid_argument(
            "tone projection requires audio and a finite Nyquist-bounded tone");

    const long double angularFrequency =
        2.0L * std::numbers::pi_v<long double>
        * static_cast<long double>(frequencyHz)
        / static_cast<long double>(sampleRateHz);
    const long double initialPhase = std::remainder(
        angularFrequency * static_cast<long double>(firstFrameOnTimeline),
        2.0L * std::numbers::pi_v<long double>);
    long double real = 0.0L;
    long double imaginary = 0.0L;
    long double windowSum = 0.0L;
    long double windowSquares = 0.0L;
    for (std::size_t frame = 0u; frame < samples.size(); ++frame)
    {
        const long double weight = static_cast<long double>(
            projectionWindowValue(window, frame, samples.size()));
        const long double phase = initialPhase
                                + angularFrequency
                                * static_cast<long double>(frame);
        const long double weightedSample = weight
            * static_cast<long double>(samples[frame]);
        real += weightedSample * std::cos(phase);
        imaginary -= weightedSample * std::sin(phase);
        windowSum += weight;
        windowSquares += weight * weight;
    }
    if (windowSum <= std::numeric_limits<long double>::epsilon())
        throw std::runtime_error("tone-projection window has zero coherent gain");

    const double edgeTolerance = 16.0
        * std::numeric_limits<double>::epsilon() * sampleRateHz;
    const bool oneSidedEdge = std::abs(frequencyHz) <= edgeTolerance
        || std::abs(std::abs(frequencyHz) - nyquist) <= edgeTolerance;
    const long double amplitudeScale = oneSidedEdge
        ? 1.0L / windowSum : 2.0L / windowSum;
    ToneProjection result;
    result.complexAmplitude = {
        static_cast<double>(real * amplitudeScale),
        static_cast<double>(imaginary * amplitudeScale) };
    result.amplitude = std::abs(result.complexAmplitude);
    result.rms = oneSidedEdge
        ? result.amplitude
        : result.amplitude / std::sqrt(2.0);
    result.phaseRadians = std::arg(result.complexAmplitude);
    result.coherentGain = static_cast<double>(
        windowSum / static_cast<long double>(samples.size()));
    result.windowEnergyGain = std::sqrt(static_cast<double>(
        windowSquares / static_cast<long double>(samples.size())));
    result.frameCount = samples.size();
    return result;
}

template <RealSample Sample, typename Allocator>
[[nodiscard]] inline ToneProjection projectTone(
    const std::vector<Sample, Allocator>& samples, double sampleRateHz,
    double frequencyHz, ProjectionWindow window = ProjectionWindow::Hann,
    std::int64_t firstFrameOnTimeline = 0)
{
    return projectTone(
        std::span<const Sample>(samples.data(), samples.size()), sampleRateHz,
        frequencyHz, window, firstFrameOnTimeline);
}

struct ReferencePathSelfCheck
{
    double hostSampleRateHz {};
    double sourceSampleRateHz {};
    std::size_t decimationFactor {};
    double passbandEdgeHz {};
    double stopbandEdgeHz {};
    std::size_t tapCount {};
    std::size_t groupDelayInputFrames {};
    std::size_t groupDelayHostFrames {};
    ReferenceFilterCheck filter;
    double toneFrequencyHz {};
    double projectedToneAmplitude {};
    double projectedToneGainDb {};
    double projectedTonePhaseRadians {};
    double analyticToneGainDb {};
    double analyticTonePhaseRadians {};
    double projectionVersusResponseDb {};
    double projectionVersusResponsePhaseRadians {};
    double maximumToneGainErrorDb {};
    double maximumTonePhaseErrorRadians { 1.0e-8 };
    double stopToneFrequencyHz {};
    double projectedStopAliasGainDb {};
    double analyticStopGainDb {};
    double stopProjectionVersusResponseDb {};
    double maximumStopProjectionErrorDb { 0.01 };
    double minimumStopPathAttenuationDb { 110.0 };
    double measuredPassbandRippleDb {};
    double measuredWorstStopbandDb {};
    bool tonePassed {};
    bool stopbandWasMeasured {};

    [[nodiscard]] bool passed() const noexcept
    {
        return filter.passed() && tonePassed && stopbandWasMeasured;
    }
};

// Exercise the complete independent FIR/decimation/projection path at a fixed
// 16x source grid.  The 44.1 kHz case deliberately gives the reference its
// narrower and therefore harder 20.0 -> 22.05 kHz transition.  Compliance is
// based on the measured response extrema, never on the Kaiser order estimate.
[[nodiscard]] inline ReferencePathSelfCheck runReferencePathSelfCheck(
    double hostSampleRateHz)
{
    constexpr std::size_t factor = 16u;
    constexpr std::size_t analysisHostFrames = 2048u;
    constexpr double passbandEdgeHz = 20000.0;
    constexpr double toneFrequencyHz = 12000.0;
    constexpr double maximumToneGainErrorDb = 0.001;

    if (!std::isfinite(hostSampleRateHz)
        || hostSampleRateHz <= 2.0 * passbandEdgeHz)
        throw std::invalid_argument(
            "reference self-check host rate cannot contain its passband");
    const LowPassSpecification specification {
        hostSampleRateHz * static_cast<double>(factor),
        passbandEdgeHz,
        0.5 * hostSampleRateHz,
        referenceFilterAttenuationDb,
        referenceFilterTapCount
    };
    const KaiserLowPass lowPass = designKaiserLowPass(specification);
    const ReferenceFilterCheck filterCheck = checkReferenceFilter(lowPass);

    const std::size_t sourceFrames = lowPass.coefficients.size()
        + factor * (analysisHostFrames - 1u);
    std::vector<double> source(sourceFrames);
    for (std::size_t frame = 0u; frame < source.size(); ++frame)
    {
        source[frame] = std::sin(
            2.0 * std::numbers::pi_v<double> * toneFrequencyHz
            * static_cast<double>(frame) / specification.sampleRateHz);
    }
    const HostAlignedSignal host = decimateToHostBoundary(
        source, factor, lowPass);
    if (host.samples.size() != analysisHostFrames)
        throw std::runtime_error(
            "reference self-check produced an unexpected host-frame count");
    const ToneProjection projection = projectTone(
        host.samples, host.hostSampleRateHz, toneFrequencyHz,
        ProjectionWindow::Hann, host.firstHostFrame);
    const auto analyticComplexAmplitude = std::complex<double> { 0.0, -1.0 }
        * zeroPhaseFrequencyResponse(lowPass, toneFrequencyHz);
    const double analyticAmplitude = std::abs(analyticComplexAmplitude);
    const double projectedGainDb = amplitudeToDecibels(projection.amplitude);
    const double analyticGainDb = amplitudeToDecibels(analyticAmplitude);
    const double projectionVersusResponseDb =
        20.0 * std::log10(projection.amplitude / analyticAmplitude);
    const double analyticPhase = std::arg(analyticComplexAmplitude);
    const double projectionVersusResponsePhase = std::remainder(
        projection.phaseRadians - analyticPhase,
        2.0 * std::numbers::pi_v<double>);

    // Drive the executable path with a source that aliases onto the passband
    // probe if the FIR is bypassed.  The coefficient-domain sweep above proves
    // the design; this second render proves decimateToHostBoundary actually
    // applies that design instead of merely subsampling it.
    const double stopToneFrequencyHz = hostSampleRateHz - toneFrequencyHz;
    std::vector<double> stopSource(sourceFrames);
    for (std::size_t frame = 0u; frame < stopSource.size(); ++frame)
    {
        stopSource[frame] = std::sin(
            2.0 * std::numbers::pi_v<double> * stopToneFrequencyHz
            * static_cast<double>(frame) / specification.sampleRateHz);
    }
    const HostAlignedSignal stopHost = decimateToHostBoundary(
        stopSource, factor, lowPass);
    const ToneProjection stopProjection = projectTone(
        stopHost.samples, stopHost.hostSampleRateHz, toneFrequencyHz,
        ProjectionWindow::Hann, stopHost.firstHostFrame);
    const double analyticStopAmplitude = std::abs(
        zeroPhaseFrequencyResponse(lowPass, stopToneFrequencyHz));
    const double projectedStopAliasGainDb = amplitudeToDecibels(
        stopProjection.amplitude);
    const double analyticStopGainDb = amplitudeToDecibels(
        analyticStopAmplitude);
    const double stopProjectionVersusResponseDb =
        projectedStopAliasGainDb - analyticStopGainDb;

    ReferencePathSelfCheck result;
    result.hostSampleRateHz = hostSampleRateHz;
    result.sourceSampleRateHz = specification.sampleRateHz;
    result.decimationFactor = factor;
    result.passbandEdgeHz = specification.passbandEdgeHz;
    result.stopbandEdgeHz = specification.stopbandEdgeHz;
    result.tapCount = specification.tapCount;
    result.groupDelayInputFrames = lowPass.groupDelayInputFrames;
    result.groupDelayHostFrames = lowPass.groupDelayInputFrames / factor;
    result.filter = filterCheck;
    result.toneFrequencyHz = toneFrequencyHz;
    result.projectedToneAmplitude = projection.amplitude;
    result.projectedToneGainDb = projectedGainDb;
    result.projectedTonePhaseRadians = projection.phaseRadians;
    result.analyticToneGainDb = analyticGainDb;
    result.analyticTonePhaseRadians = analyticPhase;
    result.projectionVersusResponseDb = projectionVersusResponseDb;
    result.projectionVersusResponsePhaseRadians =
        projectionVersusResponsePhase;
    result.maximumToneGainErrorDb = maximumToneGainErrorDb;
    result.stopToneFrequencyHz = stopToneFrequencyHz;
    result.projectedStopAliasGainDb = projectedStopAliasGainDb;
    result.analyticStopGainDb = analyticStopGainDb;
    result.stopProjectionVersusResponseDb =
        stopProjectionVersusResponseDb;
    result.measuredPassbandRippleDb =
        filterCheck.convergence.fine.passbandRippleDb;
    result.measuredWorstStopbandDb =
        filterCheck.convergence.fine.stopbandMaximumDb;
    result.tonePassed = std::isfinite(projectedGainDb)
        && std::abs(projectedGainDb) <= maximumToneGainErrorDb
        && std::isfinite(projectionVersusResponseDb)
        && std::abs(projectionVersusResponseDb) <= 0.0001
        && std::isfinite(projectionVersusResponsePhase)
        && std::abs(projectionVersusResponsePhase)
               <= result.maximumTonePhaseErrorRadians;
    result.stopbandWasMeasured = std::isfinite(projectedStopAliasGainDb)
        && projectedStopAliasGainDb
               <= -result.minimumStopPathAttenuationDb
        && std::isfinite(analyticStopGainDb)
        && std::isfinite(stopProjectionVersusResponseDb)
        && std::abs(stopProjectionVersusResponseDb)
               <= result.maximumStopProjectionErrorDb;
    return result;
}

[[nodiscard]] inline std::array<ReferencePathSelfCheck, 2>
runCanonicalReferencePathSelfChecks()
{
    return { runReferencePathSelfCheck(44100.0),
             runReferencePathSelfCheck(48000.0) };
}

struct FractionalDelaySelfCheck
{
    double hostSampleRateHz {};
    double declaredDelayHostFrames {};
    double appliedAdvanceHostFrames {};
    std::size_t interpolationHalfWidth {};
    std::int64_t firstHostFrame {};
    std::size_t frameCount {};
    std::size_t croppedInputFramesBefore {};
    std::size_t croppedInputFramesAfter {};
    RmsComparison alignedRms;
    double maximumToneAmplitudeDeltaDb {};
    double maximumTonePhaseDeltaRadians {};
    double requiredRelativeRmsErrorDb { -100.0 };
    double maximumAllowedToneAmplitudeDeltaDb { 0.001 };
    double maximumAllowedTonePhaseDeltaRadians { 1.0e-5 };

    [[nodiscard]] bool passed() const noexcept
    {
        return std::isfinite(alignedRms.relativeErrorDb)
            && alignedRms.relativeErrorDb <= requiredRelativeRmsErrorDb
            && std::isfinite(maximumToneAmplitudeDeltaDb)
            && maximumToneAmplitudeDeltaDb
                   <= maximumAllowedToneAmplitudeDeltaDb
            && std::isfinite(maximumTonePhaseDeltaRadians)
            && maximumTonePhaseDeltaRadians
                   <= maximumAllowedTonePhaseDeltaRadians
            && appliedAdvanceHostFrames == declaredDelayHostFrames;
    }
};

// Delay a deterministic, non-bin-centred two-tone signal analytically, then
// require the explicit sinc advance to recover its undelayed host timeline.
// This tests the exact path used to align isolated q-grid candidates; it never
// searches for a more favourable lag.
[[nodiscard]] inline FractionalDelaySelfCheck
runFractionalDelaySelfCheck(double declaredDelayHostFrames)
{
    constexpr double hostSampleRateHz = 48000.0;
    constexpr std::size_t inputFrames = 16384u;
    constexpr std::array frequenciesHz { 3001.25, 11987.5 };
    constexpr std::array amplitudes { 0.70, 0.30 };
    constexpr std::array phases { 0.37, -0.91 };
    const auto valueAt = [&](double hostFrame) {
        double value = 0.0;
        for (std::size_t tone = 0u; tone < frequenciesHz.size(); ++tone)
        {
            value += amplitudes[tone] * std::sin(
                2.0 * std::numbers::pi_v<double> * frequenciesHz[tone]
                * hostFrame / hostSampleRateHz + phases[tone]);
        }
        return value;
    };

    std::vector<double> delayed(inputFrames);
    for (std::size_t frame = 0u; frame < delayed.size(); ++frame)
        delayed[frame] = valueAt(static_cast<double>(frame)
                               - declaredDelayHostFrames);
    const FractionalDelayAlignedSignal aligned = compensateFractionalDelay(
        delayed, hostSampleRateHz, declaredDelayHostFrames);
    std::vector<double> expected(aligned.samples.size());
    for (std::size_t offset = 0u; offset < expected.size(); ++offset)
    {
        expected[offset] = valueAt(static_cast<double>(
            aligned.firstHostFrame + static_cast<std::int64_t>(offset)));
    }

    double maximumAmplitudeDeltaDb = 0.0;
    double maximumPhaseDelta = 0.0;
    for (const double frequencyHz : frequenciesHz)
    {
        const ToneProjection expectedProjection = projectTone(
            expected, hostSampleRateHz, frequencyHz,
            ProjectionWindow::BlackmanHarris92Db, aligned.firstHostFrame);
        const ToneProjection actualProjection = projectTone(
            aligned.samples, hostSampleRateHz, frequencyHz,
            ProjectionWindow::BlackmanHarris92Db, aligned.firstHostFrame);
        maximumAmplitudeDeltaDb = std::max(maximumAmplitudeDeltaDb,
            std::abs(20.0 * std::log10(actualProjection.amplitude
                                      / expectedProjection.amplitude)));
        maximumPhaseDelta = std::max(maximumPhaseDelta, std::abs(
            std::remainder(actualProjection.phaseRadians
                         - expectedProjection.phaseRadians,
                           2.0 * std::numbers::pi_v<double>)));
    }

    FractionalDelaySelfCheck result;
    result.hostSampleRateHz = hostSampleRateHz;
    result.declaredDelayHostFrames = declaredDelayHostFrames;
    result.appliedAdvanceHostFrames = aligned.appliedAdvanceHostFrames;
    result.interpolationHalfWidth = aligned.interpolationHalfWidth;
    result.firstHostFrame = aligned.firstHostFrame;
    result.frameCount = aligned.samples.size();
    result.croppedInputFramesBefore = aligned.croppedInputFramesBefore;
    result.croppedInputFramesAfter = aligned.croppedInputFramesAfter;
    result.alignedRms = compareRms(
        std::span<const double>(expected.data(), expected.size()),
        std::span<const double>(aligned.samples.data(), aligned.samples.size()));
    result.maximumToneAmplitudeDeltaDb = maximumAmplitudeDeltaDb;
    result.maximumTonePhaseDeltaRadians = maximumPhaseDelta;
    return result;
}

[[nodiscard]] inline std::array<FractionalDelaySelfCheck, 2>
runCanonicalFractionalDelaySelfChecks()
{
    return { runFractionalDelaySelfCheck(23.5),
             runFractionalDelaySelfCheck(35.25) };
}

} // namespace youknow::oversampling_quality
