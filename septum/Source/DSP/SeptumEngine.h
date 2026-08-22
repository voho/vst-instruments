// The Septum engine: a behavioral model of the Roland SH-201's virtual
// analog voice, grounded in the documents catalogued in
// Docs/sh201-replica-research.md. The hardware computes its whole voice on one
// custom DSP behind a documented analog output stage; this engine reproduces
// the documented architecture exactly — two tones of
// OSC1+OSC2 -> MIX/MOD -> FILTER -> AMP with three envelopes and two LFOs per
// tone, a shared modulation-delay -> reverb effects block, 10 voices halved in
// DUAL — and keeps every mapping from a 7-bit panel value to a physical
// quantity in one place (the `mapping` namespace) so each voiced constant can
// be replaced by a measurement without touching the render code.

#pragma once

#include "SeptumPatch.h"

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
// Panel-value-to-physics mappings. Tiers per the research contract:
// functions marked [settled] implement a documented law; [reported] follow a
// published measurement of related hardware; [voiced] are this project's
// choices, each owned by an open question.
// --------------------------------------------------------------------------
namespace mapping
{
    // [voiced, OQ-08] CUTOFF 0-127 -> Hz, exponential over ten octaves.
    [[nodiscard]] inline double cutoffHz (double value) noexcept
    {
        return 20.0 * std::exp2 (value * (10.0 / 127.0));
    }

    // [voiced, OQ-08] RESONANCE 0-127 -> state-variable damping k. k = 2 is
    // Q = 0.5; zero is the oscillation threshold; the top of the knob goes
    // slightly negative so self-oscillation grows until the stage limiter
    // holds it, matching the manual's "may not stop at all" warning.
    [[nodiscard]] inline double resonanceDamping (double value) noexcept
    {
        return 2.0 - 2.04 * (value / 127.0);
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

    // [voiced, OQ-11] DRIVE 0-127 -> pre-gain, up to +32 dB.
    [[nodiscard]] inline double overdrivePreGain (int drive) noexcept
    {
        return std::pow (10.0, (drive / 127.0) * (32.0 / 20.0));
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

    // [voiced] The oversampling factor that puts the shaper in that band:
    // 4x at 44.1/48 kHz, 2x at 88.2/96 kHz, none above.
    [[nodiscard]] inline int overdriveOversampling (double hostRateHz) noexcept
    {
        if (hostRateHz * 1.0 >= overdriveInternalRateHz * 0.98)
            return 1;
        if (hostRateHz * 2.0 >= overdriveInternalRateHz * 0.98)
            return 2;
        return 4;
    }

    // [voiced, OQ-12] Delay TIME 0-127 -> seconds, 1 ms to 1.3 s.
    [[nodiscard]] inline double delaySeconds (double value) noexcept
    {
        return 0.001 * std::pow (1300.0, value / 127.0);
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
    inline constexpr double fbOscLoopDamping = 0.55;
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
    // 12 dB/oct. Whether the hardware resonates on one stage or both is open.
    inline constexpr double filterSecondStageDamping = 1.2;
    // [voiced, OQ-08] Where the resonant stage's integrator states stop
    // growing, so self-oscillation is bounded as the manual's "may not stop at
    // all" implies rather than divergent.
    inline constexpr double filterStateLimit = 1.5;

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

    // [settled] One grid section's length in seconds. The manual gives the
    // divisions; the shuffle *amounts* are voiced (OQ-15) — Light and Heavy
    // are named, not measured. A shuffled pair keeps its total length and
    // moves the boundary inside it, so the beat never drifts.
    [[nodiscard]] inline double arpeggioShuffleRatio (ArpeggioGrid grid) noexcept
    {
        switch (grid)
        {
            case ArpeggioGrid::EighthLight:
            case ArpeggioGrid::SixteenthLight:
                return 0.58;
            case ArpeggioGrid::EighthHeavy:
            case ArpeggioGrid::SixteenthHeavy:
                return 0.66;
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

    // [voiced, OQ-15] The fixed velocity ACCENT 0 collapses the style's
    // programmed pattern onto.
    inline constexpr double arpeggioFlatVelocity = 100.0;

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
        const int walked = std::clamp (base + row - 1, 0, count - 1);
        int index = descending ? count - 1 - walked : walked;
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

    void prepare (double hostRateHz) noexcept;
    void clear() noexcept;
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

    struct Envelope
    {
        enum class Stage { Idle, Attack, Decay, Sustain, Release };
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
        std::uint32_t rng { 0x9e3779b9u };

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
        double combState { 0.0 };     // in-loop damping memory
        double hpfX1 { 0.0 }, hpfX2 { 0.0 }, hpfY1 { 0.0 }, hpfY2 { 0.0 };

        void clearRuntime() noexcept
        {
            combWrite = 0;
            combState = 0.0;
            hpfX1 = hpfX2 = hpfY1 = hpfY2 = 0.0;
            std::fill (comb.begin(), comb.end(), 0.0f);
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
        // Settled (part controller CC#66): the sostenuto pedal latches the
        // notes that were sounding when it went down and holds only those.
        bool sostenuto { false };
        std::uint32_t age { 0 };

        OscState osc1 {}, osc2 {};
        PitchEnvelope pitchEnv {};
        Envelope filterEnv {}, ampEnv {};
        SvfStage filter1 {}, filter2 {};
        double shelfState { 0.0 };

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
        double ampGainL { 0.0 }, ampGainR { 0.0 };
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
        };
        std::array<Row, arpeggioMaxRows> rows {};

        void clearKeys() noexcept
        {
            keyCount = 0;
            physicalCount = 0;
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
    void arpeggioStopPart (Part part);
    void handleArpeggioSwitch (bool nowOn);
    void advanceArpeggiator (int samples);
    void arpeggioFireStep();
    void arpeggioFireStepForPart (Part part, double stepSeconds);
    void processEffects (const float* dryL, const float* dryR,
                         const float* delaySendL, const float* delaySendR,
                         const float* reverbSendL, const float* reverbSendR,
                         float* outL, float* outR, int samples);
    void updateEffectCoefficients();
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
    double partLevel_ { 1.0 };
    double partPan_ { 0.0 };
    bool hold_ { false };
    bool sostenuto_ { false };

    std::array<Voice, maxPolyphony> voices_ {};
    std::array<ToneRuntime, partCount> tones_ {};
    std::array<ArpeggioRuntime, partCount> arpeggios_ {};
    double arpeggioStepRemaining_ { 0.0 };   // samples to the next boundary
    int arpeggioStep_ { 0 };
    bool arpeggioRunning_ { false };
    bool arpeggioActive_ { false };   // what the ARPEGGIO switch last was
    std::uint32_t arpeggioRng_ { 0x6d2b79f5u };
    std::uint32_t voiceClock_ { 0 };
    std::uint32_t rng_ { 0x2545f491u };

    // Effects.
    DelayLine delayL_ {}, delayR_ {};
    double delayModPhase_ { 0.0 };
    Reverb reverb_ {};
    double delayTimeSmoothed_ { 0.0 };
    double reverbFeedback_ { 0.0 };

    // External input: INPUT VOL -> CENTER CANCEL -> AUDIO FILTER on the direct
    // monitor path, and the pre-filter mono sum feeding any EXT-IN oscillator.
    ExternalInput external_ {};
    std::vector<float> externalDirectL_, externalDirectR_, externalMono_;
    SvfStage audioFilter1_[2] {}, audioFilter2_[2] {};
    double audioFilterG_ { 0.1 }, audioFilterK_ { 2.0 };
    bool audioFilterPrimed_ { false };
    double monitorGain_ { 1.0 };

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
