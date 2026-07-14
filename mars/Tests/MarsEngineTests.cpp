#include "DSP/MarsEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace mars
{
struct MarsEngineTestAccess
{
    static float processLadderStep(std::array<float, 4>& state,
                                   float& previousInput,
                                   float input,
                                   float frequencyTangent,
                                   float feedbackGain,
                                   float frequencyScale) noexcept
    {
        MarsEngine::LadderFilter filter;
        filter.stageVoltage = state;
        filter.previousInputVoltage = previousInput;
        // This seam reconstructs a private filter from externally supplied
        // state. Production voices persist their exact LUT history cache;
        // injected test state must force the same one-time recomputation.
        filter.stageTanhValid = false;
        filter.feedbackTanhValid = false;
        const float output = filter.process(input, frequencyTangent,
                                            feedbackGain, frequencyScale);
        state = filter.stageVoltage;
        previousInput = filter.previousInputVoltage;
        return output;
    }

    static int activeVoicesForRoot(const MarsEngine& engine, int midiNote) noexcept
    {
        return static_cast<int>(std::count_if(
            engine.voices_.begin(), engine.voices_.end(),
            [midiNote](const MarsEngine::Voice& voice)
            {
                return voice.active && voice.rootMidi == midiNote;
            }));
    }

    static constexpr int maximumVoiceCount() noexcept
    {
        return MarsEngine::maxVoices;
    }

};
} // namespace mars

namespace
{
constexpr int blockSize = 128;
constexpr double ladderVoltage = 0.052;
int failures = 0;

#if defined(__has_feature)
constexpr bool sanitizerBuild = __has_feature(address_sanitizer)
                             || __has_feature(undefined_behavior_sanitizer);
#elif defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__)
constexpr bool sanitizerBuild = true;
#else
constexpr bool sanitizerBuild = false;
#endif

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

using LadderState = std::array<double, 4>;

struct LadderEvaluation
{
    LadderState stageTanh {};
    LadderState residual {};
    double feedbackDerivative { 0.0 };
    double squaredResidual { 0.0 };
    double maximumResidual { 0.0 };
};

LadderEvaluation evaluateReferenceLadder(const LadderState& candidate,
                                         const LadderState& previous,
                                         double previousInput, double input,
                                         double g, double k)
{
    LadderEvaluation result;
    LadderState previousTanh {};
    for (std::size_t stage = 0; stage < candidate.size(); ++stage)
    {
        result.stageTanh[stage] = std::tanh(candidate[stage] / ladderVoltage);
        previousTanh[stage] = std::tanh(previous[stage] / ladderVoltage);
    }

    const double feedbackTanh = std::tanh(
        (input + k * candidate.back()) / ladderVoltage);
    const double previousFeedbackTanh = std::tanh(
        (previousInput + k * previous.back()) / ladderVoltage);
    result.feedbackDerivative = 1.0 - feedbackTanh * feedbackTanh;
    result.residual[0] = candidate[0] - previous[0]
                       + ladderVoltage * g
                           * (result.stageTanh[0] + previousTanh[0]
                              + feedbackTanh + previousFeedbackTanh);
    for (std::size_t stage = 1; stage < candidate.size(); ++stage)
        result.residual[stage] = candidate[stage] - previous[stage]
                               - ladderVoltage * g
                                   * (result.stageTanh[stage - 1]
                                      + previousTanh[stage - 1]
                                      - result.stageTanh[stage]
                                      - previousTanh[stage]);

    for (const double value : result.residual)
    {
        result.squaredResidual += value * value;
        result.maximumResidual = std::max(result.maximumResidual,
                                          std::abs(value));
    }
    return result;
}

bool solveLinearSystem(std::array<std::array<double, 4>, 4> matrix,
                       LadderState rightHandSide,
                       LadderState& solution)
{
    for (std::size_t column = 0; column < matrix.size(); ++column)
    {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < matrix.size(); ++row)
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column]))
                pivot = row;
        if (std::abs(matrix[pivot][column]) < 1.0e-14)
            return false;
        if (pivot != column)
        {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(rightHandSide[pivot], rightHandSide[column]);
        }

        for (std::size_t row = column + 1; row < matrix.size(); ++row)
        {
            const double factor = matrix[row][column] / matrix[column][column];
            for (std::size_t entry = column; entry < matrix.size(); ++entry)
                matrix[row][entry] -= factor * matrix[column][entry];
            rightHandSide[row] -= factor * rightHandSide[column];
        }
    }

    for (std::size_t reverse = matrix.size(); reverse-- > 0;)
    {
        double value = rightHandSide[reverse];
        for (std::size_t column = reverse + 1; column < matrix.size(); ++column)
            value -= matrix[reverse][column] * solution[column];
        solution[reverse] = value / matrix[reverse][reverse];
    }
    return true;
}

struct LadderReferenceResult
{
    LadderState state {};
    double normalizedResidual { std::numeric_limits<double>::infinity() };
    bool converged { false };
};

LadderReferenceResult solveReferenceLadder(const LadderState& previous,
                                           double previousInput, double input,
                                           double g, double k)
{
    LadderReferenceResult result;
    result.state = previous;
    auto evaluation = evaluateReferenceLadder(result.state, previous,
                                              previousInput, input, g, k);
    constexpr int maximumIterations = 64;
    constexpr int maximumBacktrackingSteps = 20;
    constexpr double tolerance = ladderVoltage * 1.0e-11;

    for (int iteration = 0;
         iteration < maximumIterations
             && evaluation.maximumResidual > tolerance;
         ++iteration)
    {
        std::array<std::array<double, 4>, 4> jacobian {};
        for (std::size_t stage = 0; stage < result.state.size(); ++stage)
        {
            const double derivative = 1.0
                - evaluation.stageTanh[stage] * evaluation.stageTanh[stage];
            jacobian[stage][stage] = 1.0 + g * derivative;
            if (stage > 0)
            {
                const double previousDerivative = 1.0
                    - evaluation.stageTanh[stage - 1]
                        * evaluation.stageTanh[stage - 1];
                jacobian[stage][stage - 1] = -g * previousDerivative;
            }
        }
        jacobian[0][3] = g * k * evaluation.feedbackDerivative;

        LadderState rightHandSide {};
        for (std::size_t stage = 0; stage < rightHandSide.size(); ++stage)
            rightHandSide[stage] = -evaluation.residual[stage];
        LadderState delta {};
        if (!solveLinearSystem(jacobian, rightHandSide, delta))
            break;

        bool accepted = false;
        double scale = 1.0;
        for (int backtrack = 0;
             backtrack < maximumBacktrackingSteps;
             ++backtrack)
        {
            LadderState candidate {};
            for (std::size_t stage = 0; stage < candidate.size(); ++stage)
                candidate[stage] = result.state[stage] + scale * delta[stage];
            const auto trial = evaluateReferenceLadder(candidate, previous,
                                                       previousInput, input, g, k);
            if (trial.squaredResidual < evaluation.squaredResidual)
            {
                result.state = candidate;
                evaluation = trial;
                accepted = true;
                break;
            }
            scale *= 0.5;
        }
        if (!accepted)
            break;
    }

    result.normalizedResidual = evaluation.maximumResidual / ladderVoltage;
    result.converged = result.normalizedResidual < 1.0e-9;
    return result;
}

struct Metrics
{
    double sum { 0.0 };
    double sumSquares { 0.0 };
    double peak { 0.0 };
    double minimum { std::numeric_limits<double>::infinity() };
    double maximum { -std::numeric_limits<double>::infinity() };
    std::size_t samples { 0 };
    std::size_t nonZero { 0 };
    bool finite { true };

    [[nodiscard]] double rms() const noexcept
    {
        return samples == 0 ? 0.0 : std::sqrt(sumSquares / static_cast<double>(samples));
    }

    [[nodiscard]] double mean() const noexcept
    {
        return samples == 0 ? 0.0 : sum / static_cast<double>(samples);
    }

    [[nodiscard]] double variance() const noexcept
    {
        if (samples == 0)
            return 0.0;
        const double average = mean();
        return std::max(0.0, sumSquares / static_cast<double>(samples)
                               - average * average);
    }

    void add(float value) noexcept
    {
        finite = finite && std::isfinite(value);
        if (!std::isfinite(value))
            return;
        const double magnitude = std::abs(static_cast<double>(value));
        peak = std::max(peak, magnitude);
        minimum = std::min(minimum, static_cast<double>(value));
        maximum = std::max(maximum, static_cast<double>(value));
        sum += static_cast<double>(value);
        sumSquares += static_cast<double>(value) * static_cast<double>(value);
        nonZero += magnitude > 1.0e-9 ? 1u : 0u;
        ++samples;
    }
};

Metrics render(mars::MarsEngine& engine, int sampleCount, int discardSamples = 0)
{
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    Metrics metrics;
    int position = 0;
    while (position < sampleCount)
    {
        const int count = std::min(blockSize, sampleCount - position);
        engine.process(left.data(), right.data(), count);
        for (int i = 0; i < count; ++i)
        {
            if (position + i >= discardSamples)
            {
                metrics.add(left[static_cast<std::size_t>(i)]);
                metrics.add(right[static_cast<std::size_t>(i)]);
            }
        }
        position += count;
    }
    return metrics;
}

mars::EngineParameters basicParameters()
{
    mars::EngineParameters p;
    p.osc1Wave = mars::OscillatorWave::Saw;
    p.osc2Wave = mars::OscillatorWave::Pulse;
    p.filterModel = mars::FilterModel::Ladder;
    p.voiceMode = mars::VoiceMode::Poly;
    p.lfoWave = mars::LfoWaveform::Triangle;
    p.osc1Octave = 0;
    p.osc2Octave = 0;
    p.osc2Semitones = 0;
    p.unisonVoices = 4;
    p.oscMix = 0.42f;
    p.osc2FineCents = 4.0f;
    p.pulseWidth = 0.47f;
    p.subLevel = 0.12f;
    p.noiseLevel = 0.01f;
    p.crossMod = 0.04f;
    p.cutoffHz = 5200.0f;
    p.resonance = 0.32f;
    p.filterDrive = 0.22f;
    p.filterShape = 0.18f;
    p.filterEnvAmount = 0.32f;
    p.filterKeyTrack = 0.45f;
    p.filterAttack = 0.006f;
    p.filterDecay = 0.18f;
    p.filterSustain = 0.42f;
    p.filterRelease = 0.11f;
    p.ampAttack = 0.004f;
    p.ampDecay = 0.12f;
    p.ampSustain = 0.82f;
    p.ampRelease = 0.10f;
    p.lfoRateHz = 4.7f;
    p.lfoPitchCents = 6.0f;
    p.lfoFilterOctaves = 0.08f;
    p.lfoPwm = 0.10f;
    p.drift = 0.24f;
    p.spread = 0.55f;
    p.glideSeconds = 0.0f;
    p.velocityAmount = 0.70f;
    p.chorusMix = 0.18f;
    p.chorusRateHz = 0.54f;
    p.outputGain = 0.68f;
    return p;
}

double estimateFrequency(double sampleRate, float pitchBend = 0.0f)
{
    mars::MarsEngine engine;
    engine.prepare(sampleRate, blockSize);
    auto p = basicParameters();
    p.osc1Wave = mars::OscillatorWave::Saw;
    p.oscMix = 0.0f;
    p.subLevel = 0.0f;
    p.noiseLevel = 0.0f;
    p.crossMod = 0.0f;
    p.cutoffHz = 18000.0f;
    p.resonance = 0.0f;
    p.filterDrive = 0.0f;
    p.filterEnvAmount = 0.0f;
    p.filterKeyTrack = 0.0f;
    p.ampAttack = 0.001f;
    p.ampDecay = 0.01f;
    p.ampSustain = 1.0f;
    p.lfoPitchCents = 0.0f;
    p.lfoFilterOctaves = 0.0f;
    p.drift = 0.0f;
    p.chorusMix = 0.0f;
    engine.setParameters(p);
    engine.setPitchBend(pitchBend);
    engine.noteOn(57, 0.8f); // A3, nominally 220 Hz.

    render(engine, static_cast<int>(0.20 * sampleRate));
    const int measurementSamples = static_cast<int>(0.45 * sampleRate);
    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    float previous = 0.0f;
    int positiveCrossings = 0;
    int position = 0;
    while (position < measurementSamples)
    {
        const int count = std::min(blockSize, measurementSamples - position);
        engine.process(left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const float current = left[static_cast<std::size_t>(sample)];
            positiveCrossings += previous <= 0.0f && current > 0.0f ? 1 : 0;
            previous = current;
        }
        position += count;
    }
    return static_cast<double>(positiveCrossings) * sampleRate
         / static_cast<double>(measurementSamples);
}

mars::EngineParameters isolatedOscillatorParameters()
{
    auto p = basicParameters();
    p.osc1Wave = mars::OscillatorWave::Triangle;
    p.oscMix = 0.0f;
    p.subLevel = 0.0f;
    p.noiseLevel = 0.0f;
    p.crossMod = 0.0f;
    p.cutoffHz = 16000.0f;
    p.resonance = 0.0f;
    p.filterDrive = 0.0f;
    p.filterEnvAmount = 0.0f;
    p.filterKeyTrack = 0.0f;
    p.ampAttack = 0.001f;
    p.ampDecay = 0.01f;
    p.ampSustain = 1.0f;
    p.ampRelease = 0.03f;
    p.lfoPitchCents = 0.0f;
    p.lfoFilterOctaves = 0.0f;
    p.lfoPwm = 0.0f;
    p.drift = 0.0f;
    p.spread = 0.0f;
    p.velocityAmount = 0.0f;
    p.chorusMix = 0.0f;
    p.outputGain = 0.72f;
    return p;
}

double measureReleaseDuration(double sampleRate)
{
    mars::MarsEngine engine;
    engine.prepare(sampleRate, 32);
    auto p = isolatedOscillatorParameters();
    p.ampRelease = 0.080f;
    engine.setParameters(p);
    engine.noteOn(60, 0.8f);
    render(engine, static_cast<int>(0.10 * sampleRate));
    engine.noteOff(60);

    std::array<float, 32> left {};
    std::array<float, 32> right {};
    int elapsed = 0;
    const int maximum = static_cast<int>(sampleRate);
    while (engine.getActiveVoiceCount() > 0 && elapsed < maximum)
    {
        engine.process(left.data(), right.data(), static_cast<int>(left.size()));
        elapsed += static_cast<int>(left.size());
    }
    return static_cast<double>(elapsed) / sampleRate;
}

double averageRenderDifference(mars::MarsEngine& first, mars::MarsEngine& second,
                               int sampleCount)
{
    std::array<float, blockSize> firstLeft {};
    std::array<float, blockSize> firstRight {};
    std::array<float, blockSize> secondLeft {};
    std::array<float, blockSize> secondRight {};
    double difference = 0.0;
    int measured = 0;
    while (measured < sampleCount)
    {
        const int count = std::min(blockSize, sampleCount - measured);
        first.process(firstLeft.data(), firstRight.data(), count);
        second.process(secondLeft.data(), secondRight.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            difference += std::abs(static_cast<double>(
                firstLeft[static_cast<std::size_t>(sample)]
                - secondLeft[static_cast<std::size_t>(sample)]));
        }
        measured += count;
    }
    return difference / static_cast<double>(std::max(measured, 1));
}

float processLeftSample(mars::MarsEngine& engine)
{
    float left = 0.0f;
    float right = 0.0f;
    engine.process(&left, &right, 1);
    return left;
}

bool findFlatAudibleSample(mars::MarsEngine& engine, float minimumMagnitude,
                           float maximumStep, float& sample)
{
    float previous = processLeftSample(engine);
    for (int attempt = 0; attempt < 96000; ++attempt)
    {
        const float current = processLeftSample(engine);
        if (std::abs(current) >= minimumMagnitude
            && std::abs(current - previous) <= maximumStep)
        {
            sample = current;
            return true;
        }
        previous = current;
    }
    sample = previous;
    return false;
}

double maximumStepAfter(mars::MarsEngine& engine, float previous, int sampleCount)
{
    double maximum = 0.0;
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const float current = processLeftSample(engine);
        maximum = std::max(maximum,
                           std::abs(static_cast<double>(current - previous)));
        previous = current;
    }
    return maximum;
}

double semShapeDistance(float firstShape, float secondShape)
{
    constexpr double sampleRate = 48000.0;
    mars::MarsEngine first;
    mars::MarsEngine second;
    first.prepare(sampleRate, blockSize);
    second.prepare(sampleRate, blockSize);
    auto firstParameters = basicParameters();
    firstParameters.filterModel = mars::FilterModel::Sem;
    firstParameters.filterShape = firstShape;
    firstParameters.cutoffHz = 1200.0f;
    firstParameters.filterEnvAmount = 0.0f;
    firstParameters.filterKeyTrack = 0.0f;
    firstParameters.resonance = 0.42f;
    firstParameters.chorusMix = 0.0f;
    auto secondParameters = firstParameters;
    secondParameters.filterShape = secondShape;
    first.setParameters(firstParameters);
    second.setParameters(secondParameters);
    first.noteOn(52, 0.82f);
    second.noteOn(52, 0.82f);
    render(first, static_cast<int>(0.20 * sampleRate));
    render(second, static_cast<int>(0.20 * sampleRate));

    std::array<float, blockSize> firstLeft {};
    std::array<float, blockSize> firstRight {};
    std::array<float, blockSize> secondLeft {};
    std::array<float, blockSize> secondRight {};
    double difference = 0.0;
    int samples = 0;
    for (int block = 0; block < 80; ++block)
    {
        first.process(firstLeft.data(), firstRight.data(), blockSize);
        second.process(secondLeft.data(), secondRight.data(), blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            difference += std::abs(static_cast<double>(
                firstLeft[static_cast<std::size_t>(sample)]
                - secondLeft[static_cast<std::size_t>(sample)]));
            ++samples;
        }
    }
    return difference / static_cast<double>(samples);
}

Metrics renderSteadyNote(const mars::EngineParameters& parameters, int midiNote,
                         double warmupSeconds = 0.24,
                         double measurementSeconds = 0.24)
{
    constexpr double rate = 48000.0;
    mars::MarsEngine engine;
    engine.prepare(rate, blockSize);
    engine.setParameters(parameters);
    // Steady-state fixtures should begin with their requested topology and
    // mixer gates, rather than measuring the deliberate automation smoothing
    // from the engine defaults into the fixture.
    engine.reset();
    engine.noteOn(midiNote, 0.82f);
    render(engine, static_cast<int>(warmupSeconds * rate));
    return render(engine, static_cast<int>(measurementSeconds * rate));
}

struct ToneMeasurement
{
    Metrics metrics {};
    double frequencyHz { 0.0 };
};

ToneMeasurement measureDeepTone(mars::OscillatorWave wave, int midiNote,
                                bool oversamplingEnabled)
{
    constexpr double rate = 48000.0;
    constexpr double warmupSeconds = 0.30;
    constexpr double measurementSeconds = 1.50;

    mars::MarsEngine engine;
    engine.prepare(rate, blockSize, oversamplingEnabled);
    auto p = isolatedOscillatorParameters();
    p.osc1Wave = wave;
    p.filterModel = mars::FilterModel::Sem;
    p.filterShape = 0.0f;
    p.cutoffHz = 12000.0f;
    engine.setParameters(p);
    engine.reset();
    engine.noteOn(midiNote, 0.82f);
    render(engine, static_cast<int>(warmupSeconds * rate));

    std::array<float, blockSize> left {};
    std::array<float, blockSize> right {};
    const int measurementSamples = static_cast<int>(measurementSeconds * rate);
    float previous = processLeftSample(engine);
    double firstCrossing = -1.0;
    double lastCrossing = -1.0;
    int crossingCount = 0;
    int position = 0;
    ToneMeasurement result;
    while (position < measurementSamples)
    {
        const int count = std::min(blockSize, measurementSamples - position);
        engine.process(left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const float current = left[static_cast<std::size_t>(sample)];
            result.metrics.add(current);
            if (previous <= 0.0f && current > 0.0f)
            {
                const double denominator = static_cast<double>(current - previous);
                const double fraction = denominator > 1.0e-12
                    ? -static_cast<double>(previous) / denominator : 0.0;
                const double crossing = static_cast<double>(position + sample) + fraction;
                if (firstCrossing < 0.0)
                    firstCrossing = crossing;
                lastCrossing = crossing;
                ++crossingCount;
            }
            previous = current;
        }
        position += count;
    }

    if (crossingCount >= 2 && lastCrossing > firstCrossing)
        result.frequencyHz = static_cast<double>(crossingCount - 1) * rate
                           / (lastCrossing - firstCrossing);
    return result;
}

void testLadderImplicitSolveAgainstReference()
{
    // The ADAA mixer is bounded by one and the maximum filter-drive mapping is
    // 0.052 * 0.55 * 2^3 volts. Exercise that complete reachable input range at
    // low, medium, and maximum prewarped cutoffs, including the resonance value
    // whose compensation produces the largest effective g.
    constexpr float maximumInput = 0.052f * 0.55f * 8.0f;
    constexpr float maximumFrequencyTangent = 6.3137515f; // tan(pi * 0.45)
    constexpr std::array frequencyTangents {
        0.5f, 2.0f, maximumFrequencyTangent
    };
    constexpr std::array resonances {
        0.0f, 0.0625f, 0.28f, 0.50f, 0.90f, 0.995f
    };

    double maximumNormalizedResidual = 0.0;
    double maximumNormalizedStateError = 0.0;
    bool allReferenceStepsConverged = true;
    bool allProductionStepsFinite = true;

    for (const float resonance : resonances)
    {
        const float k = std::min(4.0f * resonance, 3.98f);
        constexpr float cosPiOverFour = 0.7071067811865475f;
        const float fourthRootK = std::sqrt(std::sqrt(k));
        const float alphaSquared = 1.0f + std::sqrt(k)
                                 - 2.0f * fourthRootK * cosPiOverFour;
        const float frequencyScale = 1.0f
                                   / std::sqrt(std::max(alphaSquared, 1.0e-8f));

        for (const float frequencyTangent : frequencyTangents)
        {
            const double g = static_cast<double>(frequencyTangent)
                           * static_cast<double>(frequencyScale);
            for (int pattern = 0; pattern < 3; ++pattern)
            {
                std::array<float, 4> productionState {};
                float previousInput = 0.0f;
                std::uint32_t noiseState = 0x4d595df4u;
                for (int sample = 0; sample < 512; ++sample)
                {
                    float input = 0.0f;
                    if (pattern == 0)
                        input = (sample / 3) % 2 == 0
                            ? maximumInput : -maximumInput;
                    else if (pattern == 1)
                        input = maximumInput * std::sin(
                            2.0f * 3.14159265358979323846f * 0.43f
                                * static_cast<float>(sample));
                    else
                    {
                        noiseState ^= noiseState << 13u;
                        noiseState ^= noiseState >> 17u;
                        noiseState ^= noiseState << 5u;
                        const float unit = static_cast<float>(noiseState & 0x00ffffffu)
                                         / 8388607.5f - 1.0f;
                        input = maximumInput * unit;
                    }

                    LadderState previous {};
                    for (std::size_t stage = 0; stage < previous.size(); ++stage)
                        previous[stage] = productionState[stage];
                    const auto reference = solveReferenceLadder(
                        previous, previousInput, input, g, k);
                    allReferenceStepsConverged = allReferenceStepsConverged
                                               && reference.converged;

                    const float previousInputForResidual = previousInput;
                    mars::MarsEngineTestAccess::processLadderStep(
                        productionState, previousInput, input,
                        frequencyTangent, k, frequencyScale);

                    LadderState actual {};
                    for (std::size_t stage = 0; stage < actual.size(); ++stage)
                    {
                        actual[stage] = productionState[stage];
                        allProductionStepsFinite = allProductionStepsFinite
                                                && std::isfinite(actual[stage]);
                        maximumNormalizedStateError = std::max(
                            maximumNormalizedStateError,
                            std::abs(actual[stage] - reference.state[stage])
                                / ladderVoltage);
                    }
                    const auto residual = evaluateReferenceLadder(
                        actual, previous, previousInputForResidual, input, g, k);
                    maximumNormalizedResidual = std::max(
                        maximumNormalizedResidual,
                        residual.maximumResidual / ladderVoltage);
                }
            }
        }
    }

    std::cout << std::scientific << std::setprecision(3)
              << "Ladder implicit max residual: "
              << maximumNormalizedResidual << " x 2VT; state error: "
              << maximumNormalizedStateError << " x 2VT\n"
              << std::defaultfloat;
    expect(allReferenceStepsConverged,
           "independent double-precision ladder reference did not converge");
    expect(allProductionStepsFinite,
           "bounded ladder solve produced a non-finite state");
    expect(maximumNormalizedResidual < 6.0e-4,
           "ladder equation residual exceeded 6e-4 of 2VT (maximum "
               + std::to_string(maximumNormalizedResidual) + ")");
    expect(maximumNormalizedStateError < 2.0e-3,
           "ladder state disagreed with the independent reference by more than "
           "2e-3 of 2VT (maximum "
               + std::to_string(maximumNormalizedStateError) + ")");
}

void testLadderAdversarialControlJump()
{
    // This state/input/control transition was captured from a deterministic
    // full-domain stress sequence. Eight 1/2 backtracking trials cannot find
    // the required residual-decreasing step; the bounded deep-backtracking
    // rescue must solve it without publishing the former high-residual state.
    std::array<float, 4> productionState {
        -5.94421290e-05f, -0.262942106f, -0.127037540f, -0.073894285f
    };
    float previousInput = -0.045735367f;
    constexpr float input = 0.133429825f;
    constexpr float frequencyTangent = 6.3137517f;
    constexpr float k = 3.1935670f;
    constexpr float frequencyScale = 1.05613434f;
    constexpr double g = static_cast<double>(frequencyTangent)
                       * static_cast<double>(frequencyScale);

    LadderState previous {};
    for (std::size_t stage = 0; stage < previous.size(); ++stage)
        previous[stage] = productionState[stage];
    const double previousInputForResidual = previousInput;
    const auto reference = solveReferenceLadder(
        previous, previousInputForResidual, input, g, k);

    mars::MarsEngineTestAccess::processLadderStep(
        productionState, previousInput, input,
        frequencyTangent, k, frequencyScale);
    LadderState actual {};
    double maximumStateError = 0.0;
    for (std::size_t stage = 0; stage < actual.size(); ++stage)
    {
        actual[stage] = productionState[stage];
        maximumStateError = std::max(
            maximumStateError,
            std::abs(actual[stage] - reference.state[stage]) / ladderVoltage);
    }
    const auto residual = evaluateReferenceLadder(
        actual, previous, previousInputForResidual, input, g, k);
    const double normalizedResidual = residual.maximumResidual / ladderVoltage;

    expect(reference.converged,
           "adversarial ladder control-jump reference did not converge");
    expect(normalizedResidual < 6.0e-4,
           "adversarial ladder control jump published an out-of-contract residual ("
               + std::to_string(normalizedResidual) + " x 2VT)");
    expect(maximumStateError < 2.0e-3,
           "adversarial ladder control jump missed the reference state ("
               + std::to_string(maximumStateError) + " x 2VT)");
}

void testLadderFullRangeDoesNotLatch()
{
    constexpr std::array sampleRates { 44100.0, 48000.0 };
    constexpr std::array cutoffs { 8000.0f, 12000.0f, 16000.0f, 20000.0f };
    constexpr std::array resonances { 0.0f, 0.28f, 0.50f, 0.90f, 0.995f };

    for (const bool oversamplingEnabled : { false, true })
    {
        for (const double rate : sampleRates)
        {
            for (const float cutoff : cutoffs)
            {
                for (const float resonance : resonances)
                {
                    mars::MarsEngine engine;
                    engine.prepare(rate, blockSize, oversamplingEnabled);
                    auto p = isolatedOscillatorParameters();
                    p.osc1Wave = mars::OscillatorWave::Saw;
                    p.filterModel = mars::FilterModel::Ladder;
                    p.cutoffHz = cutoff;
                    p.resonance = resonance;
                    p.filterDrive = 0.22f;
                    p.outputGain = 0.62f;
                    engine.setParameters(p);
                    engine.reset();
                    engine.noteOn(60, 0.82f);

                    render(engine, static_cast<int>(0.46 * rate));
                    const auto early = render(engine, static_cast<int>(0.10 * rate));
                    render(engine, static_cast<int>(0.34 * rate));
                    const auto late = render(engine, static_cast<int>(0.10 * rate));

                    const std::string label = std::string { oversamplingEnabled ? "2x" : "1x" }
                        + " ladder at " + std::to_string(static_cast<int>(rate))
                        + " Hz, cutoff " + std::to_string(static_cast<int>(cutoff))
                        + " Hz, resonance " + std::to_string(resonance);
                    expect(early.finite && late.finite, label + " became non-finite");
                    expect(late.rms() > 1.0e-3,
                           label + " latched to silence (late RMS "
                               + std::to_string(late.rms()) + ")");
                    expect(late.variance() > 0.10 * late.rms() * late.rms(),
                           label + " collapsed to a DC equilibrium");
                    const double levelRatio = late.rms()
                        / std::max(early.rms(), 1.0e-12);
                    expect(levelRatio > 0.55 && levelRatio < 1.80,
                           label + " did not retain a stationary late level (ratio "
                               + std::to_string(levelRatio) + ")");
                }
            }
        }
    }
}

void testLadderBassCompensation()
{
    for (const bool oversamplingEnabled : { false, true })
    {
        const auto levelAtResonance = [oversamplingEnabled](float resonance)
        {
            constexpr double rate = 48000.0;
            mars::MarsEngine engine;
            engine.prepare(rate, blockSize, oversamplingEnabled);
            auto p = isolatedOscillatorParameters();
            p.osc1Wave = mars::OscillatorWave::Triangle;
            p.filterModel = mars::FilterModel::Ladder;
            p.cutoffHz = 800.0f;
            p.resonance = resonance;
            p.filterDrive = 0.22f;
            engine.setParameters(p);
            engine.reset();
            engine.noteOn(24, 0.82f);
            render(engine, static_cast<int>(0.40 * rate));
            return render(engine, static_cast<int>(0.35 * rate));
        };

        const auto noResonance = levelAtResonance(0.0f);
        const auto defaultResonance = levelAtResonance(0.28f);
        const auto highResonance = levelAtResonance(0.90f);
        const double defaultRatio = defaultResonance.rms()
            / std::max(noResonance.rms(), 1.0e-12);
        const double highRatio = highResonance.rms()
            / std::max(noResonance.rms(), 1.0e-12);
        const std::string path = oversamplingEnabled ? "2x" : "1x";
        expect(noResonance.finite && defaultResonance.finite && highResonance.finite,
               path + " bass-compensation render became non-finite");
        expect(defaultRatio > 0.75 && defaultRatio < 1.35,
               path + " default resonance changed deep-passband level excessively (ratio "
                   + std::to_string(defaultRatio) + ")");
        expect(highRatio > 0.65 && highRatio < 1.45,
               path + " high resonance changed deep-passband level excessively (ratio "
                   + std::to_string(highRatio) + ")");
    }
}

void testDeepOscillatorConsistency()
{
    for (const auto wave : { mars::OscillatorWave::Saw,
                             mars::OscillatorWave::Triangle })
    {
        double minimumLevel = std::numeric_limits<double>::infinity();
        double maximumLevel = 0.0;
        for (const int note : { 0, 12, 24, 36 })
        {
            const auto native = measureDeepTone(wave, note, false);
            const auto oversampled = measureDeepTone(wave, note, true);
            const double expectedFrequency = 440.0
                * std::exp2((static_cast<double>(note) - 69.0) / 12.0);
            const double nativeCents = 1200.0
                * std::log2(native.frequencyHz / expectedFrequency);
            const double oversampledCents = 1200.0
                * std::log2(oversampled.frequencyHz / expectedFrequency);
            const double levelRatio = oversampled.metrics.rms()
                / std::max(native.metrics.rms(), 1.0e-12);
            const auto* waveName = wave == mars::OscillatorWave::Saw ? "saw" : "triangle";
            const std::string label = std::string { waveName } + " MIDI "
                                    + std::to_string(note);

            expect(native.metrics.finite && oversampled.metrics.finite,
                   label + " became non-finite");
            expect(native.metrics.rms() > 0.02 && oversampled.metrics.rms() > 0.02,
                   label + " became unexpectedly quiet");
            expect(std::abs(nativeCents) < 3.0 && std::abs(oversampledCents) < 3.0,
                   label + " pitch moved by more than 3 cents (1x "
                       + std::to_string(nativeCents) + ", 2x "
                       + std::to_string(oversampledCents) + ")");
            expect(levelRatio > 0.97 && levelRatio < 1.03,
                   label + " level changed between 1x and 2x (ratio "
                       + std::to_string(levelRatio) + ")");
            minimumLevel = std::min({ minimumLevel, native.metrics.rms(),
                                      oversampled.metrics.rms() });
            maximumLevel = std::max({ maximumLevel, native.metrics.rms(),
                                      oversampled.metrics.rms() });
        }
        expect(maximumLevel / std::max(minimumLevel, 1.0e-12) < 1.12,
               std::string { wave == mars::OscillatorWave::Saw ? "saw" : "triangle" }
                   + " level varied excessively across MIDI 0/12/24/36");
    }
}

void testOversamplingConfigurationAndDeferredChange()
{
    mars::MarsEngine defaults;
    expect(defaults.isOversamplingEnabled(), "oversampling did not default to enabled");
    expect(defaults.getOversamplingFactor() == 2,
           "default engine did not advertise its 2x topology");

    for (const double rate : { 44100.0, 48000.0 })
    {
        mars::MarsEngine engine;
        engine.prepare(rate, blockSize, true);
        expect(engine.isOversamplingEnabled() && engine.getOversamplingFactor() == 2,
               "enabled oversampling did not select 2x at "
                   + std::to_string(static_cast<int>(rate)) + " Hz");
    }
    for (const double rate : { 88200.0, 96000.0, 192000.0, 384000.0 })
    {
        mars::MarsEngine engine;
        engine.prepare(rate, blockSize, true);
        expect(engine.isOversamplingEnabled() && engine.getOversamplingFactor() == 1,
               "high host rate did not retain native-rate processing at "
                   + std::to_string(static_cast<int>(rate)) + " Hz");
    }
    {
        mars::MarsEngine engine;
        engine.prepare(48000.0, blockSize, false);
        expect(!engine.isOversamplingEnabled() && engine.getOversamplingFactor() == 1,
               "disabled oversampling did not select the 1x topology");
        expect(!engine.setOversamplingEnabled(false),
               "repeating an unchanged oversampling request reported a change");
    }

    constexpr double rate = 48000.0;
    mars::MarsEngine engine;
    engine.prepare(rate, 64, true);
    auto p = isolatedOscillatorParameters();
    p.osc1Wave = mars::OscillatorWave::Triangle;
    p.filterModel = mars::FilterModel::Sem;
    p.cutoffHz = 6000.0f;
    p.ampRelease = 0.012f;
    engine.setParameters(p);
    engine.reset();
    engine.noteOn(36, 0.9f);
    render(engine, static_cast<int>(0.18 * rate));

    float boundarySample = 0.0f;
    const bool found = findFlatAudibleSample(engine, 0.08f, 0.0025f,
                                             boundarySample);
    const bool changedImmediately = engine.setOversamplingEnabled(false);
    const float afterRequest = processLeftSample(engine);
    expect(found, "could not locate an oversampling-request boundary");
    expect(!changedImmediately && !engine.isOversamplingEnabled(),
           "held-note oversampling request was not deferred");
    expect(engine.getOversamplingFactor() == 2 && engine.getActiveVoiceCount() == 1,
           "held-note oversampling request killed the voice or changed topology");
    expect(std::abs(afterRequest - boundarySample) < 0.01f,
           "deferred oversampling request introduced a boundary discontinuity");
    render(engine, static_cast<int>(0.04 * rate));
    expect(engine.getOversamplingFactor() == 2 && engine.getActiveVoiceCount() == 1,
           "oversampling topology changed while the note remained held");

    engine.noteOff(36);
    float previous = processLeftSample(engine);
    bool changedWhileActive = false;
    bool topologyChanged = false;
    int idleSamples = 0;
    int idleSamplesAtChange = 0;
    double topologyStep = 0.0;
    for (int sample = 0; sample < static_cast<int>(rate); ++sample)
    {
        const int previousFactor = engine.getOversamplingFactor();
        const float current = processLeftSample(engine);
        if (engine.getActiveVoiceCount() > 0 && engine.getOversamplingFactor() != 2)
            changedWhileActive = true;
        if (engine.getActiveVoiceCount() == 0)
            ++idleSamples;
        else
            idleSamples = 0;
        if (engine.getOversamplingFactor() != previousFactor)
        {
            topologyChanged = true;
            idleSamplesAtChange = idleSamples;
            topologyStep = std::abs(static_cast<double>(current - previous));
            break;
        }
        previous = current;
    }

    expect(!changedWhileActive, "oversampling topology changed before release completed");
    expect(topologyChanged && engine.getOversamplingFactor() == 1,
           "deferred oversampling request was not applied after release and idle");
    expect(idleSamplesAtChange >= static_cast<int>(0.020 * rate),
           "oversampling topology changed without the required idle guard");
    expect(topologyStep < 0.01,
           "idle oversampling topology change introduced a discontinuity");

    engine.noteOn(36, 0.9f);
    const auto nativeRateNote = render(engine, static_cast<int>(0.08 * rate),
                                       static_cast<int>(0.02 * rate));
    expect(nativeRateNote.finite && nativeRateNote.rms() > 1.0e-3,
           "1x topology failed to render a note after the deferred change");
    engine.allNotesOff();
    render(engine, static_cast<int>(0.20 * rate));
}

void testRenderMatrix()
{
    struct Scenario
    {
        mars::OscillatorWave wave;
        mars::FilterModel filter;
        mars::VoiceMode mode;
        int expectedVoices;
        const char* name;
    };
    constexpr std::array scenarios {
        Scenario { mars::OscillatorWave::Saw, mars::FilterModel::Ladder,
                   mars::VoiceMode::Poly, 1, "saw ladder" },
        Scenario { mars::OscillatorWave::Pulse, mars::FilterModel::Sem,
                   mars::VoiceMode::Unison, 4, "pulse SEM unison" },
        Scenario { mars::OscillatorWave::Triangle, mars::FilterModel::Sem,
                   mars::VoiceMode::Fifth, 2, "triangle fifth" }
    };
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0, 192000.0, 384000.0 };

    for (const double rate : sampleRates)
    {
        for (const auto& scenario : scenarios)
        {
            mars::MarsEngine engine;
            engine.prepare(rate, blockSize);
            auto p = basicParameters();
            p.osc1Wave = scenario.wave;
            p.osc2Wave = scenario.wave;
            p.filterModel = scenario.filter;
            p.voiceMode = scenario.mode;
            engine.setParameters(p);
            engine.noteOn(60, 0.82f);
            const auto held = render(engine, static_cast<int>(0.24 * rate));
            const std::string label = std::string(scenario.name) + " at "
                + std::to_string(static_cast<int>(rate)) + " Hz";
            expect(held.finite, label + " produced a NaN or infinity");
            expect(held.rms() > 1.0e-5, label + " was silent");
            expect(held.peak < 3.0, label + " escaped the output guardrail");
            expect(engine.getActiveVoiceCount() == scenario.expectedVoices,
                   label + " allocated an unexpected voice count");
            engine.noteOff(60);
            const auto released = render(engine, static_cast<int>(0.65 * rate));
            expect(released.finite && released.peak < 3.0,
                   label + " became unstable during release");
            expect(engine.getActiveVoiceCount() == 0,
                   label + " did not finish its release");
        }
    }
}

void testSustainAndRelease()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine engine;
    engine.prepare(rate, blockSize);
    auto p = basicParameters();
    p.ampRelease = 0.045f;
    p.chorusMix = 0.0f;
    engine.setParameters(p);
    engine.setSustainPedal(true);
    engine.noteOn(64, 0.8f);
    render(engine, static_cast<int>(0.12 * rate));
    engine.noteOff(64);
    const auto sustained = render(engine, static_cast<int>(0.22 * rate));
    expect(sustained.finite && sustained.rms() > 1.0e-5,
           "sustain pedal did not hold audible audio");
    expect(engine.getActiveVoiceCount() == 1,
           "sustain pedal released the voice early");
    engine.setSustainPedal(false);
    render(engine, static_cast<int>(0.35 * rate));
    expect(engine.getActiveVoiceCount() == 0,
           "lifting sustain did not release the held voice");

    engine.setSustainPedal(true);
    engine.noteOn(67, 0.8f);
    render(engine, static_cast<int>(0.08 * rate));
    engine.allNotesOff();
    const auto heldByAllNotesOff = render(engine, static_cast<int>(0.22 * rate));
    expect(heldByAllNotesOff.finite && heldByAllNotesOff.rms() > 1.0e-5,
           "allNotesOff did not leave a sustained held key audible");
    expect(engine.getActiveVoiceCount() == 1,
           "allNotesOff incorrectly overrode the sustain pedal");
    engine.setSustainPedal(false);
    render(engine, static_cast<int>(0.35 * rate));
    expect(engine.getActiveVoiceCount() == 0,
           "pedal-up did not finish an allNotesOff-sustained voice");
}

void testVoiceAllocation()
{
    mars::MarsEngine engine;
    engine.prepare(48000.0, blockSize);
    auto p = basicParameters();
    p.voiceMode = mars::VoiceMode::Poly;
    engine.setParameters(p);
    for (int note = 36; note < 52; ++note)
        engine.noteOn(note, 0.7f);
    render(engine, blockSize);
    expect(engine.getActiveVoiceCount() == 16,
           "poly mode did not fill the 16-voice render limit");
    expect(mars::MarsEngineTestAccess::maximumVoiceCount() == 16,
           "the physical Mars voice pool is not capped at 16");

    engine.noteOn(52, 0.7f);
    expect(engine.getActiveVoiceCount() == 16,
           "poly overflow exceeded the 16-voice render limit");
    expect(mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 36) == 1
               && mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 50) == 1
               && mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 51) == 0
               && mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 52) == 1,
           "poly overflow did not replace the newest existing note group");

    engine.reset();
    engine.setParameters(p);
    for (int note = 36; note < 52; ++note)
        engine.noteOn(note, 0.7f);
    engine.noteOff(50);
    engine.noteOff(51);
    engine.noteOn(52, 0.7f);
    expect(mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 50) == 1
               && mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 51) == 0
               && mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 52) == 1,
           "overflow did not replace the newest released note group first");

    engine.reset();
    p.voiceMode = mars::VoiceMode::Unison;
    p.unisonVoices = 8;
    engine.setParameters(p);
    engine.noteOn(48, 0.7f);
    engine.noteOn(49, 0.7f);
    engine.noteOn(50, 0.7f);
    render(engine, blockSize);
    expect(engine.getActiveVoiceCount() == 16,
           "8-layer unison exceeded the 16-voice render limit");
    expect(mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 48) == 8
               && mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 49) == 0
               && mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 50) == 8,
           "unison overflow did not replace the newest group atomically");

    engine.reset();
    p.unisonVoices = 3;
    engine.setParameters(p);
    for (int note = 48; note < 54; ++note)
        engine.noteOn(note, 0.7f);
    expect(engine.getActiveVoiceCount() == 15,
           "3-layer unison did not preserve whole groups within 16 voices");
    expect(mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 52) == 0
               && mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 53) == 3,
           "3-layer unison did not replace the newest complete group");

    engine.reset();
    p.voiceMode = mars::VoiceMode::Fifth;
    engine.setParameters(p);
    for (int note = 48; note < 56; ++note)
        engine.noteOn(note, 0.7f);
    engine.noteOn(56, 0.7f);
    expect(engine.getActiveVoiceCount() == 16,
           "fifth mode exceeded the 16-voice render limit");
    expect(mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 48) == 2
               && mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 55) == 0
               && mars::MarsEngineTestAccess::activeVoicesForRoot(engine, 56) == 2,
           "fifth-mode overflow did not replace the newest group atomically");
}

void testSampleRateLevelConsistency()
{
    constexpr std::array sampleRates { 44100.0, 48000.0, 96000.0, 192000.0, 384000.0 };
    std::array<double, sampleRates.size()> levels {};
    for (std::size_t index = 0; index < sampleRates.size(); ++index)
    {
        const double rate = sampleRates[index];
        mars::MarsEngine engine;
        engine.prepare(rate, blockSize);
        auto p = basicParameters();
        p.voiceMode = mars::VoiceMode::Poly;
        p.osc1Wave = mars::OscillatorWave::Saw;
        p.oscMix = 0.0f;
        p.subLevel = 0.0f;
        p.noiseLevel = 0.0f;
        p.crossMod = 0.0f;
        p.cutoffHz = 12000.0f;
        p.resonance = 0.05f;
        p.filterDrive = 0.0f;
        p.filterEnvAmount = 0.0f;
        p.filterKeyTrack = 0.0f;
        p.ampAttack = 0.001f;
        p.ampDecay = 0.01f;
        p.ampSustain = 1.0f;
        p.chorusMix = 0.0f;
        p.outputGain = 0.65f;
        engine.setParameters(p);
        engine.noteOn(57, 0.8f);
        const auto metrics = render(engine, static_cast<int>(0.55 * rate),
                                    static_cast<int>(0.15 * rate));
        expect(metrics.finite && metrics.rms() > 1.0e-5,
               "sample-rate consistency render failed");
        levels[index] = metrics.rms();
    }
    const auto [minimum, maximum] = std::minmax_element(levels.begin(), levels.end());
    expect(*maximum / std::max(*minimum, 1.0e-12) < 1.65,
           "held-note level changed by more than 4.35 dB across sample rates");
}

void testOversamplingPitchAndPitchBend()
{
    const double pitch48 = estimateFrequency(48000.0);
    const double pitch192 = estimateFrequency(192000.0);
    const double pitch384 = estimateFrequency(384000.0);
    expect(pitch48 > 205.0 && pitch48 < 235.0,
           "48 kHz oscillator frequency was outside the expected A3 range");
    expect(pitch192 > 205.0 && pitch192 < 235.0,
           "192 kHz 1x path changed oscillator pitch or timing");
    expect(std::abs(pitch192 / pitch48 - 1.0) < 0.025,
           "1x and 2x paths disagreed on oscillator pitch");
    expect(pitch384 > 205.0 && pitch384 < 235.0,
           "384 kHz host timebase changed oscillator pitch");
    expect(std::abs(pitch384 / pitch48 - 1.0) < 0.025,
           "384 kHz and 48 kHz paths disagreed on oscillator pitch");

    const double release48 = measureReleaseDuration(48000.0);
    const double release384 = measureReleaseDuration(384000.0);
    expect(release48 > 0.09 && release48 < 0.20,
           "48 kHz release timing was outside its expected range");
    expect(std::abs(release384 / release48 - 1.0) < 0.035,
           "384 kHz host timebase changed envelope timing");

    const double bent = estimateFrequency(48000.0, 1.0f);
    const double bendRatio = bent / pitch48;
    expect(bendRatio > 1.08 && bendRatio < 1.17,
           "full positive pitch bend was not close to +2 semitones");
}

void testParameterSanitisation()
{
    mars::MarsEngine engine;
    engine.prepare(48000.0, blockSize);
    auto p = basicParameters();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    p.cutoffHz = nan;
    p.resonance = std::numeric_limits<float>::infinity();
    p.filterDrive = -100.0f;
    p.pulseWidth = 100.0f;
    p.osc1Octave = -99;
    p.osc2Octave = 99;
    p.unisonVoices = 99;
    p.voiceMode = mars::VoiceMode::Unison;
    engine.setParameters(p);
    engine.setPitchBend(nan);
    engine.setModWheel(std::numeric_limits<float>::infinity());
    engine.noteOn(60, 0.8f);
    const auto metrics = render(engine, 24000);
    expect(metrics.finite && metrics.peak < 3.0,
           "non-finite or out-of-range parameters escaped sanitisation");
    expect(engine.getActiveVoiceCount() == 8,
           "unison voice count was not sanitised to the 2..8 contract");
}

void testSemShapeEndpoints()
{
    const double lowToNotch = semShapeDistance(0.0f, 0.5f);
    const double notchToHigh = semShapeDistance(0.5f, 1.0f);
    const double lowToHigh = semShapeDistance(0.0f, 1.0f);
    expect(lowToNotch > 1.0e-4,
           "SEM low-pass and notch anchors produced the same response");
    expect(notchToHigh > 1.0e-4,
           "SEM notch and high-pass anchors produced the same response");
    expect(lowToHigh > 1.0e-4,
           "SEM shape endpoints produced the same response");
}

void testDeterminismAndOscillatorResponses()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine first;
    mars::MarsEngine second;
    first.prepare(rate, blockSize);
    second.prepare(rate, blockSize);
    auto p = basicParameters();
    p.lfoWave = mars::LfoWaveform::SampleHold;
    first.setParameters(p);
    second.setParameters(p);
    for (const int note : { 48, 55, 62 })
    {
        first.noteOn(note, 0.73f);
        second.noteOn(note, 0.73f);
    }
    const double deterministicDifference = averageRenderDifference(
        first, second, static_cast<int>(0.75 * rate));
    expect(deterministicDifference < 1.0e-9,
           "identical engines and MIDI did not render deterministically");

    const auto compareWaves = [](mars::OscillatorWave firstWave,
                                 mars::OscillatorWave secondWave)
    {
        mars::MarsEngine a;
        mars::MarsEngine b;
        a.prepare(rate, blockSize);
        b.prepare(rate, blockSize);
        auto firstParameters = isolatedOscillatorParameters();
        auto secondParameters = firstParameters;
        firstParameters.osc1Wave = firstWave;
        secondParameters.osc1Wave = secondWave;
        a.setParameters(firstParameters);
        b.setParameters(secondParameters);
        a.noteOn(60, 0.8f);
        b.noteOn(60, 0.8f);
        render(a, static_cast<int>(0.15 * rate));
        render(b, static_cast<int>(0.15 * rate));
        return averageRenderDifference(a, b, static_cast<int>(0.20 * rate));
    };

    expect(compareWaves(mars::OscillatorWave::Saw, mars::OscillatorWave::Pulse) > 1.0e-3,
           "saw and pulse oscillator selections produced the same response");
    expect(compareWaves(mars::OscillatorWave::Pulse, mars::OscillatorWave::Triangle) > 1.0e-3,
           "pulse and triangle oscillator selections produced the same response");
}

void testOscillatorMixerSwitches()
{
    constexpr double rate = 48000.0;
    const mars::EngineParameters defaults;
    expect(defaults.osc1Enabled && defaults.osc2Enabled,
           "both oscillator switches must default to enabled");

    const auto loneBalanceDifference = [](bool useOscillator1)
    {
        mars::MarsEngine hardLeft;
        mars::MarsEngine hardRight;
        hardLeft.prepare(rate, blockSize);
        hardRight.prepare(rate, blockSize);
        auto leftParameters = isolatedOscillatorParameters();
        leftParameters.osc1Enabled = useOscillator1;
        leftParameters.osc2Enabled = !useOscillator1;
        leftParameters.oscMix = 0.0f;
        auto rightParameters = leftParameters;
        rightParameters.oscMix = 1.0f;
        hardLeft.setParameters(leftParameters);
        hardRight.setParameters(rightParameters);
        hardLeft.noteOn(55, 0.82f);
        hardRight.noteOn(55, 0.82f);
        render(hardLeft, static_cast<int>(0.15 * rate));
        render(hardRight, static_cast<int>(0.15 * rate));
        return averageRenderDifference(hardLeft, hardRight,
                                       static_cast<int>(0.16 * rate));
    };

    expect(loneBalanceDifference(true) < 1.0e-8,
           "VCO I level changed with Balance while VCO II was switched off");
    expect(loneBalanceDifference(false) < 1.0e-8,
           "VCO II level changed with Balance while VCO I was switched off");

    auto silentParameters = isolatedOscillatorParameters();
    silentParameters.osc1Enabled = false;
    silentParameters.osc2Enabled = false;
    silentParameters.subLevel = 0.0f;
    silentParameters.noiseLevel = 0.0f;
    for (const auto model : { mars::FilterModel::Ladder, mars::FilterModel::Sem })
    {
        silentParameters.filterModel = model;
        const auto silent = renderSteadyNote(silentParameters, 48, 0.75, 0.10);
        const auto* modelName = model == mars::FilterModel::Ladder ? "Ladder" : "SEM";
        expect(silent.finite && silent.rms() < 1.0e-6,
               std::string { "switching off both VCO mixer inputs did not silence the " }
                   + modelName + " path (RMS " + std::to_string(silent.rms()) + ")");
    }

    auto subParameters = silentParameters;
    subParameters.subLevel = 0.55f;
    const auto subOnly = renderSteadyNote(subParameters, 48);
    expect(subOnly.finite && subOnly.rms() > 1.0e-3,
           "the independent sub oscillator was muted with both VCOs off");

    mars::MarsEngine first;
    mars::MarsEngine second;
    auto firstParameters = isolatedOscillatorParameters();
    firstParameters.osc1Enabled = true;
    firstParameters.osc2Enabled = false;
    firstParameters.osc2Wave = mars::OscillatorWave::Saw;
    firstParameters.osc2Octave = -2;
    auto secondParameters = firstParameters;
    secondParameters.osc2Wave = mars::OscillatorWave::Triangle;
    secondParameters.osc2Octave = 2;
    secondParameters.osc2FineCents = 100.0f;
    first.setParameters(firstParameters);
    second.setParameters(secondParameters);
    first.prepare(rate, blockSize);
    second.prepare(rate, blockSize);
    first.noteOn(60, 0.82f);
    second.noteOn(60, 0.82f);
    render(first, static_cast<int>(0.16 * rate));
    render(second, static_cast<int>(0.16 * rate));
    expect(averageRenderDifference(first, second,
                                   static_cast<int>(0.16 * rate)) < 1.0e-8,
           "a switched-off VCO II leaked into the main audio path");
}

void testOscillatorSwitchContinuityAndCrossMod()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine switched;
    switched.prepare(rate, 64);
    auto switchedParameters = isolatedOscillatorParameters();
    switchedParameters.osc1Enabled = true;
    switchedParameters.osc2Enabled = false;
    switchedParameters.osc1Wave = mars::OscillatorWave::Triangle;
    switchedParameters.cutoffHz = 9000.0f;
    switched.setParameters(switchedParameters);
    switched.noteOn(36, 0.9f);
    render(switched, static_cast<int>(0.20 * rate));

    float boundarySample = 0.0f;
    const bool found = findFlatAudibleSample(switched, 0.06f, 0.0025f,
                                             boundarySample);
    switchedParameters.osc1Enabled = false;
    switched.setParameters(switchedParameters);
    const double switchOffStep = maximumStepAfter(switched, boundarySample, 512);
    render(switched, static_cast<int>(0.04 * rate));
    const float silentBoundary = processLeftSample(switched);
    switchedParameters.osc1Enabled = true;
    switched.setParameters(switchedParameters);
    const double switchOnStep = maximumStepAfter(switched, silentBoundary, 512);
    expect(found, "could not locate an audible oscillator-switch boundary");
    expect(switchOffStep < 0.025,
           "switching a VCO off introduced an audible one-sample discontinuity");
    expect(switchOnStep < 0.025,
           "switching a VCO on introduced an audible one-sample discontinuity");

    mars::MarsEngine plain;
    mars::MarsEngine modulated;
    plain.prepare(rate, blockSize);
    modulated.prepare(rate, blockSize);
    auto plainParameters = isolatedOscillatorParameters();
    plainParameters.osc1Enabled = true;
    plainParameters.osc2Enabled = false;
    plainParameters.osc1Wave = mars::OscillatorWave::Saw;
    plainParameters.osc2Wave = mars::OscillatorWave::Pulse;
    plainParameters.crossMod = 0.0f;
    auto modulatedParameters = plainParameters;
    modulatedParameters.crossMod = 0.82f;
    plain.setParameters(plainParameters);
    modulated.setParameters(modulatedParameters);
    plain.noteOn(55, 0.82f);
    modulated.noteOn(55, 0.82f);
    render(plain, static_cast<int>(0.22 * rate));
    render(modulated, static_cast<int>(0.22 * rate));
    const double crossModDifference = averageRenderDifference(
        plain, modulated, static_cast<int>(0.18 * rate));
    expect(crossModDifference > 1.0e-3,
           "VCO II stopped cross-modulating when its audio switch was off");

    // A hidden oscillator must keep advancing. Once its mixer switch is opened,
    // it should converge to the same phase as an otherwise identical VCO that
    // had remained audible throughout the hidden interval.
    mars::MarsEngine hidden;
    mars::MarsEngine running;
    hidden.prepare(rate, blockSize);
    running.prepare(rate, blockSize);
    auto hiddenParameters = isolatedOscillatorParameters();
    hiddenParameters.filterModel = mars::FilterModel::Sem;
    hiddenParameters.filterShape = 0.0f;
    hiddenParameters.cutoffHz = 16000.0f;
    hiddenParameters.osc1Enabled = false;
    hiddenParameters.osc2Enabled = false;
    hiddenParameters.osc2Wave = mars::OscillatorWave::Triangle;
    auto runningParameters = hiddenParameters;
    runningParameters.osc2Enabled = true;
    hidden.setParameters(hiddenParameters);
    running.setParameters(runningParameters);
    hidden.reset();
    running.reset();
    hidden.noteOn(53, 0.82f);
    running.noteOn(53, 0.82f);
    render(hidden, static_cast<int>(0.137 * rate));
    render(running, static_cast<int>(0.137 * rate));
    hiddenParameters.osc2Enabled = true;
    hidden.setParameters(hiddenParameters);
    // Let the two filter/DC histories converge after opening the hidden mixer
    // feed; the phase assertion below should not measure their unrelated servo
    // transients (the DC blocker has a deliberately low 1.5 Hz corner).
    render(hidden, static_cast<int>(1.20 * rate));
    render(running, static_cast<int>(1.20 * rate));
    const double hiddenPhaseDifference = averageRenderDifference(
        hidden, running, static_cast<int>(0.14 * rate));
    expect(hiddenPhaseDifference < 1.0e-5,
           "a mixer-disabled oscillator stopped advancing its phase (difference "
               + std::to_string(hiddenPhaseDifference) + ")");
}

void testAnalogModelResponses()
{
    const auto filterMetrics = [](mars::FilterModel model, float cutoff)
    {
        auto p = isolatedOscillatorParameters();
        p.osc1Enabled = true;
        p.osc2Enabled = false;
        p.osc1Wave = mars::OscillatorWave::Saw;
        p.filterModel = model;
        p.filterShape = 0.0f;
        p.cutoffHz = cutoff;
        p.resonance = 0.0f;
        p.filterDrive = 0.35f;
        return renderSteadyNote(p, 69);
    };

    const auto ladderLow = filterMetrics(mars::FilterModel::Ladder, 180.0f);
    const auto ladderHigh = filterMetrics(mars::FilterModel::Ladder, 9000.0f);
    expect(ladderLow.finite && ladderHigh.finite
               && ladderHigh.rms() > 1.8 * ladderLow.rms(),
           "the nonlinear four-stage ladder did not exhibit a low-pass cutoff response");

    const auto semLow = filterMetrics(mars::FilterModel::Sem, 180.0f);
    const auto semHigh = filterMetrics(mars::FilterModel::Sem, 9000.0f);
    expect(semLow.finite && semHigh.finite
               && semHigh.rms() > 1.8 * semLow.rms(),
           "the SEM-inspired state-variable model did not exhibit a low-pass response");

    auto resonantParameters = isolatedOscillatorParameters();
    resonantParameters.osc1Enabled = true;
    resonantParameters.osc2Enabled = false;
    resonantParameters.osc1Wave = mars::OscillatorWave::Saw;
    resonantParameters.filterModel = mars::FilterModel::Ladder;
    resonantParameters.cutoffHz = 1100.0f;
    resonantParameters.resonance = 0.98f;
    resonantParameters.filterDrive = 1.0f;
    const auto resonant = renderSteadyNote(resonantParameters, 45, 0.45, 0.45);
    expect(resonant.finite && resonant.rms() > 1.0e-5 && resonant.peak < 3.0,
           "the driven high-resonance transistor ladder was silent or unstable");

    auto sawParameters = isolatedOscillatorParameters();
    sawParameters.osc1Enabled = true;
    sawParameters.osc2Enabled = false;
    sawParameters.osc1Wave = mars::OscillatorWave::Saw;
    sawParameters.filterModel = mars::FilterModel::Sem;
    sawParameters.filterShape = 0.0f;
    sawParameters.cutoffHz = 20000.0f;
    const auto lowSaw = renderSteadyNote(sawParameters, 45);
    const auto highSaw = renderSteadyNote(sawParameters, 93);
    const double sawLevelRatio = highSaw.rms() / std::max(lowSaw.rms(), 1.0e-12);
    expect(lowSaw.finite && highSaw.finite
               && lowSaw.rms() > 1.0e-4 && highSaw.rms() > 1.0e-4,
           "the frequency-dependent Voyager saw contour became silent or non-finite");
    // These deliberately broad deterministic windows protect the measured
    // frequency-dependent post-EQ from being bypassed while tolerating small
    // floating-point differences between toolchains.
    expect(lowSaw.rms() > 0.097 && lowSaw.rms() < 0.1085,
           "the low-note Voyager saw spectral-contour signature changed");
    expect(highSaw.rms() > 0.092 && highSaw.rms() < 0.1045,
           "the high-note Voyager saw spectral-contour signature changed");
    expect(sawLevelRatio > 0.90 && sawLevelRatio < 1.02,
           "the Voyager saw contour produced an implausible pitch-level jump");
}

void testLongHeldTriangleStability()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine engine;
    engine.prepare(rate, blockSize);
    auto p = isolatedOscillatorParameters();
    p.filterModel = mars::FilterModel::Sem;
    p.filterShape = 0.0f;
    engine.setParameters(p);
    engine.noteOn(60, 0.8f);
    render(engine, static_cast<int>(0.75 * rate));
    const auto early = render(engine, static_cast<int>(0.50 * rate));
    render(engine, static_cast<int>(30.0 * rate));
    const auto late = render(engine, static_cast<int>(0.50 * rate));

    expect(early.finite && late.finite && early.rms() > 1.0e-4,
           "long triangle stability render was invalid or silent");
    const double levelRatio = late.rms() / std::max(early.rms(), 1.0e-12);
    expect(levelRatio > 0.96 && levelRatio < 1.04,
           "long-held triangle level drifted as its integrator accumulated error");
    expect(late.peak < 0.95,
           "long-held triangle reached a rail or the output safety limiter");
    expect(std::abs(late.mean()) < 1.0e-3,
           "long-held triangle developed a DC offset");
}

void testClicklessRetriggerAndVoiceSteal()
{
    constexpr double rate = 48000.0;
    const auto runBoundary = [](bool forceAllocationSteal)
    {
        mars::MarsEngine engine;
        engine.prepare(rate, 64);
        auto p = isolatedOscillatorParameters();
        p.cutoffHz = 7000.0f;
        p.velocityAmount = 1.0f;
        engine.setParameters(p);
        if (forceAllocationSteal)
        {
            for (int note = 60; note < 75; ++note)
                engine.noteOn(note, 1.0e-6f);
            engine.noteOn(75, 1.0f);
        }
        else
        {
            engine.noteOn(60, 1.0f);
        }
        render(engine, static_cast<int>(0.20 * rate));

        float boundarySample = 0.0f;
        const bool found = findFlatAudibleSample(engine, 0.10f, 0.0025f,
                                                 boundarySample);
        if (forceAllocationSteal)
            engine.noteOn(76, 1.0e-6f);
        else
            engine.noteOn(60, 1.0f);
        return std::pair<bool, double> {
            found, maximumStepAfter(engine, boundarySample, 128)
        };
    };

    const auto retrigger = runBoundary(false);
    const auto steal = runBoundary(true);
    expect(retrigger.first, "could not locate an audible retrigger boundary");
    expect(steal.first, "could not locate an audible voice-steal boundary");
    expect(retrigger.second < 0.045,
           "same-note retrigger introduced an audible one-sample discontinuity");
    expect(steal.second < 0.045,
           "newest-group voice stealing introduced an audible discontinuity");
}

void testClicklessFilterModelSwitch()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine engine;
    engine.prepare(rate, 64);
    auto p = isolatedOscillatorParameters();
    p.filterModel = mars::FilterModel::Ladder;
    p.cutoffHz = 900.0f;
    p.resonance = 0.48f;
    engine.setParameters(p);
    engine.noteOn(52, 0.9f);
    render(engine, static_cast<int>(0.25 * rate));

    float boundarySample = 0.0f;
    const bool found = findFlatAudibleSample(engine, 0.025f, 0.0015f,
                                             boundarySample);
    p.filterModel = mars::FilterModel::Sem;
    p.filterShape = 1.0f;
    engine.setParameters(p);
    const double maximumStep = maximumStepAfter(engine, boundarySample, 384);
    expect(found, "could not locate an audible filter-switch boundary");
    expect(maximumStep < 0.025,
           "Ladder/SEM switching introduced an audible discontinuity");
}

void testGlideAndModulationMeaningfulness()
{
    constexpr double rate = 48000.0;
    const auto earlyPitch = [](float glideSeconds)
    {
        mars::MarsEngine engine;
        engine.prepare(rate, blockSize);
        auto p = isolatedOscillatorParameters();
        p.osc1Wave = mars::OscillatorWave::Saw;
        p.glideSeconds = glideSeconds;
        p.ampRelease = 0.01f;
        engine.setParameters(p);
        engine.noteOn(48, 0.8f);
        render(engine, static_cast<int>(0.12 * rate));
        engine.noteOff(48);
        render(engine, static_cast<int>(0.08 * rate));
        engine.noteOn(72, 0.8f);

        constexpr double windowSeconds = 0.050;
        const int samples = static_cast<int>(windowSeconds * rate);
        float previous = 0.0f;
        int crossings = 0;
        for (int sample = 0; sample < samples; ++sample)
        {
            const float current = processLeftSample(engine);
            crossings += previous <= 0.0f && current > 0.0f ? 1 : 0;
            previous = current;
        }
        return static_cast<double>(crossings) / windowSeconds;
    };

    const double immediatePitch = earlyPitch(0.0f);
    const double glidingPitch = earlyPitch(0.35f);
    expect(immediatePitch > 430.0,
           "zero-glide note did not reach its target pitch immediately");
    expect(glidingPitch < 0.78 * immediatePitch,
           "glide control did not materially slow the pitch transition");

    mars::MarsEngine plain;
    mars::MarsEngine modulated;
    plain.prepare(rate, blockSize);
    modulated.prepare(rate, blockSize);
    auto p = isolatedOscillatorParameters();
    p.osc1Wave = mars::OscillatorWave::Saw;
    p.lfoWave = mars::LfoWaveform::Sine;
    p.lfoRateHz = 5.0f;
    plain.setParameters(p);
    modulated.setParameters(p);
    modulated.setModWheel(1.0f);
    plain.noteOn(60, 0.8f);
    modulated.noteOn(60, 0.8f);
    render(plain, static_cast<int>(0.25 * rate));
    render(modulated, static_cast<int>(0.25 * rate));
    const double modulationDifference = averageRenderDifference(
        plain, modulated, static_cast<int>(0.25 * rate));
    expect(modulationDifference > 1.0e-3,
           "mod wheel did not materially affect its fixed LFO routes");
}

void testExtremeAutomationStability()
{
    constexpr double rate = 48000.0;
    mars::MarsEngine engine;
    engine.prepare(rate, 64);
    auto p = basicParameters();
    p.voiceMode = mars::VoiceMode::Fifth;
    p.ampRelease = 0.04f;
    engine.setParameters(p);
    for (int note = 40; note < 52; ++note)
        engine.noteOn(note, 0.92f);

    std::array<float, 64> left {};
    std::array<float, 64> right {};
    Metrics metrics;
    for (int block = 0; block < 360; ++block)
    {
        const bool high = (block & 1) != 0;
        p.osc1Wave = static_cast<mars::OscillatorWave>(block % 3);
        p.osc2Wave = static_cast<mars::OscillatorWave>((block + 1) % 3);
        p.filterModel = high ? mars::FilterModel::Sem : mars::FilterModel::Ladder;
        p.lfoWave = static_cast<mars::LfoWaveform>(block % 3);
        p.cutoffHz = high ? 20000.0f : 20.0f;
        p.resonance = high ? 1.0f : 0.0f;
        p.filterDrive = high ? 1.0f : 0.0f;
        p.filterShape = high ? 1.0f : 0.0f;
        p.filterEnvAmount = high ? 1.0f : -1.0f;
        p.crossMod = high ? 1.0f : 0.0f;
        p.pulseWidth = high ? 0.97f : 0.03f;
        p.lfoFilterOctaves = high ? 8.0f : -8.0f;
        p.chorusMix = high ? 1.0f : 0.0f;
        engine.setParameters(p);
        engine.setPitchBend(high ? 1.0f : -1.0f);
        engine.setModWheel(high ? 1.0f : 0.0f);
        engine.process(left.data(), right.data(), 64);
        for (int i = 0; i < 64; ++i)
        {
            metrics.add(left[static_cast<std::size_t>(i)]);
            metrics.add(right[static_cast<std::size_t>(i)]);
        }
    }
    expect(metrics.finite, "extreme automation produced a NaN or infinity");
    expect(metrics.peak < 4.0, "extreme automation escaped the bounded output stage");
    expect(engine.getActiveVoiceCount() <= 16,
           "extreme automation exceeded the render-slot limit");
    engine.allNotesOff();
    render(engine, static_cast<int>(0.8 * rate));
    expect(engine.getActiveVoiceCount() == 0,
           "automated voices did not finish releasing");
}

double benchmarkCpuModel(mars::FilterModel filterModel)
{
    constexpr double rate = 96000.0;
    constexpr double renderSeconds = 0.35;
    const int trialCount = sanitizerBuild ? 1 : 3;
    mars::MarsEngine engine;
    engine.prepare(rate, blockSize);
    auto p = basicParameters();
    p.voiceMode = mars::VoiceMode::Fifth;
    p.filterModel = filterModel;
    p.filterDrive = 0.9f;
    p.resonance = 0.85f;
    p.chorusMix = 0.7f;
    engine.setParameters(p);
    engine.reset();
    for (int note = 36; note < 44; ++note)
        engine.noteOn(note, 0.85f);
    expect(engine.getActiveVoiceCount() == 16,
           "CPU regression did not fill all 16 render slots");

    // Warm instruction/data caches and let parameter smoothing reach the
    // measured steady state. Release uses the best of three equal renders so a
    // scheduler interruption cannot make the real-time contract flaky.
    render(engine, static_cast<int>(0.05 * rate));
    double bestElapsed = std::numeric_limits<double>::infinity();
    bool allTrialsValid = true;
    for (int trial = 0; trial < trialCount; ++trial)
    {
        const auto start = std::chrono::steady_clock::now();
        const auto metrics = render(engine, static_cast<int>(renderSeconds * rate));
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        bestElapsed = std::min(bestElapsed, elapsed);
        allTrialsValid = allTrialsValid
                      && metrics.finite && metrics.rms() > 1.0e-6;
    }
    const double ratio = bestElapsed / renderSeconds;
    const auto* modelName = filterModel == mars::FilterModel::Ladder
        ? "Ladder" : "SEM";
    std::cout << std::fixed << std::setprecision(3) << modelName
              << " 16-slot 96 kHz/native HQ render, best of " << trialCount
              << ": " << bestElapsed << " s (" << ratio << "x real time)\n";
    expect(allTrialsValid,
           "CPU regression render was invalid or silent");
    const auto* strictBenchmarkValue = std::getenv("MARS_STRICT_REALTIME_BENCHMARK");
    const bool strictRealtimeBenchmark = strictBenchmarkValue != nullptr
                                      && std::string { strictBenchmarkValue } == "1";
    const double maximumRatio = sanitizerBuild ? 40.0
                              : strictRealtimeBenchmark ? 0.95
                                                        : 8.0;
    expect(ratio < maximumRatio,
           sanitizerBuild
               ? "sanitized 16-slot nonlinear render exceeded its diagnostic guardrail"
               : strictRealtimeBenchmark
                   ? "Release 16-slot nonlinear render lost its 5% real-time headroom"
                   : "Release 16-slot nonlinear render exceeded the portable 8x ceiling");
    return ratio;
}

void testCpuRegression()
{
    const double semRatio = benchmarkCpuModel(mars::FilterModel::Sem);
    const double ladderRatio = benchmarkCpuModel(mars::FilterModel::Ladder);
    // The ladder may take several bounded residual-decreasing updates while the
    // SEM has one closed-form TPT step. Keep a relative guard as well as the
    // portable absolute ceiling so faster CI hardware cannot hide a solver
    // regression. The strict absolute real-time target is opt-in because shared
    // GitHub runner speed and contention are not stable benchmark fixtures.
    const double relativeLimit = sanitizerBuild ? 4.0 : 2.5;
    expect(ladderRatio < semRatio * relativeLimit,
           "Ladder render exceeded 2.5x the SEM CPU baseline");
}
} // namespace

int main()
{
    testRenderMatrix();
    testSustainAndRelease();
    testVoiceAllocation();
    testSampleRateLevelConsistency();
    testOversamplingPitchAndPitchBend();
    testLadderImplicitSolveAgainstReference();
    testLadderAdversarialControlJump();
    testLadderFullRangeDoesNotLatch();
    testLadderBassCompensation();
    testDeepOscillatorConsistency();
    testOversamplingConfigurationAndDeferredChange();
    testParameterSanitisation();
    testSemShapeEndpoints();
    testDeterminismAndOscillatorResponses();
    testOscillatorMixerSwitches();
    testOscillatorSwitchContinuityAndCrossMod();
    testAnalogModelResponses();
    testLongHeldTriangleStability();
    testClicklessRetriggerAndVoiceSteal();
    testClicklessFilterModelSwitch();
    testGlideAndModulationMeaningfulness();
    testExtremeAutomationStability();
    testCpuRegression();

    if (failures != 0)
    {
        std::cerr << failures << " Mars DSP check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Mars DSP checks passed.\n";
    return EXIT_SUCCESS;
}
