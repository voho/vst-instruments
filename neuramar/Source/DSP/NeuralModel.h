#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace neuramar
{

struct SynthesisFrame
{
    static constexpr std::size_t harmonicCount = 64;
    static constexpr std::size_t airBandCount = 8;
    static constexpr std::size_t boneModeCount = 6;

    std::array<float, harmonicCount> harmonicAmplitudes {};
    std::array<float, airBandCount> airAmplitudes {};
    std::array<float, boneModeCount> boneAmplitudes {};
    float pitchRatio { 1.0f };
};

class NeuralModel final
{
public:
    // Version 2 introduced renderer-matched overlapping Air bands. Version 3
    // adds a bounded, quantised residual trajectory on top of the neural base.
    // Version-1 development memories used different band semantics and must
    // not be silently rendered with this engine.
    static constexpr std::uint32_t currentFormatVersion = 3;
    static constexpr std::size_t previewSize = 256;
    static constexpr std::size_t harmonicCount = SynthesisFrame::harmonicCount;
    static constexpr std::size_t airBandCount = SynthesisFrame::airBandCount;
    static constexpr std::size_t boneModeCount = SynthesisFrame::boneModeCount;
    static constexpr std::size_t inputSize = 10;
    static constexpr std::size_t hiddenSize = 32;
    static constexpr std::size_t amplitudeOutputSize = harmonicCount
        + airBandCount + boneModeCount;
    static constexpr std::size_t pitchOutputIndex = amplitudeOutputSize;
    static constexpr std::size_t outputSize = amplitudeOutputSize + 1;

    struct Metadata
    {
        double sourceSampleRate { 0.0 };
        float rootFrequencyHz { 0.0f };
        int rootMidiNote { 60 };
        float rootCents { 0.0f };
        float pitchConfidence { 0.0f };
        float durationSeconds { 0.0f };
        float loopStartSeconds { 0.0f };
        float loopEndSeconds { 0.0f };
        float sourcePeak { 0.0f };
        float sourceRms { 0.0f };
        float initialLoss { 0.0f };
        float finalLoss { 0.0f };
        int trainingEpochs { 0 };
        std::array<float, previewSize> waveformPreview {};
    };

    NeuralModel(const NeuralModel&) = delete;
    NeuralModel& operator=(const NeuralModel&) = delete;
    NeuralModel(NeuralModel&&) noexcept = default;
    NeuralModel& operator=(NeuralModel&&) noexcept = default;
    ~NeuralModel() = default;

    [[nodiscard]] const Metadata& metadata() const noexcept { return metadata_; }
    [[nodiscard]] float rootFrequencyHz() const noexcept { return metadata_.rootFrequencyHz; }
    [[nodiscard]] int rootMidiNote() const noexcept { return metadata_.rootMidiNote; }
    [[nodiscard]] float rootCents() const noexcept { return metadata_.rootCents; }
    [[nodiscard]] float pitchConfidence() const noexcept { return metadata_.pitchConfidence; }
    [[nodiscard]] float durationSeconds() const noexcept { return metadata_.durationSeconds; }
    [[nodiscard]] float finalLoss() const noexcept { return metadata_.finalLoss; }
    [[nodiscard]] std::span<const float, airBandCount>
        airCentreFrequenciesHz() const noexcept { return airCentreFrequenciesHz_; }
    [[nodiscard]] std::span<const float, airBandCount>
        airBandwidthOctaves() const noexcept { return airBandwidthOctaves_; }
    [[nodiscard]] std::span<const float, boneModeCount>
        boneFrequencyRatios() const noexcept { return boneFrequencyRatios_; }
    [[nodiscard]] std::span<const float, boneModeCount>
        boneDecaySeconds() const noexcept { return boneDecaySeconds_; }
    [[nodiscard]] std::span<const float, boneModeCount>
        boneModeReliabilities() const noexcept { return boneModeReliabilities_; }

    // Evaluates the compact time-conditioned network. The time argument is
    // normalised to the learned sample duration and is clamped to [0, 1].
    void evaluate(float normalisedTime, SynthesisFrame& destination) const noexcept;

    // The binary form is fixed-layout, checksummed, explicitly little-endian,
    // and bounded. Deserialisation never accepts trailing or oversized data.
    [[nodiscard]] std::vector<std::uint8_t> serialize() const;
    [[nodiscard]] static std::unique_ptr<NeuralModel>
        deserialize(std::span<const std::uint8_t> bytes,
                    std::string* error = nullptr);

private:
    NeuralModel() = default;

    static constexpr std::uint32_t legacyFormatVersion = 2;
    static constexpr std::size_t maximumResidualKeyframes = 128;
    static constexpr float maximumResidualScale = 64.0f;

    void evaluateBaseRaw(
        float normalisedTime,
        std::array<float, outputSize>& destination) const noexcept;

    Metadata metadata_ {};
    std::array<float, harmonicCount> initialHarmonicPhases_ {};
    std::array<float, airBandCount> airCentreFrequenciesHz_ {};
    std::array<float, airBandCount> airBandwidthOctaves_ {};
    std::array<float, boneModeCount> boneFrequencyRatios_ {};
    std::array<float, boneModeCount> boneDecaySeconds_ {};
    std::array<float, boneModeCount> boneModeReliabilities_ {};
    std::array<float, boneModeCount> initialBonePhases_ {};
    std::array<float, outputSize> outputMeans_ {};
    std::array<float, outputSize> outputScales_ {};
    std::array<float, hiddenSize * inputSize> inputWeights_ {};
    std::array<float, hiddenSize> hiddenBiases_ {};
    std::array<float, outputSize * hiddenSize> outputWeights_ {};
    std::array<float, outputSize> outputBiases_ {};
    std::uint32_t residualKeyframeCount_ { 0 };
    std::array<float, outputSize> residualScales_ {};
    std::array<float, maximumResidualKeyframes> residualTimes_ {};
    std::array<std::int16_t,
               maximumResidualKeyframes * outputSize> residualValues_ {};

    friend class SampleLearner;
    friend class NeuramarEngine;
    friend struct NeuralModelTrainingAccess;
};

} // namespace neuramar
