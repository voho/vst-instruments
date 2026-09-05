#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace youknow
{

// Generalized algebraic soft clip: exactly linear as the normalised magnitude
// approaches zero and bending only as it approaches 1. The engine's VCF
// saturation uses this denominator directly, and the output summer's fixed
// exponent-eight specialization uses equivalent multiplies and roots. The BBD
// write uses the constrained quadratic variant documented in Chorus.cpp.
// This file already builds standalone, so the shared two-term primitive sits
// here.
// Evaluated in double so an extreme finite float still approaches the bound
// instead of overflowing an intermediate power and folding back to zero.
[[nodiscard]] inline double algebraicSoftClipDenominator(
    double normalisedMagnitude, double exponent) noexcept
{
    return std::pow(1.0 + std::pow(normalisedMagnitude, exponent),
                     1.0 / exponent);
}

// Panel state of the chorus selection.
//
// The original instrument's owner manual and recovered control path describe
// one enable bit plus one binary I/II bit, not a third analogue clock setting.
// Keep the user-requested physical both-button combination as a fourth product
// state; `settingsFor()` documents its compatibility policy while the exact
// original-unit transfer remains unmeasured. The audited Roland Cloud
// JUNO-106 exposes only Off/I/II and therefore supplies no I+II precedent.
enum class ChorusMode { Off, One, Two, OneTwo };

// The four parameter states map one-to-one to the four button combinations.
[[nodiscard]] constexpr ChorusMode chorusModeFor(bool one, bool two) noexcept
{
    if (one && two)
        return ChorusMode::OneTwo;
    if (two)
        return ChorusMode::Two;
    if (one)
        return ChorusMode::One;
    return ChorusMode::Off;
}

[[nodiscard]] constexpr bool chorusOneEngaged(ChorusMode mode) noexcept
{
    return mode == ChorusMode::One || mode == ChorusMode::OneTwo;
}

[[nodiscard]] constexpr bool chorusTwoEngaged(ChorusMode mode) noexcept
{
    return mode == ChorusMode::Two || mode == ChorusMode::OneTwo;
}

// Two 256-stage bucket-brigade delay lines with anti-phase clock modulation,
// as used by the modelled instrument's stereo chorus. There is deliberately no
// compander: the hiss the line adds is part of the effect's signature, so
// removing it would be a different effect.
//
// Everything here is JUCE-free so the regression suites can drive it directly
// and compare it against an independently integrated reference.
class Chorus
{
public:
    // MN3009-class line: 256 stages, two-phase clocked, so one full delay is
    // stages / (2 * clock). At the 10 kHz minimum clock that is the part's
    // quoted 12.8 ms maximum.
    static constexpr int stages = 256;
    static constexpr int cellPairs = stages / 2;

    // Clock frequencies the modelled analogue front end can reach. The line
    // aliases everything above half the clock, so the model refuses to run it
    // outside the part's rated window.
    static constexpr float minimumClockHz = 10000.0f;
    static constexpr float maximumClockHz = 200000.0f;
    // Lowest host rate the engine accepts. The fastest clock against it is the
    // most edges a single sample can ever have to consume.
    static constexpr float minimumSampleRate = 8000.0f;
    // The shipping HQ policy targets at least 176.4 kHz internally. At and
    // above that grid the input support uses the combined exact transition;
    // lower grids retain the reviewed TPT anti-alias path because an exact
    // cubic drive there worsens the independently measured SGA boundary.
    static constexpr float minimumExactInputSupportRate = 176400.0f;
    static constexpr int maximumShiftsPerSample =
        static_cast<int>(maximumClockHz / minimumSampleRate) + 2;

    // The output polyBLEP has compact support over two numerical samples. At
    // the declared 200 kHz / 8 kHz extreme that interval can contain 50 BBD
    // edges. Two shift-loop bounds leave a little explicit headroom while
    // keeping the reconstruction fixed-size and allocation-free.
    static constexpr int maximumBlepEvents = 2 * maximumShiftsPerSample;
    static_assert(maximumBlepEvents
                      >= static_cast<int>(2.0f * maximumClockHz
                                          / minimumSampleRate) + 1);
    // Future bucket values are knowable only until the write head can catch
    // them. The current support ends well before one 128-cell revolution.
    static_assert(maximumBlepEvents < cellPairs);

    // IC6 is the final inverting dry/wet summer on each output. Its 100 kOhm
    // feedback resistor (R70/R67) sees dry through 47 kOhm (R71/R73, off the
    // shared IC2b bus) and wet through 39 kOhm (R72/R74, from the Tr11/Tr12
    // mute JFETs) -- read at designator level from the Service Notes p. 15
    // scan, agreeing with both sibling-board netlists. The wet leg is the
    // hotter one. An earlier revision carried the mirror of this assignment.
    // Polarity is intentionally omitted because both paths invert together;
    // these magnitudes preserve the analogue drive into the following output
    // and VOLUME stages.
    static constexpr float dryMixGain = 100.0f / 47.0f;
    static constexpr float wetMixGain = 100.0f / 39.0f;
    static constexpr float wetToDryGain = wetMixGain / dryMixGain;
    // Explicit plug-in declick policy, not a measured mute-transistor value.
    static constexpr float wetMuteTimeConstantSeconds = 0.005f;

    // One model unit at the BBD node, in volts. This is the engine's
    // `internalVoltsPerUnit` seen from inside the chorus; the engine
    // static_asserts the two equal where it can see both, so the coordinate
    // cannot drift between the two files.
    static constexpr float nodeVoltsPerUnit = 2.6f;

    // ------------------------------------------------------------------
    // Panasonic specifies noise at the MN3009 output under fixed conditions:
    // Ta=25 C, VDD=VCPL=-15 V, VCPH=0 V, VGG=-14 V, RL=100 kOhm. The part
    // datasheet's electrical table, read at 600 dpi from the scan, gives one
    // noise row and no other:
    //
    //   Noise            Vno  fcp = 100kHz Weighted by "A" curve   Max 0.2  mVrms
    //   Signal to Noise  S/N  Maximum output voltage to noise volt. Typ 88  dB
    //
    // So 0.200 mVrms is a MAXIMUM and the part publishes NO typical noise
    // figure. The 0.150 mVrms this comment used to set against it does not
    // appear in that table at all, so the "two conflicting maxima" reading is
    // retired: there is one maximum, and the other figure belongs to some
    // other part or revision. HISS 100% keeps the 0.200 mVrms endpoint, still
    // as an upper limit and still not a measurement after Roland's external
    // tap-sum/reconstruction network.
    //
    // The same sheet's TYPICAL THD-Vi curve does carry a noise figure, by
    // implication, and it is the only typical on record. That curve is
    // U-shaped: 0.62 % at 0.3 Vrms falling to a 0.42 % minimum near 0.75 Vrms
    // and rising to about 2.2 % at 2.0 Vrms. A saturating nonlinearity cannot
    // produce the left arm, so the curve is THD+N and its low-level rise is
    // the noise floor entering the measurement. Subtracting the harmonic floor
    // in power gives 1.37, 1.29 and 1.05 mVrms at 0.3, 0.4 and 0.5 Vrms --
    // consistent across three readings, so roughly 1.1-1.4 mVrms unweighted
    // and wideband at the 40 kHz clock the curve is taken at. That is not in
    // conflict with a 0.2 mVrms A-weighted maximum at 100 kHz once weighting
    // and clock rate are accounted for, and it is derived from a small
    // log-log plot, so it is a bracket for OQ-03 rather than an anchor. It is
    // recorded here because the model separates the write nonlinearity from
    // the line noise, and this is the evidence that separation is right.
    //
    // The curve's right arm confirms both anchors bbdTransferBase is fitted
    // to: 0.3 % typ at 0.78 Vrms from the table, about 2.2 % at 2.0 Vrms here.
    // Insertion loss is Min -4 / Typ 0 / Max +4 dB, a real unit-to-unit spread
    // on wet level the model does not carry.
    // https://www.ka-electronics.com/images/pdf/Panasonic_BBD.pdf
    static constexpr float mn3009OutputNoiseAWeightedMaximumVrms = 0.200e-3f;

    // Session-compatible product policy. The old rationale combined a 1.5 V
    // guaranteed *input* swing with an 88 dB typical *maximum-output* S/N row;
    // those are different measurands and cannot derive a 59.7 uVrms typical
    // noise voltage. Retain the established control position, but do not call
    // it a Panasonic typical endpoint. OQ-03 still needs an installed-unit
    // capture to replace it.
    static constexpr float defaultNoiseScale = 0.29858038f;

    // The established HISS-100 product normalization chooses a recovered wet
    // line of the same numerical 0.200 mVrms. That equality is explicit policy,
    // not a claim that the external board belongs inside Panasonic's
    // measurand. The constant below is the complete modelled board chain's
    // A-weighted transfer from uniform edge-noise amplitude to the wet line:
    // 1/sqrt(3) times 0.6745 for hold/tap sum/reconstruction/output coupling.
    static constexpr float productWetLineNoiseTargetAWeightedVrms =
        mn3009OutputNoiseAWeightedMaximumVrms;
    //
    // Stated at 192 kHz, which is what HQ targets from the 48 kHz host-rate
    // family and is also the engine's `noiseReferenceRateHz`. The combined
    // exact output support measures 0.3894 there; this is a derived numerical
    // transfer update, not a change to the part's 0.2 mVrms row or its noise
    // law. Across HQ the recovered result moves less than 0.004 dB between the
    // 192 and 176.4 kHz internal grids. HQ-off's separately audited folded
    // power remains numerical-grid error rather than a property of the part.
    //
    // Known to about +/-0.1%, and no better: it is estimated from a finite
    // random sequence. Fixed-seed 128 s measurements put the effective
    // transfer at 0.38948-0.38953 on the 176.4 kHz family and
    // 0.38937-0.38941 on the 192 kHz family. Four figures is all the
    // measurand supports, which is why the suites allow estimator margin on
    // the resulting product-policy assertion rather than fencing it exactly.
    static constexpr float productWetLineAWeightedTransfer = 0.3894f;

    // Amplitude of the uniform random sample each line writes at its own clock
    // edges, in model units. One equation, solved once: the recovered
    // A-weighted wet line equals the declared product target. A separate
    // fixed-100-kHz raw-node regression guards Panasonic's part-output maximum.
    static constexpr float independentLineRandomAmplitude =
        productWetLineNoiseTargetAWeightedVrms
            / (nodeVoltsPerUnit * productWetLineAWeightedTransfer);

    // A live engine-quality change alters only the numerical sample grid.
    // `preserveState` retains the BBD buckets and all free-running phases/RNGs.
    // The grid histories, output correction and support state are deliberately
    // reinitialised under the engine's zero-gain transition. The exact support
    // coordinates are physical voltages, not timestep-embedded TPT carries;
    // preserving and reseeding them is a separate, still-unqualified policy.
    // The engine calls this from prepareToPlay so a later quality change only
    // selects fixed coefficients on the audio thread instead of solving matrix
    // exponentials there.
    void prepareSupportRates(double hostSampleRate) noexcept;
    void prepare(double sampleRate,
                 bool preserveState = false) noexcept;
    void reset(bool preserveLfoPhase = false) noexcept;

    // Advances one sample. `noiseScale` is the single master for every
    // declared chorus-noise component; 1.0 preserves the compatibility hiss
    // and 0.0 removes all of them. A calibrated hardware noise reference has
    // not yet been located, so the optional common/hum/spur amplitudes are zero.
    //
    // There is deliberately no separate storage-capacitance term. The MN3009's
    // C_gs is voltage dependent, but what that produces is distortion and a
    // little level-dependent high-frequency loss -- both of which the
    // datasheet's own distortion figures already measure on the complete part,
    // and which `bbdTransfer` is fitted to. Modelling it again on top would be
    // counting the same physics twice.
    // The cheap advance for a switched-off, fully settled effect. Once the
    // wet-mute glide has decayed to exactly zero, process() with mode Off
    // returns bit-exactly dryMixGain * input whatever the muted lines hold,
    // so skipping their BBD work changes nothing the player can hear now --
    // only the line history a later engage would start from. This advances
    // the free-running LFO, marks the wet path for a from-silence rebuild on
    // that engage, writes the settled output and returns true; it returns
    // false -- caller must run process() -- until the glide has settled.
    // The engine gates this behind the fast VCF tanh modes, so Exact keeps
    // the established always-running lines.
    bool processBypassedWhenSettled(float input, float& left,
                                    float& right) noexcept;

    void process(float input, ChorusMode mode, float noiseScale,
                 float& left, float& right,
                 bool enableClockBleed = false,
                 bool enableHyperbolicSweep = false,
                 float calibration = 1.0f,
                 bool useRateProportionalNoiseHypothesis = false,
                 bool enableNarrowOneTwo = true,
                 bool enableMuteDrive = false,
                 bool enableLineGainSpread = false) noexcept;

    // ------------------------------------------------------------------
    // The wet-mute drive, jack board p. 15. The CHORUS on/off line reaches
    // Tr6, then Tr5, an open-collector switch to -15 V on a node R50 10 kOhm
    // pulls to +15 V with C16 2.2 uF to the -15 V rail. That node crosses
    // R48 150 kOhm into C13 1 uF (to -15 V), then R49 560 kOhm into Tr4's
    // base with R42 39 kOhm to its -15 V emitter. Tr4's collector, R43
    // 100 kOhm to +15 V, drives the Tr11/Tr12 2SK30A gates through D4/D5:
    // Tr4 conducting pulls the gates to the negative rail and mutes the wet
    // return; Tr4 off lets the gates float to their sources and the JFETs
    // conduct. So the button reaches the JFETs only after two RCs and a
    // junction threshold:
    //   - chorus OFF: Tr5 opens, C16 charges through R50 (22 ms), C13
    //     follows through R48 against R49+R42 (120 ms) and mutes when Tr4's
    //     base reaches one junction drop -- about 81 ms after the command;
    //   - chorus ON: Tr5 saturates, C16 is emptied at once, C13 decays from
    //     its +9.0 V rest toward -15 V and un-mutes about 115 ms in.
    // Both are derived from the drawn parts with the same 0.6 V junction
    // prior the resonance and NOISE onsets use; the JFET transition itself
    // keeps the declared 5 ms glide policy below, because the 2SK30A's
    // pinch-off spread is not fixed by any source. Off by default at this
    // level so the bare-chorus suites keep their immediate switching; the
    // engine enables it.
    static constexpr float muteDrivePullUpOhms = 10.0e3f;        // R50
    static constexpr float muteDriveNodeFarads = 2.2e-6f;        // C16
    static constexpr float muteDriveSeriesOhms = 150.0e3f;       // R48
    static constexpr float muteDriveHoldFarads = 1.0e-6f;        // C13
    static constexpr float muteDriveBaseOhms = 560.0e3f;         // R49
    static constexpr float muteDriveEmitterOhms = 39.0e3f;       // R42
    static constexpr float muteDriveRailVolts = 15.0f;
    static constexpr float muteDriveJunctionVolts = 0.6f;
    // Tr4 conducts once C13 stands one junction drop, scaled by the R49/R42
    // divider, above the negative rail: -15 + 0.6 * 599/39 = -5.785 V.
    static constexpr float muteDriveThresholdVolts =
        -muteDriveRailVolts
        + muteDriveJunctionVolts
              * (muteDriveBaseOhms + muteDriveEmitterOhms)
              / muteDriveEmitterOhms;
    // C13 sees R48 against R49+R42: 120.0 ms.
    static constexpr float muteDriveHoldSeconds =
        muteDriveHoldFarads
        * (muteDriveSeriesOhms * (muteDriveBaseOhms + muteDriveEmitterOhms)
           / (muteDriveSeriesOhms + muteDriveBaseOhms + muteDriveEmitterOhms));
    static constexpr float muteDriveNodeSeconds =
        muteDrivePullUpOhms * muteDriveNodeFarads;                // 22 ms
    // Where C13 rests for a given Tr5 node voltage: the R48 / (R49+R42)
    // divider between that node and the -15 V rail.
    [[nodiscard]] static constexpr float muteDriveHoldRestVolts(
        float nodeVolts) noexcept
    {
        const float lower = muteDriveBaseOhms + muteDriveEmitterOhms;
        return (nodeVolts * lower - muteDriveRailVolts * muteDriveSeriesOhms)
             / (lower + muteDriveSeriesOhms);
    }
    [[nodiscard]] bool muteDriveMuted() const noexcept { return muteDriveMuted_; }

    // ------------------------------------------------------------------
    // Panasonic specifies the MN3009's insertion loss as Min -4 / Typ 0 /
    // Max +4 dB (the same table the noise row is read from), and there is
    // no wet-level trim on the jack board -- VR1/VR2 set bias only -- so the
    // two lines' wet returns differ by whatever their two parts drew. The
    // absolute wet level is already a normalised policy (OQ-04), so what is
    // modelled is only the two parts' RELATIVE offset: one fixed-seed draw,
    // applied +/- half to the two lines. Its full span is voiced at the same
    // conservative fraction of the anchored bound the IR3109 stage
    // capacitors use (0.02 inside 0.05), scaled by Unit Character.
    static constexpr float lineInsertionGainBoundDb = 4.0f;
    static constexpr float lineInsertionGainSpreadDb = 1.6f;
    [[nodiscard]] static float lineInsertionGainDraw() noexcept;

    // The one usable real-instrument observation of this circuit's noise:
    // mode II's output floor was reported about 3.95 dB above mode I's, on
    // Panasonic parts and again on Xvive parts (OQ-03; the printed pairs give
    // 3.96 and 3.95 dB). The unchanged capture chain makes the relative delta
    // usable even though its absolute dBFS figures and true-peak statistic
    // cannot calibrate this model. Apply 3.95 dB empirically by default while
    // leaving mode I's explicit recovered-wet-line product normalization
    // untouched. There is no corresponding
    // calibrated I+II capture, so that product mode provisionally retains the
    // measured mode-II profile until one exists.
    static constexpr float measuredModeTwoNoiseDeltaDb = 3.95f;
    static constexpr float measuredModeTwoNoiseGain = 1.57579602f;
    [[nodiscard]] static constexpr float measuredModeNoiseGain(
        ChorusMode mode) noexcept
    {
        return chorusTwoEngaged(mode) ? measuredModeTwoNoiseGain : 1.0f;
    }

    // The settled topology gives the two modes identical sweep depth and
    // clock range -- the mode line changes the modulation *rate* and nothing
    // else -- so a noise mechanism proportional to that rate, equivalently to
    // the sweep's own slope d(delay)/dt, predicts exactly
    // 20*log10(modeRateRatio()) = 4.21 dB. This remains a causal hypothesis,
    // not the default calibration: OQ-03's same-chain capture at a third,
    // artificial rate is still needed to distinguish it from noise in the
    // mode-switch network.
    //
    // Selecting the hypothesis replaces, rather than multiplies, the empirical
    // factor. Both profiles reference mode I and therefore preserve its
    // established floor exactly.
    [[nodiscard]] static float rateProportionalNoiseGain(float rateHz) noexcept;

    // The nominal (unmodulated) delay in seconds for a clock frequency.
    [[nodiscard]] static constexpr float delaySecondsForClock(float clockHz) noexcept
    {
        return static_cast<float>(cellPairs) / clockHz;
    }
    [[nodiscard]] static constexpr float clockForDelaySeconds(float seconds) noexcept
    {
        return static_cast<float>(cellPairs) / seconds;
    }

    // ------------------------------------------------------------------
    // The modulation oscillator, from Service Notes p. 15 (jack board, IC1
    // uPC062 and IC2a M5218L).
    //
    // IC1b integrates: C3 sits across pins 6 and 7 and pin 5 is grounded.
    // IC1a compares: R6 47 kOhm returns its own output to pin 3, and R7 33 kOhm
    // brings the integrator's output to that same node, so pin 3 is a summing
    // junction weighed against the grounded pin 2 -- the classic two-resistor
    // Schmitt. (An earlier transcription placed R7 on pin 2 with R15 dividing
    // pin 3 down; `lfoThresholdRatio` below records why that reading is
    // falsified.) That is an integrator-plus-comparator pair, whose capacitor
    // is charged from a constant current for the whole of each half cycle. Its
    // output is a straight, symmetric triangle -- not the exponential segments
    // an RC relaxation oscillator would give -- and `triangle()` below is that
    // shape.
    // IC2a then inverts it once through R10/R9 33 kOhm, so the second line's
    // modulation is exactly the negative of the first's rather than an
    // independent oscillator: TP4 carries the triangle and TP3 its inverse.
    //
    // The CHORUS I/II line drives JFET Tr1 through D1/Tr2/R2. R4 680 kOhm
    // runs from the R5/R8 junction to Tr1's drain -- the mode shunt -- and R3
    // 2.2 MOhm is Tr1's gate-source bleed: conducting, Tr1 grounds the
    // junction through R4 alone; off, the leg reaches ground through R4 + R3.
    // (An earlier reading had Tr1 shorting R3 itself; the original-page pass
    // relabelled the legs without moving a single value.) R5 reaches the
    // integrator's virtual ground through R8, so both legs terminate at 0 V
    // and the charging current sees
    //
    //     R_eff = R8 * R5 / (R_sh || R8) + R8 ,
    //
    // with R_sh = R4 alone while Tr1 conducts and R4 + R3 while it does not.
    // The p. 13 mode table does not label which logic level conducts Tr1; the
    // assignment rests on the derived R_eff pair matching the measured rates,
    // with the larger R_eff -- the slower leg -- as mode I.
    static constexpr double lfoSeriesOhms = 1.0e6;         // R5
    static constexpr double lfoIntegratorOhms = 2.2e6;     // R8
    static constexpr double lfoShuntOhms = 680.0e3;        // R4
    static constexpr double lfoModeShuntOhms = 2.2e6;      // R3, in the leg only in II
    // R7 / R6. The comparator trips where its summing node crosses the
    // grounded inverting input, so the triangle spans +/- (R7/R6) of the
    // saturated output and the oscillation is f = 1 / (4 * beta * R_eff * C3).
    //
    // An earlier transcription read R7 as reaching the inverting pin instead,
    // with R15 1 kOhm dividing the non-inverting one down to beta = 1/48. That
    // reading is falsified twice over: evaluating f with it puts both modes
    // some 34x above every rate ever reported for this family, and the netlist
    // of the sister board's clone (gligli/juno-chorus-clone, the Juno-60
    // chorus board this circuit is shared with) wires 47k from the comparator
    // output and 33k from the triangle into one non-inverting summing node
    // with the inverting input grounded, and has no divider resistor on that
    // node at all. The mode *ratio* is unaffected either way; the scale below
    // is not.
    static constexpr double lfoComparatorFeedbackOhms = 47.0e3;   // R6
    static constexpr double lfoComparatorTriangleOhms = 33.0e3;   // R7
    static constexpr double lfoThresholdRatio =
        lfoComparatorTriangleOhms / lfoComparatorFeedbackOhms;

    // C3, the integrator capacitor. The 106's own page prints it in Roland's
    // bare ".1" form beside C4's "220P" -- confirmed on the original First
    // Edition p. 15 scan -- and the sister
    // board clone's netlist carries 100 nF in exactly this position, across
    // the integrator's pins 6 and 7. The derivation below corroborates the
    // value against two independent measured rate pairs: scope readings of a
    // 106-chorus clone (0.537/0.879 Hz) land within 3%, and the JUNO-60's own
    // measured pair implies timing resistances in the same family as
    // lfoTimingOhms() under the same beta and C3.
    static constexpr double lfoIntegratorFarads = 0.1e-6;

    [[nodiscard]] static constexpr double lfoTimingOhms(bool modeOne) noexcept
    {
        const double shunt = modeOne ? lfoShuntOhms
                                     : lfoShuntOhms + lfoModeShuntOhms;
        const double parallel = shunt * lfoIntegratorOhms
                              / (shunt + lfoIntegratorOhms);
        return lfoIntegratorOhms * lfoSeriesOhms / parallel + lfoIntegratorOhms;
    }

    // 6.4352941 MOhm over 3.9638889 MOhm. This ratio is the JUNO-106's own,
    // derived from its own schematic, and derivedRateHz() below sets the
    // absolute scale from the same circuit -- nothing here is borrowed any
    // more. The sibling's measured pair implies 1.682, which agrees to
    // about 3.6% -- close enough to say the two instruments share this circuit,
    // not close enough to keep using the sibling's decimals here.
    [[nodiscard]] static constexpr double modeRateRatio() noexcept
    {
        return lfoTimingOhms(true) / lfoTimingOhms(false);
    }

    // The absolute modulation rate, from this instrument's own circuit alone:
    // f = 1 / (4 * beta * R_eff * C3), which lands 0.55329 Hz for mode I and
    // 0.89826 Hz for mode II. Mode I integrates through the larger resistance
    // and is the slower leg.
    [[nodiscard]] static constexpr double derivedRateHz(bool modeOne) noexcept
    {
        return 1.0 / (4.0 * lfoThresholdRatio * lfoTimingOhms(modeOne)
                      * lfoIntegratorFarads);
    }

    // The JUNO-60's measured rates. They no longer supply this model's scale
    // -- derivedRateHz() does -- and stay only as the comparison pair the
    // suites hold the derivation against: the sibling's own 1.682 ratio must
    // stay superseded, and its scale must stay retired rather than quietly
    // drifting back in.
    static constexpr double siblingMeasuredRateOneHz = 0.513;
    static constexpr double siblingMeasuredRateTwoHz = 0.863;

    // Modulation constants per panel mode, exposed so the suites can assert
    // them against the documented figures rather than re-deriving them.
    struct ModeSettings
    {
        float rateHz { 0.0f };
        float centreDelaySeconds { 0.0f };
        float sweepSeconds { 0.0f };
        float wetGain { 0.0f };
    };

    [[nodiscard]] static ModeSettings settingsFor(ChorusMode mode) noexcept;

    // The legacy low-rate input fallback, exposed as a coefficient and a
    // single step so the suites can measure where its corner actually lands
    // rather than trusting that the two match. They agree for one pairing:
    // this coefficient belongs to this recursion and to no other.
    [[nodiscard]] static float onePoleG(float cutoffHz, float sampleRate) noexcept;
    static float supportFilterStep(float& state, float input, float g) noexcept;
    // C28/C25 couple the two reconstructed wet lines into 22 kOhm bleeds.
    // When the series JFET is on, IC6's 39 kOhm wet input loads that node too.
    [[nodiscard]] static float wetOutputCouplingCornerHz(
        bool wetConnected) noexcept;

    // One of the emitter-follower two-pole sections either side of the line.
    // Each is an equal-resistor Sallen-Key, so its Q is fixed by the ratio of
    // its two capacitors alone: Q = 0.5 * sqrt(C_feedback / C_shunt).
    struct BiquadCoefficients
    {
        float g { 0.1f };   // tan(pi * fc / fs)
        float k { 2.0f };   // 1 / Q
    };
    [[nodiscard]] static BiquadCoefficients sallenKeyCoefficients(
        float cutoffHz, float q, float sampleRate) noexcept;
    [[nodiscard]] static float sallenKeyQ(float feedbackFarads,
                                          float shuntFarads) noexcept;

    // Legacy topology-preserving input section, taken at its lowpass output.
    // Two states advance together; the exact continuous support uses physical
    // six-state coordinates instead.
    struct BiquadState
    {
        float s1 { 0.0f };
        float s2 { 0.0f };
        void reset() noexcept { s1 = 0.0f; s2 = 0.0f; }
    };
    static float biquadStep(BiquadState&, float input,
                            const BiquadCoefficients&) noexcept;

    // Every coefficient the two chains need, built once per prepare(). The
    // sections are fixed networks -- nothing on the panel reaches them -- so
    // there is nothing to recompute per sample.
    struct SupportChain
    {
        struct ExactTransition
        {
            std::array<std::array<double, 6>, 6> stateByColumn {};
            std::array<std::array<double, 6>, 4> driveBySample {};
        };

        float inputCouplingG { 0.001f };    // C44 / R120, wet path only
        float passiveG { 0.1f };            // R122 / C52, ahead of the line
        // Legacy TPT coefficients retained only for the low-rate input policy.
        // The output side is exclusively the exact continuous transition.
        BiquadCoefficients antiAliasFirst {};
        BiquadCoefficients antiAliasSecond {};
        ExactTransition exactInput {};
        ExactTransition exactOutputMuted {};
        ExactTransition exactOutputConnected {};
    };
    [[nodiscard]] static SupportChain supportChainFor(float sampleRate) noexcept;

    [[nodiscard]] double getLfoPhase() const noexcept { return lfoPhase_; }

private:
    friend struct YouKnowTestAccess;
    void advanceMuteDrive(bool commandMute) noexcept;

    // OQ-03 keeps the compatibility hiss and the still-unknown mechanisms as
    // distinct components.  Every number in this profile is voiced/unknown,
    // not a JUNO-106 measurement.  The optional amplitudes therefore default
    // to zero; `noiseScale` in process() remains the one master applied to all
    // of them when future evidence supplies defensible values.
    struct OptionalNoiseComponents
    {
        float commonRandomAmplitude { 0.0f };
        // Desired stereo correlation of the synthetic common layer.  The
        // default is a voiced placeholder and is inaudible while its amplitude
        // is zero.
        float commonRandomCorrelation { 1.0f };
        float humAmplitude { 0.0f };
        // A voiced placeholder for deterministic tests only: mains frequency
        // is market/unit dependent and has not been measured at this node.
        float humFrequencyHz { 50.0f };
        float clockSpurAmplitude { 0.0f };
        // The spur follows each modulated BBD clock.  Which harmonic dominates
        // is unknown, so unity is only a disabled, voiced placeholder.
        float clockSpurHarmonic { 1.0f };
    };

    struct StereoNoiseSample
    {
        float lineA { 0.0f };
        float lineB { 0.0f };
    };

    // Pure deterministic component steps.  They are private implementation
    // details, with a narrow friend seam for regression fixtures.
    [[nodiscard]] static StereoNoiseSample correlatedRandomStep(
        std::uint32_t& commonState, std::uint32_t& orthogonalState,
        float correlation) noexcept;
    [[nodiscard]] static float deterministicToneStep(
        double& phase, float frequencyHz, float sampleRate) noexcept;

    // The two parasitic mechanisms inside the BBD, kept as separate pure
    // steps so the circuit suite can check them against the part's distortion
    // and frequency-response figures without the support filters obscuring the
    // result.
    [[nodiscard]] static float bbdTransfer(float input) noexcept;
    static float transferLossStep(float& state, float input) noexcept;
    // Reconstruct the already support-filtered input at an asynchronous BBD
    // edge from the current and three preceding numerical samples.  The edge
    // lies inside the completed previous-to-current interval, so this causal
    // four-point Lagrange interpolation needs neither lookahead nor latency.
    [[nodiscard]] static float interpolateBbdInput(
        float current, float previous, float previous2, float previous3,
        double ageInSamples) noexcept;
    [[nodiscard]] static double bbdPolyBlepResidual(
        double distanceInSamples) noexcept;

    // One bucket-brigade line: a shift register of cell pairs clocked
    // asynchronously to the host rate, with the input resampled onto the clock
    // grid and the output held between clock edges exactly as the part does.
    struct Line
    {
        struct BlepEvent
        {
            float jump { 0.0f };
            double ageInSamples { 0.0 };
        };

        std::array<float, cellPairs> cells {};
        int writeIndex { 0 };
        double clockPhase { 0.0 };
        float held { 0.0f };
        float previousInput { 0.0f };
        float previousInput2 { 0.0f };
        float previousInput3 { 0.0f };
        // The output has the tap-summing pole followed by two Sallen-Key
        // sections. The wet input's own coupling high-pass and five low-pass
        // poles are shared by the two lines and live on the Chorus: see
        // `InputSupport` below.
        //
        // The BBD's clock-grid images are physical and remain in the modeled
        // staircase. Sampling that asynchronous staircase on the numerical
        // grid creates a second, non-physical family of aliases. A short
        // polyBLEP history removes only that host-grid error before the five
        // hardware output poles. It is numerical state: unlike buckets,
        // clock phase, transfer loss and held noise, it is cleared when the
        // engine changes processing rate.
        std::array<double, 6> exactOutputState {};
        double exactOutputPrevious { 0.0 };
        double exactOutputPrevious2 { 0.0 };
        double exactOutputPrevious3 { 0.0 };
        float transferState { 0.0f };
        std::uint32_t noiseState { 0x9e3779b9u };
        std::array<BlepEvent, maximumBlepEvents> pastBlepEvents {};
        int pastBlepEventCount { 0 };

        void reset(std::uint32_t seed) noexcept;
        void resetAudioRateSupport() noexcept;
        void ageBlepEvents() noexcept;
        void rememberBlepEvent(float jump, double ageInSamples) noexcept;
        [[nodiscard]] double deterministicBlepCorrection(
            double clockIncrement) const noexcept;
        [[nodiscard]] float processClockedCore(
            float limitedInput, float clockHz, float sampleRate,
            float noiseScale) noexcept;
        // `limitedInput` has already been through the shared input support
        // chain -- see `Chorus::advanceInputSupport`.
        float process(float limitedInput, float clockHz, float sampleRate,
                      const SupportChain::ExactTransition& outputTransition,
                      float noiseScale, bool useBlep = true) noexcept;
    };

    // Both wet branches take the same node through the same input support
    // network: two Sallen-Key sections, the wet-only coupling high-pass and
    // the passive pole beside the MN3009 input. The model gives the two
    // branches' parts identical values, and both lines' support state is
    // zeroed identically by every reset, so the two chains produced the same
    // float for the life of the plug-in -- the chain was simply advanced twice
    // on the same operands. It is advanced once, here.
    //
    // This is exactly as physical as the duplicate was, and no more: it says
    // the two networks are modelled as identical, which they are. Give either
    // branch's parts their own tolerance -- a per-line Unit Character spread,
    // say -- and this has to be split back into the two lines first.
    struct InputSupport
    {
        float couplingState { 0.0f };
        float passiveState { 0.0f };
        BiquadState antiAliasFirst {};
        BiquadState antiAliasSecond {};
        std::array<double, 6> exactState {};
        double exactPrevious { 0.0 };
        double exactPrevious2 { 0.0 };
        double exactPrevious3 { 0.0 };

        void reset() noexcept;
    };

    [[nodiscard]] float advanceInputSupport(float input) noexcept;

    Line lineA_ {};
    Line lineB_ {};
    InputSupport inputSupport_ {};
    float sampleRate_ { 48000.0f };
    float inverseSampleRate_ { 1.0f / 48000.0f };
    // 1 - exp(-inverseSampleRate_ / wetMuteTimeConstantSeconds): the per-sample
    // wet-mute glide coefficient. It depends only on the sample rate, so
    // prepare() solves it once instead of process() calling std::exp on every
    // internal sample. The default matches the 48 kHz default above and is
    // overwritten by the first prepare() call in normal use.
    float wetMuteGlide_ { 0.004157998f };
    // Double precision keeps the sub-hertz free-running phase smooth at the
    // engine's highest internal rates and over long sessions.
    double lfoPhase_ { 0.0 };
    SupportChain support_ {};
    std::array<SupportChain, 3> preparedSupport_ {};
    std::array<float, 3> preparedSupportRates_ {};
    bool supportRatesPrepared_ { false };
    // Regression seam: Engine quality changes must only select these prepared
    // values, never rebuild matrix transitions on the audio callback.
    std::uint64_t supportBuildCount_ { 0 };
    float wetGain_ { 0.0f };
    float rateHz_ { 0.0f };
    float centreDelay_ { 0.0032f };
    float sweep_ { 0.0f };
    // Set while the settled-bypass path skips the muted lines; the next
    // engaged process() rebuilds the wet path from silence before use.
    bool wetPathFlushPending_ { false };
    OptionalNoiseComponents optionalNoise_ {};
    std::uint32_t commonNoiseState_ { 0xd1b54a35u };
    std::uint32_t orthogonalNoiseState_ { 0x94d049bbu };
    double humPhase_ { 0.0 };
    // The heterodyne clock bleed's own accumulators.
    double clockSpurPhaseA_ { 0.0 };
    double clockSpurPhaseB_ { 0.0 };
    // The optional clock-spur hypothesis keeps separate ones: both mechanisms
    // run at the same modulated clock, so sharing a phase advanced it twice
    // per sample and doubled each tone's frequency when both were enabled.
    double optionalSpurPhaseA_ { 0.0 };
    double optionalSpurPhaseB_ { 0.0 };
    // Bypass mutes the wet return but retains the last-selected I/II noise
    // profile, matching the clock-program behaviour this model already had.
    ChorusMode runningMode_ { ChorusMode::One };
    // Whether the glided settings have a starting point yet.
    bool primed_ { false };
    // The wet-mute drive's two capacitor voltages and Tr4's state; the
    // per-sample glides are solved in prepare().
    float muteDriveNodeVolts_ { 15.0f };
    float muteDriveHoldVolts_ { 0.0f };
    float muteDriveNodeGlide_ { 0.0f };
    float muteDriveHoldGlide_ { 0.0f };
    bool muteDriveMuted_ { true };
    bool muteDriveEnabled_ { false };
    // Per-line insertion gains at the last calibration they were solved for.
    float lineGainA_ { 1.0f };
    float lineGainB_ { 1.0f };
    float lineGainCalibration_ { 0.0f };
};

} // namespace youknow
