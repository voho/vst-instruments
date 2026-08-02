#pragma once

#include "VocalorMath.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace vocalor
{

enum class VoiceProfile { Female, Male };
enum class PerformanceMode { Solo, Choir, Chord };
enum class Vowel { Aah, Ooh, Uuh };
enum class ChordQuality { Major, Minor };

struct EngineParameters
{
    VoiceProfile profile { VoiceProfile::Female };
    PerformanceMode mode { PerformanceMode::Solo };
    Vowel vowel { Vowel::Aah };
    ChordQuality chordQuality { ChordQuality::Major };
    int choirSize { 8 };
    float breath { 0.28f };
    float resonance { 0.72f };
    float vibrato { 0.42f };
    float humanize { 0.55f };
    float spread { 0.65f };
    float tension { 0.48f };
    float room { 0.22f };
    float outputGain { 0.80f };
    // Added in 1.1: continuous vowel space, vocal-tract length, phrasing and
    // room geometry. Every default here reproduces the 1.0 behaviour.
    float vowelX { 0.5f };
    float vowelY { 0.5f };
    float vowelMorph { 0.0f };
    float formantShift { 0.0f };
    float glide { 0.0f };
    bool legato { false };
    float roomSize { 0.5f };
};

/** Lock-free snapshot of what the engine is currently doing, for the editor. */
struct EngineDisplayState
{
    std::array<float, kFormantCount> formantHz {};
    std::array<float, kFormantCount> formantBandwidth {};
    std::array<float, kFormantCount> formantGain {};
    float levelLeft { 0.0f };
    float levelRight { 0.0f };
    float vowelX { 0.5f };
    float vowelY { 0.5f };
    float sampleRate { 48000.0f };
    int activeVoices { 0 };
};

class VoiceEngine
{
public:
    VoiceEngine() noexcept;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    void allSoundOff() noexcept;
    void process(float* left, float* right, int numSamples);
    [[nodiscard]] int getActiveVoiceCount() const;
    [[nodiscard]] EngineDisplayState getDisplayState() const noexcept;

private:
    friend struct VoiceEngineTestAccess;

    static constexpr int tableSize = 2048;
    static constexpr int tableMask = tableSize - 1;
    static constexpr int tableLevels = 9;
    static constexpr int maxHarmonics = 256;
    static constexpr int maxVoices = 96;
    static constexpr int singerCount = 12;
    static constexpr int formantCount = kFormantCount;
    static constexpr int roomBufferSize = 32768;
    static constexpr int controlPeriod = 16;
    static constexpr int heldNoteCapacity = 24;

    // Voices are rendered voice-major over runs of this many samples. Chunk
    // boundaries are aligned to absolute sample positions, so nothing the
    // engine renders depends on how the host splits a buffer.
    static constexpr int chunkSize = 64;

    // Harmonic count per band-limited table level; shared by buildTables()
    // (which fills the tables) and updateVoiceControl() (which picks the
    // aliasing-safe level for the current pitch) so the two stay in sync.
    static constexpr std::array<int, tableLevels> harmonicsPerLevel {
        1, 2, 4, 8, 16, 32, 64, 128, 256
    };

    struct AtomicParameters
    {
        std::atomic<int> profile { 0 };
        std::atomic<int> mode { 0 };
        std::atomic<int> vowel { 0 };
        std::atomic<int> chordQuality { 0 };
        std::atomic<int> choirSize { 8 };
        std::atomic<float> breath { 0.28f };
        std::atomic<float> resonance { 0.72f };
        std::atomic<float> vibrato { 0.42f };
        std::atomic<float> humanize { 0.55f };
        std::atomic<float> spread { 0.65f };
        std::atomic<float> tension { 0.48f };
        std::atomic<float> room { 0.22f };
        std::atomic<float> outputGain { 0.80f };
        std::atomic<float> vowelX { 0.5f };
        std::atomic<float> vowelY { 0.5f };
        std::atomic<float> vowelMorph { 0.0f };
        std::atomic<float> formantShift { 0.0f };
        std::atomic<float> glide { 0.0f };
        std::atomic<int> legato { 0 };
        std::atomic<float> roomSize { 0.5f };
    };

    struct Resonator
    {
        float y1 { 0.0f };
        float y2 { 0.0f };
        float a1 { 0.0f };
        // Unit-peak normalisation, polarity and formant amplitude all folded
        // into one coefficient by the control update. a2 depends only on the
        // bandwidth, which every voice shares, so the caller passes it in.
        float b0 { 0.0f };

        float tick(float input, float a2) noexcept
        {
            const float value = b0 * input + a1 * y1 + a2 * y2;
            y2 = y1;
            y1 = value;
            return value;
        }

        void clear() noexcept { y1 = y2 = 0.0f; }
    };

    struct SineCosine
    {
        float sine { 0.0f };
        float cosine { 1.0f };
    };

    struct SingerIdentity
    {
        float detuneCents { 0.0f };
        float anatomy { 0.0f };
        float pan { 0.0f };
        float onsetOffset { 0.0f };
        float vibratoRate { 5.1f };
        float vibratoDepth { 1.0f };
        // Sampled once per chunk, not once per voice control update: every
        // voice sharing a singer identity reads the same three values.
        float drift { 0.0f };
        float depthDrift { 0.0f };
        float formantDrift { 0.0f };
        float driftIncrement { 0.0f };
        float depthIncrement { 0.0f };
        float formantIncrement { 0.0f };
        // Per-formant dispersion: real ensembles differ in more than a single
        // tract-length scalar, so each singer detunes each formant separately.
        std::array<float, formantCount> formantScale {};
    };

    struct Voice
    {
        bool active { false };
        bool releasing { false };
        bool alternateCycle { false };
        bool controlInitialised { false };
        bool onsetComplete { false };
        int rootMidi { -1 };
        int midiNote { 60 };
        int singer { 0 };
        int tableLevel { 0 };
        int delaySamples { 0 };
        int controlCountdown { 0 };
        std::uint64_t generation { 0 };
        std::uint32_t noiseState { 1u };
        std::uint64_t ageSamples { 0 };
        std::uint64_t lastControlAge { 0 };
        float vibratoPhase { 0.0f };
        float velocity { 0.0f };
        float amplitudeGain { 0.0f };
        float phase { 0.0f };
        float phaseIncrement { 0.0f };
        float targetPhaseIncrement { 0.0f };
        float phaseIncrementStep { 0.0f };
        float envelope { 0.0f };
        float airEnvelope { 0.0f };
        float onsetAir { 1.0f };
        float airShape { 1.0f };
        float pitchScoop { 0.0f };
        float glideCents { 0.0f };
        float jitter { 0.0f };
        float jitterSlow { 0.0f };
        float shimmer { 0.0f };
        float lastNoise { 0.0f };
        float sourceTilt { 0.0f };
        float tiltCoefficient { 1.0f };
        // Vocal effort and pan only move when a parameter does, but their
        // coefficients cost an exp2, an exp and two square roots. Cache the
        // input so a sustained note pays for them once.
        float tiltEffort { -1.0f };
        float panPosition { -2.0f };
        float irregularity { 0.0f };
        float baseFrequency { 261.63f };
        float panLeft { 0.7071f };
        float panRight { 0.7071f };
        float panTargetLeft { 0.7071f };
        float panTargetRight { 0.7071f };
        float onsetMix { 0.0f };
        float onsetMixStep { 1.0f };
        float lastLipSource { 0.0f };
        std::array<float, formantCount> formantHz {};
        std::array<Resonator, formantCount> tract {};
        std::array<Resonator, 2> early {};
    };

    EngineParameters snapshotParameters() const noexcept;
    void buildTables();
    void buildSingerIdentities();
    void initialiseVoice(Voice& voice, int rootMidi, int soundingMidi, int singer,
                         float velocity, float groupGain, int singerTotal,
                         float glideFromCents, const EngineParameters& parameters);
    void updateChunkState(const EngineParameters& parameters, bool advanceSmoothers);
    void updateVoiceControl(Voice& voice, const EngineParameters& parameters);
    void renderVoice(Voice& voice, const EngineParameters& parameters, int count);
    void silenceVoice(Voice& voice) noexcept;
    int voicesForMode(const EngineParameters& parameters) const noexcept;
    int chordMidiForSinger(int rootMidi, int singer, const EngineParameters& parameters) const noexcept;
    int findFreeVoice() const noexcept;
    void makeRoomFor(int required);
    bool retuneForLegato(int midiNote, const EngineParameters& parameters);
    void pushHeldNote(int midiNote) noexcept;
    // Outcome of a note-off against the held stack.
    enum class HeldNoteState { NotHeld, StillHeld, Released };
    HeldNoteState releaseHeldNote(int midiNote) noexcept;
    int countActiveVoices() const noexcept;
    float glottalPair(int level, float phase, float tension) const noexcept;
    float sine(float phase) const noexcept;
    SineCosine sineCosineFromCycles(float cycles) const noexcept;
    static float randomBipolar(std::uint32_t& state) noexcept;
    void updateRoom(float inputLeft, float inputRight, float& wetLeft, float& wetRight) noexcept;
    void clearRoom() noexcept;
    void publishDisplayState(int voiceCount, float blockPeakLeft, float blockPeakRight,
                             int numSamples) noexcept;
    static float midiToHz(int midiNote) noexcept;

    AtomicParameters atomicParameters_ {};
    EngineParameters blockParameters_ {};
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    int maxBlockSize_ { 512 };
    bool prepared_ { false };
    std::uint64_t generation_ { 0 };
    std::uint64_t samplePosition_ { 0 };

    std::array<Voice, maxVoices> voices_ {};
    std::array<SingerIdentity, singerCount> singers_ {};
    // Interleaved lax/pressed glottal-derivative pairs: [2i] lax, [2i+1]
    // pressed. Interleaving halves the cache lines the oscillator touches.
    std::array<std::array<float, 2 * tableSize>, tableLevels> glottalTables_ {};
    std::array<float, tableSize> sineTable_ {};

    std::array<Voice*, maxVoices> activeVoices_ {};
    int activeTotal_ { 0 };
    std::array<float, chunkSize> mixLeft_ {};
    std::array<float, chunkSize> mixRight_ {};
    std::array<float, chunkSize> breathAt_ {};
    std::array<float, chunkSize> tensionAt_ {};
    std::array<float, chunkSize> voicedScaleAt_ {};

    // Per-block envelope coefficients, shared by every voice.
    float parameterSmoothing_ { 0.0f };
    float attackCoefficient_ { 0.0f };
    float airAttackCoefficient_ { 0.0f };
    float releaseMultiplier_ { 0.0f };
    float airReleaseMultiplier_ { 0.0f };
    float onsetAirMultiplier_ { 0.0f };
    float scoopMultiplier_ { 0.0f };
    float shimmerDepth_ { 0.0f };
    // Every one of these used to be a bare per-sample or per-control-period
    // constant, which made the instrument sound different at every sample rate.
    // They are now derived from a time constant in prepare().
    float shimmerCoefficient_ { 0.0f };
    float aspirationPreEmphasis_ { 0.0f };
    float aspirationScale_ { 1.0f };
    float controlGlide_ { 0.0f };
    std::array<float, formantCount> formantGlide_ {};
    float lipZeroCoefficient_ { 0.0f };
    float jitterCoefficient_ { 0.0f };
    float jitterSlowCoefficient_ { 0.0f };
    // A noise-driven one-pole's output variance is c / (2 - c), so once c comes
    // from a time constant the depth of the shimmer and of the pitch jitter
    // would fall as 1/sqrt(sampleRate). These restore the 48 kHz depth.
    float shimmerScale_ { 1.0f };
    float jitterScale_ { 1.0f };
    float roomEnvelopeDecay_ { 0.0f };

    // Chunk-rate tract state shared by every voice.
    std::array<float, formantCount> chunkFormantHz_ {};
    std::array<float, formantCount> chunkFormantGain_ {};
    std::array<float, formantCount> chunkBandwidth_ {};
    std::array<float, formantCount> chunkPoleScale_ {};
    std::array<float, formantCount> chunkA2_ {};
    std::array<float, formantCount> chunkRadius_ {};
    std::array<float, 2> earlyPoleScale_ {};
    std::array<float, 2> earlyA2_ {};
    std::array<float, 2> earlyRadius_ {};
    // Vowel targets, formant shift and bandwidth scale the tract was last
    // resolved from. Resolving it costs more than the whole rest of the chunk
    // update, and on a sustained note none of these inputs move.
    std::array<float, formantCount + 2> tractInputs_ {};
    std::array<float, formantCount> chunkAmplitude_ {};
    float jitterHumanize_ { -1.0f };
    float glideAmount_ { -1.0f };
    // Bit per singer identity currently sounding, so the ensemble drift is only
    // advanced for the singers a note actually uses.
    std::uint32_t singersInUse_ { ~0u };
    float chunkGainCoefficient_ { 1.0f };
    float chunkGlideDecay_ { 0.0f };
    bool chunkStateValid_ { false };

    float sharedPitchDrift_ { 0.0f };
    float sharedRateDrift_ { 0.0f };
    float sharedFormantDrift_ { 0.0f };
    float smoothedRoom_ { 0.0f };
    float smoothedGain_ { 0.8f };
    float smoothedBreath_ { 0.28f };
    float smoothedTension_ { 0.48f };
    // Resonance and formant shift set the pole radii directly, and a pole radius
    // cannot be smoothed after the fact, so they are smoothed before use.
    float smoothedResonance_ { 0.72f };
    float smoothedFormantShift_ { 0.0f };
    float roomEnvelope_ { 0.0f };
    std::atomic<int> activeVoiceCount_ { 0 };

    std::array<int, heldNoteCapacity> heldNotes_ {};
    // The engine is MIDI-omni, so one pitch can be held by several controllers
    // at once. The stack keeps one entry per pitch; this saturating count keeps
    // the pitch held until its final note-off.
    std::array<std::uint16_t, 128> heldNoteCounts_ {};
    int heldCount_ { 0 };
    // True while the sounding voices reached their pitch by a legato retune
    // rather than a fresh attack. Legato can be automated off mid-phrase, and
    // the note-off fallback still has to hand those voices back to the key
    // underneath -- they were never started for the pitch being released.
    bool legatoPhrase_ { false };
    int soundingRoot_ { -1 };
    int lastRootMidi_ { -1 };

    std::array<float, roomBufferSize> roomLeft_ {};
    std::array<float, roomBufferSize> roomRight_ {};
    int roomWriteIndex_ { 0 };
    std::array<float, 4> roomBaseDelay_ { 1423.0f, 1789.0f, 1999.0f, 2131.0f };
    std::array<float, 4> roomDelay_ { 1423.0f, 1789.0f, 1999.0f, 2131.0f };
    std::array<float, 4> roomModulation_ {};
    float roomFeedback_ { 0.62f };
    float roomDampingLeft_ { 0.0f };
    float roomDampingRight_ { 0.0f };
    float roomLowCutLeft_ { 0.0f };
    float roomLowCutRight_ { 0.0f };
    float roomLowCutCoefficient_ { 0.01f };
    float roomDampingCoefficient_ { 0.28f };
    float smoothedRoomSize_ { 0.5f };

    float meterLeft_ { 0.0f };
    float meterRight_ { 0.0f };
    std::array<std::atomic<float>, formantCount> displayFormantHz_ {};
    std::array<std::atomic<float>, formantCount> displayFormantBandwidth_ {};
    std::array<std::atomic<float>, formantCount> displayFormantGain_ {};
    std::atomic<float> displayLevelLeft_ { 0.0f };
    std::atomic<float> displayLevelRight_ { 0.0f };
    std::atomic<float> displayVowelX_ { 0.5f };
    std::atomic<float> displayVowelY_ { 0.5f };
    // The editor's timer keeps reading the display state while a host can be
    // inside prepare() for a sample-rate change, so the rate is published like
    // every other display field rather than read from sampleRate_ directly.
    std::atomic<float> displaySampleRate_ { 48000.0f };
};

} // namespace vocalor
