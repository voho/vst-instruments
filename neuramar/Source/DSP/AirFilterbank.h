#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

namespace neuramar::airfilter
{

struct Coefficients
{
    float b0 { 0.0f };
    float b2 { 0.0f };
    float a1 { 0.0f };
    float a2 { 0.0f };
    float outputScale { 1.0f };
};

[[nodiscard]] inline Coefficients makeCoefficients(
    float centreHz, float bandwidthOctaves, float sampleRate) noexcept
{
    const float safeRate = std::clamp(
        std::isfinite(sampleRate) ? sampleRate : 48000.0f, 8000.0f, 768000.0f);
    const float frequency = std::clamp(
        std::isfinite(centreHz) ? centreHz : 1000.0f,
        20.0f, 0.45f * safeRate);
    const float width = std::clamp(
        std::isfinite(bandwidthOctaves) ? bandwidthOctaves : 1.0f,
        0.10f, 4.0f);
    const float q = std::clamp(
        1.0f / (2.0f * std::sinh(0.5f * 0.6931471805599453f * width)),
        0.25f, 8.0f);
    const float omega = 6.28318530717958647692f * frequency / safeRate;
    const float alpha = std::sin(omega) / (2.0f * q);
    const float inverseA0 = 1.0f / (1.0f + alpha);

    Coefficients result;
    result.b0 = alpha * inverseA0;
    result.b2 = -result.b0;
    result.a1 = -2.0f * std::cos(omega) * inverseA0;
    result.a2 = (1.0f - alpha) * inverseA0;

    // This RBJ band-pass has an exact white-noise variance gain of b0.
    // nextNoise() is uniform [-1, 1] with variance 1/3, so sqrt(3 / b0)
    // makes every band produce unit RMS before its learned amplitude is
    // applied. That keeps Air level independent of centre and sample rate.
    result.outputScale = std::sqrt(3.0f / std::max(result.b0, 1.0e-8f));
    return result;
}

[[nodiscard]] inline float unitNoisePowerResponse(
    const Coefficients& coefficients, float frequencyHz,
    float sampleRate) noexcept
{
    if (!(frequencyHz > 0.0f) || !(sampleRate > 0.0f)
        || frequencyHz >= 0.5f * sampleRate)
        return 0.0f;

    const float omega = 6.28318530717958647692f * frequencyHz / sampleRate;
    const std::complex<float> delay(std::cos(omega), -std::sin(omega));
    const auto delay2 = delay * delay;
    const auto numerator = coefficients.b0 + coefficients.b2 * delay2;
    const auto denominator = 1.0f + coefficients.a1 * delay
        + coefficients.a2 * delay2;
    if (std::norm(denominator) <= 1.0e-12f)
        return 0.0f;

    // The filter output is scaled for unit RMS uniform noise. Multiplying
    // |H|^2 by outputScale^2 and the input variance 1/3 gives its PSD.
    return std::norm(numerator / denominator)
        * coefficients.outputScale * coefficients.outputScale / 3.0f;
}

} // namespace neuramar::airfilter
