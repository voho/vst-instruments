#include "NeuralModel.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace neuramar
{
namespace
{
constexpr std::uint32_t modelMagic = 0x524d524eu; // "NRMR" in little-endian.
constexpr std::size_t headerBytes = 16;
constexpr std::size_t maximumSerializedBytes = 128 * 1024;
constexpr float twoPi = 6.28318530717958647692f;
constexpr float generatedRootFrequencyHz = 261.6255653005986f;

// Every subsystem has its own named domain. Adding a random draw to Air, for
// example, cannot silently change Core, Bone, pitch, or network conditioning
// for the same public seed.
constexpr std::uint64_t corePhaseSeedDomain = 0x636f726570680001ull;
constexpr std::uint64_t bonePhaseSeedDomain = 0x626f6e6570680002ull;
constexpr std::uint64_t coreSpectrumSeedDomain = 0x636f726573700003ull;
constexpr std::uint64_t airSpectrumSeedDomain = 0x6169727370650004ull;
constexpr std::uint64_t boneSpectrumSeedDomain = 0x626f6e6573700005ull;
constexpr std::uint64_t pitchSeedDomain = 0x7069746368000006ull;
constexpr std::uint64_t inputWeightsSeedDomain = 0x696e707574770007ull;
constexpr std::uint64_t hiddenBiasSeedDomain = 0x6869646269610008ull;
constexpr std::uint64_t coreWeightsSeedDomain = 0x636f726577740009ull;
constexpr std::uint64_t airWeightsSeedDomain = 0x616972776768000aull;
constexpr std::uint64_t boneWeightsSeedDomain = 0x626f6e657774000bull;
constexpr std::uint64_t pitchWeightsSeedDomain = 0x706974776768000cull;
constexpr std::uint64_t coreBiasSeedDomain = 0x636f72656269000dull;
constexpr std::uint64_t airBiasSeedDomain = 0x616972626961000eull;
constexpr std::uint64_t boneBiasSeedDomain = 0x626f6e656269000full;
constexpr std::uint64_t pitchBiasSeedDomain = 0x7069746269610010ull;
constexpr std::uint64_t inharmonicitySeedDomain = 0x696e6861726d0011ull;
// A generated or varied memory only ever reaches a mild wound-string stretch.
// Fitted memories may exceed this; randomisation deliberately does not.
constexpr float generatedInharmonicityCeiling = 0.0004f;

[[nodiscard]] std::uint64_t mixSeed(std::uint64_t value) noexcept
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebull;
    value ^= value >> 31;
    return value;
}

class RandomStream
{
public:
    RandomStream(std::uint64_t seed, std::uint64_t domain) noexcept
        : state(mixSeed(seed ^ domain))
    {
    }

    [[nodiscard]] float bipolar() noexcept
    {
        // The upper 24 bits map exactly into float's useful mantissa range.
        constexpr float inverse24BitRange = 1.0f / 16777216.0f;
        return 2.0f * static_cast<float>(next() >> 40)
            * inverse24BitRange - 1.0f;
    }

private:
    [[nodiscard]] std::uint64_t next() noexcept
    {
        state += 0x9e3779b97f4a7c15ull;
        return mixSeed(state);
    }

    std::uint64_t state;
};

[[nodiscard]] float boundedStrength(float strength) noexcept
{
    return std::clamp(std::isfinite(strength) ? strength : 0.0f,
                      0.0f, 1.0f);
}

[[nodiscard]] float wrapUnit(float value) noexcept
{
    value -= std::floor(value);
    return value < 0.0f ? value + 1.0f : value;
}

[[nodiscard]] float interpolateUnitPhase(float current, float target,
                                         float amount) noexcept
{
    float difference = target - current;
    difference -= std::floor(difference + 0.5f);
    return wrapUnit(current + amount * difference);
}

[[nodiscard]] float interpolate(float current, float target,
                                float amount) noexcept
{
    return current + amount * (target - current);
}

template <std::size_t Size>
[[nodiscard]] float sampleSmoothField(
    const std::array<float, Size>& anchors, float coordinate) noexcept
{
    static_assert(Size >= 2);
    const float position = std::clamp(coordinate, 0.0f, 1.0f)
        * static_cast<float>(Size - 1);
    const auto lower = std::min(
        static_cast<std::size_t>(position), Size - 2);
    const float fraction = position - static_cast<float>(lower);
    const float smoothFraction = fraction * fraction
        * (3.0f - 2.0f * fraction);
    return anchors[lower]
        + smoothFraction * (anchors[lower + 1] - anchors[lower]);
}

void normaliseRowRms(float* values, std::size_t size,
                     float targetRms) noexcept
{
    if (values == nullptr || size == 0)
        return;

    double mean = 0.0;
    for (std::size_t index = 0; index < size; ++index)
        mean += values[index];
    mean /= static_cast<double>(size);

    double squareSum = 0.0;
    for (std::size_t index = 0; index < size; ++index)
    {
        values[index] -= static_cast<float>(mean);
        squareSum += static_cast<double>(values[index]) * values[index];
    }
    const float rms = static_cast<float>(std::sqrt(
        squareSum / static_cast<double>(size)));
    const float gain = rms > 1.0e-8f ? targetRms / rms : 0.0f;
    for (std::size_t index = 0; index < size; ++index)
        values[index] *= gain;
}

template <std::size_t Size>
void normaliseEnergyWithCeiling(std::array<float, Size>& amplitudes,
                                float desiredEnergy,
                                float ceiling) noexcept
{
    desiredEnergy = std::clamp(
        std::isfinite(desiredEnergy) ? desiredEnergy : 0.0f,
        0.0f, static_cast<float>(Size) * ceiling * ceiling);
    std::array<bool, Size> fixed {};
    float remainingEnergy = desiredEnergy;

    for (std::size_t iteration = 0; iteration < Size; ++iteration)
    {
        double freeEnergy = 0.0;
        for (std::size_t index = 0; index < Size; ++index)
        {
            if (! fixed[index])
            {
                const float value = std::max(
                    std::isfinite(amplitudes[index])
                        ? amplitudes[index] : 0.0f,
                    1.0e-7f);
                amplitudes[index] = value;
                freeEnergy += static_cast<double>(value) * value;
            }
        }

        if (freeEnergy <= 1.0e-20 || remainingEnergy <= 0.0f)
        {
            for (std::size_t index = 0; index < Size; ++index)
                if (! fixed[index])
                    amplitudes[index] = 0.0f;
            break;
        }

        const float gain = std::sqrt(
            remainingEnergy / static_cast<float>(freeEnergy));
        bool cappedAny = false;
        for (std::size_t index = 0; index < Size; ++index)
        {
            if (fixed[index])
                continue;
            const float scaled = amplitudes[index] * gain;
            if (scaled > ceiling)
            {
                amplitudes[index] = ceiling;
                fixed[index] = true;
                remainingEnergy = std::max(
                    0.0f, remainingEnergy - ceiling * ceiling);
                cappedAny = true;
            }
        }

        if (! cappedAny)
        {
            for (std::size_t index = 0; index < Size; ++index)
                if (! fixed[index])
                    amplitudes[index] *= gain;
            break;
        }
    }
}

class BinaryWriter
{
public:
    explicit BinaryWriter(std::size_t reserveBytes)
    {
        bytes.reserve(reserveBytes);
    }

    void writeU32(std::uint32_t value)
    {
        for (int shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }

    void writeU16(std::uint16_t value)
    {
        for (int shift = 0; shift < 16; shift += 8)
            bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }

    void writeI16(std::int16_t value)
    {
        writeU16(std::bit_cast<std::uint16_t>(value));
    }

    void writeI32(std::int32_t value)
    {
        writeU32(std::bit_cast<std::uint32_t>(value));
    }

    void writeU64(std::uint64_t value)
    {
        for (int shift = 0; shift < 64; shift += 8)
            bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }

    void writeFloat(float value)
    {
        writeU32(std::bit_cast<std::uint32_t>(value));
    }

    void writeDouble(double value)
    {
        writeU64(std::bit_cast<std::uint64_t>(value));
    }

    std::vector<std::uint8_t> bytes;
};

class BinaryReader
{
public:
    explicit BinaryReader(std::span<const std::uint8_t> input) : bytes(input) {}

    [[nodiscard]] bool readU32(std::uint32_t& destination) noexcept
    {
        if (remaining() < 4)
            return false;

        destination = 0;
        for (int shift = 0; shift < 32; shift += 8)
            destination |= static_cast<std::uint32_t>(bytes[position++]) << shift;
        return true;
    }

    [[nodiscard]] bool readU16(std::uint16_t& destination) noexcept
    {
        if (remaining() < 2)
            return false;

        destination = 0;
        for (int shift = 0; shift < 16; shift += 8)
            destination |= static_cast<std::uint16_t>(bytes[position++]) << shift;
        return true;
    }

    [[nodiscard]] bool readI16(std::int16_t& destination) noexcept
    {
        std::uint16_t bits = 0;
        if (!readU16(bits))
            return false;
        destination = std::bit_cast<std::int16_t>(bits);
        return true;
    }

    [[nodiscard]] bool readI32(std::int32_t& destination) noexcept
    {
        std::uint32_t bits = 0;
        if (!readU32(bits))
            return false;
        destination = std::bit_cast<std::int32_t>(bits);
        return true;
    }

    [[nodiscard]] bool readU64(std::uint64_t& destination) noexcept
    {
        if (remaining() < 8)
            return false;

        destination = 0;
        for (int shift = 0; shift < 64; shift += 8)
            destination |= static_cast<std::uint64_t>(bytes[position++]) << shift;
        return true;
    }

    [[nodiscard]] bool readFloat(float& destination) noexcept
    {
        std::uint32_t bits = 0;
        if (!readU32(bits))
            return false;
        destination = std::bit_cast<float>(bits);
        return true;
    }

    [[nodiscard]] bool readDouble(double& destination) noexcept
    {
        std::uint64_t bits = 0;
        if (!readU64(bits))
            return false;
        destination = std::bit_cast<double>(bits);
        return true;
    }

    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return bytes.size() - position;
    }

private:
    std::span<const std::uint8_t> bytes;
    std::size_t position { 0 };
};

[[nodiscard]] std::uint32_t checksum(std::span<const std::uint8_t> bytes) noexcept
{
    std::uint32_t hash = 2166136261u;
    for (const auto byte : bytes)
    {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

template <typename Container>
void writeFloats(BinaryWriter& writer, const Container& values)
{
    for (const auto value : values)
        writer.writeFloat(value);
}

template <typename Container>
[[nodiscard]] bool readFloats(BinaryReader& reader, Container& values) noexcept
{
    for (auto& value : values)
        if (!reader.readFloat(value))
            return false;
    return true;
}

template <typename Container>
[[nodiscard]] bool finiteAndBounded(const Container& values,
                                    float maximumMagnitude) noexcept
{
    return std::all_of(values.begin(), values.end(),
                       [maximumMagnitude](float value)
                       {
                           return std::isfinite(value)
                               && std::abs(value) <= maximumMagnitude;
                       });
}

void setError(std::string* error, const char* message)
{
    if (error != nullptr)
        *error = message;
}
} // namespace

std::unique_ptr<NeuralModel> NeuralModel::clone() const
{
    auto result = std::unique_ptr<NeuralModel>(new NeuralModel());
    result->metadata_ = metadata_;
    result->initialHarmonicPhases_ = initialHarmonicPhases_;
    result->airCentreFrequenciesHz_ = airCentreFrequenciesHz_;
    result->airBandwidthOctaves_ = airBandwidthOctaves_;
    result->boneFrequencyRatios_ = boneFrequencyRatios_;
    result->boneDecaySeconds_ = boneDecaySeconds_;
    result->boneModeReliabilities_ = boneModeReliabilities_;
    result->initialBonePhases_ = initialBonePhases_;
    result->outputMeans_ = outputMeans_;
    result->outputScales_ = outputScales_;
    result->inputWeights_ = inputWeights_;
    result->hiddenBiases_ = hiddenBiases_;
    result->outputWeights_ = outputWeights_;
    result->outputBiases_ = outputBiases_;
    result->inharmonicity_ = inharmonicity_;
    result->residualKeyframeCount_ = residualKeyframeCount_;
    result->residualScales_ = residualScales_;
    result->residualTimes_ = residualTimes_;
    result->residualValues_ = residualValues_;
    return result;
}

void NeuralModel::refreshWaveformPreview(float blend) noexcept
{
    blend = boundedStrength(blend);
    if (blend <= 0.0f)
        return;

    // inharmonicity_ is fixed for the whole call, so the stretched ratio of
    // each visible harmonic does not depend on which of the 256 preview
    // points is being generated. Resolving it once here instead of inside
    // the per-point loop below cuts 8192 stretchedHarmonicRatio() calls
    // (256 points * 32 harmonics) down to 32 per refreshWaveformPreview(),
    // the same "hoist what the loop does not change" fix already applied to
    // the per-partial ratio tables in SampleLearner.cpp and
    // ModelVisualisation.cpp.
    constexpr std::size_t visibleHarmonicCount = 32;
    std::array<float, visibleHarmonicCount> visibleHarmonicRatios {};
    for (std::size_t harmonic = 0; harmonic < visibleHarmonicCount; ++harmonic)
        visibleHarmonicRatios[harmonic] = stretchedHarmonicRatio(
            static_cast<float>(harmonic + 1), inharmonicity_);

    std::array<float, previewSize> generated {};
    float generatedPeak = 0.0f;
    for (std::size_t point = 0; point < generated.size(); ++point)
    {
        const float time = static_cast<float>(point)
            / static_cast<float>(generated.size() - 1);
        SynthesisFrame frame;
        evaluate(time, frame);

        // A handful of visible carrier cycles lets the display communicate
        // both the generated waveform and its slower neural evolution.
        const float carrier = 7.0f * time
            + 0.11f * std::sin(twoPi * time);
        float value = 0.0f;
        for (std::size_t harmonic = 0;
             harmonic < visibleHarmonicCount; ++harmonic)
        {
            value += frame.harmonicAmplitudes[harmonic] * std::sin(
                twoPi * (carrier * visibleHarmonicRatios[harmonic]
                         + initialHarmonicPhases_[harmonic]));
        }
        for (std::size_t mode = 0; mode < boneModeCount; ++mode)
        {
            value += 0.45f * frame.boneAmplitudes[mode] * std::sin(
                twoPi * (carrier * boneFrequencyRatios_[mode]
                         + initialBonePhases_[mode]));
        }
        generated[point] = std::isfinite(value) ? value : 0.0f;
        generatedPeak = std::max(generatedPeak, std::abs(generated[point]));
    }

    const float generatedGain = generatedPeak > 1.0e-7f
        ? 0.90f / generatedPeak : 0.0f;
    for (std::size_t point = 0; point < generated.size(); ++point)
    {
        const float synthesised = generated[point] * generatedGain;
        metadata_.waveformPreview[point] = std::clamp(
            metadata_.waveformPreview[point]
                + blend * (synthesised - metadata_.waveformPreview[point]),
            -1.0f, 1.0f);
    }
}

std::unique_ptr<NeuralModel>
NeuralModel::createRandom(std::uint64_t seed, float strength)
{
    auto model = std::unique_ptr<NeuralModel>(new NeuralModel());
    auto& metadata = model->metadata_;
    metadata.sourceSampleRate = 48000.0;
    metadata.rootFrequencyHz = generatedRootFrequencyHz;
    metadata.rootMidiNote = 60;
    metadata.rootCents = 0.0f;
    // C4 is a deliberate anchor, not a pitch estimate from evidence.
    metadata.pitchConfidence = 0.0f;
    metadata.durationSeconds = 3.2f;
    metadata.loopStartSeconds = 0.72f;
    metadata.loopEndSeconds = 2.78f;
    metadata.sourcePeak = 0.90f;
    metadata.sourceRms = 0.24f;
    metadata.initialLoss = 0.0f;
    metadata.finalLoss = 0.0f;
    metadata.trainingEpochs = 0;

    constexpr float goldenFraction = 0.6180339887498948f;
    for (std::size_t harmonic = 0; harmonic < harmonicCount; ++harmonic)
    {
        const float harmonicNumber = static_cast<float>(harmonic + 1);
        model->initialHarmonicPhases_[harmonic] = wrapUnit(
            goldenFraction * harmonicNumber + 0.071f * std::sin(harmonicNumber));

        const float octave = std::log2(harmonicNumber);
        const float formantDistance = (octave - 2.35f) / 0.82f;
        const float formant = 1.0f
            + 0.24f * std::exp(-0.5f * formantDistance * formantDistance);
        const float oddEven = (harmonic & 1u) == 0u ? 1.0f : 0.76f;
        const float amplitude = std::clamp(
            0.56f * std::pow(harmonicNumber, -1.08f)
                * formant * oddEven,
            1.0e-6f, 1.0f);
        model->outputMeans_[harmonic] = std::log(amplitude);
        model->outputScales_[harmonic] = 0.55f;
    }

    constexpr float lowerAirHz = 80.0f;
    constexpr float upperAirHz = 16000.0f;
    const float airEdgeRatio = std::pow(
        upperAirHz / lowerAirHz, 1.0f / static_cast<float>(airBandCount));
    const float airWidthOctaves = std::log2(airEdgeRatio);
    for (std::size_t band = 0; band < airBandCount; ++band)
    {
        model->airCentreFrequenciesHz_[band] = lowerAirHz * std::pow(
            airEdgeRatio, static_cast<float>(band) + 0.5f);
        model->airBandwidthOctaves_[band] = airWidthOctaves;

        // The generated Air tilt is expressed as a fraction of the band count
        // so widening the filterbank moves the same spectral shape onto the
        // denser grid instead of squeezing it into the bottom half.
        const float position = (static_cast<float>(band) + 0.5f)
            / static_cast<float>(airBandCount);
        const float distance = (position - 0.5750f) / 0.2750f;
        const float amplitude = 0.008f
            + 0.010f * std::exp(-0.5f * distance * distance);
        const std::size_t output = harmonicCount + band;
        model->outputMeans_[output] = std::log(amplitude);
        model->outputScales_[output] = 0.50f;
    }

    constexpr std::array<float, boneModeCount> generatedBoneRatios {
        1.47f, 2.31f, 3.68f, 5.17f, 7.42f, 10.83f,
        12.61f, 14.97f, 17.34f, 20.11f, 23.28f, 26.71f
    };
    constexpr std::array<float, boneModeCount> generatedBoneDecays {
        1.85f, 1.52f, 1.22f, 0.96f, 0.76f, 0.58f,
        0.49f, 0.42f, 0.36f, 0.31f, 0.27f, 0.23f
    };
    constexpr std::array<float, boneModeCount> generatedBoneReliabilities {
        0.72f, 0.63f, 0.54f, 0.46f, 0.37f, 0.29f,
        0.24f, 0.20f, 0.17f, 0.14f, 0.11f, 0.09f
    };
    for (std::size_t mode = 0; mode < boneModeCount; ++mode)
    {
        model->boneFrequencyRatios_[mode] = generatedBoneRatios[mode];
        model->boneDecaySeconds_[mode] = generatedBoneDecays[mode];
        model->boneModeReliabilities_[mode]
            = generatedBoneReliabilities[mode];
        model->initialBonePhases_[mode] = wrapUnit(
            0.173f + goldenFraction * static_cast<float>(mode + 1));

        const float amplitude = 0.017f
            * std::exp(-0.28f * static_cast<float>(mode));
        const std::size_t output = harmonicCount + airBandCount + mode;
        model->outputMeans_[output] = std::log(amplitude);
        model->outputScales_[output] = 0.50f;
    }
    model->outputMeans_[pitchOutputIndex] = 0.0f;
    model->outputScales_[pitchOutputIndex] = 0.22f;

    // A deterministic, well-conditioned base network makes every setting
    // playable, including 1%, while the supplied seed supplies all identity.
    for (std::size_t unit = 0; unit < hiddenSize; ++unit)
    {
        const float unitNumber = static_cast<float>(unit + 1);
        model->hiddenBiases_[unit] = 0.07f * std::sin(0.73f * unitNumber);
        for (std::size_t input = 0; input < inputSize; ++input)
        {
            const float inputNumber = static_cast<float>(input + 1);
            model->inputWeights_[unit * inputSize + input]
                = 0.22f * std::sin(0.371f * unitNumber * inputNumber)
                + 0.07f * std::cos(0.619f * (unitNumber + inputNumber));
        }
    }
    for (std::size_t output = 0; output < outputSize; ++output)
    {
        const float outputNumber = static_cast<float>(output + 1);
        const float rowScale = output == pitchOutputIndex ? 0.0018f : 0.011f;
        for (std::size_t unit = 0; unit < hiddenSize; ++unit)
        {
            const float unitNumber = static_cast<float>(unit + 1);
            model->outputWeights_[output * hiddenSize + unit] = rowScale
                * (std::sin(0.137f * outputNumber * unitNumber)
                   + 0.45f * std::cos(
                       0.211f * (outputNumber + unitNumber)));
        }
    }

    model->refreshWaveformPreview(1.0f);
    auto randomized = model->createRandomizedVariation(seed, strength);

    float peak = 0.0f;
    double squareSum = 0.0;
    for (const auto sample : randomized->metadata_.waveformPreview)
    {
        peak = std::max(peak, std::abs(sample));
        squareSum += static_cast<double>(sample) * sample;
    }
    randomized->metadata_.sourcePeak = std::max(peak, 1.0e-6f);
    randomized->metadata_.sourceRms = std::min(
        randomized->metadata_.sourcePeak,
        static_cast<float>(std::sqrt(
            squareSum / static_cast<double>(previewSize))));
    return randomized;
}

std::unique_ptr<NeuralModel>
NeuralModel::createRandomizedVariation(std::uint64_t seed,
                                       float strength) const
{
    auto result = clone();
    const float amount = boundedStrength(strength);
    if (amount <= 0.0f)
        return result;

    // First measure the model's perceptual layer statistics. Every full-range
    // reroll is built as a fresh bounded target with the same group energy,
    // rather than adding another random offset to yesterday's random offset.
    constexpr std::size_t calibrationFrameCount = 33;
    std::array<double, harmonicCount> coreSquares {};
    std::array<double, airBandCount> airSquares {};
    std::array<double, boneModeCount> boneSquares {};
    for (std::size_t frameIndex = 0;
         frameIndex < calibrationFrameCount; ++frameIndex)
    {
        const float time = static_cast<float>(frameIndex)
            / static_cast<float>(calibrationFrameCount - 1);
        SynthesisFrame frame;
        evaluate(time, frame);
        for (std::size_t harmonic = 0; harmonic < harmonicCount; ++harmonic)
            coreSquares[harmonic] += static_cast<double>(
                frame.harmonicAmplitudes[harmonic])
                * frame.harmonicAmplitudes[harmonic];
        for (std::size_t band = 0; band < airBandCount; ++band)
            airSquares[band] += static_cast<double>(
                frame.airAmplitudes[band]) * frame.airAmplitudes[band];
        for (std::size_t mode = 0; mode < boneModeCount; ++mode)
            boneSquares[mode] += static_cast<double>(
                frame.boneAmplitudes[mode]) * frame.boneAmplitudes[mode];
    }

    std::array<float, harmonicCount> coreTargetAmplitudes {};
    std::array<float, airBandCount> airTargetAmplitudes {};
    std::array<float, boneModeCount> boneTargetAmplitudes {};
    double measuredCoreEnergy = 0.0;
    double measuredAirEnergy = 0.0;
    double measuredBoneEnergy = 0.0;
    constexpr double inverseCalibrationFrames
        = 1.0 / static_cast<double>(calibrationFrameCount);
    for (std::size_t harmonic = 0; harmonic < harmonicCount; ++harmonic)
    {
        const float square = static_cast<float>(
            coreSquares[harmonic] * inverseCalibrationFrames);
        coreTargetAmplitudes[harmonic] = std::sqrt(std::max(square, 0.0f));
        measuredCoreEnergy += square;
    }
    for (std::size_t band = 0; band < airBandCount; ++band)
    {
        const float square = static_cast<float>(
            airSquares[band] * inverseCalibrationFrames);
        airTargetAmplitudes[band] = std::sqrt(std::max(square, 0.0f));
        measuredAirEnergy += square;
    }
    for (std::size_t mode = 0; mode < boneModeCount; ++mode)
    {
        const float square = static_cast<float>(
            boneSquares[mode] * inverseCalibrationFrames);
        boneTargetAmplitudes[mode] = std::sqrt(std::max(square, 0.0f));
        measuredBoneEnergy += square;
    }

    const float desiredCoreEnergy = std::clamp(
        static_cast<float>(measuredCoreEnergy), 0.03f, 0.90f);
    const float desiredAirEnergy = std::clamp(
        static_cast<float>(measuredAirEnergy), 1.0e-6f, 0.025f);
    const float desiredBoneEnergy = std::clamp(
        static_cast<float>(measuredBoneEnergy), 1.0e-6f, 0.025f);

    auto target = clone();
    RandomStream corePhaseStream(seed, corePhaseSeedDomain);
    RandomStream bonePhaseStream(seed, bonePhaseSeedDomain);
    for (auto& phase : target->initialHarmonicPhases_)
        phase = 0.5f * (corePhaseStream.bipolar() + 1.0f);
    for (auto& phase : target->initialBonePhases_)
        phase = 0.5f * (bonePhaseStream.bipolar() + 1.0f);

    RandomStream coreSpectrumStream(seed, coreSpectrumSeedDomain);
    std::array<float, 9> coreSpectrumAnchors {};
    for (auto& anchor : coreSpectrumAnchors)
        anchor = coreSpectrumStream.bipolar();
    const float coreTilt = coreSpectrumStream.bipolar();
    const float oddEven = coreSpectrumStream.bipolar();
    for (std::size_t harmonic = 0; harmonic < harmonicCount; ++harmonic)
    {
        const float coordinate = std::log2(static_cast<float>(harmonic + 1))
            / std::log2(static_cast<float>(harmonicCount));
        const float smooth = sampleSmoothField(
            coreSpectrumAnchors, coordinate);
        const float parity = (harmonic & 1u) == 0u ? oddEven : -oddEven;
        const float field = 0.68f * smooth
            + 0.14f * coreSpectrumStream.bipolar()
            + 0.10f * coreTilt * (2.0f * coordinate - 1.0f)
            + 0.08f * parity;
        coreTargetAmplitudes[harmonic] = std::max(
            coreTargetAmplitudes[harmonic], 1.0e-6f)
            * std::exp(0.82f * field);
    }
    normaliseEnergyWithCeiling(
        coreTargetAmplitudes, desiredCoreEnergy, 0.72f);
    for (std::size_t harmonic = 0; harmonic < harmonicCount; ++harmonic)
        target->outputMeans_[harmonic] = std::log(
            std::max(coreTargetAmplitudes[harmonic], 1.0e-6f));

    RandomStream airSpectrumStream(seed, airSpectrumSeedDomain);
    std::array<float, 5> airSpectrumAnchors {};
    for (auto& anchor : airSpectrumAnchors)
        anchor = airSpectrumStream.bipolar();
    for (std::size_t band = 0; band < airBandCount; ++band)
    {
        const float coordinate = static_cast<float>(band)
            / static_cast<float>(airBandCount - 1);
        const float field = 0.82f * sampleSmoothField(
                airSpectrumAnchors, coordinate)
            + 0.18f * airSpectrumStream.bipolar();
        airTargetAmplitudes[band] = std::max(
            airTargetAmplitudes[band], 1.0e-6f)
            * std::exp(0.72f * field);
    }
    normaliseEnergyWithCeiling(
        airTargetAmplitudes, desiredAirEnergy, 0.10f);
    for (std::size_t band = 0; band < airBandCount; ++band)
    {
        target->outputMeans_[harmonicCount + band] = std::log(
            std::max(airTargetAmplitudes[band], 1.0e-6f));
    }

    RandomStream boneSpectrumStream(seed, boneSpectrumSeedDomain);
    std::array<float, 4> boneSpectrumAnchors {};
    for (auto& anchor : boneSpectrumAnchors)
        anchor = boneSpectrumStream.bipolar();
    for (std::size_t mode = 0; mode < boneModeCount; ++mode)
    {
        const float coordinate = static_cast<float>(mode)
            / static_cast<float>(boneModeCount - 1);
        const float field = 0.78f * sampleSmoothField(
                boneSpectrumAnchors, coordinate)
            + 0.22f * boneSpectrumStream.bipolar();
        boneTargetAmplitudes[mode] = std::max(
            boneTargetAmplitudes[mode], 1.0e-6f)
            * std::exp(0.72f * field);
    }
    normaliseEnergyWithCeiling(
        boneTargetAmplitudes, desiredBoneEnergy, 0.12f);
    for (std::size_t mode = 0; mode < boneModeCount; ++mode)
    {
        target->outputMeans_[harmonicCount + airBandCount + mode] = std::log(
            std::max(boneTargetAmplitudes[mode], 1.0e-6f));
    }

    RandomStream inputWeightsStream(seed, inputWeightsSeedDomain);
    for (std::size_t unit = 0; unit < hiddenSize; ++unit)
    {
        auto* row = target->inputWeights_.data() + unit * inputSize;
        for (std::size_t input = 0; input < inputSize; ++input)
            row[input] = inputWeightsStream.bipolar();
        const float rowRms = 0.22f
            + 0.08f * 0.5f * (inputWeightsStream.bipolar() + 1.0f);
        normaliseRowRms(row, inputSize, rowRms);
    }

    RandomStream hiddenBiasStream(seed, hiddenBiasSeedDomain);
    for (auto& bias : target->hiddenBiases_)
        bias = 0.18f * hiddenBiasStream.bipolar();

    RandomStream coreWeightsStream(seed, coreWeightsSeedDomain);
    for (std::size_t unit = 0; unit < hiddenSize; ++unit)
    {
        std::array<float, 8> anchors {};
        for (auto& anchor : anchors)
            anchor = coreWeightsStream.bipolar();
        for (std::size_t harmonic = 0; harmonic < harmonicCount; ++harmonic)
        {
            const float coordinate = std::log2(
                static_cast<float>(harmonic + 1))
                / std::log2(static_cast<float>(harmonicCount));
            target->outputWeights_[harmonic * hiddenSize + unit]
                = 0.84f * sampleSmoothField(anchors, coordinate)
                + 0.16f * coreWeightsStream.bipolar();
        }
    }
    for (std::size_t harmonic = 0; harmonic < harmonicCount; ++harmonic)
    {
        normaliseRowRms(
            target->outputWeights_.data() + harmonic * hiddenSize,
            hiddenSize, 0.045f);
        target->outputScales_[harmonic] = 0.42f;
    }

    RandomStream airWeightsStream(seed, airWeightsSeedDomain);
    for (std::size_t unit = 0; unit < hiddenSize; ++unit)
    {
        std::array<float, 4> anchors {};
        for (auto& anchor : anchors)
            anchor = airWeightsStream.bipolar();
        for (std::size_t band = 0; band < airBandCount; ++band)
        {
            const float coordinate = static_cast<float>(band)
                / static_cast<float>(airBandCount - 1);
            target->outputWeights_[
                (harmonicCount + band) * hiddenSize + unit]
                = 0.82f * sampleSmoothField(anchors, coordinate)
                + 0.18f * airWeightsStream.bipolar();
        }
    }
    for (std::size_t band = 0; band < airBandCount; ++band)
    {
        const std::size_t output = harmonicCount + band;
        normaliseRowRms(
            target->outputWeights_.data() + output * hiddenSize,
            hiddenSize, 0.040f);
        target->outputScales_[output] = 0.38f;
    }

    RandomStream boneWeightsStream(seed, boneWeightsSeedDomain);
    for (std::size_t unit = 0; unit < hiddenSize; ++unit)
    {
        std::array<float, 4> anchors {};
        for (auto& anchor : anchors)
            anchor = boneWeightsStream.bipolar();
        for (std::size_t mode = 0; mode < boneModeCount; ++mode)
        {
            const float coordinate = static_cast<float>(mode)
                / static_cast<float>(boneModeCount - 1);
            target->outputWeights_[
                (harmonicCount + airBandCount + mode) * hiddenSize + unit]
                = 0.80f * sampleSmoothField(anchors, coordinate)
                + 0.20f * boneWeightsStream.bipolar();
        }
    }
    for (std::size_t mode = 0; mode < boneModeCount; ++mode)
    {
        const std::size_t output = harmonicCount + airBandCount + mode;
        normaliseRowRms(
            target->outputWeights_.data() + output * hiddenSize,
            hiddenSize, 0.040f);
        target->outputScales_[output] = 0.38f;
    }

    RandomStream pitchWeightsStream(seed, pitchWeightsSeedDomain);
    auto* pitchWeights = target->outputWeights_.data()
        + pitchOutputIndex * hiddenSize;
    for (std::size_t unit = 0; unit < hiddenSize; ++unit)
        pitchWeights[unit] = pitchWeightsStream.bipolar();
    normaliseRowRms(pitchWeights, hiddenSize, 0.010f);
    target->outputScales_[pitchOutputIndex] = 0.16f;

    RandomStream coreBiasStream(seed, coreBiasSeedDomain);
    for (std::size_t harmonic = 0; harmonic < harmonicCount; ++harmonic)
        target->outputBiases_[harmonic]
            = 0.08f * coreBiasStream.bipolar();
    RandomStream airBiasStream(seed, airBiasSeedDomain);
    for (std::size_t band = 0; band < airBandCount; ++band)
        target->outputBiases_[harmonicCount + band]
            = 0.10f * airBiasStream.bipolar();
    RandomStream boneBiasStream(seed, boneBiasSeedDomain);
    for (std::size_t mode = 0; mode < boneModeCount; ++mode)
        target->outputBiases_[harmonicCount + airBandCount + mode]
            = 0.10f * boneBiasStream.bipolar();
    RandomStream pitchBiasStream(seed, pitchBiasSeedDomain);
    target->outputBiases_[pitchOutputIndex]
        = 0.10f * pitchBiasStream.bipolar();
    RandomStream pitchStream(seed, pitchSeedDomain);
    target->outputMeans_[pitchOutputIndex]
        = 0.25f * pitchStream.bipolar();

    // A squared draw keeps most generated identities close to an ideal
    // harmonic series while still occasionally growing a wound-string or
    // tine-like stretch. The bound is far below the fitted model limit.
    RandomStream inharmonicityStream(seed, inharmonicitySeedDomain);
    const float inharmonicityDraw = 0.5f
        * (inharmonicityStream.bipolar() + 1.0f);
    target->inharmonicity_ = generatedInharmonicityCeiling
        * inharmonicityDraw * inharmonicityDraw;

    target->residualKeyframeCount_ = 0;
    target->residualScales_.fill(0.0f);
    target->residualTimes_.fill(0.0f);
    target->residualValues_.fill(0);

    // Account for the target network's temporal modulation. A uniform log
    // correction per layer makes its measured average energy exactly match
    // the bounded source statistic, so repeated 100% rerolls do not walk.
    double targetCoreEnergy = 0.0;
    double targetAirEnergy = 0.0;
    double targetBoneEnergy = 0.0;
    float minimumPitch = std::numeric_limits<float>::max();
    float maximumPitch = std::numeric_limits<float>::lowest();
    for (std::size_t frameIndex = 0;
         frameIndex < calibrationFrameCount; ++frameIndex)
    {
        const float time = static_cast<float>(frameIndex)
            / static_cast<float>(calibrationFrameCount - 1);
        SynthesisFrame frame;
        target->evaluate(time, frame);
        for (const auto amplitude : frame.harmonicAmplitudes)
            targetCoreEnergy += static_cast<double>(amplitude) * amplitude;
        for (const auto amplitude : frame.airAmplitudes)
            targetAirEnergy += static_cast<double>(amplitude) * amplitude;
        for (const auto amplitude : frame.boneAmplitudes)
            targetBoneEnergy += static_cast<double>(amplitude) * amplitude;

        std::array<float, outputSize> raw {};
        target->evaluateBaseRaw(time, raw);
        minimumPitch = std::min(minimumPitch, raw[pitchOutputIndex]);
        maximumPitch = std::max(maximumPitch, raw[pitchOutputIndex]);
    }
    targetCoreEnergy *= inverseCalibrationFrames;
    targetAirEnergy *= inverseCalibrationFrames;
    targetBoneEnergy *= inverseCalibrationFrames;

    const auto calibrateLayer = [] (
        std::array<float, outputSize>& means,
        std::size_t first, std::size_t count,
        float desiredEnergy, double actualEnergy)
    {
        if (!(actualEnergy > 1.0e-20))
            return;
        const float logGain = 0.5f * std::log(
            desiredEnergy / static_cast<float>(actualEnergy));
        for (std::size_t index = 0; index < count; ++index)
            means[first + index] += logGain;
    };
    calibrateLayer(target->outputMeans_, 0, harmonicCount,
                   desiredCoreEnergy, targetCoreEnergy);
    calibrateLayer(target->outputMeans_, harmonicCount, airBandCount,
                   desiredAirEnergy, targetAirEnergy);
    calibrateLayer(target->outputMeans_, harmonicCount + airBandCount,
                   boneModeCount, desiredBoneEnergy, targetBoneEnergy);

    const float desiredPitchCentre = target->outputMeans_[pitchOutputIndex];
    target->outputMeans_[pitchOutputIndex] += desiredPitchCentre
        - 0.5f * (minimumPitch + maximumPitch);

    for (std::size_t harmonic = 0; harmonic < harmonicCount; ++harmonic)
    {
        result->initialHarmonicPhases_[harmonic] = interpolateUnitPhase(
            initialHarmonicPhases_[harmonic],
            target->initialHarmonicPhases_[harmonic], amount);
    }
    for (std::size_t mode = 0; mode < boneModeCount; ++mode)
    {
        result->initialBonePhases_[mode] = interpolateUnitPhase(
            initialBonePhases_[mode], target->initialBonePhases_[mode], amount);
    }
    for (std::size_t output = 0; output < outputSize; ++output)
    {
        result->outputMeans_[output] = interpolate(
            outputMeans_[output], target->outputMeans_[output], amount);
        result->outputScales_[output] = interpolate(
            outputScales_[output], target->outputScales_[output], amount);
        result->outputBiases_[output] = interpolate(
            outputBiases_[output], target->outputBiases_[output], amount);
    }
    for (std::size_t index = 0; index < inputWeights_.size(); ++index)
    {
        result->inputWeights_[index] = interpolate(
            inputWeights_[index], target->inputWeights_[index], amount);
    }
    for (std::size_t unit = 0; unit < hiddenSize; ++unit)
    {
        result->hiddenBiases_[unit] = interpolate(
            hiddenBiases_[unit], target->hiddenBiases_[unit], amount);
    }
    for (std::size_t index = 0; index < outputWeights_.size(); ++index)
    {
        result->outputWeights_[index] = interpolate(
            outputWeights_[index], target->outputWeights_[index], amount);
    }
    result->inharmonicity_ = std::clamp(
        interpolate(inharmonicity_, target->inharmonicity_, amount),
        0.0f, maximumInharmonicity);

    // Learned residuals deliberately pin the neural base to analysis
    // keyframes. Relaxing them by the same selected amount lets a 1% variation
    // retain almost all detail while 100% exposes the new network identity.
    const float residualRetention = 1.0f - amount;
    if (residualRetention <= 0.0f)
    {
        result->residualKeyframeCount_ = 0;
        result->residualScales_.fill(0.0f);
        result->residualTimes_.fill(0.0f);
        result->residualValues_.fill(0);
    }
    else
    {
        for (std::size_t output = 0; output < outputSize; ++output)
        {
            result->residualScales_[output] *= residualRetention;
            if (result->residualScales_[output] <= 1.0e-8f)
            {
                result->residualScales_[output] = 0.0f;
                for (std::size_t frame = 0;
                     frame < result->residualKeyframeCount_; ++frame)
                {
                    result->residualValues_[frame * outputSize + output] = 0;
                }
            }
        }
    }

    // Parameter interpolation is not energy-linear: repeatedly taking a 1%
    // or 10% step toward fresh zero-centred weight targets can otherwise
    // produce a slow loudness fade. Re-anchor each layer's measured energy
    // after the blend while leaving its new spectral shape and residual scale
    // untouched. Pathological source levels still move gradually toward the
    // target manifold's musical ceiling.
    double blendedCoreEnergy = 0.0;
    double blendedAirEnergy = 0.0;
    double blendedBoneEnergy = 0.0;
    for (std::size_t frameIndex = 0;
         frameIndex < calibrationFrameCount; ++frameIndex)
    {
        const float time = static_cast<float>(frameIndex)
            / static_cast<float>(calibrationFrameCount - 1);
        SynthesisFrame frame;
        result->evaluate(time, frame);
        for (const auto amplitude : frame.harmonicAmplitudes)
            blendedCoreEnergy += static_cast<double>(amplitude) * amplitude;
        for (const auto amplitude : frame.airAmplitudes)
            blendedAirEnergy += static_cast<double>(amplitude) * amplitude;
        for (const auto amplitude : frame.boneAmplitudes)
            blendedBoneEnergy += static_cast<double>(amplitude) * amplitude;
    }
    blendedCoreEnergy *= inverseCalibrationFrames;
    blendedAirEnergy *= inverseCalibrationFrames;
    blendedBoneEnergy *= inverseCalibrationFrames;

    calibrateLayer(
        result->outputMeans_, 0, harmonicCount,
        interpolate(static_cast<float>(measuredCoreEnergy),
                    desiredCoreEnergy, amount),
        blendedCoreEnergy);
    calibrateLayer(
        result->outputMeans_, harmonicCount, airBandCount,
        interpolate(static_cast<float>(measuredAirEnergy),
                    desiredAirEnergy, amount),
        blendedAirEnergy);
    calibrateLayer(
        result->outputMeans_, harmonicCount + airBandCount, boneModeCount,
        interpolate(static_cast<float>(measuredBoneEnergy),
                    desiredBoneEnergy, amount),
        blendedBoneEnergy);

    result->refreshWaveformPreview(amount);
    return result;
}

void NeuralModel::evaluateBaseRaw(
    float normalisedTime,
    std::array<float, outputSize>& destination) const noexcept
{
    const float time = std::clamp(
        std::isfinite(normalisedTime) ? normalisedTime : 0.0f, 0.0f, 1.0f);
    const std::array<float, inputSize> inputs = networkInputsAt(
        time, metadata_.durationSeconds);

    std::array<float, hiddenSize> hidden {};
    for (std::size_t unit = 0; unit < hiddenSize; ++unit)
    {
        float activation = hiddenBiases_[unit];
        for (std::size_t input = 0; input < inputSize; ++input)
            activation += inputWeights_[unit * inputSize + input] * inputs[input];
        hidden[unit] = std::tanh(activation);
    }

    for (std::size_t output = 0; output < outputSize; ++output)
    {
        float normalised = outputBiases_[output];
        for (std::size_t unit = 0; unit < hiddenSize; ++unit)
            normalised += outputWeights_[output * hiddenSize + unit] * hidden[unit];

        destination[output] = outputMeans_[output]
            + outputScales_[output] * normalised;
    }
}

void NeuralModel::evaluate(float normalisedTime,
                           SynthesisFrame& destination) const noexcept
{
    const float time = std::clamp(
        std::isfinite(normalisedTime) ? normalisedTime : 0.0f, 0.0f, 1.0f);
    std::array<float, outputSize> decodedOutputs {};
    evaluateBaseRaw(time, decodedOutputs);

    if (residualKeyframeCount_ > 0)
    {
        std::size_t first = 0;
        std::size_t last = residualKeyframeCount_;
        while (first < last)
        {
            const std::size_t middle = first + (last - first) / 2;
            if (residualTimes_[middle] < time)
                first = middle + 1;
            else
                last = middle;
        }
        std::size_t upper = first;

        std::size_t lower = upper;
        float fraction = 0.0f;
        if (upper == residualKeyframeCount_)
        {
            upper = residualKeyframeCount_ - 1;
            lower = upper;
        }
        else if (upper > 0 && residualTimes_[upper] != time)
        {
            lower = upper - 1;
            const float interval = residualTimes_[upper] - residualTimes_[lower];
            fraction = interval > 0.0f
                ? (time - residualTimes_[lower]) / interval : 0.0f;
        }

        constexpr float inverseQuantisation = 1.0f / 32767.0f;
        for (std::size_t output = 0; output < outputSize; ++output)
        {
            const float first = static_cast<float>(
                residualValues_[lower * outputSize + output]);
            const float second = static_cast<float>(
                residualValues_[upper * outputSize + output]);
            const float interpolated = first + fraction * (second - first);
            decodedOutputs[output] += residualScales_[output]
                * interpolated * inverseQuantisation;
        }
    }

    for (std::size_t output = 0; output < amplitudeOutputSize; ++output)
        decodedOutputs[output] = std::exp(
            std::clamp(decodedOutputs[output], -16.0f, 1.5f));

    std::copy_n(decodedOutputs.begin(), harmonicCount,
                destination.harmonicAmplitudes.begin());
    std::copy_n(decodedOutputs.begin()
                    + static_cast<std::ptrdiff_t>(harmonicCount),
                airBandCount, destination.airAmplitudes.begin());
    std::copy_n(decodedOutputs.begin()
                    + static_cast<std::ptrdiff_t>(harmonicCount + airBandCount),
                boneModeCount, destination.boneAmplitudes.begin());
    destination.pitchRatio = std::exp2(std::clamp(
        decodedOutputs[pitchOutputIndex], -4.0f, 4.0f) / 12.0f);
}

void NeuralModel::evaluate(float normalisedTime, float latentCoordinate,
                           SynthesisFrame& destination) const noexcept
{
    if (! std::isfinite(latentCoordinate) || latentCoordinate == 0.0f)
    {
        // Keep the established path literal so NOISE=0 is bit-identical for
        // old presets, tests, and hosts.
        evaluate(normalisedTime, destination);
        return;
    }

    const float time = std::clamp(
        std::isfinite(normalisedTime) ? normalisedTime : 0.0f, 0.0f, 1.0f);
    const float latent = std::clamp(latentCoordinate, -1.0f, 1.0f);
    const float edgeWindow = std::sin(0.5f * twoPi * time);
    // Keep the creative range consistent for both short gestures and long
    // recordings. Five-and-a-half percent is useful for compact memories, but
    // on a 12-30 second state it would become coarse temporal scrubbing.
    constexpr float maximumWarpSeconds = 0.180f;
    const float duration = std::max(metadata_.durationSeconds, 0.001f);
    const float maximumWarp = std::min(
        0.055f, maximumWarpSeconds / duration) * edgeWindow * edgeWindow;
    const float warpedTime = std::clamp(
        time + latent * maximumWarp, 0.0f, 1.0f);

    // This remains an ordinary evaluation of the learned neural and residual
    // trajectory. The latent coordinate only selects a nearby manifold point;
    // no signal-domain noise or out-of-model output is added.
    evaluate(warpedTime, destination);
}

std::vector<std::uint8_t> NeuralModel::serialize() const
{
    BinaryWriter payload(8192);
    payload.writeDouble(metadata_.sourceSampleRate);
    payload.writeFloat(metadata_.rootFrequencyHz);
    payload.writeI32(metadata_.rootMidiNote);
    payload.writeFloat(metadata_.rootCents);
    payload.writeFloat(metadata_.pitchConfidence);
    payload.writeFloat(metadata_.durationSeconds);
    payload.writeFloat(metadata_.loopStartSeconds);
    payload.writeFloat(metadata_.loopEndSeconds);
    payload.writeFloat(metadata_.sourcePeak);
    payload.writeFloat(metadata_.sourceRms);
    payload.writeFloat(metadata_.initialLoss);
    payload.writeFloat(metadata_.finalLoss);
    payload.writeI32(metadata_.trainingEpochs);
    writeFloats(payload, metadata_.waveformPreview);
    writeFloats(payload, initialHarmonicPhases_);
    writeFloats(payload, airCentreFrequenciesHz_);
    writeFloats(payload, airBandwidthOctaves_);
    writeFloats(payload, boneFrequencyRatios_);
    writeFloats(payload, boneDecaySeconds_);
    writeFloats(payload, boneModeReliabilities_);
    writeFloats(payload, initialBonePhases_);
    writeFloats(payload, outputMeans_);
    writeFloats(payload, outputScales_);
    writeFloats(payload, inputWeights_);
    writeFloats(payload, hiddenBiases_);
    writeFloats(payload, outputWeights_);
    writeFloats(payload, outputBiases_);
    payload.writeU32(residualKeyframeCount_);
    writeFloats(payload, residualScales_);
    for (std::size_t frame = 0; frame < residualKeyframeCount_; ++frame)
        payload.writeFloat(residualTimes_[frame]);
    for (std::size_t frame = 0; frame < residualKeyframeCount_; ++frame)
        for (std::size_t output = 0; output < outputSize; ++output)
            payload.writeI16(residualValues_[frame * outputSize + output]);
    payload.writeFloat(inharmonicity_);

    BinaryWriter complete(headerBytes + payload.bytes.size());
    complete.writeU32(modelMagic);
    complete.writeU32(currentFormatVersion);
    complete.writeU32(static_cast<std::uint32_t>(payload.bytes.size()));
    complete.writeU32(checksum(payload.bytes));
    complete.bytes.insert(complete.bytes.end(), payload.bytes.begin(), payload.bytes.end());
    return complete.bytes;
}

std::unique_ptr<NeuralModel>
NeuralModel::deserialize(std::span<const std::uint8_t> bytes,
                         std::string* error)
{
    if (error != nullptr)
        error->clear();

    if (bytes.size() < headerBytes || bytes.size() > maximumSerializedBytes)
    {
        setError(error, "Neuramar model has an invalid size");
        return nullptr;
    }

    BinaryReader header(bytes.first(headerBytes));
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t payloadSize = 0;
    std::uint32_t expectedChecksum = 0;
    if (!header.readU32(magic) || !header.readU32(version)
        || !header.readU32(payloadSize) || !header.readU32(expectedChecksum))
    {
        setError(error, "Neuramar model header is truncated");
        return nullptr;
    }

    if (magic != modelMagic)
    {
        setError(error, "Not a Neuramar model");
        return nullptr;
    }
    if (version < bandFormatVersion || version > currentFormatVersion)
    {
        setError(error, "Unsupported Neuramar model version");
        return nullptr;
    }
    if (payloadSize != bytes.size() - headerBytes)
    {
        setError(error, "Neuramar model length does not match its header");
        return nullptr;
    }

    const auto payloadBytes = bytes.subspan(headerBytes);
    if (checksum(payloadBytes) != expectedChecksum)
    {
        setError(error, "Neuramar model checksum failed");
        return nullptr;
    }

    auto model = std::unique_ptr<NeuralModel>(new NeuralModel());
    BinaryReader reader(payloadBytes);
    std::int32_t rootMidi = 0;
    std::int32_t epochs = 0;
    auto& metadata = model->metadata_;

    const bool scalarsRead = reader.readDouble(metadata.sourceSampleRate)
        && reader.readFloat(metadata.rootFrequencyHz)
        && reader.readI32(rootMidi)
        && reader.readFloat(metadata.rootCents)
        && reader.readFloat(metadata.pitchConfidence)
        && reader.readFloat(metadata.durationSeconds)
        && reader.readFloat(metadata.loopStartSeconds)
        && reader.readFloat(metadata.loopEndSeconds)
        && reader.readFloat(metadata.sourcePeak)
        && reader.readFloat(metadata.sourceRms)
        && reader.readFloat(metadata.initialLoss)
        && reader.readFloat(metadata.finalLoss)
        && reader.readI32(epochs);

    // Everything before version 5 stored 8 Air bands and 6 Bone modes, and its
    // decoder rows are laid out for that. Such a payload is read into
    // fixed-size legacy buffers and migrated: the harmonic block and the first
    // eight Air slots keep their index, the six Bone slots move past the added
    // Air slots, and the added slots are given valid but silent state so the
    // memory renders exactly as it did.
    const bool legacyLayout = version < bodyCapacityFormatVersion;
    bool arraysRead = scalarsRead
        && readFloats(reader, metadata.waveformPreview)
        && readFloats(reader, model->initialHarmonicPhases_);
    if (legacyLayout)
    {
        std::array<float, legacyAirBandCount> legacyAirCentres {};
        std::array<float, legacyAirBandCount> legacyAirWidths {};
        std::array<float, legacyBoneModeCount> legacyBoneRatios {};
        std::array<float, legacyBoneModeCount> legacyBoneDecays {};
        std::array<float, legacyBoneModeCount> legacyBoneReliabilities {};
        std::array<float, legacyBoneModeCount> legacyBonePhases {};
        std::array<float, legacyOutputSize> legacyMeans {};
        std::array<float, legacyOutputSize> legacyScales {};
        auto legacyWeights = std::make_unique<
            std::array<float, legacyOutputSize * hiddenSize>>();
        std::array<float, legacyOutputSize> legacyBiases {};
        arraysRead = arraysRead
            && readFloats(reader, legacyAirCentres)
            && readFloats(reader, legacyAirWidths)
            && readFloats(reader, legacyBoneRatios)
            && readFloats(reader, legacyBoneDecays)
            && readFloats(reader, legacyBoneReliabilities)
            && readFloats(reader, legacyBonePhases)
            && readFloats(reader, legacyMeans)
            && readFloats(reader, legacyScales)
            && readFloats(reader, model->inputWeights_)
            && readFloats(reader, model->hiddenBiases_)
            && readFloats(reader, *legacyWeights)
            && readFloats(reader, legacyBiases);
        if (arraysRead)
        {
            // The added slots have to satisfy the same validation as a real
            // band or mode, so they are given a plausible layout and then
            // decoded as silence: an amplitude output of exp(-16) with no
            // network contribution, and a Bone reliability of zero, which is
            // what the renderer tests to decide a mode is inactive.
            const float airEdgeRatio = std::pow(
                16000.0f / 80.0f, 1.0f / static_cast<float>(airBandCount));
            for (std::size_t band = legacyAirBandCount;
                 band < airBandCount; ++band)
            {
                model->airCentreFrequenciesHz_[band] = 80.0f * std::pow(
                    airEdgeRatio, static_cast<float>(band) + 0.5f);
                model->airBandwidthOctaves_[band] = std::log2(airEdgeRatio);
            }
            for (std::size_t mode = legacyBoneModeCount;
                 mode < boneModeCount; ++mode)
            {
                model->boneFrequencyRatios_[mode] = 1.0f
                    + static_cast<float>(mode);
                model->boneDecaySeconds_[mode] = 1.0f;
                model->boneModeReliabilities_[mode] = 0.0f;
                model->initialBonePhases_[mode] = 0.0f;
            }
            const auto silenceOutput = [&model](std::size_t output)
            {
                model->outputMeans_[output] = -16.0f;
                model->outputScales_[output] = 1.0e-4f;
                model->outputBiases_[output] = 0.0f;
                for (std::size_t unit = 0; unit < hiddenSize; ++unit)
                    model->outputWeights_[output * hiddenSize + unit] = 0.0f;
            };
            for (std::size_t band = legacyAirBandCount;
                 band < airBandCount; ++band)
                silenceOutput(harmonicCount + band);
            for (std::size_t mode = legacyBoneModeCount;
                 mode < boneModeCount; ++mode)
                silenceOutput(harmonicCount + airBandCount + mode);

            for (std::size_t band = 0; band < legacyAirBandCount; ++band)
            {
                model->airCentreFrequenciesHz_[band] = legacyAirCentres[band];
                model->airBandwidthOctaves_[band] = legacyAirWidths[band];
            }
            for (std::size_t mode = 0; mode < legacyBoneModeCount; ++mode)
            {
                model->boneFrequencyRatios_[mode] = legacyBoneRatios[mode];
                model->boneDecaySeconds_[mode] = legacyBoneDecays[mode];
                model->boneModeReliabilities_[mode]
                    = legacyBoneReliabilities[mode];
                model->initialBonePhases_[mode] = legacyBonePhases[mode];
            }
            for (std::size_t output = 0; output < legacyOutputSize; ++output)
            {
                const std::size_t target = migrateOutputIndex(output);
                model->outputMeans_[target] = legacyMeans[output];
                model->outputScales_[target] = legacyScales[output];
                model->outputBiases_[target] = legacyBiases[output];
                for (std::size_t unit = 0; unit < hiddenSize; ++unit)
                    model->outputWeights_[target * hiddenSize + unit]
                        = (*legacyWeights)[output * hiddenSize + unit];
            }
        }
    }
    else
    {
        arraysRead = arraysRead
            && readFloats(reader, model->airCentreFrequenciesHz_)
            && readFloats(reader, model->airBandwidthOctaves_)
            && readFloats(reader, model->boneFrequencyRatios_)
            && readFloats(reader, model->boneDecaySeconds_)
            && readFloats(reader, model->boneModeReliabilities_)
            && readFloats(reader, model->initialBonePhases_)
            && readFloats(reader, model->outputMeans_)
            && readFloats(reader, model->outputScales_)
            && readFloats(reader, model->inputWeights_)
            && readFloats(reader, model->hiddenBiases_)
            && readFloats(reader, model->outputWeights_)
            && readFloats(reader, model->outputBiases_);
    }
    if (!arraysRead)
    {
        setError(error, "Neuramar model payload is truncated or malformed");
        return nullptr;
    }

    if (version == bandFormatVersion)
    {
        // Version 2 ends after the neural base. Its correction state and
        // inharmonicity remain value-initialised to zero so rendering follows
        // the original path exactly.
        if (reader.remaining() != 0)
        {
            setError(error, "Neuramar version-2 payload has trailing data");
            return nullptr;
        }
    }
    else
    {
        const std::size_t storedOutputSize = legacyLayout
            ? legacyOutputSize : outputSize;
        std::uint32_t keyframeCount = 0;
        bool headerRead = reader.readU32(keyframeCount)
            && keyframeCount <= maximumResidualKeyframes;
        if (headerRead)
        {
            for (std::size_t output = 0; output < storedOutputSize; ++output)
            {
                float scale = 0.0f;
                if (!reader.readFloat(scale))
                {
                    headerRead = false;
                    break;
                }
                model->residualScales_[legacyLayout
                    ? migrateOutputIndex(output) : output] = scale;
            }
        }
        if (!headerRead)
        {
            setError(error, "Neuramar residual header is truncated or malformed");
            return nullptr;
        }
        model->residualKeyframeCount_ = keyframeCount;
        for (std::size_t frame = 0; frame < keyframeCount; ++frame)
        {
            if (!reader.readFloat(model->residualTimes_[frame]))
            {
                setError(error, "Neuramar residual times are truncated");
                return nullptr;
            }
        }
        for (std::size_t frame = 0; frame < keyframeCount; ++frame)
        {
            for (std::size_t output = 0; output < storedOutputSize; ++output)
            {
                std::int16_t value = 0;
                if (!reader.readI16(value))
                {
                    setError(error, "Neuramar residual values are truncated");
                    return nullptr;
                }
                const std::size_t target = legacyLayout
                    ? migrateOutputIndex(output) : output;
                model->residualValues_[frame * outputSize + target] = value;
            }
        }
        // Version 3 stops here; versions 4 and 5 append the stiff-string
        // coefficient. Anything left over is rejected in every case.
        if (version >= stretchFormatVersion
            && !reader.readFloat(model->inharmonicity_))
        {
            setError(error, "Neuramar inharmonicity field is truncated");
            return nullptr;
        }
        if (reader.remaining() != 0)
        {
            setError(error, "Neuramar model payload has trailing data");
            return nullptr;
        }
    }

    metadata.rootMidiNote = static_cast<int>(rootMidi);
    metadata.trainingEpochs = static_cast<int>(epochs);

    const bool metadataValid = std::isfinite(metadata.sourceSampleRate)
        && metadata.sourceSampleRate >= 8000.0
        && metadata.sourceSampleRate <= 768000.0
        && std::isfinite(metadata.rootFrequencyHz)
        && metadata.rootFrequencyHz >= 20.0f
        && metadata.rootFrequencyHz <= 5000.0f
        && metadata.rootMidiNote >= 0 && metadata.rootMidiNote <= 127
        && std::isfinite(metadata.rootCents)
        && std::abs(metadata.rootCents) <= 50.1f
        && std::isfinite(metadata.pitchConfidence)
        && metadata.pitchConfidence >= 0.0f && metadata.pitchConfidence <= 1.0f
        && std::isfinite(metadata.durationSeconds)
        && metadata.durationSeconds > 0.0f && metadata.durationSeconds <= 30.0f
        && std::isfinite(metadata.loopStartSeconds)
        && std::isfinite(metadata.loopEndSeconds)
        && metadata.loopStartSeconds >= 0.0f
        && metadata.loopEndSeconds > metadata.loopStartSeconds
        && metadata.loopEndSeconds <= metadata.durationSeconds
        && std::isfinite(metadata.sourcePeak) && metadata.sourcePeak > 0.0f
        && metadata.sourcePeak <= 100.0f
        && std::isfinite(metadata.sourceRms) && metadata.sourceRms >= 0.0f
        && metadata.sourceRms <= metadata.sourcePeak
        && std::isfinite(metadata.initialLoss) && metadata.initialLoss >= 0.0f
        && std::isfinite(metadata.finalLoss) && metadata.finalLoss >= 0.0f
        && metadata.trainingEpochs >= 0 && metadata.trainingEpochs <= 10000;

    bool scalesValid = true;
    for (const auto scale : model->outputScales_)
        scalesValid = scalesValid && std::isfinite(scale)
            && scale >= 1.0e-4f && scale <= 20.0f;

    bool airValid = true;
    for (std::size_t index = 0; index < airBandCount; ++index)
        airValid = airValid
            && std::isfinite(model->airCentreFrequenciesHz_[index])
            && model->airCentreFrequenciesHz_[index] >= 20.0f
            && model->airCentreFrequenciesHz_[index] <= 30000.0f
            && std::isfinite(model->airBandwidthOctaves_[index])
            && model->airBandwidthOctaves_[index] >= 0.1f
            && model->airBandwidthOctaves_[index] <= 4.0f;

    bool boneValid = true;
    for (std::size_t index = 0; index < boneModeCount; ++index)
        boneValid = boneValid
            && std::isfinite(model->boneFrequencyRatios_[index])
            && model->boneFrequencyRatios_[index] >= 0.25f
            && model->boneFrequencyRatios_[index] <= 64.0f
            && std::isfinite(model->boneDecaySeconds_[index])
            && model->boneDecaySeconds_[index] >= 0.001f
            && model->boneDecaySeconds_[index] <= 60.0f
            && std::isfinite(model->boneModeReliabilities_[index])
            && model->boneModeReliabilities_[index] >= 0.0f
            && model->boneModeReliabilities_[index] <= 1.0f;

    const bool arraysValid = finiteAndBounded(metadata.waveformPreview, 1.1f)
        && finiteAndBounded(model->initialHarmonicPhases_, 1.0f)
        && finiteAndBounded(model->initialBonePhases_, 1.0f)
        && finiteAndBounded(model->outputMeans_, 40.0f)
        && finiteAndBounded(model->inputWeights_, 100.0f)
        && finiteAndBounded(model->hiddenBiases_, 100.0f)
        && finiteAndBounded(model->outputWeights_, 100.0f)
        && finiteAndBounded(model->outputBiases_, 100.0f);

    bool residualsValid = true;
    for (const auto scale : model->residualScales_)
    {
        residualsValid = residualsValid && std::isfinite(scale)
            && scale >= 0.0f && scale <= maximumResidualScale;
    }
    for (std::size_t frame = 0;
         frame < model->residualKeyframeCount_; ++frame)
    {
        const float time = model->residualTimes_[frame];
        residualsValid = residualsValid && std::isfinite(time)
            && time >= 0.0f && time <= 1.0f
            && (frame == 0 || time > model->residualTimes_[frame - 1]);
        for (std::size_t output = 0; output < outputSize; ++output)
        {
            const auto value = model->residualValues_[frame * outputSize + output];
            residualsValid = residualsValid
                && value != std::numeric_limits<std::int16_t>::min()
                && (model->residualScales_[output] > 0.0f || value == 0);
        }
    }
    if (model->residualKeyframeCount_ == 0)
    {
        residualsValid = residualsValid && std::all_of(
            model->residualScales_.begin(), model->residualScales_.end(),
            [](float scale) { return scale == 0.0f; });
    }

    const bool inharmonicityValid = std::isfinite(model->inharmonicity_)
        && model->inharmonicity_ >= 0.0f
        && model->inharmonicity_ <= maximumInharmonicity;

    if (!metadataValid || !scalesValid || !airValid || !boneValid
        || !arraysValid || !residualsValid || !inharmonicityValid)
    {
        setError(error, "Neuramar model contains invalid or unsafe values");
        return nullptr;
    }

    return model;
}

} // namespace neuramar
