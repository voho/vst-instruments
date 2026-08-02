#include "YouKnow106Engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace youknow106
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 6.28318530717958647692f;

// Signal levels are volts at the modelled node throughout the voice, so the
// transconductor nonlinearity engages at the level the real circuit engages it.
// The summing node runs at the oscillator's own 12 Vpp; the filter module's
// output is specified at +/-2.4 V, so the attenuator between them is what sets
// the working level, and 2.4 V is what maps to full scale.
constexpr float filterInputAttenuation = 0.40f;
constexpr float voltsToSample = 1.0f / 2.6f;

// The output amplifier is linear to this fraction of full scale and folds
// smoothly above it. Running every source at maximum overdrives the real
// instrument's output stage too; what this rules out is a plug-in that answers
// with +9 dBFS instead.
constexpr float outputLinearCeiling = 0.80f;

// Deviation of the ramp from a straight line. The service notes describe an
// integrator, and the located reverse-engineering account of this oscillator
// agrees: the control voltage feeds a resistor into a virtual-ground
// integrator, which is a constant-current charge and therefore straight. An
// earlier revision kept a bow on the strength of a misreading of that
// account; the straight ramp is also the only shape consistent with the
// comparator's published 6 V / 50% duty anchor, since a bowed ramp puts 50%
// duty at a different voltage.
constexpr float rampBow = 0.0f;

// Mixer weights at the summing node ahead of the high-pass, referred to the
// ramp's own amplitude. The service notes hold both the sawtooth and the
// pulse at approximately 12 Vpp, so the two carry the same weight. The noise
// weight is voiced: the service procedure has a noise-level adjustment on the
// module board, but no located source states its target, so 4 Vpp against the
// ramp's 12 Vpp stands until a capture fixes it.
constexpr float sawMixVolts = 6.0f;
constexpr float pulseMixVolts = 6.0f;
constexpr float subMixVolts = 5.0f;
constexpr float noiseMixVolts = 2.0f;

// Input-referred noise of the transconductor stages. It is inaudible under any
// signal, and it is also the only reason a filter pushed past its oscillation
// threshold with no oscillator running has anything to start from -- a silent
// model would sit at exactly zero forever, which no analogue filter does.
constexpr float filterNoiseVolts = 2.0e-5f;

// Envelope segment behavioural anchors, from timing analysis of the
// generator firmware and its tables. The attack's duration is linear in the
// slider across the lower half -- about 8.4 ms per step, reaching roughly
// 0.54 s at mid-travel -- then accelerates through the knots below to about
// 3.3 s at the top. The published 1.5 ms minimum is the single-pass floor.
constexpr float envelopeMinimumSeconds = 0.0015f;
constexpr float attackMidTravelTicks = 254.0f; // ticks per unit travel below 0.5
// (travel, scan ticks) knots above mid-travel, log-linear between.
constexpr float attackKnotTravel[] = { 0.5f, 0.685f, 0.85f, 0.96f, 1.0f };
constexpr float attackKnotTicks[]  = { 127.0f, 255.0f, 493.0f, 628.0f, 780.0f };

// Falling-segment behavioural anchors: the per-pass retention coefficient's
// decay rate lambda = -ln(coefficient), log-linear in the slider between the
// knots below. They reproduce the firmware's measured times -- roughly
// 0.33 s, 2.2 s and 4.5 s to -20 dB at quarter, half and three-quarter
// travel, one pass at the bottom, and 9.6 s to half level at the top. The
// audible tail ends earlier than the arithmetic suggests because the
// envelope is truncated to its integer grid and the amplifier's knee
// swallows the last few percent.
constexpr float decayKnotTravel[]  = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
constexpr float decayKnotLambda[]  = { 2.77f, 0.0293f, 0.0044f, 0.00215f,
                                       0.000303f };

// The modulator's rate coefficient, normalised to the accumulator span: the
// fraction of a full half-sweep one scan pass adds. The firmware table has a
// fine-resolution bottom, two straight segments, and an accelerating top;
// the knots below reproduce its measured rates -- 0.04 Hz at the bottom,
// about 4.8 Hz at mid-travel, and 29.8 Hz at the top, where the clamp
// quantises the period to two passes per half-sweep.
constexpr float lfoKnotTravel[] = { 0.0f, 0.063f, 0.5f, 0.748f, 1.0f };
constexpr float lfoKnotCoefficient[] = { 5.0f / 8192.0f, 90.0f / 8192.0f,
                                         650.0f / 8192.0f, 1162.0f / 8192.0f,
                                         5113.0f / 8192.0f };
// The delay's fade stage: a per-pass increment selected by the top three
// bits of the pot, so the fade time is a staircase over the travel --
// instant across the bottom eighth, three short steps, then a fixed fade
// just over a second long for the whole upper half.
constexpr float lfoFadeStepSeconds[] = { 0.0f, 0.06f, 0.125f, 0.19f,
                                         1.09f, 1.09f, 1.09f, 1.09f };

// Portamento runs at a constant rate in pitch, not on a time constant: the
// panel sets seconds per octave, so a wide leap takes proportionally longer.
constexpr float portamentoFastestSecondsPerOctave = 0.05f;
constexpr float portamentoSlowestSecondsPerOctave = 12.9f;

// Amplifier control law. The converter ahead of the amplifier is a
// transistor with strong emitter degeneration, and a measured sweep of a
// real unit shows what that circuit predicts: gain tracks the control
// voltage *linearly* over most of the range -- half control is half
// amplitude, six decibels down, not thirty -- with an exponential knee
// confined to the bottom tenth where the transistor barely conducts,
// falling away at roughly 26 dB per tenth of the control range. The
// instrument's dB-linear decay tails come from the *generator's*
// exponential falling segments through this quasi-linear amplifier -- the
// opposite factorisation from the pure-exponential converter an earlier
// revision modelled, which sat mid-level sustains tens of decibels too
// quiet.
constexpr float vcaKnee = 0.12f;
constexpr float vcaKneeDbPerUnit = 260.0f;
// Below this the converter genuinely does not conduct: the amplifier shuts
// fully rather than leaking. A residual leak is what a failed voice module
// does, not what a working one does.
constexpr float vcaDeadband = 0.005f;

float clamp01(float value) noexcept
{
    return std::clamp(value, 0.0f, 1.0f);
}

float sanitised(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

// Exactly linear below the ceiling, smoothly bounded to unity above it, with a
// continuous first derivative at the join.
float outputStage(float value) noexcept
{
    const float magnitude = std::abs(value);
    if (magnitude <= outputLinearCeiling)
        return value;
    const float span = 1.0f - outputLinearCeiling;
    const float excess = (magnitude - outputLinearCeiling) / span;
    const float folded = outputLinearCeiling + span * std::tanh(excess);
    return value < 0.0f ? -folded : folded;
}
} // namespace

// ---------------------------------------------------------------------------
// Modelled hardware laws
// ---------------------------------------------------------------------------

double YouKnow106Engine::rangeClockHz(DcoRange range) noexcept
{
    switch (range)
    {
        case DcoRange::Sixteen: return masterClockHz / 8.0;
        case DcoRange::Four:    return masterClockHz / 2.0;
        case DcoRange::Eight:
        default:                return masterClockHz / 4.0;
    }
}

std::uint32_t YouKnow106Engine::dcoDivider(double frequencyHz) noexcept
{
    if (!(frequencyHz > 0.0) || !std::isfinite(frequencyHz))
        return maximumDivider;

    const double exact = rangeClockHz(DcoRange::Eight) / frequencyHz;
    if (!(exact > 0.0) || !std::isfinite(exact))
        return maximumDivider;

    const double rounded = std::floor(exact + 0.5);
    const double limited = std::clamp(rounded,
                                      static_cast<double>(minimumDivider),
                                      static_cast<double>(maximumDivider));
    return static_cast<std::uint32_t>(limited);
}

double YouKnow106Engine::dcoQuantisedFrequency(std::uint32_t divider,
                                               DcoRange range) noexcept
{
    const std::uint32_t limited = std::clamp(divider, minimumDivider, maximumDivider);
    return rangeClockHz(range) / static_cast<double>(limited);
}

float YouKnow106Engine::vcfCutoffHz(float counts) noexcept
{
    // The count law is exact through the audio band; above the knee the
    // exponential converter saturates, bending smoothly onto the measured
    // ceiling. The digital sum upstream is already clamped to the 14-bit
    // accumulator; the margin here only covers the analogue trim and drift
    // that ride on top of it.
    const float safe = std::clamp(sanitised(counts, 0.0f), -2000.0f, 20000.0f);
    const float hz = vcfBaseFrequencyHz * std::exp2(safe / vcfCountsPerOctave);
    if (hz <= vcfKneeStartHz)
        return std::max(hz, 1.0f);
    const float span = vcfConverterCeilingHz - vcfKneeStartHz;
    return vcfKneeStartHz + span * std::tanh((hz - vcfKneeStartHz) / span);
}

float YouKnow106Engine::vcfPanelCounts(float panelPosition) noexcept
{
    // The panel slider is read as a 0..127 byte and the converter is driven
    // with that byte times 128, so the whole slider spans 16256 counts.
    const float byte = std::floor(clamp01(panelPosition) * 127.0f + 0.5f);
    return byte * 128.0f;
}

float YouKnow106Engine::vcfFeedback(float panelPosition) noexcept
{
    // Regeneration reaches the four-pole cascade's oscillation threshold at
    // 90% of the travel, and the last tenth pushes past it to the
    // hardware-fitted maximum of 4.19. Below the threshold the curve is not
    // linear: a fitted measurement of this filter puts the loop gain near
    // 0.91 at 30% travel, well under a straight line's 1.33, so the model
    // takes the quadratic through that point and the threshold.
    const float position = clamp01(panelPosition);
    if (position <= vcfSelfOscillationTravel)
        return 2.3277778f * position + 2.3518519f * position * position;
    const float past = (position - vcfSelfOscillationTravel)
                     / (1.0f - vcfSelfOscillationTravel);
    return vcfSelfOscillationFeedback
         + (vcfMaximumFeedback - vcfSelfOscillationFeedback) * past;
}

float YouKnow106Engine::vcfResonanceCompensation(float feedback) noexcept
{
    // The instrument compensates the resonant passband loss on the *input*
    // side: it drives more signal in as regeneration rises. That is why a
    // high-resonance patch here grows dirtier rather than thinner, and it is
    // the opposite of an output-side make-up gain. The fitted hardware figure
    // is linear in the loop gain -- about 23% more drive per unit of it,
    // reaching just under six decibels at the top, not the nine an earlier
    // revision used.
    const float k = std::clamp(sanitised(feedback, 0.0f), 0.0f, 8.0f);
    return 1.0f + 0.2296f * k;
}

float YouKnow106Engine::vcfResonanceFrequencyTrim(float feedback) noexcept
{
    // With the loop limited in its own divider, as in the hardware, the
    // oscillation sits close to the small-signal law on its own; what is
    // left is the forward stages' slight compression at the limit-cycle
    // amplitude, absorbed by the service procedure's per-voice trimmer. The
    // trim is calibrated so the rendered oscillation lands on the control
    // law's pitch -- the service note's 248 Hz check at converter code 6272.
    const float k = std::clamp(sanitised(feedback, 0.0f), 0.0f, 8.0f);
    const float fraction = std::min(k / vcfSelfOscillationFeedback, 1.2f);
    return 1.0f + vcfOscillationTrim * fraction * fraction;
}

namespace
{
// Log-linear interpolation through behavioural knots: y moves geometrically
// between knots as x moves linearly. The laws built on this are fits through
// measured firmware behaviour, not through table data.
template <std::size_t N>
float knotInterpolate(float x, const float (&knotX)[N],
                      const float (&knotY)[N]) noexcept
{
    if (x <= knotX[0])
        return knotY[0];
    for (std::size_t i = 1; i < N; ++i)
        if (x <= knotX[i])
        {
            const float span = knotX[i] - knotX[i - 1];
            const float t = span > 0.0f ? (x - knotX[i - 1]) / span : 1.0f;
            return knotY[i - 1]
                 * std::pow(knotY[i] / knotY[i - 1], t);
        }
    return knotY[N - 1];
}

template <std::size_t N>
float knotInvert(float y, const float (&knotX)[N],
                 const float (&knotY)[N]) noexcept
{
    const bool ascending = knotY[N - 1] > knotY[0];
    if (ascending ? y <= knotY[0] : y >= knotY[0])
        return knotX[0];
    for (std::size_t i = 1; i < N; ++i)
        if (ascending ? y <= knotY[i] : y >= knotY[i])
        {
            const float t = std::log(y / knotY[i - 1])
                          / std::log(knotY[i] / knotY[i - 1]);
            return knotX[i - 1] + (knotX[i] - knotX[i - 1]) * t;
        }
    return knotX[N - 1];
}

// Scan passes an attack takes at a panel position: linear in the travel
// across the lower half, then log-linear through the firmware's knots.
float attackScanTicks(float position) noexcept
{
    const float p = std::clamp(position, 0.0f, 1.0f);
    if (p <= 0.5f)
        return std::max(1.0f, std::floor(attackMidTravelTicks * p + 0.5f));
    return knotInterpolate(p, attackKnotTravel, attackKnotTicks);
}

// Per-pass decay rate lambda; the retention coefficient is exp(-lambda).
float decayLambdaPerTick(float position) noexcept
{
    return knotInterpolate(std::clamp(position, 0.0f, 1.0f),
                           decayKnotTravel, decayKnotLambda);
}

// The modulator's per-pass accumulator step, as a fraction of a half-sweep.
float lfoCoefficient(float position) noexcept
{
    return knotInterpolate(std::clamp(position, 0.0f, 1.0f),
                           lfoKnotTravel, lfoKnotCoefficient);
}

float lfoFadeSeconds(float position) noexcept
{
    const int index = std::clamp(
        static_cast<int>(std::clamp(position, 0.0f, 1.0f) * 8.0f), 0, 7);
    return lfoFadeStepSeconds[index];
}
} // namespace

float YouKnow106Engine::envelopeAttackSeconds(float panelPosition) noexcept
{
    return attackScanTicks(clamp01(panelPosition))
         * static_cast<float>(1.0 / controlScanHz);
}

float YouKnow106Engine::envelopeDecaySeconds(float panelPosition) noexcept
{
    // Displayed as the time the segment takes to fall to a tenth of its
    // span -- the measure the hardware anchors were taken in. The full tail
    // runs longer, exactly as the instrument's does.
    const float lambda = decayLambdaPerTick(clamp01(panelPosition));
    return 2.302585f / lambda * static_cast<float>(1.0 / controlScanHz);
}

float YouKnow106Engine::envelopeReleaseSeconds(float panelPosition) noexcept
{
    return envelopeDecaySeconds(panelPosition);
}

float YouKnow106Engine::lfoRateHz(float panelPosition) noexcept
{
    // The rate the accumulator mechanism actually produces: whole passes per
    // half-sweep, so fast settings land on a quantised grid exactly as the
    // hardware's do.
    const float coefficient = lfoCoefficient(clamp01(panelPosition));
    const float passes = std::ceil(1.0f / std::max(coefficient, 1.0e-6f));
    return static_cast<float>(controlScanHz) / (4.0f * passes);
}

float YouKnow106Engine::lfoDelaySeconds(float panelPosition) noexcept
{
    // Total time from the note to full modulation depth: a silent hold that
    // grows across the whole travel at the attack table's own rate, plus the
    // stepped fade.
    const float position = clamp01(panelPosition);
    if (position <= 1.0e-4f)
        return 0.0f;
    return envelopeAttackSeconds(position) + lfoFadeSeconds(position);
}

float YouKnow106Engine::portamentoSeconds(float panelPosition) noexcept
{
    // Seconds per octave, not seconds per glide. Zero travel switches it off.
    const float position = clamp01(panelPosition);
    if (position <= 1.0e-4f)
        return 0.0f;
    return portamentoFastestSecondsPerOctave
         * std::pow(portamentoSlowestSecondsPerOctave
                    / portamentoFastestSecondsPerOctave, position);
}

namespace
{
// Inverse of a law of the form value = low * (high / low) ^ position.
float exponentialPosition(float value, float low, float high) noexcept
{
    if (!(value > 0.0f) || !std::isfinite(value))
        return 0.0f;
    const float span = std::log(high / low);
    return std::clamp(std::log(std::max(value, low) / low) / span, 0.0f, 1.0f);
}
} // namespace

float YouKnow106Engine::panelPositionForAttack(float seconds) noexcept
{
    if (!(seconds > 0.0f) || !std::isfinite(seconds))
        return 0.0f;
    const float ticks = seconds * static_cast<float>(controlScanHz);
    if (ticks <= attackKnotTicks[0])
        return std::clamp(ticks / attackMidTravelTicks, 0.0f, 0.5f);
    return knotInvert(ticks, attackKnotTravel, attackKnotTicks);
}

float YouKnow106Engine::panelPositionForDecay(float seconds) noexcept
{
    if (!(seconds > 0.0f) || !std::isfinite(seconds))
        return 0.0f;
    const float lambda = 2.302585f
                       / (seconds * static_cast<float>(controlScanHz));
    return knotInvert(lambda, decayKnotTravel, decayKnotLambda);
}

float YouKnow106Engine::panelPositionForLfoRate(float hertz) noexcept
{
    if (!(hertz > 0.0f) || !std::isfinite(hertz))
        return 0.0f;
    const float passes = std::max(
        1.0f, std::floor(static_cast<float>(controlScanHz) / (4.0f * hertz)
                         + 0.5f));
    return knotInvert(1.0f / passes, lfoKnotTravel, lfoKnotCoefficient);
}

float YouKnow106Engine::panelPositionForLfoDelay(float seconds) noexcept
{
    if (!(seconds > 0.0f) || !std::isfinite(seconds))
        return 0.0f;
    // The hold-plus-fade total is monotone in the travel but has no closed
    // inverse; a short bisection is exact enough for a typed value.
    float low = 0.0f;
    float high = 1.0f;
    for (int i = 0; i < 24; ++i)
    {
        const float mid = 0.5f * (low + high);
        if (lfoDelaySeconds(mid) < seconds)
            low = mid;
        else
            high = mid;
    }
    return 0.5f * (low + high);
}

float YouKnow106Engine::panelPositionForPortamento(float secondsPerOctave) noexcept
{
    if (!(secondsPerOctave > 0.0f) || !std::isfinite(secondsPerOctave))
        return 0.0f;
    return exponentialPosition(secondsPerOctave, portamentoFastestSecondsPerOctave,
                               portamentoSlowestSecondsPerOctave);
}

float YouKnow106Engine::panelPositionForCutoff(float hertz) noexcept
{
    if (!(hertz > 0.0f) || !std::isfinite(hertz))
        return 0.0f;
    const float counts = vcfCountsPerOctave
                       * std::log2(std::max(hertz, vcfBaseFrequencyHz) / vcfBaseFrequencyHz);
    // The travel is read as a 0..127 byte driving the converter 128 counts at
    // a time, so this is the inverse of that quantisation, not of a continuum.
    return std::clamp(counts / (127.0f * 128.0f), 0.0f, 1.0f);
}

float YouKnow106Engine::pwmControlVolts(float depth) noexcept
{
    // The comparator threshold runs from +6 V, where the ramp is bisected and
    // the pulse is square, down to +0.6 V, where it is 95% wide. It cannot be
    // driven to either rail, so 0% and 100% are unreachable.
    return 6.0f - 5.4f * clamp01(depth);
}

// Where the comparator's two edges sit within one cycle, as fractions of the
// period. The threshold is the ramp voltage the rise crosses on the way up, and
// the comparator holds until the descending reset passes back through the same
// voltage -- not until the reset begins, where the ramp is still at its
// positive rail. At a high note the reset is a noticeable part of the period,
// so dropping at its start would shorten the high interval by about
// duty * reset: a 95% pulse asked for at the top of the 4' range would come out
// nearer 90%.
float YouKnow106Engine::rampSegmentVoltage(float risePosition) noexcept
{
    return 2.0f * rampVoltage(risePosition, rampBow) - 1.0f;
}

float YouKnow106Engine::pulseRisePhase(float duty, float resetFraction) noexcept
{
    const float reset = std::clamp(resetFraction, 0.0f, 0.25f);
    const float rise = std::max(1.0f - reset, 1.0e-4f);
    return rise * (1.0f - std::clamp(duty, 0.0f, 1.0f));
}

float YouKnow106Engine::pulseFallPhase(float duty, float resetFraction) noexcept
{
    const float reset = std::clamp(resetFraction, 0.0f, 0.25f);
    const float rise = std::max(1.0f - reset, 1.0e-4f);
    // The ramp runs 0..1 over the rise and falls linearly back over the reset,
    // both mapped to -1..+1. Solving the falling segment for the rise's
    // threshold gives the fraction of the reset spent still above it.
    const float threshold = rampVoltage(1.0f - std::clamp(duty, 0.0f, 1.0f), rampBow);
    return rise + reset * std::clamp(1.0f - threshold, 0.0f, 1.0f);
}

float YouKnow106Engine::pwmDutyCycle(float controlVolts) noexcept
{
    // The ramp spans 12 V peak to peak; the fraction of the period spent above
    // the threshold is the duty cycle.
    const float volts = std::clamp(sanitised(controlVolts, 6.0f), 0.6f, 6.0f);
    return std::clamp(1.0f - volts / 12.0f, 0.05f, 0.95f);
}

float YouKnow106Engine::vcaGain(float control) noexcept
{
    // Quasi-linear: gain tracks the control voltage over most of the range,
    // as the degenerated converter makes it, with the exponential knee
    // confined to the bottom tenth. Below the conduction threshold the
    // amplifier is simply shut.
    const float level = clamp01(sanitised(control, 0.0f));
    if (level <= vcaDeadband)
        return 0.0f;
    const float kneeDb = vcaKneeDbPerUnit * std::max(0.0f, vcaKnee - level);
    return level * std::pow(10.0f, -kneeDb / 20.0f);
}

float YouKnow106Engine::highPassCornerHz(HighPassMode mode) noexcept
{
    // Four legs of a switched RC network: a shelving boost, a
    // straight-through leg, and two progressively higher corners. The cut
    // corners follow from the network's own part values -- an effective
    // 44.9 kOhm against 15 nF and 4.7 nF -- and the boost's corner is the
    // measured shelf's pole near 59 Hz.
    switch (mode)
    {
        case HighPassMode::Boost: return 59.4f;
        case HighPassMode::Two:   return 236.0f;
        case HighPassMode::Three: return 754.0f;
        case HighPassMode::One:
        default:                  return 59.4f;
    }
}

float YouKnow106Engine::highPassShelfGain(HighPassMode mode) noexcept
{
    // How much of the low band the leg returns. The boost position is a real
    // measured shelf: +10.5 dB at DC, verified against a hardware noise
    // sweep -- far more than the +3 dB an earlier account reported. The
    // straight-through leg returns the low band untouched, and the two
    // cutting legs discard it.
    switch (mode)
    {
        case HighPassMode::Boost: return std::pow(10.0f, 10.5f / 20.0f);
        case HighPassMode::One:   return 1.0f;
        case HighPassMode::Two:
        case HighPassMode::Three:
        default:                  return 0.0f;
    }
}

float YouKnow106Engine::highPassHighGain(HighPassMode mode) noexcept
{
    // The boost leg lifts the high band a little too -- the measured shelf
    // settles at +1.41 dB well above its corner. Every other leg passes the
    // high band at unity.
    switch (mode)
    {
        case HighPassMode::Boost: return std::pow(10.0f, 1.41f / 20.0f);
        case HighPassMode::One:
        case HighPassMode::Two:
        case HighPassMode::Three:
        default:                  return 1.0f;
    }
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

double YouKnow106Engine::midiToHz(double midiNote) noexcept
{
    return 440.0 * std::pow(2.0, (midiNote - 69.0) / 12.0);
}

std::uint32_t YouKnow106Engine::hash32(std::uint32_t value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float YouKnow106Engine::hashBipolar(std::uint32_t value) noexcept
{
    return static_cast<float>(hash32(value) & 0xffffffu) * (2.0f / 16777215.0f) - 1.0f;
}

float YouKnow106Engine::rampVoltage(float phase, float bow) noexcept
{
    const float p = clamp01(phase);
    if (bow <= 1.0e-4f)
        return p;
    const float denominator = 1.0f - std::exp(-bow);
    return (1.0f - std::exp(-bow * p)) / denominator;
}

float YouKnow106Engine::resetFraction(double periodSeconds) noexcept
{
    if (!(periodSeconds > 0.0))
        return 0.25f;
    const double fraction = static_cast<double>(rampResetSeconds) / periodSeconds;
    return static_cast<float>(std::clamp(fraction, 1.0e-6, 0.25));
}

// ---------------------------------------------------------------------------
// Bandlimiting
// ---------------------------------------------------------------------------

void YouKnow106Engine::BandlimitedTrack::reset() noexcept
{
    ring.fill(0.0f);
    delay.fill(0.0f);
    base = 0;
    primed = false;
}

void YouKnow106Engine::BandlimitedTrack::prime(float value) noexcept
{
    if (primed)
        return;
    delay.fill(value);
    primed = true;
}

float YouKnow106Engine::BandlimitedTrack::advance(float naive) noexcept
{
    prime(naive);

    const float output = delay[0] + ring[static_cast<std::size_t>(base)];
    ring[static_cast<std::size_t>(base)] = 0.0f;
    base = base + 1 < correctionRing ? base + 1 : 0;

    for (int i = 0; i + 1 < correctionHalfWidth; ++i)
        delay[static_cast<std::size_t>(i)] = delay[static_cast<std::size_t>(i + 1)];
    delay[static_cast<std::size_t>(correctionHalfWidth - 1)] = naive;

    return output;
}

void YouKnow106Engine::buildCorrectionTables() noexcept
{
    // Integrate a Blackman-windowed sinc to obtain the bandlimited step, then
    // subtract the ideal step to leave the residual. Integrating once more
    // gives the slope residual. Doing this numerically rather than fitting a
    // polynomial means the residual is right by construction; the earlier
    // closed-form attempt was not, and it raised the alias floor instead of
    // lowering it.
    constexpr int length = correctionTableLength;
    constexpr double step = 1.0 / correctionOversample;

    std::array<double, length> impulse {};
    for (int i = 0; i < length; ++i)
    {
        const double t = static_cast<double>(i) * step - correctionHalfWidth;
        const double sinc = std::abs(t) < 1.0e-12
            ? 1.0
            : std::sin(3.14159265358979323846 * t) / (3.14159265358979323846 * t);
        const double phase = static_cast<double>(i) / static_cast<double>(length - 1);
        const double window = 0.42
                            - 0.5 * std::cos(2.0 * 3.14159265358979323846 * phase)
                            + 0.08 * std::cos(4.0 * 3.14159265358979323846 * phase);
        impulse[static_cast<std::size_t>(i)] = sinc * window;
    }

    // Trapezoidal running integral, normalised so the step ends at exactly one.
    std::array<double, length> stepResponse {};
    double accumulator = 0.0;
    stepResponse[0] = 0.0;
    for (int i = 1; i < length; ++i)
    {
        accumulator += 0.5 * step
                     * (impulse[static_cast<std::size_t>(i - 1)]
                        + impulse[static_cast<std::size_t>(i)]);
        stepResponse[static_cast<std::size_t>(i)] = accumulator;
    }
    const double total = stepResponse[length - 1];
    if (std::abs(total) > 1.0e-12)
        for (auto& value : stepResponse)
            value /= total;

    std::array<double, length> rampResponse {};
    accumulator = 0.0;
    rampResponse[0] = 0.0;
    for (int i = 1; i < length; ++i)
    {
        accumulator += 0.5 * step
                     * (stepResponse[static_cast<std::size_t>(i - 1)]
                        + stepResponse[static_cast<std::size_t>(i)]);
        rampResponse[static_cast<std::size_t>(i)] = accumulator;
    }

    for (int i = 0; i < length; ++i)
    {
        const double t = static_cast<double>(i) * step - correctionHalfWidth;
        const double idealStep = t >= 0.0 ? 1.0 : 0.0;
        const double idealRamp = t >= 0.0 ? t : 0.0;
        stepResidual_[static_cast<std::size_t>(i)] =
            static_cast<float>(stepResponse[static_cast<std::size_t>(i)] - idealStep);
        slopeResidual_[static_cast<std::size_t>(i)] =
            static_cast<float>(rampResponse[static_cast<std::size_t>(i)] - idealRamp);
    }
}

// `samplesAgo` is how far back inside the sample just rendered the event sits,
// in [0, 1). Output sample `j` of the correction ring is `j - halfWidth`
// samples away from the sample just rendered, so the residual is read at
// `j - halfWidth + samplesAgo` and the table is offset by the half width.
void YouKnow106Engine::addStep(BandlimitedTrack& track, float height,
                               float samplesAgo) const noexcept
{
    if (!(std::abs(height) > 0.0f))
        return;
    const float offset = std::clamp(samplesAgo, 0.0f, 1.0f);
    for (int j = 0; j < correctionRing; ++j)
    {
        const float position = (static_cast<float>(j) + offset)
                             * static_cast<float>(correctionOversample);
        const int index = std::clamp(static_cast<int>(position + 0.5f), 0,
                                     correctionTableLength - 1);
        const int slot = (track.base + j) % correctionRing;
        track.ring[static_cast<std::size_t>(slot)] +=
            height * stepResidual_[static_cast<std::size_t>(index)];
    }
}

void YouKnow106Engine::addSlope(BandlimitedTrack& track, float slopeStep,
                                float samplesAgo) const noexcept
{
    if (!(std::abs(slopeStep) > 0.0f))
        return;
    const float offset = std::clamp(samplesAgo, 0.0f, 1.0f);
    for (int j = 0; j < correctionRing; ++j)
    {
        const float position = (static_cast<float>(j) + offset)
                             * static_cast<float>(correctionOversample);
        const int index = std::clamp(static_cast<int>(position + 0.5f), 0,
                                     correctionTableLength - 1);
        const int slot = (track.base + j) % correctionRing;
        track.ring[static_cast<std::size_t>(slot)] +=
            slopeStep * slopeResidual_[static_cast<std::size_t>(index)];
    }
}

// ---------------------------------------------------------------------------
// Envelope
// ---------------------------------------------------------------------------

void YouKnow106Engine::Envelope::reset() noexcept
{
    stage = EnvelopeStage::Idle;
    value = 0.0f;
}

void YouKnow106Engine::Envelope::noteOn() noexcept
{
    stage = EnvelopeStage::Attack;
}

void YouKnow106Engine::Envelope::noteOff() noexcept
{
    if (stage != EnvelopeStage::Idle)
        stage = EnvelopeStage::Release;
}

float YouKnow106Engine::Envelope::tick(float attackStep, float decayCoefficient,
                                       float sustain,
                                       float releaseCoefficient) noexcept
{
    // Truncate a level to the generator's 14-bit grid, rounding towards the
    // target so a falling segment reaches it in finite time -- exactly what
    // the integer arithmetic does.
    const auto truncated = [](float level) {
        return std::floor(level / envelopeQuantum) * envelopeQuantum;
    };

    switch (stage)
    {
        case EnvelopeStage::Attack:
            value += attackStep;
            if (value >= 1.0f)
            {
                value = 1.0f;
                stage = EnvelopeStage::Decay;
            }
            break;

        case EnvelopeStage::Decay:
        case EnvelopeStage::Sustain:
            // One state, as in the firmware: above the sustain level the
            // distance decays multiplicatively; at or below it the level
            // snaps to the target, which is also what happens when the
            // slider is pushed *up* mid-note.
            if (value > sustain)
            {
                value = sustain
                      + truncated((value - sustain) * decayCoefficient);
                if (value <= sustain)
                {
                    value = sustain;
                    stage = EnvelopeStage::Sustain;
                }
            }
            else
            {
                value = sustain;
                stage = EnvelopeStage::Sustain;
            }
            break;

        case EnvelopeStage::Release:
            value = truncated(value * releaseCoefficient);
            if (value <= 0.0f)
            {
                value = 0.0f;
                stage = EnvelopeStage::Idle;
            }
            break;

        case EnvelopeStage::Idle:
        default:
            value = 0.0f;
            break;
    }

    return value;
}

// ---------------------------------------------------------------------------
// Oscillator, filter and high-pass state
// ---------------------------------------------------------------------------

void YouKnow106Engine::Dco::reset() noexcept
{
    phase = 0.0;
    pulseState = -1.0f;
    subState = 1.0f;
    saw.reset();
    pulse.reset();
    sub.reset();
}

void YouKnow106Engine::OtaCascade::reset() noexcept
{
    state.fill(0.0f);
    voltage.fill(0.0f);
}

// One trapezoidally integrated step of the four transconductor stages with the
// inverting resonance return closed around them. The unknowns are the four
// stage voltages; the Jacobian is lower bidiagonal apart from a single corner
// term contributed by the feedback, so the Newton step is solved directly
// rather than with a general linear solver.
//
// Stage equation: Vn = s_n + g * H * tanh((V_{n-1} - V_n) / H), with
// H = 2 Vt / attenuation, the differential pair's linear span referred to the
// stage input, and V_0 = input - k * fb(V_4). The return path is itself a
// transconductor behind a 100k/1.5k divider, so fb is a tanh with its own,
// much larger span: fb(V) = Hfb * tanh(V / Hfb). That pair is what limits
// the self-oscillation amplitude in the hardware -- the loop compresses
// while the forward stages are still nearly linear, which is why the
// oscillation is close to sinusoidal and sits near the small-signal pitch.
float YouKnow106Engine::OtaCascade::process(float input, float g,
                                            float feedback) noexcept
{
    constexpr float headroom = otaHeadroomVolts;
    constexpr float inverseHeadroom = 1.0f / headroom;
    constexpr float feedbackHeadroom = vcfFeedbackHeadroomVolts;
    constexpr int maximumIterations = 8;

    const float gLimited = std::clamp(g, 0.0f, 64.0f);
    const float k = std::clamp(feedback, 0.0f, 8.0f);

    std::array<float, 4> selfDerivative {};
    std::array<float, 4> previousDerivative {};
    std::array<float, 4> residual {};

    for (int iteration = 0; iteration < maximumIterations; ++iteration)
    {
        const float feedbackTanh = std::tanh(voltage[3] / feedbackHeadroom);
        const float feedbackSech2 = 1.0f - feedbackTanh * feedbackTanh;
        float previous = input - k * feedbackHeadroom * feedbackTanh;
        for (int n = 0; n < 4; ++n)
        {
            const float x = (previous - voltage[n]) * inverseHeadroom;
            const float t = std::tanh(x);
            const float sech2 = 1.0f - t * t;
            residual[static_cast<std::size_t>(n)] =
                voltage[static_cast<std::size_t>(n)]
                - state[static_cast<std::size_t>(n)] - gLimited * headroom * t;
            selfDerivative[static_cast<std::size_t>(n)] = 1.0f + gLimited * sech2;
            previousDerivative[static_cast<std::size_t>(n)] = -gLimited * sech2;
            previous = voltage[static_cast<std::size_t>(n)];
        }

        // Solve the bidiagonal system twice: once for the residual and once for
        // the corner column, then combine. This is the rank-one correction that
        // closes the resonance loop without forming a 4x4 matrix.
        const auto solveBidiagonal = [&](const std::array<float, 4>& rhs) {
            std::array<float, 4> x {};
            x[0] = rhs[0] / selfDerivative[0];
            for (int n = 1; n < 4; ++n)
                x[static_cast<std::size_t>(n)] =
                    (rhs[static_cast<std::size_t>(n)]
                     - previousDerivative[static_cast<std::size_t>(n)]
                       * x[static_cast<std::size_t>(n - 1)])
                    / selfDerivative[static_cast<std::size_t>(n)];
            return x;
        };

        const auto a = solveBidiagonal(residual);
        std::array<float, 4> corner {};
        // The corner column is the loop's derivative with respect to the
        // fourth pole, which carries the return pair's own compression.
        corner[0] = previousDerivative[0] * (-k * feedbackSech2);
        const auto b = solveBidiagonal(corner);

        float denominator = 1.0f + b[3];
        if (std::abs(denominator) < 1.0e-9f)
            denominator = denominator < 0.0f ? -1.0e-9f : 1.0e-9f;
        const float scale = a[3] / denominator;

        float largest = 0.0f;
        for (int n = 0; n < 4; ++n)
        {
            const float delta = std::clamp(
                a[static_cast<std::size_t>(n)] - b[static_cast<std::size_t>(n)] * scale,
                -32.0f, 32.0f);
            voltage[static_cast<std::size_t>(n)] -= delta;
            largest = std::max(largest, std::abs(delta));
        }

        if (largest < 1.0e-7f)
            break;
    }

    for (int n = 0; n < 4; ++n)
    {
        // Trapezoidal carry: s_next = 2 V - s.
        state[static_cast<std::size_t>(n)] =
            2.0f * voltage[static_cast<std::size_t>(n)]
            - state[static_cast<std::size_t>(n)];
        if (!std::isfinite(state[static_cast<std::size_t>(n)]))
        {
            state[static_cast<std::size_t>(n)] = 0.0f;
            voltage[static_cast<std::size_t>(n)] = 0.0f;
        }
    }

    return voltage[3];
}

void YouKnow106Engine::HighPass::reset() noexcept
{
    state = 0.0f;
}

float YouKnow106Engine::HighPass::process(float input, float g,
                                          float shelfGain,
                                          float highGain) noexcept
{
    // Topology-preserving single pole. The cutting legs pass the high band at
    // unity and discard the low band; the boost leg is a real shelf, lifting
    // the low band strongly and the high band slightly, as the measured
    // network does.
    const float v = (input - state) * g / (1.0f + g);
    const float low = v + state;
    state = low + v;
    if (!std::isfinite(state))
        state = 0.0f;
    const float high = input - low;
    return highGain * high + shelfGain * low;
}

void YouKnow106Engine::HalfbandDecimator::reset() noexcept
{
    left.fill(0.0f);
    right.fill(0.0f);
    writeIndex = 0;
}

// ---------------------------------------------------------------------------
// Construction and preparation
// ---------------------------------------------------------------------------

YouKnow106Engine::YouKnow106Engine() noexcept
{
    buildHalfbandKernel();
    buildCorrectionTables();
    buildVoiceCards();
    clearHeldNotes();
}

void YouKnow106Engine::buildHalfbandKernel() noexcept
{
    // Blackman-Harris windowed half-band. The window is chosen over a Kaiser
    // because the C++ standard special-function Bessel is not available on
    // every toolchain this project builds with, and its stopband is already
    // well below the noise floor of everything upstream of it.
    constexpr int centre = (halfbandTaps - 1) / 2;
    float sum = 0.0f;
    for (int n = 0; n < halfbandTaps; ++n)
    {
        const float offset = static_cast<float>(n - centre);
        float ideal;
        if (std::abs(offset) < 1.0e-6f)
        {
            ideal = 0.5f;
        }
        else
        {
            const float x = pi * offset * 0.5f;
            ideal = 0.5f * std::sin(x) / x;
        }

        const float t = static_cast<float>(n) / static_cast<float>(halfbandTaps - 1);
        const float window = 0.35875f
                           - 0.48829f * std::cos(twoPi * t)
                           + 0.14128f * std::cos(2.0f * twoPi * t)
                           - 0.01168f * std::cos(3.0f * twoPi * t);
        halfbandKernel_[static_cast<std::size_t>(n)] = ideal * window;
        sum += halfbandKernel_[static_cast<std::size_t>(n)];
    }

    // Normalise to exactly unity gain at DC so decimation cannot shift level.
    if (sum > 1.0e-9f)
        for (auto& tap : halfbandKernel_)
            tap /= sum;
}

void YouKnow106Engine::buildVoiceCards() noexcept
{
    // One draw per real dispersion mechanism. The envelopes themselves carry
    // no draw: they are computed digitally in the shared processor, so every
    // voice's envelope is identical and what disperses is the analogue chain
    // each one drives.
    for (int index = 0; index < maxVoices; ++index)
    {
        auto& card = cards_[static_cast<std::size_t>(index)];
        const std::uint32_t seed = static_cast<std::uint32_t>(index) * 2654435761u + 17u;
        card.rampCurrentError = hashBipolar(seed);
        card.comparatorOffset = hashBipolar(seed + 1u);
        card.cutoffOffsetError = hashBipolar(seed + 2u);
        card.resonanceError = hashBipolar(seed + 3u);
        card.vcaOffset = hashBipolar(seed + 4u);
        card.cutoffScaleError = hashBipolar(seed + 5u);
        card.subLevelError = hashBipolar(seed + 6u);
        card.driftPhase = 0.5f * (hashBipolar(seed + 7u) + 1.0f);
        card.vcaGainError = hashBipolar(seed + 8u);
        card.noiseLevelError = hashBipolar(seed + 9u);
        card.highPassError = hashBipolar(seed + 10u);
        card.driftValue = 0.0f;
        card.driftState = seed | 1u;
    }
}

void YouKnow106Engine::prepare(double sampleRate, int /*maxBlockSize*/,
                               bool oversamplingEnabled)
{
    sampleRate_ = std::clamp(sampleRate, 8000.0, maximumSupportedSampleRate);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    oversamplingRequested_ = oversamplingEnabled;
    oversamplingEnabled_ = oversamplingEnabled;
    updateProcessingRate();
    prepared_ = true;
    reset();
}

void YouKnow106Engine::updateProcessingRate() noexcept
{
    if (!oversamplingEnabled_ || sampleRate_ >= minimumHqProcessingRate)
        oversampling_ = 1;
    else if (sampleRate_ >= minimumHqProcessingRate / 2.0)
        oversampling_ = 2;
    else
        oversampling_ = maximumOversampleFactor;

    oversampledRate_ = sampleRate_ * oversampling_;
    inverseOversampledRate_ = static_cast<float>(1.0 / oversampledRate_);
    const double deepest = totalLatencySamples(maximumOversampleFactor);
    const double running = totalLatencySamples(oversampling_);
    latencyPadSamples_ = std::clamp(
        static_cast<int>(std::floor(deepest - running + 0.5)),
        0, latencyPadRingSize - 1);
    latencyPadLeft_.fill(0.0f);
    latencyPadRight_.fill(0.0f);
    latencyPadWriteIndex_ = 0;
    oversamplingQuietSamples_ =
        std::max(1, static_cast<int>(sampleRate_ * outputPathQuietSeconds));
    // Every countdown here is measured in internal samples, so one left over
    // from the previous rate would mean something else entirely at the new one:
    // a scan pass still 806 samples away at 192 kHz is 4.2 ms, and 16.8 ms once
    // the rate drops to 48 kHz. They start again rather than being carried.
    scanCountdown_ = 0;
    lfoScanCountdown_ = 0;
    driftControlCountdown_ = 0;
    // The per-voice scan phases are sample counts too, so they are re-derived
    // for the new rate.
    const int scanPeriodSamples =
        std::max(1, static_cast<int>(oversampledRate_ / controlScanHz));
    for (int index = 0; index < maxVoices; ++index)
        voices_[static_cast<std::size_t>(index)].scanOffset =
            (index % hardwareVoices) * scanPeriodSamples / hardwareVoices;
    chorus_.prepare(oversampledRate_);
    dcCoefficient_ = static_cast<float>(
        std::exp(-2.0 * 3.14159265358979323846 * 12.0 / oversampledRate_));
}

bool YouKnow106Engine::setOversamplingEnabled(bool enabled) noexcept
{
    oversamplingRequested_ = enabled;
    return applyPendingOversamplingIfIdle();
}

bool YouKnow106Engine::applyPendingOversamplingIfIdle() noexcept
{
    if (oversamplingRequested_ == oversamplingEnabled_)
        return true;
    // The last voice retiring is not the same as the instrument being quiet.
    // The delay lines still hold up to their longest setting, the decimators
    // their own group delay, and the output coupling decays with a time
    // constant of its own -- and changing the rate empties all of them. So the
    // change waits until what is left in them has left.
    if (anyVoiceActive_ || oversamplingIdleSamples_ < oversamplingQuietSamples_)
        return false;

    oversamplingEnabled_ = oversamplingRequested_;
    updateProcessingRate();
    clearOutputPath();
    return true;
}

// Everything downstream of the voices: the delay lines, the decimation stages,
// the output coupling and the latency pad. Emptied together, because emptying
// one and not the others leaves a discontinuity where they meet.
void YouKnow106Engine::clearOutputPath() noexcept
{
    firstDecimator_.reset();
    secondDecimator_.reset();
    chorus_.reset();
    dcInput_ = 0.0f;
    dcOutput_ = 0.0f;
    latencyPadLeft_.fill(0.0f);
    latencyPadRight_.fill(0.0f);
    latencyPadWriteIndex_ = 0;
}

double YouKnow106Engine::totalLatencySamples(int factor) noexcept
{
    const int limited = std::max(1, factor);
    // The oscillator's residual tracks delay by their own half width, and that
    // delay is in internal samples -- so it shrinks, in output samples, as the
    // factor grows, while the decimators' delay grows. Both have to be counted,
    // or the two configurations do not line up.
    double latency = static_cast<double>(correctionHalfWidth)
                   / static_cast<double>(limited);
    constexpr double half = (halfbandTaps - 1) / 2.0;
    for (int step = limited; step > 1; step /= 2)
        latency += half / static_cast<double>(step);
    return latency;
}

int YouKnow106Engine::getProcessingLatencySamples() const noexcept
{
    // Always the deepest configuration's figure, whatever is running. The
    // quality setting can change while the host is playing, and a plug-in that
    // renegotiated its latency mid-transport would make the host re-align
    // everything around it; padding the shallower settings costs half a
    // millisecond and keeps the number the host was told true.
    return static_cast<int>(
        std::floor(totalLatencySamples(maximumOversampleFactor) + 0.5));
}

void YouKnow106Engine::applyLatencyPad(float& left, float& right) noexcept
{
    if (latencyPadSamples_ <= 0)
        return;

    latencyPadLeft_[static_cast<std::size_t>(latencyPadWriteIndex_)] = left;
    latencyPadRight_[static_cast<std::size_t>(latencyPadWriteIndex_)] = right;
    const int readIndex =
        (latencyPadWriteIndex_ - latencyPadSamples_ + latencyPadRingSize)
        % latencyPadRingSize;
    left = latencyPadLeft_[static_cast<std::size_t>(readIndex)];
    right = latencyPadRight_[static_cast<std::size_t>(readIndex)];
    latencyPadWriteIndex_ = (latencyPadWriteIndex_ + 1) % latencyPadRingSize;
}

void YouKnow106Engine::reset()
{
    const int scanPeriodSamples =
        std::max(1, static_cast<int>(oversampledRate_ / controlScanHz));
    for (auto& voice : voices_)
    {
        voice = Voice {};
        voice.dco.reset();
        voice.filter.reset();
        voice.highPass.reset();
        voice.envelope.reset();
    }
    for (int index = 0; index < maxVoices; ++index)
    {
        auto& voice = voices_[static_cast<std::size_t>(index)];
        voice.cardIndex = index;
        // The multiplexer reaches the six voices in turn across the pass, so
        // each slot's rewrite lands a sixth of the pass after its
        // neighbour's. Slots beyond the hardware six share the six phases.
        voice.scanOffset = (index % hardwareVoices) * scanPeriodSamples
                         / hardwareVoices;
    }

    clearOutputPath();
    clearHeldNotes();

    lfoAccumulator_ = 0.0f;
    lfoRising_ = true;
    lfoPolarity_ = 1.0f;
    lfoValue_ = 0.0f;
    lfoDelayLevel_ = 0.0f;
    resonanceCvTarget_ = 0.0f;
    resonanceCv_ = 0.0f;
    // The hold has to go with the level: a note arriving at the very first
    // sample of a new run gives the scan no idle pass in which to clear it, so
    // a hold left over from the previous run would be skipped.
    lfoDelayHoldoff_ = 0.0f;
    lfoScanCountdown_ = 0;
    scanCountdown_ = 0;
    anyKeyDown_ = false;
    noiseState_ = 0x6d2b79f5u;
    // Both the live value and the target, or a run that stopped with the bender
    // pushed over would start the next one there: hosts are not obliged to
    // resend a neutral controller when the transport restarts, and nothing
    // would bring it back until the player touched the wheel.
    pitchBendTarget_ = 0.0f;
    modWheelTarget_ = 0.0f;
    pitchBend_ = 0.0f;
    modWheel_ = 0.0f;
    sustainPedalDown_ = false;
    generation_ = 0;
    activeVoiceCount_ = 0;
    anyVoiceActive_ = false;
    displayEnvelope_ = 0.0f;
    displayLfo_ = 0.0f;
    displayVoiceMask_ = 0;
    driftControlCountdown_ = 0;
    panelGlidePrimed_ = false;
    // A reset leaves nothing in the output path, so a quality change asked for
    // before the first block does not have to wait for one that never comes.
    oversamplingIdleSamples_ = oversamplingQuietSamples_;
}

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------

EngineParameters YouKnow106Engine::sanitise(const EngineParameters& parameters) noexcept
{
    EngineParameters result = parameters;

    const auto fix01 = [](float& value, float fallback) {
        value = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
    };

    fix01(result.lfoRate, 0.42f);
    fix01(result.lfoDelay, 0.0f);
    fix01(result.dcoLfoDepth, 0.0f);
    fix01(result.pwmDepth, 0.30f);
    fix01(result.subLevel, 0.0f);
    fix01(result.noiseLevel, 0.0f);
    fix01(result.cutoff, 0.62f);
    fix01(result.resonance, 0.10f);
    fix01(result.envDepth, 0.35f);
    fix01(result.vcfLfoDepth, 0.0f);
    fix01(result.keyFollow, 0.50f);
    fix01(result.vcaLevel, 0.80f);
    fix01(result.attack, 0.04f);
    fix01(result.decay, 0.45f);
    fix01(result.sustain, 0.70f);
    fix01(result.release, 0.30f);
    fix01(result.portamento, 0.0f);
    fix01(result.benderDcoDepth, 0.30f);
    fix01(result.benderVcfDepth, 0.0f);
    fix01(result.benderLfoDepth, 0.0f);
    fix01(result.volume, 0.80f);
    fix01(result.velocityDepth, 0.0f);
    fix01(result.calibration, 0.35f);
    fix01(result.chorusNoise, 1.0f);

    result.masterTuneCents = std::isfinite(result.masterTuneCents)
                           ? std::clamp(result.masterTuneCents, -50.0f, 50.0f)
                           : 0.0f;
    result.keyTranspose = std::clamp(result.keyTranspose, -12, 12);
    result.polyphony = std::clamp(result.polyphony, 1, maxVoices);
    return result;
}

void YouKnow106Engine::setParameters(const EngineParameters& parameters)
{
    targetParameters_ = sanitise(parameters);
    // Switch positions land immediately. The three continuous controls that
    // are applied straight to the samples glide towards their new positions in
    // the render loop instead, so nothing here steps them at a block boundary.
    activeParameters_ = targetParameters_;
}

// Constant rate in pitch: a wider leap takes proportionally longer, rather than
// every glide finishing in the same time. Zero means the control is off, and a
// note steps straight to its pitch.
float YouKnow106Engine::glideStepPerScan(float portamento) noexcept
{
    const float secondsPerOctave = portamentoSeconds(portamento);
    if (!(secondsPerOctave > 0.0f))
        return 0.0f;
    return static_cast<float>(
        12.0 / (static_cast<double>(secondsPerOctave) * controlScanHz));
}

int YouKnow106Engine::voiceLimit() const noexcept
{
    return std::clamp(activeParameters_.polyphony, 1, maxVoices);
}

// ---------------------------------------------------------------------------
// Note handling
// ---------------------------------------------------------------------------

void YouKnow106Engine::rememberHeldNote(int midiNote, float velocity) noexcept
{
    if (midiNote < 0 || midiNote > 127)
        return;
    const auto index = static_cast<std::size_t>(midiNote);
    if (heldNoteCounts_[index] == 0)
    {
        heldNoteOrder_[index] = ++heldNoteCount_;
        heldNoteVelocities_[index] = velocity;
    }
    if (heldNoteCounts_[index] < std::numeric_limits<std::uint16_t>::max())
        ++heldNoteCounts_[index];
}

void YouKnow106Engine::forgetHeldNote(int midiNote) noexcept
{
    if (midiNote < 0 || midiNote > 127)
        return;
    const auto index = static_cast<std::size_t>(midiNote);
    if (heldNoteCounts_[index] > 0)
        --heldNoteCounts_[index];
    if (heldNoteCounts_[index] == 0)
        heldNoteOrder_[index] = 0;
}

void YouKnow106Engine::clearHeldNotes() noexcept
{
    heldNoteOrder_.fill(0);
    heldNoteVelocities_.fill(0.0f);
    heldNoteCounts_.fill(0);
    heldNoteCount_ = 0;
}

int YouKnow106Engine::newestHeldNote() const noexcept
{
    int best = -1;
    int newest = 0;
    for (int note = 0; note < 128; ++note)
        if (heldNoteCounts_[static_cast<std::size_t>(note)] > 0
            && heldNoteOrder_[static_cast<std::size_t>(note)] > newest)
        {
            newest = heldNoteOrder_[static_cast<std::size_t>(note)];
            best = note;
        }
    return best;
}

void YouKnow106Engine::releaseVoiceKey(Voice& voice) noexcept
{
    voice.keyDown = false;
    voice.releaseStamp = ++generation_;
    if (sustainPedalDown_)
    {
        voice.sustained = true;
    }
    else
    {
        voice.releasing = true;
        voice.envelope.noteOff();
    }
}

void YouKnow106Engine::dropFromUnison(Voice& voice) noexcept
{
    voice.unisonMember = false;
    if (voice.keyDown)
        releaseVoiceKey(voice);
}

// Unison is monophonic, so releasing the key that is sounding has to hand the
// stack back to whichever key is still down rather than silencing everything.
void YouKnow106Engine::retargetUnison(int midiNote) noexcept
{
    // Every slot in the stack, not just the ones inside the current voice
    // count: lowering that count leaves earlier voices playing, and they have
    // to be dealt with or they stay keyed to a note nobody is holding. Slots
    // that have fallen outside the count leave the stack instead of following
    // it, so the count stays the number of voices a key can sound.
    const int limit = voiceLimit();
    const float velocity = heldNoteVelocities_[static_cast<std::size_t>(midiNote)];
    for (int slot = 0; slot < maxVoices; ++slot)
    {
        auto& voice = voices_[static_cast<std::size_t>(slot)];
        if (!voice.active || !voice.unisonMember)
            continue;
        if (slot >= limit)
        {
            dropFromUnison(voice);
            continue;
        }
        voice.rootMidi = midiNote;
        voice.keyDown = true;
        voice.sustained = false;
        voice.releasing = false;
        voice.velocity = velocity;
        voice.targetMidi = static_cast<float>(midiNote + activeParameters_.keyTranspose);
        voice.lastMidi = voice.targetMidi;
        // Returning to a key that was never let go is a legato move: the
        // envelope keeps running rather than starting again.
        if (voice.envelope.stage == EnvelopeStage::Release)
            voice.envelope.stage = EnvelopeStage::Sustain;
        if (voice.glideSemitonesPerScan <= 0.0f)
            voice.currentMidi = voice.targetMidi;
    }
}

bool YouKnow106Engine::anyVoiceSounding() const noexcept
{
    for (const auto& voice : voices_)
        if (voice.active)
            return true;
    return false;
}

// The delay is a hold followed by a fade. Both start again for a new phrase.
void YouKnow106Engine::rearmLfoDelay() noexcept
{
    lfoDelayHoldoff_ = 0.0f;
    lfoDelayLevel_ =
        lfoDelaySeconds(activeParameters_.lfoDelay) > 1.0e-4f ? 0.0f : 1.0f;
}

int YouKnow106Engine::findVoiceForNote(int midiNote) const noexcept
{
    const int limit = voiceLimit();
    for (int slot = 0; slot < limit; ++slot)
    {
        const auto& voice = voices_[static_cast<std::size_t>(slot)];
        if (voice.active && voice.keyDown && voice.rootMidi == midiNote)
            return slot;
    }
    return -1;
}

// The key assigner never steals a sounding key. With every voice held, a
// further note is simply dropped -- which is the assigner firmware's own
// policy, and the reason dense chords lose notes on it. A voice whose key has
// been let go is available again even while its release rings.
int YouKnow106Engine::allocateVoice(int midiNote) noexcept
{
    const int limit = voiceLimit();
    const auto available = [this](int slot) {
        const auto& voice = voices_[static_cast<std::size_t>(slot)];
        return !voice.active || (!voice.keyDown && !voice.sustained);
    };

    if (activeParameters_.keyMode == KeyMode::Poly2)
    {
        // A plain linear scan from the first voice up, so low slots are
        // reused immediately and their release tails chopped; only the most
        // recent notes keep a full release. That truncation is the point of
        // the mode, and it is also what makes its per-voice glide musical.
        for (int slot = 0; slot < limit; ++slot)
            if (available(slot))
                return slot;
        return -1;
    }

    // Note memory first: a free voice whose *last* note -- even one whose
    // release has long finished -- matches the incoming pitch is taken, so a
    // repeated note lands on the voice already sitting at its pitch and
    // filter state. Otherwise the free voice that has been released longest
    // is taken, which is what preserves the freshest tails when keys come up
    // out of order.
    const float wanted = static_cast<float>(
        midiNote + activeParameters_.keyTranspose);
    for (int slot = 0; slot < limit; ++slot)
    {
        const auto& voice = voices_[static_cast<std::size_t>(slot)];
        if (available(slot) && voice.hasEverPlayed && voice.lastMidi == wanted)
            return slot;
    }

    int best = -1;
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
    for (int slot = 0; slot < limit; ++slot)
    {
        const auto& voice = voices_[static_cast<std::size_t>(slot)];
        if (!available(slot))
            continue;
        // A slot that has never played counts as released at the dawn of
        // time, so an empty instrument fills from the first voice up.
        const std::uint64_t stamp = voice.hasEverPlayed ? voice.releaseStamp : 0;
        if (stamp < oldest || best < 0)
        {
            oldest = stamp;
            best = slot;
        }
    }
    return best;
}

void YouKnow106Engine::initialiseVoice(Voice& voice, int slot, int midiNote,
                                       float velocity, bool retrigger) noexcept
{
    const auto& parameters = activeParameters_;
    const bool wasSounding = voice.active;

    voice.cardIndex = slot;
    voice.active = true;
    voice.keyDown = true;
    voice.sustained = false;
    voice.releasing = false;
    voice.rootMidi = midiNote;
    voice.velocity = velocity;
    voice.generation = ++generation_;
    voice.controlCountdown = 0;
    voice.energy = 0.0f;

    const float target = static_cast<float>(midiNote + parameters.keyTranspose);
    voice.targetMidi = target;

    voice.glideSemitonesPerScan = glideStepPerScan(parameters.portamento);
    if (voice.glideSemitonesPerScan > 0.0f)
    {
        // The glide integrator is per voice and survives retirement, so a
        // reassigned voice slides from whatever *it* last played -- notes
        // several assignments back, exactly the instrument's own poly-glide
        // behaviour. A slot that has never sounded this run has no history
        // and starts where it is asked to.
        if (!wasSounding)
            voice.currentMidi = voice.hasEverPlayed ? voice.currentMidi : target;
    }
    else
    {
        voice.currentMidi = target;
    }
    voice.lastMidi = target;
    voice.hasEverPlayed = true;

    if (!wasSounding || retrigger)
    {
        // The oscillator restarts; the envelope does not. The generator's
        // accumulator is never cleared by a key press, so a retriggered
        // voice attacks from the level its release had reached -- the
        // instrument's own soft legato restrike.
        voice.dco.reset();
        if (!wasSounding)
        {
            voice.filter.reset();
            voice.highPass.reset();
            voice.vca = 0.0f;
        }
        voice.noiseState = hash32(static_cast<std::uint32_t>(slot) * 2246822519u
                                  + static_cast<std::uint32_t>(midiNote) + 1u) | 1u;
    }
    voice.envelope.noteOn();
}

void YouKnow106Engine::silenceVoice(Voice& voice) noexcept
{
    voice.active = false;
    voice.keyDown = false;
    voice.sustained = false;
    voice.releasing = false;
    voice.unisonMember = false;
    voice.rootMidi = -1;
    voice.vca = 0.0f;
    voice.vcaControlTarget = 0.0f;
    voice.vcaControl = 0.0f;
    voice.energy = 0.0f;
    voice.envelope.reset();
    voice.filter.reset();
    voice.highPass.reset();
    voice.dco.reset();
    // Deliberately kept: lastMidi, currentMidi and releaseStamp. A retired
    // voice still remembers what it played -- that memory is what the
    // assigner's note affinity and the per-voice glide start from.
}

void YouKnow106Engine::noteOn(int midiNote, float velocity)
{
    if (midiNote < 0 || midiNote > 127)
        return;
    noteOnInternal(midiNote, std::clamp(velocity, 0.0f, 1.0f));
}

void YouKnow106Engine::noteOnInternal(int midiNote, float velocity) noexcept
{
    // A phrase starts when no key was down -- release tails still ringing do
    // not count, because the firmware's retrigger latch watches the key-gate
    // mask, not envelope activity. A note played over ringing releases
    // therefore restarts the delay, which the previous
    // wait-for-silence condition got wrong.
    if (!anyKeyDown_)
        rearmLfoDelay();

    rememberHeldNote(midiNote, velocity);
    anyKeyDown_ = true;

    if (activeParameters_.keyMode == KeyMode::Unison)
    {
        // Every voice takes the same note, and every note timer divides the
        // same reference by the same integer, so there is no pitch spread at
        // all: what separates the six is the analogue block after them. Adding
        // a detune here would be inventing a behaviour the instrument does not
        // have. Each slot's glide starts from that slot's own pitch history,
        // exactly as its per-voice integrator does; on the hardware all six
        // always share that history, so a slot woken into a wider stack than
        // it left -- a voice count the hardware cannot change -- adopts the
        // stack's position rather than inventing one of its own.
        const int limit = voiceLimit();
        float stackMidi = 0.0f;
        bool haveStackMidi = false;
        for (int slot = 0; slot < limit && !haveStackMidi; ++slot)
        {
            const auto& voice = voices_[static_cast<std::size_t>(slot)];
            if (voice.active && voice.unisonMember)
            {
                stackMidi = voice.currentMidi;
                haveStackMidi = true;
            }
        }
        for (int slot = 0; slot < limit; ++slot)
        {
            auto& voice = voices_[static_cast<std::size_t>(slot)];
            const bool retrigger = !voice.active || voice.rootMidi != midiNote;
            const bool fresh = !voice.hasEverPlayed;
            initialiseVoice(voice, slot, midiNote, velocity, retrigger);
            if (fresh && haveStackMidi && voice.glideSemitonesPerScan > 0.0f)
                voice.currentMidi = stackMidi;
            voice.unisonMember = true;
        }
        // A voice count lowered while a wider stack was sounding leaves slots
        // above the new count still keyed to the old note. They leave the stack
        // here rather than holding that note against the new one, which would
        // turn Unison into a chord.
        for (int slot = limit; slot < maxVoices; ++slot)
        {
            auto& voice = voices_[static_cast<std::size_t>(slot)];
            if (voice.active && voice.unisonMember)
                dropFromUnison(voice);
        }
        updateActiveVoiceCount();
        return;
    }

    const int existing = findVoiceForNote(midiNote);
    const int slot = existing >= 0 ? existing : allocateVoice(midiNote);
    if (slot < 0)
        return; // Every key is held: the note is dropped, as on the hardware.

    auto& voice = voices_[static_cast<std::size_t>(slot)];
    initialiseVoice(voice, slot, midiNote, velocity, true);
    voice.unisonMember = false;
    updateActiveVoiceCount();
}

void YouKnow106Engine::noteOff(int midiNote)
{
    if (midiNote < 0 || midiNote > 127)
        return;
    noteOffInternal(midiNote);
}

void YouKnow106Engine::noteOffInternal(int midiNote) noexcept
{
    forgetHeldNote(midiNote);
    if (heldNoteCounts_[static_cast<std::size_t>(midiNote)] > 0)
        return;

    const int remaining = newestHeldNote();
    anyKeyDown_ = remaining >= 0;

    if (activeParameters_.keyMode == KeyMode::Unison && remaining >= 0)
    {
        // Only the key that is actually sounding hands the stack on; releasing
        // an older one that the stack has already left changes nothing. A
        // polyphonic tail on the same note is not the stack, so it does not
        // count as sounding it either.
        bool sounding = false;
        for (const auto& voice : voices_)
            sounding = sounding
                || (voice.active && voice.unisonMember && voice.rootMidi == midiNote);
        if (sounding)
        {
            retargetUnison(remaining);
            return;
        }
    }

    for (auto& voice : voices_)
    {
        if (!voice.active || voice.rootMidi != midiNote || !voice.keyDown)
            continue;
        releaseVoiceKey(voice);
    }
}

void YouKnow106Engine::releaseAllNotes()
{
    clearHeldNotes();
    anyKeyDown_ = false;
    for (auto& voice : voices_)
    {
        if (!voice.active || !voice.keyDown)
            continue;
        releaseVoiceKey(voice);
    }
}

void YouKnow106Engine::allNotesOff()
{
    clearHeldNotes();
    anyKeyDown_ = false;
    sustainPedalDown_ = false;
    for (auto& voice : voices_)
        silenceVoice(voice);
    // A hard stop has to be silent now, not once the delay lines have run out.
    // Cutting the voices alone would leave the chorus playing back the last
    // few milliseconds of a held chord after the panic.
    clearOutputPath();
    oversamplingIdleSamples_ = oversamplingQuietSamples_;
    updateActiveVoiceCount();
}

void YouKnow106Engine::setPitchBend(float normalisedBipolar) noexcept
{
    pitchBendTarget_ = std::clamp(sanitised(normalisedBipolar, 0.0f), -1.0f, 1.0f);
}

void YouKnow106Engine::setModWheel(float amount) noexcept
{
    modWheelTarget_ = clamp01(sanitised(amount, 0.0f));
}

void YouKnow106Engine::setSustainPedal(bool down) noexcept
{
    if (sustainPedalDown_ == down)
        return;
    sustainPedalDown_ = down;
    if (down)
        return;

    for (auto& voice : voices_)
        if (voice.active && voice.sustained && !voice.keyDown)
        {
            voice.sustained = false;
            voice.releasing = true;
            voice.envelope.noteOff();
        }
}

void YouKnow106Engine::updateActiveVoiceCount() noexcept
{
    int count = 0;
    int mask = 0;
    for (int slot = 0; slot < maxVoices; ++slot)
        if (voices_[static_cast<std::size_t>(slot)].active)
        {
            ++count;
            if (slot < 16)
                mask |= 1 << slot;
        }
    activeVoiceCount_ = count;
    anyVoiceActive_ = count > 0;
    displayVoiceMask_ = mask;
}

// ---------------------------------------------------------------------------
// Modulation
// ---------------------------------------------------------------------------

void YouKnow106Engine::advanceLfo(const EngineParameters& parameters) noexcept
{
    // The modulator is firmware: it advances once per converter scan and holds
    // its value in between. That staircase is audible as a faint roughness on
    // deep, slow vibrato, and smoothing it away would be modelling a different
    // instrument.
    if (--lfoScanCountdown_ > 0)
        return;
    lfoScanCountdown_ = std::max(1, static_cast<int>(oversampledRate_ / controlScanHz));

    // A clamped accumulator, not a phase: the rate coefficient is added until
    // the span clamps, the direction flips there, and a polarity flip at the
    // bottom folds the two sweeps into a bipolar triangle. The clamp discards
    // whatever the last step overshot by, so fast settings quantise onto
    // whole passes per sweep -- a measured behaviour of the instrument, not
    // an artefact.
    const float sliderByte =
        std::floor(clamp01(parameters.lfoRate) * 127.0f + 0.5f) / 127.0f;
    const float coefficient = lfoCoefficient(sliderByte);
    if (lfoRising_)
    {
        lfoAccumulator_ += coefficient;
        if (lfoAccumulator_ >= 1.0f)
        {
            lfoAccumulator_ = 1.0f;
            lfoRising_ = false;
        }
    }
    else
    {
        lfoAccumulator_ -= coefficient;
        if (lfoAccumulator_ <= 0.0f)
        {
            lfoAccumulator_ = 0.0f;
            lfoRising_ = true;
            lfoPolarity_ = -lfoPolarity_;
        }
    }
    lfoValue_ = lfoPolarity_ * lfoAccumulator_;

    // Delay: a silent hold that advances at the attack table's own rate, then
    // the stepped fade. The pair is re-armed the moment a note starts with no
    // key down -- release tails still ringing keep their vibrato, because the
    // firmware's retrigger latch watches the keys, not the envelopes.
    const float delayByte =
        std::floor(clamp01(parameters.lfoDelay) * 127.0f + 0.5f) / 127.0f;
    const float total = lfoDelaySeconds(delayByte);
    const float holdoff = envelopeAttackSeconds(delayByte);
    const float fade = lfoFadeSeconds(delayByte);
    const float scanPeriod = static_cast<float>(1.0 / controlScanHz);

    if (total <= 1.0e-4f)
    {
        lfoDelayLevel_ = 1.0f;
    }
    else
    {
        if (lfoDelayHoldoff_ < holdoff)
            lfoDelayHoldoff_ += scanPeriod;
        else if (fade > 1.0e-4f)
            lfoDelayLevel_ = std::min(1.0f, lfoDelayLevel_ + scanPeriod / fade);
        else
            lfoDelayLevel_ = 1.0f;
    }

    displayLfo_ = lfoValue_ * lfoDelayLevel_;
}

void YouKnow106Engine::updateVoiceCardDrift(VoiceCard& card) noexcept
{
    // A slow, bounded wander of the analogue control chain. It is deliberately
    // small: the oscillators share one reference, so this instrument does not
    // drift the way six free-running oscillators would.
    card.driftState ^= card.driftState << 13;
    card.driftState ^= card.driftState >> 17;
    card.driftState ^= card.driftState << 5;
    const float excitation =
        static_cast<float>(card.driftState & 0xffffu) * (2.0f / 65535.0f) - 1.0f;
    card.driftValue = card.driftValue * 0.9992f + excitation * 0.004f;
}

void YouKnow106Engine::updateVoiceScan(Voice& voice,
                                       const EngineParameters& parameters,
                                       float lfoGated, float lfoRaw) noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float tolerance = parameters.calibration;

    // Every continuous panel control is digitised to the converter's seven
    // bits before the firmware sees it -- that is what makes it storable in
    // patch memory -- so every law below consumes the byte, not the slider.
    const auto byte7 = [](float value) {
        return std::floor(clamp01(value) * 127.0f + 0.5f) / 127.0f;
    };

    // --- Envelope ---------------------------------------------------------
    // A linear attack and multiplicative falling segments, advanced once per
    // scan. The published minimum of 1.5 ms is shorter than one scan pass, so
    // the shortest attack the instrument can actually produce is one pass
    // long. There is no per-voice rate error here: the generator is the
    // shared processor, so six voices' envelopes are digitally identical and
    // only the analogue chain after them disperses.
    voice.attackStep = 1.0f / attackScanTicks(byte7(parameters.attack));
    voice.decayCoefficient =
        std::exp(-decayLambdaPerTick(byte7(parameters.decay)));
    voice.releaseCoefficient =
        std::exp(-decayLambdaPerTick(byte7(parameters.release)));

    voice.envelope.tick(voice.attackStep, voice.decayCoefficient,
                        byte7(parameters.sustain), voice.releaseCoefficient);
    const float envelope = voice.envelope.value;

    // --- Pitch ------------------------------------------------------------
    // Recomputed from the key rather than cached at note-on, so moving the
    // transpose control takes a held note with it.
    if (voice.rootMidi >= 0)
        voice.targetMidi = static_cast<float>(voice.rootMidi + parameters.keyTranspose);

    // Taken from the control as it stands, not from what it read when the key
    // went down. The glide rate is a resistance in the pitch integrator's path,
    // and turning that control while a note is sliding changes the slide --
    // including turning it off, which lands the note on its pitch at the next
    // scan rather than leaving it crawling.
    voice.glideSemitonesPerScan = glideStepPerScan(parameters.portamento);

    if (voice.glideSemitonesPerScan > 0.0f)
    {
        const float distance = voice.targetMidi - voice.currentMidi;
        const float step = std::min(std::abs(distance), voice.glideSemitonesPerScan);
        voice.currentMidi += distance < 0.0f ? -step : step;
    }
    else
    {
        voice.currentMidi = voice.targetMidi;
    }

    // The bender's own modulation axis and the panel's modulator slider are
    // summed by the firmware, so pushing both reaches deeper than either
    // alone -- up to twice one slider's span, the byte arithmetic's own
    // bound.
    const float lfoPitchDepth = std::min(
        2.0f, byte7(parameters.dcoLfoDepth)
                  + byte7(parameters.benderLfoDepth) * modWheel_);
    const float cents = parameters.masterTuneCents
        + parameters.benderDcoDepth * benderPitchCents * pitchBend_
        + lfoPitchDepth * lfoPitchCents * lfoGated;
    const double midi = static_cast<double>(voice.currentMidi)
                      + static_cast<double>(cents) / 100.0;

    voice.dco.divider = dcoDivider(midiToHz(midi));
    const double frequency = dcoQuantisedFrequency(voice.dco.divider, parameters.range);
    voice.dco.periodSamples = frequency > 0.0 ? oversampledRate_ / frequency : 1.0e6;
    // The compensation voltage the firmware writes for this pitch. The count
    // above reprogrammes the timer instantly; this target reaches the
    // integrator through the hold capacitor's slew, and the ratio of the two
    // is the momentary amplitude error every pitch step leaves behind.
    voice.dcoCvTarget = frequency > 0.0 ? static_cast<float>(frequency) : 1.0f;

    // --- Filter cutoff, summed in converter counts ------------------------
    float counts = vcfPanelCounts(parameters.cutoff);
    const float envelopeSign =
        parameters.envPolarity == EnvPolarity::Normal ? 1.0f : -1.0f;
    counts += envelopeSign * byte7(parameters.envDepth) * vcfEnvelopeCounts
            * envelope;
    counts += byte7(parameters.vcfLfoDepth) * vcfLfoCounts * lfoGated;
    counts += byte7(parameters.benderVcfDepth) * vcfBenderCounts * pitchBend_;
    counts += byte7(parameters.keyFollow) * vcfCountsPerOctave
            * (voice.currentMidi - vcfKeyFollowCentreMidi) / 12.0f;
    // The firmware clamps the sum to its 14-bit accumulator -- so the digital
    // part of the control voltage can never ask for less than the law's base
    // frequency -- and hands the converter the top twelve bits, so it moves
    // in 4-count steps. The analogue trims and drift ride on top of this at
    // the audio grid, below the converter's own resolution, exactly where
    // the hardware's trimmers sit.
    counts = std::clamp(counts, 0.0f, vcfCountsCeiling);
    voice.cutoffCountsTarget =
        vcfDacCountStep * std::floor(counts / vcfDacCountStep);

    // --- Pulse width ------------------------------------------------------
    // The threshold is one of this voice's held control voltages; the raw
    // modulator drives it, because the firmware's onset envelope gates pitch
    // and cutoff but not the pulse width.
    float pwmAmount = byte7(parameters.pwmDepth);
    switch (parameters.pwmSource)
    {
        case PwmSource::Lfo:
            pwmAmount = pwmAmount * 0.5f * (1.0f + lfoRaw);
            break;
        case PwmSource::Manual:
        default:
            break;
    }
    voice.pwmVoltsTarget = pwmControlVolts(clamp01(pwmAmount));

    // --- Sub and noise levels ---------------------------------------------
    // Two more of the voice's held control voltages: both are patch bytes
    // delivered through the scan, not pots in the audio path.
    voice.subCvTarget = byte7(parameters.subLevel);
    voice.noiseCvTarget = byte7(parameters.noiseLevel);

    // --- Amplifier control ------------------------------------------------
    const float velocityGain = 1.0f
        - parameters.velocityDepth * (1.0f - voice.velocity);
    const float control = parameters.vcaMode == VcaMode::Envelope
                        ? envelope
                        : (voice.keyDown || voice.sustained ? 1.0f : 0.0f);
    voice.vcaControlTarget = clamp01(control * byte7(parameters.vcaLevel)
                                     * velocityGain
                                     + card.vcaOffset * 0.004f * tolerance);
}

void YouKnow106Engine::updateVoiceAudio(Voice& voice,
                                        const EngineParameters& parameters) noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float tolerance = parameters.calibration;

    // The regeneration control voltage is shared -- one converter output for
    // all six loops -- but each voice's loop amplifier has its own gain
    // spread.
    const float resonancePanel = clamp01(resonanceCv_
        + card.resonanceError * 0.02f * tolerance);
    voice.feedback = vcfFeedback(resonancePanel);
    voice.inputCompensation = vcfResonanceCompensation(voice.feedback);

    // The analogue side of the cutoff chain: the two per-voice trimmers --
    // one scales the control voltage, one offsets it -- imperfectly set, and
    // the slow thermal wander, all riding below the converter's own
    // resolution on the slewed digital value. The scale spread is the
    // hardware-fitted five-per-cent figure for this module; the offset is a
    // tenth of an octave of residual miscalibration at full Calibration.
    const float analogCounts = voice.cutoffCounts
        * (1.0f + card.cutoffScaleError * 0.05f * tolerance)
        + card.cutoffOffsetError * 0.07f * vcfCountsPerOctave * tolerance
        + card.driftValue * 40.0f * tolerance;
    const float cutoffHz = vcfCutoffHz(analogCounts)
                         * vcfResonanceFrequencyTrim(voice.feedback);
    const float limited =
        std::min(cutoffHz, static_cast<float>(oversampledRate_) * 0.45f);
    voice.filterG = std::tan(pi * limited * inverseOversampledRate_);

    // The comparator sees the slewed threshold against the momentarily
    // mis-scaled ramp, so a pitch step nudges the duty for a millisecond or
    // two -- the same transient the amplitude compensation leaves on the
    // ramp itself.
    const float amplitudeScale = std::clamp(
        voice.dcoCv / std::max(voice.dcoCvTarget, 1.0e-3f), 0.25f, 4.0f);
    const float threshold = voice.pwmVolts
                          + card.comparatorOffset * 0.12f * tolerance;
    voice.pulseDuty = pwmDutyCycle(threshold / amplitudeScale);

    // Each voice's high-pass leg is its own pair of parts, so its corner
    // disperses a little.
    const float corner = highPassCornerHz(parameters.highPass)
                       * (1.0f + card.highPassError * 0.03f * tolerance);
    voice.highPassG =
        std::tan(pi * std::min(corner, static_cast<float>(oversampledRate_) * 0.45f)
                 * inverseOversampledRate_);
    voice.highPassShelf = highPassShelfGain(parameters.highPass);
    voice.highPassHigh = highPassHighGain(parameters.highPass);

    voice.vca = vcaGain(voice.vcaControl)
              * (1.0f + card.vcaGainError * 0.03f * tolerance);
}

// ---------------------------------------------------------------------------
// Voice rendering
// ---------------------------------------------------------------------------

float YouKnow106Engine::renderVoice(Voice& voice, const EngineParameters& parameters,
                                    float noiseSample) noexcept
{
    auto& dco = voice.dco;
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];

    const double increment = dco.periodSamples > 1.0e-9
                           ? 1.0 / dco.periodSamples : 0.0;
    const double reset = static_cast<double>(
        resetFraction(dco.periodSamples * inverseOversampledRate_));
    const double rise = std::max(1.0 - reset, 1.0e-4);

    const double previousPhase = dco.phase;
    const double unwrapped = dco.phase + increment;
    const bool wrapped = unwrapped >= 1.0;
    dco.phase = wrapped ? unwrapped - std::floor(unwrapped) : unwrapped;
    const double phase = dco.phase;

    // How far back inside this sample an event at unwrapped position `p` sits.
    const auto samplesAgo = [&](double p) {
        return increment > 0.0
            ? static_cast<float>(std::clamp((unwrapped - p) / increment, 0.0, 1.0))
            : 0.0f;
    };
    const auto insideThisSample = [&](double p) {
        return p > previousPhase && p <= unwrapped;
    };

    // A note timer can outrun the sample clock: the divider bottoms out at
    // eight, so the top range reaches half a megahertz, and at the lowest host
    // rate the model accepts that is some sixty cycles inside one sample. Each
    // of them resets the ramp, works the comparator and clocks the divider, and
    // collapsing them into one wrap would hold the pulse low for whole periods
    // and drop the sub an octave. So every crossed cycle is walked. The bound
    // is a runaway guard rather than a limit that is reached: sixty-four covers
    // the fastest note the timer can be programmed for against the slowest rate
    // the engine runs at.
    constexpr int maximumWrapsPerSample = 64;
    const double lastCycle =
        std::min(std::floor(unwrapped), static_cast<double>(maximumWrapsPerSample));

    // --- Ramp -------------------------------------------------------------
    // The compensation voltage keeps the amplitude constant at 12 Vpp -- but
    // it arrives through the hold capacitor while the timer count steps
    // instantly, so every pitch step leaves a momentary amplitude error
    // until the voltage catches up. The ratio of the slewed voltage to the
    // one the current pitch calls for *is* that error.
    const float amplitudeScale = std::clamp(
        voice.dcoCv / std::max(voice.dcoCvTarget, 1.0e-3f), 0.25f, 4.0f);
    const float amplitude = sawMixVolts * amplitudeScale
        * (1.0f + card.rampCurrentError * 0.02f * parameters.calibration);

    const float sawNaive = phase < rise
        ? 2.0f * rampVoltage(static_cast<float>(phase / rise), rampBow) - 1.0f
        : 1.0f - 2.0f * static_cast<float>((phase - rise) / reset);

    // The ramp has no value discontinuity: the reset is a steep segment, not a
    // jump. Both of its corners are slope discontinuities, so both are repaired
    // with the slope residual rather than a step residual. The charge is
    // constant-current, so the rise is straight and both rise corners carry
    // the same slope.
    float slopeAtStart = 2.0f / static_cast<float>(rise);
    float slopeAtEnd = slopeAtStart;
    if (rampBow > 1.0e-4f)
    {
        const float denominator = 1.0f - std::exp(-rampBow);
        slopeAtStart = 2.0f * rampBow / denominator / static_cast<float>(rise);
        slopeAtEnd = 2.0f * rampBow * std::exp(-rampBow) / denominator
                   / static_cast<float>(rise);
    }
    const float fallSlope = -2.0f / static_cast<float>(reset);
    const float incrementF = static_cast<float>(increment);

    for (double base = 0.0; base <= lastCycle; base += 1.0)
    {
        if (insideThisSample(base + rise))
            addSlope(dco.saw, (fallSlope - slopeAtEnd) * incrementF,
                     samplesAgo(base + rise));
        if (insideThisSample(base + 1.0))
            addSlope(dco.saw, (slopeAtStart - fallSlope) * incrementF,
                     samplesAgo(base + 1.0));
    }

    const float sawOut = dco.saw.advance(sawNaive) * amplitude;

    // --- Pulse ------------------------------------------------------------
    // The comparator flips when the ramp crosses the threshold on the way up
    // and again when the reset drags it back down past it.
    const float duty = std::clamp(voice.pulseDuty, 0.05f, 0.95f);
    const double riseEdge =
        static_cast<double>(pulseRisePhase(duty, static_cast<float>(reset)));
    const double fallEdge =
        static_cast<double>(pulseFallPhase(duty, static_cast<float>(reset)));

    // The comparator's two edges per cycle, taken in the order they occur:
    // threshold crossing on the way up, then the reset dragging the ramp back
    // down past it. Both are walked for every cycle inside this sample.
    for (double base = 0.0; base <= lastCycle; base += 1.0)
    {
        if (insideThisSample(base + riseEdge) && dco.pulseState < 0.0f)
        {
            addStep(dco.pulse, 2.0f, samplesAgo(base + riseEdge));
            dco.pulseState = 1.0f;
        }
        if (insideThisSample(base + fallEdge) && dco.pulseState > 0.0f)
        {
            addStep(dco.pulse, -2.0f, samplesAgo(base + fallEdge));
            dco.pulseState = -1.0f;
        }
    }
    // The pulse is amplitude-compensated by the same control voltage as the
    // ramp, so it carries the same momentary scale error on pitch steps.
    const float pulseOut = dco.pulse.advance(dco.pulseState)
                         * pulseMixVolts * amplitudeScale;

    // --- Sub --------------------------------------------------------------
    // A flip-flop halves the note clock, so the sub is an exact square one
    // octave down and takes no part in pulse-width modulation. The terminal
    // pulse that fires the ramp's discharge is also what clocks the divider,
    // so the sub's edges land at the reset's *start*, not at the cycle
    // boundary. Its level is one of the scanned control voltages; its
    // amplitude is a logic square and takes no part in the ramp's
    // compensation.
    for (double base = 0.0; base <= lastCycle; base += 1.0)
    {
        if (!insideThisSample(base + rise))
            continue;
        const float target = -dco.subState;
        addStep(dco.sub, target - dco.subState, samplesAgo(base + rise));
        dco.subState = target;
    }
    const float subGain = subMixVolts * voice.subCv
        * (1.0f + card.subLevelError * 0.03f * parameters.calibration);
    const float subOut = dco.sub.advance(dco.subState) * subGain;

    // --- Summing node ------------------------------------------------------
    float mixed = 0.0f;
    if (parameters.sawEnabled)
        mixed += sawOut;
    if (parameters.pulseEnabled)
        mixed += pulseOut;
    mixed += subOut;
    mixed += noiseSample * noiseMixVolts * voice.noiseCv
           * (1.0f + card.noiseLevelError * 0.03f * parameters.calibration);

    voice.noiseState ^= voice.noiseState << 13;
    voice.noiseState ^= voice.noiseState >> 17;
    voice.noiseState ^= voice.noiseState << 5;
    mixed += (static_cast<float>(voice.noiseState & 0xffffffu) * (2.0f / 16777215.0f)
              - 1.0f) * filterNoiseVolts;

    // --- High-pass, filter, amplifier --------------------------------------
    const float shaped = voice.highPass.process(mixed, voice.highPassG,
                                                voice.highPassShelf,
                                                voice.highPassHigh);
    const float filtered = voice.filter.process(
        shaped * filterInputAttenuation * voice.inputCompensation,
        voice.filterG, voice.feedback);

    const float output = filtered * voice.vca * voltsToSample;

    voice.energy = voice.energy * 0.999f + std::abs(output) * 0.001f;
    return std::isfinite(output) ? output : 0.0f;
}

// ---------------------------------------------------------------------------
// Decimation
// ---------------------------------------------------------------------------

void YouKnow106Engine::downsamplePair(HalfbandDecimator& decimator,
                                      float firstLeft, float firstRight,
                                      float secondLeft, float secondRight,
                                      float& outputLeft, float& outputRight) noexcept
{
    decimator.left[static_cast<std::size_t>(decimator.writeIndex)] = firstLeft;
    decimator.right[static_cast<std::size_t>(decimator.writeIndex)] = firstRight;
    decimator.writeIndex = (decimator.writeIndex + 1) & (halfbandRingSize - 1);
    decimator.left[static_cast<std::size_t>(decimator.writeIndex)] = secondLeft;
    decimator.right[static_cast<std::size_t>(decimator.writeIndex)] = secondRight;
    decimator.writeIndex = (decimator.writeIndex + 1) & (halfbandRingSize - 1);

    float sumLeft = 0.0f;
    float sumRight = 0.0f;
    int index = (decimator.writeIndex - 1) & (halfbandRingSize - 1);
    for (int tap = 0; tap < halfbandTaps; ++tap)
    {
        const float coefficient = halfbandKernel_[static_cast<std::size_t>(tap)];
        if (coefficient != 0.0f)
        {
            sumLeft += coefficient * decimator.left[static_cast<std::size_t>(index)];
            sumRight += coefficient * decimator.right[static_cast<std::size_t>(index)];
        }
        index = (index - 1) & (halfbandRingSize - 1);
    }

    outputLeft = sumLeft;
    outputRight = sumRight;
}

// ---------------------------------------------------------------------------
// Block processing
// ---------------------------------------------------------------------------

void YouKnow106Engine::process(float* left, float* right, int numSamples)
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    if (!prepared_)
    {
        std::fill(left, left + numSamples, 0.0f);
        std::fill(right, right + numSamples, 0.0f);
        return;
    }

    applyPendingOversamplingIfIdle();

    const auto& parameters = activeParameters_;

    if (!panelGlidePrimed_)
    {
        glidedVolume_ = parameters.volume;
        panelGlidePrimed_ = true;
    }

    // The hold capacitors' own time constants: one for the filter family of
    // control voltages, a slower one for the amplifier's divider. These are
    // the only smoothing the scanned controls get, exactly as on the
    // hardware.
    const float controlSlew =
        1.0f - std::exp(-inverseOversampledRate_ / controlSlewSeconds);
    const float vcaSlew =
        1.0f - std::exp(-inverseOversampledRate_ / vcaSlewSeconds);
    const float outputGlide =
        1.0f - std::exp(-inverseSampleRate_ / panelGlideSeconds);
    const int scanPeriodSamples =
        std::max(1, static_cast<int>(oversampledRate_ / controlScanHz));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float outputLeft = 0.0f;
        float outputRight = 0.0f;
        bool sounding = false;

        // Two decimation stages at 4x, one at 2x, none at 1x. The inner loop
        // renders one oversampled frame.
        std::array<float, maximumOversampleFactor> stageLeft {};
        std::array<float, maximumOversampleFactor> stageRight {};

        for (int step = 0; step < oversampling_; ++step)
        {
            advanceLfo(parameters);
            const float lfo = lfoValue_ * lfoDelayLevel_;

            // One converter serves the whole instrument, but it reaches the
            // voices in turn: the multiplexer walks its hold capacitors
            // sequentially across the pass, so each voice is rewritten at
            // its own fixed phase, a sixth of the pass after its neighbour.
            // A note struck between rewrites waits for its voice's next one
            // -- which is why the shortest usable attack on this instrument
            // is a scan interval and not the published figure -- and the six
            // staircases are decorrelated by the walk, exactly as the
            // hardware's are. The lever is sampled once per pass, quantised
            // to the converter's byte; the resonance voltage is written once
            // per pass too, because one converter output serves every loop.
            if (scanCountdown_ >= scanPeriodSamples)
                scanCountdown_ = 0;
            if (scanCountdown_ == 0)
            {
                const float bendMagnitude = std::floor(
                    std::abs(pitchBendTarget_) * 255.0f + 0.5f) / 255.0f;
                pitchBend_ = pitchBendTarget_ < 0.0f ? -bendMagnitude
                                                     : bendMagnitude;
                modWheel_ =
                    std::floor(modWheelTarget_ * 127.0f + 0.5f) / 127.0f;
                resonanceCvTarget_ = std::floor(
                    parameters.resonance * 127.0f + 0.5f) / 127.0f;
            }
            resonanceCv_ += (resonanceCvTarget_ - resonanceCv_) * controlSlew;

            if (--driftControlCountdown_ <= 0)
            {
                // A fixed wall-clock rate. Counting internal samples instead
                // would make the modelled component wander four times faster
                // with oversampling on, so the same patch would drift
                // differently depending on a quality setting.
                driftControlCountdown_ = std::max(
                    1, static_cast<int>(oversampledRate_ / driftUpdateHz));
                for (auto& card : cards_)
                    updateVoiceCardDrift(card);
            }

            // One noise generator feeds every voice, so noise grows as more
            // keys are held instead of staying put.
            noiseState_ ^= noiseState_ << 13;
            noiseState_ ^= noiseState_ >> 17;
            noiseState_ ^= noiseState_ << 5;
            const float noiseSample =
                static_cast<float>(noiseState_ & 0xffffffu) * (2.0f / 16777215.0f) - 1.0f;

            float mono = 0.0f;
            float loudestEnvelope = 0.0f;

            // Every slot, not just the first `limit` of them. The voice count
            // bounds what the key assigner may take; lowering it must stop new
            // notes rather than freeze notes that are already sounding.
            for (int slot = 0; slot < maxVoices; ++slot)
            {
                auto& voice = voices_[static_cast<std::size_t>(slot)];
                if (!voice.active)
                    continue;

                // --- Converter scan -----------------------------------------
                // A voice's pitch, envelope and control voltages are only
                // rewritten when the walk reaches its phase of the pass. This
                // is why slow bends and vibrato staircase, and why the
                // shortest usable attack is a scan pass rather than the
                // published 1.5 ms.
                if (scanCountdown_ == voice.scanOffset)
                    updateVoiceScan(voice, parameters, lfo, lfoValue_);

                // Each hold capacitor's own slew turns the scan's staircase
                // back into a continuous control voltage before it reaches
                // its converter. The amplifier's hold is the slow one.
                voice.cutoffCounts +=
                    (voice.cutoffCountsTarget - voice.cutoffCounts) * controlSlew;
                voice.dcoCv += (voice.dcoCvTarget - voice.dcoCv) * controlSlew;
                voice.pwmVolts +=
                    (voice.pwmVoltsTarget - voice.pwmVolts) * controlSlew;
                voice.subCv += (voice.subCvTarget - voice.subCv) * controlSlew;
                voice.noiseCv +=
                    (voice.noiseCvTarget - voice.noiseCv) * controlSlew;
                voice.vcaControl +=
                    (voice.vcaControlTarget - voice.vcaControl) * vcaSlew;

                if (--voice.controlCountdown <= 0)
                {
                    voice.controlCountdown = controlPeriod;
                    updateVoiceAudio(voice, parameters);
                }

                mono += renderVoice(voice, parameters, noiseSample);
                loudestEnvelope = std::max(loudestEnvelope, voice.envelope.value);

                // Retire on the amplifier actually being shut. A voice card's
                // own control offset can sit above any small raw threshold yet
                // still be inside the converter's deadband, in which case the
                // voice is silent but would never be retired -- it would be
                // processed forever and would block a deferred quality change.
                if (voice.envelope.stage == EnvelopeStage::Idle
                    && !voice.keyDown && !voice.sustained
                    && vcaGain(voice.vcaControl) <= 0.0f)
                    silenceVoice(voice);
                else
                    sounding = true;
            }

            displayEnvelope_ = loudestEnvelope;
            ++scanCountdown_;

            // Series coupling before the delay lines, since a bucket-brigade
            // line integrates any offset into its own noise floor. The output
            // amplifier's rails come *after* the effect: the instrument's
            // signal order is voice sum, chorus, volume, output amplifier, so
            // dry plus wet meet the rails together. Bounding the signal ahead
            // of the chorus instead would leave the summed dry-plus-wet free
            // to leave the plug-in several decibels above full scale -- and
            // would saturate the dry path in a circuit whose first overload
            // is the delay line itself.
            const float blocked = mono - dcInput_ + dcCoefficient_ * dcOutput_;
            dcInput_ = mono;
            dcOutput_ = blocked;

            float wetLeft = blocked;
            float wetRight = blocked;
            chorus_.process(blocked, parameters.chorus, parameters.chorusNoise,
                            wetLeft, wetRight);

            stageLeft[static_cast<std::size_t>(step)] = outputStage(wetLeft);
            stageRight[static_cast<std::size_t>(step)] = outputStage(wetRight);
        }

        if (oversampling_ == 4)
        {
            float firstLeft = 0.0f;
            float firstRight = 0.0f;
            float secondLeft = 0.0f;
            float secondRight = 0.0f;
            downsamplePair(firstDecimator_, stageLeft[0], stageRight[0],
                           stageLeft[1], stageRight[1], firstLeft, firstRight);
            downsamplePair(firstDecimator_, stageLeft[2], stageRight[2],
                           stageLeft[3], stageRight[3], secondLeft, secondRight);
            downsamplePair(secondDecimator_, firstLeft, firstRight,
                           secondLeft, secondRight, outputLeft, outputRight);
        }
        else if (oversampling_ == 2)
        {
            downsamplePair(firstDecimator_, stageLeft[0], stageRight[0],
                           stageLeft[1], stageRight[1], outputLeft, outputRight);
        }
        else
        {
            outputLeft = stageLeft[0];
            outputRight = stageRight[0];
        }

        applyLatencyPad(outputLeft, outputRight);

        // How long the voices have been gone, which is what a pending quality
        // change waits on: the output path needs that long to run dry.
        if (sounding)
            oversamplingIdleSamples_ = 0;
        else if (oversamplingIdleSamples_ < oversamplingQuietSamples_)
            ++oversamplingIdleSamples_;

        glidedVolume_ += (parameters.volume - glidedVolume_) * outputGlide;
        const float volume = glidedVolume_ * glidedVolume_;
        left[sample] = std::isfinite(outputLeft) ? outputLeft * volume : 0.0f;
        right[sample] = std::isfinite(outputRight) ? outputRight * volume : 0.0f;
    }

    updateActiveVoiceCount();
}

} // namespace youknow106
