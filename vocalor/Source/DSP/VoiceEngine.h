#pragma once

#include "VocalorMath.h"

#include <array>
#include <atomic>
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
    // Added in 1.2: the base dynamic level. The mod wheel takes this over once
    // it has moved, so a controller that never sends CC 1 leaves the host
    // parameter in charge. 1.0 reproduces the 1.1 behaviour exactly.
    float dynamics { 1.0f };
    // Blend from equal temperament (0) to just intervals referred to the lowest
    // sounding note (1). 0 reproduces the 1.1 behaviour exactly.
    float intonation { 0.0f };
    // Velum coupling: 0 is a closed velum and the purely oral tract the engine
    // has always had, 1 is a closed-mouth hum. 0 reproduces 1.1 exactly.
    float nasal { 0.0f };
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
    // Whatever currently owns the dynamic level, so the editor reports the mod
    // wheel rather than the host parameter the wheel has taken over.
    float dynamics { 1.0f };
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

    // Continuous performance expression. These are driven by MIDI rather than
    // by host parameters, so they are called from the audio thread in the same
    // order as the note events: a pedal-down that precedes a note-off in the
    // block has to be seen first.

    /** Pitch bend in semitones, applied to every sounding and future voice. */
    void setPitchBend(float semitones) noexcept;
    /** Mod wheel (CC 1) or channel pressure. The first call hands the dynamic
        level to the controller for good; until then the host parameter owns it. */
    void setModWheel(float value) noexcept;
    /** Expression (CC 11). A pure output trim: it does not touch the spectrum. */
    void setExpression(float value) noexcept;
    /** Sustain pedal (CC 64). While down, note-offs are recorded rather than
        acted on; pedal-up delivers every one of them through the ordinary
        note-off path, so the legato fallback behaves as it otherwise would. */
    void setSustainPedal(bool down);
    /** Reset all controllers (CC 121). Leaves the sustain pedal alone: that is
        a switch whose physical position the message does not report. */
    void resetControllers() noexcept;

    void process(float* left, float* right, int numSamples);
    [[nodiscard]] int getActiveVoiceCount() const;
    [[nodiscard]] EngineDisplayState getDisplayState() const noexcept;

private:
    friend struct VoiceEngineTestAccess;

    static constexpr int tableSize = 2048;
    static constexpr int tableMask = tableSize - 1;
    static constexpr int tableLevels = 9;
    // The aspiration envelope needs far less resolution than the source:
    // it is smooth, it multiplies noise, and 256 entries keep it in L1 next
    // to twelve voices' worth of tract state.
    static constexpr int flowTableSize = 256;
    static constexpr int flowTableMask = flowTableSize - 1;
    static constexpr int maxHarmonics = 256;
    static constexpr int maxVoices = 96;
    static constexpr int singerCount = 12;
    static constexpr int formantCount = kFormantCount;
    static constexpr int roomBufferSize = 32768;
    static constexpr int controlPeriod = 16;
    static constexpr int heldNoteCapacity = 24;

    // How loose the section is in time. A choral entry is twelve people
    // reacting to one cue, and an ensemble's asynchrony splits into a stable
    // per-singer mean -- her habitual anticipation or lag, which is a property
    // of the singer -- and a trial-to-trial variability around it, which is a
    // property of the attempt. The engine had only the first and drew it from
    // twelve independent hashes, so every repeat was identical and the section
    // ended up with whatever dispersion those twelve draws happened to give.
    //
    // The habitual offsets are now seeded across the window instead of drawn:
    // how far a section is spread is a design decision, so it is stated rather
    // than left to a lottery. The window is the binding constraint. Onset
    // asynchrony measured over a whole piece runs 30-50 ms, which is an upper
    // bound rather than a target for a single attack, and step 8's propagation
    // delays will add up to 13 ms of their own on top; 38 ms of entry window
    // keeps the two together inside 55 ms at the ear. Within that window the
    // trial-to-trial term is what the ceiling can afford rather than what the
    // synchronisation literature would suggest: variance adds in quadrature
    // while the span adds linearly, so an even split between the two would put
    // the section's population standard deviation at 4.3 ms in its worst case
    // against the 8 ms floor. +/-2.5 ms is what fits.
    static constexpr float entryWindowSeconds = 0.038f;
    static constexpr float entryJitterSeconds = 0.0025f;
    // Cut-offs are raggeder than entries, and the same two terms describe them.
    // The habitual order is seeded on a different stride from the entry order:
    // an ictus and a cut-off are different gestures, and a singer's tendency on
    // one does not predict her tendency on the other.
    static constexpr float releaseWindowSeconds = 0.055f;
    static constexpr float releaseJitterSeconds = 0.005f;
    // Spread of the release time constant itself, as a fraction. How fast the
    // subglottal pressure falls once the larynx stops is breath support, so
    // most of it belongs to the singer; how much breath is left belongs to the
    // note.
    static constexpr float releaseTendencySpread = 0.10f;
    static constexpr float releaseNoteSpread = 0.02f;
    // Strides used to seed the two habitual orders across the twelve
    // identities. Both are coprime with singerCount, so every prefix of the
    // section -- a choir of four as much as a choir of twelve -- is spread
    // across the window rather than bunched at one end of it, and neither order
    // tracks the pan position the singer index also sets.
    static constexpr int entryOrderStride = 5;
    static constexpr int releaseOrderStride = 7;

    // Where the section stands and where it is heard from. The singers are no
    // longer mono points split by a pan law: each identity is a position in a
    // shoebox, and everything about how it reaches the listener -- arrival
    // time, level, early-reflection pattern and how wet it sounds -- falls out
    // of that position rather than being drawn.
    //
    // Depth. 1.5 m is the front of a section and 6 m the back of a large one,
    // which is 13.1 ms of arrival difference across the twelve: the
    // decorrelation an intensity pan cannot produce, because a pan applies the
    // same signal to both outputs and only a time difference makes them
    // different. The twelve are drawn from the identity hash and then ranked
    // onto the range, for the reason step 7 seeded the entry offsets rather
    // than drawing them: how deep a section stands is a design decision, and
    // twelve independent draws realise it only on average -- measured, they
    // span 2.6-4.2 m of the 4.5 m the geometry claims. The jitter breaks the
    // lattice the ranking would otherwise leave, which would comb the section's
    // summed arrival at 1/(1.19 ms) = 840 Hz.
    static constexpr float speedOfSoundMetres = 343.0f;
    static constexpr float nearSingerMetres = 1.5f;
    static constexpr float farSingerMetres = 6.0f;
    static constexpr float singerDepthJitterMetres = 0.18f;
    // Identity 0 takes the nearest position. Solo mode sings on identity 0, and
    // the applied delays are referred to the nearest singer -- a choice of time
    // origin, which leaves every relative arrival exact -- so a soloist keeps
    // the zero latency a keyboard instrument is played with.
    // How wide the section stands, as a half-angle. The azimuth is the pan
    // position the identity already carries, scaled by Spread.
    static constexpr float sectionAzimuthDegrees = 50.0f;
    // The listener is a near-coincident pair: 17 cm apart, capsules at +/-55
    // degrees, cardioid. ORTF, because it carries both cues -- the time
    // difference that decorrelates the top of the band and the level
    // difference that keeps the centre stable and the sum mono-compatible.
    static constexpr float receiverSpacingMetres = 0.17f;
    static constexpr float receiverAxisDegrees = 55.0f;
    // The shoebox, at roomSizeScale() = 1 (Size 50 %). The listener plane sits
    // 1.55 m above the floor and the ceiling 4.2 m above it, so the floor is
    // the near surface and the first reflection, which is what a room in front
    // of a choir actually does.
    static constexpr float roomHalfWidthMetres = 3.6f;
    static constexpr float roomCeilingMetres = 4.2f;
    static constexpr float roomFloorMetres = 1.55f;
    // Pressure reflectance of the four surfaces. Plaster and wood absorb
    // roughly a quarter to a third of the incident energy at speech
    // frequencies; the floor under a choir is the hardest surface in the room.
    static constexpr float wallReflectance = 0.52f;
    static constexpr float floorReflectance = 0.62f;
    // One delay line per singer identity, not per voice: twelve voices can
    // share an identity -- two simultaneous roots in Choir/12 put 24 voices on
    // twelve identities -- and a singer standing in one place is one position,
    // so the voices that share her are summed before the delay. 8192 samples is
    // 170 ms at 48 kHz and 43 ms at 192 kHz, which covers every image path the
    // geometry produces up to 96 kHz; above that the longest side-wall image in
    // the largest room clamps, exactly as roomDelay_ already clamps.
    static constexpr int placementBufferSize = 8192;
    static constexpr int placementBufferMask = placementBufferSize - 1;
    // Direct path plus first-order images in the two side walls, the floor and
    // the ceiling, each resolved to both receivers.
    static constexpr int placementTapCount = 5;
    static constexpr int placementReadCount = 2 * placementTapCount;

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
        std::atomic<float> dynamics { 1.0f };
        std::atomic<float> intonation { 0.0f };
        std::atomic<float> nasal { 0.0f };
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
        // Where this singer stands, in metres from the listener. Drawn once
        // here and never again: twelve identities carry ninety-six voices
        // between them, so a distance latched at note-on would either drag a
        // sounding note's delay line to the new note's draw -- which is a pitch
        // shifter -- or render the new note at the old note's position. Spread
        // and Size move the azimuth and the image geometry; this does not move
        // at all.
        float distanceMetres { 3.0f };
        // Habitual asynchrony: how far behind the cue this singer tends to
        // enter, and how far behind a cut-off she tends to let go, in seconds
        // at Humanize 1. Every note adds its own draw around these; see
        // buildSingerIdentities().
        float entryOffset { 0.0f };
        float releaseOffset { 0.0f };
        // How far this singer's release time constant sits from the section's,
        // as a fraction of it. Breath support and lung recoil are anatomy, so
        // this belongs to the identity rather than to the note.
        float releaseTendency { 0.0f };
        float vibratoRate { 5.1f };
        float vibratoDepth { 1.0f };
        // Sampled once per chunk, not once per voice control update: every
        // voice sharing a singer identity reads the same three values.
        float drift { 0.0f };
        // A second, incommensurate wander. With one oscillator per singer the
        // whole section returns to the same relative configuration every time
        // the slowest period comes round; with two it does not.
        float drift2 { 0.0f };
        float depthDrift { 0.0f };
        float formantDrift { 0.0f };
        float driftIncrement { 0.0f };
        float drift2Increment { 0.0f };
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
        int rootMidi { -1 };
        int midiNote { 60 };
        int singer { 0 };
        int tableLevel { 0 };
        int delaySamples { 0 };
        // How long after the common note-off this singer actually lets go. The
        // voice is already `releasing` while it counts down -- it is on its way
        // out and can be stolen -- but its envelope has not started to fall.
        int releaseDelaySamples { 0 };
        int controlCountdown { 0 };
        std::uint64_t generation { 0 };
        std::uint32_t noiseState { 1u };
        std::uint64_t ageSamples { 0 };
        std::uint64_t lastControlAge { 0 };
        float vibratoPhase { 0.0f };
        float velocity { 0.0f };
        float amplitudeGain { 0.0f };
        // amplitudeGain after the formant-tuning efficiency trim. The render
        // loop reads this one, so the trim costs nothing per sample.
        float renderGain { 0.0f };
        float phase { 0.0f };
        float phaseIncrement { 0.0f };
        float targetPhaseIncrement { 0.0f };
        float phaseIncrementStep { 0.0f };
        float envelope { 0.0f };
        float airEnvelope { 0.0f };
        float onsetAir { 1.0f };
        float airShape { 1.0f };
        // The offset gesture. A released note is not simply a note whose drive
        // is removed: the folds move, and which way they move is the phonation
        // the note was in. `abduction` is the glottal-area gesture in progress,
        // as a gain on the aspiration, and `abductionTarget` is where it is
        // headed, latched from the adduction at the moment of note-off. Both
        // are 1 while the note is held, so nothing about a sounding note moves.
        float abduction { 1.0f };
        float abductionTarget { 1.0f };
        float pitchScoop { 0.0f };
        float glideCents { 0.0f };
        // Distance from equal temperament this voice is currently singing, in
        // cents. A singer does not snap onto a just interval; she hears the
        // beating and adjusts, so this glides to its target.
        float justCents { 0.0f };
        float jitter { 0.0f };
        float jitterSlow { 0.0f };
        float shimmer { 0.0f };
        float lastNoise { 0.0f };
        float sourceTilt { 0.0f };
        float tiltCoefficient { 1.0f };
        // The two first-order shelves that carry the loudness-dependent source
        // slope, and the shelf gain itself. See sourcePresenceCoefficient_.
        float sourceSlow { 0.0f };
        float sourceSlower { 0.0f };
        float presence { 1.0f };
        // Laryngeal amplitude modulation on the vibrato cycle, as a gain on the
        // voiced source and on the presence shelf. Both carry it, which is what
        // makes a vibrato peak brighter as well as louder: the shelf gain is
        // already the note's broadband gain, so multiplying both by the same
        // factor moves the band above the corner by twice as many decibels as
        // the fundamental, which is the same 2:1 law the dynamic obeys. Ramped
        // across the control period rather than stepped, because a 6 Hz
        // modulation applied as a staircase at the control rate leaves
        // permanent sidebands 3 kHz either side of every partial.
        float vibratoGain { 1.0f };
        float vibratoGainStep { 0.0f };
        // Velocity as the singer's own output, normalised to what the same note
        // reaches at velocity 1: the level term of amplitudeGain without the
        // ensemble trim. Constant for the note, so it is resolved at note-on.
        float velocityGain { 1.0f };
        // Per-voice envelope attack. A note's attack is the folds coming onto
        // their limit cycle, and how long that takes is set by how hard the
        // note is sung, not by how loose the take is.
        float attackCoefficient { 0.0f };
        float attackDrive { -1.0f };
        // Per-voice release. The decay used to be one engine-wide coefficient
        // applied on the same sample to every voice, so twelve people let go
        // together however loosely they came in. releaseTimeScale is this
        // note's own time constant as a fraction of the section's, drawn at
        // note-on from the singer's breath support and the note's own hash;
        // releaseCoefficient is the per-sample multiplier it resolves to, and
        // releaseHumanize caches the setting it was resolved for.
        float releaseTimeScale { 1.0f };
        float releaseCoefficient { 0.0f };
        float releaseHumanize { -1.0f };
        // How far below the block's tension this note's glottal source starts.
        // sourceTensionRampDepth_ is the value at the reference velocity; a
        // hard attack begins closer to its adducted target and a soft one
        // further from it.
        float tensionSag { 0.0f };
        // Vocal effort and pan only move when a parameter does, but their
        // coefficients cost an exp2, an exp and two square roots. Cache the
        // input so a sustained note pays for them once.
        float tiltEffort { -1.0f };
        float irregularity { 0.0f };
        float baseFrequency { 261.63f };
        // Nasal branch state: the two-sample memory of the series
        // anti-resonator, and the nasal cavity's own pole in parallel with the
        // oral formants.
        float nasalX1 { 0.0f };
        float nasalX2 { 0.0f };
        float nasalY1 { 0.0f };
        float nasalY2 { 0.0f };
        Resonator nasal {};
        std::array<float, formantCount> formantHz {};
        std::array<Resonator, formantCount> tract {};
    };

    EngineParameters snapshotParameters() const noexcept;
    [[nodiscard]] float effectiveDynamics(const EngineParameters& parameters) const noexcept;
    void buildTables();
    void buildSingerIdentities();
    void initialiseVoice(Voice& voice, int rootMidi, int soundingMidi, int singer,
                         float velocity, float groupGain, int singerTotal,
                         float glideFromCents, const EngineParameters& parameters);
    void updateChunkState(const EngineParameters& parameters, bool advanceSmoothers);
    void updateVoiceControl(Voice& voice, const EngineParameters& parameters);
    void renderVoice(Voice& voice, const EngineParameters& parameters, int count);
    void silenceVoice(Voice& voice) noexcept;
    void beginRelease(Voice& voice) noexcept;
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
    void updateIntonationRoot() noexcept;
    float glottalPair(int level, float phase, float tension) const noexcept;
    float glottalFlow(float phase, float tension) const noexcept;
    float aspirationWindowGain(float tension, float depth) const noexcept;
    float sine(float phase) const noexcept;
    SineCosine sineCosineFromCycles(float cycles) const noexcept;
    static float randomBipolar(std::uint32_t& state) noexcept;
    void updateRoom(float inputLeft, float inputRight, float& wetLeft, float& wetRight) noexcept;
    void clearRoom() noexcept;
    /** Resolve the twelve positions into delays and gains. Runs at the chunk
        rate and only when Spread, Size or the mode have actually moved. */
    void updatePlacement(const EngineParameters& parameters) noexcept;
    /** Run the section's positions over @c count samples: sum each identity's
        voices into its own delay line, then read the direct path and the four
        first-order images at both receivers. */
    void renderPlacement(int count) noexcept;
    void clearPlacement() noexcept;
    void publishDisplayState(int voiceCount, float blockPeakLeft, float blockPeakRight,
                             int numSamples) noexcept;
    static float midiToHz(int midiNote) noexcept;
    /** The section's nominal release time constant, in seconds. Each voice
        scales it by its own releaseTimeScale. */
    static float sectionReleaseSeconds(float humanize) noexcept;

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
    // The glottal flow that produced those derivatives, interleaved the same
    // way and normalised to unit mean square over the period. Aspiration
    // turbulence is generated by flow through the glottal constriction, so this
    // is the noise's own envelope: no band limiting is needed because it never
    // radiates on its own, it only multiplies a broadband noise stream.
    std::array<float, 2 * flowTableSize> glottalFlowTable_ {};
    // Mean of each normalised flow prototype, and the mean of their product.
    // These are what aspirationWindowGain() needs to renormalise a crossfade.
    std::array<float, 2> flowMean_ { 1.0f, 1.0f };
    float flowCross_ { 1.0f };
    std::array<float, tableSize> sineTable_ {};

    std::array<Voice*, maxVoices> activeVoices_ {};
    int activeTotal_ { 0 };
    std::array<float, chunkSize> mixLeft_ {};
    std::array<float, chunkSize> mixRight_ {};
    // What each singer emits, before her position is applied. A voice no longer
    // pans itself into the mix: it adds itself to the identity it belongs to,
    // and the placement stage below turns that one signal into a direct arrival
    // and four reflections at two receivers.
    std::array<std::array<float, chunkSize>, singerCount> singerMix_ {};
    // The section's first-order image field, kept apart from the direct sound
    // for two reasons. It is what the recirculating network is fed -- the tail
    // is excited by the room's own early reflections rather than by the
    // already-mixed stereo pair, so it inherits the section's geometry, and the
    // send carries no 1/r of its own, which is what makes a singer at the back
    // wetter than one at the front. And it is scaled by Room on its way into
    // the dry path, because an early reflection is the room: where a singer
    // stands is geometry and is always on, but how much of the room comes back
    // is the control the player has always had, and Room 0 has always been dry.
    std::array<float, chunkSize> earlyLeft_ {};
    std::array<float, chunkSize> earlyRight_ {};
    // Aspiration level and voiced level per sample. The dynamic gains are
    // folded into these two arrays rather than applied separately, so the
    // render loop costs exactly what it did before the dynamic existed.
    std::array<float, chunkSize> airLevelAt_ {};
    std::array<float, chunkSize> tensionAt_ {};
    std::array<float, chunkSize> voicedScaleAt_ {};

    // Per-block envelope coefficients, shared by every voice. The voiced attack
    // is no longer among them: how long a note takes to reach amplitude is set
    // by how hard it is sung, so it lives on the voice.
    float parameterSmoothing_ { 0.0f };
    float airAttackCoefficient_ { 0.0f };
    // How fast the offset's glottal-area gesture completes, at the control
    // rate. A laryngeal abduction or adduction gesture runs its excursion in
    // 50-100 ms, so a one-pole at 50 ms is 86 % of the way there at 100 ms.
    float abductionCoefficient_ { 0.0f };
    float onsetAirMultiplier_ { 0.0f };
    // How far below the block's tension the glottal source starts at a note-on,
    // as a fraction of it. The folds begin abducted and lax and adduct over the
    // first tens of milliseconds, so the note starts at a higher open quotient
    // and firms up onto the block's tension on the onset time constant while
    // the tract stays put. 0.60 puts the first pulse of a Tension 0.90 patch at
    // an open quotient of 0.665 against the 0.49 it settles on, which is the
    // range voice onsets are measured over; going all the way to the lax
    // prototype raises the first-2 ms peak 5 dB on a lax female patch, because
    // that prototype carries a much larger fundamental.
    // Not const: the tests force it to zero to separate what the ramp is worth
    // from what the tract is worth.
    float sourceTensionRampDepth_ { 0.60f };
    // Corner of the two cascaded first-order shelves that carry the source's
    // loudness-dependent spectral slope. Sundberg measures partials above 1 kHz
    // rising about twice as fast in dB as overall SPL, so the shelf gain is the
    // note's own broadband gain and the shelf is what turns that into a slope
    // rather than a fader. Two stages because one first-order shelf cannot move
    // 3 kHz more than 6 dB per octave away from 450 Hz however far its corner
    // is swept, and the measured law needs about twice that.
    float sourcePresenceCoefficient_ { 0.0f };
    float scoopMultiplier_ { 0.0f };
    float shimmerDepth_ { 0.0f };
    // Every one of these used to be a bare per-sample or per-control-period
    // constant, which made the instrument sound different at every sample rate.
    // They are now derived from a time constant in prepare().
    float shimmerCoefficient_ { 0.0f };
    float aspirationPreEmphasis_ { 0.0f };
    // How much of the glottal flow's own shape the aspiration carries. The
    // turbulence that makes the noise is driven by flow through the glottal
    // constriction, so it rises through the open phase and is extinguished
    // while the folds are closed; stationary noise is heard as a separate
    // source sitting behind the voice rather than as the voice's own breath.
    // 1 is the flow itself. Not const: the tests force it to zero to prove the
    // change is a redistribution in time and not a level change.
    float aspirationModulationDepth_ { 1.0f };
    float aspirationScale_ { 1.0f };
    float controlGlide_ { 0.0f };
    // Per-formant articulator inertia: the jaw that sets F1 is heavier and
    // slower than the tongue tip and larynx that set F3 upwards, so each
    // formant reaches a new vowel target on its own timescale. Vowel
    // transitions are those articulators moving, not a de-zipper, so a formant
    // has a speed rather than a deadline: a small move settles quickly and a
    // large one takes the full articulator time.
    std::array<float, formantCount> formantGlideFast_ {};
    std::array<float, formantCount> formantGlideSlow_ {};
    std::array<float, formantCount> formantSpanScale_ {};
    // Control-rate coefficient for the intonation adjustment, expressed as a
    // time constant so a singer takes the same time to settle at every rate.
    float justGlide_ { 0.0f };
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
    // Vowel targets, formant shift and bandwidth scale the tract was last
    // resolved from. Resolving it costs more than the whole rest of the chunk
    // update, and on a sustained note none of these inputs move.
    std::array<float, formantCount + 3> tractInputs_ {};
    std::array<float, formantCount> chunkAmplitude_ {};
    // Highest F1 the jaw reaches for this profile and tract length. Formant
    // tuning stops here rather than following the pitch indefinitely.
    float chunkMaxF1_ { 1300.0f };
    // The nasal branch, resolved once per chunk: the nasal tract does not vary
    // with the vowel or with the singer, so every voice shares its coefficients
    // and pays only for its own two-sample state.
    float chunkNasalMix_ { 0.0f };
    float chunkNasalA1_ { 0.0f };
    float chunkNasalA2_ { 0.0f };
    float chunkNasalB0_ { 0.0f };
    float chunkZeroB0_ { 1.0f };
    float chunkZeroB1_ { 0.0f };
    float chunkZeroB2_ { 0.0f };
    float chunkNotchA1_ { 0.0f };
    float chunkNotchA2_ { 0.0f };
    float chunkNasalTrim_ { 1.0f };
    bool chunkNasalActive_ { false };
    // Vibrato extent the knob asks for, in cents, after the mode's own section
    // limit. It depends on nothing per-voice, and resolving it costs a pow, so
    // it is resolved once per chunk and every voice scales it by its own
    // identity depth.
    float chunkVibratoCents_ { 0.0f };
    // Linear amplitude modulation the laryngeal oscillation produces per cent
    // of extent in force. The pitch vibrato is a cricothyroid oscillation, and
    // the same oscillation moves subglottal pressure and glottal adduction, so
    // a sung vibrato carries an amplitude and a spectral component that the
    // harmonics sweeping static formant skirts cannot account for. Measured
    // amplitude vibrato runs a couple of decibels peak to peak at the extents
    // singers actually use, so 0.0020 per cent puts a full solo extent at
    // 0.217 -- 1.7 dB up, 2.1 dB down -- and the engine default at 0.35 dB.
    static constexpr float laryngealAmPerCent_ = 0.0020f;
    static constexpr float laryngealAmMaximum_ = 0.35f;
    float jitterHumanize_ { -1.0f };
    float glideAmount_ { -1.0f };
    // Dynamic response resolved once per chunk. The two gains it carries are
    // smoothed again per sample, because a 7-bit controller step across an
    // 18 dB span is audible if it lands on a chunk boundary.
    DynamicResponse chunkResponse_ {};
    float voicedDynamic_ { 1.0f };
    float airDynamic_ { 1.0f };
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
    float smoothedNasal_ { 0.0f };
    float smoothedDynamics_ { 1.0f };
    float smoothedExpression_ { 1.0f };
    float roomEnvelope_ { 0.0f };
    std::atomic<int> activeVoiceCount_ { 0 };

    // MIDI performance state. Owned by the audio thread, like the note events
    // it is interleaved with.
    float pitchBendSemitones_ { 0.0f };
    float modWheel_ { 1.0f };
    bool modWheelMoved_ { false };
    float expression_ { 1.0f };
    bool sustainPedal_ { false };
    // One counter per pitch rather than a flag: the engine is MIDI-omni, so a
    // pitch can accumulate several deferred note-offs behind a held pedal.
    std::array<std::uint16_t, 128> sustainedNotes_ {};

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
    // Lowest sounding root, which is the note the rest of the chord tunes to.
    // -1 when nothing is sounding.
    int intonationRoot_ { -1 };
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

    // One mono delay line per singer identity, laid out end to end.
    std::array<float, singerCount * placementBufferSize> placementLine_ {};
    int placementWriteIndex_ { 0 };
    // Ten reads per singer: five taps at each of the two receivers, ear 0 in
    // [0, 5) and ear 1 in [5, 10). Tap 0 is the direct path.
    //
    // A tap is an integer read plus a first-order allpass for the fraction, not
    // a linear interpolation between two samples. A linear interpolator is a
    // lowpass whose depth follows the fraction -- at a half-sample offset it is
    // 6 dB down at Nyquist and 2.6 dB down at half of it -- and the direct path
    // now carries every singer through one. Measured on the aspiration fold, a
    // fractional linear read took the glottal window's peak-to-trough from
    // 8.38 dB to 7.15 dB and moved its peak out of the open phase, which is the
    // top of the band being thrown away rather than anything about the source.
    // The allpass has exactly unit magnitude at every frequency and costs the
    // same two loads, because x[n-1] is the next sample down the same line.
    std::array<std::array<int, placementReadCount>, singerCount> placementWhole_ {};
    std::array<std::array<float, placementReadCount>, singerCount> placementAllpass_ {};
    std::array<std::array<float, placementReadCount>, singerCount> placementAllpassState_ {};
    std::array<std::array<float, placementReadCount>, singerCount> placementGain_ {};
    // The direct-path delay actually applied, in samples, per identity. This is
    // the geometric r/c less the nearest singer's, which is a choice of time
    // origin: every relative arrival is exact and the section costs the player
    // no latency it would not have had.
    std::array<float, singerCount> placementDirectSamples_ {};
    // How much longer the taps stay readable after an identity stops sounding,
    // in samples. Its line still holds up to a reflection's worth of tail.
    std::array<int, singerCount> placementHold_ {};
    // Section level against singer count: sqrt(n / sum of 1/r^2 over the first
    // n identities), so putting the section in depth redistributes it rather
    // than changing how loud it is. n = 1 leaves a soloist bit-identical.
    std::array<float, singerCount + 1> distanceNormalisation_ {};
    float placementReferenceMetres_ { 1.5f };
    float placementMetresToSamples_ { 140.0f };
    // Spread, room scale and solo-narrowing the taps were last resolved from.
    // None of them moves on a sustained note.
    float placementSpread_ { -1.0f };
    float placementScale_ { -1.0f };
    bool placementSolo_ { false };
    int placementMaximumSamples_ { 0 };
    // On-axis trim: the reference distance and the pair's cardioid gain in one
    // constant. 0.845 is what makes a single voice render at exactly the level
    // testSourceLevelCalibration pins it at, so putting the singers in a room
    // leaves the source's absolute calibration alone.
    float placementTrim_ { 0.845f };
    // How hard the image field drives the recirculating network. The network's
    // input is now the reflections rather than the stereo mix, and the
    // reflections are about 9 dB quieter than the mix was, so the send makes
    // that back: 2.84 puts the tail at the same absolute level it had before,
    // measured as the RMS of (Room 100 % minus the same render with the send
    // muted) against the RMS of the Room 0 % render.
    float placementSendGain_ { 2.84f };
    // How much of the first-order image field is rendered. 1 is the geometry.
    // Not const: the tests force it to zero to measure a property of the source
    // -- the aspiration's glottal window is a fact about the glottis, and a
    // room fills the closed phase whatever the glottis did.
    float placementReflectionDepth_ { 1.0f };
    float placementReflectionResolved_ { -1.0f };

    float meterLeft_ { 0.0f };
    float meterRight_ { 0.0f };
    std::array<std::atomic<float>, formantCount> displayFormantHz_ {};
    std::array<std::atomic<float>, formantCount> displayFormantBandwidth_ {};
    std::array<std::atomic<float>, formantCount> displayFormantGain_ {};
    std::atomic<float> displayLevelLeft_ { 0.0f };
    std::atomic<float> displayLevelRight_ { 0.0f };
    std::atomic<float> displayVowelX_ { 0.5f };
    std::atomic<float> displayVowelY_ { 0.5f };
    std::atomic<float> displayDynamics_ { 1.0f };
    // The editor's timer keeps reading the display state while a host can be
    // inside prepare() for a sample-rate change, so the rate is published like
    // every other display field rather than read from sampleRate_ directly.
    std::atomic<float> displaySampleRate_ { 48000.0f };
};

} // namespace vocalor
