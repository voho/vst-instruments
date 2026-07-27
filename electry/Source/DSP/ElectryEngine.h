#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

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
// palm mute, the fretting-hand legato (hammer-on / pull-off), or the natural
// harmonic touch. Latched by its own keyswitch bank, independently of the
// picking style.
enum class PlayStyle
{
    Sustain,
    PalmMute,
    Hammer,
    Harmonics
};

enum class PickupSelector { Neck, Both, Bridge };
enum class OutputMode { Mono, Stereo };

// Material and construction axes morph between classic solid-body anchors.
// Scale length spans a conventional electric into a modern baritone/8-string
// build, while the remaining performance controls use their full 0..1 range.
// The defaults describe a specific instrument rather than the midpoint of every
// axis, because the midpoint of every axis is not a guitar anyone owns. They are
// a thick carved set-neck blank strung with the heaviest set on a 27.6-inch
// scale, a humbucker-leaning bridge pickup, the tone control a little back, and a
// softer pick close to the bridge - the Drop-E rhythm instrument this model
// exists to be. Fitted against nine dry muted power-chord references at five
// pitches: the joint tilt-and-contour error is 5.03 dB here against 6.31 for the
// former all-midpoints defaults.
//
// The four "weight" fields on their own do not get there. Moved without the
// scale length, pickup type and pick position below, the same body, gauge, age,
// tone and pick-hardness values score 6.41 - slightly *worse* than the midpoints
// they replaced, because the pick sitting further out costs more than the thick
// blank recovers. The eleven fields are one voicing and do not decompose.
struct EngineParameters
{
    PickupSelector pickupSelector { PickupSelector::Bridge };
    float bodyWood { 0.0f };        // 0 mahogany/maple set blank, 1 swamp-ash slab
    float bodySize { 0.0f };        // 0 thick heavy blank, 1 thin light slab
    float bodyShape { 0.0f };       // 0 carved single-cut, 1 flat single-cut slab
    float construction { 0.0f };    // 0 set neck + stopbar, 1 bolt-on + through-body
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
    float muteDamping { 0.55f };    // palm-mute strength for the Muted style
    float bendTimeSeconds { 0.28f };// finger-bend travel time
    float velocityAmount { 0.65f }; // MIDI velocity to pluck strength
    float outputGain { 0.5f };      // linear output level
    float artifactAmount { 0.18f }; // sympathetic ring and incidental contact
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
    // Full-scale depth of the CC1 resonance control: how far a fully raised
    // modulation wheel can push the sympathetic coupling toward total and how
    // much amplified output is allowed to feed back into the strings. At 1 a
    // raised wheel lets a distorted tone self-resonate; at 0 CC1 does nothing.
    float resonanceDepth { 0.35f };
};

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

    static constexpr int stringCount = 8;
    static constexpr int fretCount = 22;

    // Keyswitches occupy one contiguous group below the playable range,
    // starting at 12 (C0): first the picking-style bank (Down/Up/Alternate),
    // then the play-style bank (Sustain/PalmMute/Hammer/Harmonics). The two
    // banks latch independently, so any of the twelve combinations can be
    // reached in two keyswitches at most. Notes between the banks and the
    // playable range (19..27) are ignored.
    static constexpr int firstKeyswitchNote = 12;
    static constexpr int pickStyleKeyswitchCount
        = static_cast<int>(PickStyle::Alternate) + 1;
    static constexpr int playStyleKeyswitchCount
        = static_cast<int>(PlayStyle::Harmonics) + 1;
    static constexpr int keyswitchCount = pickStyleKeyswitchCount
                                        + playStyleKeyswitchCount;
    static constexpr int firstPlayStyleKeyswitchNote
        = firstKeyswitchNote + pickStyleKeyswitchCount;
    // Drop-E eight-string, 22-fret instrument: open low E1 to fret 22 on E4.
    static constexpr int lowestPlayableNote = 28;
    static constexpr int highestPlayableNote = 86;

    static_assert(keyswitchCount == 7,
                  "three picking styles and four play styles need one keyswitch each");
    static_assert(firstKeyswitchNote + keyswitchCount <= lowestPlayableNote,
                  "keyswitches must not overlap the playable range");

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    // The pitch wheel bends every string - fingered and sympathetically
    // ringing alike - over a nominal -2..+2 semitone range, like a vibrato
    // bar. Each string follows with its own physically derived sensitivity
    // (elastic core stiffness against tension), and the strings travel to the
    // new pitch over the Bend Time parameter rather than jumping.
    void setPitchBend(float normalisedBipolar) noexcept;
    // MIDI CC1 controls the performance resonance (0 = the Sympathetic Ring
    // parameter alone, 1 = full bridge coupling plus acoustic feedback from
    // the amplified output, scaled by the Resonance Depth parameter).
    void setResonance(float normalised) noexcept;
    // MIDI CC2 adds continuous bridge-hand damping on top of the Palm Mute
    // parameter, so a phrase can be muted and opened without automation.
    void setPalmMutePressure(float normalised) noexcept;
    void setSustainPedal(bool down) noexcept;
    // The acoustic return path: what the loudspeaker is playing back at the
    // guitar, typically the previous block of the amplified output. The
    // engine keeps its own bounded copy, so the pointers only need to stay
    // valid for this call. With the resonance control at zero the stored
    // signal is never injected and the engine is bit-exact to one that was
    // never fed.
    void pushAcousticReturn(const float* left, const float* right,
                            int numSamples) noexcept;
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
    [[nodiscard]] static bool isPlayableNote(int midiNote) noexcept
    {
        return midiNote >= lowestPlayableNote && midiNote <= highestPlayableNote;
    }

private:
    // The JUCE-free regression suite inspects private string state through
    // this narrow seam. It is not part of the plug-in API.
    friend struct ElectryEngineTestAccess;

    static constexpr int delayLineSize = 16384;
    static constexpr int controlPeriod = 16;
    static constexpr int bodyModeCount = 4;
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
    // 1e-16 instead, and it is only ever run while a hand is on the string.
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
    // cumulative sums. It gives the exact rectangular-aperture sinc response
    // with O(1) work and supports a fractional window length.
    struct FractionalMovingAverage
    {
        std::array<double, apertureHistorySize> cumulativeHistory {};
        double cumulative { 0.0 };
        int writeIndex { 0 };

        void reset() noexcept
        {
            cumulativeHistory.fill(0.0);
            cumulative = 0.0;
            writeIndex = 0;
        }

        // Defined here rather than in the .cpp so it inlines into the per-voice
        // render loop, where it runs once per pickup per string per sample.
        float process(float input, float lengthSamples) noexcept
        {
            static_assert((apertureHistorySize & (apertureHistorySize - 1)) == 0,
                          "aperture history must be a power of two");
            constexpr int mask = apertureHistorySize - 1;
            lengthSamples = lengthSamples < 1.0f
                ? 1.0f
                : (lengthSamples > static_cast<float>(apertureHistorySize - 2)
                       ? static_cast<float>(apertureHistorySize - 2)
                       : lengthSamples);

            cumulative += static_cast<double>(input);
            cumulativeHistory[static_cast<std::size_t>(writeIndex)] = cumulative;

            const int whole = static_cast<int>(lengthSamples);
            const double fraction = static_cast<double>(lengthSamples)
                                  - static_cast<double>(whole);
            const int recentIndex = (writeIndex - whole) & mask;
            const int olderIndex = (recentIndex - 1) & mask;
            const double recent =
                cumulativeHistory[static_cast<std::size_t>(recentIndex)];
            const double older =
                cumulativeHistory[static_cast<std::size_t>(olderIndex)];
            const double delayed = recent + fraction * (older - recent);

            writeIndex = (writeIndex + 1) & mask;
            return static_cast<float>((cumulative - delayed)
                                      / static_cast<double>(lengthSamples));
        }
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

        // The fractional read runs six times per string per sample (two loop
        // reads plus two taps for each selected pickup). Defining it here lets
        // it inline into the render loop instead of costing a call each time.
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

        void writeAdd(float offsetSamples, float value) noexcept;
    };

    enum class ExcitationPhase { Idle, Contact, Release, Tail };

    struct VelocityProfile
    {
        float amplitude { 1.0f };
        float effort { 0.65f };
        float effortCurve { 0.72f };
        float brightness { 1.0f };
        float noise { 1.0f };
        float tension { 1.0f };
        float collision { 0.5f };
    };

    struct Voice
    {
        bool active { false };
        bool keyDown { false };
        bool sustained { false };
        bool releasing { false };
        int stringIndex { 0 };
        int midiNote { -1 };
        int fret { 0 };
        PlayStyle playStyle { PlayStyle::Sustain };
        // The concrete stroke this note was picked with, resolved from the
        // latched PickStyle (Alternate resolves per note).
        bool strokeIsUp { false };
        float velocity { 0.0f };
        VelocityProfile velocityProfile {};
        std::uint64_t startOrder { 0 };
        std::uint32_t noiseState { 1u };

        PolarisationLoop vertical {};
        PolarisationLoop horizontal {};

        // Sounding pitch program. The compensated periods cache the loop
        // filter phase compensation so the tension-modulation factor can be
        // applied cheaply every control tick.
        float baseFrequency { 110.0f };
        // The pitch the dispersion grid search was last fitted at; the fit is
        // quantised to a few cents so a wheel glide does not re-run it on
        // every control tick.
        float lastConfiguredSemitones { -999.0f };
        float lastConfiguredFrequency { -1.0f };
        // The pitch the analytic phase compensation was last evaluated at;
        // this one tracks every sub-cent move so tuning stays exact.
        float lastCompensatedSemitones { -999.0f };
        // Set whenever the loop filters move without the pitch moving, so the
        // analytic phase compensation is refreshed without paying for the
        // expensive dispersion grid search again.
        bool compensationDirty { true };
        float compensatedPeriodVertical { 100.0f };
        float compensatedPeriodHorizontal { 100.0f };
        float legatoFromFrequency { 0.0f };
        float legatoBlend { 1.0f };
        float legatoIncrement { 0.0f };

        // Tension-modulation state (attack pitch glide).
        float energyEnvelope { 0.0f };
        float tensionDepth { 0.0f };

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
        float excitationAmplitude { 0.0f };
        float excitationCombDelay { 0.0f };
        float excitationPolarity { 1.0f };
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
        float artifactClearance { 1.0f };
        std::uint32_t artifactCollisionCount { 0 };
        ModalResonator saddleRattle {};

        // Damping ramp applied by note release and palm muting.
        float releaseGain { 1.0f };
        float releaseGainTarget { 1.0f };
        float releaseGainCoefficient { 0.0f };
        bool releaseNoiseDone { true };

        // Strum travel: a chord's later strings start after the pick reaches
        // them. Zero for a simultaneous (non-strummed) note-on.
        int startDelaySamples { 0 };

        // Pickup taps and per-string pickup colouring.
        float pickupDelayNeck { 20.0f };
        float pickupDelayBridge { 6.0f };
        FractionalMovingAverage apertureNeck {};
        FractionalMovingAverage apertureBridge {};
        float apertureNeckLength { 8.0f };
        float apertureBridgeLength { 8.0f };
        float previousFluxNeck { 0.0f };
        float previousFluxBridge { 0.0f };
        OnePole emfLowpassNeck {};
        OnePole emfLowpassBridge {};
        // Per-string magnetic balance, hoisted out of the sample loop: it
        // depends only on the string and the pickup geometry.
        float fluxScale { 1.0f };

        // Per-articulation constants, resolved once per attack instead of
        // being re-selected by a switch on every rendered sample.
        float verticalWeight { 0.92f };
        float horizontalWeight { 0.42f };
        float articulationMakeup { 1.0f };

        // Bridge-coupled sympathetic ring of an unfingered string. The voice
        // reuses its own (otherwise idle) vertical waveguide, so this costs no
        // extra memory and cannot form a feedback loop: only voices with
        // `active` set drive the bridge bus, and only inactive voices read it.
        bool sympatheticReady { false };
        float sympatheticDelay { 100.0f };
        float sympatheticPickupDelay { 8.0f };
        float sympatheticPreviousFlux { 0.0f };
        float sympatheticEnergy { 0.0f };
        OnePole sympatheticEmf {};

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
        float woundCoreScale { 0.30f };    // effective bending-core fraction
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
    static float smoothStep(float value) noexcept
    {
        value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        return value * value * (3.0f - 2.0f * value);
    }
    static float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }
    static float onePolePhaseDelay(float coefficient, float omega) noexcept;
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
    // Per-string magnetic balance. It depends only on the string, so it is
    // solved once instead of inside the sample loop.
    static float stringFluxScale(int stringIndex) noexcept;
    // How far the wheel's nominal bend reaches on each string. A bar changes
    // every string's tension by stretching it, and the pitch that change buys
    // follows the string's elastic core stiffness against its tension, so the
    // strings do not move by equal semitones. Depends only on the string set.
    static float bendSensitivity(int stringIndex) noexcept;

    [[nodiscard]] VelocityProfile makeVelocityProfile(float velocity) const noexcept;

    void configureVoicePitch(Voice& voice, bool forceDelayJump) noexcept;
    void configureVoiceDamping(Voice& voice) noexcept;
    void configureVoicePickups(Voice& voice) noexcept;
    void configureSympatheticString(Voice& voice) noexcept;
    void updateStyleWeights(Voice& voice) noexcept;
    void refreshVoicingIfNeeded() noexcept;
    void configureBody() noexcept;
    void configurePickupFilters() noexcept;
    [[nodiscard]] float bodyConductanceAt(float frequencyHz) const noexcept;
    void startExcitation(Voice& voice, float velocity, bool legato) noexcept;
    void startVoice(Voice& voice, int midiNote, float velocity,
                    PlayStyle playStyle, bool strokeIsUp,
                    int startDelaySamples) noexcept;
    void legatoRetarget(Voice& voice, int midiNote, float velocity) noexcept;
    void beginVoiceRelease(Voice& voice) noexcept;
    void silenceVoice(Voice& voice) noexcept;
    int chooseString(int midiNote, PlayStyle playStyle) const noexcept;
    [[nodiscard]] float currentSoundingSemitoneOffset(const Voice& voice) const noexcept;
    void updateVoiceControl(Voice& voice) noexcept;
    void renderVoice(Voice& voice, RenderSums& sums) noexcept;
    void renderSympatheticString(Voice& voice, RenderSums& sums,
                                 float drive) noexcept;
    void freezeSharedPath() noexcept;
    // `acousticIn` is the loudspeaker signal reaching the strings this
    // internal sample; zero whenever the resonance feedback path is closed.
    [[nodiscard]] StereoSample renderInternalSample(float acousticIn) noexcept;
    void updateActiveVoiceCount() noexcept;
    [[nodiscard]] float deadSpotFactor(int stringIndex, int fret) const noexcept;
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
    std::uint64_t noteSequence_ { 0 };
    int activeVoiceCount_ { 0 };
    int sympatheticStringCount_ { 0 };
    int controlCountdown_ { 0 };
    // The wheel's nominal semitone target and the glided position the strings
    // have actually reached. The glide time constant follows the Bend Time
    // parameter, so the wheel bends like a hand rather than snapping.
    float pitchBendTarget_ { 0.0f };
    float pitchBendSemitones_ { 0.0f };
    float bendGlideCoefficient_ { 0.05f };
    float appliedBendGlideSeconds_ { -1.0f };
    // The wheel position the sympathetic strings were last retuned to.
    float sympatheticAppliedBend_ { 0.0f };
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
    int chordAnchorString_ { 0 };
    int chordWindowSamples_ { 1680 };

    // Continuous bridge-hand damping: the parameter plus the CC2 pressure.
    float palmMutePressure_ { 0.0f };
    float palmMuteBlend_ { 0.0f };
    float appliedPalmMute_ { 0.0f };

    // Sympathetic bridge bus. `sympatheticBus_` accumulates the current
    // sample's plucked-string bridge force; the coupled strings read the
    // previous sample's total, which removes any ordering dependence and any
    // algebraic loop.
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

    // Acoustic feedback from the amplified output back into the strings. The
    // host pushes its previous processed block through pushAcousticReturn();
    // the ring holds a bounded mono copy that process() consumes one host
    // sample at a time, which gives the loop the one-block latency a real
    // speaker-to-string air path has. With the resonance control at zero the
    // gain is exactly zero and nothing stored here is ever injected.
    static constexpr int feedbackRingSize = 8192;
    std::array<float, feedbackRingSize> feedbackRing_ {};
    int feedbackWriteIndex_ { 0 };
    int feedbackReadIndex_ { 0 };
    int feedbackAvailable_ { 0 };
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
    std::array<ModalResonator, stringCount> sympatheticModes_ {};
    std::array<float, bodyModeCount> bodyModeFrequencies_ {};
    std::array<float, bodyModeCount> bodyModeQs_ {};
    std::array<float, bodyModeCount> bodyModeLevels_ {};
    float outputDcCoefficient_ { 0.9993f };
    float smoothedOutputGain_ { 0.5f };
    float smoothedBodyLevel_ { 0.35f };
    float stereoWidth_ { 0.0f };
    float parameterSmoothingCoefficient_ { 0.01f };
    float contactNoiseBandCoefficient_ { 0.08f };
    bool artifactsActive_ { true };

    // Rate-derived constants that used to be recomputed with std::pow on
    // every rendered sample of every string. They depend only on the internal
    // clock, so prepare() is their only correct home.
    float handEnvelopeCoefficient_ { 0.0015f };
    float energyAttackCoefficient_ { 0.004f };
    float energyReleaseCoefficient_ { 0.00006f };
    float retireAttackCoefficient_ { 0.01f };
    float retireReleaseCoefficient_ { 0.0009f };
    float artifactBandCoefficient_ { 0.12f };
    float sympatheticEnergyCoefficient_ { 0.002f };
    float displayLevelAttack_ { 0.5f };
    float displayLevelRelease_ { 0.08f };
    float emfScale_ { 34.7f };
    float emfLowpassCoefficient_ { 0.2f };

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
