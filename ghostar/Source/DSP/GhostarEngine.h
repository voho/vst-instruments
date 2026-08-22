// Ghostar: a circuit-modelled monophonic dual-filter analog synthesizer,
// built block by block from documentation of the 1983 instrument named in
// Docs/circuit-modelling-research.md. That document is the modelling
// contract: every law the engine applies is recorded there as anchored,
// derived or voiced, and the voiced constants are listed as standing tasks
// in Docs/open-questions.md.
#pragma once

#include <array>
#include <cstdint>

namespace ghostar
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
    // The hardware puts each audio path on its own rear jack; Ghostar renders
    // the mixed signal to both channels unless asked to split the Filter/ADSR
    // path left and the Shaper Y path right.
    bool splitPaths { false };
};

class GhostarEngine
{
public:
    GhostarEngine() noexcept;

    // The host rates prepare() will run at. Anything outside this range is
    // clamped into it rather than reaching the internal grid.
    static constexpr double minimumSupportedSampleRate = 8000.0;
    static constexpr double maximumSupportedSampleRate = 768000.0;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    // MIDI All Sound Off: kills every sounding voice like reset(), but
    // keeps the controller state — bend and wheels are player positions,
    // and CC120 is specified to stop sound without resetting controllers.
    void stopAllSound();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    // Releases every held key through the normal envelope path, as a
    // controller asking for all notes off means. reset() is the hard stop.
    void releaseAllKeys() noexcept;
    // The bend lever: full travel is ±8 semitones, derived by anchoring the
    // bend network against the tune network (see the research document).
    void setPitchBend(float normalisedBipolar) noexcept;
    // The two performance wheels are attenuators toward zero volts.
    void setModWheel(float amount) noexcept;       // MOD X wheel
    void setShaperWheel(float amount) noexcept;    // SHAPER Y wheel
    void process(float* left, float* right, int numSamples);

    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate_; }
    // The keyboard's own gate: whether a key is down.
    [[nodiscard]] bool isGateOpen() const noexcept { return keyGate_; }
    // The gate the envelopes actually answer to — the OR of whichever of
    // KBD, X and Y/EXT are selected. A patch gated from the LFO square or
    // the Shaper articulates with no key down, and a key with KBD
    // deselected articulates nothing, so a display that means "is this
    // instrument sounding a note" must read this and not the keyboard.
    [[nodiscard]] bool isEnvelopeGateOpen() const noexcept
    {
        return envelopeGate_;
    }

    // How long the loudness envelope's release can ring, worst case: the
    // longest segment time constant carried down to the level at which the
    // envelope idles. Derived from the same law the engine runs, so a
    // change to the envelope timing cannot leave a stale figure behind in
    // the plug-in's advertised tail.
    [[nodiscard]] static double longestReleaseTailSeconds() noexcept;
    [[nodiscard]] int getCurrentNote() const noexcept { return currentNote_; }

    // How far behind its own instant the voice reaches the host, in output
    // samples. Both decimation stages are linear-phase, so each delays by
    // exactly half its length: 15 internal samples for the 31-tap stage at
    // 4x, and 63 stage-B samples — 126 internal — for the 127-tap stage at
    // 2x. The output sample is taken after the fourth internal step, which
    // gives three of those back, leaving 138 internal samples: 34.5 output
    // samples, whatever the host rate. Measured on a one-sample gate burst
    // through the real chain, the burst's centroid lands 34.5005 samples
    // late, and the step response crosses half at 34.409 at 44.1, 48 and
    // 96 kHz.
    [[nodiscard]] static constexpr double outputLatencySamples() noexcept
    {
        constexpr int oversample = 4;
        constexpr int internalDelay = (stageATaps - 1) / 2
                                    + (stageBTaps - 1) / 2 * 2
                                    - (oversample - 1);
        return static_cast<double>(internalDelay)
             / static_cast<double>(oversample);
    }

    // How many taps each decimation stage actually visits per output sample.
    // Exposed because a halfband's sparsity is invisible from the outside:
    // losing the structural zeros costs about twice the decimator's
    // arithmetic and changes nothing audible, so nothing else would notice.
    [[nodiscard]] int decimatorStageATaps() const noexcept
    {
        return stageAKernel_.count;
    }
    [[nodiscard]] int decimatorStageBTaps() const noexcept
    {
        return stageBKernel_.count;
    }

private:
    // ------------------------------------------------------------------ DSP
    struct Adsr
    {
        enum class Stage { Idle, Attack, Decay, Release };
        Stage stage { Stage::Idle };
        double level { 0.0 };
    };

    // One 2-pole state-variable section (TPT), with the diode shunt that
    // bounds the hardware's resonant node instead of the supply rails —
    // solved as an exact sub-step of the continuous equation, so the
    // section converges to the same filter at every rate.
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
                                 double k, double diodeDecay) noexcept;

    // True when nothing is sounding and nothing can sound (no keys, both
    // envelopes' gate paths closed, no drone), so travel and wheel changes
    // may snap instead of gliding — a state restore lands exactly.
    [[nodiscard]] bool silentForSnap() const noexcept;
    void advanceControls() noexcept;
    void advanceEnvelope(Adsr& envelope, bool gate, bool triggerPulse,
                         double attackCoefficient, double decayCoefficient,
                         double releaseCoefficient, double sustain) noexcept;
    void renderVoiceSample() noexcept;
    void handleArpClock() noexcept;

    // parameters_ carries what the voice actually runs on this sample;
    // continuous travels glide toward targetParameters_ over ~25 ms so a
    // block-latched host or a 7-bit CC never steps the audio (the
    // best-in-class plan's Step 5). Switches apply immediately, and a
    // fully silent engine snaps, so state restores land exactly.
    EngineParameters parameters_ {};
    EngineParameters targetParameters_ {};
    float targetModWheel_ { 0.0f };
    float targetShaperWheel_ { 0.0f };
    double travelSmoothing_ { 1.0 };
    double sampleRate_ { 44100.0 };
    double internalRate_ { 176400.0 };  // fixed 4x oversampling
    // exp(-lambda / internalRate_): the diode sub-step's per-internal-sample
    // decay, precomputed so the shunt's law is a rate, not a map.
    double diodeDecay_ { 1.0 };

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
    // The OR'ed gate bus as advanceControls() last computed it.
    bool envelopeGate_ { false };
    int currentNote_ { -1 };
    bool pendingTrigger_ { false };
    // RESET mode is always multiple-trigger regardless of the TRIGGER
    // switch, so the Shaper needs its own record of every key press —
    // pendingTrigger_ above answers to SINGLE/MULTIPLE and cannot serve.
    bool pendingShaperTrigger_ { false };

    float pitchBend_ { 0.0f };
    float modWheel_ { 0.0f };
    float shaperWheel_ { 0.0f };

    // --- Control state (advanced once per output sample) --------------------
    double glidedNote_ { 60.0 };
    bool glideInitialised_ { false };

    double lfoPhase_ { 0.0 };
    bool lfoSquareHigh_ { false };
    bool previousLfoSquareHigh_ { false };
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

    // MOD SOURCE = OSC B is an audio signal, not a control signal: its
    // routing is published here and read by the voice per internal sample,
    // so a 10 kHz WIDE modulator is neither undersampled nor applied as a
    // staircase. Depths are per unit of source; gain is the wheel (and the
    // SHAPE X WITH Y VCA).
    struct AudioRateMod
    {
        bool active { false };
        double gain { 0.0 };
        double aOctaves { 0.0 };
        double bOctaves { 0.0 };
        double upperOctaves { 0.0 };
        double lowerOctaves { 0.0 };
        double duty { 0.0 };
    };
    AudioRateMod controlAudioRateMod_ {};

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
    // The bandlimited oscillator core emits with one internal sample of
    // delay: an event found mid-sample corrects the held sample exactly
    // instead of predicting the event a sample ahead. Selected wave and
    // ring triangle are corrected as separate channels per oscillator.
    double heldWaveA_ { 0.0 };
    double heldTriA_ { 0.0 };
    double heldWaveB_ { 0.0 };
    double heldTriB_ { 0.0 };
    double heldDutyA_ { 0.5 };
    double heldDutyB_ { 0.5 };
    double lastOscBWave_ { 0.0 };
    double pinkState_[3] { 0.0, 0.0, 0.0 };
    // Pinking poles re-derived for the internal rate in prepare(), so the
    // noise colour does not move with the host rate.
    std::array<double, 3> pinkA_ { 0.99765, 0.96300, 0.57000 };
    std::array<double, 3> pinkG_ { 0.0990460, 0.2965164, 1.0526913 };
    double brightnessState_ { 0.0 };

    SvfSection lowerSection_ {};
    SvfSection upperFirst_ {};
    SvfSection upperSecond_ {};

    // Two-stage decimation for the 4x -> 1x output boundary, one ring per
    // stage per audio path so the split-path output decimates cleanly. The
    // first stage's transition band is wide (nothing between 0.55 and 1.55
    // of the host Nyquist can fold into the audio band before the second
    // stage has its say), so it stays short; the second stage carries the
    // sharp cut — flat to 0.45 of the host rate, ~98 dB down from 0.55 —
    // that keeps near-Nyquist images out of the measured band.
    static constexpr int stageATaps = 31;
    static constexpr int stageBTaps = 127;
    // A halfband kernel's even offsets from the centre are structurally
    // zero, so only the nonzero taps are stored and visited.
    template <std::size_t taps>
    struct SparseHalfband
    {
        // How far newer than the ring's oldest sample each stored tap
        // reaches, so the convolution needs no index arithmetic per tap.
        std::array<int, taps> offsets {};
        std::array<double, taps> values {};
        int count { 0 };
    };
    SparseHalfband<stageATaps> stageAKernel_ {};
    SparseHalfband<stageBTaps> stageBKernel_ {};
    std::array<double, stageATaps> filterStageARing_ {};
    std::array<double, stageATaps> shaperStageARing_ {};
    int stageAIndex_ { 0 };
    std::array<double, stageBTaps> filterStageBRing_ {};
    std::array<double, stageBTaps> shaperStageBRing_ {};
    int stageBIndex_ { 0 };
    // The white generator draws once per internal sample, so its per-hertz
    // density falls as the internal rate rises; this rescale keeps the
    // audible-band density at the level the mixer laws were voiced at.
    double noiseAmplitude_ { 1.0 };

    double lastFilterPathSample_ { 0.0 };
    double lastShaperPathSample_ { 0.0 };

    // Output AC coupling, as the hardware's series capacitors provide: a
    // rectangular waveform must not carry its duty-cycle DC to the jack.
    double dcPreviousInLeft_ { 0.0 };
    double dcPreviousOutLeft_ { 0.0 };
    double dcPreviousInRight_ { 0.0 };
    double dcPreviousOutRight_ { 0.0 };
};

} // namespace ghostar
