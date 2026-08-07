#include "VocalorMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace vocalor
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

// Cardinal vowel targets. The /a/, /o/ and /u/ rows deliberately reuse the
// values the engine has always used for AAH and OOH so the continuous space
// passes exactly through the historical presets.
constexpr float femaleCardinal[kCardinalVowelCount][kFormantCount] = {
    { 310.0f, 2790.0f, 3310.0f, 3900.0f, 4950.0f },  // /i/  "EE"
    { 610.0f, 2330.0f, 2990.0f, 3800.0f, 4950.0f },  // /e/  "EH"
    { 850.0f, 1220.0f, 2810.0f, 3650.0f, 4950.0f },  // /a/  "AH"
    { 450.0f,  900.0f, 2700.0f, 3600.0f, 4900.0f },  // /o/  "OH"
    { 350.0f,  800.0f, 2650.0f, 3550.0f, 4850.0f }   // /u/  "OO"
};

constexpr float maleCardinal[kCardinalVowelCount][kFormantCount] = {
    { 290.0f, 2270.0f, 2980.0f, 3400.0f, 4400.0f },  // /i/
    { 530.0f, 1840.0f, 2480.0f, 3300.0f, 4300.0f },  // /e/
    { 730.0f, 1090.0f, 2440.0f, 3250.0f, 4300.0f },  // /a/
    { 400.0f,  750.0f, 2500.0f, 3100.0f, 4200.0f },  // /o/
    { 300.0f,  700.0f, 2260.0f, 3050.0f, 4100.0f }   // /u/
};

constexpr float cardinalX[kCardinalVowelCount] { 1.00f, 0.88f, 0.50f, 0.12f, 0.00f };
constexpr float cardinalY[kCardinalVowelCount] { 0.00f, 0.50f, 1.00f, 0.50f, 0.00f };
constexpr const char* cardinalNames[kCardinalVowelCount] { "EE", "EH", "AH", "OH", "OO" };

// The three preset vowels the engine has always shipped, in the same order as
// the "vowel" host parameter.
constexpr float femalePreset[3][kFormantCount] = {
    { 850.0f, 1220.0f, 2810.0f, 3650.0f, 4950.0f },
    { 350.0f,  800.0f, 2650.0f, 3550.0f, 4850.0f },
    { 470.0f, 1120.0f, 2740.0f, 3600.0f, 4860.0f }
};

constexpr float malePreset[3][kFormantCount] = {
    { 730.0f, 1090.0f, 2440.0f, 3250.0f, 4300.0f },
    { 300.0f,  700.0f, 2260.0f, 3050.0f, 4100.0f },
    { 405.0f, 1020.0f, 2380.0f, 3150.0f, 4200.0f }
};

float clampUnit (float value) noexcept
{
    if (! std::isfinite (value))
        return 0.0f;
    return std::clamp (value, 0.0f, 1.0f);
}
} // namespace

VowelPoint cardinalVowelPosition (int index) noexcept
{
    index = std::clamp (index, 0, kCardinalVowelCount - 1);
    return { cardinalX[static_cast<std::size_t> (index)],
             cardinalY[static_cast<std::size_t> (index)] };
}

const char* cardinalVowelName (int index) noexcept
{
    index = std::clamp (index, 0, kCardinalVowelCount - 1);
    return cardinalNames[static_cast<std::size_t> (index)];
}

VowelPoint presetVowelPosition (int vowelIndex) noexcept
{
    switch (std::clamp (vowelIndex, 0, 2))
    {
        case 1:  return { 0.06f, 0.06f };   // OOH sits on the close-back corner
        case 2:  return { 0.24f, 0.34f };   // UUH is a slightly opened schwa
        default: return { 0.50f, 1.00f };   // AAH is the open anchor
    }
}

void formantsForVowelPoint (bool male, float x, float y, float* outHz) noexcept
{
    if (outHz == nullptr)
        return;

    x = clampUnit (x);
    y = clampUnit (y);

    // Softened inverse-distance weighting. The epsilon keeps the weights finite
    // on an exact corner hit while still giving that corner ~96% of the blend.
    constexpr float epsilon = 0.006f;
    std::array<float, kCardinalVowelCount> weights {};
    float total = 0.0f;
    for (int i = 0; i < kCardinalVowelCount; ++i)
    {
        const float dx = x - cardinalX[static_cast<std::size_t> (i)];
        const float dy = y - cardinalY[static_cast<std::size_t> (i)];
        const float weight = 1.0f / (dx * dx + dy * dy + epsilon);
        weights[static_cast<std::size_t> (i)] = weight;
        total += weight;
    }

    const float inverseTotal = 1.0f / total;
    for (int formant = 0; formant < kFormantCount; ++formant)
    {
        float accumulator = 0.0f;
        for (int i = 0; i < kCardinalVowelCount; ++i)
            accumulator += weights[static_cast<std::size_t> (i)]
                * (male ? maleCardinal[i][formant] : femaleCardinal[i][formant]);
        outHz[formant] = accumulator * inverseTotal;
    }
}

void formantsForPresetVowel (bool male, int vowelIndex, float* outHz) noexcept
{
    if (outHz == nullptr)
        return;

    const int vowel = std::clamp (vowelIndex, 0, 2);
    for (int formant = 0; formant < kFormantCount; ++formant)
        outHz[formant] = male ? malePreset[vowel][formant] : femalePreset[vowel][formant];
}

float formantShiftRatio (float semitones) noexcept
{
    if (! std::isfinite (semitones))
        return 1.0f;
    return std::exp2 (std::clamp (semitones, -24.0f, 24.0f) / 12.0f);
}

DynamicResponse dynamicResponse (float dynamics) noexcept
{
    const float below = 1.0f - clampUnit (dynamics);
    DynamicResponse response;
    // Level is linear in dB, which is how a dynamic layer is expected to behave
    // under a controller: an empty wheel is 18.1 dB down on the voiced source.
    response.voicedGain = std::exp2 (-3.00f * below);
    // Aspiration only loses 7.2 dB over the same span. The glottis leaks a
    // larger share of the flow at low effort, so a soft note is breathier as
    // well as quieter rather than being the same sound turned down.
    response.airGain = std::exp2 (-1.20f * below);
    // Vocal effort sets the source spectral tilt, so a soft note is dull. This
    // is the part that has to be large: a dynamic layer that only moves the
    // level by 18 dB and the presence band by the same 18 dB is an output trim.
    response.effortScale = 1.0f - 0.85f * below;
    // ... and it is produced with a laxer glottis, which is a change in the
    // pulse shape itself rather than a filter over a fixed one.
    response.sourceTensionScale = 1.0f - 0.75f * below;
    response.vibratoScale = 1.0f - 0.55f * below;
    return response;
}

float glideTimeSeconds (float glide) noexcept
{
    // Perceptually even: a small knob movement near zero stays snappy.
    const float normalised = clampUnit (glide);
    return 0.600f * normalised * normalised;
}

float roomSizeScale (float size) noexcept
{
    // 0 -> 0.45x, 0.5 -> 1.0x, 1 -> 2.22x of the historical tap lengths.
    return std::exp2 (2.3f * (clampUnit (size) - 0.5f));
}

float formantResonatorGain (float poleRadius, float sinPoleAngle) noexcept
{
    // |1 - a1 z - a2 z^2| evaluated on the pole angle, with a1 = 2r cos and
    // a2 = -r^2, collapses to (1 - r) * sqrt((1 - r)^2 + 4 r sin^2), which is
    // exactly the reciprocal of the resonator's peak gain. Written against the
    // sine rather than the cosine it is a sum of positive terms, so a narrow
    // low formant at 192 kHz does not lose the answer to cancellation.
    const float radius = std::clamp (poleRadius, 0.0f, 0.999999f);
    const float sine = std::clamp (sinPoleAngle, -1.0f, 1.0f);
    const float gap = 1.0f - radius;
    return gap * std::sqrt (gap * gap + 4.0f * radius * sine * sine);
}

void parallelFormantAmplitudes (const float* formantHz, const float* formantBandwidth,
                                int count, float sampleRate, float floorGain,
                                float* outGain) noexcept
{
    if (formantHz == nullptr || formantBandwidth == nullptr || outGain == nullptr
        || count <= 0 || ! (sampleRate > 0.0f))
        return;

    count = std::min (count, kFormantCount);
    std::array<float, kFormantCount> a1 {};
    std::array<float, kFormantCount> a2 {};
    std::array<float, kFormantCount> numerator {};
    std::array<float, kFormantCount> peakDenominator {};
    std::array<float, kFormantCount> cosOmega {};
    std::array<float, kFormantCount> sinOmega {};

    for (int i = 0; i < count; ++i)
    {
        const auto index = static_cast<std::size_t> (i);
        const float centre = std::clamp (formantHz[i], 25.0f, 0.465f * sampleRate);
        const float bandwidth = std::clamp (formantBandwidth[i], 20.0f, 0.25f * sampleRate);
        const float radius = std::exp (-pi * bandwidth / sampleRate);
        const float omega = twoPi * centre / sampleRate;
        cosOmega[index] = std::cos (omega);
        sinOmega[index] = std::sin (omega);
        a1[index] = 2.0f * radius * cosOmega[index];
        a2[index] = -radius * radius;
        // Klatt's resonator normalisation: unity gain at DC, so the cascade
        // product is the tract shape alone rather than an arbitrary scaling.
        numerator[index] = 1.0f - a1[index] - a2[index];
        // On its own pole the general expression below is a difference of two
        // near-equal numbers; the closed form is not.
        peakDenominator[index] = formantResonatorGain (radius, sinOmega[index]);
    }

    float largest = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const auto probe = static_cast<std::size_t> (i);
        const float cosine = cosOmega[probe];
        const float sine = sinOmega[probe];
        const float cosTwo = 2.0f * cosine * cosine - 1.0f;
        const float sinTwo = 2.0f * sine * cosine;

        double magnitude = 1.0;
        for (int k = 0; k < count; ++k)
        {
            const auto pole = static_cast<std::size_t> (k);
            double denominator = peakDenominator[pole];
            if (k != i)
            {
                const float real = 1.0f - a1[pole] * cosine - a2[pole] * cosTwo;
                const float imaginary = a1[pole] * sine + a2[pole] * sinTwo;
                denominator = std::sqrt (static_cast<double> (real) * real
                                         + static_cast<double> (imaginary) * imaginary);
            }
            magnitude *= denominator > 0.0
                ? static_cast<double> (numerator[pole]) / denominator : 0.0;
        }
        outGain[i] = static_cast<float> (magnitude);
        largest = std::max (largest, outGain[i]);
    }

    // Half of the tract's absolute gain is kept and half is compensated. Keeping
    // all of it makes the vowel pad a 16 dB fader; keeping none of it throws away
    // the real reason an open vowel carries further than a closed one. The
    // square root is the midpoint, and it is what a singer's own effort
    // adjustment does to the difference.
    const float scale = largest > 0.0f ? 1.0f / std::sqrt (largest) : 1.0f;
    const float floor = std::clamp (floorGain, 0.0f, 1.0f) * largest * scale;
    for (int i = 0; i < count; ++i)
        outGain[i] = std::max (floor, outGain[i] * scale);
}

float formantResponseDb (float frequencyHz, const float* formantHz,
                         const float* formantBandwidth, const float* formantGain,
                         int count, float sampleRate) noexcept
{
    if (formantHz == nullptr || formantBandwidth == nullptr || formantGain == nullptr
        || count <= 0 || ! (sampleRate > 0.0f))
        return -120.0f;

    const float nyquist = 0.5f * sampleRate;
    const float bounded = std::clamp (frequencyHz, 1.0f, 0.999f * nyquist);
    const float omega = twoPi * bounded / sampleRate;
    const float cosOmega = std::cos (omega);
    const float sinOmega = std::sin (omega);
    const float cosTwo = std::cos (2.0f * omega);
    const float sinTwo = std::sin (2.0f * omega);

    float sumReal = 0.0f;
    float sumImaginary = 0.0f;

    for (int i = 0; i < count; ++i)
    {
        const float centre = std::clamp (formantHz[i], 25.0f, 0.465f * sampleRate);
        const float bandwidth = std::clamp (formantBandwidth[i], 20.0f, 0.25f * sampleRate);
        const float radius = std::exp (-pi * bandwidth / sampleRate);
        const float poleAngle = twoPi * centre / sampleRate;
        const float a1 = 2.0f * radius * std::cos (poleAngle);
        const float a2 = -radius * radius;
        const float b0 = formantResonatorGain (radius, std::sin (poleAngle));

        const float real = 1.0f - a1 * cosOmega - a2 * cosTwo;
        const float imaginary = a1 * sinOmega + a2 * sinTwo;
        const float denominator = real * real + imaginary * imaginary;
        if (! (denominator > 0.0f))
            continue;

        const float scale = formantPolarity (i) * formantGain[i] * b0 / denominator;
        sumReal += scale * real;
        sumImaginary -= scale * imaginary;
    }

    const float magnitude = std::sqrt (sumReal * sumReal + sumImaginary * sumImaginary);
    return 20.0f * std::log10 (std::max (magnitude, 1.0e-6f));
}

float normalisedLogFrequency (float hz, float minHz, float maxHz) noexcept
{
    if (! (minHz > 0.0f) || ! (maxHz > minHz))
        return 0.0f;
    const float bounded = std::clamp (hz, minHz, maxHz);
    return std::log (bounded / minHz) / std::log (maxHz / minHz);
}

float logFrequencyForNormalised (float position, float minHz, float maxHz) noexcept
{
    if (! (minHz > 0.0f) || ! (maxHz > minHz))
        return minHz;
    return minHz * std::exp (clampUnit (position) * std::log (maxHz / minHz));
}

float linearToDecibels (float linear, float floorDb) noexcept
{
    if (! std::isfinite (linear))
        return floorDb;
    const float magnitude = std::abs (linear);
    if (magnitude <= 0.0f)
        return floorDb;
    return std::max (floorDb, 20.0f * std::log10 (magnitude));
}

float decibelsToMeterPosition (float decibels, float floorDb, float ceilingDb) noexcept
{
    if (! (ceilingDb > floorDb) || ! std::isfinite (decibels))
        return 0.0f;
    return std::clamp ((decibels - floorDb) / (ceilingDb - floorDb), 0.0f, 1.0f);
}

float smoothingCoefficient (float timeConstantSeconds, float updateIntervalSeconds) noexcept
{
    if (! (timeConstantSeconds > 0.0f) || ! (updateIntervalSeconds > 0.0f))
        return 1.0f;
    return 1.0f - std::exp (-updateIntervalSeconds / timeConstantSeconds);
}

float meterFollow (float current, float target, float attack, float release) noexcept
{
    if (! std::isfinite (current))
        current = 0.0f;
    if (! std::isfinite (target))
        target = 0.0f;
    const float coefficient = std::clamp (target > current ? attack : release, 0.0f, 1.0f);
    return current + coefficient * (target - current);
}

} // namespace vocalor
