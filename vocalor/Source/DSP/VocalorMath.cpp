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

float smoothStep (float value) noexcept
{
    value = clampUnit (value);
    return value * value * (3.0f - 2.0f * value);
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
    // under a controller. The span is what a singer covers between pianissimo
    // and fortissimo -- roughly 30-40 dB -- rather than the 18.1 dB a mixing
    // fader would give: an empty wheel is exactly 30.00 dB down on the voiced
    // source.
    response.voicedGain = std::exp2 (-4.9829f * below);
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

float vibratoExtentCents (float vibrato, float sectionLimitCents) noexcept
{
    const float knob = clampUnit (vibrato);
    if (! (knob > 0.0f))
        return 0.0f;

    // The knob's top is the definition: +/-1 semitone. What is not free is the
    // exponent, because existing sounds already use 18-46 % and the historical
    // engine compatibility anchor is 42 %. A linear scale would take every one
    // of those to 41-46 cents, which is a soloist's full vibrato on a patch
    // that asked for a third of the knob. 1.75 puts that historical 42 % anchor
    // at 21.9 cents -- comfortably above the roughly 10 cents below which a
    // vibrato is heard as unsteadiness rather than as vibrato, and well under
    // the maximum the top of the knob still reaches. The fresh 1.4 default is
    // zero; this anchor exists only for compatibility and explicit presets.
    float cents = kVibratoReachCents * std::pow (knob, 1.75f);
    // A section member does not sing a soloist's extent. What the section
    // imposes is a limit rather than a scale: she sings the gesture she would
    // sing alone until it is wider than the section tolerates, which is why
    // the same knob position means the same thing in every mode until it is
    // wide enough to smear. The knee sits at half the limit, where a
    // hyperbolic limiter is exact and continuous in its first derivative, and
    // the limit is approached rather than reached so the top of the knob keeps
    // moving instead of going dead.
    if (sectionLimitCents > 0.0f)
    {
        const float knee = 0.5f * sectionLimitCents;
        if (cents > knee)
            cents = sectionLimitCents - knee * knee / cents;
    }
    return cents;
}

float tunedFirstFormant (float baseHz, float fundamentalHz, float ceilingHz) noexcept
{
    if (! (baseHz > 0.0f) || ! std::isfinite (fundamentalHz) || ! (fundamentalHz > 0.0f))
        return baseHz;

    // Never below the vowel's own F1: this is a strategy for a fundamental that
    // has climbed too high, not a general retuning.
    const float ceiling = std::max (baseHz, std::isfinite (ceilingHz) ? ceilingHz : baseHz);
    const float target = std::min (ceiling, std::max (baseHz, fundamentalHz));
    // Fully engaged by the time the fundamental is 15 % above F1, absent below
    // 20 % under it, and smooth between so the tract never steps into it.
    const float engagement = smoothStep ((fundamentalHz / baseHz - 0.80f) / 0.35f);
    return baseHz + engagement * (target - baseHz);
}

float justIntonationOffsetCents (int semitonesAboveRoot) noexcept
{
    // just - equal, in cents, for 16:15, 9:8, 6:5, 5:4, 4:3, 45:32, 3:2, 8:5,
    // 5:3, 16:9 and 15:8. The unison and the octave are the same either way.
    static constexpr float offsets[12] = {
          0.000f,  11.731f,   3.910f,  15.641f, -13.686f,  -1.955f,
         -9.776f,   1.955f,  13.686f, -15.641f,  -3.910f, -11.731f
    };
    const int pitchClass = ((semitonesAboveRoot % 12) + 12) % 12;
    return offsets[static_cast<std::size_t> (pitchClass)];
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

void parallelFormantCoefficients (const float* formantHz,
                                  const float* formantBandwidth,
                                  int count, float sampleRate, float floorGain,
                                  float* outGain, float* outPoleA1,
                                  float* outPoleA2,
                                  float* outPeakNormaliser) noexcept
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
        if (outPoleA1 != nullptr)
            outPoleA1[i] = a1[index];
        if (outPoleA2 != nullptr)
            outPoleA2[i] = a2[index];
        if (outPeakNormaliser != nullptr)
            outPeakNormaliser[i] = peakDenominator[index];
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

void parallelFormantAmplitudes (const float* formantHz, const float* formantBandwidth,
                                int count, float sampleRate, float floorGain,
                                float* outGain) noexcept
{
    parallelFormantCoefficients (formantHz, formantBandwidth, count, sampleRate,
                                 floorGain, outGain, nullptr, nullptr, nullptr);
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
    // The double-angle identity (already used for the same pair in
    // parallelFormantCoefficients() below) turns the second cos/sin call into
    // a multiply and a subtract; a probe-frequency response curve calls this
    // once per plotted point, so it is worth avoiding here too.
    const float cosTwo = 2.0f * cosOmega * cosOmega - 1.0f;
    const float sinTwo = 2.0f * sinOmega * cosOmega;

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

void formantResponseCoefficients (const float* formantHz, const float* formantBandwidth,
                                  const float* formantGain, int count, float sampleRate,
                                  float* outA1, float* outA2, float* outScale) noexcept
{
    if (formantHz == nullptr || formantBandwidth == nullptr || formantGain == nullptr
        || outA1 == nullptr || outA2 == nullptr || outScale == nullptr
        || count <= 0 || ! (sampleRate > 0.0f))
        return;

    // Exactly the per-formant terms formantResponseDb() resolves inside its
    // probe-frequency loop -- centre/bandwidth clamp, pole radius and angle,
    // the a1/a2 coefficients, and the polarity-signed peak gain -- none of
    // which depends on the frequency being probed. Same operations, same
    // order, just resolved once per formant instead of once per formant per
    // probe.
    for (int i = 0; i < count; ++i)
    {
        const float centre = std::clamp (formantHz[i], 25.0f, 0.465f * sampleRate);
        const float bandwidth = std::clamp (formantBandwidth[i], 20.0f, 0.25f * sampleRate);
        const float radius = std::exp (-pi * bandwidth / sampleRate);
        const float poleAngle = twoPi * centre / sampleRate;
        const float a1 = 2.0f * radius * std::cos (poleAngle);
        const float a2 = -radius * radius;
        const float b0 = formantResonatorGain (radius, std::sin (poleAngle));

        outA1[i] = a1;
        outA2[i] = a2;
        outScale[i] = formantPolarity (i) * formantGain[i] * b0;
    }
}

float formantResponseDbFromCoefficients (float frequencyHz, const float* a1,
                                         const float* a2, const float* scale,
                                         int count, float sampleRate) noexcept
{
    if (a1 == nullptr || a2 == nullptr || scale == nullptr || count <= 0
        || ! (sampleRate > 0.0f))
        return -120.0f;

    const float nyquist = 0.5f * sampleRate;
    const float bounded = std::clamp (frequencyHz, 1.0f, 0.999f * nyquist);
    const float omega = twoPi * bounded / sampleRate;
    const float cosOmega = std::cos (omega);
    const float sinOmega = std::sin (omega);
    // Same double-angle identity as formantResponseDb(), which this must stay
    // bit-identical to: derived from the already-computed cosOmega/sinOmega
    // instead of two more transcendental calls. This is the function the
    // editor's response curve actually calls once per plotted point (192
    // points per repaint at its 24 Hz timer), so the saving lands here.
    const float cosTwo = 2.0f * cosOmega * cosOmega - 1.0f;
    const float sinTwo = 2.0f * sinOmega * cosOmega;

    float sumReal = 0.0f;
    float sumImaginary = 0.0f;

    for (int i = 0; i < count; ++i)
    {
        const float real = 1.0f - a1[i] * cosOmega - a2[i] * cosTwo;
        const float imaginary = a1[i] * sinOmega + a2[i] * sinTwo;
        const float denominator = real * real + imaginary * imaginary;
        if (! (denominator > 0.0f))
            continue;

        const float s = scale[i] / denominator;
        sumReal += s * real;
        sumImaginary -= s * imaginary;
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
