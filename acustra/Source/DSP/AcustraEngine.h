// Acustra: a physically modelled acoustic-guitar instrument.
//
// Runtime output uses six stiff-string waveguides coupled through a shared
// passive bridge and a measurement-derived modal body and radiation model.

#pragma once

#include "FittedPhysicalData.h"

#include <array>
#include <cstddef>
#include <cstdint>

#if !defined(ACUSTRA_BRIDGE_MODE_COUNT)
#define ACUSTRA_BRIDGE_MODE_COUNT 50
#endif

namespace acustra
{

enum class BodyShape
{
    Parlor,
    Auditorium,
    Dreadnought,
    Jumbo
};

enum class BodyMaterial
{
    Spruce,
    Cedar,
    Mahogany,
    Maple
};

enum class StringMaterial
{
    Nylon,
    Steel
};

enum class Tuning
{
    Standard,
    DropD,
    Dadgad,
    OpenG,
    HalfStepDown
};

struct EngineParameters
{
    BodyShape shape { BodyShape::Dreadnought };
    BodyMaterial bodyMaterial { BodyMaterial::Spruce };
    StringMaterial stringMaterial { StringMaterial::Steel };
    Tuning tuning { Tuning::Standard };
    float stringAge { 0.15f };       // 0 fresh, 1 worn/dead
    float pluckPosition { 0.28f };   // 0 bridgeward, 1 neckward
    float touch { 0.58f };           // 0 soft/dark, 1 hard/bright
    float bodyAmount { 0.82f };      // measurement-derived body radiation
    float stereoWidth { 0.62f };     // authored per-mode stereo gain spread
    float outputGain { 0.42f };      // linear
};

struct AcustraEngineTestAccess;

class AcustraEngine
{
public:
    static constexpr int stringCount = 6;
    static constexpr int fretCount = 20;

    AcustraEngine() noexcept;

    void prepare(double sampleRate, int maximumBlockSize);
    void reset() noexcept;
    void setParameters(const EngineParameters& parameters) noexcept;

    // Setup/offline-fitting control. If already prepared, changing the
    // calibration resets the engine; do not call it from the audio thread.
    void setPhysicalCalibration(const PhysicalCalibration&) noexcept;

    void noteOn(int midiNote, float velocity, int midiChannel = 1) noexcept;
    void noteOff(int midiNote, int midiChannel = 1) noexcept;
    void setSustainPedal(bool down, int midiChannel = 1) noexcept;
    // Continuous bridge-hand damping, 0 open to 1 fully muted. Exposed as a
    // controller rather than a panel control: it is a playing pressure, and
    // zero is an exact no-op.
    void setPalmMutePressure(float pressure) noexcept;
    void setPitchBend(float semitones, int midiChannel = 1) noexcept;
    // Acustra implements the MPE lower zone only: channel 1 is its manager
    // and the contiguous channels above it are members. Zero restores
    // conventional, fully independent MIDI channels.
    void setLowerZoneMemberCount(int memberCount) noexcept;
    void allNotesOff(int midiChannel = 1) noexcept;
    void allSoundOff(int midiChannel = 1) noexcept;
    void setBridgeCouplingEnabled(bool enabled) noexcept;
    // Offline/test isolation only; this gates the one-way idle-string
    // radiation sum without changing the bridge or string state.
    void setSympatheticStringsEnabled(bool enabled) noexcept;

    void process(float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] int getActiveVoiceCount() const noexcept;
    [[nodiscard]] int getSympatheticStringCount() const noexcept;
    [[nodiscard]] float getLastBridgeVelocity() const noexcept;
    [[nodiscard]] float getLastBridgeReactionForce() const noexcept;
    [[nodiscard]] float getLastBridgeBodyForce() const noexcept;
    [[nodiscard]] float getLastBridgeTailForce() const noexcept;
    [[nodiscard]] float getLastSympatheticRadiationForce() const noexcept;
    [[nodiscard]] float getLastBridgePower() const noexcept;
    [[nodiscard]] float getLastBridgeBodyPower() const noexcept;
    [[nodiscard]] float getLastBridgeTailPower() const noexcept;

private:
    friend struct AcustraEngineTestAccess;

    static constexpr int maximumDelaySamples = 8192;
    static constexpr int bodyModeCount = 96;
    static constexpr int bridgeModeCount = ACUSTRA_BRIDGE_MODE_COUNT;
    static constexpr int controlPeriod = 32;
    static constexpr int midiChannelCount = 16;

    struct OnePole
    {
        float state { 0.0f };
        float process(float input, float coefficient) noexcept
        {
            state = input + coefficient * (state - input);
            return state;
        }
        void reset() noexcept { state = 0.0f; }
    };

    struct SecondOrderAllpass
    {
        float x1 { 0.0f };
        float x2 { 0.0f };
        float y1 { 0.0f };
        float y2 { 0.0f };
        float process(float input, float a1, float a2) noexcept
        {
            const float output = a2 * input + a1 * x1 + x2
                               - a1 * y1 - a2 * y2;
            x2 = x1;
            x1 = input;
            y2 = y1;
            y1 = output;
            return output;
        }
        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }
    };

    struct FixedDerivative
    {
        std::array<float, 10> history {};
        int index { 0 };

        void reset(float value = 0.0f) noexcept
        {
            history.fill(value);
            index = 0;
        }
        float process(float input, float sampleRateRatio) noexcept;
    };

    struct StringLoop
    {
        std::array<float, maximumDelaySamples> delay {};
        int writeIndex { 0 };
        float currentDelay { 128.0f };
        float targetDelay { 128.0f };
        float loopGain { 0.995f };
        float broadLossMix { 0.02f };
        float highLossMix { 0.1f };
        float broadLossCoefficient { 0.5f };
        float lowpassCoefficient { 0.5f };
        float dispersionA1 { 0.0f };
        float dispersionA2 { 0.0f };
        OnePole broadLossFilter {};
        OnePole lossFilter {};
        SecondOrderAllpass dispersion {};
        FixedDerivative bridgeDerivative {};

        void reset() noexcept;
        [[nodiscard]] float readDelay(float samples) const noexcept;
        float advance(float delaySmoothing, float releaseGain) noexcept;
        void write(float value) noexcept;
    };

    struct BridgeMode
    {
        float denominator1 { 0.0f };
        float denominator2 { 0.0f };
        float numerator1 { 0.0f };
        float numerator2 { 0.0f };
        float input1 { 0.0f };
        float output1 { 0.0f };
        float output2 { 0.0f };

        float processPast(float input) noexcept;
        void reset() noexcept
        {
            input1 = output1 = output2 = 0.0f;
        }
    };

    struct BridgeLoad
    {
        // One slot past the measured modes carries the plate
        // conductance floor described in FittedPhysicalData.h.
        std::array<BridgeMode, bridgeModeCount + 1> modes {};
        float immediateAdmittance { 0.0f };
        float delayedPastResponse { 0.0f };
        float tailIntegratedForce { 0.0f };
        float previousDisplacement { 0.0f };
        float mainIntegratedForce { 0.0f };
        float bodyIntegratedForce { 0.0f };

        void reset() noexcept;
        float process(float incident, float characteristicAdmittance,
                      float tailStiffness, float samplePeriod) noexcept;
    };

    struct BodyMode
    {
        float real { 0.0f };
        float imaginary { 0.0f };
        float poleReal { 0.0f };
        float poleImaginary { 0.0f };
        float leftReal { 0.0f };
        float leftImaginary { 0.0f };
        float rightReal { 0.0f };
        float rightImaginary { 0.0f };

        void process(float input, float& left, float& right) noexcept
        {
            const float nextReal = input + poleReal * real
                                 - poleImaginary * imaginary;
            const float nextImaginary = poleImaginary * real
                                      + poleReal * imaginary;
            real = nextReal;
            imaginary = nextImaginary;
            left += 2.0f * (leftReal * real
                          - leftImaginary * imaginary);
            right += 2.0f * (rightReal * real
                           - rightImaginary * imaginary);
        }
        void reset() noexcept { real = imaginary = 0.0f; }
    };

    struct Voice
    {
        std::array<StringLoop, 2> loops {};
        // A string taken for a new note is still vibrating. This carries that
        // vibration on under the hand damping the model already uses for a
        // stopped note, instead of deleting it. Only the radiated vertical
        // polarisation is kept.
        StringLoop tailLoop {};
        float tailDamping { 1.0f };
        float tailLevel { 0.0f };
        int tailQuietSamples { 0 };
        bool tailActive { false };
        int openMidi { 40 };
        int midiNote { 40 };
        // 1 is a stopped note. Above that the string sounds open in its nth
        // mode, a natural harmonic, and the loop runs at the open pitch.
        int harmonic { 1 };
        int midiChannel { 1 };
        int fret { 0 };
        int ownerCount { 0 };
        bool played { false };
        bool keyDown { false };
        bool pedalHeld { false };
        bool mpeMember { false };
        bool memberPitchBendFrozen { false };
        std::uint64_t startOrder { 0 };
        std::uint32_t randomState { 1 };
        float velocity { 0.0f };
        float polarisationMix { 0.5f };
        float excitationEnvelope { 0.0f };
        float excitationDecay { 0.0f };
        float excitationColour { 0.0f };
        float excitationLowpass { 0.0f };
        float bridgeContactState { 0.0f };
        float bridgeContactState2 { 0.0f };
        float bridgeContactCoefficient { 0.5f };
        float bridgeContactBlend { 0.0f };
        float bridgeContactDecay { 0.99f };
        float characteristicImpedance { 0.5f };
        float bridgeTailStiffness { 10000.0f };
        float attackPitchCents { 0.0f };
        float attackPitchDecay { 1.0f };
        float frozenMemberPitchBendSemitones { 0.0f };
        float attackSlopeEnergy { 0.0f };
        float observedSlopeEnergy { 0.0f };
        float dispersionDesignFrequency { 0.0f };
        float dispersionDesignInharmonicity { -1.0f };
        float dispersionDesignAge { -1.0f };
        float dispersionDesignFrequencyLossScale { -1.0f };
        float dispersionDecayRatio { 10.0f };
        float dispersionPoleRatio { 4.0f };
        float level { 0.0f };
        float releaseDamping { 1.0f };
        int quietSamples { 0 };
    };

    struct BodyOutput
    {
        float left { 0.0f };
        float right { 0.0f };
    };

    static EngineParameters sanitise(const EngineParameters&) noexcept;
    static PhysicalCalibration sanitise(const PhysicalCalibration&) noexcept;
    static std::array<int, stringCount> openNotes(Tuning) noexcept;
    static float midiFrequency(int midiNote) noexcept;
    static float clamp(float value, float low, float high) noexcept;
    static float phaseDelayForOnePoleMix(float coefficient, float mix,
                                         float omega) noexcept;
    static float magnitudeForOnePoleMix(float coefficient, float mix,
                                        float omega) noexcept;
    static float registeredPluckAperture(float apertureSamples,
                                         float apertureScale,
                                         float referenceDelay,
                                         float currentReferenceLength,
                                         float exponent) noexcept;
    void applyDiscreteParameters(bool force) noexcept;
    void updateControlState() noexcept;
    void configureBody() noexcept;
    void configureBridge() noexcept;
    float bridgePhaseDelay(float frequency, int stringIndex) const noexcept;
    void configureVoice(Voice& voice, int stringIndex, int midiNote,
                        bool clearDelay) noexcept;
    void updateAttackPitch(Voice& voice, int stringIndex) noexcept;
    float effectiveTouch(const Voice& voice) const noexcept;
    void initialisePluck(Voice& voice, int stringIndex, float velocity) noexcept;
    void returnToOpenString(Voice& voice, int stringIndex) noexcept;
    void beginRelease(Voice& voice) noexcept;
    void captureTail(Voice& voice) noexcept;
    void resetSoundState() noexcept;
    void freezeMemberPitchBend(Voice& voice) noexcept;
    [[nodiscard]] bool isLowerZoneMaster(int midiChannel) const noexcept;
    [[nodiscard]] bool isLowerZoneMember(int midiChannel) const noexcept;
    [[nodiscard]] bool channelControlsVoice(int midiChannel,
                                            const Voice& voice) const noexcept;
    [[nodiscard]] bool sustainIsDown(const Voice& voice) const noexcept;
    int chooseString(int midiNote) const noexcept;
    struct HarmonicChoice
    {
        int string { -1 };
        int harmonic { 1 };
    };
    [[nodiscard]] HarmonicChoice chooseHarmonic(int midiNote) const noexcept;
    float renderExcitation(Voice& voice) noexcept;
    void finishVoice(Voice& voice, int stringIndex, float verticalIncident,
                     float horizontalIncident, float excitation,
                     float tailIncident, float bridgeDisplacement,
                     float bridgeVelocity, float& directLeft,
                     float& directRight, float& sympatheticForce) noexcept;
    BodyOutput renderBody(float bridgeInput) noexcept;
    float nextNoise(Voice& voice) noexcept;

    EngineParameters targetParameters_ {};
    EngineParameters parameters_ {};
    PhysicalCalibration physicalCalibration_ { fittedPhysicalCalibration };
    std::array<Voice, stringCount> voices_ {};
    std::array<BodyMode, bodyModeCount> bodyModes_ {};
    std::array<BodyMode, bodyModeCount> fadingBodyModes_ {};
    BridgeLoad bridgeLoad_ {};
    FixedDerivative bridgeVelocityDerivative_ {};
    FixedDerivative bridgeForceDerivative_ {};
    FixedDerivative bridgeBodyForceDerivative_ {};
    FixedDerivative bridgeTailForceDerivative_ {};
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    float delaySmoothing_ { 0.001f };
    float parameterSmoothing_ { 0.002f };
    float levelSmoothing_ { 0.0025f };
    std::array<float, midiChannelCount> pitchBendSemitones_ {};
    float lastBridgeVelocity_ { 0.0f };
    float lastBridgeReactionForce_ { 0.0f };
    float lastBridgeBodyForce_ { 0.0f };
    float lastBridgeTailForce_ { 0.0f };
    float lastSympatheticRadiationForce_ { 0.0f };
    float lastBridgePower_ { 0.0f };
    float lastBridgeBodyPower_ { 0.0f };
    float lastBridgeTailPower_ { 0.0f };
    float bodyAmount_ { 0.82f };
    float width_ { 0.62f };
    float outputGain_ { 0.42f };
    float palmMute_ { 0.0f };
    float targetPalmMute_ { 0.0f };
    float palmMuteSmoothing_ { 0.5f };
    float bodyModelFade_ { 1.0f };
    float bodyModelFadeStep_ { 1.0f / 1920.0f };
    int controlCounter_ { 0 };
    int lowerZoneMemberCount_ { 0 };
    std::array<bool, midiChannelCount> sustainPedals_ {};
    bool bridgeCouplingEnabled_ { true };
    bool sympatheticStringsEnabled_ { true };
    // The strings do not leave the bridge when a note ends, so the junction
    // keeps the port they present rather than switching it out from under a
    // body that is still ringing.
    float lastImpedanceSum_ { 0.0f };
    float lastTailStiffnessSum_ { 0.0f };
    bool bridgeDerivativesNeedPriming_ { true };
    bool prepared_ { false };
    bool bodyConfigured_ { false };
    std::uint64_t noteOrder_ { 0 };
};

} // namespace acustra
