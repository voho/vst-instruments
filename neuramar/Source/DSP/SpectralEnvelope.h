#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace neuramar::spectral
{

// A local, shape-preserving interpolator for positive harmonic magnitudes.
// The log1p compander behaves linearly around silence and logarithmically for
// audible partials. Every learned harmonic remains an exact knot and interval
// clamping prevents a cubic from inventing a resonance between observations.
//
// The two evidence shoulders are deliberately asymmetric. Below the first
// observed partial the envelope holds its lowest observed value: the renderer
// multiplies it by the complementary excitation residual, so holding makes
// Body Lock degenerate exactly to pitch-following there instead of notching
// partials 2-4 of every deeply transposed note by up to 12 dB. Above the last
// observed partial it still fades linearly to zero within one interval,
// because inventing energy above the measured envelope is what Body Lock
// exists to prevent.
template <std::size_t HarmonicCount>
class ShapePreservingEnvelope final
{
    static_assert(HarmonicCount >= 3);

public:
    static constexpr float compandingKnee = 1.0e-5f;
    void prepare(const std::array<float, HarmonicCount>& harmonics) noexcept
    {
        for (std::size_t harmonic = 0; harmonic < HarmonicCount; ++harmonic)
        {
            const float input = harmonics[harmonic];
            const float magnitude = std::isfinite(input)
                ? std::max(input, 0.0f) : 0.0f;
            magnitudes_[harmonic] = magnitude;
            companded_[harmonic] = std::log1p(
                magnitude / compandingKnee);
        }

        for (std::size_t knot = 1; knot + 1 < HarmonicCount; ++knot)
        {
            const float leftDelta = companded_[knot]
                - companded_[knot - 1];
            const float rightDelta = companded_[knot + 1]
                - companded_[knot];
            slopes_[knot] = monotoneSlope(leftDelta, rightDelta);
        }

        slopes_.front() = endpointSlope(companded_[1] - companded_[0],
                                        companded_[2] - companded_[1]);
        slopes_.back() = endpointSlope(
            companded_[HarmonicCount - 1] - companded_[HarmonicCount - 2],
            companded_[HarmonicCount - 2] - companded_[HarmonicCount - 3]);
    }

    [[nodiscard]] float sample(float oneBasedCoordinate) const noexcept
    {
        constexpr float lastHarmonic = static_cast<float>(HarmonicCount);
        constexpr float lastCoordinate = lastHarmonic + 1.0f;
        if (!std::isfinite(oneBasedCoordinate)
            || !(oneBasedCoordinate > 0.0f)
            || oneBasedCoordinate >= lastCoordinate)
            return 0.0f;
        if (oneBasedCoordinate < 1.0f)
            return magnitudes_.front();
        if (oneBasedCoordinate > lastHarmonic)
            return magnitudes_.back() * (lastCoordinate - oneBasedCoordinate);

        const float zeroBasedCoordinate = oneBasedCoordinate - 1.0f;
        const auto lower = static_cast<std::size_t>(zeroBasedCoordinate);
        if (lower >= HarmonicCount - 1)
            return magnitudes_.back();
        const float fraction = zeroBasedCoordinate - static_cast<float>(lower);
        if (!(fraction > 0.0f))
            return magnitudes_[lower];

        const float fractionSquared = fraction * fraction;
        const float fractionCubed = fractionSquared * fraction;
        const float firstWeight = 2.0f * fractionCubed
            - 3.0f * fractionSquared + 1.0f;
        const float firstSlopeWeight = fractionCubed
            - 2.0f * fractionSquared + fraction;
        const float secondWeight = -2.0f * fractionCubed
            + 3.0f * fractionSquared;
        const float secondSlopeWeight = fractionCubed - fractionSquared;

        float companded = firstWeight * companded_[lower]
            + firstSlopeWeight * slopes_[lower]
            + secondWeight * companded_[lower + 1]
            + secondSlopeWeight * slopes_[lower + 1];
        const float minimum = std::min(companded_[lower],
                                       companded_[lower + 1]);
        const float maximum = std::max(companded_[lower],
                                       companded_[lower + 1]);
        companded = std::clamp(companded, minimum, maximum);

        const float magnitude = compandingKnee * std::expm1(companded);
        return std::clamp(std::isfinite(magnitude) ? magnitude : 0.0f,
                          std::min(magnitudes_[lower], magnitudes_[lower + 1]),
                          std::max(magnitudes_[lower], magnitudes_[lower + 1]));
    }

private:
    [[nodiscard]] static float monotoneSlope(float leftDelta,
                                             float rightDelta) noexcept
    {
        const bool samePositiveDirection = leftDelta > 0.0f
            && rightDelta > 0.0f;
        const bool sameNegativeDirection = leftDelta < 0.0f
            && rightDelta < 0.0f;
        if (!samePositiveDirection && !sameNegativeDirection)
            return 0.0f;

        // Equal-spacing Fritsch-Butland weighted harmonic mean. It is local,
        // becomes zero at extrema, and cannot create a new extremum on a
        // monotone interval.
        return 2.0f * leftDelta * rightDelta / (leftDelta + rightDelta);
    }

    [[nodiscard]] static float endpointSlope(float edgeDelta,
                                             float adjacentDelta) noexcept
    {
        float slope = 0.5f * (3.0f * edgeDelta - adjacentDelta);
        if ((slope > 0.0f) != (edgeDelta > 0.0f))
            return 0.0f;
        if ((edgeDelta > 0.0f) != (adjacentDelta > 0.0f)
            && std::abs(slope) > 3.0f * std::abs(edgeDelta))
            slope = 3.0f * edgeDelta;
        return slope;
    }

    std::array<float, HarmonicCount> magnitudes_ {};
    std::array<float, HarmonicCount> companded_ {};
    std::array<float, HarmonicCount> slopes_ {};
};

} // namespace neuramar::spectral
