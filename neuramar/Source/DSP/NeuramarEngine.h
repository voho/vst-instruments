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
    float noise { 0.0f };                // voice-local model-space drift, 0..1
    float attackSeconds { 0.0f };         // 0..10 seconds
    float releaseSeconds { 0.35f };       // 0.005..20 seconds
    float spread { 0.35f };               // stereo voice spread, 0..1
    float rootOffsetSemitones { 0.0f };   // manual detected-root correction, -12..12
    float outputGain { 0.72f };           // linear gain, 0..2
    // Scales the model's fitted stiff-string coefficient. 1 renders the
    // learned partial stretch, 0 forces an ideal harmonic series, and 2
    // exaggerates it. Legacy memories carry no coefficient, so this control
    // is inaudible for them regardless of its value.
    float stretch { 1.0f };               // 0..2
    // Moves the resonant body in frequency without moving the played pitch.
    float formantShiftSemitones { 0.0f }; // -24..24
    // Velocity-to-timbre depth. Zero preserves the pure amplitude response
    // that every earlier Neuramar build had.
    float touch { 0.0f };                 // 0..1
    // Key tracking. Positive darkens notes played above the learned root and
    // opens up notes played below it, the way a real instrument's spectrum
    // changes across its compass. It changes tone, not level.
    float registerTilt { 0.0f };          // -1..1
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
    // The neural controller remains a compact 64-partial model. A larger,
    // engine-only bank lets Body Lock resample that observed spectrum onto the
    // denser harmonic grid required by lower notes without changing presets.
    static constexpr std::size_t renderedHarmonicCount = 256;
    static constexpr std::size_t airOutputOffset = renderedHarmonicCount;
    static constexpr std::size_t boneOutputOffset = airOutputOffset
        + NeuralModel::airBandCount;
    static constexpr std::size_t renderAmplitudeCount = boneOutputOffset
        + NeuralModel::boneModeCount;

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
        std::atomic<float> noise { 0.0f };
        std::atomic<float> attackSeconds { 0.0f };
        std::atomic<float> releaseSeconds { 0.35f };
        std::atomic<float> spread { 0.35f };
        std::atomic<float> rootOffsetSemitones { 0.0f };
        std::atomic<float> outputGain { 0.72f };
        std::atomic<float> stretch { 1.0f };
        std::atomic<float> formantShiftSemitones { 0.0f };
        std::atomic<float> touch { 0.0f };
        std::atomic<float> registerTilt { 0.0f };
    };

    // One coefficient set with two independent state pairs. The Air layer needs
    // a decorrelated second realisation for stereo width, and the side copy
    // must track exactly the same ramped response as the centred one.
    struct Bandpass
    {
        float z1 { 0.0f };
        float z2 { 0.0f };
        float sideZ1 { 0.0f };
        float sideZ2 { 0.0f };
        float b0 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float outputScale { 1.0f };
        float b0Step { 0.0f };
        float b2Step { 0.0f };
        float a1Step { 0.0f };
        float a2Step { 0.0f };
        float outputScaleStep { 0.0f };
        float configuredCentreHz { -1.0f };
        float configuredBandwidthOctaves { -1.0f };
        int rampRemaining { 0 };

        void clear() noexcept { z1 = z2 = sideZ1 = sideZ2 = 0.0f; }
        void set(float centreHz, float bandwidthOctaves,
                 float sampleRate, int rampSamples) noexcept;
        [[nodiscard]] float tick(float input) noexcept;
        // Uses the coefficients that tick() has already advanced this sample,
        // so it must be called after it and only once per sample.
        [[nodiscard]] float tickSide(float input) noexcept;
    };

    struct Voice
    {
        bool active { false };
        bool releasing { false };
        int midiNote { -1 };
        int controlCountdown { 0 };
        std::uint64_t ageStamp { 0 };
        std::array<std::uint32_t, NeuralModel::airBandCount> airNoiseStates {};
        std::array<std::uint32_t, NeuralModel::airBandCount> airSideNoiseStates {};
        float velocity { 0.0f };
        float velocityGain { 0.0f };
        float envelope { 0.0f };
        double modelTimeSeconds { 0.0 };
        std::uint64_t renderedSampleCount { 0 };
        float mutationOffset { 0.0f };
        float latentPhaseA { 0.0f };
        float latentPhaseB { 0.0f };
        float latentRateAHertz { 0.0f };
        float latentRateBHertz { 0.0f };
        float cachedBrightnessTilt { -1000.0f };
        float cachedReferenceTilt { -1000.0f };
        // Covers the whole rendered bank so the control-rate target loop can
        // factor pow(index * scale, tilt) into one cached table lookup and one
        // scalar instead of a pow() per rendered harmonic.
        std::array<float, renderedHarmonicCount> brightnessTiltTable {};
        std::array<float, NeuralModel::harmonicCount> referenceTiltTable {};
        std::array<float, NeuralModel::harmonicCount> harmonicVariationSin {};
        std::array<float, NeuralModel::airBandCount> airVariationSin {};
        float pan { 0.0f };
        float panLeft { 0.70710678f };
        float panRight { 0.70710678f };
        float panLeftStep { 0.0f };
        float panRightStep { 0.0f };
        float airSideGain { 0.0f };
        float airSideGainStep { 0.0f };
        float transpositionRatio { 1.0f };
        float pitchRatio { 1.0f };
        float pitchRatioStep { 0.0f };
        float lastLeft { 0.0f };
        float lastRight { 0.0f };
        std::size_t activeHarmonicCount { NeuralModel::harmonicCount };
        std::size_t targetHarmonicCount { NeuralModel::harmonicCount };
        // Harmonics at or above this index are silent now and stay silent for
        // the whole coming control period, so the audio loop skips them
        // entirely instead of advancing a phase nothing reads.
        std::size_t soundingHarmonicCount { 0 };
        std::array<float, renderedHarmonicCount> harmonicPhases {};
        std::array<float, NeuralModel::boneModeCount> bonePhases {};
        std::array<float, NeuralModel::boneModeCount> boneFrequenciesHz {};
        std::array<float, NeuralModel::boneModeCount> boneFrequencySteps {};
        std::array<float, renderAmplitudeCount> amplitudes {};
        std::array<float, renderAmplitudeCount> amplitudeSteps {};
        std::array<Bandpass, NeuralModel::airBandCount> airFilters {};

        void clear() noexcept;
    };

    // A stolen voice leaves its last sample behind, so the tail is a step to be
    // removed rather than a signal to be faded. A raised cosine has zero slope
    // at both ends, which keeps that removal from adding the click it exists to
    // prevent; a linear ramp corners at both ends instead.
    struct FadeTail
    {
        float left { 0.0f };
        float right { 0.0f };
        float position { 0.0f };
        float positionStep { 0.0f };
        int remaining { 0 };

        void clear() noexcept { *this = FadeTail {}; }
    };

    [[nodiscard]] EngineParameters loadParameters() const noexcept;
    [[nodiscard]] float sine(float phase) const noexcept;
    // Table lookup for a phase already reduced to [0, 1). The two wrapped
    // guard entries make the top cell and the rounded-up edge case safe.
    [[nodiscard]] float sineUnit(float unitPhase) const noexcept
    {
        const float tablePosition = unitPhase
            * static_cast<float>(sineTableSize);
        const auto lower = static_cast<std::size_t>(tablePosition);
        const float fraction = tablePosition - static_cast<float>(lower);
        return sineTable_[lower]
            + fraction * (sineTable_[lower + 1] - sineTable_[lower]);
    }
    void refreshHarmonicStretch(float inharmonicity) noexcept;
    void updateVoiceControl(Voice& voice, const NeuralModel& model,
                            const EngineParameters& parameters) noexcept;
    [[nodiscard]] static float nextNoise(std::uint32_t& state) noexcept;
    void beginFadeTail(std::size_t voiceIndex) noexcept;
    void refreshVoicePans() noexcept;

    std::atomic<const NeuralModel*> model_ { nullptr };
    AtomicParameters parameters_ {};
    std::array<Voice, maximumVoices> voices_ {};
    std::array<FadeTail, maximumVoices> fadeTails_ {};
    std::array<float, sineTableSize + 2> sineTable_ {};
    std::array<float, NeuralModel::harmonicCount> inverseHarmonicRolloff_ {};
    // Rendered partial frequency divided by the fundamental. Identity for an
    // ideal harmonic series; rebuilt only when the effective stiff-string
    // coefficient actually changes, never per voice and never per sample.
    std::array<float, renderedHarmonicCount> harmonicStretchRatio_ {};
    float cachedInharmonicity_ { -1.0f };
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    // Core anti-alias fade constants for coreNyquistGain(), precomputed so the
    // per-sample harmonic loop needs no division.
    float coreNyquistLimitHz_ { 0.49f * 48000.0f };
    float coreNyquistFadeScale_ { 1.0f / (0.06f * 48000.0f) };
    int controlPeriod_ { 192 };
    // Output level is the one control applied straight to the summed signal
    // rather than through a control-rate target, so it carries its own
    // smoother. The sentinel means "not yet primed": the first block after
    // prepare() adopts the host's value instead of sliding up to it.
    float smoothedOutputGain_ { -1.0f };
    float outputGainCoefficient_ { 1.0f };
    std::uint64_t ageCounter_ { 0 };
};

} // namespace neuramar
