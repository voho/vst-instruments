#include "NeuralModel.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <type_traits>

namespace neuramar
{
namespace
{
constexpr std::uint32_t modelMagic = 0x524d524eu; // "NRMR" in little-endian.
constexpr std::size_t headerBytes = 16;
constexpr std::size_t maximumSerializedBytes = 128 * 1024;
constexpr float twoPi = 6.28318530717958647692f;

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

void NeuralModel::evaluate(float normalisedTime,
                           SynthesisFrame& destination) const noexcept
{
    const float time = std::clamp(
        std::isfinite(normalisedTime) ? normalisedTime : 0.0f, 0.0f, 1.0f);
    const float centred = 2.0f * time - 1.0f;
    const float timeSeconds = time * std::max(metadata_.durationSeconds, 0.0f);
    const std::array<float, inputSize> inputs {
        centred,
        centred * centred,
        std::sin(twoPi * time),
        std::cos(twoPi * time),
        std::sin(2.0f * twoPi * time),
        std::cos(2.0f * twoPi * time),
        std::sin(4.0f * twoPi * time),
        std::cos(4.0f * twoPi * time),
        std::exp(-8.0f * timeSeconds),
        std::exp(-40.0f * timeSeconds)
    };

    std::array<float, hiddenSize> hidden {};
    for (std::size_t unit = 0; unit < hiddenSize; ++unit)
    {
        float activation = hiddenBiases_[unit];
        for (std::size_t input = 0; input < inputSize; ++input)
            activation += inputWeights_[unit * inputSize + input] * inputs[input];
        hidden[unit] = std::tanh(activation);
    }

    std::array<float, outputSize> decodedOutputs {};
    for (std::size_t output = 0; output < outputSize; ++output)
    {
        float normalised = outputBiases_[output];
        for (std::size_t unit = 0; unit < hiddenSize; ++unit)
            normalised += outputWeights_[output * hiddenSize + unit] * hidden[unit];

        const float decoded = outputMeans_[output]
            + outputScales_[output] * normalised;
        decodedOutputs[output] = output < amplitudeOutputSize
            ? std::exp(std::clamp(decoded, -16.0f, 1.5f))
            : decoded;
    }

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
    if (version != currentFormatVersion)
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

    if (!scalarsRead
        || !readFloats(reader, metadata.waveformPreview)
        || !readFloats(reader, model->initialHarmonicPhases_)
        || !readFloats(reader, model->airCentreFrequenciesHz_)
        || !readFloats(reader, model->airBandwidthOctaves_)
        || !readFloats(reader, model->boneFrequencyRatios_)
        || !readFloats(reader, model->boneDecaySeconds_)
        || !readFloats(reader, model->boneModeReliabilities_)
        || !readFloats(reader, model->initialBonePhases_)
        || !readFloats(reader, model->outputMeans_)
        || !readFloats(reader, model->outputScales_)
        || !readFloats(reader, model->inputWeights_)
        || !readFloats(reader, model->hiddenBiases_)
        || !readFloats(reader, model->outputWeights_)
        || !readFloats(reader, model->outputBiases_)
        || reader.remaining() != 0)
    {
        setError(error, "Neuramar model payload is truncated or malformed");
        return nullptr;
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

    if (!metadataValid || !scalesValid || !airValid || !boneValid || !arraysValid)
    {
        setError(error, "Neuramar model contains invalid or unsafe values");
        return nullptr;
    }

    return model;
}

} // namespace neuramar
