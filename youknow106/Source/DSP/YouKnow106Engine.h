#pragma once

#include "YouKnow106Chorus.h"

#include <array>
#include <cmath>
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
    // Noise 1 preserves the MN3009-derived mode-I line floor and the reported
    // approximate II-I lift; its absolute installed-unit PSD, stereo
    // correlation and parasitic layers remain OQ-03 rather than a fully
    // calibrated level.
    float velocityDepth { 0.0f };  // The hardware ignores MIDI velocity.
    // Exposed to the host as Unit Character: one master over every modelled
    // component tolerance, trimmer residual, thermal wander and optional
    // circuit non-linearity -- the IR3109 stage offsets and integrating-
    // capacitor spread, the chorus clock law, the spatial thermal gradient,
    // C14's voltage-dependent capacitance and the VCF Early effect. Every
    // optional physical-circuit mechanism answers to this one control.
    //
    // Mechanisms the *nominal* circuit has are deliberately not on it: the
    // output summer's supply rails and the passive mixer's resistor loading
    // apply at every setting, because a freshly calibrated instrument has them
    // too.
    //
    // Zero is the calibrated nominal model -- no spread, no drift, none of the
    // optional non-linear shapes leaning in -- and one models the complete,
    // real-hardware-accurate span. Neither end is a claim that any real
    // instrument sits exactly there (no qualifying post-calibration residual
    // data exists to describe a real population, OQ-10); one is simply the
    // declared "matches real hardware" reference. The shipped default is 1.0.
    //
    // Bounded at 2. Every mechanism is written as
    // nominal + (physical - nominal) * calibration, which interpolates only on
    // [0, 1]; beyond that it extrapolates without limit, and several mechanisms
    // pass through zero and change sign. Values up to 2 still exaggerate while
    // every blend stays on the same side of its nominal value.
    float calibration { 1.0f };
    // Bound for the affine blends above; see the note on `calibration`.
    static constexpr float calibrationCeiling = 2.0f;
    float chorusNoise { 1.0f };    // 1.0 is the modelled BBD noise floor.
    int polyphony { 6 };           // 6 is the hardware voice count.

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
    bool enableElectrolyticC14Nonlinearity { true };
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

    // Where the transconductor's own control current stops following the
    // anti-log converter. An AS3109 teardown reports the internal control
    // current saturating at 700 uA, which is a pole near 64 kHz on this
    // circuit's C = 240 pF / R = 68 kOhm test condition -- the physical origin
    // of the upper knee, and consistent with Roland's published 50 kHz top.
    //
    // The shape is the generalized algebraic clip the output summer and the
    // BBD write already use: numerically linear through the whole musical
    // range and bending only as the current approaches its limit. The exponent
    // is the one free parameter, fitted to a measured code-to-frequency curve
    // for a real voice card; a single pole (the exponent at one) cannot
    // describe that knee, and the revision that used one left the model up to
    // 143 cents flat around a 16 kHz cutoff.
    //
    // Like the output summer's rails this is a property of the part, so it
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
        PhaseZeroDiagnostic
    };
    // NormalizedServiceChart is an explicit compatibility/product profile: it
    // preserves the chart's sequential writes across one pass without claiming
    // exact physical timestamps. PhaseZeroDiagnostic is the minimal-evidence
    // comparison in which only ordinal order remains.
    [[nodiscard]] static std::array<double, converterWritesPerPass>
        converterEventPhases(ConverterTimingProfile profile) noexcept;

    // Output calibration is a product convention, not a JUNO-106 voltage.
    // One internal unit is still the established 2.6 V model coordinate used
    // to drive the chorus. Choosing this provisional reference makes the new
    // -18 dBFS RMS boundary exactly unity and therefore preserves sessions.
    static constexpr float internalVoltsPerUnit = 2.6f;
    // The chorus refers its BBD noise row to the same coordinate and has to
    // name it locally, so the two cannot be allowed to drift apart.
    static_assert(Chorus::nodeVoltsPerUnit == internalVoltsPerUnit,
                  "the chorus and the engine disagree about the node volt scale");
    static constexpr float minus18DbfsAmplitude = 0.125892541f;
    static constexpr float compatibilityOutputReferenceRmsVolts =
        internalVoltsPerUnit * minus18DbfsAmplitude;
    [[nodiscard]] static float outputReferenceGain(float referenceRmsVolts) noexcept;

    // IC6 cannot drive its output past its own supply rails. That bound is a
    // property of the part, not a tolerance, so it applies at every Unit
    // Character setting including zero.
    //
    // The shape is the generalized algebraic clip already used for the BBD
    // write, rather than a tanh. A tanh has no linear region at all: its
    // distortion rises as (V/asymptote)^2 from the first millivolt, which put
    // roughly 0.3% third harmonic on every sample at an ordinary 2.6 V node
    // swing. A TA75558S on +/-15 V rails delivering a few volts is specified
    // far below that. A high exponent keeps the stage numerically linear
    // through the levels it actually runs at and bends it only as it
    // approaches the rail, which is what the device does.
    static constexpr float outputSummerRailVolts = 13.5f;
    static constexpr float outputSummerClipExponent = 8.0f;
    [[nodiscard]] static float outputSummerClip(float value) noexcept;

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
    // Comparator threshold in volts against the modelled 12 Vpp ramp, and the
    // duty cycle it produces. Enabled PWM cannot reach 0% or 100%; the pulse-
    // off state drives the control to -0.8 V and pins the comparator high.
    [[nodiscard]] static float pwmControlVolts(float depth) noexcept;
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
    // The summed bus crosses C14 (10 uF bipolar) into R39 (33 kOhm) before
    // IC3 selects one of the four HPF legs. IC1a's output impedance and the
    // CMOS input loading are negligible against R39 at this boundary.
    [[nodiscard]] static float voiceBusCouplingCornerHz() noexcept;
    // Flat and Boost add a selected 47 kOhm virtual-ground input in parallel
    // with R39 at the C14 pole. The capacitor-selected cut legs are open there.
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

    static constexpr int maximumOversampleFactor = 4;
    static constexpr double minimumHqProcessingRate = 176400.0;
    static constexpr double maximumSupportedSampleRate = 768000.0;
    static constexpr int halfbandTaps = 95;
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
    // 64 kHz asserted against vcfControlSaturationHz below. The suites solve
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
    // Early-effect transconductance modulation inside the cascade. With
    // V_A ~ 100 V and a few hundred millivolts of collector swing at the
    // differential pair, the fractional change in g is a few parts per
    // thousand -- the 0.005 the modelling notes state. A revision used 0.08
    // here, sixteen times that, which is a signal-dependent cutoff shift large
    // enough to hear as odd-harmonic grit on every resonant sweep.
    // Half-span of the integrating capacitors' tolerance. The four 240 pF
    // parts are discrete, so nothing trims them into agreement; a few percent
    // is the ordinary class. Voiced under OQ-10, like the other card
    // dispersions -- no measured population fixes it.
    static constexpr float vcfStageCapacitorTolerance = 0.02f;
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
    // Pitch modulation budgets in cents.
    static constexpr float lfoPitchCents = 400.0f;
    static constexpr float benderPitchCents = 1200.0f;
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
        std::uint32_t divider { 4545u };
        double periodSamples { 100.0 };
        double phase { 0.0 };
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

        void reset() noexcept;
    };

    // Four transconductor stages plus the inverting resonance return. The
    // physical continuous-time equations are advanced directly with two
    // half-interval, five-stage Merson steps. A causal current-plus-three-past
    // reconstruction supplies the input at every stage abscissa without
    // lookahead.
    struct OtaCascade
    {
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
        void retime(float previousOmegaStep,
                    float nextOmegaStep) noexcept;
        float process(float input, float omegaStep, float feedback,
                      float headroom = otaHeadroomVolts,
                      bool enableEarlyEffect = true,
                      float calibration = 0.70f) noexcept;

        [[nodiscard]] static double reconstructInput(
            double current, const std::array<double, 3>& history,
            double intervalPosition) noexcept;
        [[nodiscard]] static double clampOmegaStep(double value) noexcept;
    };

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
        // The voice CPU requests an oscillator restart only for a different
        // pitch on a voice whose key/sustain run bits are both clear. The
        // restart is consumed when this voice's turn in the converter scan
        // arrives, never synchronously at the host MIDI event.
        bool dcoResetPending { true };
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
        float vcaControl { 0.0f };
        // Oscillator compensation CV, kept in the frequency it stands for.
        // The timer's count steps instantly; this voltage slews, and the
        // ratio of the two is the momentary amplitude error a pitch step
        // leaves on the ramp -- expressed as the slope of each rise, frozen
        // per cycle (`Dco::renderScale`). It reaches the pulse only through
        // the comparator's edge times.
        float dcoCvTarget { 261.6f };
        float dcoCv { 261.6f };
        std::uint16_t attackIncrement { envelopePeak };
        std::uint16_t decayMultiplier { 0x8000u };
        std::uint16_t releaseMultiplier { 0x8000u };
        float feedback { 0.0f };
        float inputCompensation { 1.0f };
        float vca { 0.0f };
        float pulseDuty { 0.5f };
        // PWM is a moving comparator threshold, not a pulse oscillator whose
        // edge position is frozen for one sample.  Retaining the previous
        // threshold lets renderVoice solve crossings caused by both the ramp
        // and the slewing hold voltage; without it, deep PWM can skip an edge
        // and leave a full extra pulse cycle in the output.
        float previousPulseDuty { 0.5f };
        bool pulseDutyPrimed { false };

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
    // `height` is a value discontinuity (the comparator and divider edges);
    // `slopeStep` is a per-sample slope discontinuity, which is what the
    // integrator's finite-slope reset is at each of its two corners.
    // `samplesAgo` locates the event inside the sample just rendered.
    void addStep(BandlimitedTrack& track, float height,
                 float samplesAgo) const noexcept;
    void addSlope(BandlimitedTrack& track, float slopeStep,
                  float samplesAgo) const noexcept;
    // A voice-CPU pitch write restarts the timer/ramp at the beginning of the
    // current internal interval. Unlike a hard engine reset, an audible
    // releasing card retains its delayed naive history and residual tails.
    void restartDcoBandlimited(Voice& voice,
                               double previousPeriodSamples) noexcept;
    // Fraction of the ramp's full excursion consumed by the finite-slope reset
    // at a given period, clamped so a very high note cannot invert the ramp.
    static float resetFraction(double periodSeconds) noexcept;

    void buildHalfbandKernel() noexcept;
    [[nodiscard]] static const CorrectionTables& correctionTables() noexcept;
    void buildVoiceCards() noexcept;
    // Copies each card's IR3109 per-stage trims -- input offset voltage and
    // integrating-capacitor tolerance -- into its voice, scaled by Unit
    // Character. Both are fixed properties of a card and the amount only moves
    // when the panel does, so this is called where those change, not from the
    // audio path.
    void refreshVoiceCardStageTrims() noexcept;
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
    void updateVoiceScan(Voice& voice, const EngineParameters& parameters,
                         float lfoGated) noexcept;
    void updateVoiceEnvelopeAndPitch(Voice& voice,
                                     const EngineParameters& parameters,
                                     float lfoGated) noexcept;
    void updateVoiceVcfTarget(Voice& voice,
                              const EngineParameters& parameters,
                              float lfoGated) noexcept;
    void updateVoiceVcaTarget(Voice& voice,
                              const EngineParameters& parameters) noexcept;
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
                               float lfoGated) noexcept;
    // Shared converter destinations are computed once per pass. Their proven
    // ownership is modelled; their individual RC constants and physical write
    // offsets are not yet known.
    void updateSharedScan(const EngineParameters& parameters,
                          float lfoGated) noexcept;
    // Called at the internal sample rate: turns continuously slewed analogue
    // control voltages into filter and amplifier coefficients without making
    // their bandwidth depend on the HQ factor.
    void updateVoiceAudio(Voice& voice, const EngineParameters& parameters) noexcept;
    [[nodiscard]] static float dcoCompensationRatio(const Voice& voice) noexcept;
    [[nodiscard]] float dcoLaunchScale(const Voice& voice) const noexcept;
    // The PWM comparator is physical and free-running even behind a shut VCA,
    // so it follows the shared held threshold for inactive cards as well.
    void updatePulseComparator(Voice& voice,
                               const EngineParameters& parameters) noexcept;
    [[nodiscard]] static bool pulseMixEnabled(bool requested,
                                              float duty) noexcept;
    void updateSharedHighPass(const EngineParameters& parameters) noexcept;
    float renderVoice(Voice& voice, const EngineParameters& parameters,
                      float noiseSample) noexcept;
    void advanceLfo(const EngineParameters& parameters) noexcept;
    void updateVoiceCardDrift(VoiceCard& card) noexcept;
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
    // A glide needs somewhere to start. The first render after a reset takes
    // the panel as it stands rather than sliding up to it, or a patch loaded
    // while stopped would fade in when the transport rolled.
    bool panelGlidePrimed_ { false };
    double sampleRate_ { 48000.0 };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    double oversampledRate_ { 192000.0 };
    float inverseOversampledRate_ { 1.0f / 192000.0f };
    float noiseRateScale_ { 1.0f };
    int oversampling_ { 4 };
    bool oversamplingEnabled_ { true };
    bool oversamplingRequested_ { true };
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
    // Fractional pass scheduler. Keeping phase in passes avoids truncating
    // 4.2 ms to a whole internal-sample count at arbitrary host rates.
    double controlScanPhase_ { 1.0 };
    // The B-2 and service chart establish order and that writes are sequential,
    // not exact physical offsets. The default normalized profile prevents the
    // six DCOs from being falsely reset on one sample; a measured profile can
    // replace it without changing destination ownership or queue order.
    std::array<double, converterWritesPerPass> converterEventPhases_ {};
    std::size_t nextConverterWrite_ { 0 };
    float converterPassLfoGated_ { 0.0f };
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
    float pwmVoltsFirstPole_ { 6.0f };
    float pwmVolts_ { 6.0f };
    float subCvTarget_ { 0.0f };
    float subCv_ { 0.0f };
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
    float modWheelTarget_ { 0.0f };
    float modWheel_ { 0.0f };
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
    float sharedVca_ { 0.0f };

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

};

} // namespace youknow106
