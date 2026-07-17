#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace electry
{

// Play styles selected with keyswitch notes below the playable range.
enum class Articulation
{
    Downstroke,
    Upstroke,
    HammerOn,
    Muted,
    Bend1Up,
    Bend2Up,
    Bend1Down,
    Bend2Down,
    Slap
};

enum class PickupSelector { Neck, Both, Bridge };

// Every 0..1 "voicing axis" morphs between a Gibson Les Paul-style anchor at
// 0 and a Fender Telecaster-style anchor at 1. The default 0.5 places the
// modeled guitar between the two references, as the instrument contract asks.
struct EngineParameters
{
    PickupSelector pickupSelector { PickupSelector::Bridge };
    float bodyWood { 0.5f };        // 0 mahogany/maple set blank, 1 swamp-ash slab
    float bodySize { 0.5f };        // 0 thick heavy blank, 1 thin light slab
    float bodyShape { 0.5f };       // 0 carved single-cut, 1 flat single-cut slab
    float construction { 0.5f };    // 0 set neck + stopbar, 1 bolt-on + through-body
    float scaleLength { 0.5f };     // 0 = 24.75 in, 1 = 25.5 in
    float pickupType { 0.5f };      // 0 humbucker, 1 narrow single coil
    float toneKnob { 0.8f };        // guitar's own passive tone control
    float bodyResonance { 0.35f };  // solid-body structural colour level
    float stringGauge { 0.5f };     // 0 = 0.009 set, 1 = 0.011 set
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
};

class ElectryEngine
{
public:
    ElectryEngine() noexcept;

    static constexpr int stringCount = 6;
    static constexpr int fretCount = 22;

    // Keyswitches occupy one contiguous group below the playable range:
    // 24 (C1) Downstroke, 25 Upstroke, 26 Hammer-on/Pull-off, 27 Muted,
    // 28 Bend 1 up, 29 Bend 2 up, 30 Bend 1 down, 31 Bend 2 down, 32 Slap.
    static constexpr int firstKeyswitchNote = 24;
    static constexpr int keyswitchCount = 9;
    // Standard-tuned 22-fret instrument: open low E2 to fret 22 on E4.
    static constexpr int lowestPlayableNote = 40;
    static constexpr int highestPlayableNote = 86;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    void setPitchBend(float normalisedBipolar) noexcept;
    void setSustainPedal(bool down) noexcept;
    void process(float* left, float* right, int numSamples);

    [[nodiscard]] int getActiveVoiceCount() const noexcept;
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

    static constexpr int delayLineSize = 8192;
    static constexpr int controlPeriod = 16;
    static constexpr int bodyModeCount = 4;
    // JUCE hosts commonly top out at 384 kHz; the delay lines above are sized
    // for the lowest reachable pitch (open E2 bent two semitones down, with
    // the wheel at -2) at that rate. Rates beyond the ceiling are clamped so
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
        float a1 { 0.0f }, a2 { 0.0f }, gain { 0.0f };
        float y1 { 0.0f }, y2 { 0.0f };

        void reset() noexcept { y1 = y2 = 0.0f; }
        void configure(float frequencyHz, float q, float modeGain,
                       float sampleRate) noexcept;
        float process(float input) noexcept
        {
            const float output = gain * input - a1 * y1 - a2 * y2;
            y2 = y1;
            y1 = output;
            return output;
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
        float dispersionCoefficient { 0.0f };
        OnePole damping {};
        DispersionAllpass dispersion1 {};
        DispersionAllpass dispersion2 {};

        void clear() noexcept;
        [[nodiscard]] float readFractional(float delaySamples) const noexcept;
        void writeAdd(float offsetSamples, float value) noexcept;
    };

    enum class ExcitationPhase { Idle, Contact, Release, Tail };

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
        std::uint64_t startOrder { 0 };
        std::uint32_t noiseState { 1u };

        PolarisationLoop vertical {};
        PolarisationLoop horizontal {};

        // Sounding pitch program. The compensated periods cache the loop
        // filter phase compensation so the tension-modulation factor can be
        // applied cheaply every control tick.
        float baseFrequency { 110.0f };
        float lastConfiguredSemitones { -999.0f };
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

        // Excitation state machine.
        ExcitationPhase excitationPhase { ExcitationPhase::Idle };
        int excitationRemaining { 0 };
        int excitationLength { 0 };
        float excitationAmplitude { 0.0f };
        float excitationCombDelay { 0.0f };
        float excitationPolarity { 1.0f };
        float noiseAmplitude { 0.0f };
        float noiseBandCoefficient { 0.5f };
        int noiseRemaining { 0 };
        int noiseLength { 0 };
        float excitationPulseCoefficient { 0.5f };
        OnePole excitationShaper {};
        OnePole noiseShaper {};
        float noiseBandState { 0.0f };

        // Fret-collision window used by the Slap style.
        int collisionRemaining { 0 };
        float collisionThreshold { 1.0f };

        // Damping ramp applied by note release and palm muting.
        float releaseGain { 1.0f };
        float releaseGainTarget { 1.0f };
        float releaseGainCoefficient { 0.0f };
        bool releaseNoiseDone { true };

        // Pickup taps and per-string pickup colouring.
        float pickupDelayNeck { 20.0f };
        float pickupDelayBridge { 6.0f };
        OnePole apertureNeck {};
        OnePole apertureBridge {};
        float apertureNeckCoefficient { 0.1f };
        float apertureBridgeCoefficient { 0.1f };

        int controlCountdown { 0 };
        float outputEnergy { 0.0f };
        std::uint64_t ageSamples { 0 };
    };

    struct StringSpec
    {
        int openMidiNote { 40 };
        bool wound { true };
        float plainDiameterMm { 0.4064f }; // light-set reference gauge
        float woundCoreScale { 0.42f };    // core fraction of a wound diameter
        float t60Seconds { 6.0f };
    };

    static const std::array<StringSpec, stringCount>& stringSpecs() noexcept;

    static EngineParameters sanitise(const EngineParameters& parameters) noexcept;
    static float midiToHz(float midiNote) noexcept;
    static std::uint32_t hash32(std::uint32_t value) noexcept;
    static float bipolarNoise(std::uint32_t& state) noexcept;
    static float smoothStep(float value) noexcept;
    static float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }
    static float onePolePhaseDelay(float coefficient, float omega) noexcept;
    static float allpassPhaseDelay(float coefficient, float omega) noexcept;
    static float softLimit(float value) noexcept;

    void configureVoicePitch(Voice& voice, bool forceDelayJump) noexcept;
    void configureVoiceDamping(Voice& voice) noexcept;
    void configureVoicePickups(Voice& voice) noexcept;
    void refreshVoicingIfNeeded() noexcept;
    void configureBody() noexcept;
    void configurePickupFilters() noexcept;
    void startExcitation(Voice& voice, float velocity, bool legato) noexcept;
    void startVoice(Voice& voice, int midiNote, float velocity,
                    Articulation articulation) noexcept;
    void legatoRetarget(Voice& voice, int midiNote, float velocity) noexcept;
    void beginVoiceRelease(Voice& voice) noexcept;
    void silenceVoice(Voice& voice) noexcept;
    int chooseString(int midiNote, Articulation articulation) const noexcept;
    [[nodiscard]] float currentSoundingSemitoneOffset(const Voice& voice) const noexcept;
    void updateVoiceControl(Voice& voice) noexcept;
    void renderVoice(Voice& voice, float& neckSum, float& bridgeSum,
                     float& bodySum) noexcept;
    void updateActiveVoiceCount() noexcept;
    [[nodiscard]] float deadSpotFactor(int stringIndex, int fret) const noexcept;
    [[nodiscard]] float scaleLengthMetres() const noexcept;

    EngineParameters targetParameters_ {};
    EngineParameters smoothedParameters_ {};
    EngineParameters appliedVoicingParameters_ {};
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    bool prepared_ { false };
    Articulation articulation_ { Articulation::Downstroke };
    std::uint64_t noteSequence_ { 0 };
    int activeVoiceCount_ { 0 };
    int controlCountdown_ { 0 };
    float pitchBendTarget_ { 0.0f };
    float pitchBendSemitones_ { 0.0f };
    bool sustainPedalDown_ { false };

    std::array<Voice, stringCount> voices_ {};

    // Shared electrical and structural path.
    Biquad neckCoil_ {};
    Biquad bridgeCoil_ {};
    DcBlocker outputDc_ {};
    float neckMix_ { 0.0f };
    float bridgeMix_ { 1.0f };
    float neckMixTarget_ { 0.0f };
    float bridgeMixTarget_ { 1.0f };
    float pickupMixCoefficient_ { 0.01f };
    float magneticDriveNeck_ { 0.4f };
    float magneticDriveBridge_ { 0.4f };
    std::array<ModalResonator, bodyModeCount> bodyModes_ {};
    float outputDcCoefficient_ { 0.9993f };
    float smoothedOutputGain_ { 0.5f };
    float smoothedBodyLevel_ { 0.35f };
    float parameterSmoothingCoefficient_ { 0.01f };
};

} // namespace electry
