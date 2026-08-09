#pragma once

// Research-only fixed-solve-count candidate for the four-stage OTA cascade.
//
// This deliberately lives outside Source/: it is an admissibility probe, not
// a selectable engine path.  It retains the shipping cascade's discrete
// path-average equations and all of their parameters, but replaces the
// convergence loop with exactly one frozen-modulation quasi-Newton evaluation
// and one rank-one corrected solve, realized as two bidiagonal solves, per
// sample. That bounds its work and lets the circuit suite answer whether this
// particular non-iterative adaptation is accurate enough before any production
// solver is touched. The 2026-08-09 engine-bound/standard-grid scan matrix
// rejects it; the code remains here to keep that negative result reproducible.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace youknow106::vcf_audit
{
struct FixedQuasiNewtonCascade
{
    std::array<float, 4> state {};
    std::array<float, 4> voltage {};
    std::array<float, 4> offsetVoltage {};
    std::array<float, 4> driveMemory {};
    std::array<float, 4> gScale { 1.0f, 1.0f, 1.0f, 1.0f };

    std::size_t samples { 0 };
    std::size_t systemEvaluations { 0 };
    std::size_t bidiagonalSolves { 0 };
    std::size_t recoveries { 0 };

    void reset() noexcept
    {
        state.fill(0.0f);
        voltage.fill(0.0f);
        driveMemory.fill(0.0f);
        samples = 0;
        systemEvaluations = 0;
        bidiagonalSolves = 0;
        recoveries = 0;
    }

    void retime(float previousG, float nextG) noexcept
    {
        (void) previousG;
        (void) nextG;
        state = voltage;
    }

    float process(float input, float g, float feedback,
                  float headroom, float feedbackHeadroom,
                  float earlyEffectCoefficient,
                  bool enableEarlyEffect = true,
                  float calibration = 0.70f) noexcept
    {
        ++samples;
        ++systemEvaluations;
        bidiagonalSolves += 2;

        const float inverseHeadroom = 1.0f / std::max(headroom, 1.0e-5f);
        const float gLimited = std::clamp(g, 0.0f, 64.0f);
        const float k = std::clamp(feedback, 0.0f, 8.0f);

        std::array<float, 4> selfDerivative {};
        std::array<float, 4> previousDerivative {};
        std::array<float, 4> residual {};

        const float feedbackTanh = tanh(voltage[3] / feedbackHeadroom);
        const float feedbackSech2 = 1.0f - feedbackTanh * feedbackTanh;
        float previous = input - k * feedbackHeadroom * feedbackTanh;
        for (std::size_t stage = 0; stage < voltage.size(); ++stage)
        {
            // Match one shipping iteration: the Early-effect multiplier is
            // evaluated at this state, while its voltage derivative is frozen
            // out of the approximate Jacobian.
            const float earlyMod = (enableEarlyEffect && calibration > 0.0f)
                ? (1.0f + earlyEffectCoefficient * calibration
                       * tanh(voltage[stage] * inverseHeadroom))
                : 1.0f;
            const float stageG = gLimited * gScale[stage] * earlyMod;
            const float drive = (previous - voltage[stage]
                                 + offsetVoltage[stage]) * inverseHeadroom;
            float average = 0.0f;
            float slope = 0.0f;
            pathAverage(drive, driveMemory[stage], average, slope);
            residual[stage] = voltage[stage] - state[stage]
                            - 2.0f * stageG * headroom * average;
            selfDerivative[stage] = 1.0f + 2.0f * stageG * slope;
            previousDerivative[stage] = -2.0f * stageG * slope;
            previous = voltage[stage];
        }

        const auto solveBidiagonal = [&](const std::array<float, 4>& rhs) {
            std::array<float, 4> result {};
            result[0] = rhs[0] / selfDerivative[0];
            for (std::size_t stage = 1; stage < result.size(); ++stage)
                result[stage] =
                    (rhs[stage] - previousDerivative[stage] * result[stage - 1])
                    / selfDerivative[stage];
            return result;
        };

        const auto direct = solveBidiagonal(residual);
        std::array<float, 4> corner {};
        corner[0] = previousDerivative[0] * (-k * feedbackSech2);
        const auto correction = solveBidiagonal(corner);
        float denominator = 1.0f + correction[3];
        if (std::abs(denominator) < 1.0e-9f)
            denominator = denominator < 0.0f ? -1.0e-9f : 1.0e-9f;
        const float scale = direct[3] / denominator;

        for (std::size_t stage = 0; stage < voltage.size(); ++stage)
        {
            const float delta = std::clamp(
                direct[stage] - correction[stage] * scale, -32.0f, 32.0f);
            voltage[stage] -= delta;
        }

        const float convergedFeedback = tanh(voltage[3] / feedbackHeadroom);
        previous = input - k * feedbackHeadroom * convergedFeedback;
        for (std::size_t stage = 0; stage < voltage.size(); ++stage)
        {
            driveMemory[stage] = (previous - voltage[stage]
                                  + offsetVoltage[stage]) * inverseHeadroom;
            state[stage] = voltage[stage];
            previous = voltage[stage];
            if (!std::isfinite(state[stage])
                || !std::isfinite(driveMemory[stage]))
            {
                ++recoveries;
                state[stage] = 0.0f;
                voltage[stage] = 0.0f;
                driveMemory[stage] = 0.0f;
            }
        }

        return voltage[3];
    }

private:
    static float tanh(float x) noexcept
    {
        return static_cast<float>(std::tanh(static_cast<double>(x)));
    }

    static double lnCosh(double x) noexcept
    {
        const double magnitude = std::abs(x);
        return magnitude + std::log1p(std::exp(-2.0 * magnitude))
             - 0.69314718055994530942;
    }

    static void pathAverage(float x1, float x0, float& average,
                            float& slope) noexcept
    {
        const float span = x1 - x0;
        if (std::abs(span) < 1.0e-3f)
        {
            const float t = tanh(0.5f * (x1 + x0));
            average = t;
            slope = 0.5f * (1.0f - t * t);
            return;
        }

        const float t = tanh(x1);
        average = static_cast<float>(
            (lnCosh(static_cast<double>(x1))
             - lnCosh(static_cast<double>(x0))) / static_cast<double>(span));
        slope = (t - average) / span;
    }
};
} // namespace youknow106::vcf_audit
