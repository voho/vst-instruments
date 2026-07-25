#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace electry
{

// Play styles selected with keyswitch notes below the playable range.
enum class Articulation
{
    Downstroke,
    Upstroke,
    AlternateStroke,
    HammerOn,
    Tap,
    Muted,
    Chug,
    DeadNote,
    NaturalHarmonic,
    PinchHarmonic,
    Tremolo,
    Bend1Up,
    Bend2Up,
    Bend1Down,
    Bend2Down,
    Slap
};

enum class PickupSelector { Neck, Both, Bridge };
enum class OutputMode { Mono, Stereo };

// Material and construction axes morph between classic solid-body anchors.
// Scale length spans a conventional electric into a modern baritone/8-string
// build, while the remaining performance controls use their full 0..1 range.
struct EngineParameters
{
    PickupSelector pickupSelector { PickupSelector::Bridge };
    float bodyWood { 0.5f };        // 0 mahogany/maple set blank, 1 swamp-ash slab
    float bodySize { 0.5f };        // 0 thick heavy blank, 1 thin light slab
    float bodyShape { 0.5f };       // 0 carved single-cut, 1 flat single-cut slab
    float construction { 0.5f };    // 0 set neck + stopbar, 1 bolt-on + through-body
    float scaleLength { 0.5f };     // 0 = 25.5 in, 1 = 28 in
    float pickupType { 0.5f };      // 0 humbucker, 1 narrow single coil
    float toneKnob { 0.8f };        // guitar's own passive tone control
    float bodyResonance { 0.35f };  // solid-body structural colour level
    float stringGauge { 0.5f };     // 0 = .009-.080 set, 1 = .011-.098 set
    float stringAge { 0.15f };      // 0 fresh round-wounds, 1 old dead strings
    float pickPosition { 0.35f };   // 0 close to bridge, 1 over the neck
    float pickHardness { 0.6f };    // 0 soft/rounded contact, 1 stiff sharp pick
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
    // Full-scale depth of the CC1 performance vibrato, in cents.
    float vibratoDepthCents { 35.0f };
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
    Articulation articulation { Articulation::Downstroke };
};

class ElectryEngine
{
public:
    ElectryEngine() noexcept;

    static constexpr int stringCount = 8;
    static constexpr int fretCount = 22;

    // Keyswitches occupy one contiguous group below the playable range:
    // 12 (C0) through 27 (D#1), in Articulation enum order.
    static constexpr int firstKeyswitchNote = 12;
    static constexpr int keyswitchCount = static_cast<int>(Articulation::Slap) + 1;
    // Drop-E eight-string, 22-fret instrument: open low E1 to fret 22 on E4.
    static constexpr int lowestPlayableNote = 28;
    static constexpr int highestPlayableNote = 86;

    static_assert(firstKeyswitchNote + keyswitchCount == lowestPlayableNote,
                  "keyswitches must end immediately below the playable range");
    static_assert(keyswitchCount == 16, "every articulation needs one keyswitch");
    static_assert(static_cast<int>(Articulation::Slap) + 1 == keyswitchCount,
                  "keyswitch count must match the Articulation enum");

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    void setPitchBend(float normalisedBipolar) noexcept;
    // MIDI CC1 controls a performance vibrato (0 = still, 1 = wide).
    void setVibratoAmount(float normalised) noexcept;
    // MIDI CC2 adds continuous bridge-hand damping on top of the Palm Mute
    // parameter, so a phrase can be muted and opened without automation.
    void setPalmMutePressure(float normalised) noexcept;
    void setSustainPedal(bool down) noexcept;
    void process(float* left, float* right, int numSamples);

    // Snapshot of the eight physical strings for the editor's fretboard.
    void getStringVisualState(
        std::array<StringVisualState, stringCount>& destination) const noexcept;

    [[nodiscard]] int getActiveVoiceCount() const noexcept;
    // Strings ringing purely through the sympathetic bridge coupling.
    [[nodiscard]] int getSympatheticStringCount() const noexcept;
    [[nodiscard]] Articulation getCurrentArticulation() const noexcept
    {
        return articulation_;
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
        Articulation articulation { Articulation::Downstroke };
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
        float lastConfiguredSemitones { -999.0f };
        float lastConfiguredFrequency { -1.0f };
        // Set whenever the loop filters move without the pitch moving, so the
        // analytic phase compensation is refreshed without paying for the
        // expensive dispersion grid search again.
        bool compensationDirty { true };
        float compensatedPeriodVertical { 100.0f };
        float compensatedPeriodHorizontal { 100.0f };
        float bendStartSemitones { 0.0f };
        float bendTargetSemitones { 0.0f };
        float bendProgress { 1.0f };
        float bendIncrement { 0.0f };
        int bendHoldSamples { 0 };
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

        // Fret-collision window used by the Slap style.
        int collisionRemaining { 0 };
        float collisionThreshold { 1.0f };

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

        // Tremolo picking repeatedly excites one held physical string. The
        // direction flips on every scheduled stroke without changing the
        // latched Tremolo articulation reported to the host/UI.
        int tremoloSamplesUntilRetrigger { 0 };
        std::uint32_t tremoloRetriggerCount { 0 };
        bool tremoloStrokeIsUp { false };

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
    static float allpassPhaseDelay(float coefficient, float omega) noexcept;
    // Per-string magnetic balance. It depends only on the string, so it is
    // solved once instead of inside the sample loop.
    static float stringFluxScale(int stringIndex) noexcept;

    [[nodiscard]] VelocityProfile makeVelocityProfile(float velocity) const noexcept;

    void configureVoicePitch(Voice& voice, bool forceDelayJump) noexcept;
    void configureVoiceDamping(Voice& voice) noexcept;
    void configureVoicePickups(Voice& voice) noexcept;
    void configureSympatheticString(Voice& voice) noexcept;
    void updateArticulationWeights(Voice& voice) noexcept;
    void refreshVoicingIfNeeded() noexcept;
    void configureBody() noexcept;
    void configurePickupFilters() noexcept;
    [[nodiscard]] float bodyConductanceAt(float frequencyHz) const noexcept;
    void startExcitation(Voice& voice, float velocity, bool legato) noexcept;
    void startVoice(Voice& voice, int midiNote, float velocity,
                    Articulation articulation, int startDelaySamples) noexcept;
    void legatoRetarget(Voice& voice, int midiNote, float velocity) noexcept;
    void beginVoiceRelease(Voice& voice) noexcept;
    void silenceVoice(Voice& voice) noexcept;
    int chooseString(int midiNote, Articulation articulation) const noexcept;
    [[nodiscard]] float currentSoundingSemitoneOffset(const Voice& voice) const noexcept;
    void updateVoiceControl(Voice& voice) noexcept;
    void renderVoice(Voice& voice, RenderSums& sums) noexcept;
    void renderSympatheticString(Voice& voice, RenderSums& sums,
                                 float drive) noexcept;
    void freezeSharedPath() noexcept;
    [[nodiscard]] StereoSample renderInternalSample() noexcept;
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
    Articulation articulation_ { Articulation::Downstroke };
    bool alternateNextStrokeIsUp_ { false };
    std::uint64_t noteSequence_ { 0 };
    int activeVoiceCount_ { 0 };
    int sympatheticStringCount_ { 0 };
    int controlCountdown_ { 0 };
    float pitchBendTarget_ { 0.0f };
    float pitchBendSemitones_ { 0.0f };
    float vibratoTarget_ { 0.0f };
    float vibratoAmount_ { 0.0f };
    float vibratoPhase_ { 0.0f };
    float vibratoDepthSemitones_ { 0.35f };
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
    float energyAttackCoefficient_ { 0.004f };
    float energyReleaseCoefficient_ { 0.00006f };
    float retireAttackCoefficient_ { 0.01f };
    float retireReleaseCoefficient_ { 0.0009f };
    float artifactBandCoefficient_ { 0.12f };
    float pitchBendCoefficient_ { 0.35f };
    float vibratoCoefficient_ { 0.12f };
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
