// Electry: an original, physically modelled dry electric guitar.
//
// The engine is a white-box model with named references, not a capture of any
// one instrument. Each block below names the published work it follows; what
// the model implements from each, and where the claim stops, is set out block
// by block in the README's "How it works". Constants not fixed by a cited
// result are voiced -- this project's choices inside a range the sources bound
// but do not pin.
//
//   String core, single-delay-loop waveguides
//     Karjalainen, Valimaki and Tolonen, "Plucked-String Models: From the
//     Karplus-Strong Algorithm to Digital Waveguides and Beyond", CMJ 1998.
//     http://users.spa.aalto.fi/vpv/publications/cmj98.pdf
//
//   Stiffness dispersion, B = pi^3 E d^4 / (64 T L^2)
//     Fletcher and Rossing, The Physics of Musical Instruments, for B itself;
//     Rauhala and Valimaki, "Dispersion modeling in waveguide piano synthesis
//     using tunable allpass filters",
//     https://www.researchgate.net/publication/229009513_Dispersion_modeling_in_waveguide_piano_synthesis_using_tunable_allpass_filters
//     and Abel and Smith, DAFx-06,
//     https://www.dafx.de/paper-archive/2006/papers/p_013.pdf
//     for the factored allpass design practice.
//
//   Dead spots
//     Fleischer, "Investigating Dead Spots of Electric Guitars",
//     https://www.researchgate.net/publication/233653803_Investigating_Dead_Spots_of_Electric_Guitars
//
//   Energy-derived tension modulation (default-off experiment)
//     Bank, "Physics-Based Sound Synthesis of the Piano",
//     https://dafx.de/paper-archive/2009/papers/paper_76.pdf
//     Lee et al., "Analysis of the Nonlinear Tension Modulation in Guitar
//     Strings", https://www.dafx.de/paper-archive/2009/papers/paper_86.pdf
//     Kemp et al., "A Method for Determining the Material Properties of
//     String Windings", https://doi.org/10.1371/journal.pone.0184803
//
//   Plectrum and finger excitation, touch and collisions
//     Germain and Evangelista, WASPAA 2009,
//     https://ieeexplore.ieee.org/document/5346502/
//     Evangelista and Eckerholm, "Player-Instrument Interaction Models for
//     Digital Waveguide Synthesis of Guitar: Touch and Collisions",
//     https://www.researchgate.net/publication/224130817_Player-Instrument_Interaction_Models_for_Digital_Waveguide_Synthesis_of_Guitar_Touch_and_Collisions
//
//   Palm and distributed hand/string contact
//     Biral, d'Alessandro and Freed, "Towards a Dynamic Model of the Palm Mute
//     Guitar Technique",
//     https://www.icmc14-smc14.net/images/proceedings/PS4-B10-TowardsaDynamicModel.pdf
//     Reboursiere et al., "Left and right-hand guitar playing techniques
//     detection", https://www.nime.org/proceedings/2012/nime2012_213.pdf
//     Schafer, Frenstatsky and Rabenstein, "A Physical String Model with
//     Adjustable Boundary Conditions",
//     https://dafx.de/paper-archive/2016/dafxpapers/23-DAFx-16_paper_24-PN.pdf
//     Exact corpus comparisons and their limits: Docs/evaluation.md.
//
//   Fret collisions
//     Bilbao and Torin, "Numerical modeling and sound synthesis for
//     articulated string/fretboard interactions",
//     https://www.research.ed.ac.uk/en/publications/numerical-modeling-and-sound-synthesis-for-articulated-stringfret/
//
//   Slide, and the winding contact noise
//     Pakarinen, Puputti and Valimaki, "Virtual Slide Guitar",
//     https://research.aalto.fi/en/publications/virtual-slide-guitar
//     NIME 2008 companion:
//     https://www.nime.org/proceedings/2008/nime2008_049.pdf
//
//   Pickups
//     Paiva, Pakarinen and Valimaki, "Acoustics and Modeling of Pickups",
//     https://www.researchgate.net/publication/234034228_Acoustics_and_Modeling_of_Pickups
//     Novak et al., "Measurements and Modeling of the Nonlinear Behavior of a
//     Guitar Pickup at Low Frequencies",
//     https://www.researchgate.net/publication/312046898_Measurements_and_Modeling_of_the_Nonlinear_Behavior_of_a_Guitar_Pickup_at_Low_Frequencies
//     Aperture analysis: https://www.cycfi.com/2014/08/virtual-pickups-part-3/
//
//   Sympathetic coupling and bridge admittance
//     Bank, "Model-based digital pianos ... in real time",
//     https://home.mit.bme.hu/~bank/publist/dafx10adm.pdf
//     Maestre et al., "Joint Modeling of Impedance and Radiation as a Recursive
//     Parallel Filter Structure for Efficient Synthesis of String Instrument
//     Sound by Digital Waveguides",
//     https://caml.music.mcgill.ca/lib/exe/fetch.php?media=publications%3Amaestre_jointmodeling_ieeeaslp_2017.pdf
//
//   Solid-body response, decay and direct vibro-electric transfer
//     Paté, "Lutherie de la guitare électrique solid body : aspects
//     mécaniques et perceptifs", doctoral thesis, Tables 10.1-10.2,
//     https://www.lam.jussieu.fr/Publications/Theses/these-arthur-pate.pdf
//     Paté, Le Carrou and Fabre, "Predicting the decay time of solid body
//     electric guitar tones", JASA 135(5), 3045-3055,
//     https://www.lam.jussieu.fr/Membres/LeCarrou/Articles/A8_Pate_PredictingDecayTime.pdf
//     Ray, Kaljun and Straže, "Comparison of the Vibration Damping of the Wood
//     Species Used for the Body of an Electric Guitar on the Vibration
//     Response of Open-Strings", Materials 14(18),
//     https://pmc.ncbi.nlm.nih.gov/articles/PMC8465587/
//     Elliott, Magill and Kendrick, "Vibro-Electric Transfer Path Analysis of
//     a Solid Body Electric Guitar",
//     https://salford-repository.worktribe.com/OutputFile/1490211
//
//   Amplifier and cabinet
//     Pakarinen and Yeh, "A Review of Digital Techniques for Modeling
//     Vacuum-Tube Guitar Amplifiers", CMJ 2009,
//     https://direct.mit.edu/comj/article/33/2/85/94374/A-Review-of-Digital-Techniques-for-Modeling-Vacuum
//
//   Why the runtime stays analytic rather than a solved FDTD or a learned model
//     Bilbao et al., "Real-Time Guitar Synthesis",
//     https://www.pure.ed.ac.uk/ws/portalfiles/portal/470239305/BilbaoEtal2024RealTimeGuitarSynthesis.pdf
//     and the NeurIPS 2024 sound-and-motion simulation line,
//     https://arxiv.org/abs/2407.05516
//     -- both are why the cost model rules those out for an eight-string
//     realtime voice, not why they would be wrong.

#pragma once

#include "DspMath.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#ifndef ELECTRY_ANALYTIC_RELEASE_IC
#define ELECTRY_ANALYTIC_RELEASE_IC 0
#endif

#ifndef ELECTRY_DECOUPLED_PICK_RELEASE
#define ELECTRY_DECOUPLED_PICK_RELEASE 0
#endif

#ifndef ELECTRY_MEASURED_BODY_RESPONSE
#define ELECTRY_MEASURED_BODY_RESPONSE 0
#endif

#ifndef ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
#define ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2 0
#endif

#ifndef ELECTRY_ENERGY_ATTACK_PITCH
#define ELECTRY_ENERGY_ATTACK_PITCH 0
#endif

#ifndef ELECTRY_POSITIONED_FRET_COLLISION
#define ELECTRY_POSITIONED_FRET_COLLISION 0
#endif

namespace electry
{

// How the plectrum moves. Latched by its own keyswitch bank, independently of
// the play style, so any stroke can drive any style. Alternate resolves to a
// concrete Down/Up stroke per accepted note-on.
enum class PickStyle
{
    Down,
    Up,
    Alternate
};

// What the hands do to the string: the sustained default, the bridge-hand
// palm mute, the fretting-hand legato (hammer-on / pull-off), the natural
// harmonic touch at the midpoint node, the pinch harmonic, where the picking
// hand's thumb catches the string at the pick's own position, or the slide,
// where the finger stays down and travels along the string, or the dead note,
// where the fretting hand rests across the strings without stopping them so the
// pick makes its attack and leaves a short, dark percussive body. Latched by
// its own keyswitch
// bank, independently of the picking style. New styles are appended, so every
// existing keyswitch note keeps its meaning.
enum class PlayStyle
{
    Sustain,
    PalmMute,
    Hammer,
    Harmonics,
    Pinch,
    Slide,
    Dead
};

enum class PickupSelector { Neck, Both, Bridge };
enum class OutputMode { Mono, Stereo };

inline constexpr float defaultGuitarBuild = 0.8f;

// Shipping moves the one supported walnut-to-ash material coordinate through
// Ray's paired pickup-observed modes while Guitar Build visits six curated
// scale/gauge setups. Size, Shape and Construction remain neutral because the
// reviewed evidence does not identify independent reusable laws for them. The
// 0.8 default retains the fitted 27.6-inch heavy-set Drop-E setup. The legacy
// four-mode construction path remains available only as an explicit comparator.
struct EngineParameters
{
    PickupSelector pickupSelector { PickupSelector::Bridge };
    // Shipping: the narrow walnut-like to ash-like material contrast supported
    // by Ray's controlled pair. The legacy comparator used broader voiced
    // mahogany/maple-like to swamp-ash-like labels.
#if ELECTRY_MEASURED_BODY_RESPONSE
    float bodyWood { defaultGuitarBuild };
#else
    float bodyWood { 0.0f };
#endif
    // The measured shipping model has no physical dimension behind this
    // coordinate and deliberately ignores it.
    float bodySize {
#if ELECTRY_MEASURED_BODY_RESPONSE
        0.5f
#else
        0.0f
#endif
    };
    // The shipping model has no independently measured shape law and ignores
    // this legacy carved-single-cut-to-flat-slab coordinate.
    float bodyShape {
#if ELECTRY_MEASURED_BODY_RESPONSE
        0.5f
#else
        0.0f
#endif
    };
    // The shipping model has no independently measured joint law and ignores
    // this legacy set-neck/stopbar-to-bolt-on/through-body coordinate.
    float construction {
#if ELECTRY_MEASURED_BODY_RESPONSE
        0.5f
#else
        0.0f
#endif
    };
    float scaleLength { 0.85f };    // 0 = 25.5 in, 1 = 28 in
    float pickupType { 0.32f };     // 0 humbucker, 1 narrow single coil
    float toneKnob { 0.70f };       // guitar's own passive tone control
    float bodyResonance { 0.35f };  // solid-body structural colour level
    float stringGauge { 1.0f };     // 0 = .009-.080 set, 1 = .011-.098 set
    float stringAge { 0.30f };      // 0 fresh round-wounds, 1 old dead strings
    float pickPosition { 0.18f };   // 0 close to bridge, 1 over the neck
    float pickHardness { 0.58f };   // 0 soft/rounded contact, 1 stiff sharp pick
    float pickNoise { 0.5f };       // plectrum contact/scrape level
    float fingerNoise { 0.4f };     // fretting-hand contact level
    float releaseNoise { 0.4f };    // note-end damping/lift noise level
    float muteDamping { 0.55f };    // Palm Tightness for the Palm Mute style
    float bendTimeSeconds { 0.28f };// finger-bend travel time
    float velocityAmount { 0.85f }; // MIDI velocity to pluck strength
    float outputGain { 0.5f };      // linear output level
    float artifactAmount { 0.18f }; // hardware ring, saddle buzz and incidental contact
    OutputMode outputMode { OutputMode::Mono }; // authentic DI or hex/string field
    // Bridge-coupled sympathetic resonance of the strings that are not being
    // fingered. 0 bypasses the coupled waveguides exactly.
    float sympatheticAmount { 0.20f };
    // Continuous bridge-hand damping applied to every play style, independent
    // of the Muted/Chug keyswitches. 0 leaves the model untouched.
    float palmMute { 0.0f };
    // Per-string offset of a strummed chord, in seconds of pick travel per
    // string crossed. 0 keeps simultaneous note-ons exactly simultaneous.
    float strumSpreadSeconds { 0.0f };
    // Automatic held-string repick speed. The shipping 12 strokes/s is the
    // centre of the commissioned 8/12/16-strokes/s capture protocol and maps
    // to sixteenth notes at 180 BPM without making the engine transport-aware.
    float tremoloRateHz { 12.0f };
    // Full-scale depth of the CC1 resonance control: how far a fully raised
    // modulation wheel can push the sympathetic coupling toward total and how
    // much amplified output is allowed to feed back into the strings. At 1 a
    // raised wheel lets a distorted tone self-resonate; at 0 CC1 does nothing.
    float resonanceDepth { 0.35f };
    // Widest excursion the fretting-hand vibrato reaches at a full gesture,
    // mapped over the range a finger can actually cover: 10 cents
    // at 0 is the narrow rock a player uses to keep a held note alive, and
    // 110 cents at 1 is the semitone-wide arc of a rock vibrato leaned all
    // the way in. The shipping default puts it at 40 cents, which is where
    // the fixed excursion this control replaced sat.
    float vibratoDepth { 0.30f };
};

// The plug-in exposes the structural coordinates above as one playable path.
// Pickups, tone, body-resonance amount and playing controls remain independent.
void applyGuitarBuild(EngineParameters& parameters, float build) noexcept;

// Per-string readout for the editor's fretboard display. It is produced on
// the audio thread and consumed on the message thread through the host's own
// atomics, so it is deliberately small and trivially copyable.
struct StringVisualState
{
    bool sounding { false };      // a fingered/picked note owns this string
    bool sympathetic { false };   // ringing only through bridge coupling
    bool releasing { false };     // key released, damping ramp running
    int midiNote { -1 };          // sounding note, or -1
    int fret { -1 };              // stopped fret, or -1
    float level { 0.0f };         // 0..1 ballistic display level
    PlayStyle playStyle { PlayStyle::Sustain };
    bool strokeUp { false };      // the resolved stroke that started the note
};

class ElectryEngine
{
public:
    ElectryEngine() noexcept;

    using ExpressionId = std::uint8_t;
    static constexpr ExpressionId legacyExpressionId = 0;
    static constexpr ExpressionId maximumExpressionId = 16;

    static constexpr int stringCount = 8;
    static constexpr int fretCount = 22;
    // One timestamp can contain overlaps as well as eight distinct pitches.
    // The bound keeps host input hostile-proof without allocating on the
    // audio thread; ordinary MIDI/UI attack groups are far smaller.
    static constexpr int maximumChordEvents = 128;

    // Keyswitches occupy one contiguous group below the playable range,
    // starting at 12 (C0): first the picking-style bank (Down/Up/Alternate),
    // then the play-style bank (Sustain/PalmMute/Hammer/Harmonics/Pinch/
    // Slide/Dead). The two banks latch independently, so any of the twenty-one
    // combinations can be reached in two keyswitches at most. A#0 (22) is the
    // plug-in's momentary fretting-vibrato gesture and B0 (23) its momentary
    // tremolo-picking wrist; the remaining notes before the playable range
    // (24..27) are ignored.
    static constexpr int firstKeyswitchNote = 12;
    static constexpr int pickStyleKeyswitchCount
        = static_cast<int>(PickStyle::Alternate) + 1;
    static constexpr int playStyleKeyswitchCount
        = static_cast<int>(PlayStyle::Dead) + 1;
    static constexpr int keyswitchCount = pickStyleKeyswitchCount
                                        + playStyleKeyswitchCount;
    static constexpr int firstPlayStyleKeyswitchNote
        = firstKeyswitchNote + pickStyleKeyswitchCount;
    static constexpr int vibratoGestureNote = 22; // A#0, hold; velocity = width
    static constexpr int tremoloGestureNote = 23; // B0, hold; velocity = force
    // Drop-E eight-string, 22-fret instrument: open low E1 to fret 22 on E4.
    static constexpr int lowestPlayableNote = 28;
    static constexpr int highestPlayableNote = 86;
    // E6..B6 repick the physically held strings from low to high without
    // adding another fretting-key owner. D#6 remains a silent separator.
    static constexpr int firstRepickNote = 88;
    static constexpr int repickNoteCount = stringCount;
    // How far the fretting hand reaches above its index finger. Four fret
    // spaces is one finger per fret with the ordinary stretch a player uses
    // without shifting, so the hand covers `position .. position + reach`.
    static constexpr int frettingHandReach = 4;

    static_assert(keyswitchCount == 10,
                  "three picking styles and seven play styles need one keyswitch each");
    static_assert(firstKeyswitchNote + keyswitchCount == vibratoGestureNote,
                  "A#0 vibrato must immediately follow the keyswitch banks");
    static_assert(vibratoGestureNote + 1 == tremoloGestureNote,
                  "B0 tremolo must immediately follow A#0 vibrato");
    static_assert(tremoloGestureNote < lowestPlayableNote,
                  "B0 tremolo must remain below the playable range");
    static_assert(firstKeyswitchNote + keyswitchCount <= lowestPlayableNote,
                  "keyswitches must not overlap the playable range");
    static_assert(highestPlayableNote + 2 == firstRepickNote,
                  "D#6 must separate playable notes from repick keys");
    static_assert(firstRepickNote + repickNoteCount <= 128,
                  "repick keys must fit in the MIDI note range");

    struct NoteOnEvent
    {
        int midiNote { -1 };
        float velocity { 0.0f };
        ExpressionId expressionId { legacyExpressionId };
    };

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    // Selects a deterministic player's contact, timing and strum variation
    // stream. The default zero preserves the established render contract; a
    // new seed takes effect at the next reset and never introduces wall-clock
    // random.
    void setVariationSeed(std::uint32_t seed) noexcept { variationSeed_ = seed; }
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity,
                ExpressionId expressionId = legacyExpressionId);
    // Solves one complete sample-accurate chord as a whole, so host insertion
    // order cannot change its physical strings, hand shape or variation
    // stream. Because the edge is known up front, a non-zero Strum Spread
    // delays only the strings the pick has not reached; it does not add the
    // scalar note path's causal re-anchor pre-roll to the leading string.
    void noteOnChord(std::span<const NoteOnEvent> events);
    void noteOff(int midiNote,
                 ExpressionId expressionId = legacyExpressionId);
    void allNotesOff();
    // Standard MIDI pitch bend moves every legacy-ID and sympathetically
    // ringing string by the same nominal -2..+2 semitone interval. Bend Time
    // controls how long the strings take to reach the target.
    void setPitchBend(float normalisedBipolar) noexcept;
    // Per-note expression is supplied as a semitone interval for one MIDI
    // member channel. ID 0 is reserved for the legacy global wheel above;
    // member bends glide with the same Bend Time but never add to it.
    void setExpressionPitchBend(ExpressionId expressionId,
                                float semitones) noexcept;
    // The MPE master component remains live after a member Note Off, while the
    // member component above freezes with the lifted finger. Keeping the two
    // components separate is required for a release or pedal-held tail to
    // follow later zone-master wheel movement without following channel reuse.
    void setExpressionMasterPitchBend(ExpressionId expressionId,
                                      float semitones) noexcept;
    void snapExpressionPitchBendToTarget(ExpressionId expressionId) noexcept;
    // MIDI CC1 controls the performance resonance (0 = the Sympathetic Ring
    // parameter alone, 1 = full bridge coupling plus acoustic feedback from
    // the amplified output, scaled by the Resonance Depth parameter).
    void setResonance(float normalised) noexcept;
    // MIDI CC2 adds continuous bridge-hand pressure on top of the Palm Pressure
    // parameter, so a phrase can be muted and opened without automation.
    void setPalmMutePressure(float normalised) noexcept;
    // Internal fretting-hand vibrato model. The plug-in maps its visible A#0
    // momentary gesture here while leaving MIDI pressure unassigned. Zero is
    // an exact no-op.
    void setVibrato(float normalised) noexcept;
    // Arms one shared picking wrist. Starting it schedules an immediate
    // contact on the next rendered sample; it then repicks every physically
    // held string at EngineParameters::tremoloRateHz until stopped. The
    // per-string E6..B6 commands remain compatible one-shot taps.
    void beginTremoloPicking(float velocity) noexcept;
    void endTremoloPicking() noexcept;
    // MIDI CC64. While held, a key-up leaves that string marked `sustained`
    // instead of releasing it immediately (see `noteOff()`); a string already
    // sounding when the pedal comes down is untouched either way, since it
    // only changes what a *later* key-up does. Releasing the pedal releases
    // every string that is `sustained` and still not held down, and drops the
    // flag from every voice so a subsequent key-up releases normally again.
    void setSustainPedal(bool down) noexcept;
    // The acoustic return path: what the loudspeaker is playing back at the
    // guitar. The engine keeps its own bounded copy behind a fixed, voiced
    // acoustic delay, so the pointers only need to stay valid for this call.
    // Callers process and return chunks no longer than
    // getAcousticReturnDelaySamples() to keep that delay independent of their
    // outer host block size. With the resonance control at zero the stored
    // signal is never injected and the engine is bit-exact to one that was
    // never fed.
    void pushAcousticReturn(const float* left, const float* right,
                            int numSamples) noexcept;
    [[nodiscard]] int getAcousticReturnDelaySamples() const noexcept
    {
        return feedbackDelaySamples_;
    }
    // How loud the returned signal actually is in the room, 0..1. The
    // amplifier chain manages its own listening level - a saturating stage is
    // only a few decibels louder than the dry DI - but in the room a cranked
    // amplifier is deafening while a DI is not audible at all, and it is that
    // acoustic level that decides whether the strings can regenerate. The
    // host derives this from its amplifier and distortion controls; at zero
    // the feedback path is exactly closed, so a dry instrument never howls.
    void setAcousticReturnLevel(float normalised) noexcept;
    void process(float* left, float* right, int numSamples);

    // Snapshot of the eight physical strings for the editor's fretboard.
    void getStringVisualState(
        std::array<StringVisualState, stringCount>& destination) const noexcept;

    [[nodiscard]] int getActiveVoiceCount() const noexcept;
    // Strings ringing purely through the sympathetic bridge coupling.
    [[nodiscard]] int getSympatheticStringCount() const noexcept;
    [[nodiscard]] PickStyle getCurrentPickStyle() const noexcept
    {
        return pickStyle_;
    }
    [[nodiscard]] PlayStyle getCurrentPlayStyle() const noexcept
    {
        return playStyle_;
    }
    [[nodiscard]] static bool isKeyswitchNote(int midiNote) noexcept
    {
        return midiNote >= firstKeyswitchNote
            && midiNote < firstKeyswitchNote + keyswitchCount;
    }
    [[nodiscard]] static bool isVibratoGestureNote(int midiNote) noexcept
    {
        return midiNote == vibratoGestureNote;
    }
    [[nodiscard]] static bool isTremoloGestureNote(int midiNote) noexcept
    {
        return midiNote == tremoloGestureNote;
    }
    [[nodiscard]] static bool isPlayableNote(int midiNote) noexcept
    {
        return midiNote >= lowestPlayableNote && midiNote <= highestPlayableNote;
    }
    [[nodiscard]] static bool isRepickNote(int midiNote) noexcept
    {
        return midiNote >= firstRepickNote
            && midiNote < firstRepickNote + repickNoteCount;
    }

private:
    // The JUCE-free regression suite inspects private string state through
    // this narrow seam. It is not part of the plug-in API.
    friend struct ElectryEngineTestAccess;

    static constexpr int delayLineSize = 16384;
    static constexpr int controlPeriod = 16;
#if ELECTRY_MEASURED_BODY_RESPONSE
    static constexpr int bodyModeCount = 3;
#else
    static constexpr int bodyModeCount = 4;
#endif
    static constexpr int decimatorHistorySize = 64;
    static constexpr int apertureHistorySize = 256;
    // Once no string is rendering and the shared body/coil/DC path has fallen
    // below -120 dBFS, the engine holds this many further internal samples and
    // then freezes: state is cleared, the output becomes exactly zero, and the
    // whole shared chain stops running until a note arrives.
    static constexpr int idleFreezeSamples = 64;
    static constexpr float idleFreezeLevel = 1.0e-6f;
    // JUCE hosts commonly top out at 384 kHz; the delay lines above are sized
    // for the lowest reachable pitch (open E1 with the wheel at -2) at that
    // rate. Rates beyond the ceiling are clamped so
    // hostile prepare() input cannot break the tuning contract.
    static constexpr double minimumSupportedSampleRate = 8000.0;
    static constexpr double maximumSupportedSampleRate = 384000.0;

#if ELECTRY_POSITIONED_FRET_COLLISION
    // An equal-tempered following fret is p = 1 - 2^(-1/12) of the speaking
    // length from the stopped end. Reading at D * (1 - p) gives the folded
    // ring's phase-equivalent surrogate for that longitudinal point.
    static constexpr float followingFretDelayScale = 0.9438743126816935f;

    [[nodiscard]] static float applyPositionedFretCollision(
        float bridgeSample, float followingFretSample, float clearance,
        float contact, float& excess) noexcept
    {
        const float displacement = bridgeSample - followingFretSample;
        const float magnitude = displacement < 0.0f
            ? -displacement : displacement;
        excess = magnitude > clearance ? magnitude - clearance : 0.0f;
        if (excess <= 0.0f)
            return bridgeSample;

        // Preserve the established zero-slope soft-contact knee. A reciprocal
        // two-rail junction would apply the opposite half-correction to the
        // other outgoing rail too; this default-off, one-read surrogate cannot,
        // so it is deliberately described as positioned loss, not as a passive
        // local collision. With d held fixed, its ideal two-tap modal energy
        // multiplier is 1 - d(2-d)sin^2(n*pi*p): nodes survive and antinodes
        // take the loss.
        const float contactDepth = contact * 6.0f * excess * excess
            / ((1.0f + 6.0f * excess) * magnitude);
        const float blend = 0.5f * contactDepth;
        return bridgeSample
             + blend * (followingFretSample - bridgeSample);
    }
#endif

    struct OnePole
    {
        float state { 0.0f };
        void reset() noexcept { state = 0.0f; }
        float process(float input, float coefficient) noexcept
        {
            state += (1.0f - coefficient) * (input - state);
            return state;
        }
    };

    struct DcBlocker
    {
        float previousInput { 0.0f };
        float previousOutput { 0.0f };
        void reset() noexcept { previousInput = previousOutput = 0.0f; }
        float process(float input, float coefficient) noexcept
        {
            const float output = input - previousInput + coefficient * previousOutput;
            previousInput = input;
            previousOutput = output;
            return output;
        }
    };

    struct DispersionAllpass
    {
        float state { 0.0f };
        void reset() noexcept { state = 0.0f; }
        float process(float input, float coefficient) noexcept
        {
            // Direct-form II first-order allpass (c + z^-1) / (1 + c z^-1).
            const float w = input - coefficient * state;
            const float output = coefficient * w + state;
            state = w;
            return output;
        }
    };

    struct Biquad
    {
        float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f }, a1 { 0.0f }, a2 { 0.0f };
        float z1 { 0.0f }, z2 { 0.0f };

        void reset() noexcept { z1 = z2 = 0.0f; }
        void setResonantLowpass(float frequencyHz, float q, float sampleRate) noexcept;
        float process(float input) noexcept
        {
            const float output = b0 * input + z1;
            z1 = b1 * input - a1 * output + z2;
            z2 = b2 * input - a2 * output;
            return output;
        }
    };

    // The hand's loss dip, in double for the same reason ModalResonator is: its
    // centre can sit at a few tens of hertz, where 2 - 2cos(w) is around 3.6e-5
    // formed by subtracting two numbers near two. In float the section's gain at
    // DC then carries about 0.3% of error, and since it lives inside the string's
    // feedback loop - where the only DC blocker in this engine, being on the
    // output, cannot reach it - an error in that direction is a mode that grows
    // by 0.03% per round trip and never decays. Double makes the residual around
    // 1e-16 instead. It runs only for hand contact or the default-off fitted
    // lowest-string correction.
    struct DipBiquad
    {
        double b0 { 1.0 }, b1 { 0.0 }, b2 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
        double z1 { 0.0 }, z2 { 0.0 };

        void reset() noexcept { z1 = z2 = 0.0; }
        float process(float input) noexcept
        {
            const double in = input;
            const double output = b0 * in + z1;
            z1 = b1 * in - a1 * output + z2;
            z2 = b2 * in - a2 * output;
            return static_cast<float>(output);
        }
    };

    // What the bridge hand does to the loop, as a shape rather than as running
    // state, so the decay solve and the tuning compensation read it from one
    // place and cannot disagree about what is in the loop.
    //
    // A band of loss centred on a multiple of the fundamental and returning to
    // unity above it. Returning to unity is what lets it be deep: a shelf pays
    // its full depth at the high fitted point, where the references ask for no
    // extra loss at all, and the feasibility ceiling then refuses to go further.
    struct HandLossShape
    {
        float dipOmega { 0.0f };
        float dipQ { 0.70f };
        float dipFullDepthDb { 0.0f };
    };

    struct ModalResonator
    {
        // Low structural modes can run at 384 kHz, where float coefficient
        // cancellation materially changes their gain and Q. The small shared
        // modal bank uses double state so its physical calibration survives
        // every supported host rate.
        double a1 { 0.0 }, a2 { 0.0 }, gain { 0.0 };
        double y1 { 0.0 }, y2 { 0.0 };

        void reset() noexcept { y1 = y2 = 0.0; }
        void configure(float frequencyHz, float q, float modeGain,
                       float sampleRate) noexcept;
        float process(float input) noexcept
        {
            const double output = gain * static_cast<double>(input)
                                - a1 * y1 - a2 * y2;
            y2 = y1;
            y1 = output;
            return static_cast<float>(output);
        }
    };

    // Fixed-state 2:1 output decimator. The physical model generates its own
    // signal, so it needs no input interpolator; every internal sample is fed
    // to this linear-phase halfband FIR before one host-rate sample is read.
    struct HalfbandDecimator
    {
        std::array<float, decimatorHistorySize> history {};
        int writeIndex { 0 };

        void reset() noexcept
        {
            history.fill(0.0f);
            writeIndex = 0;
        }
        void push(float input) noexcept;
        [[nodiscard]] float output() const noexcept;
    };

    // Causal finite-window spatial average implemented through a ring of
    // cumulative sums. It gives the chosen rectangular-aperture response with
    // O(1) work and supports a fractional window length.
    struct FractionalMovingAverage
    {
        std::array<double, apertureHistorySize> cumulativeHistory {};
        double cumulative { 0.0 };
        int writeIndex { 0 };
        // The window is a property of the pickup and the string, so it is
        // clamped, split and inverted once when the voice is configured rather
        // than on every sample of every pickup of every string. The division
        // this removes was the only one left in the pickup path.
        int windowWhole { 8 };
        double windowFraction { 0.0 };
        double inverseWindow { 1.0 / 8.0 };

        void reset() noexcept
        {
            cumulativeHistory.fill(0.0);
            cumulative = 0.0;
            writeIndex = 0;
        }
        void setWindow(float lengthSamples) noexcept
        {
            lengthSamples = lengthSamples < 1.0f
                ? 1.0f
                : (lengthSamples > static_cast<float>(apertureHistorySize - 2)
                       ? static_cast<float>(apertureHistorySize - 2)
                       : lengthSamples);
            windowWhole = static_cast<int>(lengthSamples);
            windowFraction = static_cast<double>(lengthSamples)
                           - static_cast<double>(windowWhole);
            inverseWindow = 1.0 / static_cast<double>(lengthSamples);
        }

        // Defined here rather than in the .cpp so it inlines into the per-voice
        // render loop, where it runs once per pickup per string per sample.
        float process(float input) noexcept
        {
            static_assert((apertureHistorySize & (apertureHistorySize - 1)) == 0,
                          "aperture history must be a power of two");
            constexpr int mask = apertureHistorySize - 1;

            cumulative += static_cast<double>(input);
            cumulativeHistory[static_cast<std::size_t>(writeIndex)] = cumulative;

            const int recentIndex = (writeIndex - windowWhole) & mask;
            const int olderIndex = (recentIndex - 1) & mask;
            const double recent =
                cumulativeHistory[static_cast<std::size_t>(recentIndex)];
            const double older =
                cumulativeHistory[static_cast<std::size_t>(olderIndex)];
            const double delayed = recent + windowFraction * (older - recent);

            writeIndex = (writeIndex + 1) & mask;
            return static_cast<float>((cumulative - delayed) * inverseWindow);
        }
    };

    // The humbucker's two coils. A pickup that sums two sensors a distance d
    // apart along the string adds the string's motion to itself delayed by the
    // time the wave takes to cross that gap, d/c, so its magnitude response is
    // |1 + b e^(-j 2 pi f d / c)| / (1 + b), dipping at c/2d. Normalised so the
    // pair has unit gain at DC, and exact identity at zero spacing, which is
    // what the single coil is.
    //
    // The second coil's weight b is below one for the same reason
    // `pickupCombDepth` is: the null of a real pickup is a dip and not a zero.
    // The screw coil sits further from the string than the slug coil and reads
    // correspondingly quieter, so the two contributions cannot cancel.
    struct CoilPairSum
    {
        std::array<float, apertureHistorySize> history {};
        int writeIndex { 0 };
        int spacingWhole { 0 };
        float spacingFraction { 0.0f };
        float balance { 1.0f };
        float normalise { 0.5f };
        bool paired { false };

        void reset() noexcept
        {
            history.fill(0.0f);
            writeIndex = 0;
        }
        void setSpacing(float delaySamples, float secondCoil) noexcept
        {
            balance = secondCoil;
            normalise = 1.0f / (1.0f + secondCoil);
            if (! (delaySamples > 0.0f))
            {
                paired = false;
                spacingWhole = 0;
                spacingFraction = 0.0f;
                return;
            }
            if (delaySamples > static_cast<float>(apertureHistorySize - 2))
                delaySamples = static_cast<float>(apertureHistorySize - 2);
            paired = true;
            spacingWhole = static_cast<int>(delaySamples);
            spacingFraction = delaySamples - static_cast<float>(spacingWhole);
        }

        // Defined here for the same reason as the aperture above: it runs once
        // per pickup per string per sample.
        float process(float input) noexcept
        {
            constexpr int mask = apertureHistorySize - 1;
            history[static_cast<std::size_t>(writeIndex)] = input;
            if (! paired)
            {
                writeIndex = (writeIndex + 1) & mask;
                return input;
            }
            const int recentIndex = (writeIndex - spacingWhole) & mask;
            const int olderIndex = (recentIndex - 1) & mask;
            const float recent = history[static_cast<std::size_t>(recentIndex)];
            const float older = history[static_cast<std::size_t>(olderIndex)];
            const float delayed = recent + spacingFraction * (older - recent);
            writeIndex = (writeIndex + 1) & mask;
            return (input + balance * delayed) * normalise;
        }
    };

    // A read at a delay that only changes when the voice is reconfigured: the
    // played and coupled strings' neck/bridge pickup position taps. Their
    // interpolation weights are a function of the delay alone, so they are
    // solved once instead of being rebuilt from a clamp, a floor and eight
    // polynomial multiplies on every sample of every string.
    struct DelayTap
    {
        int offset { 4 };
        float c0 { 0.0f }, c1 { 1.0f }, c2 { 0.0f }, c3 { 0.0f };
#if ELECTRY_POSITIONED_FRET_COLLISION
        float linear0 { 1.0f }, linear1 { 0.0f };
#endif

        void setDelay(float delaySamples) noexcept;
    };

    // One transverse polarisation of a string: a single-delay-loop waveguide
    // with loop damping, stiffness dispersion, and a loop DC guard.
    struct PolarisationLoop
    {
        std::array<float, delayLineSize> line {};
        int writeIndex { 0 };
        float currentDelay { 100.0f };
        float targetDelay { 100.0f };
        float delaySmoothing { 0.02f };
        float loopGain { 0.995f };
        float loopDampingCoefficient { 0.3f };
        // Two independently fitted four-section allpass groups match the
        // stiff-string delay deficit at a
        // low and a high partial. The factored cascade is well conditioned.
        float dispersionLowCoefficient { 0.0f };
        float dispersionHighCoefficient { 0.0f };
        OnePole damping {};
#if ELECTRY_LOW_STRING_LOSS_CORRECTION_ORDER2
        // Default-off empirical correction for the lowest physical string. It
        // reuses the same passive, minimum-phase section as the bridge hand,
        // but its fixed-Hz shape and dB/s depth law come from lowest-string
        // fret/partial decays rather than contact damping.
        DipBiquad fittedLossDip {};
        HandLossShape fittedLossShape {};
        float fittedLossDepth { 0.0f };
        bool fittedLossDipActive { false };
#endif
        // The bridge hand's loss slope, as a second pole in the loop.
        //
        // One pole fitted between f0 and 3.6 kHz has one slope, so the decay
        // envelope and the way loss varies with harmonic number are the same
        // degree of freedom - which is why no single constant could ever make
        // the fourth harmonic die while the first rings, the thing a real palm
        // mute measurably does. This shelf is the second degree of freedom:
        // unity at DC, falling above a corner set as a multiple of the
        // fundamental, so its slope is expressed in harmonic number rather than
        // in hertz and the same shape works an octave down.
        //
        // Crucially it is inside the decay solve, not added after it. Its
        // magnitude at both fitted points is divided back out of the targets, so
        // the envelope stays pinned where the references put it and the shelf
        // only bends the curve between them. An earlier attempt left it outside
        // the solve, and the extra loss simply shortened the whole note: the
        // 150-500 ms window fell 13 dB and the tail went inaudible again.
        //
        // Both terms are zero unless a hand is on the string, and OnePole with a
        // zero coefficient is an exact pass-through, so unmuted articulations
        // are bit-identical and the checks measuring them cannot be reached.
        // The shelf alone could not be deep enough, and the reason is worth
        // keeping: it is flat above its corner, so it spends its full depth at
        // the 3.6 kHz fitted point, where the references ask for no extra loss
        // at all. The one-pole cannot get flatter than flat to pay that back, so
        // feasibility refused anything deeper - measurably, since requesting
        // 0.50, 0.70 and 0.90 produced bit-identical decay. The dip returns to
        // unity above its band, so it costs almost nothing at the fitted point
        // and the ceiling stops binding.
        DipBiquad handDip {};
        HandLossShape handLossShape {};
        float handLossDepth { 0.0f };
        float handLossSolvedDepth { 0.0f };
        float handEnvelope { 0.0f };
        float handEnvelopePeak { 0.0f };
        bool handDipActive { false };
        DispersionAllpass dispersion1 {};
        DispersionAllpass dispersion2 {};
        DispersionAllpass dispersion3 {};
        DispersionAllpass dispersion4 {};
        DispersionAllpass dispersion5 {};
        DispersionAllpass dispersion6 {};
        DispersionAllpass dispersion7 {};
        DispersionAllpass dispersion8 {};

        void clear() noexcept;

        // The fully general read: the delay it is given moves every sample, so
        // its interpolation weights have to be rebuilt each time. Only the two
        // loop reads need that; the fixed pickup taps use readTap() below.
        // Defining it here lets it inline into the render loop instead of
        // costing a call each time.
        [[nodiscard]] float readFractional(float delaySamples) const noexcept
        {
            constexpr float maximumDelay = static_cast<float>(delayLineSize - 8);
            delaySamples = delaySamples < 4.0f
                ? 4.0f
                : (delaySamples > maximumDelay ? maximumDelay : delaySamples);
            const float position = static_cast<float>(writeIndex) - delaySamples;
            const int index = static_cast<int>(position) - (position < 0.0f ? 1 : 0);
            const float t = position - static_cast<float>(index);
            constexpr int mask = delayLineSize - 1;
            const float y0 = line[static_cast<std::size_t>((index - 1) & mask)];
            const float y1 = line[static_cast<std::size_t>(index & mask)];
            const float y2 = line[static_cast<std::size_t>((index + 1) & mask)];
            const float y3 = line[static_cast<std::size_t>((index + 2) & mask)];
            // Third-order Lagrange interpolation with y1..y2 as the unit
            // interval, factored to share the three bracket terms.
            const float tMinus1 = t - 1.0f;
            const float tMinus2 = t - 2.0f;
            const float tPlus1 = t + 1.0f;
            return (y0 * (-t * tMinus1 * tMinus2)
                    + y3 * (tPlus1 * t * tMinus1)) * (1.0f / 6.0f)
                 + (y1 * (tPlus1 * tMinus1 * tMinus2)
                    - y2 * (tPlus1 * t * tMinus2)) * 0.5f;
        }

        // The same third-order Lagrange read as above with its weights already
        // solved. Four multiplies instead of the clamp, floor and eight-product
        // polynomial, and its fractional position is derived from the delay
        // itself rather than from a difference against a five-digit write
        // index, so it is the more accurate of the two as well.
        [[nodiscard]] float readTap(const DelayTap& tap) const noexcept
        {
            constexpr int mask = delayLineSize - 1;
            const int index = writeIndex - tap.offset;
            return tap.c0 * line[static_cast<std::size_t>((index - 1) & mask)]
                 + tap.c1 * line[static_cast<std::size_t>(index & mask)]
                 + tap.c2 * line[static_cast<std::size_t>((index + 1) & mask)]
                 + tap.c3 * line[static_cast<std::size_t>((index + 2) & mask)];
        }

#if ELECTRY_POSITIONED_FRET_COLLISION
        // The short-lived fret-loss experiment needs one additional spatial
        // read. Linear interpolation halves its buffer traffic and multiply
        // count versus the pickup-quality cubic tap while remaining a bounded
        // fractional delay. Its extra high-frequency droop is an explicit
        // CPU/accuracy tradeoff in this default-off audition candidate.
        [[nodiscard]] float readLinearTap(const DelayTap& tap) const noexcept
        {
            constexpr int mask = delayLineSize - 1;
            const int index = writeIndex - tap.offset;
            return tap.linear0
                     * line[static_cast<std::size_t>(index & mask)]
                 + tap.linear1
                     * line[static_cast<std::size_t>((index + 1) & mask)];
        }
#endif

        void writeAdd(float offsetSamples, float value) noexcept;
    };

    enum class ExcitationPhase { Idle, Contact, Release, Tail };

    struct VelocityProfile
    {
        float amplitude { 1.0f };
        // The stroke's force, and the rate at which the string leaves the
        // plectrum. They are deliberately not the same curve: the second is
        // bounded by the pick's own stiffness, so a harder stroke is mostly
        // louder rather than proportionally sharper.
        float effort { 0.65f };
        float releaseRate { 0.72f };
        float brightness { 1.0f };
        float noise { 1.0f };
        float collision { 0.5f };
    };

    struct PendingRepick
    {
        bool active { false };
        float velocity { 0.0f };
        PlayStyle playStyle { PlayStyle::Sustain };
        bool strokeIsUp { false };
        std::uint32_t strokeVariationState { 0u };
        std::uint64_t startOrder { 0 };
        // Captured before scheduling marks the MIDI key down. At delayed
        // contact that live flag can no longer distinguish a repick from a
        // released/refretted note.
        bool preservesVibratoFinger { false };
        ExpressionId expressionId { legacyExpressionId };
    };

    struct Voice
    {
        bool active { false };
        bool keyDown { false };
        ExpressionId expressionId { legacyExpressionId };
        // A member channel may be reused while this string is still damping.
        // Once its finger lifts, retain that performed interval on the voice.
        bool expressionPitchBendFrozen { false };
        float frozenExpressionPitchBendSemitones { 0.0f };
        // Matching Note Offs still owed for this pitch. A sequencer may order
        // the next repeated Note On before the previous Note Off at the same
        // timestamp; the physical string is repicked, but that older end must
        // not release the new stroke.
        int keyDownCount { 0 };
        bool sustained { false };
        bool releasing { false };
        int stringIndex { 0 };
        // The physical string's fixed left-right placement in the stereo
        // field, in -1..1. Depends only on stringIndex and the (compile-time
        // constant) string count, so it is solved once when the voice is
        // bound to its string rather than every sample the stereo field is
        // open, in renderVoice() and renderSympatheticString() alike.
        float stereoLateral { 0.0f };
        // 1 for the lowest string down to 0 for the highest, i.e. how much
        // extra definition/brightness a low Drop-E string earns over a high
        // one. Depends only on stringIndex and the (compile-time constant)
        // string count, so - like stereoLateral above - it is solved once
        // here rather than re-derived from voice.stringIndex at every call
        // site that needs it (attack voicing, artifact-contact setup, and
        // the per-sample artifact-contact render path).
        float lowStringWeight { 0.0f };
        int midiNote { -1 };
        int fret { 0 };
        PlayStyle playStyle { PlayStyle::Sustain };
        // The latest whole-hand position applied to this ringing loop. Kept
        // apart from playStyle so a newer contact can move the damping without
        // rewriting how this note was attacked.
        PlayStyle dampingStyle { PlayStyle::Sustain };
        // The concrete stroke this note was picked with, resolved from the
        // latched PickStyle (Alternate resolves per wrist stroke).
        bool strokeIsUp { false };
        float velocity { 0.0f };
        VelocityProfile velocityProfile {};
        std::uint64_t startOrder { 0 };
        std::uint32_t noiseState { 1u };

        // What the picking hand did not repeat about this stroke. One draw is
        // shared by every string the wrist crosses and latched on each voice,
        // so delayed contacts keep their originating gesture and identical
        // MIDI still renders identical audio.
        float strokeContactOffsetMetres { 0.0f }; // along the string, from the nominal
        float strokeForceGain { 1.0f };           // linear, on the pick's amplitude
        float strokeAngleOffset { 0.0f };         // radians, on the attack's plane
        float strokeWidthScale { 1.0f };          // on the contact's duration
        // Raw draw retained so a delayed fresh contact can be promoted if a
        // legato finger reaches the string before the plectrum does.
        std::uint32_t strokeVariationState { 0u };
        // Latched stroke force applied to the bridge hand's loss rate. It uses
        // the same pick draw, so mute variation does not invent another player.
        float handContactScale { 1.0f };

        // The finger that is rocking this string. Two fingers of one hand are
        // not one oscillator: each carries its own phase, its own rate and its
        // own excursion, and the last two are redrawn every cycle from a
        // stream this voice advances itself. All of it is seeded from the note
        // counter, so identical MIDI still renders identical audio.
        float vibratoPhase { 0.0f };        // 0..1, 0 is the finger at rest
        float vibratoRateScale { 1.0f };    // this cycle's rate, relative
        float vibratoDepthScale { 1.0f };   // this cycle's excursion, relative
        float vibratoSemitones { 0.0f };    // what the pitch solve reads
        std::uint32_t vibratoSeed { 0u };
        std::uint32_t vibratoCycle { 0u };

        PolarisationLoop vertical {};
        PolarisationLoop horizontal {};

        // Sounding pitch program. The compensated periods cache the loop
        // filter phase compensation applied to the fractional delays.
        float baseFrequency { 110.0f };
        // The pitch and physical fret the dispersion grid search was last
        // fitted at. Both coordinates are quantised so a wheel or finger
        // glide does not re-run it on every control tick.
        float lastConfiguredSemitones { -999.0f };
        float lastConfiguredFrequency { -1.0f };
        float lastConfiguredLiveFret { -999.0f };
        // The live pitch/fret at which the per-round-trip loss and its
        // harmonic-number anchors were last solved. Kept separate from the
        // dispersion cache because the optional attack-pitch term moves the
        // former without changing the string's static stiffness fit.
        float lastDampedFrequency { -1.0f };
        float lastDampedLiveFret { -999.0f };
        // The pitch the analytic phase compensation was last evaluated at;
        // this one tracks every sub-cent move so tuning stays exact.
        float lastCompensatedSemitones { -999.0f };
        // The corresponding sounding period. Unlike the semitone offset it
        // remains valid when a ringing string is assigned a new base note, so
        // a damping-filter refit can translate only its phase-coordinate move
        // without folding a real bend, refret or candidate energy glide into
        // currentDelay.
        float lastCompensatedPeriod { 0.0f };
        // Set whenever the loop filters move without the pitch moving, so the
        // analytic phase compensation is refreshed without paying for the
        // expensive dispersion grid search again.
        bool compensationDirty { true };
        float compensatedPeriodVertical { 100.0f };
        float compensatedPeriodHorizontal { 100.0f };
        float legatoFromFrequency { 0.0f };
        float legatoBlend { 1.0f };
        float legatoIncrement { 0.0f };

        float palmImpactState { 0.0f };
        float palmImpactVel { 0.0f };

        // Cached physical descriptors used by the dispersion and structural
        // admittance solves. They are useful for control-rate refreshes and
        // for the JUCE-free physics regression seam.
        float inharmonicity { 0.0f };
        float dispersionLowPartial { 4.0f };
        float dispersionHighPartial { 16.0f };
        float bodyConductance { 0.0f };
        float bodyLossFactor { 1.0f };

        // Excitation state machine.
        ExcitationPhase excitationPhase { ExcitationPhase::Idle };
        int excitationRemaining { 0 };
        int excitationLength { 0 };
        // static_cast<float>(std::max(1, excitationLength)), solved once per
        // attack in startExcitation() rather than every rendered sample of
        // the Release phase, which reads it once per sample to turn
        // excitationRemaining into a 0..1 progress fraction.
        float excitationLengthDenominator { 1.0f };
        float excitationAmplitude { 0.0f };
        float excitationCombDelay { 0.0f };
        float excitationPolarity { 1.0f };
#if ELECTRY_ANALYTIC_RELEASE_IC
        // Candidate-only released-string initial condition. A positive peak is
        // also the one-shot pending flag; the physical position and contact
        // half-width are latched because pitch may move during pick contact.
        float analyticReleaseAmplitude { 0.0f };
        float analyticReleasePluckFraction { 0.18f };
        float analyticReleaseHalfWidthFraction { 0.0f };
        bool analyticReleaseFreshContact { false };
#endif
#if ELECTRY_ANALYTIC_RELEASE_IC || ELECTRY_ENERGY_ATTACK_PITCH
        float stringTensionNewtons { 80.0f };
#endif
#if ELECTRY_ENERGY_ATTACK_PITCH
        // Candidate-only normalised tension increment q = dT/T. Physical
        // transverse-string energy and the Bank elastic scale derive its seed
        // at pick release; after that q is the bounded empirical coordinate
        // that relaxes with Lee's measured common pitch component. Preserving
        // q through a refret is therefore explicit phenomenology, not a claim
        // that transverse joules are conserved while the finger moves.
        float attackPitchTensionRatio { 0.0f };
        float pendingAttackPitchEnergyJoules { 0.0f };
        float pendingAttackPitchElasticScalePerJoule { 0.0f };
        bool pendingAttackPitchClearOnRelease { false };
        float attackPitchFrequencyFactor { 1.0f };
        float lastAttackPitchFrequencyFactor { 1.0f };
#endif
        // The principal release component is shaped with two string-scaled
        // low-pass sections so its modal envelope approximates the 1/n^2
        // falloff of a triangular pluck displacement. Ordinary sustained
        // pick styles use a much smaller broad pulse for the physical edge;
        // deliberately percussive styles may weight that edge more strongly.
        float excitationTransientAmplitude { 0.0f };
        // Pick release geometry. A plectrum loads the string over most of the
        // contact and then slips off it in a fraction of that time, so the
        // release is strongly asymmetric; the symmetric raised cosine this
        // replaced gave every attack the same even bump regardless of pick.
        // The reciprocals are cached because the window is evaluated per
        // rendered sample.
        float excitationLoadScale { 1.0f / 0.72f };
        float excitationSlipScale { 1.0f / 0.28f };
        // Half of the plectrum's contact patch, in delay-line samples, so the
        // reflected excitation image is spread over the real contact width
        // instead of landing on a single point of the string.
        float excitationCombWidth { 0.0f };
        float excitationModalCoefficient { 0.99f };
        // The plectrum does not leave every string equally quickly. The
        // release's own duration low-passes the excitation, and its corner
        // follows the string rather than the fretted note.
        float excitationReleaseCoefficient { 0.9f };
        // The image's loss comes from the extra distance it travels through
        // the string, so unlike the release it does not depend on how the
        // string was excited. Sharing the release's corner made a dark
        // articulation leave far more of its own high end uncancelled than a
        // bright one, which inverted the styles' relative brightness.
        float excitationImageCoefficient { 0.9f };
        int excitationTailLength { 0 };
        float contactFeedbackGain { 1.0f };
        float noiseAmplitude { 0.0f };
        float noiseBandCoefficient { 0.5f };
        int noiseRemaining { 0 };
        int noiseLength { 0 };
        // static_cast<float>(std::max(1, noiseLength)), solved once wherever
        // noiseLength is (re)armed rather than every rendered sample of the
        // pick/release noise burst, which divides by this same clamped length
        // once per sample to form its progress window.
        float noiseLengthDenominator { 1.0f };
        float excitationPulseCoefficient { 0.5f };
        OnePole excitationShaper {};
        OnePole excitationModalShaper1 {};
        OnePole excitationModalShaper2 {};
        OnePole excitationReleaseShaper {};
        // The reflected excitation image has travelled to the pick and back
        // through the same lossy, dispersive string, so it returns darker than
        // the direct wave and cannot cancel it perfectly. Unity DC gain keeps
        // the comb's exact rejection of a net displacement.
        OnePole excitationImageShaper {};
        OnePole noiseShaper {};
        float noiseBandState { 0.0f };

        // Optional deterministic imperfections. A separate PRNG guarantees
        // that the Artifacts control never changes the ordinary pick/finger
        // noise sequence. The modal rattle is excited only by real attacks or
        // fret contact, so an idle engine remains exactly silent.
        std::uint32_t artifactNoiseState { 1u };
        OnePole artifactNoiseShaper {};
        float artifactNoiseCoefficient { 0.5f };
        float artifactNoiseBandState { 0.0f };
        int artifactCollisionRemaining { 0 };
        int artifactCollisionLength { 0 };
        // static_cast<float>(std::max(1, artifactCollisionLength)), solved
        // once in startExcitation() rather than every rendered sample of the
        // incidental fret-contact window, which divides by this same clamped
        // length once per sample to form its progress fraction.
        float artifactCollisionLengthDenominator { 1.0f };
        float artifactClearance { 1.0f };
#if ELECTRY_POSITIONED_FRET_COLLISION
        // Cached linear coefficients make the extra longitudinal read two
        // multiplies instead of rebuilding a fractional delay every sample.
        DelayTap artifactFollowingFretTap {};
#endif
        ModalResonator saddleRattle {};

        // Damping ramp applied by note release and palm muting.
        float releaseGain { 1.0f };
        float releaseGainTarget { 1.0f };
        float releaseGainCoefficient { 0.0f };
        bool releaseNoiseDone { true };

        // Strum travel: a chord's later strings start after the pick reaches
        // them. Zero for a simultaneous (non-strummed) note-on.
        int startDelaySamples { 0 };
        // A same-note repick may be scheduled before the pick reaches an
        // already-ringing string. Keep its small MIDI-side description here;
        // the sounding Voice remains the preceding stroke until contact.
        PendingRepick pendingRepick {};
        // A delayed retrigger may still contain the preceding stroke. If its
        // new contact is cancelled, keep that old ring; a genuinely fresh
        // never-contacted voice can instead retire immediately.
        bool pendingContactPreservesRing { false };
        // Which pick stroke this pending excitation belongs to. A note-on that
        // re-anchors the stroke may only push the strings of its own chord.
        std::uint64_t strumChordId { 0 };

        // Pickup taps and per-string pickup colouring.
        DelayTap pickupTapNeck {};
        DelayTap pickupTapBridge {};
        FractionalMovingAverage apertureNeck {};
        FractionalMovingAverage apertureBridge {};
        CoilPairSum coilPairNeck {};
        CoilPairSum coilPairBridge {};
        // Bend/vibrato is a separate pickup coordinate: an opposing slide can
        // keep total pitch fixed while changing tension and transverse speed.
        float pickupTensionSemitones { 0.0f };
#if ELECTRY_ENERGY_ATTACK_PITCH
        // Pickup-owned so a legato or damping solve cannot consume a small
        // attack-tension move through the loop pitch's independent cache.
        float pickupAttackPitchFrequencyFactor { 1.0f };
#endif
        // Transverse wave speed relative to the same string at its present
        // unbent fret. Fretting alone leaves this at one; tension gestures move
        // every representable pickup delay by 1/c (the aperture implementation
        // clamps a sub-sample window to a one-sample identity).
        float pickupWaveSpeedRatio { 1.0f };
        float previousFluxNeck { 0.0f };
        float previousFluxBridge { 0.0f };
        OnePole emfLowpassNeck {};
        OnePole emfLowpassBridge {};
        // Per-string magnetic balance, hoisted out of the sample loop: it
        // depends only on the string and the pickup geometry.
        float fluxScale { 1.0f };

        // Cross-mix coefficient of the two-polarisation seam. A returned wave
        // encounters it once per loop. Its nominal linear-with-f0 voicing is
        // derived from the sounding period at the 96 kHz reference clock, so a
        // given pitch gets the same coefficient at every host rate.
        float polarisationCoupling { 0.0f };

        // Per-articulation constants, resolved once per attack instead of
        // being re-selected by a switch on every rendered sample.
        float verticalWeight { 0.92f };
        float horizontalWeight { 0.42f };
        float articulationMakeup { 1.0f };

        // Bridge-coupled sympathetic ring of an unfingered string. The voice
        // reuses its own otherwise-idle vertical waveguide and pickup path, so
        // this costs no extra memory and cannot form a feedback loop: only
        // voices with `active` set drive the bridge bus, and only inactive
        // voices read it.
        bool sympatheticReady { false };
        // What this voice added to the bridge bus on the previous sample. A
        // played voice reads the bus *minus* this, so it never drives itself:
        // its own bridge termination is already carried by `bodyConductance`
        // and `bodyLossFactor`, and injecting it a second time would retune
        // every decay time in the instrument instead of coupling anything.
        float busContribution { 0.0f };
        float sympatheticEnergy { 0.0f };

        // A light finger or thumb resting on the string, at
        // `touchFraction` of its sounding length. Mode n's displacement there
        // goes as
        // sin(n pi p), so the energy a light contact removes per round trip
        // goes as sin^2(n pi p) = (1 - cos(2 pi n p)) / 2. Condensed into the
        // single delay loop as a one-tap FIR
        //
        //     H(z) = (1 - d/2) + (d/2) z^-M,   M = p * period,
        //
        // whose ideal nondispersive magnitude is 1 where the touch sits on a
        // node and 1 - d where it sits on an antinode. Both mix coefficients
        // are non-negative and sum to one for d in [0, 1], so the ideal
        // contact law is contractive.
        //
        // The tap uses the complete live fundamental period, including the
        // loop filters' phase rather than only the raw delay line. At an ideal
        // string node p = 1/k, it therefore targets an untouched surviving
        // harmonic series without retuning. Cubic fractional-delay
        // interpolation and the single temporal tap are still approximations;
        // representing every inharmonic spatial node of a dispersive stiff
        // string exactly would require a local bidirectional contact model.
        // This is why the harmonic is produced by contact rather than by
        // retuning the loop an octave up: the string keeps its own length,
        // inharmonicity, decay targets and pickup-comb geometry.
        //
        // The contact lifts once the note has formed. By then the partials it
        // removed are gone and cannot be re-excited, so lifting it is free and
        // buys back the extra delay reads.
        float touchFraction { 0.0f };
        float touchDepth { 0.0f };
        int touchHoldRemaining { 0 };
        float touchReleaseStep { 0.0f };

        // Slide friction. While the finger travels it drags across the wound
        // string's winding, and the ridges pass under it at v / w, where v is
        // the finger's speed along the string and w the winding pitch - which
        // is exactly the squeak a slide makes and why it rises in pitch with
        // the speed of the hand. Two one-poles form a band there; the
        // band and level follow the derivative of the glide's own smoothstep,
        // so the squeak rises, swells and dies with the movement and is exactly
        // zero when the finger is still. A plain string has no winding and
        // barely squeaks.
        float slideNoiseAmplitude { 0.0f };
        float slideNoiseLevel { 0.0f };
        float slideAverageBandCentreHz { 0.0f };
        float slideBandHigh { 0.5f };
        float slideBandLow { 0.9f };
        OnePole slideShaperHigh {};
        OnePole slideShaperLow {};

        int controlCountdown { 0 };
        float outputEnergy { 0.0f };
        float displayLevel { 0.0f };
        std::uint64_t ageSamples { 0 };
    };

    struct StringSpec
    {
        int openMidiNote { 40 };
        bool wound { true };
        float plainDiameterMm { 0.4064f }; // light-set reference gauge
        float bendingCoreScale { 0.30f };  // empirical flexural-core fraction
        // Axial load is carried by the steel core, not the winding. Kept
        // separate from the flexural fit so measured construction data can
        // later move either coordinate without silently changing the other.
        float axialCoreScale { 0.30f };
        float t60Seconds { 6.0f };
    };

    struct StereoSample
    {
        float left { 0.0f };
        float right { 0.0f };
    };

    struct RenderSums
    {
        std::array<float, 2> neck {};
        std::array<float, 2> bridge {};
        float body { 0.0f };
    };

    static const std::array<StringSpec, stringCount>& stringSpecs() noexcept;

    static EngineParameters sanitise(const EngineParameters& parameters) noexcept;
    static float midiToHz(float midiNote) noexcept;
    static std::uint32_t hash32(std::uint32_t value) noexcept;
    static std::uint32_t strokeVariationStateFor(
        std::uint64_t startOrder, int stringIndex) noexcept;
    // Both run inside the per-sample excitation and artifact paths, so they
    // are defined here to inline rather than call.
    static float bipolarNoise(std::uint32_t& state) noexcept
    {
        state = state * 1664525u + 1013904223u;
        const auto bits = (state >> 9) | 0x3f800000u;
        float unit;
        std::memcpy(&unit, &bits, sizeof(unit));
        return 2.0f * (unit - 1.5f);
    }
    // Three uniforms on [-1, 1] summed: unit variance exactly, and unable to
    // leave +/-3 sigma, which is the bound every stroke-, strum- and vibrato-
    // draw in this file needs. `drawStrokeVariation`, `beginChordStroke` and
    // `drawVibratoCycle` each used to carry their own byte-identical copy of
    // this one-line lambda over their own locally seeded state; this is that
    // shared arithmetic, still advancing whichever state the caller passes.
    static float sumThreeUniforms(std::uint32_t& state) noexcept
    {
        return bipolarNoise(state) + bipolarNoise(state) + bipolarNoise(state);
    }
    // clampf/lerp/smoothStep live in DspMath.h now, shared with ElectryFx and
    // ElectryVisuals; unqualified calls in this class's member functions still
    // resolve to them via the enclosing electry namespace.
    static float onePolePhaseDelay(float coefficient, float omega) noexcept;
    // Magnitude of the loop's one-pole loss filter, and the coefficient whose
    // magnitude ratio between the fundamental and the high reference matches a
    // requested ratio of decay times. Both the played strings' damping solve
    // and the bridge-coupled strings' run through these, so the two cannot
    // disagree about what a string's frequency-dependent loss is.
    static float onePoleMagnitude(float coefficient, float omega) noexcept;
    static float solveOnePoleDamping(float magnitudeRatio, float omega0,
                                     float omegaHigh) noexcept;
    // A pair of decay targets is not always realisable: the one-pole's own
    // loss at the fundamental has to be paid for out of the loop gain, and
    // that gain cannot exceed one. This solves the pair, and where it does not
    // fit it backs the high-frequency target off toward the fundamental's -
    // which is always feasible - until it does, so the fundamental's decay is
    // never the thing that gets thrown away.
    static void solveLoopLoss(float t60Fundamental, float t60High,
                              float periodSamples, float sampleRate,
                              float omega0, float omegaHigh, float gainCeiling,
                              float& coefficientOut, float& gainOut) noexcept;
    // How much faster a string's content at the high reference decays than its
    // fundamental, for the current string set and build. Shared for the same
    // reason: a coupled string is the same piece of steel as a played one.
    [[nodiscard]] float highFrequencyDecayRatio(int stringIndex) const noexcept;
    static void handLossResponse(float depth, const HandLossShape& shape,
                                 float omega, float& magnitude,
                                 float& phase) noexcept;
    // The dip's biquad coefficients for a given depth. Shared by the solve, the
    // phase compensation and the per-voice setup for the same reason as above.
    static void handDipCoefficients(float depth, const HandLossShape& shape,
                                    double& b0, double& b1, double& b2,
                                    double& a1, double& a2) noexcept;
    static void applyDipDepth(PolarisationLoop& loop, float depth) noexcept;
    static float allpassPhaseDelay(float coefficient, float omega) noexcept;
    // Convert a touch's sounding-length fraction to the live delay coordinate
    // of one polarisation. `compensatedPeriod` is the cached full period minus
    // that loop's filter phase. Adding the phase back to currentDelay keeps the
    // finger fixed through damping/dispersion refits and follows the existing
    // pitch smoother and horizontal detune without another state.
    [[nodiscard]] static float touchReadDelay(
        const Voice& voice, const PolarisationLoop& loop,
        float compensatedPeriod) noexcept
    {
        const float physicalPeriod = loop.currentDelay
                                   + voice.lastCompensatedPeriod
                                   - compensatedPeriod;
        return loop.currentDelay + voice.touchFraction * physicalPeriod;
    }
    // Per-string magnetic balance. It depends only on the string, so it is
    // solved once instead of inside the sample loop.
    static float stringFluxScale(int stringIndex) noexcept;
    [[nodiscard]] VelocityProfile makeVelocityProfile(float velocity) const noexcept;

    void configureVoicePitch(Voice& voice, bool forceDelayJump) noexcept;
    void configureVoiceDamping(Voice& voice, PlayStyle dampingStyle) noexcept;
    void configureVoiceDamping(Voice& voice, PlayStyle dampingStyle,
                               float liveFrequency, float liveFret) noexcept;
    void configureVoiceDispersion(Voice& voice, float configuredF0,
                                  float liveFret,
                                  float liveUnbentFrequency,
                                  float dispersionOmega,
                                  float dispersionPeriod) noexcept;
    void configurePickupGeometry(Voice& voice, float soundingLength,
                                 float period, float waveSpeed) noexcept;
    void configureVoicePickups(Voice& voice) noexcept;
    void configureSympatheticString(Voice& voice) noexcept;
    static void resetVoicePickupState(Voice& voice) noexcept;
    void updateStyleWeights(Voice& voice, bool legato = false) noexcept;
    void refreshVoicingIfNeeded() noexcept;
    void configureBody() noexcept;
    void configurePickupFilters() noexcept;
    [[nodiscard]] float bodyConductanceAt(float frequencyHz) const noexcept;
    void startExcitation(Voice& voice, float velocity, bool legato) noexcept;
#if ELECTRY_ANALYTIC_RELEASE_IC
    struct ReleasedRail
    {
        float delay { 4.0f };
        float scale { 0.0f };
        std::array<float, 3> positions {};
        std::array<float, 3> relativeCompliance {};
    };
    static ReleasedRail prepareReleasedRail(
        float delay, float peakAmplitude, float pluckFraction,
        float halfWidthFraction, float polarityAndWeight) noexcept;
    static float releasedRailAtCell(float cell,
                                    const ReleasedRail& rail) noexcept;
    static void seedReleasedDisplacement(PolarisationLoop& loop,
                                         float peakAmplitude,
                                         float pluckFraction,
                                         float halfWidthFraction,
                                         float polarityAndWeight) noexcept;
    static void primeReleasedPickupHistory(
        Voice& voice, const DelayTap& tap,
        FractionalMovingAverage& aperture, CoilPairSum& coilPair) noexcept;
#endif
    [[nodiscard]] static bool plectrumContacts(PlayStyle style, bool legato) noexcept;
    void drawStrokeVariation(Voice& voice, std::uint32_t state) noexcept;
    [[nodiscard]] bool hasHeldFrettedFinger(
        const Voice* excluded = nullptr) const noexcept;
    void resetVibratoOnset() noexcept;
    void seedVibratoFinger(Voice& voice) noexcept;
    void beginChordStroke(int stringIndex, bool strokeIsUp,
                          float spreadSeconds, bool completeChord) noexcept;
    void reAnchorChordStroke(int stringIndex) noexcept;
    int strumTravelSamples(int crossings) const noexcept;
    void drawVibratoCycle(Voice& voice) noexcept;
    void startVoice(Voice& voice, int midiNote, float velocity,
                    PlayStyle playStyle, bool strokeIsUp,
                    int startDelaySamples,
                    std::uint32_t strokeVariationState,
                    std::uint64_t reservedStartOrder = 0,
                    bool keyStateAlreadyApplied = false,
                    ExpressionId expressionId = legacyExpressionId) noexcept;
    void repickHeldString(int stringIndex, float velocity,
                          bool reanchorTremolo = true);
    void noteOnInternal(int midiNote, float velocity, int forcedStringIndex,
                        bool addKeyOwner, bool handPositionPlanned,
                        bool completeChordStart = false,
                        int completeChordAnchor = -1,
                        bool reanchorTremolo = true,
                        ExpressionId expressionId = legacyExpressionId);
    void legatoRetarget(Voice& voice, int midiNote, float velocity,
                        PlayStyle playStyle,
                        ExpressionId expressionId = legacyExpressionId) noexcept;
    void freezeExpressionPitchBend(Voice& voice) noexcept;
    void stopLegatoTravel(Voice& voice) noexcept;
    void beginVoiceRelease(Voice& voice) noexcept;
    void silenceVoice(Voice& voice) noexcept;
    int chooseString(int midiNote, PlayStyle playStyle,
                     ExpressionId expressionId) const noexcept;
    // `Voice::fret` is the written destination during a glide; allocation
    // decisions need the fractional fret under the finger at this event.
    [[nodiscard]] static float performedFret(
        const Voice& voice) noexcept;
    // What it costs the fretting hand to take this note on this string, in
    // fret-distance units. Lower wins; ties resolve toward the thicker string,
    // as they did when the rule was simply the lowest fret.
    [[nodiscard]] float frettingCost(int fret) const noexcept;
    [[nodiscard]] static float frettingCost(int fret,
                                            float handPosition) noexcept;
    void returnFrettingHandIfIdle(bool newChord) noexcept;
    // The hand moves only when it has to, and only at the start of a chord.
    void updateFrettingHand(int fret, bool newChord) noexcept;
    void updateVoiceControl(Voice& voice) noexcept;
    // Splits a neck/bridge-summed contribution across the stereo field the
    // same way renderVoice() and renderSympatheticString() each did with
    // their own copy of the channelsLinked_ branch: everything into channel 0
    // when the field is linked, or panned by the voice's own lateral position
    // otherwise.
    inline void accumulateStereoContribution(RenderSums& sums,
                                             float stereoLateral,
                                             float neckWeight, float neckSignal,
                                             float bridgeWeight,
                                             float bridgeSignal) const noexcept;
    void renderVoice(Voice& voice, RenderSums& sums) noexcept;
    void renderSympatheticString(Voice& voice, RenderSums& sums,
                                 float drive) noexcept;
    void freezeSharedPath() noexcept;
    // `acousticIn` is the loudspeaker signal reaching the strings this
    // internal sample; zero whenever the resonance feedback path is closed.
    [[nodiscard]] StereoSample renderInternalSample(float acousticIn) noexcept;
    void updateActiveVoiceCount() noexcept;
    // Re-solves the played-string bridge coupling against the row-sum bound.
    // Called wherever the active set or the loop gains can have moved.
    void solveBridgeCoupling() noexcept;
    [[nodiscard]] float deadSpotFactor(int stringIndex, float fret) const noexcept;
    [[nodiscard]] float scaleLengthMetres() const noexcept;

    EngineParameters targetParameters_ {};
    EngineParameters smoothedParameters_ {};
    EngineParameters appliedVoicingParameters_ {};
    double hostSampleRate_ { 48000.0 };
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    int oversamplingFactor_ { 1 };
    bool prepared_ { false };
    PickStyle pickStyle_ { PickStyle::Down };
    PlayStyle playStyle_ { PlayStyle::Sustain };
    bool alternateNextStrokeIsUp_ { false };
    std::uint64_t variationSeed_ { 0 };
    std::uint64_t noteSequence_ { 0 };
    int activeVoiceCount_ { 0 };
    int sympatheticStringCount_ { 0 };
    int controlCountdown_ { 0 };
    // The wheel's nominal semitone target and the glided position the strings
    // have actually reached. The glide time constant follows the Bend Time
    // parameter, so the wheel bends like a hand rather than snapping.
    float pitchBendTarget_ { 0.0f };
    float pitchBendSemitones_ { 0.0f };
    static constexpr std::size_t expressionPitchBendCount
        = static_cast<std::size_t>(maximumExpressionId) + 1;
    std::array<float, expressionPitchBendCount> expressionPitchBendTargets_ {};
    std::array<float, expressionPitchBendCount> expressionPitchBendSemitones_ {};
    std::array<float, expressionPitchBendCount>
        expressionMasterPitchBendTargets_ {};
    std::array<float, expressionPitchBendCount>
        expressionMasterPitchBendSemitones_ {};
    float bendGlideCoefficient_ { 0.05f };
    float appliedBendGlideSeconds_ { -1.0f };
    // The wheel position the sympathetic strings were last retuned to.
    float sympatheticAppliedBend_ { 0.0f };
    // Optional fretting-hand vibrato. One hand, but not one finger: the phase,
    // rate and excursion live on the voice, while amount and onset are shared.
    // Upward-biased, so its
    // minimum is the fretted pitch rather than its mean, because a finger can
    // only lengthen the string's path. Its depth is deliberately expressed in
    // equal semitones: a finger controls pitch and adjusts its displacement to
    // get it.
    static constexpr float vibratoMinimumSemitones = 0.10f;
    static constexpr float vibratoMaximumSemitones = 1.10f;
    // The pressure ramps at a bounded rate and is then shaped by smoothStep,
    // so the hand accelerates from rest instead of leaving at its steepest -
    // and stops the same way. The ramp is 258 ms long, which puts 90 % of the
    // settled depth at 207 ms, where the one-pole this replaces put it
    // (ln(10) times its 90 ms time constant): the change is one of shape, not
    // of speed.
    static constexpr float vibratoOnsetSeconds = 0.258f;
    float vibratoTarget_ { 0.0f };
    float vibratoAmount_ { 0.0f };
    float vibratoRamp_ { 0.0f };
    float vibratoOnsetIncrement_ { 0.002f };
    float vibratoPhaseIncrement_ { 0.0f };

    // CC1 performance resonance and the acoustic feedback path it opens.
    float resonanceTarget_ { 0.0f };
    float resonanceAmount_ { 0.0f };
    float resonanceCoefficient_ { 0.12f };
    bool sustainPedalDown_ { false };

    // Strum travel. The engine clock is the internal (oversampled) sample
    // count, so the chord window and the per-string offset are sample-accurate
    // at every host rate.
    std::int64_t engineClock_ { 0 };
    std::int64_t lastNoteOnClock_ { -(1ll << 40) };
    // B0 consumes a same-sample played pick, but a hammer or legato slide is
    // only the fretting hand and cannot suppress the wrist's due contact.
    std::int64_t lastPlectrumContactClock_ { -(1ll << 40) };
    // The most recent real string contact owns the shared muting-hand
    // position. Keep that history at engine scope: retiring the voice that
    // received the contact must not reveal an older voice and move the hand
    // backward without a new performance event.
    std::int64_t lastHandContactClock_ { -1 };
    PlayStyle lastHandContactPlayStyle_ { PlayStyle::Sustain };
    std::uint64_t lastHandContactOrder_ { 0 };
    // The neck edge the pick entered from, the direction it is travelling, and
    // the clock the chord's first note-on arrived on. Every voice of the chord
    // is scheduled against that one clock, so the ramp is laid down in stroke
    // order however the host interleaved the note-ons. Alternate reserves one
    // direction per accepted MIDI chord, not once per string. A fully pending
    // current chord may return it before a later chord depends on that order;
    // asking for an already-crossed string begins the next stroke.
    int chordAnchorString_ { 0 };
    bool chordStrokeIsUp_ { false };
    bool chordAlternateConsumed_ { false };
    bool chordContactOccurred_ { false };
    // Manual/playable strokes reset a held B0 wrist at their first physical
    // contact. B0's own generated strokes retain its fractional clock.
    bool chordReanchorsTremolo_ { true };
    std::int64_t chordFirstNoteOnClock_ { -(1ll << 40) };
    std::uint64_t chordSequence_ { 0 };
    std::uint32_t chordStrokeVariationState_ { 0u };
    // A separately seeded player reaches one picked wrist stroke a little
    // before or after another player. Causality makes this lane the later one;
    // all strings crossed by the stroke share the same offset.
    int chordPerformanceDelaySamples_ { 0 };
    int chordWindowSamples_ { 1680 };
    // The causal pre-roll used only while a scalar note stream may still
    // reveal a different chord edge, and the travel time from the anchor to
    // each further string. A complete noteOnChord batch knows its edge and
    // therefore uses zero pre-roll even when travel is active. Both remain
    // zero at a zero Strum Spread, which keeps a block chord bit-exact.
    int strumPreRollSamples_ { 0 };
    int strumReAnchorSamples_ { 0 };
    std::array<int, stringCount> chordTravelSamples_ {};

    // Where the fretting hand is. The index finger sits at this fret and the
    // little finger reaches `frettingHandReach` frets above it; open strings
    // need no finger at all and are always available. The hand only moves when
    // the note it has been asked for is outside that span, and only on the
    // first note of a chord, because a chord is one hand shape. It returns to
    // the nut when the phrase ends.
    //
    // Without this the allocator played every note at the lowest fret that
    // could produce it, which is a good model of first position and of nothing
    // else: it can never be up the neck, so the sounding length, inharmonicity
    // and pickup comb geometry that the fret drives were unreachable for most
    // of the range.
    float frettingHandPosition_ { 0.0f };
    int handReturnSamples_ { 72000 };
    // MIDI-key ownership outlives audible waveguide state: a held Mute or Dead
    // string may decay below the voice retirement floor and still be available
    // to its per-string picking-hand trigger.
    std::array<int, stringCount> heldMidiNotes_ {};
    std::array<int, stringCount> heldNoteCounts_ {};
    std::array<ExpressionId, stringCount> heldExpressionIds_ {};
    // The visible B0 gesture is one picking wrist, not eight clocks. Its
    // phase is measured in strokes so a rate change takes effect immediately
    // without resetting time or accumulating integer-sample drift.
    float tremoloPickingVelocity_ { 0.0f };
    double tremoloPickingPhase_ { 0.0 };

    // Continuous bridge-hand damping: the parameter plus the CC2 pressure.
    float palmMutePressure_ { 0.0f };
    float palmMuteBlend_ { 0.0f };
    float appliedPalmMute_ { 0.0f };

    // Sympathetic bridge bus. `sympatheticBus_` accumulates the current
    // sample's bounded displacement-domain bridge-motion proxies; the coupled
    // strings read the previous sample's total, which removes any ordering
    // dependence and any algebraic loop.
    float sympatheticBus_ { 0.0f };
    float sympatheticBusDelayed_ { 0.0f };
    float sympatheticGain_ { 0.0f };
    // The hand that mutes a chug or a palm-muted riff lies across every
    // string, not only the one being picked, so the coupled strings are damped
    // and starved of new energy exactly when the player is muting.
    float sympatheticInjection_ { 0.0f };
    float sympatheticHandGain_ { 1.0f };
    float sympatheticHandGainTarget_ { 1.0f };
    float sympatheticHandMute_ { -1.0f };
    bool sympatheticActive_ { false };

    // The same bus read by the strings that *are* being played, which closes
    // the coupling graph. `bridgeCouplingNominal_` is what the controls ask
    // for; `bridgeCouplingInjection_` is what survives the row-sum bound, and
    // `bridgeCouplingRowSum_` is that bound's left-hand side as it currently
    // stands, kept so the stability contract can be read at the seam rather
    // than recomputed from constants.
    float bridgeCouplingNominal_ { 0.0f };
    float bridgeCouplingInjection_ { 0.0f };
    float bridgeCouplingRowSum_ { 0.0f };

    // Acoustic feedback from the amplified output back into the strings. The
    // voiced nominal 5.805 ms path corresponds to about two metres of free-air
    // travel and retains the voiced 256-sample reference at 44.1 kHz. The FIFO
    // starts with that many silent samples and consumes/appends one host-rate
    // sample at a time, so the nominal delay does not move with a DAW's
    // block size. With the resonance control at zero the gain is exactly zero
    // and nothing stored here is ever injected.
    static constexpr double feedbackDelaySeconds = 256.0 / 44100.0;
    static constexpr int feedbackRingSize = 8192;
    std::array<float, feedbackRingSize> feedbackRing_ {};
    int feedbackWriteIndex_ { 0 };
    int feedbackReadIndex_ { 0 };
    int feedbackAvailable_ { 0 };
    int feedbackDelaySamples_ { 256 };
    float feedbackCurrent_ { 0.0f };
    float feedbackPrevious_ { 0.0f };
    float feedbackGain_ { 0.0f };
    // The rig's acoustic loudness, set by the host from its amplifier
    // controls and smoothed at the control tick.
    float returnLevelTarget_ { 0.0f };
    float returnLevel_ { 0.0f };
    // The bounded drive injected into the strings this internal sample, and
    // its hand-starved copy for the sympathetic loops.
    float feedbackDrive_ { 0.0f };
    float feedbackHandScale_ { 1.0f };

    std::array<Voice, stringCount> voices_ {};

    // Shared electrical and structural path.
    std::array<Biquad, 2> neckCoils_ {};
    std::array<Biquad, 2> bridgeCoils_ {};
    std::array<DcBlocker, 2> outputDc_ {};
    float neckMix_ { 0.0f };
    float bridgeMix_ { 1.0f };
    float neckMixTarget_ { 0.0f };
    float bridgeMixTarget_ { 1.0f };
    float pickupMixCoefficient_ { 0.01f };
    float magneticDriveNeck_ { 0.4f };
    float magneticDriveBridge_ { 0.4f };
    float magneticDriveNeckInverse_ { 2.5f };
    float magneticDriveBridgeInverse_ { 2.5f };
    // A pickup whose selector mix has faded to silence is skipped entirely;
    // its per-voice aperture and EMF state is cleared when it comes back so
    // the crossfade starts from a clean, click-free path.
    bool neckPathActive_ { false };
    bool bridgePathActive_ { true };
    // Mono is exact dual mono, so only one channel of the shared coil, DC and
    // decimation chain has to run. State is copied to the second channel at
    // the instant the field opens, which is exact because both channels had
    // identical inputs up to that sample.
    bool channelsLinked_ { true };
    std::array<ModalResonator, bodyModeCount> bodyModes_ {};
    float previousBodyDisplacement_ { 0.0f };
    OnePole bodyEmfLowpass_ {};
    float bodyEmfLowpassCoefficient_ { 0.5f };
    // Feed-forward hardware-ring proxy for the Artifacts control: each mode is
    // tuned to a string's open pitch, but this bank only colours the pickup
    // drive with an open-string/hardware buzz approximation. It is not the
    // waveguide-coupled Sympathetic Ring feature (`sympatheticBus_` and
    // friends below), which actually vibrates the modelled strings.
    std::array<ModalResonator, stringCount> artifactRingModes_ {};
    // Each mode's own clamped omega, omega-squared and loss rate, solved once
    // here in configureBody() rather than by bodyConductanceAt() on every
    // call: that call runs per partial inside configureVoiceDamping() (up to
    // six times per voice), which itself runs on every note-on and on every
    // control tick a damping-relevant control moves, while these three values
    // only change when configureBody() itself re-runs.
    std::array<float, bodyModeCount> bodyModeOmega_ {};
    std::array<float, bodyModeCount> bodyModeOmegaSquared_ {};
    std::array<float, bodyModeCount> bodyModeDamping_ {};
    std::array<float, bodyModeCount> bodyModeLevels_ {};
    float outputDcCoefficient_ { 0.9993f };
    float smoothedOutputGain_ { 0.5f };
    float smoothedBodyLevel_ { 0.35f };
    float stereoWidth_ { 0.0f };
    // 0.24f * stereoWidth_, resolved once whenever stereoWidth_ changes (at
    // most once per control tick) instead of on every internal sample of
    // every voice. renderVoice() and renderSympatheticString() both only
    // ever use stereoWidth_ through this product with the 0.24f stereo-field
    // constant, so caching it here removes a multiply per voice per sample
    // that stayed at the same value between control ticks anyway.
    float stereoSideScale_ { 0.0f };
    float parameterSmoothingCoefficient_ { 0.01f };
    float contactNoiseBandCoefficient_ { 0.08f };
    bool artifactsActive_ { true };

    // Rate-derived constants that used to be recomputed with std::pow on
    // every rendered sample of every string. They depend only on the internal
    // clock, so prepare() is their only correct home.
    float handEnvelopeCoefficient_ { 0.0015f };
#if ELECTRY_ENERGY_ATTACK_PITCH
    float attackPitchTensionRatioRetention_ { 0.999f };
#endif
    float retireAttackCoefficient_ { 0.01f };
    float retireReleaseCoefficient_ { 0.0009f };
    float artifactBandCoefficient_ { 0.12f };
    // The palm-mute impact thud's 85 Hz one-pole corner, applied to
    // voice.palmImpactState in renderVoice(). Fixed corner, rate-derived
    // coefficient - belongs here for the same reason as its neighbours above.
    float palmImpactThudCoefficient_ { 0.0f };
    // Per-sample retention of the impact's driving velocity. The voicing was
    // calibrated as 0.992 at 48 kHz and is converted to the internal clock in
    // prepare(), so oversampling and high-rate hosts keep one physical decay.
    float palmImpactVelocityRetention_ { 0.992f };
    float sympatheticEnergyCoefficient_ { 0.002f };
    float displayLevelAttack_ { 0.5f };
    float displayLevelRelease_ { 0.08f };
    float emfScale_ { 34.7f };
    float emfLowpassCoefficient_ { 0.2f };
    // The two polarisations' fixed detuning offset, calibrated at the 96 kHz
    // internal clock and scaled so it stays the same fraction of a period -
    // that is, the same number of cents - at every host rate.
    float horizontalDetuneSamples_ { 0.11f };
    // The 6 ms delay-smoothing time constant shared by configureVoicePitch()
    // and configureSympatheticString(): fast enough to track a bend or a
    // wheel-driven coupled-string retune transparently. Depends only on
    // controlPeriod and the internal clock, both fixed by prepare(), so it is
    // resolved once here instead of with std::exp at every control tick of
    // every voice and every coupled-string wake.
    float voiceDelaySmoothing_ { 0.5f };

    // Artifact shaping constants, evaluated once per control tick.
    float artifactContactShape_ { 0.0f };
    float artifactBuzzAmount_ { 0.0f };

    // Idle freeze. A guitar track is silent most of the time; running four
    // modal resonators, four coil biquads, two DC blockers and two halfband
    // decimators through an inaudible tail is pure waste, and it is exactly
    // where a float path ends up producing denormals.
    int silentInternalSamples_ { 0 };
    bool idleFrozen_ { true };

    std::array<HalfbandDecimator, 2> decimators_ {};
};

} // namespace electry
