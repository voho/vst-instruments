#include "MarsEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace mars
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;
constexpr float envelopeRange = 6.90775527898f; // -60 dB in the requested time.
constexpr float thermalVoltage = 0.026f;
constexpr float twiceThermalVoltage = 2.0f * thermalVoltage;
constexpr std::size_t transistorPairTableIntervals = 1024;
constexpr float transistorPairTableLimit = 7.0f;
const std::array<float, transistorPairTableIntervals + 1> transistorPairTable = []
{
    std::array<float, transistorPairTableIntervals + 1> values {};
    for (std::size_t index = 0; index < values.size(); ++index)
        values[index] = std::tanh(
            transistorPairTableLimit * static_cast<float>(index)
            / static_cast<float>(transistorPairTableIntervals));
    return values;
}();

float transistorPairTanh(float value) noexcept
{
    // The ladder evaluates transistor differential pairs throughout its bounded
    // nonlinear solve. Linear interpolation through a dense 4 KiB table of
    // exact tanh points keeps that physical transfer practical at full
    // polyphony. An exhaustive float-domain check measured less than 4.6e-6
    // absolute error (the analytic interpolation bound is 4.7e-6);
    // outside +/-7 the reference is already within 1.7e-6 of its asymptote.
    const float magnitude = std::abs(value);
    if (magnitude < 0.000244140625f)
        return value;
    if (magnitude >= transistorPairTableLimit)
        return std::copysign(1.0f, value);

    constexpr float tableScale = static_cast<float>(transistorPairTableIntervals)
                               / transistorPairTableLimit;
    const float position = magnitude * tableScale;
    const auto index = std::min(static_cast<std::size_t>(position),
                                transistorPairTableIntervals - 1);
    const float t = position - static_cast<float>(index);
    const float y0 = transistorPairTable[index];
    const float y1 = transistorPairTable[index + 1];
    const float interpolated = y0 + t * (y1 - y0);
    return std::copysign(interpolated, value);
}

float finiteOr(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float triangleAtPhase(float phase) noexcept
{
    phase -= std::floor(phase);
    return phase < 0.5f ? -1.0f + 4.0f * phase : 3.0f - 4.0f * phase;
}

float safetyLimit(float value) noexcept
{
    if (!std::isfinite(value))
        return 0.0f;

    const float magnitude = std::abs(value);
    if (magnitude <= 0.98f)
        return value;

    const float excess = magnitude - 0.98f;
    const float limited = 0.98f + excess / (1.0f + excess / 0.22f);
    return std::copysign(std::min(limited, 1.2f), value);
}
} // namespace

void MarsEngine::Envelope::reset() noexcept
{
    stage = EnvelopeStage::Idle;
    value = 0.0f;
}

void MarsEngine::Envelope::noteOn() noexcept
{
    value = 0.0f;
    stage = EnvelopeStage::Attack;
}

void MarsEngine::Envelope::noteOff() noexcept
{
    if (stage != EnvelopeStage::Idle)
        stage = EnvelopeStage::Release;
}

float MarsEngine::Envelope::tick(float attackCoefficient, float decayCoefficient,
                                 float sustain, float releaseCoefficient) noexcept
{
    sustain = std::clamp(sustain, 0.0f, 1.0f);

    switch (stage)
    {
        case EnvelopeStage::Idle:
            value = 0.0f;
            break;

        case EnvelopeStage::Attack:
            value += (1.001f - value) * attackCoefficient;
            if (value >= 0.999f)
            {
                value = 1.0f;
                stage = EnvelopeStage::Decay;
            }
            break;

        case EnvelopeStage::Decay:
            value += (sustain - value) * decayCoefficient;
            if (std::abs(value - sustain) < 0.0005f)
            {
                value = sustain;
                stage = EnvelopeStage::Sustain;
            }
            break;

        case EnvelopeStage::Sustain:
            value += (sustain - value) * decayCoefficient;
            break;

        case EnvelopeStage::Release:
            value += (0.0f - value) * releaseCoefficient;
            if (value < 0.00001f)
                reset();
            break;
    }

    return value;
}

void MarsEngine::LadderFilter::reset() noexcept
{
    stageVoltage.fill(0.0f);
    stageTanh.fill(0.0f);
    previousInputVoltage = 0.0f;
    previousFeedbackTanh = 0.0f;
    cachedFeedbackGain = 0.0f;
    stageTanhValid = true;
    feedbackTanhValid = false;
}

/*
    The four-stage nonlinear ladder below solves the bilinear-discretised
    implicit circuit equations from:

    S. D'Angelo and V. Valimaki, "Generalized Moog Ladder Filter: Part II--
    Explicit Nonlinear Model through a Novel Delay-Free Loop Implementation
    Method," IEEE/ACM TASLP, vol. 22, no. 12, 2014.

    The authors' non-iterative reference implementation is excellent inside its
    documented range, but its finite-precision evaluation is reported coherent
    only through fc <= fs/8. Mars exposes a 20 kHz cutoff even in native-rate
    mode, so it solves the original implicit equations with a residual-decreasing
    damped Newton method instead. Its small cyclic-bidiagonal Jacobian is solved
    analytically. Both the Newton and backtracking counts have fixed maxima, so
    no allocation, unbounded iteration, or artificial delay is introduced.
*/
float MarsEngine::LadderFilter::process(float inputVoltage,
                                        float frequencyTangent,
                                        float feedbackGain,
                                        float frequencyScale) noexcept
{
    inputVoltage = std::clamp(inputVoltage, -0.8f, 0.8f);
    frequencyTangent = std::clamp(frequencyTangent, 0.00001f, 8.0f);
    const float k = std::clamp(feedbackGain, 0.0f, 3.98f);
    const float g = frequencyTangent * std::clamp(frequencyScale, 0.5f, 2.0f);

    const auto previous = stageVoltage;
    std::array<float, 4> estimate = previous;
    std::array<float, 4> previousTanh = stageTanh;
    if (!stageTanhValid)
        for (std::size_t stage = 0; stage < previous.size(); ++stage)
            previousTanh[stage] = transistorPairTanh(
                previous[stage] / twiceThermalVoltage);
    const float historyFeedbackTanh = feedbackTanhValid && cachedFeedbackGain == k
        ? previousFeedbackTanh
        : transistorPairTanh(
            (previousInputVoltage + k * previous.back()) / twiceThermalVoltage);

    struct Evaluation
    {
        std::array<float, 4> currentTanh {};
        std::array<float, 4> residual {};
        float feedbackTanh { 0.0f };
        float squaredResidual { std::numeric_limits<float>::infinity() };
        float maximumResidual { std::numeric_limits<float>::infinity() };
        bool finite { false };
    };

    const auto evaluate = [&] (const std::array<float, 4>& candidate,
                               const std::array<float, 4>* knownTanh = nullptr) noexcept
    {
        Evaluation result;
        result.finite = std::all_of(candidate.begin(), candidate.end(),
                                    [] (float value) noexcept
                                    {
                                        return std::isfinite(value);
                                    });
        if (!result.finite)
            return result;

        if (knownTanh != nullptr)
            result.currentTanh = *knownTanh;
        else
            for (std::size_t stage = 0; stage < candidate.size(); ++stage)
                result.currentTanh[stage] = transistorPairTanh(
                    candidate[stage] / twiceThermalVoltage);

        result.feedbackTanh = transistorPairTanh(
            (inputVoltage + k * candidate.back()) / twiceThermalVoltage);
        result.residual[0] = candidate[0] - previous[0]
                           + twiceThermalVoltage * g
                               * (result.currentTanh[0] + previousTanh[0]
                                  + result.feedbackTanh + historyFeedbackTanh);
        for (std::size_t stage = 1; stage < result.residual.size(); ++stage)
            result.residual[stage] = candidate[stage] - previous[stage]
                                   - twiceThermalVoltage * g
                                       * (result.currentTanh[stage - 1]
                                          + previousTanh[stage - 1]
                                          - result.currentTanh[stage]
                                          - previousTanh[stage]);

        result.squaredResidual = 0.0f;
        result.maximumResidual = 0.0f;
        for (const float value : result.residual)
        {
            result.squaredResidual += value * value;
            result.maximumResidual = std::max(result.maximumResidual,
                                              std::abs(value));
        }
        result.finite = std::isfinite(result.squaredResidual)
                     && std::isfinite(result.maximumResidual);
        return result;
    };

    // Starting at the previous state is a good predictor for ordinary audio,
    // but a high-cutoff full-scale discontinuity can make an unrestricted
    // Newton step cross several saturated tanh regions. Accepting only a step
    // that reduces ||residual||^2 prevents that divergence. Typical musical
    // samples finish in the first bounded pass. A second pass with deeper
    // backtracking is reserved for hostile input/control jumps, so its stronger
    // global-convergence protection adds no cost to the normal path.
    constexpr int normalNewtonUpdates = 16;
    constexpr int normalBacktrackingSteps = 8;
    constexpr int rescueNewtonUpdates = 16;
    constexpr int rescueBacktrackingSteps = 20;
    // The 1024-segment tanh interpolator itself contributes at most about
    // 1.7e-4 of normalized equation residual at the reachable maximum g. A
    // target of the same order keeps the combined exact-tanh residual beneath
    // the independently tested 6e-4 bound without wasting another update.
    constexpr float residualTolerance = twiceThermalVoltage * 3.0e-4f;
    Evaluation evaluation = evaluate(estimate, &previousTanh);
    const auto improve = [&] (int maximumUpdates,
                              int maximumBacktrackingSteps) noexcept
    {
        for (int iteration = 0;
             iteration < maximumUpdates
                 && evaluation.finite
                 && evaluation.maximumResidual > residualTolerance;
             ++iteration)
        {
            const auto& residual = evaluation.residual;
            std::array<float, 4> derivative {};
            for (std::size_t stage = 0; stage < derivative.size(); ++stage)
                derivative[stage] = 1.0f
                                  - evaluation.currentTanh[stage]
                                      * evaluation.currentTanh[stage];
            const float feedbackDerivative = 1.0f
                                           - evaluation.feedbackTanh
                                               * evaluation.feedbackTanh;

            // Solve J * delta = -residual. Rows 1..3 are lower-bidiagonal;
            // row zero adds only the cyclic feedback term in column three.
            const float diagonal0 = 1.0f + g * derivative[0];
            const float feedbackCoupling = g * k * feedbackDerivative;
            std::array<float, 4> slope {};
            std::array<float, 4> offset {};
            for (std::size_t stage = 1; stage < slope.size(); ++stage)
            {
                const float lower = -g * derivative[stage - 1];
                const float diagonal = 1.0f + g * derivative[stage];
                const float previousSlope = stage == 1 ? 1.0f : slope[stage - 1];
                const float previousOffset = stage == 1 ? 0.0f : offset[stage - 1];
                slope[stage] = -lower * previousSlope / diagonal;
                offset[stage] = (-residual[stage] - lower * previousOffset) / diagonal;
            }

            const float denominator = diagonal0 + feedbackCoupling * slope[3];
            const float delta0 = (-residual[0] - feedbackCoupling * offset[3])
                               / std::max(denominator, 1.0e-8f);
            std::array<float, 4> delta {};
            delta[0] = delta0;
            for (std::size_t stage = 1; stage < estimate.size(); ++stage)
                delta[stage] = slope[stage] * delta0 + offset[stage];

            float stepScale = 1.0f;
            bool accepted = false;
            for (int backtrack = 0;
                 backtrack < maximumBacktrackingSteps;
                 ++backtrack)
            {
                std::array<float, 4> candidate {};
                for (std::size_t stage = 0; stage < candidate.size(); ++stage)
                    candidate[stage] = estimate[stage] + stepScale * delta[stage];

                auto trial = evaluate(candidate);
                if (trial.finite
                    && trial.squaredResidual < evaluation.squaredResidual)
                {
                    estimate = candidate;
                    evaluation = trial;
                    accepted = true;
                    break;
                }
                stepScale *= 0.5f;
            }

            if (!accepted)
                break;
        }
    };

    improve(normalNewtonUpdates, normalBacktrackingSteps);
    if (evaluation.finite && evaluation.maximumResidual > residualTolerance)
    {
        improve(rescueNewtonUpdates, rescueBacktrackingSteps);
    }

    if (!evaluation.finite)
    {
        reset();
        return 0.0f;
    }

    // Never publish a state that failed the documented LUT-equation residual
    // ceiling. The previous committed state/input/cache remain a coherent,
    // finite one-sample hold; the next sample gets a fresh bounded solve. This
    // is an emergency guard after both fixed-cost solve passes, not a normal
    // approximation path.
    if (evaluation.maximumResidual > residualTolerance)
    {
        return (previous.back() / twiceThermalVoltage) * (1.0f + k);
    }

    stageVoltage = estimate;
    stageTanh = evaluation.currentTanh;
    stageTanhValid = true;
    previousInputVoltage = inputVoltage;
    previousFeedbackTanh = evaluation.feedbackTanh;
    cachedFeedbackGain = k;
    feedbackTanhValid = true;

    constexpr float silenceVoltage = twiceThermalVoltage * 1.0e-4f;
    if (std::abs(inputVoltage) < 1.0e-12f
        && std::all_of(stageVoltage.begin(), stageVoltage.end(),
                       [] (float value) noexcept
                       {
                           return std::abs(value) < silenceVoltage;
                       }))
    {
        reset();
        return 0.0f;
    }

    // The circuit state is expressed in volts. The reference implementation's
    // optional DC compensation multiplies by (1 + k), cancelling the ladder's
    // otherwise severe passband loss as resonance rises. The raw loss is a
    // genuine ladder property, but at Mars' default resonance it removes about
    // 6.5 dB from bass fundamentals and sounds more broken than characterful.
    // Nonlinear resonance and self-oscillation remain; only the static low-band
    // attenuation is restored before the bounded VCA stage.
    return (stageVoltage.back() / twiceThermalVoltage) * (1.0f + k);
}

void MarsEngine::StateVariableFilter::reset() noexcept
{
    ic1eq = 0.0f;
    ic2eq = 0.0f;
}

void MarsEngine::StateVariableFilter::process(float input, float g,
                                              float resonance,
                                              float& low, float& band,
                                              float& high) noexcept
{
    g = std::clamp(g, 0.00001f, 8.0f);
    resonance = std::clamp(resonance, 0.0f, 0.995f);

    // This remains a stable TPT state-variable approximation rather than a
    // component-level SEM clone. Nonlinearity is applied once, at the shared
    // physical-voltage filter input, before this method is called.
    const float driven = input;
    const float damping = 2.0f - 1.94f * resonance;
    const float a1 = 1.0f / (1.0f + g * (g + damping));
    const float a2 = g * a1;
    const float a3 = g * a2;
    const float v3 = driven - ic2eq;
    const float v1 = a1 * ic1eq + a2 * v3;
    const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
    ic1eq = 2.0f * v1 - ic1eq;
    ic2eq = 2.0f * v2 - ic2eq;

    band = v1;
    low = v2;
    const float highRaw = driven - damping * v1 - v2;
    high = highRaw;
}

MarsEngine::MarsEngine() noexcept
{
    targetParameters_ = sanitise(EngineParameters {});
    smoothedParameters_ = targetParameters_;
}

EngineParameters MarsEngine::sanitise(const EngineParameters& source) noexcept
{
    EngineParameters p = source;
    if (p.voiceMode != VoiceMode::Poly && p.voiceMode != VoiceMode::Unison
        && p.voiceMode != VoiceMode::Fifth)
        p.voiceMode = VoiceMode::Poly;
    const auto validWave = [](OscillatorWave wave)
    {
        return wave == OscillatorWave::Saw || wave == OscillatorWave::Pulse || wave == OscillatorWave::Triangle;
    };
    if (!validWave(p.osc1Wave))
        p.osc1Wave = OscillatorWave::Saw;
    if (!validWave(p.osc2Wave))
        p.osc2Wave = OscillatorWave::Pulse;
    if (p.filterModel != FilterModel::Ladder && p.filterModel != FilterModel::Sem)
        p.filterModel = FilterModel::Ladder;
    if (p.lfoWave != LfoWaveform::Triangle && p.lfoWave != LfoWaveform::Sine
        && p.lfoWave != LfoWaveform::SampleHold)
        p.lfoWave = LfoWaveform::Triangle;
    p.unisonVoices = std::clamp(p.unisonVoices, 2, 8);
    p.osc1Octave = std::clamp(p.osc1Octave, -2, 2);
    p.osc2Octave = std::clamp(p.osc2Octave, -2, 2);
    p.osc2Semitones = std::clamp(p.osc2Semitones, -12, 12);
    p.osc2FineCents = std::clamp(finiteOr(p.osc2FineCents, 0.0f), -100.0f, 100.0f);
    p.pulseWidth = std::clamp(finiteOr(p.pulseWidth, 0.48f), 0.05f, 0.95f);
    p.crossMod = std::clamp(finiteOr(p.crossMod, 0.08f), 0.0f, 1.0f);
    p.oscMix = std::clamp(finiteOr(p.oscMix, 0.42f), 0.0f, 1.0f);
    p.subLevel = std::clamp(finiteOr(p.subLevel, 0.18f), 0.0f, 1.0f);
    p.noiseLevel = std::clamp(finiteOr(p.noiseLevel, 0.04f), 0.0f, 1.0f);
    p.cutoffHz = std::clamp(finiteOr(p.cutoffHz, 4200.0f), 18.0f, 22000.0f);
    p.resonance = std::clamp(finiteOr(p.resonance, 0.28f), 0.0f, 0.995f);
    p.filterDrive = std::clamp(finiteOr(p.filterDrive, 0.24f), 0.0f, 1.0f);
    p.filterShape = std::clamp(finiteOr(p.filterShape, 0.35f), 0.0f, 1.0f);
    p.filterEnvAmount = std::clamp(finiteOr(p.filterEnvAmount, 0.42f), -1.0f, 1.0f);
    p.lfoFilterOctaves = std::clamp(finiteOr(p.lfoFilterOctaves, 0.15f), -8.0f, 8.0f);
    p.filterKeyTrack = std::clamp(finiteOr(p.filterKeyTrack, 0.45f), 0.0f, 1.0f);
    p.ampAttack = std::clamp(finiteOr(p.ampAttack, 0.008f), 0.0005f, 20.0f);
    p.ampDecay = std::clamp(finiteOr(p.ampDecay, 0.38f), 0.0005f, 20.0f);
    p.ampSustain = std::clamp(finiteOr(p.ampSustain, 0.78f), 0.0f, 1.0f);
    p.ampRelease = std::clamp(finiteOr(p.ampRelease, 0.55f), 0.0005f, 20.0f);
    p.filterAttack = std::clamp(finiteOr(p.filterAttack, 0.012f), 0.0005f, 20.0f);
    p.filterDecay = std::clamp(finiteOr(p.filterDecay, 0.45f), 0.0005f, 20.0f);
    p.filterSustain = std::clamp(finiteOr(p.filterSustain, 0.34f), 0.0f, 1.0f);
    p.filterRelease = std::clamp(finiteOr(p.filterRelease, 0.62f), 0.0005f, 20.0f);
    p.lfoRateHz = std::clamp(finiteOr(p.lfoRateHz, 4.8f), 0.02f, 30.0f);
    p.lfoPitchCents = std::clamp(finiteOr(p.lfoPitchCents, 8.0f), -200.0f, 200.0f);
    p.lfoPwm = std::clamp(finiteOr(p.lfoPwm, 0.18f), 0.0f, 1.0f);
    p.drift = std::clamp(finiteOr(p.drift, 0.28f), 0.0f, 1.0f);
    p.spread = std::clamp(finiteOr(p.spread, 0.58f), 0.0f, 1.0f);
    p.glideSeconds = std::clamp(finiteOr(p.glideSeconds, 0.0f), 0.0f, 5.0f);
    p.velocityAmount = std::clamp(finiteOr(p.velocityAmount, 0.72f), 0.0f, 1.0f);
    p.chorusMix = std::clamp(finiteOr(p.chorusMix, 0.32f), 0.0f, 1.0f);
    p.chorusRateHz = std::clamp(finiteOr(p.chorusRateHz, 0.56f), 0.03f, 8.0f);
    p.outputGain = std::clamp(finiteOr(p.outputGain, 0.72f), 0.0f, 2.0f);
    return p;
}

void MarsEngine::updateProcessingRate() noexcept
{
    // HQ doubles host rates through 48 kHz (88.2/96 kHz internally for the
    // common 44.1/48 kHz cases), then stays native at higher production rates.
    oversampling_ = oversamplingEnabled_ && sampleRate_ <= maximumOversampledHostRate
        ? oversampleFactor : 1;
    oversampledRate_ = static_cast<float>(sampleRate_ * static_cast<double>(oversampling_));
    filterCrossfadeSamples_ = std::max(1, static_cast<int>(std::lround(
        0.003 * static_cast<double>(oversampledRate_))));
    // A stolen voice contributes its last sample to this -60 dB / 2 ms tail.
    // It is short enough to remain a transient treatment rather than another
    // voice, and fixed state keeps the note-on path allocation-free.
    stealTailCoefficient_ = std::exp(-envelopeRange
        / (0.002f * std::max(oversampledRate_, 1.0f)));
}

void MarsEngine::prepare(double sampleRate, int maxBlockSize,
                         bool oversamplingEnabled)
{
    (void) maxBlockSize;
    sampleRate = std::isfinite(sampleRate) ? sampleRate : 48000.0;
    sampleRate_ = std::clamp(sampleRate, 8000.0, maximumSupportedSampleRate);
    inverseSampleRate_ = static_cast<float>(1.0 / sampleRate_);
    oversamplingEnabled_ = oversamplingEnabled;
    oversamplingRequested_ = oversamplingEnabled;
    updateProcessingRate();
    // A 1.5 Hz servo still removes accumulated DC while retaining sub-octave
    // fundamentals on the bottom MIDI octave. The former 8 Hz corner was
    // already 3 dB down at the sub oscillator's lowest musically useful notes.
    dcCoefficient_ = std::exp(-twoPi * 1.5f * inverseSampleRate_);

    buildVoiceCards();
    prepared_ = true;
    reset();
}

bool MarsEngine::setOversamplingEnabled(bool enabled) noexcept
{
    if (oversamplingRequested_ == enabled)
        return false;

    oversamplingRequested_ = enabled;
    return applyPendingOversamplingIfIdle();
}

bool MarsEngine::applyPendingOversamplingIfIdle() noexcept
{
    if (oversamplingEnabled_ == oversamplingRequested_)
        return false;

    const int quietSamplesRequired = std::max(
        1, static_cast<int>(std::lround(0.025 * sampleRate_)));
    if (activeVoiceCount_ != 0 || oversamplingIdleSamples_ < quietSamplesRequired)
        return false;

    oversamplingEnabled_ = oversamplingRequested_;
    const int previousFactor = oversampling_;
    updateProcessingRate();
    if (oversampling_ == previousFactor)
        return false;

    // Quality changes are non-automatable and deferred until the engine and
    // short ensemble tail have been idle for 25 ms. Reinitialising only then
    // avoids interpreting oscillator, envelope, and nonlinear-filter states at
    // a different timebase without cutting a held note or producing a click.
    reset();
    return true;
}

void MarsEngine::reset()
{
    for (auto& voice : voices_)
        voice = Voice {};

    oversampleLeftHistory_.fill(0.0f);
    oversampleRightHistory_.fill(0.0f);
    oversampleWriteIndex_ = 0;

    chorusLeft_.fill(0.0f);
    chorusRight_.fill(0.0f);
    chorusWriteIndex_ = 0;
    chorusPhase_ = 0.173f;
    chorusLowLeft_ = 0.0f;
    chorusLowRight_ = 0.0f;
    dcInputLeft_ = dcInputRight_ = 0.0f;
    dcOutputLeft_ = dcOutputRight_ = 0.0f;
    lfoPhase_ = 0.0f;
    randomLfoValue_ = -0.27f;
    globalNoiseState_ = 0x6d2b79f5u;
    pitchBendTarget_ = 0.0f;
    pitchBend_ = 0.0f;
    modWheelTarget_ = 0.0f;
    modWheel_ = 0.0f;
    sustainPedalDown_ = false;
    smoothedParameters_ = targetParameters_;
    if (smoothedParameters_.osc1Enabled && smoothedParameters_.osc2Enabled)
    {
        oscillator1MixGain_ = std::sqrt(std::max(0.0f, 1.0f - smoothedParameters_.oscMix));
        oscillator2MixGain_ = std::sqrt(std::max(0.0f, smoothedParameters_.oscMix));
    }
    else
    {
        oscillator1MixGain_ = smoothedParameters_.osc1Enabled ? 1.0f : 0.0f;
        oscillator2MixGain_ = smoothedParameters_.osc2Enabled ? 1.0f : 0.0f;
    }
    stealTailLeft_ = 0.0f;
    stealTailRight_ = 0.0f;
    generation_ = 0;
    activeVoiceCount_ = 0;
    oversamplingIdleSamples_ = std::max(
        1, static_cast<int>(std::lround(0.025 * sampleRate_)));
    lastPlayedMidi_ = 60.0f;
    hasLastPlayedMidi_ = false;
}

void MarsEngine::setParameters(const EngineParameters& parameters)
{
    targetParameters_ = sanitise(parameters);
}

std::uint32_t MarsEngine::hash32(std::uint32_t value) noexcept
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value == 0u ? 1u : value;
}

float MarsEngine::hashBipolar(std::uint32_t value) noexcept
{
    return static_cast<float>(hash32(value) & 0x00ffffffu) / 8388607.5f - 1.0f;
}

void MarsEngine::buildVoiceCards() noexcept
{
    for (int i = 0; i < maxVoices; ++i)
    {
        auto& card = cards_[static_cast<std::size_t>(i)];
        const std::uint32_t seed = 0x9e3779b9u * static_cast<std::uint32_t>(i + 1);
        card.osc1Cents = 3.2f * hashBipolar(seed + 1u);
        card.osc2Cents = 4.1f * hashBipolar(seed + 2u);
        card.cutoffError = hashBipolar(seed + 3u);
        card.resonanceError = hashBipolar(seed + 4u);
        card.envelopeError = hashBipolar(seed + 5u);
        card.driveError = hashBipolar(seed + 6u);
        card.panError = hashBipolar(seed + 7u);
        card.pulseSkew = hashBipolar(seed + 8u);
        card.driftRate = 0.031f + 0.091f * (0.5f + 0.5f * hashBipolar(seed + 9u));
        card.driftDepth = 0.72f + 0.56f * (0.5f + 0.5f * hashBipolar(seed + 10u));
        card.driftPhase = 0.5f + 0.5f * hashBipolar(seed + 11u);
    }
}

float MarsEngine::midiToHz(float midiNote) noexcept
{
    return 440.0f * std::exp2((midiNote - 69.0f) / 12.0f);
}

float MarsEngine::wrapPhase(float phase) noexcept
{
    return phase - std::floor(phase);
}

float MarsEngine::smoothStep(float value) noexcept
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

float MarsEngine::envelopeCoefficient(float seconds, float sampleRate) noexcept
{
    seconds = std::max(seconds, 0.0005f);
    return 1.0f - std::exp(-envelopeRange / (seconds * sampleRate));
}

float MarsEngine::polyBlep(float phase, float increment) noexcept
{
    increment = std::clamp(increment, 0.0000001f, 0.499f);
    if (phase < increment)
    {
        const float x = phase / increment;
        return x + x - x * x - 1.0f;
    }
    if (phase > 1.0f - increment)
    {
        const float x = (phase - 1.0f) / increment;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

float MarsEngine::softSaturate(float value) noexcept
{
    value = std::clamp(value, -3.0f, 3.0f);
    const float square = value * value;
    return value * (27.0f + square) / (27.0f + 9.0f * square);
}

float MarsEngine::softSaturateDerivative(float value) noexcept
{
    if (value <= -3.0f || value >= 3.0f)
        return 0.0f;
    const float square = value * value;
    const float numerator = 27.0f * value + value * square;
    const float denominator = 27.0f + 9.0f * square;
    const float numeratorDerivative = 27.0f + 3.0f * square;
    const float denominatorDerivative = 18.0f * value;
    return (numeratorDerivative * denominator - numerator * denominatorDerivative)
         / (denominator * denominator);
}

float MarsEngine::adaaShape(float value) noexcept
{
    // A smooth diode-like transfer whose antiderivative is cheap and stable.
    // The bounded input also protects the divided difference under hostile
    // automation or corrupt host state.
    value = std::clamp(value, -12.0f, 12.0f);
    return value / std::sqrt(1.0f + value * value);
}

float MarsEngine::adaaAntiderivative(float value) noexcept
{
    value = std::clamp(value, -12.0f, 12.0f);
    return std::sqrt(1.0f + value * value);
}

float MarsEngine::processAdaaMixer(float value, Voice& voice) noexcept
{
    value = std::clamp(finiteOr(value, 0.0f), -12.0f, 12.0f);
    if (!voice.mixerInitialised)
    {
        voice.previousMixerInput = value;
        voice.mixerInitialised = true;
        return adaaShape(value);
    }

    const float previous = voice.previousMixerInput;
    const float difference = value - previous;
    voice.previousMixerInput = value;

    // First-order ADAA. Around coincident samples the quotient loses precision,
    // so evaluate the underlying nonlinearity at the midpoint instead.
    float output = std::abs(difference) < 1.0e-4f * (1.0f + std::abs(value))
        ? adaaShape(0.5f * (value + previous))
        : (adaaAntiderivative(value) - adaaAntiderivative(previous)) / difference;
    if (!std::isfinite(output))
        output = adaaShape(value);
    return output;
}

float MarsEngine::filterInputVoltage(float value, float drive) noexcept
{
    value = std::clamp(finiteOr(value, 0.0f), -4.0f, 4.0f);
    drive = std::clamp(finiteOr(drive, 0.0f), 0.0f, 1.0f);

    // The D'Angelo-Valimaki circuit equations operate in volts and use a
    // transistor thermal voltage of 26 mV. This maps the panel's 0..1 drive
    // range to roughly -5 dB .. +13 dB around 2*VT, once and only once at the
    // filter input. Mixer and VCA transfer functions do not follow this knob.
    const float levelInThermalPairs = 0.55f * std::exp2(3.0f * drive);
    return value * twiceThermalVoltage * levelInThermalPairs;
}

int MarsEngine::layersForMode(const EngineParameters& parameters) const noexcept
{
    if (parameters.voiceMode == VoiceMode::Unison)
        return std::clamp(parameters.unisonVoices, 2, 8);
    if (parameters.voiceMode == VoiceMode::Fifth)
        return 2;
    return 1;
}

int MarsEngine::fifthIntervalForLayer(int layer) const noexcept
{
    return layer == 0 ? 0 : 7;
}

int MarsEngine::findFreeVoice() const noexcept
{
    for (int i = 0; i < maxVoices; ++i)
        if (!voices_[static_cast<std::size_t>(i)].active)
            return i;
    return -1;
}

void MarsEngine::makeRoomFor(int required) noexcept
{
    int freeVoices = 0;
    for (const auto& voice : voices_)
        freeVoices += voice.active ? 0 : 1;

    while (freeVoices < required)
    {
        int candidate = -1;
        std::uint64_t oldestGeneration = std::numeric_limits<std::uint64_t>::max();
        for (int i = 0; i < maxVoices; ++i)
        {
            const auto& voice = voices_[static_cast<std::size_t>(i)];
            if (voice.active && voice.releasing && voice.generation < oldestGeneration)
            {
                candidate = i;
                oldestGeneration = voice.generation;
            }
        }
        if (candidate < 0)
        {
            for (int i = 0; i < maxVoices; ++i)
            {
                const auto& voice = voices_[static_cast<std::size_t>(i)];
                if (voice.active && voice.generation < oldestGeneration)
                {
                    candidate = i;
                    oldestGeneration = voice.generation;
                }
            }
        }
        if (candidate < 0)
            break;

        const auto generation = voices_[static_cast<std::size_t>(candidate)].generation;
        for (auto& voice : voices_)
        {
            if (voice.active && voice.generation == generation)
            {
                silenceVoice(voice, true);
                ++freeVoices;
            }
        }
    }
}

void MarsEngine::initialiseVoice(Voice& voice, int slot, int rootMidi,
                                 int soundingMidi, int layer, int layerCount,
                                 float velocity,
                                 const EngineParameters& parameters) noexcept
{
    voice = Voice {};
    voice.active = true;
    voice.keyDown = true;
    voice.rootMidi = rootMidi;
    voice.soundingMidi = soundingMidi;
    voice.layer = layer;
    voice.cardIndex = slot;
    voice.generation = generation_;
    voice.velocity = velocity;
    voice.targetMidi = static_cast<float>(soundingMidi);
    const float interval = static_cast<float>(soundingMidi - rootMidi);
    voice.currentMidi = parameters.glideSeconds > 0.0f && hasLastPlayedMidi_
        ? std::clamp(lastPlayedMidi_ + interval, 0.0f, 127.0f)
        : voice.targetMidi;

    const float layerPosition = layerCount > 1
        ? 2.0f * static_cast<float>(layer) / static_cast<float>(layerCount - 1) - 1.0f
        : 0.0f;
    if (parameters.voiceMode == VoiceMode::Unison)
    {
        voice.unisonCents = layerPosition * (4.0f + 20.0f * parameters.drift);
        voice.panBase = 0.86f * layerPosition;
        voice.groupGain = 0.72f / std::sqrt(static_cast<float>(layerCount));
    }
    else if (parameters.voiceMode == VoiceMode::Fifth)
    {
        voice.unisonCents = 0.35f * layerPosition;
        voice.panBase = 0.76f * layerPosition;
        voice.groupGain = 0.57f / std::sqrt(static_cast<float>(layerCount));
    }
    else
    {
        voice.groupGain = 0.82f;
        voice.panBase = 0.0f;
    }

    const std::uint32_t seed = hash32(static_cast<std::uint32_t>(generation_)
                                    ^ static_cast<std::uint32_t>(rootMidi * 977 + layer * 131));
    voice.noiseState = seed;
    voice.oscillator1.phase = wrapPhase(0.17f + 0.37f * (0.5f + 0.5f * hashBipolar(seed + 1u)));
    voice.oscillator2.phase = wrapPhase(0.53f + 0.41f * (0.5f + 0.5f * hashBipolar(seed + 2u)));
    voice.subOscillator.phase = wrapPhase(voice.oscillator1.phase * 0.5f);
    // The leaky polyBLEP triangle stores the low-pass integrator state, whose
    // steady-state amplitude is approximately one quarter of the output.
    voice.oscillator1.triangle = 0.25f * triangleAtPhase(voice.oscillator1.phase);
    voice.oscillator2.triangle = 0.25f * triangleAtPhase(voice.oscillator2.phase);
    voice.subOscillator.triangle = 0.25f * triangleAtPhase(voice.subOscillator.phase);
    voice.driftPhase = wrapPhase(cards_[static_cast<std::size_t>(slot)].driftPhase
                               + 0.173f * static_cast<float>(layer));
    voice.ampEnvelope.noteOn();
    voice.filterEnvelope.noteOn();
    voice.controlCountdown = 0;
    updateVoiceControl(voice, parameters, 0.0f);
}

void MarsEngine::noteOn(int midiNote, float velocity)
{
    if (velocity <= 0.0f || !std::isfinite(velocity))
    {
        noteOff(midiNote);
        return;
    }
    if (!prepared_)
        prepare(sampleRate_, 512);

    midiNote = std::clamp(midiNote, 0, 127);
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    const EngineParameters parameters = targetParameters_;
    const int layerCount = layersForMode(parameters);

    // Repeated MIDI notes retrigger as one physical key action.
    for (auto& voice : voices_)
        if (voice.active && voice.rootMidi == midiNote)
            silenceVoice(voice, true);

    for (;;)
    {
        int heldGroups = 0;
        std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
        for (const auto& voice : voices_)
        {
            if (voice.active && voice.keyDown && voice.layer == 0)
            {
                ++heldGroups;
                oldest = std::min(oldest, voice.generation);
            }
        }
        if (heldGroups < 16 || oldest == std::numeric_limits<std::uint64_t>::max())
            break;
        for (auto& voice : voices_)
            if (voice.active && voice.generation == oldest)
                silenceVoice(voice, true);
    }

    makeRoomFor(layerCount);
    ++generation_;
    for (int layer = 0; layer < layerCount; ++layer)
    {
        const int slot = findFreeVoice();
        if (slot < 0)
            break;
        const int interval = parameters.voiceMode == VoiceMode::Fifth
            ? fifthIntervalForLayer(layer) : 0;
        const int soundingMidi = std::clamp(midiNote + interval, 0, 127);
        initialiseVoice(voices_[static_cast<std::size_t>(slot)], slot, midiNote,
                        soundingMidi, layer, layerCount, velocity, parameters);
    }
    lastPlayedMidi_ = static_cast<float>(midiNote);
    hasLastPlayedMidi_ = true;
    updateActiveVoiceCount();
}

void MarsEngine::noteOff(int midiNote)
{
    midiNote = std::clamp(midiNote, 0, 127);
    for (auto& voice : voices_)
    {
        if (voice.active && voice.rootMidi == midiNote)
        {
            voice.keyDown = false;
            if (sustainPedalDown_)
            {
                voice.sustained = true;
            }
            else
            {
                voice.releasing = true;
                voice.ampEnvelope.noteOff();
                voice.filterEnvelope.noteOff();
            }
        }
    }
}

void MarsEngine::allNotesOff()
{
    for (auto& voice : voices_)
    {
        if (!voice.active)
            continue;

        const bool wasHeld = voice.keyDown;
        voice.keyDown = false;
        if (sustainPedalDown_ && wasHeld && !voice.releasing)
        {
            voice.sustained = true;
            continue;
        }

        if (!sustainPedalDown_)
        {
            voice.sustained = false;
            voice.releasing = true;
            voice.ampEnvelope.noteOff();
            voice.filterEnvelope.noteOff();
        }
    }
}

void MarsEngine::setPitchBend(float normalisedBipolar) noexcept
{
    pitchBendTarget_ = std::clamp(finiteOr(normalisedBipolar, 0.0f), -1.0f, 1.0f);
}

void MarsEngine::setModWheel(float amount) noexcept
{
    modWheelTarget_ = std::clamp(finiteOr(amount, 0.0f), 0.0f, 1.0f);
}

void MarsEngine::setSustainPedal(bool down) noexcept
{
    if (sustainPedalDown_ == down)
        return;

    sustainPedalDown_ = down;
    if (down)
        return;

    for (auto& voice : voices_)
    {
        if (voice.active && voice.sustained && !voice.keyDown)
        {
            voice.sustained = false;
            voice.releasing = true;
            voice.ampEnvelope.noteOff();
            voice.filterEnvelope.noteOff();
        }
    }
}

void MarsEngine::addVoiceToStealTail(const Voice& voice) noexcept
{
    if (!voice.active || !std::isfinite(voice.lastOutput))
        return;

    constexpr float maximumTail = 8.0f;
    stealTailLeft_ = std::clamp(stealTailLeft_ + voice.lastOutput * voice.panLeft,
                                -maximumTail, maximumTail);
    stealTailRight_ = std::clamp(stealTailRight_ + voice.lastOutput * voice.panRight,
                                 -maximumTail, maximumTail);
}

void MarsEngine::renderStealTail(float& left, float& right) noexcept
{
    left = stealTailLeft_;
    right = stealTailRight_;
    stealTailLeft_ *= stealTailCoefficient_;
    stealTailRight_ *= stealTailCoefficient_;
    if (std::abs(stealTailLeft_) < 1.0e-7f)
        stealTailLeft_ = 0.0f;
    if (std::abs(stealTailRight_) < 1.0e-7f)
        stealTailRight_ = 0.0f;
}

void MarsEngine::silenceVoice(Voice& voice, bool preserveTail) noexcept
{
    if (preserveTail)
        addVoiceToStealTail(voice);
    voice.active = false;
    voice.releasing = false;
    voice.keyDown = false;
    voice.sustained = false;
    voice.ampEnvelope.reset();
    voice.filterEnvelope.reset();
    voice.ladder.reset();
    voice.stateVariable.reset();
}

void MarsEngine::updateActiveVoiceCount() noexcept
{
    int count = 0;
    for (const auto& voice : voices_)
        count += voice.active ? 1 : 0;
    activeVoiceCount_ = count;
}

float MarsEngine::nextNoise(Voice& voice) noexcept
{
    std::uint32_t value = voice.noiseState;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    voice.noiseState = value == 0u ? 1u : value;
    return static_cast<float>(voice.noiseState & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float MarsEngine::nextLfoValue(const EngineParameters& parameters) noexcept
{
    lfoPhase_ += parameters.lfoRateHz * inverseSampleRate_;
    if (lfoPhase_ >= 1.0f)
    {
        lfoPhase_ -= std::floor(lfoPhase_);
        globalNoiseState_ = hash32(globalNoiseState_ + 0x9e3779b9u);
        randomLfoValue_ = hashBipolar(globalNoiseState_);
    }

    if (parameters.lfoWave == LfoWaveform::Sine)
        return std::sin(twoPi * lfoPhase_);
    if (parameters.lfoWave == LfoWaveform::SampleHold)
        return randomLfoValue_;
    return 1.0f - 4.0f * std::abs(lfoPhase_ - 0.5f);
}

void MarsEngine::updateVoiceControl(Voice& voice,
                                    const EngineParameters& parameters,
                                    float lfoValue) noexcept
{
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float ageScale = 0.25f + 0.75f * parameters.drift;
    const float voiceAge = static_cast<float>(voice.ageSamples) / oversampledRate_;
    const float lfoFade = smoothStep(voiceAge / 0.14f);
    const float driftValue = std::sin(twoPi * voice.driftPhase);
    voice.driftPhase = wrapPhase(voice.driftPhase
                               + card.driftRate * static_cast<float>(controlPeriod) / oversampledRate_);

    if (parameters.glideSeconds <= 0.0005f)
    {
        voice.currentMidi = voice.targetMidi;
    }
    else
    {
        const float glideCoefficient = 1.0f - std::exp(
            -envelopeRange * static_cast<float>(controlPeriod)
            / (parameters.glideSeconds * oversampledRate_));
        voice.currentMidi += glideCoefficient * (voice.targetMidi - voice.currentMidi);
    }

    const float wanderCents = parameters.drift * card.driftDepth * (2.6f * driftValue);
    const float lfoCents = (parameters.lfoPitchCents + 40.0f * modWheel_)
                         * lfoFade * lfoValue;
    const float oscillator1Cents = voice.unisonCents + ageScale * card.osc1Cents
                                 + wanderCents + lfoCents;
    const float oscillator2Cents = -0.37f * voice.unisonCents + parameters.osc2FineCents
                                 + ageScale * card.osc2Cents - 0.72f * wanderCents
                                 + lfoCents;
    const float bendSemitones = 2.0f * pitchBend_;
    const float oscillator1Note = voice.currentMidi + static_cast<float>(12 * parameters.osc1Octave)
                                + bendSemitones
                                + oscillator1Cents / 100.0f;
    const float oscillator2Note = voice.currentMidi
                                + static_cast<float>(12 * parameters.osc2Octave
                                                     + parameters.osc2Semitones)
                                + bendSemitones
                                + oscillator2Cents / 100.0f;
    const float oscillator1Target = std::clamp(midiToHz(oscillator1Note) / oversampledRate_,
                                               0.0000001f, 0.45f);
    const float oscillator2Target = std::clamp(midiToHz(oscillator2Note) / oversampledRate_,
                                               0.0000001f, 0.45f);
    const float subTarget = std::max(0.0000001f, 0.5f * oscillator1Target);

    const float keyOctaves = parameters.filterKeyTrack
                           * (voice.currentMidi - 60.0f) / 12.0f;
    const float envelopeOctaves = 5.4f * parameters.filterEnvAmount
                                * voice.filterEnvelope.value;
    const float lfoOctaves = (parameters.lfoFilterOctaves + 0.5f * modWheel_)
                           * lfoFade * lfoValue;
    const float componentOctaves = ageScale * (0.075f * card.cutoffError)
                                 + parameters.drift * 0.028f * driftValue;
    const float maximumCutoff = 0.45f * static_cast<float>(sampleRate_);
    const float cutoff = std::clamp(parameters.cutoffHz
                                    * std::exp2(keyOctaves + envelopeOctaves
                                                + lfoOctaves + componentOctaves),
                                    18.0f, maximumCutoff);
    const float stateTarget = std::tan(pi * cutoff / oversampledRate_);
    const float ladderTarget = stateTarget;

    const float panMotion = parameters.voiceMode == VoiceMode::Fifth
        ? 0.16f * std::sin(twoPi * (voice.driftPhase + 0.19f * static_cast<float>(voice.layer)))
        : 0.0f;
    const float pan = std::clamp(parameters.spread
                                 * (voice.panBase + panMotion + 0.12f * ageScale * card.panError),
                                 -1.0f, 1.0f);
    const float targetLeft = std::sqrt(0.5f * (1.0f - pan));
    const float targetRight = std::sqrt(0.5f * (1.0f + pan));

    if (!voice.controlInitialised)
    {
        voice.oscillator1Increment = oscillator1Target;
        voice.oscillator2Increment = oscillator2Target;
        voice.subIncrement = subTarget;
        voice.ladderGain = ladderTarget;
        voice.stateGain = stateTarget;
        voice.panLeft = targetLeft;
        voice.panRight = targetRight;
        voice.controlInitialised = true;
    }

    const float period = static_cast<float>(controlPeriod);
    voice.oscillator1Step = (oscillator1Target - voice.oscillator1Increment) / period;
    voice.oscillator2Step = (oscillator2Target - voice.oscillator2Increment) / period;
    voice.subStep = (subTarget - voice.subIncrement) / period;
    voice.ladderGainStep = (ladderTarget - voice.ladderGain) / period;
    voice.stateGainStep = (stateTarget - voice.stateGain) / period;
    voice.panLeftStep = (targetLeft - voice.panLeft) / period;
    voice.panRightStep = (targetRight - voice.panRight) / period;
    voice.resonance = std::clamp(parameters.resonance
                                 + 0.045f * ageScale * card.resonanceError, 0.0f, 0.995f);
    // D'Angelo-Valimaki resonance compensation is constant between control
    // updates. Hoisting its square roots out of every oversampled ladder step
    // materially reduces the 32-voice worst-case cost without changing the
    // circuit equations.
    constexpr float cosPiOverFour = 0.7071067811865475f;
    voice.ladderFeedbackGain = 4.0f * voice.resonance;
    const float fourthRootK = std::sqrt(std::sqrt(voice.ladderFeedbackGain));
    const float alphaSquared = 1.0f + std::sqrt(voice.ladderFeedbackGain)
                             - 2.0f * fourthRootK * cosPiOverFour;
    voice.ladderFrequencyScale = 1.0f
                               / std::sqrt(std::max(alphaSquared, 1.0e-8f));
    voice.drive = std::clamp(parameters.filterDrive
                             * (1.0f + 0.16f * ageScale * card.driveError), 0.0f, 1.0f);

    const float envelopeScale = std::clamp(1.0f + 0.14f * ageScale * card.envelopeError,
                                           0.72f, 1.28f);
    voice.ampAttackCoefficient = envelopeCoefficient(parameters.ampAttack * envelopeScale,
                                                      oversampledRate_);
    voice.ampDecayCoefficient = envelopeCoefficient(parameters.ampDecay * envelopeScale,
                                                     oversampledRate_);
    voice.ampReleaseCoefficient = envelopeCoefficient(parameters.ampRelease * envelopeScale,
                                                       oversampledRate_);
    voice.filterAttackCoefficient = envelopeCoefficient(parameters.filterAttack * envelopeScale,
                                                         oversampledRate_);
    voice.filterDecayCoefficient = envelopeCoefficient(parameters.filterDecay * envelopeScale,
                                                        oversampledRate_);
    voice.filterReleaseCoefficient = envelopeCoefficient(parameters.filterRelease * envelopeScale,
                                                          oversampledRate_);
}

float MarsEngine::renderOscillator(Oscillator& oscillator, OscillatorWave waveform,
                                   float increment, float pulseWidth,
                                   bool& wrapped) noexcept
{
    increment = std::clamp(increment, 0.0000001f, 0.45f);
    pulseWidth = std::clamp(pulseWidth, 0.05f, 0.95f);
    const float phase = oscillator.phase;
    float output = 0.0f;
    if (waveform == OscillatorWave::Pulse)
    {
        oscillator.sawContourInitialised = false;
        const float shiftedPulsePhase = wrapPhase(phase - pulseWidth);
        output = (phase < pulseWidth ? 1.0f : -1.0f)
               + polyBlep(phase, increment)
               - polyBlep(shiftedPulsePhase, increment);
        oscillator.triangle = 0.25f * triangleAtPhase(phase);
    }
    else if (waveform == OscillatorWave::Triangle)
    {
        oscillator.sawContourInitialised = false;
        // A one-pole leaky integrator of the BLEP-corrected square is the
        // standard stable polyBLEP triangle construction. Unlike an unbounded
        // accumulator it cannot collect floating-point DC error over long pads.
        const float triangleSquare = (phase < 0.5f ? 1.0f : -1.0f)
                                   + polyBlep(phase, increment)
                                   - polyBlep(wrapPhase(phase - 0.5f), increment);
        oscillator.triangle = increment * triangleSquare
                            + (1.0f - increment) * oscillator.triangle;
        oscillator.triangle = std::clamp(finiteOr(oscillator.triangle, 0.0f),
                                         -0.35f, 0.35f);
        output = 4.0f * oscillator.triangle;
    }
    else
    {
        output = 2.0f * phase - 1.0f - polyBlep(phase, increment);
        oscillator.triangle = 0.25f * triangleAtPhase(phase);

        // Frequency-dependent first-order post-equalisation fitted by
        // Pekonen et al. to recorded Minimoog Voyager sawtooth spectra:
        // J. Pekonen et al., "Discrete-Time Modelling of the Moog Sawtooth
        // Oscillator Waveform," EURASIP JASP, 2011, Article 785103.
        // Their coefficients below are for an ideally bandlimited saw source;
        // Mars applies the same stable contour to its polyBLEP saw while making
        // no measured-hardware claim for pulse or triangle.
        const float oscillatorFrequency = increment * oversampledRate_;
        const float fittedFrequency = std::clamp(oscillatorFrequency, 86.0f, 8300.0f);
        const float frequencySquared = fittedFrequency * fittedFrequency;
        const float referenceGain = 0.5400f + 4.473e-5f * fittedFrequency;
        const float referenceZero = 0.3894f - 3.102e-4f * fittedFrequency
                                  + 2.417e-8f * frequencySquared;
        const float referencePole = 0.6398f - 2.417e-4f * fittedFrequency
                                  + 1.335e-8f * frequencySquared;

        // The identified coefficients use a 44.1 kHz discrete-time base.
        // Bilinear-remap their pole and zero to the current internal rate, then
        // restore the fitted DC gain. Without this step, enabling oversampling
        // moves the contour's analogue corner and changes the same note's tone.
        constexpr float referenceRate = 44100.0f;
        const float rateRatio = referenceRate / oversampledRate_;
        const auto remapCoefficient = [rateRatio] (float coefficient) noexcept
        {
            return ((1.0f + rateRatio) * coefficient + (1.0f - rateRatio))
                 / ((1.0f - rateRatio) * coefficient + (1.0f + rateRatio));
        };
        const float zero = remapCoefficient(referenceZero);
        const float pole = remapCoefficient(referencePole);
        const float referenceDcGain = referenceGain * (1.0f - referenceZero)
                                    / (1.0f - referencePole);
        const float gain = referenceDcGain * (1.0f - pole) / (1.0f - zero);

        if (!oscillator.sawContourInitialised)
        {
            oscillator.previousSawInput = output;
            oscillator.previousSawOutput = output;
            oscillator.sawContourInitialised = true;
        }
        else
        {
            const float rawSaw = output;
            const float contoured = gain
                                  * (rawSaw - zero * oscillator.previousSawInput)
                                  + pole * oscillator.previousSawOutput;
            oscillator.previousSawInput = rawSaw;
            oscillator.previousSawOutput = std::isfinite(contoured) ? contoured : rawSaw;

            // Pekonen et al.'s fitted coefficients begin at about 86 Hz. Fade
            // back to the neutral antialiased saw over the octave below that
            // boundary instead of freezing an out-of-range analogue contour
            // across every deep note. This keeps bass pitch and weight clean
            // while preserving the measured character throughout its domain.
            const float contourBlend = smoothStep((oscillatorFrequency - 43.0f) / 43.0f);
            output = rawSaw + contourBlend * (oscillator.previousSawOutput - rawSaw);
        }
    }

    oscillator.phase += increment;
    wrapped = oscillator.phase >= 1.0f;
    if (wrapped)
        oscillator.phase -= std::floor(oscillator.phase);
    return output;
}

float MarsEngine::renderVoiceOversample(Voice& voice,
                                        const EngineParameters& parameters,
                                        float lfoValue) noexcept
{
    if (!voice.active)
        return 0.0f;

    if (--voice.controlCountdown <= 0)
    {
        updateVoiceControl(voice, parameters, lfoValue);
        voice.controlCountdown = controlPeriod;
    }

    voice.oscillator1Increment += voice.oscillator1Step;
    voice.oscillator2Increment += voice.oscillator2Step;
    voice.subIncrement += voice.subStep;
    voice.ladderGain += voice.ladderGainStep;
    voice.stateGain += voice.stateGainStep;
    voice.panLeft += voice.panLeftStep;
    voice.panRight += voice.panRightStep;

    const float ampEnvelope = voice.ampEnvelope.tick(voice.ampAttackCoefficient,
                                                     voice.ampDecayCoefficient,
                                                     parameters.ampSustain,
                                                     voice.ampReleaseCoefficient);
    voice.filterEnvelope.tick(voice.filterAttackCoefficient,
                              voice.filterDecayCoefficient,
                              parameters.filterSustain,
                              voice.filterReleaseCoefficient);

    const float voiceAge = static_cast<float>(voice.ageSamples) / oversampledRate_;
    const float lfoFade = smoothStep(voiceAge / 0.14f);
    const auto& card = cards_[static_cast<std::size_t>(voice.cardIndex)];
    const float ageScale = 0.25f + 0.75f * parameters.drift;
    const float pulseWidth = std::clamp(parameters.pulseWidth
                                        + 0.42f * parameters.lfoPwm * lfoFade * lfoValue
                                        + 0.035f * ageScale * card.pulseSkew,
                                        0.05f, 0.95f);

    bool oscillator2Wrapped = false;
    float oscillator2 = renderOscillator(voice.oscillator2, parameters.osc2Wave,
                                         voice.oscillator2Increment, pulseWidth,
                                         oscillator2Wrapped);
    (void) oscillator2Wrapped;

    // Cross-mod changes VCO I's instantaneous frequency while VCO II keeps
    // running independently of its audio mixer gate. The lower bound prevents
    // phase reversal and the effective increment is also the value seen by the
    // BLEP event correction.
    // A variable-width pulse has mean 2*PW-1. Keep that physical DC in the
    // audio mixer/filter path, but remove it from the exponential-frequency
    // modulation source so PWM changes timbre rather than average VCO I pitch.
    const float modulationOscillator = parameters.osc2Wave == OscillatorWave::Pulse
        ? oscillator2 - (2.0f * pulseWidth - 1.0f)
        : oscillator2;
    const float frequencyModulation = std::clamp(
        1.0f + 0.72f * parameters.crossMod * modulationOscillator, 0.20f, 1.80f);
    const float oscillator1Increment = voice.oscillator1Increment
                                     * frequencyModulation;
    bool oscillator1Wrapped = false;
    float oscillator1 = renderOscillator(voice.oscillator1, parameters.osc1Wave,
                                         oscillator1Increment, pulseWidth,
                                         oscillator1Wrapped);

    (void) oscillator1Wrapped;

    bool subWrapped = false;
    const float sub = renderOscillator(voice.subOscillator, OscillatorWave::Pulse,
                                       voice.subIncrement, 0.5f, subWrapped);
    (void) subWrapped;

    const float shaping = 1.05f + 0.28f * parameters.drift;
    const float normalisation = std::max(0.2f, softSaturate(shaping));
    oscillator1 = softSaturate(oscillator1 * shaping) / normalisation;
    oscillator2 = softSaturate(oscillator2 * shaping) / normalisation;

    const float whiteNoise = nextNoise(voice);
    const float noiseCoefficient = std::clamp(twoPi * 5200.0f / oversampledRate_, 0.001f, 0.42f);
    voice.noiseColour += noiseCoefficient * (whiteNoise - voice.noiseColour);
    const float filteredNoise = 0.62f * whiteNoise + 0.38f * voice.noiseColour;
    const float rawMix = oscillator1MixGain_ * oscillator1
                       + oscillator2MixGain_ * oscillator2
                       + parameters.subLevel * 0.72f * sub
                       + parameters.noiseLevel * filteredNoise;
    const float mixed = processAdaaMixer(rawMix, voice);

    if (!voice.filterModelInitialised)
    {
        voice.activeFilterModel = parameters.filterModel;
        voice.filterBlend = parameters.filterModel == FilterModel::Sem ? 1.0f : 0.0f;
        voice.filterBlendStep = 0.0f;
        voice.filterCrossfadeRemaining = 0;
        voice.filterModelInitialised = true;
    }
    else if (voice.activeFilterModel != parameters.filterModel)
    {
        // At a steady endpoint the inactive model has stale state, so restart
        // it silently under a zero-gain side of the crossfade. If automation
        // reverses an in-flight transition both models are already current and
        // the blend simply changes direction without a discontinuity.
        if (voice.filterCrossfadeRemaining == 0)
        {
            if (parameters.filterModel == FilterModel::Ladder)
                voice.ladder.reset();
            else
                voice.stateVariable.reset();
        }

        voice.activeFilterModel = parameters.filterModel;
        voice.filterCrossfadeRemaining = filterCrossfadeSamples_;
        const float targetBlend = parameters.filterModel == FilterModel::Sem ? 1.0f : 0.0f;
        voice.filterBlendStep = (targetBlend - voice.filterBlend)
                              / static_cast<float>(filterCrossfadeSamples_);
    }

    float filtered = 0.0f;
    const float inputVoltage = filterInputVoltage(mixed, voice.drive);
    const auto processLadder = [&voice, inputVoltage]() noexcept
    {
        return voice.ladder.process(inputVoltage, voice.ladderGain,
                                    voice.ladderFeedbackGain,
                                    voice.ladderFrequencyScale);
    };
    const auto processSem = [&voice, &parameters, inputVoltage]() noexcept
    {
        float low = 0.0f;
        float band = 0.0f;
        float high = 0.0f;
        const float semInput = softSaturate(inputVoltage / twiceThermalVoltage);
        voice.stateVariable.process(semInput, voice.stateGain, voice.resonance,
                                    low, band, high);

        // The original SEM's 50 kOhm mode pot is a linear crossfade between
        // simultaneous low- and high-pass outputs. Its centre is therefore a
        // half-level notch sum, 0.5 * (low + high), rather than band-pass.
        // The filter core remains SEM-inspired rather than component-level.
        (void) band;
        const float shape = std::clamp(parameters.filterShape, 0.0f, 1.0f);
        return (1.0f - shape) * low + shape * high;
    };

    if (voice.filterCrossfadeRemaining > 0)
    {
        const float ladderOutput = processLadder();
        const float semOutput = processSem();
        filtered = (1.0f - voice.filterBlend) * ladderOutput
                 + voice.filterBlend * semOutput;

        --voice.filterCrossfadeRemaining;
        if (voice.filterCrossfadeRemaining == 0)
        {
            voice.filterBlend = voice.activeFilterModel == FilterModel::Sem ? 1.0f : 0.0f;
            voice.filterBlendStep = 0.0f;
        }
        else
        {
            voice.filterBlend = std::clamp(voice.filterBlend + voice.filterBlendStep,
                                           0.0f, 1.0f);
        }
    }
    else if (voice.activeFilterModel == FilterModel::Ladder)
        filtered = processLadder();
    else
        filtered = processSem();

    const float velocityCurve = std::sqrt(voice.velocity);
    const float velocityGain = 1.0f + parameters.velocityAmount * (velocityCurve - 1.0f);
    const float vcaInput = filtered * ampEnvelope * velocityGain * voice.groupGain;
    const float output = softSaturate(vcaInput);
    voice.lastOutput = output;
    ++voice.ageSamples;

    if (voice.ampEnvelope.stage == EnvelopeStage::Idle)
        silenceVoice(voice);
    return output;
}

void MarsEngine::downsampleStereo(float firstLeft, float firstRight,
                                  float secondLeft, float secondRight,
                                  float& outputLeft, float& outputRight) noexcept
{
    constexpr int historySize = 15;
    const auto push = [this](float left, float right)
    {
        oversampleLeftHistory_[static_cast<std::size_t>(oversampleWriteIndex_)] = left;
        oversampleRightHistory_[static_cast<std::size_t>(oversampleWriteIndex_)] = right;
        oversampleWriteIndex_ = (oversampleWriteIndex_ + 1) % historySize;
    };
    push(firstLeft, firstRight);
    push(secondLeft, secondRight);

    const auto filter = [this](const std::array<float, historySize>& history)
    {
        const auto delayed = [this, &history](int samples)
        {
            int index = oversampleWriteIndex_ - 1 - samples;
            if (index < 0)
                index += historySize;
            return history[static_cast<std::size_t>(index)];
        };
        return -0.000269694680235f * (delayed(0) + delayed(14))
               + 0.00939858691896f * (delayed(2) + delayed(12))
               - 0.056935395043f * (delayed(4) + delayed(10))
               + 0.29782827755f * (delayed(6) + delayed(8))
               + 0.499956450509f * delayed(7);
    };

    // 15-tap Kaiser-windowed half-band low-pass, cutoff at the eventual output
    // Nyquist. It follows the summed nonlinear voices, which both avoids 32
    // redundant FIRs and prevents their out-of-band products from folding.
    outputLeft = filter(oversampleLeftHistory_);
    outputRight = filter(oversampleRightHistory_);
}

float MarsEngine::readFractional(const std::array<float, chorusBufferSize>& buffer,
                                 float delaySamples) const noexcept
{
    float position = static_cast<float>(chorusWriteIndex_) - delaySamples;
    while (position < 0.0f)
        position += static_cast<float>(chorusBufferSize);
    const int index = static_cast<int>(position) & (chorusBufferSize - 1);
    const float fraction = position - std::floor(position);
    const float first = buffer[static_cast<std::size_t>(index)];
    const float second = buffer[static_cast<std::size_t>((index + 1) & (chorusBufferSize - 1))];
    return first + fraction * (second - first);
}

void MarsEngine::processChorus(float inputLeft, float inputRight,
                               const EngineParameters& parameters,
                               float& outputLeft, float& outputRight) noexcept
{
    chorusLeft_[static_cast<std::size_t>(chorusWriteIndex_)] = inputLeft;
    chorusRight_[static_cast<std::size_t>(chorusWriteIndex_)] = inputRight;

    const float baseDelay = 0.0095f;
    const float depth = 0.0028f;
    chorusPhase_ = wrapPhase(chorusPhase_ + parameters.chorusRateHz * inverseSampleRate_);
    const float leftMod = std::sin(twoPi * chorusPhase_);
    const float rightMod = std::sin(twoPi * wrapPhase(chorusPhase_ + 0.31f));
    float wetLeft = readFractional(chorusRight_, (baseDelay + depth * leftMod)
                                                  * static_cast<float>(sampleRate_));
    float wetRight = readFractional(chorusLeft_, (baseDelay + depth * rightMod)
                                                   * static_cast<float>(sampleRate_));
    const float bandwidthCoefficient = std::clamp(
        1.0f - std::exp(-twoPi * 7200.0f * inverseSampleRate_), 0.01f, 0.72f);
    chorusLowLeft_ += bandwidthCoefficient * (wetLeft - chorusLowLeft_);
    chorusLowRight_ += bandwidthCoefficient * (wetRight - chorusLowRight_);
    wetLeft = softSaturate(1.08f * chorusLowLeft_);
    wetRight = softSaturate(1.08f * chorusLowRight_);

    chorusWriteIndex_ = (chorusWriteIndex_ + 1) & (chorusBufferSize - 1);
    const float mix = parameters.chorusMix;
    outputLeft = (1.0f - 0.16f * mix) * inputLeft + 0.74f * mix * wetLeft;
    outputRight = (1.0f - 0.16f * mix) * inputRight + 0.74f * mix * wetRight;
}

float MarsEngine::processDcBlocker(float input, float& previousInput,
                                   float& previousOutput) const noexcept
{
    const float output = input - previousInput + dcCoefficient_ * previousOutput;
    if (std::abs(input) < 1.0e-10f && std::abs(output) < 1.0e-4f)
    {
        previousInput = 0.0f;
        previousOutput = 0.0f;
        return 0.0f;
    }
    previousInput = input;
    previousOutput = output;
    return output;
}

void MarsEngine::process(float* left, float* right, int numSamples)
{
    if (numSamples <= 0)
        return;
    if (!prepared_)
        prepare(sampleRate_, numSamples);

    applyPendingOversamplingIfIdle();

    const EngineParameters targets = targetParameters_;
    smoothedParameters_.voiceMode = targets.voiceMode;
    smoothedParameters_.osc1Wave = targets.osc1Wave;
    smoothedParameters_.osc2Wave = targets.osc2Wave;
    smoothedParameters_.osc1Enabled = targets.osc1Enabled;
    smoothedParameters_.osc2Enabled = targets.osc2Enabled;
    smoothedParameters_.filterModel = targets.filterModel;
    smoothedParameters_.lfoWave = targets.lfoWave;
    smoothedParameters_.unisonVoices = targets.unisonVoices;
    smoothedParameters_.osc1Octave = targets.osc1Octave;
    smoothedParameters_.osc2Octave = targets.osc2Octave;
    smoothedParameters_.osc2Semitones = targets.osc2Semitones;

    const float smoothing = 1.0f - std::exp(-inverseSampleRate_ / 0.014f);
    const float performanceSmoothing = 1.0f - std::exp(-inverseSampleRate_ / 0.005f);
    const float mixerGateSmoothing = 1.0f - std::exp(-inverseSampleRate_ / 0.004f);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto smooth = [smoothing](float& current, float target)
        {
            current += smoothing * (target - current);
        };
        smooth(smoothedParameters_.osc2FineCents, targets.osc2FineCents);
        smooth(smoothedParameters_.pulseWidth, targets.pulseWidth);
        smooth(smoothedParameters_.crossMod, targets.crossMod);
        smooth(smoothedParameters_.oscMix, targets.oscMix);
        smooth(smoothedParameters_.subLevel, targets.subLevel);
        smooth(smoothedParameters_.noiseLevel, targets.noiseLevel);
        smooth(smoothedParameters_.cutoffHz, targets.cutoffHz);
        smooth(smoothedParameters_.resonance, targets.resonance);
        smooth(smoothedParameters_.filterDrive, targets.filterDrive);
        smooth(smoothedParameters_.filterShape, targets.filterShape);
        smooth(smoothedParameters_.filterEnvAmount, targets.filterEnvAmount);
        smooth(smoothedParameters_.filterKeyTrack, targets.filterKeyTrack);
        smooth(smoothedParameters_.ampAttack, targets.ampAttack);
        smooth(smoothedParameters_.ampDecay, targets.ampDecay);
        smooth(smoothedParameters_.ampSustain, targets.ampSustain);
        smooth(smoothedParameters_.ampRelease, targets.ampRelease);
        smooth(smoothedParameters_.filterAttack, targets.filterAttack);
        smooth(smoothedParameters_.filterDecay, targets.filterDecay);
        smooth(smoothedParameters_.filterSustain, targets.filterSustain);
        smooth(smoothedParameters_.filterRelease, targets.filterRelease);
        smooth(smoothedParameters_.lfoRateHz, targets.lfoRateHz);
        smooth(smoothedParameters_.lfoPitchCents, targets.lfoPitchCents);
        smooth(smoothedParameters_.lfoPwm, targets.lfoPwm);
        smooth(smoothedParameters_.lfoFilterOctaves, targets.lfoFilterOctaves);
        smooth(smoothedParameters_.drift, targets.drift);
        smooth(smoothedParameters_.spread, targets.spread);
        smooth(smoothedParameters_.glideSeconds, targets.glideSeconds);
        smooth(smoothedParameters_.velocityAmount, targets.velocityAmount);
        smooth(smoothedParameters_.chorusMix, targets.chorusMix);
        smooth(smoothedParameters_.chorusRateHz, targets.chorusRateHz);
        smooth(smoothedParameters_.outputGain, targets.outputGain);
        pitchBend_ += performanceSmoothing * (pitchBendTarget_ - pitchBend_);
        modWheel_ += performanceSmoothing * (modWheelTarget_ - modWheel_);
        // Balance is equal-power only while both mixer inputs are enabled.
        // A lone VCO is unity-gain at every Balance setting, matching a
        // hardware mixer switch. The VCOs themselves keep advancing and the
        // roughly 4 ms gain transition makes host automation click-resistant.
        float oscillator1TargetGain = 0.0f;
        float oscillator2TargetGain = 0.0f;
        if (smoothedParameters_.osc1Enabled && smoothedParameters_.osc2Enabled)
        {
            oscillator1TargetGain = std::sqrt(
                std::max(0.0f, 1.0f - smoothedParameters_.oscMix));
            oscillator2TargetGain = std::sqrt(
                std::max(0.0f, smoothedParameters_.oscMix));
        }
        else
        {
            oscillator1TargetGain = smoothedParameters_.osc1Enabled ? 1.0f : 0.0f;
            oscillator2TargetGain = smoothedParameters_.osc2Enabled ? 1.0f : 0.0f;
        }
        oscillator1MixGain_ += mixerGateSmoothing
                             * (oscillator1TargetGain - oscillator1MixGain_);
        oscillator2MixGain_ += mixerGateSmoothing
                             * (oscillator2TargetGain - oscillator2MixGain_);
        if (std::abs(oscillator1MixGain_ - oscillator1TargetGain) < 1.0e-5f)
            oscillator1MixGain_ = oscillator1TargetGain;
        if (std::abs(oscillator2MixGain_ - oscillator2TargetGain) < 1.0e-5f)
            oscillator2MixGain_ = oscillator2TargetGain;

        const float lfoValue = nextLfoValue(smoothedParameters_);
        float dryLeft = 0.0f;
        float dryRight = 0.0f;
        if (oversampling_ == 2)
        {
            float firstLeft = 0.0f;
            float firstRight = 0.0f;
            float secondLeft = 0.0f;
            float secondRight = 0.0f;
            for (auto& voice : voices_)
            {
                if (!voice.active)
                    continue;
                const float rendered = renderVoiceOversample(
                    voice, smoothedParameters_, lfoValue);
                firstLeft += rendered * voice.panLeft;
                firstRight += rendered * voice.panRight;
            }
            float tailLeft = 0.0f;
            float tailRight = 0.0f;
            renderStealTail(tailLeft, tailRight);
            firstLeft += tailLeft;
            firstRight += tailRight;
            for (auto& voice : voices_)
            {
                if (!voice.active)
                    continue;
                const float rendered = renderVoiceOversample(
                    voice, smoothedParameters_, lfoValue);
                secondLeft += rendered * voice.panLeft;
                secondRight += rendered * voice.panRight;
            }
            renderStealTail(tailLeft, tailRight);
            secondLeft += tailLeft;
            secondRight += tailRight;
            downsampleStereo(firstLeft, firstRight, secondLeft, secondRight,
                             dryLeft, dryRight);
        }
        else
        {
            // Above 48 kHz the host rate is already in the HQ nonlinear-island
            // range; running exactly one internal step preserves pitch and
            // envelope timing without needless CPU work.
            for (auto& voice : voices_)
            {
                if (!voice.active)
                    continue;

                const float rendered = renderVoiceOversample(
                    voice, smoothedParameters_, lfoValue);
                dryLeft += rendered * voice.panLeft;
                dryRight += rendered * voice.panRight;
            }
            float tailLeft = 0.0f;
            float tailRight = 0.0f;
            renderStealTail(tailLeft, tailRight);
            dryLeft += tailLeft;
            dryRight += tailRight;
        }

        float chorusLeft = 0.0f;
        float chorusRight = 0.0f;
        processChorus(dryLeft, dryRight, smoothedParameters_, chorusLeft, chorusRight);
        float outputLeft = processDcBlocker(chorusLeft * smoothedParameters_.outputGain,
                                            dcInputLeft_, dcOutputLeft_);
        float outputRight = processDcBlocker(chorusRight * smoothedParameters_.outputGain,
                                             dcInputRight_, dcOutputRight_);
        outputLeft = safetyLimit(outputLeft);
        outputRight = safetyLimit(outputRight);

        if (left != nullptr)
            left[sample] = outputLeft;
        if (right != nullptr && right != left)
            right[sample] = outputRight;
        else if (right != nullptr)
            right[sample] = 0.5f * (outputLeft + outputRight);
    }

    updateActiveVoiceCount();
    if (activeVoiceCount_ == 0)
        oversamplingIdleSamples_ = std::min(
            oversamplingIdleSamples_ + numSamples,
            std::max(1, static_cast<int>(std::lround(0.025 * sampleRate_))));
    else
        oversamplingIdleSamples_ = 0;
}

int MarsEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount_;
}

} // namespace mars
