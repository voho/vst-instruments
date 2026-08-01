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
// The retention is set so the aggregate droop at the clock's own Nyquist is
// 2 dB, the middle of the 1-4 dB the per-stage-loss literature implies for a
// 256-stage low-noise line (droop = e^(-2 * stages * epsilon) with epsilon in
// the 1e-4..1e-3 band). For a one-pole advanced once per clock edge the
// Nyquist gain is a / (2 - a), so a = 2 d / (1 + d) with d = 10^(-2/20).
// A retention far below this -- an earlier revision used 0.34 -- parks a
// lowpass at ~0.07 of the clock rate, i.e. a 1.6-5.2 kHz corner *swept by the
// chorus modulation*, several times darker than either published account of
// this circuit's wet path allows.
constexpr float transferNyquistDroop = 0.794328f; // -2 dB
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

// Modelled noise floor of one line, referred to its own input. Uncompanded,
// the part measures 55 to 65 dB signal to noise in this circuit against its own
// 88 dB datasheet figure; 60 dB is the middle of that band. Modelling it is the
// point: the missing compander is what the effect is known for.
constexpr float lineNoiseAmplitude = 1.0e-3f;

// Support-filter corners. The published fifth-order model of this circuit puts
// them at 9.9 kHz in and 9.5 kHz out, while a direct measurement of the wet
// path in a sibling instrument fits a single pole at 14 kHz across the whole
// audio band -- a fifth-order pair at 9.5 kHz would be some 20 dB darker at
// 15 kHz than that measurement. The model takes the published corners at first
// order each, which lands between the two, and the research document records
// the disagreement rather than hiding it.
constexpr float antiAliasCornerHz = 9900.0f;
constexpr float reconstructionCornerHz = 9500.0f;

// Line gain. The wet path measures a few decibels above the dry; 2.3 dB is
// the figure the sibling-instrument measurement reports, and no capture of
// this instrument's own wet gain has been located, so the value is voiced on
// that measurement.
constexpr float lineGain = 1.303f;

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
    // Anchored to the published measurement of the sibling instrument's
    // chorus, the closest calibrated capture located: delay 1.66 ms to
    // 5.35 ms in both modes, rates 0.513 Hz and 0.863 Hz. The figures
    // reported for this instrument itself -- approximately 0.5 Hz and 0.8 Hz,
    // modes differing in rate alone -- agree to the reported precision, so
    // the sibling's numbers stand in for the unmeasured decimals. Modes I and
    // II differ in speed alone, not in depth, which is why II reads as more
    // agitated rather than wider.
    constexpr float centre = 0.5f * (0.00166f + 0.00535f);
    constexpr float sweep = 0.5f * (0.00535f - 0.00166f);
    switch (mode)
    {
        case ChorusMode::One:  return { 0.513f, centre, sweep, lineGain };
        case ChorusMode::Two:  return { 0.863f, centre, sweep, lineGain };
        case ChorusMode::Off:
        default:               return { 0.513f, centre, 0.0f, 0.0f };
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

    // Reconstruct the held staircase.
    return Chorus::supportFilterStep(reconstructionState, held, reconstructionG);
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
