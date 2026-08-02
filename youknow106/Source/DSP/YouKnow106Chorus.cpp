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

// The voice bus is stepped down before the line and brought back up after it.
// The divider is 33 kOhm in series against 12 kOhm to ground, so the line sees
// 12/45 of the bus -- 11.5 dB down -- which is what keeps a nominal bus inside
// the part's rated 0.78 Vrms instead of sitting on its overload point.
//
// Applying it explicitly rather than folding it into the saturation threshold
// is what makes the two things it governs come out right at once. The line now
// needs 11.5 dB more drive before it grits, so the chorus stays clean at
// ordinary levels and dirties only when pushed, as the hardware does. And the
// line's own noise is attenuated relative to nothing while the restoring gain
// lifts it with the signal -- which is precisely why an uncompanded delay
// hisses, and is the mechanism this circuit is known for rather than a floor
// bolted on afterwards.
constexpr float bbdInputDivider = 12.0f / 45.0f;   // -11.48 dB
constexpr float bbdOutputRestore = 1.0f / bbdInputDivider;

// Modelled noise floor of one line, referred to the line's own input. The
// circuit measures 60 dB signal to noise unweighted over 20 Hz to 20 kHz
// referred to 0 dBu, against the part's own 88 dB datasheet figure -- the gap
// being clock feedthrough, op-amp noise and the absence of any compander.
//
// Stated at the line's input, so the restoring gain above is what carries it to
// the output: the constant is the 60 dB output figure divided back down through
// the divider, which leaves the audible floor exactly where it was measured.
constexpr float lineNoiseAmplitude = 1.0e-3f * bbdInputDivider;

// Support-filter corners. The published fifth-order model of this circuit puts
// them at 9.9 kHz in and 9.5 kHz out, while a direct measurement of the wet
// path in a sibling instrument fits a single pole at 14 kHz across the whole
// audio band -- a fifth-order pair at 9.5 kHz would be some 20 dB darker at
// 15 kHz than that measurement. The model takes the published corners at first
// order each, which lands between the two, and the research document records
// the disagreement rather than hiding it.
constexpr float antiAliasCornerHz = 9900.0f;
constexpr float reconstructionCornerHz = 9500.0f;

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
    // Anchored to a calibrated capture of this instrument, which replaces the
    // sibling's numbers an earlier revision stood in with: delay 1.54 ms to
    // 5.15 ms, the same in every mode, rates 0.513 Hz and 0.863 Hz. Those
    // bounds imply an MN3009 clock of 83.1 kHz and 24.9 kHz -- 256 stages at
    // 128/f_clock -- both comfortably inside the part's 10-200 kHz window,
    // which is the check that the capture describes this circuit at all.
    //
    // Modes I and II differ in speed alone, not in depth, which is why II
    // reads as more agitated rather than wider.
    constexpr float centre = 0.5f * (0.00154f + 0.00515f);
    constexpr float sweep = 0.5f * (0.00515f - 0.00154f);
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

void Chorus::Line::reset(std::uint32_t seed) noexcept
{
    cells.fill(0.0f);
    writeIndex = 0;
    clockPhase = 0.0;
    held = 0.0f;
    previousInput = 0.0f;
    antiAliasState = 0.0f;
    reconstructionState = 0.0f;
    transferState = 0.0f;
    noiseState = seed | 1u;
}

float Chorus::Line::process(float input, float clockHz, float sampleRate,
                            float antiAliasG, float reconstructionG,
                            float noiseScale) noexcept
{
    // Band-limit ahead of the line. Everything above half the clock would fold,
    // exactly as it does in the part.
    const float limited = Chorus::supportFilterStep(antiAliasState, input, antiAliasG);

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
        const float atEdge =
            (limited + (previousInput - limited) * fraction) * bbdInputDivider;
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

    // Reconstruct the held staircase, and undo the input divider. The line's
    // noise rides up with the signal here, which is the whole reason an
    // uncompanded delay is audible at rest.
    return bbdOutputRestore
         * Chorus::supportFilterStep(reconstructionState, held, reconstructionG);
}

void Chorus::prepare(double sampleRate) noexcept
{
    sampleRate_ = static_cast<float>(std::clamp(sampleRate, 8000.0, 768000.0));
    inverseSampleRate_ = 1.0f / sampleRate_;
    antiAliasG_ = onePoleG(antiAliasCornerHz, sampleRate_);
    reconstructionG_ = onePoleG(reconstructionCornerHz, sampleRate_);
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

    const float wetA = lineA_.process(input, clockA, sampleRate_, antiAliasG_,
                                      reconstructionG_, noiseScale);
    const float wetB = lineB_.process(input, clockB, sampleRate_, antiAliasG_,
                                      reconstructionG_, noiseScale);

    // Both channels carry dry plus wet. The width comes from the two lines
    // being clocked in antiphase, not from inverting one side, so summing to
    // mono thins the effect rather than cancelling it.
    left = input + wetA * wetGain_;
    right = input + wetB * wetGain_;
}

} // namespace youknow106
