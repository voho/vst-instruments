#include "YouKnow106Chorus.h"

#include <algorithm>
#include <cmath>

namespace youknow106
{
namespace
{
// Aggregate charge-transfer inefficiency of the whole line. A bucket-brigade
// stage hands on all but a small fraction of its charge, and the residue smears
// forward; over the line's stages the aggregate is well approximated by one
// pole running at the clock rate. This is the standard first-order
// condensation, not a per-stage solve. Unlike the support filters below it is
// specified directly as the per-edge retention coefficient rather than as a
// corner frequency, so it belongs in the plain recursion and not in the
// prewarped one.
//
// The retention is set from the part's own datasheet rather than from the
// per-stage-loss literature: the MN3009 is specified at -3 dB at 12 kHz with a
// 40 kHz clock, 256 stages. For a one-pole advanced once per clock edge the
// Nyquist gain is a / (2 - a), and the retention that puts the half-power point
// at 0.3 of the clock rate is 0.7725, which is a Nyquist droop of -4.0 dB.
//
// That is the dark end of the 1-4 dB band the literature implies, and it
// supersedes the -2 dB an earlier revision took from the middle of that band:
// a measured figure for this part beats a range inferred for the class. It is
// still far brighter than the 0.34 retention used before either -- that parked
// the corner near 2.7 kHz and swept it with the modulation.
constexpr float transferNyquistDroop = 0.629328f; // -4.0 dB, from -3 dB at 12 kHz
constexpr float transferSmear =
    2.0f * transferNyquistDroop / (1.0f + transferNyquistDroop);

// Where the line itself starts to compress, referred to the model's signal
// scale (1.0 = 2.6 V at the node). The part is rated 0.3% distortion at
// 0.78 Vrms and overloads a few decibels above; its bias window at a 15 V
// supply gives roughly +/-2.9 V of swing. The line is the first nonlinear
// element in the wet path -- the surrounding op-amps run on +/-15 V rails and
// stay linear at synth bus levels -- which is why a driven chorus grits the
// wet signal while the dry stays clean, a documented signature of this
// instrument.
constexpr float bbdSaturationLevel = 1.1f; // ~2.9 V at the node

// There is no divider ahead of the line. A previous revision put one here --
// 33 kOhm against 12 kOhm -- on a reading of the schematic that the schematic
// does not support: the 100 kOhm at each line's input injects adjustable DC
// bias from VR1/VR2 and is not the lower leg of anything, and the 10 kOhm
// against 2.2 nF beside it is a low-pass pole, not an attenuator. Roland sets
// the drive by calibration instead, trimming VR1/VR2 for symmetrical clipping
// with 10 Vpp at 1 kHz on the module board's TP2.
//
// The voice summer ahead of the whole effect does attenuate, but it attenuates
// dry and wet alike, so it is not a chorus divider either and cannot change the
// balance the summing resistors set.

// Modelled noise floor of one line, referred to its own input. Uncompanded,
// this circuit is reported between 55 and 65 dB signal to noise against the
// part's own 88 dB datasheet figure, and **that band still has no citation**:
// the closest located capture gives noise level alone, with no reference tone,
// calibrated level, weighting or stated bandwidth, so it cannot be turned into
// a signal-to-noise figure. Modelling the floor is the point regardless -- the
// missing compander is what the effect is known for.
constexpr float lineNoiseAmplitude = 1.0e-3f;

// Support filters, from the schematic rather than from an estimate of it. Each
// side of the line carries two emitter-follower Sallen-Key sections built on
// equal 22 kOhm pairs, so each section's corner comes from its capacitors and
// its Q from their ratio alone:
//
//   Tr13 / Tr15   820 pF feedback, 680 pF shunt  ->  9.69 kHz, Q 0.549
//   Tr14 / Tr16   1.8 nF feedback, 270 pF shunt  -> 10.38 kHz, Q 1.291
//
// and the input adds one passive pole, R122 10 kOhm against C52 2.2 nF, at
// 7.23 kHz. Five poles in, four modelled out.
//
// This replaces a single pole at 9.9 kHz in and 9.5 kHz out, which was a guess
// at a fifth-order response rather than the response itself, and was therefore
// far too bright: four poles near 10 kHz reach -12 dB at 15 kHz where one
// reaches -4. It also settles the standing disagreement in the right
// direction. A sibling's wet path had been fitted with a *single* pole at
// 14 kHz, which cannot describe this circuit; the schematic shows why a
// single-pole fit was never going to.
constexpr float antiAliasFirstHz = 9688.0f;
constexpr float antiAliasFirstFeedbackF = 820.0e-12f;
constexpr float antiAliasFirstShuntF = 680.0e-12f;
constexpr float antiAliasSecondHz = 10377.0f;
constexpr float antiAliasSecondFeedbackF = 1.8e-9f;
constexpr float antiAliasSecondShuntF = 270.0e-12f;
constexpr float antiAliasPassiveHz = 7234.0f;   // R122 10 kOhm x C52 2.2 nF

// The output's own fifth pole is the tap-summing node, and its corner is not
// set by the passive parts alone -- the part's output impedance is in series
// with them and is not specified. On the schematic's values against a summing
// node's low source impedance it lands well above the audio band, so it is left
// out rather than voiced at a number that would be doing no work.
constexpr float reconstructionFirstHz = 9688.0f;
constexpr float reconstructionSecondHz = 10377.0f;

// Line gain, from the summing stage's own parts rather than from a sibling's
// measurement. Dry and wet meet at an inverting op-amp through 47 kOhm and
// 39 kOhm into a 100 kOhm feedback, so the wet path arrives 47/39 louder --
// 1.62 dB. An earlier revision voiced 2.3 dB from a sibling capture.
//
// It is a ratio of two resistors in the same stage, so the feedback value
// cancels: only the imbalance reaches the model, which is why the absolute
// gains (6.56 dB and 8.18 dB) do not appear here.
constexpr float lineGain = 47.0f / 39.0f;   // +1.62 dB

std::uint32_t nextNoiseState(std::uint32_t state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float noiseFromState(std::uint32_t state) noexcept
{
    return static_cast<float>(state & 0xffffffu) * (2.0f / 16777215.0f) - 1.0f;
}

// Symmetric triangle over a 0..1 phase, in -1..+1.
float triangle(float phase) noexcept
{
    const float folded = phase < 0.5f ? phase : 1.0f - phase;
    return folded * 4.0f - 1.0f;
}
} // namespace

Chorus::ModeSettings Chorus::settingsFor(ChorusMode mode) noexcept
{
    // These are a *Juno-60's* measured figures, and they are labelled as such
    // deliberately: delay 1.66 ms to 5.35 ms, rates 0.513 Hz and 0.863 Hz, read
    // from a calibrated capture of that instrument. No qualifying capture of a
    // Juno-106's own chorus has been located -- a revision briefly adopted
    // 1.54-5.15 ms as one, which turned out not to exist -- and the figures
    // published for this instrument are only "about 0.5 Hz" and "about 0.8 Hz".
    //
    // The schematic does anchor the *ratio*. Mode II leaves a 2.2 MOhm resistor
    // in series that Mode I bypasses, giving effective timing resistances of
    // 6.435 and 3.964 MOhm and so a rate ratio of 1.623. The sibling's measured
    // pair gives 1.682, which agrees to about 3.6% -- close enough to say the
    // two instruments share this circuit, not close enough to claim the
    // decimals are this one's. The timing capacitor is illegible in the
    // schematic, so no absolute rate can be computed from it.
    //
    // Modes I and II differ in speed alone, not in depth, which is why II
    // reads as more agitated rather than wider.
    constexpr float centre = 0.5f * (0.00166f + 0.00535f);
    constexpr float sweep = 0.5f * (0.00535f - 0.00166f);
    constexpr float rateOne = 0.513f;
    constexpr float rateTwo = 0.863f;
    switch (mode)
    {
        case ChorusMode::One:  return { rateOne, centre, sweep, lineGain };
        case ChorusMode::Two:  return { rateTwo, centre, sweep, lineGain };
        // Both buttons down. Each switch shunts its own resistor into the
        // modulation oscillator's timing network, so closing both puts the two
        // in parallel: the conductances add, and with them the rate. That makes
        // I+II faster than either alone rather than a repeat of II, which is
        // what the setting is known for. The depth is untouched -- nothing in
        // the delay path changes, only how quickly it is swept.
        case ChorusMode::OneTwo:
            return { rateOne + rateTwo, centre, sweep, lineGain };
        case ChorusMode::Off:
        default:               return { rateOne, centre, 0.0f, 0.0f };
    }
}

float Chorus::onePoleG(float cutoffHz, float sampleRate) noexcept
{
    const float limited = std::clamp(cutoffHz, 20.0f, sampleRate * 0.45f);
    const float g = std::tan(3.14159265358979324f * limited / sampleRate);
    return g / (1.0f + g);
}

// One topology-preserving-transform lowpass step. The coefficient above only
// places the corner where it was asked for if the state is advanced by twice
// the difference term: the plain `state += g * (input - state)` recursion wants
// a different coefficient entirely and would put the 9.9 kHz corner near
// 4.6 kHz at the engine's 192 kHz internal rate.
float Chorus::supportFilterStep(float& state, float input, float g) noexcept
{
    const float difference = (input - state) * g;
    const float output = difference + state;
    state = output + difference;
    return output;
}

// An equal-resistor Sallen-Key's damping comes entirely from its capacitor
// ratio. Both sections here are built that way -- 22 kOhm twice -- so this is
// the whole of their Q.
float Chorus::sallenKeyQ(float feedbackFarads, float shuntFarads) noexcept
{
    if (shuntFarads <= 0.0f)
        return 0.5f;
    return 0.5f * std::sqrt(feedbackFarads / shuntFarads);
}

Chorus::BiquadCoefficients Chorus::sallenKeyCoefficients(float cutoffHz, float q,
                                                         float sampleRate) noexcept
{
    const float limited = std::clamp(cutoffHz, 20.0f, sampleRate * 0.45f);
    BiquadCoefficients coefficients;
    coefficients.g = std::tan(3.14159265358979324f * limited / sampleRate);
    coefficients.k = 1.0f / std::max(q, 0.05f);
    return coefficients;
}

// Zavalishin's topology-preserving state-variable form, lowpass output. The
// prewarped `g` puts the corner where it was asked for at any host rate, which
// a direct-form biquad with bilinear coefficients would not do as cleanly at
// the engine's 192 kHz internal rate.
float Chorus::biquadStep(BiquadState& state, float input,
                         const BiquadCoefficients& coefficients) noexcept
{
    const float g = coefficients.g;
    const float k = coefficients.k;
    // `k` is the form's own damping term, which is already 1/Q -- doubling it
    // here would halve every section's Q and darken the whole chain.
    const float highPass = (input - (k + g) * state.s1 - state.s2)
                         / (1.0f + g * (g + k));
    const float bandPass = g * highPass + state.s1;
    state.s1 = g * highPass + bandPass;
    const float lowPass = g * bandPass + state.s2;
    state.s2 = g * bandPass + lowPass;
    return lowPass;
}

Chorus::SupportChain Chorus::supportChainFor(float sampleRate) noexcept
{
    SupportChain chain;
    chain.passiveG = onePoleG(antiAliasPassiveHz, sampleRate);
    chain.antiAliasFirst = sallenKeyCoefficients(
        antiAliasFirstHz,
        sallenKeyQ(antiAliasFirstFeedbackF, antiAliasFirstShuntF), sampleRate);
    chain.antiAliasSecond = sallenKeyCoefficients(
        antiAliasSecondHz,
        sallenKeyQ(antiAliasSecondFeedbackF, antiAliasSecondShuntF), sampleRate);
    // The output sections are the same two networks again, part for part.
    chain.reconstructionFirst = sallenKeyCoefficients(
        reconstructionFirstHz,
        sallenKeyQ(antiAliasFirstFeedbackF, antiAliasFirstShuntF), sampleRate);
    chain.reconstructionSecond = sallenKeyCoefficients(
        reconstructionSecondHz,
        sallenKeyQ(antiAliasSecondFeedbackF, antiAliasSecondShuntF), sampleRate);
    return chain;
}

void Chorus::Line::reset(std::uint32_t seed) noexcept
{
    cells.fill(0.0f);
    writeIndex = 0;
    clockPhase = 0.0;
    held = 0.0f;
    previousInput = 0.0f;
    antiAliasState = 0.0f;
    antiAliasFirst.reset();
    antiAliasSecond.reset();
    reconstructionFirst.reset();
    reconstructionSecond.reset();
    transferState = 0.0f;
    noiseState = seed | 1u;
}

float Chorus::Line::process(float input, float clockHz, float sampleRate,
                            const SupportChain& support, float noiseScale) noexcept
{
    // Band-limit ahead of the line. Everything above half the clock would fold,
    // exactly as it does in the part. Five poles: the board's two Sallen-Key
    // sections, then its passive one.
    float limited = Chorus::biquadStep(antiAliasFirst, input, support.antiAliasFirst);
    limited = Chorus::biquadStep(antiAliasSecond, limited, support.antiAliasSecond);
    limited = Chorus::supportFilterStep(antiAliasState, limited, support.passiveG);

    const double increment =
        static_cast<double>(clockHz) / static_cast<double>(sampleRate);
    clockPhase += increment;
    // A clock above the host rate needs more than one shift per sample, which
    // is exactly what happens at 44.1 or 48 kHz with oversampling switched off.
    // The bound is the worst ratio the model supports -- the fastest clock
    // against the lowest host rate -- so every elapsed edge is consumed and no
    // backlog can build up and drag the delay off its setting.
    int shifts = 0;
    while (clockPhase >= 1.0 && shifts < maximumShiftsPerSample)
    {
        clockPhase -= 1.0;
        ++shifts;

        // Resample the input onto the clock edge. What is left in `clockPhase`
        // is measured in clock cycles, so it has to be divided by the increment
        // to become the fraction of a host sample since the edge -- using it
        // raw would place every edge far too close to the current sample.
        const float fraction = increment > 0.0
            ? static_cast<float>(std::clamp(clockPhase / increment, 0.0, 1.0))
            : 0.0f;
        const float atEdge = limited + (previousInput - limited) * fraction;
        // The line's own overload. The charge a cell can hold is bounded by
        // its bias window, so the wet path saturates before anything around
        // it does; driving the chorus hot grits the delayed signal only.
        const float bounded = bbdSaturationLevel
                            * std::tanh(atEdge / bbdSaturationLevel);

        writeIndex = writeIndex + 1 < cellPairs ? writeIndex + 1 : 0;
        const float emerging = cells[static_cast<std::size_t>(writeIndex)];
        cells[static_cast<std::size_t>(writeIndex)] = bounded;

        transferState += transferSmear * (emerging - transferState);
        noiseState = nextNoiseState(noiseState);
        held = transferState
             + noiseFromState(noiseState) * lineNoiseAmplitude * noiseScale;
    }
    // If the ratio somehow exceeded even that bound, drop the remainder rather
    // than carrying it: a backlog would make the line run slower than the clock
    // it was asked for and drift further out every sample.
    if (clockPhase >= 1.0)
        clockPhase -= std::floor(clockPhase);
    previousInput = limited;

    // Reconstruct the held staircase through the board's two output sections.
    const float first =
        Chorus::biquadStep(reconstructionFirst, held, support.reconstructionFirst);
    return Chorus::biquadStep(reconstructionSecond, first,
                              support.reconstructionSecond);
}

void Chorus::prepare(double sampleRate) noexcept
{
    sampleRate_ = static_cast<float>(std::clamp(sampleRate, 8000.0, 768000.0));
    inverseSampleRate_ = 1.0f / sampleRate_;
    support_ = supportChainFor(sampleRate_);
    reset();
}

void Chorus::reset() noexcept
{
    lineA_.reset(0x9e3779b9u);
    lineB_.reset(0x85ebca6bu);
    lfoPhase_ = 0.0f;
    wetGain_ = 0.0f;
    rateHz_ = 0.513f;
    sweep_ = 0.0f;
    centreDelay_ = settingsFor(ChorusMode::Off).centreDelaySeconds;
    // A patch loaded with the effect switched on is not a player reaching for
    // the button: there is nothing to glide from. The first sample after a
    // reset takes the mode as it stands, and only changes made afterwards
    // glide.
    primed_ = false;
}

void Chorus::process(float input, ChorusMode mode, float noiseScale,
                     float& left, float& right) noexcept
{
    const auto target = settingsFor(mode);

    if (!primed_)
    {
        rateHz_ = target.rateHz;
        sweep_ = target.sweepSeconds;
        centreDelay_ = target.centreDelaySeconds;
        wetGain_ = target.wetGain;
        primed_ = true;
    }

    // The mode buttons do not reach the delay lines through any slow network:
    // the lines keep clocking, fully modulated, even with the effect off, and
    // the switch merely un-mutes the wet through a transistor pair. So the
    // clock programme steps to its new setting at once -- the delay itself
    // cannot jump, because the cells hold their contents and only the shift
    // rate changes -- and only the wet mute carries a short declick, on the
    // scale of the switching transistors' own settling. Switching *off*
    // changes nothing but the mute, so the lines go on sweeping underneath
    // and re-engaging the effect finds them mid-sweep, as the hardware's are.
    if (mode != ChorusMode::Off)
    {
        rateHz_ = target.rateHz;
        sweep_ = target.sweepSeconds;
        centreDelay_ = target.centreDelaySeconds;
    }
    const float muteGlide = 1.0f - std::exp(-inverseSampleRate_ / 0.005f);
    wetGain_ += (target.wetGain - wetGain_) * muteGlide;

    lfoPhase_ += rateHz_ * inverseSampleRate_;
    if (lfoPhase_ >= 1.0f)
        lfoPhase_ -= std::floor(lfoPhase_);
    const float modulation = triangle(lfoPhase_);

    // The two lines are clocked in anti-phase: when one delay lengthens the
    // other shortens. That is where the width comes from, and it is also why
    // the effect partly cancels when the two channels are summed to mono.
    const float delayA = std::max(centreDelay_ + sweep_ * modulation, 1.0e-4f);
    const float delayB = std::max(centreDelay_ - sweep_ * modulation, 1.0e-4f);
    const float clockA = std::clamp(clockForDelaySeconds(delayA),
                                    minimumClockHz, maximumClockHz);
    const float clockB = std::clamp(clockForDelaySeconds(delayB),
                                    minimumClockHz, maximumClockHz);

    const float wetA = lineA_.process(input, clockA, sampleRate_, support_, noiseScale);
    const float wetB = lineB_.process(input, clockB, sampleRate_, support_, noiseScale);

    // Both channels carry dry plus wet. The width comes from the two lines
    // being clocked in antiphase, not from inverting one side, so summing to
    // mono thins the effect rather than cancelling it.
    left = input + wetA * wetGain_;
    right = input + wetB * wetGain_;
}

} // namespace youknow106
