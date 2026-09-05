#pragma once

#include <algorithm>
#include <cmath>

namespace taikor
{
// Zero-latency, stereo-linked sample-peak limiter. The 1 ms hold and 80 ms
// release keep overload attenuation across a drum's waveform instead of
// clipping each channel independently. This is not a true-peak oversampler.
class OutputLimiter
{
public:
    static constexpr float ceiling = 0.891250938f; // -1 dBFS

    void prepare (double sampleRate) noexcept
    {
        const auto rate = std::isfinite (sampleRate)
            ? std::clamp (sampleRate, 8000.0, 768000.0) : 48000.0;
        releaseStep = -std::expm1 (-1.0 / (0.080 * rate));
        holdSamples = static_cast<int> (std::ceil (0.001 * rate));
        reset();
    }

    void reset() noexcept
    {
        gain = 1.0;
        holdRemaining = 0;
    }

    void process (float& left, float& right) noexcept
    {
        if (! std::isfinite (left) || ! std::isfinite (right))
        {
            left = right = 0.0f;
            reset();
            return;
        }

        const auto peak = static_cast<double> (
            std::max (std::abs (left), std::abs (right)));
        const double requiredGain = peak > ceiling ? ceiling / peak : 1.0;
        if (requiredGain <= gain && requiredGain < 1.0)
        {
            gain = requiredGain;
            holdRemaining = holdSamples;
        }
        else if (holdRemaining > 0)
        {
            --holdRemaining;
        }
        else
        {
            gain = std::min (requiredGain, gain + releaseStep * (1.0 - gain));
        }

        // Double precision keeps even very large finite inputs bounded; clamp
        // only catches rounding at the ceiling after the shared gain change.
        left = std::clamp (static_cast<float> (left * gain), -ceiling, ceiling);
        right = std::clamp (static_cast<float> (right * gain), -ceiling, ceiling);
    }

private:
    double gain = 1.0;
    double releaseStep = -std::expm1 (-1.0 / (0.080 * 48000.0));
    int holdSamples = 48;
    int holdRemaining = 0;
};
} // namespace taikor
