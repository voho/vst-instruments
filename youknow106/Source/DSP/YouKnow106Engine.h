#pragma once

#include "YouKnow106Chorus.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

#if (defined(__aarch64__) && defined(__ARM_NEON)) \
    || (defined(__x86_64__) && defined(__SSE2__))
#define YOUKNOW106_HAS_VCF_PAIR_SIMD 1
#endif

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
// Published ordinals are stored by session state and must not move. PolyZoned
// is appended rather than folded into ZonedHermite so a session saved on Fast
// keeps reproducing the table-Hermite kernel bit for bit; the polynomial rung
// replaces the fine-table lookup with an inner-zone odd polynomial
// (|x| < 1 covers 95..99.8% of the arguments real patches produce, max
// transfer error 5.08e-6 against libm) and batches the four stage
// nonlinearities per right-hand-side evaluation.
enum class VcfTanhMode { Exact = 0, ZonedHermite = 1, PolyZoned = 2 };
enum class VcfFastEarlyMode { Hermite = 0, Cubic = 1 };
// Which explicit Runge-Kutta tableau advances the four OTA capacitor states
// over one internal interval. Numerical kernel only: every rung integrates the
// same continuous circuit equations, with the same controls at the same
// abscissae, and differs only in truncation error and cost.
//
// `MersonHalfSteps` is the established reference and this struct's own default,
// so the JUCE-free tools and every frozen fingerprint and work-counter contract
// keep testing it. It is not what a plug-in instance starts on: after the blind
// A/B set returned no audible difference between rungs, the host layer ships
// `Rk4Single`. See PluginProcessor's `vcfSolverDefaultChoice`. The two cheaper
// rungs exist because the fixed two-half-step Merson pair spends
// ten right-hand-side evaluations per card per internal sample where the step
// sizes musical settings actually produce need far fewer; see
// Docs/decisions.md for the measured error and CPU of each.
//
// Every rung's abscissae are a subset of `OtaCascade::controlNodePositions`,
// so none of them moves a control node, changes the hold trajectory the
// converter writes produce, or needs a second reconstruction grid.
enum class VcfSolverMode
{
    // Two half-interval five-stage Merson steps: ten evaluations. Unchanged.
    MersonHalfSteps = 0,
    // Two half-interval classic four-stage RK4 steps: eight evaluations. Same
    // fourth order and the same half-interval step size as the default, so it
    // is the rung that lowers cost without changing the order of the method.
    Rk4HalfSteps = 1,
    // One full-interval classic RK4 step: four evaluations, escalating to the
    // half-step pair -- and to Merson beyond that -- on the intervals where
    // one step is not admissible. This is the ladder's cheapest rung. A
    // three-stage Kutta step was measured beside it and rejected: it saves one further evaluation and costs
    // 30-45 dB of solve accuracy, reaching -64.7 dB relative error at an
    // ordinary 1 kHz / resonant / 1x operating point where this rung holds
    // -97.5 dB, and it loses the self-oscillating limit cycle outright.
    Rk4Single = 2
};

// The assign mode latched by the two momentary POLY buttons. A single press
// selects that mode; pressing both selects Solo Unison. Firmware never leaves
// both lamps off. `false, false` is accepted only as a compatibility input and
// canonicalises to the power-on Poly 1 state.
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
    float attack { 0.0f };
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
    // Velocity zero and six voices are structurally hardware-aligned. Chorus
    // Chorus Noise defaults to the MN3009's inferred typical-S/N end; 1 keeps
    // the datasheet maximum available. The reported approximate II-I lift is
    // preserved; installed-unit PSD, stereo correlation and parasitic layers
    // remain OQ-03 rather than a fully calibrated level.
    float velocityDepth { 0.0f };  // The hardware ignores MIDI velocity.
    // Exposed to the host as Unit Character: one master over every modelled
    // component tolerance, trimmer residual, thermal wander and optional
    // circuit non-linearity -- the IR3109 stage offsets and integrating-
    // capacitor spread, the chorus clock law, the spatial thermal gradient,
    // the optional C14 voltage-dependence candidate and the VCF Early effect.
    // Every optional physical-circuit mechanism answers to this one control.
    //
    // Mechanisms the *nominal* circuit has are deliberately not on it: the
    // output summer's provisional loaded-swing bound and the passive mixer's
    // resistor loading apply at every setting, because a freshly calibrated
    // instrument has them too.
    //
    // Zero is the calibrated nominal model -- no spread, no drift, none of the
    // optional non-linear shapes leaning in -- and one selects the declared
    // full-character reference. Neither end is a claim that any real instrument
    // sits exactly there (no qualifying post-calibration residual data exists
    // to describe a real population, OQ-10); one is simply the declared
    // "matches real hardware" reference. The shipped default is 1.0.
    //
    // Bounded at 2. Every mechanism is written as
    // nominal + (physical - nominal) * calibration, which interpolates only on
    // [0, 1]; beyond that it extrapolates without limit, and several mechanisms
    // pass through zero and change sign. Values up to 2 still exaggerate while
    // every blend stays on the same side of its nominal value.
    float calibration { 1.0f };
    // Bound for the affine blends above; see the note on `calibration`.
    static constexpr float calibrationCeiling = 2.0f;
    float chorusNoise { Chorus::defaultNoiseScale };
    int polyphony { 6 };           // 6 is the hardware voice count.
    // Exact preserves the established always-running sound and state.
    // ZonedHermite and PolyZoned trade bounded kernel error for lower VCF CPU
    // use and also enable the documented inactive-card and Chorus-Off skips.
    VcfTanhMode vcfTanhMode { VcfTanhMode::Exact };
    // Numerical kernel only, and independent of the tanh choice above: which
    // Runge-Kutta tableau advances the capacitor states. MersonHalfSteps
    // preserves the established sound and state bit for bit.
    VcfSolverMode vcfSolverMode { VcfSolverMode::MersonHalfSteps };

    // --- Optional physical-circuit mechanisms --------------------------------
    // Each is a card dispersion or an inherent non-linearity that the
    // calibrated nominal model does not carry. Mechanisms that turned out to
    // be unreachable, mis-attributed or contradicted by an anchored claim have
    // been removed rather than left switchable; see the modelling notes.
    bool enableVcfStageOffsets { true };
    bool enableOpAmpSlewLimiting { true };
    bool enableVcfEarlyEffect { true };
    bool enableSpatialThermalGradient { true };
    // Only the heterodyne clock-bleed tone is implemented (see
    // Chorus::process); no Thiran fractional-delay filter exists. Off by
    // default -- its amplitude is an unvalidated placeholder pending OQ-03.
    bool enableChorusClockBleed { false };
    // Off by default: the only trajectory measurement in existence (KR-106's
    // ~50-point click-timing series, 16 us RMS residual against a straight
    // line) reads the 106's delay as linear in time, so the linear sweep
    // ships and the frequency-linear hypothesis waits behind this switch for
    // the calibrated capture OQ-01 still requests.
    bool enableChorusHyperbolicSweep { false };
    // The reported approximately 3.95 dB II-I output-floor delta ships as the
    // empirical default. This internal switch substitutes the rate-proportional
    // 4.21 dB causal hypothesis for controlled comparisons; it does not
    // multiply the two profiles. OQ-03 still owns absolute level and causality.
    bool useChorusRateNoiseHypothesis { false };
    // Comparison-only. The former default used an unmeasured 0.15 voltage
    // coefficient, driven from the bus rather than the voltage across C14.
    // Current aluminum-electrolytic manufacturer guidance says voltage bias
    // does not change capacitance; leave the candidate off until an installed
    // 10 uF non-polar part is measured under the OQ-21 conditions.
    bool enableElectrolyticC14Nonlinearity { false };
    // On by default: uses CircuitDerivedResonanceProfile's
    // linear-above-onset byte-to-loop-gain shape (drawn control chain plus
    // BA662-family linear gm, 2026-08-20) instead of the voiced quadratic-then-
    // linear panel curve. Same anchored endpoint, same compensation and
    // frequency correction; only the shape between the ends changes. False
    // retains the legacy voiced curve for comparisons. OQ-09's measured
    // family still owns the final calibration; the physical topology is the
    // stronger prior in its absence.
    bool useCircuitDerivedResonanceShape { true };
    // Engine-level aged-unit extension, exposed as the Aging host parameter
    // (2026-08-21, on request) and still defaulted off. Zero is
    // the freshly calibrated instrument every other mechanism describes; one
    // applies the single documented recalibration lead (2026-08-20 pass): a
    // unit re-trimmed after about four years whose undisturbed VCF trims had
    // drifted flat by up to about a quarter tone with one card near dead-on,
    // and whose noise trim had drifted 6 Vp-p against the 4 Vp-p spec
    // (+3.5 dB). Voiced, single-unit lineage, qualitative pattern only;
    // OQ-10's population data owns any promotion.
    float aging { 0.0f };
    // Ignored by Exact. The opt-in cubic replaces only the small
    // Character/Early multiplier transfer in the Fast kernel.
    VcfFastEarlyMode vcfFastEarlyMode { VcfFastEarlyMode::Hermite };

    // Hosts commonly present the same complete parameter snapshot on every
    // block. Value equality is the right test for that public control image:
    // equal switch/control positions have no new physical write for
    // setParameters() to perform, while a NaN cannot compare equal and therefore
    // still reaches sanitise().
    [[nodiscard]] bool operator==(const EngineParameters&) const noexcept = default;
};

class YouKnow106Engine
{
public:
    YouKnow106Engine() noexcept;

    // The quality ladder. The requested factor is what the player asks for; the
    // applied factor is that request capped so the internal rate never has to
    // run further above the bandlimiting target than it needs to (see
    // updateProcessingRate), which is why a 192 kHz host already runs at 1x
    // with the highest setting selected.
    static constexpr int minimumOversampleFactor = 1;
    static constexpr int maximumOversampleFactor = 4;
    // The host rates prepare() will run at. Anything outside this is clamped
    // into it rather than reaching the internal grid; see prepare().
    static constexpr double minimumSupportedSampleRate = 8000.0;
    static constexpr double maximumSupportedSampleRate = 768000.0;
    // The three rungs the panel offers, in increasing cost.
    static constexpr std::array<int, 3> oversampleFactors {
        minimumOversampleFactor, 2, maximumOversampleFactor };
    // Nearest supported rung at or below `factor`; hostile input falls to 1x,
    // which is the cheapest and can never overrun the internal grid.
    [[nodiscard]] static constexpr int sanitiseOversampleFactor(int factor) noexcept
    {
        if (factor >= 4)
            return 4;
        return factor >= 2 ? 2 : 1;
    }

    void prepare(double sampleRate, int maxBlockSize,
                 bool oversamplingEnabled = true);
    void prepare(double sampleRate, int maxBlockSize, int requestedFactor);
    bool setOversamplingEnabled(bool enabled) noexcept;
    // Requests a rung of the quality ladder. Like the boolean form, the change
    // is deferred until the instrument is idle, and the return value says
    // whether it has been applied yet.
    bool setOversamplingFactor(int factor) noexcept;
    void reset();
    // A host's transport stop is not a power cycle. `reset()` above is the
    // cold one -- `prepare()` and a device change use it -- and returns every
    // state to the moment the instrument was switched on. This one clears the
    // same sounding voices, tails and transient controllers but leaves the
    // modelled circuit state that outlives a run where it was, as the voice
    // cards' own component trims already do: a chassis that has been powered
    // for ten minutes is still warm when the transport stops.
    void resetForHostStop();
    void setParameters(const EngineParameters& parameters);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    // Re-pressing the selected hardware POLY button leaves the visible mode
    // unchanged but still gates, clears and rescans all held assignments.
    void reassertKeyMode() noexcept;
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
    // The rate the engine actually runs its output grid at, which is the host's
    // once it has passed the guards in prepare(). A host that reports nothing
    // usable is not the rate the panel should be displaying.
    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] int getOversamplingFactor() const noexcept { return oversampling_; }
    [[nodiscard]] int getRequestedOversamplingFactor() const noexcept
    {
        return oversamplingRequested_;
    }
    [[nodiscard]] bool isOversamplingEnabled() const noexcept
    {
        return oversamplingRequested_ > 1;
    }
    [[nodiscard]] int getProcessingLatencySamples() const noexcept;
    // Panel display support. Plain relaxed reads of engine state; the plug-in
    // copies them into atomics for the editor.
    [[nodiscard]] float getDisplayEnvelope() const noexcept { return displayEnvelope_; }
    [[nodiscard]] float getDisplayLfo() const noexcept { return displayLfo_; }
    [[nodiscard]] int getDisplayVoiceMask() const noexcept { return displayVoiceMask_; }
    [[nodiscard]] float getDisplayTemperatureC() const noexcept
    {
        // Like the droop below: Unit Character scales the warmup the cards
        // actually see, so the thermometer reports the same rise the audio
        // model applies -- ambient, at Character zero.
        return 25.0f
             + 15.0f * activeParameters_.calibration
                     * (1.0f - std::exp(-static_cast<float>(
                                            thermalWarmupSeconds_) / 900.0f));
    }
    [[nodiscard]] float getDisplayRailDroopVolts() const noexcept
    {
        // The stored droop is a pure load measure; Unit Character scales the
        // sag the cards actually see. Report what they see.
        return powerSupplyDroop_ * activeParameters_.calibration;
    }

    // ------------------------------------------------------------------
    // Modelled hardware laws.
    //
    // These are the circuit's own transfer relations, exposed as pure
    // functions so the regression suites can check them against the
    // service-note anchors and against an independent numeric solve without
    // reaching into a live voice. The README's "How it works" records
    // where every constant below comes from, and its "Known gaps" the
    // evidence that would close each one still voiced.
    // ------------------------------------------------------------------

    // Counter clock the range divider feeds the note timer: the 8 MHz master
    // divided by 8, 4 or 2 for 16', 8' and 4'.
    [[nodiscard]] static double rangeClockHz(DcoRange range) noexcept;
    // One B-2 pitch conversion produces both values which the voice CPU later
    // writes as a single logical transaction: the M82C53 count and the
    // unshifted 12-bit ramp-current DAC code. `pitchWord` is the firmware's
    // unsigned 8.8 semitone coordinate, including its 0x1818 master offset.
    struct DcoPitchPair
    {
        std::uint32_t divider;
        std::uint16_t cvCode;
    };
    [[nodiscard]] static DcoPitchPair dcoPitchPair(
        std::uint16_t pitchWord) noexcept;
    // Host MASTER TUNE is continuous +/-50 cents; B-2 consumes one signed
    // byte in units of 1/256 semitone, so the positive hardware endpoint is
    // +127 units (+49.609375 cents) while the negative endpoint reaches -128.
    [[nodiscard]] static std::int16_t masterTunePitchWordOffset(
        double cents) noexcept;
    // Pitch Wheel remains a normalised host control, but B-2 receives the
    // assigner's reduced signed byte and combines it with the eight-bit DCO
    // sensitivity using truncating integer shifts. The maximum is therefore
    // 3063 pitch units (11.96484375 semitones), not an ideal twelve.
    [[nodiscard]] static std::int32_t dcoPitchBendWordOffset(
        float normalisedBipolar, float depth) noexcept;
    // The stored DCO-LFO slider selects a byte from B-2's nonlinear depth
    // table. A compact generator preserves that table's exact observable law
    // without distributing a ROM dump.
    [[nodiscard]] static std::uint8_t dcoLfoDepthScale(
        std::uint8_t storedDepth) noexcept;
    // B-2 multiplies the panel scaler by the shared delay byte, adds the CC1
    // path, saturates that combined depth, then multiplies the 14-bit triangle
    // accumulator into the same signed 8.8 master-pitch word.
    [[nodiscard]] static std::int32_t dcoLfoPitchWordOffset(
        std::uint16_t accumulator, bool positivePolarity,
        std::uint8_t delayByte, std::uint8_t storedDepth,
        std::uint8_t modWheel, std::uint8_t benderSensitivity) noexcept;

    // Convenience adapter for a requested middle-range frequency. Production
    // constructs the 8.8 coordinate directly; this keeps the circuit-law seam
    // useful without making a hertz value part of the firmware model.
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

    // The firmware-verified resonance path ends at the shared 12-bit DAC
    // code. Everything from that code to loop gain, input compensation and
    // oscillation-frequency correction is kept together here as one named,
    // replaceable sound-design profile. Most of its constants remain voiced:
    // they preserve YouKnow106's established sound rather than asserting a
    // measured code-to-loop transfer.
    //
    // The *endpoint* is no longer voiced. Roland's ADJUSTMENT section trims
    // every card, at BANK 3 with C4 held, to a 4.8 Vp-p self-oscillating sine
    // at 248 Hz -- two steps taken on one card in one state. The suite used to
    // check the frequency and never the amplitude, and the model sat 4.1 dB
    // under it at 2.99 Vp-p.
    //
    // The two anchors are coupled, so neither can be satisfied alone: the
    // limit cycle grows with loop gain, and the stage tanh's compression at
    // that larger amplitude pulls the oscillation flat. That coupling used to
    // be absorbed by a fitted quadratic in loop gain, which lifted cutoff at
    // every resonance setting -- +32 cents at panel 0.50 and +116 at 0.80 --
    // where the cascade carries no limit cycle and there is nothing to
    // correct. The correction is now the reciprocal of the cascade's own
    // describing-function gain at the limit cycle its loop gain sustains
    // (`frequencyTrim` below), so it is identically 1 wherever there is no
    // oscillation and only `maximumFeedback` remains to be solved. What stays
    // voiced is the quadratic-then-linear panel curve between the ends.
    //
    // Both cutoff anchors read 248 Hz at converter code 6272 -- the service
    // ADJUSTMENT's self-oscillation trim and, at resonance 0, the measured
    // code-to-frequency table OQ-18 carries -- so the model owes an
    // oscillation that lands on its own small-signal law, which is exactly
    // what cancelling the droop delivers.
    struct VoicedResonanceCompatibilityProfile
    {
        // Reported at 67.7 by an independent reverse-engineering of the same
        // module, which is why this one is not a free parameter of the solve.
        // The dksynth-lineage reconstruction netlist (Open80017a) carries
        // exactly this network -- VCF OUT through 100k against a 1.5k shunt
        // into the resonance OTA -- whose loaded read is (100k+1.5k)/1.5k,
        // the reported 67.7; this ratio keeps the unloaded 100/1.5 form as
        // the established coordinate of the solve.
        static constexpr float loopDividerRatio = 100000.0f / 1500.0f;
        static constexpr float loopHeadroomVolts =
            2.0f * 0.026f * loopDividerRatio;
        // Four identical one-poles carry 45 degrees each at their own corner,
        // where each contributes 1/sqrt(2), so the loop closes at a gain of 4
        // and at the corner itself. That is the cascade's oscillation
        // threshold, and it is where `frequencyTrim` stops being 1.
        static constexpr float nominalOscillationFeedback = 4.0f;
        static constexpr float nominalOscillationTravel = 0.9f;
        // Solved against the 4.8 Vp-p service trim; was a voiced 4.19, then a
        // jointly solved 4.51. It is now the only free constant in the
        // endpoint solve, because the frequency correction below is derived:
        // the amplitude anchor alone fixes it and the 248 Hz anchor is a
        // prediction rather than a second fit. Re-solved against the
        // amplitude alone once the joint trade was gone, which is what moved
        // the rendered limit cycle from 4.83 Vp-p on to 4.80.
        static constexpr float maximumFeedback = 4.504f;
        // Voiced, but bracketed by the same reconstruction: its resonance
        // OTA takes VCF IN through 24k/1.5k (1/17.0) on the non-inverting
        // input, VCF OUT through 100k/1.5k on the inverting one, and injects
        // its output current at the first stage's 4.7k/560/68k summing node.
        // With stage 1's own -68k/4.7k feedback gain the OTA's gm cancels
        // and the slope in this loop-gain coordinate is resistor-only,
        // (67.7/17.0)*(4.7/68) = 0.275 -- about 20% above the value below
        // (which sits 17% under it), same linear-in-k form. One
        // reconstruction lineage, so not promoted; OQ-09's measured family
        // still owns this number.
        static constexpr float inputCompensationPerFeedback = 0.2296f;

        [[nodiscard]] static float loopGain(float panelPosition) noexcept;
        [[nodiscard]] static float inputCompensation(float feedback) noexcept;
        // The reciprocal of the pole scaling the cascade's own limit cycle
        // imposes on itself, as a function of the loop gain that sustains it.
        // Exactly 1 at and below `nominalOscillationFeedback`, where there is
        // no limit cycle. See the derivation over `frequencyTrim`'s body.
        [[nodiscard]] static float frequencyTrim(float feedback) noexcept;
    };

    // OQ-09 shipping shape, selectable through
    // `useCircuitDerivedResonanceShape`. The
    // 2026-08-20 junction-level read of the module board's control chain --
    // shared 0..+10 V RESO CV hold, per-card series trimmer plus 27 kOhm into
    // a grounded-base 2SA1015-class stage, collector straight into the
    // resonance BA662's control pin with no converter drawn anywhere on the
    // path -- makes the control current linear in the held voltage above one
    // emitter-junction drop. With the BA662 architecture's gm linear in its
    // control current (Rohm BA6110 and Alfa AS662 bracket data), loop gain is
    // linear in the stored byte above that onset. The per-card trimmer sets
    // only the slope, which is exactly what the 4.8 Vp-p service adjustment
    // calibrates away, so the anchored endpoint is the voiced profile's own
    // `maximumFeedback` and the onset is the one new constant. Between those
    // ends nothing here is measured: this is a derivable shape beside the
    // retained voiced compatibility curve, and OQ-09's measured
    // response-versus-resonance family can still supersede it.
    struct CircuitDerivedResonanceProfile
    {
        // Byte 127 -> aligned word 0x3F80 -> physical code 4064 on the
        // 0..+10 V branch (Service Notes p. 8): 10 * 4064 / 4096.
        static constexpr float controlFullScaleVolts = 9.921875f;
        // Nominal silicon emitter-junction drop of the grounded-base stage.
        // One reconstruction lineage reads ~150 mV for a *calibrated* card
        // (atosynth); that trimmed figure is recorded under OQ-09 and not
        // adopted -- the nominal drop is the defensible uncalibrated prior.
        static constexpr float onsetVolts = 0.6f;
        static constexpr float onsetTravel =
            onsetVolts / controlFullScaleVolts;

        [[nodiscard]] static float loopGain(float panelPosition) noexcept;
        // Compensation and frequency correction operate in the loop-gain
        // coordinate and belong to the mechanism, not the shape, so this
        // profile shares the voiced profile's functions for both.
    };

    // The two-term generalized algebraic soft clip used by VCF saturation is
    // `algebraicSoftClipDenominator` in YouKnow106Chorus.h. The output summer
    // fits the same curve with a fixed exponent of eight; its hot path spells
    // that case as multiplies and square roots instead of general pow. The
    // BBD's constrained quadratic variant and table live in Chorus.cpp.

    // Where the transconductor's own control current stops following the
    // anti-log converter. An AS3109 teardown reports the internal control
    // current saturating at 700 uA -- the physical origin of the upper knee,
    // and consistent with Roland's published 50 kHz top.
    //
    // That current does not by itself produce the figure below, and this
    // comment used to say it did. On the C = 240 pF the cascade above solves
    // with, and the H = 6.37 V span it solves in, Ig / (2 pi C H) is
    // 72.9 kHz. The same 700 uA the cascade comment reads as 8.9 MHz ahead of
    // the 560/68560 divider lands at 8.9 MHz * 0.0081680 = 72.9 kHz behind it;
    // the two figures carried here stood in a ratio of 139 where that divider
    // is 122.43. 64 kHz is instead what the same equation returns on 270 pF --
    // the Open80017a reconstruction's integrator value, not the service
    // circuit's -- or what 614 uA returns on 240 pF. Which of those the number
    // came from is not recorded.
    //
    // The constant is deliberately NOT changed here. The exponent below was
    // fitted to a measured code-to-frequency curve with this ceiling already
    // standing, so the pair moves together or not at all, and the 248 Hz
    // self-oscillation anchor pins absolute cutoff either way. What changes is
    // its classification: voiced, bracketed by 64.8 kHz (270 pF) and 72.9 kHz
    // (240 pF), no longer presented as derived from 700 uA on 240 pF.
    // Refitting the pair belongs to OQ-18, beside the 240-vs-270 pF
    // integrator question it shares a cause with.
    //
    // The shape is the generalized algebraic clip above, shared with the
    // output summer and the BBD write: numerically linear through the whole
    // musical range and bending only as the current approaches its limit.
    // The exponent is the one free parameter, fitted to a measured
    // code-to-frequency curve for a real voice card; a single pole (the
    // exponent at one) cannot describe that knee, and the revision that used
    // one left the model up to 143 cents flat around a 16 kHz cutoff.
    //
    // Like other shared component limits this is a property of the part, so it
    // applies at every Unit Character setting. Gating it left the "calibrated
    // reference" with a filter that kept tracking the exponential law past the
    // point the transconductor can follow it, 292 cents sharp near the top.
    static constexpr float vcfControlSaturationHz = 64000.0f;
    static constexpr float vcfControlSaturationExponent = 1.7f;

    // Complete default-profile cutoff after the compatibility profile's
    // frequency correction and the transconductor's control-current
    // saturation. The explicit product safety cap applies after every
    // correction so no composition can exceed the declared boundary.
    [[nodiscard]] static float vcfEffectiveCutoffHz(float counts,
                                                    float feedback) noexcept;

    // Integral non-linearity of the R-2R cutoff converter, in counts, for a
    // summed count value. A measured code-to-frequency table for a real voice
    // card shows excess steps of -4.64, +23.31 and -4.48 cents at the three
    // top bit boundaries (DAC codes 1024, 2048 and 3072), which is where an
    // R-2R ladder's major-carry error physically belongs. This is a persistent
    // offset on the converter's own output, not an impulse: a revision wrote
    // it into the field the same converter write reassigns, so it measured
    // bit-identical and was removed.
    //
    // Scaled by Unit Character, because an ideal ladder has no carry error at
    // all: the magnitude is resistor matching, which is a tolerance.
    [[nodiscard]] static float vcfConverterCarryCounts(float counts) noexcept;

    [[nodiscard]] static float envelopeAttackSeconds(float panelPosition) noexcept;
    [[nodiscard]] static float envelopeDecaySeconds(float panelPosition) noexcept;
    [[nodiscard]] static float envelopeReleaseSeconds(float panelPosition) noexcept;
    [[nodiscard]] static float lfoRateHz(float panelPosition) noexcept;
    [[nodiscard]] static float lfoDelaySeconds(float panelPosition) noexcept;
    [[nodiscard]] static float portamentoSeconds(float panelPosition) noexcept;

    // The PORTAMENTO control is a physical divider before it is a byte:
    // a 50KB linear track across +5 V whose wiper reaches the slave CPU's
    // AN1 through the on/off switch, against R16 47 kOhm to ground (Service
    // Notes p. 16 bender-board read, 2026-08-20). Wiper travel x therefore
    // lands at x*R_L / (R_L + x*(1-x)*R_T) of the ADC's full scale -- 0.395
    // at half travel -- and the knob's taper is the loaded pot's, not a
    // linear byte ramp. These map between knob travel and that ADC
    // fraction; the raw-code-addressed laws above stay byte-exact and
    // unchanged. Switch-open Off is the pulled-down raw 0 the recurrence
    // already treats as immediate, so travel 0 still means Off.
    [[nodiscard]] static float portamentoTravelAdcFraction(
        float travel) noexcept;
    [[nodiscard]] static float portamentoTravelForAdcFraction(
        float fraction) noexcept;

    // Hash-matched B-2 coefficient laws. These functions reproduce the
    // observable 0..127 behaviour without embedding the ROM or a coefficient
    // table dump in the project.
    [[nodiscard]] static std::uint16_t envelopeAttackIncrement(
        float panelPosition) noexcept;
    [[nodiscard]] static std::uint16_t envelopeDecayReleaseMultiplier(
        float panelPosition) noexcept;
    [[nodiscard]] static std::uint16_t lfoRateIncrement(
        float panelPosition) noexcept;
    [[nodiscard]] static std::uint16_t lfoDelayFadeIncrement(
        float panelPosition) noexcept;
    [[nodiscard]] static std::uint8_t portamentoIncrement(
        float panelPosition) noexcept;

    enum class ConverterDestination : std::uint8_t
    {
        Resonance, CommonVca, Sub, Pitch, Pwm, Vcf, VoiceVca, Noise
    };
    struct ConverterWrite
    {
        ConverterDestination destination;
        int voice; // -1 for a shared destination, otherwise 0..5.
    };
    static constexpr std::size_t converterWritesPerPass = 23;
    [[nodiscard]] static const std::array<ConverterWrite,
                                          converterWritesPerPass>&
        converterWriteOrder() noexcept;
    enum class ConverterTimingProfile : std::uint8_t
    {
        NormalizedServiceChart,
        PhaseZeroDiagnostic,
        MeasuredChartGeometry
    };
    // NormalizedServiceChart is an explicit compatibility/product profile: it
    // preserves the chart's sequential writes across one pass without claiming
    // exact physical timestamps. PhaseZeroDiagnostic is the minimal-evidence
    // comparison in which only ordinal order remains. MeasuredChartGeometry
    // carries the 2026-08-20 pixel measurement of the p. 8 D/A & S/H timing
    // chart itself (three slot-width classes on a 10:7:5 drafting grid,
    // rebased from the chart's NOISE-first origin to this queue's
    // RESONANCE-first ordinal 0): drawn-artwork proportions, deliberately
    // non-uniform, still not hardware timestamps -- the figure is drafting,
    // not a capture. It ships as a selectable comparison profile only; the
    // reset path keeps NormalizedServiceChart.
    [[nodiscard]] static std::array<double, converterWritesPerPass>
        converterEventPhases(ConverterTimingProfile profile) noexcept;
    // Selects the profile reset()/prepare() install, so a comparison profile
    // can drive the complete shipping signal path (the A-Z rules forbid
    // offline approximations). Mid-pass switching is deliberately
    // unsupported: the phases are pass-relative coordinates and moving them
    // under a running pass would invent an event discontinuity no hardware
    // has, so a selection takes effect at the next reset()/prepare().
    void selectConverterTimingProfile(ConverterTimingProfile profile) noexcept;

    // Output calibration is a product convention, not a JUNO-106 voltage.
    // One internal unit is still the established 2.6 V model coordinate used
    // to drive the chorus. Choosing this provisional reference makes the new
    // -18 dBFS RMS boundary exactly unity and therefore preserves sessions.
    static constexpr float internalVoltsPerUnit = 2.6f;
    // The chorus refers its BBD noise row to the same coordinate and has to
    // name it locally, so the two cannot be allowed to drift apart.
    static_assert(std::bit_cast<std::uint32_t> (Chorus::nodeVoltsPerUnit)
                      == std::bit_cast<std::uint32_t> (internalVoltsPerUnit),
                  "the chorus and the engine disagree about the node volt scale");
    static constexpr float minus18DbfsAmplitude = 0.125892541f;
    static constexpr float compatibilityOutputReferenceRmsVolts =
        internalVoltsPerUnit * minus18DbfsAmplitude;
    [[nodiscard]] static float outputReferenceGain(float referenceRmsVolts) noexcept;

    // IC6's loaded output swing stops inside its +/-15 V supply rails. With no
    // installed-unit capture, 13.5 V is a provisional model asymptote: about
    // 0.4 V below the datasheet's 25 C typical curve at the traced 8.22 kOhm
    // midband load. It is not a guaranteed part limit or a supply-rail value.
    // As a shared output-stage policy rather than a per-unit tolerance, it
    // applies at every Unit Character setting including zero.
    //
    // The shape is the generalized algebraic clip above, already used for the
    // VCF saturation and the BBD write, rather than a tanh. A tanh has no
    // linear region at all: its distortion rises as (V/asymptote)^2 from the
    // first millivolt, which put roughly 0.3% third harmonic on every sample
    // at an ordinary 2.6 V node swing. A few volts is well inside the TA75558S
    // datasheet's typical loaded-swing envelope, but it supplies no THD row.
    // The provisional high exponent keeps the model numerically linear there;
    // OQ-05 still owns the installed low-level distortion, knee and swing.
    static constexpr float outputSummerSwingAsymptoteVolts = 13.5f;
    static constexpr float outputSummerClipExponent = 8.0f;
    [[nodiscard]] static float outputSummerClip(float value) noexcept;

    // Digital full scale follows the provisional model asymptote through the
    // volume wiper at its loudest setting. Mapping 0 dBFS lower would add a
    // second digital ceiling below the current analogue policy; this is not a
    // claim that every installed IC6 reaches 13.5 V (OQ-05).
    //
    // The bound is the steady-state one. The output coupling is a high-pass, so
    // a large enough transient can overshoot it; the measured worst case over
    // every source at once, six voices and both controls at maximum is -1.45
    // dBFS, so the margin is real but is not a mathematical guarantee.
    [[nodiscard]] static float outputBoundaryGain() noexcept;

    // In the supplied hash-matched B-2 image, stored continuous controls are
    // seven-bit bytes: b<<7 in the 14-bit working domain, followed by b<<5 at
    // the physical 12-bit converter after two low bits are discarded.
    static constexpr std::uint16_t envelopePeak = 0x3fffu;
    [[nodiscard]] static std::uint16_t storedControlAlignedWord(
        float panelPosition) noexcept;
    [[nodiscard]] static std::uint16_t storedControlDacCode(
        float panelPosition) noexcept;
    [[nodiscard]] static std::uint16_t envelopeAttackLevel(
        std::uint16_t level, std::uint16_t increment) noexcept;
    [[nodiscard]] static std::uint16_t envelopeDecayLevel(
        std::uint16_t level, std::uint16_t sustain,
        std::uint16_t multiplier) noexcept;
    [[nodiscard]] static std::uint16_t envelopeReleaseLevel(
        std::uint16_t level, std::uint16_t multiplier) noexcept;
    // The recurrence retains all 14 state bits, but the physical 12-bit DAC
    // receives E>>2. This is the analogue-control fraction actually presented
    // to the VCF envelope summing path and the ENV-mode voice VCA.
    [[nodiscard]] static float envelopeDacFraction(
        std::uint16_t level) noexcept;

    // Inverses of the laws above. The panel travel is what the plug-in stores,
    // but what it *displays* is the value the circuit produces, so a host
    // letting someone type "1.00 kHz" needs a way back to the travel that
    // produces it. Without these the typed number would be read as travel.
    [[nodiscard]] static float panelPositionForAttack(float seconds) noexcept;
    [[nodiscard]] static float panelPositionForDecay(float seconds) noexcept;
    [[nodiscard]] static float panelPositionForRelease(float seconds) noexcept;
    [[nodiscard]] static float panelPositionForLfoRate(float hertz) noexcept;
    [[nodiscard]] static float panelPositionForLfoDelay(float seconds) noexcept;
    [[nodiscard]] static float panelPositionForPortamento(float secondsPerOctave) noexcept;
    [[nodiscard]] static float panelPositionForCutoff(float hertz) noexcept;
    // B-2 doubles the stored seven-bit PWM byte, multiplies it by either
    // 0x3fff (manual) or the raw bipolar 0x2000 +/- LFO accumulator, subtracts
    // that product from 0x3fff and presents the upper twelve bits to the DAC.
    // The two analogue calibration anchors are that DAC code 0x0fff gives the
    // printed +6 V / 50% state, while code zero is the printed -0.8 V pulse-off
    // state. The panel's loaded pot normally stops near byte 101 and 95%; raw
    // SysEx bytes above that physical travel deliberately retain the firmware's
    // overrange and can pin the comparator high.
    [[nodiscard]] static std::uint16_t pwmDacCode(
        float panelPosition, PwmSource source, std::uint16_t lfoAccumulator,
        bool positivePolarity) noexcept;
    [[nodiscard]] static float pwmDacVolts(std::uint16_t code) noexcept;
    [[nodiscard]] static float pwmDutyCycle(float controlVolts) noexcept;
    // The optional second argument exposes pitch-slew and card-current changes
    // of the physical ramp used by that same comparator. A scale of one is the
    // calibrated nominal 12 Vpp ramp.
    [[nodiscard]] static float pwmDutyCycle(float controlVolts,
                                            float rampAmplitudeScale) noexcept;
    // The ramp's constant-current rising segment: 0..1 across the rise and
    // -1..+1 out. The reset that follows it is a straight fall back to the
    // negative rail over the remainder of the cycle.
    [[nodiscard]] static float rampSegmentVoltage(float risePosition) noexcept;
    // Comparator edge positions within a cycle, as fractions of the period.
    [[nodiscard]] static float pulseRisePhase(float duty,
                                              float resetFraction) noexcept;
    [[nodiscard]] static float pulseFallPhase(float duty,
                                              float resetFraction) noexcept;
    // The voice amplifier's schematic-informed compatibility law. The BA662
    // is current-controlled and Roland draws no intentional volts-per-decade
    // converter in this path. A grounded-base stage outside the module makes
    // the control current:
    //
    //   VCA CV (0..+10 V from the S/H) -> R106 10k -> node -> R105 22k
    //     -> Tr20 emitter, base grounded, collector = pin 11 VCA CONT
    //
    // The idealized I_ABC = (V_cv - V_be) / 32 kOhm relation motivates a
    // quasi-linear response above conduction. It does not establish Tr20's
    // installed onset or the BA662's gm-versus-current behavior near cutoff.
    //
    // The softplus below is a smooth, replaceable approximation to that
    // topology. Its 150 mV onset comes from a circuit reconstruction and its
    // thermal knee from an ideal BJT; neither is a measured Juno-106 transfer.
    // It replaced a much wider voiced knee that put 13-15 dB more attenuation
    // on the bottom of the renderer's envelopes.
    //
    // A published teardown infers the opposite -- "the envelope generators are
    // linear and generated by the CPU, so the VCA response must be
    // exponential". That is an inference; the schematic is a drawing, and it
    // wins. Whether the firmware pre-shapes the envelope DAC data is a
    // separate question and is not assumed here.
    struct VoiceVcaControlLaw
    {
        // 150 mV of envelope travel above the control rail's anchored
        // operating point, normalised on the converter's 10 V span.
        // Service Notes pp. 18-19 adjust VR34 (10KB) for +0.25...+0.27 V at
        // TP7 with the D/A forced to 0 V, and p. 13 puts VR34's injection
        // through R126/R127 into distribution amplifier IC27b, whose output
        // is TP7 and feeds the VCA-group demux IC26 -- so the per-voice VCA
        // control rail already stands at about +0.26 V at envelope zero. The
        // reconstruction's ~150 mV no-current region was measured from the CV
        // input on a calibrated unit, i.e. on top of that trimmed standoff,
        // which is the coordinate this constant is expressed in. The standoff
        // is anchored; the 150 mV itself remains the surviving voiced free
        // parameter and OQ-19's sweep owns it. Do not add the +0.26 V again
        // as a separate offset -- it is already the adjusted state.
        static constexpr float turnOn = 0.015f;
        // Ideal-BJT kT/q at room temperature on that same span; compatibility
        // approximation, not a measured BA662/Juno knee.
        static constexpr float knee = 0.0026f;
        // Below this the modelled leakage is more than 95 dB down, so the
        // model returns an exact zero rather than a denormal tail. Product
        // policy, not a measured off-isolation figure.
        static constexpr float deadband = 0.001f;
        // What the block-processing loop treats as a shut amplifier when it
        // decides a voice may be retired. Product policy: set above the gain at
        // the largest modelled control offset a card can present -- 0.008 at the
        // Unit Character ceiling -- which is 75 dB down, so a voice cannot idle
        // forever above the threshold and block a deferred quality change. A
        // real card can retain input-offset thump after calibration, but its
        // unmeasured residual is not synthesized by the nominal model.
        static constexpr float silenceGain = 3.0e-4f;

        [[nodiscard]] static float gain(float control) noexcept;
    };
    // The stored VCA LEVEL trim drives a second, shared uPC1252H2 after the
    // voice sum. Roland's converter chart and jack-board drawing establish the
    // complete nominal path: stored byte b becomes 12-bit code b<<5, the
    // +4..-6 V hold crosses R30/R32 into the R31/R165-biased GC1 node, and NEC
    // specifies -5.9 mV/dB typical. The two helpers expose the intermediate
    // voltage and C7's derived time constant so the suite can check the
    // resistor solve independently of the final gain conversion.
    [[nodiscard]] static float commonVcaControlVolts(
        float dacFraction) noexcept;
    [[nodiscard]] static float commonVcaHoldTimeConstantSeconds() noexcept;
    [[nodiscard]] static float patchLevelGain(float dacFraction) noexcept;
    // Single-pole high-pass corner for a panel position, the gain the leg
    // returns the low band with, and the gain it returns the high band with.
    // The bass-boost position's shelf is derived from the jack-board branch
    // itself -- dry R25 plus the DC-coupled IC4b leg -- and lands on
    // +10.50 dB at DC falling to +1.41 dB in the high band across the
    // R22*(C9+C8) pole at 59.41 Hz; the third-party noise sweep those figures
    // were once fitted to now stands as corroboration.
    [[nodiscard]] static float highPassCornerHz(HighPassMode mode) noexcept;
    [[nodiscard]] static float highPassShelfGain(HighPassMode mode) noexcept;
    [[nodiscard]] static float highPassHighGain(HighPassMode mode) noexcept;

    // Each IC6 output is AC-coupled through C17/C20 (10 uF), then R54/R57
    // (1.5 kOhm), into one 10 kOhm track of the dual main VOLUME pot. These
    // expose the earlier unloaded/full-track reference transfer so research
    // fixtures can compare it with the in-circuit overloads below.
    [[nodiscard]] static float outputCouplingCornerHz() noexcept;
    [[nodiscard]] static float outputCouplingHighGain() noexcept;
    // Loaded transfer at a shaft position. The fixed per-wiper internal load
    // is the 41.3 kOhm selector ladder in parallel with the 101 kOhm headphone
    // input. External jack loads and mono normaling remain outside this scope.
    [[nodiscard]] static float outputCouplingCornerHz(
        float volumePosition) noexcept;
    [[nodiscard]] static float outputCouplingHighGain(
        float volumePosition) noexcept;

    // The instrument has six voice cards; the engine will run more of them for
    // players who want the chords the hardware drops. Public because the host
    // layer clamps its VOICES parameter to the same bound and the editor draws
    // one lamp per available voice.
    static constexpr int hardwareVoices = 6;
    static constexpr int maxVoices = 16;

    // IC1a sums each voice through 33 kOhm against a 3.3 kOhm feedback
    // resistor before the shared HPF and VCA LEVEL circuit.
    static constexpr float voiceSummerGain = 3.3f / 33.0f;
    [[nodiscard]] static constexpr float voiceBusInput(float summedVoices) noexcept
    {
        return summedVoices * voiceSummerGain;
    }
    // The summed bus crosses C14 (10 uF bipolar) into the IC3 common node,
    // where R39 (33 kOhm) returns it to ground. IC1a's output impedance and
    // the CMOS input loading are negligible against R39 at this boundary.
    [[nodiscard]] static float voiceBusCouplingCornerHz() noexcept;
    // Flat and Boost add a selected 47 kOhm virtual-ground input in parallel
    // with R39 at the C14 pole. In either Cut mode, the series C10/C11 path is
    // open at the sub-hertz asymptote but its mux-side 1 MOhm bleed remains a
    // direct parallel load on the common node.
    [[nodiscard]] static float voiceBusCouplingCornerHz(
        HighPassMode mode) noexcept;
    // IC5/uPC1252H2 follows the switched HPF through the manufacturer's
    // application input network: C12 10 uF bipolar and R36 33 kOhm.
    [[nodiscard]] static float commonVcaInputCouplingCornerHz() noexcept;
    // C56/C50 10 uF NP, the per-voice coupling from the summed WAVE node into
    // the voice module's pin 1 VCF IN (module board p. 13). The capacitor is a
    // designator-level read; the resistance it works against is not, so the
    // corner itself is voiced -- see the constant's note in the .cpp.
    [[nodiscard]] static float moduleCouplingCornerHz() noexcept;
    // C59 1 uF/50 V NP, the per-voice coupling from pin 3 VCF OUT into the
    // VR27/R108 network and pin 9 VCA IN (module board pp. 18-19). The
    // capacitor is anchored; the load it works against is not, so the corner
    // is voiced and bracketed -- see the constant's note in the .cpp.
    [[nodiscard]] static float vcaInputCouplingCornerHz() noexcept;
    // The shared noise generator's own support circuit, module board p. 13:
    // Tr21's collector noise crosses C42 1 uF into the BA662 level OTA's
    // 4.7 kOhm input bias (high-pass), and the OTA's output is loaded by
    // C41 100 pF against R79 330 kOhm (low-pass). The level control sits
    // between the two poles and is a plain scalar, so shaping the shared
    // source once ahead of the per-voice level scaling is exact.
    [[nodiscard]] static float noiseSourceHighPassHz() noexcept;
    [[nodiscard]] static float noiseSourceLowPassHz() noexcept;

private:
    // The JUCE-free suites use this narrow friend to drive one filter step, one
    // oscillator period and one envelope segment against independent
    // double-precision solves. It is not part of the plug-in API.
    friend struct YouKnow106TestAccess;

    static constexpr double minimumHqProcessingRate = 176400.0;
    static constexpr int halfbandTaps = 95;
    static constexpr int halfbandRingSize = 128;
    static constexpr int latencyPadRingSize = 64;

    // --- Modelled hardware constants ---------------------------------------

    // One crystal feeds every voice's note timer, so the six voices are
    // inherently in tune with one another; what little pitch instability the
    // instrument has comes from the reference and the control chain, not from
    // six independent oscillator cores.
    static constexpr double masterClockHz = 8000000.0;
    // IC29 executes one state every 250 ns. The recovered pitch-write paths
    // below are timed in these states, independently of the selected DCO clock.
    // https://www.synfo.nl/servicemanuals/Roland/ROLAND_JUNO-106_SERVICE_NOTES_1st.pdf#page=8
    static constexpr double voiceCpuStateHz = 4000000.0;
    // These are recovered no-interrupt instruction-start anchors, not pin
    // edges. T is product policy at the start of ANI PA,$EF: reset control
    // starts at T-389, running LSB at T-334, and both paths' MSB at T-323 CPU
    // states. NEC does not publish exact clock-to-/WR edges, the PA4 write
    // point inside ANI, or loaded HD14051BP/0.01-uF settling. The ADC interrupt
    // is re-masked before this window. A serial handler can abandon only the
    // running path's 13 states before DI; the reset path enters DI four states
    // before its control store. Once either DI begins, the complete protected
    // PIT transaction finishes before a voice handler restarts the main loop.
    // Unpublished serial wire and NMOS entry timing stay unmodelled.
    static constexpr double dcoPitchPrestageStates = 389.0;
    static constexpr double pitResetDiToControlStates = 4.0;
    static constexpr double pitControlToLsbStates = 55.0;
    static constexpr double pitDiToLsbStates = 42.0;
    static constexpr double pitLsbToMsbStates = 11.0;
    static constexpr std::uint32_t minimumDivider = 8u;
    static constexpr std::uint32_t maximumDivider = 65535u;
    // The whole instrument is served by one converter and three 8-way muxes:
    // 18 per-card holds (DCO, VCF and ENV/GATE VCA for six cards), five shared
    // holds (sub, stored VCA LEVEL, PWM, resonance and noise), and one unused
    // mux channel. The scan repeats every 4.2 ms.
    static constexpr double controlScanHz = 1000.0 / 4.2;
    // Rate at which the modelled component wander advances. Fixed in seconds,
    // not in samples, so it does not change with the quality setting.
    static constexpr double driftUpdateHz = 375.0;

    // Ramp generator. The compensation voltage drives a resistor into the
    // integrator's virtual ground, so capacitor current is constant and the
    // rising ramp is straight. The discharge transistor gives only the reset
    // its finite slope.
    static constexpr float rampResetSeconds = 2.2e-6f;
    static constexpr float rampAmplitudeVolts = 12.0f;
    // At B-2 coordinate 0x5400 the pair is code 0x0100 and count 0x1dfb.
    // Their product is a gain-free centre anchor for the approximately 12 Vpp
    // ramp: other pairs retain the B-2 law's small settled amplitude ripple,
    // and its saturated upper codes can no longer be normalized back to unity.
    static constexpr float dcoRampReferenceProduct =
        0x0100u * 0x1dfbu;
    // Compatibility ceiling for delayed hybrid cycles: the schematic powers
    // the ramp MC5534A from +/-15 V but does not specify its installed output
    // swing. +15 V is therefore an ideal supply bound, not a measured clamp.
    static constexpr float dcoPositiveRailVolts = 15.0f;

    // Four-pole transconductor cascade. The 560/68560 divider below is inside
    // the part, so each differential pair sees an attenuated copy of the stage
    // difference and the small-signal pole in module-node coordinates is
    //
    //     wc = Ig * stageAttenuation / (2 Vt C)  with C = 240 pF,
    //
    // equivalently wc = Ig / (C H) with H = 2 Vt / stageAttenuation = 6.37 V,
    // the pair's linear span referred to the node and the coordinate the
    // solver actually runs in. Dropping the divider here states a pole 122x
    // too high -- it would put the 700 uA saturation at 8.9 MHz instead of the
    // 72.9 kHz that divider actually leaves. (The shipped
    // vcfControlSaturationHz is 64 kHz, which is *not* that figure; see its
    // own comment for why the constant nonetheless stands.) The suites solve
    // the same ODE.
    static constexpr float thermalVoltage = 0.026f;
    static constexpr float poleCapacitorFarads = 240.0e-12f;
    // Each stage attenuates its differential input by 560 / (68000 + 560)
    // before the transconductor's differential pair sees it. That attenuator,
    // not any voiced drive control, is what sets where this filter starts to
    // compress: it places the pair's linear span at +/-6.4 V, right at the peak
    // of a full-level ramp.
    static constexpr float stageAttenuation = 560.0f / (68000.0f + 560.0f);
    static constexpr float otaHeadroomVolts = 2.0f * thermalVoltage / stageAttenuation;
    // Half-span of the integrating capacitors' tolerance. The four 240 pF
    // parts are discrete, so nothing trims them into agreement; a few percent
    // is the ordinary class. Voiced under OQ-10, like the other card
    // dispersions -- no measured population fixes it.
    static constexpr float vcfStageCapacitorTolerance = 0.02f;
    // The two VCF trims are the exception among the card dispersions: Roland
    // prints their acceptance. ADJUSTMENT procedures 7/8 (p. 19) repeat the
    // FREQ trim (248 Hz with C4 held, converter code 6272) and the WIDTH
    // trim (992 Hz with C6 held, two octaves up) "until satisfactory result
    // is obtained (within +/-10 cents on the tuner)" -- the procedure bounds
    // the two CHECK POINTS, not an offset and a slope separately, and the
    // note that the procedures interact means both windows hold jointly on a
    // passing card. The model therefore draws each check point's residual
    // independently inside +/-10 cents and takes the line through them:
    // `cutoffOffsetError` is the C4-point draw, `cutoffScaleError` the
    // C6-point draw, interpolated in counts about the anchored code-6272
    // trim point (extrapolation beyond the checked span is unbounded, as the
    // procedure leaves it). Anchored acceptance windows (2026-08-20 pass),
    // replacing the former voiced +/-0.07 octave and +/-5% magnitudes that
    // no source bounded. Field drift beyond the windows belongs to `aging`.
    static constexpr float vcfTrimResidualOctaves = 10.0f / 1200.0f;
    static constexpr float vcfFreqTrimAnchorCounts = 6272.0f;
    static constexpr float vcfWidthTrimSpanCounts = 2.0f * vcfCountsPerOctave;
    // The aged-unit lead's two magnitudes (see EngineParameters::aging):
    // about a quarter tone of flatward VCF drift at full weight, and the
    // +3.52 dB noise-trim drift the same account measured against the
    // 4 Vp-p spec. Voiced, single-unit lineage.
    static constexpr float agingCutoffDriftCents = -50.0f;
    static constexpr float agingNoiseDriftDecibels = 3.52f;
    // Early-effect transconductance modulation inside the cascade. With
    // V_A ~ 100 V and a few hundred millivolts of collector swing at the
    // differential pair, the fractional change in g is a few parts per
    // thousand -- the 0.005 the modelling notes state. A revision used 0.08
    // here, sixteen times that, which is a signal-dependent cutoff shift large
    // enough to hear as odd-harmonic grit on every resonant sweep.
    static constexpr float otaEarlyVoltage = 100.0f;
    static constexpr float otaEarlyEffectCoefficient = 0.005f;
    // Temperature coefficient of the transconductor's cutoff control path, from
    // the AS3109 datasheet -- the IR3109 clone whose own test condition is this
    // circuit's 240 pF and 68 kOhm. It is what turns the modelled chassis
    // thermal gradient into a per-card cutoff difference.
    //
    // A revision instead spread the six cards by 1 + 0.04 * (card - 2.5), which
    // is +/-165 cents: roughly ten times what this coefficient supports across
    // the 4 degC gradient computed beside it, linear in the card index while
    // that gradient is exponential in it, and absent from the README's own Unit
    // Character table. The module board also carries R111, a 560 Ohm positor --
    // a PTC thermistor, listed as such in the parts legend -- returning the CV
    // divider node to ground precisely to cancel this tempco, so the derived
    // figure below is an upper bound on what survives it rather than a
    // measured residual. How much the positor actually leaves is OQ-10.
    static constexpr float vcfCutoffTempcoPerCelsius = 0.0033f;
    // Card-to-card thermal gradient across the chassis, in degrees Celsius at
    // the card nearest the supply, falling exponentially with the card index.
    // Shared by the headroom and cutoff paths so the two cannot disagree.
    static constexpr float chassisGradientPeakCelsius = 4.0f;
    static constexpr float chassisGradientCards = 2.5f;
    [[nodiscard]] static float chassisGradientCelsius(int cardIndex) noexcept;
    // The same profile averaged over the six physical cards. The FREQ trim is
    // set at operating temperature, so what a calibrated instrument carries is
    // the spread about that mean, not the mean itself.
    [[nodiscard]] static float chassisGradientMeanCelsius() noexcept;
    [[nodiscard]] static float boundedThermalFilterOmegaStep(
        float baseOmegaStep, const EngineParameters& parameters,
        int cardIndex) noexcept;
    // Roland publishes an approximate 5 Hz--50 kHz range, but no qualifying
    // capture fixes the high-code saturation shape. Keep the established
    // exponential law and apply an explicit product safety cap at that stated
    // endpoint rather than inventing a knee and asymptote.
    static constexpr float vcfSafetyCapHz = 50000.0f;

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
    // Hold-capacitor slew after the converter. VCF and voice-VCA use the
    // supported 522/687 us values; the common VCA derives its separate value
    // from C7 and its loaded jack-board resistor network. PWM and SUB derive
    // theirs from p. 13's designator-complete post-hold smoothing networks
    // (OQ-07): the PWM hold reaches the comparators through R117/C62 and then
    // R116/C63 around IC17a -- two cascaded poles -- and the stored SUB level
    // reaches its mixer OTA through R11 into C1 ahead of the R9/R10 inverter.
    // Both networks settle to their held value, so the calibrated DC laws are
    // untouched; what they add is the lag the hardware's PWM LFO and level
    // staircase actually cross. The remaining nodes retain compatibility
    // values behind separate names so a measured destination can be replaced
    // without silently changing the others.
    static constexpr float vcfHoldSlewSeconds = 522.0e-6f;
    static constexpr float voiceVcaHoldSlewSeconds = 687.0e-6f;
    static constexpr float pwmSmoothingR117Ohms = 100.0e3f;
    static constexpr float pwmSmoothingC62Farads = 47.0e-9f;
    static constexpr float pwmSmoothingR116Ohms = 560.0e3f;
    static constexpr float pwmSmoothingC63Farads = 4.7e-9f;
    static constexpr float pwmHoldFirstPoleSeconds =         // 4.7 ms
        pwmSmoothingR117Ohms * pwmSmoothingC62Farads;
    static constexpr float pwmHoldSecondPoleSeconds =        // 2.632 ms
        pwmSmoothingR116Ohms * pwmSmoothingC63Farads;
    static constexpr float subSmoothingR11Ohms = 1.0e3f;
    static constexpr float subSmoothingC1Farads = 10.0e-6f;
    static constexpr float subHoldSlewSeconds =              // 10 ms
        subSmoothingR11Ohms * subSmoothingC1Farads;
    static constexpr float dcoHoldSlewSecondsVoiced = 522.0e-6f;
    static constexpr float resonanceHoldSlewSecondsVoiced = 522.0e-6f;
    static constexpr float noiseHoldSlewSecondsVoiced = 522.0e-6f;
    static_assert(std::bit_cast<std::uint32_t> (vcfHoldSlewSeconds)
                      == std::bit_cast<std::uint32_t> (
                          resonanceHoldSlewSecondsVoiced),
                  "the shared exact VCF-hold trajectory requires equal "
                  "cutoff and resonance constants");
    // The shared white-noise generator and each card's microscopic filter
    // excitation represent continuous-time noise densities.  Their discrete
    // sample amplitudes therefore grow with sqrt(processing rate).  The
    // existing 48 kHz HQ render runs at 192 kHz internally, so that rate is
    // the compatibility reference whose sound and factory balance stay put.
    static constexpr double noiseReferenceRateHz = 192000.0;
    // How fast a card's current draw, and therefore the regulator loading it
    // causes, follows the audio it is passing. This used to be a fixed
    // per-internal-sample coefficient, which made the same patch droop four
    // times faster with HQ on than with it off -- a quality setting is not
    // allowed to change what the supply does. Expressed against the same
    // 192 kHz compatibility reference as the noise densities, so the existing
    // 48 kHz HQ render keeps the behaviour it had.
    static constexpr float voiceEnergyFollowerSeconds =
        static_cast<float>(1000.0 / noiseReferenceRateHz);
    // Cutoff counts the reference moves per volt of rail deviation. Named so
    // the droop has one transfer rather than an unlabelled number at the
    // summing point. Voiced, like the droop coefficient it multiplies.
    static constexpr float railToCutoffCountsPerVolt = 35.0f;
    static constexpr float vcfKeyFollowCentreMidi = 60.0f; // C4
    enum class EnvelopeStage { Idle, Attack, Decay, Sustain, Release };

    // Hash-matched B-2 firmware mechanics: a 14-bit integer advanced once per
    // scan. Compact arithmetic generators reproduce the coefficient tables'
    // observable laws without storing either the ROM or complete table dumps.
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
        std::uint16_t level { 0u };
        float value { 0.0f };

        void reset() noexcept;
        void noteOn() noexcept;
        void noteOff() noexcept;
        float tick(std::uint16_t attackIncrement,
                   std::uint16_t decayMultiplier,
                   std::uint16_t sustain,
                   std::uint16_t releaseMultiplier) noexcept;
    };

    // Bandlimiting support. Every discontinuity the oscillator produces is
    // repaired from a continuous step response built by numerically integrating
    // a windowed sinc, rather than from a closed-form polynomial fit. The ideal
    // discontinuity is subtracted only after interpolation so lookup never
    // crosses the residual's unit jump at t=0. Integrating once more and
    // subtracting the ideal ramp yields the continuous slope residual used for
    // slope discontinuities.
    static constexpr int correctionHalfWidth = 24;
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

    // Immutable reconstruction primitives are shared by all plug-in instances.
    // H=24 at 64x is about 24 KiB, and rebuilding/copying those same values into
    // every engine would add instance memory without adding any unit character.
    struct CorrectionTables
    {
        std::array<float, correctionTableLength> stepResponse {};
        std::array<float, correctionTableLength> slopeResidual {};
    };

    // Programmable divider, analogue ramp integrator, pulse comparator and
    // divide-by-two sub: one complete oscillator cell.
    struct Dco
    {
        enum class PitState : std::uint8_t
        {
            stopped,
            awaitingCount,
            awaitingInitialLoad,
            running
        };

        enum class PitEvent : std::uint8_t
        {
            initialLoad,
            fallingEdge,
            risingEdge
        };

        enum class PitWriteState : std::uint8_t
        {
            idle,
            awaitingPitchPrestage,
            awaitingLsb,
            awaitingMsb
        };

        // `divider` is the count currently in the counter element. A complete
        // count-only LSB/MSB write updates only the count register; Mode 3
        // transfers it to the counter element after the current OUT half-cycle.
        std::uint32_t divider { 4545u };
        std::uint32_t pendingDivider { 4545u };
        bool pendingDividerValid { false };
        PitState pitState { PitState::stopped };
        bool pitOutHigh { true };
        // One voice CPU can have only its current fixed pitch transaction in
        // flight. Its pre-stage latches pitch state, then the LSB latch becomes
        // a complete count register on the MSB; no general CPU queue is needed.
        PitWriteState pitWriteState { PitWriteState::idle };
        std::uint32_t pitWriteDivider { 4545u };
        double cpuStatesToWrite { 0.0 };
        // Engine construction has no earlier powered-card capacitor state.
        // Keep that initialization policy explicit; a warm ramp held at its
        // positive supply bound also has zero slope/reset time and must not be
        // mistaken for cold.
        bool coldInitialLoadPending { false };
        // Remaining selected input-clock periods to the next CE load or OUT
        // transition. While CE awaits both count bytes this is instead only
        // the continuously running selected-CLK phase. A pending shared range
        // handoff may make the retained old-cycle remainder longer than one
        // newly selected period.
        double pitClocksToEvent { 0.0 };
        double periodSamples { 100.0 };
        // Linear C54 compatibility model. Positive OUT starts the discharge;
        // its exact transistor waveform remains unmeasured, so the established
        // finite linear reset is retained without claiming it is that waveform.
        double rampValue { -1.0 };
        double rampSlopePerSecond { 0.0 };
        double resetSecondsRemaining { 0.0 };
        // A supply-limited charge is distinct from every other zero-slope
        // state. While held, live card-current changes reproject rampValue so
        // the physical capacitor node remains exactly +15 V.
        bool positiveRailHeld { false };
        // The compensation ratio the current cycle's ramp was launched with.
        // The physical ramp integrates whatever current its slewing CV set at
        // the discharge, so a CV still catching up changes the *slope of the
        // next rise*, never the value mid-cycle. Freezing the ratio per cycle
        // is what keeps the rendered ramp value-continuous: it only takes a
        // new value at a wrap, where both cycles share the -1 rail.
        float renderScale { 1.0f };
        float pulseState { -1.0f };
        // The divider's output level is the whole of its state. Holding a
        // separate toggle alongside it only creates a way for the two to
        // disagree, which is what made the first half-cycle after a retrigger
        // twice as long as every other one.
        float subState { 1.0f };
        BandlimitedTrack saw {};
        BandlimitedTrack pulse {};
        BandlimitedTrack sub {};

        [[nodiscard]] static std::uint32_t mode3HalfClocks(
            std::uint32_t count, bool outHigh) noexcept;
        // A control word stops CE while the selected PIT input-clock phase
        // continues. The later complete LSB/MSB pair arms the next-clock load.
        [[nodiscard]] bool programMode3(
            double clocksToNextInputEdge) noexcept;
        void stageMode3Count(std::uint32_t count) noexcept;
        [[nodiscard]] PitEvent consumePitEvent() noexcept;
        void reset() noexcept;
    };

    // Four transconductor stages plus the inverting resonance return. The
    // physical continuous-time equations are advanced directly with two
    // half-interval, five-stage Merson steps. A causal current-plus-three-past
    // reconstruction supplies the input at every stage abscissa without
    // lookahead.
    struct OtaCascade
    {
        static constexpr std::array<double, 7> controlNodePositions {
            0.0, 1.0 / 6.0, 1.0 / 4.0, 1.0 / 2.0,
            2.0 / 3.0, 3.0 / 4.0, 1.0
        };
        struct ControlTrajectory
        {
            std::array<double, controlNodePositions.size()> omegaStep {};
            std::array<double, controlNodePositions.size()> feedback {};
            std::array<double, controlNodePositions.size()> headroom {};
        };

        // Capacitor voltages are the complete physical state. Double precision
        // keeps the explicit high-order step from throwing away the accuracy
        // it gains over the former float path-average solve.
        std::array<double, 4> state {};
        std::array<float, 4> offsetVoltage {};
        // Sample-grid support, not circuit memory. Point zero is the most
        // recent completed endpoint; the current endpoint supplied to process
        // is the fourth point of the causal cubic interpolant.
        std::array<double, 3> inputHistory {};
        // Startup and a changed numerical grid do not yet own three uniformly
        // spaced endpoints. Ramp linear -> quadratic -> cubic instead of
        // treating reset zeros or old-grid points as measured history.
        int inputHistoryCount { 0 };
        // Each stage integrates into its own 240 pF capacitor, so each pole
        // sits where that capacitor's tolerance puts it. Unity is the
        // calibrated nominal model, where all four coincide.
        std::array<float, 4> gScale { 1.0f, 1.0f, 1.0f, 1.0f };
        // Cutoff, resonance and thermal headroom are physical controls which
        // can slew between internal endpoints. Interpolating their two known
        // endpoints at the RK nodes avoids turning the present endpoint into
        // a zero-order hold. The first call is primed at its current values.
        double previousOmegaStep { 0.0 };
        double previousFeedback { 0.0 };
        double previousHeadroom { 0.0 };
        bool parameterHistoryPrimed { false };

        static constexpr int integrationSubsteps = 2;
        static constexpr int rhsEvaluationsPerInterval = 10;

        // The concrete tableaux `VcfSolverMode` resolves to. A mode names the
        // *cheapest* tableau it is willing to use; `planTableau` below escalates
        // it on the intervals where that tableau is not numerically
        // admissible, so a rung is a cost ceiling rather than a promise about
        // a particular interval. Merson is the bottom of that escalation for
        // every rung, not a peer of the other two.
        //
        // Every abscissa is a `controlNodePositions` entry, which is what lets
        // the whole ladder share one reconstruction grid and one hold
        // trajectory:
        //
        //   MersonHalf  0, 1/6, 1/6, 1/4, 1/2 | 1/2, 2/3, 2/3, 3/4, 1
        //   Rk4Half     0, 1/4, 1/4, 1/2      | 1/2, 3/4, 3/4, 1
        //   Rk4Full     0, 1/2, 1/2, 1
        enum class Tableau : std::uint8_t
        {
            MersonHalf, Rk4Half, Rk4Full
        };
        // Which of the seven control nodes each tableau reads, as a bit per
        // ordinal. The reconstruction loop skips the rest: a full-interval rung
        // needs three of the seven causal-cubic reconstructions, not all seven.
        [[nodiscard]] static constexpr unsigned int tableauNodeMask(
            Tableau tableau) noexcept
        {
            switch (tableau)
            {
                case Tableau::MersonHalf: return 0b1111111u;
                case Tableau::Rk4Half:    return 0b1101101u;
                case Tableau::Rk4Full:    return 0b1001001u;
            }
            return 0b1111111u;
        }
        [[nodiscard]] static constexpr int tableauRhsEvaluations(
            Tableau tableau) noexcept
        {
            switch (tableau)
            {
                case Tableau::MersonHalf: return 10;
                case Tableau::Rk4Half:    return 8;
                case Tableau::Rk4Full:    return 4;
            }
            return 10;
        }

        // Largest normalized step `|h * lambda|` over the whole interval that
        // each RK4 rung is allowed to take before `planTableau` escalates it.
        //
        // `singleStepRk4Limit` is an accuracy bound. Classic RK4 stays stable
        // along the negative real axis to about 2.785 and along the imaginary
        // axis to about 2.828, so a full-interval step of 1.25 is far inside
        // the stability region; what sets it is the measured sweep against an
        // independent 96-substep reference solve, which holds below -87 dB
        // relative error out to 2.35 and only leaves the reference near 2.97.
        // At 1.25 the sweep's worst case over every rate, cutoff and resonance
        // is -97.5 dB, and that worst case comes from the causal input
        // reconstruction rather than from the step size, so lowering the limit
        // further does not improve it.
        //
        // `halfStepRk4Limit` is a *stability* bound, and it is why the ladder
        // needs Merson underneath it rather than beside it: Merson's five-stage
        // region reaches about 3.55 on the negative real axis where classic
        // RK4 reaches 2.785, and the product grid's own 0.9*pi cap can put the
        // cascade's fastest closed-loop eigenvalue past the smaller of the two.
        // The suite's zero-input cap fixture finds the onset of a false limit
        // cycle at 2.66 to 3.00 per half step -- exactly where RK4's region
        // ends -- so the bound below is 2.0 per half step, 72% of that radius.
        // Merson runs those intervals for every rung; see
        // Docs/decisions.md for both measurements.
        static constexpr double singleStepRk4Limit = 1.25;
        static constexpr double halfStepRk4Limit = 4.0;
        // Bound on the cascade's fastest closed-loop eigenvalue, in units of
        // the small-signal pole. Four identical one-poles closed through a
        // gain of `feedback` put the loop roots at
        // `s/w = -1 + feedback^(1/4) * exp(i*(pi + 2*pi*m)/4)`, whose largest
        // magnitude is `sqrt((1 + q)^2 + q^2)` with `q = feedback^(1/4)/sqrt2`.
        // The stage capacitor spread scales `w` per stage, so the caller
        // supplies the largest stage pole rather than the nominal one.
        [[nodiscard]] static double closedLoopSpectralFactor(
            double feedback) noexcept;
        // `closedLoopSpectralFactor` at the sanitized feedback ceiling of
        // eight, which is the most any admissible loop gain can stretch the
        // small-signal pole. A step small enough to pass against this is
        // admissible whatever the resonance is, which is what lets
        // `planTableau` decide the common case without evaluating the factor
        // at all. The closed form is sqrt((1 + 2^(1/4))^2 + sqrt(2)): `q` at
        // feedback eight is 8^(1/4)/sqrt(2), which reduces to exactly
        // 2^(1/4).
        static constexpr double maximumClosedLoopSpectralFactor =
            2.491353317928156;
        // The tableau one interval actually runs, given the cheapest one its
        // mode allows, the largest stage pole the interval presents and the
        // largest loop gain it presents. `poleStep` is the normalized step
        // before the resonance term; the two are kept apart rather than
        // multiplied by the caller so the ceiling short circuit above can be
        // tried first. A non-finite `poleStep` fails every comparison and
        // therefore resolves to Merson, which is the safe answer.
        [[nodiscard]] static Tableau planTableau(
            VcfSolverMode mode, double poleStep, double feedback) noexcept
        {
            if (mode == VcfSolverMode::MersonHalfSteps)
                return Tableau::MersonHalf;
            // The ceiling short circuit: a step this small is admissible at any
            // resonance, so the common case never evaluates the factor.
            if (poleStep * maximumClosedLoopSpectralFactor
                    <= singleStepRk4Limit)
                return mode == VcfSolverMode::Rk4Single
                     ? Tableau::Rk4Full : Tableau::Rk4Half;
            const double normalisedStep =
                poleStep * closedLoopSpectralFactor(feedback);
            if (mode == VcfSolverMode::Rk4Single
                && normalisedStep <= singleStepRk4Limit)
                return Tableau::Rk4Full;
            return normalisedStep <= halfStepRk4Limit
                 ? Tableau::Rk4Half : Tableau::MersonHalf;
        }
        // The renderer's product-grid safety cap is 0.45 cycles per internal
        // sample. Apply the card's thermal spread before this numerical bound
        // so Unit Character cannot make the Merson solver see a larger
        // interval.
        static constexpr double maximumOmegaStep =
            2.8274333882308138; // 0.9*pi
        static constexpr double maximumInputReconstructionL1 =
            1.631131;

        void reset() noexcept;
        // A rate change retains physical charge and the most recent input
        // endpoint. Older uniformly-spaced samples belong to the old grid and
        // are collapsed under the engine's existing zero-gain transition.
        // Named previousStep/nextStep, not previousOmegaStep/nextOmegaStep,
        // because the .cpp definition reads and reassigns the member
        // previousOmegaStep in its body -- a same-named parameter would
        // shadow it there and silently change which value gets scaled.
        void retime(float previousStep, float nextStep) noexcept;
        template <bool useCubicEarly = false>
        float process(float input, float omegaStep, float feedback,
                      float headroom = otaHeadroomVolts,
                      bool enableEarlyEffect = true,
                      float calibration = 0.70f,
                      const ControlTrajectory* trajectory = nullptr,
                      VcfTanhMode tanhMode = VcfTanhMode::Exact,
                      VcfSolverMode solverMode =
                          VcfSolverMode::MersonHalfSteps) noexcept;
#if defined(YOUKNOW106_HAS_VCF_PAIR_SIMD)
        // Settled shipping intervals resolve to either one full RK4 step or
        // two half steps. Advance two independent cards taking the same
        // tableau in two FP64 SIMD lanes; false means the caller must use
        // `process` without this function having changed either cascade.
        static bool tryProcessSettledRk4Pair(
            OtaCascade& first, float firstInput, float firstOmegaStep,
            float firstFeedback, float firstHeadroom,
            OtaCascade& second, float secondInput, float secondOmegaStep,
            float secondFeedback, float secondHeadroom,
            bool enableEarlyEffect, float calibration,
            float& firstOutput, float& secondOutput) noexcept;
        // Keep the deeper Merson solve separate from the hot RK4 function.
        // It is tried only after that function rejects, and false likewise
        // leaves both cascades and output sentinels untouched.
        static bool tryProcessSettledMersonPair(
            OtaCascade& first, float firstInput, float firstOmegaStep,
            float firstFeedback, float firstHeadroom,
            OtaCascade& second, float secondInput, float secondOmegaStep,
            float secondFeedback, float secondHeadroom,
            bool enableEarlyEffect, float calibration,
            float& firstOutput, float& secondOutput) noexcept;
        // Four settled Merson cards share one FP32 SIMD solve, bounded by the
        // 10 uV scalar-error regression. A rejected group leaves every
        // cascade and output sentinel unchanged.
        static bool tryProcessSettledMersonQuad(
            const std::array<OtaCascade*, 4>& cascades,
            const std::array<float, 4>& inputs,
            const std::array<float, 4>& omegaSteps,
            const std::array<float, 4>& feedbacks,
            const std::array<float, 4>& headrooms,
            bool enableEarlyEffect, float calibration,
            std::array<float, 4>& outputs) noexcept;
#endif

        [[nodiscard]] static double reconstructInput(
            double current, const std::array<double, 3>& history,
            double intervalPosition) noexcept;
        [[nodiscard]] static double clampOmegaStep(double value) noexcept;
        [[nodiscard]] static double zonedHermiteTanh(double value) noexcept;
        [[nodiscard]] static double zonedHermiteTanhUnchecked(
            double value) noexcept;
        [[nodiscard]] static double cubicEarlyTanh(double value) noexcept;
    };

    // Exact continuous trajectory of one 522 us VCF hold over an internal
    // interval. It is a value object: constructing it neither consumes the
    // converter cursor nor changes the official held target. That separation
    // lets the audio interval see a write at its fractional physical time
    // while the recovered firmware scheduler still polls it at the following
    // internal boundary.
    struct VcfHoldInterval
    {
        std::array<double, OtaCascade::controlNodePositions.size()> value {};
        float endpoint { 0.0f };
    };
    [[nodiscard]] static VcfHoldInterval exactVcfHoldInterval(
        float state, float target, bool hasEvent, double eventPosition,
        float eventTarget, double intervalSeconds) noexcept;

    // The same converter timing rule applies to every passive hold whose
    // post-write trajectory is modelled, including resonance's explicitly
    // voiced Step 11 companion trajectory. There can be at most one physical
    // write inside an internal interval, so one scalar payload is sufficient:
    // the firmware cursor still commits it at the next ordinary poll while
    // the analogue network sees it at the fractional physical time.
    struct PassiveHoldEventLatch
    {
        bool valid { false };
        bool nextPass { false };
        std::size_t ordinal { 0u };
        ConverterWrite write { ConverterDestination::Resonance, -1 };
        float target { 0.0f };
        double eventPosition { 0.0 };
    };

    [[nodiscard]] static double exactOnePoleHoldEndpoint(
        double state, float target, bool hasEvent, double eventPosition,
        float eventTarget, double intervalSeconds, double timeConstantSeconds,
        double fullIntervalDecay) noexcept;

    struct PwmHoldCoefficients
    {
        double firstDecay { 1.0 };
        double secondDecay { 1.0 };
        double firstToSecond { 0.0 };
    };
    struct PwmHoldState
    {
        double first { 0.0 };
        double second { 0.0 };
    };

    // Every member is derived solely from the negotiated host/internal rates.
    // Keeping the original expression types here lets process() consume the
    // exact same values without repeating exponentials and divisions for every
    // host block.
    struct ProcessingCoefficients
    {
        float vcfSlew { 0.0f };
        float dcoSlew { 0.0f };
        float resonanceSlew { 0.0f };
        float noiseSlew { 0.0f };
        double internalIntervalSeconds { 0.0 };
        double voiceVcaDecay { 1.0 };
        double commonVcaTime { 1.0 };
        double commonVcaDecay { 1.0 };
        double subDecay { 1.0 };
        PwmHoldCoefficients pwmFullInterval {};
        float outputGlide { 0.0f };
        double scanPhasePerInternalSample { 0.0 };
        float outputBoundaryGain { 1.0f };
        float outputSlewMaxStep { 0.0f };
    };
    [[nodiscard]] static PwmHoldCoefficients pwmHoldCoefficients(
        double intervalSeconds) noexcept;
    [[nodiscard]] static PwmHoldState advancePwmHold(
        PwmHoldState state, double target,
        const PwmHoldCoefficients& coefficients) noexcept;
    [[nodiscard]] static PwmHoldState exactPwmHoldEndpoint(
        PwmHoldState state, float target, bool hasEvent,
        double eventPosition, float eventTarget, double intervalSeconds,
        const PwmHoldCoefficients& fullIntervalCoefficients) noexcept;

    struct HighPass
    {
        // Double precision matters for the 1.38 Hz final coupling pole: at
        // high host rates a float state rounds to the input while a visible
        // residual is still present, leaving false DC instead of converging.
        double state { 0.0 };

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

    // Optional per-voice analogue variation, drawn once from a deterministic
    // seed so a given Unit Character setting always produces the same
    // instrument. Each field represents a plausible dispersion mechanism: the two
    // cutoff axes are the two per-voice trimmers -- one offsets the control
    // voltage, one scales it -- left imperfectly set; the amplifier gets a
    // control-hold offset and gain error because that path is analogue. It does
    // not reuse either as a BA662 signal-input offset: VR30 trims that separate
    // mechanism for minimum thump, whose calibrated residual is unmeasured;
    // and there is deliberately *no* envelope-rate error, because the
    // envelopes are computed digitally in the shared processor and are
    // therefore identical across voices. What differs between voices is the
    // analogue chain each envelope drives, not the envelope itself. The
    // distribution magnitudes remain voiced rather than measured residuals.
    struct VoiceCard
    {
        float rampCurrentError { 0.0f };
        float comparatorOffset { 0.0f };
        float cutoffOffsetError { 0.0f };
        float cutoffScaleError { 0.0f };
        float resonanceError { 0.0f };
        float vcaControlOffset { 0.0f };
        float vcaGainError { 0.0f };
        float subLevelError { 0.0f };
        float noiseLevelError { 0.0f };
        // Fixed per-card cutoff-temperature factor. It is refreshed only when
        // Unit Character or the spatial-gradient switch moves; renderVoice
        // then applies it without rebuilding the same exponent-derived card
        // coordinate every internal sample.
        double thermalFilterOmegaScale { 1.0 };
        // How much of the aged-unit cutoff flattening this card takes: a
        // seeded uniform [0, 1] draw, so some cards drift little -- the
        // documented recalibration's qualitative pattern (most voices about a
        // quarter tone flat, one near dead-on) without fitting that single
        // unit's exact residuals. Consumed only when `aging` is nonzero.
        float agingWeight { 0.0f };
        // The precomputed aged cutoff shift in converter counts (zero unless
        // `aging` is raised), so the static count transform can stay pure.
        float agingCutoffCounts { 0.0f };
        float driftPhase { 0.0f };
        float driftValue { 0.0f };
        std::uint32_t driftState { 1u };
        // Per-stage input offsets *of the differential pairs*, in volts at
        // the pair. The cascade's arithmetic runs in module-node volts, where
        // the pair sees everything through the anchored 560/68560 divider, so
        // the transfer into the filter divides by `stageAttenuation` -- a
        // 1.5 mV pair offset stands 183.6 mV tall in node coordinates. An
        // earlier revision handed these to the node unconverted, which shrank
        // the documented mechanism 122x into inaudibility.
        std::array<float, 4> vcfStageOffsets { 0.0015f, -0.0012f, 0.0018f, -0.0010f };
        // Signed, unbiased draw for each stage's integrating capacitor.
        std::array<float, 4> vcfStageGErrors {};
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
        // The voice CPU takes its Mode-3 control-word path only for a different
        // pitch on a voice whose key/sustain run bits are both clear. This
        // request is consumed when the voice's converter turn arrives, never
        // synchronously at the host MIDI event. Mode programming then forces
        // the explicitly stored 82C53 OUT high; only low-to-high discharges C54.
        bool dcoResetPending { true };
        // The retired card is advancing only its free-running state (DCO
        // PIT/ramp, sub-divider level, render scale, card noise) through the
        // cheap freewheel path rather than the full render whose output the
        // engine discards anyway. Set only under the fast VCF tanh modes;
        // Exact keeps the established always-on card render. Waking resumes
        // from the frozen support state rather than flushing it -- measured
        // closer to the always-rendered reference; see initialiseVoice.
        bool freewheeling { false };
        // The assigner's note-memory table and the voice CPU's pitch byte are
        // separate RAM. A POLY-button handler clears the former, but the
        // latter -- and the portamento integrator it drives -- keep running.
        bool hasAllocatorHistory { false };
        bool hasVoicePitchHistory { false };
        int rootMidi { -1 };
        int cardIndex { 0 };
        std::uint64_t generation { 0 };
        // When this slot last released its key, on the shared generation
        // counter. The assigner prefers the slot that has been free longest.
        std::uint64_t releaseStamp { 0 };
        float velocity { 0.0f };
        float currentMidi { 60.0f };
        float targetMidi { 60.0f };
        // The last physical key this slot sounded, before transpose. The
        // assigner stores keyboard-note identity; transpose is added only to
        // the pitch message sent to the voice board.
        int lastRootMidi { -1 };
        // The last pitch byte received by the voice CPU, after transpose. Its
        // oscillator-reset comparison lives in this domain, not in the
        // assigner's physical-key domain above.
        int lastVoiceMidi { -1 };
        float glideSemitonesPerScan { 0.0f };
        // Dimensionless physical interval omega*dt for the current cutoff.
        // Unlike the former TPT coefficient this is consumed directly by the
        // continuous-time RK step and therefore needs no tan/atan round trip.
        float filterOmegaStep { 0.1f };
        // The counts and loop gain `filterOmegaStep` was last solved for.
        // Both are compared for exact equality, so this memo cannot return
        // anything the chain would not have recomputed; a sentinel that no
        // real count can equal forces the first solve. The internal rate is
        // not part of the key because a rate change rebuilds voice state.
        float cutoffChainCounts { -1.0e30f };
        float cutoffChainFeedback { -1.0e30f };
        // Converter counts and control voltages, before and after the sample
        // and hold's own slew. The staircase the scan writes is smoothed by a
        // real time constant on each hold capacitor, so modulation arrives
        // rounded rather than stepped. Only pitch, cutoff and ENV/GATE VCA are
        // per-card holds; PWM, sub and noise are shared converter outputs kept
        // on the engine below.
        float cutoffCountsTarget { 0.0f };
        float cutoffCounts { 0.0f };
        float vcaControlTarget { 0.0f };
        // Physical capacitor charge. Double precision keeps very slow tails
        // moving at high internal rates after their float-sized increment has
        // fallen below half an ULP of the present state.
        double vcaControl { 0.0 };
        // Oscillator compensation hold in the firmware's unshifted 12-bit DAC
        // code. The timer's count steps independently; this code slews, and
        // code*active-count is the momentary ramp-amplitude coordinate frozen
        // per cycle (`Dco::renderScale`). It reaches the pulse only through
        // the comparator's edge times.
        float dcoCvTarget { 256.0f };
        float dcoCv { 256.0f };
        // A physical card computes one paired PIT/CV transaction at T-389.
        // The PIT bytes then run independently; only this captured hold target
        // is committed when the converter cursor reaches T.
        bool dcoPitchTransactionValid { false };
        bool dcoPitchTransactionColdStart { false };
        float dcoPitchTransactionCvTarget { 256.0f };
        std::uint16_t attackIncrement { envelopePeak };
        std::uint16_t decayMultiplier { 0x8000u };
        std::uint16_t releaseMultiplier { 0x8000u };
        float feedback { 0.0f };
        float inputCompensation { 1.0f };
        float vca { 0.0f };
        // VoiceVcaControlLaw::gain(vcaControl) alone, before updateVoiceAudio
        // folds in the per-card gain error to produce `vca` above. The main
        // scan loop's post-render silence check compares against this same
        // softplus law on the same vcaControl a moment later; caching it here
        // spares that check the log1p/exp pair updateVoiceAudio already paid
        // for every active voice, every internal sample.
        float vcaGain { 0.0f };
        float pulseDuty { 0.5f };
        // Physical comparator threshold. `pulseDuty` remains the public/
        // diagnostic nominal duty, but its endpoint clamps lose reachable
        // thresholds on low-amplitude ramps and the actual -0.8 V Pulse-Off
        // write. Keeping volts also survives a renderScale change mid-sample.
        float pulseThresholdVolts { 6.0f };
        bool pulsePinnedHigh { false };
        // rampCurrentScaleFor(card, calibration), refreshed when calibration
        // moves and by updatePulseComparator for rendered cards. renderVoice
        // then consumes this exact cached scale, including on the retired fast
        // path that deliberately skips the comparator update.
        float rampCurrentScale { 1.0f };
        // PWM is a moving comparator threshold, not a pulse oscillator whose
        // edge position is frozen for one sample.  Retaining the previous
        // threshold lets renderVoice solve crossings caused by both the ramp
        // and the slewing hold voltage; without it, deep PWM can skip an edge
        // and leave a full extra pulse cycle in the output.
        float previousPulseThresholdVolts { 6.0f };
        bool previousPulsePinnedHigh { false };
        bool pulseThresholdPrimed { false };

        float energy { 0.0f };
        std::uint32_t noiseState { 1u };
        Envelope envelope {};
        Dco dco {};
        OtaCascade filter {};
        // C56/C50, the per-voice module-input coupling. It stands between the
        // summed WAVE node and pin 1 VCF IN, so no mixer DC reaches the
        // filter core or the voice VCA behind it.
        HighPass moduleCoupling {};
        // C59, the per-voice coupling out of pin 3 VCF OUT and into pin 9
        // VCA IN. The filter core makes DC of its own -- stage offsets and the
        // duty-asymmetric pulse the cascade only partly removes -- and this is
        // the capacitor that stops the envelope from multiplying it.
        HighPass vcaInputCoupling {};
        // The pin 9 node itself, held after each internal sample: the value
        // the voice VCA multiplies, in volts. It is what the service
        // procedure's VR30/R112 null is adjusted against, and the DC
        // regression reads it here rather than inferring it from the mix,
        // where three further couplings have already removed any DC.
        float vcaInputVolts { 0.0f };
    };

    static EngineParameters sanitise(const EngineParameters& parameters) noexcept;
    static double midiToHz(double midiNote) noexcept;
    static std::uint32_t hash32(std::uint32_t value) noexcept;
    static float hashBipolar(std::uint32_t value) noexcept;
    // The xorshift32 step shared by the shared noise generator, each card's
    // microscopic filter excitation and the per-card analogue drift wander:
    // three call sites advanced this identical 13/17/5 shift-xor sequence on
    // their own state word by hand before this was pulled out.
    [[nodiscard]] static std::uint32_t xorshift32(std::uint32_t state) noexcept;
    // A generator state's top 24 bits read as a signed unit value, -1..+1.
    // hashBipolar and both 24-bit noise call sites below all performed this
    // identical bit extraction inline; the drift wander keeps its own 16-bit
    // read local to updateVoiceCardDrift, which is the only place that
    // resolution is used.
    [[nodiscard]] static float bipolarFromState(std::uint32_t state) noexcept;
    // The oversampled lookup addStep and addSlope both walk: same ring index,
    // same subsample offset, same clamp/lerp arithmetic, only the table
    // differs. Solved once here so the two callers stop repeating the
    // identical interpolation for every ring sample of every event.
    [[nodiscard]] static float interpolatedCorrectionSample(
        const std::array<float, correctionTableLength>& table,
        int ringIndex, float offset) noexcept;
    // `height` is a value discontinuity (the comparator and divider edges);
    // `slopeStep` is a per-sample slope discontinuity, which is what the
    // integrator's finite-slope reset is at each of its two corners.
    // `samplesAgo` locates the event inside the sample just rendered.
    void addStep(BandlimitedTrack& track, float height,
                 float samplesAgo) const noexcept;
    void addSlope(BandlimitedTrack& track, float slopeStep,
                  float samplesAgo) const noexcept;
    // Start the retained finite-linear C54 discharge on a verified positive
    // M82C53 OUT edge and clock the sub flip-flop at the same timestamp.
    void beginDcoDischarge(Voice& voice, float samplesAgo,
                           bool addCorrections) noexcept;
    void beginDcoCharge(Voice& voice, float samplesAgo,
                        bool addCorrections) noexcept;
    void writeDcoMode3Control(Voice& voice, double clocksToNextInputEdge,
                              float samplesAgo,
                              bool addCorrections) noexcept;
    void prestageDcoPitchTransaction(Voice& voice,
                                     double clocksToNextInputEdge,
                                     float samplesAgo,
                                     bool addCorrections) noexcept;
    void programDcoCount(Voice& voice, std::uint32_t count,
                         bool writesControlWord) noexcept;
    void beginRangeClockTransition(DcoRange previous,
                                   DcoRange next) noexcept;
    void advanceRangeClock(DcoRange range) noexcept;
    void updateActiveDcoPeriod(Dco& dco, DcoRange range) noexcept;
    void advanceDcoPitAndRamp(Voice& voice, DcoRange range,
                              float previousThresholdVolts,
                              float thresholdVolts,
                              bool previousPinnedHigh,
                              bool pinnedHigh,
                              bool addCorrections) noexcept;
    // Fraction of the ramp's full excursion consumed by the finite-slope reset
    // at a given period, clamped so a very high note cannot invert the ramp.
    static float resetFraction(double periodSeconds) noexcept;
    [[nodiscard]] static double dcoPositiveBaseRail(
        double totalRampScale) noexcept;
    void buildHalfbandKernel() noexcept;
    [[nodiscard]] static const CorrectionTables& correctionTables() noexcept;
    void buildVoiceCards() noexcept;
    // Copies each card's IR3109 per-stage trims -- input offset voltage and
    // integrating-capacitor tolerance -- into its voice, scaled by Unit
    // Character. Both are fixed properties of a card and the amount only moves
    // when the panel does, so this is called where those change, not from the
    // audio path.
    void refreshVoiceCardStageTrims() noexcept;
    void refreshVoiceCardThermalScales() noexcept;
    void refreshVoiceRampCurrentScales() noexcept;
    void refreshAgedUnitState() noexcept;
    // One internal sample of chassis warm-up: the wall-clock timer and the
    // exponential the voices read. The render loop's only way to advance it,
    // so a fixture that drives it directly drives exactly what audio does.
    void advanceThermalWarmup() noexcept;
    // The OTA headroom the cascade is solved with on one card, in module-node
    // volts: 2 Vt(T) / stageAttenuation at the chassis temperature the warm-up
    // clock has reached, plus this card's place in the spatial gradient.
    [[nodiscard]] float dynamicOtaHeadroomVolts(
        const EngineParameters& parameters, int cardIndex) const noexcept;
    void noteOnInternal(int midiNote, float velocity) noexcept;
    // Assigns a note already present in the held-key table. Kept separate from
    // noteOnInternal so a POLY-mode rebuild does not count the physical key a
    // second time.
    void assignHeldNote(int midiNote, float velocity) noexcept;
    // B-2's serial Voice On/Off handlers discard their interrupt return by
    // replacing SP and jumping to the start of the voice-board loop. Model
    // that loop restart at the logical command boundary while leaving the
    // still-unpublished serial wire phase and NMOS interrupt-entry delay out.
    void finishProtectedPitWritesBeforeSerialVoiceCommand() noexcept;
    void restartVoiceBoardScanAfterSerialVoiceCommand() noexcept;
    void noteOffInternal(int midiNote) noexcept;
    void initialiseVoice(Voice& voice, int slot, int midiNote,
                         float velocity) noexcept;
    void silenceVoice(Voice& voice) noexcept;
    [[nodiscard]] bool anyVoiceSounding() const noexcept;
    void rearmLfoDelay() noexcept;
    // Empties only the blocks whose state depends on the internal processing
    // rate. The final host-rate coupling capacitors survive an HQ rebuild.
    void clearRateDependentOutputPath(
        bool preserveFreeRunningState = false) noexcept;
    // Empties everything downstream of the voices, including those coupling
    // capacitors, for reset and hard-stop semantics.
    void clearOutputPath() noexcept;
    // Glide rate for the eight-bit performance-control code, in 1/256-
    // semitone units per converter scan.
    [[nodiscard]] static float glideStepPerScan(float portamento) noexcept;
    // Memoized wrapper around glideStepPerScan(): PORTAMENTO is the one shared
    // performance control, so every sounding voice's Pitch write resolves the
    // same table lookup from it. See the note beside glideLawPortamento_.
    [[nodiscard]] float resolveGlideStepPerScan(float portamento) noexcept;
    // Lift the key on one slot: sustain it if the pedal is down, release it
    // otherwise. Every path that lets go of a note goes through here.
    void releaseVoiceKey(Voice& voice) noexcept;
    // Hand a slot out of the Unison stack, releasing it as if its key had been
    // let go. Used when the voice count shrinks below the stack that is
    // already sounding.
    void dropFromUnison(Voice& voice) noexcept;
    int findVoiceForNote(int midiNote) const noexcept;
    int allocateVoice(int midiNote) noexcept;
    // True only for the physical-key bit's low-to-high transition. Repeated
    // MIDI Note Ons are counted so their matching offs remain balanced, but
    // do not retrigger the assigner or envelope.
    bool rememberHeldNote(int midiNote, float velocity) noexcept;
    // Highest key still physically held, or -1. After the assigner clears and
    // rescans its key table, scan order gives the highest held note priority in
    // Solo Unison.
    [[nodiscard]] int highestHeldNote() const noexcept;
    // POLY-button changes gate current assignments and clear only allocator
    // state immediately. The keyboard is rescanned after the next complete
    // converter boundary, so the voice CPUs can observe gate-off first.
    void beginVoiceAssignmentRescan() noexcept;
    void completeVoiceAssignmentRescan() noexcept;
    // True only for a held-key bit's final high-to-low transition. An off for
    // a key whose bit is already clear is ignored completely.
    bool forgetHeldNote(int midiNote) noexcept;
    void clearHeldNotes() noexcept;
    // One complete voice update, retained as a narrow test seam. Realtime
    // processing calls the split destination methods through the recovered
    // converter queue below.
    [[nodiscard]] std::uint32_t updateVoiceScan(
        Voice& voice, const EngineParameters& parameters,
        float lfoGated) noexcept;
    [[nodiscard]] std::uint32_t updateVoiceEnvelopeAndPitch(
        Voice& voice, const EngineParameters& parameters) noexcept;
    void updateVoiceVcfTarget(Voice& voice,
                              const EngineParameters& parameters,
                              float lfoGated) noexcept;
    [[nodiscard]] float voiceVcfTarget(
        const Voice& voice, const EngineParameters& parameters,
        float lfoGated) const noexcept;
    void updateVoiceVcaTarget(Voice& voice,
                              const EngineParameters& parameters) noexcept;
    [[nodiscard]] float voiceVcaTarget(
        const Voice& voice, const EngineParameters& parameters) const noexcept;
    // The velocity extension's one gain. The modelled hardware has no velocity
    // input at all, so `velocityDepth` is 0 by default and this is identically
    // 1.0f -- a multiply by exactly one, which leaves the faithful render bit
    // for bit where it was. When a player turns it up it scales both places a
    // note's dynamics reach: the amplifier control and the envelope's own
    // amount into the filter.
    [[nodiscard]] static float velocityGain(
        const EngineParameters& parameters, const Voice& voice) noexcept;
    void performConverterWrite(const ConverterWrite& write,
                               const EngineParameters& parameters,
                               float lfoGated,
                               const float* passiveHoldTargetOverride = nullptr) noexcept;
    [[nodiscard]] static bool isPassiveHoldWrite(
        const ConverterWrite& write) noexcept;
    [[nodiscard]] float passiveHoldWriteTarget(
        const ConverterWrite& write, const EngineParameters& parameters,
        float lfoGated) const noexcept;
    [[nodiscard]] bool latchUpcomingPassiveHoldEvent(
        double phase, double phasePerInternalSample,
        const EngineParameters& parameters) noexcept;
    void scheduleUpcomingDcoPitchPrestages(
        double phase, double phasePerInternalSample) noexcept;
    // Shared converter destinations are computed once per pass. Their proven
    // ownership is modelled; their individual RC constants and physical write
    // offsets are not yet known.
    void updateSharedScan(const EngineParameters& parameters) noexcept;
    // Called at the internal sample rate: turns continuously slewed analogue
    // control voltages into filter and amplifier coefficients without making
    // their bandwidth depend on the HQ factor.
    void updateVoiceAudio(Voice& voice, const EngineParameters& parameters) noexcept;
    // The two per-card tolerance transforms updateVoiceAudio applies to the
    // settled control voltages are also what renderVoice must reapply to the
    // interior nodes of an exact held-interval reconstruction, so both call
    // through here rather than risk the two paths drifting apart.
    [[nodiscard]] static float resonanceFeedbackFor(
        float resonanceCv, const VoiceCard& card, float calibration,
        bool circuitDerivedShape) noexcept;
    [[nodiscard]] static float cutoffAnalogCounts(
        float cutoffCounts, const VoiceCard& card, float calibration,
        float powerSupplyDroop) noexcept;
    // The ramp charging-current tolerance updatePulseComparator solves the
    // comparator threshold against is the identical per-card scale
    // renderVoice applies to the rendered ramp amplitude; shared here so the
    // two cannot drift apart the way resonanceFeedbackFor/cutoffAnalogCounts
    // above were split out to prevent.
    [[nodiscard]] static float rampCurrentScaleFor(
        const VoiceCard& card, float calibration) noexcept;
    [[nodiscard]] float dcoLaunchScale(const Voice& voice) const noexcept;
    // The PWM comparator is physical and free-running even behind a shut VCA,
    // so it follows the shared held threshold for inactive cards as well.
    void updatePulseComparator(Voice& voice,
                               const EngineParameters& parameters) noexcept;
    [[nodiscard]] static bool pulseMixEnabled(bool requested,
                                              float duty) noexcept;
    void updateSharedHighPass(const EngineParameters& parameters) noexcept;
    struct VoiceFilterFrame
    {
        float input {};
        float omegaStep {};
        float headroom {};
        OtaCascade::ControlTrajectory trajectory {};
        bool hasTrajectory { false };
        bool needsFilter { false };
    };
    [[nodiscard]] VoiceFilterFrame prepareVoiceFilter(
        Voice& voice, const EngineParameters& parameters,
        float noiseSample) noexcept;
    [[nodiscard]] float finishVoiceFilter(Voice& voice,
                                          float filtered) noexcept;
    template <bool useCubicEarly = false>
    float renderVoice(Voice& voice, const EngineParameters& parameters,
                      float noiseSample) noexcept;
#if defined(YOUKNOW106_HAS_VCF_PAIR_SIMD)
    [[nodiscard]] std::array<float, 2> renderVoicePair(
        Voice& first, Voice& second, const EngineParameters& parameters,
        float noiseSample) noexcept;
    [[nodiscard]] std::array<float, 4> renderVoiceQuad(
        const std::array<Voice*, 4>& voices,
        const EngineParameters& parameters, float noiseSample) noexcept;
#endif
    // The cheap advance a retired physical card takes under the fast VCF
    // tanh modes: exactly the free-running state a reassignment can hear --
    // DCO PIT/ramp state, sub-divider level, per-cycle render scale, card noise --
    // with none of the reconstruction, comparator or filter work whose
    // output is discarded behind the shut VCA. See Voice::freewheeling.
    void freewheelVoiceCard(Voice& voice) noexcept;
    void advanceLfo(const EngineParameters& parameters) noexcept;
    void advanceLfoDelay(const EngineParameters& parameters) noexcept;
    void updateVoiceCardDrift(VoiceCard& card) noexcept;
    // The factor the engine would actually run for a requested rung at the
    // current host rate: the request, sanitised, capped by the deepest rung
    // that rate still needs to reach the bandlimiting target.
    [[nodiscard]] int effectiveOversampleFactor(int requestedFactor) const noexcept;
    void updateProcessingRate(bool preserveFreeRunningState = false) noexcept;
    void rebuildRateDependentVoiceState() noexcept;
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
    // A glide needs somewhere to start. The first valid, positive-length
    // render after a reset takes the panel as it stands rather than sliding up
    // to it, or a startup snapshot would fade in when the transport rolled.
    // That same one-shot boundary owns startup hold priming: repeated snapshots
    // may replace the reset image until this flag is set, but later silence or
    // panic must not turn an ordinary panel edit into a direct converter write.
    // The historic name is retained because executable-local audit probes read
    // this private state as the Volume glide's first-render marker.
    bool panelGlidePrimed_ { false };
    // Resolved when a sanitized snapshot is accepted. It occupies existing
    // padding before sampleRate_, leaving all later member offsets unchanged.
    bool useCubicEarly_ { false };
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    double oversampledRate_ { 192000.0 };
    float inverseOversampledRate_ { 1.0f / 192000.0f };
    ProcessingCoefficients processingCoefficients_ {};
    // Holds both discrete noise sources' power density constant with rate, so a
    // quality change does not move the level a listener hears. It deliberately
    // does NOT equalise their total power: a shallower grid carries less
    // bandwidth, and the shaped rail's total RMS is therefore ~0.9 dB lower at
    // 1x than at 4x. Nearly all of that sits above 8 kHz -- the 20 Hz-2 kHz
    // band holds to within 0.5 dB, which is what
    // testMainNoiseDensityIsProcessingRateInvariant guards. Equalising total
    // power instead was tried and rejected: it buys the inaudible top octave
    // back by pushing the audible band 0.65 dB the wrong way.
    float noiseRateScale_ { 1.0f };
    int oversampling_ { 4 };
    // Applied and requested rungs of the quality ladder, as factors. They differ
    // only while a deferred change waits for the instrument to fall idle.
    int oversamplingApplied_ { maximumOversampleFactor };
    int oversamplingRequested_ { maximumOversampleFactor };
    // How long the voices have been silent, and how long the output path needs
    // to run dry once they are: the delay lines' longest setting plus the
    // decimation and filter stages' group delay.
    static constexpr double outputPathQuietSeconds = 0.04;
    int oversamplingIdleSamples_ { 0 };
    int oversamplingQuietSamples_ { 1 };
    // A processing-rate change has to rebuild rate-dependent filters and BBD
    // lines. A short output fade brackets that reset even when the path only
    // carries an unobservable tail; this keeps the transition independent of
    // host block size and of a simultaneous Chorus switch. A newly sounding
    // voice reverses a pending fade instead of being muted to finish a quality
    // change. The host-rate output capacitors are deliberately not rebuilt.
    enum class RateTransition { Idle, FadingOut, FadingIn };
    static constexpr double rateTransitionSeconds = 0.005;
    RateTransition rateTransition_ { RateTransition::Idle };
    float rateTransitionGain_ { 1.0f };
    float rateTransitionStep_ { 1.0f };
    // IC35 and TP5 are one clock source shared by all six PITs. This is the
    // wall time to its next rising edge expressed in periods of the currently
    // selected range clock. During a RANGE handoff it can exceed one: the old
    // synchronous count must finish before the new modulus begins.
    double rangeClockClocksToEdge_ { 1.0 };
    bool rangeClockTransitionPending_ { false };
    // Fractional pass scheduler. Keeping phase in passes avoids truncating
    // 4.2 ms to a whole internal-sample count at arbitrary host rates.
    double controlScanPhase_ { 1.0 };
    // The B-2 and service chart establish order and that writes are sequential,
    // not exact physical offsets. The default normalized profile prevents the
    // six DCOs from being falsely reset on one sample; a measured profile can
    // replace it without changing destination ownership or queue order.
    ConverterTimingProfile converterTimingProfile_ {
        ConverterTimingProfile::NormalizedServiceChart };
    std::array<double, converterWritesPerPass> converterEventPhases_ {};
    std::size_t nextConverterWrite_ { 0 };
    // Delayed float path for VCF and extension-voice scans. DCO pitch uses its
    // exact integer word. PWM has its own exact FF4F-derived DAC code, computed
    // beside the late-loop LFO update and held until the next PWM converter
    // write so a host edit cannot splice two firmware passes together.
    float converterPassLfoGated_ { 0.0f };
    std::uint16_t converterPassPwmDacCode_ { 0x0fffu };
    PassiveHoldEventLatch passiveHoldEventLatch_ {};
    VcfHoldInterval resonanceVcfHoldInterval_ {};
    std::array<VcfHoldInterval, maxVoices> cutoffVcfHoldIntervals_ {};
    std::array<bool, maxVoices> exactVcfControlInterval_ {};
    // Precomputed 10^(aging * drift / 20) so the per-sample noise mix never
    // pays a pow; exactly 1 at aging zero.
    float agedNoiseGain_ { 1.0f };
    bool assignmentRescanPending_ { false };
    bool assignmentRescanPassArmed_ { false };
    // A mutable plug-in voice count has no hardware equivalent. If a Unison
    // stack is widened during a rescan, newly admitted slots adopt the old
    // stack's glide position rather than joining from unrelated CPU history.
    std::array<bool, maxVoices> rescanPreviousUnisonMembers_ {};
    bool rescanUnisonMidiValid_ { false };
    float rescanUnisonMidi_ { 60.0f };
    bool prepared_ { false };
    bool anyVoiceActive_ { false };
    int activeVoiceCount_ { 0 };
    std::uint64_t generation_ { 0 };

    // One shared free-running modulator, exactly as the hardware has. Because
    // it is shared, six voices vibrato together rather than smearing. It is
    // not a phase accumulator: the supplied hash-matched B-2 code adds a
    // rate coefficient to a
    // clamped integer accumulator and flips direction on the clamp, and a
    // polarity bit halves that into a bipolar triangle. The clamp discards
    // whatever the last step overshot by, which quantises fast settings onto
    // a coarse grid of rates. The mechanism is kept separate from the still-
    // recovered coefficient law documented in OQ-13.
    std::uint16_t lfoAccumulator_ { 0u }; // 0..0x1fff
    bool lfoRising_ { true };
    float lfoPolarity_ { 1.0f };
    float lfoValue_ { 0.0f };
    float lfoDelayLevel_ { 0.0f };
    std::uint8_t lfoDelayByte_ { 0u };
    // The modulator, its delay envelope and the note generators all advance on
    // the converter scan, so the modulator's output is a staircase at that rate
    // rather than a continuous triangle.
    std::uint32_t lfoDelayHoldoff_ { 0u }; // 0..0x4000
    std::uint32_t lfoDelayFade_ { 0u };    // 0..0x10000
    bool anyKeyDown_ { false };
    // The resonance control voltage: one converter output shared by every
    // voice's regeneration amplifier, quantised to the panel byte and slewed
    // on its own hold capacitor like the rest of the scanned points.
    float resonanceCvTarget_ { 0.0f };
    float resonanceCv_ { 0.0f };

    // The service timing chart routes exactly one hold each to PWM, sub level
    // and noise level. Every voice card consumes these shared voltages, while
    // its downstream comparator and level errors remain card-specific.
    // The PWM hold crosses two smoothing poles on its way to the comparators
    // -- R117/C62, then R116/C63 around IC17a -- so it carries the R117/C62
    // node as a second continuous state between the target and the value the
    // cards see.
    float pwmVoltsTarget_ { 6.0f };
    double pwmVoltsFirstPole_ { 6.0 };
    double pwmVolts_ { 6.0 };
    float subCvTarget_ { 0.0f };
    double subCv_ { 0.0 };
    float noiseCvTarget_ { 0.0f };
    float noiseCv_ { 0.0f };

    // One noise generator serves every voice, so noise sums coherently as more
    // keys are held instead of staying at a fixed level. Its own support
    // circuit band-shapes it before the NOISE rail: C42 into the BA662's
    // 4.7 kOhm input bias makes a 33.9 Hz high-pass ahead of the level OTA,
    // and C41 against R79 loads the OTA's output with a 4.82 kHz pole. Both
    // run at the internal rate; their states are physical node voltages, so
    // they survive a quality change like the coupling capacitors do.
    std::uint32_t noiseState_ { 0x6d2b79f5u };
    HighPass noiseSourceHighPass_;
    HighPass noiseSourceLowPass_;
    float noiseSourceHighPassG_ { 0.01f };
    float noiseSourceLowPassG_ { 0.1f };

    // The lever is read by the converter, not wired to the voices: its value
    // is sampled once per scan pass, quantised to the converter's byte, and
    // whatever smoothing the player hears is the hold capacitors' own. A fast
    // flick therefore steps at the scan rate, as the hardware's does.
    float pitchBendTarget_ { 0.0f };
    float pitchBend_ { 0.0f };
    std::int32_t dcoPitchBendWord_ { 0 };
    std::int32_t dcoLfoPitchWord_ { 0 };
    float modWheelTarget_ { 0.0f };
    bool sustainPedalDown_ { false };

    float outputSlewStateLeft_ { 0.0f };
    float outputSlewStateRight_ { 0.0f };

    float displayEnvelope_ { 0.0f };
    float displayLfo_ { 0.0f };
    int displayVoiceMask_ { 0 };

    std::array<float, 128> heldNoteVelocities_ {};
    std::array<std::uint16_t, 128> heldNoteCounts_ {};

    std::array<Voice, maxVoices> voices_ {};
    std::array<VoiceCard, maxVoices> cards_ {};
    HalfbandDecimator firstDecimator_ {};
    HalfbandDecimator secondDecimator_ {};
    std::array<float, halfbandTaps> halfbandKernel_ {};
    // The half-band kernel is analytically zero at every even non-centre tap,
    // so 46 of its 95 entries contribute nothing. `downsamplePair` used to walk
    // all 95 and skip those with a per-tap branch, paying 95 loads and 95
    // compares to perform 49 stereo multiply-accumulates. The active taps are
    // compacted here once, at the same point the kernel is built, so the hot
    // loop visits only the 49 that matter.
    //
    // The retained coefficients are exactly symmetric after float
    // normalisation. downsamplePair therefore accumulates mirrored samples as
    // pairs: the transfer and group delay are unchanged, while float addition
    // is associated differently from the original ascending-tap loop.
    struct HalfbandActiveTap
    {
        float coefficient { 0.0f };
        int tap { 0 };   // ordinal in the full kernel, for the ring offset
    };
    std::array<HalfbandActiveTap, halfbandTaps> halfbandActiveTaps_ {};
    int halfbandActiveTapCount_ { 0 };
    std::array<float, latencyPadRingSize> latencyPadLeft_ {};
    std::array<float, latencyPadRingSize> latencyPadRight_ {};
    int latencyPadWriteIndex_ { 0 };
    int latencyPadSamples_ { 0 };

    int driftControlCountdown_ { 0 };

    Chorus chorus_ {};

    // C14/R39 AC-couple the summed voices before the switched high-pass. Both
    // networks occur once on the jack board rather than once per voice, which
    // is why they live here and carry no per-voice dispersion.
    HighPass voiceBusCoupling_ {};
    float voiceBusCouplingG_ { 0.0001f };
    // Shared by all six cards: one part number, one nominal corner. The state
    // is per voice because each card has its own capacitor.
    float moduleCouplingG_ { 0.0001f };
    // C59/VR27, between each card's filter output and its own amplifier. Same
    // arrangement as the module input above: one nominal corner, one state per
    // voice, because each card carries its own capacitor.
    float vcaInputCouplingG_ { 0.0001f };
    HighPass highPass_ {};
    float highPassG_ { 0.01f };
    float highPassShelf_ { 1.0f };
    float highPassHigh_ { 1.0f };

    // C12/R36 immediately before the shared uPC1252H2 VCA.
    HighPass commonVcaInputCoupling_ {};
    float commonVcaInputCouplingG_ { 0.0001f };

    // One independent C17/C20 charge state per IC6 output. Its coefficient and
    // observed gain follow the glided pot position and fixed internal wiper
    // load, but the capacitor itself stays continuous when Volume moves.
    HighPass outputCouplingLeft_ {};
    HighPass outputCouplingRight_ {};
    float outputCouplingG_ { 0.0001f };

    // VCA LEVEL controls the single jack-board VCA after the six voice cards
    // and shared HPF. It is not part of each voice's envelope VCA.
    float sharedVcaTarget_ { 0.0f };
    double sharedVca_ { 0.0 };

    // Deterministic physical circuit state: voice card thermal warmup timer (s)
    // and power supply rail droop (V) under heavy polyphonic loading.
    //
    // Double precision, for the same reason `HighPass::state` is: the timer is
    // advanced once per *internal* sample, so its increment is 5.208e-6 s at a
    // 192 kHz internal rate. A float total passing 128 s carries a 1.526e-5 s
    // ULP -- three times that increment -- and every further addition rounds
    // away, freezing the clock at 128.0 s and the modelled chassis at 26.99 C
    // for the rest of the session. Which power-of-two boundary caught it
    // depended on the internal rate, so the quality switch moved the modelled
    // physics: at a 48 kHz internal rate the freeze was at 512.0 s and
    // 31.51 C, which is exactly what `voiceEnergyFollowerSeconds` above
    // forbids for the same reason. In double the increment stays
    // eight orders of magnitude above half an ULP and the 900 s law runs to
    // completion at every rate.
    double thermalWarmupSeconds_ { 0.0 };
    // 1 - exp(-t/900), advanced once per internal sample beside the timer
    // above. It is chassis-wide, so recomputing it per voice recomputed the
    // same number six times.
    float thermalWarmupFraction_ { 0.0f };
    float powerSupplyDroop_ { 0.0f };
    // Settled once per block beside the converter hold coefficients, because
    // it depends on the internal rate the pending quality switch may just have
    // changed.
    float voiceEnergyFollower_ { 0.0f };

    // The envelope generator is the one shared digital processor: ATTACK,
    // DECAY and RELEASE resolve to the same increment/multiplier for every
    // voice (see the note in updateVoiceEnvelopeAndPitch), so recomputing
    // them from the panel position on every voice's Pitch write recomputed
    // the same three answers as many times as there are sounding cards. The
    // panel position is compared for exact equality, so this memo cannot
    // return anything the piecewise law would not have recomputed; sentinels
    // outside the control's 0..1 travel force the first solve.
    float envelopeLawAttack_ { -1.0f };
    float envelopeLawDecay_ { -1.0f };
    float envelopeLawRelease_ { -1.0f };
    std::uint16_t envelopeLawAttackIncrement_ { 0 };
    std::uint16_t envelopeLawDecayMultiplier_ { 0 };
    std::uint16_t envelopeLawReleaseMultiplier_ { 0 };

    // The glide law is the same shared-processor story: glideStepPerScan()
    // resolves PORTAMENTO's panel position through one eight-bit ADC lookup
    // that is identical for every voice, but both initialiseVoice() and
    // updateVoiceEnvelopeAndPitch() called it fresh on every voice's note-on
    // and Pitch write. resolveGlideStepPerScan() memoizes it the same way the
    // envelopeLaw* cache above memoizes ATTACK/DECAY/RELEASE: comparison is
    // exact equality against the same parameters.portamento source, so the
    // memo can never return anything the unconditional call would not have.
    float glideLawPortamento_ { -1.0f };
    float glideLawStepPerScan_ { 0.0f };
};

} // namespace youknow106
