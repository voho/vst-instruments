// Ghost: a circuit-modelled monophonic dual-filter analog synthesizer,
// built block by block from documentation of the 1983 instrument named in
// Docs/circuit-modelling-research.md. That document is the modelling
// contract: every law the engine applies is recorded there as anchored,
// derived or voiced, and the voiced constants are listed as standing tasks
// in Docs/open-questions.md.
#pragma once

#include <array>
#include <cstdint>

namespace ghost
{

// Panel switch positions. Every enumerator is a physical detent on the
// modelled front panel, so the order is the panel order (which wins over the
// owner's-manual prose where the two disagree — see the research document).
enum class Waveform
{
    Triangle,
    RectWide,     // A: 50 %  B: 40 %
    RectMid,      // A: 30 %  B: 20 %
    RectNarrow,   // A: 15 %  B: 10 %
    RectThin,     // A:  6 %  B:  3 %
    Sawtooth
};
enum class MasterOctave { ThirtyTwo, Sixteen, Eight, Four };
enum class OscBRange { MinusOne, Unison, PlusOne, PlusTwo, Bass, Wide };
enum class LowerFilterMode { Out, Overdrive, BandPass, HighPass };
enum class UpperSlope { TwelveDb, TwentyFourDb };
enum class UpperResonanceMode { Low, Variable };
enum class TrackingMode { Formant, Dynamic };
enum class ModSource
{
    LfoTriangle,
    LfoSquare,
    SampleHoldRandom,
    SampleHoldY,
    RedNoise,
    OscB
};
enum class ModXDestination { Off, OscAB, OscA, OscARwm, FilterUL, FilterU };
enum class ShaperYDestination { Off, OscAB, OscB, OscBRwm, LfoRate, FilterL };
enum class ShaperMode { Free, KbdHold, Reset, Run };
enum class ArpeggiatorMode { Off, Ripple, Arpeggio, Leap };
enum class GlideMode { Off, Auto, On };
enum class TriggerMode { Single, Multiple };

struct EngineParameters
{
    // Panel travel is 0..1 throughout: the engine maps each control through
    // the modelled hardware law (documented control by control in
    // Docs/circuit-modelling-research.md) rather than storing pre-cooked
    // seconds or hertz.

    // --- MASTER ------------------------------------------------------------
    float tune { 0.5f };                       // ± a minor third; 0.5 = centre
    MasterOctave octave { MasterOctave::Eight };

    // --- OSCILLATOR A -----------------------------------------------------
    Waveform oscAWaveform { Waveform::Sawtooth };
    bool sync { false };                       // hard sync, A resets B

    // --- OSCILLATOR B -----------------------------------------------------
    Waveform oscBWaveform { Waveform::Sawtooth };
    OscBRange oscBRange { OscBRange::Unison };
    // ± a perfect fifth in the octave positions; the drone pitch in
    // BASS (30..300 Hz) and WIDE (2..10,000 Hz).
    float interval { 0.5f };

    // --- AUDIO MIXER ------------------------------------------------------
    float masterVolume { 0.8f };
    float brightness { 1.0f };                 // Shaper path 6 dB/oct lowpass
    float shaperPathA { 0.0f };
    float shaperPathB { 0.0f };
    float shaperPathRing { 0.0f };
    float shaperPathNoise { 0.0f };
    float filterPathA { 0.8f };
    float filterPathB { 0.0f };
    float filterPathNoise { 0.0f };

    // --- UPPER FILTER U / LOWER FILTER L ------------------------------------
    float cutoff { 0.62f };                    // MASTER: both filters, always
    float lowerOnly { 0.8f };                  // cutoffs coincide at 0.8
    UpperResonanceMode upperResonance { UpperResonanceMode::Low };
    float resonance { 0.1f };                  // Lower always; Upper if VARIABLE
    UpperSlope slope { UpperSlope::TwentyFourDb };
    float kbAmount { 0.5f };                   // 0 .. ~110 % tracking at 1
    LowerFilterMode lowerMode { LowerFilterMode::Out };
    TrackingMode tracking { TrackingMode::Dynamic };

    // --- FILTER ENVELOPE ----------------------------------------------------
    // Bipolar AMOUNT, centre 0.5 = no effect; full travel spans ±2.5 octaves
    // straddling the cutoff; below centre the envelope inverts.
    float filterEnvAmount { 0.5f };
    float filterAttack { 0.0f };
    float filterDecay { 0.45f };
    float filterSustain { 0.5f };
    float filterRelease { 0.3f };

    // --- LOUDNESS ENVELOPE --------------------------------------------------
    bool vcaBypass { false };                  // holds the path VCA fully open
    float loudnessAttack { 0.0f };
    float loudnessDecay { 0.45f };
    float loudnessSustain { 0.8f };
    float loudnessRelease { 0.3f };

    // --- TRIGGER / GATE SELECT ----------------------------------------------
    TriggerMode trigger { TriggerMode::Multiple };
    // At least one gate source must be on for the envelopes to run at all;
    // the hardware prints exactly that warning on the panel.
    bool gateKbd { true };
    bool gateX { false };                      // the LFO square: auto-repeat
    bool gateYExt { false };                   // the Shaper's own gate

    // --- MOD X --------------------------------------------------------------
    ArpeggiatorMode arpeggiator { ArpeggiatorMode::Off };
    ModSource modSource { ModSource::LfoTriangle };
    float lfoRate { 0.42f };                   // <1 Hz .. ~50 Hz; also the
                                               // S&H and arpeggiator clock

    // --- SHAPER Y -----------------------------------------------------------
    ShaperMode shaperMode { ShaperMode::Free };
    float shaperShape { 0.5f };                // rise/fall split of the period
    float shaperRate { 0.5f };

    // --- WHEEL DESTINATIONS -------------------------------------------------
    ModXDestination modXTo { ModXDestination::Off };
    bool shapeXWithY { false };                // Y envelopes the X wheel signal
    ShaperYDestination shaperYTo { ShaperYDestination::Off };

    // --- Performance --------------------------------------------------------
    float glide { 0.0f };
    GlideMode glideMode { GlideMode::Off };

    // --- Product policy (not hardware controls) -----------------------------
    // The hardware puts each audio path on its own rear jack; Ghost renders
    // the mixed signal to both channels unless asked to split the Filter/ADSR
    // path left and the Shaper Y path right.
    bool splitPaths { false };
};

class GhostEngine
{
public:
    GhostEngine() noexcept;

    // The host rates prepare() will run at. Anything outside this range is
    // clamped into it rather than reaching the internal grid.
    static constexpr double minimumSupportedSampleRate = 8000.0;
    static constexpr double maximumSupportedSampleRate = 768000.0;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    // The bend lever: full travel is ±8 semitones, derived by anchoring the
    // bend network against the tune network (see the research document).
    void setPitchBend(float normalisedBipolar) noexcept;
    // The two performance wheels are attenuators toward zero volts.
    void setModWheel(float amount) noexcept;       // MOD X wheel
    void setShaperWheel(float amount) noexcept;    // SHAPER Y wheel
    void process(float* left, float* right, int numSamples);

    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] bool isGateOpen() const noexcept { return keyGate_; }
    [[nodiscard]] int getCurrentNote() const noexcept { return currentNote_; }

private:
    // ------------------------------------------------------------------ DSP
    struct Adsr
    {
        enum class Stage { Idle, Attack, Decay, Release };
        Stage stage { Stage::Idle };
        double level { 0.0 };
    };

    // One 2-pole state-variable section (TPT), with the diode limiter that
    // bounds the hardware's resonant node instead of the supply rails.
    struct SvfSection
    {
        double ic1 { 0.0 };
        double ic2 { 0.0 };
    };

    struct SvfOutputs
    {
        double lp;
        double bp;
        double hp;
    };

    static SvfOutputs runSection(SvfSection& section, double input, double g,
                                 double k) noexcept;

    void advanceControls() noexcept;
    void advanceEnvelope(Adsr& envelope, bool gate, bool triggerPulse,
                         double attackCoefficient, double decayCoefficient,
                         double releaseCoefficient, double sustain) noexcept;
    void renderVoiceSample() noexcept;
    void handleArpClock() noexcept;

    EngineParameters parameters_ {};
    double sampleRate_ { 44100.0 };
    double internalRate_ { 88200.0 };   // fixed 2x oversampling

    // Cached per-parameter-change values (updated in setParameters).
    double oscADuty_ { 0.5 };
    double oscBDuty_ { 0.5 };

    // --- Keyboard ----------------------------------------------------------
    // Held-key memory: a released newer key falls back to the newest key
    // still down, at its pitch, without retriggering — as the hardware's
    // scanner does. The capacity covers the whole MIDI note domain.
    static constexpr int keyStackCapacity = 128;
    std::array<std::int16_t, keyStackCapacity> keyStack_ {};
    int keyStackSize_ { 0 };
    bool keyGate_ { false };
    int currentNote_ { -1 };
    bool pendingTrigger_ { false };

    float pitchBend_ { 0.0f };
    float modWheel_ { 0.0f };
    float shaperWheel_ { 0.0f };

    // --- Control state (advanced once per output sample) --------------------
    double glidedNote_ { 60.0 };
    bool glideInitialised_ { false };

    double lfoPhase_ { 0.0 };
    bool lfoSquareHigh_ { false };
    double redNoiseState_ { 0.0 };
    double sampleHoldValue_ { 0.0 };
    std::uint32_t noiseSeed_ { 0x9e3779b9u };

    double shaperLevel_ { 0.0 };
    bool shaperRising_ { true };
    bool shaperCycleActive_ { false };
    bool shaperGate_ { false };
    bool previousGateForShaper_ { false };

    Adsr filterEnvelope_ {};
    Adsr loudnessEnvelope_ {};

    // Arpeggiator
    int arpStep_ { 0 };
    int arpSoundingNote_ { -1 };

    // Latest control-rate results consumed by the audio-rate voice.
    double controlOscAOctaves_ { 0.0 };
    double controlOscBOctaves_ { 0.0 };
    bool controlOscBDrone_ { false };
    double controlOscBDroneHz_ { 110.0 };
    double controlPwmA_ { 0.0 };
    double controlPwmB_ { 0.0 };
    double controlUpperCutoffHz_ { 1000.0 };
    double controlLowerCutoffHz_ { 1000.0 };
    double controlUpperK_ { 1.5 };
    double controlLowerK_ { 1.5 };
    double controlLoudnessGain_ { 0.0 };
    double controlShaperVcaGain_ { 0.0 };
    double controlBrightnessCoefficient_ { 1.0 };

    // --- Audio state (advanced at the internal rate) ------------------------
    double phaseA_ { 0.0 };
    double phaseB_ { 0.0 };
    double lastOscBWave_ { 0.0 };
    double pinkState_[3] { 0.0, 0.0, 0.0 };
    double brightnessState_ { 0.0 };

    SvfSection lowerSection_ {};
    SvfSection upperFirst_ {};
    SvfSection upperSecond_ {};

    // Halfband decimator history for the 2x -> 1x output boundary, one ring
    // per audio path so the split-path output decimates cleanly.
    static constexpr int halfbandTaps = 63;
    std::array<double, halfbandTaps> filterRing_ {};
    std::array<double, halfbandTaps> shaperRing_ {};
    int decimatorIndex_ { 0 };
    std::array<double, halfbandTaps> halfbandKernel_ {};

    double lastFilterPathSample_ { 0.0 };
    double lastShaperPathSample_ { 0.0 };

    // Output AC coupling, as the hardware's series capacitors provide: a
    // rectangular waveform must not carry its duty-cycle DC to the jack.
    double dcPreviousInLeft_ { 0.0 };
    double dcPreviousOutLeft_ { 0.0 };
    double dcPreviousInRight_ { 0.0 };
    double dcPreviousOutRight_ { 0.0 };
};

} // namespace ghost
