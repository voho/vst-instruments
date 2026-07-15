#pragma once

#include "NeuralModel.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace neuramar
{

struct EngineParameters
{
    float imprint { 1.0f };             // learned harmonic core, 0..1
    float bodyLock { 0.65f };            // fixed formants vs pitch-follow, 0..1
    float air { 0.35f };                 // residual/noise branch, 0..1
    float bone { 0.30f };                // inharmonic modal branch, 0..1
    float brightness { 0.50f };          // spectral tilt, 0..1
    float evolutionRate { 1.0f };        // learned-time rate, 0.125..4
    float orbit { 0.15f };               // one-shot to learned-loop blend, 0..1
    float mutation { 0.10f };            // deterministic per-voice variation, 0..1
    float attackSeconds { 0.008f };       // 0..10 seconds
    float releaseSeconds { 0.35f };       // 0.005..20 seconds
    float spread { 0.35f };               // stereo voice spread, 0..1
    float rootOffsetSemitones { 0.0f };   // manual detected-root correction, -12..12
    float outputGain { 0.72f };           // linear gain, 0..2
};

class NeuramarEngine final
{
public:
    static constexpr int maximumVoices = 8;

    NeuramarEngine() noexcept;

    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;

    // The engine never owns or destroys the model. The caller must keep the
    // pointed-to immutable model alive until all process calls that could have
    // observed it have completed.
    void setModel(const NeuralModel* immutableModel) noexcept;
    void setParameters(const EngineParameters& parameters) noexcept;

    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void allNotesOff() noexcept;
    void allSoundOff() noexcept;
    void process(float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] int getActiveVoiceCount() const noexcept;

private:
    static constexpr int sineTableSize = 4096;

    struct AtomicParameters
    {
        std::atomic<float> imprint { 1.0f };
        std::atomic<float> bodyLock { 0.65f };
        std::atomic<float> air { 0.35f };
        std::atomic<float> bone { 0.30f };
        std::atomic<float> brightness { 0.50f };
        std::atomic<float> evolutionRate { 1.0f };
        std::atomic<float> orbit { 0.15f };
        std::atomic<float> mutation { 0.10f };
        std::atomic<float> attackSeconds { 0.008f };
        std::atomic<float> releaseSeconds { 0.35f };
        std::atomic<float> spread { 0.35f };
        std::atomic<float> rootOffsetSemitones { 0.0f };
        std::atomic<float> outputGain { 0.72f };
    };

    struct Bandpass
    {
        float z1 { 0.0f };
        float z2 { 0.0f };
        float b0 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float outputScale { 1.0f };

        void clear() noexcept { z1 = z2 = 0.0f; }
        void set(float centreHz, float bandwidthOctaves,
                 float sampleRate) noexcept;
        [[nodiscard]] float tick(float input) noexcept;
    };

    struct Voice
    {
        bool active { false };
        bool releasing { false };
        int midiNote { -1 };
        int controlCountdown { 0 };
        std::uint64_t ageStamp { 0 };
        std::array<std::uint32_t, NeuralModel::airBandCount> airNoiseStates {};
        float velocity { 0.0f };
        float envelope { 0.0f };
        double modelTimeSeconds { 0.0 };
        float mutationOffset { 0.0f };
        float pan { 0.0f };
        float panLeft { 0.70710678f };
        float panRight { 0.70710678f };
        float transpositionRatio { 1.0f };
        float pitchRatio { 1.0f };
        float pitchRatioStep { 0.0f };
        float lastLeft { 0.0f };
        float lastRight { 0.0f };
        std::array<float, NeuralModel::harmonicCount> harmonicPhases {};
        std::array<float, NeuralModel::boneModeCount> bonePhases {};
        std::array<float, NeuralModel::boneModeCount> boneFrequenciesHz {};
        std::array<float, NeuralModel::amplitudeOutputSize> amplitudes {};
        std::array<float, NeuralModel::amplitudeOutputSize> amplitudeSteps {};
        std::array<Bandpass, NeuralModel::airBandCount> airFilters {};

        void clear() noexcept;
    };

    struct FadeTail
    {
        float left { 0.0f };
        float right { 0.0f };
        float gain { 0.0f };
        float gainStep { 0.0f };
        int remaining { 0 };

        void clear() noexcept { *this = FadeTail {}; }
    };

    [[nodiscard]] EngineParameters loadParameters() const noexcept;
    [[nodiscard]] float sine(float phase) const noexcept;
    void updateVoiceControl(Voice& voice, const NeuralModel& model,
                            const EngineParameters& parameters) noexcept;
    [[nodiscard]] static float nextNoise(std::uint32_t& state) noexcept;
    void beginFadeTail(std::size_t voiceIndex) noexcept;
    void refreshVoicePans() noexcept;

    std::atomic<const NeuralModel*> model_ { nullptr };
    AtomicParameters parameters_ {};
    std::array<Voice, maximumVoices> voices_ {};
    std::array<FadeTail, maximumVoices> fadeTails_ {};
    std::array<float, sineTableSize + 1> sineTable_ {};
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    int controlPeriod_ { 192 };
    std::uint64_t ageCounter_ { 0 };
};

} // namespace neuramar
