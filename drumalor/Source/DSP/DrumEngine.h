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

    // The 909's cymbal ROMs. On the machine these are mask ROMs holding a
    // recorded cymbal, and every hit reads the same data - so they are engine
    // state built once, not per-voice state. Drumalor generates the contents
    // rather than embedding a recording, but what the counter walks is a fixed
    // table either way.
    //
    // The table is walked cyclically and its contents are built to loop
    // seamlessly, which is the one place this departs from the hardware: a
    // real ROM is finite and the counter stops at its end. Here the address
    // envelope is what ends the sound, so the source has to keep going until
    // it does.
    static constexpr int cymbalRomSize = 32768;
    static constexpr int cymbalRomMask = cymbalRomSize - 1;

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
        // sin(omega)/radius: what a unit strike has to put into the state for
        // the mode to answer with an amplitude of exactly one.
        float strikeGain { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
        float y1 { 0.0f };
        float y2 { 0.0f };

        [[nodiscard]] float tick (float input) noexcept;
        // Set a mode in motion rather than pushing a sample through it. A
        // struck mode starts at rest and answers A r^n sin(n omega): zero at the
        // instant of contact, rising to A a quarter period later. Driving the
        // same impulse through inputGain instead lands A times the input gain on
        // the very first sample, and that gain is proportional to sin(omega) -
        // six times larger at 8 kHz than at 48 for the same mode, which a bank
        // of twelve modes struck together turns into an audible difference.
        void strike (float amplitude) noexcept;
        void clear() noexcept;
    };

    // One cymbal channel, laid out the way the two machines that made this
    // sound actually lay out: a TR-808 analogue channel and a TR-909 digital
    // one, summed at a single buffer amplifier.
    //
    // The 808 half is the classic block chain - six Schmitt-trigger inverter
    // oscillators summed at a virtual earth, two active band-passes on that
    // node, a trigger pulse through an attack smoother into two envelope
    // generators, three swing-type VCAs, a Sallen-Key high-pass on each band,
    // and a tone-control mixer.
    //
    // The 909 half is a counter walking a ROM into a companded 6-bit DAC, a
    // second DAC on the address lines through an anti-log converter to make
    // the envelope, the VCA that restores it, and the low-pass that removes
    // the sample clock. Drumalor generates the ROM contents rather than
    // embedding any recording.
    struct CymbalChannel
    {
        // 808: the two band-passes hanging off the oscillator summing node.
        Biquad bandLow {};
        Biquad bandHigh {};
        // 808: one Sallen-Key high-pass per band, ahead of the tone mixer.
        Biquad highpassLow {};
        Biquad highpassMid {};
        Biquad highpassHigh {};
        // 909: the reconstruction filter after the DAC and its VCA.
        Biquad reconstruction {};

        // 808 trigger path. The trigger pulse is not a step: it charges the
        // envelope capacitors through the attack smoother, so every band opens
        // over a short ramp rather than switching on.
        float gate { 0.0f };
        float gateCoefficient { 1.0f };
        float peak { 1.0f };
        // 808 swing VCAs: the control-voltage knee and the control leakage
        // that a steered pair puts through even with no signal.
        float vcaKnee { 0.22f };
        float feedthrough { 0.0f };
        // 808 tone-control mixer weights for the three high-passed bands.
        float lowGain { 0.0f };
        float midGain { 0.0f };
        float highGain { 0.0f };
        // Ages past which the mid and high VCAs are shut so far that their
        // bands cannot reach even -150 dB, so the sections feeding them are
        // skipped. Derived from the voice's own decay, so the boundary lands
        // at the same instant at every sample rate and under every host block
        // partitioning. The low band carries the DECAY control and runs for as
        // long as the voice does.
        std::uint64_t midActiveSamples { 0 };
        std::uint64_t highActiveSamples { 0 };

        // 909 sample clock, as a phase increment per engine sample.
        float clockPhase { 1.0f };
        float clockIncrement { 0.625f };
        // The DAC's held output and the source it was quantized from.
        float hold { 0.0f };
        // The address-line DAC's anti-log envelope and its per-clock ratio.
        // Because it steps with the counter rather than with time, retuning
        // the clock moves pitch and tail length together, exactly as the
        // machine does.
        float romEnvelope { 1.0f };
        float romDecay { 1.0f };
        // An OTA's transconductance sets its bandwidth as well as its gain, so
        // a channel closing down loses its top before it loses its level. This
        // is the pole that moves with the control current; it is retuned once
        // per sample clock rather than once per sample.
        float vcaBandwidthState { 0.0f };
        float vcaBandwidthCoefficient { 1.0f };
        float vcaBandwidthOpen { 1.0f };
        // What the counter is reading, and where it has got to. The ROM is
        // engine-owned and shared by every voice; the address is per-voice
        // because two crashes overlapping are two counters walking the same
        // mask at their own rates, which is exactly what the machine does.
        const float* rom { nullptr };
        float romPhase { 0.0f };
        // The tone mixer is the last stage before the buffer amplifier, so the
        // digital channel passes through it too. It has no three analogue legs
        // of its own, so it is split by a single first-order crossover and
        // weighted by the same control.
        float pcmSplitState { 0.0f };
        float pcmSplitCoefficient { 0.3f };
        float pcmLowGain { 0.0f };
        float pcmHighGain { 0.0f };
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
        float kickBaseRadius { 0.0f };
        // How long the beater or the stick stays on the head. Hertzian: it
        // shortens as the strike gets harder, and everything the strike can
        // reach follows from it. The phase runs from 0 to 1 across the contact
        // and then stops; the increment is one sample's worth of it.
        float contactSeconds { 0.001f };
        float contactPhase { 1.0f };
        float contactIncrement { 1.0f };
        float chokeGain { 1.0f };
        float chokeMultiplier { 1.0f };
        float recentPeak { 0.0f };
        float bandLimitedNoiseCurrent { 0.0f };
        float bandLimitedNoiseNext { 0.0f };
        float bandLimitedNoisePhase { 0.0f };
        float baseFrequency { 100.0f };
        float sweepAmount { 0.0f };
        float panLeft { 0.70710678f };
        float panRight { 0.70710678f };
        std::array<float, oscillatorCount> phases {};
        std::array<float, oscillatorCount> phaseIncrements {};
        std::array<float, oscillatorCount> oscillatorAsymmetries {};
        // How many of the bank's slots this voice actually filled. The render
        // loops used to carry the count as a literal, which is how the toms
        // ended up ringing five modes out of twelve.
        int modeCount { 0 };
        std::array<float, resonatorCount> modeGains {};
        std::array<std::uint64_t, 4> burstStarts {};
        std::array<Resonator, resonatorCount> resonators {};
        Biquad filterA {};
        Biquad filterB {};
        Biquad filterC {};
        // Only the Ride and Crash use this; every other voice leaves it at its
        // reset state and never ticks it.
        CymbalChannel cymbal {};
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

    // How a struck object's damping rises with frequency. Every real material
    // loses more per cycle as the mode goes up, but by laws that differ enough
    // to be the difference between materials: a stretched film loses to
    // hysteresis in the plastic and to the air it pushes, so its damping climbs
    // steeply; cast bronze has almost no internal loss at all, which is why a
    // cymbal rings for seconds where a drumhead is gone in one.
    //
    // The three shares are normalised to sum to one at the fundamental, so
    // asking for a decay time always gets exactly that decay time and only the
    // modes above it are shortened.
    struct ModalLoss
    {
        float fixed { 1.0f };       // frequency-independent
        float hysteretic { 0.0f };  // rises as omega: a constant loss angle
        float viscous { 0.0f };     // rises as omega squared: rate-of-strain
        // Sound that actually leaves. Only the head bank uses it, because only
        // there does the engine know each mode's multipole order - and it is
        // the term that decides which modes are loud and which ones last, since
        // for a drum those are opposite questions.
        float radiation { 0.0f };
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
                               float spread, float brightness,
                               ModalLoss loss,
                               const float* excitation = nullptr) noexcept;

    // Everything a two-headed drum is, above its fundamental. Geometry that
    // belongs to the instrument rather than to the hit.
    struct HeadGeometry
    {
        // How much of the air the head drags reaches it. The load itself
        // follows from the drum: a wide head against a light film carries far
        // more air than a small one, and it falls off for the finer modes,
        // which move less air per unit area. That is what pushes a real head's
        // overtones above the ideal Bessel ratios rather than below them.
        float airLoadScale { 1.0f };
        float radius { 0.20f };       // m, the head itself
        float strikeRadius { 0.22f }; // where the stick lands, as a fraction of a
        float headDensity { 0.35f };  // kg/m^2 of the film
        float shellDepth { 0.40f };   // m between the two heads
        float contactSeconds { 0.001f }; // how long the strike is on the head
    };

    int buildHeadBank (Voice& voice, float fundamental, const HeadGeometry& head,
                       float decaySeconds, float brightness, ModalLoss loss) noexcept;
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

    [[nodiscard]] float advanceContact (Voice& voice) noexcept;
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

    // The TR-808 analogue cymbal channel and the TR-909 digital one. Both
    // cymbal voices run both; only the balance and the tuning differ, and all
    // of that is resolved into the voice at note-on.
    void configureCymbalChannel (Voice& voice, Instrument instrument,
                                 float velocity) noexcept;
    [[nodiscard]] static float swingVcaGain (float control, float knee) noexcept;
    struct CymbalBands
    {
        float low { 0.0f };
        float mid { 0.0f };
        float high { 0.0f };
    };
    [[nodiscard]] CymbalBands renderCymbalBands (Voice& voice,
                                                 float source) noexcept;
    [[nodiscard]] float boardDriftAt (std::uint64_t sampleIndex) const noexcept;
    [[nodiscard]] static float companding6BitDac (float value) noexcept;
    [[nodiscard]] float nextCymbalPcm (Voice& voice) const noexcept;
    void buildCymbalRoms() noexcept;
    void sineAndCosineLookup (float phase, float& sine, float& cosine) const noexcept;
    [[nodiscard]] static float nextNoise (Voice& voice) noexcept;
    [[nodiscard]] float nextBandLimitedNoise (Voice& voice) const noexcept;
    [[nodiscard]] static std::uint32_t hash32 (std::uint32_t value) noexcept;
    [[nodiscard]] static float signedUnitFromHash (std::uint32_t value) noexcept;
    [[nodiscard]] float applyAnalogOutputStage (Voice& voice, float input) const noexcept;
    void configureHighpass (Biquad& filter, float frequency, float q) const noexcept;
    void configureBandpass (Biquad& filter, float frequency, float q) const noexcept;
    void configureLowpass (Biquad& filter, float frequency, float q) const noexcept;
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
    // Engine time, in samples since the last reset. Advanced by whole blocks,
    // so it does not depend on how the host divides them.
    std::uint64_t engineSamples_ { 0 };
    std::array<Voice, maxVoices> voices_ {};
    std::array<Voice, retiringVoiceCount> retiringVoices_ {};
    std::array<RelaxationOscillatorBank, metallicBankCount> metallicBanks_ {};
    std::array<int, metallicBankCount> metallicBankVoiceCounts_ {};
    std::array<float, maximumMetallicDecimatorTaps> metallicDecimatorCoefficients_ {};
    std::array<float, sineTableSize> sineTable_ {};
    // Two masks, because the machine has two: the ride and the crash are
    // separate recordings, not one sample at two speeds.
    std::array<float, cymbalRomSize> rideRom_ {};
    std::array<float, cymbalRomSize> crashRom_ {};
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
