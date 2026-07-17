#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mars
{

enum class OscillatorWave { Saw, Pulse, Triangle };
enum class OscillatorModel { Vco, Dco };
enum class FilterModel { Ladder, Sem };
enum class VoiceMode { Poly, Unison, Fifth, Mono };
enum class LfoWaveform { Triangle, Sine, SampleHold };

struct EngineParameters
{
    OscillatorWave osc1Wave { OscillatorWave::Saw };
    OscillatorWave osc2Wave { OscillatorWave::Pulse };
    OscillatorModel osc1Model { OscillatorModel::Vco };
    OscillatorModel osc2Model { OscillatorModel::Vco };
    bool chorusCompander { false };
    FilterModel filterModel { FilterModel::Ladder };
    VoiceMode voiceMode { VoiceMode::Poly };
    LfoWaveform lfoWave { LfoWaveform::Triangle };
    bool osc1Enabled { true };
    bool osc2Enabled { true };
    int osc1Octave { 0 };
    int osc2Octave { 0 };
    int osc2Semitones { 0 };
    int polyphonyLimit { 16 };
    int unisonVoices { 4 };
    float oscMix { 0.42f };
    float osc2FineCents { 0.0f };
    float pulseWidth { 0.48f };
    float subLevel { 0.18f };
    float noiseLevel { 0.04f };
    float crossMod { 0.08f };
    float cutoffHz { 4200.0f };
    float resonance { 0.28f };
    float filterDrive { 0.24f };
    float filterShape { 0.35f };
    float filterEnvAmount { 0.42f };
    float filterKeyTrack { 0.45f };
    float filterAttack { 0.012f };
    float filterDecay { 0.45f };
    float filterSustain { 0.34f };
    float filterRelease { 0.62f };
    float ampAttack { 0.008f };
    float ampDecay { 0.38f };
    float ampSustain { 0.78f };
    float ampRelease { 0.55f };
    float lfoRateHz { 4.8f };
    float lfoPitchCents { 8.0f };
    float lfoFilterOctaves { 0.15f };
    float lfoPwm { 0.18f };
    float drift { 0.28f };
    float spread { 0.58f };
    float glideSeconds { 0.0f };
    float velocityAmount { 0.72f };
    float chorusMix { 0.32f };
    float chorusRateHz { 0.56f };
    float outputGain { 0.72f };
};

class MarsEngine
{
public:
    MarsEngine() noexcept;

    void prepare(double sampleRate, int maxBlockSize,
                 bool oversamplingEnabled = true);
    bool setOversamplingEnabled(bool enabled) noexcept;
    void reset();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    void setPitchBend(float normalisedBipolar) noexcept;
    void setModWheel(float amount) noexcept;
    void setSustainPedal(bool down) noexcept;
    void process(float* left, float* right, int numSamples);
    [[nodiscard]] int getActiveVoiceCount() const noexcept;
    [[nodiscard]] int getOversamplingFactor() const noexcept { return oversampling_; }
    [[nodiscard]] int getProcessingLatencySamples() const noexcept;
    [[nodiscard]] bool isOversamplingEnabled() const noexcept
    {
        return oversamplingRequested_;
    }

private:
    // The JUCE-free regression suite uses this narrow friend to inspect one
    // ladder step and verify the private implicit state against an independent
    // double-precision solve. It is not part of the plug-in API.
    friend struct MarsEngineTestAccess;

    static constexpr int maximumOversampleFactor = 4;
    static constexpr double minimumHqProcessingRate = 176400.0;
    static constexpr int maxVoices = 16;
    static constexpr int controlPeriod = 8;
    static constexpr int halfbandTaps = 137;
    static constexpr int halfbandRingSize = 256;
    static constexpr int latencyCompensationRingSize = 64;
    static constexpr int bbdStagePairs = 128;
    // JUCE hosts commonly top out at 384 kHz. Keeping an explicit ceiling well
    // above that protects coefficient calculations from pathological API input
    // without changing the timebase at any practical production sample rate.
    static constexpr double maximumSupportedSampleRate = 768000.0;

    enum class EnvelopeStage { Idle, Attack, Decay, Sustain, Release };

    struct Envelope
    {
        EnvelopeStage stage { EnvelopeStage::Idle };
        float value { 0.0f };

        void reset() noexcept;
        void noteOn() noexcept;
        void noteOff() noexcept;
        float tick(float attackCoefficient, float decayCoefficient,
                   float sustain, float releaseCoefficient) noexcept;
    };

    struct Oscillator
    {
        float phase { 0.0f };
        float dcoPhase { 0.0f };
        float triangle { -1.0f };
        float previousSawInput { 0.0f };
        float previousSawOutput { 0.0f };
        std::array<float, 2> vcoSawDelay {};
        std::array<float, 4> vcoSawCorrection {};
        std::array<float, 2> pulseDelay {};
        std::array<float, 4> pulseCorrection {};
        std::array<float, 2> triangleSquareDelay {};
        std::array<float, 4> triangleSquareCorrection {};
        std::array<float, 2> dcoSawDelay {};
        std::array<float, 4> dcoSawCorrection {};
        std::array<float, 2> dcoPulseDelay {};
        std::array<float, 4> dcoPulseCorrection {};
        std::array<float, 2> dcoTriangleSquareDelay {};
        std::array<float, 4> dcoTriangleSquareCorrection {};
        float dcoIncrement { 0.001f };
        float dcoRampVolts { 0.0f };
        float dcoHeldSlopeVoltsPerSecond { 5280.0f };
        float dcoHeldPulseWidth { 0.5f };
        float dcoPulseSlew { 1.0f };
        float dcoRangeClockHz { 2000000.0f };
        float dcoPendingRangeClockHz { 2000000.0f };
        float dcoResetResidue { 0.0f };
        float dcoChargeInjectionVolts { 0.014f };
        float dcoTriangle { -0.25f };
        float dcoExpectedPulseAtNextSample { 1.0f };
        float dcoLastResetSamplesToNext { 0.0f };
        float expectedPulseAtNextSample { 1.0f };
        std::array<float, 3> dcoReconstruction {};
        std::array<bool, 3> dcoReconstructionInitialised {};
        std::array<float, 3> waveformBlend { 1.0f, 0.0f, 0.0f };
        std::array<float, 3> waveformBlendStep {};
        float modelBlend { 0.0f };
        float modelBlendStep { 0.0f };
        int waveformCrossfadeRemaining { 0 };
        int modelCrossfadeRemaining { 0 };
        int dcoControlCountdown { 0 };
        std::uint32_t dcoTimerDivisor { 1u };
        std::uint32_t dcoPendingDivisor { 1u };
        OscillatorWave activeWave { OscillatorWave::Saw };
        OscillatorModel activeModel { OscillatorModel::Vco };
        bool sawContourInitialised { false };
        bool vcoSawBlepInitialised { false };
        bool pulseBlepInitialised { false };
        bool expectedPulseInitialised { false };
        bool triangleSquareBlepInitialised { false };
        bool dcoSawBlepInitialised { false };
        bool dcoPulseBlepInitialised { false };
        bool dcoTriangleSquareBlepInitialised { false };
        bool dcoExpectedPulseInitialised { false };
        bool dcoAnalogueInitialised { false };
        bool waveformInitialised { false };
        bool dcoClockInitialised { false };
        bool dcoWrappedThisSample { false };
        bool modelInitialised { false };
    };

    struct HalfbandDecimator
    {
        std::array<float, halfbandRingSize> left {};
        std::array<float, halfbandRingSize> right {};
        int writeIndex { 0 };

        void reset() noexcept;
    };

    struct ParallelAnalogFilter
    {
        std::array<float, 5> stateReal {};
        std::array<float, 5> stateImag {};
        std::array<float, 5> poleReal {};
        std::array<float, 5> poleImag {};
        std::array<float, 5> gainReal {};
        std::array<float, 5> gainImag {};
        float normalisation { 1.0f };

        void configure(float sampleRate, bool inputFilter,
                       float frequencyScale) noexcept;
        void reset() noexcept;
        float process(float input) noexcept;
    };

    struct BbdLine
    {
        std::array<float, bbdStagePairs> cells {};
        std::array<float, bbdStagePairs> transferLogHistory {};
        ParallelAnalogFilter inputFilter {};
        ParallelAnalogFilter outputFilter {};
        double clockPhase { 0.0 };
        float heldOutput { 0.0f };
        float transferMemory { 0.0f };
        float rollingTransferLog { 0.0f };
        std::array<float, 4> feedthroughCorrection {};
        float previousFilteredInput { 0.0f };
        std::uint32_t noiseState { 0x6d2b79f5u };
        int writeIndex { 0 };
        bool filteredInputInitialised { false };

        void configure(float sampleRate, float frequencyScale) noexcept;
        void reset(double initialClockPhase) noexcept;
        float process(float input, float clockFrequency,
                      float sampleRate, float parasiticGain = 1.0f) noexcept;
    };

    struct BbdCompander
    {
        static constexpr float nominalLineGain = 1.30316678f;
        float compressorEnvelope { 0.1f };
        float expanderEnvelope { 0.1f * nominalLineGain };
        float detectorCoefficient { 0.01f };

        void configure(float sampleRate) noexcept;
        void reset() noexcept;
        float compress(float input) noexcept;
        void expand(float& left, float& right) noexcept;
    };

    struct LadderFilter
    {
        std::array<float, 4> stageVoltage {};
        std::array<float, 4> stageTanh {};
        float previousInputVoltage { 0.0f };
        float previousFeedbackTanh { 0.0f };
        float cachedFeedbackGain { 0.0f };
        bool stageTanhValid { true };
        bool feedbackTanhValid { false };

        void reset() noexcept;
        float process(float inputVoltage, float frequencyTangent,
                      float feedbackGain, float frequencyScale) noexcept;
    };

    struct StateVariableFilter
    {
        float ic1eq { 0.0f };
        float ic2eq { 0.0f };

        void reset() noexcept;
        void process(float input, float g, float resonance,
                     float& low, float& band, float& high) noexcept;
    };

    struct VoiceCard
    {
        float osc1Cents { 0.0f };
        float osc2Cents { 0.0f };
        float cutoffError { 0.0f };
        float resonanceError { 0.0f };
        float envelopeError { 0.0f };
        float driveError { 0.0f };
        float panError { 0.0f };
        float pulseSkew { 0.0f };
        float driftRate { 0.08f };
        float driftDepth { 1.0f };
        float driftPhase { 0.0f };
        float driftSlow { 0.0f };
        float driftFast { 0.0f };
        std::uint32_t driftNoiseState { 1u };
    };

    struct Voice
    {
        bool active { false };
        bool releasing { false };
        bool keyDown { false };
        bool sustained { false };
        bool controlInitialised { false };
        bool filterModelInitialised { false };
        FilterModel activeFilterModel { FilterModel::Ladder };
        float filterBlend { 0.0f };
        float filterBlendStep { 0.0f };
        int filterCrossfadeRemaining { 0 };
        int rootMidi { -1 };
        int soundingMidi { 60 };
        int layer { 0 };
        int cardIndex { 0 };
        int controlCountdown { 0 };
        std::uint64_t generation { 0 };
        std::uint64_t ageSamples { 0 };
        std::uint32_t noiseState { 1u };
        float velocity { 0.0f };
        float targetVelocity { 0.0f };
        float currentMidi { 60.0f };
        float targetMidi { 60.0f };
        float groupGain { 1.0f };
        float unisonCents { 0.0f };
        float panBase { 0.0f };
        float panLeft { 0.7071067f };
        float panRight { 0.7071067f };
        float panLeftStep { 0.0f };
        float panRightStep { 0.0f };
        float driftPhase { 0.0f };
        float oscillator1Increment { 0.001f };
        float oscillator2Increment { 0.001f };
        float subIncrement { 0.0005f };
        float oscillator1Step { 0.0f };
        float oscillator2Step { 0.0f };
        float subStep { 0.0f };
        float ladderGain { 0.1f };
        float stateGain { 0.1f };
        float ladderGainStep { 0.0f };
        float stateGainStep { 0.0f };
        float resonance { 0.0f };
        float ladderFeedbackGain { 0.0f };
        float ladderFrequencyScale { 1.0f };
        float drive { 0.0f };
        float noiseColour { 0.0f };
        float ampAttackCoefficient { 1.0f };
        float ampDecayCoefficient { 1.0f };
        float ampReleaseCoefficient { 1.0f };
        float filterAttackCoefficient { 1.0f };
        float filterDecayCoefficient { 1.0f };
        float filterReleaseCoefficient { 1.0f };
        float previousMixerInput { 0.0f };
        float lastOutput { 0.0f };
        float outputEnergy { 0.0f };
        bool mixerInitialised { false };
        Envelope ampEnvelope {};
        Envelope filterEnvelope {};
        Oscillator oscillator1 {};
        Oscillator oscillator2 {};
        Oscillator subOscillator {};
        std::array<float, 2> dcoSubDelay {};
        std::array<float, 4> dcoSubCorrection {};
        float dcoSubReconstruction { 0.0f };
        bool dcoSubBlepInitialised { false };
        bool dcoSubReconstructionInitialised { false };
        bool dcoSubActive { false };
        bool oscillator1CycleOdd { false };
        LadderFilter ladder {};
        StateVariableFilter stateVariable {};
    };

    static EngineParameters sanitise(const EngineParameters& parameters) noexcept;
    static float midiToHz(float midiNote) noexcept;
    static float polyBlep(float phase, float increment) noexcept;
    static void addFourthOrderBlep(std::array<float, 4>& correction,
                                   float step, float samplesToNext) noexcept;
    static float processFourthOrderBlep(float input,
                                        std::array<float, 2>& delay,
                                        std::array<float, 4>& correction,
                                        bool& initialised) noexcept;
    static int waveformIndex(OscillatorWave waveform) noexcept;
    static float halfbandCoefficient(int tap) noexcept;
    static float softSaturate(float value) noexcept;
    static float softSaturateDerivative(float value) noexcept;
    static float adaaShape(float value) noexcept;
    static float adaaAntiderivative(float value) noexcept;
    static float processAdaaMixer(float value, Voice& voice) noexcept;
    static float filterInputVoltage(float value, float drive) noexcept;
    static float wrapPhase(float phase) noexcept;
    static float smoothStep(float value) noexcept;
    static float envelopeCoefficient(float seconds, float sampleRate) noexcept;
    static std::uint32_t hash32(std::uint32_t value) noexcept;
    static float hashBipolar(std::uint32_t value) noexcept;

    void buildVoiceCards() noexcept;
    void initialiseVoice(Voice& voice, int slot, int rootMidi, int soundingMidi,
                         int layer, int layerCount, float velocity,
                         const EngineParameters& parameters) noexcept;
    void silenceVoice(Voice& voice, bool preserveTail = false) noexcept;
    void addVoiceToStealTail(const Voice& voice) noexcept;
    void renderStealTail(float& left, float& right) noexcept;
    void updateVoiceControl(Voice& voice, const EngineParameters& parameters,
                            float lfoValue) noexcept;
    float renderOscillator(Oscillator& oscillator, OscillatorWave waveform,
                           OscillatorModel model, float increment, float pulseWidth,
                           float dcoRangeClockHz, bool& wrapped) noexcept;
    float renderVoiceOversample(Voice& voice, const EngineParameters& parameters,
                                float lfoValue) noexcept;
    void downsamplePair(HalfbandDecimator& decimator,
                        float firstLeft, float firstRight,
                        float secondLeft, float secondRight,
                        float& outputLeft, float& outputRight) noexcept;
    int layersForMode(const EngineParameters& parameters) const noexcept;
    int fifthIntervalForLayer(int layer) const noexcept;
    int findFreeVoice() const noexcept;
    std::uint64_t selectStealGeneration(bool releasingOnly) const noexcept;
    void makeRoomFor(int required, int voiceLimit) noexcept;
    void enforcePolyphonyLimit(int voiceLimit) noexcept;
    int findMonoVoice() const noexcept;
    void rememberHeldNote(int midiNote, float velocity) noexcept;
    void forgetHeldNote(int midiNote) noexcept;
    void clearHeldNotes() noexcept;
    void retargetMonoVoice(Voice& voice, int midiNote, float velocity) noexcept;
    void updateActiveVoiceCount() noexcept;
    float nextNoise(Voice& voice) noexcept;
    float nextLfoValue(const EngineParameters& parameters) noexcept;
    void updateVoiceCardDrift(VoiceCard& card) noexcept;
    void processChorus(float inputLeft, float inputRight,
                       const EngineParameters& parameters,
                       float& outputLeft, float& outputRight) noexcept;
    float processDcBlocker(float input, float& previousInput,
                           float& previousOutput) const noexcept;
    void updateProcessingRate() noexcept;
    bool applyPendingOversamplingIfIdle() noexcept;

    EngineParameters targetParameters_ {};
    EngineParameters smoothedParameters_ {};
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    float oversampledRate_ { 192000.0f };
    int oversampling_ { 4 };
    bool oversamplingEnabled_ { true };
    bool oversamplingRequested_ { true };
    int oversamplingIdleSamples_ { 0 };
    int filterCrossfadeSamples_ { 288 };
    int oscillatorModelCrossfadeSamples_ { 192 };
    int waveformCrossfadeSamples_ { 288 };
    float dcoReconstructionGain_ { 0.69f };
    float dcoPulseRise_ { 0.8f };
    float dcoPulseFall_ { 0.7f };
    int dcoControlPeriodSamples_ { 806 };
    float velocitySmoothing_ { 0.02f };
    float outputEnergySmoothing_ { 0.01f };
    float driftSlowRho_ { 0.999985f };
    float driftFastRho_ { 0.999445f };
    float driftSlowExcitation_ { 0.00945f };
    float driftFastExcitation_ { 0.05773f };
    float noiseColourCoefficient_ { 0.1702f };
    bool prepared_ { false };
    std::uint64_t generation_ { 0 };
    int activeVoiceCount_ { 0 };
    int driftControlCountdown_ { 0 };
    float lastPlayedMidi_ { 60.0f };
    bool hasLastPlayedMidi_ { false };
    std::array<int, 128> heldNoteOrder_ {};
    std::array<float, 128> heldNoteVelocities_ {};
    std::array<std::uint16_t, 128> heldNoteCounts_ {};
    int heldNoteCount_ { 0 };

    std::array<Voice, maxVoices> voices_ {};
    std::array<VoiceCard, maxVoices> cards_ {};
    HalfbandDecimator firstDecimator_ {};
    HalfbandDecimator secondDecimator_ {};
    std::array<float, latencyCompensationRingSize> latencyDelayLeft_ {};
    std::array<float, latencyCompensationRingSize> latencyDelayRight_ {};
    int latencyDelayWriteIndex_ { 0 };

    float lfoPhase_ { 0.0f };
    float randomLfoValue_ { 0.0f };
    std::uint32_t globalNoiseState_ { 0x6d2b79f5u };
    float pitchBendTarget_ { 0.0f };
    float pitchBend_ { 0.0f };
    float modWheelTarget_ { 0.0f };
    float modWheel_ { 0.0f };
    bool sustainPedalDown_ { false };
    float oscillator1MixGain_ { 0.7615773f };
    float oscillator2MixGain_ { 0.6480741f };
    float stealTailLeft_ { 0.0f };
    float stealTailRight_ { 0.0f };
    float stealTailCoefficient_ { 0.96f };

    BbdLine chorusLeft_ {};
    BbdLine chorusRight_ {};
    BbdCompander chorusCompander_ {};
    float chorusPhase_ { 0.0f };
    float chorusActivity_ { 0.0f };
    float chorusActivityAttack_ { 0.05f };
    float chorusActivityRelease_ { 0.99f };
    float companderBlend_ { 0.0f };
    float companderBlendCoefficient_ { 0.01f };

    float dcCoefficient_ { 0.9987f };
    float dcInputLeft_ { 0.0f };
    float dcInputRight_ { 0.0f };
    float dcOutputLeft_ { 0.0f };
    float dcOutputRight_ { 0.0f };
};

} // namespace mars
