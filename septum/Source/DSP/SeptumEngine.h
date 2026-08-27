// The Septum engine: a behavioral model of the Roland SH-201's virtual
// analog voice, grounded in the documents catalogued in
// the README's "How it works". The hardware computes its whole voice on one
// custom DSP behind a documented analog output stage; this engine reproduces
// the documented architecture exactly — two tones of
// OSC1+OSC2 -> MIX/MOD -> FILTER -> AMP with three envelopes and two LFOs per
// tone, a shared modulation-delay -> reverb effects block, 10 voices halved in
// DUAL — and keeps every mapping from a 7-bit panel value to a physical
// quantity in one place (the `mapping` namespace) so each voiced constant can
// be replaced by a measurement without touching the render code.

#pragma once

#include "SeptumPatch.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace septum
{

inline constexpr int maxPolyphony = 10;     // settled: 10 voices
inline constexpr int dualPolyphony = 5;     // settled: DUAL halves polyphony
inline constexpr int controlInterval = 8;   // samples per control tick (voiced)

// --------------------------------------------------------------------------
// Panel-value-to-physics mappings. Tiers per the README's "How it works":
// functions marked [settled] implement a documented law; [reported] follow a
// published measurement of related hardware; [voiced] are this project's
// choices, each owned by an open question.
// --------------------------------------------------------------------------
namespace mapping
{
    inline constexpr double pi = 3.14159265358979323846;

    // [voiced, OQ-08] CUTOFF 0-127 -> Hz, exponential over ten octaves.
    [[nodiscard]] inline double cutoffHz (double value) noexcept
    {
        return 20.0 * std::exp2 (value * (10.0 / 127.0));
    }

    // [voiced, OQ-08] RESONANCE 0-127 -> state-variable damping k. k = 2 is
    // Q = 0.5; zero is the oscillation threshold; the top of the knob goes
    // slightly negative so self-oscillation grows until the stage limiter
    // holds it, matching the manual's "may not stop at all" warning.
    //
    // Both endpoints are settled. The shape between them is not, and a linear
    // taper put the whole audible range of the control in the top fifth of
    // its travel: the filter peaked by 1.38 dB at the exact centre of the
    // knob. The square-root taper below was **chosen by ear** in the
    // 2026-08-22 listening test recorded in Docs/decisions.md; it
    // brings the centre of the knob to about +5.5 dB. That is a choice, not a
    // measurement — OQ-08 is still open, and a swept response from a real
    // unit is still what would close it.
    [[nodiscard]] inline double resonanceDamping (double value) noexcept
    {
        return 2.0 - 2.04 * std::sqrt (std::clamp (value / 127.0, 0.0, 1.0));
    }

    // [voiced, OQ-09] Envelope attack 0-127 -> seconds, 1 ms to 5 s.
    [[nodiscard]] inline double attackSeconds (int value) noexcept
    {
        return 0.001 * std::pow (5000.0, value / 127.0);
    }

    // [voiced, OQ-09] Envelope decay/release 0-127 -> seconds to -60 dB,
    // 2 ms to 12 s.
    [[nodiscard]] inline double decaySeconds (int value) noexcept
    {
        return 0.002 * std::pow (6000.0, value / 127.0);
    }

    // [voiced, OQ-10] LFO RATE 0-127 -> Hz, 0.03 to 30 Hz.
    [[nodiscard]] inline double lfoRateHz (double value) noexcept
    {
        return 0.03 * std::pow (1000.0, value / 127.0);
    }

    // [settled] Tempo-synced LFO frequency from the documented note table.
    [[nodiscard]] inline double lfoSyncHz (int tempoBpm, int noteIndex) noexcept
    {
        const double wholeNotes = lfoTempoSyncWholeNotes[static_cast<std::size_t> (
            std::clamp (noteIndex, 0, 19))];
        return (tempoBpm / 60.0) / (4.0 * wholeNotes);
    }

    // [voiced, OQ-10] LFO fade time 0-127 -> seconds.
    [[nodiscard]] inline double lfoFadeSeconds (int value) noexcept
    {
        const double n = value / 127.0;
        return n * n * 10.0;
    }

    // [voiced, OQ-03] Pulse width 0-127 -> duty cycle. Settled endpoints only
    // in words: left approaches a square (50 %), right broadens the pulse.
    [[nodiscard]] inline double pulseDuty (double value) noexcept
    {
        return 0.5 + 0.45 * (value / 127.0);
    }

    // [reported] Szabo's fitted 11th-degree polynomial mapping the detune
    // knob (0-1) to the supersaw offset scaler, quoted verbatim.
    [[nodiscard]] inline double superSawDetuneAmount (double knob) noexcept
    {
        const double x = std::clamp (knob, 0.0, 1.0);
        return ((((((((((10028.7312891634 * x - 50818.8652045924) * x
                        + 111363.4808729368) * x - 138150.6761080548) * x
                      + 106649.6679158292) * x - 53046.9642751875) * x
                    + 17019.9518580080) * x - 3425.0836591318) * x
                  + 404.2703938388) * x - 24.1878824391) * x
                + 0.6717417634) * x + 0.0030115596;
    }

    // [reported] Szabo's measured per-oscillator frequency offsets at full
    // detune, relative to the center oscillator.
    inline constexpr std::array<double, 7> superSawOffsets {
        -0.11002313, -0.06288439, -0.01952356, 0.0,
        0.01991221, 0.06216538, 0.10745242
    };

    // [reported + voiced, OQ-05] Szabo's JP-8000 mix laws evaluated at the
    // fixed mix m = 0.75 the SH-201's missing MIX control is voiced to.
    inline constexpr double superSawFixedMix = 0.75;
    [[nodiscard]] inline double superSawCenterGain() noexcept
    {
        return -0.55366 * superSawFixedMix + 0.99785;
    }
    [[nodiscard]] inline double superSawSideGain() noexcept
    {
        return (-0.73764 * superSawFixedMix + 1.2841) * superSawFixedMix
               + 0.044372;
    }

    // [voiced, OQ-06] FB OSC feedback 0-127 -> loop gain; past unity the
    // in-loop soft clip bounds it.
    [[nodiscard]] inline double fbOscGain (double value) noexcept
    {
        return 1.15 * (value / 127.0);
    }

    // [voiced, OQ-09] Pitch-envelope depth -63..+63 -> semitones.
    [[nodiscard]] inline double pitchEnvSemitones (int depth) noexcept
    {
        return depth * (24.0 / 63.0);
    }

    // [voiced, OQ-08] Filter-envelope depth -63..+63 -> octaves of cutoff.
    [[nodiscard]] inline double filterEnvOctaves (int depth) noexcept
    {
        return depth * (10.0 / 63.0);
    }

    // [voiced, OQ-08] Cutoff velocity sensitivity: offset in octaves at the
    // velocity extremes.
    [[nodiscard]] inline double cutoffVelocityOctaves (int sens,
                                                      double velocity) noexcept
    {
        return (sens / 63.0) * (velocity - 0.5) * 2.0 * 4.0;
    }

    // [voiced, OQ-10] LFO pitch depth -63..+63 -> cents, squared taper to a
    // one-octave extreme.
    [[nodiscard]] inline double lfoPitchCents (int depth) noexcept
    {
        const double n = depth / 63.0;
        return (n >= 0.0 ? 1.0 : -1.0) * n * n * 1200.0;
    }

    // [voiced, OQ-10] LFO filter depth -63..+63 -> octaves.
    [[nodiscard]] inline double lfoFilterOctaves (int depth) noexcept
    {
        return depth * (5.0 / 63.0);
    }

    // [voiced, OQ-11] DRIVE 0-127 -> pre-gain, up to +32 dB, and the output
    // compensation that follows the clipper. The exponent was the last bare
    // number left in the render code; the contract already named it
    // ("output-compensated, pre^-0.4"), so it belongs here with the gain it
    // compensates.
    [[nodiscard]] inline double overdrivePreGain (int drive) noexcept
    {
        return std::pow (10.0, (drive / 127.0) * (32.0 / 20.0));
    }
    inline constexpr double overdriveCompensationExponent = -0.4;
    [[nodiscard]] inline double overdriveCompensation (double preGain) noexcept
    {
        return std::pow (preGain, overdriveCompensationExponent);
    }

    // [voiced] The rate the OVERDRIVE shaper is evaluated at. The modelled
    // engine runs at one fixed rate (OQ-01); a plug-in runs at the host's, so
    // a shaper evaluated at the host rate folds a *different* amount of alias
    // energy depending on what the user's interface is set to — the character
    // of the port, not of the instrument. The stage is oversampled to land
    // inside this band whatever the host does, so its alias signature is a
    // property of the model. Closing OQ-11 with a captured transfer would say
    // what curve to evaluate; it would not change where.
    inline constexpr double overdriveInternalRateHz = 176400.0;

    // [voiced] The power-of-two factor whose internal rate lands closest to
    // that target, in octaves: 4x at 44.1/48 kHz, 2x at 88.2/96 kHz, none at
    // 176.4/192 kHz. A power-of-two ladder cannot hit a fixed rate exactly
    // from an arbitrary host rate, so what it guarantees is a bound rather
    // than a number: for every host rate from 22.05 to 192 kHz the shaper
    // runs within ±1.0 octave of the target, against the 3.1 octaves those
    // host rates themselves span. Choosing the *nearest* factor rather than
    // the first one at or above the target is what makes that true of the
    // in-between rates — 64 kHz would otherwise be pushed to 256 kHz while
    // 88.2 kHz sat at 176.4.
    //
    // The ladder stops at 4 because the shaper stops at 4: `process` has an
    // outer and an inner half-band stage and nothing beyond them. Offering 8
    // bought nothing at 22.05 and 24 kHz - those rates ran at 4 regardless -
    // while making this function claim a rate the signal path never reached.
    // A third stage would cost 2p at 4x host, which is 1.5 host samples: a
    // fractional group delay to report and to align the bypass path against,
    // for two sample rates. The bound is stated at what the ladder delivers
    // instead, and 22.05 kHz is the one rate that sits a whole octave low.
    [[nodiscard]] inline int overdriveOversampling (double hostRateHz) noexcept
    {
        const double rate = std::max (1.0, hostRateHz);
        int best = 1;
        double bestDistance = std::abs (std::log2 (rate / overdriveInternalRateHz));
        for (int factor : { 2, 4 })
        {
            const double distance =
                std::abs (std::log2 (rate * factor / overdriveInternalRateHz));
            if (distance < bestDistance - 1.0e-9)
            {
                bestDistance = distance;
                best = factor;
            }
        }
        return best;
    }

    // [voiced, OQ-01/OQ-03] The rate the NOISE source is generated at.
    //
    // One full-scale value per *host* sample is white to the host Nyquist, and
    // a white sequence spreads its power over that whole band, so its audible
    // level falls as the host rate rises: measured 3.06 dB quieter at 96 kHz
    // and 6.02 dB at 192 kHz than at 44.1 kHz, against a SAW in the same patch
    // that is rate-invariant to 0.04 dB. Flat and level are separate claims and
    // only the first was being made — the same defect the OVERDRIVE and the
    // FB OSC loop already carry a fixed reference rate to avoid, and the same
    // reason: it is the character of the port rather than of the instrument.
    //
    // The three properties cannot all hold at once. A sequence flat to the host
    // Nyquist whose audible level does not move needs a variance proportional
    // to the host rate, which is unbounded. What a fixed-rate instrument does
    // is the fourth option: its noise is flat across the audio band and simply
    // has no content above its own Nyquist. So the source is drawn at the
    // instrument's rate and interpolated up, which fixes the level without a
    // gain constant — unity-gain half-band interpolation preserves the variance
    // *and* the audio-band density, because it changes only where the same
    // continuous-time signal is sampled.
    //
    // The instrument's own rate. Roland's driver readmes name one CoreAudio
    // device for the SH-201, "Roland SH-201 44.1kHz", twice over three years,
    // from a driver whose table carries six rates and composes the name as
    // <device> <rate> — so the number is the instrument's, and the clock tree
    // puts the engine on it (OQ-01, rate answered).
    //
    // It is used as a floor rather than a target because a power-of-two ladder
    // cannot reach 44.1 kHz from every host rate: generating *below* the rate
    // the instrument runs at would band-limit the noise harder than the
    // hardware ever does, so the ladder stops at the last factor that does not.
    // A 48 kHz host therefore generates at 48 rather than 44.1, 9 % high, which
    // is 0.37 dB of density — the size of the approximation, stated rather than
    // hidden.
    inline constexpr double noiseInternalRateFloorHz = 44100.0;

    // The largest power-of-two decimation whose internal rate stays at or above
    // that floor: none at 44.1/48 kHz, 2x at 88.2/96 kHz, 4x at 176.4/192 kHz.
    // Like the OVERDRIVE's ladder this stops at 4 because the interpolator
    // stops at 4 — two half-band stages and nothing beyond them — so a host
    // rate that is not a power of two above 44.1 kHz generates above the floor
    // rather than at it.
    [[nodiscard]] inline int noiseDecimation (double hostRateHz) noexcept
    {
        int factor = 1;
        for (int candidate : { 2, 4 })
            if (hostRateHz / candidate >= noiseInternalRateFloorHz - 1.0e-9)
                factor = candidate;
        return factor;
    }

    // [voiced, OQ-12] Delay TIME 0-127 -> seconds, 1 ms to 1.3 s.
    [[nodiscard]] inline double delaySeconds (double value) noexcept
    {
        return 0.001 * std::pow (1300.0, value / 127.0);
    }

    // [settled realisation] One-pole coefficient whose -3 dB point lands on
    // `cornerHz` — for the documented damping tables and the analog output
    // stage's RC poles, whose frequencies Roland publishes.
    //
    // The obvious `1 - exp(-2*pi*fc/fs)` is the *time-constant* one-pole, and
    // its -3 dB point is not fc: at 44.1 kHz the settled 8 kHz HF-DAMP entry
    // came out at 9055 Hz and the 12.5 kHz HIGH CUT entry never reached -3 dB
    // at all. Storing a published frequency and then building a filter a
    // quarter of an octave away from it spends the settled evidence without
    // delivering it. Solving |H(w0)| = 1/sqrt(2) for the pole gives
    // p = c - sqrt(c^2 - 1) with c = 2 - cos(w0), which is exact at every
    // rate — standard first-order algebra, no fitted number.
    //
    // A corner at or above Nyquist has no -3 dB point to hit, so there the
    // coefficient is chosen to match the analog magnitude at Nyquist instead;
    // both ends (DC and Nyquist) are then exact and the shape between them is
    // a first-order curve either way. `cornerHz <= 0` is BYPASS.
    [[nodiscard]] inline double onePoleAtCorner (double cornerHz,
                                                 double sampleRateHz) noexcept
    {
        if (cornerHz <= 0.0)
            return 1.0;
        const double fs = std::max (1.0, sampleRateHz);
        const double w = 2.0 * pi * cornerHz / fs;
        if (w < pi)
        {
            const double c = 2.0 - std::cos (w);
            return 1.0 - (c - std::sqrt (c * c - 1.0));
        }
        const double nyquist = 0.5 * fs;
        const double magnitude =
            1.0 / std::sqrt (1.0 + (nyquist / cornerHz) * (nyquist / cornerHz));
        return 2.0 * magnitude / (1.0 + magnitude);
    }

    // [settled] Reverb PRE DELAY: the address map stores 0-125 and the manual
    // prints 0.0-100.0 ms (OM p. 65), which this read linearly - 0.8 ms per
    // step. Roland's own SH-201 Editor prints the table, and it is not linear:
    // `delayTime0-100Table` in its Resource.xml is 126 entries in four regular
    // runs, 0.1 ms to 4.9, then 0.5 ms steps to 9.5, then 1 ms steps to 49,
    // then 2 ms steps to 100. So the knob spends its first two fifths inside
    // the first five milliseconds, where a pre-delay does its audible work,
    // and the linear reading put raw 50 at 40 ms where the unit puts it at 5.
    // Written as the four runs rather than as 126 literals: the table is
    // regular and the arithmetic is the document.
    [[nodiscard]] inline double reverbPreDelayMs (int raw) noexcept
    {
        const int value = std::clamp (raw, 0, 125);
        if (value < 50)
            return 0.1 * value;
        if (value < 60)
            return 5.0 + 0.5 * (value - 50);
        if (value < 100)
            return 10.0 + 1.0 * (value - 60);
        return 50.0 + 2.0 * (value - 100);
    }

    // [voiced, OQ-12] Reverb TIME 0-127 with SIZE 0-7 -> RT60 seconds.
    [[nodiscard]] inline double reverbSeconds (int time, int size) noexcept
    {
        const double sizeScale = 0.5 + size * (1.0 / 7.0);
        return 0.15 * std::pow (10.0 / 0.15, time / 127.0) * sizeScale;
    }

    // [voiced, OQ-13] Portamento time 0-127 -> seconds of glide.
    [[nodiscard]] inline double portamentoSeconds (int value) noexcept
    {
        const double n = value / 127.0;
        return n * n * 5.0;
    }

    // [settled] KEY FOLLOW -200..+200: octaves of cutoff per octave of key
    // distance from C4 (+100 tracks 1:1 per the manual's p.36 diagram).
    [[nodiscard]] inline double keyFollowOctavesPerOctave (int keyFollow) noexcept
    {
        return keyFollow / 100.0;
    }

    // [voiced, OQ-07] MIX/MOD LOW FREQ shelf: corner and gain.
    inline constexpr double lowShelfHz = 200.0;
    inline constexpr double lowShelfGainDb = 8.0;

    // [voiced, OQ-07] BALANCE -63..+63. Settled only at the endpoints (fully
    // left is OSC1 alone, fully right OSC2 alone); each leg sits at unity in
    // the centre and the opposite leg fades linearly to silence. The same law
    // sits between the two tones for TONE BALANCE.
    [[nodiscard]] inline double balanceLegGain (int balance, bool firstLeg) noexcept
    {
        const double signed_ = firstLeg ? -balance : balance;
        return std::min (1.0, (63.0 + signed_) / 63.0);
    }

    // [voiced, OQ-06] FB OSC loop shape: where the comb taps, how much the
    // loop is damped per sample, the trim that keeps it from running away, and
    // the output level that keeps a full-feedback oscillator in the same
    // loudness range as the other waves.
    inline constexpr double fbOscDelayRatio = 0.5;      // half the period
    // The in-loop damping, expressed at a reference rate rather than per
    // sample. As a bare per-sample coefficient its -3 dB point was 0.134 x fs,
    // so it sat at 5.9 kHz at 44.1 kHz and 25.8 kHz at 192 kHz — and it is
    // inside the feedback loop, so it set the loop gain at every comb
    // resonance: the same patch measured 3 dB louder and a quarter of an
    // octave brighter at 96 kHz than at 44.1 kHz. That is the character of the
    // port rather than of the instrument, which the contract rules out for the
    // overdrive in the same words. The constant is unchanged and 44.1 kHz is
    // bit-identical; every other host rate now matches it.
    inline constexpr double fbOscLoopDamping = 0.55;
    inline constexpr double fbOscLoopDampingReferenceRateHz = 44100.0;
    [[nodiscard]] inline double fbOscLoopDampingCoeff (double sampleRateHz) noexcept
    {
        const double fs = std::max (1.0, sampleRateHz);
        return 1.0
               - std::pow (1.0 - fbOscLoopDamping,
                           fbOscLoopDampingReferenceRateHz / fs);
    }
    inline constexpr double fbOscLoopTrim = 0.995;
    inline constexpr double fbOscOutputGain = 0.6;

    // [reported + voiced, OQ-05] The seven-saw stack sums to roughly +/-2.5
    // before the tracked high-pass; this brings it back to the range the other
    // waves occupy.
    inline constexpr double superSawStackNormalisation = 1.0 / 2.5;

    // [voiced] The ten-voice sum needs fixed headroom before the output stage;
    // the demos and tests treat full scale as the analog stage's clip point.
    inline constexpr double voiceHeadroom = 0.22;

    // [voiced, OQ-10] How far the modulation lever reaches into each of the
    // settled MODULATION ASSIGN destinations at full travel.
    inline constexpr double leverVibratoCents = 60.0;
    inline constexpr double leverPulseWidth = 63.0;     // of the 0-127 span
    inline constexpr double leverFilterOctaves = 2.0;
    inline constexpr double leverAmpDepth = 1.0;        // full tremolo

    // [voiced, OQ-12] Delay modulation: the rate the MOD RATE knob spans and
    // how far MOD DEPTH sweeps the delay time.
    [[nodiscard]] inline double delayModulationRateHz (int value) noexcept
    {
        return 0.02 * std::pow (400.0, value / 127.0);
    }
    inline constexpr double delayModulationDepthSeconds = 0.008;
    // [voiced] Slew on the delay time itself, so a TIME move glides rather
    // than jumping the read head.
    inline constexpr double delayTimeSlewSeconds = 0.08;

    // [voiced, OQ-12] Reverb geometry. Mutually prime line lengths spread over
    // roughly 30-90 ms at SIZE 8, four series input allpasses, and the SIZE
    // scaling that shrinks both.
    inline constexpr std::array<double, 8> reverbLineSeconds {
        0.0297, 0.0371, 0.0411, 0.0437, 0.0533, 0.0631, 0.0733, 0.0797
    };
    inline constexpr std::array<double, 4> reverbDiffuserSeconds {
        0.0043, 0.0083, 0.0151, 0.0223
    };
    [[nodiscard]] inline double reverbSizeScale (int size) noexcept
    {
        return 0.35 + 0.65 * ((std::clamp (size, 0, 7) + 1) / 8.0);
    }
    // [voiced, OQ-12] DIFFUSION and DENSITY as allpass coefficients, the level
    // the diffused input is injected into the network at, and the wet return.
    [[nodiscard]] inline double reverbDiffusionGain (int value) noexcept
    {
        return 0.25 + 0.5 * (value / 127.0);
    }
    [[nodiscard]] inline double reverbDensityGain (int value) noexcept
    {
        return 0.2 + 0.55 * (value / 127.0);
    }
    inline constexpr double reverbInputInjection = 0.35;
    inline constexpr double reverbWetReturn = 0.8;

    // [voiced, OQ-08] The -24 dB path's second stage. The first stage carries
    // the resonance; the second is a fixed, gentler 2-pole that adds the extra
    // 12 dB/oct. Whether the hardware resonates on one stage or both is open,
    // and OQ-08 names the capture that would settle it. A coupled second stage
    // fitted as `max(0.12, 0.40*k1 + 0.35)` shipped briefly: three constants
    // no measurement produced, under a comment asserting that the SH-201
    // couples resonance into its second stage, which no source in the
    // contract states. It moved the -24 dB resonant peak by up to 5 dB and
    // invalidated the measurement table Step 7 published.
    inline constexpr double filterSecondStageDamping = 1.2;
    // [voiced, OQ-08] Where the resonant stage's integrator states stop
    // growing, so self-oscillation is bounded as the manual's "may not stop at
    // all" implies rather than divergent.
    //
    // It has to sit above every state an *unresonant* filter reaches, or it
    // stops being a bound on runaway and becomes a waveshaper on the signal
    // path. Measured at RESONANCE 0 with both oscillators at unity, LEVEL and
    // velocity at maximum and LOW FREQ BOOST, across all eight waveforms,
    // three filter types, both slopes and CUTOFF 0..127, the largest state is
    // 6.15 (NOISE at CUTOFF 127; SQU/PW-SQU 5.02, SAW 4.70, SINE 4.82, TRI
    // 4.41, FB-OSC 2.83, SUPER-SAW 2.59). Eight is that with headroom.
    //
    // It was 1.5, which is under a third of what an ordinary patch produces —
    // hence the -27 dB THD at RESONANCE 0 that Step 30 measured. The repair
    // there was to run the limiter only where the stage's own damping has
    // gone non-positive, but `resonanceDamping` crosses zero at RESONANCE
    // 122.07, so 0-122 ran with no bound at all: the level ran away from
    // about 118, pinned on the output limiter at 121-122, and fell 17.8 dB at
    // 123. Raising the knee is what lets the limiter run everywhere, which is
    // what makes the knob monotone.
    inline constexpr double filterStateLimit = 8.0;

    // [voiced] The output stage's final safety knee: transparent below this,
    // saturating above, so a reasonably driven patch never touches it and an
    // unreasonable one cannot hand the host a sample outside +/-1.05.
    inline constexpr double outputLimitKnee = 0.9;
    inline constexpr double outputLimitRange = 0.15;

    // [voiced] Received CC#10 tilts the final pair with constant power, and is
    // normalised to unity at the centre rather than to unity at an extreme.
    inline constexpr double partPanCentreGain = 1.4142135623730951;

    // [voiced] Slew on the master gain chain, so a level move does not step.
    inline constexpr double masterSlewSeconds = 0.01;

    // [voiced] How long the output meter takes to fall by 1/e. A display
    // choice, not a claim about the instrument; what matters is that it is a
    // time rather than a factor per render call, so the same sound reads the
    // same at every host buffer size.
    inline constexpr double meterFallSeconds = 0.30;

    // [voiced, OQ-14] INPUT VOL 0-127 -> amplitude. The knob is analog on the
    // hardware, ahead of the codec; a squared law matches the AMP LEVEL knob's
    // and is the same "silent at the far left" the manual describes.
    [[nodiscard]] inline double externalInputGain (int value) noexcept
    {
        const double n = value / 127.0;
        return n * n;
    }

    // [voiced, OQ-14] The AUDIO FILTER's RESONANCE. The manual describes it as
    // a boost around the cutoff and, unlike the voice filter's, never warns
    // that it may not stop: the curve is the voice filter's, floored short of
    // the oscillation threshold so an input filter cannot run away.
    [[nodiscard]] inline double audioFilterDamping (double value) noexcept
    {
        return std::max (0.05, resonanceDamping (value));
    }

    // [voiced, OQ-14] How far the AUDIO-FILTER LFO destination and the
    // modulation lever move that cutoff.
    [[nodiscard]] inline double audioFilterLfoOctaves (int depth) noexcept
    {
        return depth * (5.0 / 63.0);
    }
    inline constexpr double audioFilterLeverOctaves = 2.0;

    // [voiced, OQ-14] How fast the direct monitor path is muted when an EXT-IN
    // voice takes the input over, and unmuted when it stops.
    inline constexpr double externalMonitorFadeSeconds = 0.005;

    // [settled divisions, voiced amounts, OQ-15] Where the boundary inside a
    // shuffled pair falls. The manual gives the divisions and names Light and
    // Heavy; it does not say how much either one is, so these two numbers are
    // the project's own and were the last bare ones left inside a function
    // this file tagged `[settled]` — a tag that overstated them. A shuffled
    // pair keeps its total length and moves the boundary inside it, so the
    // beat never drifts whatever the amounts turn out to be.
    inline constexpr double arpeggioShuffleLight = 0.58;
    inline constexpr double arpeggioShuffleHeavy = 0.66;
    [[nodiscard]] inline double arpeggioShuffleRatio (ArpeggioGrid grid) noexcept
    {
        switch (grid)
        {
            case ArpeggioGrid::EighthLight:
            case ArpeggioGrid::SixteenthLight:
                return arpeggioShuffleLight;
            case ArpeggioGrid::EighthHeavy:
            case ArpeggioGrid::SixteenthHeavy:
                return arpeggioShuffleHeavy;
            case ArpeggioGrid::Quarter:
            case ArpeggioGrid::Eighth:
            case ArpeggioGrid::Twelfth:
            case ArpeggioGrid::Sixteenth:
            case ArpeggioGrid::TwentyFourth:
                break;
        }
        return 0.5;   // an unshuffled grid splits its pair evenly
    }

    [[nodiscard]] inline double arpeggioStepSeconds (double bpm, ArpeggioGrid grid,
                                                     int stepIndex) noexcept
    {
        const double beat = 60.0 / std::clamp (bpm, 5.0, 300.0);
        const bool firstOfPair = (stepIndex & 1) == 0;
        const double ratio = arpeggioShuffleRatio (grid);
        switch (grid)
        {
            case ArpeggioGrid::Quarter:        return beat;
            case ArpeggioGrid::Eighth:         return beat * 0.5;
            case ArpeggioGrid::EighthLight:
            case ArpeggioGrid::EighthHeavy:
                return beat * (firstOfPair ? ratio : 1.0 - ratio);
            case ArpeggioGrid::Twelfth:        return beat / 3.0;
            case ArpeggioGrid::Sixteenth:      return beat * 0.25;
            case ArpeggioGrid::SixteenthLight:
            case ArpeggioGrid::SixteenthHeavy:
                return beat * 0.5 * (firstOfPair ? ratio : 1.0 - ratio);
            case ArpeggioGrid::TwentyFourth:   return beat / 6.0;
        }
        return beat * 0.25;
    }

    // [settled] DURATION as a fraction of the final grid section.
    [[nodiscard]] inline double arpeggioDurationFraction (ArpeggioDuration d) noexcept
    {
        switch (d)
        {
            case ArpeggioDuration::P30:  return 0.30;
            case ArpeggioDuration::P40:  return 0.40;
            case ArpeggioDuration::P50:  return 0.50;
            case ArpeggioDuration::P60:  return 0.60;
            case ArpeggioDuration::P70:  return 0.70;
            case ArpeggioDuration::P80:  return 0.80;
            case ArpeggioDuration::P90:  return 0.90;
            case ArpeggioDuration::P100: return 1.00;
            case ArpeggioDuration::P120: return 1.20;
            case ArpeggioDuration::Full: return 0.0;   // held, not timed
        }
        return 0.80;
    }

    // [voiced, OQ-15] The reference played velocity the style's programmed
    // cell values are normalised against, so that ACCENT blends the played
    // velocity toward the pattern rather than scaling it.
    //
    // It is *not* a floor, and the name it used to carry said it was. The
    // manual's two ACCENT endpoints — at 100 "the arpeggiated notes will have
    // the velocities that are programmed by the arpeggio style", at 0 "all
    // arpeggiated notes will be sounded at a fixed velocity" — are reproduced
    // only when ARPEGGIO VELOCITY is a fixed value; with VELOCITY = REAL,
    // ACCENT 0 plays what you played and ACCENT 100 scales it. Whether the
    // hardware's ACCENT overrides REAL is exactly what OQ-15's MIDI capture
    // would settle, so the arithmetic is not changed ahead of it.
    inline constexpr double arpeggioCellReferenceVelocity = 100.0;

    // [settled] Where a style's note row lands on the keys held down.
    //
    // `keys` is the held chord sorted ascending, `row` is the style's 1-based
    // note row, `span` the highest row the style uses, and `cycle` counts
    // completed passes through the style. The MOTIF suffixes are the manual's:
    // (L) sounds the lowest key every time, (L&H) the lowest and the highest
    // every time, (-) neither, and the window over the remaining keys walks up,
    // down, up-and-down or at random once per pass.
    //
    // This is not inferred. The manual works three examples of the style
    // "1-2-3-2" against the keys C-D-E-F-G (OM p. 67), and this function
    // reproduces all three exactly:
    //   UP(-)        C-D-E-D -> D-E-F-E -> E-F-G-F
    //   UP(L)        C-D-E-D -> C-E-F-E -> C-F-G-F
    //   UP&DOWN(L&H) C-D-G-D -> C-E-G-E -> C-F-G-F -> C-E-G-E
    // PHRASE is the exception: it reads the rows as semitone steps above the
    // last key pressed, which is voiced (OQ-15).
    // Returns the index into the sorted chord, or -1 when the motif does not
    // read the chord positionally (PHRASE).
    [[nodiscard]] inline int arpeggioKeyIndexForRow (ArpeggioMotif motif, int count,
                                                     int row, int span,
                                                     int cycle) noexcept
    {
        if (count <= 0 || motif == ArpeggioMotif::Phrase)
            return -1;

        const bool descending = motif == ArpeggioMotif::DownL
                                || motif == ArpeggioMotif::DownLowHigh
                                || motif == ArpeggioMotif::Down;
        const bool pinLow = motif == ArpeggioMotif::UpL
                            || motif == ArpeggioMotif::UpLowHigh
                            || motif == ArpeggioMotif::DownL
                            || motif == ArpeggioMotif::DownLowHigh
                            || motif == ArpeggioMotif::UpDownL
                            || motif == ArpeggioMotif::UpDownLowHigh
                            || motif == ArpeggioMotif::RandomL;
        const bool pinHigh = motif == ArpeggioMotif::UpLowHigh
                             || motif == ArpeggioMotif::DownLowHigh
                             || motif == ArpeggioMotif::UpDownLowHigh;

        const int positions = std::max (1, count - std::max (1, span) + 1);
        int base = 0;
        if (motif == ArpeggioMotif::UpDownL || motif == ArpeggioMotif::UpDownLowHigh
            || motif == ArpeggioMotif::UpDown)
        {
            const int period = std::max (1, 2 * positions - 2);
            const int phase = ((cycle % period) + period) % period;
            base = phase < positions ? phase : period - phase;
        }
        else
        {
            base = ((cycle % positions) + positions) % positions;
        }

        // The window walks; DOWN reads it from the top of the chord. The pins
        // are applied *after* that reversal, because "(L)" names the lowest key
        // pressed and "(L&H)" the lowest and the highest — which key that is
        // does not depend on which way the window is walking.
        //
        // [settled] A style can ask for more rows than the chord has keys, and
        // the manual says what happens then: "When the number of keys played
        // is less than the number of notes in the arpeggio style, the
        // highest-pitched of the pressed keys is played by default" (OM p.66).
        // That is a statement about pitch, not about the window, so it holds
        // whichever way the window is walking — a descending motif that ran
        // off the end used to fall back on the *lowest* key instead.
        const int position = base + row - 1;
        int index = position >= count
                        ? count - 1
                        : (descending ? count - 1 - position : position);
        if (pinLow && row == 1)
            index = 0;
        else if (pinHigh && row == span)
            index = count - 1;
        return std::clamp (index, 0, count - 1);
    }

    [[nodiscard]] inline int arpeggioKeyForRow (ArpeggioMotif motif,
                                                const int* keys, int count,
                                                int lastPressed, int row,
                                                int span, int cycle) noexcept
    {
        if (count <= 0 || keys == nullptr)
            return -1;
        if (motif == ArpeggioMotif::Phrase)
        {
            const int reference = lastPressed >= 0 ? lastPressed : keys[0];
            return std::clamp (reference + row - 1, 0, 127);
        }
        return keys[arpeggioKeyIndexForRow (motif, count, row, span, cycle)];
    }

    // [voiced] Parameter slew for the filter's panel-side controls. It is not
    // a model of anything the hardware does — it exists so a patch edit or an
    // S&H LFO edge cannot put a discontinuity into the filter coefficient —
    // so it is applied to the panel side only and never to an envelope.
    inline constexpr double controlSlewSeconds = 0.0025;

    // [voiced, OQ-14] How long a switch on the external-input path takes to
    // cross between the two signals it chooses between. CENTER CANCEL and the
    // audio filter's ON, SLOPE and TYPE switches are all automatable and all
    // select among paths whose instantaneous samples differ, so an instant
    // change steps the output on live audio however warm the states are kept.
    // Long enough that the step is inaudible, short enough that the control
    // still reads as a switch rather than a fade.
    inline constexpr double externalSwitchFadeSeconds = 0.005;
}

// --------------------------------------------------------------------------
// Engine
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Half-band polyphase resampling, used only around the AMP overdrive.
//
// A half-band FIR of length N = 4p+1 has its centre tap at exactly 0.5 and
// every other even tap at zero, so both directions cost p folded multiplies
// per slow-rate sample and the even phase is a pure delay. Both filters are
// equiripple designs (Parks-McClellan, then used with their natural half-band
// structure rather than a forced one); their measured responses are quoted at
// the coefficient tables. Each direction delays by p slow-rate samples, so a
// round trip through one stage costs 2p.
// --------------------------------------------------------------------------
struct HalfBandStage
{
    static constexpr int historySize = 32;   // power of two, >= 2 * maxPairs
    static constexpr int historyMask = historySize - 1;

    const double* taps { nullptr };  // the folded odd taps, `pairs` of them
    int pairs { 0 };                 // p

    std::array<double, historySize> upHistory {};
    std::array<double, historySize> downEven {};
    std::array<double, historySize> downOdd {};
    int upWrite { 0 }, downWrite { 0 };

    void configure (const double* coefficients, int pairCount) noexcept
    {
        taps = coefficients;
        pairs = pairCount;
        clear();
    }

    void clear() noexcept
    {
        upHistory.fill (0.0);
        downEven.fill (0.0);
        downOdd.fill (0.0);
        upWrite = downWrite = 0;
    }

    // One slow-rate sample in, two fast-rate samples out.
    void upsample (double x, double& even, double& odd) noexcept
    {
        upHistory[static_cast<std::size_t> (upWrite)] = x;
        even = upHistory[static_cast<std::size_t> ((upWrite - pairs) & historyMask)];
        double sum = 0.0;
        const int span = 2 * pairs - 1;
        for (int j = 0; j < pairs; ++j)
            sum += taps[j]
                   * (upHistory[static_cast<std::size_t> ((upWrite - j) & historyMask)]
                      + upHistory[static_cast<std::size_t> ((upWrite - (span - j))
                                                            & historyMask)]);
        odd = 2.0 * sum;
        upWrite = (upWrite + 1) & historyMask;
    }

    // Two fast-rate samples in, one slow-rate sample out.
    [[nodiscard]] double downsample (double even, double odd) noexcept
    {
        downEven[static_cast<std::size_t> (downWrite)] = even;
        downOdd[static_cast<std::size_t> (downWrite)] = odd;
        double sum =
            0.5 * downEven[static_cast<std::size_t> ((downWrite - pairs) & historyMask)];
        const int span = 2 * pairs - 1;
        for (int j = 0; j < pairs; ++j)
            sum += taps[j]
                   * (downOdd[static_cast<std::size_t> ((downWrite - 1 - j) & historyMask)]
                      + downOdd[static_cast<std::size_t> ((downWrite - 1 - (span - j))
                                                          & historyMask)]);
        downWrite = (downWrite + 1) & historyMask;
        return sum;
    }
};

// Outer stage, host rate <-> 2x. N = 33, passband to 0.215, stopband from
// 0.285: passband droop 0.06 dB, stopband -43.1 dB, delay 8 slow samples.
inline constexpr std::array<double, 8> halfBandOuterTaps {
    -0.0067193719785342918, 0.0086866417484826458, -0.014239894060670263,
    0.022346726321779357, -0.034675518871777167, 0.055571891864522723,
    -0.10108701398731906, 0.31661168588312411
};

// Inner stage, 2x <-> 4x, where the images sit an octave further out. N = 13,
// passband to 0.180, stopband from 0.320: droop 0.19 dB, stopband -33.3 dB,
// delay 3 slow samples.
inline constexpr std::array<double, 3> halfBandInnerTaps {
    0.03358705057227105, -0.08268763122280455, 0.30989245062299142
};

// NOISE, generated at the instrument's rate and interpolated to the host's.
//
// `mapping::noiseDecimation` carries the argument for why. The mechanics: one
// value is drawn per *internal* sample and pushed through as many half-band
// interpolators as the ladder asks for, which yields `factor` host-rate samples
// at a time; `next` hands them out one per call. At 44.1 and 48 kHz the factor
// is one and the draw is returned untouched, so those rates render exactly what
// they rendered before.
struct NoiseSource
{
    int factor { 1 };
    HalfBandStage outer {}, inner {};
    std::array<double, 4> pending {};
    int pendingIndex { 0 };

    void prepare (int decimation) noexcept
    {
        factor = decimation;
        outer.configure (halfBandOuterTaps.data(),
                         static_cast<int> (halfBandOuterTaps.size()));
        inner.configure (halfBandInnerTaps.data(),
                         static_cast<int> (halfBandInnerTaps.size()));
        clear();
    }

    void clear() noexcept
    {
        outer.clear();
        inner.clear();
        pending.fill (0.0);
        pendingIndex = 0;
    }

    // xorshift32. It replaced a 23-bit Galois LFSR whose *state* was read as
    // the sample: successive states of a Galois LFSR are not independent
    // (v(n+1) = 0.5 v(n) + 0.5 b(n)), so that wave was a one-pole-filtered bit
    // stream — 10.9 dB darker across the band at 44.1 kHz and 1.1 dB at
    // 192 kHz, which made the timbre a property of the user's interface rather
    // than of the instrument. Its comment also named a Roland polynomial no
    // document in the contract's source list settles.
    [[nodiscard]] static double draw (std::uint32_t& rng) noexcept
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (rng >> 8) * (1.0 / 8388607.5) - 1.0;
    }

    [[nodiscard]] double next (std::uint32_t& rng) noexcept
    {
        if (factor == 1)
            return draw (rng);

        if (pendingIndex == 0)
        {
            if (factor == 2)
            {
                outer.upsample (draw (rng), pending[0], pending[1]);
            }
            else
            {
                // The inner stage runs internal -> 2x internal and the outer
                // one 2x -> host, the same order and the same two filters the
                // OVERDRIVE climbs, so each of the inner stage's two outputs
                // becomes two host-rate samples.
                double half0 = 0.0, half1 = 0.0;
                inner.upsample (draw (rng), half0, half1);
                outer.upsample (half0, pending[0], pending[1]);
                outer.upsample (half1, pending[2], pending[3]);
            }
        }
        const double value = pending[static_cast<std::size_t> (pendingIndex)];
        pendingIndex = (pendingIndex + 1) % factor;
        return value;
    }
};

// The AMP section's OVERDRIVE, evaluated at a fixed internal rate.
//
// Inside the oversampled loop the `tanh` shaper is evaluated with first-order
// antiderivative anti-aliasing (Parker, Zavalishin & Bozkurt, DAFx-16): the
// sample's output is the transfer's average over the step just taken rather
// than its value at the step's end, which is what a continuous-time
// nonlinearity would produce. It costs one `log1p` in place of one `tanh` and
// it composes with the oversampling rather than replacing it.
//
// The stage has a fixed group delay, so a voice whose OVERDRIVE is *off* is
// pushed through a matched pure delay instead of the resampler. Without that
// an overdriven UPPER and a clean LOWER would sound the same note 19 samples
// apart and comb each other around 1 kHz.
struct OverdriveStage
{
    int factor { 1 };
    int latency { 0 };               // host-rate samples, both paths
    HalfBandStage outer {}, inner {};
    double previousInput { 0.0 };    // the ADAA step's left endpoint
    double previousIntegral { 0.0 };
    std::array<double, 32> bypass {};
    int bypassWrite { 0 };
    // OVERDRIVE is automatable, and every state in the chain carries across
    // samples. Rather than shape unconditionally, the stage notices the
    // switch coming back and rebuilds itself from the history the bypass line
    // is already keeping.
    bool wasEnabled { false };

    void prepare (double hostRateHz) noexcept;
    void clear() noexcept;
    // One host sample through the resampler and the shaper, ADAA state and
    // all. The output is the shaped sample before output compensation.
    [[nodiscard]] double shapeChain (double x, double preGain) noexcept;
    [[nodiscard]] double process (double x, double preGain, double compensation,
                                  bool enabled) noexcept;
};

class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // Full-patch update; safe between process calls. Continuous values are
    // smoothed inside the engine, so per-block updates do not zipper.
    void setPatch (const Patch& patch);
    [[nodiscard]] const Patch& currentPatch() const noexcept { return patch_; }

    // System-common controls (documented ranges).
    // The external-input path is a system setting, not patch data (OM p. 49-51).
    void setExternalInput (const ExternalInput& settings) noexcept;
    [[nodiscard]] const ExternalInput& externalInput() const noexcept
    {
        return external_;
    }

    void setMasterLevel (int level0to127) noexcept;
    void setMasterTuneHz (double a4Hz) noexcept;         // 415.30-466.20
    void setMasterKeyShift (int semitones) noexcept;     // -24..+24
    void setKeyboardOctaveShift (int octaves) noexcept;  // -3..+3
    void setTranspose (int semitones) noexcept;          // -5..+6

    // Performance inputs.
    void noteOn (int note, int velocity1to127);
    void noteOff (int note);
    void setHold (bool down);
    void setSostenuto (bool down);
    void setPitchBend (double normalised);   // -1..+1 over the per-tone range
    void setModulation (double amount0to1);  // mod lever / CC#1
    void setExpression (double amount0to1);  // CC#11
    void setPartLevel (double amount0to1);   // CC#7
    void setPartPan (double pan);            // CC#10, -1 (left)..+1 (right)
    void setPortamentoControl (int note);    // CC#84: glide source for the next key
    void allNotesOff();
    void allSoundOff();

    // `inputLeft`/`inputRight` carry the block's external audio, or null when
    // the host gives the instrument no input bus (the hardware's INPUT jacks
    // with nothing plugged in).
    void process (float* left, float* right, int numSamples,
                  const float* inputLeft = nullptr,
                  const float* inputRight = nullptr);

    [[nodiscard]] int activeVoiceCount() const noexcept;
    [[nodiscard]] float getOutputLevel (int channel) const noexcept
    {
        return outputLevel_[channel == 0 ? 0 : 1].load (std::memory_order_relaxed);
    }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

    // The tail a host should allow for: covers the longest reverb (RT60 15 s
    // at TIME 127 / SIZE 8) with margin. Extreme settings — delay feedback at
    // the documented ±98 % — ring longer on the hardware too; a finite report
    // is the practical bound, not a claim that ±98 % ever fully dies.
    [[nodiscard]] static double maximumTailSeconds() noexcept { return 30.0; }

    // The AMP overdrive's oversampling chain has a fixed group delay, and
    // every voice carries it whether its OVERDRIVE is on or off so layered
    // tones stay aligned. That makes it the instrument's latency, and a host
    // can compensate for it.
    [[nodiscard]] int latencySamples() const noexcept { return latencySamples_; }

private:
    enum class Part { Upper, Lower };
    static constexpr int partCount = 2;

    // A crossfade across a switch's discrete positions that stays continuous
    // when the switch moves again before the previous cross has finished.
    //
    // A single "position it came from" plus one fade scalar cannot: it holds
    // exactly one outgoing signal, so a second change part way through either
    // re-aims the destination while the fade is already non-zero, or — moving
    // back to where it started — collapses the whole expression onto the
    // source in one sample. Both step the output by however much had been
    // mixed in, which is the discontinuity the cross exists to prevent.
    // Keeping a weight per position holds the whole mixture instead, and no
    // weight ever moves faster than the fade rate.
    template <int positionCount>
    struct SwitchCrossfade
    {
        std::array<double, static_cast<std::size_t> (positionCount)> weight {};

        void snapTo (int position) noexcept
        {
            weight.fill (0.0);
            weight[clampPosition (position)] = 1.0;
        }

        void advance (int position, double step) noexcept
        {
            const auto selected = clampPosition (position);
            double total = 0.0;
            for (std::size_t i = 0; i < weight.size(); ++i)
            {
                const double target = (i == selected) ? 1.0 : 0.0;
                weight[i] += std::clamp (target - weight[i], -step, step);
                total += weight[i];
            }
            // The steps are taken independently, so the weights drift off
            // unity by at most one step; renormalising holds the mix at unit
            // gain without changing how fast any one of them moves.
            if (total > 1.0e-12)
                for (auto& w : weight)
                    w /= total;
            else
                snapTo (position);
        }

        // Sum a per-position signal against the weights. Every position but
        // the one being crossed to and from carries a zero weight, so this
        // costs one multiply-add per live position, not per position.
        template <typename Response>
        [[nodiscard]] double mix (Response&& response) const noexcept
        {
            double out = 0.0;
            for (std::size_t i = 0; i < weight.size(); ++i)
                if (weight[i] > 0.0)
                    out += weight[i] * response (static_cast<int> (i));
            return out;
        }

    private:
        [[nodiscard]] static std::size_t clampPosition (int position) noexcept
        {
            return static_cast<std::size_t> (
                std::clamp (position, 0, positionCount - 1));
        }
    };

    struct Envelope
    {
        enum class Stage { Idle, Attack, Decay, Sustain, Release };
        // Where a decay counts as arrived. Shared by the Decay branch's exit
        // test and by `configure`'s re-entry test so the two cannot disagree
        // about whether a level and its sustain are the same number.
        static constexpr double settled = 1.0e-4;
        Stage stage { Stage::Idle };
        double level { 0.0 };
        double attackRate { 0.0 };   // level per sample while attacking
        double decayCoeff { 0.0 };
        double releaseCoeff { 0.0 };
        double sustain { 1.0 };

        void configure (double sr, int a, int d, int s, int r) noexcept;
        void trigger() noexcept { stage = Stage::Attack; }
        void release() noexcept
        {
            if (stage != Stage::Idle)
                stage = Stage::Release;
        }
        void kill() noexcept { stage = Stage::Idle; level = 0.0; }
        double advance (int samples) noexcept;
        [[nodiscard]] bool idle() const noexcept { return stage == Stage::Idle; }
    };

    // Two-stage pitch envelope: linear ramp up over A, exponential fall over D.
    struct PitchEnvelope
    {
        double level { 0.0 };
        bool attacking { false };
        bool active { false };
        double attackRate { 0.0 };
        double decayCoeff { 0.0 };

        void configure (double sr, int a, int d) noexcept;
        void trigger() noexcept { active = true; attacking = true; level = 0.0; }
        double advance (int samples) noexcept;
    };

    struct Lfo
    {
        double phase { 0.0 };
        double heldValue { 0.0 };      // S&H current value
        double randomFrom { 0.0 };     // RND segment endpoints
        double randomTo { 0.0 };
        double fadeLevel { 1.0 };
        bool primed { false };
        // S&H and RANDOM draw from here. Seeded per LFO in prepare(), because
        // one shared default made all four of a patch's LFOs — both tones,
        // both slots — walk the same sequence: two S&H modulators at the same
        // rate produced bit-identical output, which is the one thing two
        // random modulators must not do. Fixed constants rather than a clock,
        // so a render stays reproducible.
        std::uint32_t rng { 0x9e3779b9u };
        void seed (std::uint32_t value) noexcept { rng = value | 1u; }

        void restart (bool resetFade) noexcept;
        double nextRandomValue() noexcept;
        double advance (const LfoParams& params, double hz, double fadePerTick,
                        int samples, double sr) noexcept;
    };

    struct SvfStage
    {
        double ic1eq { 0.0 };
        double ic2eq { 0.0 };
        void clear() noexcept { ic1eq = ic2eq = 0.0; }
    };

    struct OscState
    {
        double phase { 0.0 };
        std::array<double, 7> superPhases {};
        std::vector<float> comb;      // FB-OSC feedback line
        int combWrite { 0 };
        // How much of that line holds anything. Writes walk it from zero
        // upward and wrap, so whatever is in it is always a prefix, and a
        // voice that has never run an FB OSC has a prefix of nothing. Every
        // non-legato note-on used to zero both oscillators' whole lines
        // whatever waveform they were set to -- 2 x 3095 floats at 44.1 kHz
        // and 2 x 13448 at 192 kHz, per voice, inside the render callback.
        int combTouched { 0 };
        double combState { 0.0 };     // in-loop damping memory
        double hpfX1 { 0.0 }, hpfX2 { 0.0 }, hpfY1 { 0.0 }, hpfY2 { 0.0 };
        // Per oscillator rather than per voice: the interpolator's ladder is a
        // rate, so two oscillators both set to NOISE cannot share one chain
        // without driving it at twice the host rate. The draws still come off
        // the voice's one generator, so at a factor of one the two are exactly
        // the interleaved stream they have always been.
        NoiseSource noise {};

        void clearRuntime() noexcept
        {
            combWrite = 0;
            combState = 0.0;
            hpfX1 = hpfX2 = hpfY1 = hpfY2 = 0.0;
            const auto touched =
                std::min (static_cast<std::size_t> (std::max (0, combTouched)),
                          comb.size());
            std::fill (comb.begin(),
                       comb.begin() + static_cast<std::ptrdiff_t> (touched), 0.0f);
            combTouched = 0;
        }
    };

    struct BiquadCoeffs
    {
        double b0 { 1.0 }, b1 { 0.0 }, b2 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
    };

    struct Voice
    {
        bool active { false };
        Part part { Part::Upper };
        int note { -1 };
        double velocity { 0.0 };
        bool held { false };          // key (or pedal) still down
        std::uint32_t age { 0 };          // when it was triggered
        // When it entered release. Voice stealing takes the longest-*released*
        // voice, which trigger order does not tell you: a note held from the
        // start and let go last has the smallest `age` and the freshest tail.
        std::uint32_t releaseAge { 0 };

        OscState osc1 {}, osc2 {};
        PitchEnvelope pitchEnv {};
        Envelope filterEnv {}, ampEnv {};
        SvfStage filter1 {}, filter2 {};
        double shelfState { 0.0 };
        // How much of the shelf's output is currently added: crossed between
        // the three LOW FREQ positions rather than switched. A weight per
        // position, like the filter's TYPE, so the cross takes the registered
        // fade time whichever two positions it runs between — crossing the
        // *depth* instead made the duration depend on how far apart the two
        // endpoints happened to be.
        SwitchCrossfade<3> lowFreqMix {};

        double glidePitch { 0.0 };    // current portamento pitch in note units
        double targetPitch { 0.0 };

        // Control-tick scalars.
        double inc1 { 0.0 }, inc2 { 0.0 };
        double duty1 { 0.5 }, duty2 { 0.5 };
        double superAmount1 { 0.0 }, superAmount2 { 0.0 };
        double fbGain1 { 0.0 }, fbGain2 { 0.0 };
        // Filter coefficients at the start of the control tick, and where they
        // must arrive by its end. The render loop walks between them one
        // sample at a time, so a control tick never steps the coefficient.
        double filterG { 0.1 }, filterK { 2.0 };
        double filterGTarget { 0.1 }, filterKTarget { 2.0 };
        // TYPE and SLOPE are crossed, not thrown. TYPE has four positions and
        // is automatable, so it carries a weight each; SLOPE has two, and a
        // single scalar re-aimed mid-walk is continuous.
        SwitchCrossfade<4> filterTypeMix {};
        double filterSlopeFade { 1.0 };
        // The amp gain at the start of the control tick, and where it must
        // arrive by its end. Walked one sample at a time exactly as the
        // filter coefficients are: LEVEL, the velocity offset and an LFO on
        // the AMP destination all land here, and an S&H or SQR LFO's edge
        // stepped the whole tick's gain in one sample.
        double ampGainL { 0.0 }, ampGainR { 0.0 };
        double ampGainLTarget { 0.0 }, ampGainRTarget { 0.0 };
        double ampGainLSlewed { 0.0 }, ampGainRSlewed { 0.0 };
        BiquadCoeffs superHpf1 {}, superHpf2 {};
        OverdriveStage overdrive {};
        std::uint32_t noiseRng { 0x1234567u };
        // Control slew (voiced ~2.5 ms) on the *parameter* side of the cutoff
        // sum only — the knob, key follow, velocity offset and the LFO, which
        // are what step when a patch is edited or an S&H LFO fires. The
        // filter envelope is deliberately outside it: the two envelopes read
        // the same slider through the same mapping, so smoothing one of them
        // and not the other would make the documented "fast ADSR response"
        // true of the amp and false of the filter.
        double cutoffParamOctSlewed { 8.0 };
        double filterEnvOctSlewed { 0.0 };
        double resonanceSlewed { 0.0 };
        bool controlsPrimed { false };
    };

    // The arpeggiator's state for one tone. The keys it holds are the keys the
    // keyboard routed to that tone; the rows are the style's note rows, each
    // of which can have a note of its own sounding, so chord styles work.
    struct ArpeggioRuntime
    {
        std::array<int, 16> keys {};          // sorted ascending
        std::array<int, 16> velocities {};    // aligned with `keys`
        int keyCount { 0 };
        // The keys physically down, which is not the same list once HOLD is on:
        // the chord is latched only when the last of them is released.
        std::array<int, 16> physicalKeys {};
        std::array<int, 16> physicalVelocities {};
        // How many presses of that pitch are outstanding. Sequenced parts
        // routinely overlap two notes of the same pitch by a few
        // milliseconds; counting them keeps the first release from taking
        // the chord entry the second press is still holding.
        std::array<int, 16> physicalPresses {};
        int physicalCount { 0 };
        int lastPressed { -1 };               // PHRASE's reference key
        int lastVelocity { 100 };
        bool latched { false };               // HOLD is keeping this chord
        int cycle { 0 };                      // completed passes through the style
        // Where the motif's window sits. Normally the cycle count, but a
        // RANDOM motif redraws it, and OCTAVE RANGE must keep counting cycles
        // either way — the two controls are independent.
        int windowCycle { 0 };

        struct Row
        {
            int note { -1 };
            int remaining { 0 };   // samples until the scheduled note-off
            bool sustained { false };  // DURATION = FUL: no scheduled off
            // DURATION 120 % means a gate outlives its grid, so the note it
            // overlaps into has to start while the previous one is still
            // running. One tail slot is enough: the overlap is a fifth of a
            // step, so at most one predecessor is ever still sounding.
            int tailNote { -1 };
            int tailRemaining { 0 };
        };
        std::array<Row, arpeggioMaxRows> rows {};

        void clearKeys() noexcept
        {
            keyCount = 0;
            physicalCount = 0;
            physicalPresses.fill (0);
            latched = false;
        }
    };

    struct ToneRuntime
    {
        Lfo lfo1 {}, lfo2 {};
        double lfo1Value { 0.0 }, lfo2Value { 0.0 };
        // Solo-mode held-note stack, most recent last.
        std::array<int, 16> heldNotes {};
        std::array<int, 16> heldVelocities {};
        int heldCount { 0 };
        double lastPitch { 60.0 };
        bool anyKeyDown { false };
        // Settled (part controller CC#66): the sostenuto pedal latches the
        // *notes* that were sounding when it went down, not the voices
        // playing them. A mono tone hands its one voice from note to note by
        // last-note priority, so a latch tied to the voice would be lost the
        // moment another key borrowed it.
        std::array<std::uint64_t, 2> sostenutoNotes {};

        [[nodiscard]] bool sostenutoHolds (int note) const noexcept
        {
            if (note < 0 || note > 127)
                return false;
            return (sostenutoNotes[static_cast<std::size_t> (note >> 6)]
                    & (1ull << (note & 63)))
                   != 0ull;
        }
    };

    struct DelayLine
    {
        std::vector<float> buffer;
        int write { 0 };
        double dampState { 0.0 };
        // Samples written since the last panic. A read reaching further back
        // than this lands on pre-panic material and must return silence, so
        // a panic never has to clear the whole buffer on the audio thread.
        int fresh { 1 << 30 };
    };

    struct Reverb
    {
        static constexpr int lineCount = 8;
        std::array<std::vector<float>, lineCount> lines {};
        std::array<int, lineCount> writes {};
        std::array<int, lineCount> lengths {};
        std::array<double, lineCount> lowStates {};
        std::array<double, lineCount> highStates {};
        std::array<std::vector<float>, 4> diffusers {};
        std::array<int, 4> diffuserWrites {};
        std::vector<float> preDelay;
        int preDelayWrite { 0 };
        double highCutStateL { 0.0 }, highCutStateR { 0.0 };
        // Same panic bookkeeping as DelayLine::fresh, shared by every buffer
        // in the network: their write heads advance in lockstep.
        int fresh { 1 << 30 };
        void clear();
    };

    // -- helpers -----------------------------------------------------------
    const TonePatch& tonePatch (Part part) const noexcept
    {
        return part == Part::Upper ? patch_.upper : patch_.lower;
    }
    ToneRuntime& toneRuntime (Part part) noexcept
    {
        return tones_[part == Part::Upper ? 0 : 1];
    }
    [[nodiscard]] bool partSounds (Part part) const noexcept;
    [[nodiscard]] bool keyStillDown (const Voice& voice) noexcept;
    void releaseIfNoPedalHolds (Voice& voice) noexcept;
    // The one place a voice enters release, so `releaseAge` is stamped once
    // per release rather than once per sweep over the voice array.
    void beginRelease (Voice& voice) noexcept;
    [[nodiscard]] int partVoiceLimit() const noexcept;
    void startNoteForPart (Part part, int note, int velocity);
    void releaseNoteForPart (Part part, int note);
    Voice* allocateVoice (Part part);
    void triggerVoice (Voice& voice, Part part, int note, double velocity,
                       bool legato);
    void updateVoiceControls (Voice& voice, int tickSamples);
    void renderVoiceTick (Voice& voice, float* mono, int samples,
                          const float* external);
    void prepareExternalTick (const float* inputLeft, const float* inputRight,
                              int offset, int samples);
    [[nodiscard]] bool anyVoiceUsesExternalInput() const noexcept;
    void advanceToneLfos (int samples);

    // -- arpeggiator -------------------------------------------------------
    [[nodiscard]] bool arpeggioDrives (Part part) const noexcept;
    void arpeggioAddKey (Part part, int note, int velocity);
    void arpeggioRemoveKey (Part part, int note);
    // Whether a still-running arpeggiator is currently gating this note on
    // this part — its own note, not a key. All Notes Off has to know: the
    // keys come up, but a chord ARPEGGIO HOLD has latched keeps playing, and
    // the note it has open at that instant is not a key that was released.
    [[nodiscard]] bool arpeggioIsSounding (Part part, int note) const noexcept;
    void arpeggioStopPart (Part part);
    // Notices a part crossing the arpeggiator's boundary. Called wherever a
    // key event or a render can observe the crossing, not only from the
    // render, because a host can land both at the same sample position.
    void syncArpeggioRouting();
    void handleArpeggioRouting (Part part, bool nowDriven);
    void advanceArpeggiator (int samples);
    void arpeggioFireStep();
    void arpeggioFireStepForPart (Part part, double stepSeconds);
    void processEffects (const float* dryL, const float* dryR,
                         const float* delaySendL, const float* delaySendR,
                         const float* reverbSendL, const float* reverbSendR,
                         float* outL, float* outR, int samples);
    [[nodiscard]] double noteToHz (double note) const noexcept;
    [[nodiscard]] std::uint32_t nextRandom() noexcept;

    // -- state -------------------------------------------------------------
    Patch patch_ {};
    double sampleRate_ { 44100.0 };
    int maxBlock_ { 512 };
    int latencySamples_ { 0 };

    int masterLevel_ { 127 };
    double masterTuneHz_ { 440.0 };
    int masterKeyShift_ { 0 };
    int octaveShift_ { 0 };
    int transpose_ { 0 };

    double pitchBend_ { 0.0 };
    double modulation_ { 0.0 };
    double expression_ { 1.0 };
    // EXPRESSION reaches the tone(s) EXPRESSION DESTINATION names, so it is
    // carried per tone and smoothed there rather than in the master chain.
    std::array<double, 2> smoothedExpression_ { 1.0, 1.0 };
    double partLevel_ { 1.0 };
    double partPan_ { 0.0 };
    bool hold_ { false };
    bool sostenuto_ { false };

    std::array<Voice, maxPolyphony> voices_ {};
    std::array<ToneRuntime, partCount> tones_ {};
    std::array<ArpeggioRuntime, partCount> arpeggios_ {};
    double arpeggioStepRemaining_ { 0.0 };   // samples to the next boundary
    int arpeggioStep_ { 0 };
    // Grid sections since the pattern armed. A shuffled grid takes its
    // long/short parity from this rather than from the pattern step, because
    // the shuffle belongs to the beat and the pattern does not: with an odd
    // END STEP the step index repeats its parity and the pair stops summing
    // to its division.
    int arpeggioGridSection_ { 0 };
    bool arpeggioRunning_ { false };
    bool arpeggioActive_ { false };   // what the ARPEGGIO switch last was
    // Whether each part was arpeggiated last tick. The ARPEGGIO switch is not
    // the only thing that decides it - SPLIT ARPEGGIO, the keyboard mode and
    // the keyboard part all move a part in or out of the arpeggiator's hands,
    // and all of them are automatable.
    std::array<bool, partCount> arpeggioDriven_ {};
    std::uint32_t arpeggioRng_ { 0x6d2b79f5u };
    std::uint32_t voiceClock_ { 0 };
    std::uint32_t rng_ { 0x2545f491u };

    // Effects.
    DelayLine delayL_ {}, delayR_ {};
    double delayModPhase_ { 0.0 };
    Reverb reverb_ {};
    double delayTimeSmoothed_ { 0.0 };

    // External input: INPUT VOL -> CENTER CANCEL -> AUDIO FILTER on the direct
    // monitor path, and the pre-filter mono sum feeding any EXT-IN oscillator.
    ExternalInput external_ {};
    std::vector<float> externalDirectL_, externalDirectR_, externalMono_;
    SvfStage audioFilter1_[2] {}, audioFilter2_[2] {};
    double audioFilterG_ { 0.1 }, audioFilterK_ { 2.0 };
    bool audioFilterPrimed_ { false };
    double monitorGain_ { 1.0 };
    // The voices carry the overdrive stage's group delay whether they are
    // shaping or not, and the plug-in reports it; the monitor path has to
    // carry it too, or the input would arrive ahead of everything else.
    std::array<std::array<float, 32>, 2> monitorDelay_ {};
    int monitorDelayWrite_ { 0 };
    double smoothedMonitorLevel_ { 1.0 };
    double smoothedInputGain_ { 0.62 };
    // Every switch on this path chooses between signals that differ sample by
    // sample, so each one is crossed rather than thrown. 0 is the first named
    // position in each pair, 1 the second.
    double centerCancelFade_ { 0.0 };        // 0 through, 1 cancelled
    double audioFilterOnFade_ { 0.0 };       // 0 dry, 1 filtered
    double audioFilterSlopeFade_ { 0.0 };    // 0 = -12 dB, 1 = -24 dB
    SwitchCrossfade<4> audioFilterTypeMix_ {};

    // Analog output stage state (documented component values).
    double dcX1_[2] {}, dcY1_[2] {};
    double rcState1_[2] {}, rcState2_[2] {};
    double dcCoeff_ { 0.999 };
    double rcCoeff1_ { 1.0 }, rcCoeff2_ { 1.0 };

    // Smoothed globals.
    double smoothedMaster_ { 0.0 };

    // Scratch buffers sized in prepare().
    std::vector<float> scratchMono_, dryL_, dryR_, sendDelayL_, sendDelayR_,
        sendReverbL_, sendReverbR_;

    std::array<std::atomic<float>, 2> outputLevel_ { 0.0f, 0.0f };
};

} // namespace septum
