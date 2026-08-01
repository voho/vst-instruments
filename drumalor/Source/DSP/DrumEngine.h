#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace drumalor
{

enum class Instrument : std::uint8_t
{
    Kick,
    Snare,
    Clap,
    ClosedHat,
    OpenHat,
    Ride,
    Crash,
    LowTom,
    MidTom,
    HighTom,
    Shaker,
    Perc1,
    Perc2,
    Count
};

inline constexpr std::size_t instrumentCount = static_cast<std::size_t> (Instrument::Count);
inline constexpr double maximumTailSeconds = 8.0;
// Choke/mute groups A, B and C. Group 0 means the voice never chokes anything.
inline constexpr int chokeGroupCount = 3;
inline constexpr float minimumVoiceLevelDecibels = -24.0f;
inline constexpr float maximumVoiceLevelDecibels = 6.0f;

struct InstrumentParameters
{
    float characterA { 0.5f };
    float characterB { 0.5f };
    float pitch { 0.0f };
    float decay { 0.5f };
    // Per-voice mixer. Level is in decibels so its unity default is exact;
    // pan is the usual -1 (left) to +1 (right) constant-power position.
    float level { 0.0f };
    float pan { 0.0f };
    // 0 = no group, 1..chokeGroupCount = mute group A..C.
    int chokeGroup { 0 };
};

// Kit-wide controls that shape every voice and the shared output bus.
struct KitParameters
{
    // 0.5 reproduces the original fixed analogue drift depth exactly; 0 makes
    // the kit machine-tight and 1.0 doubles the per-hit variation.
    float humanise { 0.5f };
    // Both bus stages are fully bypassed at 0.
    float busDrive { 0.0f };
    float busCompression { 0.0f };
};

struct InstrumentMetadata
{
    Instrument instrument {};
    std::string_view displayName;
    std::string_view slug;
    int standardMidiNote { 36 };
    std::string_view characterALabel;
    std::string_view characterBLabel;
    InstrumentParameters defaultParameters {};
};

[[nodiscard]] const InstrumentMetadata& getInstrumentMetadata (Instrument instrument) noexcept;
[[nodiscard]] std::string_view getInstrumentDisplayName (Instrument instrument) noexcept;
[[nodiscard]] std::string_view getInstrumentSlug (Instrument instrument) noexcept;
[[nodiscard]] int getStandardMidiNote (Instrument instrument) noexcept;
[[nodiscard]] std::string_view getCharacterALabel (Instrument instrument) noexcept;
[[nodiscard]] std::string_view getCharacterBLabel (Instrument instrument) noexcept;
[[nodiscard]] std::optional<Instrument> instrumentForMidiNote (int midiNote) noexcept;

class DrumEngine
{
public:
    DrumEngine() noexcept;

    void prepare (double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void setInstrumentParameters (Instrument instrument,
                                  const InstrumentParameters& parameters) noexcept;
    void setKitParameters (const KitParameters& parameters) noexcept;
    void setOutputGain (float linearGain) noexcept;
    void trigger (Instrument instrument, float velocity) noexcept;
    [[nodiscard]] bool triggerMidi (int midiNote, float velocity) noexcept;
    void allSoundsOff() noexcept;
    void process (float* left, float* right, int numSamples) noexcept;
    // Includes short fade-only tails retained to make voice stealing click-free.
    [[nodiscard]] int getActiveVoiceCount() const noexcept;

    // Metering, published once per processed chunk for the editor. All three
    // are already ballistically smoothed inside the engine so a UI that polls
    // slower than the block rate cannot miss a transient.
    [[nodiscard]] float getOutputLevel (int channel) const noexcept;
    [[nodiscard]] float getInstrumentLevel (Instrument instrument) const noexcept;
    // Linear gain currently applied by the bus compressor; 1.0 means no
    // reduction and the stage may be bypassed entirely.
    [[nodiscard]] float getBusGain() const noexcept;

private:
    static constexpr int maxVoices = 64;
    static constexpr int retiringVoiceCount = maxVoices;
    static constexpr int oscillatorCount = 8;
    static constexpr int resonatorCount = 12;
    static constexpr int metallicBankCount = 5;
    static constexpr int metallicOscillatorCount = 6;
    static constexpr int maximumMetallicDecimatorTaps = 401;
    static constexpr int sineTableSize = 2048;
    static constexpr int sineTableMask = sineTableSize - 1;

    struct AtomicInstrumentParameters
    {
        std::atomic<float> characterA { 0.5f };
        std::atomic<float> characterB { 0.5f };
        std::atomic<float> pitch { 0.0f };
        std::atomic<float> decay { 0.5f };
        std::atomic<float> level { 0.0f };
        std::atomic<float> pan { 0.0f };
        std::atomic<int> chokeGroup { 0 };
    };

    struct Biquad
    {
        float b0 { 1.0f };
        float b1 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float z1 { 0.0f };
        float z2 { 0.0f };

        [[nodiscard]] float tick (float input) noexcept;
        void clear() noexcept;
    };

    struct Resonator
    {
        float inputGain { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float y1 { 0.0f };
        float y2 { 0.0f };

        [[nodiscard]] float tick (float input) noexcept;
        void clear() noexcept;
    };

    struct HitVariation
    {
        float pitchCents { 0.0f };
        float decayScale { 1.0f };
        float characterAOffset { 0.0f };
        float characterBOffset { 0.0f };
        float transientScale { 1.0f };
        float circuitDriveOffset { 0.0f };
        float circuitBias { 0.0f };
        float phaseOffset { 0.0f };
    };

    struct Voice
    {
        bool active { false };
        bool choking { false };
        bool bandLimitedNoiseReady { false };
        Instrument instrument { Instrument::Kick };
        int chokeGroup { 0 };
        std::uint64_t generation { 0 };
        std::uint64_t ageSamples { 0 };
        std::uint64_t maximumSamples { 0 };
        std::uint64_t minimumSilenceSamples { 0 };
        // Age after which the modal bank has rung down past -150 dB and can be
        // skipped. Derived from the voice's own decay, so it stays identical at
        // every sample rate and under every host block partitioning.
        std::uint64_t modalActiveSamples { 0 };
        std::uint32_t noiseState { 1u };
        std::uint32_t quietSamples { 0u };
        float velocity { 0.0f };
        float characterA { 0.5f };
        float characterB { 0.5f };
        float pitchRatio { 1.0f };
        float decaySeconds { 0.5f };
        float envelope { 1.0f };
        float envelopeMultiplier { 0.999f };
        float auxiliaryEnvelope { 1.0f };
        float auxiliaryMultiplier { 0.999f };
        float transientEnvelope { 1.0f };
        float transientMultiplier { 0.99f };
        float pitchEnvelope { 1.0f };
        float pitchEnvelopeMultiplier { 0.99f };
        float transientScale { 1.0f };
        float excitationScale { 1.0f };
        // Softer strikes excite fewer high partials on a real drum. This scales
        // the struck-timbre filters and stick content, not just the VCA gain.
        float velocityTimbre { 1.0f };
        float levelGain { 1.0f };
        float circuitDrive { 1.2f };
        float circuitBias { 0.0f };
        // Transfer curve of the voice's output stage. Both curvatures, the
        // quiescent operating point and the makeup gain follow only from the
        // instrument and characterB, so they are resolved once at note-on
        // instead of being rebuilt for every sample of the voice.
        float analogPositiveCurvature { 0.205f };
        float analogNegativeCurvature { 0.165f };
        float analogMakeup { 1.0f };
        float analogZero { 0.0f };
        float analogPreviousInput { 0.0f };
        // Antiderivative of the transfer curve at analogPreviousInput. Carrying
        // it forward halves the transcendental count of the ADAA stage.
        float analogPreviousPrimitive { 0.0f };
        float supplySag { 0.0f };
        float kickStateX { 0.0f };
        float kickStateY { 0.0f };
        float kickCharge { 0.0f };
        float kickChargeMultiplier { 0.0f };
        float kickBaseRadius { 0.0f };
        float chokeGain { 1.0f };
        float chokeMultiplier { 1.0f };
        float recentPeak { 0.0f };
        float bandLimitedNoiseCurrent { 0.0f };
        float bandLimitedNoiseNext { 0.0f };
        float bandLimitedNoisePhase { 0.0f };
        float cymbalClockPhase { 1.0f };
        float cymbalPcmValue { 0.0f };
        float cymbalPcmReconstructed { 0.0f };
        float baseFrequency { 100.0f };
        float sweepAmount { 0.0f };
        float panLeft { 0.70710678f };
        float panRight { 0.70710678f };
        std::array<float, oscillatorCount> phases {};
        std::array<float, oscillatorCount> phaseIncrements {};
        std::array<float, oscillatorCount> oscillatorAsymmetries {};
        std::array<float, resonatorCount> modeGains {};
        std::array<std::uint64_t, 4> burstStarts {};
        std::array<Resonator, resonatorCount> resonators {};
        Biquad filterA {};
        Biquad filterB {};
        Biquad filterC {};
    };

    struct RelaxationOscillatorBank
    {
        Instrument instrument { Instrument::ClosedHat };
        int activeOscillators { metallicOscillatorCount };
        // False for the ride and crash banks, whose mix reads only the Schmitt
        // pulses, so their RC integrators can be skipped entirely.
        bool usesCapacitors { true };
        float characterA { 0.5f };
        float output { 0.0f };
        float lastParameterPitch { 0.0f };
        float lastParameterCharacterA { 0.5f };
        std::array<float, metallicOscillatorCount> phases {};
        std::array<float, metallicOscillatorCount> currentIncrements {};
        std::array<float, metallicOscillatorCount> targetIncrements {};
        std::array<float, metallicOscillatorCount> dutyCycles {};
        std::array<float, metallicOscillatorCount> thresholds {};
        std::array<float, metallicOscillatorCount> capacitorStates {};
        std::array<float, metallicOscillatorCount> riseCoefficients {};
        std::array<float, metallicOscillatorCount> fallCoefficients {};
        std::array<float, metallicOscillatorCount> fixedTolerances {};
        std::array<float, maximumMetallicDecimatorTaps> decimatorHistory {};
        int decimatorWriteIndex { 0 };
        // Samples this bank's circuit has been skipped because no voice could
        // observe it. Restored analytically by wakeMetallicOscillatorBank().
        std::uint64_t frozenSamples { 0 };
    };

    [[nodiscard]] static bool validInstrument (Instrument instrument) noexcept;
    [[nodiscard]] static std::size_t indexFor (Instrument instrument) noexcept;
    [[nodiscard]] InstrumentParameters snapshotParameters (Instrument instrument) const noexcept;
    [[nodiscard]] float decaySecondsFor (Instrument instrument, float normalizedDecay) const noexcept;
    [[nodiscard]] int findVoiceSlot() const noexcept;
    void initialiseVoice (Voice& voice, Instrument instrument, float velocity,
                          const InstrumentParameters& parameters, std::uint32_t seed,
                          const HitVariation& variation) noexcept;
    void initialiseModalVoice (Voice& voice, const float* ratios, int modeCount,
                               float baseFrequency, float decaySeconds,
                               float spread, float brightness) noexcept;
    void chokeGroup (int group) noexcept;
    void beginChoke (Voice& voice, float seconds) noexcept;
    void beginFadeToSilence (Voice& voice, float multiplier) noexcept;
    void retireVoice (const Voice& voice) noexcept;
    void silenceVoice (Voice& voice) noexcept;
    void addBankReference (Instrument instrument) noexcept;
    void releaseBankReference (Instrument instrument) noexcept;
    void updateActiveVoiceCount() noexcept;
    void applyBusStage (float& left, float& right, float driveAmount,
                        float compressionAmount) noexcept;
    void resetBusStage() noexcept;

    [[nodiscard]] float renderVoice (Voice& voice) noexcept;
    [[nodiscard]] float renderKick (Voice& voice) noexcept;
    [[nodiscard]] float renderSnare (Voice& voice) noexcept;
    [[nodiscard]] float renderClap (Voice& voice) noexcept;
    [[nodiscard]] float renderHat (Voice& voice) noexcept;
    [[nodiscard]] float renderRide (Voice& voice) noexcept;
    [[nodiscard]] float renderCrash (Voice& voice) noexcept;
    [[nodiscard]] float renderTom (Voice& voice) noexcept;
    [[nodiscard]] float renderShaker (Voice& voice) noexcept;
    [[nodiscard]] float renderPerc1 (Voice& voice) noexcept;
    [[nodiscard]] float renderPerc2 (Voice& voice) noexcept;

    [[nodiscard]] float oscillator (Voice& voice, int oscillatorIndex) const noexcept;
    void resetMetallicOscillatorBanks() noexcept;
    void wakeMetallicOscillatorBank (RelaxationOscillatorBank& bank) noexcept;
    void wakeMetallicOscillatorBankFor (Instrument instrument) noexcept;
    void configureMetallicDecimator() noexcept;
    void updateMetallicBankParameterTargets() noexcept;
    void configureMetallicOscillatorBank (Instrument instrument, float pitchRatio,
                                          float characterA, bool snap) noexcept;
    void renderMetallicOscillatorBanks (std::uint32_t activeBankMask) noexcept;
    [[nodiscard]] float renderMetallicBankSubstep (
        RelaxationOscillatorBank& bank) noexcept;
    [[nodiscard]] float decimateMetallicBank (
        const RelaxationOscillatorBank& bank) const noexcept;
    [[nodiscard]] float metallicSourceFor (Instrument instrument) const noexcept;
    [[nodiscard]] static int metallicBankIndexFor (Instrument instrument) noexcept;
    [[nodiscard]] float nextCymbalPcm (Voice& voice, float source) const noexcept;
    void sineAndCosineLookup (float phase, float& sine, float& cosine) const noexcept;
    [[nodiscard]] static float nextNoise (Voice& voice) noexcept;
    [[nodiscard]] float nextBandLimitedNoise (Voice& voice) const noexcept;
    [[nodiscard]] static std::uint32_t hash32 (std::uint32_t value) noexcept;
    [[nodiscard]] static float signedUnitFromHash (std::uint32_t value) noexcept;
    [[nodiscard]] float applyAnalogOutputStage (Voice& voice, float input) const noexcept;
    void configureHighpass (Biquad& filter, float frequency, float q) const noexcept;
    void configureBandpass (Biquad& filter, float frequency, float q) const noexcept;
    void configureResonator (Resonator& resonator, float frequency,
                             float decaySeconds) const noexcept;

    // Level and Pan are channel-strip controls rather than per-hit properties,
    // so their current values are republished each block and applied to voices
    // that are already ringing.
    struct MixerTarget
    {
        float levelGain { 1.0f };
        float panLeft { 0.7071f };
        float panRight { 0.7071f };
    };
    std::array<MixerTarget, instrumentCount> mixerTargets_ {};
    std::array<AtomicInstrumentParameters, instrumentCount> parameters_ {};
    std::array<std::uint64_t, instrumentCount> triggerCounters_ {};
    std::array<float, instrumentCount> componentDrift_ {};
    std::array<Voice, maxVoices> voices_ {};
    std::array<Voice, retiringVoiceCount> retiringVoices_ {};
    std::array<RelaxationOscillatorBank, metallicBankCount> metallicBanks_ {};
    std::array<int, metallicBankCount> metallicBankVoiceCounts_ {};
    std::array<float, maximumMetallicDecimatorTaps> metallicDecimatorCoefficients_ {};
    std::array<float, sineTableSize> sineTable_ {};
    std::array<std::atomic<float>, instrumentCount> instrumentLevels_ {};

    std::atomic<float> outputGain_ { 0.82f };
    std::atomic<float> humanise_ { 0.5f };
    std::atomic<float> busDrive_ { 0.0f };
    std::atomic<float> busCompression_ { 0.0f };
    std::atomic<float> outputLevelLeft_ { 0.0f };
    std::atomic<float> outputLevelRight_ { 0.0f };
    std::atomic<float> busGainMeter_ { 1.0f };
    std::atomic<int> activeVoiceCount_ { 0 };
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    int maxBlockSize_ { 512 };
    bool prepared_ { false };
    bool anyVoiceActive_ { false };
    std::uint32_t metallicBankMask_ { 0u };
    std::uint64_t generation_ { 0 };
    std::uint64_t maximumVoiceSamples_ { 384000 };
    std::uint64_t forcedFadeStartSamples_ { 383760 };
    std::uint32_t naturalQuietHoldSamples_ { 2160u };
    float peakReleaseMultiplier_ { 0.999f };
    float retirementFadeMultiplier_ { 0.999f };
    float forcedFadeMultiplier_ { 0.999f };
    float sagAttackCoefficient_ { 0.01f };
    float sagReleaseCoefficient_ { 0.001f };
    float gainSmoothingCoefficient_ { 0.001f };
    float dcBlockerCoefficient_ { 0.9984f };
    float modalNoiseScale_ { 1.0f };
    float bandLimitedNoiseIncrement_ { 1.0f };
    float metallicInternalSampleRate_ { 192000.0f };
    float metallicInverseSampleRate_ { 1.0f / 192000.0f };
    float metallicIncrementSmoothing_ { 0.01f };
    float cymbalClockIncrement_ { 0.625f };
    float cymbalReconstructionCoefficient_ { 0.8f };
    int metallicOversampleFactor_ { 4 };
    int metallicDecimatorTapCount_ { 257 };
    float smoothedOutputGain_ { 0.82f };
    // Both bus controls are ramped at the master gain's 20 ms constant instead
    // of stepping once per block, which used to click on automation.
    float smoothedBusDrive_ { 0.0f };
    float smoothedBusCompression_ { 0.0f };
    float dcInputLeft_ { 0.0f };
    float dcInputRight_ { 0.0f };
    float dcOutputLeft_ { 0.0f };
    float dcOutputRight_ { 0.0f };
    float masterAdaaPreviousLeft_ { 0.0f };
    float masterAdaaPreviousRight_ { 0.0f };
    float masterAdaaPrimitiveLeft_ { 0.0f };
    float masterAdaaPrimitiveRight_ { 0.0f };
    float busEnvelope_ { 0.0f };
    float busGain_ { 1.0f };
    float busDriveAdaaLeft_ { 0.0f };
    float busDriveAdaaRight_ { 0.0f };
    float busAttackCoefficient_ { 0.05f };
    float busReleaseCoefficient_ { 0.002f };
    float meterPeakLeft_ { 0.0f };
    float meterPeakRight_ { 0.0f };
};

} // namespace drumalor
