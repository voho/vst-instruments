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
// integrator, which charges at constant current and is therefore linear; one
// reverse-engineering account of the same oscillator family describes a plain
// resistive charge, which bows. The truth is a nearly linear ramp with a small
// bend from the charging transistor's own base-emitter drop, so the model keeps
// a small bow rather than choosing a side. Docs record the disagreement.
constexpr float rampBow = 0.35f;

// Mixer weights at the summing node ahead of the high-pass, referred to the
// ramp's own amplitude. The noise generator's service target is 4 Vpp against
// the ramp's 12 Vpp.
constexpr float sawMixVolts = 6.0f;
constexpr float pulseMixVolts = 5.4f;
constexpr float subMixVolts = 5.0f;
constexpr float noiseMixVolts = 2.0f;

// Input-referred noise of the transconductor stages. It is inaudible under any
// signal, and it is also the only reason a filter pushed past its oscillation
// threshold with no oscillator running has anything to start from -- a silent
// model would sit at exactly zero forever, which no analogue filter does.
constexpr float filterNoiseVolts = 2.0e-5f;

// Published envelope segment endpoints. Attack tops out four times shorter
// than the two falling segments do.
constexpr float envelopeMinimumSeconds = 0.0015f;
constexpr float attackMaximumSeconds = 3.0f;
constexpr float decayMaximumSeconds = 12.0f;

constexpr float lfoMinimumHz = 0.1f;
constexpr float lfoMaximumHz = 30.0f;
// The delay is a silent hold followed by a straight fade-in, and the fade
// itself saturates around a second; past that only the hold keeps growing.
constexpr float lfoDelayMaximumSeconds = 3.0f;
constexpr float lfoFadeMaximumSeconds = 1.08f;

// Portamento runs at a constant rate in pitch, not on a time constant: the
// panel sets seconds per octave, so a wide leap takes proportionally longer.
constexpr float portamentoFastestSecondsPerOctave = 0.05f;
constexpr float portamentoSlowestSecondsPerOctave = 12.9f;

// Amplifier control span. The exponential converter ahead of the amplifier
// gives roughly three millivolts per decibel, and the envelope's own swing
// covers about this much of it.
constexpr float vcaDynamicRangeDb = 66.0f;
// The converter's transistor does not conduct over the bottom of the control
// range, so the amplifier shuts fully rather than leaking. A residual leak is
// what a failed voice module does, not what a working one does.
constexpr float vcaDeadband = 0.03f;

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
    const float safe = std::clamp(sanitised(counts, 0.0f), -8000.0f, 40000.0f);
    const float hz = vcfBaseFrequencyHz * std::exp2(safe / vcfCountsPerOctave);
    return std::clamp(hz, 1.0f, vcfMaximumHz);
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
    // 90% of the travel, and the last tenth pushes past it into a stable,
    // transconductor-limited oscillation.
    const float position = clamp01(panelPosition);
    return vcfSelfOscillationFeedback * position / vcfSelfOscillationTravel;
}

float YouKnow106Engine::vcfResonanceCompensation(float feedback) noexcept
{
    // The instrument compensates the resonant passband loss on the *input*
    // side: it drives more signal in as regeneration rises. That is why a
    // high-resonance patch here grows dirtier rather than thinner, and it is
    // the opposite of an output-side make-up gain.
    const float k = std::clamp(sanitised(feedback, 0.0f), 0.0f, 8.0f);
    const float maximumBoostDb = 9.0f;
    const float fraction = k / vcfSelfOscillationFeedback;
    return std::pow(10.0f, maximumBoostDb * std::min(fraction, 1.2f) / 20.0f);
}

float YouKnow106Engine::vcfResonanceFrequencyTrim(float feedback) noexcept
{
    // Calibrated against the instrument's own anchor: with resonance at maximum
    // the oscillation must sit on the control law's nominal frequency, which is
    // what the service note's 248 Hz check at converter code 6272 asserts.
    const float k = std::clamp(sanitised(feedback, 0.0f), 0.0f, 8.0f);
    const float fraction = std::min(k / vcfSelfOscillationFeedback, 1.2f);
    return 1.0f + vcfOscillationTrim * fraction * fraction;
}

float YouKnow106Engine::envelopeAttackSeconds(float panelPosition) noexcept
{
    const float position = clamp01(panelPosition);
    return envelopeMinimumSeconds
         * std::pow(attackMaximumSeconds / envelopeMinimumSeconds, position);
}

float YouKnow106Engine::envelopeDecaySeconds(float panelPosition) noexcept
{
    const float position = clamp01(panelPosition);
    return envelopeMinimumSeconds
         * std::pow(decayMaximumSeconds / envelopeMinimumSeconds, position);
}

float YouKnow106Engine::envelopeReleaseSeconds(float panelPosition) noexcept
{
    return envelopeDecaySeconds(panelPosition);
}

float YouKnow106Engine::lfoRateHz(float panelPosition) noexcept
{
    const float position = clamp01(panelPosition);
    return lfoMinimumHz * std::pow(lfoMaximumHz / lfoMinimumHz, position);
}

float YouKnow106Engine::lfoDelaySeconds(float panelPosition) noexcept
{
    // Total time from the note to full modulation depth: a silent hold that
    // keeps growing across the whole travel, plus a fade that stops growing
    // about a third of the way up.
    const float position = clamp01(panelPosition);
    return lfoDelayMaximumSeconds * position * position;
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

float YouKnow106Engine::pwmControlVolts(float depth) noexcept
{
    // The comparator threshold runs from +6 V, where the ramp is bisected and
    // the pulse is square, down to +0.6 V, where it is 95% wide. It cannot be
    // driven to either rail, so 0% and 100% are unreachable.
    return 6.0f - 5.4f * clamp01(depth);
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
    // The generator upstream is linear; this is where the curve comes from.
    // Below the converter's conduction threshold the amplifier is simply shut.
    const float level = clamp01(sanitised(control, 0.0f));
    if (level <= vcaDeadband)
        return 0.0f;
    const float above = (level - vcaDeadband) / (1.0f - vcaDeadband);
    return std::pow(10.0f, vcaDynamicRangeDb * (above - 1.0f) / 20.0f);
}

float YouKnow106Engine::highPassCornerHz(HighPassMode mode) noexcept
{
    // Four legs of a switched single-pole network: a shelving boost, a
    // straight-through leg, and two progressively higher corners. Two
    // independent accounts agree that only two of the four positions filter at
    // all and that the top one is near 720 Hz; they put the middle one at
    // 225 Hz and 240 Hz respectively, so the model takes 240 Hz and the
    // research document records the spread.
    switch (mode)
    {
        case HighPassMode::Boost: return 70.0f;
        case HighPassMode::Two:   return 240.0f;
        case HighPassMode::Three: return 720.0f;
        case HighPassMode::One:
        default:                  return 70.0f;
    }
}

float YouKnow106Engine::highPassShelfGain(HighPassMode mode) noexcept
{
    // How much of the low band the leg returns. The boost position returns it
    // with 3 dB of lift, the straight-through leg returns it untouched, and the
    // two cutting legs discard it.
    switch (mode)
    {
        case HighPassMode::Boost: return std::pow(10.0f, 3.0f / 20.0f);
        case HighPassMode::One:   return 1.0f;
        case HighPassMode::Two:
        case HighPassMode::Three:
        default:                  return 0.0f;
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

float YouKnow106Engine::Envelope::tick(float attackStep, float decayStep,
                                       float sustain, float releaseStep) noexcept
{
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
            value -= decayStep;
            if (value <= sustain)
            {
                value = sustain;
                stage = EnvelopeStage::Sustain;
            }
            break;

        case EnvelopeStage::Sustain:
            value = sustain;
            break;

        case EnvelopeStage::Release:
            value -= releaseStep;
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
    subToggle = false;
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
// stage input, and V_0 = input - k * V_4.
float YouKnow106Engine::OtaCascade::process(float input, float g,
                                            float feedback) noexcept
{
    constexpr float headroom = otaHeadroomVolts;
    constexpr float inverseHeadroom = 1.0f / headroom;
    constexpr int maximumIterations = 8;

    const float gLimited = std::clamp(g, 0.0f, 64.0f);
    const float k = std::clamp(feedback, 0.0f, 8.0f);

    std::array<float, 4> selfDerivative {};
    std::array<float, 4> previousDerivative {};
    std::array<float, 4> residual {};

    for (int iteration = 0; iteration < maximumIterations; ++iteration)
    {
        float previous = input - k * voltage[3];
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
        corner[0] = previousDerivative[0] * (-k);
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
                                          float shelfGain) noexcept
{
    // Topology-preserving single pole. The high band always passes; the boost
    // position returns the low band with lift instead of discarding it.
    const float v = (input - state) * g / (1.0f + g);
    const float low = v + state;
    state = low + v;
    if (!std::isfinite(state))
        state = 0.0f;
    const float high = input - low;
    return high + shelfGain * low;
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
    for (int index = 0; index < maxVoices; ++index)
    {
        auto& card = cards_[static_cast<std::size_t>(index)];
        const std::uint32_t seed = static_cast<std::uint32_t>(index) * 2654435761u + 17u;
        card.rampCurrentError = hashBipolar(seed);
        card.comparatorOffset = hashBipolar(seed + 1u);
        card.cutoffCountError = hashBipolar(seed + 2u);
        card.resonanceError = hashBipolar(seed + 3u);
        card.vcaOffset = hashBipolar(seed + 4u);
        card.envelopeRateError = hashBipolar(seed + 5u);
        card.subLevelError = hashBipolar(seed + 6u);
        card.driftPhase = 0.5f * (hashBipolar(seed + 7u) + 1.0f);
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
    if (anyVoiceActive_)
        return false;

    oversamplingEnabled_ = oversamplingRequested_;
    updateProcessingRate();
    firstDecimator_.reset();
    secondDecimator_.reset();
    chorus_.reset();
    return true;
}

int YouKnow106Engine::getProcessingLatencySamples() const noexcept
{
    if (oversampling_ <= 1)
        return 0;

    // Each decimation stage contributes its own group delay, measured at the
    // rate that stage runs at and expressed in output samples.
    constexpr double half = (halfbandTaps - 1) / 2.0;
    double latency = 0.0;
    for (int factor = oversampling_; factor > 1; factor /= 2)
        latency += half / static_cast<double>(factor);
    return static_cast<int>(std::floor(latency + 0.5));
}

void YouKnow106Engine::reset()
{
    for (auto& voice : voices_)
    {
        voice = Voice {};
        voice.dco.reset();
        voice.filter.reset();
        voice.highPass.reset();
        voice.envelope.reset();
    }
    for (int index = 0; index < maxVoices; ++index)
        voices_[static_cast<std::size_t>(index)].cardIndex = index;

    firstDecimator_.reset();
    secondDecimator_.reset();
    chorus_.reset();
    clearHeldNotes();

    lfoPhase_ = 0.0f;
    lfoValue_ = 0.0f;
    lfoDelayLevel_ = 0.0f;
    anyKeyDown_ = false;
    noiseState_ = 0x6d2b79f5u;
    pitchBend_ = pitchBendTarget_;
    modWheel_ = modWheelTarget_;
    sustainPedalDown_ = false;
    generation_ = 0;
    roundRobinCursor_ = 0;
    activeVoiceCount_ = 0;
    anyVoiceActive_ = false;
    displayEnvelope_ = 0.0f;
    displayLfo_ = 0.0f;
    displayVoiceMask_ = 0;
    dcInput_ = 0.0f;
    dcOutput_ = 0.0f;
    driftControlCountdown_ = 0;
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
    smoothedParameters_ = targetParameters_;
}

int YouKnow106Engine::voiceLimit() const noexcept
{
    return std::clamp(smoothedParameters_.polyphony, 1, maxVoices);
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

// Unison is monophonic, so releasing the key that is sounding has to hand the
// stack back to whichever key is still down rather than silencing everything.
void YouKnow106Engine::retargetUnison(int midiNote) noexcept
{
    const int limit = voiceLimit();
    const float velocity = heldNoteVelocities_[static_cast<std::size_t>(midiNote)];
    for (int slot = 0; slot < limit; ++slot)
    {
        auto& voice = voices_[static_cast<std::size_t>(slot)];
        if (!voice.active)
            continue;
        voice.rootMidi = midiNote;
        voice.keyDown = true;
        voice.sustained = false;
        voice.releasing = false;
        voice.velocity = velocity;
        voice.targetMidi = static_cast<float>(midiNote + smoothedParameters_.keyTranspose);
        // Returning to a key that was never let go is a legato move: the
        // envelope keeps running rather than starting again.
        if (voice.envelope.stage == EnvelopeStage::Release)
            voice.envelope.stage = EnvelopeStage::Sustain;
        if (voice.glideSemitonesPerScan <= 0.0f)
            voice.currentMidi = voice.targetMidi;
    }
    lastPlayedMidi_ = static_cast<float>(midiNote);
    hasLastPlayedMidi_ = true;
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
// further note is simply dropped -- which is the documented policy of the
// instrument this models, and the reason dense chords lose notes on it. A voice
// whose key has been let go is available again even while its release rings.
int YouKnow106Engine::allocateVoice(int midiNote) noexcept
{
    const int limit = voiceLimit();
    const auto available = [this](int slot) {
        const auto& voice = voices_[static_cast<std::size_t>(slot)];
        return !voice.active || (!voice.keyDown && !voice.sustained);
    };

    if (smoothedParameters_.keyMode == KeyMode::Poly2)
    {
        // Fixed priority from the first voice down: low voices are reused
        // immediately, so only the most recent notes keep a full release.
        for (int slot = 0; slot < limit; ++slot)
            if (available(slot))
                return slot;
        return -1;
    }

    // Rotation with note affinity: an available voice that last played this
    // pitch is pulled forward, otherwise the rotation continues from where it
    // left off.
    for (int slot = 0; slot < limit; ++slot)
        if (available(slot) && voices_[static_cast<std::size_t>(slot)].rootMidi == midiNote)
        {
            roundRobinCursor_ = (slot + 1) % limit;
            return slot;
        }

    for (int attempt = 0; attempt < limit; ++attempt)
    {
        const int slot = (roundRobinCursor_ + attempt) % limit;
        if (available(slot))
        {
            roundRobinCursor_ = (slot + 1) % limit;
            return slot;
        }
    }
    return -1;
}

void YouKnow106Engine::initialiseVoice(Voice& voice, int slot, int midiNote,
                                       float velocity, bool retrigger) noexcept
{
    const auto& parameters = smoothedParameters_;
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
    voice.scanCountdown = 0;
    voice.energy = 0.0f;

    const float target = static_cast<float>(midiNote + parameters.keyTranspose);
    voice.targetMidi = target;

    const float secondsPerOctave = portamentoSeconds(parameters.portamento);
    if (secondsPerOctave > 0.0f)
    {
        if (!wasSounding && hasLastPlayedMidi_)
            voice.currentMidi = lastPlayedMidi_
                              + static_cast<float>(parameters.keyTranspose);
        // Constant rate in pitch: a wider leap takes proportionally longer,
        // rather than every glide finishing in the same time.
        voice.glideSemitonesPerScan = static_cast<float>(
            12.0 / (static_cast<double>(secondsPerOctave) * controlScanHz));
    }
    else
    {
        voice.currentMidi = target;
        voice.glideSemitonesPerScan = 0.0f;
    }

    if (!wasSounding || retrigger)
    {
        voice.envelope.reset();
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

    lastPlayedMidi_ = static_cast<float>(midiNote);
    hasLastPlayedMidi_ = true;
}

void YouKnow106Engine::silenceVoice(Voice& voice) noexcept
{
    voice.active = false;
    voice.keyDown = false;
    voice.sustained = false;
    voice.releasing = false;
    voice.rootMidi = -1;
    voice.vca = 0.0f;
    voice.vcaControlTarget = 0.0f;
    voice.vcaControl = 0.0f;
    voice.energy = 0.0f;
    voice.envelope.reset();
    voice.filter.reset();
    voice.highPass.reset();
    voice.dco.reset();
}

void YouKnow106Engine::noteOn(int midiNote, float velocity)
{
    if (midiNote < 0 || midiNote > 127)
        return;
    noteOnInternal(midiNote, std::clamp(velocity, 0.0f, 1.0f));
}

void YouKnow106Engine::noteOnInternal(int midiNote, float velocity) noexcept
{
    rememberHeldNote(midiNote, velocity);
    anyKeyDown_ = true;

    if (smoothedParameters_.keyMode == KeyMode::Unison)
    {
        // Every voice takes the same note, and every note timer divides the
        // same reference by the same integer, so there is no pitch spread at
        // all: what separates the six is the analogue block after them. Adding
        // a detune here would be inventing a behaviour the instrument does not
        // have.
        const int limit = voiceLimit();
        for (int slot = 0; slot < limit; ++slot)
        {
            auto& voice = voices_[static_cast<std::size_t>(slot)];
            const bool retrigger = !voice.active || voice.rootMidi != midiNote;
            initialiseVoice(voice, slot, midiNote, velocity, retrigger);
        }
        updateActiveVoiceCount();
        return;
    }

    const int existing = findVoiceForNote(midiNote);
    const int slot = existing >= 0 ? existing : allocateVoice(midiNote);
    if (slot < 0)
        return; // Every key is held: the note is dropped, as on the hardware.

    initialiseVoice(voices_[static_cast<std::size_t>(slot)], slot, midiNote,
                    velocity, true);
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

    if (smoothedParameters_.keyMode == KeyMode::Unison && remaining >= 0)
    {
        // Only the key that is actually sounding hands the stack on; releasing
        // an older one that the stack has already left changes nothing.
        bool sounding = false;
        for (const auto& voice : voices_)
            sounding = sounding || (voice.active && voice.rootMidi == midiNote);
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

        voice.keyDown = false;
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
}

void YouKnow106Engine::releaseAllNotes()
{
    clearHeldNotes();
    anyKeyDown_ = false;
    for (auto& voice : voices_)
    {
        if (!voice.active || !voice.keyDown)
            continue;
        voice.keyDown = false;
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
}

void YouKnow106Engine::allNotesOff()
{
    clearHeldNotes();
    anyKeyDown_ = false;
    sustainPedalDown_ = false;
    for (auto& voice : voices_)
        silenceVoice(voice);
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

    const float rate = lfoRateHz(parameters.lfoRate);
    lfoPhase_ += static_cast<float>(rate / controlScanHz);
    if (lfoPhase_ >= 1.0f)
        lfoPhase_ -= std::floor(lfoPhase_);

    // Triangle: the only shape this modulator produces.
    const float folded = lfoPhase_ < 0.5f ? lfoPhase_ : 1.0f - lfoPhase_;
    lfoValue_ = folded * 4.0f - 1.0f;

    // Delay: a silent hold, then a straight fade in. It only re-arms once every
    // voice has gone quiet, so legato playing and overlapping chords do not
    // restart it under the player's hands.
    const float total = lfoDelaySeconds(parameters.lfoDelay);
    const float fade = std::min(total, lfoFadeMaximumSeconds);
    const float holdoff = std::max(0.0f, total - fade);
    const float scanPeriod = static_cast<float>(1.0 / controlScanHz);

    if (!anyKeyDown_ && !anyVoiceActive_)
    {
        lfoDelayArmed_ = true;
        lfoDelayHoldoff_ = 0.0f;
        lfoDelayLevel_ = total > 1.0e-4f ? 0.0f : 1.0f;
    }
    else if (total <= 1.0e-4f)
    {
        lfoDelayLevel_ = 1.0f;
    }
    else
    {
        lfoDelayArmed_ = false;
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
                                       float lfoValue) noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float tolerance = parameters.calibration;
    const float scanRate = static_cast<float>(controlScanHz);

    // --- Envelope ---------------------------------------------------------
    // Straight-line segments, advanced once per scan. The published minimum of
    // 1.5 ms is shorter than one scan pass, so the shortest attack the
    // instrument can actually produce is one pass long.
    const float rateError = 1.0f + card.envelopeRateError * 0.06f * tolerance;
    const float attackSeconds = envelopeAttackSeconds(parameters.attack) * rateError;
    const float decaySeconds = envelopeDecaySeconds(parameters.decay) * rateError;
    const float releaseSeconds = envelopeReleaseSeconds(parameters.release) * rateError;

    voice.attackStep = 1.0f / std::max(1.0f, attackSeconds * scanRate);
    voice.decayStep = 1.0f / std::max(1.0f, decaySeconds * scanRate);
    voice.releaseStep = 1.0f / std::max(1.0f, releaseSeconds * scanRate);

    voice.envelope.tick(voice.attackStep, voice.decayStep, parameters.sustain,
                        voice.releaseStep);
    const float envelope = voice.envelope.value;

    // --- Pitch ------------------------------------------------------------
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

    // The bender's own modulation axis and the panel's modulator slider reach
    // the same summing point, so the deeper of the two wins rather than the
    // pair adding to twice the depth.
    const float lfoPitchDepth = std::max(parameters.dcoLfoDepth,
                                         parameters.benderLfoDepth * modWheel_);
    const float cents = parameters.masterTuneCents
        + parameters.benderDcoDepth * benderPitchCents * pitchBend_
        + lfoPitchDepth * lfoPitchCents * lfoValue;
    const double midi = static_cast<double>(voice.currentMidi)
                      + static_cast<double>(cents) / 100.0;

    voice.dco.divider = dcoDivider(midiToHz(midi));
    const double frequency = dcoQuantisedFrequency(voice.dco.divider, parameters.range);
    voice.dco.periodSamples = frequency > 0.0 ? oversampledRate_ / frequency : 1.0e6;

    // --- Filter cutoff, summed in converter counts ------------------------
    float counts = vcfPanelCounts(parameters.cutoff);
    const float envelopeSign =
        parameters.envPolarity == EnvPolarity::Normal ? 1.0f : -1.0f;
    counts += envelopeSign * parameters.envDepth * vcfEnvelopeCounts * envelope;
    counts += parameters.vcfLfoDepth * vcfLfoCounts * lfoValue;
    counts += parameters.benderVcfDepth * vcfBenderCounts * pitchBend_;
    counts += parameters.keyFollow * vcfCountsPerOctave
            * (voice.currentMidi - vcfKeyFollowCentreMidi) / 12.0f;
    // Voice-to-voice cutoff spread. A hardware-fitted figure for this module is
    // about five per cent, which is a little over a tenth of an octave.
    counts += card.cutoffCountError * 0.07f * vcfCountsPerOctave * tolerance;
    counts += card.driftValue * 40.0f * tolerance;
    voice.cutoffCountsTarget = counts;

    // --- Pulse width ------------------------------------------------------
    float pwmAmount = parameters.pwmDepth;
    switch (parameters.pwmSource)
    {
        case PwmSource::Lfo:
            pwmAmount = parameters.pwmDepth * 0.5f * (1.0f + lfoValue);
            break;
        case PwmSource::Manual:
        default:
            break;
    }
    const float threshold = pwmControlVolts(clamp01(pwmAmount))
                          + card.comparatorOffset * 0.12f * tolerance;
    voice.pulseWidth = pwmDutyCycle(threshold);

    // --- Amplifier control ------------------------------------------------
    const float velocityGain = 1.0f
        - parameters.velocityDepth * (1.0f - voice.velocity);
    const float control = parameters.vcaMode == VcaMode::Envelope
                        ? envelope
                        : (voice.keyDown || voice.sustained ? 1.0f : 0.0f);
    voice.vcaControlTarget = clamp01(control * parameters.vcaLevel * velocityGain
                                     + card.vcaOffset * 0.004f * tolerance);
}

void YouKnow106Engine::updateVoiceAudio(Voice& voice,
                                        const EngineParameters& parameters) noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];

    const float resonancePanel = clamp01(parameters.resonance
        + card.resonanceError * 0.02f * parameters.calibration);
    voice.feedback = vcfFeedback(resonancePanel);
    voice.inputCompensation = vcfResonanceCompensation(voice.feedback);

    const float cutoffHz = vcfCutoffHz(voice.cutoffCounts)
                         * vcfResonanceFrequencyTrim(voice.feedback);
    const float limited =
        std::min(cutoffHz, static_cast<float>(oversampledRate_) * 0.45f);
    voice.filterG = std::tan(pi * limited * inverseOversampledRate_);

    const float corner = highPassCornerHz(parameters.highPass);
    voice.highPassG =
        std::tan(pi * std::min(corner, static_cast<float>(oversampledRate_) * 0.45f)
                 * inverseOversampledRate_);
    voice.highPassShelf = highPassShelfGain(parameters.highPass);

    voice.vca = vcaGain(voice.vcaControl);
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
    const bool crossedRise = previousPhase < rise && unwrapped >= rise;

    // --- Ramp -------------------------------------------------------------
    const float amplitude = sawMixVolts
        * (1.0f + card.rampCurrentError * 0.02f * parameters.calibration);

    const float sawNaive = phase < rise
        ? 2.0f * rampVoltage(static_cast<float>(phase / rise), rampBow) - 1.0f
        : 1.0f - 2.0f * static_cast<float>((phase - rise) / reset);

    // The ramp has no value discontinuity: the reset is a steep segment, not a
    // jump. Both of its corners are slope discontinuities, so both are repaired
    // with the slope residual rather than a step residual.
    const float denominator = 1.0f - std::exp(-rampBow);
    const float slopeAtStart =
        2.0f * rampBow / denominator / static_cast<float>(rise);
    const float slopeAtEnd =
        2.0f * rampBow * std::exp(-rampBow) / denominator / static_cast<float>(rise);
    const float fallSlope = -2.0f / static_cast<float>(reset);
    const float incrementF = static_cast<float>(increment);

    if (crossedRise)
        addSlope(dco.saw, (fallSlope - slopeAtEnd) * incrementF, samplesAgo(rise));
    if (wrapped)
        addSlope(dco.saw, (slopeAtStart - fallSlope) * incrementF, samplesAgo(1.0));

    const float sawOut = dco.saw.advance(sawNaive) * amplitude;

    // --- Pulse ------------------------------------------------------------
    // The comparator flips when the ramp crosses the threshold on the way up
    // and again when the reset drags it back down past it.
    const double duty = std::clamp(static_cast<double>(voice.pulseWidth), 0.05, 0.95);
    const double riseEdge = rise * (1.0 - duty);
    if (previousPhase < riseEdge && unwrapped >= riseEdge && dco.pulseState < 0.0f)
    {
        addStep(dco.pulse, 2.0f, samplesAgo(riseEdge));
        dco.pulseState = 1.0f;
    }
    if (crossedRise && dco.pulseState > 0.0f)
    {
        addStep(dco.pulse, -2.0f, samplesAgo(rise));
        dco.pulseState = -1.0f;
    }
    const float pulseOut = dco.pulse.advance(dco.pulseState) * pulseMixVolts;

    // --- Sub --------------------------------------------------------------
    // A flip-flop halves the note clock, so the sub is an exact square one
    // octave down and takes no part in pulse-width modulation.
    if (wrapped)
    {
        dco.subToggle = !dco.subToggle;
        const float target = dco.subToggle ? 1.0f : -1.0f;
        addStep(dco.sub, target - dco.subState, samplesAgo(1.0));
        dco.subState = target;
    }
    const float subGain = subMixVolts * parameters.subLevel
        * (1.0f + card.subLevelError * 0.03f * parameters.calibration);
    const float subOut = dco.sub.advance(dco.subState) * subGain;

    // --- Summing node ------------------------------------------------------
    float mixed = 0.0f;
    if (parameters.sawEnabled)
        mixed += sawOut;
    if (parameters.pulseEnabled)
        mixed += pulseOut;
    mixed += subOut;
    mixed += noiseSample * noiseMixVolts * parameters.noiseLevel;

    voice.noiseState ^= voice.noiseState << 13;
    voice.noiseState ^= voice.noiseState >> 17;
    voice.noiseState ^= voice.noiseState << 5;
    mixed += (static_cast<float>(voice.noiseState & 0xffffffu) * (2.0f / 16777215.0f)
              - 1.0f) * filterNoiseVolts;

    // --- High-pass, filter, amplifier --------------------------------------
    const float shaped = voice.highPass.process(mixed, voice.highPassG,
                                                voice.highPassShelf);
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

    const auto& parameters = smoothedParameters_;
    const float bendSmoothing = 1.0f - std::exp(-inverseOversampledRate_ * 60.0f);
    const float controlSlew =
        1.0f - std::exp(-inverseOversampledRate_ / controlSlewSeconds);
    const int scanPeriodSamples =
        std::max(1, static_cast<int>(oversampledRate_ / controlScanHz));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float outputLeft = 0.0f;
        float outputRight = 0.0f;

        // Two decimation stages at 4x, one at 2x, none at 1x. The inner loop
        // renders one oversampled frame.
        std::array<float, maximumOversampleFactor> stageLeft {};
        std::array<float, maximumOversampleFactor> stageRight {};

        for (int step = 0; step < oversampling_; ++step)
        {
            pitchBend_ += (pitchBendTarget_ - pitchBend_) * bendSmoothing;
            modWheel_ += (modWheelTarget_ - modWheel_) * bendSmoothing;

            advanceLfo(parameters);
            const float lfo = lfoValue_ * lfoDelayLevel_;

            if (--driftControlCountdown_ <= 0)
            {
                driftControlCountdown_ = controlPeriod * 64;
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
                // The whole instrument shares one converter, so a voice's
                // pitch, envelope and cutoff are only rewritten when the scan
                // reaches it. This is why slow bends and vibrato staircase, and
                // why the shortest usable attack is a scan pass rather than the
                // published 1.5 ms.
                if (--voice.scanCountdown <= 0)
                {
                    voice.scanCountdown = scanPeriodSamples;
                    updateVoiceScan(voice, parameters, lfo);
                }

                // The hold capacitor's own slew turns the scan's staircase back
                // into a continuous control voltage before it reaches the
                // exponential converters.
                voice.cutoffCounts +=
                    (voice.cutoffCountsTarget - voice.cutoffCounts) * controlSlew;
                voice.vcaControl +=
                    (voice.vcaControlTarget - voice.vcaControl) * controlSlew;

                if (--voice.controlCountdown <= 0)
                {
                    voice.controlCountdown = controlPeriod;
                    updateVoiceAudio(voice, parameters);
                }

                mono += renderVoice(voice, parameters, noiseSample);
                loudestEnvelope = std::max(loudestEnvelope, voice.envelope.value);

                if (voice.envelope.stage == EnvelopeStage::Idle
                    && !voice.keyDown && !voice.sustained
                    && voice.vcaControl < 1.0e-4f)
                    silenceVoice(voice);
            }

            displayEnvelope_ = loudestEnvelope;

            // Series coupling before the delay lines, since a bucket-brigade
            // line integrates any offset into its own noise floor, then the
            // output amplifier. This order matters: coupling out the offset of
            // a narrow pulse raises its peaks, and the amplifier is what the
            // raised peaks meet.
            const float blocked = mono - dcInput_ + dcCoefficient_ * dcOutput_;
            dcInput_ = mono;
            dcOutput_ = blocked;
            const float driven = outputStage(blocked);

            float wetLeft = driven;
            float wetRight = driven;
            chorus_.process(driven, parameters.chorus, parameters.chorusNoise,
                            wetLeft, wetRight);

            stageLeft[static_cast<std::size_t>(step)] = wetLeft;
            stageRight[static_cast<std::size_t>(step)] = wetRight;
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

        const float volume = parameters.volume * parameters.volume;
        left[sample] = std::isfinite(outputLeft) ? outputLeft * volume : 0.0f;
        right[sample] = std::isfinite(outputRight) ? outputRight * volume : 0.0f;
    }

    updateActiveVoiceCount();
}

} // namespace youknow106
