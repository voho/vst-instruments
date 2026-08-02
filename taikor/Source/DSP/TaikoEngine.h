#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace taikor
{

// The twelve strokes of the kumi-daiko vocabulary, one per pitch class. An
// octave of the keyboard is therefore a complete stick technique, and the
// octave number alone chooses the drum's pitch: higher octave, higher drum.
enum class Articulation : std::uint8_t
{
    Don,     // C  - full centre strike, the open voice of the drum
    Do,      // C# - open stroke a little off centre
    Tsu,     // D  - damped centre, the free hand resting on the head
    Su,      // D# - ghost stroke, barely sounded
    DonRim,  // E  - head and rim struck together
    Ka,      // F  - on the edge of the head, near the tacks
    Kara,    // F# - extreme edge, thin and cutting
    Ko,      // G  - light tap at mid radius
    Katsu,   // G# - bachi on the wooden shell
    Buzz,    // A  - press roll
    Flam,    // A# - grace note into a full stroke
    Bachi,   // B  - stick against stick, no drum at all
    Count
};

inline constexpr std::size_t articulationCount =
    static_cast<std::size_t> (Articulation::Count);

// An odaiko's fundamental can still be running after ten seconds at low
// damping, so the host is told to keep the tail alive that long.
inline constexpr double maximumTailSeconds = 12.0;

// Playable range: six octaves, C1..B6. The octave containing `referenceNote`
// plays the drum at exactly the size and tension the parameters describe;
// every octave above or below rescales the physical model (see EngineParameters
// ::octaveBody), so the whole taiko family from odaiko to shime-daiko sits
// under the hands at once.
inline constexpr int lowestPlayableNote = 24;   // C1
inline constexpr int highestPlayableNote = 95;  // B6
inline constexpr int referenceNote = 48;        // C3
inline constexpr int lowestOctaveOffset = -2;
inline constexpr int highestOctaveOffset = 3;

struct ArticulationMetadata
{
    Articulation articulation {};
    std::string_view displayName;
    std::string_view slug;
    // The spoken drum syllable (kuchi shoka) this stroke is named for.
    std::string_view mnemonic;
    std::string_view description;
    int pitchClass { 0 };
};

[[nodiscard]] const ArticulationMetadata& getArticulationMetadata (
    Articulation articulation) noexcept;
[[nodiscard]] std::string_view getArticulationDisplayName (
    Articulation articulation) noexcept;
[[nodiscard]] std::string_view getArticulationSlug (Articulation articulation) noexcept;
// Pitch class selects the stroke; the octave selects the drum.
[[nodiscard]] std::optional<Articulation> articulationForMidiNote (int midiNote) noexcept;
[[nodiscard]] std::optional<int> octaveOffsetForMidiNote (int midiNote) noexcept;
[[nodiscard]] int midiNoteFor (Articulation articulation, int octaveOffset) noexcept;

// Every control the player has over the instrument. The first block describes
// the physical drum, the second how it is struck, and the third the close pair
// of microphones in front of it. Nothing here is a voicing preset: each field
// feeds a term of the model, so the defaults describe one specific drum - a
// 55 cm nagado-daiko with a thick cowhide head on a heavy zelkova shell,
// struck with a medium-hard oak bachi - rather than the midpoint of every axis.
struct EngineParameters
{
    // --- The drum -------------------------------------------------------
    // Head diameter in metres. Sets the membrane radius directly, so it moves
    // pitch as 1/a while leaving the modal ratios (fixed Bessel zeros) alone.
    float headDiameter { 0.95f };
    // Body depth as a fraction of the diameter, 0 -> 0.40, 1 -> 1.30. The
    // enclosed volume is what couples the two heads, so a shallow drum splits
    // its axisymmetric modes much further apart than a deep one.
    float bodyDepth { 0.5f };
    // Head tension. Mapped geometrically onto 1.2..22 kN/m, the range a tacked
    // or rope-laced hide actually occupies. Wave speed is sqrt(T/sigma), so
    // this and the head material together set the pitch.
    float tension { 0.55f };
    // 0 = thin synthetic film, 1 = thick heavy cowhide. Sets the head's areal
    // density and its internal loss factor at the same time, because both come
    // from the same piece of material.
    float headMaterial { 0.75f };
    // 0 = light laminated ply, 1 = dense carved zelkova. Sets the shell's ring
    // frequencies, their Q, and how much the rim absorbs from the head.
    float shellMaterial { 0.8f };
    // Tension of the far (resonant) head relative to the batter head,
    // 0 -> 0.85, 1 -> 1.15. Detuning the pair is the traditional way to
    // lengthen or shorten a taiko's boom.
    float resonantTension { 0.5f };
    // How strongly the enclosed air ties the two heads together. 0 leaves the
    // batter head free, as though the body were open; 1 is a fully sealed
    // shell. Only the axisymmetric modes couple - every other mode moves the
    // same amount of air in and out and cannot compress the cavity.
    float cavityCoupling { 0.85f };
    // Extra loss in the head on top of the material's own, 0..1.
    float headDamping { 0.50f };
    // Level of the wooden shell's own ring modes, 0..1.
    float shellResonance { 0.4f };
    // Musical transposition applied on top of the physics, in semitones. It
    // scales the membrane's wave speed, so it retunes the head exactly the way
    // the tension control does without disturbing the body.
    float pitch { 0.0f };

    // --- The stroke -----------------------------------------------------
    // 0 = a soft felt-wrapped beater, 1 = a hard oak bachi. Drives the Hertz
    // contact stiffness, which sets how long the stick stays on the head and
    // therefore how much high partial content a stroke carries.
    float bachiHardness { 0.7f };
    // Bipolar offset added to each stroke's own strike radius, -1 (towards the
    // centre) to +1 (towards the rim). 0 leaves the vocabulary as written.
    float strikePosition { 0.0f };
    // How much MIDI velocity is allowed to move the impact speed, 0..1. The
    // timbre change that comes with it is not separately adjustable because it
    // is not a separate effect: contact time follows impact speed as v^(-1/5).
    float velocityDepth { 0.75f };
    // Depth of the attack pitch glide. A hard stroke stretches the head, which
    // raises its tension and drops back as the stroke decays.
    float tensionModulation { 0.4f };
    // Level of the broadband contact noise the stick makes on the hide, 0..1.
    float strikeNoise { 0.35f };
    // Per-stroke variation in position, angle, impact speed and contact time.
    // 0 makes every stroke of a given articulation identical.
    float humanise { 0.4f };
    // How an octave of transposition is realised physically, 0..1. At 0 the
    // drum keeps its size and only the head tension changes, which is the same
    // drum tuned up. At 1 the drum's diameter halves per octave and the tension
    // is left alone, which is a genuinely smaller drum: its cavity coupling,
    // air loading and radiation do not scale with it, so it sounds smaller
    // rather than merely higher.
    float octaveBody { 0.7f };

    // --- The microphones ------------------------------------------------
    // Distance of the close pair from the batter head, 0 -> 3 cm, 1 -> 40 cm.
    // Inside that range the mics sit in the near field of every mode that
    // matters, so they sample the membrane's shape rather than its radiation.
    float micDistance { 0.35f };
    // How far apart the pair is spread across the head, 0 = nearly coincident
    // over the centre, 1 = one mic over each side near the rim. This is where
    // the stereo image comes from: two points on the head see different signs
    // and amplitudes of every mode with a circumferential order, so the pair
    // decorrelates for real rather than through a delay or a reverb.
    float micSpread { 0.55f };
    // Width trim on the resulting pair. At 0.5 the two channels are exactly
    // what the microphones picked up, which is the default: this instrument's
    // image comes from the model, so widening it by default would be widening
    // a real measurement for effect. Below that it folds towards mono, and 0 is
    // an exact mono sum. Above it the side signal is deliberately exaggerated
    // past what the pair captured - useful, but with the microphones close
    // together over the head it can push strokes out of phase, exactly as
    // pushing a real wide spaced pair through a widener does.
    float stereoWidth { 0.5f };
    // Gentle output-stage saturation, 0 = exactly bypassed.
    float drive { 0.0f };
    // Linear output gain.
    float outputGain { 0.100000f };
};

// Snapshot of the drum for the editor's head display. Produced on the audio
// thread and read on the message thread through relaxed atomics, so it is kept
// small and trivially copyable.
struct DrumVisualState
{
    // Where the most recent stroke landed, in head-relative polar coordinates.
    float strikeRadius { 0.0f };   // 0 centre .. 1 rim
    float strikeAngle { 0.0f };    // radians
    float strikeLevel { 0.0f };    // 0..1, decays after the stroke
    // Sounding fundamental of the drum as currently configured, in hertz.
    float fundamentalHz { 0.0f };
    Articulation lastArticulation { Articulation::Don };
    int lastOctaveOffset { 0 };
    int activeVoices { 0 };
};

class TaikoEngine
{
public:
    TaikoEngine() noexcept;

    void prepare (double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;
    void setParameters (const EngineParameters& parameters) noexcept;

    // Strike the drum directly. `octaveOffset` is relative to the reference
    // octave and is clamped to the playable range.
    void trigger (Articulation articulation, int octaveOffset, float velocity) noexcept;
    // Returns false for notes outside the playable range, which stay silent.
    [[nodiscard]] bool triggerMidi (int midiNote, float velocity) noexcept;
    // A hand laid on the head, from MIDI CC1. It damps everything still
    // ringing, and it keeps damping while it is held - so a stroke struck with
    // the hand down is a muted stroke, which is what a hand resting on a drum
    // head actually does. Exempting new strokes would model a hand that lifts
    // itself out of the way.
    void setHandDamping (float normalised) noexcept;
    // Pressing the head raises its tension, so the wheel bends the drum up the
    // way a palm does: sharp, and with a slightly shorter tail.
    void setPitchBend (float normalisedBipolar) noexcept;
    void allSoundsOff() noexcept;

    void process (float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] int getActiveVoiceCount() const noexcept;
    [[nodiscard]] float getOutputLevel (int channel) const noexcept;
    // Sounding fundamental of the drum for the octave last played, in hertz.
    [[nodiscard]] float getFundamentalHz() const noexcept;
    void getVisualState (DrumVisualState& destination) const noexcept;

    // Physical readouts. They describe the drum the parameters currently
    // configure, independently of whether anything is sounding, so the editor
    // and the regression suite can both check the model rather than the audio.
    struct DrumMeasurements
    {
        float radiusMetres { 0.275f };
        float depthMetres { 0.275f };
        float tensionNewtonsPerMetre { 6000.0f };
        float arealDensityKgPerSquareMetre { 1.2f };
        float waveSpeedMetresPerSecond { 70.0f };
        // Ideal (0,1) membrane mode, before air loading and cavity coupling.
        float idealFundamentalHz { 100.0f };
        // Lowest and strongest sounding modes once the air has been accounted
        // for. On a sealed drum these differ: the cavity lifts the mode that
        // changes the body's volume well above the one that does not.
        float loadedFundamentalHz { 88.0f };
        float breathingModeHz { 140.0f };
        // How long the drum audibly rings: the longer-lived of the two
        // axisymmetric branches, each solved with its own radiation share.
        // Reporting only one of them described whichever mode happened to be
        // chosen rather than the drum.
        float tailSeconds { 1.2f };
    };

    [[nodiscard]] DrumMeasurements measureDrum (int octaveOffset) const noexcept;
    // The same measurement without an engine instance, so the editor can show
    // what drum the current controls describe without keeping a second copy of
    // the whole voice pool alive to ask.
    [[nodiscard]] static DrumMeasurements measure (
        const EngineParameters& parameters, int octaveOffset,
        float pitchBendSemitones = 0.0f) noexcept;

    // Contact time of a stroke in seconds, as the Hertz impact solve returns
    // it. Exposed because the velocity-to-timbre law is the single most
    // audible piece of physics in the instrument and is worth testing directly.
    [[nodiscard]] float measureContactSeconds (Articulation articulation,
                                               int octaveOffset,
                                               float velocity) const noexcept;
    [[nodiscard]] static float measureContact (const EngineParameters& parameters,
                                               Articulation articulation,
                                               int octaveOffset,
                                               float velocity) noexcept;

private:
    friend struct TaikoEngineTestAccess;

    static constexpr int maxVoices = 16;
    // Membrane modes (m, n) with Bessel zeros up to about 12.3. Four are
    // axisymmetric and split in two by the cavity; the other sixteen carry a
    // circumferential order and split in two by orientation instead.
    static constexpr int modeEntryCount = 20;
    static constexpr int axisymmetricEntryCount = 4;
    static constexpr int membraneResonatorCount = 40;
    static constexpr int shellResonatorCount = 6;
    static constexpr int resonatorCount =
        membraneResonatorCount + shellResonatorCount;
    // Bands of the head's high-frequency modal continuum. Above a few hundred
    // hertz a struck membrane has far more modes than can usefully be resolved
    // one at a time - the spacing falls below their own bandwidth and the
    // response stops being a set of peaks and becomes statistical. Resolving
    // that region mode by mode would need hundreds of resonators; what it
    // actually sounds like is a shaped noise burst that decays faster the
    // higher it sits, which is what these bands are. Without them the model
    // simply stopped at its highest resolved mode and the drum had no body
    // above about three hundred hertz at all.
    static constexpr int continuumBandCount = 5;

    // Free-free bending modes of one bachi, used by the stick-on-stick stroke.
    // It borrows the shell's slots in the bank because the two never sound
    // together, but it is a separate count so that changing one bank's size
    // cannot silently resize the other.
    static constexpr int stickResonatorCount = 6;
    static_assert (stickResonatorCount <= shellResonatorCount,
                   "the stick bank shares the shell's slots in Voice::modes");
    // Contact solves and mode retirement run on this stride rather than per
    // sample. 32 samples is 0.67 ms at 48 kHz, well under the shortest glide.
    static constexpr int controlPeriod = 32;
    static constexpr int maxContactEvents = 12;
    // Long enough for the widest path difference the model can produce at the
    // highest supported sample rate. The worst case is the largest reachable
    // drum - a 120 cm head with Octave Body at Family, two octaves down, which
    // resolves to a 2.4 m radius - struck at the rim with the microphones fully
    // opened: about 4.25 m of path, 12.4 ms, or 4763 samples at 384 kHz. A
    // shorter line would clamp those delays and collapse distinct strike
    // positions onto the same arrival time, which is the cue that places a
    // stroke across the image. Must be a power of two.
    static constexpr int directLineSize = 8192;
    static constexpr double minimumSupportedSampleRate = 8000.0;
    static constexpr double maximumSupportedSampleRate = 384000.0;
    // A resonator is dropped from the render once its envelope has fallen this
    // far below the level it started at.
    static constexpr float modeRetirementFloor = 1.0e-7f;
    // Fade applied to a voice that reaches the hard tail cap while still
    // audible. Long enough to be inaudible, short against the shortest cap.
    static constexpr float forcedFadeSeconds = 0.060f;

    struct MembraneModeEntry
    {
        int circumferentialOrder { 0 };
        double besselZero { 2.4048255576957728 };
    };

    // A single damped resonator. Coefficients are recomputed only when the
    // drum is retuned, so the inner loop is three multiplies and two adds.
    // A two-pole resonator, in double precision.
    //
    // Not for elegance: a1 is -2 r cos(omega), which sits arbitrarily close to
    // -2 as the mode's frequency falls against the sample rate, and the pair
    // y[n-1], y[n-2] then very nearly cancel. Both effects consume mantissa in
    // proportion to (rate / frequency)^2, and this instrument's lowest mode is
    // around fifty hertz - one part in eight thousand at 384 kHz. In float that
    // mistuned the drum by thirty cents there and by seven at 192 kHz, which is
    // audible against anything else in the session. In double the same margin
    // costs a handful of the fifty-three bits available and the drum stays in
    // tune at every supported rate. The recursion is serial, so no vectorising
    // is being given up for it.
    struct Resonator
    {
        double a1 { 0.0 };
        double a2 { 0.0 };
        double b0 { 0.0 };
        double y1 { 0.0 };
        double y2 { 0.0 };

        void clear() noexcept { y1 = y2 = 0.0; }
        [[nodiscard]] float tick (float input) noexcept
        {
            const double output = b0 * static_cast<double> (input) - a1 * y1 - a2 * y2;
            y2 = y1;
            y1 = output;
            return static_cast<float> (output);
        }
    };

    // Everything about one sounding mode that the render loop needs, laid out
    // together so a mode is one contiguous read.
    struct Mode
    {
        Resonator resonator {};
        // Drive gain: mode shape at the strike point over modal mass, already
        // divided by the sample rate so the resonator integrates a force.
        float drive { 0.0f };
        float micLeft { 0.0f };
        float micRight { 0.0f };
        // Undamped frequency in radians per sample, kept so the tension glide
        // can retune the resonator without redoing the whole physical solve.
        float omega { 0.0f };
        float decayRate { 0.0f };
        // The decay split into what moves with the head and what does not, so a
        // mode retuned while it is still sounding can be re-damped rather than
        // keeping the rate it was built with. The hide's loss goes as omega and
        // as omega squared, and the mounting is steeply low-pass in frequency,
        // so a stroke automated an octave up would otherwise carry the mounting
        // loss of the note it started on - which on a large drum is most of its
        // damping - and empty in a fraction of the time it should. decayFixed
        // is the rest: what the mode radiates and what the rim takes.
        float decayFixed { 0.0f };
        float lossOmega { 0.0f };
        float lossOmegaSquared { 0.0f };
        // log(level / retirement floor), so the lifetime below can be redone
        // from a new decay rate without the whole bank's levels to hand. Zero
        // for a mode that was never audible.
        float retirementLog { 0.0f };
        // True for the membrane, false for the shell. The attack glide and the
        // wheel stretch the head; neither of them touches the wooden body, and
        // the bank is sorted by lifetime so the two kinds interleave.
        bool membrane { true };
        // Sample count after which this mode has fallen below the retirement
        // floor. Modes are stored in descending order of this value, so the
        // render loop only has to track a shrinking count.
        std::uint64_t audibleSamples { 0 };
    };

    // One scheduled stick contact. An ordinary stroke has a single contact; a
    // flam has two and a press roll has a train of them.
    struct ContactEvent
    {
        std::uint32_t startSample { 0 };
        std::uint32_t lengthSamples { 0 };
        float amplitude { 0.0f };
        float noiseAmplitude { 0.0f };
    };

    struct Voice
    {
        bool active { false };
        Articulation articulation { Articulation::Don };
        int octaveOffset { 0 };
        std::uint64_t startOrder { 0 };
        std::uint64_t ageSamples { 0 };
        std::uint64_t maximumSamples { 0 };
        std::uint32_t noiseState { 1u };

        std::array<Mode, resonatorCount> modes {};
        int modeCount { 0 };
        int activeModeCount { 0 };

        std::array<ContactEvent, maxContactEvents> contacts {};
        int contactCount { 0 };
        int nextContact { 0 };
        // Running contact, if one is in progress.
        std::uint32_t contactRemaining { 0u };
        std::uint32_t contactLength { 0u };
        float contactAmplitude { 0.0f };
        float contactNoiseAmplitude { 0.0f };
        // Amplitude of the stroke's first contact, so a later one can relight
        // the continuum in proportion to it.
        float contactReference { 0.0f };
        float noiseBandState { 0.0f };
        float noiseBandCoefficient { 0.5f };

        // One band of the continuum: noise through a one-pole band-pass, under
        // its own decaying envelope. It belongs to the head, so the hand damps
        // it along with the resolved modes.
        struct ContinuumBand
        {
            // Two one-poles per side, cascaded, so each edge falls at twelve
            // decibels an octave rather than six. A single pole is not enough
            // to make a band: its skirt falls so slowly that the lowest band,
            // which is also the loudest, was louder four octaves up than the
            // band that belongs there, and the whole continuum above the first
            // octave was inaudible under it. Nothing that shaped the upper
            // bands - their tilt, their contact-duration cut - could be heard
            // at all, because none of them were what the ear was hearing.
            float lowStateLeft { 0.0f };
            float lowStateLeft2 { 0.0f };
            float highStateLeft { 0.0f };
            float highStateLeft2 { 0.0f };
            float lowStateRight { 0.0f };
            float lowStateRight2 { 0.0f };
            float highStateRight { 0.0f };
            float highStateRight2 { 0.0f };
            float lowCoefficient { 0.5f };
            float highCoefficient { 0.5f };
            float level { 0.0f };
            float envelope { 0.0f };
            float envelopeDecay { 0.99f };
            // Kept so the strike can shade the band by how long the stick
            // stayed on the head: a short contact reaches further up.
            float centre { 0.0f };
            // How much of the band the two microphones hear in common. A
            // wavelength long against the spacing arrives at both alike; one
            // short against it does not, so the top of the continuum is very
            // nearly two independent signals and the bottom is one.
            float common { 1.0f };
            float independent { 0.0f };
        };
        std::array<ContinuumBand, continuumBandCount> continuum {};

        // Attack pitch glide. The head is stretched by the stroke, so its
        // tension - and every mode with it - starts sharp and settles.
        float tensionEnvelope { 0.0f };
        float tensionDecay { 0.999f };
        float tensionDepth { 0.0f };
        float appliedTensionShift { 1.0f };
        // The mounting loss this stroke was built with, kept so retuning can
        // re-evaluate it at the mode's new frequency.
        float mountLoss { 0.0f };
        float mountCorner { 80.0f };
        // The continuum's decay as a function of where a band sits: a constant
        // from the rim, a term in omega from the hide's hysteresis and the
        // rim's per-order share, and one in omega squared from the hide's
        // viscosity. Stored rather than summed so a retuned band can be
        // re-damped exactly, instead of by scaling what it had - which
        // compounds, and drifts with every block the glide runs.
        float continuumLossFixed { 0.0f };
        float continuumLossOmega { 0.0f };
        float continuumLossOmegaSquared { 0.0f };
        // Added to every audible lifetime once the contact schedule is known,
        // and kept so a recomputed lifetime can have it put back.
        std::uint64_t retirementOffset { 0 };

        // Accumulated hand damping, folded into the resonator states at the
        // control tick so the envelope never runs away.
        float handGain { 1.0f };

        // A low-loss drum can ring far longer than the tail the host is told
        // to expect, so a voice still has to end at the cap - but it has to be
        // faded out over the last few tens of milliseconds rather than cut,
        // or the truncation is a click and the shared DC blocker rings for it.
        float retireGain { 1.0f };
        float retireStep { 0.0f };

        // The drum's total tuning offset, in semitones, when this stroke was
        // struck - the Pitch control and the wheel together. The voice's modes
        // already carry that much, so only the change since then has to be
        // applied to a voice that is already ringing. Both belong here because
        // both are head tension: automating Pitch retunes a ringing tail for
        // exactly the reason the wheel does.
        float tuningAtStrike { 0.0f };

        // The airborne path from the stick to each microphone. A close pair
        // hears the impact itself, not only what the head does afterwards, and
        // because the two mics are different distances from the strike it
        // arrives at different levels and different times. That is the cue
        // that puts a stroke somewhere on the drum rather than in the middle
        // of it, and it is why a spaced pair over a real drum stays in phase
        // even on an edge strike that the membrane modes alone would cancel.
        std::array<float, directLineSize> directLine {};
        int directWriteIndex { 0 };
        float directDelayLeft { 0.0f };
        float directDelayRight { 0.0f };
        float directGainLeft { 0.0f };
        float directGainRight { 0.0f };
        float directPrevious { 0.0f };
        // The contact patch has a size, so it cannot radiate wavelengths
        // shorter than itself. Without this the differentiated force is an
        // unbounded spike and the attack reads as a click on top of a drum
        // rather than as the drum's own attack.
        float directLowpassState { 0.0f };
        float directLowpassCoefficient { 0.5f };

        float velocity { 0.0f };
        float strikeRadius { 0.0f };
        float strikeAngle { 0.0f };
        float peakLevel { 0.0f };
        int controlCountdown { 0 };
    };

    struct StrikeProfile
    {
        float radius { 0.0f };        // 0 centre .. 1 rim
        float hardnessScale { 1.0f }; // multiplies the contact stiffness
        float membraneGain { 1.0f };
        float shellGain { 0.2f };
        float noiseGain { 1.0f };
        float levelScale { 1.0f };
        // Extra head damping the free hand applies for a muted stroke.
        float muteAmount { 0.0f };
        // Contact schedule: single, flam, or press roll.
        int contactCount { 1 };
        // Rim contribution: a shot that catches the hoop as well as the head.
        float rimGain { 0.0f };
        // Shell mode retune, used by the strokes that catch the hoop to shorten
        // the body's ring. Ignored when the bank is not the drum's body.
        float shellFrequencyScale { 1.0f };
        float shellDecayScale { 1.0f };
        // Whether this stroke's resonant bank is the drum's own body. The
        // stick-on-stick stroke rings two pieces of wood that never touch the
        // drum, so it reads a StickState instead and no drum control - the
        // shell's material, the head's diameter, its depth or its tension - may
        // reach it.
        bool usesDrumBody { true };
    };

    // The physical drum, resolved from the parameters for one octave. Every
    // per-strike computation reads this rather than the raw parameters.
    struct DrumState
    {
        float radius { 0.275f };
        float depth { 0.275f };
        float tension { 6000.0f };
        float resonantTension { 6000.0f };
        float batterDensity { 1.2f };
        float resonantDensity { 1.2f };
        float waveSpeed { 70.0f };
        float resonantWaveSpeed { 70.0f };
        float headLossFactor { 0.012f };
        // The viscous half of the hide's loss, damping as omega squared where
        // headLossFactor damps as omega. See resolveDrumFor.
        float headViscousFactor { 0.0f };
        float edgeLoss { 0.6f };
        // Cavity stiffness per unit area, before the per-mode 4/lambda^2
        // volume-efficiency weighting. Zero on an uncoupled (open) body.
        float cavityStiffness { 0.0f };
        float radiationScale { 0.10f };
        // Close-pair geometry, resolved once so every stroke places the mics
        // identically. Radius is in metres; angles are in radians.
        float micRadius { 0.0f };
        float micAngleLeft { 0.0f };
        float micAngleRight { 0.0f };
        float micDistanceMetres { 0.16f };
        float micProximity { 0.6f };
        std::array<float, shellResonatorCount> shellFrequencies {};
        std::array<float, shellResonatorCount> shellDecays {};
        // Modal mass of the shell wall, so a stroke on the body drives it
        // through the same force-over-mass path the head uses rather than
        // through a bare level constant.
        float shellModalMass { 12.0f };
        float shellLevel { 0.4f };
        // Loss into the shell, hoops and stand, and the frequency below which
        // a mode is long enough to move them.
        float mountLoss { 0.0f };
        float mountCorner { 80.0f };
    };

    // A pair of bachi, for the stroke that claps them together and never
    // touches the drum. It is deliberately a separate structure from DrumState
    // and is resolved by a function that cannot see one: the stick's pitch is a
    // property of the stick, and reading it off the drum's body made Shell
    // Material, Head Diameter and Body Depth all retune the click.
    struct StickState
    {
        std::array<float, stickResonatorCount> frequencies {};
        std::array<float, stickResonatorCount> decays {};
        // Effective mass of one stick in a bending mode, the analogue of
        // DrumState::shellModalMass.
        float modalMass { 0.08f };
        // Reduced mass of the collision. Two equal sticks meeting each other
        // give m/2, which is what sets the impulse.
        float strikerMass { 0.04f };
        // Resistive driving-point impedance of the bar at its first bending
        // mode, the analogue of the membrane's 8*sqrt(T*sigma): the floor on
        // how quickly the two sticks can separate.
        float impedance { 300.0f };
        // Projected side area of the cylinder, which is what pushes air.
        float radiatingArea { 0.01f };
    };

    [[nodiscard]] static EngineParameters sanitise (
        const EngineParameters& parameters) noexcept;
    [[nodiscard]] static const std::array<MembraneModeEntry, modeEntryCount>&
        membraneModes() noexcept;
    [[nodiscard]] static const StrikeProfile& strikeProfile (
        Articulation articulation) noexcept;

    // Bessel function of the first kind, evaluated by its ascending series.
    // Only ever called while setting a stroke up, never in the render loop.
    [[nodiscard]] static double besselJ (int order, double x) noexcept;
    // Low-frequency multipole radiation efficiency of a mode of
    // circumferential order m at the given ka. This is the reason a centre
    // strike is heard as a boom and an edge strike as a slap: modes with a
    // circumferential order move the same air in and out and barely radiate.
    [[nodiscard]] static float radiationEfficiency (int order, float ka) noexcept;
    // Loss into the mounting, which only the lowest modes suffer.
    [[nodiscard]] static float mountingLoss (const DrumState& drum,
                                             float frequency) noexcept;
    [[nodiscard]] static float mountingLossAt (float mountLoss, float mountCorner,
                                               float frequency) noexcept;
    // What a membrane mode's decay is once the head has been stretched to put
    // it at this frequency. Used by the attack glide, the wheel and Pitch
    // automation, all of which move a mode after it has been built.
    [[nodiscard]] static float membraneDecayAt (const Voice& voice, const Mode& mode,
                                                float omega) noexcept;
    // The hide's own loss at a given frequency: hysteretic plus viscous. Shared
    // by the resolved modes and by the continuum above them, which is the whole
    // point of it - the two have to sit on one curve or they do not sound like
    // one head.
    [[nodiscard]] static float materialDamping (const DrumState& drum, float omega,
                                                float extraDamping) noexcept;
    // Fractional read of the airborne-path delay line. Extracted from the
    // render loop so the trickiest index arithmetic in this file can be tested
    // against a known ramp rather than inferred from the stereo image.
    [[nodiscard]] static float readDelayLine (
        const std::array<float, directLineSize>& line, int writeIndex,
        float delaySamples) noexcept;
    // One branch of the coupled axisymmetric pair: its eigenvalue and the two
    // head components of its eigenvector. Shared by the render path and the
    // readout so they cannot disagree about the drum - in particular about the
    // degenerate, uncoupled case.
    static void solveAxisymmetricBranch (float diagonalB, float diagonalR,
                                         float offDiagonal, int branch,
                                         float& eigenvalue, float& vectorB,
                                         float& vectorR) noexcept;
    [[nodiscard]] static std::uint32_t hash32 (std::uint32_t value) noexcept;
    [[nodiscard]] static float signedUnitFromHash (std::uint32_t value) noexcept;
    [[nodiscard]] static float nextNoise (std::uint32_t& state) noexcept;

    // Resolving a drum depends only on the parameter block, the wheel and the
    // octave, so it is static and the instance method simply supplies its own.
    [[nodiscard]] static DrumState resolveDrumFor (const EngineParameters& parameters,
                                                   float pitchBendSemitones,
                                                   int octaveOffset) noexcept;
    [[nodiscard]] DrumState resolveDrum (int octaveOffset) const noexcept;
    // The pair of sticks. Takes the parameter block rather than a DrumState on
    // purpose: it reads only the hardness control and the octave, so no drum
    // control can reach the stick-on-stick stroke through it.
    [[nodiscard]] static StickState resolveStickFor (const EngineParameters& parameters,
                                                     int octaveOffset) noexcept;
    // Hertz impact, returning the contact duration in seconds and the peak
    // force. Contact time follows impact speed as v^(-1/5) and is floored by
    // the struck body's own resistive impedance, because the stick cannot leave
    // faster than that body carries the energy away. The caller supplies both
    // the striking mass and that impedance, so the same solver serves a stick
    // meeting a head and a stick meeting another stick.
    static void solveContact (float strikerMass, float targetImpedance,
                              const StrikeProfile& profile, float bachiHardness,
                              float impactSpeed, float& contactSeconds,
                              float& peakForce) noexcept;
    // The striking mass and the head's resistive impedance for a drum stroke.
    static void drumContactTerms (const DrumState& drum, float& strikerMass,
                                  float& impedance) noexcept;
    void buildVoiceModes (Voice& voice, const DrumState& drum, const StickState& stick,
                          const StrikeProfile& profile, float extraDamping) noexcept;
    void scheduleContacts (Voice& voice, const StrikeProfile& profile,
                           float contactSeconds, float peakForce,
                           float noiseLevel) noexcept;
    void applyTensionShift (Voice& voice, float shift) noexcept;
    void updateVoiceControl (Voice& voice) noexcept;
    [[nodiscard]] float renderVoice (Voice& voice, float& rightOut) noexcept;
    [[nodiscard]] int findVoiceSlot() noexcept;
    void silenceVoice (Voice& voice) noexcept;
    void updateActiveVoiceCount() noexcept;
    void refreshDrumIfNeeded() noexcept;
    // Configures one resonator from a physical frequency and decay rate.
    void configureResonator (Resonator& resonator, float frequencyHz,
                             float decayRate, float gain) const noexcept;

    // Sanitised parameters, as published by setParameters(). Like the other
    // engines in this repository the setter is called from the audio thread
    // (or is otherwise externally synchronised with it), so a plain copy is
    // enough and no atomics are needed here.
    EngineParameters applied_ {};

    std::array<Voice, maxVoices> voices_ {};
    // One resolved drum per playable octave. Strokes are common and the solve
    // involves a Bessel series and an eigen-decomposition per mode, so it is
    // done once per parameter change rather than once per stroke.
    std::array<DrumState, highestOctaveOffset - lowestOctaveOffset + 1> drumCache_ {};
    // One pair of sticks per octave, cached alongside the drums. Smaller drums
    // are played with smaller sticks, which is the only thing the octave does
    // to them.
    std::array<StickState, highestOctaveOffset - lowestOctaveOffset + 1> stickCache_ {};
    bool drumCacheValid_ { false };
    // The wheel position the cache was built at. Comparing against this rather
    // than against the per-sample increment matters at high sample rates, where
    // the increment falls below any sensible epsilon long before the glide has
    // actually arrived - leaving the cache stale, and new strokes flat, by up
    // to a quarter of a semitone.
    float drumCacheBend_ { 0.0f };

    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    int maxBlockSize_ { 512 };
    bool prepared_ { false };
    std::uint64_t noteSequence_ { 0 };

    float handDampingTarget_ { 0.0f };
    float handDamping_ { 0.0f };
    float handDampingCoefficient_ { 0.05f };
    float pitchBendTarget_ { 0.0f };
    float pitchBend_ { 0.0f };
    float pitchBendCoefficient_ { 0.05f };

    float smoothedOutputGain_ { 0.5f };
    float smoothedDrive_ { 0.0f };
    float smoothedWidth_ { 0.6f };
    float gainSmoothing_ { 0.001f };
    float dcCoefficient_ { 0.9985f };
    float dcInputLeft_ { 0.0f };
    float dcInputRight_ { 0.0f };
    float dcOutputLeft_ { 0.0f };
    float dcOutputRight_ { 0.0f };
    float driveAdaaLeft_ { 0.0f };
    float driveAdaaRight_ { 0.0f };
    // A drum track is silent most of the time. Once nothing is sounding and
    // the shared DC and drive path has rung out, the whole output chain is
    // switched off and the buffer is written as exact zeros - which is both
    // where a float path would otherwise sit generating denormals, and the
    // difference between "inaudible" and "silent" for anything downstream.
    static constexpr int idleFreezeSamples = 64;
    static constexpr float idleFreezeLevel = 1.0e-9f;
    int silentSamples_ { idleFreezeSamples };
    bool idleFrozen_ { true };

    float meterReleaseMultiplier_ { 0.9999f };
    float meterLeft_ { 0.0f };
    float meterRight_ { 0.0f };

    std::atomic<int> activeVoiceCount_ { 0 };
    std::atomic<float> outputLevelLeft_ { 0.0f };
    std::atomic<float> outputLevelRight_ { 0.0f };
    std::atomic<float> fundamentalHz_ { 0.0f };
    std::atomic<float> visualStrikeRadius_ { 0.0f };
    std::atomic<float> visualStrikeAngle_ { 0.0f };
    std::atomic<float> visualStrikeLevel_ { 0.0f };
    std::atomic<int> visualArticulation_ { 0 };
    std::atomic<int> visualOctave_ { 0 };
    float visualDecayMultiplier_ { 0.9999f };
    float visualLevel_ { 0.0f };
};

} // namespace taikor
