#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace taikor
{
// One-pole (6 dB/octave) high-pass, with a prewarped bilinear integrator:
// https://dsprelated.com/freebooks/pasp/Bilinear_Transformation.html
// Coefficient and wet-gain smoothing keep automation continuous; fading out
// at the last active cutoff avoids freezing a DC offset as cutoff reaches 0.
class OutputHighPass
{
public:
    void prepare (double rate) noexcept
    {
        sampleRate = std::isfinite (rate) ? std::clamp (rate, 8000.0, 768000.0)
                                        : 48000.0;
        smoothing = -std::expm1 (-1.0 / (0.015 * sampleRate));
        setCutoff (cutoff);
        reset();
    }

    void setCutoff (float hertz) noexcept
    {
        cutoff = std::isnan (hertz) ? 0.0f : std::clamp (hertz, 0.0f, 500.0f);
        targetWet = cutoff > 0.0f ? 1.0 : 0.0;
        if (cutoff > 0.0f)
        {
            const double g = std::tan (3.14159265358979323846 * cutoff / sampleRate);
            targetCoefficient = g / (1.0 + g);
        }
    }

    void reset() noexcept
    {
        state.fill (0.0);
        coefficient = targetCoefficient;
        wet = targetWet;
    }

    void process (float& left, float& right) noexcept
    {
        if (wet == 0.0 && targetWet == 0.0)
            return; // Exact bypass, including the existing output samples.

        coefficient += smoothing * (targetCoefficient - coefficient);
        wet += smoothing * (targetWet - wet);
        if (targetWet == 0.0 && wet < 1.0e-7)
        {
            reset();
            return;
        }

        if (! std::isfinite (left) || ! std::isfinite (right))
        {
            left = right = 0.0f;
            reset();
            return;
        }

        const auto filter = [this] (float& sample, double& memory) noexcept
        {
            const double v = (static_cast<double> (sample) - memory) * coefficient;
            const double low = v + memory;
            memory = low + v;
            sample = static_cast<float> (sample - wet * low);
        };
        filter (left, state[0]);
        filter (right, state[1]);
    }

private:
    double sampleRate = 48000.0;
    float cutoff = 0.0f;
    double smoothing = -std::expm1 (-1.0 / (0.015 * 48000.0));
    double coefficient = 0.0, targetCoefficient = 0.0;
    double wet = 0.0, targetWet = 0.0;
    std::array<double, 2> state {};
};
} // namespace taikor
