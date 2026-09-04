// Ghostar: a circuit-modelled monophonic dual-filter analog synthesizer,
// built block by block from documentation of the 1983 instrument named in
// the README's "How it works", which is the modelling contract: every law
// the engine applies is recorded there as anchored, derived or voiced, with
// the primary sources it rests on, and the voiced constants are listed as
// standing tasks under the README's "Known gaps".
#pragma once

#include "DSP/SpiritNoise.h"

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
    // the modelled hardware law (documented control by control in the
    // README) rather than storing pre-cooked
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
    float brightness { 1.0f };                 // post-Shaper-VCA passive shelf
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
    // Bipolar AMOUNT, centre 0.5 = no effect. The published full-travel law
    // mirrors the envelope across CUTOFF from -2.5 to +2.5 octaves.
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
    // The bend lever: the network derives ±15.88 semitones at the pot's full
    // electrical travel; ±8 is the explicitly voiced spring-wheel endpoint.
    void setPitchBend(float normalisedBipolar) noexcept;
    // The two performance wheels are attenuators toward zero volts.
    void setModWheel(float amount) noexcept;       // MOD X wheel
    void setShaperWheel(float amount) noexcept;    // SHAPER Y wheel
    // The rear EXTERNAL GATE socket is switched: no plug normals the
    // Shaper's SG gate into Y/EXT, while an inserted plug replaces SG with
    // the external voltage. The factory manual specifies a strict >6 V
    // acceptance threshold, so 6.0 V itself remains low.
    void setExternalGateInput(bool jackInserted, double volts) noexcept;
    // P1017's EXTERNAL PITCH socket is switched ahead of the Glide network.
    // Empty, it passes the keyboard/arpeggiator CV; inserted, a Spirit
    // Keyboard Pitch Out-equivalent source behind 15k replaces pitch only
    // while the keyboard keeps supplying gate and trigger. The engine
    // applies the documented D/N/P loading itself.
    void setExternalPitchInput(bool jackInserted,
                               double sourceVolts) noexcept;
    // The unswitched OSC B PEDAL socket accepts a 100k potentiometer wired
    // as a tip-to-sleeve rheostat. Cable absence is infinite resistance; a
    // connected value is clamped to its 0..100 kOhm travel. This remains a
    // live performance input, outside programs.
    void setOscBPedalInput(bool jackInserted,
                           double resistanceKOhm) noexcept;
    // FILTER PEDAL uses the same physical pedal contract. Its switched
    // cutoff destinations are resolved by the circuit model below.
    void setFilterPedalInput(bool jackInserted,
                             double resistanceKOhm) noexcept;
    // P1017's EXTERNAL AUDIO socket is another switched contact. Empty, it
    // normals IC4A's audible pink-noise output to both NOISE sliders;
    // inserted, the mono tip replaces that signal while RED NOISE continues
    // from its upstream branch. Jack presence is deliberately independent
    // of whether a host has routed or enabled the optional input bus.
    void setExternalAudioInput(bool jackInserted) noexcept;
    void process(float* left, float* right, int numSamples);
    void process(const float* externalAudio, float* left, float* right,
                 int numSamples);

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

    // How long the loudness path's release can remain audible, worst case:
    // the longest segment time constant carried down to the CEM3360 control
    // network's derived 1/15 zero-gain threshold. Derived from the same law
    // the engine runs, so a
    // change to the envelope timing cannot leave a stale figure behind in
    // the plug-in's advertised tail.
    [[nodiscard]] static double longestReleaseTailSeconds() noexcept;
    [[nodiscard]] int getCurrentNote() const noexcept { return currentNote_; }

    // Host audio reaches the 4x circuit grid through the same two halfbands
    // in reverse order. Their centres are 63 samples at 2x plus 15 at 4x:
    // 141 internal ticks, or 35.25 host samples. The four internally
    // generated mixer sources take an equal FIFO delay, so inserted and
    // normalled signals remain phase-aligned and jack changes never move the
    // instrument in time.
    [[nodiscard]] static constexpr int
    externalInputLatencyInternalSamples() noexcept
    {
        return (stageATaps - 1) / 2 + (stageBTaps - 1) / 2 * 2;
    }

    // The output decimator has the same 141-tick linear-phase centre, but
    // the host sample is taken after internal substep three, leaving 138
    // ticks, or 34.5 host samples.
    [[nodiscard]] static constexpr double decimatorLatencySamples() noexcept
    {
        constexpr int oversample = 4;
        return static_cast<double>(
                   externalInputLatencyInternalSamples() - (oversample - 1))
             / static_cast<double>(oversample);
    }

    // Both signal origins now reach the host after 141 + 138 internal ticks:
    // 69.75 samples at every supported host rate. A host can publish only an
    // integer delay, so the processor reports the nearest sample.
    [[nodiscard]] static constexpr double outputLatencySamples() noexcept
    {
        return static_cast<double>(externalInputLatencyInternalSamples()) / 4.0
             + decimatorLatencySamples();
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
    friend struct GhostarCircuitTestAccess;

    // ------------------------------------------------------------------ DSP
    struct Adsr
    {
        enum class Stage { Idle, Attack, Decay, Release };
        Stage stage { Stage::Idle };
        double level { 0.0 };
    };

    // R82/C72 and R118/C77 bypass the two CEM3340 multiplier outputs.  Each
    // capacitor therefore remembers the complete pitch-current sum before
    // the exponential converter, including audio-rate modulation.
    struct PitchControlLag
    {
        double output { 0.0 };
        double previousInput { 0.0 };
        bool initialised { false };
    };

    // One 2-pole TPT state-variable section. The Lower keeps the same two
    // CEM state companions but solves its three moving mixer inputs around
    // them in runLowerSection(); the Upper couples two of these sections in
    // runUpperCascade() because SW4 moves a live timing capacitor between
    // them.
    struct SvfSection
    {
        double ic1 { 0.0 };
        double ic2 { 0.0 };
    };

    struct HighQBranch
    {
        // Trapezoidal companion for charge transferred through C33/C37,
        // normalised to the CEM's 22 nF timing capacitor and engine units.
        double chargeCompanion { 0.0 };
        double amplifierGain { 0.0 };
        double sourceResistanceOhms { 0.0 };
    };

    struct SvfOutputs
    {
        double lp;
        double bp;
        double hp;
    };

    struct OutputWipers
    {
        double filter;
        double shaper;
        double shaperTop;
    };

    struct ExternalPitchNodes
    {
        double loadedVolts;
        double conditionedVolts;
    };

    static constexpr double keyboardPitchSourceOhms = 2.2e3;
    static constexpr double externalPitchSourceOhms = 15.0e3;
    static constexpr double externalPitchInputOhms = 95.3e3;
    static constexpr double externalPitchFeedbackOhms = 100.0e3;
    static constexpr double externalPitchInputFarads = 100.0e-9;

    [[nodiscard]] static constexpr ExternalPitchNodes externalPitchNodes(
        double sourceVolts) noexcept
    {
        // A Spirit-compatible source is Thevenin-equivalent to D behind
        // P1017 R1=15k. IC16B receives N through R42=95.3k and uses
        // R44=100k feedback, producing the inverted P/KCV voltage.
        const double loaded = sourceVolts * externalPitchInputOhms
                            / (externalPitchSourceOhms
                               + externalPitchInputOhms);
        return { loaded,
                 -loaded * externalPitchFeedbackOhms
                     / externalPitchInputOhms };
    }

    static double p1014SelectedWaveVolts(Waveform waveform,
                                         double bipolarSample) noexcept;
    double runPitchControlLag(PitchControlLag& lag, double input) noexcept;
    static SvfOutputs runSection(SvfSection& section, double input, double g,
                                 double k, HighQBranch* highQ,
                                 double chargeStep) noexcept;
    SvfOutputs runLowerSection(
        const std::array<double, 3>& sourceTops,
        const std::array<double, 3>& sliderTravels,
        double dryInput, double g, double k) noexcept;
    double runUpperCascade(double input, double g, double controlledK,
                           double controlledInputGain,
                           UpperSlope slope) noexcept;
    void selectUpperSlope(UpperSlope slope) noexcept;
    double processOverdrive(double lowerLowpass) noexcept;
    double processFilterCoupling(double input) noexcept;
    OutputWipers processOutputNetwork(double filterInput,
                                      double shaperInput,
                                      double masterTravel,
                                      bool split) noexcept;
    OutputWipers processOutputNetwork(double filterInput,
                                      double shaperInput,
                                      double masterTravel,
                                      bool split,
                                      double brightnessResistanceOhms) noexcept;
    double processRingModulator(double triangleA, double triangleB) noexcept;

    // True when no program signal is sounding or can sound (no keys, both
    // envelopes' gate paths closed, no drone), so travel and wheel changes
    // may snap instead of gliding while the physical VCA floor continues.
    [[nodiscard]] bool silentForSnap() const noexcept;
    [[nodiscard]] static double consumeLfoKtDuration(
        double& ktSecondsRemaining, double intervalSeconds) noexcept;
    void advanceControls() noexcept;
    void advanceEnvelope(Adsr& envelope, bool gate, bool triggerPulse,
                         double attackCoefficient, double attackPeak,
                         double decayCoefficient,
                         double releaseResistanceOhms,
                         double sustain) noexcept;
    void renderVoiceSample(double externalAudio = 0.0) noexcept;
    [[nodiscard]] double reconstructExternalAudio(
        double hostSample, int internalStep) noexcept;
    [[nodiscard]] bool handleArpClock() noexcept;

    // parameters_ carries what the voice actually runs on this sample;
    // continuous travels glide toward targetParameters_ over ~25 ms so a
    // block-latched host or a 7-bit CC never steps the audio (the
    // Docs/decisions.md). Switches apply immediately, and a
    // fully silent engine snaps, so state restores land exactly.
    EngineParameters parameters_ {};
    EngineParameters targetParameters_ {};
    float targetModWheel_ { 0.0f };
    float targetShaperWheel_ { 0.0f };
    double travelSmoothing_ { 1.0 };
    double sampleRate_ { 44100.0 };
    double internalRate_ { 176400.0 };  // fixed 4x oversampling
    // h/(2*C*S) for the external limiter's trapezoidal charge companion.
    double highQChargeStep_ { 0.0 };
    double overdriveCouplingConductance_ { 0.0 };
    double cem3360OutputNoiseScale_ { 0.0 };
    double pitchLagPole_ { 0.0 };
    double pitchLagNow_ { 1.0 };
    double pitchLagPrevious_ { 0.0 };

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
    // Records raw KT for the modeled non-overlap reset branch, independently
    // of the envelope TRIGGER and GATE SELECT settings. Dynamic AA arbitration
    // awaits a measured pulse width/propagation record.
    bool pendingLfoReset_ { false };
    // RESET mode is always multiple-trigger regardless of the TRIGGER
    // switch, so the Shaper needs its own record of every key press —
    // pendingTrigger_ above carries only the envelope's selected MULTIPLE
    // KT pulse and cannot serve.
    bool pendingShaperTrigger_ { false };

    float pitchBend_ { 0.0f };
    float modWheel_ { 0.0f };
    float shaperWheel_ { 0.0f };

    // --- Control state (advanced once per output sample) --------------------
    double glidedNote_ { 60.0 };
    bool glideInitialised_ { false };
    double lastInternalPitchNote_ { 60.0 };

    // C13's retained triangle voltage and IC10B's direction state. They are
    // separate because keyboard reset clamps the TL068 output while C13
    // keeps charging behind it; a wrapped phase cannot represent that
    // hidden voltage or its brief above-threshold recovery.
    double lfoCapLevel_ { -1.0 }; // C13 / 5 V; ordinary reversals are +/-1
    bool lfoRising_ { true };     // P1015 LG comparator state
    bool lfoSquareHigh_ { false };
    bool previousLfoSquareHigh_ { false };
    double lfoKtSecondsRemaining_ { 0.0 };
    double lastLfoTriangle_ { -1.0 }; // clamped TL068 output, not hidden C13
    double sampleHoldValue_ { 0.0 };

    double shaperLevel_ { 0.0 };
    bool shaperRising_ { true };
    bool shaperCycleActive_ { false };
    bool shaperGate_ { false };
    bool previousGateForShaper_ { false };

    // P1017's rear switching contact. Keep presence independent of voltage:
    // a plugged-in low cable disconnects normalled SG just as surely as a
    // high one. These are external live states, so reset() must not invent an
    // unplug/replug transition.
    bool externalGateJackInserted_ { false };
    double externalGateVolts_ { 0.0 };
    // The live source-side value is distinct from cable presence: a plugged
    // 0 V source opens the keyboard-pitch normal just as a nonzero one does.
    bool externalPitchJackInserted_ { false };
    double externalPitchSourceVolts_ { 0.0 };
    // P1017 C1 retains the selected N-node voltage. It has no switched
    // discharge, so panic/reset and jack changes must not clear it.
    double externalPitchNodeVolts_ { 0.0 };
    bool externalPitchNodeInitialised_ { false };
    double keyboardPitchInputCoefficient_ { 1.0 };
    double externalPitchInputCoefficient_ { 1.0 };
    // C47/C48 retain the pedal nodes. The manual specifies 100k but not
    // taper, so resistance — rather than a guessed travel-to-resistance law
    // — is the physical API. Cables and charges survive reset/CC120.
    bool oscBPedalJackInserted_ { false };
    double oscBPedalResistanceKOhm_ { 100.0 };
    double oscBPedalNodeVolts_ { 0.0 };
    bool oscBPedalNodeInitialised_ { false };
    bool filterPedalJackInserted_ { false };
    double filterPedalResistanceKOhm_ { 100.0 };
    double filterPedalNodeVolts_ { 0.0 };
    bool filterPedalNodeInitialised_ { false };
    // Counterfactual C48 charge for the same TRACKING-mode history with the
    // jack open. Comparing like-for-like retained states keeps an unplugged
    // mode switch neutral without erasing a real cable-removal transient.
    double filterPedalOpenNodeVolts_ { 0.0 };
    bool filterPedalOpenNodeInitialised_ { false };
    // Separate physical state for the audio switching jack. A connected but
    // silent/disabled host bus must still open the IC4A normal contact.
    bool externalAudioJackInserted_ { false };

    // P1015 does not feed accepted edges straight to the two attacks.
    // X and Y/EXT edges pull their common GS/reset line low for the drawing's
    // nominal ~5 ms; MULTIPLE-key KT and arpeggiator AA use a separate 10 ms
    // lane. Both caps release during that notch and its final GS rise triggers
    // both 556 halves. X/Y are tracked before the OR because their edges stay
    // effective while another source already holds the selected bus high.
    bool previousEnvelopeXGate_ { false };
    bool previousEnvelopeYGate_ { false };
    bool previousEnvelopeGs_ { false };
    std::uint32_t envelopeResetSamplesRemaining_ { 0 };

    Adsr filterEnvelope_ {};
    Adsr loudnessEnvelope_ {};

    // Arpeggiator
    int arpStep_ { 0 };
    int arpSoundingNote_ { -1 };

    // OSC B and continuous RED NOISE are audio-rate MOD X signals. Their
    // routing is published here and read by the voice per internal sample,
    // so neither is undersampled or applied as a host-rate staircase. Depths
    // are per unit of source; gain is the wheel (and the SHAPE X WITH Y VCA).
    struct AudioRateMod
    {
        bool active { false };
        ModSource source { ModSource::OscB };
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
    double controlUpperInputGain_ { 2.5 };
    double controlLowerK_ { 1.5 };
    double controlLoudnessGain_ { 0.0 };
    double controlShaperVcaGain_ { 0.0 };
    double controlBrightnessResistanceOhms_ { 0.0 };
    double controlFilterMixA_ { 0.0 };
    double controlFilterMixB_ { 0.0 };
    double controlFilterMixNoise_ { 0.0 };
    double controlShaperMixA_ { 0.0 };
    double controlShaperMixB_ { 0.0 };
    double controlShaperMixRing_ { 0.0 };
    double controlShaperMixNoise_ { 0.0 };

    // --- Audio state (advanced at the internal rate) ------------------------
    double phaseA_ { 0.0 };
    double phaseB_ { 0.0 };
    // The bandlimited oscillator core emits with one internal sample of
    // delay: an event found mid-sample corrects the held sample exactly
    // instead of predicting the event a sample ahead. Selected wave and
    // ring triangle are corrected as separate channels per oscillator.
    double heldWaveA_ { 0.0 };
    Waveform heldWaveformA_ { Waveform::Sawtooth };
    double heldTriA_ { 0.0 };
    double heldWaveB_ { 0.0 };
    Waveform heldWaveformB_ { Waveform::Sawtooth };
    double heldTriB_ { 0.0 };
    // Raw comparator thresholds; values outside 0..1 retain the voltage
    // trajectory into and out of the constant-low/high PWM plateaus.
    double heldDutyA_ { 0.5 };
    double heldDutyB_ { 0.5 };
    PitchControlLag pitchLagA_ {};
    PitchControlLag pitchLagB_ {};
    // Previous post-P1014 selected B wave. The pitch capacitors make their
    // feedback causal; hard sync still needs this tap to break the remaining
    // B -> A-frequency -> A-reset -> B loop. Downstream routes read fresher.
    double lastOscBWave_ { 0.0 };
    SpiritNoise noise_ {};
    std::uint32_t loudnessVcaNoiseState_ { 0x6d2b79f5u };
    std::uint32_t shaperVcaNoiseState_ { 0xa511e9b3u };
    double brightnessG_ { 0.0 };
    double brightnessCompanion_ { 0.0 };
    double filterCouplingG_ { 0.0 };
    double filterCouplingCompanion_ { 0.0 };
    double ringCouplingG_ { 0.0 };
    double ringCouplingCompanion_ { 0.0 };
    // C34's trapezoidal voltage companion, left plate minus right plate.
    double overdriveCouplingCompanion_ { 0.0 };

    SvfSection lowerSection_ {};
    // Trapezoidal voltage companions for the three 68 pF wiper-to-VBP arms.
    std::array<double, 3> lowerMixerCompanions_ {};
    SvfSection upperControlled_ {};
    SvfSection upperFixed_ {};
    UpperSlope upperSlopeState_ { UpperSlope::TwentyFourDb };
    // Last physical VLP endpoints. C40=1 nF always equals the selected one;
    // SW4 transfers that voltage to the other 22 nF node by charge sharing.
    double upperControlledLp_ { 0.0 };
    double upperFixedLp_ { 0.0 };
    HighQBranch lowerHighQ_ {
        0.0,
        (1.0 + 33000.0 / 220.0) * 2200.0 / (22000.0 + 2200.0),
        22000.0 * 2200.0 / (22000.0 + 2200.0)
    };
    HighQBranch upperHighQ_ { 0.0, 1.0 + 33000.0 / 2200.0, 0.0 };

    // Two-stage decimation for the 4x -> 1x output boundary, one ring per
    // stage per audio path so the split-path output decimates cleanly. The
    // first stage's transition band is wide (nothing between 0.55 and 1.55
    // of the host Nyquist can fold into the audio band before the second
    // stage has its say), so it stays short; the second stage carries the
    // sharp cut — flat to 0.45 of the host rate, ~98 dB down from 0.55 —
    // that keeps near-Nyquist images out of the measured band.
    static constexpr int stageATaps = 31;
    static constexpr int stageBTaps = 127;
    static constexpr std::size_t preMixerDelaySamples =
        static_cast<std::size_t>(
            (stageATaps - 1) / 2 + (stageBTaps - 1) / 2 * 2);
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

    // Reverse B -> A halfband cascade for 1x -> 4x host-input
    // reconstruction. Zero insertion is explicit and each stage carries the
    // conventional x2 interpolation gain.
    std::array<double, stageBTaps> externalStageBRing_ {};
    int externalStageBIndex_ { 0 };
    std::array<double, stageATaps> externalStageARing_ {};
    int externalStageAIndex_ { 0 };

    struct PreMixerFrame
    {
        double oscillatorA { 0.0 };
        double oscillatorB { 0.0 };
        double ring { 0.0 };
        double pinkNoise { 0.0 };
        double audioModUpper { 0.0 };
        double audioModLower { 0.0 };
        double upperCutoffHz { 1000.0 };
        double lowerCutoffHz { 1000.0 };
        double upperK { 1.5 };
        double upperInputGain { 2.5 };
        double lowerK { 1.5 };
        double loudnessGain { 0.0 };
        double shaperVcaGain { 0.0 };
        double brightnessResistanceOhms { 0.0 };
        double filterMixA { 0.0 };
        double filterMixB { 0.0 };
        double filterMixNoise { 0.0 };
        double shaperMixA { 0.0 };
        double shaperMixB { 0.0 };
        double shaperMixRing { 0.0 };
        double shaperMixNoise { 0.0 };
        double loudnessVcaNoise { 0.0 };
        double shaperVcaNoise { 0.0 };
        float filterPathA { 0.0f };
        float filterPathB { 0.0f };
        float filterPathNoise { 0.0f };
        float masterVolume { 0.0f };
        LowerFilterMode lowerMode { LowerFilterMode::Out };
        UpperSlope slope { UpperSlope::TwentyFourDb };
        bool splitPaths { false };
        bool externalAudioJackInserted { false };
    };
    // Always active, even with the jack unplugged: this is the matching half
    // of the input reconstructor's fixed group delay, not a mode-dependent
    // effect.
    std::array<PreMixerFrame, preMixerDelaySamples> preMixerDelay_ {};
    int preMixerDelayIndex_ { 0 };

    std::array<double, stageATaps> filterStageARing_ {};
    std::array<double, stageATaps> shaperStageARing_ {};
    int stageAIndex_ { 0 };
    std::array<double, stageBTaps> filterStageBRing_ {};
    std::array<double, stageBTaps> shaperStageBRing_ {};
    int stageBIndex_ { 0 };
    double lastFilterPathSample_ { 0.0 };
    double lastShaperPathSample_ { 0.0 };

};

} // namespace ghostar
