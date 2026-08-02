#pragma once

#include "YouKnow106Chorus.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace youknow106
{

// Panel switch positions. Every enumerator is a physical detent on the
// modelled front panel, so the order is the panel order and the integer value
// is what the plug-in parameter stores.
enum class DcoRange { Sixteen, Eight, Four };
// The pulse-width source switch has two positions on this instrument, not the
// three some of its siblings carry.
enum class PwmSource { Lfo, Manual };
enum class HighPassMode { Boost, One, Two, Three };
enum class EnvPolarity { Normal, Inverted };
enum class VcaMode { Envelope, Gate };
enum class KeyMode { Poly1, Poly2, Unison };

// The assign mode the two latching POLY buttons select. They are independent
// switches, not a three-way selector, and holding both down is how the
// instrument is put into unison -- there is no third button for it.
//
// With neither down the key assigner still has to put a note somewhere, so it
// falls back to rotation. That combination is not reachable on the hardware,
// whose buttons are interlocked so one is always lit; it is reachable here
// because the panel exposes the two switches independently.
[[nodiscard]] constexpr KeyMode keyModeFor(bool poly1, bool poly2) noexcept
{
    if (poly1 && poly2)
        return KeyMode::Unison;
    if (poly2)
        return KeyMode::Poly2;
    return KeyMode::Poly1;
}

[[nodiscard]] constexpr bool poly1Engaged(KeyMode mode) noexcept
{
    return mode == KeyMode::Poly1 || mode == KeyMode::Unison;
}

[[nodiscard]] constexpr bool poly2Engaged(KeyMode mode) noexcept
{
    return mode == KeyMode::Poly2 || mode == KeyMode::Unison;
}

struct EngineParameters
{
    // Panel travel is 0..1 throughout: the engine maps each slider through the
    // modelled hardware law rather than storing pre-cooked seconds or hertz.

    // --- LFO ---------------------------------------------------------------
    float lfoRate { 0.42f };
    float lfoDelay { 0.0f };

    // --- DCO ---------------------------------------------------------------
    float dcoLfoDepth { 0.0f };
    float pwmDepth { 0.30f };
    PwmSource pwmSource { PwmSource::Manual };
    DcoRange range { DcoRange::Eight };
    bool sawEnabled { true };
    bool pulseEnabled { false };
    float subLevel { 0.0f };
    float noiseLevel { 0.0f };

    // --- HPF ---------------------------------------------------------------
    HighPassMode highPass { HighPassMode::One };

    // --- VCF ---------------------------------------------------------------
    float cutoff { 0.62f };
    float resonance { 0.10f };
    EnvPolarity envPolarity { EnvPolarity::Normal };
    float envDepth { 0.35f };
    float vcfLfoDepth { 0.0f };
    float keyFollow { 0.50f };

    // --- VCA ---------------------------------------------------------------
    VcaMode vcaMode { VcaMode::Envelope };
    float vcaLevel { 0.80f };

    // --- ENV ---------------------------------------------------------------
    float attack { 0.04f };
    float decay { 0.45f };
    float sustain { 0.70f };
    float release { 0.30f };

    // --- CHORUS ------------------------------------------------------------
    ChorusMode chorus { ChorusMode::Off };

    // --- Performance controls ---------------------------------------------
    KeyMode keyMode { KeyMode::Poly1 };
    float portamento { 0.0f };
    int keyTranspose { 0 };
    float benderDcoDepth { 0.30f };
    float benderVcfDepth { 0.0f };
    // Depth of the vibrato the lever's forward axis applies. The lever's
    // position arrives separately, as the modulation controller.
    float benderLfoDepth { 0.0f };
    float masterTuneCents { 0.0f };
    float volume { 0.80f };

    // --- Controls the modelled hardware does not have ----------------------
    // Each defaults to the value that reproduces hardware behaviour exactly,
    // so the default patch is a hardware-faithful patch.
    float velocityDepth { 0.0f };  // The hardware ignores MIDI velocity.
    float calibration { 0.35f };   // Per-voice analogue component tolerance.
    float chorusNoise { 1.0f };    // 1.0 is the modelled BBD noise floor.
    int polyphony { 6 };           // 6 is the hardware voice count.
};

class YouKnow106Engine
{
public:
    YouKnow106Engine() noexcept;

    void prepare(double sampleRate, int maxBlockSize,
                 bool oversamplingEnabled = true);
    bool setOversamplingEnabled(bool enabled) noexcept;
    void reset();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    // Releases every sounding key through the normal envelope path, as a
    // controller asking for all notes off means. `allNotesOff` is the hard
    // stop, for all-sound-off and for panic.
    void releaseAllNotes();
    void allNotesOff();
    // The bender lever: left/right bends pitch, and sweeps the filter when the
    // VCF bender slider is up. Its separate LFO axis arrives as the mod wheel.
    void setPitchBend(float normalisedBipolar) noexcept;
    void setModWheel(float amount) noexcept;
    void setSustainPedal(bool down) noexcept;
    void process(float* left, float* right, int numSamples);

    [[nodiscard]] int getActiveVoiceCount() const noexcept { return activeVoiceCount_; }
    [[nodiscard]] int getOversamplingFactor() const noexcept { return oversampling_; }
    [[nodiscard]] bool isOversamplingEnabled() const noexcept
    {
        return oversamplingRequested_;
    }
    [[nodiscard]] int getProcessingLatencySamples() const noexcept;
    // Panel display support. Plain relaxed reads of engine state; the plug-in
    // copies them into atomics for the editor.
    [[nodiscard]] float getDisplayEnvelope() const noexcept { return displayEnvelope_; }
    [[nodiscard]] float getDisplayLfo() const noexcept { return displayLfo_; }
    [[nodiscard]] int getDisplayVoiceMask() const noexcept { return displayVoiceMask_; }

    // ------------------------------------------------------------------
    // Modelled hardware laws.
    //
    // These are the circuit's own transfer relations, exposed as pure
    // functions so the regression suites can check them against the
    // service-note anchors and against an independent numeric solve without
    // reaching into a live voice. Docs/circuit-modelling-research.md records
    // where every constant below comes from.
    // ------------------------------------------------------------------

    // Counter clock the range divider feeds the note timer: the 8 MHz master
    // divided by 8, 4 or 2 for 16', 8' and 4'.
    [[nodiscard]] static double rangeClockHz(DcoRange range) noexcept;
    // The integer the note timer is programmed with. The timer is never told
    // which range is selected -- the range divider changes the clock arriving
    // at it, not the count -- so the count is always computed against the
    // middle range's clock and the switch transposes by whole octaves.
    [[nodiscard]] static std::uint32_t dcoDivider(double frequencyHz) noexcept;
    // The frequency that count actually produces at the selected range. Pitch
    // is quantised to this grid, which is why slow bends and vibrato staircase
    // on high notes.
    [[nodiscard]] static double dcoQuantisedFrequency(std::uint32_t divider,
                                                      DcoRange range) noexcept;

    // Cutoff control chain. Modulation is summed in the converter's own count
    // domain ahead of the antilog stage, exactly as the hardware sums it, so
    // every modulation source is exponential in hertz. The firmware clamps the
    // summed accumulator to 14 bits and hands the top 12 to the converter, so
    // the digital part of the control voltage moves in 4-count steps and can
    // never ask for less than the law's own base frequency.
    static constexpr float vcfBaseFrequencyHz = 5.53f;
    static constexpr float vcfCountsPerOctave = 1143.0f;
    static constexpr float vcfCountsCeiling = 16383.0f;
    static constexpr float vcfDacCountStep = 4.0f;
    [[nodiscard]] static float vcfCutoffHz(float counts) noexcept;
    [[nodiscard]] static float vcfPanelCounts(float panelPosition) noexcept;
    // Resonance feedback factor. k = 4 is the four-pole cascade's oscillation
    // threshold and is reached at 90% of the panel travel.
    [[nodiscard]] static float vcfFeedback(float panelPosition) noexcept;
    // Input gain the resonance-compensation path adds. The hardware boosts the
    // signal entering the filter as resonance rises, which is why a high-Q
    // patch drives the transconductor nonlinearity harder rather than thinning.
    [[nodiscard]] static float vcfResonanceCompensation(float feedback) noexcept;
    // Frequency trim applied as regeneration rises. A four-pole transconductor
    // cascade oscillates below its own small-signal corner, because the large
    // limit-cycle amplitude compresses the differential pairs and lowers their
    // effective transconductance. The instrument's service procedure has a
    // per-voice trimmer for exactly this, set so the oscillation lands on the
    // published pitch; this is that trimmer.
    [[nodiscard]] static float vcfResonanceFrequencyTrim(float feedback) noexcept;

    [[nodiscard]] static float envelopeAttackSeconds(float panelPosition) noexcept;
    [[nodiscard]] static float envelopeDecaySeconds(float panelPosition) noexcept;
    [[nodiscard]] static float envelopeReleaseSeconds(float panelPosition) noexcept;
    [[nodiscard]] static float lfoRateHz(float panelPosition) noexcept;
    [[nodiscard]] static float lfoDelaySeconds(float panelPosition) noexcept;
    [[nodiscard]] static float portamentoSeconds(float panelPosition) noexcept;

    // Inverses of the laws above. The panel travel is what the plug-in stores,
    // but what it *displays* is the value the circuit produces, so a host
    // letting someone type "1.00 kHz" needs a way back to the travel that
    // produces it. Without these the typed number would be read as travel.
    [[nodiscard]] static float panelPositionForAttack(float seconds) noexcept;
    [[nodiscard]] static float panelPositionForDecay(float seconds) noexcept;
    [[nodiscard]] static float panelPositionForLfoRate(float hertz) noexcept;
    [[nodiscard]] static float panelPositionForLfoDelay(float seconds) noexcept;
    [[nodiscard]] static float panelPositionForPortamento(float secondsPerOctave) noexcept;
    [[nodiscard]] static float panelPositionForCutoff(float hertz) noexcept;
    // Comparator threshold in volts against the modelled 12 Vpp ramp, and the
    // duty cycle it produces. The hardware cannot reach 0% or 100%.
    [[nodiscard]] static float pwmControlVolts(float depth) noexcept;
    [[nodiscard]] static float pwmDutyCycle(float controlVolts) noexcept;
    // The ramp's rising segment: 0..1 across the rise, -1..+1 out, at the
    // modelled curvature. The reset that follows it is a straight fall back to
    // the negative rail over the remainder of the cycle.
    [[nodiscard]] static float rampSegmentVoltage(float risePosition) noexcept;
    // Comparator edge positions within a cycle, as fractions of the period.
    [[nodiscard]] static float pulseRisePhase(float duty,
                                              float resetFraction) noexcept;
    [[nodiscard]] static float pulseFallPhase(float duty,
                                              float resetFraction) noexcept;
    // Output amplifier control law.
    [[nodiscard]] static float vcaGain(float control) noexcept;
    // Single-pole high-pass corner for a panel position, the gain the leg
    // returns the low band with, and the gain it returns the high band with.
    // The bass-boost position is a real shelf measured on the hardware:
    // +10.5 dB at DC falling to +1.4 dB in the high band across a corner
    // near 60 Hz.
    [[nodiscard]] static float highPassCornerHz(HighPassMode mode) noexcept;
    [[nodiscard]] static float highPassShelfGain(HighPassMode mode) noexcept;
    [[nodiscard]] static float highPassHighGain(HighPassMode mode) noexcept;

    // The instrument has six voice cards; the engine will run more of them for
    // players who want the chords the hardware drops. Public because the host
    // layer clamps its VOICES parameter to the same bound and the editor draws
    // one lamp per available voice.
    static constexpr int hardwareVoices = 6;
    static constexpr int maxVoices = 16;

private:
    // The JUCE-free suites use this narrow friend to drive one filter step, one
    // oscillator period and one envelope segment against independent
    // double-precision solves. It is not part of the plug-in API.
    friend struct YouKnow106TestAccess;

    static constexpr int maximumOversampleFactor = 4;
    static constexpr double minimumHqProcessingRate = 176400.0;
    static constexpr double maximumSupportedSampleRate = 768000.0;
    static constexpr int controlPeriod = 8;
    static constexpr int halfbandTaps = 63;
    static constexpr int halfbandRingSize = 128;
    static constexpr int latencyPadRingSize = 64;

    // --- Modelled hardware constants ---------------------------------------

    // One crystal feeds every voice's note timer, so the six voices are
    // inherently in tune with one another; what little pitch instability the
    // instrument has comes from the reference and the control chain, not from
    // six independent oscillator cores.
    static constexpr double masterClockHz = 8000000.0;
    static constexpr std::uint32_t minimumDivider = 8u;
    static constexpr std::uint32_t maximumDivider = 65535u;
    // The whole instrument is served by one converter multiplexed over 36
    // control points, and the scan repeats every 4.2 ms. Pitch, cutoff and
    // level therefore move on this grid rather than continuously.
    static constexpr double controlScanHz = 1000.0 / 4.2;
    // Rate at which the modelled component wander advances. Fixed in seconds,
    // not in samples, so it does not change with the quality setting.
    static constexpr double driftUpdateHz = 375.0;

    // Ramp generator. The control voltage charges the integrating capacitor
    // through a plain resistor rather than a current source, so the ramp is the
    // head of an RC exponential and bows slightly; the discharge transistor
    // gives the reset a finite slope.
    static constexpr float rampChargeSeconds = 200.0e-6f;
    static constexpr float rampResetSeconds = 2.2e-6f;
    static constexpr float rampAmplitudeVolts = 12.0f;

    // Four-pole transconductor cascade. Small-signal pole per stage is
    // wc = Ig / (2 Vt C) with C = 240 pF; the suites solve the same ODE.
    static constexpr float thermalVoltage = 0.026f;
    static constexpr float poleCapacitorFarads = 240.0e-12f;
    // Each stage attenuates its differential input by 560 / (68000 + 560)
    // before the transconductor's differential pair sees it. That attenuator,
    // not any voiced drive control, is what sets where this filter starts to
    // compress: it places the pair's linear span at +/-6.4 V, right at the peak
    // of a full-level ramp.
    static constexpr float stageAttenuation = 560.0f / (68000.0f + 560.0f);
    static constexpr float otaHeadroomVolts = 2.0f * thermalVoltage / stageAttenuation;
    // The resonance loop closes through its own transconductor, and the
    // fourth pole's output is divided 100k/1.5k before that pair sees it. The
    // divider is what limits the oscillation: referred back to the pole
    // output, the loop pair's linear span is 2 Vt times the divide ratio,
    // about 3.5 V, so the limit cycle compresses the *loop* long before the
    // forward stages hear it. That is why the hardware's self-oscillation is
    // near-sinusoidal and sits within a few cents of the small-signal law
    // rather than sagging flat.
    static constexpr float vcfFeedbackDividerRatio = 100000.0f / 1500.0f;
    static constexpr float vcfFeedbackHeadroomVolts =
        2.0f * thermalVoltage * vcfFeedbackDividerRatio;
    static constexpr float vcfSelfOscillationFeedback = 4.0f;
    static constexpr float vcfSelfOscillationTravel = 0.9f;
    // Loop gain the panel's far end reaches, a hardware-fitted figure: the
    // last tenth of the travel pushes past the threshold to about 4.19.
    static constexpr float vcfMaximumFeedback = 4.19f;
    // Residual oscillation-frequency trim. With the loop limited in the
    // divider, as in the hardware, the forward stages still compress at the
    // limit-cycle amplitude -- the first pole sees nearly three times the
    // fourth's swing, so the residue is a few per cent, not the fifth the
    // unlimited loop needed. This factor is the service procedure's per-voice
    // trimmer absorbing that residue, calibrated so the rendered oscillation
    // lands on the control law's pitch.
    static constexpr float vcfOscillationTrim = 0.045f;
    // The converter's top. The published specification runs to 50 kHz and a
    // measured example of the voice chip tops out near 52 kHz: the count law
    // holds to well past 20 kHz and the exponential converter then saturates,
    // so the model keeps the law exact through the audio band and bends it
    // smoothly onto the measured ceiling above the knee.
    static constexpr float vcfKneeStartHz = 24000.0f;
    static constexpr float vcfConverterCeilingHz = 52200.0f;

    // Modulation budgets, in converter counts, taken from the instrument's own
    // control tables. 1143 counts is one octave.
    static constexpr float vcfEnvelopeCounts = 16255.0f;
    static constexpr float vcfLfoCounts = 4047.0f;
    // The bender's filter axis at maximum: the firmware multiplies the
    // sensitivity byte by the bend byte and keeps the top bits, topping out at
    // 4064 counts -- just over three and a half octaves each way. An earlier
    // account claimed the whole cutoff range; the firmware arithmetic settles
    // it.
    static constexpr float vcfBenderCounts = 4064.0f;
    // Hold-capacitor slew after the converter. This is what turns the scan's
    // staircase back into a continuous control voltage. The capacitors are
    // alike but their charge paths are not: the amplifier's divider gives its
    // hold a slower time constant than the filter's.
    static constexpr float controlSlewSeconds = 522.0e-6f;
    static constexpr float vcaSlewSeconds = 687.0e-6f;
    static constexpr float vcfKeyFollowCentreMidi = 60.0f; // C4
    // Pitch modulation budgets in cents.
    static constexpr float lfoPitchCents = 400.0f;
    static constexpr float benderPitchCents = 1200.0f;
    // The generator is a 14-bit integer advanced once per scan, so envelope
    // levels live on this grid and a falling segment ends by truncation.
    static constexpr float envelopeQuantum = 1.0f / 16383.0f;

    enum class EnvelopeStage { Idle, Attack, Decay, Sustain, Release };

    // The generator is firmware: a 14-bit integer advanced once per scan.
    // The attack is a straight line -- a fixed increment added per pass. The
    // falling segments are *multiplicative*: the distance to the target is
    // scaled by a per-pass coefficient, so decay and release curve
    // exponentially in the control-voltage domain and a falling segment ends
    // when integer truncation reaches the target exactly. A key that comes
    // back mid-release attacks from the level it is at, because the firmware
    // never resets the accumulator, and raising the sustain slider mid-note
    // snaps the level up in one pass while lowering it decays down at the
    // decay rate -- both of which the straight-line model this replaced got
    // wrong.
    struct Envelope
    {
        EnvelopeStage stage { EnvelopeStage::Idle };
        float value { 0.0f };

        void reset() noexcept;
        void noteOn() noexcept;
        void noteOff() noexcept;
        float tick(float attackStep, float decayCoefficient, float sustain,
                   float releaseCoefficient) noexcept;
    };

    // Bandlimiting support. Every discontinuity the oscillator produces is
    // repaired by adding a residual read from a table built by numerically
    // integrating a windowed sinc, rather than from a closed-form polynomial
    // fit. The table is exact to the window's own stopband, and the same table
    // integrated once more repairs slope discontinuities.
    static constexpr int correctionHalfWidth = 4;
    static constexpr int correctionRing = 2 * correctionHalfWidth;
    static constexpr int correctionOversample = 64;
    static constexpr int correctionTableLength =
        correctionRing * correctionOversample + 1;

    // One waveform's correction accumulator. The naive signal is delayed by the
    // table's half width so a discontinuity can be repaired symmetrically,
    // which a causal-only correction cannot do.
    struct BandlimitedTrack
    {
        std::array<float, correctionRing> ring {};
        std::array<float, correctionHalfWidth> delay {};
        int base { 0 };
        bool primed { false };

        void reset() noexcept;
        void prime(float value) noexcept;
        [[nodiscard]] float advance(float naive) noexcept;
    };

    // Programmable divider, analogue ramp integrator, pulse comparator and
    // divide-by-two sub: one complete oscillator cell.
    struct Dco
    {
        std::uint32_t divider { 4545u };
        double periodSamples { 100.0 };
        double phase { 0.0 };
        float pulseState { -1.0f };
        // The divider's output level is the whole of its state. Holding a
        // separate toggle alongside it only creates a way for the two to
        // disagree, which is what made the first half-cycle after a retrigger
        // twice as long as every other one.
        float subState { 1.0f };
        BandlimitedTrack saw {};
        BandlimitedTrack pulse {};
        BandlimitedTrack sub {};

        void reset() noexcept;
    };

    // Four transconductor stages plus the inverting resonance return, solved
    // together as one implicit system.
    struct OtaCascade
    {
        std::array<float, 4> state {};
        std::array<float, 4> voltage {};

        void reset() noexcept;
        float process(float input, float g, float feedback) noexcept;
    };

    struct HighPass
    {
        float state { 0.0f };

        void reset() noexcept;
        float process(float input, float g, float shelfGain,
                      float highGain) noexcept;
    };

    struct HalfbandDecimator
    {
        std::array<float, halfbandRingSize> left {};
        std::array<float, halfbandRingSize> right {};
        int writeIndex { 0 };

        void reset() noexcept;
    };

    // Per-voice analogue tolerance, drawn once from a deterministic seed so a
    // given Calibration setting always produces the same instrument. Each
    // field is a real dispersion mechanism of the modelled voice: the two
    // cutoff axes are the two per-voice trimmers -- one offsets the control
    // voltage, one scales it -- left imperfectly set; the amplifier gets both
    // an offset and a gain error because its converter and hold are analogue;
    // and there is deliberately *no* envelope-rate error, because the
    // envelopes are computed digitally in the shared processor and are
    // therefore identical across voices. What differs between voices is the
    // analogue chain each envelope drives, not the envelope itself.
    struct VoiceCard
    {
        float rampCurrentError { 0.0f };
        float comparatorOffset { 0.0f };
        float cutoffOffsetError { 0.0f };
        float cutoffScaleError { 0.0f };
        float resonanceError { 0.0f };
        float vcaOffset { 0.0f };
        float vcaGainError { 0.0f };
        float subLevelError { 0.0f };
        float noiseLevelError { 0.0f };
        float highPassError { 0.0f };
        float driftPhase { 0.0f };
        float driftValue { 0.0f };
        std::uint32_t driftState { 1u };
    };

    struct Voice
    {
        bool active { false };
        bool keyDown { false };
        bool sustained { false };
        bool releasing { false };
        // Whether this slot belongs to the current Unison stack. A polyphonic
        // release tail left over from before the mode changed is still active
        // and still has a root note, so without this it would be swept into the
        // next unison retarget and resurrected on a key it never played.
        bool unisonMember { false };
        // Whether this slot has ever sounded a note this run. A voice that
        // has never played has no pitch history for the glide to start from.
        bool hasEverPlayed { false };
        int rootMidi { -1 };
        int cardIndex { 0 };
        int controlCountdown { 0 };
        // One converter serves every voice, but it reaches them in turn: the
        // multiplexer walks its hold capacitors sequentially, so each slot's
        // rewrite lands a fixed fraction of the pass after its neighbour's.
        int scanOffset { 0 };
        std::uint64_t generation { 0 };
        // When this slot last released its key, on the shared generation
        // counter. The assigner prefers the slot that has been free longest.
        std::uint64_t releaseStamp { 0 };
        float velocity { 0.0f };
        float currentMidi { 60.0f };
        float targetMidi { 60.0f };
        // The last note this slot sounded, surviving retirement: the assigner
        // matches a returning pitch against it, and the glide integrator
        // starts from it, both exactly as the hardware's per-voice state does.
        float lastMidi { 60.0f };
        float glideSemitonesPerScan { 0.0f };
        float filterG { 0.05f };
        // Converter counts and control voltages, before and after the sample
        // and hold's own slew. The staircase the scan writes is smoothed by a
        // real time constant on each hold capacitor, so modulation arrives
        // rounded rather than stepped. The oscillator's compensation voltage,
        // the pulse threshold, the sub and noise levels and the amplifier
        // control all take this path, because on the hardware each of them is
        // one of the multiplexed points -- none of them is a pot in the audio
        // path.
        float cutoffCountsTarget { 0.0f };
        float cutoffCounts { 0.0f };
        float vcaControlTarget { 0.0f };
        float vcaControl { 0.0f };
        // Oscillator compensation CV, kept in the frequency it stands for.
        // The timer's count steps instantly; this voltage slews, and the
        // ratio of the two is the momentary amplitude error a pitch step
        // leaves on the ramp and the pulse.
        float dcoCvTarget { 261.6f };
        float dcoCv { 261.6f };
        float pwmVoltsTarget { 6.0f };
        float pwmVolts { 6.0f };
        float subCvTarget { 0.0f };
        float subCv { 0.0f };
        float noiseCvTarget { 0.0f };
        float noiseCv { 0.0f };
        float attackStep { 1.0f };
        float decayCoefficient { 0.5f };
        float releaseCoefficient { 0.5f };
        float feedback { 0.0f };
        float inputCompensation { 1.0f };
        float vca { 0.0f };
        float pulseDuty { 0.5f };
        float highPassG { 0.01f };
        float highPassShelf { 1.0f };
        float highPassHigh { 1.0f };
        float energy { 0.0f };
        std::uint32_t noiseState { 1u };
        Envelope envelope {};
        Dco dco {};
        HighPass highPass {};
        OtaCascade filter {};
    };

    static EngineParameters sanitise(const EngineParameters& parameters) noexcept;
    static double midiToHz(double midiNote) noexcept;
    static std::uint32_t hash32(std::uint32_t value) noexcept;
    static float hashBipolar(std::uint32_t value) noexcept;
    // `height` is a value discontinuity (the comparator and divider edges);
    // `slopeStep` is a per-sample slope discontinuity, which is what the
    // integrator's finite-slope reset is at each of its two corners.
    // `samplesAgo` locates the event inside the sample just rendered.
    void addStep(BandlimitedTrack& track, float height,
                 float samplesAgo) const noexcept;
    void addSlope(BandlimitedTrack& track, float slopeStep,
                  float samplesAgo) const noexcept;
    // Normalised ramp voltage at a phase in 0..1, including the bow the
    // resistive charge path produces. Returns 0 at phase 0 and 1 at phase 1.
    static float rampVoltage(float phase, float bow) noexcept;
    // Fraction of the ramp's full excursion consumed by the finite-slope reset
    // at a given period, clamped so a very high note cannot invert the ramp.
    static float resetFraction(double periodSeconds) noexcept;

    void buildHalfbandKernel() noexcept;
    void buildCorrectionTables() noexcept;
    void buildVoiceCards() noexcept;
    void noteOnInternal(int midiNote, float velocity) noexcept;
    void noteOffInternal(int midiNote) noexcept;
    void initialiseVoice(Voice& voice, int slot, int midiNote, float velocity,
                         bool retrigger) noexcept;
    void silenceVoice(Voice& voice) noexcept;
    [[nodiscard]] bool anyVoiceSounding() const noexcept;
    void rearmLfoDelay() noexcept;
    // Empties everything downstream of the voices.
    void clearOutputPath() noexcept;
    // Glide rate for a panel position, in semitones per converter scan.
    [[nodiscard]] static float glideStepPerScan(float portamento) noexcept;
    // Lift the key on one slot: sustain it if the pedal is down, release it
    // otherwise. Every path that lets go of a note goes through here.
    void releaseVoiceKey(Voice& voice) noexcept;
    // Hand a slot out of the Unison stack, releasing it as if its key had been
    // let go. Used when the voice count shrinks below the stack that is
    // already sounding.
    void dropFromUnison(Voice& voice) noexcept;
    int findVoiceForNote(int midiNote) const noexcept;
    int allocateVoice(int midiNote) noexcept;
    void rememberHeldNote(int midiNote, float velocity) noexcept;
    // Newest key still physically held, or -1.
    [[nodiscard]] int newestHeldNote() const noexcept;
    void retargetUnison(int midiNote) noexcept;
    void forgetHeldNote(int midiNote) noexcept;
    void clearHeldNotes() noexcept;
    // Called when the converter scan reaches this voice: everything the
    // firmware recomputes once per pass. The modulator arrives twice: gated
    // by its own delay envelope for pitch and cutoff, and raw for the pulse
    // width, because the firmware applies the onset envelope to the first two
    // and not to the third.
    void updateVoiceScan(Voice& voice, const EngineParameters& parameters,
                         float lfoGated, float lfoRaw) noexcept;
    // Called on the audio control grid: turns the slewed control voltages into
    // filter and amplifier coefficients.
    void updateVoiceAudio(Voice& voice, const EngineParameters& parameters) noexcept;
    float renderVoice(Voice& voice, const EngineParameters& parameters,
                      float noiseSample) noexcept;
    void advanceLfo(const EngineParameters& parameters) noexcept;
    void updateVoiceCardDrift(VoiceCard& card) noexcept;
    void updateProcessingRate() noexcept;
    bool applyPendingOversamplingIfIdle() noexcept;
    // Padding that keeps the reported latency constant. Reporting a different
    // figure when the quality setting changes would make the host renegotiate
    // its compensation mid-transport, so the shallower configurations are
    // padded out to the deepest one's group delay instead.
    void applyLatencyPad(float& left, float& right) noexcept;
    // Group delay of the whole chain for an oversampling factor, in output
    // samples: the bandlimiting tracks' own delay, which runs at the internal
    // rate, plus each decimation stage's.
    [[nodiscard]] static double totalLatencySamples(int factor) noexcept;
    void downsamplePair(HalfbandDecimator& decimator,
                        float firstLeft, float firstRight,
                        float secondLeft, float secondRight,
                        float& outputLeft, float& outputRight) noexcept;
    void updateActiveVoiceCount() noexcept;
    [[nodiscard]] int voiceLimit() const noexcept;

    // What the panel is set to. Discrete settings -- key mode, voice count,
    // waveform switches, chorus mode -- take effect the moment they are
    // written, because the switches they model are switches.
    EngineParameters targetParameters_ {};
    EngineParameters activeParameters_ {};
    // Volume is the one continuous control wired straight into the audio
    // path -- a true potentiometer, not a patch parameter -- so it is glided
    // per sample. Every other continuous control on the panel is digitised
    // to seven bits and delivered through the converter scan, exactly as the
    // patch memory implies: a control that can be saved and recalled cannot
    // be a pot in the signal path.
    static constexpr float panelGlideSeconds = 0.005f;
    float glidedVolume_ { 0.8f };
    // A glide needs somewhere to start. The first render after a reset takes
    // the panel as it stands rather than sliding up to it, or a patch loaded
    // while stopped would fade in when the transport rolled.
    bool panelGlidePrimed_ { false };
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    double oversampledRate_ { 192000.0 };
    float inverseOversampledRate_ { 1.0f / 192000.0f };
    int oversampling_ { 4 };
    bool oversamplingEnabled_ { true };
    bool oversamplingRequested_ { true };
    // How long the voices have been silent, and how long the output path needs
    // to run dry once they are: the delay lines' longest setting, the decimation
    // stages' group delay and several time constants of the output coupling.
    static constexpr double outputPathQuietSeconds = 0.04;
    int oversamplingIdleSamples_ { 0 };
    int oversamplingQuietSamples_ { 1 };
    // The shared converter's pass, counted in internal samples.
    int scanCountdown_ { 0 };
    bool prepared_ { false };
    bool anyVoiceActive_ { false };
    int activeVoiceCount_ { 0 };
    std::uint64_t generation_ { 0 };

    // One shared free-running modulator, exactly as the hardware has. Because
    // it is shared, six voices vibrato together rather than smearing. It is
    // not a phase accumulator: the firmware adds a rate coefficient to a
    // clamped integer accumulator and flips direction on the clamp, and a
    // polarity bit halves that into a bipolar triangle. The clamp discards
    // whatever the last step overshot by, which quantises fast settings onto
    // a coarse grid of rates -- a real, measured behaviour of the instrument,
    // so it is modelled rather than smoothed away.
    float lfoAccumulator_ { 0.0f };
    bool lfoRising_ { true };
    float lfoPolarity_ { 1.0f };
    float lfoValue_ { 0.0f };
    float lfoDelayLevel_ { 0.0f };
    // The modulator, its delay envelope and the note generators all advance on
    // the converter scan, so the modulator's output is a staircase at that rate
    // rather than a continuous triangle.
    int lfoScanCountdown_ { 0 };
    float lfoDelayHoldoff_ { 0.0f };
    bool anyKeyDown_ { false };
    // The resonance control voltage: one converter output shared by every
    // voice's regeneration amplifier, quantised to the panel byte and slewed
    // on its own hold capacitor like the rest of the scanned points.
    float resonanceCvTarget_ { 0.0f };
    float resonanceCv_ { 0.0f };

    // One noise generator serves every voice, so noise sums coherently as more
    // keys are held instead of staying at a fixed level.
    std::uint32_t noiseState_ { 0x6d2b79f5u };

    // The lever is read by the converter, not wired to the voices: its value
    // is sampled once per scan pass, quantised to the converter's byte, and
    // whatever smoothing the player hears is the hold capacitors' own. A fast
    // flick therefore steps at the scan rate, as the hardware's does.
    float pitchBendTarget_ { 0.0f };
    float pitchBend_ { 0.0f };
    float modWheelTarget_ { 0.0f };
    float modWheel_ { 0.0f };
    bool sustainPedalDown_ { false };

    float displayEnvelope_ { 0.0f };
    float displayLfo_ { 0.0f };
    int displayVoiceMask_ { 0 };

    std::array<int, 128> heldNoteOrder_ {};
    std::array<float, 128> heldNoteVelocities_ {};
    std::array<std::uint16_t, 128> heldNoteCounts_ {};
    int heldNoteCount_ { 0 };

    std::array<Voice, maxVoices> voices_ {};
    std::array<VoiceCard, maxVoices> cards_ {};
    HalfbandDecimator firstDecimator_ {};
    HalfbandDecimator secondDecimator_ {};
    std::array<float, halfbandTaps> halfbandKernel_ {};
    std::array<float, correctionTableLength> stepResidual_ {};
    std::array<float, correctionTableLength> slopeResidual_ {};
    std::array<float, latencyPadRingSize> latencyPadLeft_ {};
    std::array<float, latencyPadRingSize> latencyPadRight_ {};
    int latencyPadWriteIndex_ { 0 };
    int latencyPadSamples_ { 0 };

    int driftControlCountdown_ { 0 };

    Chorus chorus_ {};

    float dcInput_ { 0.0f };
    float dcOutput_ { 0.0f };
    float dcCoefficient_ { 0.9987f };
};

} // namespace youknow106
