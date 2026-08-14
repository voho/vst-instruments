#include "NeuramarEngine.h"

#include "AirFilterbank.h"
#include "SpectralEnvelope.h"

#include <algorithm>
#include <cmath>

namespace neuramar
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

[[nodiscard]] float finiteOr(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

[[nodiscard]] float clampParameter(float value, float minimum,
                                   float maximum, float fallback) noexcept
{
    return std::clamp(finiteOr(value, fallback), minimum, maximum);
}

[[nodiscard]] float interpolatePhaseShortest(float first, float second,
                                             float amount) noexcept
{
    float difference = second - first;
    difference -= std::floor(difference + 0.5f);
    float result = first + amount * difference;
    result -= std::floor(result);
    return result;
}

// The limit is 0.49 * sampleRate and the fade scale is 1 / (0.06 *
// sampleRate); both are precomputed in prepare() so the per-sample harmonic
// loop multiplies instead of dividing for every audible harmonic.
[[nodiscard]] float coreNyquistGain(float frequencyHz, float nyquistLimitHz,
                                    float nyquistFadeScale) noexcept
{
    return std::clamp((nyquistLimitHz - frequencyHz) * nyquistFadeScale,
                      0.0f, 1.0f);
}

// Ceiling on the level a voice-steal fade tail may hold. One frozen sample from
// one voice is far below this, and the first steal into an idle slot is stored
// unclamped, so an ordinary hand-off never reaches it; it only bounds the case
// where the same slot is stolen again and again inside one 3 ms window, where a
// small residual step is preferable to walking the sum into the finite-output
// guard.
constexpr float maximumFadeTailLevel = 4.0f;

[[nodiscard]] float guardFiniteOutput(float value) noexcept
{
    // Hosts process floating-point audio above 0 dBFS. Preserve that headroom
    // instead of waveshaping every overload at base rate, which would fold new
    // harmonics into high-register notes. This is only a pathological-state
    // guard; ordinary level control remains exactly linear.
    constexpr float emergencyMagnitude = 7.95f;
    return std::clamp(finiteOr(value, 0.0f),
                      -emergencyMagnitude, emergencyMagnitude);
}

[[nodiscard]] std::array<float, NeuralModel::harmonicCount>
makeSourceFilterEnvelope(
    const std::array<float, NeuralModel::harmonicCount>& amplitudes) noexcept
{
    // Parity-balanced power kernel: half of the total weight sits on even
    // offsets (the centre and +/-2 taps) and half on the odd +/-1 taps, so an
    // alternating odd/even excitation cancels exactly. Narrowing the shoulder
    // from the former 0.0625 to 0.015625 keeps that cancellation but blurs a
    // narrow formant over a much smaller span, which is what actually limits
    // held-out register accuracy. Reflecting at both ends preserves the parity
    // balance at the evidence boundaries instead of renormalising a truncated,
    // parity-biased weight sum.
    constexpr float shoulderWeight = 0.015625f;
    constexpr std::array<float, 5> weights {
        shoulderWeight, 0.25f, 0.5f - 2.0f * shoulderWeight, 0.25f,
        shoulderWeight
    };
    std::array<float, NeuralModel::harmonicCount> envelope {};
    constexpr auto lastIndex = static_cast<std::ptrdiff_t>(
        NeuralModel::harmonicCount) - 1;
    for (std::size_t harmonic = 0; harmonic < amplitudes.size(); ++harmonic)
    {
        double power = 0.0;
        for (int offset = -2; offset <= 2; ++offset)
        {
            auto neighbour = static_cast<std::ptrdiff_t>(harmonic)
                + static_cast<std::ptrdiff_t>(offset);
            if (neighbour < 0)
                neighbour = -neighbour;
            else if (neighbour > lastIndex)
                neighbour = 2 * lastIndex - neighbour;
            neighbour = std::clamp<std::ptrdiff_t>(neighbour, 0, lastIndex);

            const float weight = weights[static_cast<std::size_t>(offset + 2)];
            const float amplitude = std::max(
                amplitudes[static_cast<std::size_t>(neighbour)], 0.0f);
            power += static_cast<double>(weight) * amplitude * amplitude;
        }
        envelope[harmonic] = static_cast<float>(std::sqrt(std::max(power, 0.0)));
    }
    return envelope;
}

[[nodiscard]] float blendMagnitude(float first, float second,
                                   float amount) noexcept
{
    // Companded log interpolation is perceptually smooth, keeps both endpoints
    // exact (including silence), and avoids the level swell of a linear blend.
    constexpr float knee = 1.0e-5f;
    first = std::max(finiteOr(first, 0.0f), 0.0f);
    second = std::max(finiteOr(second, 0.0f), 0.0f);
    amount = std::clamp(finiteOr(amount, 0.0f), 0.0f, 1.0f);
    const float firstLog = std::log1p(first / knee);
    const float secondLog = std::log1p(second / knee);
    return knee * std::expm1(firstLog + amount * (secondLog - firstLog));
}

[[nodiscard]] float wrapUnit(float phase) noexcept
{
    phase -= std::floor(phase);
    return phase;
}

// sin(2 pi unitPhase) for a phase already reduced to [0, 1).
//
// The quarter-period fold is exact in floating point: every subtraction below
// is between neighbouring magnitudes, so the folded coordinate carries the
// argument's full precision. The remaining series is the odd Taylor expansion
// through theta^11 over [0, pi/2]; it alternates, so its truncation error is
// bounded by the first omitted term, (pi/2)^13/13! = 5.7e-8, about -145 dB, and
// what is left on top of that is float rounding. Measured against
// double-precision sin() over 2e7 uniformly spaced phases the result is a peak
// error of 2.08e-7 (-133.6 dB) and an RMS error of 3.78e-8 (-148.5 dB), against
// 5.28e-7 (-125.6 dB) and 1.87e-7 (-134.6 dB) for the 4096-entry linearly
// interpolated table this replaces. It also needs no memory at all and - unlike
// a table gather - lets the whole partial loop vectorise.
[[nodiscard]] float unitSine(float unitPhase) noexcept
{
    const float offset = unitPhase - 0.5f;
    const float folded = 0.25f - std::abs(std::abs(offset) - 0.25f);
    const float angle = twoPi * folded;
    const float square = angle * angle;
    float series = -2.50521084e-8f;              // -1/11!
    series = series * square + 2.75573192e-6f;   //  1/9!
    series = series * square - 1.98412698e-4f;   // -1/7!
    series = series * square + 8.33333333e-3f;   //  1/5!
    series = series * square - 1.66666667e-1f;   // -1/3!
    series = series * square + 1.0f;
    const float magnitude = angle * series;
    return offset < 0.0f ? magnitude : -magnitude;
}

[[nodiscard]] float sine(float phase) noexcept
{
    return unitSine(wrapUnit(phase));
}

// Raised-cosine fade-tail window: 0 at the start of a steal's carry-over, 1
// once it has run its full length. beginFadeTail() and the per-sample mix in
// process() both shape the same tail with this curve; sharing it means the
// two can never drift apart the way two independently maintained copies of
// the same formula could.
[[nodiscard]] float fadeTailWindow(float position) noexcept
{
    return 0.5f + 0.5f * sine(0.25f + 0.5f * std::min(position, 1.0f));
}
} // namespace

void NeuramarEngine::Bandpass::set(float centreHz, float bandwidthOctaves,
                                   float sampleRate, int rampSamples) noexcept
{
    // Control frames re-assert the same centre for as long as the pitch and
    // brightness stand still. Once the previous ramp has finished there is
    // nothing left to move, so the sinh/sin/cos/sqrt redesign is skipped. An
    // early re-assertion mid-ramp still falls through and re-anchors, exactly
    // like a changed centre.
    if (centreHz == configuredCentreHz
        && bandwidthOctaves == configuredBandwidthOctaves
        && rampRemaining == 0)
        return;
    configuredCentreHz = centreHz;
    configuredBandwidthOctaves = bandwidthOctaves;

    const auto coefficients = airfilter::makeCoefficients(
        centreHz, bandwidthOctaves, sampleRate);
    if (rampSamples <= 0)
    {
        b0 = coefficients.b0;
        b2 = coefficients.b2;
        a1 = coefficients.a1;
        a2 = coefficients.a2;
        outputScale = coefficients.outputScale;
        b0Step = b2Step = a1Step = a2Step = outputScaleStep = 0.0f;
        rampRemaining = 0;
        return;
    }

    const float inverseRamp = 1.0f / static_cast<float>(rampSamples);
    b0Step = (coefficients.b0 - b0) * inverseRamp;
    b2Step = (coefficients.b2 - b2) * inverseRamp;
    a1Step = (coefficients.a1 - a1) * inverseRamp;
    a2Step = (coefficients.a2 - a2) * inverseRamp;
    outputScaleStep = (coefficients.outputScale - outputScale) * inverseRamp;
    rampRemaining = rampSamples;
}

void NeuramarEngine::Bandpass::finishRamp() noexcept
{
    if (rampRemaining <= 0)
        return;
    const auto remaining = static_cast<float>(rampRemaining);
    b0 += b0Step * remaining;
    b2 += b2Step * remaining;
    a1 += a1Step * remaining;
    a2 += a2Step * remaining;
    outputScale += outputScaleStep * remaining;
    b0Step = b2Step = a1Step = a2Step = outputScaleStep = 0.0f;
    rampRemaining = 0;
}

float NeuramarEngine::Bandpass::tickSide(float input) noexcept
{
    const float output = b0 * input + sideZ1;
    sideZ1 = sideZ2 - a1 * output;
    sideZ2 = b2 * input - a2 * output;
    return output * outputScale;
}

float NeuramarEngine::Bandpass::tick(float input) noexcept
{
    if (rampRemaining > 0)
    {
        b0 += b0Step;
        b2 += b2Step;
        a1 += a1Step;
        a2 += a2Step;
        outputScale += outputScaleStep;
        if (--rampRemaining == 0)
            b0Step = b2Step = a1Step = a2Step = outputScaleStep = 0.0f;
    }
    const float output = b0 * input + z1;
    z1 = z2 - a1 * output;
    z2 = b2 * input - a2 * output;
    return output * outputScale;
}

void NeuramarEngine::Voice::clear() noexcept
{
    *this = Voice {};
}

NeuramarEngine::NeuramarEngine() noexcept
{
    for (std::size_t harmonic = 0; harmonic < NeuralModel::harmonicCount; ++harmonic)
        inverseHarmonicRolloff_[harmonic] = 1.0f
            / std::pow(static_cast<float>(harmonic + 1), 1.15f);
    refreshHarmonicStretch(0.0f);
}

// Circular interpolation of the learned onset phases at a fractional harmonic
// coordinate. The unit vectors are cached by setModel(), so a note-on that has
// to map all 256 rendered partials costs one atan2 each instead of two sines,
// two cosines and an atan2 - the whole mapping happens on the audio thread.
float NeuramarEngine::initialPhaseAt(const NeuralModel& model,
                                     float oneBasedIndex) const noexcept
{
    const auto& phases = model.initialHarmonicPhases_;
    if (!(oneBasedIndex >= 1.0f))
        return phases.front();
    if (oneBasedIndex >= static_cast<float>(phases.size()))
        return phases.back();

    const float zeroBased = oneBasedIndex - 1.0f;
    const auto lower = static_cast<std::size_t>(zeroBased);
    const float fraction = zeroBased - static_cast<float>(lower);
    if (!(fraction > 0.0f))
        return phases[lower];
    const float real = (1.0f - fraction) * initialPhaseCos_[lower]
        + fraction * initialPhaseCos_[lower + 1];
    const float imaginary = (1.0f - fraction) * initialPhaseSin_[lower]
        + fraction * initialPhaseSin_[lower + 1];
    if (std::abs(real) + std::abs(imaginary) < 1.0e-8f)
        return phases[lower];
    float result = std::atan2(imaginary, real) / twoPi;
    result -= std::floor(result);
    return result;
}

void NeuramarEngine::refreshHarmonicStretch(float inharmonicity) noexcept
{
    inharmonicity = std::clamp(finiteOr(inharmonicity, 0.0f), 0.0f,
                               2.0f * NeuralModel::maximumInharmonicity);
    if (inharmonicity == cachedInharmonicity_)
        return;

    cachedInharmonicity_ = inharmonicity;
    for (std::size_t harmonic = 0; harmonic < renderedHarmonicCount; ++harmonic)
    {
        harmonicStretchRatio_[harmonic] = stretchedHarmonicRatio(
            static_cast<float>(harmonic + 1), inharmonicity);
    }
}

void NeuramarEngine::prepare(double sampleRate, int maxBlockSize)
{
    (void) maxBlockSize;
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 768000.0)
        sampleRate = 48000.0;

    sampleRate_ = sampleRate;
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    coreNyquistLimitHz_ = 0.49f * static_cast<float>(sampleRate_);
    coreNyquistFadeScale_ = static_cast<float>(
        1.0 / (0.06 * sampleRate_));
    boneCeilingHz_ = coreNyquistLimitHz_;
    // 20 kHz is the top of the audible band, so an Air band or a Bone mode
    // that reaches it is faded out identically whether the host runs at 48 or
    // at 192 kHz. Only a host rate low enough to bring its own Nyquist below
    // that ceiling narrows the limit further, which is unavoidable.
    const auto rate = static_cast<float>(sampleRate_);
    airEdgeLimitHz_ = std::min(0.45f * rate, 20000.0f);
    airEdgeFadeHz_ = std::min(0.07f * rate, 2000.0f);
    boneEdgeLimitHz_ = std::min(0.49f * rate, 20000.0f);
    boneEdgeFadeHz_ = std::min(0.04f * rate, 1400.0f);
    controlPeriod_ = std::clamp(static_cast<int>(std::lround(sampleRate_ / 250.0)),
                                16, 4096);
    // 3 ms, floored at 16 samples. Depends on nothing but the sample rate, so
    // it is resolved here once rather than on every voice steal.
    fadeTailSamples_ = std::max(16,
        static_cast<int>(std::lround(0.003 * sampleRate_)));
    // A 6 ms smoother: fast enough to feel immediate, slow enough that a host
    // sending one value per block cannot step the level between blocks.
    outputGainCoefficient_ = static_cast<float>(
        1.0 - std::exp(-1.0 / (0.006 * sampleRate_)));
    smoothedOutputGain_ = -1.0f;
    reset();
}

void NeuramarEngine::reset() noexcept
{
    for (auto& voice : voices_)
        voice.clear();
    for (auto& tail : fadeTails_)
        tail.clear();
    ageCounter_ = 0;
}

void NeuramarEngine::setModel(const NeuralModel* immutableModel) noexcept
{
    const auto* previous = model_.load(std::memory_order_acquire);
    if (previous == immutableModel)
        return;

    if (immutableModel == nullptr)
    {
        allSoundOff();
        dampingExponent_ = 0.0f;
        model_.store(nullptr, std::memory_order_release);
        return;
    }

    for (std::size_t index = 0; index < voices_.size(); ++index)
    {
        if (voices_[index].active)
            beginFadeTail(index);
        voices_[index].clear();
    }
    for (std::size_t harmonic = 0; harmonic < NeuralModel::harmonicCount;
         ++harmonic)
    {
        const float angle = twoPi
            * immutableModel->initialHarmonicPhases_[harmonic];
        initialPhaseCos_[harmonic] = std::cos(angle);
        initialPhaseSin_[harmonic] = std::sin(angle);
    }
    sampleLoopLevelTrajectory(*immutableModel);
    // Fit at the gains the parameters are actually sitting at, so the very
    // first block after a model swap is already correct.
    const auto current = loadParameters();
    refreshLoopLevelSlope(current.air, current.bone);
    dampingExponent_ = fitDampingExponent(*immutableModel);
    model_.store(immutableModel, std::memory_order_release);
}

// The three layers' power across the loop region, sampled at the points the
// slope fit below uses. Kept per layer rather than summed, because the slope
// that matters is the slope of the mix the renderer produces, and the renderer
// weights Air and Bone by their controls.
void NeuramarEngine::sampleLoopLevelTrajectory(const NeuralModel& model) noexcept
{
    const float duration = std::max(model.metadata_.durationSeconds, 0.001f);
    const float loopStart = std::clamp(model.metadata_.loopStartSeconds,
                                       0.0f, duration);
    const float loopEnd = std::clamp(model.metadata_.loopEndSeconds,
                                     loopStart + 0.001f, duration);
    loopFitLengthSeconds_ = std::max(loopEnd - loopStart, 0.001f);
    // setModel() runs on the audio thread, so the sampling is budgeted rather
    // than generous. The trajectory being fitted is a smooth decoder output,
    // not sampled data, so 32 points land within 0.03% of a 256-point fit on
    // every fixture that has a trend at all, and within 0.002 dB of correction
    // across the loop on models whose loop region is already flat. They are
    // about 120 us of decoder evaluation, against roughly 250 us that a model
    // swap already spends clearing its voices and rebuilding the onset-phase
    // table. Splitting the sum three ways costs none of that again.
    for (int point = 0; point < loopFitPoints; ++point)
    {
        const auto slot = static_cast<std::size_t>(point);
        const float offsetSeconds = loopFitLengthSeconds_
            * (static_cast<float>(point) / static_cast<float>(loopFitPoints - 1));
        SynthesisFrame frame;
        model.evaluate((loopStart + offsetSeconds) / duration, frame);
        double core = 0.0;
        double air = 0.0;
        double bone = 0.0;
        for (const float amplitude : frame.harmonicAmplitudes)
            core += static_cast<double>(amplitude) * amplitude;
        for (const float amplitude : frame.airAmplitudes)
            air += static_cast<double>(amplitude) * amplitude;
        // A Bone mode the analysis could not vouch for is never rendered, so it
        // is never fitted either: `active` in the renderer's Bone targets is
        // this same reliability test, and leaving those modes in the fit let a
        // trend nobody hears set the correction for the one they do.
        for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
            if (model.boneModeReliabilities_[mode] > 0.0f)
                bone += static_cast<double>(frame.boneAmplitudes[mode])
                    * frame.boneAmplitudes[mode];
        loopFitSeconds_[slot] = offsetSeconds;
        loopFitCorePower_[slot] = static_cast<float>(core);
        loopFitAirPower_[slot] = static_cast<float>(air);
        loopFitBonePower_[slot] = static_cast<float>(bone);
    }
    // Force the next refresh to do its work whatever the gains are.
    loopFitAir_ = -1.0f;
    loopFitBone_ = -1.0f;
}

// Least-squares slope of the model's own log amplitude across the loop region,
// taken on the mix the renderer is actually producing.
//
// A learned loop is a stable region, not a stationary one: it still carries the
// source's overall decay, so reading it forward and wrapping back to its start
// steps the level up by exactly this trend on every pass. Fitting the trend and
// dividing it out is what makes the wrap continuous. Fitting a *line in log
// amplitude* rather than replacing the level with the region's mean is the
// whole point: a mean hold would also delete the tremolo, breath pulsing and
// beating the region carries, and those are the residuals about this line.
//
// The layers are weighted by the squares of their controls because the samples
// are powers and the controls are amplitude gains. Everything else the renderer
// puts on a layer - the Core's register normalisation, the Air and Bone edge
// fades, Touch - is constant across the loop for a given note, and a constant
// factor moves a log-power line's intercept without touching its slope, so it
// cannot change the answer and is not carried here. What does change the answer
// is the balance *between* layers, and that is what these two controls set.
void NeuramarEngine::refreshLoopLevelSlope(float air, float bone) noexcept
{
    if (air == loopFitAir_ && bone == loopFitBone_)
        return;
    loopFitAir_ = air;
    loopFitBone_ = bone;

    const double airWeight = static_cast<double>(air) * air;
    const double boneWeight = static_cast<double>(bone) * bone;
    double sumTime = 0.0;
    double sumLevel = 0.0;
    double sumTimeSquared = 0.0;
    double sumTimeLevel = 0.0;
    for (int point = 0; point < loopFitPoints; ++point)
    {
        const auto slot = static_cast<std::size_t>(point);
        const double power = static_cast<double>(loopFitCorePower_[slot])
            + airWeight * loopFitAirPower_[slot]
            + boneWeight * loopFitBonePower_[slot];
        const double level = 0.5 * std::log(std::max(power, 1.0e-24));
        const double time = static_cast<double>(loopFitSeconds_[slot]);
        sumTime += time;
        sumLevel += level;
        sumTimeSquared += time * time;
        sumTimeLevel += time * level;
    }
    constexpr double count = static_cast<double>(loopFitPoints);
    const double denominator = count * sumTimeSquared - sumTime * sumTime;
    if (!(denominator > 0.0))
    {
        loopLevelSlopePerSecond_ = 0.0f;
        return;
    }
    const double slope = (count * sumTimeLevel - sumTime * sumLevel)
        / denominator;
    if (!std::isfinite(slope))
    {
        loopLevelSlopePerSecond_ = 0.0f;
        return;
    }
    // A safety limit, not a routine constraint: the correction applied across
    // one pass is bounded to 12 dB either way, so a pathological loop region
    // cannot turn into a large gain. The learned loops measured here need
    // between 0.04 dB and 9.5 dB, so the bound is a guard rather than
    // something a real memory runs into.
    const double limit = (12.0 / 8.685889638)
        / static_cast<double>(std::max(loopFitLengthSeconds_, 0.001f));
    loopLevelSlopePerSecond_ = static_cast<float>(std::clamp(slope, -limit, limit));
}

// The exponent p in tau(f) = tau_1 (f/f_1)^-p, read out of the model's own
// per-partial amplitude trajectories. Real damping is frequency-dependent -
// air viscosity dominates at low frequency and internal friction at high, so
// decay time falls with frequency (Desvages, Bilbao, Ducceschi and Chabassier,
// POMA 28, 035005, 2017) - and this is the only place the instrument can find
// out how steeply it falls for the sound the user actually dropped in.
//
// Each partial's own time constant is fitted first, in log amplitude, over the
// span between that partial's peak and the point where it has lost 40 dB; then
// log(1/tau) is regressed against log(harmonic number), whose slope is p. Two
// gates decide whether the answer is evidence or noise. A partial that loses
// less than 6 dB across the window it was fitted on carries a trend rather
// than a decay and is left out, and if fewer than six partials survive that -
// which is what a sustained source looks like - the fit is abandoned. So is a
// fit whose partials scatter more than 0.15 in log tau about the fitted line,
// which is where a struck inharmonic body and a bowed note land: measured
// residuals are 0.0002 on a tau_h = tau_1 h^-0.75 source and 0.33 on a
// sustained one. Abandoning it returns exactly zero, and a zero exponent
// renders the release the instrument has always had.
float NeuramarEngine::fitDampingExponent(const NeuralModel& model) noexcept
{
    constexpr int scanPoints = 64;
    constexpr std::size_t harmonics = NeuralModel::harmonicCount;
    const double duration = std::max(
        static_cast<double>(model.metadata_.durationSeconds), 0.001);

    struct PartialFit
    {
        double peak { 0.0 };
        double startSeconds { 0.0 };
        double spanSeconds { 0.0 };
        double sumTime { 0.0 };
        double sumLevel { 0.0 };
        double sumTimeSquared { 0.0 };
        double sumTimeLevel { 0.0 };
        int count { 0 };
        bool finished { false };
    };
    std::array<PartialFit, harmonics> partials {};

    // One forward pass. Restarting a partial's accumulators whenever it sets a
    // new maximum is what makes a single pass equivalent to fitting from the
    // peak: past the true peak the running maximum is already final, so no
    // learned onset is ever fitted as if it were decay.
    //
    // The scan is warped by the same 1.24 the trajectory grid itself uses past
    // the onset, so the points land where the model actually carries detail. A
    // uniform scan under-samples exactly the early span where the fast
    // partials live and have already finished: on a source whose fundamental
    // decays with a 0.20 s time constant it recovers p = 0.703 against 0.735
    // warped, and the residual falls from 0.056 to 0.004.
    for (int point = 0; point < scanPoints; ++point)
    {
        const double position = std::pow(static_cast<double>(point)
            / static_cast<double>(scanPoints - 1), 1.24);
        SynthesisFrame frame;
        model.evaluate(static_cast<float>(position), frame);
        const double time = position * duration;
        for (std::size_t harmonic = 0; harmonic < harmonics; ++harmonic)
        {
            const double amplitude = frame.harmonicAmplitudes[harmonic];
            PartialFit& partial = partials[harmonic];
            if (amplitude > partial.peak)
            {
                partial = PartialFit {};
                partial.peak = amplitude;
                partial.startSeconds = time;
            }
            else if (partial.finished
                     || amplitude < 0.01 * partial.peak
                     || amplitude < 1.0e-6)
            {
                partial.finished = true;
                continue;
            }
            const double offset = time - partial.startSeconds;
            const double level = std::log(std::max(amplitude, 1.0e-12));
            partial.spanSeconds = offset;
            partial.sumTime += offset;
            partial.sumLevel += level;
            partial.sumTimeSquared += offset * offset;
            partial.sumTimeLevel += offset * level;
            ++partial.count;
        }
    }

    double sumX = 0.0;
    double sumY = 0.0;
    double sumXX = 0.0;
    double sumXY = 0.0;
    double sumYY = 0.0;
    int used = 0;
    for (std::size_t harmonic = 0; harmonic < harmonics; ++harmonic)
    {
        const PartialFit& partial = partials[harmonic];
        if (partial.peak < 1.0e-5 || partial.count < 4)
            continue;
        const double count = static_cast<double>(partial.count);
        const double denominator = count * partial.sumTimeSquared
            - partial.sumTime * partial.sumTime;
        if (!(denominator > 0.0))
            continue;
        const double rate = -(count * partial.sumTimeLevel
            - partial.sumTime * partial.sumLevel) / denominator;
        if (!(rate > 0.0) || rate * partial.spanSeconds < 0.69)
            continue;
        const double x = std::log(static_cast<double>(harmonic + 1));
        const double y = std::log(rate);
        sumX += x;
        sumY += y;
        sumXX += x * x;
        sumXY += x * y;
        sumYY += y * y;
        ++used;
    }
    if (used < 6)
        return 0.0f;
    const double count = static_cast<double>(used);
    const double denominator = count * sumXX - sumX * sumX;
    if (!(denominator > 0.0))
        return 0.0f;
    const double slope = (count * sumXY - sumX * sumY) / denominator;
    if (!std::isfinite(slope))
        return 0.0f;
    // Residual root-mean-square in log tau, computed from the accumulated
    // moments rather than from a second pass over the partials.
    const double intercept = (sumY - slope * sumX) / count;
    const double squared = sumYY - 2.0 * intercept * sumY
        - 2.0 * slope * sumXY + count * intercept * intercept
        + 2.0 * slope * intercept * sumX + slope * slope * sumXX;
    const double residual = std::sqrt(std::max(squared, 0.0) / count);
    if (!(residual < 0.15))
        return 0.0f;
    return static_cast<float>(std::clamp(slope, 0.0, 1.5));
}

// The per-slot release factors, built once at note-off and rebuilt only if
// Dissolve itself moves. This is where the step's pow() budget is spent: 284
// of them per note-off, against one multiply per slot per control period for
// the rest of the release.
//
// The law is tau_rel(k) = tau_rel(1) * (f_k/f_1)^-p, and what is stored is the
// *excess* rate each slot owes over partial 1, because the audio loop's single
// release scalar already runs every slot at partial 1's rate. Partial 1's
// factor is therefore exactly 1, the Dissolve time keeps its meaning, and the
// voice still retires on the slot the panel's number describes.
//
// Both clamps are load-bearing. The fast side bounds the rate ratio at 12, so
// a badly conditioned fit cannot turn Dissolve into a brickwall on the top of
// the spectrum. The slow side bounds it at 1, which is what makes partial 1
// the slowest slot: f_1 is the *played* fundamental while the Air centres are
// the model's own fixed 94 Hz to 13.6 kHz grid and the Bone centres are
// rootFrequencyHz * ratio, so slots below the played fundamental are routine.
// Three of sixteen Air bands sit below it at the root note itself and twelve
// of sixteen at MIDI 108 at full Body Lock; without the clamp the slowest of
// them would still be 5.8 dB down when the voice retires underneath it.
void NeuramarEngine::buildReleaseShape(
    Voice& voice, float releaseSeconds, float renderedFundamentalHz,
    const std::array<float, NeuralModel::airBandCount>& airCentresHz,
    const std::array<float, NeuralModel::boneModeCount>& boneCentresHz)
    const noexcept
{
    const bool firstRelease = !(voice.releaseShapeSeconds > 0.0f);
    voice.releaseShapeSeconds = releaseSeconds;
    // Dissolve is a duration to the retirement level, not a time constant, so
    // partial 1's time constant is that duration divided by the number of
    // nepers between unity and retirement - and then divided by the clock rate
    // scale, because the release is key-tracked by the same r^p the model
    // clock is. The excess every other slot owes is measured against that
    // key-tracked fundamental, so the whole release moves together and the
    // ratio between two slots stays a property of the source rather than of
    // the key.
    const float slowestTau = releaseSeconds
        / std::max(voice.clockRateScale, 1.0e-4f) / -std::log(retirementLevel);
    const float frameSeconds = static_cast<float>(controlPeriod_)
        / static_cast<float>(sampleRate_);
    const float fundamental = std::max(renderedFundamentalHz, 1.0f);
    const float perFrame = frameSeconds / std::max(slowestTau, 1.0e-6f);
    const auto excessDecay = [this, perFrame](float frequencyRatio) noexcept
    {
        const float rateRatio = std::clamp(
            std::pow(std::max(frequencyRatio, 1.0e-4f), dampingExponent_),
            1.0f, 12.0f);
        return std::exp(-(rateRatio - 1.0f) * perFrame);
    };
    const float fundamentalStretch = std::max(harmonicStretchRatio_.front(),
                                              1.0e-4f);
    for (std::size_t harmonic = 0; harmonic < renderedHarmonicCount; ++harmonic)
        voice.releaseSlotDecay[harmonic] = excessDecay(
            harmonicStretchRatio_[harmonic] / fundamentalStretch);
    for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
        voice.releaseSlotDecay[airOutputOffset + band] = excessDecay(
            airCentresHz[band] / fundamental);
    for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
        voice.releaseSlotDecay[boneOutputOffset + mode] = excessDecay(
            boneCentresHz[mode] / fundamental);
    if (firstRelease)
        voice.releaseSlotGain.fill(1.0f);
}

void NeuramarEngine::setParameters(const EngineParameters& parameters) noexcept
{
    parameters_.imprint.store(clampParameter(parameters.imprint, 0.0f, 1.0f, 1.0f),
                              std::memory_order_relaxed);
    parameters_.bodyLock.store(clampParameter(parameters.bodyLock, 0.0f, 1.0f, 0.65f),
                               std::memory_order_relaxed);
    parameters_.air.store(clampParameter(parameters.air, 0.0f, 1.0f, 0.35f),
                          std::memory_order_relaxed);
    parameters_.bone.store(clampParameter(parameters.bone, 0.0f, 1.0f, 0.30f),
                           std::memory_order_relaxed);
    parameters_.brightness.store(clampParameter(parameters.brightness, 0.0f, 1.0f, 0.50f),
                                 std::memory_order_relaxed);
    parameters_.evolutionRate.store(clampParameter(parameters.evolutionRate, 0.125f, 4.0f, 1.0f),
                                    std::memory_order_relaxed);
    parameters_.orbit.store(clampParameter(parameters.orbit, 0.0f, 1.0f, 0.15f),
                            std::memory_order_relaxed);
    parameters_.mutation.store(clampParameter(parameters.mutation, 0.0f, 1.0f, 0.10f),
                               std::memory_order_relaxed);
    parameters_.noise.store(clampParameter(parameters.noise, 0.0f, 1.0f, 0.0f),
                            std::memory_order_relaxed);
    parameters_.attackSeconds.store(clampParameter(parameters.attackSeconds, 0.0f, 10.0f, 0.0f),
                                    std::memory_order_relaxed);
    parameters_.releaseSeconds.store(clampParameter(parameters.releaseSeconds, 0.005f, 20.0f, 0.35f),
                                     std::memory_order_relaxed);
    parameters_.spread.store(clampParameter(parameters.spread, 0.0f, 1.0f, 0.35f),
                             std::memory_order_relaxed);
    parameters_.rootOffsetSemitones.store(
        clampParameter(parameters.rootOffsetSemitones, -12.0f, 12.0f, 0.0f),
        std::memory_order_relaxed);
    parameters_.outputGain.store(clampParameter(parameters.outputGain, 0.0f, 2.0f, 0.72f),
                                 std::memory_order_relaxed);
    parameters_.stretch.store(clampParameter(parameters.stretch, 0.0f, 2.0f, 1.0f),
                              std::memory_order_relaxed);
    parameters_.formantShiftSemitones.store(
        clampParameter(parameters.formantShiftSemitones, -24.0f, 24.0f, 0.0f),
        std::memory_order_relaxed);
    parameters_.touch.store(clampParameter(parameters.touch, 0.0f, 1.0f, 0.0f),
                            std::memory_order_relaxed);
    parameters_.registerTilt.store(
        clampParameter(parameters.registerTilt, -1.0f, 1.0f, 0.0f),
        std::memory_order_relaxed);
}

EngineParameters NeuramarEngine::loadParameters() const noexcept
{
    EngineParameters result;
    result.imprint = parameters_.imprint.load(std::memory_order_relaxed);
    result.bodyLock = parameters_.bodyLock.load(std::memory_order_relaxed);
    result.air = parameters_.air.load(std::memory_order_relaxed);
    result.bone = parameters_.bone.load(std::memory_order_relaxed);
    result.brightness = parameters_.brightness.load(std::memory_order_relaxed);
    result.evolutionRate = parameters_.evolutionRate.load(std::memory_order_relaxed);
    result.orbit = parameters_.orbit.load(std::memory_order_relaxed);
    result.mutation = parameters_.mutation.load(std::memory_order_relaxed);
    result.noise = parameters_.noise.load(std::memory_order_relaxed);
    result.attackSeconds = parameters_.attackSeconds.load(std::memory_order_relaxed);
    result.releaseSeconds = parameters_.releaseSeconds.load(std::memory_order_relaxed);
    result.spread = parameters_.spread.load(std::memory_order_relaxed);
    result.rootOffsetSemitones = parameters_.rootOffsetSemitones.load(std::memory_order_relaxed);
    result.outputGain = parameters_.outputGain.load(std::memory_order_relaxed);
    result.stretch = parameters_.stretch.load(std::memory_order_relaxed);
    result.formantShiftSemitones = parameters_.formantShiftSemitones.load(
        std::memory_order_relaxed);
    result.touch = parameters_.touch.load(std::memory_order_relaxed);
    result.registerTilt = parameters_.registerTilt.load(
        std::memory_order_relaxed);
    return result;
}

void NeuramarEngine::noteOn(int midiNote, float velocity) noexcept
{
    const auto* model = model_.load(std::memory_order_acquire);
    if (model == nullptr || midiNote < 0 || midiNote > 127)
        return;

    velocity = clampParameter(velocity, 0.0f, 1.0f, 0.0f);
    if (velocity <= 0.0f)
    {
        noteOff(midiNote);
        return;
    }

    Voice* selected = nullptr;
    std::size_t selectedIndex = 0;
    for (std::size_t index = 0; index < voices_.size(); ++index)
    {
        if (!voices_[index].active)
        {
            selected = &voices_[index];
            selectedIndex = index;
            break;
        }
    }

    if (selected == nullptr)
    {
        selectedIndex = 0;
        for (std::size_t index = 1; index < voices_.size(); ++index)
        {
            const auto& candidate = voices_[index];
            const auto& current = voices_[selectedIndex];
            if ((candidate.releasing && !current.releasing)
                || (candidate.releasing == current.releasing
                    && candidate.ageStamp < current.ageStamp))
                selectedIndex = index;
        }
        selected = &voices_[selectedIndex];
    }

    if (selected->active)
        beginFadeTail(selectedIndex);
    selected->clear();
    selected->active = true;
    selected->midiNote = midiNote;
    selected->velocity = velocity;
    selected->ageStamp = ++ageCounter_;
    selected->controlCountdown = 0;

    const auto mixHash = [](std::uint32_t value) noexcept
    {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        return value != 0u ? value : 1u;
    };
    const auto noteHash = mixHash(
        static_cast<std::uint32_t>(midiNote + 1) * 0x9e3779b9u);
    const auto voiceHash = mixHash(
        noteHash ^ static_cast<std::uint32_t>(selected->ageStamp));
    const float mutationAmount = parameters_.mutation.load(
        std::memory_order_relaxed);
    // With Mutation and model-space Noise both at zero, the same note restarts
    // every oscillator and the audible Air stream identically, independent of
    // prior voice allocation. Positive Mutation gives Air a new voice stream;
    // positive Noise independently selects a fresh latent trajectory below.
    const auto airNoiseHash = mutationAmount <= 0.0f ? noteHash : voiceHash;
    for (std::size_t band = 0; band < selected->airNoiseStates.size(); ++band)
    {
        selected->airNoiseStates[band] = mixHash(
            airNoiseHash
                ^ (0x9e3779b9u * static_cast<std::uint32_t>(band + 1)));
        // The stereo side realisation must be uncorrelated with the centred
        // one, but just as reproducible.
        selected->airSideNoiseStates[band] = mixHash(
            airNoiseHash
                ^ (0x85ebca6bu * static_cast<std::uint32_t>(band + 1))
                ^ 0x165667b1u);
    }
    selected->mutationOffset = 2.0f
        * static_cast<float>(voiceHash & 0xffffu) / 65535.0f - 1.0f;
    // What differs between two nominally identical hand strikes is the energy
    // delivered, so Mutation varies the excitation strength and lets every
    // consequence of a harder or softer strike follow from it. On a real
    // instrument level and brightness co-vary, because a harder strike shortens
    // the contact time and pushes the excitation spectrum's corner up; that
    // coupling already exists here in the Touch path, so writing the jitter
    // into the voice's own velocity - before velocityGain below and before
    // touchTilt and touchAirGain read it in updateVoiceControl() - reproduces
    // the correlated level, tilt and Air variation without drawing anything
    // new. The alternative, a per-take output gain, would move level alone and
    // leave the timbre of a hard strike identical to a soft one.
    //
    // 0.75 is set by the acceptance target rather than chosen freely. With
    // velocityGain = v(0.72 + 0.28*sqrt(v)) a draw of +/-0.09 about the
    // reference velocity spans about 2.2 dB end to end, and a uniform draw of
    // that width has a standard deviation near 0.64 dB, which is the middle of
    // the 1.5-3 dB peak variation a hand-struck acoustic instrument shows. At
    // 0.55 the same arithmetic lands at 1.6 dB end to end, under that floor.
    // The 0.05 floor is here to stop the perturbation driving a note to
    // silence, so it belongs to the perturbation rather than to the velocity.
    // Applied unconditionally it raised every velocity under 0.05 whether or
    // not anything had been added: MIDI velocity 1 arrived as 0.05, which is
    // 16.5 dB of velocityGain the player never asked for, and Mutation at zero
    // stopped being behaviour-preserving. Floor at the note's own velocity
    // when that is already below the bound, so a soft note keeps its level and
    // the perturbation still cannot push it under one.
    const float perturbed = selected->velocity
                          + 0.75f * mutationAmount * selected->mutationOffset;
    selected->velocity = std::clamp(
        perturbed, std::min(selected->velocity, 0.05f), 1.0f);
    selected->velocityGain = selected->velocity
        * (0.72f + 0.28f * std::sqrt(selected->velocity));
    // These sinusoidal variation shapes depend only on the voice identity, so
    // one evaluation at note-on replaces the same 72 sines every control frame
    // in the reference-target and Air paths.
    for (std::size_t harmonic = 0; harmonic < NeuralModel::harmonicCount; ++harmonic)
        selected->harmonicVariationSin[harmonic] = std::sin(
            2.173f * static_cast<float>(harmonic + 1)
            + 3.7f * selected->mutationOffset);
    for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
        selected->airVariationSin[band] = std::sin(
            1.37f * static_cast<float>(band + 1) + 5.0f * selected->mutationOffset);

    // Model-space NOISE owns a separate deterministic domain. It neither
    // consumes the Air PRNG nor reuses Mutation's static scalar.
    const auto latentPhaseAHash = mixHash(voiceHash ^ 0xa511e9b3u);
    const auto latentPhaseBHash = mixHash(voiceHash ^ 0x63d83595u);
    const auto latentRateAHash = mixHash(voiceHash ^ 0xc2b2ae35u);
    const auto latentRateBHash = mixHash(voiceHash ^ 0x27d4eb2fu);
    constexpr float inverseU32 = 1.0f / 4294967295.0f;
    selected->latentPhaseA = static_cast<float>(latentPhaseAHash) * inverseU32;
    selected->latentPhaseB = static_cast<float>(latentPhaseBHash) * inverseU32;
    selected->latentRateAHertz = 0.07f
        + 0.10f * static_cast<float>(latentRateAHash) * inverseU32;
    selected->latentRateBHertz = 0.19f
        + 0.18f * static_cast<float>(latentRateBHash) * inverseU32;
    const float phaseMutation = mutationAmount * selected->mutationOffset;
    const float correctedRootMidi = static_cast<float>(model->metadata_.rootMidiNote)
        + parameters_.rootOffsetSemitones.load(std::memory_order_relaxed);
    // Pan is a function of the played note alone, fixed here and never revised
    // for as long as the voice sounds. On a piano, harp, marimba or guitar the
    // sounding elements are laid out monotonically in pitch across the width of
    // the radiating body, and a string that is already vibrating does not move
    // when another one is struck; both properties follow from placing a voice
    // by its own pitch instead of by its rank among whatever else happens to be
    // sounding. Two octaves either side of the corrected root reaches the edge
    // of the image. Spread scales this and the per-note jitter is added to it
    // in updateVoiceControl(), so a single note sits centred when it is the
    // root note or when Spread is zero, and off centre otherwise.
    selected->pan = (static_cast<float>(midiNote) - correctedRootMidi) / 24.0f;
    const float phaseRatio = std::clamp(
        std::exp2((static_cast<float>(midiNote) - correctedRootMidi) / 12.0f)
            * (1.0f + 0.0012f * phaseMutation),
        0.0078125f, 128.0f);
    const float bodyLock = parameters_.bodyLock.load(std::memory_order_relaxed);
    for (std::size_t harmonic = 0;
         harmonic < selected->harmonicPhases.size(); ++harmonic)
    {
        const float harmonicNumber = static_cast<float>(harmonic + 1);
        const bool hasPitchFollowingPhase = harmonic < NeuralModel::harmonicCount;
        const float pitchFollowingPhase = hasPitchFollowingPhase
            ? model->initialHarmonicPhases_[harmonic]
            : initialPhaseAt(*model, harmonicNumber * phaseRatio);
        const float sourceCoordinate = harmonicNumber * phaseRatio;
        const bool hasBodyPhase = sourceCoordinate > 0.0f
            && sourceCoordinate
                <= static_cast<float>(NeuralModel::harmonicCount) + 0.999f;
        const float bodyPhase = hasBodyPhase
            ? initialPhaseAt(*model, sourceCoordinate)
            : pitchFollowingPhase;
        selected->harmonicPhases[harmonic] = wrapUnit(
            interpolatePhaseShortest(pitchFollowingPhase, bodyPhase, bodyLock)
            + 0.003f * phaseMutation * static_cast<float>(harmonic));
    }
    for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
        selected->bonePhases[mode] = wrapUnit(
            model->initialBonePhases_[mode] + 0.007f * phaseMutation);
}

void NeuramarEngine::noteOff(int midiNote) noexcept
{
    Voice* oldestHeld = nullptr;
    for (auto& voice : voices_)
    {
        if (!voice.active || voice.releasing || voice.midiNote != midiNote)
            continue;
        if (oldestHeld == nullptr || voice.ageStamp < oldestHeld->ageStamp)
            oldestHeld = &voice;
    }
    if (oldestHeld != nullptr)
        oldestHeld->releasing = true;
}

void NeuramarEngine::allNotesOff() noexcept
{
    for (auto& voice : voices_)
        if (voice.active)
            voice.releasing = true;
}

void NeuramarEngine::allSoundOff() noexcept
{
    for (auto& voice : voices_)
        voice.clear();
    for (auto& tail : fadeTails_)
        tail.clear();
}

void NeuramarEngine::beginFadeTail(std::size_t voiceIndex) noexcept
{
    if (voiceIndex >= voices_.size())
        return;

    const auto& voice = voices_[voiceIndex];
    auto& tail = fadeTails_[voiceIndex];

    // A slot stolen twice inside one fade window still owes the listener the
    // remainder of the first tail, so the value the running tail was about to
    // emit is carried into a fresh window rather than discarded. Because the
    // carry is the *emitted* value, the hand-off steps by exactly
    // voice.lastLeft wherever in the fade it lands, and the running tail's own
    // decay is applied before the sum rather than compensated for afterwards.
    //
    // What a steal must not do is push the deadline out. Restarting a
    // full-length fade on every steal makes the tail an integrator whose value
    // grows with the length of a note-on burst rather than a hand-off bounded
    // by one voice's level, and a dense enough burst then drives the mix into
    // the finite-output guard. So the window is re-cut across the samples the
    // first steal already budgeted: all the energy stacked into one slot still
    // dies within one fade of that first steal.
    const float window = tail.remaining > 0
        ? fadeTailWindow(tail.position)
        : 0.0f;

    // Once the running window has closed to nothing the old tail contributes
    // nothing to carry, so a fresh full-length fade is both safe and better:
    // the 50x attenuation on anything carried across it is what keeps repeated
    // late steals from accumulating.
    const int remaining = (tail.remaining > 0 && window > 0.02f)
        ? tail.remaining
        : fadeTailSamples_;

    tail.left = std::clamp(tail.left * window + voice.lastLeft,
                           -maximumFadeTailLevel, maximumFadeTailLevel);
    tail.right = std::clamp(tail.right * window + voice.lastRight,
                            -maximumFadeTailLevel, maximumFadeTailLevel);
    tail.position = 0.0f;
    tail.positionStep = 1.0f / static_cast<float>(remaining);
    tail.remaining = remaining;
}

float NeuramarEngine::nextNoise(std::uint32_t& state) noexcept
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state) * (2.0f / 4294967295.0f) - 1.0f;
}

void NeuramarEngine::updateVoiceControl(Voice& voice, const NeuralModel& model,
                                        const EngineParameters& parameters) noexcept
{
    const bool firstControlFrame = voice.modelTimeSeconds <= 0.0;
    const float correctedRootMidi = static_cast<float>(model.metadata_.rootMidiNote)
        + parameters.rootOffsetSemitones;
    const float noteDistance = static_cast<float>(voice.midiNote) - correctedRootMidi;
    const float basicRatio = std::exp2(noteDistance / 12.0f);
    const float mutationDetune = 1.0f + 0.0012f * parameters.mutation
        * voice.mutationOffset;
    voice.transpositionRatio = std::clamp(basicRatio * mutationDetune,
                                          0.0078125f, 128.0f);

    // Key-track the model clock. Without this every learned trajectory
    // replays in absolute seconds at every key, so a plucked or struck memory
    // rings for very nearly the source's wall-clock duration two octaves up:
    // T20(81)/T20(57) measures 0.978 where the source's own damping law
    // predicts 0.354.
    //
    // The law is the one fitDampingExponent() already fitted:
    // tau(f) = tau_1 (f/f_1)^-p is a statement about *frequency*, not about
    // harmonic number, so a note played at transposition ratio r has
    // fundamental f_1 r and the consistent decay time tau_1 r^-p. A decay time
    // is the reciprocal of a rate, so the clock that replays the trajectory
    // runs r^p times faster, clamped to a factor of four either way so a
    // badly-conditioned fit can neither make a high note vanish before it
    // sounds nor stretch a low one into a drone. At the root r is exactly 1
    // and the render is bit-identical, and a source that fits p = 0 keeps a
    // pitch-invariant decay at every key - which is the whole reason the
    // exponent is fitted from the sound the user dropped in rather than drawn.
    //
    // This clock is the engine's only time variable, so key-tracking it also
    // key-tracks the learned onset and any learned pitch contour. That is
    // accepted deliberately, not overlooked, and the reason is measured. A
    // player's vibrato lives on a *driven* source, and a driven source has no
    // free decay to fit: the sustained fixture with a 5 Hz, 0.35 semitone
    // vibrato fits p = 0.00000 exactly and is left on the absolute clock. The
    // fit only survives on a source that is genuinely dying, and the faster it
    // dies the more it survives - the tau_1 = 0.9 s decaying fixture with the
    // same vibrato also fits p = 0, and the tau_1 = 0.20 s one fits 0.79 but
    // has fallen 20 dB in about 0.16 s at MIDI 81, which is roughly two cycles
    // of the 14 Hz its 5 Hz vibrato would become. The alternative - a second,
    // un-tracked clock for the pitch trajectory - costs a second decoder
    // evaluation per control frame, 3.26 us here, or 1.3% of a core at
    // sixteen voices, to move something that is over before it is heard.
    // Key-tracking the onset needs no apology at all: a struck string does
    // speak faster at the top of its compass.
    voice.clockRateScale = dampingExponent_ > 0.0f
        ? std::clamp(std::pow(voice.transpositionRatio, dampingExponent_),
                     0.25f, 4.0f)
        : 1.0f;
    // Dissolve is key-tracked by the same law, so the top of the keyboard also
    // damps faster than the bottom. tau_rel(1) = releaseSeconds * r^-p is the
    // tau_1 r^-p the clock rate above is derived from; the direction is easy
    // to get backwards, and multiplying the time constant by r^p instead of
    // dividing puts every key on the wrong side of the root. Partial 1 is what
    // the audio loop's single release scalar carries, and buildReleaseShape()
    // takes the same scale so the excess each other slot owes is measured
    // against the key-tracked fundamental rather than the panel's number.
    //
    // The rate has to be per voice, so it moves here from the one number the
    // block used to build. The only consequence is that a Dissolve automated
    // mid-note now reaches the envelope at the next control frame rather than
    // the next block, which is at most 4 ms on a parameter whose smallest
    // setting is 5 ms.
    voice.releaseMultiplier = std::exp(
        std::log(retirementLevel) * inverseSampleRate_ * voice.clockRateScale
        / std::max(parameters.releaseSeconds, 0.005f));

    const float duration = std::max(model.metadata_.durationSeconds, 0.001f);
    // Except for the first sounding sample, predict one control interval
    // ahead. The per-sample ramp then arrives at that target at the time it
    // describes instead of reproducing every learned gesture 4 ms late.
    const double evaluationModelTime = voice.modelTimeSeconds
        + (firstControlFrame ? 0.0
            : static_cast<double>(parameters.evolutionRate
                * voice.clockRateScale
                * static_cast<float>(controlPeriod_ - 1)) / sampleRate_);
    const float loopStart = std::clamp(model.metadata_.loopStartSeconds,
                                       0.0f, duration);
    const float loopEnd = std::clamp(model.metadata_.loopEndSeconds,
                                     loopStart + 0.001f, duration);
    const float loopLength = std::max(loopEnd - loopStart, 0.001f);
    const double oneShotTime = std::min(evaluationModelTime,
                                        static_cast<double>(duration));
    // Orbit reads the loop region forward only. The triangle fold this
    // replaced played every second leg backwards, which turns a decaying
    // trajectory into a rising one - a negative-damping segment, the one thing
    // a passive resonator cannot produce - and made a held note exactly
    // periodic at twice the loop length.
    //
    // Successive passes start from a different point in the region, advanced by
    // (phi - 1) of the loop length each time. The golden ratio is the advance
    // that maximises the smallest gap between successive offsets, so no short
    // lag ever lines up and the sustain is quasi-periodic rather than periodic.
    //
    // No crossfade is written here. Every wrap is a step in the control-rate
    // amplitude *targets*, and the ramp below already walks each amplitude
    // linearly from its old value to its new one over one control period. That
    // is an equal-gain crossfade of one control period, which is the correct
    // weighting: voice.harmonicPhases advance from phaseStep alone and never
    // consult the model clock, so both sides of a wrap drive the same
    // oscillators at the same phases and are perfectly correlated. Equal-power
    // weights of 1/sqrt(2) would raise the level 3.01 dB at the midpoint of
    // every wrap, which is a wrap-rate pumping artefact of exactly the kind
    // this exists to remove.
    constexpr double goldenAdvance = 0.6180339887498949;
    double orbitTime = evaluationModelTime;
    double loopOffsetSeconds = 0.0;
    if (orbitTime > loopStart)
    {
        const double length = static_cast<double>(loopLength);
        const double elapsed = orbitTime - loopStart;
        const double passOffset = std::fmod(
            std::floor(elapsed / length) * goldenAdvance, 1.0) * length;
        loopOffsetSeconds = std::fmod(
            std::fmod(elapsed, length) + passOffset, length);
        orbitTime = loopStart + loopOffsetSeconds;
    }
    // Mutation used to add a per-voice start-time offset here. It bought its
    // variation by deleting transient: the clamp below is one-sided over the
    // first mutation * |offset| * 0.018 * duration seconds, so a positive
    // offset skipped into the attack and a negative one did nothing, and the
    // first 10 ms of a take relative to its own peak moved almost 1 dB between
    // takes at the shipping Mutation. Excitation strength varies at note-on
    // instead; nothing per-voice displaces the read position.
    const float effectiveTime = static_cast<float>(std::clamp(
        oneShotTime + parameters.orbit * (orbitTime - oneShotTime),
        0.0, static_cast<double>(duration)));
    // Divide out the loop region's own level trend, in proportion to how much
    // of the read position Orbit is actually supplying. The region falls
    // 2.7 dB across its length on the decay fixture, so without this every
    // wrap would step the level back up by that much - which is the pumping,
    // not a property of the source. Detrending leaves the fine structure
    // alone, and is the only form of level correction the parameter can
    // express: Orbit is a time blend, oneShotTime + orbit * (orbitTime -
    // oneShotTime), and a level hold is not expressible inside it.
    const float loopDetrendGain = std::exp(
        -loopLevelSlopePerSecond_ * parameters.orbit
        * static_cast<float>(loopOffsetSeconds));

    SynthesisFrame frame;
    const float normalisedTime = effectiveTime / duration;
    if (parameters.noise > 0.0f)
    {
        const std::uint64_t predictedSample = voice.renderedSampleCount
            + static_cast<std::uint64_t>(
                firstControlFrame ? 0 : controlPeriod_ - 1);
        const double latentTimeSeconds = static_cast<double>(predictedSample)
            / sampleRate_;
        const float phaseA = static_cast<float>(std::fmod(
            static_cast<double>(voice.latentPhaseA)
                + static_cast<double>(voice.latentRateAHertz)
                    * latentTimeSeconds,
            1.0));
        const float phaseB = static_cast<float>(std::fmod(
            static_cast<double>(voice.latentPhaseB)
                + static_cast<double>(voice.latentRateBHertz)
                    * latentTimeSeconds,
            1.0));
        const float latentCoordinate = parameters.noise * std::clamp(
            0.62f * sine(phaseA) + 0.38f * sine(phaseB),
            -1.0f, 1.0f);
        model.evaluate(normalisedTime, latentCoordinate, frame);
    }
    else
    {
        model.evaluate(normalisedTime, frame);
    }
    // Applying the detrend to the decoded frame, rather than to the finished
    // targets, keeps it out of the register compensation: that gain is a ratio
    // of two powers taken from this same frame, so both sides move together and
    // it is left exactly where it was.
    if (loopDetrendGain != 1.0f)
    {
        for (float& amplitude : frame.harmonicAmplitudes)
            amplitude *= loopDetrendGain;
        for (float& amplitude : frame.airAmplitudes)
            amplitude *= loopDetrendGain;
        for (float& amplitude : frame.boneAmplitudes)
            amplitude *= loopDetrendGain;
    }

    // Touch turns MIDI velocity into an excitation strength rather than a
    // volume knob: a harder note leans the harmonic tilt brighter and lets
    // more of the learned Air through, exactly as a stronger physical
    // excitation would. 0.72 is the reference "mezzo-forte" velocity, so the
    // learned timbre is reproduced unchanged there for any Touch depth.
    constexpr float touchReferenceVelocity = 0.72f;
    const float touchOffset = voice.velocity - touchReferenceVelocity;
    const float touchTilt = parameters.touch * 0.55f * touchOffset;
    const float touchAirGain = std::clamp(
        1.0f + parameters.touch * 1.35f * touchOffset, 0.0f, 3.0f);

    // Three contributions shape the spectrum: Brightness is the player's
    // control, Touch is the performance, and Register is key tracking - a note
    // transposed far above its source is darkened and one transposed far below
    // is opened up. Only the register term is left out of the reference below,
    // which is what makes it change tone without changing loudness.
    const float referenceTilt = (parameters.brightness - 0.5f) * 1.2f
        + touchTilt;
    const float registerOctaves = std::log2(
        std::max(voice.transpositionRatio, 1.0e-4f));
    const float registerContribution = std::clamp(
        -parameters.registerTilt * 0.30f * registerOctaves, -0.45f, 0.45f);
    const float brightnessTilt = referenceTilt + registerContribution;
    const float characteristic = parameters.imprint;
    // Formant moves the learned resonant body in absolute frequency without
    // moving the played pitch. Dividing the envelope lookup coordinate by the
    // shift is what reads the learned envelope one shift lower, which places
    // its resonances one shift higher in the render.
    const float formantScale = std::exp2(
        parameters.formantShiftSemitones * (1.0f / 12.0f));
    const float envelopeRatio = std::clamp(
        voice.transpositionRatio / formantScale, 0.0078125f, 512.0f);
    const bool needsBodyEnvelope = parameters.bodyLock > 0.0f
        && characteristic > 0.0f;
    spectral::ShapePreservingEnvelope<NeuralModel::harmonicCount> bodyEnvelope;
    std::array<float, NeuralModel::harmonicCount> sourceFilterEnvelope {};
    if (needsBodyEnvelope)
    {
        sourceFilterEnvelope = makeSourceFilterEnvelope(
            frame.harmonicAmplitudes);
        bodyEnvelope.prepare(sourceFilterEnvelope);
    }
    const float fundamentalReference = std::max(frame.harmonicAmplitudes.front(),
                                                1.0e-5f);

    // The reference targets always evaluate the brightness tilt at integer
    // harmonic numbers, so the power curve is rebuilt only when Brightness
    // actually moves instead of on every control frame.
    if (brightnessTilt != voice.cachedBrightnessTilt)
    {
        voice.cachedBrightnessTilt = brightnessTilt;
        for (std::size_t harmonic = 0; harmonic < renderedHarmonicCount; ++harmonic)
            voice.brightnessTiltTable[harmonic] = std::pow(
                static_cast<float>(harmonic + 1), brightnessTilt);
    }
    if (referenceTilt != voice.cachedReferenceTilt)
    {
        voice.cachedReferenceTilt = referenceTilt;
        for (std::size_t harmonic = 0;
             harmonic < NeuralModel::harmonicCount; ++harmonic)
            voice.referenceTiltTable[harmonic] = std::pow(
                static_cast<float>(harmonic + 1), referenceTilt);
    }

    const float availableMappedHarmonics =
        (static_cast<float>(NeuralModel::harmonicCount) + 0.999f)
        / envelopeRatio;
    // Retire a previous contraction only after its slots are actually silent.
    // A control update can arrive early, so the presence of another update
    // alone does not prove a full ramp period elapsed.
    bool retiringHarmonicsAreSilent = true;
    for (std::size_t harmonic = voice.targetHarmonicCount;
         harmonic < voice.activeHarmonicCount; ++harmonic)
    {
        retiringHarmonicsAreSilent = retiringHarmonicsAreSilent
            && voice.amplitudes[harmonic] <= 1.0e-6f;
    }
    if (!firstControlFrame && retiringHarmonicsAreSilent
        && voice.targetHarmonicCount < voice.activeHarmonicCount)
        voice.activeHarmonicCount = voice.targetHarmonicCount;

    std::size_t desiredHarmonicCount = NeuralModel::harmonicCount;
    if (availableMappedHarmonics
        > static_cast<float>(NeuralModel::harmonicCount))
    {
        desiredHarmonicCount = std::min(
            renderedHarmonicCount,
            static_cast<std::size_t>(std::floor(availableMappedHarmonics)));
    }
    voice.targetHarmonicCount = desiredHarmonicCount;
    if (desiredHarmonicCount > voice.activeHarmonicCount)
    {
        if (!firstControlFrame)
        {
            std::fill(voice.amplitudes.begin()
                          + static_cast<std::ptrdiff_t>(voice.activeHarmonicCount),
                      voice.amplitudes.begin()
                          + static_cast<std::ptrdiff_t>(desiredHarmonicCount),
                      0.0f);
        }
        voice.activeHarmonicCount = desiredHarmonicCount;
    }

    std::array<float, renderAmplitudeCount> targets {};
    std::array<float, NeuralModel::harmonicCount> referenceTargets {};
    const bool capacityLimited = desiredHarmonicCount
            == renderedHarmonicCount
        && envelopeRatio
                * static_cast<float>(renderedHarmonicCount)
            < static_cast<float>(NeuralModel::harmonicCount - 1);
    // The Body-Locked spectral coordinate is always a fixed multiple of the
    // harmonic index, so pow(index * scale, tilt) factors into one cached
    // table lookup and one scalar. That removes a pow() and a sin() from every
    // rendered harmonic of every control frame.
    const float bodyCoordinateScale = 1.0f
        + parameters.bodyLock * (envelopeRatio - 1.0f);
    const float evidenceTiltScale = std::pow(
        std::max(bodyCoordinateScale, 1.0e-6f), brightnessTilt);
    const bool harmonicsVary = parameters.mutation > 0.0f;
    const float variationDepth = parameters.mutation * 0.045f;
    constexpr float inverseTwoPi = 1.0f / twoPi;
    const float renderedFundamental = model.metadata_.rootFrequencyHz
        * voice.transpositionRatio * frame.pitchRatio;
    double renderedPower = 0.0;
    for (std::size_t harmonic = 0;
         harmonic < desiredHarmonicCount; ++harmonic)
    {
        const float harmonicNumber = static_cast<float>(harmonic + 1);
        const bool hasPitchFollowing = harmonic < NeuralModel::harmonicCount;
        const float pitchFollowing = hasPitchFollowing
            ? frame.harmonicAmplitudes[harmonic] : 0.0f;
        const float sourceCoordinate = harmonicNumber * envelopeRatio;
        const bool hasBodyEvidence = sourceCoordinate > 0.0f
            && sourceCoordinate
                <= static_cast<float>(NeuralModel::harmonicCount) + 0.999f;
        float excitationResidual = 1.0f;
        if (hasPitchFollowing && needsBodyEnvelope)
        {
            const float envelopeAtHarmonic = sourceFilterEnvelope[harmonic];
            excitationResidual = envelopeAtHarmonic > 1.0e-7f
                ? pitchFollowing / envelopeAtHarmonic : 0.0f;
            excitationResidual = std::clamp(excitationResidual, 0.0f, 4.0f);
        }
        float bodyLocked = needsBodyEnvelope
            ? excitationResidual * bodyEnvelope.sample(sourceCoordinate)
            : pitchFollowing;
        // Keep a modest pitch anchor when a fixed-frequency lookup would
        // otherwise erase the played fundamental. Other coordinates above the
        // observed envelope fade out instead of inventing upper-band energy.
        if (harmonic == 0)
            bodyLocked = std::max(bodyLocked, 0.25f * pitchFollowing);
        // The excitation/filter factorisation keeps odd/even and reed/bow
        // character attached to harmonic index while moving only the smooth
        // resonant envelope in absolute frequency.  For virtual low-register
        // harmonics no excitation evidence exists, so use the neutral envelope
        // rather than fabricating a repeated residual pattern.
        const float learned = hasPitchFollowing
            ? blendMagnitude(pitchFollowing, bodyLocked, parameters.bodyLock)
            : parameters.bodyLock * bodyLocked;
        const float neutral = hasPitchFollowing
            ? fundamentalReference * inverseHarmonicRolloff_[harmonic]
            : 0.0f;
        const float spectralCoordinate = hasBodyEvidence
            ? harmonicNumber * bodyCoordinateScale : harmonicNumber;
        const float tilt = spectralCoordinate < 1.0f
            ? 1.0f
            : (hasBodyEvidence ? evidenceTiltScale : 1.0f)
                * voice.brightnessTiltTable[harmonic];
        const float variation = harmonicsVary
            ? 1.0f + variationDepth * sine(
                  (2.173f * spectralCoordinate
                   + 3.7f * voice.mutationOffset) * inverseTwoPi)
            : 1.0f;
        float target = std::clamp(
            (neutral + characteristic * (learned - neutral)) * tilt * variation,
            0.0f, 2.0f);
        if (capacityLimited
            && harmonicNumber
                > static_cast<float>(renderedHarmonicCount - 12))
        {
            const float position = (harmonicNumber
                - static_cast<float>(renderedHarmonicCount - 12)) / 12.0f;
            target *= 0.5f + 0.5f * std::cos(pi * position);
        }
        // Fold the anti-alias taper into the ramped amplitude. The audio loop
        // then needs no per-sample band-limiting arithmetic at all, and every
        // harmonic that the taper silences drops out of the render entirely.
        target *= coreNyquistGain(
            renderedFundamental * harmonicStretchRatio_[harmonic],
            coreNyquistLimitHz_, coreNyquistFadeScale_);
        targets[harmonic] = target;
        renderedPower += static_cast<double>(target) * target;

        if (hasPitchFollowing)
        {
            const float referenceNeutral = fundamentalReference
                * inverseHarmonicRolloff_[harmonic];
            const float referenceTiltValue = voice.referenceTiltTable[harmonic];
            const float referenceVariation = 1.0f
                + parameters.mutation * 0.045f
                    * voice.harmonicVariationSin[harmonic];
            referenceTargets[harmonic] = std::clamp(
                (referenceNeutral + characteristic
                    * (pitchFollowing - referenceNeutral))
                    * referenceTiltValue * referenceVariation,
                0.0f, 2.0f);
        }
    }

    // Factor register-dependent harmonic shape from overall Core power. This
    // compensates both the denser fixed-envelope grid on low notes and the
    // harmonics safely removed near Nyquist on high notes. The upper bound is
    // deliberately modest: unavailable spectrum must never become a huge gain
    // boost, and all changes continue to use the normal control-rate ramp.
    //
    // The reference deliberately covers only as many partials as this note
    // actually renders. Measuring against the model's full bank instead asks a
    // high note - whose upper partials do not exist, because they are past the
    // anti-alias limit or past the observed Body-Lock envelope - to make up
    // that missing energy from the few partials it has left. The demand grows
    // without bound, so the gain saturated at its ceiling somewhere around
    // root+17 semitones and the whole upper keyboard faded out behind it:
    // 3.9 dB across MIDI 12-108 at the default Body Lock and 14.4 dB at full
    // Body Lock. Like-for-like leaves the low register's genuine grid-density
    // difference, which is what this exists to correct, and makes the bound
    // below a safety limit rather than a routine constraint.
    double referencePower = 0.0;
    const float referenceFundamental = model.metadata_.rootFrequencyHz
        * frame.pitchRatio;
    const std::size_t comparableHarmonics = std::min(
        desiredHarmonicCount, NeuralModel::harmonicCount);
    for (std::size_t harmonic = 0; harmonic < comparableHarmonics; ++harmonic)
    {
        const float antialias = coreNyquistGain(
            referenceFundamental * harmonicStretchRatio_[harmonic],
            coreNyquistLimitHz_, coreNyquistFadeScale_);
        const double amplitude = referenceTargets[harmonic] * antialias;
        referencePower += amplitude * amplitude;
    }
    float registerGain = 1.0f;
    if (referencePower > 1.0e-12 && renderedPower > 1.0e-12)
    {
        registerGain = std::clamp(static_cast<float>(std::sqrt(
            referencePower / renderedPower)), 0.25f, 4.0f);
    }
    for (std::size_t harmonic = 0;
         harmonic < desiredHarmonicCount; ++harmonic)
        targets[harmonic] *= registerGain;

    // Air and Bone are resonant body layers, so the Formant shift moves them
    // in absolute frequency together with the Body-Locked Core envelope.
    const float spectralScale = std::pow(
        voice.transpositionRatio * frame.pitchRatio,
        1.0f - parameters.bodyLock) * formantScale;
    const float brightnessScale = std::exp2(parameters.brightness - 0.5f);
    std::array<float, NeuralModel::airBandCount> airCentresHz {};
    bool airSounding = false;
    for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
    {
        const std::size_t output = airOutputOffset + band;
        const float variation = 1.0f + parameters.mutation * 0.12f
            * voice.airVariationSin[band];
        const float desiredFrequency = model.airCentreFrequenciesHz_[band]
            * spectralScale * brightnessScale;
        const float edgeGain = std::clamp(
            (desiredFrequency - 20.0f) / 30.0f, 0.0f, 1.0f)
            * std::clamp((airEdgeLimitHz_ - desiredFrequency)
                             / airEdgeFadeHz_,
                         0.0f, 1.0f);
        // Air and Bone deliberately do not take the register compensation.
        // That gain does not scale the Core, it normalises it: after the loop
        // above, Core power is referencePower, which barely depends on the
        // played note. Multiplying an un-normalised layer by the same factor
        // therefore makes the noise-to-tone ratio A / sqrt(renderedPower), so
        // every Core-only rendering artefact - the anti-alias taper, the
        // Body-Lock envelope running out of range - rides straight into the
        // layer balance. It moved the Air layer's own level by 23.4 dB across
        // MIDI 12-108 at full Body Lock, where its band centres are frozen and
        // nothing else can touch it, and the Air-to-Core balance by 7.8 dB over
        // MIDI 12-72 at the shipping defaults. A compensation for partials the
        // Core could not render is not evidence about how much breath noise the
        // source had. The two are measured independently and stay independent
        // here.
        targets[output] = std::clamp(frame.airAmplitudes[band]
            * parameters.air * variation * edgeGain * touchAirGain,
            0.0f, 2.0f);
        airCentresHz[band] = desiredFrequency;
        airSounding = airSounding || targets[output] > 0.0f
            || voice.amplitudes[output] > 0.0f;
    }

    // Designing the bands is the expensive half of the Air layer, so a layer
    // that contributes exactly zero for this whole control period skips it as
    // well as the per-sample filtering. Retiring any open coefficient ramp is
    // what makes that free rather than merely deferred: tick() is what normally
    // retires it, and a silent band is never ticked, so an open ramp would
    // otherwise defeat the unchanged-centre early-out in set() and redesign
    // every band on every frame for as long as the layer stays silent.
    if (airSounding)
    {
        for (std::size_t band = 0; band < NeuralModel::airBandCount; ++band)
            voice.airFilters[band].set(
                airCentresHz[band], model.airBandwidthOctaves_[band],
                static_cast<float>(sampleRate_),
                firstControlFrame ? 0 : controlPeriod_);
    }
    else
    {
        for (auto& filter : voice.airFilters)
            filter.finishRamp();
    }

    std::array<float, NeuralModel::boneModeCount> boneCentresHz {};
    for (std::size_t mode = 0; mode < NeuralModel::boneModeCount; ++mode)
    {
        const std::size_t output = boneOutputOffset + mode;
        const float active = model.boneModeReliabilities_[mode] > 0.0f
            ? 1.0f : 0.0f;
        const float desiredFrequency = model.metadata_.rootFrequencyHz
            * model.boneFrequencyRatios_[mode] * spectralScale;
        boneCentresHz[mode] = desiredFrequency;
        const float edgeGain = std::clamp(
            (desiredFrequency - 10.0f) / 20.0f, 0.0f, 1.0f)
            * std::clamp((boneEdgeLimitHz_ - desiredFrequency)
                             / boneEdgeFadeHz_,
                         0.0f, 1.0f);
        // No register compensation here either, for the reason given at the
        // Air targets above: the modal ring the source had is measured
        // independently of the Core and does not follow the Core's
        // normalisation.
        targets[output] = std::clamp(frame.boneAmplitudes[mode]
            * active * parameters.bone * edgeGain, 0.0f, 2.0f);
        const float boundedFrequency = std::clamp(desiredFrequency, 10.0f,
                                                  boneCeilingHz_);
        if (firstControlFrame)
        {
            voice.boneFrequenciesHz[mode] = boundedFrequency;
            voice.boneFrequencySteps[mode] = 0.0f;
        }
        else
        {
            voice.boneFrequencySteps[mode] =
                (boundedFrequency - voice.boneFrequenciesHz[mode])
                / static_cast<float>(controlPeriod_);
        }
    }

    // Damp the release by frequency. A note-off is a damper, and a damper is
    // frequency-dependent: the tone should darken as it dies, where one gain
    // fade on the summed voice takes all 256 partials, 16 Air bands and 12
    // Bone modes down by exactly the same number of dB and leaves the released
    // tail's spectral centroid 0.17% from the held note's.
    //
    // The shape is the source's own: p is fitted from the model's per-partial
    // trajectories by fitDampingExponent(), so a source whose partials decayed
    // at the same rate at every frequency fits zero and renders bit-identically
    // to every earlier build. Applying a free-decay exponent to a damper is an
    // analogy rather than a derivation - what it defends is that the release
    // should be frequency-dependent, and that its direction and rough size
    // should come from the sound the user dropped in rather than from a drawn
    // curve.
    //
    // The excess folds into the control-rate targets rather than into the
    // per-sample loop, so it costs one multiply per slot per control period.
    // The first control frame after a note-off can be up to one control period
    // late, which offsets every slot's release by the same constant and cancels
    // out of any measurement of how fast one slot falls against another.
    if (voice.releasing && dampingExponent_ > 0.0f)
    {
        const float releaseSeconds = std::max(parameters.releaseSeconds,
                                              0.005f);
        if (voice.releaseShapeSeconds != releaseSeconds)
            buildReleaseShape(voice, releaseSeconds, renderedFundamental,
                              airCentresHz, boneCentresHz);
        for (std::size_t output = 0; output < renderAmplitudeCount; ++output)
        {
            const float gain = voice.releaseSlotGain[output]
                * voice.releaseSlotDecay[output];
            // At the ratio ceiling and a 0.65 s Dissolve the fastest slot
            // loses 6.8 dB per control period, so it would walk into the
            // denormal range long before the voice retires. The floor is 400
            // dB below any level the output carries and keeps the running gain
            // in normal floats.
            voice.releaseSlotGain[output] = gain > 1.0e-20f ? gain : 0.0f;
            targets[output] *= voice.releaseSlotGain[output];
        }
    }

    // Everything above the last harmonic that is audible now, or becomes
    // audible during the coming ramp, contributes exactly zero for the whole
    // control period. Recording that boundary lets the audio loop stop there.
    std::size_t soundingHarmonics = 0;
    for (std::size_t harmonic = 0;
         harmonic < voice.activeHarmonicCount; ++harmonic)
    {
        if (targets[harmonic] > 0.0f || voice.amplitudes[harmonic] > 0.0f)
            soundingHarmonics = harmonic + 1;
    }
    voice.soundingHarmonicCount = soundingHarmonics;

    voice.airSounding = airSounding;
    voice.boneSounding = false;
    for (std::size_t output = boneOutputOffset; output < renderAmplitudeCount;
         ++output)
        voice.boneSounding = voice.boneSounding
            || targets[output] > 0.0f || voice.amplitudes[output] > 0.0f;

    for (std::size_t output = 0; output < renderAmplitudeCount; ++output)
    {
        if (firstControlFrame)
        {
            // No previous sounding sample exists to smooth from. Installing
            // the learned onset immediately avoids adding an arbitrary
            // control-period attack in front of the model's own trajectory.
            voice.amplitudes[output] = targets[output];
            voice.amplitudeSteps[output] = 0.0f;
        }
        else
        {
            voice.amplitudeSteps[output] =
                (targets[output] - voice.amplitudes[output])
                / static_cast<float>(controlPeriod_);
        }
    }
    if (firstControlFrame)
    {
        voice.pitchRatio = frame.pitchRatio;
        voice.pitchRatioStep = 0.0f;
    }
    else
    {
        voice.pitchRatioStep = (frame.pitchRatio - voice.pitchRatio)
            / static_cast<float>(controlPeriod_);
    }

    // The per-note jitter is inside the Spread multiply, not beside it: added
    // outside, it displaced a voice by up to +/-0.0096 at the shipping Mutation
    // even at Spread 0, so the instrument was not exactly mono at its own
    // narrowest setting. Multiplied, Spread 0 collapses the placement and the
    // jitter together and the mono claim holds exactly.
    const float pan = std::clamp(parameters.spread
        * (voice.pan + 0.08f * parameters.mutation * voice.mutationOffset),
        -1.0f, 1.0f);
    const float targetLeft = std::sqrt(0.5f * (1.0f - pan));
    const float targetRight = std::sqrt(0.5f * (1.0f + pan));
    // Air gains a decorrelated side realisation so the noise layer occupies the
    // stereo field instead of collapsing to the voice's pan position. It is
    // added to the left and subtracted from the right, so it cancels exactly in
    // a mono sum and a wider setting cannot change what a mono listener hears.
    const float targetSideGain = 0.7071068f * parameters.spread;
    if (firstControlFrame)
    {
        voice.panLeft = targetLeft;
        voice.panRight = targetRight;
        voice.panLeftStep = 0.0f;
        voice.panRightStep = 0.0f;
        voice.airSideGain = targetSideGain;
        voice.airSideGainStep = 0.0f;
    }
    else
    {
        voice.airSideGainStep = (targetSideGain - voice.airSideGain)
            / static_cast<float>(controlPeriod_);
        // A voice's own placement no longer moves once it has sounded, but a
        // host automating Spread or Mutation still moves it. A stepped gain
        // change there is an audible click, so the new placement is reached
        // over one control interval like every other target.
        voice.panLeftStep = (targetLeft - voice.panLeft)
            / static_cast<float>(controlPeriod_);
        voice.panRightStep = (targetRight - voice.panRight)
            / static_cast<float>(controlPeriod_);
    }
}

void NeuramarEngine::process(float* left, float* right, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const auto* model = model_.load(std::memory_order_acquire);
    const EngineParameters parameters = loadParameters();
    // The stiff-string ratio table is shared by every voice and only depends
    // on the model coefficient and the Stretch control, so it is rebuilt at
    // most once per block and usually never. No allocation is involved.
    refreshHarmonicStretch(model != nullptr
        ? model->inharmonicity() * parameters.stretch : 0.0f);
    // The Orbit detrend is fitted to the mix Air and Bone are set to render,
    // so it follows those two controls. No decoder work is involved and the
    // refresh returns immediately when neither has moved.
    refreshLoopLevelSlope(parameters.air, parameters.bone);
    // Adopt the host's level on the first block rather than sliding up to it.
    if (!(smoothedOutputGain_ >= 0.0f))
        smoothedOutputGain_ = parameters.outputGain;
    // Awaken is a fade-in time, so it advances a position rather than driving
    // a one-pole. A one-pole reaches only 63% of the level in the time its
    // label promises - full opening took roughly 4.6 times the stated seconds
    // - and it has its steepest slope at the instant of the note-on, which is
    // the very thing a soft attack exists to remove. The smoothstep below
    // finishes exactly on time and has zero slope at both ends.
    const float attackStep = parameters.attackSeconds <= 0.0f
        ? 1.0f
        : inverseSampleRate_ / parameters.attackSeconds;
    // The release rate is key-tracked, so it is no longer one number for the
    // whole block: each voice carries its own, built by updateVoiceControl()
    // from the same r^p that scales its model clock.

    // With no model, no sounding voice, and no click-fade remnant, every
    // output sample is exactly zero: skip the loop and write silence.
    bool anythingSounding = model != nullptr
        && std::any_of(voices_.begin(), voices_.end(),
                       [](const Voice& voice) noexcept { return voice.active; });
    for (const auto& tail : fadeTails_)
        anythingSounding = anythingSounding || tail.remaining > 0;
    if (!anythingSounding)
    {
        if (left != nullptr)
            std::fill(left, left + numSamples, 0.0f);
        if (right != nullptr && right != left)
            std::fill(right, right + numSamples, 0.0f);
        return;
    }

    // One kilobyte of stack that stays hot in the first-level cache for the
    // whole call. It decouples the vectorised partial evaluation from the
    // running sum without touching the heap or growing the voice state.
    std::array<float, renderedHarmonicCount> partialScratch {};

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float outputLeft = 0.0f;
        float outputRight = 0.0f;

        if (model != nullptr)
        {
            for (auto& voice : voices_)
            {
                if (!voice.active)
                    continue;

                if (voice.controlCountdown <= 0)
                {
                    const bool firstControlFrame = voice.modelTimeSeconds <= 0.0;
                    updateVoiceControl(voice, *model, parameters);
                    // Let the first sample install the exact onset, then begin
                    // forward interpolation on the following sample.
                    voice.controlCountdown = firstControlFrame
                        ? 1 : controlPeriod_;
                }

                const auto advanceAmplitude = [&voice](std::size_t output) noexcept
                {
                    voice.amplitudes[output] = std::max(0.0f,
                        voice.amplitudes[output] + voice.amplitudeSteps[output]);
                };
                const std::size_t soundingHarmonics
                    = voice.soundingHarmonicCount;
                // A silent layer's amplitudes and steps are all exactly zero for
                // the whole control period, so skipping its ramp is not an
                // approximation; each layer is skipped on its own so a silent
                // Bone does not ride along with a sounding Air.
                if (voice.airSounding)
                    for (std::size_t output = airOutputOffset;
                         output < boneOutputOffset; ++output)
                        advanceAmplitude(output);
                if (voice.boneSounding)
                    for (std::size_t output = boneOutputOffset;
                         output < renderAmplitudeCount; ++output)
                        advanceAmplitude(output);
                voice.pitchRatio = std::clamp(
                    voice.pitchRatio + voice.pitchRatioStep, 0.75f, 1.35f);
                --voice.controlCountdown;

                if (voice.releasing)
                {
                    voice.envelope *= voice.releaseMultiplier;
                }
                else
                {
                    voice.attackPosition = std::min(
                        voice.attackPosition + attackStep, 1.0f);
                    voice.envelope = voice.attackPosition * voice.attackPosition
                        * (3.0f - 2.0f * voice.attackPosition);
                }

                const float fundamentalHz = model->metadata_.rootFrequencyHz
                    * voice.transpositionRatio * voice.pitchRatio;
                const float phaseStep = fundamentalHz * inverseSampleRate_;
                // The band-limiting taper already lives in the ramped
                // amplitude, so this pass carries no per-sample anti-alias
                // arithmetic. It is deliberately split in two: the first loop
                // is purely elementwise over contiguous arrays - no reduction,
                // no memory gather, and no library call - so the compiler
                // vectorises it, while the running sum stays in a separate
                // pass with four independent accumulators so its serial
                // floating-point addition chain does not set the throughput of
                // the whole low-register render. Every stored phase is already
                // in [0, 1) and the increment is non-negative, so truncation
                // is exactly floor() and needs no rounding instruction.
                for (std::size_t harmonic = 0;
                     harmonic < soundingHarmonics; ++harmonic)
                {
                    const float ramped = voice.amplitudes[harmonic]
                        + voice.amplitudeSteps[harmonic];
                    const float amplitude = ramped > 0.0f ? ramped : 0.0f;
                    voice.amplitudes[harmonic] = amplitude;
                    const float phase = voice.harmonicPhases[harmonic];
                    partialScratch[harmonic] = amplitude * unitSine(phase);
                    const float advanced = phase
                        + phaseStep * harmonicStretchRatio_[harmonic];
                    voice.harmonicPhases[harmonic] = advanced
                        - static_cast<float>(static_cast<int>(advanced));
                }
                float coreAccumulator[4] { 0.0f, 0.0f, 0.0f, 0.0f };
                const std::size_t unrolledHarmonics
                    = soundingHarmonics & ~std::size_t { 3 };
                for (std::size_t harmonic = 0;
                     harmonic < unrolledHarmonics; harmonic += 4)
                {
                    coreAccumulator[0] += partialScratch[harmonic];
                    coreAccumulator[1] += partialScratch[harmonic + 1];
                    coreAccumulator[2] += partialScratch[harmonic + 2];
                    coreAccumulator[3] += partialScratch[harmonic + 3];
                }
                float coreSample = (coreAccumulator[0] + coreAccumulator[1])
                    + (coreAccumulator[2] + coreAccumulator[3]);
                for (std::size_t harmonic = unrolledHarmonics;
                     harmonic < soundingHarmonics; ++harmonic)
                    coreSample += partialScratch[harmonic];

                float airSample = 0.0f;
                float airSideSample = 0.0f;
                if (voice.airSounding)
                {
                    const bool stereoAir = voice.airSideGain > 1.0e-4f
                        || voice.airSideGainStep > 0.0f;
                    for (std::size_t band = 0;
                         band < NeuralModel::airBandCount; ++band)
                    {
                        const std::size_t output = airOutputOffset + band;
                        const float level = voice.amplitudes[output];
                        auto& filter = voice.airFilters[band];
                        airSample += level * filter.tick(
                            nextNoise(voice.airNoiseStates[band]));
                        if (stereoAir)
                            airSideSample += level * filter.tickSide(
                                nextNoise(voice.airSideNoiseStates[band]));
                    }
                }

                float boneSample = 0.0f;
                if (voice.boneSounding)
                {
                    for (std::size_t mode = 0;
                         mode < NeuralModel::boneModeCount; ++mode)
                    {
                        const std::size_t output = boneOutputOffset + mode;
                        boneSample += voice.amplitudes[output]
                            * unitSine(voice.bonePhases[mode]);
                        voice.boneFrequenciesHz[mode] = std::clamp(
                            voice.boneFrequenciesHz[mode]
                                + voice.boneFrequencySteps[mode],
                            10.0f, boneCeilingHz_);
                        const float advanced = voice.bonePhases[mode]
                            + voice.boneFrequenciesHz[mode] * inverseSampleRate_;
                        voice.bonePhases[mode] = advanced
                            - static_cast<float>(static_cast<int>(advanced));
                    }
                }

                voice.panLeft += voice.panLeftStep;
                voice.panRight += voice.panRightStep;
                voice.airSideGain += voice.airSideGainStep;
                const float gain = voice.envelope * voice.velocityGain;
                const float voiceSample = gain
                    * (coreSample + airSample + boneSample);
                const float sideSample = gain * voice.airSideGain
                    * airSideSample;
                voice.lastLeft = voiceSample * voice.panLeft + sideSample;
                voice.lastRight = voiceSample * voice.panRight - sideSample;
                outputLeft += voice.lastLeft;
                outputRight += voice.lastRight;
                // Key-tracked: r^p times the panel's rate, so the learned
                // trajectory plays out in the time the source's own damping
                // law says this key should take. The trajectory still freezes
                // at t = duration, so a high note reaches the frozen final
                // frame r^p times sooner in wall-clock and then holds it - but
                // it holds the *same* frame, at the same level relative to its
                // own onset, so the sustain floor a note settles on is
                // unchanged by this and only arrives when the note has already
                // died to it. On the decay fixture MIDI 81 reaches the freeze
                // at 0.458 s, well past its own 0.243 s T20.
                voice.modelTimeSeconds += static_cast<double>(
                        parameters.evolutionRate * voice.clockRateScale)
                    / sampleRate_;
                ++voice.renderedSampleCount;

                // A retiring voice takes its own pan with it. Every other
                // sounding voice keeps the placement its own pitch gave it, so
                // nothing has to be recomputed here.
                if (voice.releasing && voice.envelope <= retirementLevel)
                    voice.clear();
            }
        }

        for (auto& tail : fadeTails_)
        {
            if (tail.remaining <= 0)
                continue;
            const float window = fadeTailWindow(tail.position);
            outputLeft += tail.left * window;
            outputRight += tail.right * window;
            tail.position += tail.positionStep;
            if (--tail.remaining <= 0)
                tail.clear();
        }

        smoothedOutputGain_ += outputGainCoefficient_
            * (parameters.outputGain - smoothedOutputGain_);
        const auto finishOutput = [gain = smoothedOutputGain_](float value) noexcept
        {
            return guardFiniteOutput(gain * value);
        };
        outputLeft = finishOutput(outputLeft);
        outputRight = finishOutput(outputRight);

        if (left != nullptr)
            left[sample] = outputLeft;
        if (right != nullptr && right != left)
            right[sample] = outputRight;
        else if (right != nullptr)
            right[sample] = 0.5f * (outputLeft + outputRight);
    }
}

int NeuramarEngine::getActiveVoiceCount() const noexcept
{
    int result = 0;
    for (const auto& voice : voices_)
        result += voice.active ? 1 : 0;
    return result;
}

} // namespace neuramar
