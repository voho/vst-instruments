// The Ghostar voice: two bandlimited oscillators with hard sync and a
// triangle-cross ring modulator, one fixed-clock MM5837 noise source, two
// parallel audio paths (series dual filter + ADSR VCA; Shaper-driven VCA +
// passive brightness shelf), two ADSRs behind OR'ed gate sources, MOD X with six
// sources and the arpeggiator, and the Shaper Y variable-rate integrator —
// all per the modelling contract in the README's "How it works".
// Constants marked "voiced" here are the first-pass choices recorded in
// the README's "Known gaps".

#include "DSP/GhostarEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

namespace ghostar
{
namespace
{
    constexpr double pi = 3.14159265358979323846264338327950288;

    [[nodiscard]] double clamp01(double value) noexcept
    {
        return std::clamp(value, 0.0, 1.0);
    }

    [[nodiscard]] double bipolarWhite(std::uint32_t& state) noexcept
    {
        // A separate deterministic xorshift stream for each physical noise
        // source keeps offline renders reproducible without correlating the
        // two VCA cells or reusing the MM5837's documented PRBS sequence.
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        return (static_cast<double>(state) - 2147483647.5)
             / 2147483647.5;
    }

    // A 100k linear pot loaded by a fixed series arm into a virtual-earth
    // node. This is exact for the Shaper summer and MOD RATE control. The
    // Filter dry-output seam also uses it; the live Lower state uses the
    // complete moving-node solve in runLowerSection().
    [[nodiscard]] double loadedLinearPot(double travel,
                                         double seriesKilohms) noexcept
    {
        const double t = clamp01(travel);
        return t * seriesKilohms
             / (seriesKilohms + 100.0 * t * (1.0 - t));
    }

    [[nodiscard]] constexpr double parallelKilohms(double a,
                                                    double b) noexcept
    {
        return a * b / (a + b);
    }

    // The two performance wheels look alike mechanically but are not the
    // same circuit. MOD X's CEM3360 current output drives a 100k rheostat to
    // ground, so each selected destination changes its transimpedance.
    // SHAPER Y is a voltage through R60=15k into a conventional 100k divider,
    // whose wiper is loaded by the selected destination (SM DWG 2 P1013/14).
    // Their absolute source scales remain open; normalize X->Osc A and
    // Y->Osc B to the existing one-octave full-wheel anchors, then let the
    // documented resistors determine every other pitch/filter depth and the
    // very different loaded travels. Neither wheel pot is marked LIN, so UI
    // travel == resistance fraction remains the explicit taper assumption.
    constexpr double wheelTrackKilohms = 100.0;
    constexpr double oscillatorModShuntKilohms = 22.0;
    constexpr double oscillatorModInputKilohms = 100.0;
    constexpr double filterModInputKilohms = 100.0;
    // P1014's mirrored RWM inputs present 200k || 620k to either wheel. This
    // closes only the passive travel; the following BC308/PW-trim/CEM3340
    // duty conversion remains the separately voiced full-depth seam.
    constexpr double rwmDestinationLoadKilohms =
        parallelKilohms(200.0, 620.0);
    constexpr double shaperYSourceKilohms = 15.0;
    constexpr double oscillatorSingleLoadKilohms = parallelKilohms(
        oscillatorModShuntKilohms, oscillatorModInputKilohms);
    constexpr double oscillatorPairLoadKilohms = parallelKilohms(
        oscillatorSingleLoadKilohms, oscillatorModInputKilohms);
    constexpr double xOscAFullTransimpedanceKilohms = parallelKilohms(
        wheelTrackKilohms, oscillatorSingleLoadKilohms);
    constexpr double yOscBFullVoltageGain =
        xOscAFullTransimpedanceKilohms
        / (shaperYSourceKilohms + xOscAFullTransimpedanceKilohms);
    constexpr double filterOctavesPerModVoltRelativeToOscillator =
        21.2 / 19.6;

    [[nodiscard]] double modXWheelGain(double travel,
                                       double destinationLoadKilohms) noexcept
    {
        return parallelKilohms(wheelTrackKilohms * clamp01(travel),
                               destinationLoadKilohms)
             / xOscAFullTransimpedanceKilohms;
    }

    [[nodiscard]] double shaperYWheelGain(
        double travel, double destinationLoadKilohms) noexcept
    {
        const double t = clamp01(travel);
        const double lowerArm = wheelTrackKilohms * t;
        const double upperArm = wheelTrackKilohms * (1.0 - t);
        const double loadedLower = parallelKilohms(
            lowerArm, destinationLoadKilohms);
        const double voltageGain = loadedLower
            / (shaperYSourceKilohms + upperArm + loadedLower);
        return voltageGain / yOscBFullVoltageGain;
    }

    // The unresolved RS7 dry transfer and the same-unit MM5837 level remain
    // explicit calibrations. Oscillator volts enter the Lower MNA directly.
    constexpr double filterDryMixGain = 0.45;
    constexpr double filterNoiseMixGain = 0.45;
    constexpr double shaperMixGain = 0.45; // absolute scale open, OQ-20
    constexpr double redNoiseBusGain = 0.26; // absolute scale open, OQ-17
    // DWG2's printed SL3/SL4 functional labels are transposed: the P1013
    // assembly puts the physical panel order at SL1, SL2, SL4, SL3. Thus
    // errata-corrected R45=6k8 belongs to RING/SL4; PINK NOISE IN reaches
    // SL3/R44=47k.
    constexpr double shaperRingRelativeGain = 47.0 / 6.8;
    // RS7's installed rotor phase does not source-close these output gains.
    // Keep explicit voiced bridges for the owner's-manual BANDPASS peak and
    // HIGHPASS rejection; IC12B/B8's traced 151x path is not assigned to a
    // named detent until continuity proves it (OQ-20).
    constexpr double lowerBandPassOutputGain = 11.0;
    constexpr double lowerHighPassOutputGain = 8.0;

    // P1013's two output-path capacitor networks. BRIGHTNESS is a series
    // C18/P3 shunt across the Shaper CEM3360's fixed 20k master-pot load;
    // C30 AC-couples the filter output into R132 and R133 before its VCA.
    constexpr double masterTrackOhms = 20.0e3;
    constexpr double outputArmOhms = 10.0e3;
    constexpr double brightnessCapacitance = 27.0e-9;
    constexpr double brightnessPotOhms = 100.0e3;
    constexpr double filterCouplingCapacitance = 470.0e-9;
    constexpr double filterCouplingLoadOhms =
        24.0e3 * 100.0e3 / (24.0e3 + 100.0e3);
    constexpr double ringCouplingCapacitance = 1.0e-6;
    constexpr double ringCarrierLoadOhms =
        39.0e3 * 100.0e3 / (39.0e3 + 100.0e3);

    // The CEM3360 production sheet specifies 0.4 nA RMS typical output
    // noise in a 16 Hz--16 kHz measurement bandwidth. It does not publish
    // the spectrum or its control dependence, so white is explicitly the
    // equivalent density of that integrated figure, not a device claim.
    // IC7's Loudness cell and IC5's Shaper cell each drive an independently
    // fixed 20k Master track, making the term independent of mixer scale.
    // prepare() converts the density to uniform noise on the internal grid.
    constexpr double cem3360OutputNoiseAmpsRms = 0.4e-9;
    constexpr double cem3360OutputNoiseBandwidthHz = 16000.0 - 16.0;

    // ---------------------------------------------------------------------
    // Bandlimited oscillator core. Every waveform discontinuity — sawtooth
    // and pulse value jumps, the triangle's slope corners, and the value
    // *and* slope jumps a hard-sync reset makes at an arbitrary phase — is
    // an event with a sub-sample time, corrected by a two-sample
    // polynomial BLEP (value jumps) or its integral, the BLAMP (slope
    // jumps). An event discovered mid-sample must also correct the sample
    // *before* it; rather than predicting events one sample ahead (exact
    // only while the frequency holds still), each oscillator emits with one
    // internal sample of delay, so the earlier half of every correction is
    // applied to a sample that has not left the oscillator yet.
    //
    // Conventions: an event at fraction u of the current internal sample
    // corrects the held (previous) sample by r(-u) and the current one by
    // r(1-u), where for a unit value step r is the C1 quadratic BLEP
    // residual r(a) = (1+a)^2/2 for a in [-1,0), -(1-a)^2/2 for a in
    // [0,1); the BLAMP residual is its integral, (1+a)^3/6 and (1-a)^3/6,
    // scaled by the slope change per sample.

    struct OscCorrections
    {
        double selectedHeld { 0.0 };
        double selectedNow { 0.0 };
        double triangleHeld { 0.0 };
        double triangleNow { 0.0 };
    };

    void addStepEvent(double& held, double& now, double u,
                      double delta) noexcept
    {
        const double before = 1.0 - u;
        held += delta * before * before * 0.5;
        now -= delta * u * u * 0.5;
    }

    void addRampEvent(double& held, double& now, double u,
                      double slopeChangePerSample) noexcept
    {
        const double before = 1.0 - u;
        held += slopeChangePerSample * before * before * before / 6.0;
        now += slopeChangePerSample * u * u * u / 6.0;
    }

    // A slider or pot's 0..1 travel mapped exponentially across a stated
    // range, the law every time and rate control in the contract uses.
    [[nodiscard]] double exponentialTravel(double travel, double low,
                                           double high) noexcept
    {
        return low * std::pow(high / low, clamp01(travel));
    }

    // P1013's panel-cutoff pots are linear voltage dividers, but their
    // wipers are visibly loaded by the following virtual-earth summers.
    // Resistances are in kOhm here so the equations remain legible beside
    // the drawing: P6 sees R50||R51, while P5 sees R48 alone.
    [[nodiscard]] double loadedMasterCutoffVolts(double travel) noexcept
    {
        const double x = clamp01(travel);
        return (24.0 * x - 12.0) * 110.5
             / (110.5 + 100.0 * x * (1.0 - x));
    }

    [[nodiscard]] double loadedLowerOnlyVolts(double travel) noexcept
    {
        const double x = clamp01(travel);
        return -12.0 * (1.0 - x) * 150.0
             / (150.0 + 100.0 * x * (1.0 - x));
    }

    constexpr double cem3350VoltsPerOctave = 0.0196;
    constexpr double panelMasterMixerGain = 100.0 / 221.0;
    constexpr double panelLowerOnlyMixerGain = 100.0 / 150.0;
    constexpr double dynamicCutoffNodeGain =
        (1.0 / 12.1)
        / (2.0 / 12.1 + 1.0 / 16.0 + 1.0 / 0.274 + 1.0 / 68.0);
    // In FORMANT, R142 and corrected R188=22k form the second 34.1k arm.
    constexpr double formantCutoffNodeGain =
        (1.0 / 12.1)
        / (1.0 / 12.1 + 1.0 / 34.1 + 1.0 / 16.0
           + 1.0 / 0.274 + 1.0 / 68.0);

    // The ADSR sliders' RC time constant. Each of the six A/D/R sliders is
    // 2 MΩ log into its 4.7 µF cap. A voiced ~1 kΩ effective endpoint
    // resistance lands the printed fast limit. P1015's R23/R24=100 Ω sit
    // between the common A/D/R + 556-threshold node and the actual cap, so
    // every segment also crosses one of them. Thus tau runs 5.17 ms to
    // 9.40047 s, matching the manual's nominal "5 milliseconds to 10
    // seconds" while retaining the separately drawn resistor (SM DWG 3;
    // OQ-04).
    constexpr double envelopeCapacitance = 4.7e-6;
    constexpr double envelopeSeriesOhms = 1.0e3;
    constexpr double envelopeSliderOhms = 2.0e6;
    constexpr double envelopeSenseOhms = 100.0;
    constexpr double envelopeReferenceVolts = 7.5;
    // P1015 has two reset-pulse lanes. X and Y/EXT use 10 nF through
    // 470 kOhm (~5 ms), while keyboard KT in MULTIPLE and arpeggiator AA
    // share the separately annotated 10 ms node (SM DWG 3).
    constexpr double envelopeXyResetSeconds = 0.005;
    constexpr double envelopeKtAaResetSeconds = 0.010;
    // The 2023 factory manual specifies that EXTERNAL GATE must be higher
    // than 6 V; equality is deliberately not accepted.
    constexpr double externalGateThresholdVolts = 6.0;
    // P1015 annotates KT as a 25 us pulse from P1016's debounce monostable.
    // IC10 gates that live pulse with /AA and drives Q2 directly; C7/R15 are
    // upstream timing parts, not a downstream 1 ms reset stretcher.
    constexpr double keyboardLfoResetSeconds = 25.0e-6;
    // P1015 gives the MOD RATE converter 132 mV of full travel. The original
    // CEM3360 production sheet specifies 3.0 mV/dB typical, hence 44 dB or
    // 158.489319:1. Anchoring the manual's approximate 50 Hz fast end fixes
    // the nominal slow end (OQ-21).
    constexpr double lfoFastHz = 50.0;
    constexpr double lfoSlowHz = 0.3154786722400966;
    // What the attack charges toward, as a multiple of the envelope's peak:
    // the 556's output-high level less the series diode, against the +7.5 V
    // the drawing labels at the control-voltage pin. The documents bound it
    // to 1.22–1.35; the nominal ships (derived, OQ-04).
    constexpr double attackAimRatio = 1.3;
    // R135/R136/R137 independently put the Loudness CEM3360's zero-control
    // crossing at 0.5 V. Keep this separate from D15's nominal floor: a
    // future measured diode calibration must not move the derived VCA seam.
    constexpr double loudnessControlInputOhms = 10.0e3;
    constexpr double loudnessControlGroundOhms = 4.7e3;
    constexpr double loudnessControlNegativeOhms = 240.0e3;
    constexpr double cem3360LinearGainPerVolt = 0.52;
    constexpr double loudnessZeroLevel = 1.0 / 15.0;
    constexpr double loudnessZeroVolts =
        envelopeReferenceVolts * loudnessZeroLevel;

    [[nodiscard]] double envelopeResistance(double travel) noexcept
    {
        return exponentialTravel(travel, envelopeSeriesOhms,
                                 envelopeSliderOhms);
    }

    [[nodiscard]] double envelopeTau(double travel) noexcept
    {
        return envelopeCapacitance
            * (envelopeResistance(travel) + envelopeSenseOhms);
    }

    // R23/R24 are below the 556 threshold node. During attack the cap is
    // therefore lower than that node by 100 ohms times the charging current
    // when the timer changes phase. Solving that KVL at Vthreshold=7.5 V
    // gives the characteristic fast-attack undershoot; it vanishes toward
    // the slow end but is about three percent at the voiced 1 kOhm endpoint.
    [[nodiscard]] double envelopeAttackPeak(double travel) noexcept
    {
        const double sliderOhms = envelopeResistance(travel);
        return 1.0 - envelopeSenseOhms / sliderOhms
            * (attackAimRatio - 1.0);
    }

    // SL3 and SL7 are two 100k tracks from the shared +7.5 V reference to
    // one D15-biased lower rail. D15's type is absent from the factory parts
    // list. The nominal matched-silicon model deliberately calibrates that
    // rail to 0.5 V: the independently derived zero-control point of the
    // Loudness CEM3360. The 43 mV effective slope is a voiced vintage
    // small-signal-diode nominal; Is follows from
    // 2*(7.5-0.5)/100k = Is*expm1(0.5/43mV).
    // This exposes the original common-floor/release-knee topology without
    // pretending to be a same-unit temperature calibration (OQ-04).
    constexpr double envelopeSustainFloorVolts = 0.5;
    constexpr double envelopeSustainFloorLevel =
        envelopeSustainFloorVolts / envelopeReferenceVolts;
    constexpr double envelopeDiodeSlopeVolts = 0.043;
    constexpr double envelopeDiodeSaturationAmps =
        1.2479467973540046e-9;

    // Invert V = R*i + a*log(1+i/Is) for a forward diode in series with a
    // positive resistance. As with the BA130 solve below, retain the physical
    // [0,V/R] bracket, reject Newton steps that leave it and stop on the KVL
    // residual. The logarithmic branch choice avoids evaluating an
    // overflowing exponential. D11/D14 use this nominal law against an
    // ideal-low GS; their real shared 4075 output resistance remains a
    // hardware seam.
    [[nodiscard]] double envelopeDiodeCurrent(double driveVolts,
                                               double seriesOhms) noexcept
    {
        assert(std::isfinite(seriesOhms) && seriesOhms > 0.0);
        if (!(driveVolts > 0.0))
            return 0.0;

        const double maximumCurrent = driveVolts / seriesOhms;
        const double resistorOnlyLog =
            std::log1p(maximumCurrent / envelopeDiodeSaturationAmps);
        const double diodeOnlyLog =
            driveVolts / envelopeDiodeSlopeVolts;
        double current = resistorOnlyLog <= diodeOnlyLog
                             ? maximumCurrent
                             : envelopeDiodeSaturationAmps
                                 * std::expm1(diodeOnlyLog);

        double lowerCurrent = 0.0;
        double upperCurrent = maximumCurrent;
        constexpr int maximumIterations = 12;
        constexpr double voltageTolerance =
            16.0 * std::numeric_limits<double>::epsilon();
        for (int step = 0; step < maximumIterations; ++step)
        {
            const double error = std::fma(seriesOhms, current, -driveVolts)
                + envelopeDiodeSlopeVolts
                    * std::log1p(
                        current / envelopeDiodeSaturationAmps);
            if (std::abs(error) <= voltageTolerance * (1.0 + driveVolts))
                break;

            if (error < 0.0)
                lowerCurrent = current;
            else
                upperCurrent = current;

            const double slope = seriesOhms
                + envelopeDiodeSlopeVolts
                    / (envelopeDiodeSaturationAmps + current);
            const double newton = current - error / slope;
            current = newton > lowerCurrent && newton < upperCurrent
                ? newton
                : 0.5 * (lowerCurrent + upperCurrent);
        }
        return current;
    }

    [[nodiscard]] double envelopeReleaseTime(double fromVolts,
                                              double toVolts,
                                              double resistanceOhms) noexcept
    {
        const double fromCurrent =
            envelopeDiodeCurrent(fromVolts, resistanceOhms);
        const double toCurrent =
            envelopeDiodeCurrent(toVolts, resistanceOhms);
        const double resistive = resistanceOhms
            * std::log(fromCurrent / toCurrent);
        const double diode = envelopeDiodeSlopeVolts
            / envelopeDiodeSaturationAmps
            * (std::log1p(envelopeDiodeSaturationAmps / toCurrent)
               - std::log1p(
                   envelopeDiodeSaturationAmps / fromCurrent));
        return envelopeCapacitance * (resistive + diode);
    }

    // Below this the envelope is treated as finished and snaps to idle.
    constexpr double envelopeIdleLevel = 1.0e-5;

    // The panel duty-cycle sets: A = 50/30/15/6 %, B = 40/20/10/3 %
    // (anchored: panel line-art and photos; see the research document).
    [[nodiscard]] double dutyFor(Waveform waveform, bool oscA) noexcept
    {
        switch (waveform)
        {
            case Waveform::RectWide:   return oscA ? 0.50 : 0.40;
            case Waveform::RectMid:    return oscA ? 0.30 : 0.20;
            case Waveform::RectNarrow: return oscA ? 0.15 : 0.10;
            case Waveform::RectThin:   return oscA ? 0.06 : 0.03;
            default:                   return 0.50;
        }
    }

    // Naive waveforms: what the events above correct. Triangle is −1 at
    // phase 0, +1 at phase 0.5.
    [[nodiscard]] double triangleWave(double phase) noexcept
    {
        return phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
    }

    [[nodiscard]] double triangleSlope(double phase) noexcept
    {
        return phase < 0.5 ? 4.0 : -4.0;
    }

    [[nodiscard]] double naiveWave(Waveform waveform, double phase,
                                   double duty) noexcept
    {
        switch (waveform)
        {
            case Waveform::Triangle: return triangleWave(phase);
            case Waveform::Sawtooth: return 2.0 * phase - 1.0;
            default:                 return phase < duty ? 1.0 : -1.0;
        }
    }

    // Downstream engine seams predate a physical-volt Lower solve. Five volts
    // is P1014's own affine offset, so it is the explicit unit reference while
    // retaining the circuit's waveform-dependent swing and DC bias.
    constexpr double selectedWaveVoltsPerEngineUnit = 5.0;
    // A host has no physical dBFS voltage. Use the engine's existing 5 V
    // source reference: +/-1.0 at the bus is +/-5 V at the jack. The service
    // drawing has no jack clamp, so the +/-12 V bound below is explicitly a
    // product-safety policy for the still-unmodelled downstream overload, not
    // a claim about P1017. Upstream gain remains the calibration until a
    // same-unit sensitivity capture exists.
    constexpr double externalAudioLimitEngineUnits =
        12.0 / selectedWaveVoltsPerEngineUnit;
    constexpr double selectedWaveConditionerGain =
        1.0 + 10.0 / 24.0 + 10.0 / 91.0;

    // P1016's DAC0800 uses six key bits with its two LSBs grounded, so each
    // semitone advances the DAC word by four. At IC16A, that sink current
    // opposes +12A through R39=26.6k; both currents share the same rail, so
    // their cancellation code depends only on R31=4.99k / R39. The lowest
    // original-keyboard C is MIDI 48, putting nominal KCV zero just 0.006
    // semitone above its second C (derived, OQ-13).
    constexpr double keyboardTrackingPivotMidi =
        48.0 + 64.0 * 4.99 / 26.6;

    // P1016 labels the keyboard source D = 1.1 V/oct, the loaded jack node
    // N = 0.95 V/oct and IC16B's output P = -1 V/oct. The rounded labels are
    // independently reproduced by its 15k source arm, 95.3k summing resistor
    // and 100k feedback: 1.1 * 95.3/(15+95.3) = 0.9504 V at N, then IC16B
    // gives -0.9973 V/oct at P. The existing note coordinate already carries
    // the calibrated keyboard's complete D -> N -> P gain, so normalize an
    // external source against that same specified 1.1 V octave instead of
    // applying the resistor loss a second time. The later inverting pitch
    // summer restores increasing source voltage to increasing pitch.
    constexpr double externalPitchSourceVoltsPerOctave = 1.1;

    // P1013 gives OSC B PEDAL a +12 V / R192=33k pull-up and C47=100n
    // storage before P1014's R72=383k plus 25k trim pitch arm. The trim's
    // electrical midpoint is an explicit nominal assumption.
    //
    // The FILTER scan leaves the J8/3/J8/4 crossing visually ambiguous. The
    // functional reading used here puts R191/C48 on J8/4: it is the reading
    // consistent with the official manual's 100k TS potentiometer-pedal
    // instruction, P1017 R2=10k, and the drawn one/two switched 100k
    // destination arms.
    // Reducing those destination buses to independent virtual-earth loads is
    // also explicit. Resistance is the host API because pedal taper is not
    // specified for either socket.
    constexpr double pedalPullupVolts = 12.0;
    constexpr double pedalPullupOhms = 33.0e3;
    constexpr double pedalCapacitanceFarads = 100.0e-9;
    constexpr double oscBPedalInputOhms = 383.0e3 + 0.5 * 25.0e3;
    constexpr double filterPedalShuntOhms = 10.0e3;
    constexpr double filterPedalInputOhms = 100.0e3;

    // P1014 does not send the three CEM3340 outputs straight to RS5/RS6.
    // Triangle is direct, but saw sees 10k series / 10k shunt and pulse sees
    // 10k series / 6.8k shunt.  Include the data-sheet's typical 100-ohm saw
    // output impedance and its loaded open-emitter pulse law; the selector
    // taps consequently land within 10% of one another despite the very
    // different raw chip swings.  The 1458 input/contact load is unspecified
    // and negligible in the source-nominal reduction.
    [[nodiscard]] double selectedTapHighVolts(Waveform waveform) noexcept
    {
        if (waveform == Waveform::Triangle)
            return 12.0 / 3.0;
        if (waveform == Waveform::Sawtooth)
            return (2.0 * 12.0 / 3.0) * 10.0e3
                 / (100.0 + 10.0e3 + 10.0e3);

        constexpr double pulseSeriesOhms = 10.0e3;
        constexpr double pulseShuntOhms = 6.8e3;
        constexpr double pulseLoadOhms =
            pulseSeriesOhms + pulseShuntOhms;
        constexpr double loadedEmitterHigh = (12.0 - 0.3)
            / (1.0 + 1.3e3 / pulseLoadOhms);
        return loadedEmitterHigh * pulseShuntOhms / pulseLoadOhms;
    }

    // A BLEP/BLAMP residual is a voltage delta, so IC10's -5 V affine offset
    // must not be applied to it. This also lets a correction discovered under
    // a newly selected waveform pre-ring the old held sample at the new
    // waveform's physical scale without reinterpreting that held sample.
    [[nodiscard]] double selectedWaveDeltaVolts(
        Waveform waveform, double bipolarDelta) noexcept
    {
        return 0.5 * selectedTapHighVolts(waveform)
             * selectedWaveConditionerGain * bipolarDelta;
    }

    // Registers the discontinuity events one linear phase segment crosses:
    // the pulse's falling edge at the duty boundary, the triangle's corner
    // at 0.5, and the wrap at 1 (a saw or pulse value jump and a triangle
    // corner at once). The frequency cap keeps a sample's advance under
    // half a cycle, so each boundary is crossed at most once — except the
    // duty and 0.5 boundaries, which can be crossed a second time after a
    // wrap, and are re-checked there.
    void scanLinearSegment(double startPhase, double uStart, double uEnd,
                           double step, Waveform waveform, double duty,
                           OscCorrections& c, double& endPhase) noexcept
    {
        const double advance = (uEnd - uStart) * step;
        const double target = startPhase + advance;
        const bool isPulse =
            waveform != Waveform::Triangle && waveform != Waveform::Sawtooth;

        const auto crossing = [&](double boundary) noexcept {
            return uStart + (boundary - startPhase) / step;
        };
        const auto pulseEdge = [&](double u) noexcept {
            addStepEvent(c.selectedHeld, c.selectedNow, u, -2.0);
        };
        const auto triangleCorner = [&](double u, double change) noexcept {
            addRampEvent(c.triangleHeld, c.triangleNow, u, change * step);
            if (waveform == Waveform::Triangle)
                addRampEvent(c.selectedHeld, c.selectedNow, u,
                             change * step);
        };

        // Boundaries below 1, in phase order. duty may sit on either side
        // of the triangle corner, or exactly on it (the 50 % position), so
        // each boundary fires its own events once and a coincident pair is
        // visited once.
        const double first = std::min(duty, 0.5);
        const double second = std::max(duty, 0.5);
        for (int index = 0; index < (second > first ? 2 : 1); ++index)
        {
            const double boundary = index == 0 ? first : second;
            if (!(startPhase < boundary && boundary <= target))
                continue;
            const double u = crossing(boundary);
            if (boundary == duty && isPulse)
                pulseEdge(u);
            if (boundary == 0.5)
                triangleCorner(u, -8.0);
        }

        if (target >= 1.0)
        {
            const double u = crossing(1.0);
            if (waveform == Waveform::Sawtooth)
                addStepEvent(c.selectedHeld, c.selectedNow, u, -2.0);
            else if (isPulse)
                addStepEvent(c.selectedHeld, c.selectedNow, u, 2.0);
            triangleCorner(u, 8.0);

            const double wrapped = target - 1.0;
            if (isPulse && duty <= wrapped)
                pulseEdge(crossing(1.0 + duty));
            if (0.5 <= wrapped)
                triangleCorner(crossing(1.5), -8.0);
            endPhase = wrapped;
        }
        else
        {
            endPhase = target;
        }
    }

    // Advances one oscillator through one internal sample, registering every
    // discontinuity it crosses — including a hard-sync reset at fraction
    // resetU, which jumps the value to phase 0's and breaks the slope, both
    // corrected against the phase the wave actually held at the reset
    // instant. Returns the end-of-sample phase.
    [[nodiscard]] double scanOscillatorSample(double oldPhase, double step,
                                              Waveform waveform, double duty,
                                              double resetU,
                                              OscCorrections& c) noexcept
    {
        double endPhase = oldPhase;
        if (resetU >= 0.0)
        {
            scanLinearSegment(oldPhase, 0.0, resetU, step, waveform, duty, c,
                              endPhase);
            const double preReset = endPhase;

            const double selectedJump = naiveWave(waveform, 0.0, duty)
                                      - naiveWave(waveform, preReset, duty);
            if (selectedJump != 0.0)
                addStepEvent(c.selectedHeld, c.selectedNow, resetU,
                             selectedJump);
            if (waveform == Waveform::Triangle)
                addRampEvent(c.selectedHeld, c.selectedNow, resetU,
                             (4.0 - triangleSlope(preReset)) * step);

            addStepEvent(c.triangleHeld, c.triangleNow, resetU,
                         -1.0 - triangleWave(preReset));
            addRampEvent(c.triangleHeld, c.triangleNow, resetU,
                         (4.0 - triangleSlope(preReset)) * step);

            scanLinearSegment(0.0, resetU, 1.0, step, waveform, duty, c,
                              endPhase);
        }
        else
        {
            scanLinearSegment(oldPhase, 0.0, 1.0, step, waveform, duty, c,
                              endPhase);
        }
        return endPhase;
    }

    // P1013's BA130 pairs. The digitised typical curve in the Fairchild
    // 1978 Diode Data Book (BA128·BA130 p.3-12, curve family p.4-6) gives
    // 99 mV/decade: ideality n ~= 1.68, Is ~= 2.3 nA, n*Vt ~= 43 mV.
    constexpr double diodeSaturationAmps = 2.3e-9;
    constexpr double diodeThermalVolts = 0.043;

    // Each CEM3340 multiplier current output (pin 14) returns through 1.82
    // kOhm to ground, bypassed by 1 nF: A uses R82/C72 and B uses R118/C77.
    // Curtis gives fLP=1/(2*pi*R*C); the normalized parallel-RC
    // current-to-voltage reduction is H(s)=1/(1+sRC), before exponentiation.
    constexpr double cemPitchMultiplierOhms = 1.82e3;
    constexpr double cemPitchMultiplierFarads = 1.0e-9;
    constexpr double cemPitchMultiplierTau =
        cemPitchMultiplierOhms * cemPitchMultiplierFarads;

    constexpr double cemTimingCapacitance = 22.0e-9;
    // SW4 carries C40 between the two Upper VLP timing nodes. In 12 dB it
    // selects the controlled node and leaves R194 between the two nodes; in
    // 24 dB it selects the fixed node and shorts R194 out of that path.
    constexpr double upperSlopeMemoryCapacitance = 1.0e-9;
    constexpr double upperSelectedLpCapacitance =
        cemTimingCapacitance + upperSlopeMemoryCapacitance;
    constexpr double upperLpCouplingOhms = 1.0e6;
    constexpr double upperFixedInputGain = 3.0; // VIF + VIV at Q=0.5
    constexpr double upperTwelveDbOutputGain = 201.0;
    constexpr double upperTwentyFourDbOutputGain = 101.0;
    constexpr double lowerMixerPotOhms = 100.0e3;
    constexpr double lowerMixerResistanceOhms = 220.0e3;
    constexpr double lowerMixerCapacitance = 68.0e-12;
    constexpr double lowerMixerCapacitanceRatio =
        lowerMixerCapacitance / cemTimingCapacitance;
    constexpr double highQCouplingCapacitance = 1.0e-9;
    constexpr double highQCapacitanceRatio =
        cemTimingCapacitance / highQCouplingCapacitance;

    // The selected waves and the Lower CEM states share one voltage domain.
    // P1014's exact -5 V affine offset defines one engine unit as 5 V; this
    // removes the former arbitrary 24 mV state scale from both BA130 loops.
    constexpr double filterNodeVoltsPerUnit =
        selectedWaveVoltsPerEngineUnit;

    // Solve V = R*i + Vd*asinh(i/(2*Is)), the inverse anti-parallel diode
    // equation. Current space avoids sinh overflow and has one monotone root.
    // This is the scalar zero-delay-feedback reduction advocated for
    // nonlinear virtual-analog circuits by Yeh et al., IEEE TASLP 18(4),
    // DOI 10.1109/TASL.2010.2047331: the diode is solved inside the
    // trapezoidal circuit, rather than clipped after the linear filter.
    //
    // A plain fixed-count Newton iteration is fast, but it is not a circuit
    // solver: a large transient or an unusual host rate can leave a finite
    // KVL error in the capacitor update. Keep the physical [0, V/R] current
    // bracket and accept Newton only inside it. The fallback bisection keeps
    // every iterate physical and moving toward the unique root; the voltage
    // residual normally stops at roundoff rather than at an arbitrary
    // "four iterations" of one.
    [[nodiscard]] double diodePairCurrent(double driveVolts,
                                          double seriesOhms) noexcept
    {
        assert(std::isfinite(seriesOhms) && seriesOhms > 0.0);
        const double magnitude = std::abs(driveVolts);
        if (magnitude == 0.0)
            return 0.0;

        constexpr double pairSaturationAmps = 2.0 * diodeSaturationAmps;
        const double maximumCurrent = magnitude / seriesOhms;
        const double resistorOnlyVoltage =
            std::asinh(maximumCurrent / pairSaturationAmps);
        const double diodeOnlyVoltage = magnitude / diodeThermalVolts;
        // Start at the smaller of the resistor-only and diode-only current
        // bounds. The comparison avoids evaluating sinh where it could grow.
        double current = resistorOnlyVoltage <= diodeOnlyVoltage
                             ? maximumCurrent
                             : pairSaturationAmps
                                 * std::sinh(diodeOnlyVoltage);

        double lowerCurrent = 0.0;
        double upperCurrent = maximumCurrent;
        constexpr int maximumIterations = 12;
        constexpr double voltageTolerance =
            16.0 * std::numeric_limits<double>::epsilon();
        for (int step = 0; step < maximumIterations; ++step)
        {
            const double error = std::fma(seriesOhms, current, -magnitude)
                + diodeThermalVolts
                    * std::asinh(current / pairSaturationAmps);
            if (std::abs(error) <= voltageTolerance * (1.0 + magnitude))
                break;

            if (error < 0.0)
                lowerCurrent = current;
            else
                upperCurrent = current;

            const double slope = seriesOhms
                + diodeThermalVolts
                    / std::hypot(pairSaturationAmps, current);
            const double newton = current - error / slope;
            current = newton > lowerCurrent && newton < upperCurrent
                ? newton
                : 0.5 * (lowerCurrent + upperCurrent);
        }

        return std::copysign(current, driveVolts);
    }

    // ------------------------------------------------- The OVERDRIVE stage
    // Exact ideal-op-amp reduction of IC12A in RS7's diode-closing throw.
    // q + B*2Is*sinh(q/Vd) = A*x; solving in current space uses the same
    // monotone pair equation as the high-Q branches.
    constexpr double overdriveFeedbackOhms = 330.0e3;
    constexpr double overdriveClampOhms = 2.2e3;
    constexpr double overdriveInvertingOhms = 2.2e3;
    constexpr double overdriveReturnOhms = 470.0;
    constexpr double overdriveLinearGain =
        1.0 + overdriveFeedbackOhms / overdriveInvertingOhms
            + overdriveFeedbackOhms / overdriveReturnOhms;
    constexpr double overdriveDriveGain = 0.5 * overdriveLinearGain - 1.0;
    constexpr double overdriveDiodeSeriesOhms =
        0.5 * (overdriveFeedbackOhms + overdriveClampOhms);

    // Model of the hypothesised A3+B7+C10 network: IC12A reaches C34 through
    // R187=47 kΩ while Lower VLP reaches it through R167=33 kΩ. The terminal
    // values are traced, but assignment of this offset-wafer combination to
    // the panel OVERDRIVE detent remains unresolved (OQ-10).
    constexpr double overdrivePickupOhms = 47.0e3;
    constexpr double overdriveShuntOhms = 33.0e3;
    constexpr double overdriveLoadOhms = 220.0;
    constexpr double overdriveCouplingFarads = 220.0e-9;

    // Sub-1e-30 state decays cost real time as denormals on hosts without
    // flush-to-zero; audio content never lives down there.
    [[nodiscard]] double flushDenormal(double value) noexcept
    {
        return std::abs(value) < 1.0e-30 ? 0.0 : value;
    }

    // Trapezoidal companions are physical endpoint memories: x[n] is stored
    // as 2*v[n]-x[n-1].  A fused update retains the small new memory when the
    // two large terms nearly cancel (most visibly after a switch transient),
    // and rounds only once.  This one law is shared by the resolved Spirit
    // capacitors rather than adding an ungrounded damping term to any of them.
    [[nodiscard]] double trapezoidalCompanion(double endpoint,
                                               double oldCompanion) noexcept
    {
        return flushDenormal(std::fma(2.0, endpoint, -oldCompanion));
    }

    // ------------------------------------------------- The resonance law
    // Derived, not voiced: the CEM3350's Q control is exponential at
    // −65 mV per decade of Q (datasheet © 1984; −62/−65/−68 mV window),
    // and the Spirit's own network around it is legible in SM DWG 2. The
    // RESONANCE pot is 100 kΩ linear with its top grounded and its bottom
    // at −12 V; its wiper feeds each chip's Q pin through 18k2, and each Q
    // pin carries a 221 Ω shunt to ground against a pull-up to +12 V —
    // 91 kΩ at the Upper chip, 75 kΩ at the Lower, so the two filters do
    // not share a travel-to-Q curve. The pot's own output impedance,
    // 100 kΩ·t·(1−t), sits in series with the feed, and is what flattens
    // the law through mid-travel.
    //
    // The absolute anchor the datasheet lacks comes from the panel: with
    // the RESONANCE switch at LOW the pot is disconnected and the Upper Q
    // pin rests at 12 V·221/(91 kΩ + 221 Ω) = +29.1 mV, where the manual
    // says Q = 0.5. That one anchored point calibrates the whole law.
    constexpr double resonancePotOhms = 100.0e3;
    constexpr double resonanceFeedOhms = 18.2e3;
    constexpr double resonanceShuntOhms = 221.0;
    constexpr double upperPullupOhms = 91.0e3;
    constexpr double lowerPullupOhms = 75.0e3;
    constexpr double railVolts = 12.0;
    constexpr double qVoltsPerDecade = 0.065;
    constexpr double lowSwitchQ = 0.5;
    constexpr double lowSwitchDamping = 1.0 / lowSwitchQ;
    constexpr double lowSwitchVolts =
        railVolts * resonanceShuntOhms
        / (upperPullupOhms + resonanceShuntOhms);

    [[nodiscard]] double qPinVolts(double travel, double pullupOhms) noexcept
    {
        const double t = clamp01(travel);
        const double wiperOhms =
            resonanceFeedOhms + resonancePotOhms * t * (1.0 - t);
        const double conductance = 1.0 / wiperOhms + 1.0 / pullupOhms
                                 + 1.0 / resonanceShuntOhms;
        const double current =
            -railVolts * t / wiperOhms + railVolts / pullupOhms;
        return current / conductance;
    }

    [[nodiscard]] double resonanceQ(double travel, double pullupOhms) noexcept
    {
        return lowSwitchQ
             * std::pow(10.0, -(qPinVolts(travel, pullupOhms)
                                - lowSwitchVolts) / qVoltsPerDecade);
    }

    // Q to the TPT damping k = 1/Q. The enhancement subtraction below is
    // only the declared VARIABLE/self-oscillation extension. LOW and the
    // fixed cascade half are physically biased to actual Q=0.5 and use
    // k=2 exactly; the chip's typical ceiling must not perturb that anchor.
    // The network commands far more Q than
    // the chip can hold — nominally 82 at full travel against the
    // datasheet's 30 min / 50 typ "Maximum Q Without Enhancement" — while
    // the manual anchors that resonance reaches self-oscillation at
    // maximum. Reading that ceiling as the point where the chip's own loss
    // is exactly cancelled reconciles the two: commanded Q beyond it is net
    // negative damping, and the section sings against the BA130 limiter.
    // The ceiling is the one number in this law still voiced (OQ-12).
    constexpr double chipCeilingQ = 50.0;

    [[nodiscard]] double dampingFromQ(double q) noexcept
    {
        return 1.0 / q - 1.0 / chipCeilingQ;
    }

    [[nodiscard]] double lowerDamping(double travel) noexcept
    {
        return dampingFromQ(resonanceQ(travel, lowerPullupOhms));
    }

    // Zeroth-order modified Bessel function, for the Kaiser windows the
    // decimation kernels are designed with.
    [[nodiscard]] double besselI0(double x) noexcept
    {
        double sum = 1.0;
        double term = 1.0;
        for (int k = 1; k < 64; ++k)
        {
            const double factor = x / (2.0 * k);
            term *= factor * factor;
            sum += term;
            if (term < 1.0e-14 * sum)
                break;
        }
        return sum;
    }

    // A Kaiser-windowed halfband lowpass: cutoff at a quarter of its input
    // rate, unit DC gain. Length and beta set the transition width and
    // stopband depth per stage. Only the nonzero taps are stored: a
    // halfband's sinc vanishes at every even offset from the centre, so
    // half the coefficients are structurally zero and skipping them halves
    // the decimator's arithmetic rather than multiplying by nothing.
    template <typename Kernel>
    void designKaiserHalfband(Kernel& kernel, double beta) noexcept
    {
        const int taps = static_cast<int>(kernel.values.size());
        const int center = taps / 2;
        const double denominator = besselI0(beta);
        std::vector<double> full(static_cast<std::size_t>(taps));
        double sum = 0.0;
        for (int index = 0; index < taps; ++index)
        {
            const int offset = index - center;
            const double n = static_cast<double>(offset);
            // The structural zeros are written as zero rather than computed.
            // sin(pi * n / 2) at even n is a rounding residue around 1e-16,
            // not 0, so evaluating it and then testing the result against
            // zero would keep every tap — and the kernel would not be sparse
            // at all.
            const double sinc = offset == 0 ? 1.0
                              : offset % 2 == 0
                                  ? 0.0
                                  : std::sin(pi * n / 2.0) / (pi * n / 2.0);
            const double ratio = n / static_cast<double>(center);
            const double window =
                besselI0(beta
                         * std::sqrt(std::max(0.0, 1.0 - ratio * ratio)))
                / denominator;
            full[static_cast<std::size_t>(index)] = sinc * window;
            sum += sinc * window;
        }
        kernel.count = 0;
        for (int index = 0; index < taps; ++index)
        {
            // Skipped by position, for the same reason: the tap that is
            // structurally zero is known from where it sits, not from what
            // its arithmetic came out as.
            const int offset = index - center;
            if (offset != 0 && offset % 2 == 0)
                continue;
            const double value = full[static_cast<std::size_t>(index)] / sum;
            kernel.offsets[static_cast<std::size_t>(kernel.count)] =
                taps - 1 - index;
            kernel.values[static_cast<std::size_t>(kernel.count)] = value;
            ++kernel.count;
        }
    }

    // The ring holds the last `taps` inputs, oldest at oldestIndex. Kernel
    // tap t multiplies the sample (taps-1-t) positions newer than the
    // oldest, and that distance is what the design stored.
    template <typename Kernel, std::size_t taps>
    [[nodiscard]] double convolveRing(const Kernel& kernel,
                                      const std::array<double, taps>& ring,
                                      int oldestIndex) noexcept
    {
        // Neumaier compensation matters at the steep halfband boundary,
        // where large alternating products cancel to make a tiny stop-band
        // result. Preserve that cancellation instead of letting its error
        // become a low-level alias floor.
        double accumulator = 0.0;
        double compensation = 0.0;
        for (int index = 0; index < kernel.count; ++index)
        {
            int ringIndex =
                oldestIndex
                + kernel.offsets[static_cast<std::size_t>(index)];
            if (ringIndex >= static_cast<int>(taps))
                ringIndex -= static_cast<int>(taps);
            const double product =
                kernel.values[static_cast<std::size_t>(index)]
                * ring[static_cast<std::size_t>(ringIndex)];
            const double next = accumulator + product;
            compensation += std::abs(accumulator) >= std::abs(product)
                ? (accumulator - next) + product
                : (product - next) + accumulator;
            accumulator = next;
        }
        return accumulator + compensation;
    }

    [[nodiscard]] float sanitisedTravel(float value, float fallback) noexcept
    {
        if (!std::isfinite(value))
            return fallback;
        return std::clamp(value, 0.0f, 1.0f);
    }

    // A switch enum smuggled in out of range (a corrupted preset, a hostile
    // host) must not index past a lookup table; it falls back to the
    // power-on detent instead.
    template <typename Enum>
    [[nodiscard]] Enum sanitisedSwitch(Enum value, int positionCount,
                                       Enum fallback) noexcept
    {
        const int raw = static_cast<int>(value);
        return raw < 0 || raw >= positionCount ? fallback : value;
    }
} // namespace

GhostarEngine::GhostarEngine() noexcept = default;

double GhostarEngine::p1014SelectedWaveVolts(Waveform waveform,
                                             double bipolarSample) noexcept
{
    // The selected P1014 tap then enters IC10's 10k-feedback conditioner,
    // biased by 24k to +12 V and 91k to ground:
    // Vout=(1+10/24+10/91)*Vtap-5 V (SM DWG 2A, P1014).
    const double tapHighVolts = selectedTapHighVolts(waveform);
    const double tapVolts = 0.5 * tapHighVolts * (bipolarSample + 1.0);
    // Only enforce the proven supply rails for out-of-range bandlimiting
    // residuals; the nominal selector levels remain far inside them.
    return std::clamp(selectedWaveConditionerGain * tapVolts - 5.0,
                      -12.0, 12.0);
}

double GhostarEngine::runPitchControlLag(PitchControlLag& lag,
                                         double input) noexcept
{
    if (!lag.initialised)
    {
        // Power-up has no invented pitch swoop: the capacitor starts at the
        // present static sum. Ordinary pitch and routing changes retain the
        // charge and expose the physical 1.82 us transition.
        lag.output = input;
        lag.previousInput = input;
        lag.initialised = true;
        return input;
    }

    // Exact first-order evolution for a linearly interpolated input. Unlike
    // a TPT mapping it keeps a monotone step response even when the 4x grid
    // is slower than 1/(2RC), while preserving the analog DC group delay.
    lag.output = flushDenormal(std::fma(
        pitchLagPole_, lag.output,
        std::fma(pitchLagNow_, input,
                 pitchLagPrevious_ * lag.previousInput)));
    lag.previousInput = input;
    return lag.output;
}

void GhostarEngine::prepare(double sampleRate, int maxBlockSize)
{
    (void) maxBlockSize;
    // std::clamp passes NaN through (its comparisons are all false), so a
    // host reporting a non-finite rate must be caught before the clamp.
    if (!std::isfinite(sampleRate))
        sampleRate = 44100.0;
    sampleRate_ = std::clamp(sampleRate, minimumSupportedSampleRate,
                             maximumSupportedSampleRate);
    internalRate_ = 4.0 * sampleRate_;
    // The travel smoother's one-pole: ~25 ms to target at any host rate.
    // expm1 retains the small difference from one at very high host rates.
    travelSmoothing_ = -std::expm1(-1.0 / (0.025 * sampleRate_));
    highQChargeStep_ = 1.0
        / (2.0 * internalRate_ * cemTimingCapacitance
           * filterNodeVoltsPerUnit);
    overdriveCouplingConductance_ =
        2.0 * internalRate_ * overdriveCouplingFarads;
    brightnessG_ = 2.0 * internalRate_ * brightnessCapacitance;
    filterCouplingG_ = 2.0 * internalRate_ * filterCouplingCapacitance;
    ringCouplingG_ = 2.0 * internalRate_ * ringCouplingCapacitance;
    const double cem3360NoiseDensity = cem3360OutputNoiseAmpsRms
        / std::sqrt(cem3360OutputNoiseBandwidthHz);
    cem3360OutputNoiseScale_ = cem3360NoiseDensity
        * std::sqrt(1.5 * internalRate_)
        * masterTrackOhms / selectedWaveVoltsPerEngineUnit;
    const double pitchLagRatio = 1.0
        / (internalRate_ * cemPitchMultiplierTau);
    pitchLagPole_ = std::exp(-pitchLagRatio);
    const double pitchLagAverage =
        -std::expm1(-pitchLagRatio) / pitchLagRatio;
    pitchLagNow_ = 1.0 - pitchLagAverage;
    pitchLagPrevious_ = pitchLagAverage - pitchLagPole_;
    const double externalPitchInputTau =
        (externalPitchSourceOhms * externalPitchInputOhms
         / (externalPitchSourceOhms + externalPitchInputOhms))
        * externalPitchInputFarads;
    const double keyboardPitchInputTau =
        (keyboardPitchSourceOhms * externalPitchInputOhms
         / (keyboardPitchSourceOhms + externalPitchInputOhms))
        * externalPitchInputFarads;
    keyboardPitchInputCoefficient_ = -std::expm1(
        -1.0 / (sampleRate_ * keyboardPitchInputTau));
    externalPitchInputCoefficient_ = -std::expm1(
        -1.0 / (sampleRate_ * externalPitchInputTau));

    // The decimation chain's two Kaiser halfbands (see the header for the
    // division of labour). Betas chosen for ~126 dB (first stage, whose
    // window is short because its transition is wide) and ~98 dB (second
    // stage, 0.45–0.55 of the host rate transition).
    designKaiserHalfband(stageAKernel_, 12.9);
    designKaiserHalfband(stageBKernel_, 9.88);

    noise_.prepare(internalRate_);

    reset();
}

double GhostarEngine::longestReleaseTailSeconds() noexcept
{
    // The host tail ends where R135/R136/R137 close the Loudness VCA at
    // 0.5 V. D11's nominal nonlinear knee makes that later than tau*ln(15),
    // although the cap continues its inaudible physical tail afterward.
    const double continuous = envelopeReleaseTime(
        envelopeReferenceVolts, loudnessZeroVolts,
        envelopeResistance(1.0) + envelopeSenseOhms);
    // Backward Euler reaches the threshold up to ~2.5 samples later at the
    // supported 8 kHz floor. Four samples also cover threshold quantisation,
    // so the host never advertises a tail shorter than the discrete engine.
    return continuous + 4.0 / minimumSupportedSampleRate;
}

void GhostarEngine::reset()
{
    // The travel smoother snaps: whatever targets stand are what the
    // engine runs on from the first sample after a reset.
    parameters_ = targetParameters_;
    keyStackSize_ = 0;
    keyGate_ = false;
    envelopeGate_ = false;
    currentNote_ = -1;
    pendingTrigger_ = false;
    pendingLfoReset_ = false;
    pendingShaperTrigger_ = false;
    pitchBend_ = 0.0f;
    modWheel_ = 0.0f;
    shaperWheel_ = 0.0f;
    targetModWheel_ = 0.0f;
    targetShaperWheel_ = 0.0f;

    // P1017 C1, pedal C47/C48 and Glide C6 have no reset switch. Their
    // voltages and the keyboard DAC's last pitch therefore survive
    // panic/CC120 just as the live rear-jack states do. Fresh instances seed
    // every retained capacitor at its first selected source equilibrium.

    lfoCapLevel_ = -1.0;
    lfoRising_ = true;
    lfoSquareHigh_ = false;
    previousLfoSquareHigh_ = false;
    lfoKtSecondsRemaining_ = 0.0;
    lastLfoTriangle_ = -1.0;
    sampleHoldValue_ = 0.0;
    noise_.reset();

    shaperLevel_ = 0.0;
    shaperRising_ = true;
    shaperCycleActive_ = false;
    shaperGate_ = false;
    // Reset clears internally generated gates, but an inserted external
    // cable remains physically where it is. Seed both edge detectors from
    // its live selected level so reset/CC120 cannot turn a continuously held
    // HIGH into a fictitious unplug/replug edge on the next sample.
    const bool heldExternalGate = parameters_.gateYExt
        && externalGateJackInserted_
        && externalGateVolts_ > externalGateThresholdVolts;
    previousGateForShaper_ = heldExternalGate;
    previousEnvelopeXGate_ = false;
    previousEnvelopeYGate_ = heldExternalGate;
    previousEnvelopeGs_ = heldExternalGate;
    envelopeResetSamplesRemaining_ = 0;

    filterEnvelope_ = Adsr {};
    loudnessEnvelope_ = Adsr {};

    arpStep_ = 0;
    arpSoundingNote_ = -1;

    phaseA_ = 0.0;
    phaseB_ = 0.0;
    heldWaveA_ = naiveWave(parameters_.oscAWaveform, 0.0, oscADuty_);
    heldWaveformA_ = parameters_.oscAWaveform;
    heldTriA_ = triangleWave(0.0);
    heldWaveB_ = naiveWave(parameters_.oscBWaveform, 0.0, oscBDuty_);
    heldWaveformB_ = parameters_.oscBWaveform;
    heldTriB_ = triangleWave(0.0);
    heldDutyA_ = oscADuty_;
    heldDutyB_ = oscBDuty_;
    pitchLagA_ = PitchControlLag {};
    pitchLagB_ = PitchControlLag {};
    lastOscBWave_ = p1014SelectedWaveVolts(heldWaveformB_, heldWaveB_)
                  / selectedWaveVoltsPerEngineUnit;
    brightnessCompanion_ = 0.0;
    filterCouplingCompanion_ = 0.0;
    ringCouplingCompanion_ = 0.0;
    overdriveCouplingCompanion_ = 0.0;
    loudnessVcaNoiseState_ = 0x6d2b79f5u;
    shaperVcaNoiseState_ = 0xa511e9b3u;

    lowerSection_ = SvfSection {};
    lowerMixerCompanions_.fill(0.0);
    upperControlled_ = SvfSection {};
    upperFixed_ = SvfSection {};
    upperSlopeState_ = parameters_.slope;
    upperControlledLp_ = 0.0;
    upperFixedLp_ = 0.0;
    lowerHighQ_.chargeCompanion = 0.0;
    upperHighQ_.chargeCompanion = 0.0;

    externalStageBRing_.fill(0.0);
    externalStageBIndex_ = 0;
    externalStageARing_.fill(0.0);
    externalStageAIndex_ = 0;
    preMixerDelay_.fill(PreMixerFrame {});
    preMixerDelayIndex_ = 0;

    filterStageARing_.fill(0.0);
    shaperStageARing_.fill(0.0);
    stageAIndex_ = 0;
    filterStageBRing_.fill(0.0);
    shaperStageBRing_.fill(0.0);
    stageBIndex_ = 0;

}

void GhostarEngine::stopAllSound()
{
    // Implemented over reset() so the voice-killing list can never drift
    // out of step with it; only the controller positions survive — both
    // the smoothed wheel values and the targets they glide toward.
    const float pitchBend = pitchBend_;
    const float modWheel = modWheel_;
    const float shaperWheel = shaperWheel_;
    const float modTarget = targetModWheel_;
    const float shaperTarget = targetShaperWheel_;
    reset();
    pitchBend_ = pitchBend;
    modWheel_ = modWheel;
    shaperWheel_ = shaperWheel;
    targetModWheel_ = modTarget;
    targetShaperWheel_ = shaperTarget;
}

void GhostarEngine::setParameters(const EngineParameters& parameters)
{
    // A NaN smuggled in through host automation must neither reach the
    // control laws nor outlive the next valid set; every travel field is
    // normalised to a finite value in 0..1, falling back to its power-on
    // default.
    constexpr EngineParameters defaults {};
    EngineParameters sane = parameters;

    const auto travel = [](float value, float fallback) noexcept {
        return sanitisedTravel(value, fallback);
    };
    sane.tune = travel(parameters.tune, defaults.tune);
    sane.interval = travel(parameters.interval, defaults.interval);
    sane.masterVolume = travel(parameters.masterVolume, defaults.masterVolume);
    sane.brightness = travel(parameters.brightness, defaults.brightness);
    sane.shaperPathA = travel(parameters.shaperPathA, defaults.shaperPathA);
    sane.shaperPathB = travel(parameters.shaperPathB, defaults.shaperPathB);
    sane.shaperPathRing =
        travel(parameters.shaperPathRing, defaults.shaperPathRing);
    sane.shaperPathNoise =
        travel(parameters.shaperPathNoise, defaults.shaperPathNoise);
    sane.filterPathA = travel(parameters.filterPathA, defaults.filterPathA);
    sane.filterPathB = travel(parameters.filterPathB, defaults.filterPathB);
    sane.filterPathNoise =
        travel(parameters.filterPathNoise, defaults.filterPathNoise);
    sane.cutoff = travel(parameters.cutoff, defaults.cutoff);
    sane.lowerOnly = travel(parameters.lowerOnly, defaults.lowerOnly);
    sane.resonance = travel(parameters.resonance, defaults.resonance);
    sane.kbAmount = travel(parameters.kbAmount, defaults.kbAmount);
    sane.filterEnvAmount =
        travel(parameters.filterEnvAmount, defaults.filterEnvAmount);
    sane.filterAttack = travel(parameters.filterAttack, defaults.filterAttack);
    sane.filterDecay = travel(parameters.filterDecay, defaults.filterDecay);
    sane.filterSustain =
        travel(parameters.filterSustain, defaults.filterSustain);
    sane.filterRelease =
        travel(parameters.filterRelease, defaults.filterRelease);
    sane.loudnessAttack =
        travel(parameters.loudnessAttack, defaults.loudnessAttack);
    sane.loudnessDecay =
        travel(parameters.loudnessDecay, defaults.loudnessDecay);
    sane.loudnessSustain =
        travel(parameters.loudnessSustain, defaults.loudnessSustain);
    sane.loudnessRelease =
        travel(parameters.loudnessRelease, defaults.loudnessRelease);
    sane.lfoRate = travel(parameters.lfoRate, defaults.lfoRate);
    sane.shaperShape = travel(parameters.shaperShape, defaults.shaperShape);
    sane.shaperRate = travel(parameters.shaperRate, defaults.shaperRate);
    sane.glide = travel(parameters.glide, defaults.glide);

    sane.octave = sanitisedSwitch(parameters.octave, 4, defaults.octave);
    sane.oscAWaveform =
        sanitisedSwitch(parameters.oscAWaveform, 6, defaults.oscAWaveform);
    sane.oscBWaveform =
        sanitisedSwitch(parameters.oscBWaveform, 6, defaults.oscBWaveform);
    sane.oscBRange =
        sanitisedSwitch(parameters.oscBRange, 6, defaults.oscBRange);
    sane.lowerMode =
        sanitisedSwitch(parameters.lowerMode, 4, defaults.lowerMode);
    sane.slope = sanitisedSwitch(parameters.slope, 2, defaults.slope);
    sane.upperResonance = sanitisedSwitch(parameters.upperResonance, 2,
                                          defaults.upperResonance);
    sane.tracking =
        sanitisedSwitch(parameters.tracking, 2, defaults.tracking);
    sane.trigger = sanitisedSwitch(parameters.trigger, 2, defaults.trigger);
    sane.modSource =
        sanitisedSwitch(parameters.modSource, 6, defaults.modSource);
    sane.modXTo = sanitisedSwitch(parameters.modXTo, 6, defaults.modXTo);
    sane.shaperYTo =
        sanitisedSwitch(parameters.shaperYTo, 6, defaults.shaperYTo);
    sane.shaperMode =
        sanitisedSwitch(parameters.shaperMode, 4, defaults.shaperMode);
    sane.arpeggiator =
        sanitisedSwitch(parameters.arpeggiator, 4, defaults.arpeggiator);
    sane.glideMode =
        sanitisedSwitch(parameters.glideMode, 3, defaults.glideMode);

    // Continuous travels glide toward the new values (~25 ms, advanced per
    // sample in advanceControls) so block-latched automation and 7-bit CCs
    // never step the audio; switches always apply immediately. A fully
    // silent engine snaps instead, so a state restore before playing —
    // and a test configuring a law — lands exactly.
    const bool incomingQuiet =
        !sane.vcaBypass && sane.shaperPathA < 1.0e-4f
        && sane.shaperPathB < 1.0e-4f && sane.shaperPathRing < 1.0e-4f
        && sane.shaperPathNoise < 1.0e-4f;

    targetParameters_ = sane;
    if (silentForSnap() && incomingQuiet)
    {
        parameters_ = sane;
    }
    else
    {
        EngineParameters blended = sane;
        blended.tune = parameters_.tune;
        blended.interval = parameters_.interval;
        blended.masterVolume = parameters_.masterVolume;
        blended.brightness = parameters_.brightness;
        blended.shaperPathA = parameters_.shaperPathA;
        blended.shaperPathB = parameters_.shaperPathB;
        blended.shaperPathRing = parameters_.shaperPathRing;
        blended.shaperPathNoise = parameters_.shaperPathNoise;
        blended.filterPathA = parameters_.filterPathA;
        blended.filterPathB = parameters_.filterPathB;
        blended.filterPathNoise = parameters_.filterPathNoise;
        blended.cutoff = parameters_.cutoff;
        blended.lowerOnly = parameters_.lowerOnly;
        blended.resonance = parameters_.resonance;
        blended.kbAmount = parameters_.kbAmount;
        blended.filterEnvAmount = parameters_.filterEnvAmount;
        blended.filterAttack = parameters_.filterAttack;
        blended.filterDecay = parameters_.filterDecay;
        blended.filterSustain = parameters_.filterSustain;
        blended.filterRelease = parameters_.filterRelease;
        blended.loudnessAttack = parameters_.loudnessAttack;
        blended.loudnessDecay = parameters_.loudnessDecay;
        blended.loudnessSustain = parameters_.loudnessSustain;
        blended.loudnessRelease = parameters_.loudnessRelease;
        blended.lfoRate = parameters_.lfoRate;
        blended.shaperShape = parameters_.shaperShape;
        blended.shaperRate = parameters_.shaperRate;
        blended.glide = parameters_.glide;
        parameters_ = blended;
    }
    oscADuty_ = dutyFor(parameters_.oscAWaveform, true);
    oscBDuty_ = dutyFor(parameters_.oscBWaveform, false);
}

void GhostarEngine::noteOn(int midiNote, float velocity)
{
    if (midiNote < 0 || midiNote > 127)
        return;

    // Running status lets a MIDI sender encode Note Off as Note On with
    // velocity zero; treating it as a press would hold the gate open
    // forever. Beyond that the hardware keyboard has no velocity at all.
    if (!(velocity > 0.0f))
    {
        noteOff(midiNote);
        return;
    }

    const bool startsFreshArpeggiatorPhrase = keyStackSize_ == 0;

    // Re-pressing a held key moves it to the top of the stack rather than
    // duplicating it. The stack spans the whole MIDI note domain, so after
    // deduplication it cannot be full.
    for (int index = 0; index < keyStackSize_; ++index)
    {
        if (keyStack_[static_cast<std::size_t>(index)] == midiNote)
        {
            for (int shift = index; shift < keyStackSize_ - 1; ++shift)
                keyStack_[static_cast<std::size_t>(shift)] =
                    keyStack_[static_cast<std::size_t>(shift + 1)];
            --keyStackSize_;
            break;
        }
    }
    keyStack_[static_cast<std::size_t>(keyStackSize_)] =
        static_cast<std::int16_t>(midiNote);
    ++keyStackSize_;

    // Every newly held group is a new bottom-to-top scan.  Do this at the
    // zero-to-one key transition rather than waiting for an LFO edge to
    // notice an empty stack: a short no-key gap between clocks must not let
    // the next phrase inherit the preceding phrase's step.
    if (startsFreshArpeggiatorPhrase)
        arpStep_ = 0;

    keyGate_ = true;
    currentNote_ = midiNote;

    // MULTIPLE sends every new-key KT pulse into P1015's shared reset-pulse
    // network. SINGLE has no KT branch: its first attack comes from the
    // selected gate bus rising, and a legato press (or one hidden under an
    // already-high X/Y gate) does nothing. The Shaper's RESET mode remains
    // multiple-trigger regardless of that switch, so it records every press.
    if (parameters_.trigger == TriggerMode::Multiple)
        pendingTrigger_ = true;
    pendingLfoReset_ = true;
    pendingShaperTrigger_ = true;
}

void GhostarEngine::noteOff(int midiNote)
{
    for (int index = 0; index < keyStackSize_; ++index)
    {
        if (keyStack_[static_cast<std::size_t>(index)] == midiNote)
        {
            for (int shift = index; shift < keyStackSize_ - 1; ++shift)
                keyStack_[static_cast<std::size_t>(shift)] =
                    keyStack_[static_cast<std::size_t>(shift + 1)];
            --keyStackSize_;
            break;
        }
    }

    if (midiNote != currentNote_)
        return;

    if (keyStackSize_ > 0)
    {
        // Fall back to the newest key still held, at its pitch, without
        // retriggering — the hardware scanner's held-note memory.
        currentNote_ = keyStack_[static_cast<std::size_t>(keyStackSize_ - 1)];
        return;
    }

    keyGate_ = false;
}

void GhostarEngine::releaseAllKeys() noexcept
{
    keyStackSize_ = 0;
    keyGate_ = false;
}

void GhostarEngine::setPitchBend(float normalisedBipolar) noexcept
{
    if (!std::isfinite(normalisedBipolar))
        normalisedBipolar = 0.0f;
    pitchBend_ = std::clamp(normalisedBipolar, -1.0f, 1.0f);
}

void GhostarEngine::setExternalGateInput(bool jackInserted,
                                         double volts) noexcept
{
    externalGateJackInserted_ = jackInserted;
    externalGateVolts_ = std::isfinite(volts) ? volts : 0.0;
}

void GhostarEngine::setExternalPitchInput(bool jackInserted,
                                          double sourceVolts) noexcept
{
    externalPitchJackInserted_ = jackInserted;
    externalPitchSourceVolts_ = std::isfinite(sourceVolts)
        ? sourceVolts : 0.0;
}

void GhostarEngine::setOscBPedalInput(bool jackInserted,
                                      double resistanceKOhm) noexcept
{
    oscBPedalJackInserted_ = jackInserted;
    oscBPedalResistanceKOhm_ = std::clamp(
        std::isfinite(resistanceKOhm) ? resistanceKOhm : 100.0,
        0.0, 100.0);
}

void GhostarEngine::setFilterPedalInput(bool jackInserted,
                                        double resistanceKOhm) noexcept
{
    filterPedalJackInserted_ = jackInserted;
    filterPedalResistanceKOhm_ = std::clamp(
        std::isfinite(resistanceKOhm) ? resistanceKOhm : 100.0,
        0.0, 100.0);
}

void GhostarEngine::setExternalAudioInput(bool jackInserted) noexcept
{
    externalAudioJackInserted_ = jackInserted;
}

void GhostarEngine::setModWheel(float amount) noexcept
{
    if (!std::isfinite(amount))
        amount = 0.0f;
    targetModWheel_ = std::clamp(amount, 0.0f, 1.0f);
    // A restored wheel position lands exactly while nothing sounds, like
    // the panel travels; only a ridden wheel glides.
    if (silentForSnap())
        modWheel_ = targetModWheel_;
}

void GhostarEngine::setShaperWheel(float amount) noexcept
{
    if (!std::isfinite(amount))
        amount = 0.0f;
    targetShaperWheel_ = std::clamp(amount, 0.0f, 1.0f);
    if (silentForSnap())
        shaperWheel_ = targetShaperWheel_;
}

GhostarEngine::SvfOutputs GhostarEngine::runSection(SvfSection& section,
                                                double input, double g,
                                                double k,
                                                HighQBranch* highQ,
                                                double chargeStep) noexcept
{
    const double a = 1.0 / (1.0 + g * (g + k));
    const double baselineBp =
        a * (section.ic1 + g * (input - section.ic2));

    if (highQ == nullptr)
    {
        const double lp = section.ic2 + g * baselineBp;
        const double hp = input - k * baselineBp - lp;
        section.ic1 = trapezoidalCompanion(baselineBp, section.ic1);
        section.ic2 = trapezoidalCompanion(lp, section.ic2);
        return { lp, baselineBp, hp };
    }

    // BP sense -> non-inverting TL082 gain -> BA130 pair -> 1 nF -> the
    // same half's LP timing node. The 1 nF charge is a third trapezoidal
    // companion; solving its endpoint current together with BP/LP conserves
    // charge and keeps the result independent of host rate.
    const double bpPerAmp = -a * g * chargeStep;
    const double baselineLp = section.ic2 + g * baselineBp;
    const double driveVolts = filterNodeVoltsPerUnit
        * (highQ->amplifierGain * baselineBp - baselineLp
           - highQCapacitanceRatio * highQ->chargeCompanion);
    const double seriesOhms = highQ->sourceResistanceOhms
        + filterNodeVoltsPerUnit
            * (g * bpPerAmp
               + (1.0 + highQCapacitanceRatio) * chargeStep
               - highQ->amplifierGain * bpPerAmp);
    const double current = diodePairCurrent(driveVolts, seriesOhms);

    const double bp = baselineBp + bpPerAmp * current;
    const double lp = baselineLp
                    + (g * bpPerAmp + chargeStep) * current;
    const double hp = input - k * bp - lp;
    const double charge = highQ->chargeCompanion + chargeStep * current;

    section.ic1 = trapezoidalCompanion(bp, section.ic1);
    section.ic2 = trapezoidalCompanion(lp, section.ic2);
    highQ->chargeCompanion = trapezoidalCompanion(
        charge, highQ->chargeCompanion);
    return { lp, bp, hp };
}

void GhostarEngine::selectUpperSlope(UpperSlope slope) noexcept
{
    if (slope == upperSlopeState_)
        return;

    // C40 leaves the old VLP node holding its physical endpoint voltage,
    // then meets the other section's 22 nF timing capacitor. With contact
    // resistance negligible on an audio timescale, charge conservation fixes
    // the new endpoint. The same instantaneous delta belongs in that node's
    // trapezoidal companion; the old node and C37 state do not move.
    const double c40Voltage = upperSlopeState_ == UpperSlope::TwelveDb
        ? upperControlledLp_ : upperFixedLp_;
    SvfSection& selectedSection = slope == UpperSlope::TwelveDb
        ? upperControlled_ : upperFixed_;
    double& selectedLp = slope == UpperSlope::TwelveDb
        ? upperControlledLp_ : upperFixedLp_;
    const double shared =
        (cemTimingCapacitance * selectedLp
         + upperSlopeMemoryCapacitance * c40Voltage)
        / upperSelectedLpCapacitance;
    selectedSection.ic2 = flushDenormal(
        selectedSection.ic2 + shared - selectedLp);
    selectedLp = flushDenormal(shared);
    upperSlopeState_ = slope;
}

double GhostarEngine::runUpperCascade(double input, double g,
                                      double controlledK,
                                      double controlledInputGain,
                                      UpperSlope slope) noexcept
{
    selectUpperSlope(slope);

    const bool twelveDb = slope == UpperSlope::TwelveDb;
    const double controlledLpCapacitance = twelveDb
        ? upperSelectedLpCapacitance : cemTimingCapacitance;
    const double fixedLpCapacitance = twelveDb
        ? cemTimingCapacitance : upperSelectedLpCapacitance;
    const double controlledLpG =
        g * cemTimingCapacitance / controlledLpCapacitance;
    const double fixedLpG =
        g * cemTimingCapacitance / fixedLpCapacitance;
    const double controlledCoupling = twelveDb
        ? 1.0 / (2.0 * internalRate_ * upperLpCouplingOhms
                 * controlledLpCapacitance)
        : 0.0;
    const double fixedCoupling = twelveDb
        ? 1.0 / (2.0 * internalRate_ * upperLpCouplingOhms
                 * fixedLpCapacitance)
        : 0.0;

    // Eliminate each BP node from its trapezoidal equation. The controlled
    // VIF+VIV input is u*(1+1/Q_commanded); the fixed Q=0.5 half similarly
    // receives 3*VLP1. What remains is the coupled 2x2 VLP solve created by
    // R194. In 24 dB that resistor is shorted by SW4 and both off-diagonal
    // terms vanish.
    const double controlledBpDenominator = 1.0 + g * controlledK;
    const double controlledBpBase =
        (upperControlled_.ic1 + g * controlledInputGain * input)
        / controlledBpDenominator;
    const double controlledBpFromLp = g / controlledBpDenominator;

    const double fixedBpDenominator = 1.0 + g * lowSwitchDamping;
    const double fixedBpBase = upperFixed_.ic1 / fixedBpDenominator;
    const double fixedBpFromControlledLp =
        g * upperFixedInputGain / fixedBpDenominator;
    const double fixedBpFromLp = g / fixedBpDenominator;

    const double l11 = 1.0 + controlledCoupling
        + controlledLpG * controlledBpFromLp;
    const double l12 = -controlledCoupling;
    const double l21 = -fixedCoupling
        - fixedLpG * fixedBpFromControlledLp;
    const double l22 = 1.0 + fixedCoupling
        + fixedLpG * fixedBpFromLp;
    const double rhsControlled = upperControlled_.ic2
        + controlledLpG * controlledBpBase;
    const double rhsFixed = upperFixed_.ic2
        + fixedLpG * fixedBpBase;
    const double determinant = l11 * l22 - l12 * l21;

    const double baselineControlledLp =
        (rhsControlled * l22 - l12 * rhsFixed) / determinant;
    const double baselineFixedLp =
        (l11 * rhsFixed - l21 * rhsControlled) / determinant;
    const double baselineControlledBp = controlledBpBase
        - controlledBpFromLp * baselineControlledLp;
    const double baselineFixedBp = fixedBpBase
        + fixedBpFromControlledLp * baselineControlledLp
        - fixedBpFromLp * baselineFixedLp;

    // C37 injects into the controlled VLP node. Its response includes the
    // downstream section and, in 12 dB, R194's return path. C37's own charge
    // companion remains normalised to the nominal 22 nF timing capacitor;
    // only its deposit into the selected 23 nF node changes with SLOPE.
    const double controlledChargeStep = highQChargeStep_
        * cemTimingCapacitance / controlledLpCapacitance;
    const double controlledLpPerAmp =
        controlledChargeStep * l22 / determinant;
    const double fixedLpPerAmp =
        -controlledChargeStep * l21 / determinant;
    const double controlledBpPerAmp =
        -controlledBpFromLp * controlledLpPerAmp;
    const double fixedBpPerAmp =
        fixedBpFromControlledLp * controlledLpPerAmp
        - fixedBpFromLp * fixedLpPerAmp;

    const double driveVolts = filterNodeVoltsPerUnit
        * (upperHighQ_.amplifierGain * baselineControlledBp
           - baselineControlledLp
           - highQCapacitanceRatio * upperHighQ_.chargeCompanion);
    const double seriesOhms = upperHighQ_.sourceResistanceOhms
        + filterNodeVoltsPerUnit
            * (controlledLpPerAmp
               + highQCapacitanceRatio * highQChargeStep_
               - upperHighQ_.amplifierGain * controlledBpPerAmp);
    const double current = diodePairCurrent(driveVolts, seriesOhms);

    const double controlledLp = baselineControlledLp
                              + controlledLpPerAmp * current;
    const double fixedLp = baselineFixedLp + fixedLpPerAmp * current;
    const double controlledBp = baselineControlledBp
                              + controlledBpPerAmp * current;
    const double fixedBp = baselineFixedBp + fixedBpPerAmp * current;
    const double highQCharge = upperHighQ_.chargeCompanion
                             + highQChargeStep_ * current;

    upperControlled_.ic1 = trapezoidalCompanion(
        controlledBp, upperControlled_.ic1);
    upperControlled_.ic2 = trapezoidalCompanion(
        controlledLp, upperControlled_.ic2);
    upperFixed_.ic1 = trapezoidalCompanion(fixedBp, upperFixed_.ic1);
    upperFixed_.ic2 = trapezoidalCompanion(fixedLp, upperFixed_.ic2);
    upperHighQ_.chargeCompanion = trapezoidalCompanion(
        highQCharge, upperHighQ_.chargeCompanion);
    upperControlledLp_ = controlledLp;
    upperFixedLp_ = fixedLp;

    // IC14B's absolute gain belongs at the physical Upper output. SW4 opens
    // one 470 ohm leg at 24 dB, changing that gain from 201 to 101.
    return twelveDb ? upperTwelveDbOutputGain * controlledLp
                    : upperTwentyFourDbOutputGain * fixedLp;
}

GhostarEngine::SvfOutputs GhostarEngine::runLowerSection(
    const std::array<double, 3>& sourceTops,
    const std::array<double, 3>& sliderTravels,
    double dryInput, double g, double k) noexcept
{
    // Each unbuffered 100k slider is a Thevenin source whose wiper reaches
    // VLP through 220k and VBP through 68p. Neither destination is virtual
    // earth, so all three wipers depend on both CEM state nodes. Substitute
    // those three affine wiper equations into the two trapezoidal CEM state
    // equations: the whole production network then remains one 2x2 solve.
    const double capConductance = 2.0 * internalRate_
                                * lowerMixerCapacitance;
    const double resistanceStep = 1.0
        / (2.0 * internalRate_ * cemTimingCapacitance
           * lowerMixerResistanceOhms);

    std::array<double, 3> a0 {};
    std::array<double, 3> aLp {};
    std::array<double, 3> aBp {};
    double sumA0 = 0.0;
    double sumALp = 0.0;
    double sumABp = 0.0;
    double sumCompanion = 0.0;
    for (std::size_t index = 0; index < sourceTops.size(); ++index)
    {
        const double travel = clamp01(sliderTravels[index]);
        const double theveninOhms = lowerMixerPotOhms
                                  * travel * (1.0 - travel);
        const double source = travel * sourceTops[index];
        const double denominator = 1.0 + theveninOhms
            * (1.0 / lowerMixerResistanceOhms + capConductance);
        a0[index] = (source + theveninOhms * capConductance
                                * lowerMixerCompanions_[index])
                  / denominator;
        aLp[index] = theveninOhms / lowerMixerResistanceOhms
                   / denominator;
        aBp[index] = theveninOhms * capConductance / denominator;
        sumA0 += a0[index];
        sumALp += aLp[index];
        sumABp += aBp[index];
        sumCompanion += lowerMixerCompanions_[index];
    }

    constexpr double branchCount = 3.0;
    const double l11 = 1.0
        + resistanceStep * (branchCount - sumALp);
    const double l12 = -(g + resistanceStep * sumABp);
    const double l21 = g - lowerMixerCapacitanceRatio * sumALp;
    const double l22 = 1.0 + g * k
        + lowerMixerCapacitanceRatio * (branchCount - sumABp);
    const double rhsLp = lowerSection_.ic2 + resistanceStep * sumA0;
    const double rhsBp = lowerSection_.ic1
        + lowerMixerCapacitanceRatio * (sumA0 - sumCompanion);
    const double determinant = l11 * l22 - l12 * l21;

    const double baselineLp = (rhsLp * l22 - l12 * rhsBp) / determinant;
    const double baselineBp = (l11 * rhsBp - l21 * rhsLp) / determinant;
    const double lpPerAmp = highQChargeStep_ * l22 / determinant;
    const double bpPerAmp = -highQChargeStep_ * l21 / determinant;

    // C33's BA130 endpoint current is implicit in both CEM states. Their
    // derivatives above turn the remaining nonlinear equation into the same
    // monotone diode-pair solve used by the Upper section.
    const double driveVolts = filterNodeVoltsPerUnit
        * (lowerHighQ_.amplifierGain * baselineBp - baselineLp
           - highQCapacitanceRatio * lowerHighQ_.chargeCompanion);
    const double seriesOhms = lowerHighQ_.sourceResistanceOhms
        + filterNodeVoltsPerUnit
            * (lpPerAmp
               + highQCapacitanceRatio * highQChargeStep_
               - lowerHighQ_.amplifierGain * bpPerAmp);
    const double current = diodePairCurrent(driveVolts, seriesOhms);
    const double lp = baselineLp + lpPerAmp * current;
    const double bp = baselineBp + bpPerAmp * current;
    const double highQCharge = lowerHighQ_.chargeCompanion
                             + highQChargeStep_ * current;

    double nextBpCompanion = trapezoidalCompanion(bp, lowerSection_.ic1);
    lowerSection_.ic2 = trapezoidalCompanion(lp, lowerSection_.ic2);
    lowerHighQ_.chargeCompanion = trapezoidalCompanion(
        highQCharge, lowerHighQ_.chargeCompanion);

    for (std::size_t index = 0; index < sourceTops.size(); ++index)
    {
        const double wiper = a0[index] + aLp[index] * lp
                           + aBp[index] * bp;
        const double capacitorVoltage = wiper - bp;
        double nextCompanion = trapezoidalCompanion(
            capacitorVoltage, lowerMixerCompanions_[index]);

        // At either exact pot end the zero-ohm source fixes the wiper. A
        // trapezoidal capacitor can otherwise hide an alternating companion
        // mode there. Project it to the known voltage and transfer the same
        // discrete charge into VBP's companion, preserving the solved state.
        const double travel = clamp01(sliderTravels[index]);
        if (travel == 0.0 || travel == 1.0)
        {
            nextBpCompanion += lowerMixerCapacitanceRatio
                             * (capacitorVoltage - nextCompanion);
            nextCompanion = capacitorVoltage;
        }
        lowerMixerCompanions_[index] = flushDenormal(nextCompanion);
    }
    lowerSection_.ic1 = flushDenormal(nextBpCompanion);
    return { lp, bp, dryInput - k * bp - lp };
}

double GhostarEngine::processOverdrive(double lowerLowpass) noexcept
{
    const double inputVolts = filterNodeVoltsPerUnit * lowerLowpass;
    const double diodeCurrent = diodePairCurrent(
        overdriveDriveGain * inputVolts, overdriveDiodeSeriesOhms);
    const double outputVolts = overdriveLinearGain * inputVolts
                             - overdriveFeedbackOhms * diodeCurrent;
    const double output = outputVolts / filterNodeVoltsPerUnit;

    // Physical C34 companion. Its left plate sees IC12A through R187 and the
    // clean Lower VLP state through R167; the latter is not a ground shunt.
    // R173 is the grounded load on the other plate.
    // Keeping the actual plate-voltage history is essential for the Spirit's
    // switch transient once the remaining RS7 throws are continuity-traced.
    const double sourceVoltage =
        (output * overdriveShuntOhms
         + lowerLowpass * overdrivePickupOhms)
        / (overdrivePickupOhms + overdriveShuntOhms);
    const double sourceOhms = overdrivePickupOhms * overdriveShuntOhms
                            / (overdrivePickupOhms
                               + overdriveShuntOhms);
    const double seriesOhms = sourceOhms + overdriveLoadOhms;
    const double capacitorCurrent = overdriveCouplingConductance_
        * (sourceVoltage - overdriveCouplingCompanion_)
        / (1.0 + overdriveCouplingConductance_ * seriesOhms);
    const double capacitorVoltage = sourceVoltage
                                   - seriesOhms * capacitorCurrent;
    overdriveCouplingCompanion_ = trapezoidalCompanion(
        capacitorVoltage, overdriveCouplingCompanion_);
    return overdriveLoadOhms * capacitorCurrent;
}

double GhostarEngine::processFilterCoupling(double input) noexcept
{
    const double current = filterCouplingG_
        * (input - filterCouplingCompanion_)
        / (1.0 + filterCouplingG_ * filterCouplingLoadOhms);
    const double output = filterCouplingLoadOhms * current;
    const double capacitorVoltage = input - output;
    filterCouplingCompanion_ = trapezoidalCompanion(
        capacitorVoltage, filterCouplingCompanion_);
    return output;
}

GhostarEngine::OutputWipers GhostarEngine::processOutputNetwork(
    double filterInput, double shaperInput, double masterTravel,
    bool split) noexcept
{
    return processOutputNetwork(filterInput, shaperInput, masterTravel, split,
                                controlBrightnessResistanceOhms_);
}

GhostarEngine::OutputWipers GhostarEngine::processOutputNetwork(
    double filterInput, double shaperInput, double masterTravel,
    bool split, double brightnessResistanceOhms) noexcept
{
    // Treat each CEM3360 output as its Norton current into the full 20k
    // Master track. When P1017's SHAPED jack is empty, its two 10k output
    // arms connect the wipers and cross-load both tracks; C18/P3 makes that
    // loading frequency- and BRIGHTNESS-dependent. This is the exact
    // high-impedance-destination MNA, not merely the equal-source half-sum.
    const double jf = filterInput / masterTrackOhms;
    const double js = shaperInput / masterTrackOhms;
    const double m = clamp01(masterTravel);
    const double h = split ? 0.0 : 1.0 / (2.0 * outputArmOhms);
    const double gb = brightnessG_
        / (1.0 + brightnessG_ * brightnessResistanceOhms);

    const double lowerTrack = m * masterTrackOhms;
    const double upperTrack = (1.0 - m) * masterTrackOhms;

    // Eliminate the Shaper top node into a Norton source at its wiper. This
    // 2x2 form is exact at both pot endpoints and avoids huge conductances.
    const double topDenominator = 1.0 + gb * upperTrack;
    const double inverseTop = 1.0 / topDenominator;
    const double shaperDrive = js + gb * brightnessCompanion_;
    const double shaperConductance = gb * inverseTop;
    const double shaperCurrent = shaperDrive * inverseTop;
    const double coupling = h * lowerTrack;
    const double filterDiagonal = 1.0 + coupling;
    const double shaperDiagonal =
        1.0 + coupling + shaperConductance * lowerTrack;
    const double filterSource = jf * lowerTrack;
    const double shaperSource = shaperCurrent * lowerTrack;
    const double determinant =
        filterDiagonal * shaperDiagonal - coupling * coupling;
    const double inverseDeterminant = 1.0 / determinant;
    const double filterWiper =
        (filterSource * shaperDiagonal + coupling * shaperSource)
        * inverseDeterminant;
    const double shaperWiper =
        (filterDiagonal * shaperSource + coupling * filterSource)
        * inverseDeterminant;
    const double shaperTop =
        (shaperWiper + upperTrack * shaperDrive)
        * inverseTop;

    const double branchCurrent = gb * (shaperTop - brightnessCompanion_);
    const double capacitorVoltage = shaperTop
        - brightnessResistanceOhms * branchCurrent;
    brightnessCompanion_ = trapezoidalCompanion(
        capacitorVoltage, brightnessCompanion_);
    return { filterWiper, shaperWiper, shaperTop };
}

double GhostarEngine::processRingModulator(double triangleA,
                                           double triangleB) noexcept
{
    const double current = ringCouplingG_
        * (triangleA - ringCouplingCompanion_)
        / (1.0 + ringCouplingG_ * ringCarrierLoadOhms);
    const double coupledA = ringCarrierLoadOhms * current;
    const double capacitorVoltage = triangleA - coupledA;
    ringCouplingCompanion_ = trapezoidalCompanion(
        capacitorVoltage, ringCouplingCompanion_);

    // With P2 adjusted to cancel A, the 0..4 V CEM3340 triangle and
    // R23/R24 bias make B's bipolar coefficient 15/26. The A branch's
    // removed 2 V midpoint contributes the other factor of two.
    return -(15.0 / 13.0) * coupledA * triangleB;
}

void GhostarEngine::advanceEnvelope(Adsr& envelope, bool gate, bool triggerPulse,
                                  double attackCoefficient,
                                  double attackPeak,
                                  double decayCoefficient,
                                  double releaseResistanceOhms,
                                  double sustain) noexcept
{
    if (triggerPulse)
        envelope.stage = Adsr::Stage::Attack;
    else if (!gate && envelope.stage != Adsr::Stage::Idle)
        envelope.stage = Adsr::Stage::Release;

    switch (envelope.stage)
    {
        case Adsr::Stage::Attack:
            // The 556 half charges the 4.7 µF cap from its OUTPUT pin
            // through its untyped steering diode and 2 MΩ attack slider, so it
            // aims at V_OH − V_D ≈ 9.2–10.1 V against the envelope peak the
            // service drawing labels +7.5 V at the control-voltage pin:
            // a ratio of ≈1.3, not the 1.5 of a monostable charging toward
            // its rail (SM DWG 3; derived, see OQ-04).
            // R23/R24 put the threshold node one 100-ohm current drop above
            // the cap. A retained cap already above that trip level is not
            // dumped on retrigger: the timer simply returns to Decay.
            if (envelope.level < attackPeak)
            {
                envelope.level +=
                    (attackAimRatio - envelope.level) * attackCoefficient;
                if (envelope.level >= attackPeak)
                {
                    envelope.level = attackPeak;
                    envelope.stage = Adsr::Stage::Decay;
                }
            }
            else
                envelope.stage = Adsr::Stage::Decay;
            break;
        case Adsr::Stage::Decay:
        {
            // Both 100k sustain tracks share D15's biased lower rail. The
            // affine target makes nominal Loudness gain equal panel sustain:
            // the floor is the same 0.5 V at which its VCA closes.
            const double target = envelopeSustainFloorLevel
                + clamp01(sustain) * (1.0 - envelopeSustainFloorLevel);
            envelope.level += (target - envelope.level) * decayCoefficient;
            break;
        }
        case Adsr::Stage::Release:
        {
            // C*dV/dt=-i and V=R*i+a*log(1+i/Is). Backward Euler reduces to
            // the same diode solve with R+h/C, so the long knee is stable at
            // the 8 kHz test floor without adding another hidden state.
            const double oldVolts =
                envelopeReferenceVolts * envelope.level;
            const double h = 1.0 / sampleRate_;
            const double current = envelopeDiodeCurrent(
                oldVolts,
                releaseResistanceOhms + h / envelopeCapacitance);
            const double newVolts = oldVolts
                - h * current / envelopeCapacitance;
            envelope.level = std::max(
                0.0, newVolts / envelopeReferenceVolts);
            if (envelope.level < envelopeIdleLevel)
            {
                envelope.level = 0.0;
                envelope.stage = Adsr::Stage::Idle;
            }
            break;
        }
        case Adsr::Stage::Idle:
            break;
    }
}

bool GhostarEngine::handleArpClock() noexcept
{
    if (parameters_.arpeggiator == ArpeggiatorMode::Off || keyStackSize_ == 0)
    {
        arpSoundingNote_ = -1;
        arpStep_ = 0;
        return false;
    }

    // The scan is chromatic bottom-to-top of whatever is held, wrapped.
    std::array<std::int16_t, keyStackCapacity> sorted {};
    for (int index = 0; index < keyStackSize_; ++index)
        sorted[static_cast<std::size_t>(index)] =
            keyStack_[static_cast<std::size_t>(index)];
    std::sort(sorted.begin(), sorted.begin() + keyStackSize_);

    const int count = keyStackSize_;
    const int noteIndex = arpStep_ % count;
    static constexpr std::array<int, 3> octavePattern { 0, 12, -12 };

    int octaveOffset = 0;
    switch (parameters_.arpeggiator)
    {
        case ArpeggiatorMode::Ripple:
            break;
        case ArpeggiatorMode::Arpeggio:
            // The whole held sequence at pitch, then one octave up, then one
            // octave down, repeating.
            octaveOffset = octavePattern[static_cast<std::size_t>(
                (arpStep_ / count) % 3)];
            break;
        case ArpeggiatorMode::Leap:
            // The octave pattern advances per note, not per pass.
            octaveOffset =
                octavePattern[static_cast<std::size_t>(arpStep_ % 3)];
            break;
        case ArpeggiatorMode::Off:
            break;
    }

    // The transposed value is an internal CV, not a MIDI event: clamping it
    // to the MIDI domain would turn the documented octave step into seven
    // semitones at the extremes. The oscillator already bounds its rendered
    // frequency.
    arpSoundingNote_ =
        static_cast<int>(sorted[static_cast<std::size_t>(noteIndex)])
        + octaveOffset;
    ++arpStep_;
    return true;
}

// D11's physical capacitor tail continues far below the Loudness VCA's
// 0.5 V zero. Once that path carries no program signal it is safe for
// parameter restores to snap, while the retained cap state and output-device
// floor keep advancing for an authentic later retrigger.
bool GhostarEngine::silentForSnap() const noexcept
{
    const bool loudnessClosed =
        loudnessEnvelope_.stage == Adsr::Stage::Idle
        || (loudnessEnvelope_.stage == Adsr::Stage::Release
            && loudnessEnvelope_.level <= loudnessZeroLevel);
    return loudnessClosed
        && keyStackSize_ == 0 && !parameters_.vcaBypass
        && !targetParameters_.vcaBypass
        && parameters_.shaperPathA < 1.0e-4f
        && parameters_.shaperPathB < 1.0e-4f
        && parameters_.shaperPathRing < 1.0e-4f
        && parameters_.shaperPathNoise < 1.0e-4f;
}

double GhostarEngine::consumeLfoKtDuration(
    double& ktSecondsRemaining, double intervalSeconds) noexcept
{
    const double ktDuration = std::min(
        std::max(0.0, ktSecondsRemaining),
        std::max(0.0, intervalSeconds));
    ktSecondsRemaining = std::max(0.0, ktSecondsRemaining - ktDuration);
    return ktDuration;
}

void GhostarEngine::advanceControls() noexcept
{
    const double dt = 1.0 / sampleRate_;

    // The travel smoother: every continuous panel value and both wheels
    // glide to their latched targets (~25 ms), per the plan's Step 5. A
    // value lands exactly once it is within hearing of its target, so the
    // one-pole cannot stall on a float residue short of it.
    {
        const float k = static_cast<float>(travelSmoothing_);
        const auto follow = [k](float& value, float target) noexcept {
            value += k * (target - value);
            if (std::fabs(target - value) < 1.0e-6f)
                value = target;
        };
        const EngineParameters& t = targetParameters_;
        follow(parameters_.tune, t.tune);
        follow(parameters_.interval, t.interval);
        follow(parameters_.masterVolume, t.masterVolume);
        follow(parameters_.brightness, t.brightness);
        follow(parameters_.shaperPathA, t.shaperPathA);
        follow(parameters_.shaperPathB, t.shaperPathB);
        follow(parameters_.shaperPathRing, t.shaperPathRing);
        follow(parameters_.shaperPathNoise, t.shaperPathNoise);
        follow(parameters_.filterPathA, t.filterPathA);
        follow(parameters_.filterPathB, t.filterPathB);
        follow(parameters_.filterPathNoise, t.filterPathNoise);
        follow(parameters_.cutoff, t.cutoff);
        follow(parameters_.lowerOnly, t.lowerOnly);
        follow(parameters_.resonance, t.resonance);
        follow(parameters_.kbAmount, t.kbAmount);
        follow(parameters_.filterEnvAmount, t.filterEnvAmount);
        follow(parameters_.filterAttack, t.filterAttack);
        follow(parameters_.filterDecay, t.filterDecay);
        follow(parameters_.filterSustain, t.filterSustain);
        follow(parameters_.filterRelease, t.filterRelease);
        follow(parameters_.loudnessAttack, t.loudnessAttack);
        follow(parameters_.loudnessDecay, t.loudnessDecay);
        follow(parameters_.loudnessSustain, t.loudnessSustain);
        follow(parameters_.loudnessRelease, t.loudnessRelease);
        follow(parameters_.lfoRate, t.lfoRate);
        follow(parameters_.shaperShape, t.shaperShape);
        follow(parameters_.shaperRate, t.shaperRate);
        follow(parameters_.glide, t.glide);
        follow(modWheel_, targetModWheel_);
        follow(shaperWheel_, targetShaperWheel_);
    }

    const EngineParameters& p = parameters_;

    // Cache the virtual-earth slider laws once per host sample. The Filter
    // values now serve only RS7's still-open dry-output seam; its live state
    // is driven by the full moving-node mixer below.
    controlFilterMixA_ =
        filterDryMixGain * loadedLinearPot(p.filterPathA, 220.0);
    controlFilterMixB_ =
        filterDryMixGain * loadedLinearPot(p.filterPathB, 220.0);
    controlFilterMixNoise_ =
        filterDryMixGain * loadedLinearPot(p.filterPathNoise, 220.0);
    controlShaperMixA_ =
        shaperMixGain * loadedLinearPot(p.shaperPathA, 47.0);
    controlShaperMixB_ =
        shaperMixGain * loadedLinearPot(p.shaperPathB, 47.0);
    controlShaperMixRing_ = shaperMixGain * shaperRingRelativeGain
        * loadedLinearPot(p.shaperPathRing, 6.8);
    controlShaperMixNoise_ =
        shaperMixGain * loadedLinearPot(p.shaperPathNoise, 47.0);

    // ---------------------------------------------------------------- Gate
    const bool anyGateSelected = p.gateKbd || p.gateX || p.gateYExt;

    // ----------------------------------------------------------------- LFO
    // P2=100k LIN is loaded by R33=200k into the CEM3360 log-converter node,
    // bending its electrical travel before the source-derived 44 dB span.
    const double loadedRateTravel = loadedLinearPot(p.lfoRate, 200.0);
    double lfoHz = exponentialTravel(loadedRateTravel, lfoSlowHz, lfoFastHz);
    if (p.shaperYTo == ShaperYDestination::LfoRate)
    {
        const double ySignal = clamp01(shaperLevel_) * shaperWheel_;
        lfoHz *= std::pow(60.0 / lfoHz, clamp01(ySignal));
        lfoHz = std::min(lfoHz, 60.0);
    }

    // P1016 makes LFO RESET = KT AND /AA, independently of GATE SELECT and
    // the SINGLE/MULTIPLE envelope switch. This model covers the ordinary
    // non-overlap branch. The width and propagation of a dynamic AA pulse are
    // not published, so coincident AA arbitration remains capture-owned
    // rather than being represented as a host-sample-sized pulse.
    struct LfoAdvance
    {
        double area;
        double highDuration;
        bool highFirst;
    };
    const auto advanceLfoCap = [lfoHz](double seconds, double& cap,
                                       bool& rising) noexcept -> LfoAdvance {
        if (cap >= 1.0)
            rising = false;
        else if (cap <= -1.0)
            rising = true;

        const double slope = 4.0 * lfoHz;
        if (rising)
        {
            const double crossing = (1.0 - cap) / slope;
            if (crossing <= seconds)
            {
                const double remainder = seconds - crossing;
                const double area = 0.5 * (cap + 1.0) * crossing
                                  + (1.0 - 0.5 * slope * remainder)
                                        * remainder;
                cap = 1.0 - slope * remainder;
                rising = false;
                return { area, crossing, true };
            }
            const double end = cap + slope * seconds;
            const double area = 0.5 * (cap + end) * seconds;
            cap = end;
            return { area, seconds, true };
        }

        const double crossing = (cap + 1.0) / slope;
        if (crossing <= seconds)
        {
            const double remainder = seconds - crossing;
            const double area = 0.5 * (cap - 1.0) * crossing
                              + (-1.0 + 0.5 * slope * remainder)
                                    * remainder;
            cap = -1.0 + slope * remainder;
            rising = true;
            return { area, remainder, false };
        }
        const double end = cap - slope * seconds;
        const double area = 0.5 * (cap + end) * seconds;
        cap = end;
        return { area, 0.0, false };
    };

    // P1015 Q2 does not discharge C13.
    // It clamps the TL068 output near the negative reversal voltage and
    // forces IC10B into the rising state while C13 keeps charging behind the
    // buffer. Keeping capacitor voltage and direction as separate states is
    // what preserves that charge and the short above-threshold recovery.
    if (pendingLfoReset_)
        lfoKtSecondsRemaining_ = std::max(
            lfoKtSecondsRemaining_, keyboardLfoResetSeconds);
    pendingLfoReset_ = false;

    const double lfoResetDuration = consumeLfoKtDuration(
        lfoKtSecondsRemaining_, dt);
    const bool lfoResetActive = lfoResetDuration > 0.0;
    if (lfoResetActive)
    {
        lfoRising_ = true;
        lfoCapLevel_ += 4.0 * lfoHz * lfoResetDuration;
    }
    // Releasing RESET above +5 V turns the comparator downward but does not
    // reflect or discard C13's overshoot. Integrating the remainder of this
    // host interval separately preserves the annotated 25 us at every rate.
    const double normalDuration = std::max(0.0, dt - lfoResetDuration);
    LfoAdvance normalAdvance { 0.0, 0.0, true };
    if (normalDuration > 0.0)
        normalAdvance = advanceLfoCap(
            normalDuration, lfoCapLevel_, lfoRising_);
    double lfoHighDuration = normalAdvance.highDuration;
    bool lfoHighFirst = normalAdvance.highFirst;
    if (lfoResetActive)
    {
        // RESET forces LG high from the start of the interval. From the
        // bounded +/-1 operating range, the remaining interval can contain
        // at most its later high-to-low reversal.
        lfoHighDuration += lfoResetDuration;
        lfoHighFirst = true;
    }
    double lfoTriangle = lfoCapLevel_;
    if (lfoResetActive)
    {
        // Controls are held over one host interval. Preserve a sub-sample
        // clamp as its interval average instead of discarding it outright;
        // when KT fills the interval this reduces exactly to the -5 V clamp.
        lfoTriangle = (-lfoResetDuration
                       + normalAdvance.area) / dt;
    }
    lastLfoTriangle_ = lfoTriangle;
    lfoSquareHigh_ = lfoRising_;
    const double lfoSquare = lfoResetActive
        ? 2.0 * lfoHighDuration / dt - 1.0
        : (lfoSquareHigh_ ? 1.0 : -1.0);
    // The S&H and arpeggiator clock on the square's rising edge — including
    // the very first one after a reset, so the arpeggiator's opening step is
    // the documented bottom-of-the-scan note, not a full clock period of the
    // last-pressed key.
    const bool resetClockEdge =
        lfoResetActive && !previousLfoSquareHigh_;
    const bool clockEdge = resetClockEdge
        || (lfoSquareHigh_ && !previousLfoSquareHigh_);
    previousLfoSquareHigh_ = lfoSquareHigh_;

    // P1013's RED NOISE branch is the R6/C8 junction followed by IC4B's
    // 2k2/100k non-inverting stage. Only its normalisation into the engine's
    // unit X bus remains voiced (OQ-17), held near the prior patch depth.
    const double redNoise =
        std::clamp(noise_.red() * redNoiseBusGain, -1.0, 1.0);

    bool arpeggiatorPulse = false;
    if (clockEdge)
    {
        sampleHoldValue_ = p.modSource == ModSource::SampleHoldY
                               ? shaperLevel_
                               : redNoise;
        // P1016 sends AA for every selected arpeggiator step to P1015's
        // 10 ms reset lane. An idle/off arpeggiator produces no AA pulse.
        arpeggiatorPulse = handleArpClock();
    }

    // ------------------------------------------------------------- Shaper Y
    // US 3,943,456: RATE sets the total period, SHAPE apportions it between
    // rise and fall without changing it. On P1015, D19/D20 steer opposite
    // ends of the 1M linear P4 through the same 27k R62. The two half-cycle
    // resistances therefore sum to 1M + 2*27k at every setting.
    // Arpeggiator modes clock-slave the Shaper to the LFO except in FREE.
    double shaperPeriod = exponentialTravel(1.0 - p.shaperRate, 0.045, 20.0);
    if (p.arpeggiator != ArpeggiatorMode::Off
        && p.shaperMode != ShaperMode::Free)
        shaperPeriod = 1.0 / std::max(lfoHz, 1.0e-3);
    const double riseFraction =
        (27.0 + 1000.0 * static_cast<double>(p.shaperShape)) / 1054.0;
    const double riseRate =
        1.0 / std::max(shaperPeriod * riseFraction, 1.0e-4);
    const double fallRate =
        1.0 / std::max(shaperPeriod * (1.0 - riseFraction), 1.0e-4);
    const double riseStep = dt * riseRate;
    const double fallStep = dt * fallRate;

    // J2/7 normals SG to the Y/EXT selector only while the EXTERNAL GATE
    // socket is empty. A plug opens that contact even when its tip is low;
    // the factory manual gives the following 4075 input a strict >6 V
    // threshold. Query this at both consumers because normalled SG can
    // legitimately change between the Shaper and envelope portions below.
    const auto yExtGate = [this]() noexcept {
        return externalGateJackInserted_
            ? externalGateVolts_ > externalGateThresholdVolts
            : shaperGate_;
    };
    const bool otherGateNow = (p.gateKbd && keyGate_)
        || (p.gateYExt && yExtGate());
    const bool combinedGateNow = anyGateSelected
        && (otherGateNow || (p.gateX && lfoSquareHigh_));
    // At host rates below 40 kHz, RESET can force LG high and return it low
    // inside one interval. Keep that real edge visible to the selected X gate
    // even though its final sampled level is low.
    const bool gateRise =
        (combinedGateNow || (p.gateX && clockEdge))
        && !previousGateForShaper_;
    // RESET mode is always multiple-trigger: every key press restarts it
    // regardless of the TRIGGER switch — including a legato press under
    // SINGLE. Key presses alone drive the reset; a gate-bus edge must not,
    // because with Y/EXT as the only selected source the Shaper's own gate
    // would clamp the Shaper back to zero the moment it crossed its own
    // threshold, and the single rise/fall cycle could never complete.
    const bool newKeyForShaper = pendingShaperTrigger_;
    const bool shaperRetrigger =
        p.shaperMode == ShaperMode::Reset && newKeyForShaper;
    // RUN obeys the panel's ordinary keyboard-trigger mode.  MULTIPLE's KT
    // pulse remains visible during legato, and RUN accepts it once its rise
    // has completed; SINGLE still requires a genuine selected-bus edge.
    const bool runKeyboardRetrigger =
        p.shaperMode == ShaperMode::Run
        && p.trigger == TriggerMode::Multiple && p.gateKbd
        && newKeyForShaper;
    pendingShaperTrigger_ = false;

    switch (p.shaperMode)
    {
        case ShaperMode::Free:
            // A free-running oscillator, symmetric about zero.
            if (shaperRising_)
            {
                shaperLevel_ += 2.0 * riseStep;
                if (shaperLevel_ >= 1.0)
                {
                    shaperLevel_ = 1.0;
                    shaperRising_ = false;
                }
            }
            else
            {
                shaperLevel_ -= 2.0 * fallStep;
                if (shaperLevel_ <= -1.0)
                {
                    shaperLevel_ = -1.0;
                    shaperRising_ = true;
                }
            }
            break;
        case ShaperMode::KbdHold:
        {
            // Rises while gated and holds at maximum; releases to zero. LG
            // can pulse entirely between host samples during keyboard reset,
            // so integrate its exact high/low durations rather than using
            // only the final sampled Gate-X level.
            double highDuration = 0.0;
            bool highFirst = true;
            if (otherGateNow)
                highDuration = dt;
            else if (p.gateX)
            {
                highDuration = std::clamp(lfoHighDuration, 0.0, dt);
                highFirst = lfoHighFirst;
            }

            const auto riseFor = [&](double seconds) {
                if (seconds <= 0.0)
                    return;
                shaperRising_ = shaperLevel_ < 1.0;
                if (shaperRising_)
                {
                    shaperLevel_ += seconds * riseRate;
                    if (shaperLevel_ >= 1.0)
                    {
                        shaperLevel_ = 1.0;
                        shaperRising_ = false;
                    }
                }
            };
            const auto fallFor = [&](double seconds) {
                if (seconds <= 0.0)
                    return;
                shaperRising_ = false;
                shaperLevel_ = std::max(
                    0.0, shaperLevel_ - seconds * fallRate);
            };

            if (highFirst)
            {
                riseFor(highDuration);
                fallFor(dt - highDuration);
            }
            else
            {
                fallFor(dt - highDuration);
                riseFor(highDuration);
            }
            break;
        }
        case ShaperMode::Reset:
            if (shaperRetrigger || (gateRise && !shaperCycleActive_))
            {
                shaperLevel_ = 0.0;
                shaperRising_ = true;
                shaperCycleActive_ = true;
            }
            [[fallthrough]];
        case ShaperMode::Run:
            if (p.shaperMode == ShaperMode::Run
                && (gateRise || runKeyboardRetrigger)
                && !(shaperCycleActive_ && shaperRising_))
            {
                // RUN never abandons a rise in progress; a new gate is
                // ignored until the rising segment has completed.
                shaperLevel_ = 0.0;
                shaperRising_ = true;
                shaperCycleActive_ = true;
            }
            if (shaperCycleActive_)
            {
                if (shaperRising_)
                {
                    shaperLevel_ += riseStep;
                    if (shaperLevel_ >= 1.0)
                    {
                        shaperLevel_ = 1.0;
                        shaperRising_ = false;
                    }
                }
                else
                {
                    shaperLevel_ -= fallStep;
                    if (shaperLevel_ <= 0.0)
                    {
                        shaperLevel_ = 0.0;
                        shaperCycleActive_ = false;
                    }
                }
            }
            break;
    }
    previousGateForShaper_ = combinedGateNow;

    // SG is IC6's hysteretic phase state, not a level detector. It is high
    // only on a rising leg and flips low at the positive reversal point. In
    // the envelope modes it then remains low through top hold, fall and the
    // zero-level idle state until the next accepted trigger (SM DWG 3).
    switch (p.shaperMode)
    {
        case ShaperMode::Free:
            shaperGate_ = shaperRising_;
            break;
        case ShaperMode::KbdHold:
            shaperGate_ = combinedGateNow && shaperRising_;
            break;
        case ShaperMode::Reset:
        case ShaperMode::Run:
            shaperGate_ = shaperCycleActive_ && shaperRising_;
            break;
    }

    // ------------------------------------------------------------ Envelopes
    // P1015 has two related but distinct networks. Its first 4075 OR makes
    // the selected gate bus. Its edge branches separately accept every X
    // and Y/EXT rise, plus every raw KT pulse in MULTIPLE, even while another
    // source already holds the OR'ed bus high. X/Y use the annotated ~5 ms
    // lane; KT and the arpeggiator's AA pulse use the distinct 10 ms lane.
    // All pull GS (both active-low 556 RESETs and both release-diode returns)
    // low, so the caps release during the notch; the final GS rise creates TS
    // and starts both attacks from their retained post-notch levels. SINGLE
    // has no KT branch: a keyboard press only attacks when it can raise the
    // selected bus itself (SM DWG 3, OQ-04).
    const bool selectedEnvelopeXGate = p.gateX && lfoSquareHigh_;
    const bool selectedEnvelopeYGate = p.gateYExt && yExtGate();
    const bool combinedEnvelopeGate = anyGateSelected
        && ((p.gateKbd && keyGate_) || selectedEnvelopeXGate
            || selectedEnvelopeYGate);
    const bool xRise = p.gateX
        && (clockEdge
            || (selectedEnvelopeXGate && !previousEnvelopeXGate_));
    const bool yRise = selectedEnvelopeYGate && !previousEnvelopeYGate_;
    // KT reaches the reset stretcher through the TRIGGER switch, before and
    // independently of the KBD gate-selector deck. A selected gate must still
    // be high when the notch ends for either envelope to attack.
    const bool multipleKeyPulse =
        p.trigger == TriggerMode::Multiple && pendingTrigger_;
    pendingTrigger_ = false;

    const auto resetSamples = [this](double seconds) {
        return static_cast<std::uint32_t>(
            std::max(1.0, std::round(seconds * sampleRate_)));
    };
    if (xRise || yRise)
    {
        envelopeResetSamplesRemaining_ = std::max(
            envelopeResetSamplesRemaining_,
            resetSamples(envelopeXyResetSeconds));
    }
    if (multipleKeyPulse || arpeggiatorPulse)
    {
        envelopeResetSamplesRemaining_ = std::max(
            envelopeResetSamplesRemaining_,
            resetSamples(envelopeKtAaResetSeconds));
    }

    const bool resetPulseActive = envelopeResetSamplesRemaining_ != 0;
    const bool gsHigh = combinedEnvelopeGate && !resetPulseActive;
    const bool triggerPulse = gsHigh && !previousEnvelopeGs_;
    previousEnvelopeXGate_ = selectedEnvelopeXGate;
    previousEnvelopeYGate_ = selectedEnvelopeYGate;
    previousEnvelopeGs_ = gsHigh;
    envelopeGate_ = combinedEnvelopeGate;

    // Every segment uses its 4.7 µF cap and own 2 MΩ log slider, so travel
    // maps to a *time constant*. Attack and decay are RC charges; release
    // includes the traced D11/D14 knee into the shared GS line. The panel's
    // labelled 5 ms–10 s is that time constant: (2 MΩ + 100 Ω) × 4.7 µF
    // is 9.40047 s, and the voiced ~1 kΩ effective slider endpoint plus
    // R23/R24 puts the fast end at 5.17 ms — one assumption making both printed
    // endpoints land, where the previous three-time-constants read fit
    // neither (derived from SM DWG 3, OQ-04).
    const auto segmentCoefficient = [dt](double travel) {
        return -std::expm1(-dt / envelopeTau(travel));
    };
    advanceEnvelope(filterEnvelope_, gsHigh, triggerPulse,
                    segmentCoefficient(p.filterAttack),
                    envelopeAttackPeak(p.filterAttack),
                    segmentCoefficient(p.filterDecay),
                    envelopeResistance(p.filterRelease) + envelopeSenseOhms,
                    static_cast<double>(p.filterSustain));
    advanceEnvelope(loudnessEnvelope_, gsHigh, triggerPulse,
                    segmentCoefficient(p.loudnessAttack),
                    envelopeAttackPeak(p.loudnessAttack),
                    segmentCoefficient(p.loudnessDecay),
                    envelopeResistance(p.loudnessRelease) + envelopeSenseOhms,
                    static_cast<double>(p.loudnessSustain));

    if (envelopeResetSamplesRemaining_ != 0)
        --envelopeResetSamplesRemaining_;

    // ---------------------------------------------------------- MOD X value
    // OSC B and the continuous R6/C8 -> IC4B RED NOISE branch are audio-rate
    // sources. Publish their routing to the voice, which reads the selected
    // source on every internal tick. The two S+H positions remain clocked and
    // held here, while the LFO sources remain safely below the host Nyquist.
    const bool audioRateSource = p.modSource == ModSource::OscB
                              || p.modSource == ModSource::RedNoise;
    double modXSource = 0.0;
    switch (p.modSource)
    {
        case ModSource::LfoTriangle:      modXSource = lfoTriangle; break;
        case ModSource::LfoSquare:        modXSource = lfoSquare; break;
        case ModSource::SampleHoldRandom: modXSource = sampleHoldValue_; break;
        case ModSource::SampleHoldY:      modXSource = sampleHoldValue_; break;
        case ModSource::RedNoise:
        case ModSource::OscB:
            // Both continuous sources are read in the voice, so neither is
            // sampled onto this host-rate control frame.
            break;
    }
    // SHAPE X WITH Y is the VCA ahead of the X wheel. The wheel itself is a
    // loaded rheostat, so its gain cannot be applied until the destination
    // switch has established the load below.
    const double xSourceGain = p.shapeXWithY
        ? clamp01(shaperLevel_) : 1.0;
    const double xSource = audioRateSource
        ? 0.0 : modXSource * xSourceGain;
    const double ySource = shaperLevel_;

    // X->Osc A and Y->Osc B retain one octave as separate source-scale
    // anchors; the drawings cannot close either CEM3360 top gain or the
    // loaded Shaper swing. Pitch, filter and RWM wheel travels follow the
    // P1013/14 loading; the active RWM conversion remains the explicitly
    // voiced ±0.42 duty seam (OQ-14).
    constexpr double dutyDepth = 0.42;

    double modAOctaves = 0.0;
    double modBOctaves = 0.0;
    double modUpperOctaves = 0.0;
    double modLowerOctaves = 0.0;
    controlPwmA_ = 0.0;
    controlPwmB_ = 0.0;
    controlAudioRateMod_ = AudioRateMod {};
    controlAudioRateMod_.source = p.modSource;
    controlAudioRateMod_.gain = audioRateSource ? xSourceGain : 0.0;

    // Where the X bus lands. An audio-rate source writes the same depths
    // into the published routing instead of into this sample's sums, so
    // exactly one of the two paths carries the modulation.
    auto& audioMod = controlAudioRateMod_;
    const auto routePitchA = [&](double depth) {
        (audioRateSource ? audioMod.aOctaves : modAOctaves) += depth;
    };
    const auto routePitchB = [&](double depth) {
        (audioRateSource ? audioMod.bOctaves : modBOctaves) += depth;
    };
    switch (p.modXTo)
    {
        case ModXDestination::Off: break;
        case ModXDestination::OscAB:
        {
            const double scale = modXWheelGain(
                modWheel_, oscillatorPairLoadKilohms);
            routePitchA(audioRateSource ? scale : xSource * scale);
            routePitchB(audioRateSource ? scale : xSource * scale);
            break;
        }
        case ModXDestination::OscA:
        {
            const double scale = modXWheelGain(
                modWheel_, oscillatorSingleLoadKilohms);
            routePitchA(audioRateSource ? scale : xSource * scale);
            break;
        }
        case ModXDestination::OscARwm:
        {
            const double scale =
                modXWheelGain(modWheel_, rwmDestinationLoadKilohms)
                / modXWheelGain(1.0, rwmDestinationLoadKilohms);
            if (audioRateSource)
                audioMod.duty = scale * dutyDepth;
            else
                controlPwmA_ = xSource * scale * dutyDepth;
            break;
        }
        case ModXDestination::FilterUL:
        {
            const double scale = modXWheelGain(
                modWheel_, 0.5 * filterModInputKilohms)
                * filterOctavesPerModVoltRelativeToOscillator;
            if (audioRateSource)
            {
                audioMod.upperOctaves += scale;
                audioMod.lowerOctaves += scale;
            }
            else
            {
                modUpperOctaves += xSource * scale;
                modLowerOctaves += xSource * scale;
            }
            break;
        }
        case ModXDestination::FilterU:
        {
            const double scale = modXWheelGain(
                modWheel_, filterModInputKilohms)
                * filterOctavesPerModVoltRelativeToOscillator;
            if (audioRateSource)
                audioMod.upperOctaves += scale;
            else
                modUpperOctaves += xSource * scale;
            break;
        }
    }
    // FORMANT cuts the lower filter off the modulation buses, at either
    // rate, exactly as it cuts the control-rate sum below.
    if (p.tracking != TrackingMode::Dynamic)
        audioMod.lowerOctaves = 0.0;
    audioMod.active = audioRateSource && audioMod.gain != 0.0
        && (audioMod.aOctaves != 0.0 || audioMod.bOctaves != 0.0
            || audioMod.upperOctaves != 0.0 || audioMod.lowerOctaves != 0.0
            || audioMod.duty != 0.0);
    switch (p.shaperYTo)
    {
        case ShaperYDestination::Off: break;
        case ShaperYDestination::OscAB:
        {
            const double scale = shaperYWheelGain(
                shaperWheel_, oscillatorPairLoadKilohms);
            modAOctaves += ySource * scale;
            modBOctaves += ySource * scale;
            break;
        }
        case ShaperYDestination::OscB:
            modBOctaves += ySource * shaperYWheelGain(
                shaperWheel_, oscillatorSingleLoadKilohms);
            break;
        case ShaperYDestination::OscBRwm:
        {
            const double scale =
                shaperYWheelGain(shaperWheel_, rwmDestinationLoadKilohms)
                / shaperYWheelGain(1.0, rwmDestinationLoadKilohms);
            controlPwmB_ = ySource * scale * dutyDepth;
            break;
        }
        case ShaperYDestination::LfoRate:
            break; // consumed above, before the LFO advanced
        case ShaperYDestination::FilterL:
            modLowerOctaves += ySource * shaperYWheelGain(
                shaperWheel_, filterModInputKilohms)
                * filterOctavesPerModVoltRelativeToOscillator;
            break;
    }

    // Solve each capacitor against the complete parallel conductance. An
    // ordinary DSP start begins at the selected DC equilibrium; subsequent
    // cable/pedal and FILTER-mode changes retain charge. A zero-ohm pedal is
    // the exact grounded-node limit, avoiding an artificial epsilon.
    const auto advancePedalNode = [dt](
        bool inserted, double resistanceKOhm, double loadConductance,
        double& nodeVolts, bool& initialised) noexcept {
        const double openConductance = 1.0 / pedalPullupOhms
                                     + loadConductance;
        const double openVolts = (pedalPullupVolts / pedalPullupOhms)
                               / openConductance;
        double targetVolts = openVolts;
        double coefficient = -std::expm1(
            -dt * openConductance / pedalCapacitanceFarads);
        if (inserted)
        {
            if (resistanceKOhm == 0.0)
            {
                targetVolts = 0.0;
                coefficient = 1.0;
            }
            else
            {
                const double conductance = openConductance
                    + 1.0 / (resistanceKOhm * 1000.0);
                targetVolts = (pedalPullupVolts / pedalPullupOhms)
                            / conductance;
                coefficient = -std::expm1(
                    -dt * conductance / pedalCapacitanceFarads);
            }
        }
        if (!initialised)
        {
            nodeVolts = targetVolts;
            initialised = true;
        }
        else
        {
            nodeVolts = flushDenormal(
                nodeVolts + coefficient * (targetVolts - nodeVolts));
        }
        return openVolts;
    };

    const double oscBPedalOpenVolts = advancePedalNode(
        oscBPedalJackInserted_, oscBPedalResistanceKOhm_,
        1.0 / oscBPedalInputOhms,
        oscBPedalNodeVolts_, oscBPedalNodeInitialised_);
    modBOctaves += (oscBPedalNodeVolts_ - oscBPedalOpenVolts)
                 * 100.0e3 / oscBPedalInputOhms;

    const bool lowerDynamic = p.tracking == TrackingMode::Dynamic;
    const double filterPedalLoadConductance = 1.0 / filterPedalShuntOhms
        + (lowerDynamic ? 2.0 : 1.0) / filterPedalInputOhms;
    advancePedalNode(
        filterPedalJackInserted_, filterPedalResistanceKOhm_,
        filterPedalLoadConductance,
        filterPedalNodeVolts_, filterPedalNodeInitialised_);
    advancePedalNode(
        false, 100.0, filterPedalLoadConductance,
        filterPedalOpenNodeVolts_, filterPedalOpenNodeInitialised_);
    const double filterPedalOctaves =
        (filterPedalNodeVolts_ - filterPedalOpenNodeVolts_)
        * filterOctavesPerModVoltRelativeToOscillator;
    modUpperOctaves += filterPedalOctaves;
    if (lowerDynamic)
        modLowerOctaves += filterPedalOctaves;

    // ------------------------------------------------------------ Keyboard CV
    const int soundingNote = (p.arpeggiator != ArpeggiatorMode::Off
                              && arpSoundingNote_ >= 0)
                                 ? arpSoundingNote_
                                 : currentNote_;
    if (soundingNote >= 0)
        lastInternalPitchNote_ = static_cast<double>(soundingNote);
    // The rear jack replaces the keyboard/arpeggiator voltage before P1/C6,
    // not their gates. P1017 C1=100n is on the selected N node. The normalled
    // D source reaches it through P1016 R41=2k2; the declared external source
    // is behind P1017 R1=15k. Their respective parallels with R42=95k3 give
    // 0.215 ms and 1.296 ms before the separately selectable Glide.
    const auto nominalExternalPitch = externalPitchNodes(
        externalPitchSourceVoltsPerOctave);
    // The labelled N/P values and the already-derived keyboard note law set
    // the calibrated DC scale. R41 adds C1's source resistance here; do not
    // apply its divider again to a coordinate that already contains it.
    double targetNodeVolts = nominalExternalPitch.loadedVolts
        * (lastInternalPitchNote_ - keyboardTrackingPivotMidi) / 12.0;
    double inputCoefficient = keyboardPitchInputCoefficient_;
    if (externalPitchJackInserted_)
    {
        targetNodeVolts =
            externalPitchNodes(externalPitchSourceVolts_).loadedVolts;
        inputCoefficient = externalPitchInputCoefficient_;
    }
    if (!externalPitchNodeInitialised_)
    {
        // Ordinary DSP startup is not a cold-power simulation. Seed C1 at
        // the initially selected source equilibrium, but retain it through
        // every later note, cable and source change.
        externalPitchNodeVolts_ = targetNodeVolts;
        externalPitchNodeInitialised_ = true;
    }
    else
    {
        externalPitchNodeVolts_ = flushDenormal(
            externalPitchNodeVolts_
            + inputCoefficient
                * (targetNodeVolts - externalPitchNodeVolts_));
    }
    const double conditionedVolts =
        -externalPitchNodeVolts_ * externalPitchFeedbackOhms
            / externalPitchInputOhms;
    const double pitchTarget = keyboardTrackingPivotMidi
        + 12.0 * conditionedVolts
            / nominalExternalPitch.conditionedVolts;
    if (!glideInitialised_)
    {
        glidedNote_ = pitchTarget;
        glideInitialised_ = true;
    }
    const bool glideActive =
        p.glideMode == GlideMode::On
        || (p.glideMode == GlideMode::Auto && keyStackSize_ >= 2);
    if (glideActive && p.glide > 0.0f)
    {
        // Single-pole lag: P1=2M into C6=470n, tau up to 0.94 s.
        // P1 has no taper mark, so the quadratic travel remains voiced.
        const double tau = std::max(
            1.0e-4, 0.94 * static_cast<double>(p.glide * p.glide));
        const double coefficient = -std::expm1(-dt / tau);
        glidedNote_ += coefficient * (pitchTarget - glidedNote_);
    }
    else
    {
        glidedNote_ = pitchTarget;
    }

    static constexpr std::array<double, 4> masterOctaveOffset {
        -2.0, -1.0, 0.0, 1.0 };
    const double octaveOffset =
        masterOctaveOffset[static_cast<std::size_t>(p.octave)];
    // P1013's 100k linear TUNE pot is loaded at its wiper by R19=1.8M
    // into IC6's virtual earth. Preserve physical noon as the model's zero
    // while retaining the resulting slight endpoint asymmetry.
    constexpr double loadedTuneCentre = 36.0 / 73.0;
    const double tuneOctaves =
        (loadedLinearPot(p.tune, 1800.0) - loadedTuneCentre) * (6.0 / 12.0);
    const double bendOctaves =
        static_cast<double>(pitchBend_) * (8.0 / 12.0);

    const double keyboardOctaves = (glidedNote_ - 69.0) / 12.0;
    const double masterBus =
        keyboardOctaves + octaveOffset + tuneOctaves + bendOctaves;

    controlOscAOctaves_ = masterBus + modAOctaves;

    controlOscBDrone_ = p.oscBRange == OscBRange::Bass
                     || p.oscBRange == OscBRange::Wide;
    if (controlOscBDrone_)
    {
        // BASS 30..300 Hz, WIDE 2..10,000 Hz; disconnected from keyboard,
        // tune, octave and bend — X/Y modulation still applies.
        const double droneHz =
            p.oscBRange == OscBRange::Bass
                ? exponentialTravel(p.interval, 30.0, 300.0)
                : exponentialTravel(p.interval, 2.0, 10000.0);
        controlOscBDroneHz_ = droneHz * std::exp2(modBOctaves);
    }
    else
    {
        static constexpr std::array<double, 4> rangeOffset {
            -1.0, 0.0, 1.0, 2.0 };
        const double intervalOctaves =
            (static_cast<double>(p.interval) - 0.5) * 2.0 * (7.0 / 12.0);
        controlOscBOctaves_ = masterBus
            + rangeOffset[static_cast<std::size_t>(p.oscBRange)]
            + intervalOctaves + modBOctaves;
    }

    // ------------------------------------------------------------ Filter CVs
    // The cutoff bus, in octaves. P6's loaded ±12 V endpoints, IC15's
    // 100k/221k gain and the CEM3350 node ladder give ±5.882 octaves about
    // panel 5. Only the 100 kΩ trimmer's factory placement is absent from
    // the documents, so retain the previous geometric centre while using
    // the complete derived span and loaded-pot curvature (OQ-02).
    //
    // Tracking's 108 % falls out of the same ladder: full KB AMOUNT puts
    // the keyboard's 1 V/octave bus through 12k1, so 21.2 mV/V ÷
    // 19.6 mV/oct = 1.083 octaves per octave — which independently
    // reproduces the manual's "slightly over 100 %" from the resistors.
    // P1016 fixes KCV zero from two same-rail currents: the DAC0800 sink and
    // R39's +12A current cancel at code 48.024, or MIDI 60.006015 with its
    // four DAC counts per semitone. Rail voltage, IC16A's feedback resistor
    // and downstream gain therefore cannot move the nominal pivot. The
    // 108.3% tracking amount itself is independently component-derived from
    // P1013's CEM3350 ladder (OQ-13).
    constexpr double trackingOctavesPerOctave = 1.083;
    constexpr double voicedCutoffCentreHz = 565.685424949238;
    const double masterCutoffVolts = loadedMasterCutoffVolts(p.cutoff);
    const double upperMasterOctaves = dynamicCutoffNodeGain
        * panelMasterMixerGain * masterCutoffVolts
        / cem3350VoltsPerOctave;
    const double upperBaseHz =
        voicedCutoffCentreHz * std::exp2(upperMasterOctaves);
    const double trackingOctaves = static_cast<double>(p.kbAmount)
        * trackingOctavesPerOctave
        * (glidedNote_ - keyboardTrackingPivotMidi) / 12.0;
    // The owner's manual defines the full NORMAL/INVERT motion as a mirrored
    // five-octave sweep about CUTOFF. P1015 shows an unusual four-lug P1, but
    // neither its fixed-tap resistance nor SW5's residual FORMANT loading is
    // published, so retain the anchored transfer without invented curvature.
    const double envelopeOctaves =
        (2.0 * static_cast<double>(p.filterEnvAmount) - 1.0) * 2.5
        * (2.0 * filterEnvelope_.level - 1.0);

    const double upperOctaves =
        trackingOctaves + envelopeOctaves + modUpperOctaves;
    controlUpperCutoffHz_ = upperBaseHz * std::exp2(upperOctaves);

    // P5 is loaded by R48, so LOWER ONLY has a curved -7.101..+1.566 octave
    // law about its documented panel-8 coincidence rather than a linear
    // guessed span. FORMANT's changed frequency-node load makes that range
    // 1.389% wider and gives MASTER a small, real ±0.0817-octave mismatch;
    // both filters coincide throughout MASTER only in DYNAMIC.
    const double lowerNodeGain = lowerDynamic
        ? dynamicCutoffNodeGain : formantCutoffNodeGain;
    const double lowerOnlyVolts = loadedLowerOnlyVolts(p.lowerOnly);
    constexpr double lowerCoincidenceVolts =
        -12.0 * (1.0 - 0.8) * 150.0
        / (150.0 + 100.0 * 0.8 * (1.0 - 0.8));
    const double lowerOffsetOctaves = lowerNodeGain
        * panelLowerOnlyMixerGain
        * (lowerOnlyVolts - lowerCoincidenceVolts)
        / cem3350VoltsPerOctave;
    const double formantMasterDriftOctaves = (lowerNodeGain
        - dynamicCutoffNodeGain) * panelMasterMixerGain * masterCutoffVolts
        / cem3350VoltsPerOctave;
    const double lowerOctaves = lowerOffsetOctaves
        + formantMasterDriftOctaves
        + (lowerDynamic ? trackingOctaves + envelopeOctaves + modLowerOctaves
                        : 0.0);
    controlLowerCutoffHz_ = upperBaseHz * std::exp2(lowerOctaves);

    controlLowerK_ = lowerDamping(p.resonance);
    // LOW throws the pot off the Upper chip's Q pin, leaving it on its own
    // bias — the anchored Q = 0.5 the whole law is calibrated against. Both
    // CEM signal inputs are tied: VIF contributes unity while VIV contributes
    // 1/Q_commanded. The latter must use the commanded Q before C37's
    // external enhancement subtracts the chip's residual damping.
    const bool upperLow =
        p.upperResonance == UpperResonanceMode::Low;
    const double commandedUpperDamping = upperLow
        ? lowSwitchDamping
        : 1.0 / resonanceQ(p.resonance, upperPullupOhms);
    controlUpperInputGain_ = 1.0 + commandedUpperDamping;
    controlUpperK_ = upperLow
        ? commandedUpperDamping
        : commandedUpperDamping - 1.0 / chipCeilingQ;

    // -------------------------------------------------------------- Gains
    // LC reaches the CEM3360 linear-control pin through R135=10k, with the
    // factory drawing's R136=4k7 to ground and R137=240k to -12 V. Solve that
    // terminal voltage before applying the production sheet's nominal 52%/V
    // linear scale and 1.0 maximum cell-current gain. The exact zero remains
    // LC=0.5 V (e=1/15), while the nominal cell reaches maximum gain before
    // the envelope peak. Per-device control bias, feedthrough and gain spread
    // are deliberately not claimed by this nominal transfer.
    const double loudnessControlConductance =
        1.0 / loudnessControlInputOhms
        + 1.0 / loudnessControlGroundOhms
        + 1.0 / loudnessControlNegativeOhms;
    const double loudnessControlVolts =
        (envelopeReferenceVolts * loudnessEnvelope_.level
             / loudnessControlInputOhms
         - 12.0 / loudnessControlNegativeOhms)
        / loudnessControlConductance;
    controlLoudnessGain_ = p.vcaBypass
        ? 1.0
        : clamp01(cem3360LinearGainPerVolt * loudnessControlVolts);
    // P1013 uses two *parallel* BC173 emitter followers: their bases share
    // FREE/R29/R30/R32, while separate emitters feed the audio- and MOD-X-VCA
    // control branches through R39 and R31. Outside FREE the audio branch is
    // already beyond one transistor's documented 5 V reverse-E-B region near
    // Y=0; ITT gives only a lower-bound breakdown point, not the reverse I/V
    // curve, and the other branch changes the shared-base load. Retain the
    // explicit half-wave seam until simultaneous F/S/Vc captures close OQ-26.
    controlShaperVcaGain_ = std::max(0.0, shaperLevel_);

    // P3 is marked 100k LOG but its manufacturer taper is not given. Keep
    // that one visible voicing (2.5 decades, OQ-22) between the now-exact
    // zero- and 100k-ohm endpoints of the P1013 rheostat.
    constexpr double brightnessTaperRange = 316.22776601683796; // 10^2.5
    controlBrightnessResistanceOhms_ = brightnessPotOhms
        * (std::pow(brightnessTaperRange,
                    static_cast<double>(p.brightness)) - 1.0)
        / (brightnessTaperRange - 1.0);
}

void GhostarEngine::renderVoiceSample(double externalAudio) noexcept
{
    const EngineParameters& p = parameters_;
    const double dt = 1.0 / internalRate_;

    // ------------------------------------------------- Audio-rate MOD X bus
    // One fixed-clock MM5837 source drives both hardware branches. Advance it
    // exactly once at the start of the circuit tick: IC4A's audio output and
    // IC4B's continuous RED NOISE modulation tap then remain the same physical
    // source sample, while S+H continues to use its separately clocked value.
    const double noise = noise_.process();
    const auto& audioMod = controlAudioRateMod_;
    const bool modulatedByOscB = audioMod.active
                              && audioMod.source == ModSource::OscB;
    const bool modulatedByRedNoise = audioMod.active
                                  && audioMod.source == ModSource::RedNoise;
    const double independentModSource = modulatedByRedNoise
        ? std::clamp(noise_.red() * redNoiseBusGain, -1.0, 1.0)
              * audioMod.gain
        : 0.0;

    // With MOD SOURCE = OSC B the mod board carries the post-switch, post-IC10
    // selected wave. R82/C72 and R118/C77 lag each CEM3340's complete pitch
    // sum, making self-FM causal without replacing a physical 1.82 us memory
    // with a sample-choice heuristic. With SYNC off, B can emit before A reads
    // it. SYNC closes the remaining B -> A-frequency -> A-reset -> B loop, so
    // A/PWM uses the causal sample there. PWM keeps that sync-dependent source
    // choice; filters use fresh B. RED NOISE is independent of both oscillators
    // and therefore uses the current circuit tick everywhere.
    double audioModUpper = 0.0;
    double audioModLower = 0.0;
    double audioModDuty = 0.0;

    // ----------------------------------------------------------- Oscillators
    const double previousModSource = modulatedByOscB
        ? lastOscBWave_ * audioMod.gain : independentModSource;
    const double basePitchB = controlOscBDrone_
        ? std::log2(controlOscBDroneHz_ / 440.0)
        : controlOscBOctaves_;
    const double previousPitchInputB = basePitchB
        + previousModSource * audioMod.bOctaves;
    double pitchB = 0.0;
    if (modulatedByOscB)
    {
        auto predictedPitchLagB = pitchLagB_;
        pitchB = runPitchControlLag(predictedPitchLagB, previousPitchInputB);
    }
    else
    {
        pitchB = runPitchControlLag(pitchLagB_, previousPitchInputB);
    }
    const double frequencyB = std::min(
        440.0 * std::exp2(pitchB),
        0.45 * internalRate_);
    const double stepB = frequencyB * dt;
    // The selector's narrowest B pulse is 3%, but the CEM3340 PWM input is
    // explicitly capable of 0..100%.  Wheel modulation therefore reaches
    // the constant-low/high endpoint plateaus instead of an invented 3%
    // guard band; coincident endpoint BLEP events cancel algebraically.
    const double dutyB =
        std::clamp(oscBDuty_ + controlPwmB_, 0.0, 1.0);
    OscCorrections corrB {};

    // A modulated duty boundary can cross the standing phase between two
    // samples — a value jump no phase crossing sees — so the jump the duty
    // move itself makes is registered as an event at this sample's start.
    const auto dutyMoveEvent = [](Waveform previousWaveform,
                                  Waveform waveform, double phase,
                                  double previousDuty, double duty,
                                  OscCorrections& c) noexcept {
        // A selector change has no previous duty boundary in the newly
        // selected waveform. Its old held sample is handled separately;
        // only continuous pulse-to-same-pulse PWM can move a boundary.
        if (previousWaveform != waveform
            || waveform == Waveform::Triangle
            || waveform == Waveform::Sawtooth)
            return;
        const double jump = naiveWave(waveform, phase, duty)
                          - naiveWave(waveform, phase, previousDuty);
        if (jump != 0.0)
            addStepEvent(c.selectedHeld, c.selectedNow, 0.0, jump);
    };

    double waveB = 0.0;
    double triB = 0.0;
    const auto emitB = [&]() noexcept {
        if (heldWaveformB_ == p.oscBWaveform)
        {
            waveB = p1014SelectedWaveVolts(
                p.oscBWaveform, heldWaveB_ + corrB.selectedHeld)
                / selectedWaveVoltsPerEngineUnit;
        }
        else
        {
            // The deferred base sample still belongs to the old selector
            // position. Preserve its own voltage scale; only event residuals
            // discovered after the switch use the new position's scale.
            waveB = std::clamp(
                        p1014SelectedWaveVolts(
                            heldWaveformB_, heldWaveB_)
                            + selectedWaveDeltaVolts(
                                p.oscBWaveform, corrB.selectedHeld),
                        -12.0, 12.0)
                  / selectedWaveVoltsPerEngineUnit;
        }
        triB = heldTriB_ + corrB.triangleHeld;
        heldWaveB_ = naiveWave(p.oscBWaveform, phaseB_, dutyB)
                   + corrB.selectedNow;
        heldWaveformB_ = p.oscBWaveform;
        heldTriB_ = triangleWave(phaseB_) + corrB.triangleNow;
        heldDutyB_ = dutyB;
    };

    // Without hard sync B has no current-sample dependency on A, so emit it
    // first and let A/PWM receive the fully corrected physical waveform.
    if (!p.sync)
    {
        dutyMoveEvent(heldWaveformB_, p.oscBWaveform, phaseB_,
                      heldDutyB_, dutyB, corrB);
        phaseB_ = scanOscillatorSample(
            phaseB_, stepB, p.oscBWaveform, dutyB, -1.0, corrB);
        emitB();
    }

    const double sourceForA = modulatedByOscB
        ? (p.sync ? lastOscBWave_ : waveB) * audioMod.gain
        : independentModSource;
    const double pitchInputA = controlOscAOctaves_
        + sourceForA * audioMod.aOctaves;
    double pitchA = 0.0;
    if (p.sync && modulatedByOscB)
    {
        auto predictedPitchLagA = pitchLagA_;
        pitchA = runPitchControlLag(predictedPitchLagA, pitchInputA);
    }
    else
    {
        pitchA = runPitchControlLag(pitchLagA_, pitchInputA);
    }
    audioModDuty = sourceForA * audioMod.duty;

    const double frequencyA =
        std::min(440.0 * std::exp2(pitchA),
                 0.45 * internalRate_);
    const double stepA = frequencyA * dt;
    const double dutyA =
        std::clamp(oscADuty_ + controlPwmA_ + audioModDuty, 0.0, 1.0);
    OscCorrections corrA {};
    dutyMoveEvent(heldWaveformA_, p.oscAWaveform, phaseA_,
                  heldDutyA_, dutyA, corrA);

    if (p.sync)
    {
        dutyMoveEvent(heldWaveformB_, p.oscBWaveform, phaseB_,
                      heldDutyB_, dutyB, corrB);
        // P1014's conventional hard-sync network leaves both CEM pin-6
        // inputs open.  Instead A's raw 8->0 V saw fall (before RS5) passes
        // SW2/C24/BC308/R107 into B's pins 9/10 and resets B at that wrap's
        // sub-sample instant.  A's selected wave and PWM edges cannot fire
        // it; B's resulting value/slope jumps are bandlimited as usual.
        const bool aWillWrap = phaseA_ + stepA >= 1.0;
        const double resetU = (aWillWrap && stepA > 0.0)
                                  ? (1.0 - phaseA_) / stepA
                                  : -1.0;
        phaseA_ = scanOscillatorSample(
            phaseA_, stepA, p.oscAWaveform, dutyA, -1.0, corrA);
        phaseB_ = scanOscillatorSample(
            phaseB_, stepB, p.oscBWaveform, dutyB, resetU, corrB);
        emitB();
    }
    else
    {
        phaseA_ = scanOscillatorSample(
            phaseA_, stepA, p.oscAWaveform, dutyA, -1.0, corrA);
    }

    // Cyclic routes predicted their phase step from causal prior B above.
    // Commit the real capacitor endpoints once, against the newly emitted
    // voltage, so base-CV changes do not acquire an extra internal-sample
    // delay and the physical charge is current for the next interval.
    const double currentModSource = modulatedByOscB
        ? waveB * audioMod.gain : independentModSource;
    if (modulatedByOscB)
        runPitchControlLag(
            pitchLagB_, basePitchB + currentModSource * audioMod.bOctaves);
    if (p.sync && modulatedByOscB)
        runPitchControlLag(
            pitchLagA_, controlOscAOctaves_
                + currentModSource * audioMod.aOctaves);

    // Deferred emit: each sample leaving the oscillator is the previous one,
    // now carrying the earlier half of every event discovered since.
    double waveA = 0.0;
    if (heldWaveformA_ == p.oscAWaveform)
    {
        waveA = p1014SelectedWaveVolts(
            p.oscAWaveform, heldWaveA_ + corrA.selectedHeld)
              / selectedWaveVoltsPerEngineUnit;
    }
    else
    {
        waveA = std::clamp(
                    p1014SelectedWaveVolts(heldWaveformA_, heldWaveA_)
                        + selectedWaveDeltaVolts(
                            p.oscAWaveform, corrA.selectedHeld),
                    -12.0, 12.0)
              / selectedWaveVoltsPerEngineUnit;
    }
    const double triA = heldTriA_ + corrA.triangleHeld;
    heldWaveA_ = naiveWave(p.oscAWaveform, phaseA_, dutyA)
               + corrA.selectedNow;
    heldWaveformA_ = p.oscAWaveform;
    heldTriA_ = triangleWave(phaseA_) + corrA.triangleNow;
    heldDutyA_ = dutyA;
    lastOscBWave_ = waveB;

    if (audioMod.active)
    {
        const double source = modulatedByOscB
            ? waveB * audioMod.gain : independentModSource;
        audioModUpper = source * audioMod.upperOctaves;
        audioModLower = source * audioMod.lowerOctaves;
    }

    // P1013 AC-couples A's triangle into both IC7's signal input and IC6's
    // dry reference; P2 trims their A-carrier cancellation while B controls
    // the OTA. The printed nominal is a four-quadrant product with no
    // invented symmetric carrier leak (unit-specific residual remains OQ-06).
    const double ring = processRingModulator(triA, triB);

    // The causal 1x -> 4x reconstructor below takes 141 internal ticks. Delay
    // the complete frame that drives the shared audio circuit by the same
    // amount. Capturing the downstream controls and the two VCA noise draws,
    // rather than only four source samples, preserves the old source/control
    // relationship exactly: the circuit behaves as before, just later.
    PreMixerFrame currentFrame;
    currentFrame.oscillatorA = waveA;
    currentFrame.oscillatorB = waveB;
    currentFrame.ring = ring;
    currentFrame.pinkNoise = noise;
    currentFrame.audioModUpper = audioModUpper;
    currentFrame.audioModLower = audioModLower;
    currentFrame.upperCutoffHz = controlUpperCutoffHz_;
    currentFrame.lowerCutoffHz = controlLowerCutoffHz_;
    currentFrame.upperK = controlUpperK_;
    currentFrame.upperInputGain = controlUpperInputGain_;
    currentFrame.lowerK = controlLowerK_;
    currentFrame.loudnessGain = controlLoudnessGain_;
    currentFrame.shaperVcaGain = controlShaperVcaGain_;
    currentFrame.brightnessResistanceOhms =
        controlBrightnessResistanceOhms_;
    currentFrame.filterMixA = controlFilterMixA_;
    currentFrame.filterMixB = controlFilterMixB_;
    currentFrame.filterMixNoise = controlFilterMixNoise_;
    currentFrame.shaperMixA = controlShaperMixA_;
    currentFrame.shaperMixB = controlShaperMixB_;
    currentFrame.shaperMixRing = controlShaperMixRing_;
    currentFrame.shaperMixNoise = controlShaperMixNoise_;
    currentFrame.loudnessVcaNoise = cem3360OutputNoiseScale_
        * bipolarWhite(loudnessVcaNoiseState_);
    currentFrame.shaperVcaNoise = cem3360OutputNoiseScale_
        * bipolarWhite(shaperVcaNoiseState_);
    currentFrame.filterPathA = p.filterPathA;
    currentFrame.filterPathB = p.filterPathB;
    currentFrame.filterPathNoise = p.filterPathNoise;
    currentFrame.masterVolume = p.masterVolume;
    currentFrame.lowerMode = p.lowerMode;
    currentFrame.slope = p.slope;
    currentFrame.splitPaths = p.splitPaths;
    currentFrame.externalAudioJackInserted = externalAudioJackInserted_;

    const PreMixerFrame frame = preMixerDelay_[
        static_cast<std::size_t>(preMixerDelayIndex_)];
    preMixerDelay_[static_cast<std::size_t>(preMixerDelayIndex_)] =
        currentFrame;
    preMixerDelayIndex_ = (preMixerDelayIndex_ + 1)
        % externalInputLatencyInternalSamples();

    const double mixerWaveA = frame.oscillatorA;
    const double mixerWaveB = frame.oscillatorB;
    const double mixerRing = frame.ring;
    // An inserted silent cable is silence, not an invitation to fall back to
    // IC4A. The always-running MM5837 above still feeds its upstream RED
    // NOISE modulation branch.
    const double mixerNoise = frame.externalAudioJackInserted
        ? externalAudio : frame.pinkNoise;

    // ------------------------------------------------------ Filter/ADSR path
    // RS7's dry output mapping is not legible enough to name its final net,
    // so keep that one scalar seam explicit. The Lower filter itself is fed
    // below by the three physical slider networks, including their loading.
    double filterPath = frame.filterMixA * mixerWaveA
                      + frame.filterMixB * mixerWaveB
                      + frame.filterMixNoise * mixerNoise;

    const double upperHz = frame.audioModUpper != 0.0
        ? frame.upperCutoffHz * std::exp2(frame.audioModUpper)
        : frame.upperCutoffHz;
    const double lowerHz = frame.audioModLower != 0.0
        ? frame.lowerCutoffHz * std::exp2(frame.audioModLower)
        : frame.lowerCutoffHz;
    const double upperG = std::tan(
        pi * std::min(upperHz, 0.45 * internalRate_) / internalRate_);
    const double lowerG = std::tan(
        pi * std::min(lowerHz, 0.45 * internalRate_) / internalRate_);

    const auto lower = runLowerSection(
        { mixerWaveA, mixerWaveB, filterNoiseMixGain * mixerNoise },
        { static_cast<double>(frame.filterPathA),
          static_cast<double>(frame.filterPathB),
          static_cast<double>(frame.filterPathNoise) },
        filterPath, lowerG, frame.lowerK);
    // RS7's individual terminal nets are traced but its three-deck rotor
    // phase is not.
    // C9/C11 ground Lower VLP, C10 feeds it to C34 through 33 kΩ and C12
    // feeds it directly; B6/B7 select IC12 pin 1 and B8 selects pin 7. The
    // named OVERDRIVE behavior below applies the A3+B7+C10 functional
    // hypothesis; a standard same-index switch would instead pair A3/B7 with
    // C11, so no panel detent is source-closed. Non-OD C34 routing is likewise
    // unresolved. Relaxing it through the same hypothesis at zero excitation
    // is an explicitly nonphysical interim approximation, not a traced path.
    const double overdriven = processOverdrive(
        frame.lowerMode == LowerFilterMode::Overdrive ? lower.lp : 0.0);
    bool upperInputIsPhysical = false;
    switch (frame.lowerMode)
    {
        case LowerFilterMode::BandPass:
            // Owner-manual behavioral surrogate: dry plus resonant BP gives
            // the stated parametric peak. The explicit voiced bridge remains
            // separate from B8's traced-but-unassigned IC12B gain until
            // installed-switch continuity closes the exact MNA.
            filterPath += lowerBandPassOutputGain * lower.bp;
            break;
        case LowerFilterMode::Overdrive:
        {
            // IC12A/D1/D2 followed by R187/R167/C34/R173; the Upper filter
            // re-filters the resulting distortion products. This is already
            // the physical R173 voltage in the shared 5 V/unit state domain.
            filterPath = overdriven;
            upperInputIsPhysical = true;
            break;
        }
        case LowerFilterMode::HighPass:
            // Owner-manual behavioral surrogate: the Lower section's
            // low-pass-state subtraction followed by the Upper low-pass makes
            // the stated double-peak response. The open output bridge keeps
            // that physical state in the dry seam's output-referred domain;
            // RS7 continuity still owns its exact MNA.
            filterPath -= lowerHighPassOutputGain * lower.lp;
            break;
        case LowerFilterMode::Out:
            // The manual anchors the dry transfer into Upper; P1013 does not
            // expose which remaining throw/net implements it.
            break;
    }

    // OUT/BANDPASS/HIGHPASS remain output-referred behavioral seams until
    // RS7's installed rotor phase is measured. Refer them back through the
    // 12 dB output buffer before entering the physical CEM states. OVERDRIVE
    // already arrived through C34/R173 and must not be attenuated twice.
    if (!upperInputIsPhysical)
        filterPath /= upperTwelveDbOutputGain;

    // The two Upper halves cannot be advanced independently: their tied CEM
    // inputs, SW4's moving C40 timing capacitor, R194 cross-state path and
    // linked IC14B gain pole form one stateful network (SM DWG 2, OQ-09).
    filterPath = runUpperCascade(filterPath, upperG, frame.upperK,
                                 frame.upperInputGain, frame.slope);

    // C30=470n sees R132=24k in parallel with R133=100k before the loudness
    // CEM3360. Its state keeps advancing while that VCA is shut. The cell's
    // own output-current noise joins after gain, directly at its 20k load.
    filterPath = processFilterCoupling(filterPath) * frame.loudnessGain;
    filterPath += frame.loudnessVcaNoise;

    // -------------------------------------------------------- Shaper Y path
    // The P1013 assembly resolves the drawing's transposed SL3/SL4 labels:
    // A/B/NOISE use 47k arms, while RING's SL4 wiper alone reaches the
    // errata-corrected R45=6k8. The finite pot resistance belongs in series,
    // so the 47/6.8 ratio is not constant across its stroke.
    double shaperPath = frame.shaperMixA * mixerWaveA
                      + frame.shaperMixB * mixerWaveB
                      + frame.shaperMixRing * mixerRing
                      + frame.shaperMixNoise * mixerNoise;
    shaperPath *= frame.shaperVcaGain;
    shaperPath += frame.shaperVcaNoise;

    // IC5 is the Shaper VCA. C18/P3 colours its current output across the
    // full 20k Master track, and P1017's normal contact cross-loads both
    // Master wipers through R49/R50. Solve that coupled network before the
    // output decimator so its capacitor lives at the internal rate.
    const auto output = processOutputNetwork(
        filterPath, shaperPath, frame.masterVolume, frame.splitPaths,
        frame.brightnessResistanceOhms);
    // Route on the same delayed circuit tick that selected the output
    // network's cross-loading, then preserve the two resulting output lanes
    // through identical decimators. This makes a live SPLIT transition one
    // coherent FIR-smoothed jack event instead of combining an old network
    // state with a current host-rate switch.
    if (frame.splitPaths)
    {
        lastFilterPathSample_ = output.filter;
        lastShaperPathSample_ = output.shaper;
    }
    else
    {
        const double normalled = 0.5 * (output.filter + output.shaper);
        lastFilterPathSample_ = normalled;
        lastShaperPathSample_ = normalled;
    }
}

double GhostarEngine::reconstructExternalAudio(
    double hostSample, int internalStep) noexcept
{
    // Stage B advances at 2x: the host sample occupies its even phase and a
    // structural zero its odd phase. Stage A repeats that operation at 4x.
    // The halfband kernels have unit sum for decimation, so interpolation
    // needs x2 at each stage to retain unity DC gain.
    double stageAInput = 0.0;
    if ((internalStep & 1) == 0)
    {
        externalStageBRing_[
            static_cast<std::size_t>(externalStageBIndex_)] =
                internalStep == 0 ? hostSample : 0.0;
        externalStageBIndex_ = (externalStageBIndex_ + 1) % stageBTaps;
        stageAInput = 2.0 * convolveRing(
            stageBKernel_, externalStageBRing_, externalStageBIndex_);
    }

    externalStageARing_[static_cast<std::size_t>(externalStageAIndex_)] =
        stageAInput;
    externalStageAIndex_ = (externalStageAIndex_ + 1) % stageATaps;
    return 2.0 * convolveRing(
        stageAKernel_, externalStageARing_, externalStageAIndex_);
}

void GhostarEngine::process(float* left, float* right, int numSamples)
{
    process(nullptr, left, right, numSamples);
}

void GhostarEngine::process(const float* externalAudio, float* left,
                            float* right, int numSamples)
{
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Capture before writing left[sample]: JUCE may alias the mono input
        // and first output channel in its process buffer. One hostile sample
        // must not poison either FIR for the following ~71 host samples.
        const float hostSample = externalAudio != nullptr
            ? externalAudio[sample] : 0.0f;
        double externalSample = static_cast<double>(hostSample);
        // Classify before promotion: every nonzero float subnormal is a
        // perfectly normal double, so checking only externalSample would let
        // denormals live in both reconstruction FIR histories.
        if (!std::isfinite(hostSample)
            || std::fpclassify(hostSample) == FP_SUBNORMAL)
            externalSample = 0.0;
        externalSample = std::clamp(externalSample,
                                    -externalAudioLimitEngineUnits,
                                    externalAudioLimitEngineUnits);

        advanceControls();
        // Four internal steps per output sample; every second one feeds the
        // first decimation stage's output into the second stage, and the
        // second stage picks the output value.
        for (int step = 0; step < 4; ++step)
        {
            renderVoiceSample(reconstructExternalAudio(externalSample, step));
            filterStageARing_[static_cast<std::size_t>(stageAIndex_)] =
                lastFilterPathSample_;
            shaperStageARing_[static_cast<std::size_t>(stageAIndex_)] =
                lastShaperPathSample_;
            stageAIndex_ = (stageAIndex_ + 1) % stageATaps;
            if ((step & 1) == 1)
            {
                filterStageBRing_[static_cast<std::size_t>(stageBIndex_)] =
                    convolveRing(stageAKernel_, filterStageARing_,
                                 stageAIndex_);
                shaperStageBRing_[static_cast<std::size_t>(stageBIndex_)] =
                    convolveRing(stageAKernel_, shaperStageARing_,
                                 stageAIndex_);
                stageBIndex_ = (stageBIndex_ + 1) % stageBTaps;
            }
        }

        const double filterOut =
            convolveRing(stageBKernel_, filterStageBRing_, stageBIndex_);
        const double shaperOut =
            convolveRing(stageBKernel_, shaperStageBRing_, stageBIndex_);

        left[sample] = static_cast<float>(filterOut);
        right[sample] = static_cast<float>(shaperOut);
    }
}

} // namespace ghostar
