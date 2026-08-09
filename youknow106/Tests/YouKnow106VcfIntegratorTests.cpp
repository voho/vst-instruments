// Focused numerical contracts for the production OTA-cascade integrator.

#include "DSP/YouKnow106Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace youknow106
{
// Narrow seam into the private cascade. It is local to this test executable
// and is not part of the plug-in API.
struct YouKnow106TestAccess
{
    using Cascade = YouKnow106Engine::OtaCascade;
    using ControlTrajectory = Cascade::ControlTrajectory;
    using HoldInterval = YouKnow106Engine::VcfHoldInterval;

    static constexpr double pi = 3.14159265358979323846264338327950288;

    static constexpr int integrationSubsteps() noexcept
    {
        return Cascade::integrationSubsteps;
    }

    static constexpr int rhsEvaluationsPerInterval() noexcept
    {
        return Cascade::rhsEvaluationsPerInterval;
    }

    static constexpr double maximumOmegaStep() noexcept
    {
        return Cascade::maximumOmegaStep;
    }

    static constexpr double maximumInputReconstructionL1() noexcept
    {
        return Cascade::maximumInputReconstructionL1;
    }

    static constexpr const auto& controlNodePositions() noexcept
    {
        return Cascade::controlNodePositions;
    }

    static HoldInterval exactHold(
        float state, float target, bool hasEvent, double eventPosition,
        float eventTarget, double intervalSeconds) noexcept
    {
        return YouKnow106Engine::exactVcfHoldInterval(
            state, target, hasEvent, eventPosition, eventTarget,
            intervalSeconds);
    }

    static constexpr double headroom() noexcept
    {
        return YouKnow106Engine::otaHeadroomVolts;
    }

    static constexpr double feedbackHeadroom() noexcept
    {
        return YouKnow106Engine::VoicedResonanceCompatibilityProfile::
            loopHeadroomVolts;
    }

    static constexpr double earlyCoefficient() noexcept
    {
        return YouKnow106Engine::otaEarlyEffectCoefficient;
    }

    static constexpr double calibrationCeiling() noexcept
    {
        return youknow106::EngineParameters::calibrationCeiling;
    }

    static double thermalCutoffSpread(int cardIndex,
                                      double calibration) noexcept
    {
        return 1.0 + YouKnow106Engine::vcfCutoffTempcoPerCelsius
            * calibration
            * (YouKnow106Engine::chassisGradientCelsius(cardIndex)
               - YouKnow106Engine::chassisGradientMeanCelsius());
    }

    static double boundedThermalOmegaStep(
        double baseOmega, int cardIndex, double calibration) noexcept
    {
        EngineParameters parameters;
        parameters.enableSpatialThermalGradient = true;
        parameters.calibration = static_cast<float>(calibration);
        return YouKnow106Engine::boundedThermalFilterOmegaStep(
            static_cast<float>(baseOmega), parameters, cardIndex);
    }

    static double clampOmegaStep(double value) noexcept
    {
        return Cascade::clampOmegaStep(value);
    }

    static std::array<float, 4> maximumCharacterCardScales(
        int cardIndex) noexcept
    {
        const std::uint32_t seed = static_cast<std::uint32_t>(cardIndex)
            * 2654435761u + 17u;
        std::array<float, 4> result {};
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = 1.0f
                + YouKnow106Engine::hashBipolar(
                    seed + 20u + static_cast<std::uint32_t>(stage))
                    * YouKnow106Engine::vcfStageCapacitorTolerance
                    * static_cast<float>(calibrationCeiling());
        return result;
    }

    static std::array<float, 4> maximumCharacterCardOffsets(
        int cardIndex) noexcept
    {
        const std::uint32_t seed = static_cast<std::uint32_t>(cardIndex)
            * 2654435761u + 17u;
        std::array<float, 4> result {};
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = 0.0015f
                * YouKnow106Engine::hashBipolar(
                    seed + 10u + static_cast<std::uint32_t>(stage))
                / YouKnow106Engine::stageAttenuation
                * static_cast<float>(calibrationCeiling());
        return result;
    }

    static double maximumCharacterHeadroom(
        int cardIndex, double warmupFraction) noexcept
    {
        YouKnow106Engine engine;
        engine.thermalWarmupFraction_ = static_cast<float>(warmupFraction);
        EngineParameters parameters;
        parameters.enableSpatialThermalGradient = true;
        parameters.calibration =
            static_cast<float>(calibrationCeiling());
        return engine.dynamicOtaHeadroomVolts(parameters, cardIndex);
    }

    static double reconstruct(double current,
                              const std::array<double, 3>& history,
                              double position) noexcept
    {
        return Cascade::reconstructInput(current, history, position);
    }

    static void configure(
        Cascade& cascade, const std::array<float, 4>& scale,
        const std::array<float, 4>& offset) noexcept
    {
        cascade.gScale = scale;
        cascade.offsetVoltage = offset;
    }

    static void setState(Cascade& cascade,
                         const std::array<double, 4>& state) noexcept
    {
        cascade.state = state;
    }

    static const std::array<double, 4>& state(const Cascade& cascade) noexcept
    {
        return cascade.state;
    }

    static void seedRetimeState(
        Cascade& cascade, const std::array<double, 4>& state,
        const std::array<double, 3>& history, double omega) noexcept
    {
        cascade.state = state;
        cascade.inputHistory = history;
        cascade.inputHistoryCount = 2;
        cascade.previousOmegaStep = omega;
        cascade.previousFeedback = 3.0;
        cascade.previousHeadroom = headroom();
        cascade.parameterHistoryPrimed = true;
    }

    static const std::array<double, 3>& inputHistory(
        const Cascade& cascade) noexcept
    {
        return cascade.inputHistory;
    }

    static double previousOmegaStep(const Cascade& cascade) noexcept
    {
        return cascade.previousOmegaStep;
    }
};
} // namespace youknow106

namespace
{
using Access = youknow106::YouKnow106TestAccess;
using Cascade = Access::Cascade;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expectNear(double actual, double expected, double tolerance,
                const std::string& message)
{
    expect(std::isfinite(actual) && std::abs(actual - expected) <= tolerance,
           message + ": got " + std::to_string(actual)
               + ", expected " + std::to_string(expected));
}

template <typename Function>
double cubicPolynomial(Function coefficient, double x)
{
    return coefficient(0) + x * (coefficient(1)
        + x * (coefficient(2) + x * coefficient(3)));
}

std::array<double, 4> reconstructionWeights(double position)
{
    std::array<double, 4> result {};
    for (std::size_t sample = 0; sample < result.size(); ++sample)
    {
        std::array<double, 3> history {};
        const double current = sample == 0 ? 1.0 : 0.0;
        if (sample > 0)
            history[sample - 1] = 1.0;
        result[sample] = Access::reconstruct(current, history, position);
    }
    return result;
}

double independentReconstruction(double current,
                                 const std::array<double, 3>& history,
                                 double position)
{
    const double t = position;
    const double currentWeight = t * (t + 1.0) * (t + 2.0) / 6.0;
    const double previousWeight =
        -(t - 1.0) * (t + 1.0) * (t + 2.0) / 2.0;
    const double previous2Weight =
        (t - 1.0) * t * (t + 2.0) / 2.0;
    const double previous3Weight =
        -(t - 1.0) * t * (t + 1.0) / 6.0;
    return currentWeight * current + previousWeight * history[0]
         + previous2Weight * history[1] + previous3Weight * history[2];
}

void testCausalCubicContracts()
{
    constexpr std::array<std::array<double, 4>, 4> polynomials {{
        {{ 0.75, 0.0, 0.0, 0.0 }},
        {{ -0.2, 1.3, 0.0, 0.0 }},
        {{ 0.4, -0.7, 0.25, 0.0 }},
        {{ -0.3, 0.8, -0.15, 0.09 }}
    }};
    for (const auto& coefficients : polynomials)
    {
        const auto polynomial = [&](int term) {
            return coefficients[static_cast<std::size_t>(term)];
        };
        const std::array<double, 3> history {
            cubicPolynomial(polynomial, 0.0),
            cubicPolynomial(polynomial, -1.0),
            cubicPolynomial(polynomial, -2.0)
        };
        const double current = cubicPolynomial(polynomial, 1.0);
        for (int point = 0; point <= 32; ++point)
        {
            const double position = static_cast<double>(point) / 32.0;
            expectNear(Access::reconstruct(current, history, position),
                       cubicPolynomial(polynomial, position), 2.0e-14,
                       "causal reconstruction is not cubic-exact");
        }
        expectNear(Access::reconstruct(current, history, 0.0), history[0],
                   0.0, "causal reconstruction misses its old endpoint");
        expectNear(Access::reconstruct(current, history, 1.0), current,
                   0.0, "causal reconstruction misses its current endpoint");
    }

    constexpr int divisions = 200000;
    double maximumL1 = 0.0;
    double coefficientEnergyIntegral = 0.0;
    double previousEnergy = 0.0;
    double maximumContinuousAlternating = 0.0;
    for (int point = 0; point <= divisions; ++point)
    {
        const double position = static_cast<double>(point) / divisions;
        const auto weights = reconstructionWeights(position);
        double l1 = 0.0;
        double energy = 0.0;
        for (double weight : weights)
        {
            l1 += std::abs(weight);
            energy += weight * weight;
        }
        maximumL1 = std::max(maximumL1, l1);
        maximumContinuousAlternating = std::max(
            maximumContinuousAlternating,
            std::abs(-weights[0] + weights[1] - weights[2] + weights[3]));
        if (point > 0)
            coefficientEnergyIntegral +=
                0.5 * (previousEnergy + energy) / divisions;
        previousEnergy = energy;
    }
    expect(maximumL1 <= Access::maximumInputReconstructionL1(),
           "causal cubic exceeds its declared finite-gain bound");
    expectNear(maximumL1, 1.63113030944, 2.0e-10,
               "causal cubic L1 exposure changed");
    expectNear(10.0 * std::log10(coefficientEnergyIntegral),
               -0.00920116, 2.0e-7,
               "causal cubic white-noise variance changed");
    expectNear(maximumContinuousAlternating, 1.18807518, 2.0e-8,
               "causal cubic continuous alternating exposure changed");

    constexpr std::array<double, 7> mersonNodes {
        0.0, 1.0 / 6.0, 1.0 / 4.0, 1.0 / 2.0,
        2.0 / 3.0, 3.0 / 4.0, 1.0
    };
    const auto maximumMersonAlternating = [&] {
        double result = 0.0;
        for (double position : mersonNodes)
        {
            const auto weights = reconstructionWeights(position);
            result = std::max(
                result,
                std::abs(-weights[0] + weights[1]
                         - weights[2] + weights[3]));
        }
        return result;
    };
    expectNear(maximumMersonAlternating(), 19.0 / 16.0, 2.0e-12,
               "causal cubic Merson-node alternating exposure changed");
}

void testExactFractionalVcfHoldTrajectory()
{
    constexpr float state = 0.21f;
    constexpr float initialTarget = 0.78f;
    constexpr float eventTarget = 0.06f;
    constexpr double eventPosition = 0.37;
    constexpr double intervalSeconds = 1.0 / 32000.0;
    constexpr double holdSeconds = static_cast<double>(522.0e-6f);
    const auto interval = Access::exactHold(
        state, initialTarget, true, eventPosition, eventTarget,
        intervalSeconds);

    const auto independent = [=](double position) {
        const double eventState = static_cast<double>(initialTarget)
            + (static_cast<double>(state)
               - static_cast<double>(initialTarget))
                * std::exp(-eventPosition * intervalSeconds / holdSeconds);
        if (position < eventPosition)
        {
            return static_cast<double>(initialTarget)
                + (static_cast<double>(state)
                   - static_cast<double>(initialTarget))
                    * std::exp(-position * intervalSeconds / holdSeconds);
        }
        return static_cast<double>(eventTarget)
            + (eventState - static_cast<double>(eventTarget))
                * std::exp(-(position - eventPosition)
                           * intervalSeconds / holdSeconds);
    };
    for (std::size_t point = 0;
         point < Access::controlNodePositions().size(); ++point)
    {
        expectNear(interval.value[point],
                   independent(Access::controlNodePositions()[point]),
                   2.0e-15,
                   "fractional VCF hold misses its segmented exact value");
    }
    expectNear(interval.value.front(), state, 0.0,
               "fractional VCF hold changed its interval-start state");
    expectNear(interval.endpoint,
               static_cast<float>(independent(1.0)), 0.0,
               "fractional VCF hold endpoint was rounded inconsistently");

    const auto eventAtEnd = Access::exactHold(
        state, initialTarget, true, 1.0, eventTarget, intervalSeconds);
    const auto noEvent = Access::exactHold(
        state, initialTarget, false, 0.0, eventTarget, intervalSeconds);
    expect(eventAtEnd.value == noEvent.value
               && eventAtEnd.endpoint == noEvent.endpoint,
           "an endpoint VCF write affected the hold before its physical time");

    const auto eventAtStart = Access::exactHold(
        state, initialTarget, true, 0.0, eventTarget, intervalSeconds);
    for (std::size_t point = 0;
         point < Access::controlNodePositions().size(); ++point)
    {
        const double position = Access::controlNodePositions()[point];
        const double expected = static_cast<double>(eventTarget)
            + (static_cast<double>(state) - eventTarget)
                * std::exp(-position * intervalSeconds / holdSeconds);
        expectNear(eventAtStart.value[point], expected, 2.0e-15,
                   "a boundary VCF write did not own the new interval");
    }

    expect(Access::controlNodePositions()
               == std::array<double, 7> {
                    0.0, 1.0 / 6.0, 1.0 / 4.0, 1.0 / 2.0,
                    2.0 / 3.0, 3.0 / 4.0, 1.0 },
           "VCF hold nodes no longer coincide with the fixed Merson tableau");
}

void testExplicitControlTrajectoryVisitsEveryMersonNode()
{
    constexpr float input = 1.4f;
    constexpr float omega = 0.72f;
    constexpr float feedback = 2.6f;
    const float headroom = static_cast<float>(Access::headroom());
    Access::ControlTrajectory baselineTrajectory;
    baselineTrajectory.omegaStep.fill(omega);
    baselineTrajectory.feedback.fill(feedback);
    baselineTrajectory.headroom.fill(headroom);

    const auto render = [&](const Access::ControlTrajectory& trajectory) {
        Cascade cascade;
        cascade.reset();
        Access::setState(cascade, { 0.31, -0.19, 0.11, -0.07 });
        cascade.process(input, omega, feedback, headroom, true, 0.70f,
                        &trajectory);
        return Access::state(cascade);
    };
    const auto baseline = render(baselineTrajectory);
    for (std::size_t point = 0;
         point < Access::controlNodePositions().size(); ++point)
    {
        auto perturbed = baselineTrajectory;
        perturbed.omegaStep[point] += 0.19;
        const auto actual = render(perturbed);
        double maximumDifference = 0.0;
        for (std::size_t stage = 0; stage < actual.size(); ++stage)
            maximumDifference = std::max(
                maximumDifference,
                std::abs(actual[stage] - baseline[stage]));
        expect(maximumDifference > 1.0e-10,
               "explicit VCF trajectory ignored Merson control node "
                   + std::to_string(point));
    }
}

void testMersonTableauAndStabilityPolynomial()
{
    // Merson's five explicit rows. Row sums are its abscissae and the update
    // weights sum to one; the repeated one-third node is intentionally one
    // shared reconstruction/control evaluation in each half-step.
    constexpr std::array<double, 5> rowSums {
        0.0,
        1.0 / 3.0,
        1.0 / 6.0 + 1.0 / 6.0,
        1.0 / 8.0 + 3.0 / 8.0,
        1.0 / 2.0 - 3.0 / 2.0 + 2.0
    };
    constexpr std::array<double, 5> abscissae {
        0.0, 1.0 / 3.0, 1.0 / 3.0, 1.0 / 2.0, 1.0
    };
    expect(rowSums == abscissae,
           "Merson tableau rows no longer land on their declared abscissae");
    expectNear(1.0 / 6.0 + 2.0 / 3.0 + 1.0 / 6.0,
               1.0, 2.0e-16,
               "Merson update weights no longer sum to one");

    const auto mersonStep = [](double value, double z, double step) {
        const auto rhs = [z](double state) { return z * state; };
        const double k1 = rhs(value);
        const double k2 = rhs(value + step * k1 / 3.0);
        const double k3 = rhs(value + step * (k1 + k2) / 6.0);
        const double k4 = rhs(value + step * (k1 / 8.0 + 3.0 * k3 / 8.0));
        const double k5 = rhs(value + step
            * (k1 / 2.0 - 3.0 * k3 / 2.0 + 2.0 * k4));
        return value + step * (k1 / 6.0 + 2.0 * k4 / 3.0 + k5 / 6.0);
    };
    const auto polynomial = [](double z) {
        return 1.0 + z + z * z / 2.0 + z * z * z / 6.0
            + z * z * z * z / 24.0 + z * z * z * z * z / 144.0;
    };
    for (double z : { -4.0, -2.75, -0.5, 0.0, 0.75, 2.0 })
    {
        expectNear(mersonStep(1.0, z, 1.0), polynomial(z), 3.0e-14,
                   "Merson tableau changed its stability polynomial");
        const double expectedFull = polynomial(0.5 * z)
                                  * polynomial(0.5 * z);
        const double actualFull = mersonStep(
            mersonStep(1.0, z, 0.5), z, 0.5);
        expectNear(actualFull, expectedFull, 4.0e-14,
                   "two-half-step Merson composition changed");
    }
}

struct ReferenceCascade
{
    std::array<double, 4> state {};
    std::array<double, 3> inputHistory {};
    std::array<double, 4> scale { 1.0, 1.0, 1.0, 1.0 };
    std::array<double, 4> offset {};
    double previousOmega {};
    double previousFeedback {};
    double previousHeadroom {};
    bool primed {};
    int inputHistoryCount {};

    double process(double input, double omega, double feedback,
                   double headroom, bool early, double calibration,
                   int substeps = 96)
    {
        if (!primed)
        {
            previousOmega = omega;
            previousFeedback = feedback;
            previousHeadroom = headroom;
            primed = true;
        }
        const auto derivative = [&](const std::array<double, 4>& value,
                                    double position) {
            const double runningOmega =
                previousOmega + position * (omega - previousOmega);
            const double runningFeedback = previousFeedback
                + position * (feedback - previousFeedback);
            const double runningHeadroom = previousHeadroom
                + position * (headroom - previousHeadroom);
            double drive = 0.0;
            if (inputHistoryCount == 0)
                drive = position * input
                      + (1.0 - position) * inputHistory[0];
            else if (inputHistoryCount == 1)
                drive = 0.5 * position * (position + 1.0) * input
                      + (1.0 - position * position) * inputHistory[0]
                      + 0.5 * position * (position - 1.0) * inputHistory[1];
            else
                drive = independentReconstruction(
                    input, inputHistory, position);
            double previous = drive - runningFeedback
                * Access::feedbackHeadroom()
                * std::tanh(value[3] / Access::feedbackHeadroom());
            std::array<double, 4> result {};
            for (std::size_t stage = 0; stage < result.size(); ++stage)
            {
                const double earlyScale = early && calibration > 0.0
                    ? 1.0 + Access::earlyCoefficient() * calibration
                        * std::tanh(value[stage] / runningHeadroom)
                    : 1.0;
                result[stage] = runningOmega * scale[stage] * earlyScale
                    * runningHeadroom
                    * std::tanh((previous - value[stage] + offset[stage])
                                / runningHeadroom);
                previous = value[stage];
            }
            return result;
        };
        const auto advance = [](const std::array<double, 4>& origin,
                                const std::array<double, 4>& slope,
                                double distance) {
            std::array<double, 4> result {};
            for (std::size_t stage = 0; stage < result.size(); ++stage)
                result[stage] = origin[stage] + distance * slope[stage];
            return result;
        };

        const double step = 1.0 / substeps;
        for (int substep = 0; substep < substeps; ++substep)
        {
            const double position = substep * step;
            const auto k1 = derivative(state, position);
            const auto k2 = derivative(
                advance(state, k1, 0.5 * step), position + 0.5 * step);
            const auto k3 = derivative(
                advance(state, k2, 0.5 * step), position + 0.5 * step);
            const auto k4 = derivative(
                advance(state, k3, step), position + step);
            for (std::size_t stage = 0; stage < state.size(); ++stage)
                state[stage] += step * (k1[stage] + 2.0 * k2[stage]
                                     + 2.0 * k3[stage] + k4[stage]) / 6.0;
        }
        inputHistory[2] = inputHistory[1];
        inputHistory[1] = inputHistory[0];
        inputHistory[0] = input;
        inputHistoryCount = std::min(inputHistoryCount + 1, 2);
        previousOmega = omega;
        previousFeedback = feedback;
        previousHeadroom = headroom;
        return state[3];
    }
};

void testIntegratorAgainstIndependentReference()
{
    constexpr double pi = Access::pi;
    constexpr double sampleRate = 192000.0;
    constexpr std::size_t length = 8192;
    constexpr std::array<float, 4> scale {
        0.985f, 1.012f, 1.021f, 0.992f
    };
    constexpr std::array<float, 4> offset {
        0.0015f, -0.0010f, 0.0005f, -0.0015f
    };
    constexpr double calibration = 0.70;

    Cascade cascade;
    cascade.reset();
    Access::configure(cascade, scale, offset);
    ReferenceCascade reference;
    for (std::size_t stage = 0; stage < scale.size(); ++stage)
    {
        reference.scale[stage] = scale[stage];
        reference.offset[stage] = offset[stage];
    }

    double errorEnergy = 0.0;
    double referenceEnergy = 0.0;
    double maximumError = 0.0;
    for (std::size_t index = 0; index < length; ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const float input = static_cast<float>(
            2.7 * std::sin(2.0 * pi * 220.0 * time)
            + 0.35 * std::sin(2.0 * pi * 1046.502 * time));
        const double cutoff = 11000.0
            + 4800.0 * std::sin(2.0 * pi * 37.0 * time);
        const float omega = static_cast<float>(2.0 * pi * cutoff / sampleRate);
        const float feedback = static_cast<float>(
            3.55 + 0.25 * std::sin(2.0 * pi * 53.0 * time));
        const float headroom = static_cast<float>(Access::headroom()
            * (1.0 + 0.08 * std::sin(2.0 * pi * 29.0 * time)));
        cascade.process(
            input, omega, feedback, headroom, true,
            static_cast<float>(calibration));
        const double actual = Access::state(cascade)[3];
        const double expected = reference.process(
            input, omega, feedback, headroom, true, calibration);
        if (index >= 512)
        {
            const double error = actual - expected;
            errorEnergy += error * error;
            referenceEnergy += expected * expected;
            maximumError = std::max(maximumError, std::abs(error));
        }
    }
    const double relativeErrorDb =
        10.0 * std::log10(errorEnergy / referenceEnergy);
    std::cout << "VCF fixed Merson independent-reference error: "
              << relativeErrorDb << " dB, peak " << maximumError << " V\n";
    expect(relativeErrorDb < -68.0,
           "fixed Merson misses the independently integrated dynamic OTA equations");
    expect(maximumError < 2.0e-3,
           "fixed Merson peak error is too large under dynamic controls");
}

void testRapidControlAlternationAgainstIndependentReference()
{
    constexpr std::size_t length = 6144;
    Cascade cascade;
    cascade.reset();
    ReferenceCascade reference;
    double errorEnergy = 0.0;
    double referenceEnergy = 0.0;
    for (std::size_t index = 0; index < length; ++index)
    {
        const float input = static_cast<float>(
            2.4 * std::sin(0.19 * static_cast<double>(index))
            + 0.35 * std::sin(0.071 * static_cast<double>(index)));
        const float omega = (index & 1u) == 0u ? 1.64f : 1.66f;
        cascade.process(input, omega, 3.8f,
                        static_cast<float>(Access::headroom()), false, 0.0f);
        const double actual = Access::state(cascade)[3];
        const double expected = reference.process(
            input, omega, 3.8, Access::headroom(), false, 0.0);
        if (index >= 512)
        {
            const double error = actual - expected;
            errorEnergy += error * error;
            referenceEnergy += expected * expected;
        }
    }
    const double relativeErrorDb =
        10.0 * std::log10(errorEnergy / referenceEnergy);
    std::cout << "VCF fixed-work alternating-control reference error: "
              << relativeErrorDb << " dB\n";
    expect(relativeErrorDb < -52.0,
           "fixed Merson misses alternating controls in the continuous reference");
}

void testProductGridOmegaCapAndThermalContainment()
{
    const double cap = Access::maximumOmegaStep();
    expectNear(cap, 0.9 * Access::pi, 1.0e-15,
               "VCF numerical cap is not the 0.45-cycle product-grid boundary");
    expect(Access::clampOmegaStep(cap) == cap
               && Access::clampOmegaStep(1.1 * cap) == cap,
           "VCF clamp does not include and contain its declared boundary");

    bool observedPositiveSpread = false;
    for (int card = 0; card < 6; ++card)
    {
        const double spread = Access::thermalCutoffSpread(
            card, Access::calibrationCeiling());
        const double uncapped = cap * spread;
        const double bounded = Access::boundedThermalOmegaStep(
            cap, card, Access::calibrationCeiling());
        expect(bounded <= static_cast<double>(static_cast<float>(cap)),
               "thermal Character escaped the post-spread VCF cap on card "
                   + std::to_string(card));
        if (spread > 1.0)
        {
            observedPositiveSpread = true;
            expect(uncapped > cap,
                   "positive thermal Character fixture does not cross the cap");
            expectNear(bounded, static_cast<float>(cap), 0.0,
                       "post-thermal product was not clamped at the VCF cap");
        }

        const double baseNonCap = 0.4 * cap;
        const double expectedNonCap = baseNonCap * spread;
        const double actualNonCap = Access::boundedThermalOmegaStep(
            baseNonCap, card, Access::calibrationCeiling());
        expectNear(actualNonCap, expectedNonCap, 4.0e-7,
                   "thermal product cap altered a non-cap VCF fixture");
    }
    expect(observedPositiveSpread,
           "six-card thermal fixture no longer exercises cap containment");
}

void testHighGStabilityAndRecovery()
{
    const float maximumReachableOmega =
        static_cast<float>(Access::maximumOmegaStep());

    double worstStableTail = 0.0;
    for (int card = 0; card < 6; ++card)
    {
        const auto cardScale = Access::maximumCharacterCardScales(card);
        const auto cardOffset = Access::maximumCharacterCardOffsets(card);
        for (double warmupFraction : { 0.0, 1.0 })
        {
            const float cardHeadroom = static_cast<float>(
                Access::maximumCharacterHeadroom(card, warmupFraction));
            for (float feedback : { 0.0f, 2.0f, 3.8f })
            {
                Cascade belowThreshold;
                belowThreshold.reset();
                Access::configure(belowThreshold, cardScale, cardOffset);
                Access::setState(belowThreshold,
                                 { 1.0e-6, -1.0e-6, 1.0e-6, -1.0e-6 });
                double tailMinimum =
                    std::numeric_limits<double>::infinity();
                double tailMaximum =
                    -std::numeric_limits<double>::infinity();
                for (int sample = 0; sample < 24000; ++sample)
                {
                    const double output = belowThreshold.process(
                        0.0f, maximumReachableOmega, feedback,
                        cardHeadroom, true,
                        static_cast<float>(Access::calibrationCeiling()));
                    if (sample >= 22000)
                    {
                        tailMinimum = std::min(tailMinimum, output);
                        tailMaximum = std::max(tailMaximum, output);
                    }
                }
                const double tailMotion = tailMaximum - tailMinimum;
                worstStableTail = std::max(worstStableTail, tailMotion);
                expect(tailMotion < 1.0e-8,
                       "Merson creates a false cap-frequency limit cycle on card "
                           + std::to_string(card) + " at k="
                           + std::to_string(feedback) + " warmup="
                           + std::to_string(warmupFraction));
            }
        }
    }
    std::cout << "VCF Merson worst six-card cold/warm cap tail motion: "
              << worstStableTail << " V\n";

    Cascade hostile;
    hostile.reset();
    double peak = 0.0;
    for (int sample = 0; sample < 30000; ++sample)
    {
        const float omega = std::array<float, 4> {
            0.0004f, maximumReachableOmega, 1.0f,
            static_cast<float>(Access::maximumOmegaStep())
        }[static_cast<std::size_t>(sample) % 4u];
        const float feedback = std::array<float, 3> {
            0.0f, 4.504f, 8.0f
        }[static_cast<std::size_t>(sample) % 3u];
        const float input = 3.0f
            * std::sin(0.31f * static_cast<float>(sample));
        const double output = hostile.process(input, omega, feedback);
        peak = std::max(peak, std::abs(output));
        expect(std::isfinite(output),
               "Merson emitted a non-finite hostile-control sample");
    }
    expect(peak < 16.0,
           "Merson exceeded its circuit-scale hostile-control bound");

    Cascade sanitised;
    sanitised.reset();
    const float nonFinite = sanitised.process(
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN());
    expect(std::isfinite(nonFinite),
           "Merson does not sanitise non-finite input and controls");
    Access::setState(sanitised, { 128.0, 0.0, 0.0, 0.0 });
    const float recovered = sanitised.process(0.0f, 1.0f, 0.0f);
    expect(recovered == 0.0f,
           "Merson does not recover an impossible capacitor state");
    expect(std::all_of(Access::state(sanitised).begin(),
                       Access::state(sanitised).end(),
                       [](double state) { return state == 0.0; }),
           "Merson recovery did not clear the whole coupled cascade");
}

void testRetimePreservesPhysicalState()
{
    Cascade cascade;
    cascade.reset();
    constexpr std::array<double, 4> state {
        0.25, -0.5, 0.75, -1.0
    };
    constexpr std::array<double, 3> history { 0.4, -0.3, 0.2 };
    Access::seedRetimeState(cascade, state, history, 0.24);
    cascade.retime(0.20f, 0.10f);
    expect(Access::state(cascade) == state,
           "VCF retime disturbed physical capacitor charge");
    expect(Access::inputHistory(cascade)
               == std::array<double, 3> { history[0], history[0], history[0] },
           "VCF retime reinterpreted old-grid cubic support");
    expectNear(Access::previousOmegaStep(cascade), 0.12, 2.0e-9,
               "VCF retime lost its effective omega ratio");

    const double cap = Access::maximumOmegaStep();
    const double capped = Access::boundedThermalOmegaStep(
        cap, 0, Access::calibrationCeiling());
    const double uncapped = Access::boundedThermalOmegaStep(
        0.5 * cap, 0, Access::calibrationCeiling());
    expectNear(capped, static_cast<float>(cap), 0.0,
               "Character retime fixture does not begin on the cap");
    expect(uncapped < cap,
           "Character retime fixture does not cross below the cap");

    Cascade cappedToUncapped;
    cappedToUncapped.reset();
    Access::seedRetimeState(
        cappedToUncapped, state, history, capped);
    cappedToUncapped.retime(static_cast<float>(capped),
                            static_cast<float>(uncapped));
    expectNear(Access::previousOmegaStep(cappedToUncapped), uncapped,
               2.0e-7,
               "capped-to-uncapped Character retime mapped the wrong endpoint");

    Cascade uncappedToCapped;
    uncappedToCapped.reset();
    Access::seedRetimeState(
        uncappedToCapped, state, history, uncapped);
    uncappedToCapped.retime(static_cast<float>(uncapped),
                            static_cast<float>(capped));
    expectNear(Access::previousOmegaStep(uncappedToCapped), capped,
               2.0e-7,
               "uncapped-to-capped Character retime mapped the wrong endpoint");

    Cascade cappedToCapped;
    cappedToCapped.reset();
    Access::seedRetimeState(cappedToCapped, state, history, capped);
    cappedToCapped.retime(static_cast<float>(capped),
                          static_cast<float>(capped));
    expect(Access::state(cappedToCapped) == state,
           "same-cap grid retime disturbed physical capacitor charge");
    expect(Access::inputHistory(cappedToCapped)
               == std::array<double, 3> { history[0], history[0], history[0] },
           "same-cap grid retime retained old-grid cubic support");
    expectNear(Access::previousOmegaStep(cappedToCapped), capped,
               2.0e-7,
               "same-cap grid retime changed its effective endpoint");

    Cascade settled;
    settled.reset();
    float before = 0.0f;
    for (int sample = 0; sample < 20000; ++sample)
        before = settled.process(0.5f, 0.20f, 0.0f);
    settled.retime(0.20f, 0.10f);
    const float after = settled.process(0.5f, 0.10f, 0.0f);
    expectNear(after, before, 1.0e-6,
               "settled VCF stepped across a numerical-grid change");
}
} // namespace

int main()
{
    expect(Access::integrationSubsteps() == 2
               && Access::rhsEvaluationsPerInterval() == 10,
           "production VCF work is no longer fixed Merson x2 / 10 RHS");
    testCausalCubicContracts();
    testExactFractionalVcfHoldTrajectory();
    testExplicitControlTrajectoryVisitsEveryMersonNode();
    testMersonTableauAndStabilityPolynomial();
    testIntegratorAgainstIndependentReference();
    testRapidControlAlternationAgainstIndependentReference();
    testProductGridOmegaCapAndThermalContainment();
    testHighGStabilityAndRecovery();
    testRetimePreservesPhysicalState();

    if (failures != 0)
    {
        std::cerr << failures << " VCF integrator test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All VCF integrator tests passed\n";
    return EXIT_SUCCESS;
}
