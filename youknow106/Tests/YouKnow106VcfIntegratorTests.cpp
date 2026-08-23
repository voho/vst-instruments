// Focused numerical contracts for the production OTA-cascade integrator.

#include "DSP/YouKnow106Engine.h"

#include <algorithm>
#include <array>
#include <bit>
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

    using Tableau = Cascade::Tableau;
    static Tableau planTableau(youknow106::VcfSolverMode mode, double poleStep,
                               double feedback) noexcept
    {
        return Cascade::planTableau(mode, poleStep, feedback);
    }
    static constexpr unsigned int tableauNodeMask(Tableau tableau) noexcept
    {
        return Cascade::tableauNodeMask(tableau);
    }
    static constexpr int tableauRhsEvaluations(Tableau tableau) noexcept
    {
        return Cascade::tableauRhsEvaluations(tableau);
    }
    static constexpr double singleStepRk4Limit() noexcept
    {
        return Cascade::singleStepRk4Limit;
    }
    static constexpr double halfStepRk4Limit() noexcept
    {
        return Cascade::halfStepRk4Limit;
    }
    static constexpr double maximumOmegaStepValue() noexcept
    {
        return Cascade::maximumOmegaStep;
    }
    static constexpr double maximumClosedLoopSpectralFactor() noexcept
    {
        return Cascade::maximumClosedLoopSpectralFactor;
    }
    static double closedLoopSpectralFactor(double feedback) noexcept
    {
        return Cascade::closedLoopSpectralFactor(feedback);
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

    static double zonedHermiteTanh(double value) noexcept
    {
        return Cascade::zonedHermiteTanh(value);
    }

    static double zonedHermiteTanhUnchecked(double value) noexcept
    {
        return Cascade::zonedHermiteTanhUnchecked(value);
    }

    static double cubicEarlyTanh(double value) noexcept
    {
        return Cascade::cubicEarlyTanh(value);
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

float processSelected(
    Cascade& cascade, float input, float omegaStep, float feedback,
    float headroom, bool enableEarlyEffect, float calibration,
    const Access::ControlTrajectory* trajectory,
    youknow106::VcfTanhMode tanhMode,
    youknow106::VcfFastEarlyMode earlyMode,
    youknow106::VcfSolverMode solverMode =
        youknow106::VcfSolverMode::MersonHalfSteps) noexcept
{
    if (tanhMode == youknow106::VcfTanhMode::ZonedHermite
        && earlyMode == youknow106::VcfFastEarlyMode::Cubic)
        return cascade.process<true>(
            input, omegaStep, feedback, headroom, enableEarlyEffect,
            calibration, trajectory, tanhMode, solverMode);
    return cascade.process(
        input, omegaStep, feedback, headroom, enableEarlyEffect,
        calibration, trajectory, tanhMode, solverMode);
}

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

void testZonedHermiteTanhKernelAndDispatch()
{
    expect(youknow106::EngineParameters {}.vcfTanhMode
               == youknow106::VcfTanhMode::Exact,
           "EngineParameters no longer defaults the VCF tanh mode to exact");
    expect(youknow106::EngineParameters {}.vcfFastEarlyMode
               == youknow106::VcfFastEarlyMode::Hermite,
           "EngineParameters no longer defaults Fast Early to Hermite");

    const double positiveZero = Access::zonedHermiteTanh(0.0);
    const double negativeZero = Access::zonedHermiteTanh(-0.0);
    expect(positiveZero == 0.0 && !std::signbit(positiveZero)
               && negativeZero == 0.0 && std::signbit(negativeZero),
           "zoned Hermite tanh does not preserve signed zero");

    const double minimumNormal = std::numeric_limits<double>::min();
    const double largestSubnormal = std::nextafter(minimumNormal, 0.0);
    const double aboveMinimumNormal = std::nextafter(
        minimumNormal, std::numeric_limits<double>::infinity());
    for (const double magnitude : {
             std::numeric_limits<double>::denorm_min(),
             std::bit_cast<double>(std::uint64_t { 0x0008000000000000 }),
             largestSubnormal })
        for (const double value : { magnitude, -magnitude })
        {
            expect(std::bit_cast<std::uint64_t>(Access::zonedHermiteTanh(value))
                       == std::bit_cast<std::uint64_t>(value),
                   "zoned Hermite tanh does not preserve a subnormal input");
            expect(std::bit_cast<std::uint64_t>(
                       Access::zonedHermiteTanhUnchecked(value))
                       == std::bit_cast<std::uint64_t>(value),
                   "trusted zoned Hermite tanh changed a subnormal input");
        }
    expect(Access::zonedHermiteTanh(largestSubnormal)
               <= Access::zonedHermiteTanh(minimumNormal)
               && Access::zonedHermiteTanh(minimumNormal)
                      <= Access::zonedHermiteTanh(aboveMinimumNormal),
           "zoned Hermite tanh is not monotone at the subnormal bypass boundary");

    constexpr double nearZero = 1.0e-8;
    expect(std::abs(Access::zonedHermiteTanh(nearZero) / nearZero - 1.0)
               <= 2.0e-12,
           "zoned Hermite tanh lost unit slope near zero");

    const double nan = std::numeric_limits<double>::quiet_NaN();
    expect(std::isnan(Access::zonedHermiteTanh(nan))
               && std::isnan(Access::zonedHermiteTanh(-nan)),
           "zoned Hermite tanh does not propagate NaN");
    for (double value : { 19.0, 20.0, std::numeric_limits<double>::max(),
                          std::numeric_limits<double>::infinity() })
    {
        expect(Access::zonedHermiteTanh(value) == 1.0
                   && Access::zonedHermiteTanh(-value) == -1.0,
               "zoned Hermite tanh does not saturate explicitly at +/-19");
    }

    constexpr int divisions = 1000000;
    constexpr double limit = 19.0;
    double maximumError = 0.0;
    double maximumRelativeError = 0.0;
    double errorArgument = 0.0;
    double previous = 0.0;
    bool finiteAndBounded = true;
    bool monotone = true;
    bool odd = true;
    for (int point = 0; point <= divisions; ++point)
    {
        const double argument = limit * static_cast<double>(point)
                              / static_cast<double>(divisions);
        const double actual = Access::zonedHermiteTanh(argument);
        const double negative = Access::zonedHermiteTanh(-argument);
        const double error = std::abs(actual - std::tanh(argument));
        if (argument > 0.0)
            maximumRelativeError = std::max(
                maximumRelativeError, error / std::tanh(argument));
        if (error > maximumError)
        {
            maximumError = error;
            errorArgument = argument;
        }
        finiteAndBounded = finiteAndBounded
            && std::isfinite(actual) && actual >= 0.0 && actual <= 1.0;
        monotone = monotone && actual >= previous;
        odd = odd && negative == -actual;
        previous = actual;
    }
    expect(finiteAndBounded,
           "zoned Hermite tanh escaped its finite unit bounds");
    expect(monotone, "zoned Hermite tanh is not monotone on [0, 19]");
    expect(odd, "zoned Hermite tanh is not exactly odd");
    expect(maximumError <= 1.3e-8,
           "zoned Hermite tanh exceeds its dense-grid error bound: "
               + std::to_string(maximumError));
    expect(maximumRelativeError <= 1.0e-7,
           "zoned Hermite tanh exceeds its relative-error bound: "
               + std::to_string(maximumRelativeError));
    std::cout << "Zoned Hermite tanh dense maximum error: "
              << maximumError << " at x=" << errorArgument
              << ", maximum relative error: " << maximumRelativeError
              << '\n';

    const auto checkNode = [](double argument) {
        expectNear(Access::zonedHermiteTanh(argument), std::tanh(argument),
                   2.0e-15, "zoned Hermite tanh misses an exact table node");
        if (argument > 0.0 && argument < 19.0)
        {
            const double left = std::nextafter(argument, 0.0);
            const double right = std::nextafter(
                argument, std::numeric_limits<double>::infinity());
            expect(Access::zonedHermiteTanh(left)
                       <= Access::zonedHermiteTanh(argument)
                       && Access::zonedHermiteTanh(argument)
                              <= Access::zonedHermiteTanh(right),
                   "zoned Hermite tanh is not monotone at a table boundary");
        }
    };
    for (int node = 0; node <= 160; ++node)
        checkNode(static_cast<double>(node) / 32.0);
    for (int node = 1; node <= 56; ++node)
        checkNode(5.0 + static_cast<double>(node) / 4.0);

    const double beforeSaturation = Access::zonedHermiteTanh(
        std::nextafter(19.0, 0.0));
    expect(beforeSaturation <= Access::zonedHermiteTanh(19.0)
               && beforeSaturation >= std::nextafter(1.0, 0.0),
           "zoned Hermite tanh has more than a one-ULP saturation seam");

    // The fine and tail tables share the exact value and analytic derivative
    // at x=5. Check both one-sided numerical slopes so a future table change
    // cannot introduce an otherwise-monotone kink at the hot/cold seam.
    constexpr double zoneSeam = 5.0;
    constexpr double derivativeStep = 1.0e-5;
    const double seamValue = Access::zonedHermiteTanh(zoneSeam);
    const double leftDerivative = (seamValue
        - Access::zonedHermiteTanh(zoneSeam - derivativeStep))
        / derivativeStep;
    const double rightDerivative = (Access::zonedHermiteTanh(
        zoneSeam + derivativeStep) - seamValue) / derivativeStep;
    expect(std::abs(leftDerivative - rightDerivative) <= 5.0e-9,
           "zoned Hermite tanh loses derivative continuity at x=5");

    // A former exact-linear shortcut ended immediately below this value and
    // created a tiny downward step that the uniform dense grid did not sample.
    // Keep an explicit neighbour check at the retired seam as a regression.
    constexpr double retiredLinearSeam = 1.0e-4;
    expect(Access::zonedHermiteTanh(std::nextafter(retiredLinearSeam, 0.0))
               <= Access::zonedHermiteTanh(retiredLinearSeam)
               && Access::zonedHermiteTanh(retiredLinearSeam)
                      <= Access::zonedHermiteTanh(std::nextafter(
                          retiredLinearSeam,
                          std::numeric_limits<double>::infinity())),
           "zoned Hermite tanh is not monotone at the retired tiny-input seam");

    Cascade exact;
    Cascade approximate;
    exact.reset();
    approximate.reset();
    Access::setState(exact, { 0.6, -0.4, 0.2, -0.1 });
    Access::setState(approximate, { 0.6, -0.4, 0.2, -0.1 });
    double maximumStateDifference = 0.0;
    for (int sample = 0; sample < 2048; ++sample)
    {
        const float input = 2.8f
            * std::sin(0.17f * static_cast<float>(sample));
        exact.process(input, 1.7f, 4.5f,
                      static_cast<float>(Access::headroom()), true, 1.0f,
                      nullptr, youknow106::VcfTanhMode::Exact);
        approximate.process(input, 1.7f, 4.5f,
                            static_cast<float>(Access::headroom()), true,
                            1.0f, nullptr,
                            youknow106::VcfTanhMode::ZonedHermite);
        for (std::size_t stage = 0; stage < Access::state(exact).size(); ++stage)
        {
            maximumStateDifference = std::max(
                maximumStateDifference,
                std::abs(Access::state(exact)[stage]
                         - Access::state(approximate)[stage]));
        }
    }
    expect(maximumStateDifference > 0.0,
           "VCF tanh mode selector did not reach the nonlinear solver");
}

void testCubicEarlyTanhKernelAndDispatch()
{
    const double positiveZero = Access::cubicEarlyTanh(0.0);
    const double negativeZero = Access::cubicEarlyTanh(-0.0);
    expect(positiveZero == 0.0 && !std::signbit(positiveZero)
               && negativeZero == 0.0 && std::signbit(negativeZero),
           "cubic Early tanh does not preserve signed zero");

    const double nan = std::numeric_limits<double>::quiet_NaN();
    expect(std::isnan(Access::cubicEarlyTanh(nan)),
           "cubic Early tanh does not propagate NaN");
    expect(Access::cubicEarlyTanh(
               std::numeric_limits<double>::infinity()) == 1.0
               && Access::cubicEarlyTanh(
                   -std::numeric_limits<double>::infinity()) == -1.0,
           "cubic Early tanh does not saturate infinities");

    constexpr int divisions = 1000000;
    constexpr double limit = 3.0;
    double maximumError = 0.0;
    double previous = 0.0;
    bool finiteAndBounded = true;
    bool monotone = true;
    bool odd = true;
    for (int point = 0; point <= divisions; ++point)
    {
        const double argument = limit * static_cast<double>(point)
                              / static_cast<double>(divisions);
        const double actual = Access::cubicEarlyTanh(argument);
        maximumError = std::max(
            maximumError, std::abs(actual - std::tanh(argument)));
        finiteAndBounded = finiteAndBounded
            && std::isfinite(actual) && actual >= 0.0 && actual <= 1.0;
        monotone = monotone && actual >= previous;
        odd = odd && Access::cubicEarlyTanh(-argument) == -actual;
        previous = actual;
    }
    expect(finiteAndBounded && monotone && odd,
           "cubic Early tanh lost its odd, monotone unit bounds");
    expect(maximumError <= 0.112848,
           "cubic Early tanh exceeds its transfer-error bound");
    const double maximumMultiplierError = maximumError
        * Access::earlyCoefficient() * Access::calibrationCeiling();
    expect(maximumMultiplierError <= 0.00113,
           "cubic Early multiplier exceeds its Character-2 error bound");

    constexpr double nearZero = 1.0e-8;
    expect(std::abs(Access::cubicEarlyTanh(nearZero) / nearZero - 1.0)
               <= 1.0e-15,
           "cubic Early tanh lost unit slope near zero");
    constexpr double seam = 1.5;
    constexpr double step = 1.0e-5;
    const double leftSlope = (Access::cubicEarlyTanh(seam)
        - Access::cubicEarlyTanh(seam - step)) / step;
    const double rightSlope = (Access::cubicEarlyTanh(seam + step)
        - Access::cubicEarlyTanh(seam)) / step;
    expect(Access::cubicEarlyTanh(seam) == 1.0
               && std::abs(leftSlope - rightSlope) <= 1.0e-5,
           "cubic Early tanh is not C1 at saturation");

    const auto render = [](youknow106::VcfTanhMode tanhMode,
                           youknow106::VcfFastEarlyMode earlyMode,
                           bool enableEarly, float calibration) {
        Cascade cascade;
        cascade.reset();
        Access::setState(cascade, { 0.6, -0.4, 0.2, -0.1 });
        for (int sample = 0; sample < 512; ++sample)
        {
            const float input = 2.8f
                * std::sin(0.17f * static_cast<float>(sample));
            processSelected(
                cascade, input, 1.7f, 4.5f,
                static_cast<float>(Access::headroom()), enableEarly,
                calibration, nullptr, tanhMode, earlyMode);
        }
        return Access::state(cascade);
    };
    const auto exactHermite = render(
        youknow106::VcfTanhMode::Exact,
        youknow106::VcfFastEarlyMode::Hermite, true, 1.0f);
    const auto exactCubic = render(
        youknow106::VcfTanhMode::Exact,
        youknow106::VcfFastEarlyMode::Cubic, true, 1.0f);
    expect(exactHermite == exactCubic,
           "Fast Early selector changed the frozen Exact path");

    const auto fastHermite = render(
        youknow106::VcfTanhMode::ZonedHermite,
        youknow106::VcfFastEarlyMode::Hermite, true, 1.0f);
    const auto fastCubic = render(
        youknow106::VcfTanhMode::ZonedHermite,
        youknow106::VcfFastEarlyMode::Cubic, true, 1.0f);
    expect(fastHermite != fastCubic,
           "Fast Early selector did not reach the Character transfer");
    expect(render(youknow106::VcfTanhMode::ZonedHermite,
                  youknow106::VcfFastEarlyMode::Hermite, true, 0.0f)
               == render(youknow106::VcfTanhMode::ZonedHermite,
                         youknow106::VcfFastEarlyMode::Cubic, true, 0.0f)
               && render(youknow106::VcfTanhMode::ZonedHermite,
                         youknow106::VcfFastEarlyMode::Hermite, false, 1.0f)
               == render(youknow106::VcfTanhMode::ZonedHermite,
                         youknow106::VcfFastEarlyMode::Cubic, false, 1.0f),
           "Fast Early selector changed a bypassed Early path");

    const auto renderMovingBypass = [](youknow106::VcfFastEarlyMode earlyMode,
                                       bool enableEarly,
                                       float calibration) {
        Cascade cascade;
        cascade.reset();
        Access::setState(cascade, { 0.6, -0.4, 0.2, -0.1 });
        std::vector<float> output;
        output.reserve(2048);
        for (int sample = 0; sample < 2048; ++sample)
        {
            const float phase = static_cast<float>(sample);
            output.push_back(processSelected(
                cascade,
                2.8f * std::sin(0.17f * phase),
                0.05f + 2.70f * (0.5f + 0.5f * std::sin(0.0031f * phase)),
                7.8f * (0.5f + 0.5f * std::sin(0.0053f * phase)),
                0.01f + 0.08f * (0.5f + 0.5f * std::cos(0.0047f * phase)),
                enableEarly, calibration, nullptr,
                youknow106::VcfTanhMode::ZonedHermite, earlyMode));
        }
        return std::make_pair(output, Access::state(cascade));
    };
    for (const auto bypass : {
             std::pair { false, 2.0f },
             std::pair { true, 0.0f } })
    {
        const auto hermite = renderMovingBypass(
            youknow106::VcfFastEarlyMode::Hermite,
            bypass.first, bypass.second);
        const auto cubic = renderMovingBypass(
            youknow106::VcfFastEarlyMode::Cubic,
            bypass.first, bypass.second);
        expect(hermite == cubic,
               "cubic integrator changed a moving-control Early bypass");
    }

    std::cout << "Cubic Early tanh maximum transfer error: "
              << maximumError << ", Character-2 multiplier error: "
              << maximumMultiplierError << '\n';
}

void testFastReciprocalNormalizationBound()
{
    std::vector<double> headrooms {
        1.0e-5,
        Access::headroom(),
        Access::feedbackHeadroom()
    };
    for (int card = 0; card < 6; ++card)
        for (double warmup : { 0.0, 1.0 })
            headrooms.push_back(
                Access::maximumCharacterHeadroom(card, warmup));
    std::sort(headrooms.begin(), headrooms.end());
    headrooms.erase(std::unique(headrooms.begin(), headrooms.end()),
                    headrooms.end());

    std::vector<double> boundaries;
    for (int node = 0; node <= 160; ++node)
        boundaries.push_back(static_cast<double>(node) / 32.0);
    for (int node = 1; node <= 56; ++node)
        boundaries.push_back(5.0 + static_cast<double>(node) / 4.0);

    double maximumArgumentDelta = 0.0;
    double maximumTransferDelta = 0.0;
    bool tightlyBounded = true;
    for (const double headroom : headrooms)
        for (const double boundary : boundaries)
            for (const double sign : { -1.0, 1.0 })
            {
                const double centre = sign * boundary * headroom;
                for (const double input : {
                         std::nextafter(centre,
                                        -std::numeric_limits<double>::infinity()),
                         centre,
                         std::nextafter(centre,
                                        std::numeric_limits<double>::infinity()) })
                {
                    const double divided = input / headroom;
                    const double reciprocal = input * (1.0 / headroom);
                    const double argumentDelta = std::abs(divided - reciprocal);
                    const double transferDelta = std::abs(
                        Access::zonedHermiteTanh(divided)
                        - Access::zonedHermiteTanh(reciprocal));
                    maximumArgumentDelta = std::max(
                        maximumArgumentDelta, argumentDelta);
                    maximumTransferDelta = std::max(
                        maximumTransferDelta, transferDelta);
                    tightlyBounded = tightlyBounded
                        && argumentDelta <= 8.0
                            * std::numeric_limits<double>::epsilon()
                            * std::max(1.0, std::abs(divided))
                        && transferDelta <= 4.0
                            * std::numeric_limits<double>::epsilon();
                }
            }
    expect(tightlyBounded,
           "Fast reciprocal normalization exceeds its binary64 rounding bound");
    std::cout << "Fast reciprocal normalization boundary maximums: argument "
              << maximumArgumentDelta << ", transfer "
              << maximumTransferDelta << '\n';
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

// ---------------------------------------------------------------------------
// The solver ladder
// ---------------------------------------------------------------------------

const char* solverName(youknow106::VcfSolverMode mode)
{
    switch (mode)
    {
        case youknow106::VcfSolverMode::Rk4HalfSteps: return "RK4 x2";
        case youknow106::VcfSolverMode::Rk4Single:    return "RK4 x1";
        case youknow106::VcfSolverMode::MersonHalfSteps: break;
    }
    return "Merson x2";
}

void testSolverLadderTableauContracts()
{
    using youknow106::VcfSolverMode;
    // Every abscissa a tableau reads has to be one of the seven shared control
    // nodes, and the node mask has to name exactly those. Any other pairing
    // would either read an uninitialised control or pay for a reconstruction
    // nothing evaluates at.
    struct Expectation
    {
        Access::Tableau tableau;
        unsigned int mask;
        int evaluations;
        // The distinct positions this tableau evaluates at, over the whole
        // interval. Written out here rather than derived, so a change to the
        // tableau bodies has to be restated deliberately.
        std::vector<double> abscissae;
    };
    const auto& positions = Access::controlNodePositions();
    const std::array<Expectation, 3> expectations {
        Expectation { Access::Tableau::MersonHalf, 0b1111111u, 10,
                      { 0.0, 1.0 / 6.0, 1.0 / 4.0, 1.0 / 2.0,
                        2.0 / 3.0, 3.0 / 4.0, 1.0 } },
        Expectation { Access::Tableau::Rk4Half, 0b1101101u, 8,
                      { 0.0, 1.0 / 4.0, 1.0 / 2.0, 3.0 / 4.0, 1.0 } },
        Expectation { Access::Tableau::Rk4Full, 0b1001001u, 4,
                      { 0.0, 1.0 / 2.0, 1.0 } }
    };
    for (const auto& expectation : expectations)
    {
        expect(Access::tableauNodeMask(expectation.tableau) == expectation.mask,
               "a solver tableau changed which control nodes it reads");
        expect(Access::tableauRhsEvaluations(expectation.tableau)
                   == expectation.evaluations,
               "a solver tableau changed its right-hand-side count");
        for (const double abscissa : expectation.abscissae)
        {
            bool found = false;
            for (std::size_t point = 0; point < positions.size(); ++point)
                if (std::abs(positions[point] - abscissa) <= 1.0e-15
                    && ((expectation.mask >> point) & 1u) != 0u)
                    found = true;
            expect(found,
                   "a solver tableau abscissa is not a masked control node");
        }
        // The mask must not name a node the tableau never lands on either.
        std::size_t maskedNodes = 0;
        for (std::size_t point = 0; point < positions.size(); ++point)
            maskedNodes += (expectation.mask >> point) & 1u;
        expect(maskedNodes == expectation.abscissae.size(),
               "a solver tableau mask names a node it never evaluates at");
    }

    // Classic RK4's stability polynomial, which is what makes the single-step
    // rung's escalation limit meaningful.
    const auto rk4Step = [](double value, double z, double step) {
        const auto rhs = [z](double state) { return z * state; };
        const double k1 = rhs(value);
        const double k2 = rhs(value + 0.5 * step * k1);
        const double k3 = rhs(value + 0.5 * step * k2);
        const double k4 = rhs(value + step * k3);
        return value + step * (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
    };
    const auto polynomial = [](double z) {
        return 1.0 + z + z * z / 2.0 + z * z * z / 6.0
            + z * z * z * z / 24.0;
    };
    for (double z : { -2.5, -1.25, -0.5, 0.0, 0.75, 2.0 })
    {
        expectNear(rk4Step(1.0, z, 1.0), polynomial(z), 3.0e-14,
                   "the RK4 tableau changed its stability polynomial");
        expectNear(rk4Step(rk4Step(1.0, z, 0.5), z, 0.5),
                   polynomial(0.5 * z) * polynomial(0.5 * z), 4.0e-14,
                   "two-half-step RK4 composition changed");
    }

    // The escalation rule. `Rk4Single` is a ceiling, not a promise: it must
    // take one step where one is admissible and two where it is not, and the
    // ceiling short circuit must agree with the exact factor everywhere.
    expect(Access::planTableau(VcfSolverMode::MersonHalfSteps, 0.0, 0.0)
               == Access::Tableau::MersonHalf
           && Access::planTableau(VcfSolverMode::MersonHalfSteps, 9.0, 8.0)
               == Access::Tableau::MersonHalf,
           "the Merson rung escalated or de-escalated");
    expect(Access::planTableau(VcfSolverMode::Rk4HalfSteps, 0.0, 0.0)
               == Access::Tableau::Rk4Half,
           "the RK4 x2 rung did not take its own tableau on an easy interval");
    // Merson is underneath both RK4 rungs, not beside them: the product grid's
    // own cap can put the cascade's fastest eigenvalue outside classic RK4's
    // stability region, and the only tableau here whose region reaches that
    // far is the five-stage one.
    expect(Access::planTableau(VcfSolverMode::Rk4HalfSteps, 9.0, 8.0)
               == Access::Tableau::MersonHalf
           && Access::planTableau(VcfSolverMode::Rk4Single, 9.0, 8.0)
               == Access::Tableau::MersonHalf,
           "a cheap rung stayed on RK4 past its stability bound");
    {
        // The worst interval the product grid can present: the omega cap, the
        // Character-ceiling stage spread and the sanitized feedback ceiling.
        const double worstPoleStep = Access::maximumOmegaStepValue() * 1.04
                                   * 1.01;
        for (const auto mode : { VcfSolverMode::Rk4HalfSteps,
                                 VcfSolverMode::Rk4Single })
            expect(Access::planTableau(mode, worstPoleStep, 8.0)
                       == Access::Tableau::MersonHalf,
                   "the product grid cap does not fall back to Merson");
    }
    expect(Access::closedLoopSpectralFactor(0.0) == 1.0,
           "an open loop no longer leaves the small-signal pole alone");
    expect(std::abs(Access::closedLoopSpectralFactor(8.0)
                    - Access::maximumClosedLoopSpectralFactor()) <= 1.0e-12,
           "the declared spectral-factor ceiling is not the value at feedback "
           "eight");
    for (double feedback : { 0.0, 0.5, 2.0, 4.0, 6.0, 8.0 })
    {
        const double factor = Access::closedLoopSpectralFactor(feedback);
        expect(factor >= 1.0
                   && factor <= Access::maximumClosedLoopSpectralFactor(),
               "the closed-loop spectral factor left its declared bounds");
        const double edge = Access::singleStepRk4Limit() / factor;
        expect(Access::planTableau(VcfSolverMode::Rk4Single, edge * 0.99,
                                   feedback) == Access::Tableau::Rk4Full,
               "the single-step rung escalated below its own limit");
        expect(Access::planTableau(VcfSolverMode::Rk4Single, edge * 1.01,
                                   feedback) == Access::Tableau::Rk4Half,
               "the single-step rung did not escalate above its own limit");
        const double halfEdge = Access::halfStepRk4Limit() / factor;
        for (const auto mode : { VcfSolverMode::Rk4HalfSteps,
                                 VcfSolverMode::Rk4Single })
        {
            expect(Access::planTableau(mode, halfEdge * 0.99, feedback)
                       == Access::Tableau::Rk4Half,
                   "an RK4 rung fell back to Merson below the half-step limit");
            expect(Access::planTableau(mode, halfEdge * 1.01, feedback)
                       == Access::Tableau::MersonHalf,
                   "an RK4 rung stayed on RK4 above the half-step limit");
        }
    }
    // Hostile controls must still resolve to a tableau rather than to a
    // comparison against NaN that silently falls through to the cheap one.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    expect(Access::planTableau(VcfSolverMode::Rk4Single, nan, 3.0)
               == Access::Tableau::MersonHalf
           && Access::planTableau(VcfSolverMode::Rk4Single, 0.1, nan)
               == Access::Tableau::Rk4Full,
           "a non-finite control did not resolve to the safe tableau");
}

void testSolverLadderReadsOnlyItsOwnControlNodes()
{
    using youknow106::VcfSolverMode;
    constexpr float input = 1.4f;
    // Chosen so the single-step rung stays on its full-interval tableau both
    // before and after the perturbation below: the point of this test is which
    // nodes that tableau *evaluates* at. The escalation planner deliberately
    // scans all seven trajectory nodes whatever the tableau reads, because a
    // hold's interior curvature can leave the endpoint interval, so a
    // perturbation large enough to change the plan would move the output at
    // every ordinal and prove nothing.
    constexpr float omega = 0.18f;
    constexpr float feedback = 2.6f;
    const float headroom = static_cast<float>(Access::headroom());
    Access::ControlTrajectory baselineTrajectory;
    baselineTrajectory.omegaStep.fill(omega);
    baselineTrajectory.feedback.fill(feedback);
    baselineTrajectory.headroom.fill(headroom);

    const auto render = [&](const Access::ControlTrajectory& trajectory,
                            VcfSolverMode solver) {
        Cascade cascade;
        cascade.reset();
        Access::setState(cascade, { 0.31, -0.19, 0.11, -0.07 });
        cascade.process(input, omega, feedback, headroom, true, 0.70f,
                        &trajectory, youknow106::VcfTanhMode::Exact, solver);
        return Access::state(cascade);
    };

    for (const auto solver : { VcfSolverMode::MersonHalfSteps,
                               VcfSolverMode::Rk4HalfSteps,
                               VcfSolverMode::Rk4Single })
    {
        const auto tableau = Access::planTableau(solver, omega, feedback);
        const unsigned int mask = Access::tableauNodeMask(tableau);
        const auto baseline = render(baselineTrajectory, solver);
        for (std::size_t point = 0;
             point < Access::controlNodePositions().size(); ++point)
        {
            auto perturbed = baselineTrajectory;
            perturbed.omegaStep[point] += 0.19;
            const auto actual = render(perturbed, solver);
            double maximumDifference = 0.0;
            for (std::size_t stage = 0; stage < actual.size(); ++stage)
                maximumDifference = std::max(
                    maximumDifference,
                    std::abs(actual[stage] - baseline[stage]));
            const bool reads = ((mask >> point) & 1u) != 0u;
            const std::string where = std::string(solverName(solver))
                + " control node " + std::to_string(point);
            if (reads)
                expect(maximumDifference > 1.0e-10,
                       "a solver rung ignored " + where);
            else
                expect(maximumDifference == 0.0,
                       "a solver rung read " + where
                           + ", which its mask excludes");
        }
    }
}

void testSolverLadderAgainstIndependentReference()
{
    using youknow106::VcfSolverMode;
    constexpr double pi = Access::pi;
    constexpr std::size_t length = 8192;
    constexpr std::array<float, 4> scale { 0.985f, 1.012f, 1.021f, 0.992f };
    constexpr std::array<float, 4> offset {
        0.0015f, -0.0010f, 0.0005f, -0.0015f };
    constexpr double calibration = 0.70;

    struct Rung
    {
        VcfSolverMode mode;
        // Bounds with roughly ten decibels of margin under the measured
        // values, not theoretical order arguments. They exist so a later
        // change to the tableaux or to the escalation limits cannot quietly
        // cost solve accuracy. The 1x column is the tighter one for every
        // rung because the causal input reconstruction has four times as much
        // signal motion to bridge there.
        double fastGridDb;
        double slowGridDb;
        double peakVolts;
    };
    const std::array<Rung, 3> rungs {
        Rung { VcfSolverMode::MersonHalfSteps, -150.0, -135.0, 2.0e-3 },
        Rung { VcfSolverMode::Rk4HalfSteps,    -140.0, -118.0, 2.0e-3 },
        Rung { VcfSolverMode::Rk4Single,       -115.0,  -92.0, 2.0e-3 }
    };

    // Two internal grids: 192 kHz is 4x at a 48 kHz host, and 48 kHz is 1x.
    for (const double sampleRate : { 192000.0, 48000.0 })
        for (const auto& rung : rungs)
        {
            const double errorBound = sampleRate >= 96000.0
                ? rung.fastGridDb : rung.slowGridDb;
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
                const double cutoff = sampleRate >= 96000.0
                    ? 11000.0 + 4800.0 * std::sin(2.0 * pi * 37.0 * time)
                    : 2600.0 + 1200.0 * std::sin(2.0 * pi * 37.0 * time);
                const float omega = static_cast<float>(
                    2.0 * pi * cutoff / sampleRate);
                const float feedback = static_cast<float>(
                    3.55 + 0.25 * std::sin(2.0 * pi * 53.0 * time));
                const float headroom = static_cast<float>(
                    Access::headroom()
                    * (1.0 + 0.08 * std::sin(2.0 * pi * 29.0 * time)));
                cascade.process(
                    input, omega, feedback, headroom, true,
                    static_cast<float>(calibration), nullptr,
                    youknow106::VcfTanhMode::Exact, rung.mode);
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
            std::cout << "VCF " << solverName(rung.mode) << " at "
                      << static_cast<int>(sampleRate)
                      << " Hz independent-reference error: "
                      << relativeErrorDb << " dB, peak " << maximumError
                      << " V\n";
            expect(relativeErrorDb < errorBound,
                   std::string("VCF ") + solverName(rung.mode)
                       + " misses the independently integrated OTA equations");
            expect(maximumError < rung.peakVolts,
                   std::string("VCF ") + solverName(rung.mode)
                       + " peak error is too large under dynamic controls");
        }
}

void testSolverLadderPreservesTheSelfOscillationAnchor()
{
    using youknow106::VcfSolverMode;
    // The service ADJUSTMENT trims resonance against a self-oscillating sine,
    // so the limit cycle's amplitude and frequency are the audible property
    // the ladder must not move. A free-running oscillator's *phase* is not:
    // two solvers drift apart there without either being wrong.
    const auto limitCycle = [](VcfSolverMode solver) {
        Cascade cascade;
        cascade.reset();
        Access::setState(cascade, { 1.0, 1.0, 1.0, 1.0 });
        const float headroom = static_cast<float>(Access::headroom());
        constexpr float omega = 0.0081f; // about 248 Hz on a 192 kHz grid
        constexpr float feedback = 4.504f;
        for (int sample = 0; sample < 200000; ++sample)
            cascade.process(0.0f, omega, feedback, headroom, true, 1.0f,
                            nullptr, youknow106::VcfTanhMode::Exact, solver);

        double minimum = std::numeric_limits<double>::infinity();
        double maximum = -std::numeric_limits<double>::infinity();
        std::vector<double> trace;
        trace.reserve(48000u);
        for (int sample = 0; sample < 48000; ++sample)
        {
            cascade.process(0.0f, omega, feedback, headroom, true, 1.0f,
                            nullptr, youknow106::VcfTanhMode::Exact, solver);
            const double volts = Access::state(cascade)[3];
            minimum = std::min(minimum, volts);
            maximum = std::max(maximum, volts);
            trace.push_back(volts);
        }
        int crossings = 0;
        std::size_t first = 0;
        std::size_t last = 0;
        for (std::size_t index = 1; index < trace.size(); ++index)
            if (trace[index - 1] <= 0.0 && trace[index] > 0.0)
            {
                if (crossings++ == 0)
                    first = index;
                last = index;
            }
        const double cycles = crossings > 1
            ? static_cast<double>(crossings - 1)
                  / static_cast<double>(last - first)
            : 0.0;
        return std::array<double, 2> { maximum - minimum, cycles };
    };

    const auto merson = limitCycle(VcfSolverMode::MersonHalfSteps);
    expect(merson[0] > 0.5 && merson[1] > 0.0,
           "the self-oscillation fixture did not oscillate");
    for (const auto solver : { VcfSolverMode::Rk4HalfSteps,
                               VcfSolverMode::Rk4Single })
    {
        const auto take = limitCycle(solver);
        const double amplitudeRatio = take[0] / merson[0];
        const double cents = 1200.0 * std::log2(
            take[1] / std::max(merson[1], 1.0e-12));
        std::cout << "VCF " << solverName(solver)
                  << " limit cycle: amplitude x" << amplitudeRatio
                  << ", " << cents << " cents\n";
        // A tenth of a decibel of amplitude and one cent of pitch. The
        // frequency gate is a hundredth of the +/-10 cent window the service
        // procedure itself accepts for this trim; the amplitude gate has no
        // published tolerance to sit under, so it is set at the smallest
        // level difference worth naming.
        expect(std::abs(amplitudeRatio - 1.0) < 0.0115,
               std::string("VCF ") + solverName(solver)
                   + " moved the self-oscillation amplitude");
        expect(std::abs(cents) < 1.0,
               std::string("VCF ") + solverName(solver)
                   + " moved the self-oscillation frequency");
    }
}

void testSolverRungMayBeSwitchedUnderASoundingNote()
{
    using youknow106::VcfSolverMode;
    // The rung is documented as taking effect immediately, with no idle window
    // and no latency change, which is only honest if changing it mid-note is
    // continuous. The capacitor state, the input history and the parameter
    // history are all shared across the rungs, so the only discontinuity a
    // switch can introduce is the difference between two solves of the same
    // interval. Bound that against the sample-to-sample motion the signal
    // already has.
    constexpr std::size_t settle = 2048;
    constexpr std::size_t length = 6144;
    const float headroom = static_cast<float>(Access::headroom());
    const auto drive = [](std::size_t index) {
        return static_cast<float>(
            2.4 * std::sin(0.19 * static_cast<double>(index))
            + 0.35 * std::sin(0.071 * static_cast<double>(index)));
    };

    for (const auto rung : { VcfSolverMode::Rk4HalfSteps,
                             VcfSolverMode::Rk4Single })
    {
        Cascade cascade;
        cascade.reset();
        double previous = 0.0;
        double ordinaryMotion = 0.0;
        double switchMotion = 0.0;
        for (std::size_t index = 0; index < length; ++index)
        {
            // One switch, at the midpoint, under a note that is already
            // sounding and mid-cycle.
            const auto solver = index < length / 2u
                ? VcfSolverMode::MersonHalfSteps : rung;
            const double output = cascade.process(
                drive(index), 0.31f, 3.4f, headroom, true, 1.0f, nullptr,
                youknow106::VcfTanhMode::Exact, solver);
            const double motion = std::abs(output - previous);
            previous = output;
            if (index <= settle)
                continue;
            if (index == length / 2u)
                switchMotion = motion;
            else
                ordinaryMotion = std::max(ordinaryMotion, motion);
        }
        expect(ordinaryMotion > 1.0e-6,
               "the solver-switch fixture produced no signal to compare with");
        expect(switchMotion <= ordinaryMotion,
               std::string("switching to ") + solverName(rung)
                   + " under a sounding note moved the output further than the "
                     "signal's own largest step");
    }
}

void testMersonRungIsTheUnchangedDefault()
{
    // The ladder must not have moved the shipping path. An explicit Merson
    // selection and the historical call with no solver argument have to be the
    // same bits, sample for sample, under controls that exercise the
    // trajectory, the Early effect and the resonance return.
    Access::ControlTrajectory trajectory;
    for (std::size_t point = 0;
         point < Access::controlNodePositions().size(); ++point)
    {
        trajectory.omegaStep[point] = 0.31 + 0.04 * static_cast<double>(point);
        trajectory.feedback[point] = 3.1 + 0.05 * static_cast<double>(point);
        trajectory.headroom[point] = Access::headroom()
            * (1.0 + 0.01 * static_cast<double>(point));
    }
    Cascade historical;
    Cascade selected;
    historical.reset();
    selected.reset();
    Access::setState(historical, { 0.31, -0.19, 0.11, -0.07 });
    Access::setState(selected, { 0.31, -0.19, 0.11, -0.07 });
    for (int sample = 0; sample < 4096; ++sample)
    {
        const float input = static_cast<float>(
            2.4 * std::sin(0.19 * static_cast<double>(sample)));
        const float omega = 0.31f + 0.02f * static_cast<float>(sample % 7);
        const auto* trajectoryFor = sample % 5 == 0 ? &trajectory : nullptr;
        const float first = historical.process(
            input, omega, 3.4f, static_cast<float>(Access::headroom()), true,
            1.0f, trajectoryFor, youknow106::VcfTanhMode::Exact);
        const float second = selected.process(
            input, omega, 3.4f, static_cast<float>(Access::headroom()), true,
            1.0f, trajectoryFor, youknow106::VcfTanhMode::Exact,
            youknow106::VcfSolverMode::MersonHalfSteps);
        if (std::bit_cast<std::uint32_t>(first)
                != std::bit_cast<std::uint32_t>(second))
        {
            expect(false,
                   "selecting Merson explicitly is not bit-identical to the "
                   "historical default at sample "
                       + std::to_string(sample));
            return;
        }
    }
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

void testHighGStabilityAndRecovery(youknow106::VcfTanhMode tanhMode,
                                   const char* modeName,
                                   youknow106::VcfFastEarlyMode earlyMode =
                                       youknow106::VcfFastEarlyMode::Hermite,
                                   youknow106::VcfSolverMode solverMode =
                                       youknow106::VcfSolverMode::
                                           MersonHalfSteps)
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
                    const double output = processSelected(
                        belowThreshold, 0.0f, maximumReachableOmega,
                        feedback, cardHeadroom, true,
                        static_cast<float>(Access::calibrationCeiling()),
                        nullptr, tanhMode, earlyMode, solverMode);
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
                           + std::to_string(warmupFraction) + " in "
                           + modeName + " tanh mode");
            }
        }
    }
    std::cout << "VCF Merson " << modeName
              << " worst six-card cold/warm cap tail motion: "
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
        const double output = processSelected(
            hostile, input, omega, feedback,
            static_cast<float>(Access::headroom()), true, 0.70f, nullptr,
            tanhMode, earlyMode, solverMode);
        peak = std::max(peak, std::abs(output));
        expect(std::isfinite(output),
               std::string("Merson emitted a non-finite hostile-control sample in ")
                   + modeName + " tanh mode");
    }
    expect(peak < 16.0,
           std::string("Merson exceeded its circuit-scale hostile-control bound in ")
               + modeName + " tanh mode");

    Cascade sanitised;
    sanitised.reset();
    const float nonFinite = processSelected(
        sanitised, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), true, 0.70f,
        nullptr, tanhMode, earlyMode, solverMode);
    expect(std::isfinite(nonFinite),
           std::string("Merson does not sanitise non-finite input and controls in ")
               + modeName + " tanh mode");
    for (double poisonedState : {
             128.0,
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity() })
    {
        Cascade recovering;
        recovering.reset();
        Access::setState(recovering, { poisonedState, 0.0, 0.0, 0.0 });
        const float recovered = processSelected(
            recovering, 0.0f, 1.0f, 0.0f,
            static_cast<float>(Access::headroom()), true, 0.70f, nullptr,
            tanhMode, earlyMode, solverMode);
        expect(recovered == 0.0f,
               std::string("Merson does not recover an impossible capacitor state in ")
                   + modeName + " tanh mode");
        expect(std::all_of(Access::state(recovering).begin(),
                           Access::state(recovering).end(),
                           [](double state) { return state == 0.0; }),
               std::string("Merson recovery did not clear the whole coupled cascade in ")
                   + modeName + " tanh mode");
    }
}

void testCalibrationAndTrajectoryNonFiniteGuards(
    youknow106::VcfTanhMode tanhMode,
    youknow106::VcfFastEarlyMode earlyMode =
        youknow106::VcfFastEarlyMode::Hermite,
    youknow106::VcfSolverMode solverMode =
        youknow106::VcfSolverMode::MersonHalfSteps)
{
    // `OtaCascade::process`'s scalar `calibration` argument falls back to
    // 0.0 -- disabling the early-effect term exactly as an explicit 0.0f
    // would -- whenever it is not finite. The engine's only production call
    // site always hands it `parameters.calibration`, which `sanitise()`
    // already guarantees is finite, so this guard had never fired outside
    // a test.
    constexpr float input = 0.83f;
    constexpr float omega = 0.6f;
    constexpr float feedback = 2.1f;
    const float headroomVolts = static_cast<float>(Access::headroom());
    constexpr std::array<double, 4> seedState { 0.12, -0.08, 0.05, -0.03 };

    const auto renderWithCalibration = [&](float calibration) {
        Cascade cascade;
        cascade.reset();
        Access::setState(cascade, seedState);
        const float output = processSelected(
            cascade, input, omega, feedback, headroomVolts, true,
            calibration, nullptr, tanhMode, earlyMode, solverMode);
        return std::make_pair(output, Access::state(cascade));
    };

    const auto calibrationFallback = renderWithCalibration(0.0f);
    for (float poisoned : {
             std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity() })
    {
        const auto actual = renderWithCalibration(poisoned);
        expect(actual.first == calibrationFallback.first
                   && actual.second == calibrationFallback.second,
               "OtaCascade::process's non-finite calibration guard no "
               "longer matches its documented 0.0 fallback");
    }

    const auto renderWithHeadroom = [&](float headroom) {
        Cascade cascade;
        cascade.reset();
        Access::setState(cascade, seedState);
        const float output = processSelected(
            cascade, input, omega, feedback, headroom, true, 0.70f,
            nullptr, tanhMode, earlyMode, solverMode);
        return std::make_pair(output, Access::state(cascade));
    };
    const auto scalarHeadroomFallback = renderWithHeadroom(1.0e-5f);
    for (float poisoned : {
             0.0f,
             -1.0f,
             std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity() })
    {
        const auto actual = renderWithHeadroom(poisoned);
        expect(actual.first == scalarHeadroomFallback.first
                   && actual.second == scalarHeadroomFallback.second,
               "OtaCascade::process's scalar headroom guard no longer "
               "matches its documented 1e-5 fallback");
    }

    // The explicit control trajectory used for VCF hold events feeds every
    // Merson node its own feedback/headroom value, guarded by the same
    // non-finite fallback contract as the scalar arguments above (feedback
    // -> 0.0, headroom -> max(0.0, 1e-5) = 1e-5). The engine only ever
    // builds that trajectory from already-finite interpolated state, so
    // neither per-node guard had ever fired outside a test either.
    Access::ControlTrajectory baseline;
    baseline.omegaStep.fill(omega);
    baseline.feedback.fill(feedback);
    baseline.headroom.fill(headroomVolts);
    constexpr std::size_t poisonedPoint = 3;
    constexpr std::array<double, 3> poisonedValues {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    const auto renderWithTrajectory =
        [&](const Access::ControlTrajectory& trajectory) {
            Cascade cascade;
            cascade.reset();
            Access::setState(cascade, seedState);
            const float output = processSelected(
                cascade, input, omega, feedback, headroomVolts, true,
                0.70f, &trajectory, tanhMode, earlyMode, solverMode);
            return std::make_pair(output, Access::state(cascade));
        };

    auto feedbackFallback = baseline;
    feedbackFallback.feedback[poisonedPoint] = 0.0;
    const auto feedbackFallbackResult = renderWithTrajectory(feedbackFallback);
    for (double poisoned : poisonedValues)
    {
        auto poisonedTrajectory = baseline;
        poisonedTrajectory.feedback[poisonedPoint] = poisoned;
        const auto actual = renderWithTrajectory(poisonedTrajectory);
        expect(actual.first == feedbackFallbackResult.first
                   && actual.second == feedbackFallbackResult.second,
               "OtaCascade::process's explicit-trajectory feedback guard no "
               "longer matches its documented 0.0 per-node fallback");
    }

    auto headroomFallback = baseline;
    headroomFallback.headroom[poisonedPoint] = 1.0e-5;
    const auto headroomFallbackResult = renderWithTrajectory(headroomFallback);
    for (double poisoned : poisonedValues)
    {
        auto poisonedTrajectory = baseline;
        poisonedTrajectory.headroom[poisonedPoint] = poisoned;
        const auto actual = renderWithTrajectory(poisonedTrajectory);
        expect(actual.first == headroomFallbackResult.first
                   && actual.second == headroomFallbackResult.second,
               "OtaCascade::process's explicit-trajectory headroom guard no "
               "longer matches its documented 1e-5 per-node fallback");
    }
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
    testZonedHermiteTanhKernelAndDispatch();
    testCubicEarlyTanhKernelAndDispatch();
    testFastReciprocalNormalizationBound();
    testCausalCubicContracts();
    testExactFractionalVcfHoldTrajectory();
    testExplicitControlTrajectoryVisitsEveryMersonNode();
    testMersonTableauAndStabilityPolynomial();
    testIntegratorAgainstIndependentReference();
    testRapidControlAlternationAgainstIndependentReference();
    testSolverLadderTableauContracts();
    testSolverLadderReadsOnlyItsOwnControlNodes();
    testSolverLadderAgainstIndependentReference();
    testSolverLadderPreservesTheSelfOscillationAnchor();
    testSolverRungMayBeSwitchedUnderASoundingNote();
    testMersonRungIsTheUnchangedDefault();
    testProductGridOmegaCapAndThermalContainment();
    testHighGStabilityAndRecovery(
        youknow106::VcfTanhMode::Exact, "exact");
    testHighGStabilityAndRecovery(
        youknow106::VcfTanhMode::ZonedHermite, "zoned Hermite");
    testHighGStabilityAndRecovery(
        youknow106::VcfTanhMode::ZonedHermite, "zoned Hermite/cubic Early",
        youknow106::VcfFastEarlyMode::Cubic);
    // The cheap rungs have to survive the same hostile grid: the product cap
    // is where a single full-interval step would be least admissible, so this
    // is also what proves the escalation actually fires there.
    testHighGStabilityAndRecovery(
        youknow106::VcfTanhMode::Exact, "exact/RK4 x2",
        youknow106::VcfFastEarlyMode::Hermite,
        youknow106::VcfSolverMode::Rk4HalfSteps);
    testHighGStabilityAndRecovery(
        youknow106::VcfTanhMode::Exact, "exact/RK4 x1",
        youknow106::VcfFastEarlyMode::Hermite,
        youknow106::VcfSolverMode::Rk4Single);
    testHighGStabilityAndRecovery(
        youknow106::VcfTanhMode::ZonedHermite, "zoned Hermite/RK4 x1",
        youknow106::VcfFastEarlyMode::Hermite,
        youknow106::VcfSolverMode::Rk4Single);
    testHighGStabilityAndRecovery(
        youknow106::VcfTanhMode::ZonedHermite,
        "zoned Hermite/cubic Early/RK4 x1",
        youknow106::VcfFastEarlyMode::Cubic,
        youknow106::VcfSolverMode::Rk4Single);
    testCalibrationAndTrajectoryNonFiniteGuards(
        youknow106::VcfTanhMode::Exact);
    testCalibrationAndTrajectoryNonFiniteGuards(
        youknow106::VcfTanhMode::ZonedHermite);
    testCalibrationAndTrajectoryNonFiniteGuards(
        youknow106::VcfTanhMode::ZonedHermite,
        youknow106::VcfFastEarlyMode::Cubic);
    testCalibrationAndTrajectoryNonFiniteGuards(
        youknow106::VcfTanhMode::Exact,
        youknow106::VcfFastEarlyMode::Hermite,
        youknow106::VcfSolverMode::Rk4Single);
    testCalibrationAndTrajectoryNonFiniteGuards(
        youknow106::VcfTanhMode::ZonedHermite,
        youknow106::VcfFastEarlyMode::Cubic,
        youknow106::VcfSolverMode::Rk4Single);
    testRetimePreservesPhysicalState();

    if (failures != 0)
    {
        std::cerr << failures << " VCF integrator test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All VCF integrator tests passed\n";
    return EXIT_SUCCESS;
}
