#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace taikor
{

// The strokes of the kumi-daiko vocabulary, one per pitch class, four to a
// drum. An octave of the keyboard is one drum and its four bottom semitones are
// the four things a player does to it, so the whole instrument is a 4x4 grid:
// four drums down the keyboard, four strokes across each of them.
//
// Four strokes, and each is a different mechanism rather than a different
// amount of the same one: the middle of the head, the edge out by the tacks,
// the middle with a hand on it, and the head and the hoop together. There were
// eight, and the four that went were either duplicates or specialities - Su is
// a light Don and velocity already covers it; Katsu, Buzz and Bachi are one
// technique each and none of them is a drum stroke a grid has to spend a key on.
enum class Articulation : std::uint8_t
{
    Don,     // C  - full open stroke, a hand's width in from the middle
    Ka,      // C# - out on the head near the tacks, thin and cutting
    Tsu,     // D  - damped centre, the free hand resting on the head
    DonRim,  // D# - head and hoop struck together, the loud accent
    Count
};

inline constexpr std::size_t articulationCount =
    static_cast<std::size_t> (Articulation::Count);

// An odaiko's fundamental can still be running after ten seconds at low
// damping, so the host is told to keep the tail alive that long.
inline constexpr double maximumTailSeconds = 12.0;

// Playable range: four octaves, C3..B6, one drum per octave. Each octave is a
// different instrument of the taiko family rather than the same drum rescaled -
// see getDrumDescription - and the four strokes sit on the bottom four
// semitones of each. Everything else is silent.
inline constexpr int lowestPlayableNote = 48;   // C3
inline constexpr int highestPlayableNote = 95;  // B6
inline constexpr int referenceNote = 48;        // C3
inline constexpr int lowestOctaveOffset = 0;
inline constexpr int highestOctaveOffset = 3;
inline constexpr int drumCount = highestOctaveOffset - lowestOctaveOffset + 1;

// The rate the static measurement assumes when a caller has no host to ask.
// It matters because which of a drum's modes the renderer can instantiate at
// all is a question about the host's clock - see TaikoEngine::measure.
inline constexpr double defaultSampleRate = 48000.0;

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

// One of the four drums the keyboard lays out, described as the instrument it
// is rather than as a size. Every field here is a physical property of a real
// member of the taiko family - what it is built out of, how deep its body is,
// how thick its hide is and how hard that hide is pulled - so the four octaves
// are four instruments and not one instrument at four scales. A rescaling
// cannot change a ratio, and these drums differ in their ratios: see
// Docs/best-in-class-plan.md for the measured spread.
//
// The four control fields are in the same units EngineParameters uses, so the
// player's own controls can be carried across the family as a trim on them
// (see TaikoEngine::parametersForOctave).
struct DrumDescription
{
    std::string_view displayName;
    std::string_view slug;
    // What the drum is, in one line, for the octave strip's tooltip.
    std::string_view summary;
    // Head diameter in metres, as the instrument is actually built.
    float headDiameterMetres { 0.95f };
    // Body depth, head tension, head material and shell material in control
    // units. See drumDescriptionTable in TaikoEngine.cpp for where each number
    // comes from.
    float bodyDepth { 0.5f };
    float tension { 0.55f };
    float headMaterial { 0.75f };
    float shellMaterial { 0.8f };
};

// The drum an octave plays. Out-of-range offsets are clamped, so this is always
// safe to call with whatever a host sent.
[[nodiscard]] const DrumDescription& getDrumDescription (int octaveOffset) noexcept;

// Every control the player has over the instrument. The first block describes
// the physical drum, the second how it is struck, and the third the close pair
// of microphones in front of it. Nothing here is a voicing preset: each field
// feeds a term of the model, so the defaults describe one specific drum - a
// 1.50 m odaiko with a thick cowhide head on a heavy zelkova shell,
// struck with a medium-hard oak bachi - rather than the midpoint of every axis.
struct EngineParameters
{
    // --- The drum -------------------------------------------------------
    // Head diameter in metres. Sets the membrane radius directly, so it moves
    // pitch as 1/a. The modal ratios move too, but only through the head's own
    // bending stiffness measured against its tension - see stiffnessStretch.
    float headDiameter { 1.50f };
    // Body depth as a fraction of the diameter, 0 -> 0.40, 1 -> 1.30. The
    // enclosed volume is what couples the two heads, so a shallow drum splits
    // its axisymmetric modes much further apart than a deep one.
    float bodyDepth { 0.5f };
    // Head tension. Mapped geometrically onto 1.2..22 kN/m, the range a tacked
    // or rope-laced hide actually occupies. Wave speed is sqrt(T/sigma), so
    // this and the head material together set the pitch.
    float tension { 0.62f };
    // 0 = thin synthetic film, 1 = thick heavy cowhide. Sets the head's areal
    // density, its internal loss factor and its bending stiffness at once,
    // because all three come from the same piece of material - and the last of
    // them goes as the cube of the thickness, so the two ends of this control
    // are two and a half orders of magnitude apart in how far they open the
    // modal ratios out.
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
    // Depth of the attack pitch glide, which is the head stretching itself: a
    // membrane clamped at its rim gets longer when it moves, and a longer head
    // is a tighter one. At 0 the head is treated as linear.
    float tensionModulation { 0.4f };
    // Level of the broadband contact noise the stick makes on the hide, 0..1.
    float strikeNoise { 0.35f };
    // Per-stroke variation in position, angle, impact speed and contact time.
    // 0 makes every stroke of a given articulation identical.
    float humanise { 0.4f };
    // What an octave up the keyboard actually changes, 0..1. At 0 the drum
    // described by the controls above is simply retuned - same head, same body,
    // same hide, more tension - which is one drum played four times. At 1 each
    // octave is its own instrument: the o-daiko, the chu-daiko, the okedo-daiko
    // and the shime-daiko of getDrumDescription, each with its own diameter,
    // body depth, hide thickness, tension and shell. In between, the drum is
    // blended from one towards the other.
    //
    // It also chooses how the residual tuning is taken, because the four drums
    // are real instruments and do not land on exact octaves by themselves: at 0
    // that residual is head tension, at 1 it is the drum's size. Both are how a
    // drum is actually brought to pitch, and at 1 it is a couple of per cent.
    float octaveBody { 1.0f };

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
    // Linear output gain. Set so that the loudest single stroke the instrument
    // has - a full-velocity rim shot on the reference drum, with the humanising
    // jitter pushing the impact speed as far as it goes - still clears the
    // safety limiter. It came down from 0.100000 when the reference o-daiko went
    // from three shaku to five: a rim shot catches the hoop and the body as well
    // as the head, and the body of a five-shaku drum is a great deal more of
    // the stroke.
    float outputGain { 0.075000f };

    [[nodiscard]] bool operator== (const EngineParameters&) const noexcept = default;
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
    // Lightweight analytic pitch estimate for the head animation, in hertz.
    // The numerical editor readout uses measureDrum(), whose soundingHz runs
    // the exact nonlinear contact audit off the audio thread.
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
    // Lightweight analytic pitch estimate for the octave last played. This is
    // safe to publish from trigger(); use measureDrum().soundingHz when the
    // exact post-contact sounding partial is required.
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
        // The pitch the drum is heard at: the mode that reaches the microphones
        // with the most energy over the window a struck note's pitch is taken
        // from, under the stroke Strike Position currently describes. On a small
        // tightly laced head that is the loaded fundamental above; on a large
        // slack one it is not, because the fundamental displaces no net air and
        // the mounting empties it in half a second while the (1,1) mode a fifth
        // and a half above it rings on. Moving the stick towards the middle of
        // the head changes it again, because a centred stroke cannot drive a
        // mode with a nodal diameter at all.
        //
        // Closely related to, but not the same as, the quantity the keyboard's
        // octaves are solved against. That one is a *latched* mode of the drum
        // evaluated at the centred stroke - see tuningModeFor and
        // tuningStrikeRadius - so that neither Strike Position nor a near-tie
        // between two modes can retune the instrument. The two agree at and
        // near the four instruments the family table describes, which is what
        // puts the factory keyboard on heard octaves; they part company on a
        // drum the controls have taken a long way from those, and where they do
        // it is this figure that is right about what you can hear.
        //
        // Zero means the drum has no membrane tone at this sample rate, and it
        // is the only value in this struct that is a marker rather than a
        // measurement. The renderer refuses every mode at or above 0.98 of
        // Nyquist, and a very small head at the tension ceiling taken up the
        // keyboard can put its *lowest* membrane mode past that: at Head
        // Diameter 15 cm, Head Tension 1.0, a thin film and Pitch +12 the top
        // pad's fundamental is 25565 Hz, which no 44.1 or 48 kHz host will ever
        // sound. There is then no partial to name, and naming one anyway - as
        // this used to, by ranking modes the renderer had already thrown away -
        // is reporting a pitch that is not in the audio. See soundingMode.
        float soundingHz { 88.0f };
        // How long the drum audibly rings: the longer-lived of the two
        // axisymmetric branches, each solved with its own radiation share.
        // Reporting only one of them described whichever mode happened to be
        // chosen rather than the drum.
        float tailSeconds { 1.2f };
        // How stiff the head is against its own tension: the dimensionless B in
        // f(lambda) = f_membrane(lambda) * sqrt((1 + B lambda^2)/(1 + B
        // lambda_0^2)). Zero is an ideal membrane, where every modal ratio is a
        // constant of the geometry; a thick hide on a small tight drum reaches
        // the order of 10^-3, which stretches the top of the resolved bank by
        // well over a semitone.
        float headStiffnessParameter { 0.0f };
        // The enclosed air's stiffness as a fraction of the lumped rho c^2 / L
        // an infinite spring would give. A drum's cavity is a column of finite
        // length, and its exact input stiffness is x cot x times the lumped
        // value with x = omega L / 2c - one only as the wavelength runs away
        // from the body, and falling towards zero as the half column
        // approaches its quarter-wave resonance, where it stops tying the two
        // heads together at all. It is reported because it is solved for: the
        // frequency it is evaluated at is the frequency it sets, so the drum
        // resolve converges on it rather than computing it.
        float cavityStiffnessFactor { 1.0f };
    };

    [[nodiscard]] DrumMeasurements measureDrum (int octaveOffset) const noexcept;
    // The same measurement without an engine instance, so the editor can show
    // what drum the current controls describe without keeping a second copy of
    // the whole voice pool alive to ask.
    //
    // The sample rate is here because one of the figures depends on it:
    // `soundingHz` names a partial the renderer will actually instantiate, and
    // which partials those are is decided by the host's clock. Everything else
    // in the struct describes the drum rather than the render and is the same
    // at every rate. An instance uses its own prepared rate; a caller without
    // one gets 48 kHz.
    [[nodiscard]] static DrumMeasurements measure (
        const EngineParameters& parameters, int octaveOffset,
        float pitchBendSemitones = 0.0f,
        double sampleRateHz = defaultSampleRate) noexcept;

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

    struct ContinuumVarianceCacheEntry
    {
        float lowCoefficient { 0.0f };
        float highCoefficient { 0.0f };
        float variance { 1.0f };
        bool valid { false };
    };

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
        // Physical contact data kept separate from the legacy force-integration
        // gain above. inverseModalMass belongs to the canonical degree of
        // freedom; contactShape belongs to the strike used while building a
        // scratch projection. The nonlinear contact path uses the same shape
        // to sense head displacement and to spread force back into the bank.
        float inverseModalMass { 0.0f };
        float contactShape { 0.0f };
        // Batter-head participation and the area-averaged gradient norm of
        // this spatial basis. Together they recover the membrane strain that
        // drives Berger/von Karman tension without depending on output scale.
        float batterParticipation { 0.0f };
        float stretchNorm { 0.0f };
        float micLeft { 0.0f };
        float micRight { 0.0f };
        // Resting angular frequency in radians per second, kept so the tension
        // glide can retune the resonator without redoing the physical solve.
        float omega { 0.0f };
        // The physical radian frequency and pole radius currently encoded in
        // the resonator. Unlike omega, these follow Tension Mod, automation and
        // the wheel. A collision needs both, and caching values already known
        // when the coefficients move avoids decomposing every live pole with
        // transcendental functions at every hit.
        double liveOmega { 0.0 };
        double poleRadius { 0.0 };
        // Structural amplitude-decay rate. The resonator radius also includes
        // appliedPalmDecay while a hand is down; keeping the two separate lets
        // retuning recompute material/radiation loss without dropping the palm.
        float decayRate { 0.0f };
        float appliedPalmDecay { 0.0f };
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
        // Radiation moves with the mode as well, through its efficiency: ka
        // climbs with frequency and the efficiency climbs with it until the
        // mode is large against the sound it makes. Everything in front of that
        // - the calibration, the air, and how much volume the mode shifts - is
        // fixed, so it is kept here and the efficiency re-evaluated at whatever
        // frequency the head has been stretched to.
        float radiationPrefactor { 0.0f };
        // Velocity-loss rate contributed while a muted articulation keeps its
        // palm-sized local damping patch on the batter head.
        float localMuteDampingRate { 0.0f };
        // Full-pressure CC1 palm loss at this mode, in inverse seconds. CC1
        // has no position channel, so the controller uses one fixed central
        // palm patch and scales this physical rate by the squared pressure.
        float handDampingRate { 0.0f };
        std::uint8_t circumferentialOrder { 0 };
        // Which row of the mode table this came from, so a later stroke can
        // find the mode's shape at its own contact point without the whole
        // Bessel solve being redone per mode. Membrane modes only.
        std::uint8_t modeEntry { 0 };
        // Stable construction identity inside one physical drum: two slots per
        // membrane table row, followed by the six shell modes. Per-contact
        // projections use this key to sum their forces before the one canonical
        // resonator bank is advanced; lifetime sorting may move the Mode object
        // but can never change which physical degree of freedom it names.
        std::uint8_t physicalIndex { 0 };
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
        // False for a lightweight strike/contact slot, true for one of the four
        // canonical physical drum banks. During the migration the two share a
        // storage type so the extensively tested geometry/projection builder
        // remains one source of truth; only a physical bank's resonator and
        // continuum states are ever rendered.
        bool physicalBank { false };
        // Generation of the drum geometry, material, loss and observation
        // parameters used to build a canonical bank. Strike slots leave this
        // at zero. A changed generation is rebuilt by stable physical mode ID
        // while preserving displacement and velocity, so automation cannot
        // leave a shared bank frozen at the first value it happened to see.
        std::uint64_t configurationRevision { 0 };
        // APVTS Pitch used by the exact drum solve that built this bank. Pitch
        // automation retunes a ringing bank cheaply; the next strike compares
        // this value and remaps the bank once, outside the render loop, so its
        // exact eigenvectors and modal masses match the new contact projection.
        float configurationPitch { 0.0f };
        bool active { false };
        Articulation articulation { Articulation::Don };
        int octaveOffset { 0 };
        // physicalDrums_ index for octaveOffset, i.e. octaveOffset clamped to
        // the playable range and rebased at zero. octaveOffset is only ever
        // assigned from an already-clamped octave (see trigger() and
        // ensurePhysicalDrum()), so this is exact for the voice's whole
        // lifetime and lets the per-sample render loop use it directly
        // instead of re-deriving it - clamp, subtract, cast - on every
        // sample of every active voice.
        std::uint8_t physicalDrumIndex { 0 };
        std::uint64_t startOrder { 0 };
        std::uint64_t ageSamples { 0 };
        std::uint64_t maximumSamples { 0 };
        std::uint32_t noiseState { 1u };

        std::array<Mode, resonatorCount> modes {};
        int modeCount { 0 };
        int activeModeCount { 0 };
        // Force already projected into stable physical-mode order. Every due
        // contact on this drum adds here, then the bank consumes and clears it
        // in one tick. This is the structural guarantee that two simultaneous
        // hits feed one recurrence rather than create two copies of the drum.
        std::array<float, resonatorCount> modalInput {};
        // Strike slots keep only these stable-ID force projections after the
        // geometry builder has run; their temporary Mode objects are cleared
        // before trigger() returns. Physical banks leave this array unused.
        std::array<float, resonatorCount> modeProjection {};
        // Dimensionless, reciprocal membrane footprint in the same stable-ID
        // order. Unlike modeProjection this contains neither modal mass nor a
        // time-integration gain, so it can both sense p^T q and apply p F.
        std::array<float, resonatorCount> contactProjection {};

        std::array<ContactEvent, maxContactEvents> contacts {};
        int contactCount { 0 };
        int nextContact { 0 };
        // Running contact, if one is in progress.
        std::uint32_t contactRemaining { 0u };
        std::uint32_t contactLength { 0u };
        float contactAmplitude { 0.0f };
        float contactNoiseAmplitude { 0.0f };
        // Dynamic bachi state for the reciprocal Hunt-Crossley contact. The
        // stick coordinate is positive into the head. Its two positions share
        // the same sample instants as every modal y1/y2 pair, so compression is
        // obtained directly without a separately integrated velocity state.
        bool nonlinearContactActive { false };
        bool nonlinearContactHasForce { false };
        bool continuumInjected { false };
        double stickPosition { 0.0 };
        double stickPrevious { 0.0 };
        double stickMass { 0.1 };
        double contactStiffness { 0.0 };
        double contactDamping { 0.0 };
        double residualImpedance { 1.0 };
        double referenceContactEnergy { 1.0 };
        double solvedContactEnergyStep { 0.0 };
        float solvedContactForce { 0.0f };
        // Amplitude of the stroke's first contact, so a later one can relight
        // the continuum in proportion to it.
        float contactReference { 0.0f };
        float noiseBandState { 0.0f };
        float noiseBandCoefficient { 0.5f };

        // The tack line. A nagado-daiko is byo-uchi: the head is nailed to the
        // shell with a ring of iron tacks, and each of them holds down its
        // share of the head's tension. A stroke that catches the hoop lifts the
        // head against that preload, and where it wins the tack chatters
        // against the wood. It is a threshold and not a level - below the
        // preload nothing rattles at all, which is why a firm rim shot has a
        // metallic edge a light one has no trace of.
        float tackPreload { 0.0f };      // newtons a stroke has to beat
        float tackRimGain { 0.0f };      // how much of the stroke reaches the hoop
        float tackScale { 0.0f };        // level per newton of excess
        // Its own noise source, not the stroke's. Sharing one would mean that
        // whether the tacks rattled decided which numbers the hide's contact
        // noise and the head's continuum were given, so two renders that differ
        // only in whether a rim shot cleared the preload would differ
        // everywhere - which makes the rattle impossible to measure and the
        // rest of the stroke needlessly dependent on it.
        std::uint32_t tackNoiseState { 1u };
        // A lifted tack does not go quiet the instant the stick leaves: it
        // chatters against the wood while the head settles back onto it, which
        // is a few milliseconds rather than the one the contact lasts.
        float tackEnvelope { 0.0f };
        float tackEnvelopeDecay { 0.99f };
        float tackLowState { 0.0f };
        float tackHighState { 0.0f };
        float tackLowCoefficient { 0.5f };
        float tackHighCoefficient { 0.5f };

        // One band of the continuum: noise through a ninth-order band-pass,
        // under its own decaying envelope. It belongs to the head, so the hand
        // damps it along with the resolved modes.
        struct ContinuumBand
        {
            // The low states are two cascaded high-pass stages and the high
            // states are seven cascaded low-pass stages. This is deliberately a
            // serial band-pass rather than the difference of two low-passes:
            // two low-passes both approach unity at DC, so their difference
            // only has a first-order zero however many poles each side has.
            // That old topology let the loudest, lowest continuum band mask
            // all four bands above it. The serial topology has a genuine
            // second-order lower skirt and a seventh-order upper skirt. The extra
            // poles are spent only where masking can happen: a louder
            // low band leaking upward into a quieter high one.
            float lowStateLeft { 0.0f };
            float lowStateLeft2 { 0.0f };
            float highStateLeft { 0.0f };
            float highStateLeft2 { 0.0f };
            float highStateLeft3 { 0.0f };
            float highStateLeft4 { 0.0f };
            float highStateLeft5 { 0.0f };
            float highStateLeft6 { 0.0f };
            float highStateLeft7 { 0.0f };
            float lowStateRight { 0.0f };
            float lowStateRight2 { 0.0f };
            float highStateRight { 0.0f };
            float highStateRight2 { 0.0f };
            float highStateRight3 { 0.0f };
            float highStateRight4 { 0.0f };
            float highStateRight5 { 0.0f };
            float highStateRight6 { 0.0f };
            float highStateRight7 { 0.0f };
            float lowCoefficient { 0.5f };
            float highCoefficient { 0.5f };
            // What the rim takes from this band, which is a constant of the
            // band and not of the tuning. The share is set by how many
            // circumferential orders the head carries at this frequency - its
            // dimensionless wavenumber, omega a / c - and stretching a head
            // raises omega and c together, so that number does not move. Only
            // the hide's share follows the bend.
            float lossFixed { 0.0f };
            // Physical RMS before the band-pass throws most of a white-noise
            // input away. Kept separately so tests can inspect the calibrated
            // energy rather than the filter's compensating input gain.
            float targetRms { 0.0f };
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
        // Calibrated RMS injected into the one physical residual field when a
        // scheduled contact begins. Kept outside ContinuumBand because a
        // strike owns an injection, never filter or noise state.
        std::array<float, continuumBandCount> continuumInjection {};

        // Attack pitch glide. A membrane held at a fixed rim cannot move
        // transversely without getting longer, and a longer head is a tighter
        // one: the tension rises with the square of the displacement, so a
        // struck head starts sharp and settles as it empties. That is the whole
        // of the mechanism, and it is why the glide has no clock of its own -
        // it decays because the head does.
        //
        // tensionEnvelope is a peak-following area-mean squared slope recovered
        // from the membrane modes' Bessel gradient norms;
        // tensionDecay is its release per control tick; tensionDepth is the
        // geometry/material coefficient in front of it - the head's in-plane
        // stiffness against its tension, over radius squared and modelScale
        // squared. The live Tension Mod control multiplies it at each control
        // tick, so automation reaches a head that is already ringing.
        float tensionEnvelope { 0.0f };
        float tensionDecay { 0.999f };
        float tensionDepth { 0.0f };
        float appliedTensionShift { 1.0f };
        // The mounting loss this stroke was built with, kept so retuning can
        // re-evaluate it at the mode's new frequency.
        float mountLoss { 0.0f };
        float mountCorner { 80.0f };
        // The head's radius, for the ka that radiation efficiency is a function
        // of. Stretching a head does not change its size.
        float radiusMetres { 0.275f };
        // The hide's share of the continuum's decay: a term in omega from its
        // hysteresis and one in omega squared from its viscosity. Stored rather
        // than summed so a retuned band can be re-damped exactly, instead of by
        // scaling what it had - which compounds, and drifts with every block
        // the glide runs. The rim's share is per band and does not move at all;
        // it lives on the band, in lossFixed.
        float continuumLossOmega { 0.0f };
        float continuumLossOmegaSquared { 0.0f };
        // Added to every audible lifetime once the contact schedule is known,
        // and kept so a recomputed lifetime can have it put back.
        std::uint64_t retirementOffset { 0 };

        int localMuteTicksRemaining { 0 };
        float continuumMuteDampingRate { 0.0f };
        bool palmDampingActive { false };
        // Full-pressure CC1 velocity-loss rate for unresolved modal energy.
        // Its RMS envelope receives half this exponent; the rate is the
        // high-density limit of the same finite palm-area projection used by
        // the resolved modes.
        float continuumHandDampingRate { 0.0f };

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
        // A finite palm contact on motion that was already ringing. Kept
        // separate because Ka's small muteAmount is part of its established
        // constrained-head voicing, not a free hand left on the membrane.
        bool palmContact { false };
        // Contact schedule: single, flam, or press roll.
        int contactCount { 1 };
        // Rim contribution: a shot that catches the hoop as well as the head.
        float rimGain { 0.0f };
        // Shell mode retune, used by the strokes that catch the hoop to shorten
        // the body's ring.
        float shellFrequencyScale { 1.0f };
        float shellDecayScale { 1.0f };
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
        // Bending stiffness of each head against its own tension, as the
        // dimensionless B of stiffnessStretch(). A taiko head is a stretched
        // plate rather than an ideal membrane, so its modal ratios are not
        // constants of the geometry: they open out with the mode's order, and
        // they open out further the smaller and thicker the head is.
        float stiffnessBatter { 0.0f };
        float stiffnessResonant { 0.0f };
        // The head's in-plane stiffness measured against its own tension,
        // E h / ((1 - nu^2) T). It is the coefficient of (w/a)^2 in the tension
        // a clamped membrane gains from being displaced, and therefore the only
        // thing the attack pitch glide needs from the drum: a slack head bends
        // a long way sharp and a tight one barely moves.
        float stretchStiffness { 0.0f };
        float headLossFactor { 0.012f };
        // The viscous half of the hide's loss, damping as omega squared where
        // headLossFactor damps as omega. See resolveDrumFor.
        float headViscousFactor { 0.0f };
        float edgeLoss { 0.6f };
        // Cavity stiffness per unit area, before the per-mode 4/lambda^2
        // volume-efficiency weighting. Zero on an uncoupled (open) body.
        // This is the finite-column stiffness: the lumped rho c^2 / L already
        // multiplied by cavityColumnFactor below.
        float cavityStiffness { 0.0f };
        // How much of the lumped air spring the finite column actually
        // presents at the frequency the drum settles on, x cot x with
        // x = omega L / 2c. One at the low-frequency limit and falling as the
        // body gets deep against the wavelength. See resolveDrumFor.
        float cavityColumnFactor { 1.0f };
        float radiationScale { 0.10f };
        // Close-pair geometry, resolved once so every stroke places the mics
        // identically. Radius is in metres; angles are in radians.
        float micRadius { 0.0f };
        float micAngleLeft { 0.0f };
        float micAngleRight { 0.0f };
        float micDistanceMetres { 0.16f };
        float micProximity { 0.6f };
        // The width trim the output stage will apply to the finished pair. It
        // belongs to the drum's resolved state because it decides what a mode
        // is worth once the two capsules have been combined - and at width 0
        // that is a mono sum, in which a mode with a nodal diameter between the
        // two capsules very nearly cancels. See observeMode.
        float stereoWidth { 0.5f };
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
        // How long a full open stroke stays on this head, as the Hertz impact
        // solve returns it for the neutral impact speed. A stroke is a force
        // pulse of finite length, not an impulse, so it cannot drive a mode
        // whose period is shorter than the contact: everything above about
        // 1/tau comes out of the stroke already attenuated. That is the whole
        // of why a felt beater sounds dull, and it is the term that decides
        // which mode is loudest whenever two of them straddle 1/tau - see
        // observeMode and contactSpectrum.
        float contactSeconds { 0.0015f };
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
    // What the head's own bending stiffness does to a mode. A membrane under
    // tension T with flexural rigidity D obeys omega^2 = (T k^2 + D k^4)/sigma,
    // so the stiff term climbs as the square of the mode's wavenumber and the
    // modal ratios open out with order - the same mechanism that stretches a
    // piano's partials, on a head that Ando measured at 3.5 GPa.
    //
    // Taken relative to the (0,1) mode rather than applied absolutely, because
    // a drum is tuned by the pitch it sounds: a player brings the fundamental
    // back to where it belongs with the ropes or the tacks, and what stiffness
    // leaves behind afterwards is the spread above it, not a transposition.
    // That also keeps an octave an octave, which an absolute shift would not:
    // B falls as the tension rises and as the square of the radius, so the two
    // halves of the Octave Body transform move it in opposite directions.
    [[nodiscard]] static float stiffnessStretch (float besselZero,
                                                 float stiffness) noexcept;
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
    // Exact white-noise variance of the continuum's two-high-pass/seven-low-pass
    // cascade. Computed only while a voice is built; rendering needs the nine
    // one-pole state updates per channel and band, but no matrix work.
    [[nodiscard]] static float continuumBandVariance (float lowCoefficient,
                                                       float highCoefficient) noexcept;
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
    // x cot x, the exact reactive input stiffness of a rigidly terminated air
    // column relative to its own low-frequency limit, with x = omega l / c.
    // See resolveDrumFor for why it is floored at the quarter-wave.
    [[nodiscard]] static float columnStiffnessFactor (float x) noexcept;
    // The angular frequency of the volume-changing branch of the (0,1) pair for
    // a given cavity stiffness. The cavity correction is solved against this
    // branch because it is the only one the enclosed air stiffens.
    [[nodiscard]] static float volumeBranchOmega (const DrumState& drum,
                                                  float cavityStiffness) noexcept;
    [[nodiscard]] static std::uint32_t hash32 (std::uint32_t value) noexcept;
    [[nodiscard]] static float signedUnitFromHash (std::uint32_t value) noexcept;
    [[nodiscard]] static float nextNoise (std::uint32_t& state) noexcept;

    // The whole (0,1) pair of a resolved drum: both branches, their
    // eigenvectors, and which of the two the batter head can actually be heard
    // in. The readout reports it, and the octave transform is solved against
    // it, so the pitch the keyboard buys and the pitch the panel shows are the
    // same quantity by construction rather than by agreement.
    struct AxisymmetricPair
    {
        float upperHz { 0.0f };
        float lowerHz { 0.0f };
        float upperBatter { 0.0f };
        float upperResonant { 0.0f };
        float lowerBatter { 0.0f };
        float lowerResonant { 0.0f };
        bool upperAudible { false };
        bool lowerAudible { false };
        // What measure() reports. With both branches audible these are the two
        // above; with one, it is that one twice, because a body with no cavity
        // to split it has a single axisymmetric mode and saying so twice is the
        // honest description of it.
        float breathingHz { 0.0f };
        float loadedFundamentalHz { 0.0f };
    };
    [[nodiscard]] static AxisymmetricPair solveAxisymmetricPair (
        const DrumState& drum) noexcept;

    // The window a struck note's pitch is taken from: from the end of the
    // attack, where the stick's own noise has gone, to nine tenths of a second
    // later, by which time even a large drum has said what it is. Everything
    // that decides which mode a drum is heard at is a comparison over this
    // window - a loud mode that empties in a third of a second and a quiet one
    // that outlasts it are not ranked by level alone.
    static constexpr float pitchWindowStart = 0.08f;
    static constexpr float pitchWindowEnd = 0.98f;

    // How much of a stroke reaches a mode at `omegaTau`, where tau is the
    // contact time: the magnitude of the Hertz force pulse's own transform,
    // normalised at zero. The pulse the render drives the bank with is a
    // sin^1.5 arch of length tau, so this is
    //   |integral of sin(pi u)^1.5 e^(-i x u) du over [0,1]| / (2.3963 / pi),
    // which has no elementary closed form and is fitted here.
    [[nodiscard]] static float contactSpectrum (float omegaTau) noexcept;

    // One membrane mode of a resolved drum: where it is, what a stroke at
    // `strikeRadius` is worth in it at the microphones, how fast it empties,
    // and what the three together are worth over the window above.
    struct ModeObservation
    {
        float frequencyHz { 0.0f };
        float amplitude { 0.0f };
        float decayRate { 0.0f };
        float weight { 0.0f };
        // The same quantity in nepers, which is the one that survives a drum
        // whose modes are all emptied inside the pitch window. `weight` holds a
        // difference of two exponentials, and on a 15 cm head at the tension
        // ceiling both of them underflow to zero for every mode of the drum -
        // whereupon a comparison on `weight` alone has nothing to choose
        // between and reports no mode at all. This is computed from the
        // exponents rather than from the exponentials, so it stays finite as
        // far down as a float exponent reaches.
        float logWeight { -std::numeric_limits<float>::infinity() };
    };
    [[nodiscard]] static ModeObservation observeMode (const DrumState& drum,
                                                      int entryIndex, int branch,
                                                      float strikeRadius) noexcept;

    // Which mode a pitch is being taken in: a row of the mode table, and for
    // the axisymmetric rows which branch of the cavity-split pair. This is an
    // identity rather than a frequency, and it is the thing the octave
    // transform holds fixed - see tuningModeFor.
    struct ModeIdentity
    {
        std::uint8_t entryIndex { 0 };
        std::uint8_t branch { 0 };

        [[nodiscard]] bool operator== (const ModeIdentity& other) const noexcept
        {
            return entryIndex == other.entryIndex && branch == other.branch;
        }
    };

    // The mode a drum is heard at, which is the loudest one over that window
    // and is not always the lowest. See soundingMode's definition for why half
    // of this family is heard a fifth and a half above its own fundamental.
    struct SoundingMode
    {
        float frequencyHz { 0.0f };
        float weight { 0.0f };
        ModeIdentity identity {};
    };
    // `ceilingHz` bounds the comparison to the modes the caller's question is
    // about. The readout passes the renderer's own cutoff, so it can only name
    // a partial that will be in the audio; the octave transform passes
    // infinity, because which mode an instrument is tuned by is a property of
    // the instrument and must not follow the host's clock. See the definition.
    [[nodiscard]] static SoundingMode soundingMode (const DrumState& drum,
                                                    float strikeRadius,
                                                    float ceilingHz) noexcept;
    // The readout's excitation spectrum comes from the same passive contact
    // solve as the renderer. A felt bachi can ride a low mode for milliseconds,
    // which no prescribed pulse duration can predict truthfully.
    [[nodiscard]] static SoundingMode dynamicSoundingMode (
        const EngineParameters& parameters, int octaveOffset,
        float pitchBendSemitones, double sampleRateHz) noexcept;

    // The highest frequency the render will instantiate a resonator at. This
    // is configureResonator's own test, written once so the readout and the
    // renderer cannot drift apart: a mode at or above it is silently dropped
    // from the bank, so a readout that names one names a partial that is not
    // in the audio.
    [[nodiscard]] static float renderedModeCeilingHz (double sampleRateHz) noexcept;

    // The mode each octave of the family is tuned by: an identity latched from
    // the four instruments the table describes, and never re-chosen from the
    // player's controls. An argmax is a discontinuous function of every control
    // that feeds it, so tuning against one made a hundredth of a semitone of
    // Pitch automation drop a drum by a tenth of an octave and re-solve its
    // size; a latched identity is what lets the same solve be written
    // continuously. See the definition for what it does and does not depend on.
    [[nodiscard]] static ModeIdentity tuningModeFor (int octaveOffset,
                                                     float octaveBody) noexcept;

    // Where the octave transform takes its pitches from: a full open stroke on
    // the head with Strike Position centred. The transform is deliberately
    // anchored here rather than at the player's own strike position, so that
    // Strike Position stays a timbre control with no tuning side effect.
    [[nodiscard]] static float tuningStrikeRadius() noexcept;
    // Where a Don actually lands with the controls as they are, which is what
    // the readout has to describe: an off-centre strike drives a different
    // balance of modes and is genuinely heard at a different pitch.
    [[nodiscard]] static float readoutStrikeRadius (
        const EngineParameters& parameters) noexcept;

    // The drum an octave is built from: the player's controls carried across
    // the family by whichever of the four instruments this octave plays, with
    // Octave Body choosing how much of that instrument is taken. At octave 0,
    // and at Octave Body 0 anywhere, this is the parameter block unchanged.
    [[nodiscard]] static EngineParameters parametersForOctave (
        const EngineParameters& applied, int octaveOffset) noexcept;
    // Resolving a drum depends only on the parameter block, the wheel and the
    // octave, so it is static and the instance method simply supplies its own.
    [[nodiscard]] static DrumState resolveDrumFor (const EngineParameters& parameters,
                                                   float pitchBendSemitones,
                                                   int octaveOffset) noexcept;
    // The head and the air behind it for one choice of the octave transform:
    // geometry, tension, wave speeds, bending stiffness, the loss terms and the
    // converged cavity stiffness. Split out of resolveDrumFor because the
    // octave transform is now solved against the (0,1) pair, so everything the
    // pair depends on runs several times per octave while the shell, the
    // mounting and the microphones run once, from the answer.
    static void resolveDrumGeometry (const EngineParameters& applied,
                                     float radiusFactor,
                                     float tensionOctaveFactor,
                                     float tensionPitchFactor,
                                     DrumState& drum) noexcept;
    [[nodiscard]] DrumState resolveDrum (int octaveOffset) const noexcept;
    // Hertz impact, returning the contact duration in seconds and the peak
    // force. Contact time follows impact speed as v^(-1/5) and is floored by
    // the struck body's own resistive impedance, because the stick cannot leave
    // faster than that body carries the energy away. The caller supplies both
    // the striking mass and that impedance, so the same solver serves a stick
    // meeting a head and a stick meeting another stick.
    static void solveContact (float collisionMass, float targetImpedance,
                              const StrikeProfile& profile, float bachiHardness,
                              float impactSpeed, float& contactSeconds,
                              float& peakForce) noexcept;
    [[nodiscard]] static float contactStiffnessFor (
        const StrikeProfile& profile, float bachiHardness) noexcept;
    // The striking mass and the head's resistive impedance for a drum stroke.
    static void drumContactTerms (const DrumState& drum, float& strikerMass,
                                  float& impedance) noexcept;
    // Reduced mass of the bachi and the resolved head at one contact patch.
    // The paired angular/cavity branches are complete orthogonal bases, so
    // their squared participation sums without knowing their orientation.
    [[nodiscard]] static float contactCollisionMass (
        const DrumState& drum, const StrikeProfile& profile,
        float strikeRadius, float strikerMass) noexcept;
    void buildVoiceModes (Voice& voice, const DrumState& drum,
                          const StrikeProfile& profile, float extraDamping) noexcept;
    // A muted Tsu leaves a finite-area free-hand damper on the one canonical
    // head. This schedules that local passive loss; bachi/head momentum
    // exchange belongs to advancePhysicalContacts().
    void dampPhysicalDrum (Voice& physical, const StrikeProfile& profile,
                           float strikeRadius, const DrumState& drum) noexcept;
    static void palmDampingRates (
        const DrumState& drum, float strikeRadius,
        std::array<float, modeEntryCount>& modeRates,
        float& continuumRate) noexcept;
    void ensurePhysicalDrum (int octave, const DrumState& drum) noexcept;
    void scheduleContacts (Voice& voice, const StrikeProfile& profile,
                           float contactSeconds, float peakForce,
                           float noiseLevel) noexcept;
    void applyTensionShift (Voice& voice, float shift) noexcept;
    void updateVoiceControl (Voice& voice) noexcept;
    void advancePhysicalContacts (Voice& physical) noexcept;
    [[nodiscard]] float renderVoice (Voice& voice, Voice* physical,
                                     float& rightOut) noexcept;
    [[nodiscard]] int findVoiceSlot() noexcept;
    void silenceVoice (Voice& voice) noexcept;
    void updateActiveVoiceCount() noexcept;
    void refreshDrumIfNeeded() noexcept;
    // Changes continuous pole loss while preserving instantaneous displacement
    // and physical velocity. `amplitudeDecay` is the palm's extra exponent.
    void setPalmDecay (Mode& mode, float amplitudeDecay) noexcept;
    // Legacy/test utility for an instantaneous passive velocity-retention step.
    // Continuous Tsu and CC1 palms use setPalmDecay() instead.
    static void applyCollisionRetention (Mode& mode, float retention) noexcept;
    // Radial coordinates of the symmetric centre-plus-cardinals palm rule.
    static std::array<float, 5> palmPatchRadii (float centreRadius,
                                                float patchRadius) noexcept;
    // Configures one resonator from a physical frequency and decay rate.
    void configureResonator (Resonator& resonator, float frequencyHz,
                             float decayRate, float gain) const noexcept;

    // Sanitised parameters, as published by setParameters(). Like the other
    // engines in this repository the setter is called from the audio thread
    // (or is otherwise externally synchronised with it), so a plain copy is
    // enough and no atomics are needed here.
    EngineParameters applied_ {};

    std::array<Voice, maxVoices> voices_ {};
    // The four keyboard octaves are four physical instruments. Strikes are
    // transient contacts routed into these banks; they never own resonators.
    std::array<Voice, drumCount> physicalDrums_ {};
    // Incremented only by controls that alter the physical bank. Pitch-bend
    // smoothing deliberately does not touch it: a wheel retunes the live poles
    // instead of rebuilding forty-six modes at audio rate.
    std::uint64_t physicalConfigurationRevision_ { 1 };
    // One resolved drum per playable octave - which is now one per instrument
    // of the family. Strokes are common and the solve involves a Bessel series
    // and an eigen-decomposition per mode, so it is done once per parameter
    // change rather than once per stroke.
    std::array<DrumState, drumCount> drumCache_ {};
    // Filter variance depends only on its two coefficients, yet the exact
    // Lyapunov solve used to be repeated five times for every simultaneous
    // note. One lazy entry per physical drum and band makes a chord reuse the
    // first note's exact result without changing any rendered state.
    std::array<std::array<ContinuumVarianceCacheEntry, continuumBandCount>, drumCount>
        continuumVarianceCache_ {};
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
