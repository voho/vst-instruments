#include "DSP/AirFilterbank.h"
#include "DSP/ModelVisualisation.h"
#include "DSP/NeuramarEngine.h"
#include "DSP/NeuralModel.h"
#include "DSP/SampleLearner.h"
#include "DSP/SpectralEnvelope.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define NEURAMAR_SANITIZED_TEST_BUILD 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) \
    || __has_feature(undefined_behavior_sanitizer)
#define NEURAMAR_SANITIZED_TEST_BUILD 1
#else
#define NEURAMAR_SANITIZED_TEST_BUILD 0
#endif
#else
#define NEURAMAR_SANITIZED_TEST_BUILD 0
#endif

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr std::array<float, neuramar::NeuralModel::boneModeCount>
    persistentBoneRatios { 1.35f, 2.35f, 3.35f, 4.35f, 5.35f, 6.35f };
constexpr std::array<float, neuramar::NeuralModel::boneModeCount>
    transientBoneRatios { 1.68f, 2.68f, 3.68f, 4.68f, 5.68f, 6.68f };
int failures = 0;

constexpr std::size_t modelHeaderBytes = 16;
// Every payload before version 5 stored 8 Air bands, 6 Bone modes, and the
// 79-output decoder that follows from them.
constexpr std::size_t legacyPitchOutputIndex =
    neuramar::NeuralModel::legacyOutputSize - 1;
constexpr std::size_t modelScalarBytes = 8 + 12 * 4;
constexpr std::size_t version2PayloadBytes = modelScalarBytes
    + 4 * (neuramar::NeuralModel::previewSize
        + neuramar::NeuralModel::harmonicCount
        + 2 * neuramar::NeuralModel::legacyAirBandCount
        + 4 * neuramar::NeuralModel::legacyBoneModeCount
        + 2 * neuramar::NeuralModel::legacyOutputSize
        + neuramar::NeuralModel::hiddenSize * neuramar::NeuralModel::inputSize
        + neuramar::NeuralModel::hiddenSize
        + neuramar::NeuralModel::legacyOutputSize
            * neuramar::NeuralModel::hiddenSize
        + neuramar::NeuralModel::legacyOutputSize);
// Where the residual extension starts inside a payload written by the current
// format. It is no longer the same offset as the version-2 payload length.
constexpr std::size_t currentBasePayloadBytes = modelScalarBytes
    + 4 * (neuramar::NeuralModel::previewSize
        + neuramar::NeuralModel::harmonicCount
        + 2 * neuramar::NeuralModel::airBandCount
        + 4 * neuramar::NeuralModel::boneModeCount
        + 2 * neuramar::NeuralModel::outputSize
        + neuramar::NeuralModel::hiddenSize * neuramar::NeuralModel::inputSize
        + neuramar::NeuralModel::hiddenSize
        + neuramar::NeuralModel::outputSize * neuramar::NeuralModel::hiddenSize
        + neuramar::NeuralModel::outputSize);

[[nodiscard]] std::uint32_t readLittleU32(
    const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8)
        value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
    return value;
}

[[nodiscard]] float readLittleFloat(
    const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return std::bit_cast<float>(readLittleU32(bytes, offset));
}

void writeLittleU32(std::vector<std::uint8_t>& bytes,
                    std::size_t offset, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        bytes[offset++] = static_cast<std::uint8_t>((value >> shift) & 0xffu);
}

void writeLittleU16(std::vector<std::uint8_t>& bytes,
                    std::size_t offset, std::uint16_t value)
{
    for (int shift = 0; shift < 16; shift += 8)
        bytes[offset++] = static_cast<std::uint8_t>((value >> shift) & 0xffu);
}

void appendLittleU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    const auto offset = bytes.size();
    bytes.resize(offset + 4);
    writeLittleU32(bytes, offset, value);
}

void appendLittleFloat(std::vector<std::uint8_t>& bytes, float value)
{
    appendLittleU32(bytes, std::bit_cast<std::uint32_t>(value));
}

void appendLittleI16(std::vector<std::uint8_t>& bytes, std::int16_t value)
{
    const auto offset = bytes.size();
    bytes.resize(offset + 2);
    writeLittleU16(bytes, offset, std::bit_cast<std::uint16_t>(value));
}

[[nodiscard]] std::uint32_t modelChecksum(
    std::span<const std::uint8_t> bytes)
{
    std::uint32_t hash = 2166136261u;
    for (const auto byte : bytes)
    {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

void refreshModelHeader(std::vector<std::uint8_t>& bytes,
                        std::uint32_t version)
{
    writeLittleU32(bytes, 4, version);
    writeLittleU32(bytes, 8, static_cast<std::uint32_t>(
        bytes.size() - modelHeaderBytes));
    writeLittleU32(bytes, 12, modelChecksum(std::span<const std::uint8_t>(bytes)
        .subspan(modelHeaderBytes)));
}

// Rebuilds a genuine version-2 payload out of a current serialization. Before
// version 5 the body arrays and the decoder rows were narrower, so this cannot
// be a truncation: the Air and Bone arrays keep their first 8 and 6 entries,
// and every decoder array is gathered through the same index map the loader
// uses in reverse.
[[nodiscard]] std::vector<std::uint8_t> makeVersion2ModelBytes(
    const neuramar::NeuralModel& model)
{
    using Model = neuramar::NeuralModel;
    const auto current = model.serialize();
    const std::size_t prefix = modelHeaderBytes + modelScalarBytes;
    if (current.size() < prefix)
        return {};
    std::vector<std::uint8_t> bytes(current.begin(),
                                    current.begin() + static_cast<
                                        std::ptrdiff_t>(prefix));
    std::size_t cursor = prefix;
    const auto take = [&cursor](std::size_t count)
    {
        const std::size_t offset = cursor;
        cursor += 4 * count;
        return offset;
    };
    const auto copyRun = [&bytes, &current](std::size_t offset,
                                            std::size_t count)
    {
        for (std::size_t index = 0; index < count; ++index)
            appendLittleFloat(bytes,
                              readLittleFloat(current, offset + 4 * index));
    };
    const auto copyDecoderRun = [&bytes, &current](std::size_t offset,
                                                   std::size_t stride)
    {
        for (std::size_t output = 0; output < Model::legacyOutputSize; ++output)
        {
            const std::size_t source = Model::migrateOutputIndex(output)
                * stride;
            for (std::size_t element = 0; element < stride; ++element)
                appendLittleFloat(bytes, readLittleFloat(
                    current, offset + 4 * (source + element)));
        }
    };

    copyRun(take(Model::previewSize), Model::previewSize);
    copyRun(take(Model::harmonicCount), Model::harmonicCount);
    copyRun(take(Model::airBandCount), Model::legacyAirBandCount);
    copyRun(take(Model::airBandCount), Model::legacyAirBandCount);
    for (int array = 0; array < 4; ++array)
        copyRun(take(Model::boneModeCount), Model::legacyBoneModeCount);
    copyDecoderRun(take(Model::outputSize), 1);
    copyDecoderRun(take(Model::outputSize), 1);
    copyRun(take(Model::hiddenSize * Model::inputSize),
            Model::hiddenSize * Model::inputSize);
    copyRun(take(Model::hiddenSize), Model::hiddenSize);
    copyDecoderRun(take(Model::outputSize * Model::hiddenSize),
                   Model::hiddenSize);
    copyDecoderRun(take(Model::outputSize), 1);
    if (bytes.size() != modelHeaderBytes + version2PayloadBytes
        || cursor > current.size())
        return {};
    refreshModelHeader(bytes, 2);
    return bytes;
}

// Builds a canonical version-3 payload: the version-2 neural base plus a
// two-keyframe residual trajectory and no inharmonicity field at all.
[[nodiscard]] std::vector<std::uint8_t> makeLinearResidualModelBytes(
    const neuramar::NeuralModel& model, float pitchCorrectionSemitones,
    std::uint32_t version = 3, float inharmonicity = 0.0f)
{
    auto bytes = makeVersion2ModelBytes(model);
    if (bytes.empty())
        return {};
    appendLittleU32(bytes, 2);
    for (std::size_t output = 0;
         output < neuramar::NeuralModel::legacyOutputSize; ++output)
    {
        appendLittleFloat(bytes, output == legacyPitchOutputIndex
                ? pitchCorrectionSemitones : 0.0f);
    }
    appendLittleFloat(bytes, 0.0f);
    appendLittleFloat(bytes, 1.0f);
    for (std::size_t frame = 0; frame < 2; ++frame)
    {
        for (std::size_t output = 0;
             output < neuramar::NeuralModel::legacyOutputSize; ++output)
        {
            appendLittleI16(bytes,
                frame == 1 && output == legacyPitchOutputIndex
                    ? static_cast<std::int16_t>(32767) : 0);
        }
    }
    if (version >= 4)
        appendLittleFloat(bytes, inharmonicity);
    refreshModelHeader(bytes, version);
    return bytes;
}

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testShapePreservingSpectralInterpolation()
{
    using SmallEnvelope = neuramar::spectral::ShapePreservingEnvelope<8>;
    const std::array<float, 8> notched {
        0.82f, 0.006f, 0.47f, 0.0f, 0.19f, 0.031f, 0.11f, 0.002f
    };
    SmallEnvelope notchedEnvelope;
    notchedEnvelope.prepare(notched);

    expect(notchedEnvelope.sample(0.0f) == 0.0f
           && notchedEnvelope.sample(9.0f) == 0.0f
           && notchedEnvelope.sample(-1.0f) == 0.0f
           && notchedEnvelope.sample(
               std::numeric_limits<float>::quiet_NaN()) == 0.0f,
           "spectral interpolation did not preserve its silent evidence boundary");
    for (std::size_t harmonic = 0; harmonic < notched.size(); ++harmonic)
    {
        expect(notchedEnvelope.sample(static_cast<float>(harmonic + 1))
                   == notched[harmonic],
               "spectral interpolation changed an observed harmonic knot");
    }

    std::array<float, 10> knots {};
    std::copy(notched.begin(), notched.end(), knots.begin() + 1);
    for (std::size_t interval = 0; interval + 1 < knots.size(); ++interval)
    {
        const float minimum = std::min(knots[interval], knots[interval + 1]);
        const float maximum = std::max(knots[interval], knots[interval + 1]);
        for (int step = 1; step < 32; ++step)
        {
            const float coordinate = static_cast<float>(interval)
                + static_cast<float>(step) / 32.0f;
            const float value = notchedEnvelope.sample(coordinate);
            expect(std::isfinite(value) && value >= minimum - 1.0e-8f
                       && value <= maximum + 1.0e-8f,
                   "spectral interpolation overshot adjacent learned magnitudes");
        }
    }

    // A smooth exponential roll-off is linear in log magnitude. The companded
    // cubic should reproduce its fractional coordinates much more accurately
    // than the former arithmetic interpolation without changing its knots.
    using SmoothEnvelope = neuramar::spectral::ShapePreservingEnvelope<32>;
    std::array<float, 32> smooth {};
    for (std::size_t harmonic = 0; harmonic < smooth.size(); ++harmonic)
        smooth[harmonic] = 0.8f * std::exp(
            -0.09f * static_cast<float>(harmonic + 1));
    SmoothEnvelope smoothEnvelope;
    smoothEnvelope.prepare(smooth);
    double smartSquaredError = 0.0;
    double linearSquaredError = 0.0;
    for (int interval = 2; interval < 30; ++interval)
    {
        for (const float fraction : { 0.125f, 0.375f, 0.625f, 0.875f })
        {
            const float coordinate = static_cast<float>(interval) + fraction;
            const float expected = 0.8f * std::exp(-0.09f * coordinate);
            const float smart = smoothEnvelope.sample(coordinate);
            const float linear = smooth[static_cast<std::size_t>(interval - 1)]
                + fraction * (smooth[static_cast<std::size_t>(interval)]
                    - smooth[static_cast<std::size_t>(interval - 1)]);
            smartSquaredError += static_cast<double>(smart - expected)
                * (smart - expected);
            linearSquaredError += static_cast<double>(linear - expected)
                * (linear - expected);
        }
    }
    expect(smartSquaredError < 0.10 * linearSquaredError,
           "shape-preserving interpolation did not improve a smooth log-spectral "
           "trajectory over arithmetic interpolation");

    using FormantEnvelope = neuramar::spectral::ShapePreservingEnvelope<64>;
    const auto formantMagnitude = [](float coordinate)
    {
        const float firstDistance = (coordinate - 14.0f) / 3.2f;
        const float secondDistance = (coordinate - 37.0f) / 6.0f;
        return std::exp(-1.0f - 0.035f * coordinate
            + 1.8f * std::exp(-0.5f * firstDistance * firstDistance)
            + 0.9f * std::exp(-0.5f * secondDistance * secondDistance));
    };
    std::array<float, 64> formants {};
    for (std::size_t harmonic = 0; harmonic < formants.size(); ++harmonic)
        formants[harmonic] = formantMagnitude(
            static_cast<float>(harmonic + 1));
    FormantEnvelope formantEnvelope;
    formantEnvelope.prepare(formants);
    double smartDbSquaredError = 0.0;
    double linearDbSquaredError = 0.0;
    for (int interval = 2; interval < 62; ++interval)
    {
        for (const float fraction : { 0.125f, 0.375f, 0.625f, 0.875f })
        {
            const float coordinate = static_cast<float>(interval) + fraction;
            const float expected = formantMagnitude(coordinate);
            const float smart = formantEnvelope.sample(coordinate);
            const float linear = formants[static_cast<std::size_t>(interval - 1)]
                + fraction * (formants[static_cast<std::size_t>(interval)]
                    - formants[static_cast<std::size_t>(interval - 1)]);
            const double smartDbError = 20.0 * std::log10(
                std::max(smart, 1.0e-12f) / expected);
            const double linearDbError = 20.0 * std::log10(
                std::max(linear, 1.0e-12f) / expected);
            smartDbSquaredError += smartDbError * smartDbError;
            linearDbSquaredError += linearDbError * linearDbError;
        }
    }
    const double exponentialErrorRatio = smartSquaredError
        / std::max(linearSquaredError, 1.0e-30);
    const double formantErrorRatio = smartDbSquaredError
        / std::max(linearDbSquaredError, 1.0e-30);
    std::cout << "Interpolation diagnostics: exponential error ratio "
              << exponentialErrorRatio << ", curved-formant dB error ratio "
              << formantErrorRatio << '\n';
    expect(formantErrorRatio < 0.35,
           "shape-preserving interpolation did not improve a curved smooth "
           "log-spectral envelope over arithmetic interpolation");
}

[[nodiscard]] std::vector<float> makeLearningSample(double sampleRate,
                                                     double frequencyHz,
                                                     double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    std::uint32_t noiseState = 0x73594a1du;
    float previousNoise = 0.0f;
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double attack = 1.0 - std::exp(-time / 0.008);
        const double tail = 0.82 + 0.18 * std::exp(-time / 0.48);
        const double evolution = 0.5 + 0.5 * std::sin(2.0 * pi * 0.73 * time);
        double value = 0.58 * std::sin(2.0 * pi * frequencyHz * time + 0.21);
        value += (0.20 + 0.08 * evolution)
            * std::sin(2.0 * pi * 2.0 * frequencyHz * time + 1.07);
        value += (0.105 - 0.035 * evolution)
            * std::sin(2.0 * pi * 3.0 * frequencyHz * time + 2.11);
        value += 0.055 * std::sin(2.0 * pi * 5.0 * frequencyHz * time + 0.73);
        value += 0.055 * std::exp(-time / 0.34)
            * std::sin(2.0 * pi * 2.71 * frequencyHz * time + 1.61);

        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        const float noise = static_cast<float>(noiseState)
            * (2.0f / 4294967295.0f) - 1.0f;
        const float highNoise = noise - 0.92f * previousNoise;
        previousNoise = noise;
        value += 0.012 * (0.35 + 0.65 * evolution) * highNoise;
        sample[index] = static_cast<float>(0.78 * attack * tail * value);
    }
    return sample;
}

[[nodiscard]] std::vector<float> makeMissingFundamentalSample(
    double sampleRate, double fundamentalHz, double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    constexpr double levels[] { 0.0, 0.0, 0.52, 0.34, 0.23, 0.16 };
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double envelope = (1.0 - std::exp(-time / 0.006))
            * (0.88 + 0.12 * std::exp(-time / 0.55));
        double value = 0.0;
        for (int harmonic = 2; harmonic <= 5; ++harmonic)
            value += levels[harmonic] * std::sin(
                2.0 * pi * fundamentalHz * static_cast<double>(harmonic) * time
                + 0.37 * static_cast<double>(harmonic));
        sample[index] = static_cast<float>(envelope * value);
    }
    return sample;
}

[[nodiscard]] double gaussianLogFrequency(double frequencyHz,
                                          double centreHz,
                                          double widthOctaves) noexcept
{
    const double distance = std::log2(
        std::max(frequencyHz, 1.0) / centreHz) / widthOctaves;
    return std::exp(-0.5 * distance * distance);
}

[[nodiscard]] std::vector<float> makeFormantDominantRootSample(
    double sampleRate, double fundamentalHz, double durationSeconds,
    bool includeFundamental)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    const int harmonicCount = std::min(96, static_cast<int>(std::floor(
        0.44 * sampleRate / fundamentalHz)));
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double envelope = (1.0 - std::exp(-time / 0.006))
            * (0.88 + 0.12 * std::exp(-time / 0.40));
        double value = 0.0;
        for (int harmonic = 1; harmonic <= harmonicCount; ++harmonic)
        {
            if (harmonic == 1 && !includeFundamental)
                continue;
            const double frequency = fundamentalHz * harmonic;
            const double formants = 0.13
                + 1.55 * gaussianLogFrequency(frequency, 720.0, 0.23)
                + 0.92 * gaussianLogFrequency(frequency, 1810.0, 0.19)
                + 0.58 * gaussianLogFrequency(frequency, 3470.0, 0.22);
            const double excitation = (harmonic & 1) != 0 ? 1.0 : 0.055;
            const double amplitude = excitation * formants
                / std::pow(static_cast<double>(harmonic), 0.72);
            value += amplitude * std::sin(
                2.0 * pi * frequency * time
                + 0.19 * static_cast<double>(harmonic * harmonic));
        }
        sample[index] = static_cast<float>(0.16 * envelope * value);
    }
    return sample;
}

[[nodiscard]] std::vector<float> makeOctaveDominantRootSample(
    double sampleRate, double fundamentalHz, double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    constexpr std::array<double, 5> levels {
        0.06, 0.80, 0.17, 0.12, 0.09
    };
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double envelope = (1.0 - std::exp(-time / 0.005))
            * (0.86 + 0.14 * std::exp(-time / 0.45));
        double value = 0.0;
        for (std::size_t harmonic = 0; harmonic < levels.size(); ++harmonic)
        {
            value += levels[harmonic] * std::sin(
                2.0 * pi * fundamentalHz
                    * static_cast<double>(harmonic + 1) * time
                + 0.31 * static_cast<double>(harmonic + 1));
        }
        sample[index] = static_cast<float>(0.72 * envelope * value);
    }
    return sample;
}

[[nodiscard]] std::vector<float> makeBrightHighPitchSample(
    double sampleRate, double fundamentalHz, double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    const int harmonicCount = std::max(1, std::min(18,
        static_cast<int>(std::floor(0.44 * sampleRate / fundamentalHz))));
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double envelope = (1.0 - std::exp(-time / 0.004))
            * (0.80 + 0.20 * std::exp(-time / 0.42));
        double value = 0.0;
        for (int harmonic = 1; harmonic <= harmonicCount; ++harmonic)
        {
            const double level = 0.31 / std::pow(static_cast<double>(harmonic), 0.62);
            value += level * std::sin(
                2.0 * pi * fundamentalHz * static_cast<double>(harmonic) * time
                + 0.19 * static_cast<double>(harmonic * harmonic));
        }
        sample[index] = static_cast<float>(envelope * value);
    }
    return sample;
}

[[nodiscard]] std::vector<float> makeSlowPitchBendSample(
    double sampleRate, double centreFrequencyHz, double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    double phase = 0.0;
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double position = static_cast<double>(index)
            / static_cast<double>(std::max<std::size_t>(1, sample.size() - 1));
        const double semitones = -1.7 + 3.4 * position;
        const double frequency = centreFrequencyHz * std::exp2(semitones / 12.0);
        phase += frequency / sampleRate;
        phase -= std::floor(phase);
        const double angle = 2.0 * pi * phase;
        const double envelope = (1.0 - std::exp(-time / 0.006))
            * (0.94 + 0.06 * std::exp(-time / 0.5));
        sample[index] = static_cast<float>(envelope
            * (0.68 * std::sin(angle + 0.13)
               + 0.21 * std::sin(2.0 * angle + 0.71)
               + 0.08 * std::sin(3.0 * angle - 0.39)));
    }
    return sample;
}

[[nodiscard]] std::vector<float> makePureSineSample(
    double sampleRate, double frequencyHz, double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double envelope = 1.0 - std::exp(-time / 0.004);
        sample[index] = static_cast<float>(
            0.72 * envelope * std::sin(2.0 * pi * frequencyHz * time + 0.31));
    }
    return sample;
}

[[nodiscard]] std::vector<float> makeAirSeparationSample(
    double sampleRate, bool includeNoise)
{
    constexpr double fundamentalHz = 187.5;
    constexpr double durationSeconds = 0.96;
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    const int harmonicCount = std::max(1, std::min(64,
        static_cast<int>(std::floor(0.42 * sampleRate / fundamentalHz))));
    std::uint32_t noiseState = 0x6a09e667u;
    float previousNoise = 0.0f;
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double envelope = 1.0 - std::exp(-time / 0.006);
        double value = 0.0;
        for (int harmonic = 1; harmonic <= harmonicCount; ++harmonic)
        {
            const double level = 0.42
                / std::pow(static_cast<double>(harmonic), 0.82);
            value += level * std::sin(
                2.0 * pi * fundamentalHz * static_cast<double>(harmonic) * time
                + 0.137 * static_cast<double>(harmonic * harmonic));
        }

        if (includeNoise)
        {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            const float noise = static_cast<float>(noiseState)
                * (2.0f / 4294967295.0f) - 1.0f;
            const float highNoise = noise - 0.94f * previousNoise;
            previousNoise = noise;
            value += 0.085 * static_cast<double>(highNoise);
        }
        sample[index] = static_cast<float>(envelope * value);
    }
    return sample;
}

[[nodiscard]] std::vector<float> makePersistentBoneSample(double sampleRate)
{
    constexpr double fundamentalHz = 187.5;
    constexpr double durationSeconds = 1.20;
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double tonalAttack = 1.0 - std::exp(-time / 0.004);
        double value = tonalAttack
            * (0.62 * std::sin(2.0 * pi * fundamentalHz * time + 0.17)
               + 0.18 * std::sin(2.0 * pi * 2.0 * fundamentalHz * time + 0.83)
               + 0.08 * std::sin(2.0 * pi * 3.0 * fundamentalHz * time - 0.41));

        for (std::size_t mode = 0; mode < persistentBoneRatios.size(); ++mode)
        {
            value += tonalAttack * 0.050 * std::sin(
                2.0 * pi * fundamentalHz * persistentBoneRatios[mode] * time
                + 0.43 * static_cast<double>(mode + 1));
            value += 0.16 * std::exp(-time / 0.012) * std::sin(
                2.0 * pi * fundamentalHz * transientBoneRatios[mode] * time
                + 0.61 * static_cast<double>(mode + 1));
        }
        sample[index] = static_cast<float>(value);
    }
    return sample;
}

[[nodiscard]] std::vector<float> makeEvolvingAirSample(double sampleRate)
{
    constexpr double fundamentalHz = 187.5;
    constexpr double durationSeconds = 1.10;
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    std::uint32_t noiseState = 0xbb67ae85u;
    float lowNoise = 0.0f;
    float previousNoise = 0.0f;
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double position = static_cast<double>(index)
            / static_cast<double>(std::max<std::size_t>(1, sample.size() - 1));
        const double envelope = 1.0 - std::exp(-time / 0.006);
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        const float noise = static_cast<float>(noiseState)
            * (2.0f / 4294967295.0f) - 1.0f;
        lowNoise += 0.08f * (noise - lowNoise);
        const float highNoise = noise - previousNoise;
        previousNoise = noise;
        const double stochastic = (1.0 - position) * 0.50 * lowNoise
            + position * 0.075 * highNoise;
        const double tonal = 0.54 * std::sin(
            2.0 * pi * fundamentalHz * time + 0.17)
            + 0.17 * std::sin(
                2.0 * pi * 2.0 * fundamentalHz * time + 0.71);
        sample[index] = static_cast<float>(envelope * (tonal + stochastic));
    }
    return sample;
}

[[nodiscard]] std::vector<float> makeTransientAirSample(double sampleRate)
{
    constexpr double fundamentalHz = 196.0;
    constexpr double durationSeconds = 0.82;
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    std::uint32_t noiseState = 0x3c6ef372u;
    float previousNoise = 0.0f;
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double tonalAttack = 1.0 - std::exp(-time / 0.004);
        const double tonal = tonalAttack
            * (0.58 * std::sin(2.0 * pi * fundamentalHz * time + 0.19)
               + 0.18 * std::sin(
                   2.0 * pi * 2.0 * fundamentalHz * time + 0.73));
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        const float noise = static_cast<float>(noiseState)
            * (2.0f / 4294967295.0f) - 1.0f;
        const float highNoise = noise - 0.96f * previousNoise;
        previousNoise = noise;
        const double burst = 0.24 * std::exp(-time / 0.008)
            * static_cast<double>(highNoise);
        sample[index] = static_cast<float>(tonal + burst);
    }
    return sample;
}

[[nodiscard]] float rms(const std::vector<float>& values,
                        std::size_t first = 0)
{
    if (first >= values.size())
        return 0.0f;
    double sum = 0.0;
    for (std::size_t index = first; index < values.size(); ++index)
        sum += static_cast<double>(values[index]) * values[index];
    return static_cast<float>(std::sqrt(sum
        / static_cast<double>(values.size() - first)));
}

[[nodiscard]] std::vector<float> renderNote(
    neuramar::NeuramarEngine& engine, int midiNote, int sampleCount);

[[nodiscard]] double modelFrameDistance(
    const neuramar::NeuralModel& reference,
    const neuramar::NeuralModel& candidate)
{
    constexpr std::array<float, 9> times {
        0.0f, 0.03f, 0.08f, 0.16f, 0.28f, 0.43f, 0.61f, 0.79f, 1.0f
    };
    double squared = 0.0;
    std::size_t count = 0;
    for (const auto time : times)
    {
        neuramar::SynthesisFrame first;
        neuramar::SynthesisFrame second;
        reference.evaluate(time, first);
        candidate.evaluate(time, second);
        const auto accumulateAmplitude = [&squared, &count] (
            auto firstValues, auto secondValues)
        {
            for (std::size_t index = 0; index < firstValues.size(); ++index)
            {
                const double difference = std::log(
                    std::max(secondValues[index], 1.0e-8f))
                    - std::log(std::max(firstValues[index], 1.0e-8f));
                squared += difference * difference;
                ++count;
            }
        };
        accumulateAmplitude(first.harmonicAmplitudes,
                            second.harmonicAmplitudes);
        accumulateAmplitude(first.airAmplitudes, second.airAmplitudes);
        accumulateAmplitude(first.boneAmplitudes, second.boneAmplitudes);
        const double pitchSemitones = 12.0 * std::log2(
            second.pitchRatio / first.pitchRatio);
        squared += pitchSemitones * pitchSemitones;
        ++count;
    }
    return std::sqrt(squared / static_cast<double>(std::max<std::size_t>(1, count)));
}

[[nodiscard]] bool framesExactlyEqual(
    const neuramar::SynthesisFrame& first,
    const neuramar::SynthesisFrame& second) noexcept
{
    return first.harmonicAmplitudes == second.harmonicAmplitudes
        && first.airAmplitudes == second.airAmplitudes
        && first.boneAmplitudes == second.boneAmplitudes
        && first.pitchRatio == second.pitchRatio;
}

[[nodiscard]] double synthesisFrameDistance(
    const neuramar::SynthesisFrame& first,
    const neuramar::SynthesisFrame& second)
{
    double squared = 0.0;
    std::size_t count = 0;
    const auto accumulate = [&squared, &count] (
        const auto& firstValues, const auto& secondValues)
    {
        for (std::size_t index = 0; index < firstValues.size(); ++index)
        {
            const double difference = static_cast<double>(
                secondValues[index] - firstValues[index]);
            squared += difference * difference;
            ++count;
        }
    };
    accumulate(first.harmonicAmplitudes, second.harmonicAmplitudes);
    accumulate(first.airAmplitudes, second.airAmplitudes);
    accumulate(first.boneAmplitudes, second.boneAmplitudes);
    const double pitchDifference = static_cast<double>(
        second.pitchRatio - first.pitchRatio);
    squared += pitchDifference * pitchDifference;
    ++count;
    return std::sqrt(squared
        / static_cast<double>(std::max<std::size_t>(1, count)));
}

struct ModelFieldMetrics
{
    double energy { 0.0 };
    float minimumPitchRatio { std::numeric_limits<float>::max() };
    float maximumPitchRatio { 0.0f };
    std::size_t upperAmplitudeClamps { 0 };
    bool finite { true };
};

[[nodiscard]] ModelFieldMetrics measureModelField(
    const neuramar::NeuralModel& model)
{
    ModelFieldMetrics metrics;
    constexpr float maximumDecodedAmplitude = 4.4816890703380645f;
    for (int sample = 0; sample <= 64; ++sample)
    {
        neuramar::SynthesisFrame frame;
        model.evaluate(static_cast<float>(sample) / 64.0f, frame);
        const auto accumulate = [&metrics, maximumDecodedAmplitude] (
            const auto& amplitudes)
        {
            for (const auto amplitude : amplitudes)
            {
                metrics.finite = metrics.finite
                    && std::isfinite(amplitude) && amplitude >= 0.0f;
                if (std::isfinite(amplitude))
                    metrics.energy += static_cast<double>(amplitude) * amplitude;
                if (amplitude >= maximumDecodedAmplitude * (1.0f - 1.0e-6f))
                    ++metrics.upperAmplitudeClamps;
            }
        };
        accumulate(frame.harmonicAmplitudes);
        accumulate(frame.airAmplitudes);
        accumulate(frame.boneAmplitudes);
        metrics.finite = metrics.finite && std::isfinite(frame.pitchRatio);
        metrics.minimumPitchRatio = std::min(
            metrics.minimumPitchRatio, frame.pitchRatio);
        metrics.maximumPitchRatio = std::max(
            metrics.maximumPitchRatio, frame.pitchRatio);
    }
    return metrics;
}

void testRandomNeuralSeed()
{
    constexpr std::uint64_t seed = 0x6e657572616d6172ull;
    auto subtle = neuramar::NeuralModel::createRandom(seed, 0.01f);
    auto subtleAgain = neuramar::NeuralModel::createRandom(seed, 0.01f);
    auto evolve = neuramar::NeuralModel::createRandom(seed, 0.10f);
    auto wild = neuramar::NeuralModel::createRandom(seed, 1.0f);
    auto anotherSeed = neuramar::NeuralModel::createRandom(seed + 1u, 0.10f);
    expect(subtle && subtleAgain && evolve && wild && anotherSeed,
           "sample-free neural randomization did not create every model");
    if (!subtle || !subtleAgain || !evolve || !wild || !anotherSeed)
        return;

    expect(subtle->serialize() == subtleAgain->serialize(),
           "identical neural seeds were not deterministic");
    expect(subtle->serialize() != evolve->serialize()
               && evolve->serialize() != wild->serialize(),
           "1%, 10%, and 100% random ranges produced the same neural state");
    expect(evolve->serialize() != anotherSeed->serialize(),
           "different neural seeds produced the same model");

    const auto& metadata = evolve->metadata();
    expect(metadata.trainingEpochs == 0
               && metadata.rootMidiNote == 60
               && std::abs(metadata.rootFrequencyHz - 261.625565f) < 0.01f
               && metadata.durationSeconds > 0.1f
               && metadata.loopEndSeconds > metadata.loopStartSeconds,
           "generated neural model metadata is not a valid playable C4 anchor");
    const auto previewPeak = *std::max_element(
        metadata.waveformPreview.begin(), metadata.waveformPreview.end(),
        [] (float first, float second)
        {
            return std::abs(first) < std::abs(second);
        });
    expect(std::abs(previewPeak) > 0.05f,
           "generated neural model did not provide a waveform preview");

    for (const auto* model : { subtle.get(), evolve.get(), wild.get() })
    {
        double frameEnergy = 0.0;
        for (const float time : { 0.0f, 0.1f, 0.35f, 0.7f, 1.0f })
        {
            neuramar::SynthesisFrame frame;
            model->evaluate(time, frame);
            for (const auto amplitude : frame.harmonicAmplitudes)
            {
                expect(std::isfinite(amplitude) && amplitude >= 0.0f,
                       "generated Core frame contained invalid amplitude");
                frameEnergy += static_cast<double>(amplitude) * amplitude;
            }
            for (const auto amplitude : frame.airAmplitudes)
            {
                expect(std::isfinite(amplitude) && amplitude >= 0.0f,
                       "generated Air frame contained invalid amplitude");
                frameEnergy += static_cast<double>(amplitude) * amplitude;
            }
            for (const auto amplitude : frame.boneAmplitudes)
            {
                expect(std::isfinite(amplitude) && amplitude >= 0.0f,
                       "generated Bone frame contained invalid amplitude");
                frameEnergy += static_cast<double>(amplitude) * amplitude;
            }
            expect(std::isfinite(frame.pitchRatio)
                       && frame.pitchRatio > 0.7f && frame.pitchRatio < 1.4f,
                   "generated neural pitch trajectory exceeded its musical bound");
        }
        expect(frameEnergy > 1.0e-5,
               "generated neural model evaluated to silence");
    }

    std::string error;
    auto restored = neuramar::NeuralModel::deserialize(
        wild->serialize(), &error);
    expect(restored != nullptr && restored->serialize() == wild->serialize(),
           "generated neural model did not survive serialization: " + error);

    auto evolutionaryWalk = neuramar::NeuralModel::createRandom(
        seed ^ 0x57414c4bull, 0.10f);
    const auto walkBaseline = evolutionaryWalk != nullptr
        ? measureModelField(*evolutionaryWalk) : ModelFieldMetrics {};
    constexpr float oneSemitone = 1.0594630943592953f;
    for (std::uint64_t step = 0; evolutionaryWalk && step < 32; ++step)
    {
        evolutionaryWalk = evolutionaryWalk->createRandomizedVariation(
            seed + 0x9e3779b97f4a7c15ull * (step + 1u), 1.0f);
        expect(evolutionaryWalk != nullptr,
               "repeated full-range variation lost the neural model");
        if (! evolutionaryWalk)
            break;

        const auto metrics = measureModelField(*evolutionaryWalk);
        const auto energyRatio = metrics.energy
            / std::max(walkBaseline.energy, 1.0e-12);
        expect(metrics.finite && metrics.upperAmplitudeClamps == 0,
               "repeated full-range variation saturated model amplitudes");
        expect(energyRatio >= 0.25 && energyRatio <= 4.0,
               "repeated full-range variation escaped its loudness range");
        expect(metrics.minimumPitchRatio >= 1.0f / oneSemitone
                   && metrics.maximumPitchRatio <= oneSemitone,
               "repeated full-range variation drifted beyond one semitone");

        error.clear();
        auto walkedRoundTrip = neuramar::NeuralModel::deserialize(
            evolutionaryWalk->serialize(), &error);
        expect(walkedRoundTrip != nullptr,
               "cumulative neural variation failed state validation: " + error);

        if (step == 15 || step == 31)
        {
            for (const int midiNote : { 0, 24, 60, 127 })
            {
                neuramar::NeuramarEngine engine;
                engine.prepare(48000.0, 128);
                engine.setModel(evolutionaryWalk.get());
                neuramar::EngineParameters parameters;
                parameters.imprint = 0.90f;
                parameters.bodyLock = 0.52f;
                parameters.air = 0.82f;
                parameters.bone = 0.78f;
                parameters.brightness = 0.50f;
                parameters.evolutionRate = 1.0f;
                parameters.orbit = 0.0f;
                parameters.mutation = 0.12f;
                parameters.attackSeconds = 0.0f;
                parameters.releaseSeconds = 0.65f;
                parameters.spread = 0.58f;
                parameters.outputGain = 0.5011872f;
                engine.setParameters(parameters);
                const auto audio = renderNote(engine, midiNote, 12000);
                expect(rms(audio, 1024) > 1.0e-6f,
                       "repeated full-range variation became silent");
                for (const auto value : audio)
                    expect(std::isfinite(value) && std::abs(value) < 7.94f,
                           "repeated full-range variation hit the output guard");
            }
        }
    }

    auto evolveWalk = neuramar::NeuralModel::createRandom(
        seed ^ 0x45564f4c5645ull, 0.10f);
    const auto evolveBaseline = evolveWalk != nullptr
        ? measureModelField(*evolveWalk) : ModelFieldMetrics {};
    for (std::uint64_t step = 0; evolveWalk && step < 256; ++step)
    {
        evolveWalk = evolveWalk->createRandomizedVariation(
            seed ^ (0xd1b54a32d192ed03ull * (step + 1u)), 0.10f);
        expect(evolveWalk != nullptr,
               "repeated ten-percent variation lost the neural model");
        if (! evolveWalk)
            break;

        if (step == 63 || step == 127 || step == 191 || step == 255)
        {
            const auto metrics = measureModelField(*evolveWalk);
            const auto energyRatio = metrics.energy
                / std::max(evolveBaseline.energy, 1.0e-12);
            expect(metrics.finite && metrics.upperAmplitudeClamps == 0,
                   "repeated ten-percent variation saturated model amplitudes");
            expect(energyRatio >= 0.85 && energyRatio <= 1.15,
                   "repeated ten-percent variation accumulated a loudness fade "
                   "or runaway gain");
            expect(metrics.minimumPitchRatio >= 1.0f / oneSemitone
                       && metrics.maximumPitchRatio <= oneSemitone,
                   "repeated ten-percent variation drifted beyond one semitone");
        }
    }
    if (evolveWalk)
    {
        error.clear();
        auto evolveRoundTrip = neuramar::NeuralModel::deserialize(
            evolveWalk->serialize(), &error);
        expect(evolveRoundTrip != nullptr,
               "256-step ten-percent variation failed state validation: "
                   + error);
    }

    for (const int midiNote : {
             wild->rootMidiNote() - 36,
             wild->rootMidiNote(),
             wild->rootMidiNote() + 36 })
    {
        neuramar::NeuramarEngine engine;
        engine.prepare(48000.0, 128);
        engine.setModel(wild.get());
        neuramar::EngineParameters parameters;
        parameters.imprint = 1.0f;
        parameters.air = 0.45f;
        parameters.bone = 0.45f;
        parameters.mutation = 0.0f;
        parameters.attackSeconds = 0.0f;
        parameters.outputGain = 0.05f;
        engine.setParameters(parameters);
        const auto audio = renderNote(engine, midiNote, 12000);
        expect(rms(audio, 1024) > 1.0e-4f,
               "generated neural seed was silent across the wide MIDI range");
        for (const auto sample : audio)
            expect(std::isfinite(sample) && std::abs(sample) <= 7.95001f,
                   "generated neural seed rendered invalid or unbounded audio");
    }
}

void testRandomizedVariations(const neuramar::NeuralModel& learned)
{
    constexpr std::uint64_t seed = 0x766172696174696full;
    const auto originalBytes = learned.serialize();
    auto subtle = learned.createRandomizedVariation(seed, 0.01f);
    auto subtleAgain = learned.createRandomizedVariation(seed, 0.01f);
    auto evolve = learned.createRandomizedVariation(seed, 0.10f);
    auto wild = learned.createRandomizedVariation(seed, 1.0f);
    expect(subtle && subtleAgain && evolve && wild,
           "learned neural memory could not create random variations");
    if (!subtle || !subtleAgain || !evolve || !wild)
        return;

    expect(subtle->serialize() == subtleAgain->serialize(),
           "learned neural variation was not deterministic for a fixed seed");
    expect(learned.serialize() == originalBytes,
           "immutable learned model changed while deriving a variation");
    for (const auto* variation : { subtle.get(), evolve.get(), wild.get() })
    {
        const auto& originalMetadata = learned.metadata();
        const auto& variedMetadata = variation->metadata();
        expect(variedMetadata.rootFrequencyHz == originalMetadata.rootFrequencyHz
                   && variedMetadata.rootMidiNote == originalMetadata.rootMidiNote
                   && variedMetadata.rootCents == originalMetadata.rootCents
                   && variedMetadata.durationSeconds
                       == originalMetadata.durationSeconds
                   && variedMetadata.loopStartSeconds
                       == originalMetadata.loopStartSeconds
                   && variedMetadata.loopEndSeconds
                       == originalMetadata.loopEndSeconds,
               "neural variation changed source pitch or time semantics");
    }

    const auto subtleDistance = modelFrameDistance(learned, *subtle);
    const auto evolveDistance = modelFrameDistance(learned, *evolve);
    const auto wildDistance = modelFrameDistance(learned, *wild);
    expect(subtleDistance > 1.0e-6,
           "1% neural variation did not alter the learned memory");
    expect(evolveDistance > subtleDistance * 1.5,
           "10% neural variation was not materially broader than 1%");
    expect(wildDistance > evolveDistance * 1.5,
           "100% neural variation was not materially broader than 10%");
}

void testModelSpaceNoise(const neuramar::NeuralModel& model)
{
    for (const float time : { 0.0f, 0.07f, 0.31f, 0.63f, 1.0f })
    {
        neuramar::SynthesisFrame established;
        neuramar::SynthesisFrame zeroLatent;
        neuramar::SynthesisFrame invalidLatent;
        model.evaluate(time, established);
        model.evaluate(time, 0.0f, zeroLatent);
        model.evaluate(
            time, std::numeric_limits<float>::quiet_NaN(), invalidLatent);
        expect(framesExactlyEqual(established, zeroLatent)
                   && framesExactlyEqual(established, invalidLatent),
               "zero/invalid model-space Noise changed the established neural "
               "evaluation path");
    }

    neuramar::SynthesisFrame negativeLatent;
    neuramar::SynthesisFrame positiveLatent;
    model.evaluate(0.43f, -1.0f, negativeLatent);
    model.evaluate(0.43f, 1.0f, positiveLatent);
    expect(synthesisFrameDistance(negativeLatent, positiveLatent) > 1.0e-7,
           "model-space Noise did not move through the learned trajectory");

    const auto makeParameters = [] (float noise)
    {
        neuramar::EngineParameters parameters;
        parameters.imprint = 1.0f;
        parameters.bodyLock = 0.35f;
        parameters.air = 0.0f;
        parameters.bone = 0.0f;
        parameters.brightness = 0.5f;
        parameters.evolutionRate = 1.0f;
        parameters.orbit = 1.0f;
        parameters.mutation = 0.0f;
        parameters.noise = noise;
        parameters.attackSeconds = 0.0f;
        parameters.releaseSeconds = 0.25f;
        parameters.spread = 0.0f;
        parameters.outputGain = 0.08f;
        return parameters;
    };
    const auto configure = [&model, &makeParameters] (
        neuramar::NeuramarEngine& engine, float noise)
    {
        engine.prepare(48000.0, 128);
        engine.setModel(&model);
        engine.setParameters(makeParameters(noise));
    };

    neuramar::NeuramarEngine exactRecall;
    neuramar::NeuramarEngine evolvingA;
    neuramar::NeuramarEngine evolvingB;
    configure(exactRecall, 0.0f);
    configure(evolvingA, 1.0f);
    configure(evolvingB, 1.0f);
    constexpr int renderSamples = 72000;
    const auto recalled = renderNote(
        exactRecall, model.rootMidiNote(), renderSamples);
    const auto evolvedA = renderNote(
        evolvingA, model.rootMidiNote(), renderSamples);
    const auto evolvedB = renderNote(
        evolvingB, model.rootMidiNote(), renderSamples);
    const auto evolvedASecond = renderNote(
        evolvingA, model.rootMidiNote(), renderSamples);
    const auto evolvedBSecond = renderNote(
        evolvingB, model.rootMidiNote(), renderSamples);

    double changedSquareSum = 0.0;
    double retriggerSquareSum = 0.0;
    float deterministicMaximumDifference = 0.0f;
    float secondDeterministicMaximumDifference = 0.0f;
    for (std::size_t index = 0; index < recalled.size(); ++index)
    {
        const double difference = static_cast<double>(
            evolvedA[index] - recalled[index]);
        changedSquareSum += difference * difference;
        const double retriggerDifference = static_cast<double>(
            evolvedASecond[index] - evolvedA[index]);
        retriggerSquareSum += retriggerDifference * retriggerDifference;
        deterministicMaximumDifference = std::max(
            deterministicMaximumDifference,
            std::abs(evolvedA[index] - evolvedB[index]));
        secondDeterministicMaximumDifference = std::max(
            secondDeterministicMaximumDifference,
            std::abs(evolvedASecond[index] - evolvedBSecond[index]));
        expect(std::isfinite(evolvedA[index])
                   && std::abs(evolvedA[index]) < 7.94f,
               "model-space Noise produced invalid or guarded audio");
    }
    const double changedRms = std::sqrt(
        changedSquareSum / static_cast<double>(recalled.size()));
    expect(deterministicMaximumDifference == 0.0f,
           "identical model-space Noise voices were not deterministic");
    expect(secondDeterministicMaximumDifference == 0.0f,
           "identical model-space Noise event sequences diverged");
    expect(changedRms > 1.0e-6,
           "Noise did not evolve the rendered neural trajectory");
    const double retriggerRms = std::sqrt(
        retriggerSquareSum / static_cast<double>(evolvedA.size()));
    expect(retriggerRms > 1.0e-6,
           "positive Noise reused the same latent path on every retrigger");

    evolvingA.reset();
    const auto evolvedAfterReset = renderNote(
        evolvingA, model.rootMidiNote(), renderSamples);
    float resetMaximumDifference = 0.0f;
    for (std::size_t index = 0; index < evolvedA.size(); ++index)
    {
        resetMaximumDifference = std::max(
            resetMaximumDifference,
            std::abs(evolvedAfterReset[index] - evolvedA[index]));
    }
    expect(resetMaximumDifference == 0.0f,
           "reset did not reproduce the deterministic Noise event sequence");

    const auto energyRatio = static_cast<double>(rms(evolvedA, 4096))
        / std::max(static_cast<double>(rms(recalled, 4096)), 1.0e-12);
    expect(energyRatio >= 0.35 && energyRatio <= 2.85,
           "model-space Noise escaped the model's established energy range");

    neuramar::NeuramarEngine automated;
    configure(automated, 0.0f);
    automated.noteOn(model.rootMidiNote(), 0.82f);
    constexpr int automationSegmentSamples = 12288;
    std::vector<float> beforeAutomation(
        static_cast<std::size_t>(automationSegmentSamples), 0.0f);
    std::vector<float> beforeAutomationRight(
        static_cast<std::size_t>(automationSegmentSamples), 0.0f);
    std::vector<float> afterAutomation(
        static_cast<std::size_t>(automationSegmentSamples), 0.0f);
    std::vector<float> afterAutomationRight(
        static_cast<std::size_t>(automationSegmentSamples), 0.0f);
    automated.process(
        beforeAutomation.data(), beforeAutomationRight.data(),
        automationSegmentSamples);
    automated.setParameters(makeParameters(1.0f));
    automated.process(
        afterAutomation.data(), afterAutomationRight.data(),
        automationSegmentSamples);
    const float boundaryStep = std::abs(
        afterAutomation.front() - beforeAutomation.back());
    float neighbouringStep = 0.0f;
    for (std::size_t index = beforeAutomation.size() - 512;
         index + 1 < beforeAutomation.size(); ++index)
    {
        neighbouringStep = std::max(
            neighbouringStep,
            std::abs(beforeAutomation[index + 1] - beforeAutomation[index]));
    }
    for (std::size_t index = 0; index + 1 < 512; ++index)
    {
        neighbouringStep = std::max(
            neighbouringStep,
            std::abs(afterAutomation[index + 1] - afterAutomation[index]));
    }
    expect(boundaryStep <= 6.0f * neighbouringStep + 1.0e-4f,
           "mid-note Noise automation introduced a discontinuous output step");
    for (const auto sample : afterAutomation)
    {
        expect(std::isfinite(sample) && std::abs(sample) < 7.94f,
               "mid-note Noise automation produced invalid audio");
    }

    for (const int midiNote : { 0, 24, 60, 96, 127 })
    {
        neuramar::NeuramarEngine wideRegister;
        configure(wideRegister, 1.0f);
        const auto audio = renderNote(wideRegister, midiNote, 24000);
        expect(rms(audio, 2048) > 1.0e-7f,
               "model-space Noise became silent in the wide MIDI range");
        for (const auto sample : audio)
        {
            expect(std::isfinite(sample) && std::abs(sample) < 7.94f,
                   "model-space Noise was unstable in the wide MIDI range");
        }
    }
}

[[nodiscard]] float renderLayerDifferenceRms(
    const neuramar::NeuralModel& model, double sampleRate, int midiNote,
    float airLevel, float boneLevel)
{
    neuramar::NeuramarEngine dry;
    neuramar::NeuramarEngine wet;
    dry.prepare(sampleRate, 128);
    wet.prepare(sampleRate, 128);
    dry.setModel(&model);
    wet.setModel(&model);

    neuramar::EngineParameters parameters;
    parameters.imprint = 1.0f;
    parameters.bodyLock = 0.0f;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.brightness = 0.5f;
    parameters.mutation = 0.0f;
    parameters.attackSeconds = 0.0f;
    parameters.spread = 0.0f;
    parameters.outputGain = 0.08f;
    dry.setParameters(parameters);
    parameters.air = airLevel;
    parameters.bone = boneLevel;
    wet.setParameters(parameters);

    dry.noteOn(midiNote, 1.0f);
    wet.noteOn(midiNote, 1.0f);
    const int sampleCount = static_cast<int>(std::lround(0.82 * sampleRate));
    std::vector<float> difference(static_cast<std::size_t>(sampleCount), 0.0f);
    std::array<float, 128> dryLeft {};
    std::array<float, 128> dryRight {};
    std::array<float, 128> wetLeft {};
    std::array<float, 128> wetRight {};
    for (int offset = 0; offset < sampleCount; offset += 128)
    {
        const int count = std::min(128, sampleCount - offset);
        dry.process(dryLeft.data(), dryRight.data(), count);
        wet.process(wetLeft.data(), wetRight.data(), count);
        for (int index = 0; index < count; ++index)
            difference[static_cast<std::size_t>(offset + index)]
                = wetLeft[static_cast<std::size_t>(index)]
                - dryLeft[static_cast<std::size_t>(index)];
    }
    return rms(difference, static_cast<std::size_t>(0.16 * sampleRate));
}

struct AirMetrics
{
    float medianFraction { 0.0f };
    float medianAir { 0.0f };
    float medianCore { 0.0f };
};

[[nodiscard]] AirMetrics measureAir(const neuramar::NeuralModel& model)
{
    constexpr std::array<float, 7> times {
        0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f
    };
    std::array<float, times.size()> fractions {};
    std::array<float, times.size()> airValues {};
    std::array<float, times.size()> coreValues {};
    for (std::size_t index = 0; index < times.size(); ++index)
    {
        neuramar::SynthesisFrame frame;
        model.evaluate(times[index], frame);
        double airSquare = 0.0;
        double coreSquare = 0.0;
        for (const float amplitude : frame.airAmplitudes)
            airSquare += static_cast<double>(amplitude) * amplitude;
        for (const float amplitude : frame.harmonicAmplitudes)
            coreSquare += static_cast<double>(amplitude) * amplitude;
        airValues[index] = static_cast<float>(std::sqrt(airSquare));
        coreValues[index] = static_cast<float>(std::sqrt(coreSquare));
        fractions[index] = airValues[index] / std::max(coreValues[index], 1.0e-6f);
    }
    std::sort(fractions.begin(), fractions.end());
    std::sort(airValues.begin(), airValues.end());
    std::sort(coreValues.begin(), coreValues.end());
    const std::size_t middle = times.size() / 2;
    return { fractions[middle], airValues[middle], coreValues[middle] };
}

[[nodiscard]] std::string describeBoneModes(const neuramar::NeuralModel& model)
{
    std::string result;
    const auto ratios = model.boneFrequencyRatios();
    const auto decays = model.boneDecaySeconds();
    const auto reliabilities = model.boneModeReliabilities();
    for (std::size_t mode = 0; mode < ratios.size(); ++mode)
    {
        if (!result.empty())
            result += ", ";
        result += "(" + std::to_string(ratios[mode])
            + ", rel=" + std::to_string(reliabilities[mode])
            + ", decay=" + std::to_string(decays[mode]) + ")";
    }
    return result;
}

[[nodiscard]] float predictedAirCentroid(const neuramar::NeuralModel& model,
                                         float normalisedTime)
{
    neuramar::SynthesisFrame frame;
    model.evaluate(normalisedTime, frame);
    const auto centres = model.airCentreFrequenciesHz();
    const auto widths = model.airBandwidthOctaves();
    std::array<neuramar::airfilter::Coefficients,
               neuramar::NeuralModel::airBandCount> coefficients {};
    for (std::size_t band = 0; band < coefficients.size(); ++band)
        coefficients[band] = neuramar::airfilter::makeCoefficients(
            centres[band], widths[band], 48000.0f);

    double weightedFrequency = 0.0;
    double totalPower = 0.0;
    constexpr int analysisBins = 1024;
    for (int bin = 1; bin < analysisBins; ++bin)
    {
        const float frequency = 16000.0f * static_cast<float>(bin)
            / static_cast<float>(analysisBins);
        double power = 0.0;
        for (std::size_t band = 0; band < coefficients.size(); ++band)
        {
            const double amplitude = frame.airAmplitudes[band];
            power += amplitude * amplitude
                * neuramar::airfilter::unitNoisePowerResponse(
                    coefficients[band], frequency, 48000.0f);
        }
        weightedFrequency += static_cast<double>(frequency) * power;
        totalPower += power;
    }
    return static_cast<float>(weightedFrequency
        / std::max(totalPower, 1.0e-20));
}

[[nodiscard]] float spectralPeak(const std::vector<float>& signal,
                                 double sampleRate, float expectedHz)
{
    const std::size_t first = std::min<std::size_t>(signal.size() / 5,
                                                    signal.size());
    const std::size_t count = std::min<std::size_t>(16384, signal.size() - first);
    const float lower = 0.94f * expectedHz;
    const float upper = 1.06f * expectedHz;
    const float step = std::max(0.20f, expectedHz / 1000.0f);
    // A parabolic vertex over the strongest three probes keeps the estimate
    // limited by the render rather than by this sweep's step size.
    std::vector<double> energies;
    std::size_t best = 0;
    for (float frequency = lower; frequency <= upper; frequency += step)
    {
        std::complex<double> sum;
        const std::complex<double> rotation = std::polar(
            1.0, -2.0 * pi * static_cast<double>(frequency) / sampleRate);
        std::complex<double> oscillator(1.0, 0.0);
        for (std::size_t index = 0; index < count; ++index)
        {
            const double window = 0.5 - 0.5 * std::cos(
                2.0 * pi * static_cast<double>(index)
                / static_cast<double>(std::max<std::size_t>(1, count - 1)));
            sum += static_cast<double>(signal[first + index]) * window * oscillator;
            oscillator *= rotation;
        }
        energies.push_back(std::norm(sum));
        if (energies.back() > energies[best])
            best = energies.size() - 1;
    }
    if (energies.empty())
        return expectedHz;

    double offset = 0.0;
    if (best > 0 && best + 1 < energies.size())
    {
        const double left = std::log(std::max(energies[best - 1], 1.0e-300));
        const double centre = std::log(std::max(energies[best], 1.0e-300));
        const double right = std::log(std::max(energies[best + 1], 1.0e-300));
        const double denominator = left - 2.0 * centre + right;
        if (denominator < -1.0e-18)
            offset = std::clamp(0.5 * (left - right) / denominator, -0.5, 0.5);
    }
    return lower + static_cast<float>(
        (static_cast<double>(best) + offset) * static_cast<double>(step));
}

[[nodiscard]] std::vector<float> renderNote(neuramar::NeuramarEngine& engine,
                                            int midiNote, int sampleCount)
{
    std::vector<float> left(static_cast<std::size_t>(sampleCount), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(sampleCount), 0.0f);
    engine.noteOn(midiNote, 0.82f);
    constexpr int blockSize = 128;
    for (int offset = 0; offset < sampleCount; offset += blockSize)
    {
        const int count = std::min(blockSize, sampleCount - offset);
        engine.process(left.data() + offset, right.data() + offset, count);
    }
    engine.allSoundOff();
    for (std::size_t index = 0; index < left.size(); ++index)
        left[index] = 0.5f * (left[index] + right[index]);
    return left;
}

[[nodiscard]] float windowedSinusoidAmplitude(
    const std::vector<float>& signal, double sampleRate,
    double centreSeconds, double frequencyHz)
{
    const auto centre = static_cast<std::ptrdiff_t>(std::llround(
        centreSeconds * sampleRate));
    const auto halfWindow = static_cast<std::ptrdiff_t>(std::llround(
        0.040 * sampleRate));
    const auto first = std::max<std::ptrdiff_t>(0, centre - halfWindow);
    const auto last = std::min<std::ptrdiff_t>(
        static_cast<std::ptrdiff_t>(signal.size()), centre + halfWindow + 1);
    if (last - first < 32)
        return 0.0f;

    double cosineCosine = 0.0;
    double sineSine = 0.0;
    double cosineSine = 0.0;
    double signalCosine = 0.0;
    double signalSine = 0.0;
    const double denominator = static_cast<double>(last - first - 1);
    for (auto index = first; index < last; ++index)
    {
        const double position = static_cast<double>(index - first) / denominator;
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * position);
        const double angle = 2.0 * pi * frequencyHz
            * static_cast<double>(index) / sampleRate;
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        const double value = signal[static_cast<std::size_t>(index)];
        cosineCosine += window * cosine * cosine;
        sineSine += window * sine * sine;
        cosineSine += window * cosine * sine;
        signalCosine += window * value * cosine;
        signalSine += window * value * sine;
    }
    const double determinant = cosineCosine * sineSine
        - cosineSine * cosineSine;
    if (determinant <= 1.0e-12)
        return 0.0f;
    const double cosineCoefficient = (signalCosine * sineSine
        - signalSine * cosineSine) / determinant;
    const double sineCoefficient = (signalSine * cosineCosine
        - signalCosine * cosineSine) / determinant;
    return static_cast<float>(std::hypot(cosineCoefficient, sineCoefficient));
}

[[nodiscard]] float segmentRms(const std::vector<float>& signal,
                               double sampleRate,
                               double firstSeconds,
                               double lastSeconds)
{
    const auto first = std::min(signal.size(), static_cast<std::size_t>(
        std::max(0.0, std::floor(firstSeconds * sampleRate))));
    const auto last = std::min(signal.size(), static_cast<std::size_t>(
        std::max(0.0, std::ceil(lastSeconds * sampleRate))));
    if (last <= first)
        return 0.0f;
    double squareSum = 0.0;
    for (std::size_t index = first; index < last; ++index)
        squareSum += static_cast<double>(signal[index]) * signal[index];
    return static_cast<float>(std::sqrt(
        squareSum / static_cast<double>(last - first)));
}

void testWideRegisterCore(const neuramar::NeuralModel& model)
{
    constexpr double sampleRate = 48000.0;
    const auto render = [&model](int midiNote, float bodyLock)
    {
        neuramar::NeuramarEngine engine;
        engine.prepare(sampleRate, 128);
        engine.setModel(&model);
        neuramar::EngineParameters parameters;
        parameters.imprint = 1.0f;
        parameters.bodyLock = bodyLock;
        parameters.air = 0.0f;
        parameters.bone = 0.0f;
        parameters.brightness = 0.5f;
        parameters.evolutionRate = 1.0f;
        parameters.orbit = 0.0f;
        parameters.mutation = 0.0f;
        parameters.attackSeconds = 0.0f;
        parameters.spread = 0.0f;
        parameters.outputGain = 0.04f;
        engine.setParameters(parameters);
        return renderNote(engine, midiNote,
                          static_cast<int>(std::lround(0.82 * sampleRate)));
    };

    const auto root = render(model.rootMidiNote(), 1.0f);
    const auto lockedDown = render(model.rootMidiNote() - 12, 1.0f);
    const auto followingDown = render(model.rootMidiNote() - 12, 0.0f);
    const auto lockedDownTwoOctaves = render(model.rootMidiNote() - 24, 1.0f);
    const auto lockedUp = render(model.rootMidiNote() + 12, 1.0f);
    const auto followingUp = render(model.rootMidiNote() + 12, 0.0f);
    constexpr double analysisTime = 0.52;
    neuramar::SynthesisFrame frame;
    model.evaluate(static_cast<float>(analysisTime / model.durationSeconds()), frame);

    constexpr std::array<int, 3> sourceHarmonics { 40, 48, 60 };
    double lockedRatioSum = 0.0;
    double followingRatioSum = 0.0;
    for (const int sourceHarmonic : sourceHarmonics)
    {
        const double frequency = model.rootFrequencyHz() * frame.pitchRatio
            * static_cast<float>(sourceHarmonic);
        const float rootAmplitude = windowedSinusoidAmplitude(
            root, sampleRate, analysisTime, frequency);
        const float lockedAmplitude = windowedSinusoidAmplitude(
            lockedDown, sampleRate, analysisTime, frequency);
        const float followingAmplitude = windowedSinusoidAmplitude(
            followingDown, sampleRate, analysisTime, frequency);
        lockedRatioSum += lockedAmplitude / std::max(rootAmplitude, 1.0e-8f);
        followingRatioSum += followingAmplitude / std::max(rootAmplitude, 1.0e-8f);
    }
    const double lockedRatio = lockedRatioSum
        / static_cast<double>(sourceHarmonics.size());
    const double followingRatio = followingRatioSum
        / static_cast<double>(sourceHarmonics.size());
    const float rootRms = segmentRms(root, sampleRate, 0.20, 0.72);
    const float downRms = segmentRms(lockedDown, sampleRate, 0.20, 0.72);
    const float levelRatio = downRms / std::max(rootRms, 1.0e-8f);

    constexpr std::array<int, 3> twoOctaveSourceHarmonics { 40, 48, 60 };
    double twoOctaveRatioSum = 0.0;
    for (const int sourceHarmonic : twoOctaveSourceHarmonics)
    {
        const double frequency = model.rootFrequencyHz() * frame.pitchRatio
            * static_cast<float>(sourceHarmonic);
        const float rootAmplitude = windowedSinusoidAmplitude(
            root, sampleRate, analysisTime, frequency);
        const float lowAmplitude = windowedSinusoidAmplitude(
            lockedDownTwoOctaves, sampleRate, analysisTime, frequency);
        twoOctaveRatioSum += lowAmplitude / std::max(rootAmplitude, 1.0e-8f);
    }
    const double twoOctaveRatio = twoOctaveRatioSum
        / static_cast<double>(twoOctaveSourceHarmonics.size());
    const float twoOctaveRms = segmentRms(
        lockedDownTwoOctaves, sampleRate, 0.20, 0.72);
    const float twoOctaveLevelRatio = twoOctaveRms
        / std::max(rootRms, 1.0e-8f);

    constexpr std::array<int, 2> upperOutputHarmonics { 36, 40 };
    double lockedOutsideRatioSum = 0.0;
    for (const int outputHarmonic : upperOutputHarmonics)
    {
        const double frequency = 2.0 * model.rootFrequencyHz() * frame.pitchRatio
            * static_cast<float>(outputHarmonic);
        const float lockedAmplitude = windowedSinusoidAmplitude(
            lockedUp, sampleRate, analysisTime, frequency);
        const float followingAmplitude = windowedSinusoidAmplitude(
            followingUp, sampleRate, analysisTime, frequency);
        lockedOutsideRatioSum += lockedAmplitude
            / std::max(followingAmplitude, 1.0e-8f);
    }
    const double lockedOutsideRatio = lockedOutsideRatioSum
        / static_cast<double>(upperOutputHarmonics.size());

    std::cout << "Wide-register diagnostics: locked high-band ratio "
              << lockedRatio << ", following ratio " << followingRatio
              << ", locked -12/root RMS ratio " << levelRatio
              << ", -24 high-band ratio " << twoOctaveRatio
              << ", -24/root RMS ratio " << twoOctaveLevelRatio
              << ", +12 out-of-envelope ratio " << lockedOutsideRatio << '\n';
    expect(lockedRatio >= 0.45 && lockedRatio <= 1.30,
           "Body Lock did not preserve the learned upper spectrum one octave down "
           "(mean partial ratio " + std::to_string(lockedRatio) + ")");
    expect(followingRatio <= 0.15 * lockedRatio + 0.02,
           "pitch-follow mode fabricated the extended Body-Locked partial bank");
    expect(levelRatio >= 0.70f && levelRatio <= 1.35f,
           "Body-Locked Core level drifted excessively one octave down (ratio "
               + std::to_string(levelRatio) + ")");
    expect(twoOctaveRatio >= 0.35 && twoOctaveRatio <= 1.10,
           "Body Lock did not retain evidence-backed upper partials two octaves down "
           "(mean partial ratio " + std::to_string(twoOctaveRatio) + ")");
    expect(twoOctaveLevelRatio >= 0.65f && twoOctaveLevelRatio <= 1.40f,
           "Body-Locked Core level drifted excessively two octaves down (ratio "
               + std::to_string(twoOctaveLevelRatio) + ")");
    expect(lockedOutsideRatio < 0.03,
           "strict Body Lock retained pitch-following energy above its observed "
           "source envelope (ratio " + std::to_string(lockedOutsideRatio) + ")");
}

void testHighRegisterOutputLinearity()
{
    constexpr double sampleRate = 48000.0;
    const auto learned = neuramar::SampleLearner::learn(
        makePureSineSample(sampleRate, 1000.0, 0.72), sampleRate);
    expect(static_cast<bool>(learned),
           "high-register alias fixture failed to learn: " + learned.error);
    if (!learned.model)
        return;

    const auto& model = *learned.model;
    neuramar::NeuramarEngine engine;
    engine.prepare(sampleRate, 128);
    engine.setModel(&model);
    neuramar::EngineParameters parameters;
    parameters.imprint = 1.0f;
    parameters.bodyLock = 0.0f;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.brightness = 0.5f;
    parameters.orbit = 0.0f;
    parameters.mutation = 0.0f;
    parameters.attackSeconds = 0.0f;
    parameters.spread = 0.0f;
    parameters.outputGain = 2.0f;
    engine.setParameters(parameters);

    constexpr int semitones = 38;
    const int midiNote = std::min(127, model.rootMidiNote() + semitones);
    const auto rendered = renderNote(
        engine, midiNote, static_cast<int>(std::lround(0.62 * sampleRate)));
    constexpr double analysisTime = 0.42;
    neuramar::SynthesisFrame frame;
    model.evaluate(static_cast<float>(analysisTime / model.durationSeconds()), frame);
    const double ratio = std::exp2(
        static_cast<double>(midiNote - model.rootMidiNote()) / 12.0);
    const double fundamental = model.rootFrequencyHz() * frame.pitchRatio * ratio;
    const double foldedThird = std::abs(sampleRate - 3.0 * fundamental);
    const float fundamentalAmplitude = windowedSinusoidAmplitude(
        rendered, sampleRate, analysisTime, fundamental);
    const float foldedAmplitude = windowedSinusoidAmplitude(
        rendered, sampleRate, analysisTime, foldedThird);
    const float foldRatio = foldedAmplitude
        / std::max(fundamentalAmplitude, 1.0e-8f);
    std::cout << "High-register diagnostics: f0 " << fundamental
              << " Hz, folded-third ratio " << foldRatio << '\n';
    expect(fundamental > 7000.0 && fundamental < 11000.0,
           "alias fixture did not land in the intended high register");
    expect(foldRatio < 0.005f,
           "maximum user output gain folded a high-register third harmonic "
           "back into the audible band (ratio " + std::to_string(foldRatio) + ")");
}

struct LearnedFixture
{
    std::vector<float> source;
    neuramar::SampleLearner::LearnResult first;
    neuramar::SampleLearner::LearnResult second;
};

void testInputValidationAndCancellation()
{
    const std::vector<float> silence(4096, 0.0f);
    const auto silentResult = neuramar::SampleLearner::learn(silence, 48000.0);
    expect(!silentResult && !silentResult.error.empty(),
           "silent input was accepted as a learnable instrument");

    auto source = makeLearningSample(32000.0, 329.627557, 0.72);
    int cancellationChecks = 0;
    const auto cancelled = neuramar::SampleLearner::learn(
        source, 32000.0, {}, [&cancellationChecks]
        {
            // Reach the cancellable conditioning/resampling loops rather than
            // only exercising the stage-boundary guard.
            return ++cancellationChecks >= 20;
        });
    expect(cancelled.cancelled && !cancelled.model,
           "cancelled learning published a partial model");
}

void testAirFilterNormalisation()
{
    constexpr std::size_t integrationSize = 65536;
    for (const float sampleRate : { 44100.0f, 48000.0f, 96000.0f })
    {
        for (const auto& [centre, width] : std::array {
                 std::pair { 120.0f, 1.10f },
                 std::pair { 1200.0f, 0.75f },
                 std::pair { 10000.0f, 1.25f } })
        {
            const auto coefficients = neuramar::airfilter::makeCoefficients(
                centre, width, sampleRate);
            double integratedPower = 0.0;
            for (std::size_t bin = 1; bin < integrationSize / 2; ++bin)
            {
                const float frequency = static_cast<float>(bin) * sampleRate
                    / static_cast<float>(integrationSize);
                integratedPower += 2.0
                    * neuramar::airfilter::unitNoisePowerResponse(
                        coefficients, frequency, sampleRate)
                    / static_cast<double>(integrationSize);
            }
            expect(std::abs(integratedPower - 1.0) < 0.012,
                   "Air filter was not unit-RMS calibrated at "
                       + std::to_string(static_cast<int>(sampleRate))
                       + " Hz (centre " + std::to_string(centre)
                       + " Hz, integrated power "
                       + std::to_string(integratedPower) + ")");
        }
    }
}

void testRootAcrossRatesAndRegisters()
{
    struct Scenario
    {
        double sampleRate;
        double frequencyHz;
        double durationSeconds;
        int expectedMidi;
    };
    constexpr std::array scenarios {
        Scenario { 32000.0, 329.627557, 0.72, 64 },
        Scenario { 44100.0, 110.0, 1.05, 45 }
    };

    for (const auto& scenario : scenarios)
    {
        const auto sample = makeLearningSample(
            scenario.sampleRate, scenario.frequencyHz, scenario.durationSeconds);
        const auto learned = neuramar::SampleLearner::learn(sample, scenario.sampleRate);
        expect(static_cast<bool>(learned),
               "cross-register learning failed: " + learned.error);
        if (!learned.model)
            continue;
        expect(std::abs(learned.model->rootFrequencyHz()
                            / static_cast<float>(scenario.frequencyHz) - 1.0f) < 0.018f,
               "cross-register root frequency missed its target");
        expect(learned.model->rootMidiNote() == scenario.expectedMidi,
               "cross-register root MIDI note was incorrect");
    }
}

[[nodiscard]] LearnedFixture learnFixture()
{
    LearnedFixture fixture;
    fixture.source = makeLearningSample(48000.0, 220.0, 0.92);
    float previousProgress = 0.0f;
    bool sawPitch = false;
    bool sawTraining = false;
    fixture.first = neuramar::SampleLearner::learn(
        fixture.source, 48000.0,
        [&](const neuramar::SampleLearner::Progress& progress)
        {
            expect(progress.overallProgress + 1.0e-6f >= previousProgress,
                   "learner progress moved backwards");
            previousProgress = progress.overallProgress;
            sawPitch = sawPitch
                || progress.stage == neuramar::SampleLearner::Stage::Pitch;
            sawTraining = sawTraining
                || progress.stage == neuramar::SampleLearner::Stage::Training;
        });
    expect(sawPitch && sawTraining, "learner omitted a public progress stage");
    expect(previousProgress >= 0.999f, "learner did not report completion");
    fixture.second = neuramar::SampleLearner::learn(fixture.source, 48000.0);
    return fixture;
}

void testLearningAndRoot(const LearnedFixture& fixture)
{
    expect(static_cast<bool>(fixture.first),
           "first deterministic learning pass failed: " + fixture.first.error);
    expect(static_cast<bool>(fixture.second),
           "second deterministic learning pass failed: " + fixture.second.error);
    if (!fixture.first.model || !fixture.second.model)
        return;

    const auto& metadata = fixture.first.model->metadata();
    expect(std::abs(metadata.rootFrequencyHz / 220.0f - 1.0f) < 0.012f,
           "multi-resolution root estimate missed 220 Hz");
    expect(metadata.rootMidiNote == 57, "root MIDI note was not A3");
    expect(std::abs(metadata.rootCents) < 20.0f,
           "root cents were implausibly far from A3");
    expect(metadata.pitchConfidence > 0.55f,
           "clean harmonic fixture had low pitch confidence");
    expect(fixture.first.finalLoss < fixture.first.initialLoss,
           "Adam fitting did not reduce the neural loss");
    expect(fixture.first.finalLoss <= 0.82f * fixture.first.initialLoss,
           "Adam fitting reduced loss too little");
    expect(fixture.first.trainingEpochs == 180,
           "deterministic fit did not complete its bounded epoch budget");

    const auto firstBytes = fixture.first.model->serialize();
    const auto secondBytes = fixture.second.model->serialize();
    expect(firstBytes == secondBytes,
           "equal samples did not produce byte-identical neural models");
    expect(fixture.first.initialLoss == fixture.second.initialLoss
           && fixture.first.finalLoss == fixture.second.finalLoss,
           "deterministic fits reported different losses");
}

void testResidualScheduleAndQuality(const LearnedFixture& fixture)
{
    if (!fixture.first.model)
        return;

    const auto& corrected = *fixture.first.model;
    const auto bytes = corrected.serialize();
    const std::size_t extension = modelHeaderBytes + currentBasePayloadBytes;
    expect(readLittleU32(bytes, 4)
               == neuramar::NeuralModel::currentFormatVersion,
           "learner did not write the current v4 model format");
    const auto keyframeCount = readLittleU32(bytes, extension);
    expect(keyframeCount == 128,
           "learner did not fill the bounded 128-frame residual trajectory");
    const std::size_t expectedBytes = modelHeaderBytes + currentBasePayloadBytes
        + 4 + 4 * neuramar::NeuralModel::outputSize
        + keyframeCount * (4 + 2 * neuramar::NeuralModel::outputSize)
        + 4;
    expect(bytes.size() == expectedBytes && bytes.size() < 128 * 1024,
           "learned v4 payload escaped its exact bounded layout");
    expect(readLittleFloat(bytes, expectedBytes - 4)
               == corrected.inharmonicity(),
           "learned payload did not end with its inharmonicity coefficient");

    const std::size_t timesOffset = extension + 4
        + 4 * neuramar::NeuralModel::outputSize;
    std::size_t onsetFrames = 0;
    float previousTime = -1.0f;
    const float onsetEnd = std::min(
        1.0f, 0.120f / corrected.durationSeconds());
    for (std::size_t frame = 0; frame < keyframeCount; ++frame)
    {
        const float time = readLittleFloat(bytes, timesOffset + 4 * frame);
        expect(std::isfinite(time) && time >= 0.0f && time <= 1.0f
                   && time > previousTime,
               "residual schedule contained a duplicate or unsafe time");
        if (time <= onsetEnd + 1.0e-6f)
            ++onsetFrames;
        previousTime = time;
    }
    expect(readLittleFloat(bytes, timesOffset) == 0.0f
               && readLittleFloat(bytes,
                    timesOffset + 4 * (keyframeCount - 1)) == 1.0f,
           "residual schedule did not span the complete learned duration");
    expect(onsetFrames >= 40 && onsetFrames <= 48,
           "residual schedule did not reserve 40-48 frames for the first 120 ms "
               "(found " + std::to_string(onsetFrames) + ")");

    std::string error;
    auto neuralBase = neuramar::NeuralModel::deserialize(
        makeVersion2ModelBytes(corrected), &error);
    expect(neuralBase != nullptr,
           "could not isolate the v2 neural base for residual quality testing: "
               + error);
    if (!neuralBase)
        return;

    constexpr std::array<float, 7> positions {
        0.10f, 0.20f, 0.34f, 0.49f, 0.65f, 0.79f, 0.89f
    };
    constexpr std::array<int, 4> harmonics { 1, 2, 3, 5 };
    const auto ratioError = [&fixture, &positions, &harmonics](
        const neuramar::NeuralModel& model)
    {
        neuramar::NeuramarEngine engine;
        engine.prepare(48000.0, 128);
        engine.setModel(&model);
        neuramar::EngineParameters parameters;
        parameters.imprint = 1.0f;
        parameters.bodyLock = 0.0f;
        parameters.air = 0.0f;
        parameters.bone = 0.0f;
        parameters.brightness = 0.5f;
        parameters.evolutionRate = 1.0f;
        parameters.orbit = 0.0f;
        parameters.mutation = 0.0f;
        parameters.attackSeconds = 0.0f;
        parameters.spread = 0.0f;
        parameters.outputGain = 0.08f;
        engine.setParameters(parameters);
        const auto rendered = renderNote(
            engine, model.rootMidiNote(),
            static_cast<int>(fixture.source.size()));

        double totalErrorDb = 0.0;
        std::size_t count = 0;
        for (const float position : positions)
        {
            neuramar::SynthesisFrame frame;
            model.evaluate(position, frame);
            const double time = position * model.durationSeconds();
            const double renderedFundamental = model.rootFrequencyHz()
                * frame.pitchRatio;
            std::array<float, harmonics.size()> sourceAmplitudes {};
            std::array<float, harmonics.size()> renderedAmplitudes {};
            for (std::size_t index = 0; index < harmonics.size(); ++index)
            {
                sourceAmplitudes[index] = windowedSinusoidAmplitude(
                    fixture.source, 48000.0, time,
                    220.0 * static_cast<double>(harmonics[index]));
                renderedAmplitudes[index] = windowedSinusoidAmplitude(
                    rendered, 48000.0, time,
                    renderedFundamental * static_cast<double>(harmonics[index]));
            }
            for (std::size_t index = 1; index < harmonics.size(); ++index)
            {
                const double sourceRatio = sourceAmplitudes[index]
                    / std::max(sourceAmplitudes.front(), 1.0e-8f);
                const double modelRatio = renderedAmplitudes[index]
                    / std::max(renderedAmplitudes.front(), 1.0e-8f);
                totalErrorDb += std::abs(20.0 * std::log10(
                    std::max(modelRatio, 1.0e-8)
                    / std::max(sourceRatio, 1.0e-8)));
                ++count;
            }
        }
        return totalErrorDb / static_cast<double>(std::max<std::size_t>(count, 1));
    };

    const double baseErrorDb = ratioError(*neuralBase);
    const double correctedErrorDb = ratioError(corrected);
    std::cout << "Residual diagnostics: onset frames " << onsetFrames
              << ", neural-base harmonic-ratio MAE " << baseErrorDb
              << " dB, corrected MAE " << correctedErrorDb << " dB\n";
    expect(correctedErrorDb < 0.40 * baseErrorDb,
           "residual trajectory did not materially improve the neural base "
           "harmonic-ratio error (base " + std::to_string(baseErrorDb)
               + " dB, corrected " + std::to_string(correctedErrorDb) + " dB)");
}

void testRootCoreReconstruction(const LearnedFixture& fixture)
{
    if (!fixture.first.model)
        return;

    const auto& model = *fixture.first.model;
    neuramar::NeuramarEngine engine;
    engine.prepare(48000.0, 128);
    engine.setModel(&model);
    neuramar::EngineParameters parameters;
    parameters.imprint = 1.0f;
    parameters.bodyLock = 0.0f;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.brightness = 0.5f;
    parameters.evolutionRate = 1.0f;
    parameters.orbit = 0.0f;
    parameters.mutation = 0.0f;
    parameters.attackSeconds = 0.0f;
    parameters.spread = 0.0f;
    parameters.outputGain = 0.08f;
    engine.setParameters(parameters);
    const auto rendered = renderNote(
        engine, model.rootMidiNote(), static_cast<int>(fixture.source.size()));

    constexpr std::array<float, 7> positions {
        0.10f, 0.20f, 0.34f, 0.49f, 0.65f, 0.79f, 0.89f
    };
    constexpr std::array<int, 4> harmonics { 1, 2, 3, 5 };
    std::array<double, positions.size()> sourceFundamentalDb {};
    std::array<double, positions.size()> renderedFundamentalDb {};
    double ratioErrorDb = 0.0;
    int ratioCount = 0;
    for (std::size_t frameIndex = 0; frameIndex < positions.size(); ++frameIndex)
    {
        const float position = positions[frameIndex];
        const double time = position * model.durationSeconds();
        neuramar::SynthesisFrame frame;
        model.evaluate(position, frame);
        const double renderedFundamental = model.rootFrequencyHz()
            * frame.pitchRatio;
        std::array<float, harmonics.size()> sourceAmplitudes {};
        std::array<float, harmonics.size()> renderedAmplitudes {};
        for (std::size_t harmonicIndex = 0;
             harmonicIndex < harmonics.size(); ++harmonicIndex)
        {
            const int harmonic = harmonics[harmonicIndex];
            sourceAmplitudes[harmonicIndex] = windowedSinusoidAmplitude(
                fixture.source, 48000.0, time, 220.0 * harmonic);
            renderedAmplitudes[harmonicIndex] = windowedSinusoidAmplitude(
                rendered, 48000.0, time, renderedFundamental * harmonic);
        }
        sourceFundamentalDb[frameIndex] = 20.0 * std::log10(
            std::max(sourceAmplitudes.front(), 1.0e-8f));
        renderedFundamentalDb[frameIndex] = 20.0 * std::log10(
            std::max(renderedAmplitudes.front(), 1.0e-8f));
        for (std::size_t harmonicIndex = 1;
             harmonicIndex < harmonics.size(); ++harmonicIndex)
        {
            const double sourceRatio = sourceAmplitudes[harmonicIndex]
                / std::max(sourceAmplitudes.front(), 1.0e-8f);
            const double renderedRatio = renderedAmplitudes[harmonicIndex]
                / std::max(renderedAmplitudes.front(), 1.0e-8f);
            ratioErrorDb += std::abs(20.0 * std::log10(
                std::max(renderedRatio, 1.0e-8)
                / std::max(sourceRatio, 1.0e-8)));
            ++ratioCount;
        }
    }
    ratioErrorDb /= std::max(ratioCount, 1);

    double meanEnvelopeOffset = 0.0;
    for (std::size_t frame = 0; frame < positions.size(); ++frame)
        meanEnvelopeOffset += renderedFundamentalDb[frame]
            - sourceFundamentalDb[frame];
    meanEnvelopeOffset /= static_cast<double>(positions.size());
    double envelopeErrorDb = 0.0;
    for (std::size_t frame = 0; frame < positions.size(); ++frame)
        envelopeErrorDb += std::abs(
            renderedFundamentalDb[frame] - sourceFundamentalDb[frame]
            - meanEnvelopeOffset);
    envelopeErrorDb /= static_cast<double>(positions.size());

    const float sourceReference = segmentRms(
        fixture.source, 48000.0, 0.100, 0.160);
    const float renderedReference = segmentRms(
        rendered, 48000.0, 0.100, 0.160);
    constexpr std::array<std::pair<double, double>, 5> attackWindows {
        std::pair { 0.0, 0.004 }, std::pair { 0.002, 0.006 },
        std::pair { 0.004, 0.008 }, std::pair { 0.008, 0.016 },
        std::pair { 0.016, 0.032 }
    };
    double attackEnvelopeErrorDb = 0.0;
    double maximumAttackErrorDb = 0.0;
    for (const auto& [first, last] : attackWindows)
    {
        const double sourceRatio = segmentRms(
            fixture.source, 48000.0, first, last)
            / std::max(sourceReference, 1.0e-8f);
        const double renderedRatio = segmentRms(
            rendered, 48000.0, first, last)
            / std::max(renderedReference, 1.0e-8f);
        const double errorDb = 20.0 * std::log10(
            std::max(renderedRatio, 1.0e-8)
            / std::max(sourceRatio, 1.0e-8));
        attackEnvelopeErrorDb += std::abs(errorDb);
        maximumAttackErrorDb = std::max(
            maximumAttackErrorDb, std::abs(errorDb));
    }
    attackEnvelopeErrorDb /= static_cast<double>(attackWindows.size());

    std::cout << "Root reconstruction diagnostics: harmonic ratio MAE "
              << ratioErrorDb << " dB, fundamental envelope MAE "
              << envelopeErrorDb << " dB, attack envelope MAE "
              << attackEnvelopeErrorDb << " dB\n";
    expect(ratioErrorDb < 0.35,
           "root-note neural Core lost too much harmonic identity (mean ratio error "
               + std::to_string(ratioErrorDb) + " dB)");
    expect(envelopeErrorDb < 0.30,
           "root-note neural Core flattened its temporal envelope (mean error "
               + std::to_string(envelopeErrorDb) + " dB)");
    // The legacy fixed 85 ms aperture plus trailing control ramp measured
    // about 2.5 dB here; keep both transient fixes under objective guard.
    expect(attackEnvelopeErrorDb < 1.30,
           "root-note neural Core smeared its attack (mean early-envelope error "
               + std::to_string(attackEnvelopeErrorDb) + " dB)");
    expect(maximumAttackErrorDb < 3.50,
           "one root-note attack segment remained materially smeared (maximum error "
               + std::to_string(maximumAttackErrorDb) + " dB)");
}

void testMissingFundamentalRoot()
{
    constexpr float expectedRoot = 110.0f;
    const auto source = makeMissingFundamentalSample(48000.0, expectedRoot, 0.82);
    const auto learned = neuramar::SampleLearner::learn(source, 48000.0);
    expect(static_cast<bool>(learned),
           "missing-fundamental fixture failed to learn: " + learned.error);
    if (!learned.model)
        return;

    const auto& metadata = learned.model->metadata();
    expect(std::abs(metadata.rootFrequencyHz / expectedRoot - 1.0f) < 0.018f,
           "missing-fundamental complex was assigned to an upper harmonic");
    expect(metadata.rootMidiNote == 45,
           "missing-fundamental complex did not map to A2");
}

void testFormantAndOctaveDominantRoots()
{
    constexpr double sampleRate = 48000.0;
    constexpr float expectedRoot = 220.0f;
    for (const bool includeFundamental : { true, false })
    {
        const auto learned = neuramar::SampleLearner::learn(
            makeFormantDominantRootSample(
                sampleRate, expectedRoot, 0.82, includeFundamental),
            sampleRate);
        const std::string label = includeFundamental
            ? "formant-dominant odd spectrum"
            : "formant-dominant missing fundamental";
        expect(static_cast<bool>(learned),
               label + " failed to learn: " + learned.error);
        if (!learned.model)
            continue;
        expect(std::abs(learned.model->rootFrequencyHz() / expectedRoot - 1.0f)
                   < 0.012f,
               label + " locked onto a loud upper partial");
        expect(learned.model->rootMidiNote() == 57,
               label + " mapped to the wrong MIDI note");
        expect(learned.model->pitchConfidence() > 0.75f,
               label + " produced unexpectedly weak pitch confidence");
    }

    const auto octaveDominant = neuramar::SampleLearner::learn(
        makeOctaveDominantRootSample(sampleRate, expectedRoot, 0.82),
        sampleRate);
    expect(static_cast<bool>(octaveDominant),
           "octave-dominant root fixture failed to learn: "
               + octaveDominant.error);
    if (octaveDominant.model)
    {
        expect(std::abs(octaveDominant.model->rootFrequencyHz()
                            / expectedRoot - 1.0f) < 0.015f,
               "octave-dominant root fixture followed one loud partial");
        expect(octaveDominant.model->rootMidiNote() == 57,
               "octave-dominant fixture mapped to the upper octave");
    }
}

void testBrightHighPitchAcrossRates()
{
    constexpr float expectedRoot = 880.0f;
    float firstEstimate = 0.0f;
    for (const double sampleRate : { 48000.0, 96000.0 })
    {
        const auto source = makeBrightHighPitchSample(sampleRate, expectedRoot, 0.68);
        const auto learned = neuramar::SampleLearner::learn(source, sampleRate);
        expect(static_cast<bool>(learned),
               "bright high-pitch fixture failed to learn at "
                   + std::to_string(static_cast<int>(sampleRate)) + " Hz: "
                   + learned.error);
        if (!learned.model)
            continue;

        const float estimate = learned.model->rootFrequencyHz();
        expect(std::abs(estimate / expectedRoot - 1.0f) < 0.015f,
               "bright A5 root estimate drifted at "
                   + std::to_string(static_cast<int>(sampleRate)) + " Hz");
        expect(learned.model->rootMidiNote() == 81,
               "bright A5 fixture mapped to the wrong MIDI note at "
                   + std::to_string(static_cast<int>(sampleRate)) + " Hz");
        if (firstEstimate == 0.0f)
            firstEstimate = estimate;
        else
            expect(std::abs(estimate / firstEstimate - 1.0f) < 0.006f,
                   "root estimate changed materially with analysis sample rate");
    }
}

void testHarmonicSubtractedAir()
{
    const auto harmonicOnly = neuramar::SampleLearner::learn(
        makeAirSeparationSample(48000.0, false), 48000.0);
    const auto harmonicAndNoise = neuramar::SampleLearner::learn(
        makeAirSeparationSample(48000.0, true), 48000.0);
    expect(static_cast<bool>(harmonicOnly),
           "harmonic-only Air fixture failed to learn: " + harmonicOnly.error);
    expect(static_cast<bool>(harmonicAndNoise),
           "harmonic-plus-noise Air fixture failed to learn: "
               + harmonicAndNoise.error);
    if (!harmonicOnly || !harmonicAndNoise)
        return;

    const AirMetrics pure = measureAir(*harmonicOnly.model);
    const AirMetrics noisy = measureAir(*harmonicAndNoise.model);
    const std::string diagnostics = "pure fraction="
        + std::to_string(pure.medianFraction) + ", pure Air="
        + std::to_string(pure.medianAir) + ", pure core="
        + std::to_string(pure.medianCore) + ", noisy fraction="
        + std::to_string(noisy.medianFraction) + ", noisy Air="
        + std::to_string(noisy.medianAir) + ", noisy core="
        + std::to_string(noisy.medianCore);

    expect(pure.medianCore > 0.02f,
           "harmonic-only fixture learned no meaningful pitched core ("
               + diagnostics + ")");
    expect(pure.medianFraction < 0.002f,
           "harmonic subtraction leaked too much deterministic harmonic energy "
           "into Air (" + diagnostics + ")");
    expect(noisy.medianFraction > std::max(2.5f * pure.medianFraction,
                                           pure.medianFraction + 0.015f),
           "Air did not distinguish shaped noise from an otherwise identical "
           "harmonic source (" + diagnostics + ")");

    const float air48k = renderLayerDifferenceRms(
        *harmonicAndNoise.model, 48000.0,
        harmonicAndNoise.model->rootMidiNote(), 1.0f, 0.0f);
    const float air96k = renderLayerDifferenceRms(
        *harmonicAndNoise.model, 96000.0,
        harmonicAndNoise.model->rootMidiNote(), 1.0f, 0.0f);
    const float sampleRateRatio = air96k / std::max(air48k, 1.0e-9f);
    expect(air48k > 1.0e-5f,
           "learned Air was effectively inaudible in the render regression");
    expect(sampleRateRatio >= 0.90f && sampleRateRatio <= 1.10f,
           "Air level changed materially with host sample rate (48 kHz RMS "
               + std::to_string(air48k) + ", 96 kHz RMS "
               + std::to_string(air96k) + ", ratio "
               + std::to_string(sampleRateRatio) + ")");

    const float foldedAir = renderLayerDifferenceRms(
        *harmonicAndNoise.model, 8000.0, 127, 1.0f, 0.0f);
    expect(foldedAir < 0.01f * air48k + 1.0e-7f,
           "out-of-range Air bands piled up at the host Nyquist edge");

    testWideRegisterCore(*harmonicOnly.model);
}

void testPersistentBoneModeSelection()
{
    constexpr float sourceRootHz = 187.5f;
    const auto learned = neuramar::SampleLearner::learn(
        makePersistentBoneSample(48000.0), 48000.0);
    expect(static_cast<bool>(learned),
           "persistent Bone fixture failed to learn: " + learned.error);
    if (!learned)
        return;

    const auto& model = *learned.model;
    const auto ratios = model.boneFrequencyRatios();
    const auto decays = model.boneDecaySeconds();
    const auto reliabilities = model.boneModeReliabilities();
    const float ratioTolerance = std::max(
        0.08f, 1.5f * (48000.0f / 4096.0f) / model.rootFrequencyHz());
    int activeModeCount = 0;
    int persistentMatches = 0;
    int transientMatches = 0;
    int persistentDecayMatches = 0;
    for (const float reliability : reliabilities)
        activeModeCount += reliability > 0.0f ? 1 : 0;

    for (const float expectedRatio : persistentBoneRatios)
    {
        float closestDistance = std::numeric_limits<float>::max();
        std::size_t closestMode = 0;
        for (std::size_t mode = 0; mode < ratios.size(); ++mode)
        {
            if (reliabilities[mode] <= 0.0f)
                continue;
            const float distance = std::abs(ratios[mode] - expectedRatio);
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestMode = mode;
            }
        }
        if (closestDistance <= ratioTolerance)
        {
            ++persistentMatches;
            persistentDecayMatches += decays[closestMode] > 0.35f ? 1 : 0;
        }
    }
    for (const float expectedRatio : transientBoneRatios)
    {
        for (std::size_t mode = 0; mode < ratios.size(); ++mode)
        {
            if (reliabilities[mode] > 0.0f
                && std::abs(ratios[mode] - expectedRatio) <= ratioTolerance)
            {
                ++transientMatches;
                break;
            }
        }
    }

    const std::string diagnostics = "root="
        + std::to_string(model.rootFrequencyHz()) + " Hz, tolerance="
        + std::to_string(ratioTolerance) + ", active="
        + std::to_string(activeModeCount) + ", persistent matches="
        + std::to_string(persistentMatches) + ", transient matches="
        + std::to_string(transientMatches) + ", long-decay matches="
        + std::to_string(persistentDecayMatches) + ", modes="
        + describeBoneModes(model);
    const auto boneAir = measureAir(model);
    std::cout << "Bone separation diagnostics: Air/Core "
              << boneAir.medianFraction << '\n';
    expect(std::abs(model.rootFrequencyHz() / sourceRootHz - 1.0f) < 0.02f,
           "Bone fixture root estimate drifted (" + diagnostics + ")");
    expect(activeModeCount >= 5,
           "too few reliable Bone modes survived persistence scoring ("
               + diagnostics + ")");
    expect(persistentMatches >= 5,
           "persistence scoring failed to retain sustained inharmonic modes ("
               + diagnostics + ")");
    expect(transientMatches <= 1,
           "onset-only distractors displaced sustained Bone modes ("
               + diagnostics + ")");
    expect(persistentDecayMatches >= 5,
           "sustained Bone modes were assigned implausibly short decays ("
               + diagnostics + ")");
    expect(boneAir.medianFraction < 0.025f,
           "short-window modal leakage was mislearned as broadband Air (Air/Core "
               + std::to_string(boneAir.medianFraction) + ")");

    const float rootBone = renderLayerDifferenceRms(
        model, 48000.0, model.rootMidiNote(), 0.0f, 1.0f);
    const float foldedBone = renderLayerDifferenceRms(
        model, 8000.0, 127, 0.0f, 1.0f);
    expect(rootBone > 1.0e-6f,
           "reliable Bone fixture rendered no modal layer");
    expect(foldedBone < 0.01f * rootBone + 1.0e-7f,
           "out-of-range Bone modes piled up at the host Nyquist edge");
}

void testEvolvingAirColour()
{
    const auto learned = neuramar::SampleLearner::learn(
        makeEvolvingAirSample(48000.0), 48000.0);
    expect(static_cast<bool>(learned),
           "evolving-Air fixture failed to learn: " + learned.error);
    if (!learned.model)
        return;

    const float earlyCentroid = predictedAirCentroid(*learned.model, 0.18f);
    const float lateCentroid = predictedAirCentroid(*learned.model, 0.82f);
    expect(earlyCentroid > 80.0f && lateCentroid < 17000.0f,
           "evolving-Air fit produced an invalid spectral centroid");
    expect(lateCentroid > 1.65f * earlyCentroid,
           "neural Air field did not preserve the low-to-high colour evolution "
               "(early " + std::to_string(earlyCentroid) + " Hz, late "
               + std::to_string(lateCentroid) + " Hz)");
}

void testTransientAirResolution()
{
    const auto learned = neuramar::SampleLearner::learn(
        makeTransientAirSample(48000.0), 48000.0);
    expect(static_cast<bool>(learned),
           "transient-Air fixture failed to learn: " + learned.error);
    if (!learned.model)
        return;

    const auto airNorm = [&model = *learned.model](float timeSeconds)
    {
        neuramar::SynthesisFrame frame;
        model.evaluate(timeSeconds / model.durationSeconds(), frame);
        double power = 0.0;
        for (const float amplitude : frame.airAmplitudes)
            power += static_cast<double>(amplitude) * amplitude;
        return static_cast<float>(std::sqrt(power));
    };
    const float onsetAir = airNorm(0.004f);
    const float lateAir = airNorm(0.050f);
    std::cout << "Transient Air diagnostics: onset " << onsetAir
              << ", 50 ms " << lateAir << '\n';
    expect(onsetAir > 0.005f,
           "short noisy onset produced no learned Air energy");
    expect(onsetAir > 3.15f * lateAir,
           "multi-resolution analysis smeared a short Air burst beyond 50 ms "
               "(onset " + std::to_string(onsetAir) + ", late "
               + std::to_string(lateAir) + ")");
}

void testLearnedPitchContour(const neuramar::NeuralModel& steadyModel)
{
    const auto bentSource = makeSlowPitchBendSample(48000.0, 220.0, 1.18);
    const auto bent = neuramar::SampleLearner::learn(bentSource, 48000.0);
    expect(static_cast<bool>(bent),
           "slow pitch-bend fixture failed to learn: " + bent.error);
    if (!bent.model)
        return;

    neuramar::SynthesisFrame early;
    neuramar::SynthesisFrame middle;
    neuramar::SynthesisFrame late;
    bent.model->evaluate(0.16f, early);
    bent.model->evaluate(0.50f, middle);
    bent.model->evaluate(0.84f, late);
    expect(early.pitchRatio >= 0.78f && late.pitchRatio <= 1.28f,
           "learned bend escaped the constrained pitch-ratio range");
    expect(middle.pitchRatio > early.pitchRatio * 1.025f
           && late.pitchRatio > middle.pitchRatio * 1.025f,
           "learned pitch contour did not preserve the rising bend direction");
    expect(late.pitchRatio / early.pitchRatio > 1.075f,
           "learned pitch contour flattened a material three-semitone bend (early "
               + std::to_string(early.pitchRatio) + ", late "
               + std::to_string(late.pitchRatio) + ")");

    for (const float time : { 0.16f, 0.50f, 0.84f })
    {
        neuramar::SynthesisFrame steady;
        steadyModel.evaluate(time, steady);
        expect(std::abs(steady.pitchRatio - 1.0f) < 0.035f,
               "steady source acquired a material learned pitch contour at t="
                   + std::to_string(time) + " (ratio "
                   + std::to_string(steady.pitchRatio) + ")");
    }
}

void testSerialization(const neuramar::NeuralModel& model)
{
    const auto bytes = model.serialize();
    expect(bytes.size() > 1000 && bytes.size() < 128 * 1024,
           "serialized model size escaped its expected bound");
    std::string error;
    auto restored = neuramar::NeuralModel::deserialize(bytes, &error);
    expect(restored != nullptr, "valid model failed to deserialize: " + error);
    if (restored)
    {
        expect(restored->serialize() == bytes,
               "model binary did not survive a byte-exact round trip");
        expect(restored->rootMidiNote() == model.rootMidiNote(),
               "round trip changed root metadata");
    }

    auto damaged = bytes;
    damaged[damaged.size() / 2] ^= 0x40u;
    error.clear();
    auto rejected = neuramar::NeuralModel::deserialize(damaged, &error);
    expect(rejected == nullptr && !error.empty(),
           "checksum corruption was not rejected");

    error.clear();
    auto truncated = neuramar::NeuralModel::deserialize(
        std::span<const std::uint8_t>(bytes.data(), bytes.size() - 1), &error);
    expect(truncated == nullptr && !error.empty(),
           "truncated model was not rejected");

    const auto version2Bytes = makeVersion2ModelBytes(model);
    expect(version2Bytes.size() == modelHeaderBytes + version2PayloadBytes
               && readLittleU32(version2Bytes, 4) == 2,
           "test fixture did not produce a canonical version-2 payload");
    error.clear();
    auto version2 = neuramar::NeuralModel::deserialize(version2Bytes, &error);
    expect(version2 != nullptr,
           "real version-2 neural payload failed to deserialize: " + error);
    if (version2)
    {
        const auto migratedBytes = version2->serialize();
        const auto extension = modelHeaderBytes + currentBasePayloadBytes;
        expect(readLittleU32(migratedBytes, 4)
                   == neuramar::NeuralModel::currentFormatVersion
                   && readLittleU32(migratedBytes, extension) == 0,
               "version-2 model did not migrate to a zero-correction v3 payload");
        auto migrated = neuramar::NeuralModel::deserialize(migratedBytes, &error);
        expect(migrated != nullptr,
               "zero-correction v3 migration failed to deserialize: " + error);
        if (migrated)
        {
            for (const float time : { 0.0f, 0.017f, 0.25f, 0.5f, 0.83f, 1.0f })
            {
                neuramar::SynthesisFrame legacyFrame;
                neuramar::SynthesisFrame migratedFrame;
                version2->evaluate(time, legacyFrame);
                migrated->evaluate(time, migratedFrame);
                expect(legacyFrame.harmonicAmplitudes
                               == migratedFrame.harmonicAmplitudes
                           && legacyFrame.airAmplitudes
                               == migratedFrame.airAmplitudes
                           && legacyFrame.boneAmplitudes
                               == migratedFrame.boneAmplitudes
                           && legacyFrame.pitchRatio == migratedFrame.pitchRatio,
                       "zero-correction migration changed exact v2 rendering");
            }
        }
    }

    auto version2WithTrailingData = bytes;
    writeLittleU32(version2WithTrailingData, 4, 2);
    error.clear();
    auto trailingRejected = neuramar::NeuralModel::deserialize(
        version2WithTrailingData, &error);
    expect(trailingRejected == nullptr && !error.empty(),
           "version-2 decoder accepted a version-3 residual extension");

    const auto interpolatedBytes = makeLinearResidualModelBytes(model, 0.75f);
    error.clear();
    auto interpolated = neuramar::NeuralModel::deserialize(
        interpolatedBytes, &error);
    auto interpolationBase = neuramar::NeuralModel::deserialize(
        version2Bytes, &error);
    expect(interpolated != nullptr && interpolationBase != nullptr,
           "crafted residual interpolation fixture failed to deserialize: " + error);
    if (interpolated && interpolationBase)
    {
        for (const float time : { 0.0f, 0.125f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            neuramar::SynthesisFrame baseFrame;
            neuramar::SynthesisFrame correctedFrame;
            interpolationBase->evaluate(time, baseFrame);
            interpolated->evaluate(time, correctedFrame);
            const float correctionSemitones = 12.0f * std::log2(
                correctedFrame.pitchRatio / baseFrame.pitchRatio);
            expect(std::abs(correctionSemitones - 0.75f * time) < 2.0e-4f,
                   "residual interpolation did not follow its raw-space linear "
                   "trajectory at t=" + std::to_string(time));
            expect(correctedFrame.harmonicAmplitudes
                           == baseFrame.harmonicAmplitudes
                       && correctedFrame.airAmplitudes == baseFrame.airAmplitudes
                       && correctedFrame.boneAmplitudes == baseFrame.boneAmplitudes,
                   "a zero-scale residual altered an unrelated amplitude output");
        }
    }

    const std::size_t extension = modelHeaderBytes + currentBasePayloadBytes;
    auto excessiveKeyframes = bytes;
    writeLittleU32(excessiveKeyframes, extension, 129);
    refreshModelHeader(excessiveKeyframes,
                       neuramar::NeuralModel::currentFormatVersion);
    error.clear();
    auto excessiveRejected = neuramar::NeuralModel::deserialize(
        excessiveKeyframes, &error);
    expect(excessiveRejected == nullptr && !error.empty(),
           "v3 decoder accepted more than 128 residual keyframes");

    const auto keyframeCount = readLittleU32(bytes, extension);
    const std::size_t timesOffset = extension + 4
        + 4 * neuramar::NeuralModel::outputSize;
    if (keyframeCount >= 2)
    {
        auto duplicateTimes = bytes;
        writeLittleU32(duplicateTimes, timesOffset + 4,
                       readLittleU32(duplicateTimes, timesOffset));
        refreshModelHeader(duplicateTimes,
                           neuramar::NeuralModel::currentFormatVersion);
        error.clear();
        auto duplicateRejected = neuramar::NeuralModel::deserialize(
            duplicateTimes, &error);
        expect(duplicateRejected == nullptr && !error.empty(),
               "v3 decoder accepted duplicate residual keyframe times");

        auto forbiddenQuantisedValue = bytes;
        const std::size_t valuesOffset = timesOffset + 4 * keyframeCount;
        writeLittleU16(forbiddenQuantisedValue, valuesOffset, 0x8000u);
        refreshModelHeader(forbiddenQuantisedValue,
                           neuramar::NeuralModel::currentFormatVersion);
        error.clear();
        auto quantisedRejected = neuramar::NeuralModel::deserialize(
            forbiddenQuantisedValue, &error);
        expect(quantisedRejected == nullptr && !error.empty(),
               "v3 decoder accepted the non-canonical -32768 residual value");
    }

    neuramar::SynthesisFrame atZero;
    neuramar::SynthesisFrame atNan;
    model.evaluate(0.0f, atZero);
    model.evaluate(std::numeric_limits<float>::quiet_NaN(), atNan);
    expect(atNan.harmonicAmplitudes == atZero.harmonicAmplitudes
           && atNan.airAmplitudes == atZero.airAmplitudes
           && atNan.boneAmplitudes == atZero.boneAmplitudes
           && atNan.pitchRatio == atZero.pitchRatio,
           "non-finite model time was not safely mapped to the start frame");
}

void testRenderAndTransposition(const neuramar::NeuralModel& model)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(48000.0, 512);
    engine.setModel(&model);
    neuramar::EngineParameters parameters;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.attackSeconds = 0.002f;
    parameters.releaseSeconds = 0.04f;
    parameters.outputGain = 0.48f;
    engine.setParameters(parameters);

    const auto root = renderNote(engine, model.rootMidiNote(), 30000);
    const auto octave = renderNote(engine, model.rootMidiNote() + 12, 30000);
    expect(rms(root, 5000) > 0.005f, "root render was effectively silent");
    expect(rms(octave, 5000) > 0.003f, "transposed render was effectively silent");
    for (const auto value : root)
        expect(std::isfinite(value) && std::abs(value) <= 1.25001f,
               "root render produced non-finite or unbounded audio");
    for (const auto value : octave)
        expect(std::isfinite(value) && std::abs(value) <= 1.25001f,
               "octave render produced non-finite or unbounded audio");

    const float rootPeak = spectralPeak(root, 48000.0, model.rootFrequencyHz());
    const float octavePeak = spectralPeak(octave, 48000.0,
                                          2.0f * model.rootFrequencyHz());
    expect(std::abs(rootPeak / model.rootFrequencyHz() - 1.0f) < 0.012f,
           "root render did not follow the learned tuning");
    expect(std::abs(octavePeak / rootPeak - 2.0f) < 0.018f,
           "MIDI octave did not transpose synthesis by 2:1");
}

void testRootCorrectionRelabelsSourceKey(const neuramar::NeuralModel& model)
{
    neuramar::NeuramarEngine original;
    neuramar::NeuramarEngine corrected;
    original.prepare(48000.0, 128);
    corrected.prepare(48000.0, 128);
    original.setModel(&model);
    corrected.setModel(&model);

    neuramar::EngineParameters parameters;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.mutation = 0.0f;
    parameters.attackSeconds = 0.0f;
    parameters.spread = 0.0f;
    parameters.outputGain = 0.20f;
    original.setParameters(parameters);
    parameters.rootOffsetSemitones = 1.0f;
    corrected.setParameters(parameters);

    const auto reference = renderNote(
        original, model.rootMidiNote(), 4096);
    const auto relabelled = renderNote(
        corrected, model.rootMidiNote() + 1, 4096);
    float maximumDifference = 0.0f;
    for (std::size_t index = 0; index < reference.size(); ++index)
        maximumDifference = std::max(
            maximumDifference, std::abs(reference[index] - relabelled[index]));
    expect(maximumDifference < 1.0e-6f,
           "Root Correction retuned the source instead of relabelling its MIDI key");
}

void testReleaseAndVoiceBound(const neuramar::NeuralModel& model)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(48000.0, 256);
    engine.setModel(&model);
    neuramar::EngineParameters parameters;
    parameters.releaseSeconds = 0.025f;
    engine.setParameters(parameters);

    // Four more note-ons than the engine has voices, so the ceiling is what
    // bounds the count rather than the number of keys held.
    for (int note = 40; note < 44 + neuramar::NeuramarEngine::maximumVoices;
         ++note)
        engine.noteOn(note, 0.7f);
    expect(engine.getActiveVoiceCount() == neuramar::NeuramarEngine::maximumVoices,
           "polyphony was not bounded at the engine's voice ceiling");
    engine.allSoundOff();

    // A twelve-note held cluster - a four-note chord under a sustain pedal
    // while its predecessor still rings - has to sound twelve voices rather
    // than steal four of them.
    for (int note = 48; note < 60; ++note)
        engine.noteOn(note, 0.7f);
    expect(engine.getActiveVoiceCount() == 12,
           "a twelve-note held cluster was voice-stolen");
    engine.allSoundOff();
    for (int note = 40; note < 44 + neuramar::NeuramarEngine::maximumVoices;
         ++note)
        engine.noteOn(note, 0.7f);
    engine.allNotesOff();
    std::vector<float> left(30000, 0.0f);
    std::vector<float> right(30000, 0.0f);
    engine.process(left.data(), right.data(), static_cast<int>(left.size()));
    expect(engine.getActiveVoiceCount() == 0,
           "released voices did not become idle");
    expect(rms(left, left.size() - 2048) < 1.0e-4f,
           "release tail did not converge to silence");

    engine.noteOn(60, 1.0f);
    expect(engine.getActiveVoiceCount() == 1, "note-on did not activate a voice");
    engine.allSoundOff();
    expect(engine.getActiveVoiceCount() == 0,
           "allSoundOff did not stop immediately");
}

void testReleaseDurationSemantics(const neuramar::NeuralModel& model)
{
    constexpr double sampleRate = 48000.0;
    constexpr float configuredReleaseSeconds = 0.080f;
    neuramar::NeuramarEngine engine;
    engine.prepare(sampleRate, 64);
    engine.setModel(&model);
    neuramar::EngineParameters parameters;
    parameters.attackSeconds = 0.0f;
    parameters.releaseSeconds = configuredReleaseSeconds;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    engine.setParameters(parameters);

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    engine.noteOn(model.rootMidiNote(), 0.8f);
    for (int block = 0; block < 24; ++block)
        engine.process(left.data(), right.data(), static_cast<int>(left.size()));
    engine.noteOff(model.rootMidiNote());

    int releaseSamples = 0;
    const int hardLimit = static_cast<int>(2.0 * sampleRate);
    while (engine.getActiveVoiceCount() != 0 && releaseSamples < hardLimit)
    {
        engine.process(left.data(), right.data(), static_cast<int>(left.size()));
        releaseSamples += static_cast<int>(left.size());
    }
    const float measuredSeconds = static_cast<float>(releaseSamples / sampleRate);
    expect(engine.getActiveVoiceCount() == 0,
           "voice did not retire within the release regression guard");
    expect(measuredSeconds >= 0.70f * configuredReleaseSeconds,
           "voice retired substantially before its configured release duration");
    expect(measuredSeconds <= 1.35f * configuredReleaseSeconds + 0.002f,
           "configured release behaved as an exponential time constant instead of a duration ("
               + std::to_string(configuredReleaseSeconds) + " s configured, "
               + std::to_string(measuredSeconds) + " s measured)");
}

void testMutationZeroRetriggerDeterminism(const neuramar::NeuralModel& model)
{
    const auto prepareEngine = [&model](neuramar::NeuramarEngine& engine)
    {
        engine.prepare(48000.0, 128);
        engine.setModel(&model);
        neuramar::EngineParameters parameters;
        parameters.mutation = 0.0f;
        parameters.air = 0.55f;
        parameters.bone = 0.55f;
        parameters.attackSeconds = 0.0f;
        parameters.outputGain = 0.25f;
        engine.setParameters(parameters);
    };

    neuramar::NeuramarEngine pristine;
    prepareEngine(pristine);
    const auto reference = renderNote(pristine, model.rootMidiNote(), 4096);

    neuramar::NeuramarEngine advanced;
    prepareEngine(advanced);
    (void) renderNote(advanced, model.rootMidiNote() + 7, 512);
    const auto afterHistory = renderNote(advanced, model.rootMidiNote(), 4096);
    float maximumDifference = 0.0f;
    for (std::size_t index = 0; index < reference.size(); ++index)
        maximumDifference = std::max(maximumDifference,
            std::abs(reference[index] - afterHistory[index]));
    expect(maximumDifference < 1.0e-6f,
           "Mutation=0 still changed note phase/audio according to prior voice history (max delta "
               + std::to_string(maximumDifference) + ")");
}

void testStereoPanAndOverlappingNoteOff(const neuramar::NeuralModel& model)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(48000.0, 128);
    engine.setModel(&model);
    neuramar::EngineParameters parameters;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.mutation = 0.0f;
    parameters.attackSeconds = 0.0f;
    parameters.releaseSeconds = 0.020f;
    parameters.spread = 1.0f;
    parameters.outputGain = 0.35f;
    engine.setParameters(parameters);

    std::vector<float> left(2048, 0.0f);
    std::vector<float> right(2048, 0.0f);
    engine.noteOn(model.rootMidiNote(), 0.8f);
    engine.process(left.data(), right.data(), static_cast<int>(left.size()));
    float monoDelta = 0.0f;
    for (std::size_t index = 0; index < left.size(); ++index)
        monoDelta = std::max(monoDelta, std::abs(left[index] - right[index]));
    expect(monoDelta < 1.0e-7f,
           "a single note was not centred at maximum stereo spread");

    engine.allSoundOff();
    engine.noteOn(model.rootMidiNote(), 0.8f);
    engine.noteOn(model.rootMidiNote(), 0.8f);
    expect(engine.getActiveVoiceCount() == 2,
           "overlapping note-ons did not allocate two voices");
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    engine.process(left.data(), right.data(), static_cast<int>(left.size()));
    expect(std::abs(rms(left) - rms(right)) < 1.0e-6f,
           "a two-voice unison was not panned as a balanced pair");

    engine.noteOff(model.rootMidiNote());
    std::vector<float> releaseLeft(1600, 0.0f);
    std::vector<float> releaseRight(1600, 0.0f);
    engine.process(releaseLeft.data(), releaseRight.data(),
                   static_cast<int>(releaseLeft.size()));
    expect(engine.getActiveVoiceCount() == 1,
           "one note-off released every overlapping copy of a MIDI note");
    expect(rms(releaseLeft, 1200) + rms(releaseRight, 1200) > 0.002f,
           "the newer overlapping note stopped sounding after one note-off");

    engine.noteOff(model.rootMidiNote());
    engine.process(releaseLeft.data(), releaseRight.data(),
                   static_cast<int>(releaseLeft.size()));
    expect(engine.getActiveVoiceCount() == 0,
           "the second note-off did not release the remaining overlapping voice");
}

// A stiff-string partial series with a known coefficient. Upper partials decay
// faster than low ones, exactly as a struck string does.
[[nodiscard]] std::vector<float> makeStiffStringSample(
    double sampleRate, double fundamentalHz, double inharmonicity,
    double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    constexpr int partialCount = 36;
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double attack = 1.0 - std::exp(-time / 0.004);
        double value = 0.0;
        for (int partial = 1; partial <= partialCount; ++partial)
        {
            const double number = static_cast<double>(partial);
            const double frequency = fundamentalHz * number
                * std::sqrt(1.0 + inharmonicity * number * number);
            if (frequency >= 0.44 * sampleRate)
                break;
            const double decay = std::exp(
                -time * (0.85 + 0.052 * number));
            const double amplitude = std::pow(number, -1.05) * decay;
            value += amplitude * std::sin(
                2.0 * pi * frequency * time
                + 0.31 * number + 0.11 * number * number);
        }
        sample[index] = static_cast<float>(0.42 * attack * value);
    }
    return sample;
}

// Two fixed resonances well away from the fundamental, so a Formant shift has
// something unambiguous to move.
[[nodiscard]] std::vector<float> makeResonantBodySample(
    double sampleRate, double fundamentalHz, double durationSeconds)
{
    const auto sampleCount = static_cast<std::size_t>(
        std::llround(sampleRate * durationSeconds));
    std::vector<float> sample(sampleCount, 0.0f);
    const int partialCount = std::min(56, static_cast<int>(std::floor(
        0.44 * sampleRate / fundamentalHz)));
    for (std::size_t index = 0; index < sample.size(); ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double envelope = (1.0 - std::exp(-time / 0.007))
            * (0.86 + 0.14 * std::exp(-time / 0.42));
        double value = 0.0;
        for (int partial = 1; partial <= partialCount; ++partial)
        {
            const double frequency = fundamentalHz
                * static_cast<double>(partial);
            const double body = 0.09
                + 1.40 * gaussianLogFrequency(frequency, 780.0, 0.30)
                + 0.80 * gaussianLogFrequency(frequency, 2450.0, 0.26);
            const double amplitude = body
                / std::pow(static_cast<double>(partial), 0.65);
            value += amplitude * std::sin(
                2.0 * pi * frequency * time + 0.23 * static_cast<double>(partial));
        }
        sample[index] = static_cast<float>(0.13 * envelope * value);
    }
    return sample;
}

[[nodiscard]] double harmonicCentroidHz(const std::vector<float>& rendered,
                                        double sampleRate,
                                        double fundamentalHz,
                                        int partialCount,
                                        double analysisTime)
{
    double weighted = 0.0;
    double total = 0.0;
    for (int partial = 1; partial <= partialCount; ++partial)
    {
        const double frequency = fundamentalHz * static_cast<double>(partial);
        if (frequency >= 0.45 * sampleRate)
            break;
        const double amplitude = windowedSinusoidAmplitude(
            rendered, sampleRate, analysisTime, frequency);
        const double power = amplitude * amplitude;
        weighted += frequency * power;
        total += power;
    }
    return weighted / std::max(total, 1.0e-24);
}

[[nodiscard]] std::vector<float> renderWithParameters(
    const neuramar::NeuralModel& model, int midiNote, float velocity,
    const neuramar::EngineParameters& parameters, int sampleCount)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(48000.0, 128);
    engine.setModel(&model);
    engine.setParameters(parameters);

    std::vector<float> left(static_cast<std::size_t>(sampleCount), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(sampleCount), 0.0f);
    engine.noteOn(midiNote, velocity);
    for (int offset = 0; offset < sampleCount; offset += 128)
    {
        const int count = std::min(128, sampleCount - offset);
        engine.process(left.data() + offset, right.data() + offset, count);
    }
    for (std::size_t index = 0; index < left.size(); ++index)
        left[index] = 0.5f * (left[index] + right[index]);
    return left;
}

[[nodiscard]] neuramar::EngineParameters cleanCoreParameters()
{
    neuramar::EngineParameters parameters;
    parameters.imprint = 1.0f;
    parameters.bodyLock = 1.0f;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.brightness = 0.5f;
    parameters.evolutionRate = 1.0f;
    parameters.orbit = 0.0f;
    parameters.mutation = 0.0f;
    parameters.noise = 0.0f;
    parameters.attackSeconds = 0.0f;
    parameters.spread = 0.0f;
    parameters.outputGain = 0.20f;
    return parameters;
}

void testInharmonicPartialSeries()
{
    constexpr double sampleRate = 48000.0;
    constexpr double fundamental = 220.0;
    constexpr double trueInharmonicity = 3.0e-4;
    const auto source = makeStiffStringSample(
        sampleRate, fundamental, trueInharmonicity, 1.05);
    const auto learned = neuramar::SampleLearner::learn(source, sampleRate);
    expect(static_cast<bool>(learned),
           "stiff-string fixture failed to learn: " + learned.error);
    if (!learned.model)
        return;

    const auto& model = *learned.model;
    const float fitted = model.inharmonicity();
    std::cout << "Inharmonicity diagnostics: true B " << trueInharmonicity
              << ", fitted B " << fitted << ", root "
              << model.rootFrequencyHz() << " Hz\n";
    expect(fitted > 0.0f,
           "a clearly stiff partial series was fitted as perfectly harmonic");
    expect(std::abs(static_cast<double>(fitted) / trueInharmonicity - 1.0) < 0.35,
           "fitted stiff-string coefficient missed its known value (fitted "
               + std::to_string(fitted) + ")");
    expect(std::abs(model.rootFrequencyHz() / fundamental - 1.0f) < 0.02f,
           "stiff-string root drifted away from its known fundamental");

    // Render at the learned root and measure where each partial actually
    // lands, with the stretch enabled and forced off.
    auto stretched = cleanCoreParameters();
    stretched.bodyLock = 0.0f;
    auto flattened = stretched;
    flattened.stretch = 0.0f;
    constexpr int renderSamples = 24000;
    const auto stretchedRender = renderWithParameters(
        model, model.rootMidiNote(), 0.85f, stretched, renderSamples);
    const auto flatRender = renderWithParameters(
        model, model.rootMidiNote(), 0.85f, flattened, renderSamples);

    neuramar::SynthesisFrame frame;
    model.evaluate(0.20f / model.durationSeconds(), frame);
    const double renderedRoot = static_cast<double>(model.rootFrequencyHz())
        * frame.pitchRatio;
    double stretchedCentsError = 0.0;
    double flatCentsError = 0.0;
    int measured = 0;
    // Below partial 15 the ideal-series spacing stays wider than the peak
    // search window, so every measurement is unambiguous.
    for (int partial = 6; partial <= 14; partial += 2)
    {
        const double number = static_cast<double>(partial);
        const double expectedHz = renderedRoot * number
            * std::sqrt(1.0 + trueInharmonicity * number * number);
        if (expectedHz >= 0.40 * sampleRate)
            break;
        const double stretchedHz = spectralPeak(
            stretchedRender, sampleRate, static_cast<float>(expectedHz));
        const double flatHz = spectralPeak(
            flatRender, sampleRate, static_cast<float>(expectedHz));
        stretchedCentsError += std::abs(
            1200.0 * std::log2(stretchedHz / expectedHz));
        flatCentsError += std::abs(1200.0 * std::log2(flatHz / expectedHz));
        ++measured;
    }
    expect(measured > 0, "no stiff-string partials were measurable");
    if (measured == 0)
        return;
    stretchedCentsError /= static_cast<double>(measured);
    flatCentsError /= static_cast<double>(measured);
    std::cout << "Inharmonicity diagnostics: partial placement error "
              << stretchedCentsError << " cents with Stretch, "
              << flatCentsError << " cents without\n";
    expect(stretchedCentsError < 18.0,
           "stretched rendering did not place its partials on the learned "
           "stiff-string series (" + std::to_string(stretchedCentsError)
               + " cents)");
    expect(stretchedCentsError < 0.45 * flatCentsError,
           "the Stretch control did not materially improve partial placement "
           "over an ideal harmonic bank");

    // A genuinely harmonic source must still fit exactly zero, so nothing
    // about an ordinary sustained note changes.
    const auto harmonicSource = makeLearningSample(sampleRate, 220.0, 0.82);
    const auto harmonicLearned = neuramar::SampleLearner::learn(
        harmonicSource, sampleRate);
    expect(harmonicLearned.model != nullptr
               && harmonicLearned.model->inharmonicity() == 0.0f,
           "an ideal harmonic source was given a non-zero stiff-string "
           "coefficient");
}

void testFormantShift()
{
    constexpr double sampleRate = 48000.0;
    constexpr double fundamental = 220.0;
    const auto source = makeResonantBodySample(sampleRate, fundamental, 0.85);
    const auto learned = neuramar::SampleLearner::learn(source, sampleRate);
    expect(static_cast<bool>(learned),
           "resonant-body fixture failed to learn: " + learned.error);
    if (!learned.model)
        return;

    const auto& model = *learned.model;
    constexpr int renderSamples = 24000;
    const auto render = [&](float semitones)
    {
        auto parameters = cleanCoreParameters();
        parameters.formantShiftSemitones = semitones;
        return renderWithParameters(model, model.rootMidiNote(), 0.85f,
                                    parameters, renderSamples);
    };

    const auto neutral = render(0.0f);
    const auto raised = render(12.0f);
    const auto lowered = render(-12.0f);
    const double neutralCentroid = harmonicCentroidHz(
        neutral, sampleRate, static_cast<double>(model.rootFrequencyHz()),
        40, 0.25);
    const double raisedCentroid = harmonicCentroidHz(
        raised, sampleRate, static_cast<double>(model.rootFrequencyHz()),
        40, 0.25);
    const double loweredCentroid = harmonicCentroidHz(
        lowered, sampleRate, static_cast<double>(model.rootFrequencyHz()),
        40, 0.25);
    std::cout << "Formant diagnostics: centroid " << loweredCentroid
              << " / " << neutralCentroid << " / " << raisedCentroid
              << " Hz at -12 / 0 / +12 st\n";
    expect(raisedCentroid > 1.35 * neutralCentroid,
           "a +12 semitone Formant shift did not move the resonant body up");
    expect(loweredCentroid < 0.85 * neutralCentroid,
           "a -12 semitone Formant shift did not move the resonant body down");

    const float neutralRoot = spectralPeak(neutral, sampleRate,
                                           model.rootFrequencyHz());
    const float raisedRoot = spectralPeak(raised, sampleRate,
                                          model.rootFrequencyHz());
    expect(std::abs(raisedRoot / std::max(neutralRoot, 1.0e-6f) - 1.0f) < 0.004f,
           "Formant shifting also moved the played pitch");
}

void testVelocityTouch(const neuramar::NeuralModel& model)
{
    constexpr double sampleRate = 48000.0;
    constexpr int renderSamples = 20000;
    const auto centroidAt = [&](float touch, float velocity, float air)
    {
        auto parameters = cleanCoreParameters();
        parameters.bodyLock = 0.0f;
        parameters.air = air;
        parameters.touch = touch;
        const auto rendered = renderWithParameters(
            model, model.rootMidiNote(), velocity, parameters, renderSamples);
        return harmonicCentroidHz(
            rendered, sampleRate, static_cast<double>(model.rootFrequencyHz()),
            32, 0.22);
    };

    const double neutralSoft = centroidAt(0.0f, 0.25f, 0.0f);
    const double neutralHard = centroidAt(0.0f, 1.00f, 0.0f);
    expect(std::abs(neutralHard / std::max(neutralSoft, 1.0e-9) - 1.0) < 0.01,
           "Touch at zero still made velocity change the harmonic balance");

    const double touchedSoft = centroidAt(0.9f, 0.25f, 0.0f);
    const double touchedHard = centroidAt(0.9f, 1.00f, 0.0f);
    std::cout << "Touch diagnostics: centroid " << touchedSoft << " -> "
              << touchedHard << " Hz across velocity at Touch 0.9 (neutral "
              << neutralSoft << " Hz)\n";
    expect(touchedHard > 1.08 * touchedSoft,
           "Touch did not brighten a harder note");
    expect(touchedSoft < 0.95 * neutralSoft,
           "Touch did not darken a softer note");

    // Everything except the Air layer renders identically between these two
    // passes, so their sample-wise difference is the stochastic layer itself.
    // Subtracting them in the power domain instead is only valid on average:
    // Air here is well under 1% of Core, and the sample correlation between two
    // independent signals over a 0.28 s window is about 1/sqrt(N), which makes
    // the cross term larger than the quantity being estimated. That estimator
    // returned a negative Air power on this fixture.
    const auto airOnlyRms = [&](float touch, float velocity)
    {
        auto parameters = cleanCoreParameters();
        parameters.bodyLock = 0.0f;
        parameters.touch = touch;
        parameters.air = 1.0f;
        const auto withAir = renderWithParameters(
            model, model.rootMidiNote(), velocity, parameters, renderSamples);
        parameters.air = 0.0f;
        const auto withoutAir = renderWithParameters(
            model, model.rootMidiNote(), velocity, parameters, renderSamples);
        std::vector<float> airOnly(withAir.size(), 0.0f);
        for (std::size_t index = 0; index < airOnly.size(); ++index)
            airOnly[index] = withAir[index] - withoutAir[index];
        // Remove the plain velocity gain so only the timbral change is left.
        const double gain = velocity
            * (0.72 + 0.28 * std::sqrt(static_cast<double>(velocity)));
        return segmentRms(airOnly, sampleRate, 0.10, 0.38)
            / std::max(gain, 1.0e-6);
    };
    const double softAir = airOnlyRms(0.9f, 0.25f);
    const double hardAir = airOnlyRms(0.9f, 1.00f);
    std::cout << "Touch diagnostics: level-compensated Air " << softAir
              << " -> " << hardAir << " across velocity\n";
    expect(softAir > 1.0e-7,
           "the Touch fixture produced no measurable Air layer");
    expect(hardAir > 1.20 * softAir,
           "Touch did not open the Air layer for a harder note");
}

void testStretchFormantTouchSafety(const neuramar::NeuralModel& model)
{
    constexpr float quietNan = std::numeric_limits<float>::quiet_NaN();
    constexpr float infinity = std::numeric_limits<float>::infinity();
    const std::array<std::array<float, 3>, 6> hostile {{
        { quietNan, quietNan, quietNan },
        { infinity, -infinity, infinity },
        { -5.0f, 900.0f, -3.0f },
        { 2.0f, 24.0f, 1.0f },
        { 2.0f, -24.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f }
    }};

    for (const auto& values : hostile)
    {
        auto parameters = cleanCoreParameters();
        parameters.air = 0.7f;
        parameters.bone = 0.7f;
        parameters.stretch = values[0];
        parameters.formantShiftSemitones = values[1];
        parameters.touch = values[2];
        parameters.outputGain = 1.0f;

        neuramar::NeuramarEngine engine;
        engine.prepare(44100.0, 64);
        engine.setModel(&model);
        engine.setParameters(parameters);
        std::vector<float> left(8192, 0.0f);
        std::vector<float> right(8192, 0.0f);
        engine.noteOn(model.rootMidiNote() + 30, 1.0f);
        engine.noteOn(model.rootMidiNote() - 26, 0.2f);
        for (int offset = 0; offset < 8192; offset += 64)
            engine.process(left.data() + offset, right.data() + offset, 64);

        bool finite = true;
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            finite = finite && std::isfinite(left[index])
                && std::isfinite(right[index])
                && std::abs(left[index]) <= 8.0f
                && std::abs(right[index]) <= 8.0f;
        }
        expect(finite,
               "hostile Stretch/Formant/Touch values produced non-finite or "
               "unbounded audio");

        // A retired voice must leave exactly zero behind: no subnormal filter
        // or oscillator tail is allowed to keep trickling into the output.
        engine.allNotesOff();
        for (int block = 0; block < 400 && engine.getActiveVoiceCount() != 0;
             ++block)
            engine.process(left.data(), right.data(), 64);
        expect(engine.getActiveVoiceCount() == 0,
               "voices did not retire under hostile parameter values");
        std::fill(left.begin(), left.begin() + 64, 1.0f);
        std::fill(right.begin(), right.begin() + 64, 1.0f);
        engine.process(left.data(), right.data(), 64);
        bool exactlySilent = true;
        for (int index = 0; index < 64; ++index)
            exactlySilent = exactlySilent && left[index] == 0.0f
                && right[index] == 0.0f;
        expect(exactlySilent,
               "the render path left a denormal or residual tail after every "
               "voice retired");
    }
}

void testLegacyModelStretchCompatibility(const neuramar::NeuralModel& model)
{
    std::string error;
    const auto version2Bytes = makeVersion2ModelBytes(model);
    auto version2 = neuramar::NeuralModel::deserialize(version2Bytes, &error);
    expect(version2 != nullptr,
           "version-2 memory failed to load after the v4 extension: " + error);

    const auto version3Bytes = makeLinearResidualModelBytes(model, 0.5f, 3);
    error.clear();
    auto version3 = neuramar::NeuralModel::deserialize(version3Bytes, &error);
    expect(version3 != nullptr,
           "version-3 memory failed to load after the v4 extension: " + error);
    expect(version2 == nullptr || version2->inharmonicity() == 0.0f,
           "a version-2 memory reported a non-zero stiff-string coefficient");
    expect(version3 == nullptr || version3->inharmonicity() == 0.0f,
           "a version-3 memory reported a non-zero stiff-string coefficient");

    // A stored v3 memory must render bit-identically no matter where Stretch
    // sits, because it carries no coefficient for Stretch to scale.
    if (version3 != nullptr)
    {
        auto parameters = cleanCoreParameters();
        parameters.stretch = 0.0f;
        const auto flat = renderWithParameters(
            *version3, version3->rootMidiNote(), 0.8f, parameters, 6000);
        parameters.stretch = 2.0f;
        const auto stretched = renderWithParameters(
            *version3, version3->rootMidiNote(), 0.8f, parameters, 6000);
        float maximumDifference = 0.0f;
        for (std::size_t index = 0; index < flat.size(); ++index)
            maximumDifference = std::max(maximumDifference,
                std::abs(flat[index] - stretched[index]));
        expect(maximumDifference == 0.0f,
               "Stretch changed the rendering of a legacy memory that has no "
               "stiff-string coefficient");
    }

    // Round trip a version-4 payload that does carry a coefficient.
    const auto version4Bytes = makeLinearResidualModelBytes(
        model, 0.5f, 4, 4.5e-4f);
    error.clear();
    auto version4 = neuramar::NeuralModel::deserialize(version4Bytes, &error);
    expect(version4 != nullptr,
           "crafted version-4 memory failed to load: " + error);
    if (version4 != nullptr)
    {
        expect(std::abs(version4->inharmonicity() - 4.5e-4f) < 1.0e-9f,
               "version-4 round trip lost the stiff-string coefficient");
        // Version 5 widened the body arrays, so a v4 payload no longer
        // re-serialises to its own bytes. What must survive is the decoded
        // state: re-reading what the migration wrote has to evaluate
        // identically to the migrated model at every point on its trajectory.
        const auto migratedBytes = version4->serialize();
        expect(readLittleU32(migratedBytes, 4)
                   == neuramar::NeuralModel::currentFormatVersion,
               "a migrated version-4 memory was not rewritten in the current "
               "format");
        error.clear();
        auto reloaded = neuramar::NeuralModel::deserialize(migratedBytes,
                                                           &error);
        expect(reloaded != nullptr,
               "a migrated version-4 payload failed to reload: " + error);
        expect(reloaded == nullptr
                   || reloaded->serialize() == migratedBytes,
               "a migrated version-4 payload did not survive a byte-exact "
               "round trip of its own format");
        if (reloaded != nullptr)
        {
            for (const float time : { 0.0f, 0.13f, 0.37f, 0.61f, 1.0f })
            {
                neuramar::SynthesisFrame first;
                neuramar::SynthesisFrame second;
                version4->evaluate(time, first);
                reloaded->evaluate(time, second);
                expect(first.harmonicAmplitudes == second.harmonicAmplitudes
                           && first.airAmplitudes == second.airAmplitudes
                           && first.boneAmplitudes == second.boneAmplitudes
                           && first.pitchRatio == second.pitchRatio,
                       "migrating a version-4 memory changed its rendering");
            }
        }
    }

    const auto outOfRange = makeLinearResidualModelBytes(
        model, 0.5f, 4, 10.0f * neuramar::NeuralModel::maximumInharmonicity);
    error.clear();
    auto rejected = neuramar::NeuralModel::deserialize(outOfRange, &error);
    expect(rejected == nullptr && !error.empty(),
           "an out-of-range stiff-string coefficient was accepted");

    auto truncatedV4 = makeLinearResidualModelBytes(model, 0.5f, 4, 0.0f);
    truncatedV4.resize(truncatedV4.size() - 2);
    refreshModelHeader(truncatedV4, 4);
    error.clear();
    auto truncatedRejected = neuramar::NeuralModel::deserialize(
        truncatedV4, &error);
    expect(truncatedRejected == nullptr && !error.empty(),
           "a truncated version-4 inharmonicity field was accepted");
}

void testModelVisualisation(const neuramar::NeuralModel& model)
{
    using neuramar::ModelAnatomy;
    expect(std::abs(neuramar::anatomyPositionOf(
               ModelAnatomy::lowestFrequencyHz)) < 1.0e-6f
               && std::abs(neuramar::anatomyPositionOf(
                      ModelAnatomy::highestFrequencyHz) - 1.0f) < 1.0e-6f,
           "the anatomy frequency mapping did not span its declared range");
    for (const float position : { 0.0f, 0.13f, 0.5f, 0.77f, 1.0f })
    {
        const float frequency = neuramar::anatomyFrequencyAt(position);
        expect(std::abs(neuramar::anatomyPositionOf(frequency) - position)
                   < 1.0e-5f,
               "the anatomy frequency mapping did not round trip");
    }
    expect(neuramar::anatomyPositionOf(-1.0f) == 0.0f
               && neuramar::anatomyPositionOf(
                      std::numeric_limits<float>::quiet_NaN()) == 0.0f
               && std::abs(neuramar::anatomyFrequencyAt(
                      std::numeric_limits<float>::quiet_NaN())
                          - ModelAnatomy::lowestFrequencyHz) < 0.01f,
           "the anatomy mapping did not clamp hostile inputs");
    expect(neuramar::anatomySliceAt(-3.0f) == 0
               && neuramar::anatomySliceAt(1.0f)
                      == ModelAnatomy::sliceCount - 1
               && neuramar::anatomySliceAt(
                      std::numeric_limits<float>::quiet_NaN()) == 0,
           "the anatomy slice selector escaped its trajectory bounds");

    expect(neuramar::followMeter(0.0f, 1.0f, 0.5f, 0.1f) == 0.5f
               && std::abs(neuramar::followMeter(1.0f, 0.0f, 0.5f, 0.25f)
                           - 0.75f) < 1.0e-6f
               && neuramar::followMeter(
                      std::numeric_limits<float>::quiet_NaN(), 0.4f,
                      0.5f, 0.5f) == 0.4f
               && neuramar::followMeter(0.5f, 4.0f, 1.0f, 1.0f) == 1.0f,
           "the meter follower did not honour its attack, release, and bounds");

    ModelAnatomy anatomy;
    neuramar::clearModelAnatomy(anatomy);
    expect(!anatomy.valid
               && std::abs(anatomy.binFrequencyHz.front()
                           - ModelAnatomy::lowestFrequencyHz) < 0.01f
               && std::abs(anatomy.binFrequencyHz.back()
                           - ModelAnatomy::highestFrequencyHz) < 1.0f,
           "a cleared anatomy was not in its empty display state");

    neuramar::buildModelAnatomy(model, anatomy);
    expect(anatomy.valid, "a learned memory produced no anatomy");
    expect(anatomy.peakAmplitude > 0.0f
               && std::isfinite(anatomy.peakAmplitude),
           "the anatomy peak amplitude was not usable");
    expect(std::abs(anatomy.rootFrequencyHz - model.rootFrequencyHz())
               < 1.0e-3f,
           "the anatomy did not carry the learned root");

    bool ordered = true;
    bool bounded = true;
    float largest = 0.0f;
    for (std::size_t bin = 1; bin < ModelAnatomy::binCount; ++bin)
        ordered = ordered && anatomy.binFrequencyHz[bin]
            > anatomy.binFrequencyHz[bin - 1];
    for (std::size_t slice = 0; slice < ModelAnatomy::sliceCount; ++slice)
    {
        for (std::size_t bin = 0; bin < ModelAnatomy::binCount; ++bin)
        {
            for (const float value : { anatomy.core[slice][bin],
                                       anatomy.air[slice][bin],
                                       anatomy.bone[slice][bin] })
            {
                bounded = bounded && std::isfinite(value) && value >= 0.0f
                    && value <= 1.0f;
                largest = std::max(largest, value);
            }
        }
        bounded = bounded && anatomy.coreLevel[slice] >= 0.0f
            && anatomy.coreLevel[slice] <= 1.0f
            && anatomy.airLevel[slice] >= 0.0f
            && anatomy.airLevel[slice] <= 1.0f
            && anatomy.boneLevel[slice] >= 0.0f
            && anatomy.boneLevel[slice] <= 1.0f;
    }
    expect(ordered, "the anatomy frequency axis was not strictly increasing");
    expect(bounded, "the anatomy produced values outside its display range");
    expect(largest > 0.98f,
           "the anatomy did not normalise its loudest cell to full height");

    // The fundamental has to appear in the bin that actually contains it.
    neuramar::SynthesisFrame frame;
    model.evaluate(0.35f, frame);
    const auto slice = neuramar::anatomySliceAt(0.35f);
    const float fundamental = model.rootFrequencyHz() * frame.pitchRatio;
    const auto expectedBin = static_cast<std::size_t>(std::lround(
        neuramar::anatomyPositionOf(fundamental)
        * static_cast<float>(ModelAnatomy::binCount - 1)));
    expect(anatomy.core[slice][expectedBin] > 0.0f,
           "the anatomy placed no Core energy at the learned fundamental");
    expect(anatomy.core[slice][0] == 0.0f
               || model.rootFrequencyHz() < 40.0f,
           "the anatomy leaked Core energy far below the fundamental");

    // Generated memories must also be displayable.
    const auto generated = neuramar::NeuralModel::createRandom(0x51df3719ull,
                                                               1.0f);
    expect(generated != nullptr, "randomizer produced no model to display");
    if (generated != nullptr)
    {
        ModelAnatomy generatedAnatomy;
        neuramar::buildModelAnatomy(*generated, generatedAnatomy);
        expect(generatedAnatomy.valid,
               "a generated memory produced no anatomy");
    }
}

// The shipped polyphase resampler must match a direct evaluation of the same
// windowed-sinc kernel, and it exists to be much faster than one. Both halves
// are measured here so the efficiency claim has a reproducible number.
void testResamplerAccuracyAndCost()
{
    constexpr double sourceRate = 48000.0;
    constexpr double destinationRate = 12000.0;
    constexpr std::size_t sourceSize = 240000;
    std::vector<float> source(sourceSize, 0.0f);
    for (std::size_t index = 0; index < sourceSize; ++index)
    {
        const double time = static_cast<double>(index) / sourceRate;
        source[index] = static_cast<float>(
            0.55 * std::sin(2.0 * pi * 220.0 * time)
            + 0.25 * std::sin(2.0 * pi * 1310.0 * time + 0.7)
            + 0.12 * std::sin(2.0 * pi * 4400.0 * time + 1.9));
    }

    // Literal transcription of the pre-1.1 direct-evaluation resampler.
    const auto referenceResample = [&source]()
    {
        constexpr int halfTaps = 24;
        const double sourcePerOutput = sourceRate / destinationRate;
        const double cutoff = 0.94 * std::min(1.0,
                                              destinationRate / sourceRate);
        const auto destinationSize = static_cast<std::size_t>(std::max(1.0,
            std::round(static_cast<double>(source.size())
                       * destinationRate / sourceRate)));
        std::vector<float> destination(destinationSize, 0.0f);
        for (std::size_t output = 0; output < destinationSize; ++output)
        {
            const double centre = static_cast<double>(output) * sourcePerOutput;
            const auto first = static_cast<std::ptrdiff_t>(std::floor(centre))
                - halfTaps + 1;
            double weighted = 0.0;
            double weightSum = 0.0;
            for (int tap = 0; tap < 2 * halfTaps; ++tap)
            {
                const auto input = first + tap;
                if (input < 0
                    || input >= static_cast<std::ptrdiff_t>(source.size()))
                    continue;
                const double distance = centre - static_cast<double>(input);
                const double angle = pi * cutoff * distance;
                const double sinc = std::abs(angle) < 1.0e-12
                    ? 1.0 : std::sin(angle) / angle;
                const double window = 0.5 + 0.5 * std::cos(
                    pi * distance / halfTaps);
                const double weight = cutoff * sinc * window;
                weighted += static_cast<double>(
                    source[static_cast<std::size_t>(input)]) * weight;
                weightSum += weight;
            }
            destination[output] = std::abs(weightSum) > 1.0e-12
                ? static_cast<float>(weighted / weightSum) : 0.0f;
        }
        return destination;
    };

    const auto referenceStart = std::chrono::steady_clock::now();
    const auto reference = referenceResample();
    const double referenceSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - referenceStart).count();

    const auto shippedStart = std::chrono::steady_clock::now();
    const auto shipped = neuramar::SampleLearner::resampleForTests(
        source, sourceRate, destinationRate);
    const double shippedSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - shippedStart).count();

    expect(shipped.size() == reference.size(),
           "the polyphase resampler produced a different output length");
    if (shipped.size() != reference.size())
        return;

    double squaredError = 0.0;
    double squaredSignal = 0.0;
    for (std::size_t index = 0; index < shipped.size(); ++index)
    {
        const double error = static_cast<double>(shipped[index])
            - reference[index];
        squaredError += error * error;
        squaredSignal += static_cast<double>(reference[index])
            * reference[index];
    }
    const double errorDb = 10.0 * std::log10(
        std::max(squaredError, 1.0e-300)
        / std::max(squaredSignal, 1.0e-300));
    const double speedup = referenceSeconds
        / std::max(shippedSeconds, 1.0e-9);
    std::cout << "Resampler diagnostics: error " << errorDb
              << " dB relative to direct evaluation, " << referenceSeconds
              << " s direct vs " << shippedSeconds << " s tabulated ("
              << speedup << "x)\n";
    expect(errorDb < -90.0,
           "the polyphase resampler diverged from direct kernel evaluation ("
               + std::to_string(errorDb) + " dB)");
    expect(speedup > 2.0,
           "the tabulated resampler was not materially faster than direct "
           "kernel evaluation (" + std::to_string(speedup) + "x)");
}

void testHighRegisterRenderThroughput(const neuramar::NeuralModel& model)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(48000.0, 512);
    engine.setModel(&model);
    auto parameters = cleanCoreParameters();
    parameters.air = 0.6f;
    parameters.bone = 0.6f;
    parameters.outputGain = 0.25f;
    engine.setParameters(parameters);

    // Two octaves above the root: most of the learned bank sits above Nyquist
    // and is skipped entirely instead of being phase-advanced and faded.
    const int firstNote = std::clamp(model.rootMidiNote() + 24, 0, 119);
    for (int note = firstNote; note < firstNote + 8; ++note)
        engine.noteOn(note, 0.75f);

    std::vector<float> left(48000, 0.0f);
    std::vector<float> right(48000, 0.0f);
    const auto start = std::chrono::steady_clock::now();
    for (int offset = 0; offset < static_cast<int>(left.size()); offset += 128)
    {
        const int count = std::min(128, static_cast<int>(left.size()) - offset);
        engine.process(left.data() + offset, right.data() + offset, count);
    }
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    const double realtimeFactor = 1.0 / std::max(elapsed, 1.0e-9);
    std::cout << "Performance diagnostics: eight-voice high-register 1 s "
              << "render " << elapsed << " s (" << realtimeFactor
              << "x realtime)\n";
    constexpr double throughputFloor = NEURAMAR_SANITIZED_TEST_BUILD
        ? 1.0 : 8.0;
    expect(realtimeFactor > throughputFloor,
           "eight-voice high-register rendering fell below its throughput "
           "floor (" + std::to_string(realtimeFactor) + "x realtime)");
    expect(rms(left) > 1.0e-5f,
           "high-register throughput fixture rendered silence");
}

[[nodiscard]] std::vector<float> renderNoteAtVelocity(
    neuramar::NeuramarEngine& engine, int midiNote, float velocity,
    int sampleCount)
{
    std::vector<float> left(static_cast<std::size_t>(sampleCount), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(sampleCount), 0.0f);
    engine.noteOn(midiNote, velocity);
    constexpr int blockSize = 128;
    for (int offset = 0; offset < sampleCount; offset += blockSize)
    {
        const int count = std::min(blockSize, sampleCount - offset);
        engine.process(left.data() + offset, right.data() + offset, count);
    }
    engine.allSoundOff();
    for (std::size_t index = 0; index < left.size(); ++index)
        left[index] = 0.5f * (left[index] + right[index]);
    return left;
}

// The instrument has to hold a usable level across the whole compass. Measuring
// the register reference against the model's full bank instead of against the
// partials a note actually renders saturated the compensator at its ceiling
// around root+17 semitones: the upper keyboard then faded out by 3.9 dB at the
// default Body Lock and 14.4 dB at full Body Lock.
void testKeyboardLevelFlatness()
{
    constexpr double sampleRate = 48000.0;
    const auto model = neuramar::NeuralModel::createRandom(1234, 0.0f);
    expect(model != nullptr, "keyboard flatness fixture could not be created");
    if (!model)
        return;

    const auto noteLevel = [&model](int midiNote, float bodyLock)
    {
        neuramar::NeuramarEngine engine;
        engine.prepare(sampleRate, 128);
        engine.setModel(model.get());
        neuramar::EngineParameters parameters;
        parameters.imprint = 1.0f;
        parameters.bodyLock = bodyLock;
        parameters.air = 0.0f;
        parameters.bone = 0.0f;
        parameters.brightness = 0.5f;
        parameters.orbit = 0.0f;
        parameters.mutation = 0.0f;
        parameters.attackSeconds = 0.0f;
        parameters.spread = 0.0f;
        parameters.touch = 0.0f;
        parameters.registerTilt = 0.0f;
        parameters.outputGain = 1.0f;
        engine.setParameters(parameters);
        const auto audio = renderNote(engine, midiNote,
                                      static_cast<int>(0.32 * sampleRate));
        // Skip the attack so the measurement describes the sustained body.
        return rms(audio, audio.size() / 4);
    };

    for (const float bodyLock : { 0.0f, 0.52f, 1.0f })
    {
        const float reference = noteLevel(model->rootMidiNote(), bodyLock);
        expect(reference > 1.0e-5f, "keyboard flatness reference was silent");
        if (!(reference > 1.0e-5f))
            continue;

        float lowest = 1.0e9f;
        float highest = -1.0e9f;
        int lowestNote = 0;
        int highestNote = 0;
        for (int midiNote = 12; midiNote <= 108; midiNote += 12)
        {
            const float decibels = 20.0f * std::log10(
                std::max(noteLevel(midiNote, bodyLock), 1.0e-9f) / reference);
            if (decibels < lowest) { lowest = decibels; lowestNote = midiNote; }
            if (decibels > highest) { highest = decibels; highestNote = midiNote; }
        }
        const float span = highest - lowest;
        std::cout << "Keyboard flatness diagnostics: Body Lock " << bodyLock
                  << " span " << span << " dB (min " << lowest << " dB at MIDI "
                  << lowestNote << ", max " << highest << " dB at MIDI "
                  << highestNote << ")\n";
        // Full Body Lock deliberately loses upper partials past the observed
        // envelope, so it is allowed slightly more drift than the other modes.
        const float limit = bodyLock >= 1.0f ? 3.0f : 2.0f;
        expect(span < limit,
               "MIDI 12-108 level drifted by " + std::to_string(span)
                   + " dB at Body Lock " + std::to_string(bodyLock));
    }
}

// Register is key tracking: a note played above the learned root is darkened
// and one played below it is opened up, without changing its level.
void testRegisterTiltTracksTheKeyboard(const neuramar::NeuralModel& model)
{
    constexpr double sampleRate = 48000.0;
    constexpr double analysisTime = 0.22;
    const int highNote = std::min(120, model.rootMidiNote() + 12);
    const auto measure = [&model, highNote](float registerTilt)
    {
        neuramar::NeuramarEngine engine;
        engine.prepare(sampleRate, 128);
        engine.setModel(&model);
        neuramar::EngineParameters parameters;
        parameters.imprint = 1.0f;
        parameters.bodyLock = 0.0f;
        parameters.air = 0.0f;
        parameters.bone = 0.0f;
        parameters.brightness = 0.5f;
        parameters.orbit = 0.0f;
        parameters.mutation = 0.0f;
        parameters.attackSeconds = 0.0f;
        parameters.spread = 0.0f;
        parameters.touch = 0.0f;
        parameters.registerTilt = registerTilt;
        parameters.outputGain = 1.0f;
        engine.setParameters(parameters);
        const auto audio = renderNoteAtVelocity(
            engine, highNote, 0.8f, static_cast<int>(0.40 * sampleRate));
        neuramar::SynthesisFrame frame;
        model.evaluate(static_cast<float>(analysisTime / model.durationSeconds()),
                       frame);
        const double fundamental = model.rootFrequencyHz() * frame.pitchRatio
            * std::exp2((highNote - model.rootMidiNote()) / 12.0);
        const float first = windowedSinusoidAmplitude(
            audio, sampleRate, analysisTime, fundamental);
        const float upper = windowedSinusoidAmplitude(
            audio, sampleRate, analysisTime, 6.0 * fundamental);
        return std::make_pair(upper / std::max(first, 1.0e-9f),
                              rms(audio, audio.size() / 4));
    };

    const auto neutral = measure(0.0f);
    const auto tracked = measure(1.0f);
    const double timbreRatio = tracked.first / std::max(neutral.first, 1.0e-9f);
    const double levelRatio = tracked.second / std::max(neutral.second, 1.0e-9f);
    std::cout << "Register diagnostics: harmonic-6 ratio " << timbreRatio
              << ", level ratio " << levelRatio << '\n';
    expect(timbreRatio < 0.85,
           "Register did not darken a note an octave above the root (ratio "
               + std::to_string(timbreRatio) + ")");
    expect(levelRatio > 0.75 && levelRatio < 1.33,
           "Register changed level rather than only timbre (ratio "
               + std::to_string(levelRatio) + ")");
}

// Output is the one control applied straight to the summed signal, so it needs
// its own smoother; without one, a host that writes it once per block steps the
// level at every block boundary.
void testOutputGainIsSmoothed(const neuramar::NeuralModel& model)
{
    constexpr double sampleRate = 48000.0;
    neuramar::NeuramarEngine engine;
    engine.prepare(sampleRate, 128);
    engine.setModel(&model);
    neuramar::EngineParameters parameters;
    parameters.air = 0.0f;
    parameters.bone = 0.0f;
    parameters.mutation = 0.0f;
    parameters.attackSeconds = 0.0f;
    parameters.spread = 0.0f;
    parameters.outputGain = 1.0f;
    engine.setParameters(parameters);

    constexpr int segment = 2048;
    std::vector<float> left(2 * segment, 0.0f);
    std::vector<float> right(2 * segment, 0.0f);
    engine.noteOn(model.rootMidiNote(), 0.8f);
    engine.process(left.data(), right.data(), segment);
    parameters.outputGain = 0.25f;   // a 12 dB automation jump
    engine.setParameters(parameters);
    engine.process(left.data() + segment, right.data() + segment, segment);

    float neighbouringStep = 0.0f;
    for (int index = segment - 512; index + 1 < segment; ++index)
        neighbouringStep = std::max(neighbouringStep,
            std::abs(left[static_cast<std::size_t>(index + 1)]
                     - left[static_cast<std::size_t>(index)]));
    const float boundaryStep = std::abs(
        left[static_cast<std::size_t>(segment)]
        - left[static_cast<std::size_t>(segment - 1)]);
    std::cout << "Output smoothing diagnostics: boundary step " << boundaryStep
              << " vs neighbouring " << neighbouringStep << '\n';
    expect(boundaryStep <= 2.0f * neighbouringStep + 1.0e-5f,
           "a mid-note Output change stepped at the block boundary (step "
               + std::to_string(boundaryStep) + ")");
}

// The Air layer carries a decorrelated second realisation for stereo width. It
// is added to the left and subtracted from the right, so it must vanish from a
// mono sum: a wider setting must never change what a mono listener hears.
void testStereoAirIsMonoSafe(const neuramar::NeuralModel& model)
{
    constexpr double sampleRate = 48000.0;
    constexpr int sampleCount = 6000;
    const auto render = [&model](float spread,
                                 std::vector<float>& left,
                                 std::vector<float>& right)
    {
        neuramar::NeuramarEngine engine;
        engine.prepare(sampleRate, 128);
        engine.setModel(&model);
        neuramar::EngineParameters parameters;
        parameters.air = 1.0f;
        parameters.bone = 0.0f;
        parameters.mutation = 0.0f;
        parameters.attackSeconds = 0.0f;
        parameters.spread = spread;
        parameters.outputGain = 1.0f;
        engine.setParameters(parameters);
        left.assign(sampleCount, 0.0f);
        right.assign(sampleCount, 0.0f);
        engine.noteOn(model.rootMidiNote(), 0.82f);
        for (int offset = 0; offset < sampleCount; offset += 128)
            engine.process(left.data() + offset, right.data() + offset,
                           std::min(128, sampleCount - offset));
    };

    std::vector<float> centredLeft;
    std::vector<float> centredRight;
    std::vector<float> wideLeft;
    std::vector<float> wideRight;
    render(0.0f, centredLeft, centredRight);
    render(1.0f, wideLeft, wideRight);

    float monoDifference = 0.0f;
    float sideEnergy = 0.0f;
    for (std::size_t index = 0; index < wideLeft.size(); ++index)
    {
        monoDifference = std::max(monoDifference, std::abs(
            (wideLeft[index] + wideRight[index])
            - (centredLeft[index] + centredRight[index])));
        sideEnergy = std::max(sideEnergy,
            std::abs(wideLeft[index] - wideRight[index]));
    }
    std::cout << "Stereo Air diagnostics: mono difference " << monoDifference
              << ", side level " << sideEnergy << '\n';
    expect(sideEnergy > 1.0e-3f,
           "maximum Horizon produced no stereo width in the Air layer");
    expect(monoDifference < 1.0e-5f,
           "Air stereo width changed the mono sum (difference "
               + std::to_string(monoDifference) + ")");
}

// Orbit revisits the learned loop for as long as a note is held, so the loop
// has to sit past the attack rather than inside it.
void testLoopRegionAvoidsTheAttack(const LearnedFixture& fixture)
{
    if (!fixture.first.model)
        return;
    const auto& metadata = fixture.first.model->metadata();
    const float duration = std::max(metadata.durationSeconds, 1.0e-6f);
    const float start = metadata.loopStartSeconds / duration;
    const float end = metadata.loopEndSeconds / duration;
    std::cout << "Loop region diagnostics: " << start << " to " << end
              << " of the learned duration\n";
    expect(start > 0.18f,
           "the learned loop started inside the attack (normalised "
               + std::to_string(start) + ")");
    expect(end - start > 0.12f,
           "the learned loop was too short to be a stable region (span "
               + std::to_string(end - start) + ")");
}

void testRoughPerformance(const neuramar::NeuralModel& model)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(48000.0, 512);
    engine.setModel(&model);
    neuramar::EngineParameters parameters;
    parameters.air = 0.6f;
    parameters.bone = 0.6f;
    engine.setParameters(parameters);
    const int firstNote = std::clamp(model.rootMidiNote() - 24, 0, 119);
    for (int note = firstNote; note < firstNote + 8; ++note)
        engine.noteOn(note, 0.75f);

    std::vector<float> left(24000, 0.0f);
    std::vector<float> right(24000, 0.0f);
    const auto start = std::chrono::steady_clock::now();
    for (int offset = 0; offset < static_cast<int>(left.size()); offset += 128)
    {
        const int count = std::min(128, static_cast<int>(left.size()) - offset);
        engine.process(left.data() + offset, right.data() + offset, count);
    }
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "Performance diagnostics: eight-voice wide-register 0.5 s render "
              << elapsed << " s (" << 0.5 / std::max(elapsed, 1.0e-9)
              << "x realtime)\n";
    constexpr double performanceLimit = NEURAMAR_SANITIZED_TEST_BUILD
        ? 10.0 : 1.0;
    expect(elapsed < performanceLimit,
           "eight-voice wide-register render exceeded its coarse wall-time guard");
    expect(rms(left) > 0.001f, "performance fixture rendered silence");
}

// Core-only render of one note at an arbitrary host rate, downmixed to mono.
[[nodiscard]] std::vector<float> renderCoreAtRate(
    const neuramar::NeuralModel& model, int midiNote, double sampleRate,
    double seconds)
{
    neuramar::NeuramarEngine engine;
    engine.prepare(sampleRate, 128);
    engine.setModel(&model);
    auto parameters = cleanCoreParameters();
    parameters.bodyLock = 0.0f;
    parameters.outputGain = 1.0f;
    engine.setParameters(parameters);

    const auto sampleCount = static_cast<int>(sampleRate * seconds);
    std::vector<float> left(static_cast<std::size_t>(sampleCount), 0.0f);
    std::vector<float> right(static_cast<std::size_t>(sampleCount), 0.0f);
    engine.noteOn(midiNote, 0.8f);
    for (int offset = 0; offset < sampleCount; offset += 128)
    {
        const int count = std::min(128, sampleCount - offset);
        engine.process(left.data() + offset, right.data() + offset, count);
    }
    for (std::size_t index = 0; index < left.size(); ++index)
        left[index] = 0.5f * (left[index] + right[index]);
    return left;
}

// Hann-windowed single-frequency projection, returned as an amplitude.
[[nodiscard]] double narrowbandAmplitude(const std::vector<float>& signal,
                                         std::size_t first, std::size_t count,
                                         double frequencyHz, double sampleRate)
{
    double real = 0.0;
    double imaginary = 0.0;
    double windowSum = 0.0;
    const auto span = static_cast<double>(count - 1);
    for (std::size_t index = 0; index < count; ++index)
    {
        const double window = 0.5 - 0.5 * std::cos(
            2.0 * pi * static_cast<double>(index) / span);
        const double angle = 2.0 * pi * frequencyHz
            * static_cast<double>(first + index) / sampleRate;
        const double value = signal[first + index] * window;
        real += value * std::cos(angle);
        imaginary -= value * std::sin(angle);
        windowSum += window;
    }
    return 2.0 * std::hypot(real, imaginary) / std::max(windowSum, 1.0);
}

[[nodiscard]] double peakedAmplitude(const std::vector<float>& signal,
                                     std::size_t first, std::size_t count,
                                     double frequencyHz, double sampleRate)
{
    double best = 0.0;
    for (int step = -10; step <= 10; ++step)
    {
        const double probe = frequencyHz * (1.0 + 0.0015 * step);
        best = std::max(best, narrowbandAmplitude(signal, first, count, probe,
                                                  sampleRate));
    }
    return best;
}

// Nothing may live below the played fundamental: the partial series starts
// there, the anti-alias taper deletes everything that would fold back, and the
// oscillator's sine approximation is the only other candidate for a spurious
// line. This is the guard that keeps a cheaper approximation, a broken taper,
// or an unbandlimited shortcut from shipping.
void testCoreSpurFloor()
{
    const auto model = neuramar::NeuralModel::createRandom(
        0x9e3779b97f4a7c15ull, 0.6f);
    expect(model != nullptr, "spur-floor fixture could not be created");
    if (!model)
        return;

    double worstRatioDb = -300.0;
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        for (const int offset : { 24, 36, 45 })
        {
            const int note = std::clamp(model->rootMidiNote() + offset, 0, 127);
            const auto rendered = renderCoreAtRate(*model, note, sampleRate,
                                                   0.42);
            const auto first = static_cast<std::size_t>(sampleRate * 0.30);
            const auto count = static_cast<std::size_t>(sampleRate * 0.10);
            const double fundamentalHz = static_cast<double>(
                model->rootFrequencyHz())
                * std::exp2((note - model->rootMidiNote()) / 12.0);
            // Stop well below the lowest pitch the learned contour can bend
            // the fundamental to, so a valid partial can never be counted.
            const double ceilingHz = 0.60 * fundamentalHz;
            // Half the analysis resolution, so no narrow line can hide
            // between two probes.
            const double probeStep = 0.5 * sampleRate
                / static_cast<double>(count);
            double spur = 0.0;
            for (double probe = 40.0; probe < ceilingHz; probe += probeStep)
                spur = std::max(spur, narrowbandAmplitude(
                    rendered, first, count, probe, sampleRate));

            const std::vector<float> window(
                rendered.begin() + static_cast<std::ptrdiff_t>(first),
                rendered.begin() + static_cast<std::ptrdiff_t>(first + count));
            const double reference = rms(window);
            expect(reference > 1.0e-4,
                   "spur-floor fixture rendered silence at MIDI "
                       + std::to_string(note));
            const double ratioDb = 20.0 * std::log10(
                std::max(spur, 1.0e-30) / std::max(reference, 1.0e-30));
            worstRatioDb = std::max(worstRatioDb, ratioDb);
        }
    }

    std::cout << "Spur diagnostics: worst sub-fundamental line "
              << worstRatioDb << " dB relative to the rendered signal\n";
    expect(worstRatioDb < -85.0,
           "the Core oscillator bank produced a sub-fundamental spur above its "
           "regression floor (" + std::to_string(worstRatioDb) + " dB)");
}

// The instrument must sound the same whatever the host rate is. Every partial
// that fits below the strictest anti-alias limit of the compared rates is
// measured against the 48 kHz render.
void testRenderIsSampleRateInvariant()
{
    const auto model = neuramar::NeuralModel::createRandom(
        0x9e3779b97f4a7c15ull, 0.6f);
    expect(model != nullptr, "sample-rate invariance fixture failed");
    if (!model)
        return;

    constexpr double rates[] { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    constexpr std::size_t referenceRate = 1;
    double worstDeviationDb = 0.0;
    for (const int offset : { 0, 12, 24 })
    {
        const int note = std::clamp(model->rootMidiNote() + offset, 0, 127);
        const double fundamentalHz = static_cast<double>(
            model->rootFrequencyHz())
            * std::exp2((note - model->rootMidiNote()) / 12.0);
        std::vector<std::vector<double>> amplitudes;
        for (const double sampleRate : rates)
        {
            const auto rendered = renderCoreAtRate(*model, note, sampleRate,
                                                   0.32);
            const auto first = static_cast<std::size_t>(sampleRate * 0.20);
            const auto count = static_cast<std::size_t>(sampleRate * 0.10);
            std::vector<double> partials;
            for (int partial = 1; partial <= 12; ++partial)
            {
                // 15 kHz is below the anti-alias taper of every compared rate,
                // so a partial there exists in all five renders.
                const double frequencyHz = fundamentalHz * partial;
                partials.push_back(frequencyHz < 15000.0
                    ? peakedAmplitude(rendered, first, count, frequencyHz,
                                      sampleRate)
                    : 0.0);
            }
            amplitudes.push_back(std::move(partials));
        }

        for (std::size_t rate = 0; rate < std::size(rates); ++rate)
        {
            for (std::size_t partial = 0;
                 partial < amplitudes[rate].size(); ++partial)
            {
                const double reference = amplitudes[referenceRate][partial];
                if (reference < 1.0e-3)
                    continue;
                worstDeviationDb = std::max(worstDeviationDb, std::abs(
                    20.0 * std::log10(std::max(amplitudes[rate][partial],
                                               1.0e-12) / reference)));
            }
        }
    }

    std::cout << "Sample-rate diagnostics: worst partial deviation across "
              << "44.1/48/88.2/96/192 kHz " << worstDeviationDb << " dB\n";
    expect(worstDeviationDb < 0.35,
           "the render is not sample-rate invariant (worst partial deviation "
               + std::to_string(worstDeviationDb) + " dB)");
}

// Air is stochastic and Bone is modal, so neither can be compared partial by
// partial. Subtracting an otherwise identical Core-only render isolates the
// two layers exactly - the Core path never reads the Air or Bone controls, so
// everything else cancels sample for sample - and their total power is then a
// low-variance quantity that must not move with the host rate.
//
// Their edge tapers used to be anchored to the host Nyquist, so the same
// memory faded its top band out at 44.1 kHz, kept most of it at 48 kHz and
// rendered it in full - often entirely above 20 kHz - at 96 kHz. Both tapers
// are now anchored to the audible band as well.
//
// This is a guard on the two layers as a whole, not a discriminator for the
// taper anchor: on this fixture the anchor is worth at most a few tenths of a
// dB of total layer power, because the top Air band carries little of it. What
// the anchor actually changes is pinned deterministically by
// testBoneCeilingIsAnchoredToTheAudibleBand below.
void testBodyLayersAreSampleRateInvariant()
{
    const auto model = neuramar::NeuralModel::createRandom(
        0x5851f42d4c957f2dull, 0.7f);
    expect(model != nullptr, "body-layer invariance fixture failed");
    if (!model)
        return;

    constexpr double rates[] { 44100.0, 48000.0, 96000.0, 192000.0 };
    double referencePower = 0.0;
    double worstDeviationDb = 0.0;
    // An octave above the root with Body Lock open pushes the memory's top Air
    // bands into the region where the host-anchored taper used to disagree.
    const int note = std::clamp(model->rootMidiNote() + 12, 0, 127);
    for (const double sampleRate : rates)
    {
        const auto render = [&](bool bodyLayers)
        {
            neuramar::NeuramarEngine engine;
            engine.prepare(sampleRate, 128);
            engine.setModel(model.get());
            auto parameters = cleanCoreParameters();
            parameters.bodyLock = 0.0f;
            parameters.air = bodyLayers ? 1.0f : 0.0f;
            parameters.bone = bodyLayers ? 1.0f : 0.0f;
            parameters.outputGain = 1.0f;
            engine.setParameters(parameters);
            return renderNoteAtVelocity(engine, note, 0.8f,
                                        static_cast<int>(sampleRate * 0.30));
        };
        const auto withLayers = render(true);
        const auto coreOnly = render(false);

        const auto first = static_cast<std::size_t>(sampleRate * 0.12);
        const auto last = static_cast<std::size_t>(sampleRate * 0.29);
        double power = 0.0;
        for (std::size_t index = first; index < last; ++index)
        {
            const double layers = static_cast<double>(withLayers[index])
                - coreOnly[index];
            power += layers * layers;
        }
        power /= static_cast<double>(last - first);
        expect(power > 1.0e-10, "body-layer fixture rendered no Air or Bone");
        if (referencePower <= 0.0)
            referencePower = power;
        worstDeviationDb = std::max(worstDeviationDb, std::abs(
            10.0 * std::log10(power / std::max(referencePower, 1.0e-30))));
    }

    std::cout << "Body-layer diagnostics: worst Air+Bone power deviation "
              << "across 44.1/48/96/192 kHz " << worstDeviationDb << " dB\n";
    expect(worstDeviationDb < 1.0,
           "the Air and Bone layers do not hold their level across host sample "
           "rates (worst deviation " + std::to_string(worstDeviationDb)
               + " dB)");
}

// The Bone layer is modal, so unlike Air it can be probed one line at a time,
// which makes it the deterministic gate on the audible-band ceiling that the
// total-power test above cannot be. Three octaves up with Body Lock open puts
// this memory's top mode at about 22.7 kHz: inaudible at every host rate, but
// well inside 0.49 * 96 kHz, part-way down the fade below 0.49 * 48 kHz and
// above 0.49 * 44.1 kHz altogether, so the host-anchored taper this replaces
// rendered it in full at 96 kHz, some 5 dB down at 48 kHz and not at all at
// 44.1 kHz. The audible mode below it is measured in
// the same renders, both to prove the fixture is really sounding and because
// it must not move with the host rate either.
void testBoneCeilingIsAnchoredToTheAudibleBand()
{
    const auto model = neuramar::NeuralModel::createRandom(
        0x5851f42d4c957f2dull, 0.7f);
    expect(model != nullptr, "bone-ceiling fixture failed");
    if (!model)
        return;

    const auto ratios = model->boneFrequencyRatios();
    const auto reliabilities = model->boneModeReliabilities();
    constexpr int semitones = 36;
    const int note = std::clamp(model->rootMidiNote() + semitones, 0, 127);
    const double spectralScale = std::exp2(
        static_cast<double>(note - model->rootMidiNote()) / 12.0);
    const auto modeFrequency = [&](std::size_t mode)
    {
        return static_cast<double>(model->rootFrequencyHz()) * ratios[mode]
            * spectralScale;
    };

    // Pick the pair that straddles the ceiling at this transposition rather
    // than assuming it is the top pair: the memory carries twelve modal slots
    // and the highest of them lands far above the region under test.
    std::size_t ultrasonicMode = 0;
    std::size_t audibleMode = 0;
    for (std::size_t mode = 1; mode < ratios.size(); ++mode)
    {
        if (reliabilities[mode] <= 0.0f)
            continue;
        const double frequency = modeFrequency(mode);
        if (!(frequency > 20000.0 && frequency < 23500.0))
            continue;
        for (std::size_t below = mode; below-- > 0;)
        {
            if (reliabilities[below] > 0.0f && modeFrequency(below) < 19000.0)
            {
                ultrasonicMode = mode;
                audibleMode = below;
                break;
            }
        }
        if (ultrasonicMode != 0)
            break;
    }
    expect(ultrasonicMode > 0, "bone-ceiling fixture has no usable mode pair");
    if (ultrasonicMode == 0)
        return;

    const double ultrasonicHz = modeFrequency(ultrasonicMode);
    const double audibleHz = modeFrequency(audibleMode);
    expect(ultrasonicHz > 20000.0 && ultrasonicHz < 23500.0,
           "bone-ceiling fixture no longer straddles the audible ceiling ("
               + std::to_string(ultrasonicHz) + " Hz)");
    expect(audibleHz < 19000.0,
           "bone-ceiling reference mode is not inside the audible band");

    // 44.1 kHz cannot represent the ultrasonic mode at all, so the comparison
    // runs at the two rates whose Nyquist sits above it.
    constexpr double rates[] { 48000.0, 96000.0 };
    double worstUltrasonicDb = -300.0;
    double worstAudibleDeviationDb = 0.0;
    double audibleReference = 0.0;
    for (const double sampleRate : rates)
    {
        const auto render = [&](bool boneLayer)
        {
            neuramar::NeuramarEngine engine;
            engine.prepare(sampleRate, 128);
            engine.setModel(model.get());
            auto parameters = cleanCoreParameters();
            parameters.bodyLock = 0.0f;
            parameters.bone = boneLayer ? 1.0f : 0.0f;
            parameters.outputGain = 1.0f;
            engine.setParameters(parameters);
            return renderNoteAtVelocity(engine, note, 0.8f,
                                        static_cast<int>(sampleRate * 0.30));
        };
        const auto withBone = render(true);
        const auto coreOnly = render(false);
        std::vector<float> boneOnly(withBone.size(), 0.0f);
        for (std::size_t index = 0; index < boneOnly.size(); ++index)
            boneOnly[index] = withBone[index] - coreOnly[index];

        const auto first = static_cast<std::size_t>(sampleRate * 0.12);
        const auto count = static_cast<std::size_t>(sampleRate * 0.10);
        const double audible = peakedAmplitude(boneOnly, first, count,
                                               audibleHz, sampleRate);
        const double ultrasonic = peakedAmplitude(boneOnly, first, count,
                                                  ultrasonicHz, sampleRate);
        expect(audible > 1.0e-4,
               "bone-ceiling fixture rendered no audible mode");
        worstUltrasonicDb = std::max(worstUltrasonicDb, 20.0 * std::log10(
            std::max(ultrasonic, 1.0e-30) / std::max(audible, 1.0e-30)));
        if (audibleReference <= 0.0)
            audibleReference = audible;
        worstAudibleDeviationDb = std::max(worstAudibleDeviationDb, std::abs(
            20.0 * std::log10(audible / std::max(audibleReference, 1.0e-30))));
    }

    std::cout << "Bone-ceiling diagnostics: mode at "
              << static_cast<int>(ultrasonicHz) << " Hz renders at "
              << worstUltrasonicDb << " dB relative to the audible mode at "
              << static_cast<int>(audibleHz) << " Hz, whose own level moves "
              << worstAudibleDeviationDb << " dB between 48 and 96 kHz\n";
    expect(worstUltrasonicDb < -60.0,
           "a Bone mode above the audible ceiling still renders (level "
               + std::to_string(worstUltrasonicDb)
               + " dB relative to the audible mode)");
    expect(worstAudibleDeviationDb < 0.5,
           "an audible Bone mode changed level with the host sample rate ("
               + std::to_string(worstAudibleDeviationDb) + " dB)");
}

// A voice slot stolen again and again inside one 3 ms fade window is the only
// path that carries a still-running tail into a new frozen sample. Nothing else
// in the suite reaches it - it needs more than one steal of the same slot per
// 144 samples at 48 kHz, far denser than any voice-steal, finite-output or
// determinism fixture here - so it is exercised directly. The carry must stay a
// bounded hand-off: an earlier version re-armed a full fade window around the
// carried remainder, which made the tail an integrator whose level grew with
// the length of the burst until the mix sat on the pathological-state guard.
// Both properties are pinned: the peak stays far below that guard, and it does
// not grow when the same burst is made sixteen times longer.
void testDenseVoiceStealTailStaysBounded(const neuramar::NeuralModel& model)
{
    constexpr double sampleRate = 48000.0;
    const auto burstPeak = [&model](int noteOnCount, int spacingSamples)
    {
        neuramar::NeuramarEngine engine;
        engine.prepare(sampleRate, 64);
        engine.setModel(&model);
        auto parameters = cleanCoreParameters();
        parameters.outputGain = 0.08f;
        engine.setParameters(parameters);

        float left = 0.0f;
        float right = 0.0f;
        double peak = 0.0;
        const auto advance = [&](int samples)
        {
            for (int sample = 0; sample < samples; ++sample)
            {
                // One sample at a time, exactly as the processor splits a block
                // on a sample-accurate note-on.
                engine.process(&left, &right, 1);
                peak = std::max(peak, std::abs(static_cast<double>(left)));
                peak = std::max(peak, std::abs(static_cast<double>(right)));
            }
        };
        for (int index = 0; index < noteOnCount; ++index)
        {
            engine.noteOn(48 + index % 25, 0.9f);
            advance(spacingSamples);
        }
        // Let every fade tail finish inside the measurement.
        advance(1024);
        return peak;
    };

    // Voices are stolen oldest-first, so a given slot is re-stolen once every
    // maximumVoices note-ons: at a spacing of S samples its tail is interrupted
    // S * maximumVoices samples in. Sweeping S therefore walks the steal across
    // every phase of the 3 ms window, and the closing third - where a hand-off
    // that fails to carry the running tail's emitted value leaves a step the
    // size of the stolen voice's own output - falls inside the sweep.
    //
    // The default Awaken of zero is what makes this reachable: it puts a voice
    // at full envelope within the 3 ms window, so the voice being stolen the
    // second time still has a full-amplitude sample to hand over. Lengthening
    // Awaken to quieten the mix would also quieten that sample and hide the
    // very step being measured.
    //
    // Measured worst jump over the sweep: 0.0124 here, against 0.0203 for a
    // hand-off that divides by a floored window instead of carrying the emitted
    // value. That older form degrades monotonically across S = 13..17 (0.0100,
    // 0.0134, 0.0164, 0.0187, 0.0203) and snaps back to the 0.0098 baseline at
    // S = 18, where the tail has just ended and no carry is needed.
    const auto burstJump = [&model](int spacingSamples)
    {
        neuramar::NeuramarEngine engine;
        engine.prepare(sampleRate, 64);
        engine.setModel(&model);
        auto parameters = cleanCoreParameters();
        parameters.outputGain = 0.08f;
        engine.setParameters(parameters);

        float left = 0.0f;
        float right = 0.0f;
        double previous = 0.0;
        double jump = 0.0;
        bool measuring = false;
        const auto advance = [&](int samples)
        {
            for (int sample = 0; sample < samples; ++sample)
            {
                engine.process(&left, &right, 1);
                const auto current = static_cast<double>(left);
                if (measuring)
                    jump = std::max(jump, std::abs(current - previous));
                previous = current;
            }
        };

        // Fill every slot and let the voices climb to a usable level.
        for (int index = 0; index < 8; ++index)
            engine.noteOn(48 + index, 0.9f);
        advance(24000);
        // Now steal round-robin: slot k is re-stolen every eight note-ons, so
        // the spacing sweep walks that steal through every phase of the window.
        measuring = true;
        for (int index = 0; index < 64; ++index)
        {
            engine.noteOn(48 + index % 25, 0.9f);
            advance(spacingSamples);
        }
        advance(1024);
        return jump;
    };

    const double shortBurst = burstPeak(200, 1);
    const double longBurst = burstPeak(3200, 1);
    const double spacedBurst = burstPeak(3200, 4);
    std::cout << "Voice-steal burst diagnostics: peak " << shortBurst
              << " after 200 note-ons, " << longBurst << " after 3200, "
              << spacedBurst << " at four-sample spacing (guard 2.0)\n";

    double worstJump = 0.0;
    int worstSpacing = 0;
    for (int spacing = 1; spacing <= 24; ++spacing)
    {
        const double jump = burstJump(spacing);
        if (jump > worstJump)
        {
            worstJump = jump;
            worstSpacing = spacing;
        }
    }
    std::cout << "Voice-steal hand-off diagnostics: worst inter-sample jump "
              << worstJump << " at " << worstSpacing
              << "-sample spacing (guard 0.016)\n";
    expect(worstJump < 0.016,
           "stealing a slot part-way through its fade tail left a step in the "
           "output (worst inter-sample jump " + std::to_string(worstJump)
               + " at " + std::to_string(worstSpacing) + "-sample spacing)");
    expect(longBurst < 2.0 && spacedBurst < 2.0,
           "a dense note-on burst drove the output toward the finite-output "
           "guard (peak " + std::to_string(std::max(longBurst, spacedBurst))
               + ")");
    expect(longBurst < 1.25 * shortBurst + 0.05,
           "the voice-steal fade tail grows with the length of a note-on burst "
           "(peak " + std::to_string(shortBurst) + " after 200 note-ons, "
               + std::to_string(longBurst) + " after 3200)");
}

// Awaken advertises seconds, so it has to deliver seconds. Dividing a faded
// render by an otherwise identical unfaded one recovers the envelope exactly:
// nothing else about the two voices differs.
void testAwakenIsAFadeDuration(const neuramar::NeuralModel& model)
{
    constexpr double sampleRate = 48000.0;
    constexpr float fadeSeconds = 0.25f;
    const auto renderWithAttack = [&model](float attackSeconds)
    {
        neuramar::NeuramarEngine engine;
        engine.prepare(sampleRate, 128);
        engine.setModel(&model);
        auto parameters = cleanCoreParameters();
        parameters.attackSeconds = attackSeconds;
        parameters.outputGain = 1.0f;
        engine.setParameters(parameters);
        return renderNoteAtVelocity(engine, model.rootMidiNote(), 0.8f,
                                    static_cast<int>(sampleRate * 0.5));
    };

    const auto reference = renderWithAttack(0.0f);
    const auto faded = renderWithAttack(fadeSeconds);
    const auto bucket = static_cast<std::size_t>(sampleRate * 0.002);
    std::vector<double> envelope;
    for (std::size_t start = 0; start + bucket <= reference.size();
         start += bucket)
    {
        double fadedPower = 0.0;
        double referencePower = 0.0;
        for (std::size_t index = start; index < start + bucket; ++index)
        {
            fadedPower += static_cast<double>(faded[index]) * faded[index];
            referencePower += static_cast<double>(reference[index])
                * reference[index];
        }
        envelope.push_back(referencePower > 1.0e-12
            ? std::sqrt(fadedPower / referencePower) : 0.0);
    }

    const auto at = [&envelope](double seconds)
    {
        const auto index = static_cast<std::size_t>(seconds / 0.002);
        return index < envelope.size() ? envelope[index] : 0.0;
    };
    const double early = at(0.05 * fadeSeconds);
    const double middle = at(0.5 * fadeSeconds);
    const double complete = at(1.05 * fadeSeconds);
    std::cout << "Awaken diagnostics: " << fadeSeconds << " s fade reaches "
              << early << " at 5%, " << middle << " at 50%, " << complete
              << " at 105% of its stated time\n";
    expect(complete > 0.99,
           "Awaken did not finish within the time it advertises (level "
               + std::to_string(complete) + " just past the stated fade)");
    expect(middle > 0.42 && middle < 0.58,
           "Awaken is not centred halfway through its stated time (level "
               + std::to_string(middle) + ")");
    expect(early < 0.05,
           "Awaken opens with a step rather than a zero slope (level "
               + std::to_string(early) + " at 5% of the fade)");
    bool monotone = true;
    for (std::size_t index = 1;
         index < envelope.size() && index * 0.002 < fadeSeconds; ++index)
        monotone = monotone && envelope[index] >= envelope[index - 1] - 0.01;
    expect(monotone, "the Awaken fade was not monotone");
}
} // namespace

int main()
{
    testShapePreservingSpectralInterpolation();
    testCoreSpurFloor();
    testRenderIsSampleRateInvariant();
    testBodyLayersAreSampleRateInvariant();
    testBoneCeilingIsAnchoredToTheAudibleBand();
    testKeyboardLevelFlatness();
    testInputValidationAndCancellation();
    testAirFilterNormalisation();
    testRandomNeuralSeed();
    testRootAcrossRatesAndRegisters();
    const auto fixture = learnFixture();
    testLearningAndRoot(fixture);
    testResidualScheduleAndQuality(fixture);
    testRootCoreReconstruction(fixture);
    testMissingFundamentalRoot();
    testFormantAndOctaveDominantRoots();
    testBrightHighPitchAcrossRates();
    testHighRegisterOutputLinearity();
    testHarmonicSubtractedAir();
    testPersistentBoneModeSelection();
    testEvolvingAirColour();
    testTransientAirResolution();
    testInharmonicPartialSeries();
    testFormantShift();
    testResamplerAccuracyAndCost();
    if (fixture.first.model)
    {
        testLearnedPitchContour(*fixture.first.model);
        testSerialization(*fixture.first.model);
        testLegacyModelStretchCompatibility(*fixture.first.model);
        testModelVisualisation(*fixture.first.model);
        testRandomizedVariations(*fixture.first.model);
        testModelSpaceNoise(*fixture.first.model);
        testRenderAndTransposition(*fixture.first.model);
        testRootCorrectionRelabelsSourceKey(*fixture.first.model);
        testVelocityTouch(*fixture.first.model);
        testStretchFormantTouchSafety(*fixture.first.model);
        testReleaseAndVoiceBound(*fixture.first.model);
        testReleaseDurationSemantics(*fixture.first.model);
        testAwakenIsAFadeDuration(*fixture.first.model);
        testDenseVoiceStealTailStaysBounded(*fixture.first.model);
        testMutationZeroRetriggerDeterminism(*fixture.first.model);
        testStereoPanAndOverlappingNoteOff(*fixture.first.model);
        testHighRegisterRenderThroughput(*fixture.first.model);
        testRegisterTiltTracksTheKeyboard(*fixture.first.model);
        testOutputGainIsSmoothed(*fixture.first.model);
        testStereoAirIsMonoSafe(*fixture.first.model);
        testLoopRegionAvoidsTheAttack(fixture);
        testRoughPerformance(*fixture.first.model);
    }

    if (failures != 0)
    {
        std::cerr << failures << " Neuramar DSP test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Neuramar DSP tests passed\n";
    return EXIT_SUCCESS;
}
