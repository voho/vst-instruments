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

struct InstrumentParameters
{
    float characterA { 0.5f };
    float characterB { 0.5f };
    float pitch { 0.0f };
    float decay { 0.5f };
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
    void setOutputGain (float linearGain) noexcept;
    void trigger (Instrument instrument, float velocity) noexcept;
    [[nodiscard]] bool triggerMidi (int midiNote, float velocity) noexcept;
    void allSoundsOff() noexcept;
    void process (float* left, float* right, int numSamples) noexcept;
    [[nodiscard]] int getActiveVoiceCount() const noexcept;

private:
    static constexpr int maxVoices = 64;
    static constexpr int retiringVoiceCount = maxVoices;
    static constexpr int oscillatorCount = 8;
    static constexpr int resonatorCount = 12;
    static constexpr int sineTableSize = 2048;
    static constexpr int sineTableMask = sineTableSize - 1;

    struct AtomicInstrumentParameters
    {
        std::atomic<float> characterA { 0.5f };
        std::atomic<float> characterB { 0.5f };
        std::atomic<float> pitch { 0.0f };
        std::atomic<float> decay { 0.5f };
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
        bool modalNoiseReady { false };
        Instrument instrument { Instrument::Kick };
        std::uint64_t generation { 0 };
        std::uint64_t ageSamples { 0 };
        std::uint64_t maximumSamples { 0 };
        std::uint64_t minimumSilenceSamples { 0 };
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
        float circuitDrive { 1.2f };
        float circuitBias { 0.0f };
        float analogPreviousInput { 0.0f };
        float supplySag { 0.0f };
        float kickStateX { 0.0f };
        float kickStateY { 0.0f };
        float kickCharge { 0.0f };
        float kickChargeMultiplier { 0.0f };
        float kickBaseRadius { 0.0f };
        float chokeGain { 1.0f };
        float chokeMultiplier { 1.0f };
        float recentPeak { 0.0f };
        float lastOutput { 0.0f };
        float modalNoiseCurrent { 0.0f };
        float modalNoiseNext { 0.0f };
        float modalNoisePhase { 0.0f };
        float cymbalClockPhase { 1.0f };
        float cymbalPcmValue { 0.0f };
        float baseFrequency { 100.0f };
        float sweepAmount { 0.0f };
        float panLeft { 0.70710678f };
        float panRight { 0.70710678f };
        std::array<float, oscillatorCount> phases {};
        std::array<float, oscillatorCount> phaseIncrements {};
        std::array<float, resonatorCount> modeGains {};
        std::array<std::uint64_t, 4> burstStarts {};
        std::array<Resonator, resonatorCount> resonators {};
        Biquad filterA {};
        Biquad filterB {};
        Biquad filterC {};
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
    void chokeHats() noexcept;
    void beginChoke (Voice& voice, float seconds) noexcept;
    void beginFadeToSilence (Voice& voice, float multiplier) noexcept;
    void retireVoice (const Voice& voice) noexcept;
    void silenceVoice (Voice& voice) noexcept;
    void updateActiveVoiceCount() noexcept;

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
    [[nodiscard]] float bandLimitedPulse (Voice& voice, int oscillatorIndex) const noexcept;
    [[nodiscard]] float cymbalOscillatorBank (Voice& voice) const noexcept;
    [[nodiscard]] float nextCymbalPcm (Voice& voice, float source) const noexcept;
    [[nodiscard]] float sineLookup (float phase) const noexcept;
    [[nodiscard]] static float nextNoise (Voice& voice) noexcept;
    [[nodiscard]] float nextModalNoise (Voice& voice) const noexcept;
    [[nodiscard]] static std::uint32_t hash32 (std::uint32_t value) noexcept;
    [[nodiscard]] static float signedUnitFromHash (std::uint32_t value) noexcept;
    [[nodiscard]] float applyAnalogOutputStage (Voice& voice, float input) const noexcept;
    void configureLowpass (Biquad& filter, float frequency, float q) const noexcept;
    void configureHighpass (Biquad& filter, float frequency, float q) const noexcept;
    void configureBandpass (Biquad& filter, float frequency, float q) const noexcept;
    void configureResonator (Resonator& resonator, float frequency,
                             float decaySeconds) const noexcept;

    std::array<AtomicInstrumentParameters, instrumentCount> parameters_ {};
    std::array<std::uint64_t, instrumentCount> triggerCounters_ {};
    std::array<float, instrumentCount> componentDrift_ {};
    std::array<Voice, maxVoices> voices_ {};
    std::array<Voice, retiringVoiceCount> retiringVoices_ {};
    std::array<float, sineTableSize> sineTable_ {};

    std::atomic<float> outputGain_ { 0.82f };
    std::atomic<int> activeVoiceCount_ { 0 };
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    int maxBlockSize_ { 512 };
    bool prepared_ { false };
    std::uint64_t generation_ { 0 };
    std::uint64_t maximumVoiceSamples_ { 384000 };
    std::uint64_t forcedFadeStartSamples_ { 383760 };
    std::uint32_t naturalQuietHoldSamples_ { 2160u };
    float peakReleaseMultiplier_ { 0.999f };
    float retirementFadeMultiplier_ { 0.999f };
    float forcedFadeMultiplier_ { 0.999f };
    float sagAttackCoefficient_ { 0.01f };
    float sagReleaseCoefficient_ { 0.001f };
    float modalNoiseScale_ { 1.0f };
    float modalNoisePhaseIncrement_ { 1.0f };
    float smoothedOutputGain_ { 0.82f };
    float dcInputLeft_ { 0.0f };
    float dcInputRight_ { 0.0f };
    float dcOutputLeft_ { 0.0f };
    float dcOutputRight_ { 0.0f };
    float masterAdaaPreviousLeft_ { 0.0f };
    float masterAdaaPreviousRight_ { 0.0f };
};

} // namespace drumalor
