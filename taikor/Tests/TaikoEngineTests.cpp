#include "DSP/TaikoEngine.h"
#include "DSP/UiMath.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace taikor
{
// The engine grants this struct access so the suite can exercise the airborne
// delay line's index arithmetic directly. It is not part of the plug-in API.
struct TaikoEngineTestAccess
{
    struct TuningPathMeasurement
    {
        float radiusMetres { 0.0f };
        float tensionNewtonsPerMetre { 0.0f };
        float loadedFundamentalHz { 0.0f };
        float tuningHz { 0.0f };
    };

    // Exposes the boundary every host-supplied parameter block passes
    // through, so its per-field clamping and NaN handling can be asserted
    // directly rather than only inferred from whether the audio it eventually
    // produces stays finite.
    static EngineParameters sanitise (const EngineParameters& rawParameters) noexcept
    {
        return TaikoEngine::sanitise (rawParameters);
    }

    // Exposes the octave-family transform on its own, so its two documented
    // identity cases - Octave Body 0, and octave 0 at any Octave Body - can be
    // asserted directly rather than only inferred from rendered audio.
    static EngineParameters parametersForOctave (const EngineParameters& applied,
                                                 int octaveOffset) noexcept
    {
        return TaikoEngine::parametersForOctave (applied, octaveOffset);
    }

    // Exposes the reduced bachi/head collision mass on its own, so its
    // fallback to the bachi's own mass alone - taken when the membrane's
    // inverse mass at the strike point comes out zero or non-finite - can be
    // asserted directly rather than only inferred from rendered audio.
    // DrumState and StrikeProfile are private nested types, so the default
    // drum and a profile varying only membraneGain are built here rather than
    // passed in from the test.
    static float contactCollisionMass (float membraneGain, float strikeRadius,
                                       float strikerMass) noexcept
    {
        const TaikoEngine::DrumState drum;
        TaikoEngine::StrikeProfile profile;
        profile.membraneGain = membraneGain;
        return TaikoEngine::contactCollisionMass (drum, profile, strikeRadius,
                                                   strikerMass);
    }

    static float strikeRadius (Articulation articulation,
                               float strikePosition) noexcept
    {
        return TaikoEngine::strikeRadiusFor (
            TaikoEngine::strikeProfile (articulation), strikePosition);
    }

    static TuningPathMeasurement tuningPathMeasurement (
        const EngineParameters& rawParameters, int octave) noexcept
    {
        const auto parameters = TaikoEngine::sanitise (rawParameters);
        const auto drum = TaikoEngine::resolveDrumFor (parameters, 0.0f, octave);
        const auto pair = TaikoEngine::solveAxisymmetricPair (drum);
        const auto identity = TaikoEngine::tuningModeFor (octave,
                                                          parameters.octaveBody);
        const auto tuning = TaikoEngine::observeMode (
            drum, static_cast<int> (identity.entryIndex),
            static_cast<int> (identity.branch), TaikoEngine::tuningStrikeRadius());
        return { drum.radius, drum.tension, pair.loadedFundamentalHz,
                 tuning.frequencyHz };
    }

    static std::array<float, 3> zeroAzimuthSplitProbe() noexcept
    {
        const auto parameters = TaikoEngine::sanitise (EngineParameters {});
        const auto drum = TaikoEngine::resolveDrumFor (parameters, 0.0f, 0);
        constexpr int entryIndex = TaikoEngine::axisymmetricEntryCount;

        const auto observedCosine = TaikoEngine::observeMode (
            drum, entryIndex, 0, TaikoEngine::tuningStrikeRadius(), 0.0f);
        const auto observedSine = TaikoEngine::observeMode (
            drum, entryIndex, 1, TaikoEngine::tuningStrikeRadius(), 0.0f);

        TaikoEngine engine;
        engine.prepare (48000.0, 64);
        TaikoEngine::Voice voice;
        voice.physicalBank = true;
        voice.strikeRadius = TaikoEngine::tuningStrikeRadius();
        voice.strikeAngle = 0.0f;
        engine.buildVoiceModes (voice, drum,
                                TaikoEngine::strikeProfile (Articulation::Don),
                                0.0f, false);

        float renderedCosineHz = 0.0f;
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (mode.membrane && mode.modeEntry == entryIndex
                && mode.physicalIndex == 2 * entryIndex)
            {
                renderedCosineHz = mode.omega / (2.0f * 3.14159265358979f);
                break;
            }
        }

        return {{ observedCosine.frequencyHz, renderedCosineHz,
                  observedSine.frequencyHz }};
    }

    struct AxisymmetricCavityProbe
    {
        std::array<float, TaikoEngine::axisymmetricEntryCount> factors {};
        std::array<float, TaikoEngine::axisymmetricEntryCount> requestedFactors {};
        std::array<std::array<float, 2>, TaikoEngine::axisymmetricEntryCount>
            observedHz {};
        std::array<std::array<float, 2>, TaikoEngine::axisymmetricEntryCount>
            renderedHz {};
        std::array<std::array<float, 2>, TaikoEngine::axisymmetricEntryCount>
            sharedFactorHz {};
    };

    static AxisymmetricCavityProbe axisymmetricCavityProbe (
        const EngineParameters& rawParameters, int octave) noexcept
    {
        constexpr float airSoundSpeed = 343.0f;
        static_assert (TaikoEngine::axisymmetricEntryCount == 4);

        const auto parameters = TaikoEngine::sanitise (rawParameters);
        const auto drum = TaikoEngine::resolveDrumFor (parameters, 0.0f, octave);
        AxisymmetricCavityProbe result;
        result.factors = drum.cavityColumnFactors;

        for (int entryIndex = 0;
             entryIndex < TaikoEngine::axisymmetricEntryCount;
             ++entryIndex)
        {
            const auto& entry = TaikoEngine::membraneModes()[
                static_cast<std::size_t> (entryIndex)];
            const auto slot = static_cast<std::size_t> (entryIndex);
            const float lambda = static_cast<float> (entry.besselZero);
            const auto modeOmegas = TaikoEngine::membraneModeOmegas (
                drum, drum.radius, lambda, 0.0f);

            TaikoEngine::FundamentalPair solveOmegas;
            if (entryIndex == 0)
            {
                solveOmegas = TaikoEngine::fundamentalPairOmegas (drum);
            }
            else
            {
                solveOmegas = { lambda, modeOmegas.batter, modeOmegas.resonant };
            }

            const float volumeOmega = TaikoEngine::volumeBranchOmega (
                drum, solveOmegas, drum.cavityStiffnesses[slot]);
            result.requestedFactors[slot] = TaikoEngine::columnStiffnessFactor (
                volumeOmega * drum.depth / (2.0f * airSoundSpeed));

            float sharedDiagonalB = 0.0f;
            float sharedDiagonalR = 0.0f;
            float sharedOffDiagonal = 0.0f;
            TaikoEngine::axisymmetricDiagonals (
                drum,
                TaikoEngine::FundamentalPair {
                    lambda, modeOmegas.batter, modeOmegas.resonant
                },
                drum.cavityStiffnesses[0], sharedDiagonalB, sharedDiagonalR,
                sharedOffDiagonal);

            for (int branch = 0; branch < 2; ++branch)
            {
                result.observedHz[slot][static_cast<std::size_t> (branch)] =
                    TaikoEngine::observeMode (
                        drum, entryIndex, branch,
                        TaikoEngine::tuningStrikeRadius()).frequencyHz;

                float eigenvalue = 0.0f;
                float vectorB = 0.0f;
                float vectorR = 0.0f;
                TaikoEngine::solveAxisymmetricBranch (
                    sharedDiagonalB, sharedDiagonalR, sharedOffDiagonal, branch,
                    eigenvalue, vectorB, vectorR);
                result.sharedFactorHz[slot][static_cast<std::size_t> (branch)] =
                    std::sqrt (std::max (eigenvalue, 0.0f))
                    / (2.0f * 3.14159265358979f);
            }
        }

        TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, 64);
        engine.reset();
        engine.trigger (Articulation::Don, octave, 0.9f);
        const auto& voice = physicalForOctave (engine, octave);
        for (int modeIndex = 0; modeIndex < voice.modeCount; ++modeIndex)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (modeIndex)];
            if (! mode.membrane || mode.circumferentialOrder != 0
                || mode.modeEntry >= TaikoEngine::axisymmetricEntryCount)
                continue;

            const int branch = static_cast<int> (mode.physicalIndex)
                             - 2 * static_cast<int> (mode.modeEntry);
            if (branch < 0 || branch >= 2)
                continue;

            result.renderedHz[static_cast<std::size_t> (mode.modeEntry)]
                             [static_cast<std::size_t> (branch)] =
                mode.omega / (2.0f * 3.14159265358979f);
        }

        return result;
    }

    static const TaikoEngine::Voice& physicalForOctave (const TaikoEngine& engine,
                                                         int octave = 0) noexcept
    {
        const auto index = static_cast<std::size_t> (
            std::clamp (octave, lowestOctaveOffset, highestOctaveOffset)
            - lowestOctaveOffset);
        return engine.physicalDrums_[index];
    }

    static TaikoEngine::Voice& physicalForOctave (TaikoEngine& engine,
                                                   int octave = 0) noexcept
    {
        const auto index = static_cast<std::size_t> (
            std::clamp (octave, lowestOctaveOffset, highestOctaveOffset)
            - lowestOctaveOffset);
        return engine.physicalDrums_[index];
    }

    static const TaikoEngine::Voice& physicalForSlot (const TaikoEngine& engine,
                                                       int slot = 0) noexcept
    {
        return physicalForOctave (
            engine, engine.voices_[static_cast<std::size_t> (slot)].octaveOffset);
    }

    static TaikoEngine::Voice& physicalForSlot (TaikoEngine& engine,
                                                 int slot = 0) noexcept
    {
        return physicalForOctave (
            engine, engine.voices_[static_cast<std::size_t> (slot)].octaveOffset);
    }

    static int newestStrikeSlot (const TaikoEngine& engine) noexcept
    {
        int newest = 0;
        for (int index = 1; index < TaikoEngine::maxVoices; ++index)
            if (engine.voices_[static_cast<std::size_t> (index)].startOrder
                > engine.voices_[static_cast<std::size_t> (newest)].startOrder)
                newest = index;
        return newest;
    }

    static const TaikoEngine::Voice& latestPhysicalDrum (
        const TaikoEngine& engine) noexcept
    {
        return physicalForSlot (engine, newestStrikeSlot (engine));
    }

    struct CollisionState
    {
        double displacementBefore { 0.0 };
        double displacementAfter { 0.0 };
        double velocityBefore { 0.0 };
        double velocityAfter { 0.0 };
    };

    static CollisionState applyCollision (double displacement, double previous,
                                          float retention, float poleShift = 1.0f,
                                          bool atTurningPoint = false) noexcept
    {
        TaikoEngine engine;
        engine.prepare (48000.0, 64);

        TaikoEngine::Mode mode;
        constexpr float frequency = 137.0f;
        constexpr float decay = 4.5f;
        engine.configureResonator (mode.resonator, frequency * poleShift, decay, 1.0f);
        // Deliberately leave these at the unshifted build values. A ringing
        // voice does exactly that when its live poles move under Tension Mod or
        // the pitch wheel, so collision recovery must read the poles themselves.
        mode.omega = 2.0f * 3.14159265358979f * frequency;
        mode.liveOmega = 2.0 * static_cast<double> (3.14159265358979f)
                       * static_cast<double> (frequency * poleShift);
        mode.poleRadius = std::sqrt (mode.resonator.a2);
        mode.decayRate = decay;
        mode.resonator.y1 = displacement;
        if (atTurningPoint)
        {
            const double radius = std::sqrt (mode.resonator.a2);
            const double cosine = -mode.resonator.a1 / (2.0 * radius);
            const double angle = std::acos (std::clamp (cosine, -1.0, 1.0));
            const double quadrature = -std::log (radius) / angle * displacement;
            mode.resonator.y2 =
                (displacement * cosine - quadrature * std::sin (angle)) / radius;
        }
        else
        {
            mode.resonator.y2 = previous;
        }

        const auto velocity = [] (const TaikoEngine::Mode& value)
        {
            const auto& resonator = value.resonator;
            const double radius = std::sqrt (resonator.a2);
            const double cosine = -resonator.a1 / (2.0 * radius);
            const double angle = std::acos (std::clamp (cosine, -1.0, 1.0));
            const double sine = std::sin (angle);
            const double quadrature =
                (resonator.y1 * cosine - radius * resonator.y2) / sine;
            constexpr double sampleRate = 48000.0;
            const double omega = angle * sampleRate;
            const double decay = -std::log (radius) * sampleRate;
            return omega * quadrature - decay * resonator.y1;
        };

        CollisionState result;
        result.displacementBefore = mode.resonator.y1;
        result.velocityBefore = velocity (mode);
        TaikoEngine::applyCollisionRetention (mode, retention);
        result.displacementAfter = mode.resonator.y1;
        result.velocityAfter = velocity (mode);
        return result;
    }

    // Exposes applyCollisionRetention's own defensive fallback: taken when
    // the pole coordinates it would otherwise recover velocity from have
    // collapsed - a non-positive poleRadius, an essentially-zero
    // resonator.b0, or a non-positive liveOmega - and a plain backward
    // difference between q[n] and q[n-1] is used instead. configureResonator
    // and applyTensionShift keep every ringing mode's poleRadius, b0 and
    // liveOmega strictly positive, so nothing built by trigger() or
    // applyTensionShift ever takes this branch; it exists only for a Mode
    // whose pole has collapsed outright.
    static CollisionState applyDegenerateCollision (double displacement, double previous,
                                                     float retention, double poleRadius,
                                                     double sine, double liveOmega) noexcept
    {
        TaikoEngine::Mode mode;
        mode.poleRadius = poleRadius;
        mode.liveOmega = liveOmega;
        mode.resonator.b0 = sine;
        mode.resonator.y1 = displacement;
        mode.resonator.y2 = previous;

        CollisionState result;
        result.displacementBefore = mode.resonator.y1;
        result.velocityBefore = mode.resonator.y1 - mode.resonator.y2;
        TaikoEngine::applyCollisionRetention (mode, retention);
        result.displacementAfter = mode.resonator.y1;
        result.velocityAfter = result.displacementAfter - mode.resonator.y2;
        return result;
    }

    static double shiftedPoleCacheError() noexcept
    {
        TaikoEngine engine;
        engine.prepare (48000.0, 64);
        engine.trigger (Articulation::Don, 0, 0.9f);
        auto& voice = physicalForOctave (engine);
        engine.applyTensionShift (voice, 1.37f);

        double maximum = 0.0;
        for (int index = 0; index < voice.activeModeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane || ! (mode.poleRadius > 0.0))
                continue;
            const double radius = std::sqrt (mode.resonator.a2);
            const double cosine = -mode.resonator.a1 / (2.0 * radius);
            const double angle = std::acos (std::clamp (cosine, -1.0, 1.0));
            const double omega = angle * engine.sampleRate_;
            maximum = std::max (
                maximum,
                std::abs (static_cast<double> (mode.liveOmega) - omega)
                    / std::max (omega, 1.0));
            maximum = std::max (
                maximum,
                std::abs (mode.poleRadius - radius) / std::max (radius, 1.0e-12));
            const double sine = mode.resonator.b0;
            if (std::abs (sine) > 1.0e-12)
            {
                const double current = cosine / sine;
                const double previous = -radius / sine;
                maximum = std::max (
                    maximum,
                    std::abs (mode.quadratureFromCurrent - current)
                        / std::max (std::abs (current), 1.0));
                maximum = std::max (
                    maximum,
                    std::abs (mode.quadratureFromPrevious - previous)
                        / std::max (std::abs (previous), 1.0));
            }
        }
        return maximum;
    }

    static double disabledQuadratureScale() noexcept
    {
        TaikoEngine engine;
        engine.prepare (8000.0, 64);
        engine.trigger (Articulation::Don, 3, 0.9f);
        auto& voice = physicalForOctave (engine, 3);
        engine.applyTensionShift (voice, 1.0e6f);

        double maximum = 0.0;
        for (int index = 0; index < voice.activeModeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane)
                continue;
            maximum = std::max (
                maximum,
                std::max (std::abs (mode.quadratureFromCurrent),
                          std::abs (mode.quadratureFromPrevious)));
        }
        return maximum;
    }

    static std::array<float, 2> baffledObservation (
        float radius, float micRadius, float distance, int order,
        float lambda, float omega, int minimumNodes = 0) noexcept
    {
        const auto value = TaikoEngine::baffledModeObservation (
            radius, micRadius, distance, order, lambda, omega, minimumNodes);
        return {{ value.real, value.quadrature }};
    }

    struct ComplexBankProbe
    {
        int nonAxisymmetricModes { 0 };
        float maximumReal { 0.0f };
        float maximumQuadrature { 0.0f };
    };

    static ComplexBankProbe complexBankProbe (bool complexObservation) noexcept
    {
        const auto parameters = TaikoEngine::sanitise (EngineParameters {});
        const auto drum = TaikoEngine::resolveDrumFor (parameters, 0.0f, 0);
        TaikoEngine engine;
        engine.prepare (48000.0, 64);
        TaikoEngine::Voice voice;
        voice.physicalBank = true;
        voice.strikeRadius = TaikoEngine::strikeProfile (Articulation::Don).radius;
        voice.strikeAngle = 0.0f;
        engine.buildVoiceModes (voice, drum,
                                TaikoEngine::strikeProfile (Articulation::Don),
                                0.0f, complexObservation);

        ComplexBankProbe result;
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane || mode.circumferentialOrder == 0)
                continue;
            ++result.nonAxisymmetricModes;
            result.maximumReal = std::max (
                result.maximumReal,
                std::max (std::abs (mode.micLeft), std::abs (mode.micRight)));
            result.maximumQuadrature = std::max (
                result.maximumQuadrature,
                std::max (std::abs (mode.micLeftQuadrature),
                          std::abs (mode.micRightQuadrature)));
        }
        return result;
    }

    static float releasedBankMaximumQuadrature() noexcept
    {
        TaikoEngine engine;
        engine.prepare (48000.0, 64);
        engine.trigger (Articulation::Don, 0, 0.9f);
        const auto& voice = physicalForOctave (engine);
        float maximum = 0.0f;
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            maximum = std::max (
                maximum,
                std::max (std::abs (mode.micLeftQuadrature),
                          std::abs (mode.micRightQuadrature)));
        }
        return maximum;
    }

    struct MultipoleNodeProbe
    {
        int modes { 0 };
        int analyticModes { 0 };
        float maximumLeakage { 0.0f };
        float maximumAnalyticLeakage { 0.0f };
        float maximumRotatedAnalyticLeakage { 0.0f };
    };

    // Put one capsule on a nodal azimuth and the other on a lobe for every
    // non-axisymmetric order in the released scalar observer. Circular
    // covariance makes the cosine member null on the left and the sine member
    // null on the right; a direction-independent far-field term breaks both.
    static MultipoleNodeProbe releasedMultipoleNodeProbe() noexcept
    {
        const auto parameters = TaikoEngine::sanitise (EngineParameters {});
        const auto baseDrum = TaikoEngine::resolveDrumFor (parameters, 0.0f, 0);
        TaikoEngine engine;
        engine.prepare (48000.0, 64);

        MultipoleNodeProbe result;
        for (int order = 1; order <= 8; ++order)
        {
            auto drum = baseDrum;
            drum.micAngleLeft = static_cast<float> (
                3.14159265358979 / (2.0 * static_cast<double> (order)));
            drum.micAngleRight = 0.0f;

            TaikoEngine::Voice voice;
            voice.physicalBank = true;
            voice.strikeRadius = TaikoEngine::strikeProfile (Articulation::Don).radius;
            voice.strikeAngle = 0.0f;
            engine.buildVoiceModes (voice, drum,
                                    TaikoEngine::strikeProfile (Articulation::Don),
                                    0.0f, false);

            for (int index = 0; index < voice.modeCount; ++index)
            {
                const auto& mode = voice.modes[static_cast<std::size_t> (index)];
                if (! mode.membrane || mode.circumferentialOrder != order)
                    continue;

                const int branch = static_cast<int> (mode.physicalIndex)
                                 - 2 * static_cast<int> (mode.modeEntry);
                const float node = branch == 0 ? mode.micLeft : mode.micRight;
                const float lobe = branch == 0 ? mode.micRight : mode.micLeft;
                result.maximumLeakage = std::max (
                    result.maximumLeakage,
                    std::abs (node) / std::max (std::abs (lobe), 1.0e-20f));
                ++result.modes;
            }

            drum.stereoWidth = 0.5f; // the left output is exactly the left capsule
            auto lobeDrum = drum;
            lobeDrum.micAngleLeft = 0.0f;
            const auto& entries = TaikoEngine::membraneModes();
            for (int entryIndex = 0; entryIndex < TaikoEngine::modeEntryCount;
                 ++entryIndex)
            {
                if (entries[static_cast<std::size_t> (entryIndex)]
                        .circumferentialOrder != order)
                    continue;
                const auto node = TaikoEngine::observeMode (
                    drum, entryIndex, 0, 0.72f);
                const auto lobe = TaikoEngine::observeMode (
                    lobeDrum, entryIndex, 0, 0.72f);
                result.maximumAnalyticLeakage = std::max (
                    result.maximumAnalyticLeakage,
                    node.amplitude / std::max (lobe.amplitude, 1.0e-30f));

                // Hold the microphone on its lobe and rotate the strike onto
                // that same nodal separation. This is the addition identity
                // the authored azimuth path relies on; a readout which still
                // assumes theta_s=0 leaves this ratio at one.
                const auto rotatedNode = TaikoEngine::observeMode (
                    lobeDrum, entryIndex, 0, 0.72f,
                    static_cast<float> (
                        -3.14159265358979 / (2.0 * static_cast<double> (order))));
                result.maximumRotatedAnalyticLeakage = std::max (
                    result.maximumRotatedAnalyticLeakage,
                    rotatedNode.amplitude / std::max (lobe.amplitude, 1.0e-30f));
                ++result.analyticModes;
            }
        }
        return result;
    }

    static double complexResidueRenderError() noexcept
    {
        TaikoEngine engine;
        engine.prepare (48000.0, 64);

        TaikoEngine::Voice voice;
        voice.physicalBank = true;
        voice.active = true;
        voice.modeCount = 1;
        voice.activeModeCount = 1;
        voice.maximumSamples = 48000;

        auto& mode = voice.modes[0];
        constexpr float frequency = 731.0f;
        constexpr float decay = 7.25f;
        constexpr double phase = 0.83;
        constexpr double delayPhase = 0.47;
        engine.configureResonator (mode.resonator, frequency, decay, 1.0f,
                                   &mode.poleRadius);
        mode.liveOmega = 2.0 * static_cast<double> (3.14159265358979f)
                       * frequency;
        mode.membrane = true;
        mode.physicalIndex = 0;
        mode.micLeft = mode.micRight = static_cast<float> (std::cos (delayPhase));
        mode.micLeftQuadrature = mode.micRightQuadrature =
            static_cast<float> (-std::sin (delayPhase));
        TaikoEngine::updateQuadratureScales (mode);

        const double angle = mode.liveOmega / 48000.0;
        const double radius = mode.poleRadius;
        mode.resonator.y1 = std::cos (phase - angle) / radius;
        mode.resonator.y2 = std::cos (phase - 2.0 * angle) / (radius * radius);

        float right = 0.0f;
        const float left = engine.renderVoice (voice, nullptr, right);
        const double expected = std::cos (phase - delayPhase);
        return std::max (std::abs (static_cast<double> (left) - expected),
                         std::abs (static_cast<double> (right) - expected));
    }

    static std::array<float, 5> palmRadii (float centre, float radius) noexcept
    {
        return TaikoEngine::palmPatchRadii (centre, radius);
    }

    static float readDelayLine (const std::array<float, TaikoEngine::directLineSize>& line,
                                int writeIndex, float delaySamples) noexcept
    {
        return TaikoEngine::readDelayLine (line, writeIndex, delaySamples);
    }

    static constexpr int lineSize = TaikoEngine::directLineSize;
    static constexpr int controlInterval = TaikoEngine::controlPeriod;

    static int activeModeCount (const TaikoEngine& engine) noexcept
    {
        return physicalForOctave (engine).activeModeCount;
    }

    // The two airborne path delays, in samples, as the strike geometry
    // resolved them. Read directly because the membrane modes reach both
    // microphones instantaneously in this model, so an onset measured from the
    // audio is dominated by them and says nothing about the airborne path.
    static void directDelays (const TaikoEngine& engine, float& left,
                              float& right) noexcept
    {
        left = engine.voices_[0].directDelayLeft;
        right = engine.voices_[0].directDelayRight;
    }

    static constexpr float delayCeiling = TaikoEngine::directLineSize - 4;

    // The sample the last scheduled contact of a stroke finishes on.
    static std::uint32_t lastContactEnd (const TaikoEngine& engine) noexcept
    {
        const auto& voice = engine.voices_[0];
        if (voice.contactCount <= 0)
            return 0u;
        const auto& last =
            voice.contacts[static_cast<std::size_t> (voice.contactCount - 1)];
        return last.startSample + last.lengthSamples;
    }

    // The wooden half of a voice's bank: the drum's shell. Read from the built
    // voice because these checks ask which poles and projections were used,
    // independently of the head and hoop transient in the finished audio.
    static std::vector<float> woodFrequencies (const TaikoEngine& engine,
                                               int slot = 0)
    {
        std::vector<float> result;
        const auto& voice = physicalForSlot (engine, slot);
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane)
                result.push_back (mode.omega / (2.0f * 3.14159265358979f));
        }
        return result;
    }

    // The resonator poles of one half of a voice's bank, as the render loop
    // will actually run them. This is where a retune shows up: the glide leaves
    // mode.omega alone and carries its shift in the coefficients, so a test that
    // asks whether something was retuned has to read the coefficients.
    static std::vector<double> poles (const TaikoEngine& engine, bool membrane,
                                      int slot = 0)
    {
        std::vector<double> result;
        const auto& voice = physicalForSlot (engine, slot);
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (mode.membrane == membrane)
                result.push_back (mode.resonator.a1);
        }
        return result;
    }

    // Total drive of the wooden bank, which is the quantity Shell Resonance
    // sets and therefore the one a step in that control would appear in.
    static double woodDrive (const TaikoEngine& engine, int slot = 0) noexcept
    {
        double total = 0.0;
        const auto& voice = engine.voices_[static_cast<std::size_t> (slot)];
        for (int index = TaikoEngine::membraneResonatorCount;
             index < TaikoEngine::resonatorCount; ++index)
            total += std::abs (static_cast<double> (
                voice.modeProjection[static_cast<std::size_t> (index)]));
        return total;
    }

    // Which voice each of a run of strokes landed in, and how loud each slot
    // ended up. Voice stealing is a decision about slots, so a test of it has
    // to be able to see the slots.
    static int voiceSlotOf (const TaikoEngine& engine, std::uint64_t startOrder) noexcept
    {
        for (int index = 0; index < TaikoEngine::maxVoices; ++index)
            if (engine.voices_[static_cast<std::size_t> (index)].startOrder == startOrder)
                return index;
        return -1;
    }

    static void silenceNewestVoice (TaikoEngine& engine) noexcept
    {
        const auto newest = static_cast<std::size_t> (newestStrikeSlot (engine));
        engine.silenceVoice (engine.voices_[newest]);
        engine.updateActiveVoiceCount();
    }

    struct LocalMuteState
    {
        int ticks { 0 };
        float minimumGain { 1.0f };
        float maximumGain { 1.0f };
        float fundamentalGain { 1.0f };
    };

    static LocalMuteState oldestActiveMute (const TaikoEngine& engine) noexcept
    {
        const auto* oldest = &physicalForOctave (engine);

        LocalMuteState result;
        if (! oldest->active)
            return result;
        const float secondsPerTick = static_cast<float> (TaikoEngine::controlPeriod)
                                   / static_cast<float> (engine.sampleRate_);
        result.ticks = oldest->localMuteTicksRemaining;
        result.minimumGain = 1.0f;
        result.maximumGain = 0.0f;
        for (int index = 0; index < oldest->activeModeCount; ++index)
        {
            const auto& mode = oldest->modes[static_cast<std::size_t> (index)];
            if (! mode.membrane)
                continue;
            const float gain = std::exp (-mode.localMuteDampingRate * secondsPerTick);
            result.minimumGain = std::min (result.minimumGain,
                                           gain);
            result.maximumGain = std::max (result.maximumGain,
                                           gain);
            if (mode.modeEntry == 0)
                result.fundamentalGain = gain;
        }
        return result;
    }

    static void cancelOldestMute (TaikoEngine& engine) noexcept
    {
        auto* oldest = &physicalForOctave (engine);
        if (! oldest->active)
            return;

        oldest->localMuteTicksRemaining = 0;
        oldest->continuumMuteDampingRate = 0.0f;
        for (int index = 0; index < oldest->modeCount; ++index)
            oldest->modes[static_cast<std::size_t> (index)].localMuteDampingRate = 0.0f;
    }

    struct MuteTickState
    {
        int membraneModes { 0 };
        double displacementError { 0.0 };
        double velocityError { 0.0 };
        double kineticError { 0.0 };
        double decayError { 0.0 };
        double shellStateError { 0.0 };
    };

    static MuteTickState probeMuteTick() noexcept
    {
        EngineParameters parameters;
        parameters.humanise = 0.0f;
        parameters.tensionModulation = 0.0f;

        TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, 64);
        engine.trigger (Articulation::Don, 0, 0.92f);
        for (int sample = 0; sample < 4800; ++sample)
        {
            float left = 0.0f;
            float right = 0.0f;
            engine.process (&left, &right, 1);
        }
        engine.trigger (Articulation::Tsu, 0, 0.82f);
        silenceNewestVoice (engine);

        auto* oldest = &physicalForOctave (engine);
        MuteTickState result;
        if (! oldest->active)
            return result;

        struct Before
        {
            double y1 { 0.0 };
            double y2 { 0.0 };
            double velocity { 0.0 };
            float localRate { 0.0f };
            bool membrane { false };
        };
        std::vector<Before> before (static_cast<std::size_t> (oldest->activeModeCount));

        const auto velocityOf = [] (const TaikoEngine::Mode& mode)
        {
            const auto& resonator = mode.resonator;
            if (! (mode.poleRadius > 0.0) || ! (mode.liveOmega > 0.0)
                || std::abs (resonator.b0) < 1.0e-12)
                return 0.0;
            const double cosine = -resonator.a1 / (2.0 * mode.poleRadius);
            const double quadrature =
                (resonator.y1 * cosine - mode.poleRadius * resonator.y2)
                / resonator.b0;
            return mode.liveOmega * quadrature
                 - static_cast<double> (
                       mode.decayRate + mode.appliedPalmDecay) * resonator.y1;
        };

        for (int index = 0; index < oldest->activeModeCount; ++index)
        {
            const auto& mode = oldest->modes[static_cast<std::size_t> (index)];
            auto& state = before[static_cast<std::size_t> (index)];
            state.y1 = mode.resonator.y1;
            state.y2 = mode.resonator.y2;
            state.velocity = velocityOf (mode);
            state.localRate = mode.localMuteDampingRate;
            state.membrane = mode.membrane;
        }

        engine.updateVoiceControl (*oldest);

        for (int index = 0; index < oldest->activeModeCount; ++index)
        {
            const auto& mode = oldest->modes[static_cast<std::size_t> (index)];
            const auto& state = before[static_cast<std::size_t> (index)];
            if (! state.membrane)
            {
                result.shellStateError = std::max (
                    result.shellStateError,
                    std::max (std::abs (mode.resonator.y1 - state.y1),
                              std::abs (mode.resonator.y2 - state.y2)));
                continue;
            }

            ++result.membraneModes;
            result.displacementError = std::max (
                result.displacementError,
                std::abs (mode.resonator.y1 - state.y1));
            const double velocity = velocityOf (mode);
            result.velocityError = std::max (
                result.velocityError,
                std::abs (velocity - state.velocity)
                    / std::max (std::abs (state.velocity), 1.0));
            result.kineticError = std::max (
                result.kineticError,
                std::abs (velocity * velocity - state.velocity * state.velocity)
                    / std::max (state.velocity * state.velocity, 1.0));
            const double recoveredDecay =
                -48000.0 * std::log (std::max (mode.poleRadius, 1.0e-30));
            const double expectedDecay = mode.decayRate + 0.5 * state.localRate;
            result.decayError = std::max (
                result.decayError,
                std::abs (recoveredDecay - expectedDecay)
                    / std::max (std::abs (expectedDecay), 1.0));
        }
        return result;
    }

    struct HandTickState
    {
        int membraneModes { 0 };
        double displacementError { 0.0 };
        double velocityError { 0.0 };
        double kineticIncrease { 0.0 };
        double shellStateError { 0.0 };
        double continuumError { 0.0 };
        double poleDecayError { 0.0 };
        double quadratureScaleError { 0.0 };
        double releaseError { 0.0 };
        double axisBranchScalingError { 0.0 };
        int axisBranchPairs { 0 };
        int modesAboveControlNyquist { 0 };
        float minimumRate { std::numeric_limits<float>::max() };
        float maximumRate { 0.0f };
        float fundamentalRate { 0.0f };
        float continuumRate { 0.0f };
    };

    static HandTickState probeHandTick (double sampleRate, int octave = 3) noexcept
    {
        EngineParameters parameters;
        parameters.humanise = 0.0f;
        parameters.tensionModulation = 0.0f;
        parameters.strikeNoise = 0.0f;

        TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (sampleRate, 64);
        engine.trigger (Articulation::Don, octave, 0.92f);
        engine.handDamping_ = 1.0f;
        engine.handDampingTarget_ = 1.0f;

        auto& physical = physicalForOctave (engine, octave);
        HandTickState result;
        result.continuumRate = physical.continuumHandDampingRate;

        struct Before
        {
            double y1 { 0.0 };
            double y2 { 0.0 };
            double a1 { 0.0 };
            double a2 { 0.0 };
            double velocity { 0.0 };
            float rate { 0.0f };
            bool membrane { false };
        };
        std::vector<Before> before (static_cast<std::size_t> (physical.activeModeCount));
        std::array<double, TaikoEngine::modeEntryCount> axisBaseRate {};
        std::array<bool, TaikoEngine::modeEntryCount> axisBaseSeen {};
        const auto& drum = engine.drumCache_[static_cast<std::size_t> (
            octave - lowestOctaveOffset)];

        const auto velocityOf = [] (const TaikoEngine::Mode& mode)
        {
            const auto& resonator = mode.resonator;
            if (! (mode.poleRadius > 0.0) || ! (mode.liveOmega > 0.0)
                || std::abs (resonator.b0) < 1.0e-12)
                return 0.0;
            const double cosine = -resonator.a1 / (2.0 * mode.poleRadius);
            const double quadrature =
                (resonator.y1 * cosine - mode.poleRadius * resonator.y2)
                / resonator.b0;
            return mode.liveOmega * quadrature
                 - static_cast<double> (
                       mode.decayRate + mode.appliedPalmDecay) * resonator.y1;
        };

        for (int index = 0; index < physical.activeModeCount; ++index)
        {
            auto& mode = physical.modes[static_cast<std::size_t> (index)];
            const double state = 1.0e-5 * static_cast<double> (index + 1);
            mode.resonator.y1 = state;
            mode.resonator.y2 = -0.37 * state;

            auto& captured = before[static_cast<std::size_t> (index)];
            captured.y1 = mode.resonator.y1;
            captured.y2 = mode.resonator.y2;
            captured.a1 = mode.resonator.a1;
            captured.a2 = mode.resonator.a2;
            captured.velocity = velocityOf (mode);
            captured.rate = mode.handDampingRate;
            captured.membrane = mode.membrane;
            if (! mode.membrane)
                continue;

            ++result.membraneModes;
            const double batterFraction = TaikoEngine::batterFractionFor (drum, mode);
            // Once a higher radial mode passes the column's quarter-wave its
            // two heads decouple, leaving one branch entirely on the far head.
            // A palm on the batter head must not damp that silent branch, so
            // only modes with a physical batter share belong in the spatial
            // spread measured below.
            if (batterFraction > 1.0e-5)
            {
                result.minimumRate = std::min (result.minimumRate,
                                               mode.handDampingRate);
                result.maximumRate = std::max (result.maximumRate,
                                               mode.handDampingRate);
            }
            if (mode.modeEntry == 0)
                result.fundamentalRate = mode.handDampingRate;
            if (mode.liveOmega / (2.0 * 3.14159265358979)
                    > sampleRate / (2.0 * TaikoEngine::controlPeriod))
                ++result.modesAboveControlNyquist;
            if (mode.circumferentialOrder == 0)
            {
                if (batterFraction <= 1.0e-5)
                    continue;
                const auto entry = static_cast<std::size_t> (mode.modeEntry);
                const double baseRate = mode.handDampingRate / batterFraction;
                if (axisBaseSeen[entry])
                {
                    ++result.axisBranchPairs;
                    result.axisBranchScalingError = std::max (
                        result.axisBranchScalingError,
                        std::abs (baseRate - axisBaseRate[entry])
                            / std::max ({ std::abs (baseRate),
                                         std::abs (axisBaseRate[entry]), 1.0 }));
                }
                else
                {
                    axisBaseRate[entry] = baseRate;
                    axisBaseSeen[entry] = true;
                }
            }
        }

        for (auto& band : physical.continuum)
            band.envelope = band.centre > 0.0f ? 1.0f : 0.0f;

        engine.updateVoiceControl (physical);

        const float secondsPerTick = static_cast<float> (TaikoEngine::controlPeriod)
                                   / static_cast<float> (sampleRate);
        for (int index = 0; index < physical.activeModeCount; ++index)
        {
            const auto& mode = physical.modes[static_cast<std::size_t> (index)];
            const auto& captured = before[static_cast<std::size_t> (index)];
            if (! captured.membrane)
            {
                result.shellStateError = std::max (
                    result.shellStateError,
                    std::max ({ std::abs (mode.resonator.y1 - captured.y1),
                                std::abs (mode.resonator.y2 - captured.y2),
                                std::abs (mode.resonator.a1 - captured.a1),
                                std::abs (mode.resonator.a2 - captured.a2) }));
                continue;
            }

            result.displacementError = std::max (
                result.displacementError,
                std::abs (mode.resonator.y1 - captured.y1));
            const double velocity = velocityOf (mode);
            result.velocityError = std::max (
                result.velocityError,
                std::abs (velocity - captured.velocity)
                    / std::max (std::abs (captured.velocity), 1.0));
            result.kineticIncrease = std::max (
                result.kineticIncrease,
                (velocity * velocity - captured.velocity * captured.velocity)
                    / std::max (captured.velocity * captured.velocity, 1.0));
            const double recoveredExtra =
                -sampleRate * std::log (std::max (mode.poleRadius, 1.0e-30))
                - mode.decayRate;
            const double expectedExtra = 0.5 * captured.rate;
            if (expectedExtra > 1.0e-3)
                result.poleDecayError = std::max (
                    result.poleDecayError,
                    std::abs (recoveredExtra - expectedExtra)
                        / std::max (expectedExtra, 1.0));
            const double sine = mode.resonator.b0;
            if (mode.poleRadius > 0.0 && std::abs (sine) > 1.0e-12)
            {
                const double cosine =
                    -mode.resonator.a1 / (2.0 * mode.poleRadius);
                const double expectedCurrent = cosine / sine;
                const double expectedPrevious = -mode.poleRadius / sine;
                result.quadratureScaleError = std::max (
                    result.quadratureScaleError,
                    std::abs (mode.quadratureFromCurrent - expectedCurrent)
                        / std::max (std::abs (expectedCurrent), 1.0));
                result.quadratureScaleError = std::max (
                    result.quadratureScaleError,
                    std::abs (mode.quadratureFromPrevious - expectedPrevious)
                        / std::max (std::abs (expectedPrevious), 1.0));
            }
        }

        const float expectedContinuum = std::exp (
            -0.5f * result.continuumRate * secondsPerTick);
        for (const auto& band : physical.continuum)
            if (band.centre > 0.0f)
                result.continuumError = std::max (
                    result.continuumError,
                    std::abs (static_cast<double> (band.envelope)
                              - expectedContinuum));

        struct ReleaseState
        {
            double displacement { 0.0 };
            double velocity { 0.0 };
        };
        std::vector<ReleaseState> release (
            static_cast<std::size_t> (physical.activeModeCount));
        for (int index = 0; index < physical.activeModeCount; ++index)
        {
            const auto& mode = physical.modes[static_cast<std::size_t> (index)];
            release[static_cast<std::size_t> (index)] = {
                mode.resonator.y1, velocityOf (mode)
            };
        }
        engine.handDamping_ = 0.0f;
        engine.handDampingTarget_ = 0.0f;
        engine.updateVoiceControl (physical);
        for (int index = 0; index < physical.activeModeCount; ++index)
        {
            const auto& mode = physical.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane)
                continue;
            const auto& state = release[static_cast<std::size_t> (index)];
            const double recovered =
                -sampleRate * std::log (std::max (mode.poleRadius, 1.0e-30));
            result.releaseError = std::max (
                result.releaseError,
                std::max ({ std::abs (mode.resonator.y1 - state.displacement),
                            std::abs (velocityOf (mode) - state.velocity)
                                / std::max (std::abs (state.velocity), 1.0),
                            std::abs (recovered - mode.decayRate)
                                / std::max (std::abs (
                                                static_cast<double> (mode.decayRate)),
                                            1.0) }));
        }

        return result;
    }

    // The whole modal bank of the most recently struck voice. Slot zero is not
    // it once anything else is still ringing.
    static std::vector<std::array<float, 11>> newestVoiceBank (const TaikoEngine& engine)
    {
        std::size_t newest = 0;
        for (std::size_t index = 1; index < engine.voices_.size(); ++index)
            if (engine.voices_[index].startOrder > engine.voices_[newest].startOrder)
                newest = index;

        std::vector<std::array<float, 11>> bank;
        const auto& strike = engine.voices_[newest];
        const auto& voice = physicalForSlot (engine, static_cast<int> (newest));
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            // A shared bank is still carrying the first stroke's nonlinear
            // tension when the second arrives. Cache identity is a question
            // about the new control's rest configuration, not about erasing
            // that physical history, so compare the decay reconstructed at
            // the mode's rest frequency rather than the live bent rate.
            const float restDecay = mode.membrane
                ? TaikoEngine::membraneDecayAt (voice, mode, mode.omega)
                : mode.decayRate;
            bank.push_back ({
                mode.omega, restDecay,
                strike.modeProjection[static_cast<std::size_t> (mode.physicalIndex)],
                strike.contactProjection[static_cast<std::size_t> (mode.physicalIndex)],
                mode.inverseModalMass, mode.batterParticipation,
                mode.resonantParticipation,
                mode.micLeft, mode.micRight,
                mode.micLeftQuadrature, mode.micRightQuadrature });
        }
        return bank;
    }

    // T60 of each membrane mode, ordered by frequency, for the voice just
    // struck. The spread between the lowest two is what decides whether a
    // stroke ends as a drum or as a sine.
    static std::vector<float> membraneT60s (const TaikoEngine& engine)
    {
        std::vector<std::pair<float, float>> byFrequency;
        const auto& voice = latestPhysicalDrum (engine);
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (mode.membrane && mode.decayRate > 0.0f)
                byFrequency.push_back ({ mode.omega, 6.9078f / mode.decayRate });
        }
        std::sort (byFrequency.begin(), byFrequency.end());

        std::vector<float> result;
        result.reserve (byFrequency.size());
        for (const auto& entry : byFrequency)
            result.push_back (entry.second);
        return result;
    }

    // Every membrane mode of the voice just struck, in hertz and ascending.
    // Read from the built bank rather than measured from the audio because the
    // question these answer - where does the head put its modes relative to one
    // another - is about a ratio between two partials that are forty decibels
    // apart in level.
    static std::vector<float> membraneFrequencies (const TaikoEngine& engine)
    {
        std::vector<float> result;
        const auto& voice = latestPhysicalDrum (engine);
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (mode.membrane)
                result.push_back (mode.omega / (2.0f * 3.14159265358979f));
        }
        std::sort (result.begin(), result.end());
        return result;
    }

    // The continuum's band-pass corners, which is where a retune of the head
    // shows up: the region is too broad and too short-lived to see it in the
    // summed spectrum.
    static std::vector<float> continuumCoefficients (const TaikoEngine& engine)
    {
        std::vector<float> result;
        const auto& voice = physicalForOctave (engine);
        for (const auto& band : voice.continuum)
            if (band.centre > 0.0f)
                result.push_back (band.highCoefficient);
        return result;
    }

    struct ContinuumBandInfo
    {
        float centre { 0.0f };
        float lowCoefficient { 0.0f };
        float highCoefficient { 0.0f };
        float targetRms { 0.0f };
        float distanceGain { 1.0f };
        float level { 0.0f };
        // The exact value bandPassVariance returned for this band's own
        // coefficients when buildVoiceModes populated continuumVarianceCache_ -
        // read back from the cache itself rather than recovered from level,
        // since level is scaled again by the contact's excitation and duration
        // after buildVoiceModes sets it (see the corner/excitationScale loop in
        // trigger()) and no longer equals targetRms / sqrt(variance) alone.
        float cachedVariance { 1.0f };
        float contactReference { 0.0f };
    };

    struct ContinuumDistanceGeometry
    {
        float waveSpeed { 0.0f };
        float micDistanceMetres { 0.0f };
    };

    static ContinuumDistanceGeometry continuumDistanceGeometry (
        const EngineParameters& rawParameters, int octave = 0) noexcept
    {
        const auto parameters = TaikoEngine::sanitise (rawParameters);
        const auto drum = TaikoEngine::resolveDrumFor (parameters, 0.0f, octave);
        return { drum.waveSpeed, drum.micDistanceMetres };
    }

    static std::vector<ContinuumBandInfo> continuumBands (const TaikoEngine& engine)
    {
        std::vector<ContinuumBandInfo> result;
        const auto& voice = engine.voices_[0];
        const auto drumIndex = static_cast<std::size_t> (
            std::clamp (voice.octaveOffset, taikor::lowestOctaveOffset,
                        taikor::highestOctaveOffset)
            - taikor::lowestOctaveOffset);
        for (std::size_t index = 0; index < voice.continuum.size(); ++index)
        {
            const auto& band = voice.continuum[index];
            if (band.level > 0.0f)
                result.push_back ({ band.centre, band.lowCoefficient,
                                    band.highCoefficient, band.targetRms,
                                    band.distanceGain, band.level,
                                    engine.continuumVarianceCache_[drumIndex][index]
                                        .variance,
                                    voice.contactAmplitude });
        }
        return result;
    }

    static std::vector<float> stationaryContinuumBand (
        const ContinuumBandInfo& band, std::uint32_t seed, int samples)
    {
        std::array<float, 9> state {};
        const float gain = std::sqrt (3.0f / TaikoEngine::bandPassVariance (
            band.lowCoefficient, band.highCoefficient, 2, 7));
        std::vector<float> result (static_cast<std::size_t> (samples));
        // Discard 85 ms of startup at the reference 48 kHz clock. Keep unity
        // envelope and identical stationary
        // white input across sources; the exact variance gives each filter
        // unit total output mean square instead of imposing a drum voicing.
        constexpr int settlingSamples = 4096;
        for (int sample = -settlingSamples; sample < samples; ++sample)
        {
            const float output = TaikoEngine::continuumEdgeCascade (
                state[0], state[1], state[2], state[3], state[4], state[5],
                state[6], state[7], state[8], TaikoEngine::nextNoise (seed),
                band.lowCoefficient, band.highCoefficient) * gain;
            if (sample >= 0)
                result[static_cast<std::size_t> (sample)] = output;
        }
        return result;
    }

    static float hertzContactSpectrum (float omegaTau) noexcept
    {
        return TaikoEngine::contactSpectrum (omegaTau);
    }

    struct ContinuumForceProbe
    {
        std::array<double, TaikoEngine::continuumForceNodeCount> frequencies {};
        std::array<std::complex<double>, TaikoEngine::continuumForceNodeCount> responses {};
        double decay { 1.0 };
        double power { 0.0 };
        double largestRemoval { 0.0 };
        double aggregationError { 0.0 };
        bool resetPreservesTail { false };
    };

    static ContinuumForceProbe continuumForceProbe (
        double rate, double centre, double sigma, const std::vector<double>& forces,
        const std::vector<double>& otherForces = {}, int scaleSample = -1,
        float scale = 1.0f)
    {
        TaikoEngine engine;
        auto& physical = engine.physicalDrums_[0];
        physical.physicalBank = true;
        physical.octaveOffset = 0;
        auto& band = physical.continuum[0];
        band.centre = static_cast<float> (centre);
        band.envelopeDecay = static_cast<float> (std::exp (-sigma / rate));
        engine.configureContinuumForce (band, rate);
        auto& first = engine.voices_[0];
        auto& second = engine.voices_[1];
        for (auto* contact : { &first, &second })
        {
            contact->active = true;
            contact->physicalDrumIndex = 0;
            contact->nonlinearContactActive = true;
            contact->continuumInjection[0] = 1.0f;
        }
        ContinuumForceProbe result;
        // Recover normalized Gauss weights independently from the frequency
        // nodes via P_N'(x), rather than copying the production coefficient table.
        std::array<double, TaikoEngine::continuumForceNodeCount> weights {};
        const double low = 1.0 / static_cast<double> (1.35f);
        const double high = std::min (static_cast<double> (1.35f), 0.45 * rate / centre);
        for (std::size_t node = 0; node < weights.size(); ++node)
        {
            const double x = 2.0 * std::log (band.forceFrequencyRatios[node] / low)
                           / std::log (high / low) - 1.0;
            double previous = 1.0, current = x;
            for (int order = 2; order <= static_cast<int> (weights.size()); ++order)
            {
                const double next = ((2.0 * order - 1.0) * x * current
                                     - (order - 1.0) * previous) / order;
                previous = current;
                current = next;
            }
            const double derivative = weights.size() * (x * current - previous)
                                    / (x * x - 1.0);
            weights[node] = 1.0 / ((1.0 - x * x) * derivative * derivative);
        }
        for (std::size_t sample = 0; sample < forces.size(); ++sample)
        {
            if (static_cast<int> (sample) == scaleSample)
                engine.scaleContinuumEnvelope (physical, 0, scale);
            const double before = static_cast<double> (band.envelope) * band.envelope;
            engine.advanceContinuumForce (first, physical, forces[sample]);
            engine.advanceContinuumForce (second, physical,
                sample < otherForces.size() ? otherForces[sample] : 0.0);
            const double after = static_cast<double> (band.envelope) * band.envelope;
            result.largestRemoval = std::max (result.largestRemoval, before - after);
            band.envelope *= band.envelopeDecay;
            double expectedPower = 0.0;
            for (std::size_t node = 0; node < weights.size(); ++node)
                expectedPower += weights[node]
                    * (std::norm (first.continuumForceState[0][node])
                       + std::norm (second.continuumForceState[0][node]));
            const double actualPower = static_cast<double> (band.envelope) * band.envelope;
            result.aggregationError = std::max (result.aggregationError,
                std::abs (actualPower - expectedPower) / std::max (expectedPower, 1.0e-20));
        }
        result.decay = band.envelopeDecay;
        result.power = static_cast<double> (band.envelope) * band.envelope;
        result.responses = first.continuumForceState[0];
        for (std::size_t node = 0; node < weights.size(); ++node)
            result.frequencies[node] = centre * band.forceFrequencyRatios[node];
        const float tail = band.envelope;
        engine.silenceVoice (first);
        result.resetPreservesTail = band.envelope == tail
            && std::all_of (first.continuumForceState[0].begin(),
                            first.continuumForceState[0].end(),
                            [] (const auto& state) { return state == 0.0; });
        return result;
    }

    struct ContinuumPopulationProbe
    {
        double radius { 0.0 };
        double waveSpeed { 0.0 };
        double stiffness { 0.0 };
        std::vector<std::array<double, 2>> bands;
    };

    static ContinuumPopulationProbe continuumPopulation (float stiffness)
    {
        TaikoEngine engine;
        engine.prepare (384000.0, 64);
        auto drum = TaikoEngine::resolveDrumFor (EngineParameters {}, 0.0f, 0);
        drum.stiffnessBatter = stiffness;
        TaikoEngine::Voice voice;
        voice.strikeRadius = 0.28f;
        engine.buildVoiceModes (voice, drum,
                                TaikoEngine::strikeProfile (Articulation::Don),
                                0.0f, false);
        ContinuumPopulationProbe result { drum.radius, drum.waveSpeed, stiffness, {} };
        for (const auto& band : voice.continuum)
            if (band.centre > 0.0f)
                result.bands.push_back ({ band.centre, band.targetRms });
        return result;
    }

    // Recover the performed pulse duration from its stored force-squared
    // integral, without reading the continuum's spectrum or replaying the
    // variation generator. The nominal UI measurement describes an average
    // contact, while this exposure includes the current tip and impact speed.
    static float performedReferenceContactSeconds (const TaikoEngine& engine) noexcept
    {
        const auto& voice = engine.voices_[0];
        const auto& profile = TaikoEngine::strikeProfile (voice.articulation);
        const double peakForce = voice.contactAmplitude / profile.levelScale;
        constexpr double squaredPulseIntegral =
            4.0 / (3.0 * static_cast<double> (3.14159265358979323846f));
        return static_cast<float> (voice.referenceContactExposure
            / (peakForce * peakForce * squaredPulseIntegral
                * voice.contactExposureAdmittance));
    }

    struct PhysicalContinuumState
    {
        float envelope { 0.0f };
        float distanceGain { 1.0f };
    };

    static std::vector<PhysicalContinuumState> physicalContinuumState (
        const TaikoEngine& engine, int octave = 0)
    {
        std::vector<PhysicalContinuumState> result;
        const auto& voice = physicalForOctave (engine, octave);
        for (const auto& band : voice.continuum)
            if (band.centre > 0.0f)
                result.push_back ({ band.envelope, band.distanceGain });
        return result;
    }

    struct ContinuumRetuneProbe
    {
        double lookupDbError { 0.0 };
        double livePowerDbError { 0.0 };
        double liveProductError { 0.0 };
        double roundTripPowerDbError { 0.0 };
        double roundTripProductError { 0.0 };
        double laterInjectionDbError { 0.0 };
        double rebuildPowerDbError { 0.0 };
        double rebuildProductError { 0.0 };
    };

    static ContinuumRetuneProbe continuumRetuneProbe() noexcept
    {
        ContinuumRetuneProbe result;
        constexpr float bandwidth = 1.35f;
        const auto dbError = [] (double actual, double expected)
        {
            return std::abs (10.0 * std::log10 (
                std::max (actual, 1.0e-300) / std::max (expected, 1.0e-300)));
        };
        const auto variance = [] (const TaikoEngine::Voice::ContinuumBand& band)
        {
            return static_cast<double> (TaikoEngine::bandPassVariance (
                band.lowCoefficient, band.highCoefficient, 2, 7));
        };
        const auto states = [] (TaikoEngine::Voice::ContinuumBand& band)
        {
            return std::array<float*, 18> {
                &band.lowStateLeft, &band.lowStateLeft2,
                &band.highStateLeft, &band.highStateLeft2,
                &band.highStateLeft3, &band.highStateLeft4,
                &band.highStateLeft5, &band.highStateLeft6,
                &band.highStateLeft7, &band.lowStateRight,
                &band.lowStateRight2, &band.highStateRight,
                &band.highStateRight2, &band.highStateRight3,
                &band.highStateRight4, &band.highStateRight5,
                &band.highStateRight6, &band.highStateRight7
            };
        };

        // The lookup covers the complete live domain, including both cutoff-
        // clamp seams and the sub-audio tail of a maximally downward-retuned
        // band. Compare power in dB because that is what the normalization
        // preserves and what a player hears as level.
        for (int index = 0; index <= 768; ++index)
        {
            const float fraction = static_cast<float> (index) / 768.0f;
            const float centre = std::exp2 (-18.6f + fraction * 18.1f);
            const float low = TaikoEngine::continuumEdgeCoefficient (
                centre / bandwidth, 0.5f, 1.0f);
            const float high = TaikoEngine::continuumEdgeCoefficient (
                centre * bandwidth, 0.5f, 1.0f);
            const double exact = TaikoEngine::bandPassVariance (low, high, 2, 7);
            const double lookedUp = std::exp2 (
                TaikoEngine::continuumLogVariance (centre));
            result.lookupDbError = std::max (
                result.lookupDbError, dbError (lookedUp, exact));
        }
        for (const float centre : { 0.45f / bandwidth,
                                    0.45f / bandwidth * 0.9999f,
                                    0.45f / bandwidth * 1.0001f,
                                    0.45f * bandwidth,
                                    0.45f * bandwidth * 0.9999f,
                                    0.45f * bandwidth * 1.0001f })
        {
            const float low = TaikoEngine::continuumEdgeCoefficient (
                centre / bandwidth, 0.5f, 1.0f);
            const float high = TaikoEngine::continuumEdgeCoefficient (
                centre * bandwidth, 0.5f, 1.0f);
            result.lookupDbError = std::max (
                result.lookupDbError,
                dbError (std::exp2 (TaikoEngine::continuumLogVariance (centre)),
                         TaikoEngine::bandPassVariance (low, high, 2, 7)));
        }

        EngineParameters parameters;
        parameters.humanise = 0.0f;
        parameters.tensionModulation = 0.0f;

        TaikoEngine live;
        live.setParameters (parameters);
        live.prepare (48000.0, 64);
        live.trigger (Articulation::Don, 0, 0.9f);
        auto& physical = physicalForOctave (live);
        auto& band = physical.continuum[0];
        band.envelope = 0.73f;
        const auto liveStates = states (band);
        std::array<double, 18> liveProducts {};
        for (std::size_t index = 0; index < liveStates.size(); ++index)
        {
            *liveStates[index] = static_cast<float> (index + 1)
                               * (index % 2 == 0 ? 0.013f : -0.017f);
            liveProducts[index] = band.envelope * *liveStates[index];
        }
        const double livePowerBefore = band.envelope * band.envelope * variance (band);
        live.applyTensionShift (physical, 2.0f);
        const double livePowerAfter = band.envelope * band.envelope * variance (band);
        result.livePowerDbError = dbError (livePowerAfter, livePowerBefore);
        for (std::size_t index = 0; index < liveStates.size(); ++index)
            result.liveProductError = std::max (
                result.liveProductError,
                std::abs (band.envelope * *liveStates[index] - liveProducts[index]));

        // applyTensionShift receives an absolute shift from the build tuning.
        // Returning to one must therefore recover the base variance without
        // accumulating either power or state-coordinate drift.
        live.applyTensionShift (physical, 1.0f);
        result.roundTripPowerDbError = dbError (
            band.envelope * band.envelope * variance (band), livePowerBefore);
        for (std::size_t index = 0; index < liveStates.size(); ++index)
            result.roundTripProductError = std::max (
                result.roundTripProductError,
                std::abs (band.envelope * *liveStates[index] - liveProducts[index]));
        live.applyTensionShift (physical, 2.0f);

        // A contact triggered against the base bank must be converted into the
        // live filter's input coordinate before it joins the physical field.
        auto& strike = live.voices_[static_cast<std::size_t> (
            newestStrikeSlot (live))];
        const double expectedInjectionPower =
            strike.continuumInjection[0] * strike.continuumInjection[0]
            * strike.continuum[0].baseFilterVariance;
        band.envelope = 0.0f;
        // Unit-impulse fixture isolates the coordinate conversion; the exact
        // held-force response is checked independently below.
        band.forceIntegrals.fill ({ 1.0, 0.0 });
        live.advanceContinuumForce (strike, physical, 1.0);
        result.laterInjectionDbError = dbError (
            band.envelope * band.envelope * variance (band),
            expectedInjectionPower);

        TaikoEngine rebuilt;
        rebuilt.setParameters (parameters);
        rebuilt.prepare (48000.0, 64);
        rebuilt.trigger (Articulation::Don, 0, 0.9f);
        auto& before = physicalForOctave (rebuilt).continuum[0];
        before.envelope = 0.61f;
        before.highStateLeft7 = 0.27f;
        before.highStateRight7 = -0.23f;
        const double rebuildPowerBefore =
            before.envelope * before.envelope * variance (before);
        const double rebuildLeftBefore = before.envelope * before.highStateLeft7;
        const double rebuildRightBefore = before.envelope * before.highStateRight7;
        parameters.headDiameter = 0.83f;
        rebuilt.setParameters (parameters);
        rebuilt.refreshDrumIfNeeded();
        const auto& after = physicalForOctave (rebuilt).continuum[0];
        result.rebuildPowerDbError = dbError (
            after.envelope * after.envelope * variance (after),
            rebuildPowerBefore);
        result.rebuildProductError = std::max (
            std::abs (after.envelope * after.highStateLeft7 - rebuildLeftBefore),
            std::abs (after.envelope * after.highStateRight7 - rebuildRightBefore));
        return result;
    }

    static void refreshPhysicalDrums (TaikoEngine& engine) noexcept
    {
        engine.refreshDrumIfNeeded();
    }

    struct AxisymmetricRebuildProbe
    {
        int pairs { 0 };
        int contactModes { 0 };
        double displacementError { 0.0 };
        double velocityError { 0.0 };
        double pendingInputError { 0.0 };
        double muteRateError { 0.0 };
        double contactProjectionError { 0.0 };
        double modeProjectionError { 0.0 };
    };

    static AxisymmetricRebuildProbe axisymmetricRebuildProbe() noexcept
    {
        AxisymmetricRebuildProbe result;

        const auto run = [&result] (EngineParameters beforeParameters,
                                    EngineParameters afterParameters)
        {
            beforeParameters.humanise = 0.0f;
            beforeParameters.tensionModulation = 0.0f;
            afterParameters.humanise = 0.0f;
            afterParameters.tensionModulation = 0.0f;

            TaikoEngine engine;
            engine.setParameters (beforeParameters);
            engine.prepare (48000.0, 64);
            engine.trigger (Articulation::Tsu, 0, 0.9f);
            auto& physical = physicalForOctave (engine);

            const auto velocityOf = [&engine] (const TaikoEngine::Mode& mode)
            {
                const double radius = mode.poleRadius;
                const double sine = mode.resonator.b0;
                if (radius > 0.0 && std::abs (sine) > 1.0e-12
                    && mode.liveOmega > 0.0)
                {
                    const double cosine =
                        -mode.resonator.a1 / (2.0 * radius);
                    const double quadrature =
                        (mode.resonator.y1 * cosine
                         - radius * mode.resonator.y2) / sine;
                    return mode.liveOmega * quadrature
                         - static_cast<double> (
                               mode.decayRate + mode.appliedPalmDecay)
                               * mode.resonator.y1;
                }
                return (mode.resonator.y1 - mode.resonator.y2)
                     * engine.sampleRate_;
            };

            // Give both branches of every pair independent coordinates,
            // velocities and pending increments. A branch-ID copy cannot pass
            // once Air Coupling rotates the two-head eigenbasis.
            physical.modalInput.fill (0.0f);
            for (int index = 0; index < physical.modeCount; ++index)
            {
                auto& mode = physical.modes[static_cast<std::size_t> (index)];
                if (! mode.membrane || mode.circumferentialOrder != 0)
                    continue;
                const auto id = static_cast<std::size_t> (mode.physicalIndex);
                const double displacement = 0.006 * static_cast<double> (id + 1u);
                const double velocity = 0.17 * static_cast<double> (id + 1u)
                                      - 0.43;
                mode.resonator.y1 = displacement;
                const double radius = mode.poleRadius;
                const double sine = mode.resonator.b0;
                if (radius > 0.0 && std::abs (sine) > 1.0e-12
                    && mode.liveOmega > 0.0)
                {
                    const double cosine =
                        -mode.resonator.a1 / (2.0 * radius);
                    const double quadrature =
                        (velocity
                         + static_cast<double> (
                               mode.decayRate + mode.appliedPalmDecay)
                               * displacement) / mode.liveOmega;
                    mode.resonator.y2 =
                        (displacement * cosine - quadrature * sine) / radius;
                }
                else
                {
                    mode.resonator.y2 = displacement
                                      - velocity / engine.sampleRate_;
                }
                physical.modalInput[id] =
                    0.021f * static_cast<float> (id + 1u);
            }

            using PairState = std::array<double, 7>;
            const auto capture = [&velocityOf] (const TaikoEngine::Voice& drum)
            {
                std::array<PairState, TaikoEngine::axisymmetricEntryCount> states {};
                for (int index = 0; index < drum.modeCount; ++index)
                {
                    const auto& mode = drum.modes[static_cast<std::size_t> (index)];
                    if (! mode.membrane || mode.circumferentialOrder != 0
                        || mode.modeEntry >= TaikoEngine::axisymmetricEntryCount)
                        continue;
                    auto& state = states[mode.modeEntry];
                    const auto id = static_cast<std::size_t> (mode.physicalIndex);
                    const double displacement = mode.resonator.y1;
                    const double velocity = velocityOf (mode);
                    const double pending = mode.resonator.b0 * drum.modalInput[id];
                    state[0] += mode.batterParticipation * displacement;
                    state[1] += mode.resonantParticipation * displacement;
                    state[2] += mode.batterParticipation * velocity;
                    state[3] += mode.resonantParticipation * velocity;
                    state[4] += mode.batterParticipation * pending;
                    state[5] += mode.resonantParticipation * pending;
                    state[6] += mode.localMuteDampingRate;
                }
                return states;
            };

            const auto before = capture (physical);
            engine.setParameters (afterParameters);
            engine.refreshDrumIfNeeded();
            const auto& rebuilt = physicalForOctave (engine);
            const auto after = capture (rebuilt);

            const auto relativeError = [] (double left, double right,
                                           double floor)
            {
                return std::abs (left - right)
                     / std::max ({ std::abs (left), std::abs (right), floor });
            };
            for (std::size_t entry = 0; entry < before.size(); ++entry)
            {
                result.displacementError = std::max (
                    result.displacementError,
                    std::max (relativeError (before[entry][0], after[entry][0], 1.0e-7),
                              relativeError (before[entry][1], after[entry][1], 1.0e-7)));
                result.velocityError = std::max (
                    result.velocityError,
                    std::max (relativeError (before[entry][2], after[entry][2], 1.0e-7),
                              relativeError (before[entry][3], after[entry][3], 1.0e-7)));
                result.pendingInputError = std::max (
                    result.pendingInputError,
                    std::max (relativeError (before[entry][4], after[entry][4], 1.0e-9),
                              relativeError (before[entry][5], after[entry][5], 1.0e-9)));
                result.muteRateError = std::max (
                    result.muteRateError,
                    relativeError (before[entry][6], after[entry][6], 1.0e-7));
                ++result.pairs;
            }

            const TaikoEngine::Voice* contact = nullptr;
            for (const auto& candidate : engine.voices_)
                if (candidate.active
                    && (contact == nullptr
                        || candidate.startOrder > contact->startOrder))
                    contact = &candidate;
            if (contact == nullptr)
                return;

            const auto& profile = TaikoEngine::strikeProfile (contact->articulation);
            const auto& entries = TaikoEngine::membraneModes();
            for (int index = 0; index < rebuilt.modeCount; ++index)
            {
                const auto& mode = rebuilt.modes[static_cast<std::size_t> (index)];
                if (! mode.membrane || mode.circumferentialOrder != 0)
                    continue;
                const auto id = static_cast<std::size_t> (mode.physicalIndex);
                const auto& entry = entries[static_cast<std::size_t> (mode.modeEntry)];
                const double common = TaikoEngine::besselJ (
                    0, entry.besselZero * contact->strikeRadius)
                    * profile.membraneGain * profile.levelScale;
                const double expected = common * mode.batterParticipation;
                result.contactProjectionError = std::max (
                    result.contactProjectionError,
                    std::abs (contact->contactProjection[id] - expected)
                        / std::max (std::abs (common), 1.0e-9));
                ++result.contactModes;
            }

            TaikoEngine::Voice expectedContact;
            expectedContact.strikeRadius = contact->strikeRadius;
            expectedContact.strikeAngle = contact->strikeAngle;
            engine.buildVoiceModes (
                expectedContact,
                engine.drumCache_[contact->physicalDrumIndex],
                profile, profile.muteAmount, false);
            for (int index = 0; index < expectedContact.modeCount; ++index)
            {
                const auto& mode =
                    expectedContact.modes[static_cast<std::size_t> (index)];
                const auto id = static_cast<std::size_t> (mode.physicalIndex);
                result.modeProjectionError = std::max (
                    result.modeProjectionError,
                    static_cast<double> (
                        std::abs (contact->modeProjection[id] - mode.drive)
                        / std::max (std::abs (mode.drive), 1.0e-9f)));
            }
        };

        EngineParameters coupled;
        coupled.cavityCoupling = 0.85f;
        auto uncoupled = coupled;
        uncoupled.cavityCoupling = 0.0f;
        run (coupled, uncoupled);

        EngineParameters lightOpen = uncoupled;
        lightOpen.headMaterial = 0.10f;
        lightOpen.resonantTension = 0.20f;
        EngineParameters heavyCoupled = coupled;
        heavyCoupled.headMaterial = 0.90f;
        heavyCoupled.resonantTension = 0.80f;
        run (lightOpen, heavyCoupled);
        return result;
    }

    struct HostileAxisymmetricMuteProbe
    {
        int modesBefore { 0 };
        int modesAfter { 0 };
        int muteTicks { 0 };
        float maximumOldBatterFraction { 0.0f };
        float baseRate { 0.0f };
        float rebuiltBatterRate { 0.0f };
        double rateError { 0.0 };
    };

    static HostileAxisymmetricMuteProbe hostileAxisymmetricMuteProbe() noexcept
    {
        constexpr int octave = 2;
        constexpr std::size_t entry = 1;
        EngineParameters parameters;
        parameters.headDiameter = 0.60f;
        parameters.bodyDepth = 0.24f;
        parameters.tension = 0.17f;
        parameters.headMaterial = 0.23f;
        parameters.resonantTension = 0.12f;
        parameters.cavityCoupling = 0.85f;
        parameters.pitch = 20.7f;
        parameters.octaveBody = 1.0f;
        parameters.humanise = 0.0f;
        parameters.tensionModulation = 0.0f;

        TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (8000.0, 64);
        engine.trigger (Articulation::Tsu, octave, 0.9f);

        HostileAxisymmetricMuteProbe result;
        const auto& before = physicalForOctave (engine, octave);
        result.muteTicks = before.localMuteTicksRemaining;
        result.baseRate = before.localMuteBaseDampingRates[entry];
        for (int index = 0; index < before.modeCount; ++index)
        {
            const auto& mode = before.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane || mode.circumferentialOrder != 0
                || mode.modeEntry != entry)
                continue;
            ++result.modesBefore;
            result.maximumOldBatterFraction = std::max (
                result.maximumOldBatterFraction,
                engine.drumCache_[octave].batterDensity
                    * mode.batterParticipation * mode.batterParticipation);
        }

        // At 8 kHz the old entry has only its rear-head branch. A small diameter
        // move brings the batter branch below Nyquist while the same Tsu palm is
        // still held, so no rendered branch can be used to reconstruct its base
        // damping rate.
        parameters.headDiameter = 0.63f;
        engine.setParameters (parameters);
        engine.refreshDrumIfNeeded();

        const auto& after = physicalForOctave (engine, octave);
        const auto& drum = engine.drumCache_[octave];
        for (int index = 0; index < after.modeCount; ++index)
        {
            const auto& mode = after.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane || mode.circumferentialOrder != 0
                || mode.modeEntry != entry)
                continue;
            ++result.modesAfter;
            const float batterFraction = TaikoEngine::batterFractionFor (drum, mode);
            const float expected = result.baseRate * batterFraction;
            result.rateError = std::max (
                result.rateError,
                static_cast<double> (
                    std::abs (mode.localMuteDampingRate - expected)
                    / std::max (std::abs (expected), 1.0e-7f)));
            if (batterFraction > 0.5f)
                result.rebuiltBatterRate = mode.localMuteDampingRate;
        }
        return result;
    }

    static int validContinuumVarianceEntries (const TaikoEngine& engine) noexcept
    {
        int count = 0;
        for (const auto& drum : engine.continuumVarianceCache_)
            for (const auto& band : drum)
                if (band.valid)
                    ++count;
        return count;
    }

    // Leave one continuum band as the only audible path. The contact is kept
    // because it is what lights the band's energy; resolved membrane/shell
    // modes, contact noise, tack rattle and the airborne click are silenced so
    // a test can ask whether this filter owns its octave rather than infer it
    // through the rest of a stroke.
    static void isolateContinuumBand (TaikoEngine& engine, int kept,
                                      int slot = 0) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t> (slot)];
        voice.modeProjection.fill (0.0f);
        for (auto& mode : physicalForSlot (engine, slot).modes)
        {
            mode.micLeft = mode.micRight = 0.0f;
            mode.micLeftQuadrature = mode.micRightQuadrature = 0.0f;
        }
        for (int index = 0; index < TaikoEngine::continuumBandCount; ++index)
            if (index != kept)
            {
                auto& band = voice.continuum[static_cast<std::size_t> (index)];
                band.targetRms = 0.0f;
                band.level = 0.0f;
                voice.continuumInjection[static_cast<std::size_t> (index)] = 0.0f;
            }
        for (auto& contact : voice.contacts)
            contact.noiseAmplitude = 0.0f;
        voice.contactNoiseAmplitude = 0.0f;
        voice.tackScale = 0.0f;
        voice.directGainLeft = 0.0f;
        voice.directGainRight = 0.0f;
    }

    static void isolateResolvedBank (TaikoEngine& engine, int slot = 0) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t> (slot)];
        voice.continuumInjection.fill (0.0f);
        for (auto& contact : voice.contacts)
            contact.noiseAmplitude = 0.0f;
        voice.contactNoiseAmplitude = 0.0f;
        voice.tackScale = 0.0f;
        voice.directGainLeft = 0.0f;
        voice.directGainRight = 0.0f;
    }

    // Leave only the normal-force pressure pulse. The physical bank may still
    // be advanced by the reciprocal contact, but none of its observers or the
    // statistical/tack paths can reach the output.
    static void isolateDirectPath (TaikoEngine& engine, int slot = 0) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t> (slot)];
        voice.modeProjection.fill (0.0f);
        voice.continuumInjection.fill (0.0f);
        voice.tackScale = 0.0f;
        for (auto& mode : physicalForSlot (engine, slot).modes)
        {
            mode.micLeft = mode.micRight = 0.0f;
            mode.micLeftQuadrature = mode.micRightQuadrature = 0.0f;
        }
    }

    // Leave the contact-driven resonant object, including roughness, while
    // removing the separate statistical and airborne paths.
    static void isolateContactDrivenObject (TaikoEngine& engine,
                                             int slot = 0) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t> (slot)];
        voice.continuumInjection.fill (0.0f);
        voice.tackScale = 0.0f;
        voice.directGainLeft = 0.0f;
        voice.directGainRight = 0.0f;
    }

    static void isolateMembraneBank (TaikoEngine& engine, int slot = 0) noexcept
    {
        isolateResolvedBank (engine, slot);
        for (auto& mode : physicalForSlot (engine, slot).modes)
            if (! mode.membrane)
            {
                mode.micLeft = mode.micRight = 0.0f;
                mode.micLeftQuadrature = mode.micRightQuadrature = 0.0f;
            }
    }

    static void isolateMembraneOrderGroup (TaikoEngine& engine,
                                            bool axisymmetric,
                                            int slot = 0) noexcept
    {
        isolateMembraneBank (engine, slot);
        for (auto& mode : physicalForSlot (engine, slot).modes)
            if (mode.membrane
                && (mode.circumferentialOrder == 0) != axisymmetric)
            {
                mode.micLeft = mode.micRight = 0.0f;
                mode.micLeftQuadrature = mode.micRightQuadrature = 0.0f;
            }
    }

    static void setNoteSequence (TaikoEngine& engine, std::uint64_t sequence) noexcept
    {
        engine.noteSequence_ = sequence;
    }

    static void setNoiseSeed (TaikoEngine& engine, std::uint32_t seed,
                              int slot = 0) noexcept
    {
        auto& voice = engine.voices_[static_cast<std::size_t> (slot)];
        voice.noiseState = seed | 1u;
        voice.tackNoiseState = (seed ^ 0x5bf03635u) | 1u;
        physicalForSlot (engine, slot).noiseState = (seed ^ 0x9e3779b9u) | 1u;
    }

    static void setContinuumNoiseSeed (TaikoEngine& engine, std::uint32_t seed) noexcept
    {
        // Change only the observed residual realization. Contact roughness,
        // stick state and every resolved force trajectory remain unchanged.
        physicalForSlot (engine, 0).noiseState = seed | 1u;
    }

    // Silence the head's continuum on a voice that has already been triggered,
    // leaving the resolved bank and the airborne click alone. There is no way
    // to ask a question about what the glide does to the top of the spectrum
    // through the public interface: the same glide retunes the continuum's band
    // corners, which is deliberate, so a measurement with the continuum in it
    // cannot separate the head moving from the resonators being rewritten.
    //
    // Safe to call after `trigger` and before the first block because it moves
    // `level`, which is written once when the voice is built, and the relight
    // every later contact performs moves `envelope`. A stroke that schedules one
    // contact - a Don does - therefore stays silenced for the whole voice.
    static void silenceContinuum (TaikoEngine& engine, int slot = 0) noexcept
    {
        for (auto& band : engine.voices_[static_cast<std::size_t> (slot)].continuum)
        {
            band.targetRms = 0.0f;
            band.level = 0.0f;
        }
        engine.voices_[static_cast<std::size_t> (slot)].continuumInjection.fill (0.0f);
        for (auto& band : physicalForSlot (engine, slot).continuum)
            band.envelope = 0.0f;
    }

    // The frequency multiplier the attack glide is currently applying to the
    // membrane. Read directly because the glide is a few tens of cents on a
    // partial that is gone in a second, which no window short enough to catch
    // it can resolve.
    static float appliedTensionShift (const TaikoEngine& engine, int slot = 0) noexcept
    {
        return physicalForSlot (engine, slot).appliedTensionShift;
    }

    struct TensionProjectionProbe
    {
        double eigensolveError { 0.0 };
        double musicalShiftError { 0.0 };
        double roundTripError { 0.0 };
        double shellShiftError { 0.0 };
        float minimumShare { 1.0f };
        float maximumShare { 0.0f };
        float rearOnlyShare { 0.0f };
        int rearOnlyModes { 0 };
    };

    static TensionProjectionProbe tensionProjectionProbe (
        EngineParameters parameters, int octave) noexcept
    {
        parameters.humanise = 0.0f;
        const auto drum = TaikoEngine::resolveDrumFor (
            TaikoEngine::sanitise (parameters), 0.0f, octave);
        TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, 64);
        engine.trigger (Articulation::DonRim, octave, 0.9f);
        auto& voice = physicalForOctave (engine, octave);
        TensionProjectionProbe result;
        constexpr float rise = 0.02f;
        engine.applyTensionShift (voice, std::sqrt (1.0f + rise), rise);

        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            const double ratio = mode.liveOmega / mode.omega;
            if (! mode.membrane)
            {
                result.shellShiftError = std::max (
                    result.shellShiftError, std::abs (ratio - 1.0));
                continue;
            }
            const auto& entry = TaikoEngine::membraneModes()[mode.modeEntry];
            const float lambda = static_cast<float> (entry.besselZero);
            const auto omegas = TaikoEngine::membraneModeOmegas (
                drum, drum.radius, lambda, mode.circumferentialOrder);
            const double tensile = static_cast<double> (omegas.batter)
                                 * omegas.batter
                                 / (1.0 + drum.stiffnessBatter * lambda * lambda);
            double derivative = 1.0
                / (1.0 + drum.stiffnessBatter * lambda * lambda);
            if (mode.circumferentialOrder == 0)
            {
                float diagonalB = 0.0f, diagonalR = 0.0f, offDiagonal = 0.0f;
                TaikoEngine::axisymmetricDiagonals (
                    drum, { lambda, omegas.batter, omegas.resonant },
                    drum.cavityStiffnesses[mode.modeEntry],
                    diagonalB, diagonalR, offDiagonal);
                // Independent double-precision characteristic polynomial,
                // perturbed on the batter diagonal only. Its central finite
                // difference audits the projection through rendered poles.
                const bool upper = mode.physicalIndex % 2 == 0;
                const auto eigenvalue = [&] (double tensionIncrement)
                {
                    const double batter = diagonalB + tensile * tensionIncrement;
                    const double split = std::hypot (
                        batter - diagonalR, 2.0 * offDiagonal);
                    return 0.5 * (batter + diagonalR + (upper ? split : -split));
                };
                constexpr double step = 1.0e-5;
                derivative = (eigenvalue (step) - eigenvalue (-step))
                           / (2.0 * step * eigenvalue (0.0));
                if (mode.batterParticipation == 0.0f)
                {
                    ++result.rearOnlyModes;
                    result.rearOnlyShare = std::max (
                        result.rearOnlyShare, mode.batterTensionFraction);
                }
            }
            const double renderedDerivative = (ratio * ratio - 1.0) / rise;
            result.eigensolveError = std::max (
                result.eigensolveError, std::abs (renderedDerivative - derivative));
            result.minimumShare = std::min (
                result.minimumShare, mode.batterTensionFraction);
            result.maximumShare = std::max (
                result.maximumShare, mode.batterTensionFraction);
        }

        // Removing strain must restore exact common musical transposition;
        // applying absolute shifts repeatedly must not compound the correction.
        for (const float shift : { 1.37f, 1.0f })
        {
            engine.applyTensionShift (voice, shift);
            for (int index = 0; index < voice.modeCount; ++index)
            {
                const auto& mode = voice.modes[static_cast<std::size_t> (index)];
                if (! mode.membrane)
                    continue;
                const double error = std::abs (mode.liveOmega / mode.omega - shift);
                auto& maximum = shift == 1.0f ? result.roundTripError
                                             : result.musicalShiftError;
                maximum = std::max (maximum, error);
            }
        }
        return result;
    }

    // The tack line as the stroke set it up. Read rather than measured for the
    // same reason the wooden bank is: the rattle lasts about a millisecond and
    // shares its band with the head's continuum, so the question of which
    // numbers it was given cannot be answered from the spectrum.
    struct TackLine
    {
        float preload { 0.0f };
        float rimGain { 0.0f };
        float scale { 0.0f };
        float peakContactForce { 0.0f };
    };

    static TackLine tackLine (const TaikoEngine& engine, int slot = 0) noexcept
    {
        const auto& voice = engine.voices_[static_cast<std::size_t> (slot)];
        TackLine result;
        result.preload = voice.tackPreload;
        result.rimGain = voice.tackRimGain;
        result.scale = voice.tackScale;
        result.peakContactForce = voice.solvedContactForce
                                * TaikoEngine::strikeProfile (voice.articulation).levelScale;
        return result;
    }

    static void disableTackLine (TaikoEngine& engine, int slot = 0) noexcept
    {
        engine.voices_[static_cast<std::size_t> (slot)].tackScale = 0.0f;
    }

    static bool everyDecayIsFinite (const TaikoEngine& engine)
    {
        for (const auto& voice : engine.physicalDrums_)
            for (int index = 0; index < voice.modeCount; ++index)
            {
                const auto& mode = voice.modes[static_cast<std::size_t> (index)];
                if (! std::isfinite (mode.decayRate) || ! std::isfinite (mode.drive)
                    || ! std::isfinite (mode.omega))
                    return false;
            }
        return true;
    }

    static float radiationEfficiency (int order, float ka) noexcept
    {
        return TaikoEngine::radiationEfficiency (order, ka);
    }

    // Exposes the enclosed-air column's reactive stiffness factor on its
    // own, so its low-frequency-limit fallback for a non-positive x (taken
    // whenever the axisymmetric branch it is evaluated at has collapsed to
    // omega <= 0) and its quarter-wave floor can be asserted directly rather
    // than only inferred from a resolved drum's reported cavityStiffnessFactor.
    static float columnStiffnessFactor (float x) noexcept
    {
        return TaikoEngine::columnStiffnessFactor (x);
    }

    // Exposes the head's bending-stiffness stretch factor on its own, so its
    // "!(stiffness > 0.0f)" fallback to an ideal membrane (1.0, no stretch)
    // can be asserted directly. resolveDrumGeometry's stiffnessBatter and
    // stiffnessResonant are both a positive rigidity divided by a positive
    // tension and radius squared - never zero, negative or NaN for any
    // sanitised EngineParameters - so the guard is never reached from
    // Source/; it was previously exercised only indirectly, and only on its
    // ordinary branch, through a resolved drum's headStiffnessParameter.
    static float stiffnessStretch (float besselZero, float stiffness) noexcept
    {
        return TaikoEngine::stiffnessStretch (besselZero, stiffness);
    }

    // Exposes the mounting-loss shelf's own corner frequency on its own, so
    // its "std::max(mountCorner, 1.0f)" floor can be asserted directly.
    // resolveDrumGeometry only ever derives mountCorner as a fixed constant
    // divided by drum.radius, itself clamped to [radiusFloor, radiusCeiling]
    // (0.008 m .. 3.75 m), so the corner it ever hands to this function sits
    // between roughly 7 Hz and 3.3 kHz - never at or below the floor - and
    // the guard was previously reached nowhere at all, direct or indirect.
    static float mountingLossAt (float mountLoss, float mountCorner,
                                 float frequency) noexcept
    {
        return TaikoEngine::mountingLossAt (mountLoss, mountCorner, frequency);
    }

    // Exposes the continuum band's exact state-space variance solve on its
    // own, so its "!(isfinite(variance) && variance > 1e-12)" fallback to
    // unit variance - taken when the Lyapunov iteration's own stationary
    // covariance comes out non-finite or too small to be real - can be
    // asserted directly. buildVoiceModes's sole call site always derives its
    // pair as lowCoefficient = continuumEdgeCoefficient(centre / bandwidth,
    // ...) and highCoefficient = continuumEdgeCoefficient(centre * bandwidth,
    // ...), and continuumEdgeCoefficient is non-decreasing in its cutoff, so
    // lowCoefficient sits strictly below highCoefficient for every band any
    // drum ever builds; only an inverted or coincident pair reaches the
    // fallback, and nothing in Source/ ever passes one.
    static float continuumBandVariance (float lowCoefficient,
                                        float highCoefficient) noexcept
    {
        return TaikoEngine::bandPassVariance (
            lowCoefficient, highCoefficient, 2, 7);
    }

    static float coherentRadiationPower (float selfPower, float crossPower,
                                         float phase) noexcept
    {
        return TaikoEngine::coherentRadiationPower (selfPower, crossPower, phase);
    }

    struct AxisymmetricRadiationProbe
    {
        bool found { false };
        bool shifted { false };
        bool nonAxisPathIsExact { false };
        float omega { 0.0f };
        float shiftedOmega { 0.0f };
        float delaySeconds { 0.0f };
        float selfPrefactor { 0.0f };
        float crossPrefactor { 0.0f };
        float efficiency { 0.0f };
        float shiftedEfficiency { 0.0f };
        float builtDecay { 0.0f };
        float observedDecay { 0.0f };
        float otherLoss { 0.0f };
        float shiftedDecay { 0.0f };
        float shiftedOtherLoss { 0.0f };
    };

    static AxisymmetricRadiationProbe axisymmetricRadiationProbe (
        const EngineParameters& rawParameters, int octave, float shift) noexcept
    {
        constexpr float pi = 3.14159265358979f;
        constexpr float airSoundSpeed = 343.0f;
        const auto parameters = TaikoEngine::sanitise (rawParameters);
        const auto drum = TaikoEngine::resolveDrumFor (parameters, 0.0f, octave);

        TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, 64);
        engine.reset();
        engine.trigger (Articulation::Don, octave, 0.9f);

        auto& voice = physicalForOctave (engine, octave);
        AxisymmetricRadiationProbe result;
        int targetIndex = -1;
        for (int index = 0; index < voice.activeModeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (mode.membrane && mode.circumferentialOrder == 0
                && mode.modeEntry == 0 && mode.physicalIndex == 1)
            {
                targetIndex = index;
                break;
            }
        }
        if (targetIndex < 0)
            return result;

        const auto otherLossAt = [&voice] (const TaikoEngine::Mode& mode,
                                            float omega)
        {
            return mode.decayFixed + mode.lossOmega * omega
                 + mode.lossOmegaSquared * omega * omega
                 + TaikoEngine::mountingLossAt (
                       voice.mountLoss, voice.mountCorner,
                       omega / (2.0f * pi));
        };

        const auto& mode = voice.modes[static_cast<std::size_t> (targetIndex)];
        result.found = true;
        result.omega = mode.omega;
        result.delaySeconds = voice.radiationDelaySeconds;
        result.selfPrefactor = mode.radiationPrefactor;
        result.crossPrefactor = mode.radiationCrossPrefactor;
        result.efficiency = TaikoEngine::radiationEfficiency (
            0, mode.omega * voice.radiusMetres / airSoundSpeed);
        result.builtDecay = mode.decayRate;
        result.observedDecay = TaikoEngine::observeMode (
            drum, 0, 1, TaikoEngine::tuningStrikeRadius()).decayRate;
        result.otherLoss = otherLossAt (mode, mode.omega);

        result.shiftedOmega = 2.0f * pi
                            * (mode.omega * shift
                               / (2.0f * pi));
        engine.applyTensionShift (voice, shift);
        const auto& shiftedMode =
            voice.modes[static_cast<std::size_t> (targetIndex)];
        result.shifted = shiftedMode.liveOmega > 0.0;
        result.shiftedEfficiency = TaikoEngine::radiationEfficiency (
            0, result.shiftedOmega * voice.radiusMetres / airSoundSpeed);
        result.shiftedDecay = shiftedMode.decayRate;
        result.shiftedOtherLoss = otherLossAt (shiftedMode, result.shiftedOmega);

        for (int index = 0; index < voice.activeModeCount; ++index)
        {
            const auto& nonAxis = voice.modes[static_cast<std::size_t> (index)];
            if (! nonAxis.membrane || nonAxis.circumferentialOrder == 0)
                continue;

            const float omega = 2.0f * pi
                              * (nonAxis.omega * shift
                                 / (2.0f * pi));
            const float efficiency = TaikoEngine::radiationEfficiency (
                static_cast<int> (nonAxis.circumferentialOrder),
                omega * voice.radiusMetres / airSoundSpeed);
            const float legacyDecay =
                nonAxis.decayFixed + nonAxis.lossOmega * omega
                + nonAxis.lossOmegaSquared * omega * omega
                + nonAxis.radiationPrefactor * efficiency
                + TaikoEngine::mountingLossAt (
                      voice.mountLoss, voice.mountCorner,
                      omega / (2.0f * pi));
            result.nonAxisPathIsExact =
                nonAxis.radiationCrossPrefactor == 0.0f
                && nonAxis.decayRate == legacyDecay;
            break;
        }

        return result;
    }

    static std::uint64_t strokeCount (const TaikoEngine& engine) noexcept
    {
        return engine.noteSequence_;
    }

    static void setStrokeCount (TaikoEngine& engine, std::uint64_t count) noexcept
    {
        engine.noteSequence_ = count;
    }

    static int activeVoices (const TaikoEngine& engine) noexcept
    {
        int count = 0;
        for (const auto& voice : engine.voices_)
            if (voice.active)
                ++count;
        return count;
    }

    static float cachedPitchBend (const TaikoEngine& engine) noexcept
    {
        return engine.drumCacheBend_;
    }

    static bool drumCacheValid (const TaikoEngine& engine) noexcept
    {
        return engine.drumCacheValid_;
    }

    static std::uint64_t physicalConfigurationRevision (
        const TaikoEngine& engine) noexcept
    {
        return engine.physicalConfigurationRevision_;
    }

    static float physicalConfigurationPitch (const TaikoEngine& engine,
                                              int octave = 0) noexcept
    {
        return physicalForOctave (engine, octave).configurationPitch;
    }

    struct SharedTopology
    {
        int octave0Modes { 0 };
        int octave1Modes { 0 };
        int contactSlots { 0 };
        int contactModeStates { 0 };
        bool stableAddress { false };
        bool uniqueModeIds { false };
    };

    static SharedTopology sharedTopology() noexcept
    {
        TaikoEngine engine;
        EngineParameters parameters;
        parameters.humanise = 0.0f;
        engine.setParameters (parameters);
        engine.prepare (48000.0, 64);

        engine.trigger (Articulation::Don, 0, 0.8f);
        const auto* address = &physicalForOctave (engine, 0);
        engine.trigger (Articulation::Ka, 0, 0.8f);
        engine.trigger (Articulation::Tsu, 0, 0.8f);

        SharedTopology result;
        const auto& first = physicalForOctave (engine, 0);
        result.stableAddress = address == &first;
        result.octave0Modes = first.modeCount;
        std::set<int> ids;
        for (int index = 0; index < first.modeCount; ++index)
            ids.insert (first.modes[static_cast<std::size_t> (index)].physicalIndex);
        result.uniqueModeIds = static_cast<int> (ids.size()) == first.modeCount;

        for (const auto& strike : engine.voices_)
            if (strike.active)
            {
                ++result.contactSlots;
                result.contactModeStates += strike.modeCount;
            }

        engine.trigger (Articulation::Don, 1, 0.8f);
        result.octave1Modes = physicalForOctave (engine, 1).modeCount;
        return result;
    }

    static double sharedRecurrenceError() noexcept
    {
        TaikoEngine forward;
        TaikoEngine reverse;
        EngineParameters parameters;
        parameters.humanise = 0.0f;
        parameters.strikeNoise = 0.0f;
        parameters.tensionModulation = 0.0f;
        parameters.drive = 0.0f;
        forward.setParameters (parameters);
        reverse.setParameters (parameters);
        forward.prepare (48000.0, 64);
        reverse.prepare (48000.0, 64);

        forward.trigger (Articulation::Don, 0, 0.62f);
        forward.trigger (Articulation::Ka, 0, 0.87f);
        // Reverse the slot allocation while retaining each contact's performed
        // identity. Swapping sequence indices would change the physical inputs
        // as well as their summation order, even when Humanise is zero.
        reverse.noteSequence_ = 1;
        reverse.trigger (Articulation::Ka, 0, 0.87f);
        reverse.noteSequence_ = 0;
        reverse.trigger (Articulation::Don, 0, 0.62f);

        std::array<float, 64> left {};
        std::array<float, 64> right {};
        forward.process (left.data(), right.data(), static_cast<int> (left.size()));
        reverse.process (left.data(), right.data(), static_cast<int> (left.size()));

        const auto& a = physicalForOctave (forward);
        const auto& b = physicalForOctave (reverse);
        double error = std::abs (static_cast<double> (a.ageSamples) - 64.0)
                     + std::abs (static_cast<double> (b.ageSamples) - 64.0);
        for (int index = 0; index < a.modeCount; ++index)
        {
            const auto id = a.modes[static_cast<std::size_t> (index)].physicalIndex;
            const auto found = std::find_if (
                b.modes.begin(), b.modes.begin() + b.modeCount,
                [id] (const TaikoEngine::Mode& mode)
                {
                    return mode.physicalIndex == id;
                });
            if (found == b.modes.begin() + b.modeCount)
                return 1.0;
            error = std::max (error, std::abs (
                a.modes[static_cast<std::size_t> (index)].resonator.y1
                    - found->resonator.y1));
            error = std::max (error, std::abs (
                a.modes[static_cast<std::size_t> (index)].resonator.y2
                    - found->resonator.y2));
        }
        return error;
    }

    static bool secondTriggerPreservesFirstTransient() noexcept
    {
        TaikoEngine engine;
        EngineParameters parameters;
        parameters.humanise = 0.0f;
        engine.setParameters (parameters);
        engine.prepare (48000.0, 64);
        engine.trigger (Articulation::Don, 0, 0.8f);

        const auto projection = engine.voices_[0].modeProjection;
        const auto injection = engine.voices_[0].continuumInjection;
        const auto noiseState = engine.voices_[0].noiseState;
        const auto directWrite = engine.voices_[0].directWriteIndex;
        const auto contactCount = engine.voices_[0].contactCount;
        const auto firstContact = engine.voices_[0].contacts[0];

        engine.trigger (Articulation::Ka, 0, 0.7f);
        const auto& first = engine.voices_[0];
        return first.modeProjection == projection
            && first.continuumInjection == injection
            && first.noiseState == noiseState
            && first.directWriteIndex == directWrite
            && first.contactCount == contactCount
            && first.contacts[0].startSample == firstContact.startSample
            && first.contacts[0].lengthSamples == firstContact.lengthSamples
            && first.contacts[0].amplitude == firstContact.amplitude
            && first.contacts[0].noiseAmplitude == firstContact.noiseAmplitude;
    }

    static double retriggerFadeStateError() noexcept
    {
        TaikoEngine engine;
        EngineParameters parameters;
        parameters.humanise = 0.0f;
        parameters.tensionModulation = 0.0f;
        engine.setParameters (parameters);
        engine.prepare (48000.0, 64);
        engine.trigger (Articulation::Don, 0, 0.8f);

        auto& physical = physicalForOctave (engine);
        if (physical.modeCount <= 0)
            return 1.0;
        physical.active = true;
        physical.ageSamples = 1;
        physical.modes[0].resonator.y1 = 0.25;
        physical.modes[0].resonator.y2 = -0.10;
        physical.tensionEnvelope = 0.30f;
        physical.retireGain = 0.35f;
        physical.retireStep = 0.01f;

        engine.trigger (Articulation::Don, 0, 0.8f);
        return std::max ({
            std::abs (physical.modes[0].resonator.y1 - 0.25 * 0.35),
            std::abs (static_cast<double> (physical.tensionEnvelope)
                      - 0.30 * 0.35 * 0.35),
            std::abs (static_cast<double> (physical.retireGain) - 1.0)
        });
    }

    static float contactSecondsFor (const EngineParameters& parameters,
                                    Articulation articulation, int octaveOffset,
                                    float velocity) noexcept
    {
        return TaikoEngine::measureContact (parameters, articulation, octaveOffset,
                                            velocity);
    }

    static float shapeVelocity (float velocity, float curve) noexcept
    {
        return TaikoEngine::shapeVelocity (velocity, curve);
    }

    struct NonlinearContactAudit
    {
        double durationSeconds { 0.0 };
        double impulse { 0.0 };
        double peakForce { 0.0 };
        double minimumForce { 0.0 };
        double minimumExposureStep { 0.0 };
        double normalizedResidualExposure { 0.0 };
        double impulseMomentumClosure { 0.0 };
        double stereoEnergy { 0.0 };
        double finalEnergyRatio { 0.0 };
        double maximumEnergyRatio { 0.0 };
        double maximumEnergyIncrease { 0.0 };
        bool released { false };
        bool finite { true };
    };

    struct DisabledModeContactAudit
    {
        int disabledModes { 0 };
        double forceDifference { 0.0 };
        bool finite { true };
    };

    // A live Pitch move can carry only the top of an existing bank beyond the
    // renderer's Nyquist guard. Those disabled coordinates must disappear from
    // both halves of the reciprocal contact: counting them in the compliance
    // while refusing their force increment makes the stick collide with a
    // degree of freedom that is not there. Compare production with the same
    // bank after explicitly removing those coordinates' inverse masses.
    static DisabledModeContactAudit disabledModeContactAudit() noexcept
    {
        EngineParameters low;
        low.humanise = 0.0f;
        low.strikeNoise = 0.0f;
        low.tensionModulation = 0.0f;
        low.pitch = -24.0f;
        low.headDiameter = 0.20f;
        low.tension = 1.0f;
        low.headMaterial = 0.0f;
        low.bachiHardness = 1.0f;

        TaikoEngine actual;
        TaikoEngine reference;
        actual.setParameters (low);
        reference.setParameters (low);
        actual.prepare (8000.0, 1);
        reference.prepare (8000.0, 1);
        actual.trigger (Articulation::Don, 0, 1.0f);
        reference.trigger (Articulation::Don, 0, 1.0f);

        auto raised = low;
        raised.pitch = 24.0f;
        actual.setParameters (raised);
        reference.setParameters (raised);

        auto& referenceBank = physicalForOctave (reference);
        reference.updateVoiceControl (referenceBank);

        DisabledModeContactAudit result;
        for (int index = 0; index < referenceBank.modeCount; ++index)
        {
            auto& mode = referenceBank.modes[static_cast<std::size_t> (index)];
            if (mode.membrane && std::abs (mode.resonator.b0) < 1.0e-14)
            {
                ++result.disabledModes;
                mode.inverseModalMass = 0.0f;
            }
        }

        float actualLeft = 0.0f;
        float actualRight = 0.0f;
        float referenceLeft = 0.0f;
        float referenceRight = 0.0f;
        actual.process (&actualLeft, &actualRight, 1);
        reference.process (&referenceLeft, &referenceRight, 1);

        const double actualForce = actual.voices_[0].solvedContactForce;
        const double referenceForce = reference.voices_[0].solvedContactForce;
        result.forceDifference = std::abs (actualForce - referenceForce);
        result.finite = std::isfinite (actualForce)
                     && std::isfinite (referenceForce)
                     && std::isfinite (actualLeft) && std::isfinite (actualRight)
                     && std::isfinite (referenceLeft) && std::isfinite (referenceRight);
        return result;
    }

    static NonlinearContactAudit nonlinearContactAudit (
        double sampleRate, Articulation articulation = Articulation::Don,
        float velocity = 1.0f) noexcept
    {
        TaikoEngine engine;
        EngineParameters parameters;
        parameters.humanise = 0.0f;
        parameters.strikeNoise = 0.0f;
        parameters.tensionModulation = 0.0f;
        parameters.drive = 0.0f;
        parameters.outputGain = 0.01f;
        engine.setParameters (parameters);
        engine.prepare (sampleRate, 1);
        engine.trigger (articulation, 0, velocity);

        auto& strike = engine.voices_[0];
        const double incomingVelocity =
            (strike.stickPosition - strike.stickPrevious) * sampleRate;
        const double initialEnergy = 0.5 * strike.stickMass
                                   * incomingVelocity * incomingVelocity;
        const double referenceExposure = strike.referenceContactExposure;

        const auto halfStepEnergy = [&engine, sampleRate] (
                                        const TaikoEngine::Voice& contact)
        {
            constexpr double stateScale = 292.0;
            const double h = 1.0 / sampleRate;
            const auto square = [] (double value) { return value * value; };
            const auto& physical = physicalForOctave (engine);
            double energy = 0.5 * contact.stickMass * square (
                (contact.stickPosition - contact.stickPrevious) / h);

            double currentCompression = contact.stickPosition;
            double previousCompression = contact.stickPrevious;
            for (int index = 0; index < physical.modeCount; ++index)
            {
                const auto& mode = physical.modes[static_cast<std::size_t> (index)];
                if (! mode.membrane || ! (mode.inverseModalMass > 0.0f))
                    continue;

                const double mass = 1.0 / mode.inverseModalMass;
                const double current = mode.resonator.y1 / stateScale;
                const double previous = mode.resonator.y2 / stateScale;
                const double denominator = 1.0 + mode.resonator.a2
                                         - mode.resonator.a1;
                const double matchedStiffness = 4.0 * mass / (h * h)
                    * (1.0 + mode.resonator.a2 + mode.resonator.a1)
                    / denominator;
                energy += 0.5 * mass * square ((current - previous) / h)
                        + 0.5 * matchedStiffness
                              * square (0.5 * (current + previous));

                const auto id = static_cast<std::size_t> (mode.physicalIndex);
                currentCompression -= contact.contactProjection[id] * current;
                previousCompression -= contact.contactProjection[id] * previous;
            }

            if (contact.nonlinearContactActive)
            {
                const double compression = std::max (
                    0.5 * (currentCompression + previousCompression),
                    0.0);
                energy += contact.contactStiffness / 2.5
                        * compression * compression * std::sqrt (compression);
            }
            return energy;
        };

        NonlinearContactAudit result;
        result.minimumForce = std::numeric_limits<double>::infinity();
        result.minimumExposureStep = std::numeric_limits<double>::infinity();
        double previousEnergy = halfStepEnergy (strike);
        result.maximumEnergyRatio = initialEnergy > 0.0
            ? previousEnergy / initialEnergy : 0.0;
        int forceSamples = 0;
        double deliveredExposure = 0.0;
        const int samples = static_cast<int> (std::ceil (0.03 * sampleRate));
        for (int sample = 0; sample < samples; ++sample)
        {
            float left = 0.0f;
            float right = 0.0f;
            engine.process (&left, &right, 1);
            const double force = strike.solvedContactForce;
            const double exposure = strike.solvedContactExposureStep;
            result.minimumForce = std::min (result.minimumForce, force);
            result.minimumExposureStep = std::min (
                result.minimumExposureStep, exposure);
            result.peakForce = std::max (result.peakForce, force);
            result.impulse += force / sampleRate;
            deliveredExposure += exposure;
            result.stereoEnergy += static_cast<double> (left) * left
                                 + static_cast<double> (right) * right;
            if (force > 0.0)
                ++forceSamples;
            const double energy = halfStepEnergy (strike);
            if (initialEnergy > 0.0)
                result.maximumEnergyRatio = std::max (
                    result.maximumEnergyRatio, energy / initialEnergy);
            result.maximumEnergyIncrease = std::max (
                result.maximumEnergyIncrease, energy - previousEnergy);
            previousEnergy = energy;
            result.finite = result.finite && std::isfinite (force)
                         && std::isfinite (exposure)
                         && std::isfinite (strike.remainingContactExposure)
                         && std::isfinite (left) && std::isfinite (right)
                         && std::isfinite (strike.stickPosition)
                         && std::isfinite (strike.stickPrevious);
        }

        result.durationSeconds = static_cast<double> (forceSamples) / sampleRate;
        result.released = ! strike.nonlinearContactActive
                       && strike.nonlinearContactHasForce;

        const double stickVelocity =
            (strike.stickPosition - strike.stickPrevious) * sampleRate;
        const double stickMomentumChange = strike.stickMass
                                         * (incomingVelocity - stickVelocity);
        result.impulseMomentumClosure = std::abs (
            result.impulse - stickMomentumChange)
            / std::max ({ std::abs (result.impulse),
                          std::abs (stickMomentumChange), 1.0e-12 });
        result.normalizedResidualExposure = referenceExposure > 0.0
            ? deliveredExposure / referenceExposure : 0.0;
        double finalEnergy = 0.5 * strike.stickMass * stickVelocity * stickVelocity;
        const auto& physical = physicalForOctave (engine);
        for (int index = 0; index < physical.modeCount; ++index)
        {
            const auto& mode = physical.modes[static_cast<std::size_t> (index)];
            if (! mode.membrane || ! (mode.inverseModalMass > 0.0f))
                continue;
            const double mass = 1.0 / mode.inverseModalMass;
            const double radius = mode.poleRadius;
            const double sine = mode.resonator.b0;
            double velocity = (mode.resonator.y1 - mode.resonator.y2) * sampleRate;
            if (radius > 0.0 && std::abs (sine) > 1.0e-12
                && mode.liveOmega > 0.0)
            {
                const double cosine = -mode.resonator.a1 / (2.0 * radius);
                const double quadrature =
                    (mode.resonator.y1 * cosine - radius * mode.resonator.y2) / sine;
                velocity = mode.liveOmega * quadrature
                         - static_cast<double> (mode.decayRate) * mode.resonator.y1;
            }
            const double displacement = mode.resonator.y1 / 292.0;
            velocity /= 292.0;
            const double naturalSquared = mode.liveOmega * mode.liveOmega
                                        + static_cast<double> (mode.decayRate)
                                          * mode.decayRate;
            finalEnergy += 0.5 * mass
                         * (velocity * velocity + naturalSquared
                                                   * displacement * displacement);
        }
        result.finalEnergyRatio = initialEnergy > 0.0 ? finalEnergy / initialEnergy : 0.0;
        if (! std::isfinite (result.minimumForce))
            result.minimumForce = 0.0;
        if (! std::isfinite (result.minimumExposureStep))
            result.minimumExposureStep = 0.0;
        result.finite = result.finite && std::isfinite (result.finalEnergyRatio)
                     && std::isfinite (result.maximumEnergyRatio)
                     && std::isfinite (result.maximumEnergyIncrease)
                     && std::isfinite (result.normalizedResidualExposure)
                     && std::isfinite (result.impulseMomentumClosure)
                     && std::isfinite (result.stereoEnergy);
        return result;
    }

    struct LegacyResidualExposureAudit
    {
        double deliveredFraction { 0.0 };
        double requestedFraction { 0.0 };
        double limitToReferenceRatio { 0.0 };
        double closureError { 0.0 };
        bool injected { false };
        bool released { false };
        bool finite { true };
    };

    static LegacyResidualExposureAudit legacyResidualExposureAudit (
        double sampleRate, int octave, Articulation articulation,
        float velocity = 0.9f) noexcept
    {
        TaikoEngine engine;
        EngineParameters parameters;
        parameters.humanise = 0.0f;
        parameters.tensionModulation = 0.0f;
        parameters.drive = 0.0f;
        parameters.outputGain = 0.0f;
        engine.setParameters (parameters);
        engine.prepare (sampleRate, 1);
        engine.trigger (articulation, octave, velocity);

        auto& strike = engine.voices_[0];
        const double reference = strike.referenceContactExposure;
        const double limit = strike.remainingContactExposure;
        double delivered = 0.0;
        double requested = 0.0;
        const double step = 1.0 / sampleRate;
        const int samples = static_cast<int> (std::ceil (0.05 * sampleRate));
        for (int sample = 0; sample < samples; ++sample)
        {
            float left = 0.0f;
            float right = 0.0f;
            engine.process (&left, &right, 1);
            delivered += strike.solvedContactExposureStep;
            requested += step * strike.solvedContactForce
                               * strike.solvedContactForce
                               * strike.contactExposureAdmittance;

            if (! strike.nonlinearContactActive
                && strike.nonlinearContactHasForce)
                break;
        }

        LegacyResidualExposureAudit result;
        result.released = ! strike.nonlinearContactActive
                       && strike.nonlinearContactHasForce;
        result.injected = strike.continuumInjected;
        if (limit > 0.0)
        {
            result.deliveredFraction = delivered / limit;
            result.requestedFraction = requested / limit;
            result.closureError = std::abs (
                delivered + strike.remainingContactExposure - limit)
                                  / limit;
        }
        if (reference > 0.0)
            result.limitToReferenceRatio = limit / reference;
        result.finite = std::isfinite (result.deliveredFraction)
                     && std::isfinite (result.requestedFraction)
                     && std::isfinite (result.limitToReferenceRatio)
                     && std::isfinite (result.closureError)
                     && std::isfinite (strike.remainingContactExposure);
        return result;
    }

    struct LegacyResidualExposureLifecycleAudit
    {
        bool saturatedWhileActive { false };
        bool distinctOverlappingSlots { false };
        bool secondStartedFull { false };
        bool firstStayedSpent { false };
        bool secondSpentIndependently { false };
        bool firstSlotRetired { false };
        bool reusedFirstSlot { false };
        bool reuseResetBudget { false };
    };

    static LegacyResidualExposureLifecycleAudit
    legacyResidualExposureLifecycleAudit() noexcept
    {
        constexpr double sampleRate = 48000.0;
        TaikoEngine engine;
        EngineParameters parameters;
        parameters.humanise = 0.0f;
        parameters.tensionModulation = 0.0f;
        parameters.drive = 0.0f;
        parameters.outputGain = 0.0f;
        engine.setParameters (parameters);
        engine.prepare (sampleRate, 1);
        engine.trigger (Articulation::Don, 3, 0.9f);

        const int firstSlot = newestStrikeSlot (engine);
        auto& first = engine.voices_[static_cast<std::size_t> (firstSlot)];
        for (int sample = 0;
             sample < static_cast<int> (0.02 * sampleRate)
                 && first.remainingContactExposure > 0.0
                 && first.nonlinearContactActive;
             ++sample)
        {
            float left = 0.0f;
            float right = 0.0f;
            engine.process (&left, &right, 1);
        }

        LegacyResidualExposureLifecycleAudit result;
        result.saturatedWhileActive = first.remainingContactExposure == 0.0
                                   && first.nonlinearContactActive;
        const double spentFirst = first.remainingContactExposure;

        engine.trigger (Articulation::Don, 0, 0.9f);
        const int secondSlot = newestStrikeSlot (engine);
        auto& second = engine.voices_[static_cast<std::size_t> (secondSlot)];
        result.distinctOverlappingSlots = secondSlot != firstSlot;
        result.secondStartedFull = second.remainingContactExposure > 0.0
                                && second.solvedContactExposureStep == 0.0;
        result.firstStayedSpent = first.remainingContactExposure == spentFirst;

        const double secondBefore = second.remainingContactExposure;
        float left = 0.0f;
        float right = 0.0f;
        for (int sample = 0;
             sample < 8 && second.solvedContactExposureStep == 0.0;
             ++sample)
            engine.process (&left, &right, 1);
        result.firstStayedSpent = result.firstStayedSpent
                              && first.remainingContactExposure == spentFirst;
        result.secondSpentIndependently = second.solvedContactExposureStep > 0.0
                                       && second.remainingContactExposure
                                              < secondBefore;

        for (int sample = 0; sample < static_cast<int> (0.20 * sampleRate); ++sample)
            engine.process (&left, &right, 1);
        result.firstSlotRetired = ! first.active;

        engine.trigger (Articulation::Ka, 0, 0.7f);
        const int reusedSlot = newestStrikeSlot (engine);
        const auto& reused = engine.voices_[static_cast<std::size_t> (reusedSlot)];
        result.reusedFirstSlot = reusedSlot == firstSlot;
        result.reuseResetBudget = reused.remainingContactExposure > 0.0
                               && reused.solvedContactExposureStep == 0.0;
        return result;
    }

    static double hertzReferenceImpulseRatio (double pulseIntegral) noexcept
    {
        constexpr double pi = 3.1415926535897932384626433832795;
        EngineParameters parameters;
        const auto drum = TaikoEngine::resolveDrumFor (parameters, 0.0f, 1);
        const auto& profile = TaikoEngine::strikeProfile (Articulation::Don);
        float strikerMass = 0.0f;
        float impedance = 0.0f;
        TaikoEngine::drumContactTerms (drum, strikerMass, impedance);
        const float radius = TaikoEngine::strikeRadiusFor (
            profile, parameters.strikePosition);
        const float collisionMass = TaikoEngine::contactCollisionMass (
            drum, profile, radius, strikerMass);
        constexpr float speed = 2.7f;
        float contactSeconds = 0.0f;
        float peakForce = 0.0f;
        TaikoEngine::solveContact (
            collisionMass, impedance, profile, parameters.bachiHardness, speed,
            contactSeconds, peakForce);
        const double reconstructedImpulse = static_cast<double> (peakForce)
                                          * contactSeconds * pulseIntegral
                                          / pi;
        const double collisionImpulse = static_cast<double> (collisionMass)
                                      * speed * 1.42;
        return reconstructedImpulse / collisionImpulse;
    }

    // Two absence findings, each recomputed from the resolved drum rather than
    // pinned as a literal, so a change that made either mechanism matter would
    // be caught instead of silently invalidating what the README says is left
    // out. See the 2026-08-19 adjudication entry in the README's release
    // history.

    // The shell's per-ring-mode distance gain at a given Mic Distance control
    // value, relative to this drum's own factory position.
    static std::array<float, TaikoEngine::shellResonatorCount> shellPerspective (
        int octave, float micDistanceControl) noexcept
    {
        EngineParameters raw;
        raw.micDistance = micDistanceControl;
        const auto parameters = TaikoEngine::sanitise (raw);
        const auto drum = TaikoEngine::resolveDrumFor (parameters, 0.0f, octave);
        std::array<float, TaikoEngine::shellResonatorCount> gains {};
        for (int index = 0; index < TaikoEngine::shellResonatorCount; ++index)
        {
            const auto slot = static_cast<std::size_t> (index);
            const float frequency = drum.shellFrequencies[slot];
            const float omega = 2.0f * 3.14159265358979f * frequency;
            gains[slot] = TaikoEngine::shellPerspectiveGain (
                drum, index + 2, omega, frequency, drum.micDistanceMetres);
        }
        return gains;
    }

    struct CavityLossProbe
    {
        // The largest share of any axisymmetric branch's decay that the
        // enclosed air's own thermoviscous loss would add if it were modelled.
        double worstFractionOfDecay { 0.0 };
        double worstLossFactor { 0.0 };
        double worstCavityStiffnessShare { 0.0 };
        double worstBranchHz { 0.0 };
        double worstT60Seconds { 0.0 };
        double worstT60WithLossSeconds { 0.0 };
        // The spread in the (0,1) breathing branch's tail across Body Depth on
        // the factory drum, as a ratio of longest to shortest.
        double breathingTailSpread { 1.0 };
        int branchesScanned { 0 };
    };

    // Kirchhoff's boundary-layer result for an enclosed gas: the wall thermal
    // exchange gives the cavity's compliance a loss factor
    //   eta = (gamma - 1) * delta_t * S / (2 V),   delta_t = sqrt(2 alpha / w)
    // with alpha the thermal diffusivity of air. Nothing here is fitted; S and
    // V come from the drum the controls resolve to. The viscous half is
    // omitted because a uniformly compressed cavity has almost no tangential
    // wall velocity for it to act on, and its own boundary layer is thinner
    // than the thermal one.
    static double cavityLossFactor (const TaikoEngine::DrumState& drum,
                                    double omega) noexcept
    {
        constexpr double gammaAir = 1.4;
        constexpr double thermalDiffusivity = 2.14e-5;   // m^2/s, air at 20 C
        constexpr double pi = 3.1415926535897932384626433832795;
        const double radius = drum.radius;
        const double length = drum.depth;
        if (! (radius > 0.0) || ! (length > 0.0) || ! (omega > 0.0))
            return 0.0;
        const double surface = 2.0 * pi * radius * length
                             + 2.0 * pi * radius * radius;
        const double volume = pi * radius * radius * length;
        const double layer = std::sqrt (2.0 * thermalDiffusivity / omega);
        return (gammaAir - 1.0) * layer * surface / (2.0 * volume);
    }

    // The decay the renderer actually builds for one axisymmetric branch.
    static double builtAxisymmetricDecay (const TaikoEngine::DrumState& drum,
                                          int entryIndex, int branch) noexcept
    {
        TaikoEngine engine;
        engine.prepare (48000.0, 64);
        TaikoEngine::Voice voice;
        voice.physicalBank = true;
        voice.strikeRadius = TaikoEngine::strikeRadiusFor (
            TaikoEngine::strikeProfile (Articulation::Don), 0.0f);
        voice.strikeAngle = 0.0f;
        engine.buildVoiceModes (
            voice, drum, TaikoEngine::strikeProfile (Articulation::Don), 0.0f,
            false);
        for (int index = 0; index < voice.modeCount; ++index)
        {
            const auto& mode = voice.modes[static_cast<std::size_t> (index)];
            if (mode.membrane && mode.modeEntry == entryIndex
                && mode.physicalIndex == 2 * entryIndex + branch)
                return mode.decayRate;
        }
        return 0.0;
    }

    static CavityLossProbe probeCavityLoss() noexcept
    {
        CavityLossProbe result;
        const auto& entries = TaikoEngine::membraneModes();

        for (int octave = 0; octave < drumCount; ++octave)
        {
            for (int depthStep = 0; depthStep <= 10; ++depthStep)
            {
                for (int couplingStep = 0; couplingStep <= 2; ++couplingStep)
                {
                    EngineParameters raw;
                    raw.bodyDepth = 0.1f * static_cast<float> (depthStep);
                    raw.cavityCoupling = 0.5f * static_cast<float> (couplingStep);
                    const auto parameters = TaikoEngine::sanitise (raw);
                    const auto drum =
                        TaikoEngine::resolveDrumFor (parameters, 0.0f, octave);

                    for (int entry = 0; entry < TaikoEngine::axisymmetricEntryCount;
                         ++entry)
                    {
                        const auto slot = static_cast<std::size_t> (entry);
                        const double lambda = entries[slot].besselZero;
                        const auto omegas = TaikoEngine::membraneModeOmegas (
                            drum, drum.radius, static_cast<float> (lambda), 0.0f);
                        const TaikoEngine::FundamentalPair fundamentals {
                            static_cast<float> (lambda), omegas.batter,
                            omegas.resonant
                        };
                        float diagonalB = 0.0f;
                        float diagonalR = 0.0f;
                        float offDiagonal = 0.0f;
                        TaikoEngine::axisymmetricDiagonals (
                            drum, fundamentals, drum.cavityStiffnesses[slot],
                            diagonalB, diagonalR, offDiagonal);
                        // The cavity's contribution to the symmetrised stiffness
                        // is a rank-one c*v*v^T, so an eigenvector's share of it
                        // is exact rather than apportioned.
                        const double cavity = drum.cavityStiffnesses[slot] * 4.0
                                            / (lambda * lambda);
                        const double vectorB =
                            1.0 / std::sqrt (static_cast<double> (drum.batterDensity));
                        const double vectorR =
                            1.0 / std::sqrt (static_cast<double> (drum.resonantDensity));

                        for (int branch = 0; branch < 2; ++branch)
                        {
                            float eigenvalue = 0.0f;
                            float componentB = 0.0f;
                            float componentR = 0.0f;
                            TaikoEngine::solveAxisymmetricBranch (
                                diagonalB, diagonalR, offDiagonal, branch,
                                eigenvalue, componentB, componentR);
                            if (! (eigenvalue > 0.0f))
                                continue;
                            const double omega = std::sqrt (
                                static_cast<double> (eigenvalue));
                            constexpr double pi =
                                3.1415926535897932384626433832795;
                            const double hz = omega / (2.0 * pi);
                            if (hz < 5.0 || hz > 20000.0)
                                continue;
                            const double decay =
                                builtAxisymmetricDecay (drum, entry, branch);
                            if (! (decay > 0.0))
                                continue;
                            ++result.branchesScanned;

                            const double projection = componentB * vectorB
                                                    + componentR * vectorR;
                            const double share = cavity * projection * projection
                                               / static_cast<double> (eigenvalue);
                            const double lossFactor = cavityLossFactor (drum, omega);
                            // A lossy spring adds half its loss factor times
                            // omega to the amplitude decay, weighted by the
                            // share of the modal stiffness it is.
                            const double added = 0.5 * lossFactor * share * omega;
                            const double fraction = added / decay;
                            if (fraction > result.worstFractionOfDecay)
                            {
                                result.worstFractionOfDecay = fraction;
                                result.worstLossFactor = lossFactor;
                                result.worstCavityStiffnessShare = share;
                                result.worstBranchHz = hz;
                                result.worstT60Seconds = 6.907755 / decay;
                                result.worstT60WithLossSeconds =
                                    6.907755 / (decay + added);
                            }
                        }
                    }
                }
            }
        }

        // Body Depth has authority over the tail today, through the frequency
        // dependence of the mounting, radiation and hide losses rather than
        // through any loss of the air's own.
        double shortest = std::numeric_limits<double>::max();
        double longest = 0.0;
        for (int depthStep = 0; depthStep <= 10; ++depthStep)
        {
            EngineParameters raw;
            raw.bodyDepth = 0.1f * static_cast<float> (depthStep);
            const auto parameters = TaikoEngine::sanitise (raw);
            const auto drum = TaikoEngine::resolveDrumFor (parameters, 0.0f, 0);
            const double decay = builtAxisymmetricDecay (drum, 0, 0);
            if (! (decay > 0.0))
                continue;
            const double tail = 6.907755 / decay;
            shortest = std::min (shortest, tail);
            longest = std::max (longest, tail);
        }
        if (shortest > 0.0 && shortest < std::numeric_limits<double>::max())
            result.breathingTailSpread = longest / shortest;
        return result;
    }

    struct ContactPatchProbe
    {
        // Worst spatial low-pass, in decibels, that a finite Hertz contact
        // patch would impose on the highest resolved membrane mode.
        double worstFamilyDb { 0.0 };
        double worstReachableDb { 0.0 };
        // Patch radius as a fraction of the assumed tip radius, at the same
        // corner - the Hertz solution's own small-deformation limit.
        double worstFamilyPatchFraction { 0.0 };
        // How much the family answer moves when the assumed tip radius halves.
        double familyDbAtHalfTip { 0.0 };
    };

    // 2 J1(x)/x, the spatial transfer of a uniformly loaded circular patch of
    // radius a_c onto a mode of spatial wavenumber k, with x = k a_c.
    static double patchSpatialFilterDb (double x) noexcept
    {
        if (x < 1.0e-9)
            return 0.0;
        const double transfer = 2.0 * TaikoEngine::besselJ (1, x) / x;
        return 20.0 * std::log10 (std::max (transfer, 1.0e-12));
    }

    // a_c = sqrt(R_tip) * (F / K)^(1/3) follows from the Hertz pair
    // K = (4/3) E* sqrt(R_tip) and a_c^3 = 3 F R_tip / (4 E*). The engine
    // carries only the product K, so R_tip has to be supplied here - which is
    // the finding, not an oversight of this probe.
    static double contactPatchRadius (const TaikoEngine::DrumState& drum,
                                      const TaikoEngine::StrikeProfile& profile,
                                      float bachiHardness, float impactSpeed,
                                      double tipRadiusMetres) noexcept
    {
        float strikerMass = 0.0f;
        float impedance = 0.0f;
        TaikoEngine::drumContactTerms (drum, strikerMass, impedance);
        const float radius = TaikoEngine::strikeRadiusFor (profile, 0.0f);
        const float collisionMass = TaikoEngine::contactCollisionMass (
            drum, profile, radius, strikerMass);
        float contactSeconds = 0.0f;
        float peakForce = 0.0f;
        TaikoEngine::solveContact (collisionMass, impedance, profile,
                                   bachiHardness, impactSpeed, contactSeconds,
                                   peakForce);
        const double stiffness =
            TaikoEngine::contactStiffnessFor (profile, bachiHardness);
        return std::sqrt (tipRadiusMetres)
             * std::cbrt (static_cast<double> (peakForce) / stiffness);
    }

    static double highestResolvedWavenumber (const TaikoEngine::DrumState& drum) noexcept
    {
        const auto& entries = TaikoEngine::membraneModes();
        double largest = 0.0;
        for (int index = 0; index < TaikoEngine::modeEntryCount; ++index)
            largest = std::max (
                largest, entries[static_cast<std::size_t> (index)].besselZero);
        return largest / static_cast<double> (drum.radius);
    }

    // Worst attenuation over a scan. `familyOnly` restricts it to the factory
    // controls, which is the instrument the product actually is; the full scan
    // also reaches head diameters no taiko has.
    static double worstPatchAttenuationDb (bool familyOnly, double tipRadiusMetres,
                                           double& patchFractionOfTip) noexcept
    {
        constexpr float fullVelocitySpeed = 6.0f;   // maximumImpactSpeed
        double worst = 0.0;
        patchFractionOfTip = 0.0;
        for (int octave = 0; octave < drumCount; ++octave)
        {
            for (int hardnessStep = 0; hardnessStep <= 10; ++hardnessStep)
            {
                if (familyOnly && hardnessStep != 7)
                    continue;
                for (int diameterStep = 0; diameterStep <= 4; ++diameterStep)
                {
                    if (familyOnly && diameterStep != 4)
                        continue;
                    EngineParameters raw;
                    raw.bachiHardness = 0.1f * static_cast<float> (hardnessStep);
                    raw.headDiameter =
                        0.15f + 0.3375f * static_cast<float> (diameterStep);
                    const auto parameters = TaikoEngine::sanitise (raw);
                    const auto drum =
                        TaikoEngine::resolveDrumFor (parameters, 0.0f, octave);
                    const double wavenumber = highestResolvedWavenumber (drum);
                    for (std::size_t index = 0; index < articulationCount; ++index)
                    {
                        const auto& profile = TaikoEngine::strikeProfile (
                            static_cast<Articulation> (index));
                        const double patch = contactPatchRadius (
                            drum, profile, parameters.bachiHardness,
                            fullVelocitySpeed, tipRadiusMetres);
                        const double attenuation =
                            -patchSpatialFilterDb (wavenumber * patch);
                        if (attenuation > worst)
                        {
                            worst = attenuation;
                            patchFractionOfTip = patch / tipRadiusMetres;
                        }
                    }
                }
            }
        }
        return worst;
    }

    static ContactPatchProbe probeContactPatch (double tipRadiusMetres) noexcept
    {
        ContactPatchProbe result;
        double reachableFraction = 0.0;
        result.worstFamilyDb = worstPatchAttenuationDb (
            true, tipRadiusMetres, result.worstFamilyPatchFraction);
        double ignored = 0.0;
        result.familyDbAtHalfTip = worstPatchAttenuationDb (
            true, 0.5 * tipRadiusMetres, ignored);
        result.worstReachableDb = worstPatchAttenuationDb (
            false, tipRadiusMetres, reachableFraction);
        return result;
    }
};
} // namespace taikor

namespace
{
constexpr int defaultBlockSize = 253;
constexpr double analysisPi = 3.1415926535897932384626433832795;
int failureCount = 0;

void expect (bool condition, const std::string& message)
{
    if (! condition)
    {
        ++failureCount;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct Rendered
{
    std::vector<float> left;
    std::vector<float> right;
    double peak { 0.0 };
    double rms { 0.0 };
    bool finite { true };

    [[nodiscard]] std::vector<float> mono() const
    {
        std::vector<float> result (left.size());
        for (std::size_t index = 0; index < left.size(); ++index)
            result[index] = 0.5f * (left[index] + right[index]);
        return result;
    }
};

Rendered render (taikor::TaikoEngine& engine, int numSamples,
                 int blockSize = defaultBlockSize)
{
    Rendered result;
    result.left.resize (static_cast<std::size_t> (numSamples));
    result.right.resize (static_cast<std::size_t> (numSamples));

    std::vector<float> blockLeft (static_cast<std::size_t> (blockSize));
    std::vector<float> blockRight (static_cast<std::size_t> (blockSize));

    double sumOfSquares = 0.0;
    for (int rendered = 0; rendered < numSamples;)
    {
        const int count = std::min (blockSize, numSamples - rendered);
        engine.process (blockLeft.data(), blockRight.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto index = static_cast<std::size_t> (rendered + sample);
            const float l = blockLeft[static_cast<std::size_t> (sample)];
            const float r = blockRight[static_cast<std::size_t> (sample)];
            result.left[index] = l;
            result.right[index] = r;
            result.finite = result.finite && std::isfinite (l) && std::isfinite (r);
            if (std::isfinite (l) && std::isfinite (r))
            {
                result.peak = std::max ({ result.peak, std::abs (static_cast<double> (l)),
                                          std::abs (static_cast<double> (r)) });
                sumOfSquares += static_cast<double> (l) * l + static_cast<double> (r) * r;
            }
        }
        rendered += count;
    }

    result.rms = numSamples > 0
        ? std::sqrt (sumOfSquares / (2.0 * static_cast<double> (numSamples)))
        : 0.0;
    return result;
}

// Magnitude of one frequency bin, by the Goertzel recurrence. Used instead of a
// full transform because every question in this suite is about a handful of
// modes whose predicted frequencies the engine itself reports.
double binMagnitude (const std::vector<float>& samples, double frequencyHz,
                     double sampleRate, std::size_t first = 0,
                     std::size_t last = std::numeric_limits<std::size_t>::max())
{
    const double omega = 2.0 * analysisPi * frequencyHz / sampleRate;
    const double coefficient = 2.0 * std::cos (omega);
    double s1 = 0.0;
    double s2 = 0.0;

    const auto end = std::min (last, samples.size());
    for (std::size_t index = first; index < end; ++index)
    {
        const double s0 = static_cast<double> (samples[index]) + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - coefficient * s1 * s2));
}

// The strongest partial in a band, found by scanning. Deliberately independent
// of what the engine predicts, so a test can compare the two.
double dominantFrequency (const std::vector<float>& samples, double sampleRate,
                          double lowHz, double highHz, double stepHz,
                          std::size_t first = 0,
                          std::size_t last = std::numeric_limits<std::size_t>::max())
{
    double best = -1.0;
    double bestFrequency = lowHz;
    for (double frequency = lowHz; frequency <= highHz; frequency += stepHz)
    {
        const double magnitude =
            binMagnitude (samples, frequency, sampleRate, first, last);
        if (magnitude > best)
        {
            best = magnitude;
            bestFrequency = frequency;
        }
    }
    return bestFrequency;
}

double correlation (const std::vector<float>& a, const std::vector<float>& b)
{
    double sumAB = 0.0;
    double sumAA = 0.0;
    double sumBB = 0.0;
    const auto count = std::min (a.size(), b.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        sumAB += static_cast<double> (a[index]) * b[index];
        sumAA += static_cast<double> (a[index]) * a[index];
        sumBB += static_cast<double> (b[index]) * b[index];
    }
    if (sumAA <= 0.0 || sumBB <= 0.0)
        return 1.0;
    return sumAB / std::sqrt (sumAA * sumBB);
}

// Mean-square level in a band, in decibels, taken by Parseval over the integer
// DFT bins of the whole buffer.
//
// This is the one estimator in the suite that means the same thing at two
// different sample rates, and it is used where a test has to compare them. A
// time-domain root-mean-square through a filter of fixed corner does not: the
// filter is designed per rate, its transition region moves, and what it lets
// through is not the same slice of the spectrum twice. Integer bins of a window
// of fixed duration are the same slice by construction - the bin spacing is one
// over that duration whatever the clock is doing - so the only difference left
// between two rates is the audio.
double bandLevelDb (const std::vector<float>& samples, double sampleRate,
                    double lowHz, double highHz)
{
    const auto count = static_cast<int> (samples.size());
    if (count < 4)
        return -300.0;

    const double spacing = sampleRate / static_cast<double> (count);
    const int firstBin = std::max (1, static_cast<int> (std::ceil (lowHz / spacing)));
    const int lastBin =
        std::min (count / 2, static_cast<int> (std::floor (highHz / spacing)));

    double total = 0.0;
    for (int bin = firstBin; bin <= lastBin; ++bin)
    {
        // Both halves of the spectrum, so the sum is the band's share of the
        // buffer's energy rather than half of it.
        const double magnitude = binMagnitude (
            samples, static_cast<double> (bin) * spacing, sampleRate);
        total += 2.0 * magnitude * magnitude;
    }

    const double meanSquare =
        total / (static_cast<double> (count) * static_cast<double> (count));
    return 10.0 * std::log10 (std::max (meanSquare, 1.0e-30));
}

// The same quantity with a Hann window laid over the buffer first, so that a
// quiet band next to a loud one reads what is in it rather than the sidelobes
// of what is below it. It is the same slice of the spectrum at every sample
// rate for the same reason bandLevelDb is - the bins are one over the window's
// duration - and it is the only version of that comparison worth making about a
// band the drum has almost nothing in.
//
// Both exist because they answer different questions. bandLevelDb is what the
// pass's literals were taken with and is kept so those literals still mean
// something; this is what a change of a decibel in one of those literals has to
// be checked against before it is believed.
double bandLevelHannDb (std::vector<float> samples, double sampleRate,
                        double lowHz, double highHz)
{
    const auto count = static_cast<int> (samples.size());
    if (count < 4)
        return -300.0;

    for (int index = 0; index < count; ++index)
        samples[static_cast<std::size_t> (index)] *= static_cast<float> (
            0.5 - 0.5 * std::cos (2.0 * analysisPi * static_cast<double> (index)
                                  / static_cast<double> (count - 1)));

    // Three halves of the window's mean square, which is what a Hann window
    // takes out of a broadband signal's power.
    return bandLevelDb (samples, sampleRate, lowHz, highHz)
         + 10.0 * std::log10 (8.0 / 3.0);
}

// Root-mean-square of everything above a corner, over a window, with the filter
// run from the first sample so that it is settled long before the window opens.
//
// This exists because `bandLevelDb` cannot answer a question about a quiet high
// band sitting next to a loud low one. Its window is rectangular, and a
// rectangular window's sidelobes fall only as one over the frequency offset, so
// one strong low partial that is not exactly on a bin leaks across the whole
// spectrum at a level that has nothing to do with what is there. Measured, and
// asserted in testTheGlideDoesNotBrightenTheTopOfTheSpectrum below: a single
// sinusoid at 1000.3 Hz reads -68.6 dB in the 4-10 kHz band through
// `bandLevelDb` and -164.2 dB through the same sum with a Hann window over it.
//
// Eight one-pole stages, so the skirt is 48 dB per octave and a band two octaves
// down is a hundred decibels away. Starting the filter inside the window instead
// would substitute its own start-up transient for the leakage and measure that.
double highPassedRms (const std::vector<float>& samples, double sampleRate,
                      double cornerHz, std::size_t first, std::size_t last)
{
    const double coefficient =
        1.0 - std::exp (-2.0 * analysisPi * cornerHz / sampleRate);
    std::array<double, 8> state {};

    double sum = 0.0;
    std::size_t count = 0;
    const auto end = std::min (last, samples.size());
    for (std::size_t index = 0; index < end; ++index)
    {
        double value = static_cast<double> (samples[index]);
        for (auto& stage : state)
        {
            stage += coefficient * (value - stage);
            value -= stage;
        }

        if (index >= first)
        {
            sum += value * value;
            ++count;
        }
    }
    return count > 0 ? std::sqrt (sum / static_cast<double> (count)) : 0.0;
}

// Root-mean-square over a window, so a test can ask about the settled tail
// rather than about an attack that swamps it.
double windowedRms (const std::vector<float>& samples, std::size_t first,
                    std::size_t last)
{
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t index = first; index < last && index < samples.size(); ++index)
    {
        sum += static_cast<double> (samples[index]) * samples[index];
        ++count;
    }
    return count > 0 ? std::sqrt (sum / static_cast<double> (count)) : 0.0;
}

double maximumAbsoluteDifference (const std::vector<float>& a,
                                  const std::vector<float>& b)
{
    double worst = 0.0;
    const auto count = std::min (a.size(), b.size());
    for (std::size_t index = 0; index < count; ++index)
        worst = std::max (worst,
                          std::abs (static_cast<double> (a[index]) - b[index]));
    return worst;
}

// Seconds until the signal's envelope has fallen `decibels` below its peak.
double decayTime (const std::vector<float>& samples, double sampleRate,
                  double decibels)
{
    double peak = 0.0;
    for (const float value : samples)
        peak = std::max (peak, std::abs (static_cast<double> (value)));
    if (peak <= 0.0)
        return 0.0;

    const double threshold = peak * std::pow (10.0, decibels / 20.0);
    for (std::size_t index = samples.size(); index-- > 0;)
        if (std::abs (static_cast<double> (samples[index])) > threshold)
            return static_cast<double> (index) / sampleRate;
    return 0.0;
}

taikor::EngineParameters defaultParameters()
{
    // Deliberately the shipping defaults, untouched. Overriding the output here
    // used to be harmless because it restated the engine's own default; once
    // the instrument grew loud enough to need a quieter one, the override left
    // the whole suite running sixteen decibels hot and measuring the limiter
    // rather than the model.
    return taikor::EngineParameters {};
}

// A single stroke, rendered from a clean engine.
Rendered strike (taikor::EngineParameters parameters, taikor::Articulation articulation,
                 int octaveOffset, float velocity, double sampleRate, int numSamples,
                 int blockSize = defaultBlockSize)
{
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.trigger (articulation, octaveOffset, velocity);
    return render (engine, numSamples, blockSize);
}

// A dry modal take for assertions about pitch. The continuum and contact
// texture are intentionally broadband; asking their strongest random bin to
// agree with a modal frequency turns a pitch test into a seed test.
Rendered resolvedStrike (taikor::EngineParameters parameters,
                         taikor::Articulation articulation, int octaveOffset,
                         float velocity, double sampleRate, int numSamples,
                         int blockSize = defaultBlockSize)
{
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (sampleRate, blockSize);
    engine.reset();
    engine.trigger (articulation, octaveOffset, velocity);
    taikor::TaikoEngineTestAccess::isolateResolvedBank (engine);
    return render (engine, numSamples, blockSize);
}

// ---------------------------------------------------------------------------

void testArticulationMetadataAndMidiMapping()
{
    std::set<std::string> names;
    std::set<std::string> slugs;

    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
    {
        const auto articulation = static_cast<taikor::Articulation> (index);
        const auto& metadata = taikor::getArticulationMetadata (articulation);

        expect (metadata.articulation == articulation,
                "articulation metadata is not self-consistent");
        expect (! metadata.displayName.empty(), "articulation is missing a name");
        expect (! metadata.slug.empty(), "articulation is missing a slug");
        expect (! metadata.description.empty(),
                "articulation is missing a description");
        expect (metadata.pitchClass == static_cast<int> (index),
                "articulation pitch class must equal its enumerator");

        names.insert (std::string (metadata.displayName));
        slugs.insert (std::string (metadata.slug));
    }

    expect (names.size() == taikor::articulationCount,
            "articulation display names are not unique");
    expect (slugs.size() == taikor::articulationCount,
            "articulation slugs are not unique");

    // getArticulationMetadata() clamps its own table index rather than
    // trusting a caller that reached it with something other than one of the
    // enum's own values - every call above only ever passed a valid
    // Articulation, so this is the first assertion on that fallback. An
    // out-of-range value falls back to slot zero (Don) instead of indexing
    // past the table.
    const auto& donMetadata =
        taikor::getArticulationMetadata (taikor::Articulation::Don);
    const auto& fromPastEnd = taikor::getArticulationMetadata (
        static_cast<taikor::Articulation> (taikor::articulationCount));
    expect (fromPastEnd.slug == donMetadata.slug,
            "getArticulationMetadata did not fall back on an index one past the table");
    const auto& fromFarOutOfRange =
        taikor::getArticulationMetadata (static_cast<taikor::Articulation> (255));
    expect (fromFarOutOfRange.slug == donMetadata.slug,
            "getArticulationMetadata did not fall back on a far out-of-range index");

    // The vocabulary sits inside one octave: each pitch class up to the last
    // stroke is a different stroke, the octave chooses the drum rather than the
    // stroke, and the pitch classes past the last stroke carry nothing. The
    // octave stays twelve semitones because that is what has to line up with
    // the keyboard, so the gap is real and has to be empty - casting a pitch
    // class straight to the enum would have let the top four keys of every
    // octave play a Don.
    for (int note = taikor::lowestPlayableNote; note <= taikor::highestPlayableNote;
         ++note)
    {
        const auto articulation = taikor::articulationForMidiNote (note);
        const auto octave = taikor::octaveOffsetForMidiNote (note);
        const bool carriesStroke =
            note % 12 < static_cast<int> (taikor::articulationCount);
        expect (articulation.has_value() == carriesStroke,
                "a pitch class carries a stroke exactly when one is defined for it");
        expect (octave.has_value(), "playable note produced no octave");
        if (! articulation.has_value() || ! octave.has_value())
            continue;

        expect (static_cast<int> (*articulation) == note % 12,
                "pitch class must select the stroke");
        expect (*octave >= taikor::lowestOctaveOffset
                    && *octave <= taikor::highestOctaveOffset,
                "octave offset left its declared range");
        expect (taikor::midiNoteFor (*articulation, *octave) == note,
                "note to articulation mapping does not round-trip");
    }

    for (const int note : { 0, 11, taikor::lowestPlayableNote - 1,
                            taikor::highestPlayableNote + 1, 126, 127 })
    {
        expect (! taikor::articulationForMidiNote (note).has_value(),
                "a note outside the playable range must not map to a stroke");
        expect (! taikor::octaveOffsetForMidiNote (note).has_value(),
                "a note outside the playable range must not map to an octave");
    }

    taikor::TaikoEngine engine;
    engine.setParameters (defaultParameters());
    engine.prepare (48000.0, defaultBlockSize);

    expect (! engine.triggerMidi (taikor::lowestPlayableNote - 1, 0.9f),
            "a note below the range must be rejected");
    expect (! engine.triggerMidi (taikor::highestPlayableNote + 1, 0.9f),
            "a note above the range must be rejected");
    expect (engine.triggerMidi (taikor::referenceNote, 0.9f),
            "the reference note must be playable");
}

// The instrument's central promise: within an octave the notes that carry a
// stroke are every stroke there is, and between octaves the drum's pitch rises.
void testOctavesRaisePitch()
{
    const auto parameters = defaultParameters();
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);

    float previousLoaded = 0.0f;
    float previousBreathing = 0.0f;
    float previousRadius = 1.0e9f;

    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto measurements = engine.measureDrum (octave);

        expect (measurements.loadedFundamentalHz > previousLoaded * 1.5f,
                "each octave must raise the sounding fundamental substantially");
        expect (measurements.breathingModeHz > previousBreathing,
                "each octave must raise the breathing mode");
        expect (measurements.radiusMetres < previousRadius,
                "with the default octave body, a higher octave must be a smaller drum");
        expect (measurements.breathingModeHz > measurements.loadedFundamentalHz,
                "the cavity must lift the volume-changing mode above the other one");
        expect (measurements.idealFundamentalHz > measurements.loadedFundamentalHz,
                "air loading must lower the membrane below its ideal frequency");

        previousLoaded = measurements.loadedFundamentalHz;
        previousBreathing = measurements.breathingModeHz;
        previousRadius = measurements.radiusMetres;
    }

    // An octave is an octave, and which quantity has to double is the whole of
    // step 5 of the second pass. It used to be asserted here, on the ideal
    // membrane fundamental, and that quantity is never audible on its own: the
    // air load hangs off the real mode and depends on rho_air a / sigma, which
    // does not scale with a transform that changes a. The octave transform now
    // solves for the amount of itself that doubles the *sounding* pitch, so the
    // ideal fundamental no longer doubles - at Octave Body 0.7 it steps 1.81 per
    // octave - and the clause has moved to testTheDrumIsTunedByThePitchItSounds,
    // where it is stated on the loaded fundamental and in cents.

    // And the rendered audio must actually follow the prediction. The search
    // is confined to the band the two membrane modes occupy, rather than
    // allowing unrelated higher head partials or the continuum to win a global
    // peak search. What has to be true is that the head sounds where the
    // physics says it does, and that it rises an octave at a time.
    //
    // Struck dead centre, because that is the only stroke whose partials are
    // all in the family being predicted. A full Don lands a hand's width in and
    // wakes the modes with a circumferential order, and the first of those sits
    // within a hertz of the breathing branch - so a stroke off centre puts two
    // different modes in one bin and asking which of them a peak belongs to is
    // not a question this test can answer. At radius zero J_m(0) = 0 kills all
    // of them and the axisymmetric pair is the whole of the drum.
    engine.setParameters (parameters);
    auto centred = parameters;
    centred.strikePosition = -1.0f;
    double previousDominant = 0.0;

    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto measurements = engine.measureDrum (octave);

        // The bottom of the range is a drum getting on for four metres across,
        // and the air inside a body that size splits its fundamental so far
        // that the lower branch lands at ten hertz - below hearing, and below
        // what a quarter-second window can resolve. There is no audible pitch
        // there to compare against the octave above it, and asserting one would
        // be asserting something about infrasound.
        if (measurements.loadedFundamentalHz < 20.0f)
            continue;

        const auto rendered =
            strike (centred, taikor::Articulation::Don, octave, 0.9f, 48000.0, 36000);
        const auto mono = rendered.mono();

        // Skip the attack so the measurement sees the drum ringing rather than
        // the stick landing on it, and stop before the lower head branch has
        // fallen beneath the analysis floor.
        // Twelve per cent either side of the fundamental, which holds the
        // lower branch and nothing else at any octave. Sweeping all the way up
        // to the breathing mode does not measure a pitch: the two branches are
        // driven differently and damped differently - the breathing one is the
        // radiator, so it is the one the mounting and the air take first - and
        // which of them is loudest changes with the drum. At the reference
        // octave the sweep found the lower branch and an octave up it found the
        // upper one, and reported the two as a fifth apart rather than an
        // octave.
        const auto dominant = dominantFrequency (
            mono, 48000.0, measurements.loadedFundamentalHz * 0.88,
            measurements.loadedFundamentalHz * 1.12, 0.25, 480u, 14400u);

        const auto nearFundamental =
            std::abs (dominant - measurements.loadedFundamentalHz)
                < std::max (2.0, measurements.loadedFundamentalHz * 0.06);
        const auto nearBreathing =
            std::abs (dominant - measurements.breathingModeHz)
                < std::max (2.0, measurements.breathingModeHz * 0.06);

        expect (nearFundamental || nearBreathing,
                "the rendered head does not sound at either predicted mode for octave "
                    + std::to_string (octave));
        expect (dominant > previousDominant * 1.5,
                "the rendered pitch must rise with the octave");
        previousDominant = dominant;
    }
}

// The playing grid is four drums by four strokes, and everything else on the
// keyboard is silent. Sixteen notes, C3 to D#6, and no accidents around the
// edges of them: a pitch class cast straight into the articulation enum would
// have let the eight keys above each drum play a Don, and an octave offset taken
// from a note below the range would have indexed the drum table off its front.
void testTheGridIsFourByFourAndTheRestIsSilent()
{
    expect (taikor::articulationCount == 4u, "the grid is four strokes wide");
    expect (taikor::drumCount == 4, "the grid is four drums deep");
    expect (taikor::lowestPlayableNote == 48 && taikor::highestPlayableNote == 95,
            "the grid occupies C3 to B6");

    // The stroke order the keyboard promises: C, C#, D, D#.
    const taikor::Articulation order[4] = {
        taikor::Articulation::Don, taikor::Articulation::Ka,
        taikor::Articulation::Tsu, taikor::Articulation::DonRim
    };
    for (int index = 0; index < 4; ++index)
        expect (static_cast<int> (order[index]) == index,
                "the bottom four semitones of an octave are Don, Ka, Tsu, Don Rim");

    // Every note on the keyboard, not only the playable ones. Sixteen of the
    // hundred and twenty-eight sound, they are exactly the ones the three
    // mapping functions agree on, and the rest are silent in the audio and not
    // merely absent from the map.
    int sounding = 0;
    for (int note = 0; note < 128; ++note)
    {
        const auto articulation = taikor::articulationForMidiNote (note);
        const auto octave = taikor::octaveOffsetForMidiNote (note);
        const bool inGrid = note >= taikor::lowestPlayableNote
                         && note <= taikor::highestPlayableNote
                         && note % 12 < 4;

        expect (articulation.has_value() == inGrid,
                "note " + std::to_string (note) + " is mapped to a stroke exactly "
                "when it is one of the sixteen");
        expect (octave.has_value() == (note >= taikor::lowestPlayableNote
                                       && note <= taikor::highestPlayableNote),
                "note " + std::to_string (note) + " disagrees with the octave map");

        if (articulation.has_value() && octave.has_value())
        {
            ++sounding;
            expect (*octave >= taikor::lowestOctaveOffset
                        && *octave <= taikor::highestOctaveOffset,
                    "note " + std::to_string (note) + " left the drum table");
            expect (taikor::midiNoteFor (*articulation, *octave) == note,
                    "note " + std::to_string (note) + " does not round-trip");
        }

        taikor::TaikoEngine engine;
        engine.setParameters (defaultParameters());
        engine.prepare (48000.0, defaultBlockSize);
        engine.reset();
        const bool accepted = engine.triggerMidi (note, 0.95f);
        expect (accepted == inGrid,
                "note " + std::to_string (note) + " was accepted when it is not "
                "part of the grid");

        const auto rendered = render (engine, 9600);
        if (inGrid)
            expect (rendered.peak > 1.0e-4,
                    "grid note " + std::to_string (note) + " produced no audio");
        else
            expect (rendered.peak == 0.0,
                    "note " + std::to_string (note) + " is off the grid and must "
                    "be exactly silent");
    }

    expect (sounding == 16, "the grid must have exactly sixteen playable notes");
}

// The four octaves are four instruments of the taiko family, not one drum at
// four sizes. That is the whole point of the change, so it is asserted in the
// form that a rescaling cannot satisfy: the drums have to be the sizes the
// family actually comes in, their heads have to be different hides, their bodies
// have to be different shapes, and the dimensionless quantities a listener hears
// - how far the enclosed air pushes the second axisymmetric branch above the
// first, how far the head's own stiffness opens the modal ratios out, how many
// cycles the drum rings for - have to be different numbers on each of them.
//
// A similarity transform of one drum into another preserves every one of those
// ratios by construction. So does the octave transform that used to make this
// keyboard, to the extent that it is one. An implementation that went back to
// rescaling a single drum fails the first clause on the diameters, the second on
// the hide, and the third on all four proportions at once.
void testTheFourDrumsAreFourInstruments()
{
    const auto parameters = defaultParameters();

    // What the four instruments are, as the drum table states them. The
    // diameters are the family's own: a 5-shaku o-daiko, a 2.5-shaku
    // nagado-daiko, a standing okedo and a tsuke-shime.
    struct Built
    {
        float diameterMetres;
        float depthOverDiameter;
        float arealDensity;
    };
    const Built built[4] = {
        { 1.50f, 0.850f, 1.0529f },
        { 0.78f, 1.200f, 0.8470f },
        { 0.40f, 1.250f, 0.5500f },
        { 0.30f, 0.700f, 0.4055f },
    };

    struct Signature
    {
        float depthOverDiameter;
        float breathingOverFundamental;
        float stiffness;
        float ringCycles;
        float arealDensity;
    };
    std::array<Signature, 4> signatures {};

    float previousFundamental = 0.0f;

    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto index = static_cast<std::size_t> (octave - taikor::lowestOctaveOffset);
        const auto reported = taikor::TaikoEngine::measure (parameters, octave);
        const auto& drum = taikor::getDrumDescription (octave);
        const auto where =
            " (" + std::string (drum.displayName) + ")";

        expect (drum.headDiameterMetres == built[index].diameterMetres,
                "the drum table no longer describes the instrument it names" + where);

        // The drum as resolved has to be the drum as built, to within the
        // tuning the keyboard asks of it. That residual is taken as size at the
        // factory Octave Body, and it is under half a per cent on every one of
        // the four - which is what makes it honest to call the table's numbers
        // the instrument's own.
        const auto diameter = 2.0f * reported.radiusMetres;
        expect (std::abs (diameter - built[index].diameterMetres)
                    < built[index].diameterMetres * 0.02f,
                "the resolved drum is not the size the table says it is: "
                    + std::to_string (diameter) + " m" + where);

        const auto depthRatio = reported.depthMetres / diameter;
        expect (std::abs (depthRatio - built[index].depthOverDiameter) < 0.02f,
                "the resolved body is not the shape the table says it is: "
                    + std::to_string (depthRatio) + where);

        expect (std::abs (reported.arealDensityKgPerSquareMetre
                          - built[index].arealDensity)
                    < built[index].arealDensity * 0.01f,
                "the head is not the hide the table says it is: "
                    + std::to_string (reported.arealDensityKgPerSquareMetre)
                    + " kg/m^2" + where);

        // Four different pitches, and an octave apart, because the keyboard has
        // to stay a keyboard however different the drums are. In the pitch the
        // drum is heard at and not in its loaded fundamental: the four drums are
        // not heard at the same mode of their own heads, so the fundamentals are
        // deliberately not an octave apart - they run 32.65 / 68.05 / 238.64 /
        // 477.28 Hz and the drums sound 59.66 / 119.32 / 238.64 / 477.28.
        expect (reported.soundingHz > 20.0f,
                "every drum of the grid must have an audible pitch" + where);
        if (previousFundamental > 0.0f)
        {
            const auto cents =
                1200.0f * std::log2 (reported.soundingHz / previousFundamental);
            expect (std::abs (cents - 1200.0f) < 20.0f,
                    "the step onto this drum is " + std::to_string (cents)
                        + " cents" + where);
        }
        previousFundamental = reported.soundingHz;

        signatures[index] = {
            depthRatio,
            reported.breathingModeHz / reported.loadedFundamentalHz,
            reported.headStiffnessParameter,
            reported.tailSeconds * reported.loadedFundamentalHz,
            reported.arealDensityKgPerSquareMetre
        };
    }

    // And the part a rescaling cannot produce. Every one of these is
    // dimensionless, so halving a drum and quadrupling its tension - or any
    // mixture of the two, which is the whole of what the octave transform can
    // do - leaves all of them exactly where they were. Measured on the four
    // drums:
    //
    //   depth / diameter          0.850 / 1.200 / 1.250 / 0.700
    //   breathing / fundamental   1.880 / 1.389 / 1.074 / 1.078
    //   head stiffness B (x1e4)   0.715 / 1.529 / 0.652 / 0.242
    //   ring, in cycles           136 / 204 / 347 / 804
    //   hide, kg/m^2              1.053 / 0.847 / 0.550 / 0.406
    //
    // The o-daiko and the chu-daiko are the closest pair in the modal ratios,
    // because both are thick tacked cowhide, and they are a long way apart in
    // the shape of their bodies. The okedo and the shime are the closest pair in
    // how far the air splits their pair, and they are a factor of two apart in
    // the head's own stiffness. So the clause is stated as: every pair of drums
    // has to differ by a tenth in at least one of the four ratios, and by a
    // a sixth in the hide, which is not a ratio at all but a statement about
    // what the head is made of.
    for (std::size_t a = 0; a < signatures.size(); ++a)
        for (std::size_t b = a + 1; b < signatures.size(); ++b)
        {
            const auto& first = signatures[a];
            const auto& second = signatures[b];
            const auto where =
                " (" + std::string (taikor::getDrumDescription (static_cast<int> (a)).displayName)
                + " against "
                + std::string (taikor::getDrumDescription (static_cast<int> (b)).displayName)
                + ")";

            const auto apart = [] (float x, float y)
            {
                const auto larger = std::max (std::abs (x), std::abs (y));
                return larger > 0.0f ? std::abs (x - y) / larger : 0.0f;
            };

            const auto worst = std::max ({ apart (first.depthOverDiameter,
                                                  second.depthOverDiameter),
                                           apart (first.breathingOverFundamental,
                                                  second.breathingOverFundamental),
                                           apart (first.stiffness, second.stiffness),
                                           apart (first.ringCycles, second.ringCycles) });

            expect (worst > 0.10f,
                    "two drums of the grid differ only in pitch, which is a "
                    "rescaling and not a family: worst dimensionless difference "
                        + std::to_string (worst) + where);
            // The closest pair of hides is the o-daiko's and the chu-daiko's, at
            // 1.053 and 0.847 kg/m^2 - nineteen per cent, and the two tacked
            // cowhides of the family.
            expect (apart (first.arealDensity, second.arealDensity) > 0.15f,
                    "two drums of the grid are headed with the same hide" + where);
        }

    // Finally the audio, because everything above reads measure() and an
    // implementation that changed the readout alone would pass the lot. Struck
    // dead centre, where every mode with a circumferential order is nodal and
    // the axisymmetric pair is the whole of the drum.
    auto centred = parameters;
    centred.strikePosition = -1.0f;
    centred.humanise = 0.0f;
    centred.tensionModulation = 0.0f;

    double previousSounded = 0.0;
    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto reported = taikor::TaikoEngine::measure (centred, octave);
        const auto mono =
            strike (centred, taikor::Articulation::Don, octave, 0.9f, 48000.0, 36000)
                .mono();
        const auto sounded = dominantFrequency (
            mono, 48000.0, reported.loadedFundamentalHz * 0.88,
            reported.loadedFundamentalHz * 1.12, reported.loadedFundamentalHz * 0.002,
            2400u, 2400u + 24000u);

        expect (std::abs (sounded - reported.loadedFundamentalHz)
                    < reported.loadedFundamentalHz * 0.02,
                "drum " + std::to_string (octave) + " does not have its fundamental "
                "where it says it does: " + std::to_string (sounded) + " against "
                    + std::to_string (reported.loadedFundamentalHz));
        if (previousSounded > 0.0)
            expect (sounded > previousSounded * 1.8,
                    "the four drums must sound four different fundamentals");
        previousSounded = sounded;
    }
}

// The four strokes have to be four different things done to the drum rather
// than four levels of one thing. Measured as the spectrum each one produces,
// normalised to its own loudest band so that level cannot stand in for timbre,
// and - for the pair that is deliberately the same strike with and without a
// hand on the head - as how long what is left goes on sounding.
void testTheFourStrokesAreMutuallyDistinct()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;

    // Octave bands from the o-daiko's fundamental up to the top of the head's
    // continuum, which is the whole of what the instrument produces.
    constexpr int bandCount = 8;
    const auto signatureOf = [&parameters] (taikor::Articulation articulation)
    {
        const auto mono =
            strike (parameters, articulation, 0, 0.92f, 48000.0, 48000).mono();

        std::array<double, bandCount> bands {};
        double loudest = 1.0e-30;
        for (int band = 0; band < bandCount; ++band)
        {
            const double low = 45.0 * std::pow (2.0, band);
            bands[static_cast<std::size_t> (band)] =
                bandLevelDb (mono, 48000.0, low, low * 2.0);
            loudest = std::max (loudest, bands[static_cast<std::size_t> (band)]);
        }
        for (auto& band : bands)
            band -= loudest;
        return bands;
    };

    std::array<std::array<double, bandCount>, taikor::articulationCount> signatures {};
    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        signatures[index] = signatureOf (static_cast<taikor::Articulation> (index));

    // Six pairs. The threshold is deliberately far above what a listener can
    // name: three decibels of difference somewhere in the spectrum, on curves
    // that have already had their own level divided out.
    for (std::size_t a = 0; a < taikor::articulationCount; ++a)
        for (std::size_t b = a + 1; b < taikor::articulationCount; ++b)
        {
            double worst = 0.0;
            for (int band = 0; band < bandCount; ++band)
                worst = std::max (worst,
                                  std::abs (signatures[a][static_cast<std::size_t> (band)]
                                            - signatures[b][static_cast<std::size_t> (band)]));

            const auto where =
                " ("
                + std::string (taikor::getArticulationDisplayName (
                      static_cast<taikor::Articulation> (a)))
                + " against "
                + std::string (taikor::getArticulationDisplayName (
                      static_cast<taikor::Articulation> (b)))
                + ")";
            expect (worst > 3.0,
                    "two strokes of the grid have the same spectrum to within "
                        + std::to_string (worst) + " dB" + where);
        }

    // And the pair that is the same strike with and without the free hand on
    // the head parts company in time rather than in the spectrum, so it is
    // asserted there as well. This is the clause a set of four gain-staged
    // copies of one stroke would fail.
    const auto open =
        strike (parameters, taikor::Articulation::Don, 0, 0.92f, 48000.0, 96000);
    const auto damped =
        strike (parameters, taikor::Articulation::Tsu, 0, 0.92f, 48000.0, 96000);
    expect (decayTime (damped.mono(), 48000.0, -40.0)
                < decayTime (open.mono(), 48000.0, -40.0) * 0.7,
            "Tsu is a Don with a hand on the head and must sustain far less");

    // The two that land out by the tacks part company by mechanism: only one of
    // them catches the hoop, so only one of them lifts the tack line.
    const auto tackScale = [&parameters] (taikor::Articulation articulation)
    {
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, defaultBlockSize);
        engine.reset();
        engine.trigger (articulation, 0, 1.0f);
        return taikor::TaikoEngineTestAccess::tackLine (engine).rimGain;
    };
    expect (tackScale (taikor::Articulation::DonRim)
                > tackScale (taikor::Articulation::Ka) * 2.0,
            "a rim shot must reach the hoop far harder than an edge stroke");
}

// The enclosed air used to be a spring of infinite extent. rho c^2 / L is the
// omega -> 0 limit of a cavity, and this drum runs out of that limit inside its
// own range: c/2L is 212 Hz at the factory body and 139 Hz at the deepest one,
// well under the top of the resolved bank. The air is really a column, the
// volume-changing motion of a two-headed drum is symmetric about the midplane,
// and each head therefore drives a rigidly terminated column of length L/2
// whose exact input stiffness is the lumped value times x cot x with
// x = omega L / 2c.
//
// That is an implicit eigenproblem - the stiffness depends on the frequency it
// sets - and what the drum resolve now converges on is the factor. It is
// reported, and everything below reads it, because a number that is solved for
// rather than computed is worth being able to see.
//
// The step this test lands with had one open decision in it: what to do where
// the fixed point lands at or past the quarter-wave, x = pi/2, at which the
// column's input stiffness is zero. **A decoupled pair is the answer here.**
// Above the quarter-wave the air is mass-like rather than stiff, this model has
// nowhere to put a mass - the enclosed air is a stiffness and not a degree of
// freedom - and past the second pole at x = pi the expression turns positive
// again on a branch that means something else. So the factor is floored at
// zero, which is continuous (x cot x reaches zero at the quarter-wave rather
// than jumping to it), monotone, and lands the drum in the state the readout
// already knows how to describe: two heads the air does not tie together, which
// is exactly what Air Coupling zero gives. The consequence asserted below is
// that the branches meet there and never cross, so nothing anywhere reports a
// breathing mode under the fundamental and testOctavesRaisePitch's clause that
// the cavity lifts the volume-changing mode above the other one stands
// unchanged - it is measured at the factory body, where the factor is 0.75 to
// 0.89 across the whole keyboard and the two branches are a fifth apart.
void testTheCavityIsAColumnNotAnInfiniteSpring()
{
    const auto measure = [] (taikor::EngineParameters parameters, int octave)
    { return taikor::TaikoEngine::measure (parameters, octave); };

    // Every radial mode sees a different length measured in wavelengths, so
    // each cached factor must be the fixed point of its own upper branch. The
    // probe also reads the built resonator bank: matching observeMode is what
    // proves the renderer consumed the per-entry stiffness rather than merely
    // caching four correct numbers and continuing to use entry zero for all of
    // them.
    {
        const auto probe = taikor::TaikoEngineTestAccess::axisymmetricCavityProbe (
            defaultParameters(), 0);
        const auto reported = measure (defaultParameters(), 0);

        for (std::size_t entry = 0; entry < probe.factors.size(); ++entry)
        {
            expect (std::abs (probe.factors[entry]
                              - probe.requestedFactors[entry]) < 2.0e-6f,
                    "axisymmetric cavity entry " + std::to_string (entry)
                        + " is not self-consistent");

            for (std::size_t branch = 0; branch < 2; ++branch)
                expect (std::abs (probe.renderedHz[entry][branch]
                                  - probe.observedHz[entry][branch])
                            < probe.observedHz[entry][branch] * 1.0e-6f,
                        "the rendered cavity stiffness did not follow entry "
                            + std::to_string (entry) + ", branch "
                            + std::to_string (branch));
        }

        // Entry zero is deliberately the old solve: it remains the octave
        // anchor and the only factor exposed in DrumMeasurements.
        expect (reported.cavityStiffnessFactor == probe.factors[0],
                "the public cavity factor stopped reporting entry zero");
        expect (probe.observedHz[0] == probe.sharedFactorHz[0],
                "the per-entry correction moved the (0,1) pair");
        expect (std::abs (reported.breathingModeHz - probe.observedHz[0][0])
                        < reported.breathingModeHz * 1.0e-5f
                    && std::abs (reported.loadedFundamentalHz
                                 - probe.observedHz[0][1])
                           < reported.loadedFundamentalHz * 1.0e-5f,
                "the fundamental readout stopped describing entry zero");

        // The old shared factor over-stiffened the higher radial modes. On the
        // factory drum their upper branches were 89.7667, 144.844 and 202.225
        // Hz; solving their own columns lowers all three, most audibly the
        // second radial entry, by 12.2 cents.
        const auto centsBelowShared = [&probe] (std::size_t entry)
        {
            return 1200.0f * std::log2 (
                probe.sharedFactorHz[entry][0] / probe.observedHz[entry][0]);
        };
        expect (centsBelowShared (1) > 11.0f
                    && centsBelowShared (2) > 4.0f
                    && centsBelowShared (3) > 1.0f,
                "the higher radial modes did not move below the shared-factor "
                "solve");
    }

    // Octave zero takes the full path directly. A transformed octave defers
    // the three factors its latched tuning mode cannot observe until after the
    // bisection chooses a winner, so probe that path independently: a missing
    // completion would otherwise leave every factory assertion above green.
    {
        const auto probe = taikor::TaikoEngineTestAccess::axisymmetricCavityProbe (
            defaultParameters(), 1);
        for (std::size_t entry = 0; entry < probe.factors.size(); ++entry)
        {
            expect (std::abs (probe.factors[entry]
                              - probe.requestedFactors[entry]) < 2.0e-6f,
                    "deferred cavity entry " + std::to_string (entry)
                        + " was not completed on the winning drum");
            for (std::size_t branch = 0; branch < 2; ++branch)
                expect (std::abs (probe.renderedHz[entry][branch]
                                  - probe.observedHz[entry][branch])
                            < probe.observedHz[entry][branch] * 1.0e-6f,
                        "the deferred cavity resolve did not reach the rendered "
                        "bank");
        }
    }

    // Where the cavity is shortest against the wavelength the correction has to
    // be nearly nothing: a shallow body at the top of the keyboard was already
    // an air spring. Twelve per cent and not the three this was drafted with,
    // because on the shime at Body Depth 0 - a 12 cm column under a head at
    // 498 Hz - the factor is 0.9020. It was 0.941 on the drum that sat at this
    // octave before step 5 and the four-by-four grid changed which drum that is.
    // Every literal below that is taken at an octave other than the reference
    // one was re-taken when step 5 landed, because step 5 changed which drum
    // sits at those octaves: the octave transform is now solved against the
    // sounding pitch rather than against the ideal membrane frequency, so a
    // drum an octave down is a different size and tension than it used to be
    // and its column is a different length. The claims are unchanged and every
    // one of them still holds; the numbers they are measured against are not the
    // same numbers. Each pair below is the lumped spring and the column
    // evaluated on the *same* drum, which is what this test was always about
    // and is now stated that way rather than against a frozen tree.
    {
        auto shallow = defaultParameters();
        shallow.bodyDepth = 0.0f;
        const auto reported = measure (shallow, 3);

        expect (std::abs (reported.cavityStiffnessFactor - 1.0f) < 0.14f,
                "a body short against the wavelength stopped being an air spring");
        // And its breathing mode must barely move. 565.4333 Hz is what a lumped
        // spring gives on this drum; the column gives 555.2779, which is 1.8 %.
        expect (std::abs (reported.breathingModeHz - 565.4333f) < 565.4333f * 0.025f,
                "a short cavity's breathing mode moved by more than two and a "
                "half per cent, so the correction is reaching where there is "
                "nothing to correct");
    }

    // And where the body is long against it the correction has to be large.
    // Both of these are a real body of the family rather than a rescaling of
    // one, which is what makes the point: an okedo-daiko is a tub a quarter
    // longer than it is wide, and a tub that long stops being an air spring
    // with a hide on it.
    {
        const auto okedo = measure (defaultParameters(), 2);
        expect (okedo.cavityStiffnessFactor < 0.65f,
                "the cavity did not soften inside the deepest-bodied drum of the "
                "family");
        // 271.0017 Hz with the lumped spring, 256.2846 with the column: 5.4 %.
        expect (okedo.breathingModeHz < 271.0017f * 0.95f,
                "the okedo's breathing mode did not come down by five per cent");

        auto deepest = defaultParameters();
        deepest.bodyDepth = 1.0f;
        const auto deepBody = measure (deepest, 3);
        expect (deepBody.cavityStiffnessFactor < 0.40f,
                "the deepest body's cavity did not soften");
        // 512.4915 Hz with the lumped spring, 492.6215 with the column: 3.9 %.
        expect (deepBody.breathingModeHz < 512.4915f * 0.962f,
                "the breathing mode at the deepest body did not come down by "
                "at least 3.8 per cent");
    }

    // The musical consequence. The keyboard is an octave in the drum's own
    // lowest mode and it is not an octave in the breathing branch, because the
    // branch above the fundamental is lifted by an air column whose length is a
    // property of each instrument rather than of a scaling. Measured on the four
    // drums: 759.6 / 956.6 / 1181.8 cents.
    //
    // This block used to say that the column widens every boundary against the
    // lumped spring it replaced, and that reasoning was about a family made by
    // rescaling one drum - a lumped spring stiffens as 1/L while a scaled drum
    // grows in every dimension at once. The four drums are not a scaling: their
    // bodies run 0.81, 0.66, 0.50 and 0.21 m, and their proportions run 0.85,
    // 1.20, 1.25 and 0.70 diameters, so the column argument does not point one
    // way any more. Measured with the column replaced by the lumped spring the
    // same three steps are 768.9 / 964.3 / 1134.0, so the column narrows the
    // bottom two and widens the top one - which is the drums' own proportions
    // and not a defect. What is asserted is what is still a property of the
    // instrument rather than of an argument: the steps rise, none of them is an
    // octave, and they sit where they are recorded.
    {
        const float recorded[3] = { 747.4f, 1727.1f, 1206.2f };
        float previous = 0.0f;

        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
        {
            const auto reported = measure (defaultParameters(), octave);
            if (previous > 0.0f)
            {
                const auto index =
                    static_cast<std::size_t> (octave - taikor::lowestOctaveOffset - 1);
                const float cents =
                    1200.0f * std::log2 (reported.breathingModeHz / previous);
                expect (std::abs (cents - recorded[index]) < 25.0f,
                        "the breathing branch's step into octave "
                            + std::to_string (octave) + " is "
                            + std::to_string (cents) + " cents, recorded as "
                            + std::to_string (recorded[index]));
                expect (cents > 500.0f,
                        "the breathing branch stopped rising into octave "
                            + std::to_string (octave));
            }
            previous = reported.breathingModeHz;
        }
    }

    // The lower branch has to stay where the cavity found it. Step 5 solves the
    // keyboard against that branch, and an implementation that dragged both
    // branches down would meet every clause above and leave the keyboard being
    // solved against a moving quantity.
    //
    // The anchor first: the factory drum at octave 0 reports 32.6503 Hz, and the
    // octave transform is the identity at the reference octave for every Octave
    // Body, so nothing about the keyboard can move it. Re-taken when the drum
    // table's reference o-daiko went from three shaku to five - see
    // testTheDrumIsTunedByThePitchItSounds for why the family had to widen.
    {
        expect (std::abs (measure (defaultParameters(), 0).loadedFundamentalHz
                          - 32.6503f)
                    < 32.6503f * 0.001f,
                "the loaded fundamental at the reference octave moved");
    }

    // Then the corners the control scan found worst. One and a half per cent
    // rather than the one per cent this was drafted with: the worst measured
    // drift over the scan is 1.17 %, it is in the configurations where the
    // factor has gone to zero, and it is the lower branch relaxing back to the
    // uncoupled head as the cavity is taken out from under it rather than the
    // branch being dragged anywhere.
    //
    // Re-taken when step 5 landed. They were literals from the tree before this
    // step, which was the same drum with a lumped spring in it; they are now the
    // same drum with a lumped spring in it under step 5's keyboard, measured by
    // forcing the column factor to one. The worst of the five moves 0.086 %,
    // and the largest anywhere in the scan clause below is 1.9085 %.
    //
    // Keep one recorded corner from each shipping Drum Layout. Intermediate
    // Octave Body geometries are no longer reachable product states; the full
    // endpoint scan below supplies the broader coverage.
    {
        struct Corner
        {
            float bodyDepth, headMaterial, tension, cavity, diameter, octaveBody;
            int octave;
            float loadedLumpedHz;
        };

        const Corner corners[] = {
            { 0.00f, 0.00f, 0.50f, 1.00f, 0.95f, 0.00f, 3, 1051.0128f },
            { 1.00f, 0.00f, 1.00f, 1.00f, 1.80f, 1.00f, 3, 988.7020f },
        };

        for (const auto& corner : corners)
        {
            auto parameters = defaultParameters();
            parameters.bodyDepth = corner.bodyDepth;
            parameters.headMaterial = corner.headMaterial;
            parameters.tension = corner.tension;
            parameters.cavityCoupling = corner.cavity;
            parameters.headDiameter = corner.diameter;
            parameters.octaveBody = corner.octaveBody;

            const auto reported = measure (parameters, corner.octave);
            expect (std::abs (reported.loadedFundamentalHz - corner.loadedLumpedHz)
                        < corner.loadedLumpedHz * 0.015f,
                    "the cavity correction moved the lower branch at octave "
                        + std::to_string (corner.octave));
        }
    }

    // The scan itself. Everything above is a handful of drums; these are the
    // properties that have to hold over the whole control space, and they are
    // the ones that say the reported factor is a number about the drum rather
    // than about the solver.
    {
        int total = 0;
        int decoupled = 0;
        float smallest = 1.0f;
        float worstLowerBranch = 0.0f;

        for (const float bodyDepth : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            for (const float headMaterial : { 0.0f, 0.5f, 1.0f })
                for (const float tension : { 0.0f, 0.5f, 1.0f })
                    for (const float diameter : { 0.2f, 0.95f, 1.8f })
                        for (const float octaveBody : { 0.0f, 1.0f })
                            for (int octave = taikor::lowestOctaveOffset;
                                 octave <= taikor::highestOctaveOffset; ++octave)
                            {
                                auto parameters = defaultParameters();
                                parameters.bodyDepth = bodyDepth;
                                parameters.headMaterial = headMaterial;
                                parameters.tension = tension;
                                parameters.headDiameter = diameter;
                                parameters.octaveBody = octaveBody;

                                // The same drum with the body opened, so the
                                // cavity's whole authority over the lower
                                // branch can be measured rather than assumed.
                                parameters.cavityCoupling = 0.0f;
                                const auto uncoupled =
                                    measure (parameters, octave).loadedFundamentalHz;

                                for (const float coupling : { 0.0f, 0.2f, 0.6f, 0.85f,
                                                              1.0f })
                                {
                                    parameters.cavityCoupling = coupling;
                                    const auto reported = measure (parameters, octave);
                                    ++total;

                                    smallest = std::min (smallest,
                                                         reported.cavityStiffnessFactor);
                                    if (reported.cavityStiffnessFactor <= 0.0f)
                                        ++decoupled;

                                    expect (reported.cavityStiffnessFactor >= 0.0f
                                                && reported.cavityStiffnessFactor
                                                       <= 1.0f,
                                            "the reported cavity factor left [0, 1]");
                                    // The floored case is a decoupled pair, and
                                    // a decoupled pair has one axisymmetric mode
                                    // and reports it twice. What must never
                                    // happen is the branches crossing.
                                    expect (reported.breathingModeHz
                                                >= reported.loadedFundamentalHz,
                                            "the breathing branch fell below the "
                                            "fundamental, so the column correction "
                                            "went past the quarter-wave");

                                    // At the reference octave only, and for the
                                    // same reason the monotonicity clause below
                                    // is: away from it the drum is not a fixed
                                    // function of the controls. The transform is
                                    // solved against the mode the drum is heard
                                    // at, opening the body changes which mode
                                    // that is, and the answer is then a different
                                    // drum whose lower branch has no reason to be
                                    // near this one's. What this clause is about
                                    // - how far the cavity itself can move the
                                    // branch - is exactly what octave 0 measures,
                                    // where the transform is the identity.
                                    if (uncoupled > 0.0f && octave == 0)
                                        worstLowerBranch = std::max (
                                            worstLowerBranch,
                                            std::abs (reported.loadedFundamentalHz
                                                      - uncoupled)
                                                / uncoupled);

                                    // Bit-identical on a second call. A damped
                                    // fixed point that has not converged is a
                                    // function of its iteration count, and this
                                    // is the cheapest way to say it is not.
                                    expect (measure (parameters, octave)
                                                    .cavityStiffnessFactor
                                                == reported.cavityStiffnessFactor,
                                            "two identical measurements disagreed "
                                            "about the cavity factor");
                                }

                                // Monotone in Body Depth at fixed everything
                                // else: a deeper body is a softer column, and a
                                // solve that has not converged is not monotone
                                // in anything.
                                //
                                // At the reference octave only, and the reason is
                                // the octave transform rather than this solve.
                                // Away from the reference octave the drum is no
                                // longer a fixed function of the controls: the
                                // transform is solved against the pitch the drum
                                // is heard at, deepening the body lowers that
                                // pitch, and the solve answers with a different
                                // size and tension. The factor of a *different*
                                // drum has no reason to be monotone in Body
                                // Depth, and since the solve follows whichever
                                // mode the drum is heard at, deepening the body
                                // far enough can hand the drum from one of its
                                // modes to another and move the answer by a fifth
                                // and a half at a stroke. Asserting anything
                                // there would be asserting that the family never
                                // crosses that balance, which is not a property
                                // of this solve and is not true at the corners of
                                // the control space this scan reaches - a 20 cm
                                // head at zero tension is not a drum. At octave 0
                                // the transform is the identity, the drum is a
                                // function of the controls alone, and the rise is
                                // exactly zero in every one of them, which is
                                // where this clause says what it was written to
                                // say.
                                parameters.cavityCoupling = 0.85f;
                                float previous = 2.0f;
                                for (int step = 0; step <= 20 && octave == 0; ++step)
                                {
                                    parameters.bodyDepth =
                                        static_cast<float> (step) / 20.0f;
                                    const auto factor =
                                        measure (parameters, octave)
                                            .cavityStiffnessFactor;
                                    expect (factor <= previous + 1.0e-6f,
                                            "the cavity factor is not monotone in "
                                            "Body Depth, so the solve has not "
                                            "converged");
                                    previous = factor;
                                }
                            }

        expect (total == 5400, "the cavity scan did not cover both Drum Layouts");
        // Every one that lands on the floor is a body longer than half the
        // wavelength of its own head, which is the same statement as its half
        // column having passed its quarter-wave: an 8 cm body under a head at
        // three and a half kilohertz, whose half wavelength is 4.9 cm, and the
        // like. None of them is a taiko, and the point
        // of recording the count is that a change which quietly decoupled the
        // instrument would move it.
        //
        // A third is a broad sanity bound rather than a fitted count: these are
        // deliberately hostile geometries, but decoupling most of either
        // shipping layout would mean the column solve had collapsed.
        expect (decoupled > 0 && decoupled < total / 3,
                "the number of drums whose column has passed its quarter-wave "
                "changed: " + std::to_string (decoupled) + " of "
                + std::to_string (total));
        expect (smallest <= 0.0f,
                "nothing in the scan reaches the floor any more");
        // The cavity's entire authority over the lower branch, measured at
        // 1.9085 %. It bounds what this step can do to that branch, whatever else
        // changes, which is what step 5 needs to be able to rely on.
        expect (worstLowerBranch < 0.025f,
                "the cavity moved the lower branch further than it ever did: "
                    + std::to_string (worstLowerBranch));
    }

    // And the audio has to follow the readout, which is the clause that stops
    // all of the above being met by a change to measure() alone. Struck dead
    // centre, where J_m(0) = 0 kills every mode with a circumferential order and
    // the axisymmetric pair is the whole of the drum, so the strongest partial
    // in the region is the breathing branch and nothing else.
    {
        auto centred = defaultParameters();
        centred.humanise = 0.0f;
        centred.strikePosition = -1.0f;

        const auto reported = measure (centred, 0);
        const auto mono =
            strike (centred, taikor::Articulation::Don, 0, 0.9f, 48000.0, 96000).mono();
        const auto dominant = dominantFrequency (mono, 48000.0,
                                                 reported.breathingModeHz * 0.70,
                                                 reported.breathingModeHz * 1.35, 0.05,
                                                 2400u, 50000u);

        // The render and the readout must agree. It is here so that scaling the
        // readout without scaling the audio fails rather than passes.
        expect (std::abs (dominant - reported.breathingModeHz)
                    < reported.breathingModeHz * 0.015,
                "the drum does not sound at the breathing mode it reports");
        // And the audio has actually moved. The lumped spring puts this partial
        // at 65.9416 Hz on the reference drum; the column puts it at 61.3723.
        expect (dominant < 65.9416 * 0.975,
                "the rendered breathing mode is still where an infinite spring "
                "put it");
    }
}

// A keyboard octave has to be an octave in the pitch the drum sounds, not in a
// quantity that is never audible on its own.
//
// The octave transform buys an octave by halving the radius, quadrupling the
// tension, or a mixture of the two that Octave Body chooses; all three double
// the ideal membrane frequency c*lambda/(2 pi a), and that is what the transform
// was written down to do. What a listener names is the lower branch of the
// air-loaded axisymmetric pair, and neither of the two things standing between
// the two quantities scales with the transform: the air load depends on
// rho_air a / sigma and the cavity on rho c^2 / L. So the two ways of buying an
// octave land a long way apart in the sounding pitch, and the transform that
// doubles the ideal frequency does not double the real one. Measured on the tree
// before this step, the loaded fundamental's octave steps at the factory drum
// were 1409.5 / 1359.6 / 1314.5 / 1276.3 / 1244.6 cents at Octave Body 0.7 and
// 1545.3 / 1443.2 / 1352.6 / 1286.2 / 1243.6 at Octave Body 1.0 - a keyboard
// whose bottom octave is nearly three semitones wide, worst error 345 cents.
//
// The transform is now solved rather than written down: how much of itself, in
// octaves, puts the loaded fundamental exactly an octave above the pitch this
// drum sounds untransformed. It is the principle the head's stiffness stretch
// already follows and the README already states - a drum is tuned by the pitch
// it sounds - applied to the term that was left out of it.
// The clause the whole of the step above exists for, stated where a listener
// stands: the four pads have to step in true octaves in the partial that is
// actually loudest, and each pad has to keep that partial at its fixed written
// strike as velocity changes.
//
// It is measured from rendered audio and from nothing else. measure() is what
// got this wrong in the first place - it reported four fundamentals on exact
// octaves while the instrument stepped 0 / 11.7 / 14.3 / 26.3 semitones - so
// nothing here asks the engine where to look. The scan is blind over two
// octaves either side of a nominal, and the nominal itself is only used to
// place the scan.
//
// Measured at the factory voicing with Humanise disabled, 48 kHz, the strongest
// partial of a 0.9 s window opening 80 ms after an open Don at velocities 0.35,
// 0.85 and 1.00. A Tsu is
// intentionally excluded: its palm is a time-varying boundary condition and
// can make a different mode win while the head is being choked; treating that
// as a pitch error would require a muted drum to masquerade as an open one.
//
//   o-daiko      59.56 .. 59.68 Hz    spread  3.3 cents
//   chu-daiko   119.49 .. 119.97      spread  7.0
//   okedo       238.65 .. 238.81      spread  1.1
//   shime       477.25 .. 477.39      spread  0.5
//   steps       +0.0 / +1207.1 / +2401.8 / +3601.3 cents
//
// The tolerances are twice the worst of those and no tighter, because what is
// left in them is real: the attack glide starts the head sharp and the window
// opens while it is still coming back, which is worth six cents on the slackest
// head of the family and is the same thing that costs the do-no-harm clause its
// margin above. Twenty cents is a fortieth of the smallest error this catches.
//
// Both halves would have failed before the step. The strongest partials were
// 88.96 / 174.92 / 203.09 / 405.97 Hz - steps of 0.0, 11.71, 14.29 and 26.28
// semitones - and the chu-daiko's moved 174.92 -> 102.51 Hz between velocity
// 0.85 and 1.00 on the same stroke, because two of its modes were within a
// decibel of each other and which one won depended on how hard it was hit.
void testTheFourDrumsStepInHeardOctaves()
{
    constexpr double sampleRate = 48000.0;
    // The window a pitch is taken from, which is the window the engine's own
    // sounding-mode comparison is written against: from the end of the attack
    // to nine tenths of a second later.
    const auto first = static_cast<std::size_t> (0.08 * sampleRate);
    const auto last = first + static_cast<std::size_t> (0.90 * sampleRate);
    const auto rendered = static_cast<int> (1.10 * sampleRate);

    struct Stroke
    {
        taikor::Articulation articulation;
        float velocity;
    };
    const Stroke strokes[] = {
        { taikor::Articulation::Don, 0.35f },
        { taikor::Articulation::Don, 0.85f },
        { taikor::Articulation::Don, 1.00f },
    };

    // The blind scan. Two octaves either side of the nominal, a fortieth of a
    // semitone at a time, refined twice. Each channel on its own rather than the
    // pair summed: the pair is what is played through, and a pad whose left
    // capsule is loudest at one partial and whose right is loudest at another is
    // not a pad with a pitch. It also matters here: the modes that compete on
    // this family are the ones with a nodal diameter, which is exactly the
    // family the two capsules read differently, and summing the two hides a flip
    // that either of them on its own shows plainly.
    const auto strongestPartial = [&] (const std::vector<float>& channel,
                                       double nominal)
    {
        const auto power = [&channel] (double frequency)
        {
            const auto magnitude =
                binMagnitude (channel, frequency, sampleRate, first, last);
            return magnitude * magnitude;
        };

        double best = -1.0;
        double bestFrequency = nominal;
        for (double frequency = nominal * 0.4; frequency <= nominal * 4.0;
             frequency *= 1.0006)
        {
            const auto here = power (frequency);
            if (here > best) { best = here; bestFrequency = frequency; }
        }
        for (double step : { 1.0003, 1.00008 })
            for (int around = -3; around <= 3; ++around)
            {
                const auto frequency = bestFrequency * std::pow (step, around);
                const auto here = power (frequency);
                if (here > best) { best = here; bestFrequency = frequency; }
            }
        return bestFrequency;
    };

    auto parameters = defaultParameters();
    // This is a tuning/family invariant, not a random-performance test. Keep
    // the authored strike fixed just as the neighbouring readout tests do;
    // Humanise has its own head-space and repeat-variation coverage below.
    parameters.humanise = 0.0f;
    double previous = 0.0;

    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto& drum = taikor::getDrumDescription (octave);
        const auto where = " (" + std::string (drum.displayName) + ")";
        const auto nominal = taikor::TaikoEngine::measure (parameters, octave).soundingHz;

        double lowest = 1.0e9;
        double highest = 0.0;
        double product = 1.0;

        for (const auto& stroke : strokes)
        {
            const auto take = strike (parameters, stroke.articulation, octave,
                                      stroke.velocity, sampleRate, rendered);
            expect (take.finite, "the take is not finite" + where);
            const auto left = strongestPartial (take.left, nominal);
            const auto right = strongestPartial (take.right, nominal);
            lowest = std::min ({ lowest, left, right });
            highest = std::max ({ highest, left, right });
            product *= std::sqrt (left * right);
        }

        // Stable: one pad, one pitch, however it is struck. This is the clause
        // that catches the chu-daiko's octave flip - two modes within a decibel
        // of each other and the winner decided by the velocity.
        const auto spreadCents = 1200.0 * std::log2 (highest / lowest);
        expect (spreadCents < 15.0,
                "the pad's strongest partial moves with the stroke: "
                    + std::to_string (lowest) + " to " + std::to_string (highest)
                    + " Hz, " + std::to_string (spreadCents) + " cents" + where);

        // And it has to be the pitch the engine says the drum is at, or the
        // readout and the instrument are two different claims again.
        const auto centre = std::pow (
            product, 1.0 / static_cast<double> (std::size (strokes)));
        expect (std::abs (1200.0 * std::log2 (centre / nominal)) < 20.0,
                "the pad does not sound where the engine says it does: "
                    + std::to_string (centre) + " against "
                    + std::to_string (nominal) + " Hz" + where);

        // True octaves, as heard.
        if (previous > 0.0)
        {
            const auto cents = 1200.0 * std::log2 (centre / previous);
            expect (std::abs (cents - 1200.0) < 20.0,
                    "the step onto this pad is " + std::to_string (cents)
                        + " cents in the partial a listener actually hears" + where);
        }

        previous = centre;
    }
}

// The strongest partial of one rendered take, found blind: a coarse geometric
// scan over the band the drum's own modes occupy, then three refinements. The
// band is fixed by the octave and by nothing the engine reports, so this cannot
// be fooled by a readout that has gone wrong - which is the whole point, because
// two of the clauses below are about the readout being wrong.
//
// Coarser on the first pass than testTheFourDrumsStepInHeardOctaves's scan
// because it runs a hundred times rather than twenty-four; the refinements bring
// it back to about a third of a cent, which is well under every tolerance stated
// here.
double blindStrongestPartial (const std::vector<float>& channel, double lowHz,
                              double highHz, double sampleRate, std::size_t first,
                              std::size_t last)
{
    const auto power = [&] (double frequency)
    {
        const auto magnitude =
            binMagnitude (channel, frequency, sampleRate, first, last);
        return magnitude * magnitude;
    };

    double best = -1.0;
    double bestFrequency = lowHz;
    for (double frequency = lowHz; frequency <= highHz; frequency *= 1.005)
    {
        const auto here = power (frequency);
        if (here > best) { best = here; bestFrequency = frequency; }
    }
    for (double step : { 1.0015, 1.0004, 1.0001 })
        for (int around = -4; around <= 4; ++around)
        {
            const auto frequency = bestFrequency * std::pow (step, around);
            const auto here = power (frequency);
            if (here > best) { best = here; bestFrequency = frequency; }
        }
    return bestFrequency;
}

// One dry modal Don, rendered and summed to mono. Mono rather than the two channels
// separately because these clauses ask where one stroke sits rather than whether
// the pad has one pitch: with the two capsules reading a nodal-diameter mode
// differently, a geometric mean of two channels that have landed on two
// different partials of a near-tie is a frequency that is in neither of them,
// and that is an artefact of the estimator rather than anything about the drum.
std::vector<float> monoDon (const taikor::EngineParameters& parameters, int octave)
{
    constexpr double sampleRate = 48000.0;
    const auto rendered = static_cast<int> (1.10 * sampleRate);
    return resolvedStrike (parameters, taikor::Articulation::Don, octave, 0.9f,
                           sampleRate, rendered)
        .mono();
}

// The octave transform has to be continuous in every control, and the readout has
// to describe the stroke the player is actually playing. Both of these were
// broken by the same thing, and it is worth saying what it was in one place.
//
// The transform is solved against the pitch a drum is heard at, and "heard at"
// was taken as an argmax over the drum's modes, re-run on every parameter update.
// Two of this family's modes sit within a decibel of each other over wide
// stretches of the control space, and an argmax across a near-tie is a step. So
// wherever the reference drum's two modes crossed, the quantity every other
// octave was solved against jumped by up to a tenth of an octave and every
// transformed drum re-solved for radically different geometry. Measured on the
// tree this landed on, at factory settings:
//
//   Pitch        7.48 -> 7.49 st      chu-daiko 183.8 -> 100.6 Hz, -1043 cents,
//                                     head 0.238 -> 0.404 m
//   Pitch       -5.19 -> -5.18 st     chu-daiko            -507 cents
//   Head Tension 0.9170 -> 0.9175     chu-daiko           -1043 cents
//
// A hundredth of a semitone is one step of ordinary Pitch automation, so the
// first of those is a lane a player cannot ride across. The same crossing is
// reachable from Head Tension, Head Diameter, Head Material, Air Coupling,
// Resonant Tension, Mic Distance and Mic Spread - it is not a property of the
// Pitch control, it is a property of tuning against an argmax.
//
// The fix latches the mode identity instead of re-choosing it, so the solve
// tracks one named mode of one named drum as the controls move. The invariant
// is continuity of that physical spectrum and geometry. A strongest-partial
// argmax is not a valid continuity observable at an exact near-tie: two peaks
// can exchange first place while both move smoothly, as the 44/86 Hz pair does
// around Pitch -5.19.
void testThePitchTransformIsContinuousUnderAutomation()
{
    // Three crossings: the one ordinary Pitch automation runs into, the next one
    // down the Pitch range, and the same balance reached from Head Tension
    // instead - because the defect was never about the Pitch control.
    //
    // A step of 0.01 semitones is a cent of transposition in itself, and a step
    // of 0.0005 in Head Tension is 1.26 cents through that control's own
    // geometric map, so a continuous solve cannot read zero here. Eight cents
    // leaves ample room for that requested motion while remaining two orders of
    // magnitude below the old geometry discontinuity.
    constexpr double tolerance = 8.0;

    struct Sweep
    {
        const char* what;
        bool headTension;
        double from, to, step;
    };
    const Sweep sweeps[] = {
        { "Pitch across 7.49 semitones", false, 7.46, 7.52, 0.01 },
        { "Pitch across -5.18 semitones", false, -5.21, -5.15, 0.01 },
        { "Head Tension across 0.9175", true, 0.9160, 0.9190, 0.0005 },
    };

    for (const auto& sweep : sweeps)
        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
        {
            taikor::TaikoEngine::DrumMeasurements previous {};
            bool havePrevious = false;
            double previousValue = 0.0;

            for (double value = sweep.from; value <= sweep.to + 1.0e-9;
                 value += sweep.step)
            {
                auto parameters = defaultParameters();
                // Humanise off: the question is what one setting sounds like, and
                // per-stroke scatter in where the stick lands is not part of it.
                parameters.humanise = 0.0f;
                if (sweep.headTension)
                    parameters.tension = static_cast<float> (value);
                else
                    parameters.pitch = static_cast<float> (value);

                const auto drum = taikor::TaikoEngine::measure (parameters, octave);
                expect (drum.radiusMetres > 0.0f
                            && drum.idealFundamentalHz > 0.0f
                            && drum.loadedFundamentalHz > 0.0f
                            && drum.breathingModeHz > 0.0f,
                        "the swept drum has no physical spectrum");

                if (havePrevious)
                {
                    const auto continuous = [&] (double before, double after,
                                                  const char* quantity)
                    {
                        const auto cents = 1200.0 * std::log2 (after / before);
                        expect (std::abs (cents) < tolerance,
                                std::string ("the physical drum lurches under automation: ")
                                    + sweep.what + ", octave "
                                    + std::to_string (octave) + ", "
                                    + std::to_string (previousValue) + " -> "
                                    + std::to_string (value) + " moved " + quantity
                                    + " by " + std::to_string (cents) + " cents");
                    };
                    continuous (previous.radiusMetres, drum.radiusMetres, "radius");
                    continuous (previous.idealFundamentalHz,
                                drum.idealFundamentalHz, "ideal fundamental");
                    continuous (previous.loadedFundamentalHz,
                                drum.loadedFundamentalHz, "loaded fundamental");
                }

                previous = drum;
                havePrevious = true;
                previousValue = value;
            }
        }
}

// And the second half of the same defect: the readout evaluated a Don at the
// profile's factory radius whatever Strike Position said, so the number on the
// panel was the pitch of a stroke nobody had played.
//
// An off-centre stroke really is heard at a different pitch - a stroke towards
// the middle of the head stops driving the modes with a nodal diameter and drives
// the radial orders instead - and that physics was always in the model. What was
// missing was the readout following it. Measured on the tree this landed on, with
// the reported figure frozen at the centred stroke:
//
//   Strike Position   chu-daiko heard   chu-daiko reported   error
//    0.00              119.8 Hz          119.3 Hz              -7 cents
//   -0.25              171.9              119.3              -633
//   -0.50              172.0              119.3              -633
//
// The octave solve is deliberately *not* strike-aware - see tuningStrikeRadius -
// so Strike Position moves the timbre and the readout and leaves the tuning
// alone. testEveryParameterSurvivesTheCache and the sweeps above would both catch
// it if that changed.
void testTheReadoutFollowsTheStrikePosition()
{
    constexpr double sampleRate = 48000.0;
    const auto first = static_cast<std::size_t> (0.08 * sampleRate);
    const auto last = first + static_cast<std::size_t> (0.90 * sampleRate);

    int pinned = 0;
    double worstPinnedCents = 0.0;
    double worstShare = 1.0;

    for (const float position : { 0.00f, -0.25f, -0.50f })
        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
        {
            auto parameters = defaultParameters();
            parameters.strikePosition = position;
            parameters.humanise = 0.0f;

            const auto reported =
                taikor::TaikoEngine::measure (parameters, octave).soundingHz;
            const auto mono = monoDon (parameters, octave);
            const double nominal = 59.66 * std::pow (2.0, static_cast<double> (octave));
            const double lowHz = nominal * 0.25, highHz = nominal * 6.0;
            const auto heard = blindStrongestPartial (mono, lowHz, highHz, sampleRate,
                                                      first, last);

            const auto where = ", strike position " + std::to_string (position)
                             + ", octave " + std::to_string (octave);

            // The strongest partial more than four per cent away from the winner,
            // which is what says whether this take has one pitch or several. On
            // the o-daiko struck at or near the very middle it does not: three
            // partials land within a decibel of one another, the model's own
            // ranking of them is inside its stated accuracy, and no readout can
            // name one of them and be right. Everywhere else there is a clear
            // winner and the readout has to be on it.
            double rivalBest = -1.0, rival = lowHz;
            for (double frequency = lowHz; frequency <= highHz; frequency *= 1.005)
            {
                if (frequency > heard / 1.04 && frequency < heard * 1.04)
                    continue;
                const auto magnitude =
                    binMagnitude (mono, frequency, sampleRate, first, last);
                if (magnitude > rivalBest) { rivalBest = magnitude; rival = frequency; }
            }
            (void) rival;

            const auto heardMagnitude =
                binMagnitude (mono, heard, sampleRate, first, last);
            const auto clearDb = 20.0 * std::log10 (heardMagnitude
                                                    / std::max (rivalBest, 1.0e-30));

            if (clearDb > 1.5)
            {
                ++pinned;
                const auto cents = 1200.0 * std::log2 (reported / heard);
                worstPinnedCents = std::max (worstPinnedCents, std::abs (cents));
                // Twenty cents, and the worst of the ten cases this fires on is
                // 7.0 - the chu-daiko struck normally, where the attack glide has
                // not quite settled by the time the window opens. Before the
                // readout was told where the stick lands it was 633.
                expect (std::abs (cents) < 20.0,
                        "the reported pitch is not the one the stroke is heard at: "
                            + std::to_string (reported) + " against "
                            + std::to_string (heard) + " Hz, "
                            + std::to_string (cents) + " cents" + where);
            }

            // And for every take, including the ones with no single pitch: the
            // rendered level at the reported frequency has to be within a couple
            // of decibels of the strongest partial there is. Measured worst 0.798
            // - the chu-daiko at the centred stroke, seven cents off a narrow
            // peak - against 0.068 before, which is the o-daiko's reported pitch
            // sitting twenty-three decibels down in a take it is not in.
            const auto atReported =
                binMagnitude (mono, reported, sampleRate, first, last);
            const auto share = atReported / std::max (heardMagnitude, 1.0e-30);
            worstShare = std::min (worstShare, share);
            expect (share > 0.75,
                    "the reported pitch is not where the energy is: "
                        + std::to_string (share) + " of the strongest partial"
                        + where);
        }

    // The guard on the guard: the clear-winner test must not be quietly
    // disabling the clause. Ten of the twelve takes have one pitch; the two that
    // do not are the o-daiko at -0.25 and -0.50, where the winner is 0.55 and
    // 0.51 dB clear.
    expect (pinned >= 8,
            "only " + std::to_string (pinned)
                + " of the twelve takes had an unambiguous pitch, so the clause "
                  "above has stopped saying anything");
    (void) worstPinnedCents;
    (void) worstShare;
}

// The strongest partial of a take, found blind, with the bias taken out of the
// search.
//
// blindStrongestPartial above walks the band in constant *ratio* steps and then
// refines around whichever step read highest. A window of fixed length has a
// fixed resolution in hertz - 1.11 Hz over 0.9 s - so a constant-ratio step is
// a wider and wider slice of it the higher it lands: at 68 Hz a step of 1.005
// is an eighth of the window's own width and costs a peak 0.2 dB, and at 228 Hz
// the same step is a whole width and costs it up to 4 dB. Refining only the
// coarse winner cannot undo that, because the comparison that chose the winner
// has already happened. It is enough to decide a near-tie the wrong way and it
// always decides it for the lower partial, which is exactly the direction that
// would flatter the clauses below.
//
// So every local maximum of the coarse pass within ten decibels of the best is
// refined on its own, and the comparison happens between the refined values.
// Checked against a 262144-point transform of the same window - which has no
// search in it at all - over twenty-four takes spread across the four drums,
// this agrees on every one and the single-pass search does not.
double strongestPartialUnbiased (const std::vector<float>& channel, double lowHz,
                                 double highHz, double sampleRate,
                                 std::size_t first, std::size_t last)
{
    const auto power = [&] (double frequency)
    {
        const auto magnitude =
            binMagnitude (channel, frequency, sampleRate, first, last);
        return magnitude * magnitude;
    };

    std::vector<std::pair<double, double>> coarse;
    double coarseBest = 0.0;
    for (double frequency = lowHz; frequency <= highHz; frequency *= 1.002)
    {
        const auto here = power (frequency);
        coarse.push_back ({ frequency, here });
        coarseBest = std::max (coarseBest, here);
    }

    const double gate = coarseBest * 0.1;    // ten decibels
    double best = -1.0;
    double bestFrequency = lowHz;

    for (std::size_t index = 1; index + 1 < coarse.size(); ++index)
    {
        if (coarse[index].second < gate) continue;
        if (! (coarse[index].second >= coarse[index - 1].second
               && coarse[index].second >= coarse[index + 1].second))
            continue;

        double localBest = coarse[index].second;
        double localFrequency = coarse[index].first;
        for (double step : { 1.0006, 1.00015, 1.00004 })
            for (int around = -4; around <= 4; ++around)
            {
                const auto frequency = localFrequency * std::pow (step, around);
                const auto here = power (frequency);
                if (here > localBest) { localBest = here; localFrequency = frequency; }
            }

        if (localBest > best) { best = localBest; bestFrequency = localFrequency; }
    }

    return bestFrequency;
}

// The strongest bin within `cents` of a frequency. Used to ask what the rendered
// take has at the frequency the readout names, with an allowance for the attack
// glide: the head is still stretched when the pitch window opens, so every
// membrane partial sits sharp of where it settles. It is the same *ratio* on all
// of them, because a tension shift scales the whole head at once, and measured
// over this family it reaches eleven cents. Reading at exactly the settled
// frequency therefore measures the drum on the low partials and the window's own
// 1.11 Hz resolution on the high ones - eleven cents is a quarter of a hertz at
// 68 Hz and one and a half hertz at 336 - which is a property of the estimator
// and not of the instrument.
double bandMagnitude (const std::vector<float>& channel, double centreHz,
                      double cents, double sampleRate, std::size_t first,
                      std::size_t last)
{
    double best = 0.0;
    const double low = centreHz * std::exp2 (-cents / 1200.0);
    const double high = centreHz * std::exp2 (cents / 1200.0);
    for (double frequency = low; frequency <= high; frequency *= 1.0004)
        best = std::max (best, binMagnitude (channel, frequency, sampleRate, first, last));
    return best;
}

// The readout has to name the partial the drum is actually heard at, and it has
// to keep doing it when the stick and the microphones move.
//
// This is the third defect of the same shape. The readout used to evaluate one
// fixed stroke whatever Strike Position said (fixed above); then it followed the
// stroke but compared only the modes below the head's third radial order, and
// weighted each of them as though a bachi were an impulse. Reported against
// rendered audio, octave 1, Don at 0.85, Strike Position +0.50 with the pair
// coincident: heard 68.3 Hz, reported 227.2 Hz, 2081 cents apart.
//
// Two things were wrong and one thing was not.
//
// Not wrong: the ten decibels the reported frequency appeared to be down in the
// take. That is the attack glide moving the partial nine cents sharp of where
// the model puts it, read through a window whose own resolution is 1.11 Hz -
// nine cents is 1.2 Hz at 227 Hz and a quarter of a hertz at 68. Measured where
// it actually sounds, the partial the old readout named is 0.4 dB off the
// strongest in that take. See soundingMode.
//
// Wrong, first: the bound. The o-daiko struck at its middle is heard at its
// (0,3) lower branch and the chu-daiko with the pair backed off at its (1,3),
// and a comparison that stopped at the third radial order could not name either.
// Wrong, second: the stroke. A bachi rests on the head for one to six
// milliseconds depending on how hard it is, and a force pulse that long cannot
// drive a mode whose period is not much longer than it. Leaving that out put the
// chu-daiko's (1,2) nine decibels above where the rendered take has it under a
// felt beater, and it is what decided the reported case.
//
// What is asserted, from rendered audio and from nothing the engine says about
// itself:
void testTheReadoutNamesThePartialTheDrumIsHeardAt()
{
    constexpr double sampleRate = 48000.0;
    const auto first = static_cast<std::size_t> (0.08 * sampleRate);
    const auto last = first + static_cast<std::size_t> (0.90 * sampleRate);
    const auto rendered = static_cast<int> (1.10 * sampleRate);

    // Three full Don strokes across velocity. The panel's readout is explicitly
    // resolved for Don (readoutStrikeRadius); a Tsu adds a palm constraint and
    // is allowed to leave a different survivor, so including it would ask one
    // number to describe two different physical boundary conditions.
    struct Stroke { taikor::Articulation articulation; float velocity; };
    const Stroke strokes[] = {
        { taikor::Articulation::Don, 0.35f }, { taikor::Articulation::Don, 0.85f },
        { taikor::Articulation::Don, 1.00f },
    };

    // The region the defect lives in: the strike walked across the head, crossed
    // with the two microphone controls, on all four drums, plus the corners of
    // Pitch, Head Tension and Bachi Hardness that move which mode wins. Every
    // one of these was found by sweeping, not chosen: the settings marked below
    // are the ones the pre-fix engine gets wrong.
    struct Setting
    {
        int octave;
        float strike, spread, distance, pitch, tension, bachi, width;
    };
    const auto at = [] (int octave, float strike, float spread, float distance)
    { return Setting { octave, strike, spread, distance, 0.0f, 0.62f, 0.7f, 0.5f }; };

    const Setting settings[] = {
        // The reported case, and its neighbours across the strike.
        at (1, 0.50f, 0.00f, 0.35f),      // reported 227.2 against 68.3 before
        at (1, 0.25f, 0.00f, 0.35f),
        at (1, 0.00f, 0.00f, 0.35f),
        at (1, -0.50f, 0.00f, 0.35f),
        at (1, 0.00f, 0.55f, 0.35f),      // the factory stroke and pair
        at (1, 0.50f, 0.55f, 0.35f),
        // The o-daiko, where the (0,3) branch takes over.
        at (0, -0.50f, 0.55f, 0.00f),     // reported 85.8 against 140.0 before
        at (0, -0.25f, 0.55f, 0.00f),     // reported 85.8 against 140.0 before
        at (0, 0.75f, 0.00f, 0.00f),      // reported 115.3 against 140.1 before
        at (0, 1.00f, 0.00f, 0.00f),      // reported 59.7 against 140.1 before
        at (0, 0.00f, 0.55f, 0.35f),
        at (0, -0.50f, 0.00f, 0.35f),
        // The two small drums, which are heard at their fundamentals and must
        // stay there.
        at (2, 0.00f, 0.55f, 0.35f),
        at (2, 1.00f, 0.00f, 0.35f),
        at (3, 0.00f, 0.55f, 0.35f),
        at (3, 1.00f, 0.00f, 0.35f),
        // Pitch and Head Tension taken to their corners on the o-daiko.
        { 0, -0.50f, 0.55f, 0.35f, -7.0f, 0.62f, 0.7f, 0.5f },  // 57.1 against 94.2
        { 0, -0.50f, 0.55f, 0.35f, 0.0f, 0.40f, 0.7f, 0.5f },   // 57.7 against 102.3
        // A felt beater, which is where the contact time matters most.
        { 1, 0.50f, 0.00f, 0.35f, 0.0f, 0.62f, 0.0f, 0.5f },
        { 0, 0.00f, 0.55f, 0.35f, 0.0f, 0.62f, 0.0f, 0.5f },
        // Stereo Width, with the pair fully opened so the two capsules straddle
        // the nodal diameters and the width trim decides how much of the
        // difference between them survives. At 0 they are summed and a mode of
        // order three all but cancels; at 1 the difference is exaggerated
        // instead. Ranking on the left capsule described neither.
        { 1, 1.00f, 1.00f, 0.00f, 0.0f, 0.62f, 0.7f, 0.0f },    // 210.1 against 119.8
        { 0, 1.00f, 1.00f, 0.35f, 0.0f, 0.62f, 0.7f, 0.0f },    // 107.6 against 59.7
        { 0, 1.00f, 1.00f, 0.35f, 0.0f, 0.62f, 0.7f, 1.0f },    // 107.6 against 84.6
        { 1, 1.00f, 1.00f, 0.00f, 0.0f, 0.62f, 0.7f, 1.0f },
        { 2, 1.00f, 1.00f, 0.00f, 0.0f, 0.62f, 0.7f, 1.0f },    // 238.6 against 412.3
        { 1, 0.00f, 0.55f, 0.35f, 0.0f, 0.62f, 0.7f, 0.0f },
    };

    int pitched = 0;
    double worstPitchedCents = 0.0;
    double worstShare = 1.0;

    for (const auto& setting : settings)
    {
        auto parameters = defaultParameters();
        // Humanise off, for the reason readoutStrikeRadius gives: the readout
        // describes the stroke the controls ask for, not the scatter around it.
        parameters.humanise = 0.0f;
        parameters.strikePosition = setting.strike;
        parameters.micSpread = setting.spread;
        parameters.micDistance = setting.distance;
        parameters.pitch = setting.pitch;
        parameters.tension = setting.tension;
        parameters.bachiHardness = setting.bachi;
        parameters.stereoWidth = setting.width;

        const auto reported =
            taikor::TaikoEngine::measure (parameters, setting.octave).soundingHz;
        expect (std::isfinite (reported) && reported > 0.0f,
                "the readout is not a frequency at all: "
                    + std::to_string (reported) + " Hz");

        const auto where =
            ", octave " + std::to_string (setting.octave) + ", strike "
            + std::to_string (setting.strike) + ", spread "
            + std::to_string (setting.spread) + ", distance "
            + std::to_string (setting.distance) + ", pitch "
            + std::to_string (setting.pitch) + ", tension "
            + std::to_string (setting.tension) + ", bachi "
            + std::to_string (setting.bachi) + ", width "
            + std::to_string (setting.width);

        // The band is fixed by the octave and by nothing the engine reports, so
        // a readout that has gone wrong cannot narrow the search that catches it.
        const double nominal =
            59.66 * std::pow (2.0, static_cast<double> (setting.octave));
        const double lowHz = nominal * 0.25, highHz = nominal * 6.0;

        std::array<double, 3> heard {};
        std::vector<float> reference;
        for (std::size_t index = 0; index < heard.size(); ++index)
        {
            const auto take = resolvedStrike (
                parameters, strokes[index].articulation, setting.octave,
                strokes[index].velocity, sampleRate, rendered);
            expect (take.finite, "the take is not finite" + where);
            // The left output channel, which is what `observeMode` describes -
            // the left *output*, after the width trim, and not the left
            // capsule. Those are the same signal only at Stereo Width 0.5; at 0
            // the output is the sum of the two capsules and at 1 it is their
            // difference exaggerated, and the settings above put both of those
            // in the grid. Not the pair summed offline: a mono sum of two
            // capsules that have landed on different partials of a near-tie is
            // a spectrum neither channel has.
            heard[index] = strongestPartialUnbiased (take.left, lowHz, highHz,
                                                     sampleRate, first, last);
            if (strokes[index].articulation == taikor::Articulation::Don
                && strokes[index].velocity == 0.85f)
                reference = take.left;
        }

        // The largest group of strokes that agree within 50 cents, and where
        // they agree.
        int agreeing = 0;
        double centre = heard[0];
        for (const double candidate : heard)
        {
            int count = 0;
            double product = 1.0;
            for (const double other : heard)
                if (std::abs (1200.0 * std::log2 (other / candidate)) <= 50.0)
                {
                    ++count;
                    product *= other;
                }
            if (count > agreeing)
            {
                agreeing = count;
                centre = std::pow (product, 1.0 / static_cast<double> (count));
            }
        }

        // Agreement across velocities is not enough to make a unique pitch:
        // two remote partials can remain within a fraction of a decibel. Use
        // the same 1.5 dB clarity rule as the strike-position regression, and
        // leave the energy-share clause below active even when this one is
        // deliberately skipped.
        double clearDb = -300.0;
        if (! reference.empty())
        {
            const auto heardReference = strongestPartialUnbiased (
                reference, lowHz, highHz, sampleRate, first, last);
            double rivalBest = 0.0;
            for (double frequency = lowHz; frequency <= highHz; frequency *= 1.005)
            {
                if (frequency > heardReference / 1.04
                    && frequency < heardReference * 1.04)
                    continue;
                rivalBest = std::max (
                    rivalBest,
                    binMagnitude (reference, frequency, sampleRate, first, last));
            }
            const auto heardMagnitude = binMagnitude (
                reference, heardReference, sampleRate, first, last);
            clearDb = 20.0 * std::log10 (
                heardMagnitude / std::max (rivalBest, 1.0e-30));
        }

        if (agreeing >= 2 && clearDb > 1.5)
        {
            ++pitched;
            const auto cents = 1200.0 * std::log2 (reported / centre);
            worstPitchedCents = std::max (worstPitchedCents, std::abs (cents));
            // Thirty-five cents. The readout names the settled pole while this
            // rendered window still contains the physical attack stretch; on
            // the shared bank that reaches 29 cents in this grid.
            // Over a 252-setting sweep - the strike crossed with both
            // microphone controls on all four drums, and Pitch, Head Tension
            // and Bachi Hardness crossed with the strike - 195 of the 201
            // settings that have a pitch are within 19 cents and six are not;
            // those six are misses rather than tolerance, they are recorded in
            // the plan, and none of them is here. What is inside the 19 cents
            // is the attack glide: the readout names the frequency the head
            // settles at and the window opens while the head is still sharp.
            // Before the fixes these twenty-six read 2088 cents.
            //
            // The six Stereo Width settings were added a round later, with the
            // width trim itself: on the tree before that they read 1019 cents
            // and a level share of 0.034.
            expect (std::abs (cents) < 35.0,
                    "the readout is not on the partial the drum is heard at: "
                        + std::to_string (reported) + " against "
                        + std::to_string (centre) + " Hz, "
                        + std::to_string (cents) + " cents, with "
                        + std::to_string (agreeing)
                        + " of three strokes agreeing" + where);
        }

        // And for every setting, including the ones with no single pitch: the
        // reported frequency has to be where the energy is. Measured against the
        // strongest partial of the Don at 0.85, with the glide allowance
        // bandMagnitude documents.
        expect (! reference.empty(), "the reference take was not rendered" + where);
        const auto strongest = binMagnitude (
            reference,
            strongestPartialUnbiased (reference, lowHz, highHz, sampleRate, first, last),
            sampleRate, first, last);
        const auto atReported =
            bandMagnitude (reference, reported, 20.0, sampleRate, first, last);
        const auto share = atReported / std::max (strongest, 1.0e-30);
        worstShare = std::min (worstShare, share);
        // Measured worst over these twenty-six settings: 0.999 after, 0.034
        // before - the chu-daiko at the rim with the pair fully open and summed
        // to mono, where the readout named a partial with a thirtieth of the
        // amplitude of the one that is there.
        expect (share > 0.85,
                "the reported pitch is not where the energy is: "
                    + std::to_string (share) + " of the strongest partial" + where);
    }

    // The guard on the guard. The tie rule must not be quietly emptying the
    // clause above: if a change made every take ambiguous, the pitch clause
    // would pass by never firing. At least twenty of the twenty-six must have
    // both velocity agreement and a winner more than 1.5 dB clear; the
    // deliberately excluded takes are genuine near-ties, not permission for
    // the readout assertion to become vacuous.
    expect (pitched >= 20,
            "only " + std::to_string (pitched) + " of "
                + std::to_string (std::size (settings))
                + " settings had a clear pitch, so the clause above has stopped "
                  "saying anything");
    (void) worstPitchedCents;
    (void) worstShare;
}

// Whatever the controls are set to, the drum has a pitch and the panel has to
// print one.
//
// It did not. `ModeObservation::weight` carries a difference of two
// exponentials of the mode's decay, and `soundingMode` used to accept a mode
// only if its weight beat a zero-initialised best. On a very small head at the
// tension ceiling every mode of the drum is emptied before the pitch window
// even opens - at 15 cm, Head Tension 1.0, a thin film and Pitch +12, the okedo
// pad's longest-lived mode decays at nine hundred inverse seconds - so both
// exponentials underflow to exactly zero, every weight on the drum is zero,
// nothing is ever accepted and the readout is the default 0.00 Hz:
//
//   octave 0   2466.33 Hz      octave 2      0.00 Hz  (fundamental 16793.35)
//   octave 1   5097.77         octave 3      0.00     (fundamental 25565.08)
//
// Swept over the drum controls it was 22527 of 729000 combinations, and over
// the stroke and microphone controls 6750 of 162000 more - three per cent of
// the space, all of it on the small tight heads, and every one of them a drum
// that plainly sounds. The comparison now falls back to the same quantity in
// nepers, which has the exponents rather than the exponentials in it, and takes
// the first valid mode when even that cannot separate them.
//
// This reads the reported value directly rather than the audio, because the
// defect is a literal zero: there is no pitch to measure it against.
void testTheReadoutIsAlwaysAFrequency()
{
    // What this guarantees, and it is not quite what it used to.
    //
    // It was written for a defect in which every weight on a small tight head
    // underflowed to zero, nothing was ever accepted, and the panel printed
    // 0.00 Hz for a drum that plainly sounds. It asserted the reported value was
    // positive everywhere, at one implicit sample rate.
    //
    // That is now too strong to be true, and it was always too weak to be the
    // point. Too strong, because the renderer refuses every mode at or above
    // 0.98 of Nyquist, and on the smallest tightest heads this sweep reaches
    // there is no membrane mode below it at all - the drum genuinely has no
    // membrane tone at that rate, and the honest report is that rather than a
    // number nothing can sound. Too weak, because "positive" never said the
    // number was a partial of the drum.
    //
    // So the clause is now an equivalence, checked at four sample rates and
    // against the bank the engine actually builds: the readout is positive
    // exactly where the renderer builds at least one membrane mode, and zero
    // exactly where it builds none. The original defect - a drum that sounds
    // reported as having no pitch - fails it as it always did, on any of the
    // 6336 drums below. Nothing here is relaxed: the drums that report no pitch
    // are drums with an empty bank, and that is asserted rather than assumed.
    // See testTheReadoutNamesAPartialTheRendererBuilds for the other half, which
    // pins *which* partial is named.
    long total = 0;
    long silent = 0;
    long wrong = 0;
    std::string firstBad;

    const auto rendersAnyMembraneMode = [] (const taikor::EngineParameters& parameters,
                                            int octave, double sampleRate)
    {
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (sampleRate, 256);
        engine.reset();
        engine.trigger (taikor::Articulation::Don, octave, 0.9f);
        return ! taikor::TaikoEngineTestAccess::membraneFrequencies (engine).empty();
    };

    const auto check = [&] (const taikor::EngineParameters& parameters,
                            const std::string& where)
    {
        constexpr double sampleRate = 48000.0;

        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
        {
            ++total;
            const auto reported =
                taikor::TaikoEngine::measure (parameters, octave, 0.0f, sampleRate)
                    .soundingHz;
            const bool sounds = rendersAnyMembraneMode (parameters, octave, sampleRate);

            if (! sounds)
                ++silent;

            if (std::isfinite (reported) && (reported > 0.0f) == sounds)
                continue;

            ++wrong;
            if (firstBad.empty())
                firstBad = std::to_string (reported) + " Hz at " + where
                         + ", octave " + std::to_string (octave)
                         + (sounds ? ", on a drum the renderer does sound"
                                   : ", on a drum with no membrane mode at all");
        }
    };

    // The case the zero-hertz defect was reported at, stated as itself so a
    // regression names it. Measured at 48 kHz: octaves 0, 1 and 2 report
    // 2466.33 / 5097.77 / 16793.35 Hz and each is that drum's own fundamental;
    // octave 3's fundamental is 25565.08 Hz, above the renderer's 23520 Hz
    // cutoff, so the bank is empty there and the readout says so.
    {
        auto parameters = defaultParameters();
        parameters.headDiameter = 0.15f;
        parameters.tension = 1.0f;
        parameters.headMaterial = 0.0f;
        parameters.pitch = 12.0f;
        parameters.octaveBody = 1.0f;

        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
        {
            const auto measurements =
                taikor::TaikoEngine::measure (parameters, octave, 0.0f, 48000.0);
            const auto reported = measurements.soundingHz;
            const bool sounds = rendersAnyMembraneMode (parameters, octave, 48000.0);

            expect (std::isfinite (reported) && (reported > 0.0f) == sounds,
                    "a 15 cm head at the tension ceiling reports "
                        + std::to_string (reported) + " Hz at octave "
                        + std::to_string (octave) + ", where the renderer builds "
                        + (sounds ? "a membrane bank" : "no membrane mode"));

            // And where it reports a pitch it is a mode of that drum rather than
            // any number at all. On a head this small and this tight the
            // mounting cannot reach the fundamental, so the fundamental is what
            // it is heard at - the two agree to four decimal places at every
            // octave that has one here.
            if (reported > 0.0f)
                expect (std::abs (1200.0 * std::log2 (reported
                                                      / measurements.loadedFundamentalHz))
                            < 50.0,
                        "the reported pitch is not one of this drum's modes: "
                            + std::to_string (reported) + " against a fundamental of "
                            + std::to_string (measurements.loadedFundamentalHz)
                            + " Hz at octave " + std::to_string (octave));
        }
    }

    // And the sweep the case was found in. Coarser than the 891000-point one
    // quoted in the plan - that one takes a minute - but over the same corners,
    // and it lands on the same failures.
    for (const float diameter : { 0.15f, 0.60f, 1.50f, 1.80f })
        for (const float tension : { 0.0f, 0.5f, 1.0f })
            for (const float material : { 0.0f, 0.5f, 1.0f })
                for (const float pitch : { -24.0f, 0.0f, 12.0f, 24.0f })
                    for (const float body : { 0.0f, 1.0f })
                        for (const float damping : { 0.0f, 1.0f })
                            for (const float coupling : { 0.0f, 1.0f })
                            {
                                auto parameters = defaultParameters();
                                parameters.headDiameter = diameter;
                                parameters.tension = tension;
                                parameters.headMaterial = material;
                                parameters.pitch = pitch;
                                parameters.octaveBody = body;
                                parameters.headDamping = damping;
                                parameters.cavityCoupling = coupling;
                                check (parameters,
                                       "diameter " + std::to_string (diameter)
                                           + ", tension " + std::to_string (tension)
                                           + ", material " + std::to_string (material)
                                           + ", pitch " + std::to_string (pitch)
                                           + ", body " + std::to_string (body)
                                           + ", damping " + std::to_string (damping)
                                           + ", coupling " + std::to_string (coupling));
                            }

    // The stroke and the microphones, at the extremes of the drum.
    for (const float diameter : { 0.15f, 1.80f })
        for (const float tension : { 0.0f, 1.0f })
            for (const float position : { -1.0f, 0.0f, 1.0f })
                for (const float spread : { 0.0f, 0.55f, 1.0f })
                    for (const float distance : { 0.0f, 1.0f })
                        for (const float width : { 0.0f, 0.5f, 1.0f })
                            for (const float pitch : { -24.0f, 24.0f })
                            {
                                auto parameters = defaultParameters();
                                parameters.headDiameter = diameter;
                                parameters.tension = tension;
                                parameters.strikePosition = position;
                                parameters.micSpread = spread;
                                parameters.micDistance = distance;
                                parameters.stereoWidth = width;
                                parameters.pitch = pitch;
                                check (parameters,
                                       "diameter " + std::to_string (diameter)
                                           + ", tension " + std::to_string (tension)
                                           + ", strike " + std::to_string (position)
                                           + ", spread " + std::to_string (spread)
                                           + ", distance " + std::to_string (distance)
                                           + ", width " + std::to_string (width)
                                           + ", pitch " + std::to_string (pitch));
                            }

    expect (wrong == 0,
            std::to_string (wrong) + " of " + std::to_string (total)
                + " drums disagree with their own bank about whether they have a "
                  "pitch; the first is " + firstBad);
    // Recorded, because a change that quietly stopped excluding anything would
    // move it: 98 of the 6336 have no membrane mode at all at 48 kHz, every one
    // of them a head of 15 cm carried up the keyboard or transposed two octaves
    // sharp. The zero-hertz defect this test was written for showed on 340 of
    // them, and on drums that sound.
    expect (silent > 0 && silent < total / 20,
            "the number of drums with no membrane mode at 48 kHz is "
                + std::to_string (silent) + " of " + std::to_string (total));
}

// Drum Layout has two physical meanings. Keep the old float in the engine-facing
// parameter block for session/API compatibility, but collapse every value onto
// the nearest endpoint before it can select geometry or invalidate a drum cache.
void testDrumLayoutHasOnlyPhysicalEndpoints()
{
    const auto sameDrum = [] (
        const taikor::TaikoEngineTestAccess::TuningPathMeasurement& left,
        const taikor::TaikoEngineTestAccess::TuningPathMeasurement& right)
    {
        return left.radiusMetres == right.radiusMetres
            && left.tensionNewtonsPerMetre == right.tensionNewtonsPerMetre
            && left.loadedFundamentalHz == right.loadedFundamentalHz
            && left.tuningHz == right.tuningHz;
    };

    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto at = [octave] (float rawLayout)
        {
            auto parameters = defaultParameters();
            parameters.octaveBody = rawLayout;
            return taikor::TaikoEngineTestAccess::tuningPathMeasurement (parameters,
                                                                          octave);
        };
        const auto oneDrum = at (0.0f);
        const auto fourDrums = at (1.0f);

        for (const float raw : { -1.0f, 0.24f, 0.4999f })
            expect (sameDrum (at (raw), oneDrum),
                    "a low legacy Drum Layout value did not resolve as 1 Drum at "
                        "octave " + std::to_string (octave) + ": "
                        + std::to_string (raw));
        for (const float raw : { 0.5f, 0.76f, 2.0f })
            expect (sameDrum (at (raw), fourDrums),
                    "a high legacy Drum Layout value did not resolve as 4 Drums at "
                        "octave " + std::to_string (octave) + ": "
                        + std::to_string (raw));
    }

    // Quantisation belongs before cache invalidation as well as before the solve:
    // redundant legacy automation inside one layout must not rebuild four drums.
    {
        auto parameters = defaultParameters();
        parameters.octaveBody = 0.10f;
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        const auto oneDrumRevision =
            taikor::TaikoEngineTestAccess::physicalConfigurationRevision (engine);

        parameters.octaveBody = 0.40f;
        engine.setParameters (parameters);
        expect (taikor::TaikoEngineTestAccess::physicalConfigurationRevision (engine)
                    == oneDrumRevision,
                "automation inside 1 Drum invalidated the physical drum cache");

        parameters.octaveBody = 0.50f;
        engine.setParameters (parameters);
        const auto fourDrumRevision =
            taikor::TaikoEngineTestAccess::physicalConfigurationRevision (engine);
        expect (fourDrumRevision == oneDrumRevision + 1,
                "crossing into 4 Drums did not invalidate the physical drum cache once");

        parameters.octaveBody = 0.90f;
        engine.setParameters (parameters);
        expect (taikor::TaikoEngineTestAccess::physicalConfigurationRevision (engine)
                    == fourDrumRevision,
                "automation inside 4 Drums invalidated the physical drum cache");
    }

    // The endpoints keep their recorded physical meanings. 1 Drum's tension
    // ladder is x13.53 / x4.05 / x4.00 and its radius never moves; 4 Drums uses
    // the four diameters the instrument table names.
    {
        auto oneDrum = defaultParameters();
        oneDrum.octaveBody = 0.0f;
        auto fourDrums = defaultParameters();
        fourDrums.octaveBody = 1.0f;

        const double ladder[3] = { 13.5293, 4.0517, 4.0000 };
        double previousTension = 0.0;

        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
        {
            const auto atOneDrum = taikor::TaikoEngine::measure (oneDrum, octave);
            expect (std::abs (atOneDrum.radiusMetres - 0.75f) < 1.0e-4f,
                    "1 Drum moved the drum's size at octave " + std::to_string (octave));

            if (previousTension > 0.0)
            {
                const auto index =
                    static_cast<std::size_t> (octave - taikor::lowestOctaveOffset - 1);
                const double ratio = atOneDrum.tensionNewtonsPerMetre / previousTension;
                expect (std::abs (ratio - ladder[index]) < ladder[index] * 0.001,
                        "1 Drum's tension ladder step into octave "
                            + std::to_string (octave) + " is "
                            + std::to_string (ratio) + ", recorded as "
                            + std::to_string (ladder[index]));
            }
            previousTension = atOneDrum.tensionNewtonsPerMetre;

            const auto& description = taikor::getDrumDescription (octave);
            expect (std::abs (2.0f * taikor::TaikoEngine::measure (fourDrums, octave)
                                         .radiusMetres
                              - description.headDiameterMetres)
                        < 0.005f,
                    "4 Drums is not the instrument the table names at octave "
                        + std::to_string (octave));
        }
    }
}

// Whatever the readout names, the renderer has to be able to build it.
//
// configureResonator refuses every mode at or above 0.98 of Nyquist and
// buildVoiceModes drops it before it gets that far, so the set of partials that
// exist in the audio is a function of the host's clock. The comparison that
// picks the reported pitch used to run over the whole modal bank regardless, and
// on a small head at the tension ceiling that let it name a partial nothing
// would ever sound: at Head Diameter 15 cm, Head Tension 1.0, Head Material 0
// and Pitch +12, the top pad's own fundamental is 25565.09 Hz and the panel
// printed it at a 48 kHz host, where nothing at or above 23520 Hz is
// instantiated.
//
// This is measured against the bank the engine actually builds - triggered at
// the rate in question and read out of the voice - rather than against the same
// arithmetic that produced the number, so a readout that agreed with itself and
// with nothing else fails it.
void testTheReadoutNamesAPartialTheRendererBuilds()
{
    // The degenerate pairs are split by up to 0.24 %, and the lower member of
    // each pair is always the one below the undetuned frequency the comparison
    // ranks - so a mode the comparison admits is always built, and the built
    // copy can sit a quarter of a per cent low. Half a per cent is that with
    // room and is far tighter than the gap between any two modes of this head.
    constexpr double splitTolerance = 0.005;

    const auto builtMembraneModes = [] (const taikor::EngineParameters& parameters,
                                        int octave, double sampleRate)
    {
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (sampleRate, 256);
        engine.reset();
        engine.trigger (taikor::Articulation::Don, octave, 0.9f);
        return taikor::TaikoEngineTestAccess::membraneFrequencies (engine);
    };

    long total = 0;
    long silent = 0;
    std::string firstBad;

    const auto check = [&] (const taikor::EngineParameters& parameters,
                            double sampleRate, const std::string& where)
    {
        const auto ceiling = 0.98 * 0.5 * sampleRate;

        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
        {
            ++total;
            const auto reported =
                taikor::TaikoEngine::measure (parameters, octave, 0.0f, sampleRate)
                    .soundingHz;
            const auto built = builtMembraneModes (parameters, octave, sampleRate);

            const auto fail = [&] (const std::string& what)
            {
                if (firstBad.empty())
                    firstBad = what + " at " + where + ", octave "
                             + std::to_string (octave) + ", "
                             + std::to_string (sampleRate) + " Hz";
                expect (false, what + " at " + where + ", octave "
                                   + std::to_string (octave) + ", "
                                   + std::to_string (sampleRate) + " Hz");
            };

            if (! std::isfinite (reported) || reported < 0.0f)
            {
                fail ("the readout is not a frequency: " + std::to_string (reported));
                continue;
            }

            if (reported <= 0.0f)
            {
                // No pitch reported. That is only honest if the renderer really
                // does build no membrane mode here - see the plan for why "no
                // membrane tone at this sample rate" is reported as its own
                // state rather than as the lowest mode of a drum that cannot be
                // heard.
                ++silent;
                if (! built.empty())
                    fail ("the readout reports no pitch on a drum whose bank has "
                          + std::to_string (built.size()) + " membrane modes, the "
                          "lowest at " + std::to_string (built.front()) + " Hz");
                continue;
            }

            if (built.empty())
            {
                fail ("the readout reports " + std::to_string (reported)
                      + " Hz on a drum the renderer builds no membrane mode for");
                continue;
            }

            if (! (static_cast<double> (reported) < ceiling))
            {
                fail ("the readout reports " + std::to_string (reported)
                      + " Hz, at or above the renderer's cutoff of "
                      + std::to_string (ceiling));
                continue;
            }

            bool matched = false;
            for (const float frequency : built)
                if (std::abs (static_cast<double> (frequency)
                              / static_cast<double> (reported) - 1.0) < splitTolerance)
                    matched = true;

            if (! matched)
                fail ("the readout reports " + std::to_string (reported)
                      + " Hz, which is not a mode the renderer built");
        }
    };

    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        // The case that was reported, stated as itself so a regression names it.
        // Measured: octaves 0, 1 and 2 report 2466.33 / 5097.77 / 16793.35 Hz at
        // every rate; octave 3's own fundamental is 25565.09 Hz, which is above
        // the cutoff at 44.1 and 48 kHz and below it at 96 and 192, so the
        // readout has a pitch there at the two high rates and none at the two
        // low ones.
        {
            auto parameters = defaultParameters();
            parameters.headDiameter = 0.15f;
            parameters.tension = 1.0f;
            parameters.headMaterial = 0.0f;
            parameters.pitch = 12.0f;
            parameters.octaveBody = 1.0f;
            check (parameters, sampleRate, "the reported 15 cm head");

            const auto top =
                taikor::TaikoEngine::measure (parameters, 3, 0.0f, sampleRate).soundingHz;
            const bool rendersHere = sampleRate > 2.0 * 25565.09 / 0.98;
            expect (rendersHere ? std::abs (top - 25565.09f) < 1.0f : ! (top > 0.0f),
                    "the reported case's top pad reads " + std::to_string (top)
                        + " Hz at " + std::to_string (sampleRate)
                        + " Hz, where its only membrane mode is 25565.09");
        }

        // And the corners the case was found in.
        for (const float diameter : { 0.15f, 0.60f, 1.50f, 1.80f })
            for (const float tension : { 0.0f, 0.5f, 1.0f })
                for (const float material : { 0.0f, 1.0f })
                    for (const float pitch : { -24.0f, 0.0f, 12.0f, 24.0f })
                        for (const float body : { 0.0f, 1.0f })
                        {
                            auto parameters = defaultParameters();
                            parameters.headDiameter = diameter;
                            parameters.tension = tension;
                            parameters.headMaterial = material;
                            parameters.pitch = pitch;
                            parameters.octaveBody = body;
                            check (parameters, sampleRate,
                                   "diameter " + std::to_string (diameter)
                                       + ", tension " + std::to_string (tension)
                                       + ", material " + std::to_string (material)
                                       + ", pitch " + std::to_string (pitch)
                                       + ", body " + std::to_string (body));
                        }
    }

    expect (firstBad.empty(),
            "the readout named something the renderer does not build; the first is "
                + firstBad);
    // The count is recorded because a change that quietly stopped excluding
    // anything would move it: 68 of the swept drums have no membrane mode at all
    // at 44.1 kHz, 44 at 48 kHz, 4 at 96 kHz and none at 192 kHz - 116 in all
    // over the four rates, out of 1540 measurements.
    expect (silent > 0 && silent < total / 4,
            "the number of drums with no membrane tone at their sample rate is "
                + std::to_string (silent) + " of " + std::to_string (total));
}

// Two mechanisms this instrument leaves out on purpose, each guarded by the
// arithmetic that says it is inaudible rather than by a remembered figure.
//
// The enclosed air is a lossless spring. Wall thermal exchange gives a real
// cavity a loss factor, and this recomputes Kirchhoff's boundary-layer result
// from whatever drum the controls resolve to, weights it by the share of each
// axisymmetric branch's stiffness the cavity actually holds, and requires the
// decay it would add to stay far under anything a listener could reach. It also
// pins the fact that Body Depth already has authority over the tail - through
// the frequency dependence of the mounting, radiation and hide losses, not
// through any loss of the air's own - because the README used to say it had
// none.
// The wooden body moves with the pair. Mic Distance used to change the head's
// near field and the head's continuum and leave the shell at one fixed level,
// so backing the capsules off thinned the drum around a body that never moved.
// A ring mode is now read the way a membrane mode is - an evanescent term at
// its own circumferential wavenumber n/R over the wall-to-capsule path, plus
// the same proximity lift and propagating share - taken as a ratio against
// this drum's own capsule distance at the factory Mic Distance.
//
// Three things have to hold, and the first is what keeps shellCalibration
// meaning what it was pinned to mean.
void testTheBodyMovesWithThePair()
{
    // 1. Exactly nothing happens at the factory position, on every drum and
    //    every ring mode. Per drum, because the capsules are scaled closer to
    //    the small heads: a single fixed reference passes this on the o-daiko
    //    and fails it on the okedo and the shime.
    for (int octave = taikor::lowestOctaveOffset;
         octave <= taikor::highestOctaveOffset; ++octave)
    {
        const auto gains = taikor::TaikoEngineTestAccess::shellPerspective (
            octave, 0.35f);
        for (std::size_t ring = 0; ring < gains.size(); ++ring)
            expect (std::abs (gains[ring] - 1.0f) < 1.0e-5f,
                    "the shell moved at the factory Mic Distance on octave "
                        + std::to_string (octave) + ", ring order "
                        + std::to_string (ring + 2) + ": gain "
                        + std::to_string (gains[ring]));
    }

    // 2. It does move everywhere else, monotonically, and in the right
    //    direction: closer is louder. Measured on the okedo, whose light stave
    //    shell is the drum the body is most audible on, the lowest ring mode
    //    runs +6.5 dB at the closest position and -16.0 dB at the furthest.
    const auto close = taikor::TaikoEngineTestAccess::shellPerspective (2, 0.0f);
    const auto mid = taikor::TaikoEngineTestAccess::shellPerspective (2, 0.35f);
    const auto far = taikor::TaikoEngineTestAccess::shellPerspective (2, 1.0f);
    expect (close[0] > mid[0] * 1.5f && far[0] < mid[0] * 0.5f,
            "Mic Distance no longer moves the okedo's lowest ring mode: "
                + std::to_string (close[0]) + " / " + std::to_string (mid[0])
                + " / " + std::to_string (far[0]));
    for (std::size_t ring = 0; ring < close.size(); ++ring)
        expect (close[ring] >= mid[ring] && mid[ring] >= far[ring],
                "the shell's perspective is not monotone in Mic Distance at "
                "ring order " + std::to_string (ring + 2));

    // 3. The low ring modes move far more than the high ones, which is the
    //    physical signature rather than a taper: the evanescent rate is
    //    sqrt((n/R)^2 - (w/c)^2), and a ring mode's frequency climbs as about
    //    n^2 while its wavenumber climbs as n, so the high modes are already
    //    propagating and barely care where the pair stands.
    expect (far[0] < far[close.size() - 1] * 0.5f,
            "the shell's highest ring mode now falls off with distance as fast "
            "as its lowest: " + std::to_string (far[0]) + " against "
                + std::to_string (far[close.size() - 1]));

    // 4. And it reaches the rendered bank, on the stroke that has a wooden
    //    bank to reach. A head-only stroke has no shell drive at all, so its
    //    wooden projection stays at zero however the pair is placed - the
    //    perspective must not have quietly given Don, Ka or Tsu a body.
    const auto woodAt = [] (taikor::Articulation articulation, float distance)
    {
        auto parameters = defaultParameters();
        parameters.humanise = 0.0f;
        parameters.micDistance = distance;
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (articulation, 2, 0.95f);
        return taikor::TaikoEngineTestAccess::woodDrive (engine);
    };
    expect (woodAt (taikor::Articulation::DonRim, 0.0f)
                > woodAt (taikor::Articulation::DonRim, 1.0f) * 1.5,
            "a rim shot's wooden bank did not recede as the pair backed off");
    for (const float distance : { 0.0f, 0.35f, 1.0f })
        expect (woodAt (taikor::Articulation::Don, distance) == 0.0,
                "the shell perspective gave a head-only stroke a body");
}

void testTheEnclosedAirIsLosslessOnlyWhereThatIsInaudible()
{
    const auto probe = taikor::TaikoEngineTestAccess::probeCavityLoss();

    expect (probe.branchesScanned > 100,
            "the cavity-loss scan found almost no axisymmetric branches: "
                + std::to_string (probe.branchesScanned));

    // Measured worst case over four octaves x eleven Body Depths x three Air
    // Couplings is 1.49 % of the branch's own decay, on the shallowest small
    // drum at full coupling. Two per cent leaves room for the family to move
    // without leaving room for the omission to become audible: at two per cent
    // of a one-second tail the T60 moves by 20 ms.
    expect (probe.worstFractionOfDecay < 0.02,
            "the enclosed air's own thermoviscous loss would now be "
                + std::to_string (100.0 * probe.worstFractionOfDecay)
                + " % of an axisymmetric branch's decay ("
                + std::to_string (probe.worstBranchHz) + " Hz, T60 "
                + std::to_string (probe.worstT60Seconds) + " s -> "
                + std::to_string (probe.worstT60WithLossSeconds)
                + " s), which is no longer negligible");

    // The scan has to be finding a cavity to be worth anything: if a change
    // decoupled every branch from the air, the fraction above would go to zero
    // for the wrong reason.
    expect (probe.worstCavityStiffnessShare > 0.05,
            "no axisymmetric branch stores a meaningful share of its stiffness "
            "in the cavity, so the bound above proves nothing: share "
                + std::to_string (probe.worstCavityStiffnessShare));
    expect (probe.worstLossFactor > 1.0e-5 && probe.worstLossFactor < 1.0e-2,
            "the cavity loss factor left the range boundary-layer theory puts "
            "it in: " + std::to_string (probe.worstLossFactor));

    // Body Depth moves the breathing branch's tail by about 14 % on the factory
    // drum (0.893 s at the shallowest, 1.016 s in the middle, 0.941 s at the
    // deepest). It is not a monotone control: radiation falls as the branch
    // comes down in frequency while the mounting loss rises towards its corner.
    expect (probe.breathingTailSpread > 1.05,
            "Body Depth no longer moves the breathing branch's decay at all; "
            "the spread is " + std::to_string (probe.breathingTailSpread));
}

// The bachi is a point force, not a contact patch with a radius that grows with
// the force. A finite patch would low-pass the modal bank spatially by
// 2 J1(k a_c) / (k a_c). This recomputes that factor from the engine's own
// contact solve and requires it to stay inaudible on the four instruments the
// family table describes - and separately records how far the answer moves when
// the one quantity the engine does not carry, the bachi's tip radius, is
// halved. That sensitivity is the finding: the size of the effect is set by a
// number that would have to be drawn.
void testTheContactPatchWouldNotBeAudibleOnTheResolvedBank()
{
    // A 12 mm tip is the dowel the retired stick model described.
    const auto probe = taikor::TaikoEngineTestAccess::probeContactPatch (0.012);

    // Measured 0.0330 dB at the factory controls, worst over the four strokes
    // on the shime - the smallest of the four instruments and therefore the one
    // with the highest resolved wavenumber. A plain Don there is 0.0122 dB, and
    // the o-daiko is 0.0025 dB. A tenth of a decibel leaves the family room to
    // move and is still an order of magnitude under anything audible on one
    // partial.
    expect (probe.worstFamilyDb < 0.1,
            "a finite contact patch would now attenuate the top of the resolved "
            "bank by " + std::to_string (probe.worstFamilyDb)
                + " dB on a family instrument");

    // The patch reaches 16.7 % of the tip at the factory hardness, inside the
    // small-deformation regime the Hertz solution is derived under. It does not
    // stay there at Bachi Hardness 0, where it reaches 51 % on the o-daiko and
    // 72 % against a 6 mm tip, which is the second reason the law would need a
    // cap that is itself drawn.
    expect (probe.worstFamilyPatchFraction < 0.35,
            "the Hertz patch reached "
                + std::to_string (100.0 * probe.worstFamilyPatchFraction)
                + " % of the tip radius at the factory controls, outside the "
                  "small-deformation limit the solution assumes");

    // a_c goes as the square root of the tip radius, so halving the tip takes
    // k a_c down by 1/sqrt(2) and the attenuation - which is x^2/8 to leading
    // order - down by half: 0.0330 dB against 0.0165 dB. Requiring a real
    // difference keeps that sensitivity visible. If it ever stopped mattering,
    // the effect would be derivable from what the engine carries after all.
    expect (probe.familyDbAtHalfTip < probe.worstFamilyDb * 0.75,
            "the contact patch's effect stopped depending on the tip radius the "
            "engine does not carry: "
                + std::to_string (probe.worstFamilyDb) + " dB at 12 mm against "
                + std::to_string (probe.familyDbAtHalfTip) + " dB at 6 mm");

    // Outside the family the controls reach a 3 cm head struck with the softest
    // beater, where the same factor is worth about 2.9 dB. It is recorded so a
    // future attempt knows where the mechanism does bite; it is not a taiko.
    expect (probe.worstReachableDb > probe.worstFamilyDb * 10.0
                && probe.worstReachableDb < 12.0,
            "the reachable-corner contact patch attenuation is "
                + std::to_string (probe.worstReachableDb) + " dB");
}

void testTheDrumIsTunedByThePitchItSounds()
{
    const auto measure = [] (taikor::EngineParameters parameters, int octave)
    { return taikor::TaikoEngine::measure (parameters, octave); };

    // The clause the step is for. Every octave boundary, in both layouts.
    //
    // Measured in the pitch the drum is heard at rather than in its loaded
    // fundamental, and that is the contract this test now states. The two are
    // the same quantity on the okedo and the shime and they are not on the
    // o-daiko and the chu-daiko, whose fundamentals displace no net air and are
    // emptied by the mounting long before anyone has taken a pitch from them:
    // what is left ringing there is the (1,1) mode a fifth and a half above.
    // Putting the four fundamentals on exact octaves - which the tree before
    // this step did, to a hundredth of a cent - left the four sounding pitches
    // stepping 0 / 11.7 / 14.3 / 26.3 semitones.
    for (const float body : { 0.0f, 1.0f })
    {
        auto tuned = defaultParameters();
        tuned.octaveBody = body;

        for (int octave = taikor::lowestOctaveOffset;
             octave < taikor::highestOctaveOffset; ++octave)
        {
            const auto lower = measure (tuned, octave).soundingHz;
            const auto upper = measure (tuned, octave + 1).soundingHz;
            const auto cents = 1200.0f * std::log2 (upper / lower);

            expect (std::abs (cents - 1200.0f) < 20.0f,
                    "the octave into " + std::to_string (octave + 1)
                        + " is " + std::to_string (cents)
                        + " cents in Drum Layout " + std::to_string (body));
        }
    }

    // The anchor. The transform is the identity at octave 0 in both layouts, so
    // the reference octave must not move at all. Without this a solve that met
    // every ratio above by
    // moving the whole keyboard down would pass the lot.
    for (const float body : { 0.0f, 1.0f })
    {
        auto tuned = defaultParameters();
        tuned.octaveBody = body;
        const auto reported = measure (tuned, 0);

        expect (std::abs (reported.soundingHz - 59.7474f) < 0.01f,
                "the reference octave's sounding pitch moved in Drum Layout "
                    + std::to_string (body));
        expect (std::abs (reported.loadedFundamentalHz - 32.6503f) < 0.01f,
                "the reference octave's loaded fundamental moved in Drum Layout "
                    + std::to_string (body));
        expect (std::abs (reported.radiusMetres - 0.7500f) < 1.0e-4f,
                "the reference octave is not the same drum in Drum Layout "
                    + std::to_string (body));
    }

    // Drum Layout has to keep its meaning. In 1 Drum the size never moves and
    // the whole octave comes out of the tension. In 4 Drums each octave is the
    // instrument the table describes, so its tension stays that instrument's
    // own to the last place: residual tuning is taken as size there, and the
    // solve is not free to buy any of it on the other axis.
    //
    // 7284.35 / 5834.53 / 11467.18 / 19110.93 N/m are the four drums' tensions as
    // the table states them, mapped through the control's own geometric range.
    // They are the numbers a maker would read off the instruments: the two
    // tacked drums at much the same tension as each other and the two laced ones
    // far above them.
    {
        const float tensions[4] = { 7284.35f, 5834.53f, 11467.18f, 19110.93f };

        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
        {
            auto bySize = defaultParameters();
            bySize.octaveBody = 1.0f;
            auto byTension = defaultParameters();
            byTension.octaveBody = 0.0f;

            const auto index =
                static_cast<std::size_t> (octave - taikor::lowestOctaveOffset);

            expect (std::abs (measure (byTension, octave).radiusMetres - 0.7500f)
                        < 1.0e-4f,
                    "1 Drum changed the drum's size at octave "
                        + std::to_string (octave));
            expect (std::abs (measure (bySize, octave).tensionNewtonsPerMetre
                              - tensions[index])
                        < tensions[index] * 1.0e-3f,
                    "4 Drums did not leave the drum on its own tension at "
                    "octave " + std::to_string (octave) + ": "
                        + std::to_string (
                              measure (bySize, octave).tensionNewtonsPerMetre));
        }
    }

    // A guard, and it says so: Pitch is head tension and does not touch the
    // radius, so it was already good to a cent and this clause cannot fail on
    // the tree it landed with. It is here because the bisection is free to meet
    // every clause above by redefining what the transform does to the pitch
    // control as well, and that would be caught nowhere else.
    //
    // Stated in the loaded fundamental, which is the quantity Pitch moves
    // exactly, and not in the pitch the drum is heard at. Those part company on
    // the reference drum somewhere around eight semitones up, where the
    // fundamental finally climbs clear of the mounting and becomes the loudest
    // mode the drum has: an octave of Pitch really does hand the o-daiko from
    // its (1,1) mode to its own fundamental, and the strongest partial really
    // does go 59.7 -> 84.4 -> 94.7 -> 65.3 Hz across that range in the rendered
    // audio. That is a property of the instrument rather than of this step - the
    // shipping tree does the same thing, 89.0 -> 101.5 Hz over an octave of
    // Pitch - and it is not what this clause was written to catch.
    {
        const auto centre = measure (defaultParameters(), 0).loadedFundamentalHz;

        for (const float semitones : { -12.0f, 12.0f })
        {
            auto tuned = defaultParameters();
            tuned.pitch = semitones;
            const auto cents =
                1200.0f * std::log2 (measure (tuned, 0).loadedFundamentalHz / centre);
            expect (std::abs (cents - 100.0f * semitones) < 20.0f,
                    "pitch " + std::to_string (semitones)
                        + " semitones moved the drum by " + std::to_string (cents)
                        + " cents");
        }
    }

    // And the audio, because everything above reads measure() and an
    // implementation that moved the readout alone would pass all of it.
    //
    // Do no harm: the reported sounding pitch has to be where the peak is.
    // Twelve per cent of a band around it, and the reported frequency may not be
    // far under the strongest bin in it.
    //
    // A full open stroke, and not the dead-centre one this clause used to use:
    // at radius zero every mode with a circumferential order has J_m(0) = 0, and
    // on the two large drums of the family the mode the drum is heard at is one
    // of those. Asking a stroke that cannot drive it where it is would be asking
    // about the stroke.
    auto offCentreOnly = defaultParameters();
    offCentreOnly.humanise = 0.0f;

    for (int octave = taikor::lowestOctaveOffset; octave <= taikor::highestOctaveOffset;
         ++octave)
    {
        const auto reported = measure (offCentreOnly, octave);

        // Below twenty hertz there is no audible pitch to compare, and a quarter
        // of a second cannot resolve one either. testOctavesRaisePitch skips the
        // same octaves for the same reason.
        if (reported.soundingHz < 20.0f)
            continue;

        const auto mono =
            strike (offCentreOnly, taikor::Articulation::Don, octave, 0.9f, 48000.0,
                    36000)
                .mono();
        const std::size_t first = 2400, last = 2400 + 24000;
        const auto atReported = binMagnitude (mono, reported.soundingHz,
                                              48000.0, first, last);
        const auto peak = binMagnitude (
            mono,
            dominantFrequency (mono, 48000.0, reported.soundingHz * 0.94,
                               reported.soundingHz * 1.06,
                               reported.soundingHz * 0.0005, first, last),
            48000.0, first, last);

        // A decibel and a half rather than nine tenths, and what costs it is
        // the attack glide, as it always did: the window opens fifty
        // milliseconds after a stroke that starts the head sharp, and on the
        // chu-daiko - the slackest head of the family, so the one that stretches
        // furthest - the drum has not fully settled back, which puts the
        // strongest bin six cents above the pitch it is tuned to. Measured
        // 0.978 / 0.858 / 0.994 / 0.997 over the four drums.
        expect (atReported > peak * 0.8500, // -1.41 dB
                "the rendered sounding pitch is not at the reported one for octave "
                    + std::to_string (octave) + ": "
                    + std::to_string (atReported / peak));
    }

    // Search 8–900 Hz without consulting the pitch readout, averaging POWER
    // across independent residual realizations while the mechanical stroke is
    // fixed. A single random bin can constructively overlap a weaker partial:
    // changing contact history made a quieter Nagado residual briefly rank
    // 336 Hz over its unchanged 119.5 Hz tone. Waveform averaging would remove
    // noise instead; power averaging preserves its contribution and still
    // catches an incorrectly tuned deterministic partial.
    {
        double previous = 0.0;

        auto offCentre = defaultParameters();
        offCentre.humanise = 0.0f;

        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
        {
            std::array<std::vector<float>, 16> takes;
            for (std::size_t take = 0; take < takes.size(); ++take)
            {
                taikor::TaikoEngine engine;
                engine.setParameters (offCentre);
                engine.prepare (48000.0, defaultBlockSize);
                engine.reset();
                engine.trigger (taikor::Articulation::Don, octave, 0.92f);
                taikor::TaikoEngineTestAccess::setContinuumNoiseSeed (
                    engine, 0x1f123bb5u + static_cast<std::uint32_t> (take) * 0x9e3779b9u);
                takes[take] = render (engine, 24000).mono();
            }
            double strongest = 8.0, largestPower = -1.0;
            double strongestEight = 8.0, largestPowerEight = -1.0;
            for (double frequency = 8.0; frequency <= 900.0; frequency += 0.25)
            {
                double power = 0.0;
                for (std::size_t take = 0; take < takes.size(); ++take)
                {
                    const double magnitude = binMagnitude (
                        takes[take], frequency, 48000.0, 2400u, 2400u + 16384u);
                    power += magnitude * magnitude;
                    if (take == 7 && power > largestPowerEight)
                    {
                        largestPowerEight = power;
                        strongestEight = frequency;
                    }
                }
                if (power > largestPower)
                {
                    largestPower = power;
                    strongest = frequency;
                }
            }
            std::cout << "Global averaged-power pitch, octave " << octave
                      << ": eight=" << strongestEight << " Hz, sixteen="
                      << strongest << " Hz\n";

            if (previous > 0.0)
            {
                const auto cents = 1200.0 * std::log2 (strongest / previous);

                expect (std::abs (cents - 1200.0) < 20.0,
                        "the audible octave into " + std::to_string (octave)
                            + " is " + std::to_string (cents) + " cents");
            }

            previous = strongest;
        }
    }
}

void testEveryArticulationAndSampleRate()
{
    for (const double sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        {
            const auto articulation = static_cast<taikor::Articulation> (index);
            const auto name =
                std::string (taikor::getArticulationDisplayName (articulation));

            for (const int octave : { taikor::lowestOctaveOffset, 0,
                                      taikor::highestOctaveOffset })
            {
                const auto rendered = strike (defaultParameters(), articulation, octave,
                                              0.92f, sampleRate,
                                              static_cast<int> (sampleRate * 0.5));

                expect (rendered.finite,
                        name + " produced non-finite audio at "
                            + std::to_string (static_cast<int> (sampleRate)) + " Hz");
                expect (rendered.peak > 1.0e-4,
                        name + " produced silence at "
                            + std::to_string (static_cast<int> (sampleRate)) + " Hz");
                expect (rendered.peak <= 1.0001,
                        name + " exceeded full scale at "
                            + std::to_string (static_cast<int> (sampleRate)) + " Hz");
            }
        }
    }
}

// The drum a set of controls describes must not depend on the host's clock.
void testSampleRateConsistency()
{
    const auto parameters = defaultParameters();
    double referencePitch = 0.0;

    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto rendered = strike (parameters, taikor::Articulation::Don, 0, 0.9f,
                                      sampleRate, static_cast<int> (sampleRate * 0.6));
        const auto mono = rendered.mono();
        const auto dominant = dominantFrequency (
            mono, sampleRate, 50.0, 200.0, 0.25,
            static_cast<std::size_t> (sampleRate * 0.06));

        if (referencePitch <= 0.0)
            referencePitch = dominant;
        else
            expect (std::abs (dominant - referencePitch) < 1.0,
                    "the drum's pitch moved with the sample rate");

        expect (rendered.finite, "a supported sample rate produced non-finite audio");
    }

    // Hostile rates must be clamped rather than allowed to break the tuning.
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (1.0, 64);
    engine.trigger (taikor::Articulation::Don, 0, 0.9f);
    const auto rendered = render (engine, 512, 64);
    expect (rendered.finite, "an absurd sample rate must not produce non-finite audio");
}

void testContinuumFollowsModalImpulseDisplacement()
{
    using Access = taikor::TaikoEngineTestAccess;
    constexpr double pi = 3.14159265358979323846;
    constexpr double firstRoot = 2.4048255576957728;
    constexpr double bandwidth = 1.35;

    for (const float stiffness : { 0.0f, 0.0001f, 0.01f, 1000.0f })
    {
        const auto probe = Access::continuumPopulation (stiffness);
        expect (probe.bands.size() == 5,
                "the population probe needs all five statistical bands");
        if (probe.bands.size() != 5)
            continue;

        // Independent counting experiment: enumerate the actual modes of a
        // simply supported square with the same area as the circular head.
        // Its lambda^2 = pi (m^2 + n^2); the forward tension/bending dispersion
        // assigns a frequency to every mode. The boundary correction differs
        // between square and disc, while the leading Weyl area law is shared.
        // No quadratic inversion or production helper is used in this count.
        std::array<int, 5> counts {};
        std::array<double, 5> impulseEnergy {};
        const double lastHigh = probe.bands.back()[0] * bandwidth;
        const auto frequencyFor = [&probe] (int m, int n)
        {
            const double lambdaSquared = pi * static_cast<double> (m * m + n * n);
            return probe.waveSpeed / (2.0 * pi * probe.radius)
                 * std::sqrt (lambdaSquared
                     * (1.0 + probe.stiffness * lambdaSquared)
                     / (1.0 + probe.stiffness * firstRoot * firstRoot));
        };
        for (int m = 1; frequencyFor (m, 1) <= lastHigh; ++m)
            for (int n = 1; frequencyFor (m, n) <= lastHigh; ++n)
            {
                const double frequency = frequencyFor (m, n);
                for (std::size_t band = 0; band < counts.size(); ++band)
                    if (frequency >= probe.bands[band][0] / bandwidth
                        && frequency < probe.bands[band][0] * bandwidth)
                    {
                        ++counts[band];
                        // Unit-impulse displacement q = sin(omega t)/(M omega)
                        // has mean-square displacement proportional to 1/omega^2
                        // (this is signal power, not stored mechanical energy).
                        // Common modal mass, 2*pi and the phase average cancel
                        // in the ratio. This independently sums each mode's
                        // actual response, not count / band-centre squared.
                        impulseEnergy[band] += 1.0 / (frequency * frequency);
                    }
            }
        expect (counts[0] > 20,
                "the numerical population reference needs many individual modes");
        for (std::size_t band = 0; band < counts.size(); ++band)
        {
            const double gain = probe.bands[band][1] / probe.bands[0][1];
            const double frequencyRatio = probe.bands[band][0] / probe.bands[0][0];
            const double summedGain = std::sqrt (
                impulseEnergy[band] / std::max (impulseEnergy[0], 1.0e-20));
            expect (std::abs (gain / summedGain - 1.0) < 0.08,
                    "statistical band weight disagrees with summed squared impulse displacement");
            if (stiffness == 0.0f)
                expect (std::abs (gain - 1.0) < 2.0e-6,
                        "an ideal membrane's impulse response must have flat octave RMS");
            if (stiffness == 1000.0f)
                expect (std::abs (gain * std::sqrt (frequencyRatio) - 1.0) < 2.0e-4,
                        "a bending plate's octave impulse response must fall as 1/sqrt(f)");
            expect (std::isfinite (gain) && gain >= 1.0 / std::sqrt (frequencyRatio) - 1.0e-5
                    && gain <= 1.0 + 1.0e-5,
                    "mixed tension/bending response must lie between its physical limits");
        }
    }
}

// The pitch staying put is not the whole of rate independence. The statistical
// tail is driven by discrete white noise, so its filter variance has to be
// normalised explicitly; otherwise changing the host clock changes the drum's
// upper spectrum even though none of its physical parameters moved.
void testTheContinuumDoesNotDependOnTheSampleRate()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;
    parameters.tensionModulation = 0.0f;

    constexpr double windowSeconds = 0.085;

    struct Reading
    {
        double rate { 0.0 };
        double highWindowed { 0.0 };
        double low { 0.0 };
        double wide { 0.0 };
        double isolatedTop { 0.0 };
    };

    std::vector<Reading> readings;
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto samples = static_cast<int> (sampleRate * windowSeconds);
        constexpr int ensembleSize = 8;
        double highPower = 0.0;
        double widePower = 0.0;
        double isolatedPower = 0.0;

        for (int take = 0; take < ensembleSize; ++take)
        {
            const auto seed = 0x243f6a89u
                            + static_cast<std::uint32_t> (take) * 0x9e3779b9u;

            taikor::TaikoEngine full;
            full.setParameters (parameters);
            full.prepare (sampleRate, defaultBlockSize);
            full.trigger (taikor::Articulation::Don, 0, 0.92f);
            taikor::TaikoEngineTestAccess::setNoiseSeed (full, seed);
            const auto mono = render (full, samples).mono();
            highPower += std::pow (
                10.0, bandLevelHannDb (mono, sampleRate, 4000.0, 10000.0) / 10.0);
            widePower += std::pow (
                10.0, bandLevelDb (mono, sampleRate, 400.0, 16000.0) / 10.0);

            taikor::TaikoEngine isolated;
            isolated.setParameters (parameters);
            isolated.prepare (sampleRate, defaultBlockSize);
            isolated.trigger (taikor::Articulation::Don, 0, 0.92f);
            taikor::TaikoEngineTestAccess::setNoiseSeed (isolated, seed);
            const auto bands =
                taikor::TaikoEngineTestAccess::continuumBands (isolated);
            expect (bands.size() == 5,
                    "the reference drum must carry all five continuum octaves");
            if (bands.empty())
                return;

            const int top = static_cast<int> (bands.size()) - 1;
            taikor::TaikoEngineTestAccess::isolateContinuumBand (isolated, top);
            const auto topOnly =
                render (isolated, static_cast<int> (sampleRate * 0.020)).mono();
            isolatedPower += std::pow (
                10.0, bandLevelHannDb (topOnly, sampleRate,
                                       bands.back().centre / 1.35,
                                       bands.back().centre * 1.35) / 10.0);
        }

        // This clause is about the deterministic modal bank, so remove the
        // residual, contact texture, tack and direct-air paths rather than
        // attributing their finite random variance to the resonators.
        taikor::TaikoEngine resolved;
        resolved.setParameters (parameters);
        resolved.prepare (sampleRate, defaultBlockSize);
        resolved.trigger (taikor::Articulation::Don, 0, 0.92f);
        taikor::TaikoEngineTestAccess::isolateResolvedBank (resolved);
        const auto resolvedMono = render (resolved, samples).mono();

        const auto meanDb = [] (double power) {
            return 10.0 * std::log10 (
                std::max (power / static_cast<double> (ensembleSize), 1.0e-30));
        };
        readings.push_back ({ sampleRate,
                              meanDb (highPower),
                              bandLevelDb (resolvedMono, sampleRate, 40.0, 200.0),
                              meanDb (widePower),
                              meanDb (isolatedPower) });
    }

    const auto spread = [&readings] (double Reading::* band)
    {
        double lowest = 1.0e30;
        double highest = -1.0e30;
        for (const auto& reading : readings)
        {
            lowest = std::min (lowest, reading.*band);
            highest = std::max (highest, reading.*band);
        }
        return highest - lowest;
    };

    std::string rateReadings;
    for (const auto& reading : readings)
        rateReadings += " [" + std::to_string (reading.rate) + ": high "
                      + std::to_string (reading.highWindowed) + ", isolated "
                      + std::to_string (reading.isolatedTop) + ", low "
                      + std::to_string (reading.low) + ", wide "
                      + std::to_string (reading.wide) + "]";

    expect (spread (&Reading::highWindowed) < 1.5,
            "the 4-10 kHz response moved with the host sample rate"
                + rateReadings);
    // This is the direct discriminator: modes, click and contact texture have
    // been removed and the uppermost stochastic octave is measured alone.
    // Two decibels includes the finite variance of a 20 ms noise observation;
    // the old per-sample calibration moved roughly six per rate doubling.
    expect (spread (&Reading::isolatedTop) < 2.0,
            "an isolated continuum band changed energy with the host clock"
                + rateReadings);
    expect (spread (&Reading::low) < 0.5,
            "the resolved bank moved with the host sample rate");
    expect (spread (&Reading::wide) < 1.5,
            "the drum's whole voice above 400 Hz moved with the host sample rate");

    // The continuum is calibrated against the membrane as observed by the mic,
    // not against an unobserved displacement. Read a middle band alone so the
    // click cannot dilute the distance law being guarded.
    const auto isolatedMiddle = [&parameters] (float distance)
    {
        auto placed = parameters;
        placed.micDistance = distance;
        taikor::TaikoEngine engine;
        engine.setParameters (placed);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.92f);
        const auto bands = taikor::TaikoEngineTestAccess::continuumBands (engine);
        if (bands.size() < 3)
            return -300.0;
        taikor::TaikoEngineTestAccess::isolateContinuumBand (engine, 2);
        const auto mono = render (engine, 960).mono();
        return bandLevelDb (mono, 48000.0, bands[2].centre / 1.35,
                            bands[2].centre * 1.35);
    };

    const double distanceDrop = isolatedMiddle (0.0f) - isolatedMiddle (1.0f);
    expect (distanceDrop > 17.0 && distanceDrop < 20.0,
            "Mic Distance stopped attenuating the observed head continuum: "
                + std::to_string (distanceDrop));
}

// Closed-form forced-oscillator oracles do not replay the production recurrence.
// They exercise signed cancellation, exact held-force integration, and the
// independent-contact aggregate under the physical envelope's decay/scaling.
void testContinuumFollowsActualForceHistory()
{
    using Access = taikor::TaikoEngineTestAccess;
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
        for (const double sigma : { 0.0, 100.0, 1000.0 })
        {
            const int samples = static_cast<int> (std::round (rate * 0.001));
            const std::vector<double> rectangle (static_cast<std::size_t> (samples), 1.0);
            const auto result = Access::continuumForceProbe (rate, 1200.0, sigma, rectangle);
            const double measuredSigma = -std::log (result.decay) * rate;
            for (std::size_t node = 0; node < result.responses.size(); ++node)
            {
                const std::complex<double> lambda {
                    -measuredSigma, 2.0 * analysisPi * result.frequencies[node] };
                const auto expected = (std::exp (lambda * (samples / rate)) - 1.0)
                                    / lambda * result.decay;
                expect (std::abs (result.responses[node] - expected) < 2.0e-14,
                        "a continuum node lost the rectangular force's exact sinc/decay");
            }
            expect (result.aggregationError < 2.0e-5,
                    "coherent force power drifted from the canonical envelope");
            expect (result.resetPreservesTail,
                    "retiring a force accumulator removed its physical tail");
        }

    constexpr double rate = 48000.0;
    constexpr int delay = 24;
    std::vector<double> pulses (delay + 1, 0.0);
    pulses[0] = pulses[delay] = rate;
    const auto pair = Access::continuumForceProbe (rate, 1000.0, 0.0, pulses);
    for (std::size_t node = 0; node < pair.responses.size(); ++node)
    {
        const double omega = 2.0 * analysisPi * pair.frequencies[node];
        const std::complex<double> lambda { 0.0, omega };
        const auto single = rate * (std::exp (lambda / rate) - 1.0) / lambda;
        const double expected = std::norm (single)
            * (2.0 + 2.0 * std::cos (omega * delay / rate));
        expect (std::abs (std::norm (pair.responses[node]) - expected) < 2.0e-12,
                "separated positive forces did not interfere coherently");
    }
    expect (pair.largestRemoval > 0.5,
            "positive contact force can no longer cancel earlier upper-mode motion");

    std::vector<double> first (192, 1.0);
    std::vector<double> second (192, 0.0);
    std::fill (second.begin() + 15, second.begin() + 105, 0.8);
    const auto overlap = Access::continuumForceProbe (
        rate, 1000.0, 170.0, first, second, 70, 0.37f);
    expect (overlap.aggregationError < 3.0e-5,
            "overlapping force histories lost power after physical envelope scaling");

    const auto high = Access::continuumForceProbe (rate, 20000.0, 100.0, { 1.0 });
    expect (*std::max_element (high.frequencies.begin(), high.frequencies.end()) < 0.45 * rate,
            "continuum force quadrature aliases beyond its filter's represented edge");
}

// Stick/hide roughness belongs in the resonant object it excites. Sending it
// through the derivative pressure path as well cancels that path's one-pole
// contact-patch roll-off and creates a flat random shelf up to Nyquist.
void testContactRoughnessDoesNotBecomeAnAirborneNoiseShelf()
{
    const auto capture = [] (float strikeNoise, bool directOnly)
    {
        auto parameters = defaultParameters();
        parameters.humanise = 0.0f;
        parameters.tensionModulation = 0.0f;
        parameters.strikeNoise = strikeNoise;
        parameters.drive = 0.0f;

        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.9f);
        if (directOnly)
            taikor::TaikoEngineTestAccess::isolateDirectPath (engine);
        else
            taikor::TaikoEngineTestAccess::isolateContactDrivenObject (engine);
        return render (engine, 4096);
    };

    const auto directClean = capture (0.0f, true);
    const auto directRough = capture (1.0f, true);
    const double directDifference = std::max (
        maximumAbsoluteDifference (directClean.left, directRough.left),
        maximumAbsoluteDifference (directClean.right, directRough.right));
    expect (directClean.peak == 0.0 && directRough.peak == 0.0,
            "the removed force-derivative click returned outside the struck object");
    expect (directDifference == 0.0,
            "contact roughness leaked into the differentiated airborne pressure "
            "path: " + std::to_string (directDifference));

    const auto objectClean = capture (0.0f, false);
    const auto objectRough = capture (1.0f, false);
    const double objectDifference = std::max (
        maximumAbsoluteDifference (objectClean.left, objectRough.left),
        maximumAbsoluteDifference (objectClean.right, objectRough.right));
    expect (objectDifference > 1.0e-8,
            "removing the airborne noise shelf also removed roughness from the "
            "struck object");
}

// Dense high modes do not all retain one full-head radiation pattern. Their
// coherent patches shrink with wavelength, so backing the pair away must thin
// the top of the statistical tail more than its bottom while leaving its
// factory perspective untouched.
void testContinuumDistanceFollowsItsWavelength()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;
    parameters.tensionModulation = 0.0f;

    const auto capture = [&parameters] (float distance)
    {
        auto placed = parameters;
        placed.micDistance = distance;
        taikor::TaikoEngine engine;
        engine.setParameters (placed);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.92f);
        return taikor::TaikoEngineTestAccess::continuumBands (engine);
    };

    auto nearParameters = parameters;
    nearParameters.micDistance = 0.0f;
    auto farParameters = parameters;
    farParameters.micDistance = 1.0f;
    const auto nearGeometry =
        taikor::TaikoEngineTestAccess::continuumDistanceGeometry (nearParameters);
    const auto farGeometry =
        taikor::TaikoEngineTestAccess::continuumDistanceGeometry (farParameters);
    const auto near = capture (0.0f);
    const auto far = capture (1.0f);

    const auto factory = [&]
    {
        taikor::TaikoEngine engine;
        engine.setParameters (defaultParameters());
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.92f);
        return taikor::TaikoEngineTestAccess::continuumBands (engine);
    }();
    constexpr float factoryFirstTarget = 0.000396190967876f;
    expect (factory.size() == 5,
            "the factory continuum compatibility anchor lost an octave");
    // The continuum's population/impulse-response law now sets the relative
    // upper-band levels, checked independently against summed physical modes.
    // Its original first-band level remains the exact compatibility anchor.
    if (! factory.empty())
        expect (std::abs (factory[0].targetRms - factoryFirstTarget)
                    <= 2.0e-6f * factoryFirstTarget,
                "the continuum's first-band amplitude anchor changed");
    for (const auto& band : factory)
        expect (band.distanceGain == 1.0f,
                "the factory microphone position is not the distance law's unity point");

    const auto reference = capture (defaultParameters().micDistance);

    expect (near.size() == 5 && far.size() == near.size(),
            "the continuum distance probe needs all five unresolved octaves");
    if (near.size() != far.size() || near.size() != reference.size() || near.empty())
        return;

    double previousDrop = -1.0e30;
    double firstDrop = 0.0;
    double lastDrop = 0.0;
    for (std::size_t band = 0; band < near.size(); ++band)
    {
        expect (near[band].centre == far[band].centre,
                "Mic Distance moved a continuum band's frequency");
        for (const auto* placed : { &near[band], &far[band] })
            expect (std::abs (placed->targetRms / placed->distanceGain
                             / reference[band].targetRms - 1.0f) < 2.0e-6f,
                    "distance gain changed the underlying continuum-band target");

        const double coherentRadius = nearGeometry.waveSpeed
                                    / (2.0 * near[band].centre);
        const double expectedDrop = 10.0 * std::log10 (
            (coherentRadius * coherentRadius
                + farGeometry.micDistanceMetres * farGeometry.micDistanceMetres)
            / (coherentRadius * coherentRadius
                + nearGeometry.micDistanceMetres * nearGeometry.micDistanceMetres));
        const double actualDrop = 20.0 * std::log10 (
            std::max (static_cast<double> (near[band].targetRms), 1.0e-30)
            / std::max (static_cast<double> (far[band].targetRms), 1.0e-30));

        expect (std::abs (actualDrop - expectedDrop) < 1.0e-3,
                "a continuum octave did not follow its coherent-patch distance law");
        expect (actualDrop > previousDrop,
                "all continuum octaves inherited one microphone-distance gain");
        previousDrop = actualDrop;
        if (band == 0)
            firstDrop = actualDrop;
        if (band + 1 == near.size())
            lastDrop = actualDrop;
    }

    expect (lastDrop - firstDrop > 8.0,
            "backing the microphones away did not darken the unresolved head");
}

// A microphone is an observer, so moving it while the head is already ringing
// must move the existing statistical tail as well as the next strike. Preserve
// its filter history and physical decay, but remap its observed envelope by the
// same per-band gain used for a fresh contact.
void testContinuumDistanceMovesARingingTail()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;
    parameters.tensionModulation = 0.0f;
    parameters.micDistance = 0.0f;

    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);
    engine.trigger (taikor::Articulation::Don, 0, 0.92f);
    render (engine, 480);
    const auto before =
        taikor::TaikoEngineTestAccess::physicalContinuumState (engine);

    parameters.micDistance = 1.0f;
    engine.setParameters (parameters);
    taikor::TaikoEngineTestAccess::refreshPhysicalDrums (engine);
    const auto after =
        taikor::TaikoEngineTestAccess::physicalContinuumState (engine);

    expect (before.size() == 5 && after.size() == before.size(),
            "the live continuum distance remap lost a band");
    for (std::size_t band = 0; band < before.size() && band < after.size(); ++band)
    {
        const float expected = before[band].envelope
                             * after[band].distanceGain
                             / std::max (before[band].distanceGain, 1.0e-12f);
        expect (std::abs (after[band].envelope - expected)
                    <= 2.0e-6f * std::max (std::abs (expected), 1.0e-12f),
                "Mic Distance left a ringing continuum band at its old perspective");
    }
}

// A contact owns the continuum level calculated when it was triggered, but the
// microphone may move before that contact reaches the shared head. Its pending
// injection must arrive at the physical bank's live perspective, not the stale
// one stored by the strike.
void testContinuumDistanceMovesAPendingContact()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;
    parameters.tensionModulation = 0.0f;
    parameters.micDistance = 0.0f;

    taikor::TaikoEngine automated;
    automated.setParameters (parameters);
    automated.prepare (48000.0, defaultBlockSize);
    automated.trigger (taikor::Articulation::Don, 0, 0.92f);

    parameters.micDistance = 1.0f;
    automated.setParameters (parameters);
    taikor::TaikoEngineTestAccess::refreshPhysicalDrums (automated);
    render (automated, 480);
    const auto moved =
        taikor::TaikoEngineTestAccess::physicalContinuumState (automated);

    taikor::TaikoEngine fresh;
    fresh.setParameters (parameters);
    fresh.prepare (48000.0, defaultBlockSize);
    fresh.trigger (taikor::Articulation::Don, 0, 0.92f);
    render (fresh, 480);
    const auto expected =
        taikor::TaikoEngineTestAccess::physicalContinuumState (fresh);

    expect (moved.size() == 5 && expected.size() == moved.size(),
            "the pending-contact distance probe lost a continuum band");
    for (std::size_t band = 0; band < moved.size() && band < expected.size(); ++band)
        expect (std::abs (moved[band].envelope - expected[band].envelope)
                    <= 2.0e-6f * std::max (std::abs (expected[band].envelope),
                                           1.0e-12f),
                "a pending continuum injection kept its trigger-time distance gain");
}

// Structural automation changes the eigenvectors of each cavity-coupled
// two-head pair. Stable branch IDs are not stable physical coordinates: the
// state has to pass through batter/rear displacement and velocity, and a stick
// still in contact has to receive projections in the rebuilt basis.
void testStructuralAutomationPreservesTwoHeadState()
{
    const auto probe =
        taikor::TaikoEngineTestAccess::axisymmetricRebuildProbe();
    expect (probe.pairs == 8 && probe.contactModes == 16,
            "the structural rebuild probe did not cover every two-head pair");
    expect (probe.displacementError < 2.0e-5,
            "structural automation stepped physical head displacement: "
                + std::to_string (probe.displacementError));
    expect (probe.velocityError < 2.0e-5,
            "structural automation stepped physical head velocity: "
                + std::to_string (probe.velocityError));
    expect (probe.pendingInputError < 2.0e-5,
            "structural automation left a pending force in the old eigenbasis: "
                + std::to_string (probe.pendingInputError));
    expect (probe.muteRateError < 2.0e-5,
            "structural automation reassigned a live Tsu palm by branch ID: "
                + std::to_string (probe.muteRateError));
    expect (probe.contactProjectionError < 2.0e-6,
            "a contact straddling automation sensed and drove different bases: "
                + std::to_string (probe.contactProjectionError));
    expect (probe.modeProjectionError < 2.0e-6,
            "a contact straddling automation retained its old force projection: "
                + std::to_string (probe.modeProjectionError));

    const auto hostile =
        taikor::TaikoEngineTestAccess::hostileAxisymmetricMuteProbe();
    expect (hostile.modesBefore == 1 && hostile.modesAfter == 2
                && hostile.muteTicks > 0
                && hostile.maximumOldBatterFraction < 1.0e-6f,
            "the hostile rebuild probe did not cross rear-only to a complete pair");
    expect (hostile.baseRate > 0.0f && hostile.rebuiltBatterRate > 0.0f
                && hostile.rateError < 2.0e-5,
            "a rear-only Nyquist bank forgot its live Tsu rate when the batter "
            "branch returned: " + std::to_string (hostile.rateError));
}

// Filter isolation and physical excitation are separate questions. Compare
// stationary, equal-total-power bands through the actual audio cascade: the
// previous full-hit comparison also measured contact spectrum, displacement
// response and decay, so a physically quieter high band could fail a filter
// threshold even when its filter was unchanged. Hertz and modal-impulse tests
// independently guard those frequency-dependent physical weights.
void testContinuumBandsOwnTheirOctaves()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;
    parameters.headDamping = 0.0f;
    parameters.tensionModulation = 0.0f;
    taikor::TaikoEngine probe;
    probe.setParameters (parameters);
    probe.prepare (48000.0, defaultBlockSize);
    probe.trigger (taikor::Articulation::Don, 0, 1.0f);
    const auto bands = taikor::TaikoEngineTestAccess::continuumBands (probe);
    expect (bands.size() == 5,
            "the reference drum must carry all five continuum octaves");
    if (bands.size() != 5)
        return;

    std::vector<std::vector<double>> levels (
        bands.size(), std::vector<double> (bands.size(), -300.0));
    constexpr int ensembleSize = 4;
    for (std::size_t source = 0; source < bands.size(); ++source)
    {
        std::vector<double> power (bands.size(), 0.0);
        for (int take = 0; take < ensembleSize; ++take)
        {
            const auto mono = taikor::TaikoEngineTestAccess::stationaryContinuumBand (
                bands[source], 0x243f6a89u
                    + static_cast<std::uint32_t> (take) * 0x9e3779b9u, 4096);

            for (std::size_t target = 0; target < bands.size(); ++target)
                power[target] += std::pow (
                    10.0,
                    bandLevelHannDb (mono, 48000.0, bands[target].centre / 1.35,
                                     bands[target].centre * 1.35)
                        / 10.0);
        }

        for (std::size_t target = 0; target < bands.size(); ++target)
            levels[source][target] = 10.0 * std::log10 (
                std::max (power[target] / ensembleSize, 1.0e-30));
    }

    // By the second octave the first band must be well out of the way. The
    // former broad difference-of-low-passes fails this rejection threshold.
    expect (levels[0][2] < levels[0][0] - 24.0,
            "the crossover band's upper skirt still masks the statistical tail");

    for (std::size_t target = 1; target < bands.size(); ++target)
    {
        double loudestLower = -300.0;
        for (std::size_t source = 0; source < target; ++source)
            loudestLower = std::max (loudestLower, levels[source][target]);

        expect (levels[target][target] > loudestLower + 0.5,
                "continuum octave " + std::to_string (target)
                    + " is still hidden by a lower band: own "
                    + std::to_string (levels[target][target]) + " dB, leak "
                    + std::to_string (loudestLower) + " dB");
    }
}

// The attack glide raises the head's tension while the head is ringing, and
// `applyTensionShift` rewrites every membrane resonator's coefficients under a
// running state to do it. That really does step the next output by
// da1*y[n-1] + da2*y[n-2], and the second pass read that step as the largest
// source of brightness the instrument has above 1 kHz thirty milliseconds after
// a hard stroke - about six decibels of it at the factory Tension Mod, flat
// from 1.2 to 20 kHz.
//
// It is not. The reading was the estimator. `bandLevelDb` takes a rectangular
// window, a rectangular window's sidelobes fall as one over the frequency
// offset, and the ratio of two renders of the same leakage is flat across every
// band by construction - which is exactly the signature the pass took for
// proof that the mechanism was not physical. The first block below pins that
// down on a signal whose spectrum is known, so nobody derives it from the
// instrument again; the rest measures the region the honest way and records
// what is really there.
//
// This test is a guard. It passes on the shipping engine and it passed on the
// engine before the glide was ever looked at. It is here so that the next pass
// to work on this region - the one waiting to let the head's own stretching
// pump the continuum - starts from a number rather than from an artefact.
void testTheGlideDoesNotBrightenTheTopOfTheSpectrum()
{
    constexpr double sampleRate = 48000.0;

    // One sinusoid, nothing above it, read through the suite's own band level.
    {
        std::vector<float> sine (2400);
        for (std::size_t index = 0; index < sine.size(); ++index)
            sine[index] = 0.3f
                        * static_cast<float> (std::sin (2.0 * analysisPi * 1000.3
                                                        * static_cast<double> (index)
                                                        / sampleRate));

        const double leaked = bandLevelDb (sine, sampleRate, 4000.0, 10000.0);
        const double real = 20.0 * std::log10 (highPassedRms (sine, sampleRate, 4000.0,
                                                              1200, sine.size())
                                               + 1.0e-300);

        expect (leaked > -80.0,
                "a rectangular window stopped leaking, so the caution below is "
                "no longer needed and this test can be simplified");
        expect (leaked - real > 60.0,
                "the gap between what a rectangular window reports above a lone "
                "1 kHz partial and what is there stopped being enormous");
    }

    // The glide's own contribution, with the head's continuum silenced so that
    // the continuum legitimately retuning with the head is not counted as the
    // resonator rewrite. Thirty to eighty milliseconds after the strike, past
    // the contact, where a Don at the reference octave has no modelled content
    // above about 330 Hz at all.
    const auto glideResidue = [] (float tensionModulation, bool silenced,
                                  double cornerHz, double& broadbandDb)
    {
        auto parameters = defaultParameters();
        parameters.humanise = 0.0f;
        parameters.tensionModulation = tensionModulation;

        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (sampleRate, defaultBlockSize);
        engine.reset();
        engine.trigger (taikor::Articulation::Don, 0, 1.0f);
        if (silenced)
            taikor::TaikoEngineTestAccess::silenceContinuum (engine);

        const auto rendered = render (engine, static_cast<int> (sampleRate * 0.085));
        const auto mono = rendered.mono();
        const auto first = static_cast<std::size_t> (sampleRate * 0.030);
        const auto last = static_cast<std::size_t> (sampleRate * 0.080);

        broadbandDb = 20.0 * std::log10 (windowedRms (mono, first, last) + 1.0e-300);
        return 20.0 * std::log10 (
            highPassedRms (mono, sampleRate, cornerHz, first, last) + 1.0e-300);
    };

    {
        double quietBroadband = 0.0;
        double loudBroadband = 0.0;
        const double quiet = glideResidue (0.0f, true, 1200.0, quietBroadband);
        const double loud = glideResidue (1.0f, true, 1200.0, loudBroadband);

        // Measured -119.9 dB against a stroke at -18.3 dB, and Tension Mod
        // 0 to 1 moves it by -0.00 dB. The same two renders read through
        // bandLevelDb over 1.2-2.4 kHz give -48.9 and -41.3 dB, a rise of
        // 7.66 dB, and all of it is the leakage of the drum's own bottom two
        // octaves through the window's sidelobes.
        expect (quiet < quietBroadband - 80.0 && loud < loudBroadband - 80.0,
                "the coefficient rewrite in applyTensionShift became audible "
                "above 1.2 kHz on a drum that has no content there");
        expect (std::abs (loud - quiet) < 2.0,
                "the attack glide started spraying the top of the spectrum");
    }

    // And on the instrument as it ships, with the continuum in. Here Tension
    // Mod does buy about a decibel above 1.2 kHz, which is the continuum's own
    // bands moving up with the head rather than an artefact - the mechanism the
    // engine means to have. What it does not buy is six.
    for (const double corner : { 1200.0, 4000.0 })
    {
        double ignored = 0.0;
        const double quiet = glideResidue (0.0f, false, corner, ignored);
        const double loud = glideResidue (1.0f, false, corner, ignored);

        expect (loud - quiet < 3.0,
                "Tension Mod moved the top of the spectrum by more than the "
                "continuum retuning with the head accounts for");
    }
}

// Velocity must change the timbre, not only the level, and it must do so
// through the contact time rather than through a separate brightness control.
void testVelocitySensitivity()
{
    const auto parameters = defaultParameters();
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);

    float previousContact = 0.0f;
    for (const float velocity : { 0.1f, 0.3f, 0.5f, 0.7f, 0.9f, 1.0f })
    {
        const auto contact =
            engine.measureContactSeconds (taikor::Articulation::Don, 0, velocity);
        expect (contact > 0.0f && contact < 0.05f,
                "contact time left a physically sensible range");
        if (previousContact > 0.0f)
            expect (contact < previousContact,
                    "a harder stroke must shorten the stick's contact with the head");
        previousContact = contact;
    }

    double previousPeak = 0.0;
    double previousBrightness = 0.0;
    double softestBrightness = 0.0;
    double hardestBrightness = 0.0;

    for (const float velocity : { 0.15f, 0.45f, 0.75f, 1.0f })
    {
        const auto rendered = strike (parameters, taikor::Articulation::Don, 0, velocity,
                                      48000.0, 24000);
        const auto mono = rendered.mono();

        // Ratio of high-band to low-band energy over the attack. The low pair
        // is the drum the engine reports rather than two frequencies written
        // for one particular default, so retuning the instrument cannot move
        // the measurement off the partials it is watching.
        const auto reported = taikor::TaikoEngine::measure (parameters, 0);
        const double low =
            binMagnitude (mono, reported.loadedFundamentalHz, 48000.0, 0, 4800)
            + binMagnitude (mono, reported.breathingModeHz, 48000.0, 0, 4800);
        // Integrated across the band rather than sampled at a few frequencies.
        // Most of the drum's high-frequency energy is the head's modal
        // continuum, which is stochastic by construction - it stands for
        // hundreds of modes nobody can resolve - so any single bin of it is
        // noisy enough to reverse a real trend. The band as a whole is not.
        double high = 0.0;
        for (double frequency = 700.0; frequency < 6000.0; frequency *= 1.06)
            high += binMagnitude (mono, frequency, 48000.0, 0, 4800);
        const double brightness = high / std::max (low, 1.0e-9);

        expect (rendered.peak > previousPeak,
                "a harder stroke must be louder");
        // A large reversal would mean the contact law had stopped working; a
        // small one is the continuum being what it is. The overall span is
        // checked after the loop, which is the claim that actually matters.
        expect (brightness > previousBrightness * 0.75,
                "a harder stroke must not lose high partial content");
        if (velocity <= 0.15f)
            softestBrightness = brightness;
        hardestBrightness = brightness;
        previousPeak = rendered.peak;
        previousBrightness = brightness;
    }

    expect (previousBrightness > 0.0, "brightness measurement failed to run");
    // The claim the contact law actually makes: a full-arm stroke is markedly
    // brighter than a ghost note. Stated across the whole range rather than
    // step by step, because most of the drum's high-frequency energy is the
    // head's modal continuum, which is stochastic and will not march in a
    // straight line from one velocity to the next.
    expect (hardestBrightness > softestBrightness * 1.40,
            "a full-velocity stroke must be clearly brighter than a ghost note");

    // A softer bachi must be darker than a hard one at the same velocity.
    auto soft = parameters;
    soft.bachiHardness = 0.05f;
    auto hard = parameters;
    hard.bachiHardness = 1.0f;

    expect (engine.measureContact (soft, taikor::Articulation::Don, 0, 0.8f)
                > engine.measureContact (hard, taikor::Articulation::Don, 0, 0.8f),
            "a soft beater must stay on the head longer than a hard bachi");
}

void testVelocityCurveShapesImpactSpeed()
{
    using Access = taikor::TaikoEngineTestAccess;

    // The full bipolar surface must stay finite, bounded and monotone. This is
    // deliberately denser than MIDI's 128 input values so no fractional host
    // automation value can hide a fold between notes.
    constexpr int curveSteps = 64;
    constexpr int velocitySteps = 512;
    for (int curveIndex = 0; curveIndex <= curveSteps; ++curveIndex)
    {
        const float curve = -1.0f
                          + 2.0f * static_cast<float> (curveIndex)
                                   / static_cast<float> (curveSteps);
        float previous = -1.0f;
        for (int velocityIndex = 0; velocityIndex <= velocitySteps;
             ++velocityIndex)
        {
            const float velocity = static_cast<float> (velocityIndex)
                                 / static_cast<float> (velocitySteps);
            const float shaped = Access::shapeVelocity (velocity, curve);
            expect (std::isfinite (shaped) && shaped >= 0.0f && shaped <= 1.0f,
                    "Velocity Curve left the finite unit interval");
            expect (shaped >= previous,
                    "Velocity Curve folded backwards between adjacent inputs");
            previous = shaped;
        }

        expect (Access::shapeVelocity (0.0f, curve) == 0.0f
                    && Access::shapeVelocity (1.0f, curve) == 1.0f,
                "Velocity Curve must preserve silent and full-velocity endpoints");
    }

    for (int velocityIndex = 0; velocityIndex <= velocitySteps; ++velocityIndex)
    {
        const float velocity = static_cast<float> (velocityIndex)
                             / static_cast<float> (velocitySteps);
        expect (Access::shapeVelocity (velocity, 0.0f) == velocity,
                "a zero Velocity Curve must be the exact legacy identity");

        if (velocityIndex > 0 && velocityIndex < velocitySteps)
        {
            const float soft = Access::shapeVelocity (velocity, -1.0f);
            const float hard = Access::shapeVelocity (velocity, 1.0f);
            expect (soft > velocity && velocity > hard,
                    "Soft, Linear and Hard Velocity Curves are out of order");
        }
    }

    auto linear = defaultParameters();
    linear.humanise = 0.0f;
    linear.velocityDepth = 1.0f;
    linear.velocityCurve = 0.0f;
    auto soft = linear;
    soft.velocityCurve = -1.0f;
    auto hard = linear;
    hard.velocityCurve = 1.0f;

    constexpr float subFullVelocity = 0.36f;
    const float softContact = taikor::TaikoEngine::measureContact (
        soft, taikor::Articulation::Don, 0, subFullVelocity);
    const float linearContact = taikor::TaikoEngine::measureContact (
        linear, taikor::Articulation::Don, 0, subFullVelocity);
    const float hardContact = taikor::TaikoEngine::measureContact (
        hard, taikor::Articulation::Don, 0, subFullVelocity);
    expect (softContact < linearContact && linearContact < hardContact,
            "Velocity Curve must order the measured contact time Soft, Linear, Hard");

    const float softFull = taikor::TaikoEngine::measureContact (
        soft, taikor::Articulation::Don, 0, 1.0f);
    const float linearFull = taikor::TaikoEngine::measureContact (
        linear, taikor::Articulation::Don, 0, 1.0f);
    const float hardFull = taikor::TaikoEngine::measureContact (
        hard, taikor::Articulation::Don, 0, 1.0f);
    expect (softFull == linearFull && linearFull == hardFull,
            "all Velocity Curves must meet exactly at full velocity");

    // With Humanise disabled, Hard at 0.5 reaches exactly the same impact
    // speed as the default linear response at 0.25. Identical samples prove
    // the new default takes the old arithmetic path and that the curve enters
    // the acoustic model only once, before Velocity Depth.
    const auto legacy = strike (linear, taikor::Articulation::Don, 0, 0.25f,
                                48000.0, 4096);
    const auto equivalent = strike (hard, taikor::Articulation::Don, 0, 0.5f,
                                    48000.0, 4096);
    expect (legacy.left == equivalent.left && legacy.right == equivalent.right,
            "the zero Velocity Curve changed the established render path");
}

// Each control must move the term of the model it claims to.
void testPhysicalParameterInfluence()
{
    const auto base = defaultParameters();
    taikor::TaikoEngine engine;
    engine.prepare (48000.0, defaultBlockSize);

    const auto measure = [&engine] (const taikor::EngineParameters& parameters)
    {
        engine.setParameters (parameters);
        return engine.measureDrum (0);
    };

    const auto reference = measure (base);

    // Tension raises the pitch; size lowers it.
    auto tighter = base;
    tighter.tension = 0.95f;
    expect (measure (tighter).idealFundamentalHz > reference.idealFundamentalHz * 1.2f,
            "raising the tension must raise the pitch");

    auto looser = base;
    looser.tension = 0.05f;
    expect (measure (looser).idealFundamentalHz < reference.idealFundamentalHz * 0.85f,
            "lowering the tension must lower the pitch");

    // Stated against the drum this suite actually uses rather than as an
    // absolute size, so it keeps testing the 1/a law if the default moves.
    // A fifth smaller rather than half as big again: the reference drum is a
    // five-shaku o-daiko at 1.50 m and the control's own ceiling is 1.80, so
    // there is no room above it to state this the other way round. It is the
    // same 1/a law either way.
    auto narrower = base;
    narrower.headDiameter = base.headDiameter / 1.5f;
    const auto smallerRatio =
        measure (narrower).idealFundamentalHz / reference.idealFundamentalHz;
    expect (std::abs (smallerRatio - 1.5f) < 0.02f,
            "a bigger drum must be lower in proportion to its radius");

    auto smaller = base;
    smaller.headDiameter = 0.20f;
    expect (measure (smaller).idealFundamentalHz > reference.idealFundamentalHz * 1.8f,
            "a smaller drum must be higher");

    // A heavier head is slower, so it is lower at the same tension - and it is
    // also loaded less by the air, which the model must show separately.
    auto heavy = base;
    heavy.headMaterial = 1.0f;
    auto light = base;
    light.headMaterial = 0.0f;
    const auto heavyDrum = measure (heavy);
    const auto lightDrum = measure (light);
    expect (heavyDrum.idealFundamentalHz < lightDrum.idealFundamentalHz,
            "a heavier head must sound lower at the same tension");
    expect (lightDrum.idealFundamentalHz / lightDrum.loadedFundamentalHz
                > heavyDrum.idealFundamentalHz / heavyDrum.loadedFundamentalHz,
            "a light head must be pulled down by the air more than a heavy one");

    // The cavity is what splits the two heads apart, and only the sealed body
    // can do it.
    auto sealed = base;
    sealed.cavityCoupling = 1.0f;
    auto open = base;
    open.cavityCoupling = 0.0f;
    const auto sealedDrum = measure (sealed);
    const auto openDrum = measure (open);
    expect (sealedDrum.breathingModeHz / sealedDrum.loadedFundamentalHz
                > openDrum.breathingModeHz / openDrum.loadedFundamentalHz * 1.2f,
            "sealing the body must push the breathing mode further above the other");
    expect (std::abs (openDrum.breathingModeHz - openDrum.loadedFundamentalHz)
                < openDrum.loadedFundamentalHz * 0.20f,
            "an uncoupled body must leave the two heads near their own frequencies");

    // A shallow body has a stiffer air spring than a deep one.
    auto shallow = base;
    shallow.bodyDepth = 0.0f;
    auto deep = base;
    deep.bodyDepth = 1.0f;
    expect (measure (shallow).breathingModeHz > measure (deep).breathingModeHz,
            "a shallow body must have a stiffer cavity than a deep one");

    // Pitch is a musical transposition of the whole drum.
    auto raised = base;
    raised.pitch = 12.0f;
    const auto raisedDrum = measure (raised);
    expect (std::abs (raisedDrum.idealFundamentalHz
                      / reference.idealFundamentalHz - 2.0f) < 0.02f,
            "twelve semitones of pitch must double the membrane's frequency");

    // Damping shortens the tail; it must not move the pitch.
    auto damped = base;
    damped.headDamping = 1.0f;
    const auto dampedDrum = measure (damped);
    // Half of the fundamental's loss is radiation, which the damping control
    // cannot reach, so the achievable range is bounded by physics rather than
    // by taste. It still has to be a large effect to be worth a knob.
    expect (dampedDrum.tailSeconds < reference.tailSeconds * 0.5f,
            "raising the damping must substantially shorten the tail");
    expect (dampedDrum.tailSeconds > 0.0f,
            "a fully damped head must still have a finite tail");
    expect (std::abs (dampedDrum.idealFundamentalHz - reference.idealFundamentalHz)
                < reference.idealFundamentalHz * 0.001f,
            "damping must not retune the drum");

    // And the rendered audio must agree that damping shortens the note.
    const auto openTail = strike (base, taikor::Articulation::Don, 0, 0.9f, 48000.0,
                                  48000 * 4);
    const auto dampedTail = strike (damped, taikor::Articulation::Don, 0, 0.9f, 48000.0,
                                    48000 * 4);
    expect (decayTime (dampedTail.mono(), 48000.0, -40.0)
                < decayTime (openTail.mono(), 48000.0, -40.0) * 0.75,
            "the rendered tail must shorten when the head is damped");
}

// Strike position is the whole articulation vocabulary, so it has to be real:
// a centre stroke drives the axisymmetric modes and an edge stroke does not.
void testStrikePositionShapesTheSpectrum()
{
    const auto parameters = defaultParameters();

    const auto centre = strike (parameters, taikor::Articulation::Don, 0, 0.9f,
                                48000.0, 36000);
    const auto edge = strike (parameters, taikor::Articulation::Ka, 0, 0.9f,
                              48000.0, 36000);

    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);
    const auto measurements = engine.measureDrum (0);

    const auto centreMono = centre.mono();
    const auto edgeMono = edge.mono();

    const auto fundamentalIn = [&] (const std::vector<float>& samples)
    {
        return binMagnitude (samples, measurements.loadedFundamentalHz, 48000.0, 2400)
             + binMagnitude (samples, measurements.breathingModeHz, 48000.0, 2400);
    };
    const auto upperIn = [&] (const std::vector<float>& samples)
    {
        double total = 0.0;
        for (const double frequency : { 420.0, 560.0, 720.0, 900.0 })
            total += binMagnitude (samples, frequency, 48000.0, 2400);
        return total;
    };

    const auto centreRatio = upperIn (centreMono) / std::max (fundamentalIn (centreMono), 1.0e-9);
    const auto edgeRatio = upperIn (edgeMono) / std::max (fundamentalIn (edgeMono), 1.0e-9);

    expect (edgeRatio > centreRatio * 2.0,
            "an edge stroke must carry far more upper partial energy than a centre one: "
                + std::to_string (edgeRatio) + " versus "
                + std::to_string (centreRatio));
    // The global strike-position control must move the vocabulary too.
    auto towardsRim = parameters;
    towardsRim.strikePosition = 1.0f;
    towardsRim.humanise = 0.0f;
    auto towardsCentre = parameters;
    towardsCentre.strikePosition = -1.0f;
    towardsCentre.humanise = 0.0f;

    const auto rimward = strike (towardsRim, taikor::Articulation::Don, 0, 0.9f,
                                 48000.0, 24000);
    const auto centreward = strike (towardsCentre, taikor::Articulation::Don, 0, 0.9f,
                                    48000.0, 24000);
    const auto rimwardRatio = upperIn (rimward.mono())
                            / std::max (fundamentalIn (rimward.mono()), 1.0e-9);
    const auto centrewardRatio = upperIn (centreward.mono())
                               / std::max (fundamentalIn (centreward.mono()), 1.0e-9);
    expect (rimwardRatio > centrewardRatio,
            "moving the strike towards the rim must brighten it: "
                + std::to_string (rimwardRatio) + " versus "
                + std::to_string (centrewardRatio));
}

void testStrikePositionHasNoDeadTravel()
{
    using taikor::Articulation;
    using Access = taikor::TaikoEngineTestAccess;

    struct ExpectedRange
    {
        Articulation articulation;
        float centre;
        float minimum;
        float maximum;
    };

    constexpr std::array<ExpectedRange, 4> ranges {{
        { Articulation::Don,    0.15f, 0.00f, 0.47f  },
        { Articulation::Ka,     0.91f, 0.59f, 0.985f },
        { Articulation::Tsu,    0.20f, 0.00f, 0.52f  },
        { Articulation::DonRim, 0.97f, 0.65f, 0.985f },
    }};

    for (const auto& range : ranges)
    {
        expect (std::abs (Access::strikeRadius (range.articulation, -1.0f)
                          - range.minimum) < 2.0e-6f,
                "Strike Position changed an articulation's centre endpoint");
        expect (std::abs (Access::strikeRadius (range.articulation, 0.0f)
                          - range.centre) < 2.0e-6f,
                "Strike Position changed an articulation's written position");
        expect (std::abs (Access::strikeRadius (range.articulation, 1.0f)
                          - range.maximum) < 2.0e-6f,
                "Strike Position changed an articulation's rim endpoint");
        expect (Access::strikeRadius (range.articulation, -1.0f)
                    == std::clamp (range.centre - 0.32f, 0.0f, 0.985f)
                && Access::strikeRadius (range.articulation, 0.0f) == range.centre
                && Access::strikeRadius (range.articulation, 1.0f)
                    == std::clamp (range.centre + 0.32f, 0.0f, 0.985f),
                "Strike Position endpoints/default are not bit-compatible");

        float previous = Access::strikeRadius (range.articulation, -1.0f);
        for (int step = 1; step <= 200; ++step)
        {
            const float position = -1.0f + 0.01f * static_cast<float> (step);
            const float radius = Access::strikeRadius (range.articulation, position);
            expect (radius > previous + 1.0e-5f,
                    "Strike Position contains dead travel for articulation "
                        + std::to_string (static_cast<int> (range.articulation))
                        + " at step " + std::to_string (step));
            previous = radius;
        }

        // The side which already had 0.32 of physical room keeps the exact
        // released multiplication/addition sequence. Only the formerly
        // clamped side is remapped.
        const bool negativeSideWasFree = range.centre >= 0.32f;
        for (int step = 0; step <= 100; ++step)
        {
            const float magnitude = 0.01f * static_cast<float> (step);
            const float position = negativeSideWasFree ? -magnitude : magnitude;
            const float releasedOffset = position * 0.32f;
            const float released = std::clamp (
                range.centre + releasedOffset, 0.0f, 0.985f);
            expect (Access::strikeRadius (range.articulation, position) == released,
                    "Strike Position changed an already-active half-range");
        }
    }

    // These pairs used to land on exactly the same clamped point. They must
    // now change the collision and modal projection enough to change rendered
    // audio, not merely report a different UI coordinate.
    struct FormerlyFlatPair
    {
        Articulation articulation;
        float first;
        float second;
    };
    constexpr std::array<FormerlyFlatPair, 4> formerlyFlat {{
        { Articulation::Don,    -1.00f, -0.75f },
        { Articulation::Ka,      0.25f,  0.50f },
        { Articulation::Tsu,    -1.00f, -0.75f },
        { Articulation::DonRim,  0.25f,  0.50f },
    }};

    for (const auto& pair : formerlyFlat)
    {
        auto firstParameters = defaultParameters();
        firstParameters.humanise = 0.0f;
        firstParameters.strikePosition = pair.first;
        auto secondParameters = firstParameters;
        secondParameters.strikePosition = pair.second;

        const auto first = strike (firstParameters, pair.articulation, 0, 0.9f,
                                   48000.0, 8192);
        const auto second = strike (secondParameters, pair.articulation, 0, 0.9f,
                                    48000.0, 8192);
        const double difference = std::max (
            maximumAbsoluteDifference (first.left, second.left),
            maximumAbsoluteDifference (first.right, second.right));
        expect (difference > 1.0e-4,
                "Strike Position still renders a flat region for articulation "
                    + std::to_string (static_cast<int> (pair.articulation))
                    + ": " + std::to_string (difference));
    }
}

void testExplicitPolarStrikeControl()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;
    parameters.tensionModulation = 0.0f;
    parameters.strikeNoise = 0.0f;
    parameters.drive = 0.0f;
    parameters.outputGain = 0.02f;

    // The new host coordinate must reach the real modal/contact/microphone
    // geometry, not only the head display. Repeating one coordinate remains
    // deterministic, while rotating it produces a different stereo pressure
    // history at otherwise identical settings.
    const auto onAxis = strike (parameters, taikor::Articulation::Ka, 0, 0.9f,
                                48000.0, 12000);
    const auto onAxisAgain = strike (parameters, taikor::Articulation::Ka, 0, 0.9f,
                                     48000.0, 12000);
    auto rotatedParameters = parameters;
    rotatedParameters.strikeAzimuth = 1.1f;
    const auto rotated = strike (rotatedParameters, taikor::Articulation::Ka, 0,
                                 0.9f, 48000.0, 12000);
    expect (maximumAbsoluteDifference (onAxis.left, onAxisAgain.left) == 0.0
                && maximumAbsoluteDifference (onAxis.right, onAxisAgain.right) == 0.0,
            "a fixed authored strike coordinate was not deterministic");
    expect (std::max (maximumAbsoluteDifference (onAxis.left, rotated.left),
                      maximumAbsoluteDifference (onAxis.right, rotated.right))
                > 1.0e-4,
            "Strike Azimuth changed the display but not the rendered drum");

    // The lightweight audio-thread estimate must rank the same deliberately
    // split cosine/sine resonators as the exact dry-contact readout. Treating
    // them as one degenerate addition-identity mode used to choose an 85.6 Hz
    // partial here while the rendered stroke is heard at 59.6 Hz.
    auto readoutParameters = parameters;
    readoutParameters.strikeAzimuth = -1.134f;
    taikor::TaikoEngine readoutEngine;
    readoutEngine.setParameters (readoutParameters);
    readoutEngine.prepare (48000.0, defaultBlockSize);
    readoutEngine.trigger (taikor::Articulation::Don, 0, 0.72f);
    taikor::DrumVisualState estimated;
    readoutEngine.getVisualState (estimated);
    const auto exact = taikor::TaikoEngine::measure (
        readoutParameters, 0, 0.0f, 48000.0).soundingHz;
    const auto readoutErrorCents = 1200.0f * std::log2 (
        estimated.fundamentalHz / exact);
    expect (std::abs (readoutErrorCents) < 10.0f,
            "the angle-aware trigger readout collapsed the split modal pair: "
                + std::to_string (readoutErrorCents) + " cents");

    // Zero azimuth drives only the cosine member, whose readout must retain the
    // exact frequency of the split pole the renderer builds.
    taikor::TaikoEngine zeroAngleEngine;
    zeroAngleEngine.setParameters (parameters);
    zeroAngleEngine.prepare (48000.0, defaultBlockSize);
    zeroAngleEngine.trigger (taikor::Articulation::Don, 0, 0.72f);
    taikor::DrumVisualState zeroAngle;
    zeroAngleEngine.getVisualState (zeroAngle);
    expect (std::abs (zeroAngle.fundamentalHz - 59.7474365234375f) < 1.0e-4f,
            "the zero-azimuth analytic pitch anchor moved");

    // CC coordinates are live overrides, not hidden edits to the host knobs.
    // Clearing them must reveal the host-authored point again, and reset must
    // not carry a gesture into the next performance.
    parameters.strikePosition = 0.25f;
    parameters.strikeAzimuth = 0.75f;
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);
    engine.setStrikePositionOverride (-1.0f);
    engine.setStrikeAzimuthOverride (-1.2f);
    engine.trigger (taikor::Articulation::Don, 0, 0.9f);

    taikor::DrumVisualState overridden;
    engine.getVisualState (overridden);
    expect (overridden.strikeRadius == 0.0f
                && std::abs (overridden.strikeAngle + 1.2f) < 1.0e-7f,
            "live strike overrides did not take precedence over the host controls");

    engine.clearStrikeOverrides();
    engine.allSoundsOff();
    engine.trigger (taikor::Articulation::Don, 0, 0.9f);
    taikor::DrumVisualState restored;
    engine.getVisualState (restored);
    expect (restored.strikeRadius
                == taikor::TaikoEngineTestAccess::strikeRadius (
                       taikor::Articulation::Don, parameters.strikePosition)
                && std::abs (restored.strikeAngle - parameters.strikeAzimuth) < 1.0e-7f,
            "clearing strike overrides did not return to the host coordinates");

    engine.setStrikePositionOverride (1.0f);
    engine.setStrikeAzimuthOverride (2.0f);
    engine.reset();
    engine.trigger (taikor::Articulation::Don, 0, 0.9f);
    taikor::DrumVisualState afterReset;
    engine.getVisualState (afterReset);
    expect (afterReset.strikeRadius == restored.strikeRadius
                && afterReset.strikeAngle == restored.strikeAngle,
            "reset carried live strike overrides into the next performance");
}

void testZeroAzimuthObservesTheRenderedSplitBranch()
{
    const auto probe = taikor::TaikoEngineTestAccess::zeroAzimuthSplitProbe();
    expect (probe[1] > 0.0f
                && std::abs (probe[0] - probe[1]) < probe[1] * 1.0e-6f,
            "the zero-azimuth observer did not use the rendered cosine split pole");
    expect (probe[2] == 0.0f,
            "the zero-azimuth observer stopped rejecting the silent sine branch");
}

// Humanise is small uncertainty in where a hand lands, not a random player
// teleporting around the circumference. Its radial and tangential travel use
// one head-space distance so a rim hit cannot wander farther merely because it
// has a longer angular lever arm.
void testHumaniseScattersInHeadCoordinates()
{
    constexpr float humanise = 0.4f;
    constexpr float maximumScatter = 0.055f * humanise;

    auto parameters = defaultParameters();
    parameters.humanise = humanise;
    parameters.strikeAzimuth = 0.0f;

    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);

    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
    {
        const auto articulation = static_cast<taikor::Articulation> (index);
        const float writtenRadius = taikor::TaikoEngineTestAccess::strikeRadius (
            articulation, parameters.strikePosition);
        float largestTangential = 0.0f;

        for (int hit = 0; hit < 64; ++hit)
        {
            engine.trigger (articulation, 0, 0.8f);
            taikor::DrumVisualState state;
            engine.getVisualState (state);
            const float angle = static_cast<float> (std::remainder (
                static_cast<double> (state.strikeAngle), 2.0 * analysisPi));
            const float tangential = std::abs (state.strikeRadius * angle);
            largestTangential = std::max (largestTangential, tangential);

            expect (std::abs (state.strikeRadius - writtenRadius)
                        <= maximumScatter + 1.0e-6f,
                    "Humanise exceeded its radial head-space scatter");
            expect (tangential <= maximumScatter + 1.0e-6f,
                    "Humanise moved a strike too far round the circumference");
            engine.allSoundsOff();
        }

        expect (largestTangential > 0.5f * maximumScatter,
                "the bounded tangential Humanise scatter became inert");
    }

    // Exercise the centre guard explicitly. At Centre 100 the written Don and
    // Tsu radii are zero, so dividing the tangential distance by radius would
    // produce an unbounded angle even though azimuth has no meaning there.
    auto centred = defaultParameters();
    centred.humanise = 1.0f;
    centred.strikePosition = -1.0f;
    centred.strikeAzimuth = 0.0f;

    taikor::TaikoEngine centreEngine;
    centreEngine.setParameters (centred);
    centreEngine.prepare (48000.0, defaultBlockSize);
    for (const auto articulation : { taikor::Articulation::Don,
                                     taikor::Articulation::Tsu })
        for (int hit = 0; hit < 16; ++hit)
        {
            centreEngine.trigger (articulation, 0, 0.8f);
            taikor::DrumVisualState state;
            centreEngine.getVisualState (state);
            const float angle = static_cast<float> (std::remainder (
                static_cast<double> (state.strikeAngle), 2.0 * analysisPi));
            expect (std::isfinite (state.strikeRadius) && std::isfinite (angle),
                    "centred Humanise produced a non-finite strike point");
            expect (std::abs (state.strikeRadius * angle) <= 0.055f + 1.0e-6f,
                    "centred Humanise escaped its head-space radius");

            const auto sounded = render (centreEngine, 64, 64);
            expect (sounded.finite,
                    "centred Humanise produced non-finite audio");
            centreEngine.allSoundsOff();
        }

    // Non-zero authored azimuths take the wrapped branch. Pin both addition
    // and the +/-pi boundary so a future simplification cannot make a CC16
    // performance jump outside the published polar range.
    auto wrapped = defaultParameters();
    wrapped.humanise = 1.0f;
    wrapped.strikeAzimuth = static_cast<float> (analysisPi - 0.01);
    taikor::TaikoEngine wrapEngine;
    wrapEngine.setParameters (wrapped);
    wrapEngine.prepare (48000.0, defaultBlockSize);
    for (int hit = 0; hit < 16; ++hit)
    {
        wrapEngine.trigger (taikor::Articulation::Ka, 0, 0.8f);
        taikor::DrumVisualState state;
        wrapEngine.getVisualState (state);
        const double delta = std::remainder (
            static_cast<double> (state.strikeAngle - wrapped.strikeAzimuth),
            2.0 * analysisPi);
        expect (state.strikeAngle >= -analysisPi
                    && state.strikeAngle <= analysisPi,
                "Humanise left the wrapped azimuth range");
        expect (std::abs (static_cast<double> (state.strikeRadius) * delta)
                    <= 0.055 + 1.0e-6,
                "Humanise exceeded its tangential component after wrapping");
        wrapEngine.allSoundsOff();
    }
}

// Four layered instances should sound like four people playing one authored
// part, not four phase-locked copies of the same performance. Performer is
// an identity salt for the performed contact, including the subtle natural
// differences left when Humanise is zero. Replaying each performer still has
// to reproduce the same take.
void testPerformerIdentityMakesRealEnsembles()
{
    constexpr int samples = 16000;
    auto expressive = defaultParameters();
    expressive.humanise = 0.7f;

    std::array<Rendered, 4> performers;
    for (int performer = 0; performer < 4; ++performer)
    {
        auto parameters = expressive;
        parameters.performer = performer;
        performers[static_cast<std::size_t> (performer)] = strike (
            parameters, taikor::Articulation::Ka, 0, 0.86f, 48000.0, samples);
        const auto repeated = strike (
            parameters, taikor::Articulation::Ka, 0, 0.86f, 48000.0, samples);
        expect (maximumAbsoluteDifference (
                    performers[static_cast<std::size_t> (performer)].left,
                    repeated.left) == 0.0
                    && maximumAbsoluteDifference (
                           performers[static_cast<std::size_t> (performer)].right,
                           repeated.right) == 0.0,
                "a Performer identity did not repeat deterministically");
    }

    for (int first = 0; first < 4; ++first)
        for (int second = first + 1; second < 4; ++second)
    {
        const auto& a = performers[static_cast<std::size_t> (first)];
        const auto& b = performers[static_cast<std::size_t> (second)];
        const double difference = std::max (
            maximumAbsoluteDifference (a.left, b.left),
            maximumAbsoluteDifference (a.right, b.right));
        expect (difference > 1.0e-5,
                "two Performer identities remained phase-locked copies");
    }

    // The identity has to change the performed resonant hit, not merely choose
    // a different broadband texture. Remove continuum, contact noise, tack and
    // direct pressure while retaining the salted contact point/speed/time.
    std::array<Rendered, 4> resolved;
    for (int performer = 0; performer < 4; ++performer)
    {
        auto parameters = expressive;
        parameters.performer = performer;
        resolved[static_cast<std::size_t> (performer)] = resolvedStrike (
            parameters, taikor::Articulation::Ka, 0, 0.86f, 48000.0, samples);
    }
    for (int first = 0; first < 4; ++first)
        for (int second = first + 1; second < 4; ++second)
            expect (std::max (
                    maximumAbsoluteDifference (
                        resolved[static_cast<std::size_t> (first)].left,
                        resolved[static_cast<std::size_t> (second)].left),
                    maximumAbsoluteDifference (
                        resolved[static_cast<std::size_t> (first)].right,
                        resolved[static_cast<std::size_t> (second)].right))
                    > 1.0e-5,
                "Performer changed only cheap stochastic texture, not the resonant hit");

    // Average the four takes at unity total gain, as a user layering four
    // instances would. Correlation is scale-invariant, so this specifically
    // rejects a disguised gain change rather than demanding a louder result.
    std::vector<float> layerLeft (static_cast<std::size_t> (samples));
    std::vector<float> layerRight (static_cast<std::size_t> (samples));
    for (int performer = 0; performer < 4; ++performer)
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto index = static_cast<std::size_t> (sample);
            layerLeft[index] += 0.25f
                * performers[static_cast<std::size_t> (performer)].left[index];
            layerRight[index] += 0.25f
                * performers[static_cast<std::size_t> (performer)].right[index];
        }
    expect (std::abs (correlation (performers[0].left, layerLeft)) < 0.9999
                || std::abs (correlation (performers[0].right, layerRight)) < 0.9999,
            "P1-P4 layering changed only gain, not the performed waveform");

    auto precise = defaultParameters();
    precise.humanise = 0.0f;
    precise.strikeNoise = 0.0f;
    const auto preciseP1 = resolvedStrike (
        precise, taikor::Articulation::Ka, 0, 0.86f, 48000.0, samples);
    for (int performer = 1; performer < 4; ++performer)
    {
        precise.performer = performer;
        const auto rendered = resolvedStrike (
            precise, taikor::Articulation::Ka, 0, 0.86f, 48000.0, samples);
        expect (std::max (
                    maximumAbsoluteDifference (preciseP1.left, rendered.left),
                    maximumAbsoluteDifference (preciseP1.right, rendered.right))
                    > 1.0e-6,
                "Humanise zero phase-locked two Performers' physical contacts");
    }
}

// The instrument is a stereo close pair, and the image has to come from the
// model rather than from a widener.
void testCloseMicrophonePair()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;

    // With the pair coincident over the centre of the head, the two
    // microphones are the same microphone and the output is exactly mono.
    auto coincident = parameters;
    coincident.micSpread = 0.0f;
    coincident.stereoWidth = 1.0f;
    const auto mono = strike (coincident, taikor::Articulation::Ka, 0, 0.9f, 48000.0,
                              24000);
    expect (maximumAbsoluteDifference (mono.left, mono.right) < 1.0e-6,
            "a coincident pair must produce identical channels");

    // Opening the pair must decorrelate it without ever putting it out of
    // phase: a drum recorded with a real close pair still sums to mono. The
    // relationship is not monotone and should not be asserted to be - each mode
    // reaches the two points with its own sign, so particular placements
    // recorrelate - but a wide pair must be clearly wider than a narrow one.
    double narrowCorrelation = 1.0;
    double widestCorrelation = 1.0;

    for (const float spread : { 0.25f, 0.55f, 0.85f, 1.0f })
    {
        auto spreadParameters = parameters;
        spreadParameters.micSpread = spread;
        const auto rendered = strike (spreadParameters, taikor::Articulation::Ka, 0,
                                      0.9f, 48000.0, 24000);
        const auto value = correlation (rendered.left, rendered.right);

        expect (value > 0.0,
                "the close pair must never go out of phase, at spread "
                    + std::to_string (spread));
        expect (value < 0.999,
                "an opened pair must not stay perfectly correlated, at spread "
                    + std::to_string (spread));

        if (spread <= 0.25f)
            narrowCorrelation = value;
        widestCorrelation = value;
    }

    expect (widestCorrelation < narrowCorrelation,
            "a fully opened pair must be wider than a nearly coincident one");

    // Backing the pair away from the head must narrow it, because the near
    // field that carries the membrane's shape decays with distance.
    auto near = parameters;
    near.micSpread = 0.8f;
    near.micDistance = 0.0f;
    auto far = parameters;
    far.micSpread = 0.8f;
    far.micDistance = 1.0f;

    const auto nearRendered = strike (near, taikor::Articulation::Ka, 0, 0.9f, 48000.0,
                                      24000);
    const auto farRendered = strike (far, taikor::Articulation::Ka, 0, 0.9f, 48000.0,
                                     24000);
    expect (correlation (farRendered.left, farRendered.right)
                > correlation (nearRendered.left, nearRendered.right),
            "backing the pair off the head must narrow the image");

    // Width at zero must fold the pair to mono without changing its sum.
    auto narrow = parameters;
    narrow.micSpread = 0.9f;
    narrow.stereoWidth = 0.0f;
    const auto folded = strike (narrow, taikor::Articulation::Ka, 0, 0.9f, 48000.0,
                                12000);
    expect (maximumAbsoluteDifference (folded.left, folded.right) < 1.0e-6,
            "zero width must produce identical channels");

    // No stroke may invert, anywhere in the microphone range, at or below the
    // width the microphones actually captured. Above that the control is a
    // deliberate exaggeration and can go out of phase like any widener, so the
    // invariant is stated - and swept - only where it is meant to hold. The
    // earlier version of this check used a single microphone distance and so
    // said nothing about the endpoints, where the pair is at its widest.
    double worstCorrelation = 1.0;
    for (const float width : { 0.0f, 0.25f, 0.5f })
    {
        for (const float distance : { 0.0f, 0.5f, 1.0f })
        {
            for (const float spread : { 0.0f, 0.5f, 1.0f })
            {
                auto swept = parameters;
                swept.stereoWidth = width;
                swept.micDistance = distance;
                swept.micSpread = spread;

                for (std::size_t index = 0; index < taikor::articulationCount; ++index)
                {
                    const auto articulation =
                        static_cast<taikor::Articulation> (index);
                    const auto rendered =
                        strike (swept, articulation, 0, 0.9f, 48000.0, 24000);
                    const auto value =
                        correlation (rendered.left, rendered.right);
                    worstCorrelation = std::min (worstCorrelation, value);

                    expect (value > -0.05,
                            std::string (taikor::getArticulationDisplayName (articulation))
                                + " inverted at width "
                                + std::to_string (width) + ", distance "
                                + std::to_string (distance) + ", spread "
                                + std::to_string (spread));
                }
            }
        }
    }

    // The sweep has to actually reach the decorrelated corner, or it is only
    // testing the easy middle of the range.
    expect (worstCorrelation < 0.5,
            "the mono-compatibility sweep never reached a widely spaced pair");
}

void testTailsTerminateAndVoicesRetire()
{
    auto parameters = defaultParameters();
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);

    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        engine.trigger (static_cast<taikor::Articulation> (index), 0, 1.0f);

    expect (engine.getActiveVoiceCount() > 0, "strokes did not allocate any voices");

    const auto rendered =
        render (engine, static_cast<int> (48000 * taikor::maximumTailSeconds + 48000));
    expect (rendered.finite, "a full tail produced non-finite audio");
    expect (engine.getActiveVoiceCount() == 0,
            "every voice must retire once its tail has run out");

    // With nothing sounding, the engine must be exactly silent rather than
    // dribbling denormals.
    const auto idle = render (engine, 24000);
    expect (idle.peak == 0.0, "an idle engine must produce exact silence");

    // Panic must stop everything immediately.
    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        engine.trigger (static_cast<taikor::Articulation> (index), 0, 1.0f);
    render (engine, 480);
    engine.allSoundsOff();
    expect (engine.getActiveVoiceCount() == 0, "panic must free every voice");
    const auto afterPanic = render (engine, 4800);
    expect (afterPanic.peak == 0.0, "panic must leave exact silence");
}

void testVoiceStealingStaysBounded()
{
    auto parameters = defaultParameters();
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);

    // Far more simultaneous strokes than the voice pool holds.
    for (int index = 0; index < 200; ++index)
        engine.trigger (static_cast<taikor::Articulation> (index % taikor::articulationCount),
                        (index % 6) - 2, 0.9f);

    const auto rendered = render (engine, 48000);
    expect (rendered.finite, "voice stealing produced non-finite audio");
    expect (rendered.peak <= 1.0001, "voice stealing exceeded full scale");
    expect (engine.getActiveVoiceCount() <= 16,
            "the engine must not exceed its declared voice pool");
}

// What the instrument is supposed to sound like, stated as measurements.
//
// The complaint that produced these numbers was that the drum was "very tonal"
// and should be "heavy, large, epic". Both halves of that are measurable, and
// both had the same root: the reference drum was small, so its fundamental sat
// near 94 Hz where the ear hears a pitch rather than a weight, and the
// frequency-proportional loss in the hide let that one mode outlive every other
// by nearly four to one. What is left after half a second of that is a sine.
void testTheDrumSoundsLikeADrumAndNotLikeATone()
{
    const auto parameters = defaultParameters();
    const auto measurements = taikor::TaikoEngine::measure (parameters, 0);

    // Large. A taiko that reads as epic has its pitch down where it is felt; a
    // 90-odd hertz drum is a floor tom. Stated in the pitch the drum is heard at
    // rather than in its loaded fundamental, because on a drum this size those
    // are not the same mode: the fundamental is at 32.7 Hz, moves no net air,
    // and is emptied by the mounting in half a second, and what a listener names
    // the drum by is the 59.7 Hz mode that outlasts it.
    expect (measurements.soundingHz > 40.0f && measurements.soundingHz < 65.0f,
            "the reference drum's pitch must sit in the register a large "
            "taiko occupies, not an octave above it");

    const auto rendered = strike (parameters, taikor::Articulation::Don, 0, 1.0f,
                                  48000.0, 144384);
    const auto mono = rendered.mono();

    // Heavy, and with a body. Both halves matter and they pull against each
    // other: a drum that is only weight is a sine, and one that is only body is
    // a snare. The proportions here are what third-octave analysis of recorded
    // taiko shows - a real one carries serious low end and is still nearly flat
    // from a couple of hundred hertz up to a kilohertz.
    double low = 0.0;
    double body = 0.0;
    double total = 0.0;
    for (double frequency = 25.0; frequency < 8000.0;
         frequency *= std::pow (2.0, 1.0 / 24.0))
    {
        const auto magnitude = binMagnitude (mono, frequency, 48000.0, 0u, 24000u);
        total += magnitude;
        if (frequency < 80.0)
            low += magnitude;
        // The region the resolved modal bank cannot reach on a large drum: its
        // highest Bessel zero lands a couple of hundred hertz up, and without
        // the head's continuum above that there is almost nothing here - four
        // per cent of the stroke, where a real one carries ten to twenty.
        if (frequency >= 250.0 && frequency < 4000.0)
            body += magnitude;
    }
    // Both floors are measured, not guessed. Run over the two reference
    // recordings this instrument was tuned against, this same sum gives a body
    // of nineteen per cent for the single ō-daiko stroke and twelve for the
    // played loop - so a floor above twenty is one no real taiko here clears,
    // and it was fitted to an earlier model whose whole upper half was a bed of
    // noise. Ten is comfortably under both and still an order of magnitude
    // above a drum with no continuum at all.
    //
    // The weight floor is a design choice rather than a measurement: the same
    // sum gives nineteen per cent for the ō-daiko and forty-four for the loop,
    // and this instrument is deliberately at the heavy end of that.
    //
    // The body floor was ten when the reference drum was a three-shaku o-daiko,
    // where this sum reads 10.9 per cent. The reference drum is now five shaku -
    // the family has to span a factor of fourteen in the fundamental to span
    // three octaves in what is heard, and no 95 cm tacked head is low enough to
    // be the bottom of that - and its whole resolved bank therefore sits most of
    // an octave lower, which moves this sum to 8.6 per cent on the identical
    // stroke. The floor is restated against the instrument the reference drum
    // now is rather than left where it was fitted for a smaller one: the two
    // readings differ by the size of the drum and not by anything that has been
    // taken out of its middle. It is deliberately a tenth of a point under what
    // the five-shaku drum measures, so that losing the head's continuum still
    // fails it by an order of magnitude, which is what this clause is for.
    expect (total > 0.0 && low > total * 0.22,
            "a centre stroke must put real weight below eighty hertz: "
                + std::to_string (low / std::max (total, 1.0e-12)));
    expect (total > 0.0 && body > total * 0.085,
            "a centre stroke must have a body between two hundred and fifty "
            "hertz and four kilohertz, not just a fundamental: "
                + std::to_string (body / std::max (total, 1.0e-12)));

    // Not tonal. Count the twenty-fourth-octave bands that are within twenty
    // decibels of the strongest one over the body of the stroke. A drum whose
    // tail has collapsed onto a single partial scores in the low teens; a dense
    // one scores twice that.
    std::vector<double> bands;
    for (double frequency = 40.0; frequency < 4000.0;
         frequency *= std::pow (2.0, 1.0 / 24.0))
        bands.push_back (binMagnitude (mono, frequency, 48000.0, 4800u, 19200u));

    const auto strongest = *std::max_element (bands.begin(), bands.end());
    int within20dB = 0;
    for (const auto magnitude : bands)
        if (magnitude > strongest * 0.1)
            ++within20dB;

    expect (within20dB >= 18,
            "the body of a stroke must be a cluster of partials rather than one "
            "surviving mode");

    // And the mechanism behind that: no membrane mode may outlive the one above
    // it by more than about three to one. This is the frequency-proportional
    // loss in the hide being held in check by the rim, and it is what stops the
    // stroke decaying into its own fundamental.
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);
    engine.trigger (taikor::Articulation::Don, 0, 1.0f);

    const auto bank = taikor::TaikoEngineTestAccess::membraneT60s (engine);
    expect (bank.size() >= 2, "a centre stroke must build more than one mode");
    if (bank.size() >= 2)
        expect (bank[0] < bank[1] * 3.0f,
                "the lowest mode must not outlive the one above it so far that "
                "the stroke ends as a sine");
}

// The complaint players make about every sampled taiko library is the same one:
// limited dynamics, and loud. A model has no reason to inherit it. The whole
// instrument used to cover about twenty-two decibels of level from the softest
// MIDI velocity to the hardest, and the bottom half of the keyboard's velocity
// range lived inside half a decibel of the floor because the map squared the
// control before a logarithmic curve, which compresses the bottom rather than
// expanding it.
void testTheDynamicRangeReachesFromAGhostStrokeToAFullBlow()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;
    parameters.velocityDepth = 1.0f;

    const auto peakAt = [&parameters] (taikor::Articulation articulation, float velocity)
    {
        return strike (parameters, articulation, 0, velocity, 48000.0, 24000).peak;
    };

    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
    {
        const auto articulation = static_cast<taikor::Articulation> (index);
        const auto name = std::string (taikor::getArticulationDisplayName (articulation));

        const auto ghost = peakAt (articulation, 0.02f);
        const auto full = peakAt (articulation, 1.0f);
        expect (ghost > 1.0e-6 && std::isfinite (ghost),
                "a ghost stroke must still sound: " + name);

        const auto span = 20.0 * std::log10 (full / std::max (ghost, 1.0e-12));
        // The selected removal of the extra click changes Don's peak span
        // 34.812→29.921 dB, while its 500 ms stereo RMS span changes only
        // 0.120 dB. Preserve that chosen output without adding a gain fit.
        const double minimumSpan = articulation == taikor::Articulation::Don ? 29.0 : 30.0;
        expect (span > minimumSpan,
                "the instrument lost its selected dynamic span from a ghost "
                "stroke to a full blow: " + name + " covers "
                    + std::to_string (span));
    }

    // And it has to be usable across that range rather than merely wide.
    // Impact speed is geometric in the control and level goes as a power of
    // impact speed, so equal steps of MIDI velocity are equal steps of decibels
    // - which is what an arm does. The squared map made the first fifth of the
    // range worth a decibel and the last fifth worth ten.
    std::vector<double> steps;
    double previous = 20.0 * std::log10 (peakAt (taikor::Articulation::Don, 0.2f));
    for (float velocity = 0.4f; velocity <= 1.001f; velocity += 0.2f)
    {
        const auto level = 20.0 * std::log10 (peakAt (taikor::Articulation::Don, velocity));
        steps.push_back (level - previous);
        previous = level;
    }

    const auto smallest = *std::min_element (steps.begin(), steps.end());
    const auto largest = *std::max_element (steps.begin(), steps.end());
    expect (smallest > 0.0, "every step up in velocity must be a step up in level");
    expect (largest < smallest * 1.8,
            "equal steps of velocity must be near-equal steps of level: the widest "
            "is " + std::to_string (largest) + " dB and the narrowest "
                + std::to_string (smallest));

    // The top of the range has not moved: the map is geometric between the same
    // two impact speeds it always was. One seed is not a headroom measurement,
    // though: spatial jitter, contact and short stochastic sources can align on
    // a later hit. Sweep the first 1,024 deterministic performances of the
    // loudest articulation and keep the factory Output clear of the limiter.
    auto loudest = defaultParameters();
    loudest.velocityDepth = 1.0f;
    loudest.humanise = 1.0f;
    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
    {
        const auto articulation = static_cast<taikor::Articulation> (index);
        const auto rendered = strike (loudest, articulation, 0, 1.0f, 48000.0, 24000);
        expect (rendered.peak < 0.95,
                "the loudest single stroke must stay clear of the safety limiter at "
                "the factory output level: "
                    + std::string (taikor::getArticulationDisplayName (articulation))
                    + " peaked at " + std::to_string (rendered.peak));
    }

    constexpr int headroomSamples = 2400;
    std::array<float, defaultBlockSize> left {};
    std::array<float, defaultBlockSize> right {};
    constexpr std::uint64_t headroomSequences = 1024;
    constexpr std::uint64_t hostileOrder = 421768;
    for (int performer = 0; performer < 4; ++performer)
    {
        loudest.performer = performer;
        taikor::TaikoEngine sequenceEngine;
        sequenceEngine.setParameters (loudest);
        sequenceEngine.prepare (48000.0, defaultBlockSize);
        const auto peakForOrder = [&] (std::uint64_t order)
        {
            sequenceEngine.reset();
            taikor::TaikoEngineTestAccess::setStrokeCount (sequenceEngine, order - 1);
            sequenceEngine.trigger (taikor::Articulation::DonRim, 0, 1.0f);

            double peak = 0.0;
            for (int offset = 0; offset < headroomSamples; offset += defaultBlockSize)
            {
                const int count = std::min (defaultBlockSize, headroomSamples - offset);
                sequenceEngine.process (left.data(), right.data(), count);
                for (int sample = 0; sample < count; ++sample)
                    peak = std::max ({
                        peak, std::abs (static_cast<double> (left[sample])),
                        std::abs (static_cast<double> (right[sample])) });
            }

            return peak;
        };

        double worstPeak = 0.0;
        std::uint64_t worstOrder = 0;
        for (std::uint64_t order = 1; order <= headroomSequences; ++order)
        {
            const double peak = peakForOrder (order);
            if (peak > worstPeak)
            {
                worstPeak = peak;
                worstOrder = order;
            }
        }

        // A million-order offline sweep found this rarer P1 alignment outside
        // the inexpensive contiguous window. Pin the same order for every
        // identity; their first 1,024 orders own their distinct worst cases.
        const double hostilePeak = peakForOrder (hostileOrder);
        if (hostilePeak > worstPeak)
        {
            worstPeak = hostilePeak;
            worstOrder = hostileOrder;
        }
        expect (worstPeak < 0.95,
                "the factory Output clipped a maximum-Humanise P"
                    + std::to_string (performer + 1) + " rim strike: order "
                    + std::to_string (worstOrder) + " peaked at "
                    + std::to_string (worstPeak));
    }
}

// A nagado-daiko is byo-uchi: the head is nailed on with a ring of iron tacks,
// each holding down its share of the head's tension, and a stroke that catches
// the hoop hard enough lifts the head against that preload and sets them
// chattering. What used to be here instead was a fixed 0.08 of broadband noise
// added to a contact whose amplitude runs to thousands of newtons - 87 dB down,
// inaudible at every setting, answering to neither the Stick Noise control nor
// the velocity nor the drum.
void testTheTackLineRattlesOnlyWhenItIsBeaten()
{
    const auto lineFor = [] (taikor::EngineParameters parameters,
                             taikor::Articulation articulation, float velocity,
                             int octave = 0)
    {
        parameters.humanise = 0.0f;
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, defaultBlockSize);
        engine.reset();
        engine.trigger (articulation, octave, velocity);
        auto result = taikor::TaikoEngineTestAccess::tackLine (engine);
        float left = 0.0f, right = 0.0f;
        for (int sample = 0; sample < 2400; ++sample)
        {
            engine.process (&left, &right, 1);
            result.peakContactForce = std::max (
                result.peakContactForce,
                taikor::TaikoEngineTestAccess::tackLine (engine).peakContactForce);
        }
        return result;
    };

    const auto base = defaultParameters();

    // Construction owns the source. The single-drum layout retunes one tacked
    // O-daiko across the keyboard, while the family layout changes to rope- and
    // cord-laced heads in its top two octaves. Those drums have no metal tack
    // line to excite, even on a rim stroke.
    for (const float layout : { 0.0f, 1.0f })
    {
        auto parameters = base;
        parameters.octaveBody = layout;
        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
        {
            const bool familyHasTacks = octave <= 1;
            expect (taikor::getDrumDescription (octave).tackedHead == familyHasTacks,
                    "the drum catalogue mislabeled a tacked or laced head");
            const auto line = lineFor (
                parameters, taikor::Articulation::DonRim, 1.0f, octave);
            const bool hasTacks = layout == 0.0f || familyHasTacks;
            expect ((line.scale > 0.0f) == hasTacks,
                    "the tack source did not follow the drum's head construction: "
                        + std::string (taikor::getDrumDescription (octave).displayName));
            expect ((line.rimGain > 0.0f) == hasTacks,
                    "a laced head retained a tack-line rim coupling");
            expect ((line.preload > 0.0f) == hasTacks,
                    "a laced head retained a nonexistent tack preload");
        }
    }

    // Only the strokes that reach the hoop have a tack line at all. The rest
    // land on the middle of the hide, which is not nailed to anything.
    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
    {
        const auto articulation = static_cast<taikor::Articulation> (index);
        const auto line = lineFor (base, articulation, 1.0f);
        const bool catchesHoop = articulation == taikor::Articulation::DonRim
                              || articulation == taikor::Articulation::Ka;
        expect ((line.scale > 0.0f) == catchesHoop,
                "only the strokes that catch the hoop may drive the tacks: "
                    + std::string (taikor::getArticulationDisplayName (articulation)));
    }

    // And it is contact noise, so the Stick Noise control owns it.
    auto silent = base;
    silent.strikeNoise = 0.0f;
    expect (lineFor (silent, taikor::Articulation::DonRim, 1.0f).scale == 0.0f,
            "Stick Noise at zero must silence the tacks with the rest of the "
            "contact noise");

    // The preload is the head's tension carried over one fixed tack spacing.
    // More tension holds it down harder; diameter alone does not change the
    // load carried by one tack.
    auto slack = base;
    slack.tension = 0.25f;
    auto tight = base;
    tight.tension = 0.85f;
    auto small = base;
    small.headDiameter = 0.30f;

    const auto slackLine = lineFor (slack, taikor::Articulation::DonRim, 1.0f);
    const auto tightLine = lineFor (tight, taikor::Articulation::DonRim, 1.0f);
    const auto smallLine = lineFor (small, taikor::Articulation::DonRim, 1.0f);
    const auto full = lineFor (base, taikor::Articulation::DonRim, 1.0f);
    expect (tightLine.preload > slackLine.preload * 2.0f,
            "a tighter head must hold its tacks down harder");
    expect (smallLine.preload == full.preload,
            "diameter changed the tension carried by one fixed tack spacing");

    // The threshold has to be somewhere a player crosses. A light rim shot must
    // not reach it and a full one must clear it comfortably.
    const auto quiet = lineFor (base, taikor::Articulation::DonRim, 0.10f);
    expect (quiet.peakContactForce * quiet.rimGain < quiet.preload,
            "a light rim shot must not lift the head off its tacks at all");
    expect (full.peakContactForce * full.rimGain > full.preload * 3.0f,
            "a full rim shot must clear the preload with room to spare");

    // And it must be audible, which is the part the built state cannot show.
    // Compare one deterministic rim strike with itself, changing only the tack
    // source. A centre stroke is not a valid reference: reciprocal contact made
    // its own upper band grow almost exactly as much with velocity as the rim.
    const auto rimStrike = [] (bool withTacks)
    {
        auto parameters = defaultParameters();
        parameters.humanise = 0.0f;
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, defaultBlockSize);
        engine.reset();
        engine.trigger (taikor::Articulation::DonRim, 0, 0.90f);
        if (! withTacks)
            taikor::TaikoEngineTestAccess::disableTackLine (engine);
        return render (engine, 9600).mono();
    };

    const auto withTacks = rimStrike (true);
    const auto withoutTacks = rimStrike (false);
    std::vector<float> tackOnly (withTacks.size());
    for (std::size_t index = 0; index < tackOnly.size(); ++index)
        tackOnly[index] = withTacks[index] - withoutTacks[index];

    constexpr std::size_t tenMilliseconds = 480;
    const auto fullRms = windowedRms (withTacks, 0u, tenMilliseconds);
    const auto tackRms = windowedRms (tackOnly, 0u, tenMilliseconds);
    const auto share = tackRms / std::max (fullRms, 1.0e-12);
    expect (share > 0.05 && share < 0.50,
            "the tack line must be audible but remain a detail of the rim attack: "
                + std::to_string (20.0 * std::log10 (std::max (share, 1.0e-12)))
                + " dB relative to the complete stroke");

    // The rattle is a bounded metal-on-wood band, not a white shelf whose
    // loudness and top edge follow the host clock. Subtracting the same render
    // with its tack source disabled leaves that path alone while still testing
    // its real threshold, envelope, airborne delay and output routing. Average
    // independent deterministic seeds because a four-millisecond noise event
    // is deliberately too short for one take to estimate its variance well.
    struct TackSpectrum
    {
        double rate { 0.0 };
        double rmsDb { -300.0 };
        double upperToBand { 0.0 };
    };

    std::vector<TackSpectrum> spectra;
    constexpr int tackEnsembleSize = 8;
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        const int sampleCount = static_cast<int> (sampleRate * 0.020);
        double totalPower = 0.0;
        double bandPower = 0.0;
        double upperPower = 0.0;

        for (int take = 0; take < tackEnsembleSize; ++take)
        {
            const auto seed = 0x6a09e667u
                            + static_cast<std::uint32_t> (take) * 0x9e3779b9u;
            const auto renderTack = [&] (bool enabled)
            {
                auto parameters = defaultParameters();
                parameters.humanise = 0.0f;
                taikor::TaikoEngine engine;
                engine.setParameters (parameters);
                engine.prepare (sampleRate, defaultBlockSize);
                engine.reset();
                engine.trigger (taikor::Articulation::DonRim, 0, 0.90f);
                taikor::TaikoEngineTestAccess::setNoiseSeed (engine, seed);
                if (! enabled)
                    taikor::TaikoEngineTestAccess::disableTackLine (engine);
                return render (engine, sampleCount).mono();
            };

            auto tackOnly = renderTack (true);
            const auto without = renderTack (false);
            for (std::size_t sample = 0; sample < tackOnly.size(); ++sample)
                tackOnly[sample] -= without[sample];

            const double rms = windowedRms (tackOnly, 0u, tackOnly.size());
            totalPower += rms * rms;
            bandPower += std::pow (
                10.0, bandLevelDb (tackOnly, sampleRate, 2600.0, 9000.0) / 10.0);
            upperPower += std::pow (
                10.0, bandLevelDb (tackOnly, sampleRate, 9000.0,
                                    0.5 * sampleRate) / 10.0);
        }

        spectra.push_back ({
            sampleRate,
            10.0 * std::log10 (std::max (
                totalPower / static_cast<double> (tackEnsembleSize), 1.0e-30)),
            upperPower / std::max (bandPower, 1.0e-30)
        });
    }

    double quietest = 1.0e30;
    double loudest = -1.0e30;
    std::string spectrumReadings;
    for (const auto& spectrum : spectra)
    {
        quietest = std::min (quietest, spectrum.rmsDb);
        loudest = std::max (loudest, spectrum.rmsDb);
        spectrumReadings += " [" + std::to_string (spectrum.rate) + ": "
                          + std::to_string (spectrum.rmsDb) + " dB, upper/band "
                          + std::to_string (spectrum.upperToBand) + "]";
        expect (spectrum.upperToBand < 0.15,
                "the tack rattle retained a cheap high-frequency noise shelf"
                    + spectrumReadings);
    }
    expect (loudest - quietest < 0.75,
            "the tack rattle changed level with the host sample rate"
                + spectrumReadings);
}

// A collision is an impulse. It can reverse the velocity of a light mode, but
// cannot teleport the membrane to a new displacement at that instant.
void testCollisionChangesVelocityNotDisplacement()
{
    expect (taikor::TaikoEngineTestAccess::shiftedPoleCacheError() < 1.0e-6,
            "the cached live collision coordinates did not follow a resonator retune");
    expect (taikor::TaikoEngineTestAccess::disabledQuadratureScale() == 0.0,
            "a disabled membrane pole retained a stale observation quadrature");

    for (const float poleShift : { 0.64f, 1.0f, 1.73f })
    {
        for (const float retention : { 0.75f, 0.0f, -0.35f })
        {
            const auto state = taikor::TaikoEngineTestAccess::applyCollision (
                0.23, -0.11, retention, poleShift);
            expect (std::abs (state.displacementAfter - state.displacementBefore)
                        < 1.0e-14,
                    "a stick collision stepped modal displacement instead of leaving "
                    "the head continuous");
            expect (std::abs (state.velocityAfter
                              - static_cast<double> (retention) * state.velocityBefore)
                        < std::abs (state.velocityBefore) * 1.0e-10 + 1.0e-10,
                    "a stick collision did not apply restitution to the velocity "
                    "encoded by its live, pitch-shifted poles");
        }
    }

    // At a turning point the head has displacement but no kinetic energy. A
    // collision that acts only on velocity must leave that state stationary,
    // rather than mistaking the pole curvature for motion.
    constexpr double displacement = 0.23;
    const auto turning = taikor::TaikoEngineTestAccess::applyCollision (
        displacement, 0.0, 0.15f, 1.37f, true);
    expect (std::abs (turning.velocityBefore) < 1.0e-10
                && std::abs (turning.velocityAfter) < 1.0e-10,
            "a collision created velocity at a modal turning point");
}

// applyCollisionRetention() normally recovers a mode's instantaneous velocity
// from its live pole coordinates so retention can scale only that, leaving
// displacement continuous. When the pole coordinates themselves have
// collapsed, none of poleRadius, resonator.b0 or liveOmega is usable for that
// recovery, and the function instead falls back to a plain backward
// difference: velocity = q[n] - q[n-1], and the retained q[n-1] is q[n] minus
// retention times that. No ordinary mode ever reaches this - configureResonator
// and applyTensionShift both keep those three coordinates strictly positive -
// so it was previously exercised nowhere in the suite.
void testCollisionRetentionFallsBackOnACollapsedPole()
{
    using taikor::TaikoEngineTestAccess;

    const auto checkFallback = [] (double poleRadius, double sine, double liveOmega,
                                   const std::string& why)
    {
        for (const float retention : { 0.75f, 0.0f, -0.35f })
        {
            const auto state = TaikoEngineTestAccess::applyDegenerateCollision (
                0.31, -0.08, retention, poleRadius, sine, liveOmega);
            expect (state.displacementAfter == state.displacementBefore,
                    "a collapsed pole (" + why + ") must leave displacement untouched");
            expect (std::abs (state.velocityAfter
                              - static_cast<double> (retention) * state.velocityBefore)
                        < 1.0e-12,
                    "a collapsed pole (" + why
                        + ") must scale the backward-difference velocity by retention");
        }
    };

    // Each case leaves the other two coordinates healthy so only the named
    // one is responsible for taking the fallback.
    checkFallback (0.0, 1.0, 500.0, "non-positive pole radius");
    checkFallback (0.98, 0.0, 500.0, "near-zero sine coefficient");
    checkFallback (0.98, 1.0, 0.0, "non-positive live omega");
}

// Tsu is played with the free hand resting on the hide. That hand belongs to
// the physical drum, not only to the new MIDI voice: it must choke the Don that
// was already ringing, while a Tsu on another drum leaves it alone.
void testMutedStrokeChokesTheRingingHead()
{
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;
    parameters.tensionModulation = 0.0f;
    parameters.outputGain = 0.12f;

    struct Result
    {
        std::vector<float> tail;
        taikor::TaikoEngineTestAccess::LocalMuteState mute;
    };

    const auto after = [&parameters] (std::optional<taikor::Articulation> second,
                                      int secondOctave, bool cancelPalm = false)
    {
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.92f);
        (void) render (engine, 4800);

        Result result;
        if (second.has_value())
        {
            engine.trigger (*second, secondOctave, 0.82f);
            result.mute = taikor::TaikoEngineTestAccess::oldestActiveMute (engine);
            if (cancelPalm)
                taikor::TaikoEngineTestAccess::cancelOldestMute (engine);
            // Remove the newly struck sound. What remains is the old Don after
            // the new physical contact has acted on the shared head.
            taikor::TaikoEngineTestAccess::silenceNewestVoice (engine);
        }
        result.tail = render (engine, 14400).mono();
        return result;
    };

    const auto untouched = after (std::nullopt, 0);
    const auto anotherDrum = after (taikor::Articulation::Tsu, 1);
    expect (maximumAbsoluteDifference (untouched.tail, anotherDrum.tail) < 1.0e-6,
            "a muted stroke damped a different physical drum");

    // Ka's small legacy muteAmount voices its newly struck constrained head;
    // it is not a free hand and must not schedule or prolong Tsu's palm hold on
    // a voice that was already ringing.
    const auto ka = after (taikor::Articulation::Ka, 0);
    expect (ka.mute.ticks == 0,
            "Ka's voicing loss was mistaken for a held palm contact");

    // At the geometric centre all four cardinal points are one patch radius
    // away. The inward point crosses the origin; clamping its signed coordinate
    // to zero would turn a symmetric disk quadrature into a centre-heavy one.
    const auto centredRadii =
        taikor::TaikoEngineTestAccess::palmRadii (0.0f, 0.2f);
    for (std::size_t index = 1; index < centredRadii.size(); ++index)
        expect (std::abs (centredRadii[index] - 0.2f) < 1.0e-7f,
                "a palm cardinal point crossing the centre lost radial symmetry");

    // Same Tsu contact in both branches. The control branch cancels only the
    // subsequent palm hold, so strike radius, membrane gain, restitution and
    // immediate continuum coverage cannot masquerade as mute damping.
    const auto collisionOnly = after (taikor::Articulation::Tsu, 0, true);
    const auto muted = after (taikor::Articulation::Tsu, 0);
    const double collisionTail = windowedRms (collisionOnly.tail, 2400u, 8640u);
    const double mutedTail = windowedRms (muted.tail, 2400u, 8640u);

    expect (muted.mute.ticks > 0,
            "Tsu did not leave a damping contact on the already-ringing head");
    expect (muted.mute.maximumGain - muted.mute.minimumGain > 0.002f,
            "the free hand became a global volume envelope instead of a local "
            "modal damping patch");
    expect (mutedTail < collisionTail * 0.65,
            "Tsu failed to choke the head that was already ringing: "
                + std::to_string (mutedTail / std::max (collisionTail, 1.0e-30))
                + ", gains " + std::to_string (muted.mute.minimumGain) + " to "
                + std::to_string (muted.mute.maximumGain));

    const auto tick = taikor::TaikoEngineTestAccess::probeMuteTick();
    expect (tick.membraneModes > 0,
            "the real palm-tick probe found no membrane state to inspect");
    expect (tick.displacementError < 1.0e-14,
            "a viscous palm tick stepped modal displacement");
    expect (tick.velocityError < 1.0e-7 && tick.kineticError < 1.0e-7
                && tick.decayError < 1.0e-6,
            "a Tsu palm did not enter the continuous pole without stepping state");
    expect (tick.shellStateError == 0.0,
            "a palm resting on the head changed the wooden shell state");

    // The palm has a fixed physical area. Recover its continuous damping rate
    // from the equivalent control-interval gain and check the area/modal-mass
    // law directly, rather than relying on a rendered level that also contains
    // the articulation and microphones.
    const auto muteState = [&parameters] (float diameter, double sampleRate)
    {
        auto sized = parameters;
        sized.headDiameter = diameter;

        taikor::TaikoEngine engine;
        engine.setParameters (sized);
        engine.prepare (sampleRate, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.92f);
        (void) render (engine, static_cast<int> (0.10 * sampleRate));
        engine.trigger (taikor::Articulation::Tsu, 0, 0.82f);
        return taikor::TaikoEngineTestAccess::oldestActiveMute (engine);
    };
    const auto dampingRate = [] (float gain, double sampleRate)
    {
        return -std::log (std::clamp (static_cast<double> (gain), 1.0e-30, 1.0))
             * sampleRate
             / static_cast<double> (taikor::TaikoEngineTestAccess::controlInterval);
    };

    const auto small = muteState (0.75f, 48000.0);
    const auto large = muteState (1.50f, 48000.0);
    const double smallRate = dampingRate (small.fundamentalGain, 48000.0);
    const double largeRate = dampingRate (large.fundamentalGain, 48000.0);
    const double sizeRatio = smallRate / std::max (largeRate, 1.0e-12);
    expect (sizeRatio > 2.5 && sizeRatio < 6.5,
            "a fixed palm must damp the fundamental approximately with inverse "
            "head area; recovered small/large rate ratio was "
                + std::to_string (sizeRatio));

    const double rate44 = dampingRate (
        muteState (1.50f, 44100.0).fundamentalGain, 44100.0);
    const double rate96 = dampingRate (
        muteState (1.50f, 96000.0).fundamentalGain, 96000.0);
    expect (std::abs (rate44 - rate96) < std::max (rate44, rate96) * 0.001,
            "the palm's physical damping rate changed with the host sample rate");
}

void testHandControllerIsAPhysicalPalm()
{
    const auto at48 = taikor::TaikoEngineTestAccess::probeHandTick (48000.0);
    expect (at48.membraneModes > 0,
            "the CC1 palm probe found no membrane modes");
    expect (at48.displacementError < 1.0e-14,
            "CC1 stepped head displacement instead of applying viscous loss");
    expect (at48.velocityError < 1.0e-7 && at48.kineticIncrease < 1.0e-10,
            "changing CC1 pole loss stepped modal velocity: relative error "
                + std::to_string (at48.velocityError) + ", kinetic increase "
                + std::to_string (at48.kineticIncrease));
    expect (at48.shellStateError == 0.0,
            "CC1 damped the wooden shell instead of the hide");
    expect (at48.continuumRate > 0.0f && at48.continuumError < 1.0e-7,
            "CC1 did not damp the unresolved head with the palm-area energy law");
    expect (at48.minimumRate > 0.0f
                && at48.maximumRate > at48.minimumRate * 1.1f,
            "CC1 collapsed a finite palm patch into one global gain");
    expect (at48.poleDecayError < 1.0e-6
                && at48.modesAboveControlNyquist > 0,
            "CC1 was not realised as continuous per-sample pole damping");
    expect (at48.quadratureScaleError < 1.0e-6,
            "CC1 changed a pole without refreshing its observation quadrature");
    expect (at48.releaseError < 1.0e-7,
            "lifting the CC1 palm stepped state or left damping in the pole");

    const auto largeDrum =
        taikor::TaikoEngineTestAccess::probeHandTick (48000.0, 0);
    expect (largeDrum.axisBranchPairs > 0
                && largeDrum.axisBranchScalingError < 1.0e-6,
            "CC1 ignored the batter-head energy fraction of a cavity-split mode");

    const auto at44 = taikor::TaikoEngineTestAccess::probeHandTick (44100.0);
    const auto at96 = taikor::TaikoEngineTestAccess::probeHandTick (96000.0);
    const auto sameRate = [] (float left, float right)
    {
        return std::abs (left - right)
             < 0.001f * std::max ({ std::abs (left), std::abs (right), 1.0f });
    };
    expect (sameRate (at44.fundamentalRate, at96.fundamentalRate)
                && sameRate (at44.minimumRate, at96.minimumRate)
                && sameRate (at44.maximumRate, at96.maximumRate)
                && sameRate (at44.continuumRate, at96.continuumRate),
            "CC1's physical damping rates changed with the host sample rate");
}

// A drum has one head, and a stroke lands on whatever that head is already
// doing. Every stroke used to build an independent copy of the modal bank and
// the voices only summed, so eight identical strokes were bit-identical to
// eight copies of one stroke added offline - which is exactly the arithmetic a
// sample library does, and the place a model has no excuse for matching it.

void testAStrokeLandsOnAHeadThatIsAlreadyMoving()
{
    auto parameters = defaultParameters();
    // Keep authored positions exact and the output linear. The offline sum
    // below uses each stroke's matching sequence index so the natural contact
    // differences cannot masquerade as interaction with a moving head.
    parameters.humanise = 0.0f;
    parameters.drive = 0.0f;
    parameters.outputGain = 0.02f;

    constexpr int spacing = 3000;   // 62.5 ms, a fast but playable roll
    constexpr int strokes = 8;
    constexpr int total = strokes * spacing + 48000;

    // Rendered one sample at a time so a stroke can be placed exactly, which is
    // what makes the offline superposition below an exact prediction of the old
    // behaviour rather than an approximation of it.
    const auto rollOf = [&parameters] (int count)
    {
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, 1);
        engine.reset();

        std::vector<float> mono (static_cast<std::size_t> (total));
        float left = 0.0f;
        float right = 0.0f;
        int placed = 0;
        for (int sample = 0; sample < total; ++sample)
        {
            if (placed < count && sample == placed * spacing)
            {
                engine.trigger (taikor::Articulation::Don, 0, 0.9f);
                const int slot =
                    taikor::TaikoEngineTestAccess::newestStrikeSlot (engine);
                taikor::TaikoEngineTestAccess::isolateResolvedBank (engine, slot);
                ++placed;
            }
            engine.process (&left, &right, 1);
            mono[static_cast<std::size_t> (sample)] = 0.5f * (left + right);
        }
        return mono;
    };

    const auto roll = rollOf (strokes);

    std::vector<double> superposed (static_cast<std::size_t> (total), 0.0);
    for (int stroke = 0; stroke < strokes; ++stroke)
    {
        taikor::TaikoEngine isolated;
        isolated.setParameters (parameters);
        isolated.prepare (48000.0, 1);
        taikor::TaikoEngineTestAccess::setNoteSequence (
            isolated, static_cast<std::uint64_t> (stroke));
        isolated.trigger (taikor::Articulation::Don, 0, 0.9f);
        taikor::TaikoEngineTestAccess::isolateResolvedBank (isolated);
        const auto single = render (isolated, total - stroke * spacing, 1).mono();
        for (int sample = stroke * spacing; sample < total; ++sample)
            superposed[static_cast<std::size_t> (sample)] +=
                single[static_cast<std::size_t> (sample - stroke * spacing)];
    }

    // Up to the second stroke the two must agree exactly: nothing has landed on
    // anything yet, and a mechanism that quietly changed a single stroke would
    // not be this one.
    double firstStrokeDifference = 0.0;
    for (int sample = 0; sample < spacing; ++sample)
        firstStrokeDifference = std::max (
            firstStrokeDifference,
            std::abs (superposed[static_cast<std::size_t> (sample)]
                      - roll[static_cast<std::size_t> (sample)]));
    expect (firstStrokeDifference < 1.0e-6,
            "the first stroke of a roll must be untouched by the strokes after it");

    // Measured over what is left once the last stick has come off the head.
    // Summing the whole take instead measures the eight attacks, which are the
    // one part of a roll no head damping can touch: they are the blows
    // themselves. What the mechanism decides is how much of each of them is
    // still there afterwards, and by the end of eight strokes the first one's
    // fundamental has been sat on seven times.
    constexpr int afterLastStroke = strokes * spacing;
    double engineEnergy = 0.0;
    double summedEnergy = 0.0;
    for (int sample = afterLastStroke; sample < total; ++sample)
    {
        const double engineSample = roll[static_cast<std::size_t> (sample)];
        const double summedSample = superposed[static_cast<std::size_t> (sample)];
        engineEnergy += engineSample * engineSample;
        summedEnergy += summedSample * summedSample;
    }

    expect (summedEnergy > 0.0, "the roll produced nothing to measure");
    const auto shortfall = 10.0 * std::log10 (engineEnergy / std::max (summedEnergy, 1.0e-30));
    // The stochastic continuum adds new contacts in energy and is deliberately
    // not a coherent linear superposition, so the probe above isolates the
    // resolved object whose reciprocal collision this assertion owns. A
    // collision changes velocity and leaves displacement continuous. It can
    // move the phase of a later tail toward or away from the offline sum, so a
    // finite-window energy comparison has no physically fixed sign. What must
    // be present is a bounded, measurable interaction; independent sampled
    // voices give exactly zero. The per-entry cavity solve moves this fixed
    // reference case from 5.862 to 6.608 dB by shifting the radial modes, so
    // seven keeps the same sub-decibel guard above the measured interaction.
    expect (std::abs (shortfall) > 0.02 && std::abs (shortfall) < 7.0,
            "a roll did not leave a bounded interaction with the already-moving "
            "head: " + std::to_string (shortfall) + " dB from offline addition");

    // And it has to be one drum. A stroke an octave away is a different
    // instrument standing beside it and cannot reach this head at all.
    auto probe = parameters;
    // Full velocity depth, so the interrupting stroke can be made genuinely
    // quiet: it has to be measurably weaker than what it is landing on, or the
    // window below is measuring the new stroke rather than the old one.
    probe.velocityDepth = 1.0f;

    const auto acrossDrums = [&probe] (int interruptingOctave)
    {
        taikor::TaikoEngine engine;
        engine.setParameters (probe);
        engine.prepare (48000.0, 1);
        engine.reset();

        std::vector<float> mono (static_cast<std::size_t> (total));
        float left = 0.0f;
        float right = 0.0f;
        engine.trigger (taikor::Articulation::Don, 0, 1.0f);
        for (int sample = 0; sample < total; ++sample)
        {
            // A Tsu lands near the middle, where the fundamental is. An octave
            // below the playable range means no second stroke at all.
            if (sample == 12000 && interruptingOctave >= taikor::lowestOctaveOffset)
                engine.trigger (taikor::Articulation::Tsu, interruptingOctave, 0.02f);
            engine.process (&left, &right, 1);
            mono[static_cast<std::size_t> (sample)] = 0.5f * (left + right);
        }
        return mono;
    };

    const auto reported = taikor::TaikoEngine::measure (probe, 0);
    // Half a second after the interruption and later, so the ghost stroke's own
    // attack is long gone and what is left in this band is the first stroke's
    // fundamental.
    const auto remaining = [&reported] (const std::vector<float>& samples)
    {
        return binMagnitude (samples, reported.loadedFundamentalHz, 48000.0, 24000u,
                             44000u);
    };

    const auto interrupted = remaining (acrossDrums (0));
    const auto untouched = remaining (acrossDrums (2));
    expect (untouched > 0.0, "the drum must still be ringing to measure");
    expect (interrupted < untouched * 0.85,
            "a ghost stroke on the same drum must take a real bite out of what is "
            "still ringing there");
    // The control: the same ghost stroke on the drum an octave up leaves the
    // first one exactly where it was, because it is not the same head.
    const auto undisturbed = remaining (acrossDrums (-99));
    expect (std::abs (untouched - undisturbed) < undisturbed * 0.02,
            "a stroke on a different drum must not damp this one");
}

// The attack pitch glide is the head stretching itself. A membrane clamped at
// its rim cannot move without getting longer, and a longer head is a tighter
// one, so the tension rises with the square of the displacement. Everything
// that follows from that had to be true and was not: the glide used to be a
// fixed 115 ms envelope whose depth read the impact speed and nothing else -
// not the tension it was fighting, not the size of the head, not the material -
// with a term on top of it that scaled with the engine's own output
// calibration.
void testTheAttackGlideComesFromTheHead()
{
    // The largest frequency multiplier the glide reaches over the first third
    // of a second, which is where the head is moving enough to matter.
    const auto peakGlide = [] (taikor::EngineParameters parameters,
                               taikor::Articulation articulation, float velocity)
    {
        parameters.humanise = 0.0f;
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, 64);
        engine.reset();
        engine.trigger (articulation, 0, velocity);

        std::array<float, 64> left {};
        std::array<float, 64> right {};
        float worst = 1.0f;
        for (int block = 0; block < 250; ++block)
        {
            engine.process (left.data(), right.data(), 64);
            worst = std::max (worst,
                              taikor::TaikoEngineTestAccess::appliedTensionShift (engine));
        }
        return worst;
    };

    const auto base = defaultParameters();

    auto full = base;
    full.tensionModulation = 1.0f;
    const auto hard = peakGlide (full, taikor::Articulation::Don, 1.0f);
    const auto soft = peakGlide (full, taikor::Articulation::Don, 0.25f);

    expect (hard > 1.002f,
            "a hard stroke must bend the head sharp");
    // Against the excess over unity, because these are frequency multipliers
    // and the claim is about how far the head was pushed, not about the pitch
    // it settles back to.
    expect (hard - 1.0f > (soft - 1.0f) * 3.0f,
            "the glide must follow how far the head is actually pushed, so a hard "
            "stroke must bend markedly further than a light one");

    // Off entirely at zero, with no residue. The control is a depth on a
    // physical term, so zero has to mean the head is treated as linear.
    auto none = base;
    none.tensionModulation = 0.0f;
    const auto silentGlide = peakGlide (none, taikor::Articulation::Don, 1.0f);
    expect (std::abs (silentGlide - 1.0f) < 1.0e-6f,
            "with Tension Mod at zero nothing may bend the head");

    // It is a live performance control, not a property latched by the next
    // note. Turning it on must immediately read the displacement already in the
    // shared head, and turning it back off must release the shift.
    {
        taikor::TaikoEngine engine;
        engine.setParameters (none);
        engine.prepare (48000.0, 64);
        engine.trigger (taikor::Articulation::Don, 0, 1.0f);
        render (engine, 64, 64);

        engine.setParameters (full);
        render (engine, 256, 64);
        expect (taikor::TaikoEngineTestAccess::appliedTensionShift (engine) > 1.002f,
                "automating Tension Mod on did not reach the already-ringing head");

        engine.setParameters (none);
        render (engine, 64, 64);
        expect (std::abs (
                    taikor::TaikoEngineTestAccess::appliedTensionShift (engine) - 1.0f)
                    < 1.0e-5f,
                "automating Tension Mod off left a stale attack shift");
    }

    // The claim the old envelope could not make. The fractional tension a
    // displacement buys goes as the head's in-plane stiffness over the tension
    // it already has, and the displacement a given blow produces falls with the
    // tension too, so a tight head bends very much less than a slack one. On a
    // real drum this is the difference between an o-daiko, which audibly bends
    // on every hard stroke, and a shime-daiko, which does not.
    auto slack = full;
    slack.tension = 0.25f;
    auto tight = full;
    tight.tension = 0.85f;

    const auto slackGlide = peakGlide (slack, taikor::Articulation::Don, 1.0f);
    const auto tightGlide = peakGlide (tight, taikor::Articulation::Don, 1.0f);
    expect (slackGlide - 1.0f > (tightGlide - 1.0f) * 3.0f,
            "a slack head must bend far further than a tight one struck the same way");

    // And it has to be the head's strain doing it, not one selected low mode.
    // A Ka suppresses the axisymmetric displacement but excites short,
    // high-gradient non-axisymmetric shapes at the edge. Von Karman strain is
    // the integral of |grad w|^2, so those modes stretch the hide more than the
    // smoother Don does even when their average displacement is smaller.
    const auto edgeGlide = peakGlide (full, taikor::Articulation::Ka, 1.0f);
    expect (edgeGlide - 1.0f > (hard - 1.0f) * 1.5f,
            "the high-gradient edge modes stopped contributing to head strain");
}

void testAttackGlideChangesOnlyBatterTension()
{
    using Access = taikor::TaikoEngineTestAccess;
    for (const float material : { 0.0f, 1.0f })
        for (const float coupling : { 0.0f, 1.0f })
            for (const int octave : { 0, 3 })
            {
                auto parameters = defaultParameters();
                parameters.headMaterial = material;
                parameters.cavityCoupling = coupling;
                const auto probe = Access::tensionProjectionProbe (parameters, octave);
                expect (probe.eigensolveError < 2.0e-5,
                        "attack glide disagrees with an independent two-head "
                        "stiffness perturbation: " + std::to_string (probe.eigensolveError));
                expect (probe.minimumShare >= 0.0f && probe.maximumShare <= 1.0f,
                        "the batter tensile energy fraction escaped [0, 1]");
                expect (probe.maximumShare > 0.25f,
                        "tension modulation lost its effect on batter modes");
                expect (probe.musicalShiftError < 3.0e-7
                            && probe.roundTripError < 3.0e-7,
                        "strain projection changed musical tuning or compounded "
                        "through a retuning round trip");
                expect (probe.shellShiftError < 3.0e-7,
                        "head strain retuned the wooden shell");
                if (coupling == 0.0f)
                    expect (probe.rearOnlyModes > 0 && probe.rearOnlyShare == 0.0f,
                            "straining an uncoupled batter head bent the rear head");
                else
                    expect (probe.maximumShare - probe.minimumShare > 0.2f,
                            "cavity and batter modes received the same attack glide");
            }
}

// A normal head strike transfers energy into the mounting through the resolved
// edge loss. It does not also apply a duplicate, one-way copy of the solved
// normal force to a bank of shell oscillators. Don Rim is the one articulation
// that actually catches the hoop and therefore owns the direct wooden path.
void testOnlyTheHoopStrikeDrivesTheShell()
{
    for (int octave = taikor::lowestOctaveOffset;
         octave <= taikor::highestOctaveOffset; ++octave)
    {
        for (const auto articulation : {
                 taikor::Articulation::Don,
                 taikor::Articulation::Ka,
                 taikor::Articulation::Tsu,
                 taikor::Articulation::DonRim })
        {
            auto parameters = defaultParameters();
            parameters.humanise = 0.0f;

            taikor::TaikoEngine engine;
            engine.setParameters (parameters);
            engine.prepare (48000.0, defaultBlockSize);
            engine.trigger (articulation, octave, 0.9f);

            const auto drive = taikor::TaikoEngineTestAccess::woodDrive (engine);
            if (articulation == taikor::Articulation::DonRim)
                expect (drive > 0.0, "a hoop strike stopped driving the shell");
            else
                expect (drive == 0.0,
                        "a head-only stroke fed the one-way shell bank");
        }
    }
}

// Shell Resonance is a continuous control and has to behave like one. It used
// not to: a shaper sold as saturation sat on the wooden bank's drive behind a
// gate at 1 %, and because its clamp was never reached and its cubic term was
// 58 dB down, the only thing it actually did was hand the shell a 1.2x gain the
// moment the control crossed that gate.
void testShellResonanceHasNoStepInIt()
{
    // Read the bank's drive on the one stroke that directly reaches the body.
    // Finished Don Rim audio also contains the head and hoop transient, while
    // this projection is exactly where a control discontinuity would occur.
    const auto shellDrive = [] (float shellResonance)
    {
        auto parameters = defaultParameters();
        parameters.shellResonance = shellResonance;
        parameters.humanise = 0.0f;

        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::DonRim, 0, 0.9f);
        return taikor::TaikoEngineTestAccess::woodDrive (engine);
    };

    double previous = shellDrive (0.0f);
    expect (previous > 0.0, "the body must still ring with the shell control at zero");

    // Fine steps across the gate that used to be there. A quarter of a per cent
    // of a control that spans nineteen decibels end to end cannot legitimately
    // move the output by a third of one.
    for (float shellResonance = 0.0025f; shellResonance <= 0.0501f;
         shellResonance += 0.0025f)
    {
        const auto level = shellDrive (shellResonance);
        const auto step = std::abs (20.0 * std::log10 (level / previous));
        expect (step < 0.3,
                "Shell Resonance stepped by " + std::to_string (step)
                    + " dB at " + std::to_string (shellResonance));
        previous = level;
    }

    // And the control still has to do its job over its whole range, or the
    // check above would be satisfied by a control that does nothing at all.
    expect (shellDrive (1.0f) > shellDrive (0.0f) * 1.9,
            "Shell Resonance must still open the body up across its range");
}

// A taiko head is a stretched plate rather than an ideal membrane - treated cow
// skin at about 3.5 GPa, held at a tension far above a drum-kit head's - so its
// modal ratios are not constants of the geometry. They open out with the mode's
// order, and they open out further the smaller and the thicker the head is,
// which is most of what separates a shime-daiko's spectrum from an odaiko's.
void testHeadStiffnessOpensTheModalRatios()
{
    const auto base = defaultParameters();

    // Head Material is thickness as well as density, and the flexural rigidity
    // goes as the cube of thickness, so the two ends of that control are two
    // and a half orders of magnitude apart in stiffness rather than a few per
    // cent. A thin synthetic film really is an ideal membrane.
    auto film = base;
    film.headMaterial = 0.0f;
    auto hide = base;
    hide.headMaterial = 1.0f;

    const auto filmStiffness =
        taikor::TaikoEngine::measure (film, 0).headStiffnessParameter;
    const auto hideStiffness =
        taikor::TaikoEngine::measure (hide, 0).headStiffnessParameter;

    expect (filmStiffness > 0.0f && std::isfinite (filmStiffness),
            "the head's stiffness must be a positive finite number");
    expect (hideStiffness > filmStiffness * 100.0f,
            "a thick hide must be at least two orders of magnitude stiffer than a "
            "thin film against the same tension");

    // The tuning may not move. The stretch is taken relative to the (0,1) mode
    // precisely so that the pitch a player tunes the drum to stays a membrane
    // frequency however stiff the head is; if it did not, an octave would stop
    // being an octave, because the stiffness parameter falls with tension and
    // with the square of the radius and the two halves of the Octave Body
    // transform move it in opposite directions.
    for (const float material : { 0.0f, 0.5f, 1.0f })
        for (const int octave : { -2, 0, 3 })
        {
            auto tuned = base;
            tuned.headMaterial = material;
            const auto measured = taikor::TaikoEngine::measure (tuned, octave);
            const auto membrane = measured.waveSpeedMetresPerSecond * 2.4048255577f
                                / (2.0f * 3.14159265358979f * measured.radiusMetres);
            expect (std::abs (measured.idealFundamentalHz - membrane)
                        < membrane * 1.0e-4f,
                    "the drum's tuning must stay an ideal membrane frequency however "
                    "stiff the head is");
        }

    // And the claim that actually needs the physics: how far the top of the
    // resolved bank sits above the drum's own fundamental has to depend on the
    // head's stiffness against its tension.
    //
    // Head Tension is the control that isolates it. It moves the stiffness
    // parameter as 1/T and leaves the air load - which depends only on the
    // areal density and the radius - exactly where it was, so the ratio below
    // can only move through the stiffness term. Measured against the reported
    // ideal fundamental rather than against the lowest mode the bank built,
    // because that one is the lower branch of the cavity pair and the air
    // spring behind it does not scale with the head's tension at all.
    const auto topModeRatio = [&] (float tension)
    {
        auto parameters = base;
        parameters.tension = tension;

        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.9f);

        const auto modes = taikor::TaikoEngineTestAccess::membraneFrequencies (engine);
        const auto measured = taikor::TaikoEngine::measure (parameters, 0);
        expect (! modes.empty() && measured.idealFundamentalHz > 0.0f,
                "the head must build a modal bank to measure");
        return modes.empty() ? 0.0 : modes.back() / measured.idealFundamentalHz;
    };

    const auto slack = topModeRatio (0.2f);
    const auto tight = topModeRatio (0.9f);

    expect (slack > 0.0 && tight > 0.0, "the modal ratio measurement failed to run");
    // A point and a half rather than two. The head's stiffness against its own
    // tension is D/(T a^2), so it falls as the square of the radius: on the
    // five-shaku reference drum B is 0.7e-4 where the three-shaku one it
    // replaced had 2.2e-4, and there is correspondingly less of it for the
    // tension to compete with. Measured at 1.017.
    expect (slack > tight * 1.015,
            "a slack head must spread its upper modes further above its fundamental "
            "than a tight one, because stiffness matters more the less tension there "
            "is to compete with it: " + std::to_string (slack / tight));

    // Nothing reachable from the controls may make the head's stiffness
    // meaningless. The corners are a tiny thick head at no tension and a huge
    // thin one at full tension, which is where the ratio D/(T a^2) is largest
    // and smallest.
    for (const float diameter : { 0.15f, 1.80f })
        for (const float material : { 0.0f, 1.0f })
            for (const float tension : { 0.0f, 1.0f })
                for (const int octave : { taikor::lowestOctaveOffset,
                                          taikor::highestOctaveOffset })
                {
                    auto hostile = base;
                    hostile.headDiameter = diameter;
                    hostile.headMaterial = material;
                    hostile.tension = tension;
                    const auto stiffness = taikor::TaikoEngine::measure (hostile, octave)
                                               .headStiffnessParameter;
                    expect (std::isfinite (stiffness) && stiffness >= 0.0f,
                            "the head's stiffness must stay finite everywhere the "
                            "controls can reach");
                }
}

// The head's continuum belongs to the head, so everything that happens to the
// head has to reach it. Three things did not, and all three were invisible in
// the resolved bank because that is not where most of the drum's high-frequency
// energy lives any more.
void testTheContinuumFollowsTheHead()
{
    const auto highBand = [] (const std::vector<float>& samples, std::size_t first,
                              std::size_t last)
    {
        double total = 0.0;
        for (double frequency = 700.0; frequency < 6000.0; frequency *= 1.06)
            total += binMagnitude (samples, frequency, 48000.0, first, last);
        return total;
    };

    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;

    // Pitch and the wheel are head tension, and the continuum is the head, so a
    // stroke that is already ringing has to be carried with the resolved bank
    // rather than left standing under it.
    //
    // Checked on the filters rather than on the audio. The bands are wide and
    // overlapping and the whole region is gone inside a couple of hundred
    // milliseconds, so moving their centres by an octave changes the summed
    // spectrum by less than the measurement noise - the effect is real and
    // worth having for coherence, but it is not one a spectral test can resolve
    // honestly, and an assertion that cannot fail is worse than none.
    {
        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 1.0f);
        render (engine, 480);

        const auto before = taikor::TaikoEngineTestAccess::continuumCoefficients (engine);
        expect (! before.empty(), "the stroke built no continuum to retune");

        auto raised = parameters;
        raised.pitch = 12.0f;
        engine.setParameters (raised);
        render (engine, 480);

        const auto after = taikor::TaikoEngineTestAccess::continuumCoefficients (engine);
        expect (after.size() == before.size(),
                "retuning must not change how many continuum bands there are");

        bool everyBandRose = ! before.empty();
        for (std::size_t index = 0; index < before.size() && index < after.size(); ++index)
            everyBandRose = everyBandRose && after[index] > before[index] * 1.2f;

        expect (everyBandRose,
                "automating Pitch up must carry the ringing head's continuum up "
                "with its modes");
    }

    // And the articulation's head gain must reach it exactly once. It is
    // already inside the drive the continuum is measured against, so applying
    // it again squared it and left the quiet strokes far darker than their
    // profile asks for.
    {
        const auto brightnessOf = [&] (taikor::Articulation articulation)
        {
            const auto mono = strike (parameters, articulation, 0, 1.0f, 48000.0,
                                      48128).mono();
            const auto measurements = taikor::TaikoEngine::measure (parameters, 0);
            const auto low =
                binMagnitude (mono, measurements.loadedFundamentalHz, 48000.0, 0u, 4800u);
            return highBand (mono, 0u, 4800u) / std::max (low, 1.0e-9);
        };

        // A Ka is quieter than a Don and lands out by the tacks, where the
        // short-wavelength shapes pile up, so in proportion to its own
        // fundamental it must be far brighter rather than far darker.
        expect (brightnessOf (taikor::Articulation::Ka)
                    > brightnessOf (taikor::Articulation::Don) * 1.5,
                "a quiet articulation must not lose its continuum to a squared "
                "head gain");
    }
}

// Caching must be invisible. The engine keeps one resolved drum per octave -
// which is now one per instrument of the family - and a parameter that feeds it
// but is missing from the invalidation list freezes it at whatever value
// happened to be set when the cache was first filled.
//
// This sweeps every parameter rather than that one, so the next field to grow a
// cached dependency is caught by the same test.
void testEveryParameterSurvivesTheCache()
{
    struct Control
    {
        const char* name;
        void (*apply) (taikor::EngineParameters&, float);
    };
    static const std::array<Control, 23> controls {{
        { "Head Diameter",      [] (taikor::EngineParameters& p, float v) { p.headDiameter = 0.20f + 1.00f * v; } },
        { "Body Depth",         [] (taikor::EngineParameters& p, float v) { p.bodyDepth = v; } },
        { "Tension",            [] (taikor::EngineParameters& p, float v) { p.tension = v; } },
        { "Head Material",      [] (taikor::EngineParameters& p, float v) { p.headMaterial = v; } },
        { "Shell Material",     [] (taikor::EngineParameters& p, float v) { p.shellMaterial = v; } },
        { "Resonant Head",      [] (taikor::EngineParameters& p, float v) { p.resonantTension = v; } },
        { "Air Coupling",       [] (taikor::EngineParameters& p, float v) { p.cavityCoupling = v; } },
        { "Head Damping",       [] (taikor::EngineParameters& p, float v) { p.headDamping = v; } },
        { "Shell Resonance",    [] (taikor::EngineParameters& p, float v) { p.shellResonance = v; } },
        { "Pitch",              [] (taikor::EngineParameters& p, float v) { p.pitch = -24.0f + 48.0f * v; } },
        { "Bachi Hardness",     [] (taikor::EngineParameters& p, float v) { p.bachiHardness = v; } },
        { "Strike Position",    [] (taikor::EngineParameters& p, float v) { p.strikePosition = -1.0f + 2.0f * v; } },
        { "Strike Azimuth",     [] (taikor::EngineParameters& p, float v) { p.strikeAzimuth = static_cast<float> (-analysisPi + 2.0 * analysisPi * v); } },
        { "Velocity Depth",     [] (taikor::EngineParameters& p, float v) { p.velocityDepth = v; } },
        { "Velocity Curve",     [] (taikor::EngineParameters& p, float v) { p.velocityCurve = -1.0f + 2.0f * v; } },
        { "Tension Modulation", [] (taikor::EngineParameters& p, float v) { p.tensionModulation = v; } },
        { "Strike Noise",       [] (taikor::EngineParameters& p, float v) { p.strikeNoise = v; } },
        { "Drum Layout",        [] (taikor::EngineParameters& p, float v) { p.octaveBody = v; } },
        { "Mic Distance",       [] (taikor::EngineParameters& p, float v) { p.micDistance = v; } },
        { "Mic Spread",         [] (taikor::EngineParameters& p, float v) { p.micSpread = v; } },
        { "Stereo Width",       [] (taikor::EngineParameters& p, float v) { p.stereoWidth = v; } },
        { "Drive",              [] (taikor::EngineParameters& p, float v) { p.drive = v; } },
        { "Output",             [] (taikor::EngineParameters& p, float v) { p.outputGain = 0.1f + 0.9f * v; } },
    }};

    // Keep authored coordinates exact. The reset below also rewinds the
    // performed stroke sequence, so fresh and reused engines get the same
    // contact and any difference belongs to the cache.
    const auto base = [] {
        auto parameters = defaultParameters();
        parameters.humanise = 0.0f;
        return parameters;
    }();

    for (const auto& control : controls)
        for (const auto articulation : { taikor::Articulation::Don,
                                         taikor::Articulation::Ka,
                                         taikor::Articulation::DonRim })
        {
            auto before = base;
            auto after = base;
            control.apply (before, 0.15f);
            control.apply (after, 0.85f);

            // A fresh engine that has only ever seen the new value.
            taikor::TaikoEngine fresh;
            fresh.setParameters (after);
            fresh.prepare (48000.0, defaultBlockSize);
            fresh.trigger (articulation, 0, 0.85f);
            const auto expected = taikor::TaikoEngineTestAccess::newestVoiceBank (fresh);

            // One that was used at the old value first, exactly as a player
            // would: strike, turn the control, strike again.
            taikor::TaikoEngine reused;
            reused.setParameters (before);
            reused.prepare (48000.0, defaultBlockSize);
            reused.trigger (articulation, 0, 0.85f);
            render (reused, 2400);
            // Clear the live shared bank while retaining the lazy drum cache.
            // Otherwise this compares a continuously retuned old physical
            // state with a newly constructed one, which is history rather than
            // cache identity.
            reused.reset();
            reused.setParameters (after);
            reused.trigger (articulation, 0, 0.85f);
            const auto actual = taikor::TaikoEngineTestAccess::newestVoiceBank (reused);

            const std::string where = std::string (" (") + control.name + ", "
                                    + std::string (taikor::getArticulationDisplayName (articulation))
                                    + ")";

            expect (actual.size() == expected.size(),
                    "turning a control changed how many modes a stroke builds "
                    "depending on what came before it" + where);
            if (actual.size() != expected.size())
                continue;

            bool identical = true;
            for (std::size_t mode = 0; mode < actual.size() && identical; ++mode)
                for (std::size_t term = 0; term < actual[mode].size(); ++term)
                    if (std::abs (actual[mode][term] - expected[mode][term])
                        > 1.0e-4f * std::max (1.0f, std::abs (expected[mode][term])))
                    {
                        identical = false;
                        break;
                    }

            expect (identical,
                    "a stroke after this control moved does not match the same "
                    "stroke on an engine that always had the new value - a cache "
                    "is holding a stale value" + where);
        }
}

// A continuum octave is calibrated as physical RMS after its band-pass. Live
// Pitch/Tension Mod and a structural rebuild move the filter poles, but must
// not create or remove unresolved-head energy merely by changing bandwidth.
// The current output product is pinned too: reciprocal envelope/state scaling
// keeps automation from stepping a ringing noise realization.
void testContinuumRetuningPreservesCalibratedPower()
{
    const auto result = taikor::TaikoEngineTestAccess::continuumRetuneProbe();
    expect (result.lookupDbError < 0.05,
            "the realtime continuum-variance lookup exceeds its 0.05 dB bound: "
                + std::to_string (result.lookupDbError));
    expect (result.livePowerDbError < 0.05,
            "live continuum retuning changed calibrated RMS power by "
                + std::to_string (result.livePowerDbError) + " dB");
    expect (result.liveProductError < 2.0e-7,
            "live continuum retuning stepped the current filter output product");
    expect (result.roundTripPowerDbError < 0.05,
            "an absolute continuum retune round trip accumulated RMS drift");
    expect (result.roundTripProductError < 2.0e-7,
            "an absolute continuum retune round trip accumulated state drift");
    expect (result.laterInjectionDbError < 0.05,
            "a later contact used the pre-retune continuum amplitude coordinate");
    expect (result.rebuildPowerDbError < 1.0e-5,
            "structural automation changed a ringing continuum band's RMS power");
    expect (result.rebuildProductError < 2.0e-7,
            "structural automation stepped the current continuum output product");
}

// The continuum's exact filter variance has its own lazy cache. Its complete
// key is the pair of discrete filter coefficients, so reset may keep an entry,
// while a changed host rate or drum geometry must either reuse an exactly equal
// pair or replace it. Compare both the built bands and the samples: a stale
// normalisation can leave every modal-bank term correct while changing only the
// statistical part of the heard stroke.
void testContinuumVarianceCacheLifecycle()
{
    struct Snapshot
    {
        std::vector<taikor::TaikoEngineTestAccess::ContinuumBandInfo> bands;
        Rendered audio;
    };

    const auto capture = [] (taikor::TaikoEngine& engine)
    {
        engine.trigger (taikor::Articulation::Don, 0, 0.87f);
        return Snapshot { taikor::TaikoEngineTestAccess::continuumBands (engine),
                          render (engine, 4096, 64) };
    };

    const auto compare = [] (const Snapshot& actual, const Snapshot& expected,
                             const std::string& where)
    {
        expect (actual.bands.size() == expected.bands.size(),
                "the continuum cache changed the number of live bands" + where);
        bool bandsExact = actual.bands.size() == expected.bands.size();
        for (std::size_t index = 0; index < actual.bands.size() && bandsExact; ++index)
        {
            const auto& a = actual.bands[index];
            const auto& b = expected.bands[index];
            bandsExact = a.centre == b.centre
                      && a.lowCoefficient == b.lowCoefficient
                      && a.highCoefficient == b.highCoefficient
                      && a.targetRms == b.targetRms
                      && a.distanceGain == b.distanceGain
                      && a.level == b.level;
        }
        expect (bandsExact,
                "a continuum variance-cache hit did not reproduce its miss bit for bit"
                    + where);
        expect (actual.audio.left == expected.audio.left
                    && actual.audio.right == expected.audio.right,
                "a continuum variance-cache lifecycle change altered rendered samples"
                    + where);
    };

    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;
    parameters.tensionModulation = 0.0f;

    taikor::TaikoEngine reused;
    reused.setParameters (parameters);
    reused.prepare (48000.0, 64);
    const auto miss = capture (reused);
    expect (taikor::TaikoEngineTestAccess::validContinuumVarianceEntries (reused) == 5,
            "one drum did not populate exactly its five continuum variance entries");

    reused.reset();
    expect (taikor::TaikoEngineTestAccess::validContinuumVarianceEntries (reused) == 5,
            "reset discarded the coefficient-only continuum variance cache");
    compare (capture (reused), miss, " after reset");

    reused.prepare (96000.0, 64);
    const auto reusedAtNewRate = capture (reused);
    taikor::TaikoEngine freshAtNewRate;
    freshAtNewRate.setParameters (parameters);
    freshAtNewRate.prepare (96000.0, 64);
    compare (reusedAtNewRate, capture (freshAtNewRate),
             " after a sample-rate change");

    for (int variant = 0; variant < 2; ++variant)
    {
        auto changed = parameters;
        const char* name = nullptr;
        if (variant == 0)
        {
            changed.headDiameter = 0.83f;
            name = " after a diameter change";
        }
        else
        {
            changed.pitch = 7.25f;
            name = " after a pitch change";
        }

        reused.setParameters (changed);
        reused.reset();
        const auto reusedChanged = capture (reused);

        taikor::TaikoEngine fresh;
        fresh.setParameters (changed);
        fresh.prepare (96000.0, 64);
        compare (reusedChanged, capture (fresh), name);
    }
}

// The top membrane modes carry a circumferential order of eight, so the
// radiation law's exponent is eighteen, and forming (ka)^18 on its own leaves
// float range once ka passes about 138. That is reachable on a small, hard,
// high-tension head two octaves up at a high sample rate, and it produced
// inf/inf: a NaN decay rate, which the lifetime pass then read as zero samples
// and quietly retired seven high partials out of an edge stroke.
void testRadiationEfficiencyStaysFinite()
{
    for (int order = 0; order <= 8; ++order)
    {
        float previous = -1.0f;
        for (float ka : { 1.0e-6f, 1.0e-3f, 0.1f, 1.0f, 10.0f, 100.0f, 138.0f, 260.0f,
                          1.0e4f, 1.0e8f, 1.0e20f, 1.0e30f })
        {
            const auto efficiency =
                taikor::TaikoEngineTestAccess::radiationEfficiency (order, ka);
            const std::string where = " (order " + std::to_string (order) + ", ka "
                                    + std::to_string (ka) + ")";

            expect (std::isfinite (efficiency),
                    "radiation efficiency must stay finite" + where);
            expect (efficiency >= 0.0f && efficiency <= 1.0f,
                    "radiation efficiency must stay a fraction" + where);
            expect (efficiency >= previous - 1.0e-6f,
                    "radiation efficiency must rise with ka" + where);
            previous = efficiency;
        }

        expect (taikor::TaikoEngineTestAccess::radiationEfficiency (order, 1.0e20f)
                    > 0.99f,
                "a very large ka must saturate the efficiency, not annihilate it "
                "(order " + std::to_string (order) + ")");
    }

    // And the whole way through the engine, at the extremes that reach it: the
    // smallest, hardest, most sharply tuned head this instrument can describe,
    // at every supported sample rate.
    for (const double rate : { 8000.0, 48000.0, 96000.0, 192000.0, 384000.0 })
    {
        auto parameters = defaultParameters();
        parameters.tension = 1.0f;
        parameters.headMaterial = 0.0f;
        parameters.pitch = 24.0f;
        parameters.octaveBody = 0.0f;
        parameters.headDiameter = 0.20f;

        taikor::TaikoEngine engine;
        engine.setParameters (parameters);
        engine.prepare (rate, defaultBlockSize);

        for (std::size_t index = 0; index < taikor::articulationCount; ++index)
            engine.trigger (static_cast<taikor::Articulation> (index),
                            taikor::highestOctaveOffset, 1.0f);

        expect (taikor::TaikoEngineTestAccess::everyDecayIsFinite (engine),
                "an extreme drum produced a non-finite mode at "
                + std::to_string (static_cast<int> (rate)) + " Hz");

        const auto rendered = render (engine, 4800);
        expect (rendered.finite,
                "an extreme drum produced non-finite audio at "
                + std::to_string (static_cast<int> (rate)) + " Hz");
    }
}

// The two heads are not coincident sources. Opposing head motion only cancels
// to zero in the kd -> 0 limit; across a finite body it radiates as a dipole.
// This is most exposed by the lower (0,1) branch of the shallow Shime, where
// the coincident-source approximation discarded more than six sevenths of the
// radiated power. The phase must also follow a live tension shift, not be baked
// into the mode at construction time.
void testFiniteHeadSeparationRadiatesADipole()
{
    for (const float cross : { -2.0f, 2.0f })
        for (const float phase : { 0.0f, 1.0e-5f, 0.2f, 1.0f, 3.0f, 8.0f })
        {
            const float expectedSinc = phase == 0.0f
                ? 1.0f
                : std::sin (phase) / phase;
            const float expected = 2.0f + cross * expectedSinc;
            const float actual =
                taikor::TaikoEngineTestAccess::coherentRadiationPower (
                    2.0f, cross, phase);
            expect (std::abs (actual - expected) < 2.0e-6f,
                    "finite-separation radiation stopped following sinc(kd)");
            expect (actual >= 0.0f,
                    "a passive two-head radiation pair produced negative power");
        }

    constexpr float retune = 1.37f;
    const auto probe =
        taikor::TaikoEngineTestAccess::axisymmetricRadiationProbe (
            defaultParameters(), 3, retune);
    expect (probe.found && probe.shifted,
            "the Shime lower (0,1) branch was not available to probe");
    if (! probe.found || ! probe.shifted)
        return;

    const auto sinc = [] (float phase)
    {
        return phase == 0.0f ? 1.0f : std::sin (phase) / phase;
    };
    const float phase = probe.omega * probe.delaySeconds;
    const float finitePower = probe.selfPrefactor
                            + probe.crossPrefactor * sinc (phase);
    const float coincidentPower = probe.selfPrefactor + probe.crossPrefactor;
    const float expectedRadiation = finitePower * probe.efficiency;
    const float builtRadiation = probe.builtDecay - probe.otherLoss;

    expect (probe.crossPrefactor < 0.0f && coincidentPower > 0.0f,
            "the Shime lower branch stopped being an opposing two-head pair");
    expect (finitePower > 6.0f * coincidentPower,
            "finite head separation no longer restores the Shime dipole power");
    expect (std::abs (builtRadiation - expectedRadiation)
                < 2.0e-5f * std::max (expectedRadiation, 1.0f),
            "the built axisymmetric mode did not use finite-separation power");
    expect (std::abs (probe.observedDecay - probe.builtDecay)
                < 2.0e-5f * std::max (probe.builtDecay, 1.0f),
            "pitch observation and the rendered mode disagree on radiation loss");

    const float shiftedPhase = probe.shiftedOmega * probe.delaySeconds;
    const float shiftedPower = probe.selfPrefactor
                             + probe.crossPrefactor * sinc (shiftedPhase);
    const float expectedShiftedRadiation =
        shiftedPower * probe.shiftedEfficiency;
    const float shiftedRadiation = probe.shiftedDecay - probe.shiftedOtherLoss;
    const float frozenPhaseRadiation = finitePower * probe.shiftedEfficiency;
    expect (std::abs (shiftedRadiation - expectedShiftedRadiation)
                < 2.0e-5f * std::max (expectedShiftedRadiation, 1.0f),
            "live tension retuning did not recompute the two-head phase");
    expect (std::abs (expectedShiftedRadiation - frozenPhaseRadiation)
                > 1.0e-3f,
            "the retune probe no longer distinguishes live phase from cached phase");
    expect (probe.nonAxisPathIsExact,
            "finite-separation damping changed the legacy non-axis arithmetic");
}

void testReleasedMultipolesKeepTheirNodalAzimuth()
{
    const auto probe =
        taikor::TaikoEngineTestAccess::releasedMultipoleNodeProbe();
    expect (probe.modes == 32,
            "the nodal-azimuth probe did not cover every resolved multipole");
    expect (probe.analyticModes == 16,
            "the nodal-azimuth probe did not cover every analytic multipole");
    expect (probe.maximumLeakage < 2.0e-5f,
            "the released observer gave a multipole an omnidirectional floor: "
                + std::to_string (probe.maximumLeakage));
    expect (probe.maximumAnalyticLeakage < 2.0e-5f,
            "the analytic observer gave a multipole an omnidirectional floor: "
                + std::to_string (probe.maximumAnalyticLeakage));
    expect (probe.maximumRotatedAnalyticLeakage < 2.0e-5f,
            "the analytic observer ignored the authored strike azimuth: "
                + std::to_string (probe.maximumRotatedAnalyticLeakage));
}

void testPhaseAwareResolvedObservationArchitecture()
{
    using Access = taikor::TaikoEngineTestAccess;
    constexpr double pi = 3.14159265358979;
    constexpr double airSpeed = 343.0;

    // The render consumes the imaginary residue with the exact quadrature of
    // the damped pole. A pure propagation delay has a closed answer and catches
    // the residue sign, post-tick state order and missing pole-radius factor.
    expect (Access::complexResidueRenderError() < 2.0e-6,
            "a complex modal residue did not render the requested phase delay");

    const auto infiniteFrequency = Access::baffledObservation (
        0.4f, 0.1f, 0.08f, 2, 5.135622f,
        std::numeric_limits<float>::infinity());
    const auto nanMicrophone = Access::baffledObservation (
        0.4f, std::numeric_limits<float>::quiet_NaN(), 0.08f,
        2, 5.135622f, 1000.0f, std::numeric_limits<int>::max());
    expect (infiniteFrequency[0] == 0.0f && infiniteFrequency[1] == 0.0f
                && nanMicrophone[0] == 0.0f && nanMicrophone[1] == 0.0f,
            "hostile geometry reached the Rayleigh quadrature");

    // Exercise the mode-builder integration explicitly even while release
    // activation remains gated on owned calibration and live transfer retuning.
    const auto legacyBank = Access::complexBankProbe (false);
    const auto complexBank = Access::complexBankProbe (true);
    expect (legacyBank.nonAxisymmetricModes == complexBank.nonAxisymmetricModes
                && complexBank.nonAxisymmetricModes > 0,
            "the phase-aware observer changed the resolved mode inventory");
    expect (legacyBank.maximumQuadrature == 0.0f,
            "the release observer gate leaked quadrature into the legacy bank");
    expect (Access::releasedBankMaximumQuadrature() == 0.0f,
            "the canonical physical bank enabled the calibration-gated observer");
    expect (complexBank.maximumReal > 0.0f
                && complexBank.maximumQuadrature > 0.0f,
            "the complex Rayleigh transfer did not reach the built mode bank");

    // On-axis piston radiation has an elementary Rayleigh integral. Divide the
    // observed transfer by omega^2 I: the remaining calibration must be the
    // same positive real number at every ka. This checks phase and polarity
    // without baking the deliberately arbitrary output calibration into the
    // test.
    std::complex<double> referenceCalibration;
    bool haveCalibration = false;
    for (const double ka : { 0.5, 8.0, 50.0, 138.0, 260.0 })
    {
        constexpr double radius = 0.31;
        constexpr double height = 0.12;
        const double omega = ka * airSpeed / radius;
        const auto measured = Access::baffledObservation (
            static_cast<float> (radius), 0.0f, static_cast<float> (height),
            0, 0.0f, static_cast<float> (omega));
        const std::complex<double> transfer (measured[0], measured[1]);
        const double far = std::hypot (radius, height);
        const std::complex<double> i (0.0, 1.0);
        const auto integral = 2.0 * pi
            * (std::exp (-i * omega * height / airSpeed)
               - std::exp (-i * omega * far / airSpeed))
            / (i * omega / airSpeed);
        const auto calibration = transfer / (omega * omega * integral);

        expect (calibration.real() > 0.0
                    && std::abs (calibration.imag())
                           < std::abs (calibration.real()) * 2.0e-4,
                "the baffled observer lost the outgoing piston phase or polarity at ka "
                    + std::to_string (ka));
        if (! haveCalibration)
        {
            referenceCalibration = calibration;
            haveCalibration = true;
        }
        else
        {
            const double relative = std::abs (
                calibration / referenceCalibration - std::complex<double> (1.0, 0.0));
            expect (relative < 5.0e-4,
                    "the adaptive Rayleigh quadrature drifted from the piston solution at ka "
                        + std::to_string (ka) + ": " + std::to_string (relative));
        }
    }

    // Every non-axisymmetric circular mode integrates to zero on the axis.
    // Compare with the same mode off axis so a small absolute transfer cannot
    // make the assertion vacuous.
    for (int order = 1; order <= 8; ++order)
    {
        const float lambda = 3.0f + 1.1f * static_cast<float> (order);
        constexpr float radius = 0.48f;
        constexpr float height = 0.08f;
        const float omega = 12.0f * static_cast<float> (airSpeed) / radius;
        const auto onAxis = Access::baffledObservation (
            radius, 0.0f, height, order, lambda, omega);
        const auto offAxis = Access::baffledObservation (
            radius, 0.55f * radius, height, order, lambda, omega);
        const double onMagnitude = std::hypot (onAxis[0], onAxis[1]);
        const double offMagnitude = std::hypot (offAxis[0], offAxis[1]);
        expect (offMagnitude > 0.0 && onMagnitude < offMagnitude * 2.0e-5,
                "a circumferential mode radiated on axis at order "
                    + std::to_string (order));
    }

    // Selected close/high-ka modes compare the adaptive production rule with
    // a 256-node reference. These include the former worst close projection
    // and the largest ka the supported controls reach.
    struct ConvergenceCase
    {
        int order;
        float lambda;
        float radiusFraction;
        float heightFraction;
        float ka;
    };
    const ConvergenceCase cases[] = {
        { 1, 3.83170597f, 0.474f, 0.20f, 8.0f },
        { 8, 12.2250919f, 0.474f, 1.06326f, 7.71863f },
        { 4, 11.0647095f, 0.780f, 0.008f, 17.70f },
        { 8, 12.2250919f, 0.474f, 0.040f, 260.0f },
        { 8, 12.2250919f, 0.780f, 0.008f, 260.0f },
    };
    for (const auto& entry : cases)
    {
        constexpr float radius = 0.475f;
        const float omega = entry.ka * static_cast<float> (airSpeed) / radius;
        const auto adaptive = Access::baffledObservation (
            radius, entry.radiusFraction * radius,
            entry.heightFraction * radius, entry.order, entry.lambda, omega);
        const auto reference = Access::baffledObservation (
            radius, entry.radiusFraction * radius,
            entry.heightFraction * radius, entry.order, entry.lambda, omega, 256);
        const std::complex<double> a (adaptive[0], adaptive[1]);
        const std::complex<double> b (reference[0], reference[1]);
        const double relative = std::abs (a - b) / std::max (std::abs (b), 1.0e-20);
        expect (relative < 0.006,
                "the adaptive Rayleigh quadrature missed its high-resolution transfer: "
                    + std::to_string (relative) + " at ka "
                    + std::to_string (entry.ka));
    }


    // Both quadrature regimes approximate the same integral. Force the linear
    // rule at the exact boundary and compare it with the mapped production
    // rule at the same frequency, isolating quadrature choice from propagation.
    {
        constexpr float radius = 0.475f;
        constexpr float micRadius = 0.37f;
        constexpr float height = 0.012f;
        constexpr float lambda = 11.0647095f;
        const float omega = 16.0f * static_cast<float> (airSpeed) / radius;
        const auto mapped = Access::baffledObservation (
            radius, micRadius, height, 4, lambda,
            omega);
        const auto linear = Access::baffledObservation (
            radius, micRadius, height, 4, lambda, omega, 32);
        const std::complex<double> a (mapped[0], mapped[1]);
        const std::complex<double> b (linear[0], linear[1]);
        const double relative = std::abs (a - b)
                              / std::max ({ std::abs (a), std::abs (b), 1.0e-20 });
        expect (relative < 0.01,
                "the two Rayleigh quadrature rules disagree at ka=16: "
                    + std::to_string (relative));
    }
}

// The editor's readout has to name modes the drum actually sounds. With Air
// Coupling at zero the two heads are independent, and one branch belongs
// entirely to the far head - a mode a stroke on the batter head never touches.
// Reporting the lower eigenvalue regardless named that silent mode the
// fundamental: at Resonant Head zero the readout said 88.5 Hz while the drum
// sounded 92.9 Hz, and it named the batter head's own mode the breathing mode,
// which on an open body does not exist at all.
//
// The test is presence, not dominance. The fundamental is the lowest mode the
// stroke drives, which is not always the loudest survivor of a two-second tail:
// under weak coupling the volume-changing branch is driven harder while the
// other one outlives it. Asserting dominance would be asserting something the
// model never claimed.
void testReportedModesAreActuallySounded()
{
    for (float cavity : { 0.0f, 0.02f, 0.05f, 0.10f, 0.20f, 0.35f, 0.85f, 1.0f })
        for (float resonant : { 0.0f, 0.25f, 0.5f, 1.0f })
        {
            auto parameters = defaultParameters();
            parameters.cavityCoupling = cavity;
            parameters.resonantTension = resonant;

            const auto measurements = taikor::TaikoEngine::measure (parameters, 0);
            // A second and a half of tail: long enough to resolve the pair,
            // which is four hertz apart at its closest, without spending the
            // whole suite's runtime on Goertzel sweeps.
            const auto rendered = resolvedStrike (
                parameters, taikor::Articulation::Don, 0, 0.9f, 48000.0, 81920);
            const auto mono = rendered.mono();
            // Past the attack, so this is the ringing head rather than the
            // broadband contact, and bounded at the far end so it is the note
            // being measured rather than what is left of it.
            //
            // The bound matters. The fundamental is the one mode of a head that
            // radiates properly, so it is also the one that empties fastest -
            // half a second against two or three for the poor radiators above
            // it. That ordering is the physics and it is what recordings of
            // real drums show, but it means an unbounded window stops measuring
            // the note and starts measuring the tail, where the fundamental has
            // long since gone and every partial that could not get out of the
            // head is still sounding. Half a second is ten times what the four
            // hertz between the closest pair needs to resolve.
            constexpr std::size_t afterAttack = 2400;
            constexpr std::size_t windowEnd = 26400;

            double bandPeak = 0.0;
            for (double frequency = 50.0; frequency < 320.0; frequency += 1.0)
                bandPeak = std::max (bandPeak,
                                     binMagnitude (mono, frequency, 48000.0, afterAttack,
                                                   windowEnd));

            const std::string where = " (cavity " + std::to_string (cavity)
                                    + ", resonant " + std::to_string (resonant) + ")";
            expect (bandPeak > 0.0, "the drum must sound at all" + where);
            if (! (bandPeak > 0.0))
                continue;

            const auto atFundamental =
                binMagnitude (mono, measurements.loadedFundamentalHz, 48000.0, afterAttack,
                              windowEnd)
                / bandPeak;
            const auto atBreathing =
                binMagnitude (mono, measurements.breathingModeHz, 48000.0, afterAttack,
                              windowEnd)
                / bandPeak;

            // Six per cent, not fifteen. This asks whether the reported mode is
            // driven at all, and an undriven one is not a little quieter - it
            // is J_m(0) = 0 exactly, and it renders at a hundred and thirty
            // decibels down, six orders of magnitude below this line. What sits
            // between the two is the honest range for a mode that is driven
            // hardest of all and then radiates itself away first, which is what
            // the fundamental of a drumhead does.
            expect (atFundamental > 0.06,
                    "the reported fundamental must be a mode the stroke drives"
                    + where);
            expect (atBreathing > 0.004,
                    "the reported breathing mode must be a mode the stroke drives"
                    + where);

            // On an open body only one branch sounds at all, so there the
            // strongest partial is unambiguously the fundamental and the check
            // can be exact. This is the reviewer's case: the readout used to say
            // 88.5 Hz where the drum plainly sounded 92.9 Hz. Under partial
            // coupling both branches are genuinely driven and the louder one is
            // not always the lower, so dominance is only the right question
            // once the cavity is out of the picture.
            if (cavity <= 0.0f)
            {
                // Struck dead centre for this one check. A full Don lands a
                // hand's width in from the middle, which is what a player does
                // and what wakes the modes with a circumferential order; but
                // those are not axisymmetric, they outlive the fundamental
                // because they cannot radiate, and asking which partial is
                // loudest while they are sounding is not a question about the
                // readout. At radius zero every one of them has J_m(0) = 0 and
                // the axisymmetric family is all there is, so the drum has
                // exactly one partial it can be sounding and the check is
                // exact - which is the case the readout got wrong.
                auto centred = parameters;
                centred.strikePosition = -1.0f;
                const auto atCentre = resolvedStrike (
                    centred, taikor::Articulation::Don, 0, 0.9f, 48000.0, 81920);
                // From 20 Hz rather than 50: an uncoupled five-shaku o-daiko's
                // one axisymmetric mode is at 33 Hz, and a band that starts
                // above it cannot find it.
                const auto dominant = dominantFrequency (atCentre.mono(), 48000.0,
                                                         20.0, 320.0, 0.25,
                                                         afterAttack, windowEnd);
                expect (std::abs (dominant - measurements.loadedFundamentalHz) < 1.0,
                        "with no cavity the reported fundamental must be the "
                        "partial the drum sounds" + where);
            }

            expect (measurements.breathingModeHz >= measurements.loadedFundamentalHz,
                    "the breathing mode must never be reported below the fundamental"
                    + where);
            expect (measurements.tailSeconds > 0.0f,
                    "the reported tail must describe a mode that sounds" + where);

            // An uncoupled body has no split to report, so both figures must be
            // the single mode it has. A sealed one must still show the split.
            if (cavity <= 0.0f)
                expect (std::abs (measurements.breathingModeHz
                                  - measurements.loadedFundamentalHz) < 0.01f,
                        "an uncoupled body must not report a breathing mode" + where);
            else if (cavity >= 0.85f)
                expect (measurements.breathingModeHz
                            > measurements.loadedFundamentalHz * 1.2f,
                        "a sealed body must still report the cavity split" + where);
        }
}

// Sixteen voices, and several strokes can land on the same sample. A voice that
// has been triggered but not yet rendered still reads as silent, so choosing
// the quietest voice by level alone sent every stroke in a block to one slot -
// and a chord, a flam or a roll on a buffer boundary sounded as a single note.
void testSimultaneousStrokesDoNotShareOneVoice()
{
    taikor::TaikoEngine engine;
    engine.setParameters (defaultParameters());
    engine.prepare (48000.0, defaultBlockSize);

    // Fill the transient-contact pool and advance one sample so every slot has
    // a real level while remaining alive. Modal ringing is no longer stored in
    // these slots, so waiting 100 ms would correctly retire all of them even
    // though the one physical drum continues to ring.
    for (int index = 0; index < 16; ++index)
        engine.trigger (taikor::Articulation::Don, 0, 0.9f);
    render (engine, 1);
    expect (taikor::TaikoEngineTestAccess::activeVoices (engine) == 16,
            "the pool must be full before the stealing case is exercised");

    // Now six more strokes at the same sample, with nothing rendered between
    // them. Each must displace a different voice.
    std::set<int> slots;
    for (int index = 0; index < 6; ++index)
    {
        engine.trigger (taikor::Articulation::Ka, 0, 0.9f);
        const auto order = taikor::TaikoEngineTestAccess::strokeCount (engine);
        const int slot = taikor::TaikoEngineTestAccess::voiceSlotOf (engine, order);
        expect (slot >= 0, "a triggered stroke must be findable in the pool");
        if (slot >= 0)
            slots.insert (slot);
    }

    expect (slots.size() == 6,
            "six simultaneous strokes must occupy six voices, not overwrite one");

    const auto rendered = render (engine, 24000);
    expect (rendered.finite, "simultaneous stealing produced non-finite audio");
}

// MIDI events are contacts, not copies of the instrument. These probes make
// the ownership change observable: three articulations on one octave retain
// one stable bank, their forces enter one recurrence, and allocating another
// event cannot rewrite an earlier transient.
void testStrokesShareOnePhysicalDrumState()
{
    const auto topology = taikor::TaikoEngineTestAccess::sharedTopology();
    expect (topology.stableAddress,
            "successive strokes must retain one canonical drum-bank address");
    expect (topology.octave0Modes == 46 && topology.octave1Modes == 46,
            "each playable drum must own one complete canonical modal bank");
    expect (topology.uniqueModeIds,
            "canonical modes must retain unique stable physical identities");
    expect (topology.contactSlots == 3,
            "three strokes must remain three independently scheduled contacts");
    expect (topology.contactModeStates == 0,
            "a contact slot must never retain a second resonator bank");

    expect (taikor::TaikoEngineTestAccess::sharedRecurrenceError() < 1.0e-7,
            "same-sample contacts must be summed before one canonical recurrence");
    expect (taikor::TaikoEngineTestAccess::secondTriggerPreservesFirstTransient(),
            "allocating a second strike must not mutate the first transient");
    expect (taikor::TaikoEngineTestAccess::retriggerFadeStateError() < 1.0e-7,
            "retriggering a fading drum exposed hidden unfaded physical state");

    // The editor reports sounding instruments, not the short-lived MIDI event
    // slots that excited them. A contact is gone after roughly 80 ms while its
    // one physical head continues to ring for seconds.
    {
        taikor::TaikoEngine engine;
        engine.setParameters (defaultParameters());
        engine.prepare (48000.0, 64);
        engine.trigger (taikor::Articulation::Don, 0, 0.8f);
        render (engine, 9600, 64);
        expect (taikor::TaikoEngineTestAccess::activeVoices (engine) == 0,
                "the transient slot must retire after its contact path ends");
        expect (engine.getActiveVoiceCount() == 1,
                "the UI must keep reporting the physical drum while its tail rings");
        engine.allSoundsOff();
        expect (engine.getActiveVoiceCount() == 0,
                "panic must clear the physical-drum activity readout");
    }
}

// The bachi/head exchange is an implicit, collocated contact rather than a
// prescribed force pulse. These are the invariants that make that distinction
// physical: no adhesive force, no numerical energy source, exact discrete
// impulse exchange, a clean release, and nearly the same collision when the
// host clock changes. Adjacent-velocity changes remain diagnostics until the
// owned force/mobility/pressure captures say what the acoustic curve should be.
void testPassiveNonlinearContact()
{
    const auto disabled =
        taikor::TaikoEngineTestAccess::disabledModeContactAudit();
    expect (disabled.disabledModes > 0,
            "the live-retune contact probe did not cross a Nyquist boundary");
    expect (disabled.finite,
            "a live-retuned near-Nyquist contact became non-finite");
    expect (disabled.forceDifference < 1.0e-9,
            "a disabled above-Nyquist mode contributed phantom contact compliance");

    const auto expectPassive = [] (
        const taikor::TaikoEngineTestAccess::NonlinearContactAudit& audit,
        const std::string& where)
    {
        expect (audit.finite, "the nonlinear contact became non-finite" + where);
        expect (audit.minimumForce >= -1.0e-6,
                "the bachi pulled adhesively on the head" + where);
        expect (audit.minimumExposureStep >= -1.0e-12,
                "the contact produced negative residual exposure" + where);
        expect (audit.normalizedResidualExposure >= 0.0,
                "the normalized residual exposure became negative" + where);
        expect (audit.released,
                "the bachi did not release within 30 ms" + where);
        expect (audit.maximumEnergyRatio <= 1.0 + 1.0e-8,
                "the nonlinear contact manufactured energy" + where);
        expect (audit.maximumEnergyIncrease <= 1.0e-8,
                "the nonlinear contact energy rose during an unforced step" + where);
        expect (audit.finalEnergyRatio > 0.0 && audit.finalEnergyRatio <= 1.001,
                "the post-contact stick/head energy is not passive" + where);
        expect (audit.impulseMomentumClosure < 1.0e-6,
                "contact impulse did not close against stick momentum" + where);
        expect (audit.durationSeconds > 0.0 && audit.durationSeconds < 0.03,
                "the bachi/head contact duration escaped the audit window" + where);
    };

    // Keep the deliberately hostile clock checks on the original full Don.
    for (const double rate : std::array<double, 3> {{
             8000.0, 192000.0, 384000.0,
         }})
    {
        const auto audit =
            taikor::TaikoEngineTestAccess::nonlinearContactAudit (rate);
        const auto where = " for full Don at " + std::to_string (rate) + " Hz";
        expectPassive (audit, where);
        expect (audit.durationSeconds > 0.0004 && audit.durationSeconds < 0.001,
                "the full-Don contact duration is implausible" + where);
    }

    constexpr std::array<double, 3> rates {{ 44100.0, 48000.0, 96000.0 }};
    constexpr std::array<float, 9> velocities {{
        0.20f, 0.35f, 0.40f, 0.45f, 0.50f,
        0.60f, 0.65f, 0.70f, 1.00f,
    }};
    using Audit = taikor::TaikoEngineTestAccess::NonlinearContactAudit;
    std::array<std::array<std::array<Audit, rates.size()>, velocities.size()>,
               taikor::articulationCount> audits {};

    double worstDurationRate = 0.0;
    double worstImpulseRate = 0.0;
    double worstPeakRate = 0.0;
    double worstExposureRate = 0.0;
    std::array<std::string, 4> worstRateWhere {};

    for (std::size_t articulationIndex = 0;
         articulationIndex < taikor::articulationCount; ++articulationIndex)
        for (std::size_t velocityIndex = 0;
             velocityIndex < velocities.size(); ++velocityIndex)
        {
            const auto articulation =
                static_cast<taikor::Articulation> (articulationIndex);
            const auto condition = " for "
                + std::string (taikor::getArticulationDisplayName (articulation))
                + " at velocity " + std::to_string (velocities[velocityIndex]);

            double minimumDuration = std::numeric_limits<double>::infinity();
            double maximumDuration = 0.0;
            double minimumImpulse = std::numeric_limits<double>::infinity();
            double maximumImpulse = 0.0;
            double minimumPeak = std::numeric_limits<double>::infinity();
            double maximumPeak = 0.0;
            double minimumExposure = std::numeric_limits<double>::infinity();
            double maximumExposure = 0.0;

            for (std::size_t rateIndex = 0; rateIndex < rates.size(); ++rateIndex)
            {
                auto& audit = audits[articulationIndex][velocityIndex][rateIndex];
                audit = taikor::TaikoEngineTestAccess::nonlinearContactAudit (
                    rates[rateIndex], articulation, velocities[velocityIndex]);
                expectPassive (
                    audit, condition + " at " + std::to_string (rates[rateIndex])
                               + " Hz");

                minimumDuration = std::min (minimumDuration, audit.durationSeconds);
                maximumDuration = std::max (maximumDuration, audit.durationSeconds);
                minimumImpulse = std::min (minimumImpulse, audit.impulse);
                maximumImpulse = std::max (maximumImpulse, audit.impulse);
                minimumPeak = std::min (minimumPeak, audit.peakForce);
                maximumPeak = std::max (maximumPeak, audit.peakForce);
                minimumExposure = std::min (
                    minimumExposure, audit.normalizedResidualExposure);
                maximumExposure = std::max (
                    maximumExposure, audit.normalizedResidualExposure);
            }

            expect (minimumDuration > 0.0 && minimumImpulse > 0.0
                        && minimumPeak > 0.0 && minimumExposure > 0.0,
                    "the contact matrix lost a positive observable" + condition);
            const double durationRate = maximumDuration / minimumDuration - 1.0;
            const double impulseRate = maximumImpulse / minimumImpulse - 1.0;
            const double peakRate = maximumPeak / minimumPeak - 1.0;
            const double exposureRate = maximumExposure / minimumExposure - 1.0;
            expect (durationRate < 0.06,
                    "contact duration moved by " + std::to_string (durationRate)
                        + condition);
            expect (impulseRate < 0.01,
                    "contact impulse moved by " + std::to_string (impulseRate)
                        + condition);
            expect (peakRate < 0.04,
                    "contact peak moved by " + std::to_string (peakRate)
                        + condition);
            expect (exposureRate < 0.01,
                    "normalized residual exposure moved by "
                        + std::to_string (exposureRate) + condition);

            const std::array<double, 4> rateMetrics {{
                durationRate, impulseRate, peakRate, exposureRate,
            }};
            std::array<double*, 4> worstMetrics {{
                &worstDurationRate, &worstImpulseRate,
                &worstPeakRate, &worstExposureRate,
            }};
            for (std::size_t metric = 0; metric < rateMetrics.size(); ++metric)
                if (rateMetrics[metric] > *worstMetrics[metric])
                {
                    *worstMetrics[metric] = rateMetrics[metric];
                    worstRateWhere[metric] = condition;
                }
        }

    // Diagnostic only: retain both the mechanical adjacency changes and the
    // worst 0-30 ms acoustic step so captures can arbitrate the low-speed cliff.
    // None of these readings is a monotonicity gate.
    std::array<double, 4> worstAdjacentMagnitude {{}};
    std::array<double, 4> worstAdjacentChange {{}};
    std::array<std::string, 4> worstAdjacentWhere {};
    double worstAdjacentDb = std::numeric_limits<double>::infinity();
    std::string worstAdjacentDbWhere;
    for (std::size_t articulationIndex = 0;
         articulationIndex < taikor::articulationCount; ++articulationIndex)
        for (std::size_t rateIndex = 0; rateIndex < rates.size(); ++rateIndex)
            for (std::size_t velocityIndex = 1;
                 velocityIndex < velocities.size(); ++velocityIndex)
            {
                const auto& previous =
                    audits[articulationIndex][velocityIndex - 1][rateIndex];
                const auto& current =
                    audits[articulationIndex][velocityIndex][rateIndex];
                const std::array<double, 4> changes {{
                    current.durationSeconds / previous.durationSeconds - 1.0,
                    current.impulse / previous.impulse - 1.0,
                    current.peakForce / previous.peakForce - 1.0,
                    current.normalizedResidualExposure
                        / previous.normalizedResidualExposure - 1.0,
                }};
                for (std::size_t metric = 0; metric < changes.size(); ++metric)
                    if (std::abs (changes[metric]) > worstAdjacentMagnitude[metric])
                    {
                        worstAdjacentMagnitude[metric] = std::abs (changes[metric]);
                        worstAdjacentChange[metric] = changes[metric];
                        worstAdjacentWhere[metric] = " for " + std::string (
                            taikor::getArticulationDisplayName (
                                static_cast<taikor::Articulation> (
                                    articulationIndex)))
                            + " " + std::to_string (velocities[velocityIndex - 1])
                            + "->" + std::to_string (velocities[velocityIndex])
                            + " at " + std::to_string (rates[rateIndex]) + " Hz";
                    }
                const double adjacentDb = 10.0 * std::log10 (
                    std::max (current.stereoEnergy, 1.0e-300)
                    / std::max (previous.stereoEnergy, 1.0e-300));
                if (adjacentDb < worstAdjacentDb)
                {
                    worstAdjacentDb = adjacentDb;
                    worstAdjacentDbWhere = " for " + std::string (
                        taikor::getArticulationDisplayName (
                            static_cast<taikor::Articulation> (
                                articulationIndex)))
                        + " " + std::to_string (velocities[velocityIndex - 1])
                        + "->" + std::to_string (velocities[velocityIndex])
                        + " at " + std::to_string (rates[rateIndex]) + " Hz";
                }
            }

    std::cout << "Contact matrix worst rate spreads (duration/impulse/peak/exposure): "
              << 100.0 * worstDurationRate << "%" << worstRateWhere[0] << ", "
              << 100.0 * worstImpulseRate << "%" << worstRateWhere[1] << ", "
              << 100.0 * worstPeakRate << "%" << worstRateWhere[2] << ", "
              << 100.0 * worstExposureRate << "%" << worstRateWhere[3] << "\n"
              << "Contact matrix worst signed adjacent changes"
                 " (duration/impulse/peak/exposure): "
              << 100.0 * worstAdjacentChange[0] << "%" << worstAdjacentWhere[0]
              << ", " << 100.0 * worstAdjacentChange[1] << "%"
              << worstAdjacentWhere[1]
              << ", " << 100.0 * worstAdjacentChange[2] << "%"
              << worstAdjacentWhere[2]
              << ", " << 100.0 * worstAdjacentChange[3] << "%"
              << worstAdjacentWhere[3] << "\n"
              << "Contact matrix worst adjacent 0-30 ms stereo-energy step: "
              << worstAdjacentDb << " dB" << worstAdjacentDbWhere << "\n";
}

void testHertzReferencePulsePreservesCollisionImpulse()
{
    const double analyticIntegral = std::sqrt (analysisPi)
                                  * std::tgamma (1.25) / std::tgamma (1.75);
    expect (std::abs (analyticIntegral - 1.7480383695280799) < 1.0e-13,
            "the analytic sin^1.5 Hertz impulse integral changed");
    const double impulseRatio =
        taikor::TaikoEngineTestAccess::hertzReferenceImpulseRatio (
            analyticIntegral);
    expect (std::abs (impulseRatio - 1.0) < 2.0e-6,
            "the Hertz reference peak no longer preserves collision impulse: "
                + std::to_string (impulseRatio));
}

// Preserve the old contact-solver exposure diagnostic independently of output.
// The causal continuum no longer uses this positive F^2 proxy: cutting off its
// later force would prevent physical phase cancellation. This diagnostic is
// neither a Joule/passivity bound nor an output-amplitude guarantee.
void testContinuumBoundsLegacyResidualExposurePerContact()
{
    const std::array<double, 6> rates {{
        8000.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0,
    }};

    for (const double rate : rates)
        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
            for (std::size_t index = 0; index < taikor::articulationCount; ++index)
            {
                const auto articulation = static_cast<taikor::Articulation> (index);
                const auto audit =
                    taikor::TaikoEngineTestAccess::legacyResidualExposureAudit (
                        rate, octave, articulation);
                const std::string where =
                    " for "
                    + std::string (taikor::getDrumDescription (octave).displayName)
                    + " "
                    + std::string (taikor::getArticulationDisplayName (articulation))
                    + " at " + std::to_string (rate) + " Hz";

                expect (audit.finite,
                        "the legacy residual exposure became non-finite"
                            + where);
                expect (audit.released,
                        "the residual-exposure probe never released" + where);
                expect (audit.deliveredFraction >= 0.0
                            && audit.deliveredFraction <= 1.0 + 2.0e-12,
                        "one contact exceeded its calibrated legacy residual exposure"
                            + where + ": "
                            + std::to_string (audit.deliveredFraction));
                expect (audit.closureError < 2.0e-12,
                        "delivered and remaining legacy residual exposure do not close"
                            + where + ": " + std::to_string (audit.closureError));
            }

    const auto ordinary =
        taikor::TaikoEngineTestAccess::legacyResidualExposureAudit (
            48000.0, 1, taikor::Articulation::Don, 0.9f);
    // The contact now has small speed/stiffness variation. What matters is
    // remaining strictly below the actual budget, then delivering all of the
    // requested exposure; an arbitrary 2% margin was not a physical bound.
    expect (ordinary.requestedFraction > 0.90
                && ordinary.requestedFraction < 1.0,
            "the ordinary Nagado-daiko Don no longer exercises the below-limit path: "
                + std::to_string (ordinary.requestedFraction));
    expect (std::abs (ordinary.deliveredFraction - ordinary.requestedFraction)
                < 5.0e-7,
            "a below-limit contact did not preserve its requested residual exposure");
    expect (ordinary.injected,
            "the below-limit contact did not inject the continuum");

    // This was the hostile factory case: without a cumulative limit the
    // Shime Don requested about 1040 of the former reference integrals from
    // one contact; the corrected direct Hunt-Crossley limit still sees 446.
    const auto hostile =
        taikor::TaikoEngineTestAccess::legacyResidualExposureAudit (
            48000.0, 3, taikor::Articulation::Don);
    expect (hostile.requestedFraction > 100.0,
            "the saturation probe no longer exercises the former runaway contact");
    expect (hostile.deliveredFraction >= 1.0 - 2.0e-12,
            "the hostile contact did not exhaust its diagnostic residual integral");
    expect (hostile.limitToReferenceRatio > 1.2,
            "the hostile contact fell back to the reference-pulse limit");
    expect (hostile.injected,
            "the hostile contact saturated without injecting the continuum");

    const auto lifecycle =
        taikor::TaikoEngineTestAccess::legacyResidualExposureLifecycleAudit();
    expect (lifecycle.saturatedWhileActive,
            "the hostile contact did not saturate before it released");
    expect (lifecycle.distinctOverlappingSlots,
            "overlapping contacts did not own distinct transient slots");
    expect (lifecycle.secondStartedFull,
            "an overlapping contact inherited the first contact's spent budget");
    expect (lifecycle.firstStayedSpent,
            "a second contact replenished the first contact's budget");
    expect (lifecycle.secondSpentIndependently,
            "the overlapping contact could not spend its own budget");
    expect (lifecycle.firstSlotRetired,
            "the first transient slot did not retire before reuse");
    expect (lifecycle.reusedFirstSlot && lifecycle.reuseResetBudget,
            "reusing a transient slot did not reset its contact budget");
}

void testDeterminismAndBlockPartitioning()
{
    const auto parameters = defaultParameters();

    const auto first = strike (parameters, taikor::Articulation::Don, 0, 0.83f, 48000.0,
                               24000, 64);
    const auto second = strike (parameters, taikor::Articulation::Don, 0, 0.83f, 48000.0,
                                24000, 64);
    expect (maximumAbsoluteDifference (first.left, second.left) == 0.0,
            "the engine must be bit-exactly deterministic");
    expect (maximumAbsoluteDifference (first.right, second.right) == 0.0,
            "the engine must be bit-exactly deterministic on both channels");

    // The same stroke must render identically however the host cuts the block.
    for (const int blockSize : { 1, 7, 64, 129, 512, 2048 })
    {
        const auto partitioned = strike (parameters, taikor::Articulation::Don, 0, 0.83f,
                                         48000.0, 24000, blockSize);
        expect (maximumAbsoluteDifference (first.left, partitioned.left) < 1.0e-6,
                "block partitioning changed the rendered audio at block size "
                    + std::to_string (blockSize));
    }

    // Variation is indexed by the performed stroke, not by render blocks or
    // wall-clock entropy. Include overlapping hits and a panic between them.
    const auto sequence = [] (taikor::TaikoEngine& engine, int blockSize)
    {
        Rendered take;
        for (int hit = 0; hit < 8; ++hit)
        {
            if (hit == 4)
                engine.allSoundsOff();
            engine.trigger (static_cast<taikor::Articulation> (hit % 4),
                            (hit / 2) % taikor::drumCount, 0.83f);
            const auto part = render (engine, 777 + 31 * hit, blockSize);
            take.left.insert (take.left.end(), part.left.begin(), part.left.end());
            take.right.insert (take.right.end(), part.right.begin(), part.right.end());
        }
        return take;
    };
    for (const float humanise : { 0.0f, parameters.humanise })
    {
        auto settings = parameters;
        settings.humanise = humanise;
        taikor::TaikoEngine engine;
        engine.setParameters (settings);
        engine.prepare (48000.0, defaultBlockSize);
        const auto reference = sequence (engine, 64);
        for (const int blockSize : { 1, 7, 64, 129, 512 })
        {
            engine.reset();
            const auto replay = sequence (engine, blockSize);
            expect (maximumAbsoluteDifference (reference.left, replay.left) == 0.0
                        && maximumAbsoluteDifference (reference.right, replay.right) == 0.0,
                    "stroke variation changed on replay or block partition at "
                        + std::to_string (blockSize));
        }
    }
}

void testSuccessiveStrokesVaryPhysically()
{
    // Remove every stochastic and airborne output: each hit must change the
    // resolved drum itself, even with Stick Noise and Humanise both at zero.
    auto parameters = defaultParameters();
    parameters.strikeNoise = 0.0f;
    parameters.drive = 0.0f;
    parameters.outputGain = 0.02f;
    constexpr int samples = 4096;
    constexpr std::size_t hitCount = 8;
    for (const float humanise : { 0.0f, parameters.humanise })
        for (int octave = taikor::lowestOctaveOffset;
             octave <= taikor::highestOctaveOffset; ++octave)
            for (std::size_t articulation = 0;
                 articulation < taikor::articulationCount; ++articulation)
            {
                parameters.humanise = humanise;
                taikor::TaikoEngine engine;
                engine.setParameters (parameters);
                engine.prepare (48000.0, defaultBlockSize);
                std::array<Rendered, hitCount> hits;
                for (auto& hit : hits)
                {
                    engine.trigger (static_cast<taikor::Articulation> (articulation),
                                    octave, 0.8f);
                    taikor::TaikoEngineTestAccess::isolateResolvedBank (
                        engine, taikor::TaikoEngineTestAccess::newestStrikeSlot (engine));
                    hit = render (engine, samples);
                    expect (hit.finite && hit.rms > 1.0e-8,
                            "the physical stroke-variation probe was silent or non-finite");
                    engine.allSoundsOff();
                }

                double largestShapeDifference = 0.0;
                for (std::size_t first = 0; first < hitCount; ++first)
                    for (std::size_t second = first + 1; second < hitCount; ++second)
                    {
                        // Fit one overall gain, then measure what remains.
                        // A random gain or independent hiss cannot pass this.
                        const auto& a = hits[first];
                        const auto& b = hits[second];
                        double aa = 0.0, ab = 0.0, bb = 0.0;
                        for (std::size_t index = 0; index < a.left.size(); ++index)
                        {
                            aa += static_cast<double> (a.left[index]) * a.left[index]
                                + static_cast<double> (a.right[index]) * a.right[index];
                            ab += static_cast<double> (a.left[index]) * b.left[index]
                                + static_cast<double> (a.right[index]) * b.right[index];
                            bb += static_cast<double> (b.left[index]) * b.left[index]
                                + static_cast<double> (b.right[index]) * b.right[index];
                        }
                        const double gain = ab / std::max (bb, 1.0e-30);
                        double residual = 0.0;
                        for (std::size_t index = 0; index < a.left.size(); ++index)
                        {
                            const double left = a.left[index] - gain * b.left[index];
                            const double right = a.right[index] - gain * b.right[index];
                            residual += left * left + right * right;
                        }
                        const double difference = std::sqrt (
                            residual / std::max (aa, 1.0e-30));
                        largestShapeDifference = std::max (largestShapeDifference, difference);
                        expect (difference > 1.0e-7,
                                "two physical strokes repeated or changed only gain: drum "
                                    + std::to_string (octave) + ", articulation "
                                    + std::to_string (articulation) + ", Humanise "
                                    + std::to_string (humanise) + ", pair "
                                    + std::to_string (first) + "/" + std::to_string (second));
                    }
                expect (largestShapeDifference > 1.0e-4,
                        "natural physical variation shrank to floating-point noise");
            }

    // The high half of the stroke counter must reach the performed contact;
    // otherwise the entire sequence repeats at the 32-bit boundary.
    parameters.humanise = 0.0f;
    taikor::TaikoEngine engine;
    engine.setParameters (parameters);
    engine.prepare (48000.0, defaultBlockSize);
    Rendered first;
    for (const auto sequence : { std::uint64_t { 0 }, std::uint64_t { 1 } << 32,
                                std::uint64_t { 1 } << 48 })
    {
        engine.allSoundsOff();
        taikor::TaikoEngineTestAccess::setNoteSequence (engine, sequence);
        engine.trigger (taikor::Articulation::Don, 0, 0.8f);
        taikor::TaikoEngineTestAccess::isolateResolvedBank (engine);
        const auto hit = render (engine, samples);
        if (sequence == 0)
            first = hit;
        else
            expect (std::max (maximumAbsoluteDifference (first.left, hit.left),
                             maximumAbsoluteDifference (first.right, hit.right)) > 1.0e-7,
                    "the performed contact discarded the stroke counter's high bits");
    }

    // These sequence pairs produce equal float-valued impact speeds. Contact
    // compliance must still distinguish them, including when the attack glide
    // is disabled; changing only the reference pulse or gain is insufficient.
    parameters.tensionModulation = 0.0f;
    engine.setParameters (parameters);
    constexpr std::array<std::array<std::uint64_t, 2>, 3> equalSpeedSequences {{
        {{ 387, 778 }}, {{ 155, 799 }}, {{ 355, 802 }}
    }};
    for (const auto& pair : equalSpeedSequences)
    {
        std::array<Rendered, 2> hits;
        for (std::size_t index = 0; index < hits.size(); ++index)
        {
            engine.allSoundsOff();
            taikor::TaikoEngineTestAccess::setNoteSequence (engine, pair[index]);
            engine.trigger (taikor::Articulation::Don, 0, 0.8f);
            taikor::TaikoEngineTestAccess::isolateResolvedBank (engine);
            hits[index] = render (engine, samples);
        }
        expect (std::abs (correlation (hits[0].left, hits[1].left)) < 1.0 - 1.0e-12
                    || std::abs (correlation (hits[0].right, hits[1].right)) < 1.0 - 1.0e-12,
                "equal-speed strokes lost their physical contact differences: "
                    + std::to_string (pair[0] + 1) + "/"
                    + std::to_string (pair[1] + 1));
    }

    // Let a damped shime retire naturally. Neither the bank retiring nor the
    // output becoming idle may restart the performance sequence.
    parameters.headDamping = 1.0f;
    engine.setParameters (parameters);
    engine.reset();
    engine.trigger (taikor::Articulation::Don, 3, 0.8f);
    taikor::TaikoEngineTestAccess::isolateResolvedBank (engine);
    first = render (engine, samples);
    std::array<float, 512> left {}, right {};
    for (int elapsed = 0;
         elapsed < 48000 * (taikor::maximumTailSeconds + 1.0)
             && engine.getActiveVoiceCount() > 0;
         elapsed += 512)
        engine.process (left.data(), right.data(), 512);
    expect (engine.getActiveVoiceCount() == 0,
            "the natural stroke-variation probe never retired");
    render (engine, 24000);
    expect (render (engine, 512).peak == 0.0,
            "the natural stroke-variation probe never reached exact idle");
    engine.trigger (taikor::Articulation::Don, 3, 0.8f);
    taikor::TaikoEngineTestAccess::isolateResolvedBank (engine);
    const auto next = render (engine, samples);
    expect (std::max (maximumAbsoluteDifference (first.left, next.left),
                     maximumAbsoluteDifference (first.right, next.right)) > 1.0e-7,
            "natural silence restarted the performed stroke sequence");
}

void testPerformanceControls()
{
    const auto parameters = defaultParameters();

    // A hand on the head must shorten what is already ringing.
    // Both takes are rendered over the same window from the same instant, or
    // the comparison is between an open take that includes its attack and a
    // damped one that does not - which would pass whether the damping worked
    // or not.
    taikor::TaikoEngine open;
    open.setParameters (parameters);
    open.prepare (48000.0, defaultBlockSize);
    open.trigger (taikor::Articulation::Don, 0, 0.95f);
    const auto openTail = render (open, 48000 * 3).mono();

    taikor::TaikoEngine damped;
    damped.setParameters (parameters);
    damped.prepare (48000.0, defaultBlockSize);
    damped.trigger (taikor::Articulation::Don, 0, 0.95f);
    render (damped, 4800);
    damped.setHandDamping (1.0f);
    const auto dampedTail = render (damped, 48000 * 3 - 4800).mono();

    // Compared over the same stretch of the tail, well after the hand has
    // landed and long before either take has run out.
    expect (windowedRms (dampedTail, 24000u, 96000u)
                < windowedRms (openTail, 28800u, 100800u) * 0.2,
            "a hand on the head must damp what is still ringing");

    // The wheel must raise the pitch, because pressing a head tightens it. It
    // glides rather than jumping, so the engine has to be run for the strings
    // of the smoother to arrive.
    taikor::TaikoEngine bent;
    bent.setParameters (parameters);
    bent.prepare (48000.0, defaultBlockSize);
    bent.setPitchBend (1.0f);
    render (bent, 24000);
    const auto bentMeasurements = bent.measureDrum (0);

    taikor::TaikoEngine plain;
    plain.setParameters (parameters);
    plain.prepare (48000.0, defaultBlockSize);
    const auto plainMeasurements = plain.measureDrum (0);

    expect (bentMeasurements.idealFundamentalHz > plainMeasurements.idealFundamentalHz,
            "the wheel must raise the drum's pitch");
    expect (bentMeasurements.idealFundamentalHz
                < plainMeasurements.idealFundamentalHz * 1.3f,
            "the wheel's range must stay within a couple of semitones");

    // Drive at zero must be exactly bypassed.
    auto clean = parameters;
    clean.drive = 0.0f;
    auto driven = parameters;
    driven.drive = 1.0f;

    const auto cleanRendered = strike (clean, taikor::Articulation::Don, 0, 0.95f,
                                       48000.0, 12000);
    const auto drivenRendered = strike (driven, taikor::Articulation::Don, 0, 0.95f,
                                        48000.0, 12000);
    expect (maximumAbsoluteDifference (cleanRendered.left, drivenRendered.left) > 1.0e-4,
            "the drive control must change the output");
    expect (drivenRendered.finite && drivenRendered.peak <= 1.0001,
            "the drive stage must stay bounded");

    // Output gain must scale the result predictably.
    // Half the default rather than an absolute figure, so the check stays a
    // statement about the control and not about what the default happens to be.
    auto quiet = parameters;
    quiet.outputGain = parameters.outputGain * 0.5f;
    const auto quietRendered = strike (quiet, taikor::Articulation::Don, 0, 0.95f,
                                       48000.0, 12000);
    const auto ratio = quietRendered.peak / std::max (cleanRendered.peak, 1.0e-9);
    expect (ratio > 0.4 && ratio < 0.6, "output gain must scale the result linearly");
}

// testInvalidInputSafety below sends every field of EngineParameters out of
// range at once and checks only that the resulting audio stays finite and
// bounded - a true end-to-end guarantee, but one that cannot tell a field
// clamped to the wrong bound from one clamped to the right one, since a
// mis-clamped control can easily still render finite, in-range audio. This
// asserts what TaikoEngine::sanitise() itself returns for each field, one at
// a time, against the exact bounds documented on EngineParameters.
void testSanitiseClampsEveryField()
{
    using taikor::TaikoEngineTestAccess;
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto infinity = std::numeric_limits<float>::infinity();

    // Every field with a symmetric [low, high] clamp, checked from both a
    // huge excursion and NaN. clampFloat folds NaN to `low`, so that is the
    // value a NaN control must sanitise to as well.
    struct Field
    {
        std::string_view name;
        float taikor::EngineParameters::* member;
        float low;
        float high;
    };
    const Field fields[] = {
        { "headDiameter", &taikor::EngineParameters::headDiameter, 0.15f, 1.80f },
        { "bodyDepth", &taikor::EngineParameters::bodyDepth, 0.0f, 1.0f },
        { "tension", &taikor::EngineParameters::tension, 0.0f, 1.0f },
        { "headMaterial", &taikor::EngineParameters::headMaterial, 0.0f, 1.0f },
        { "shellMaterial", &taikor::EngineParameters::shellMaterial, 0.0f, 1.0f },
        { "resonantTension", &taikor::EngineParameters::resonantTension, 0.0f, 1.0f },
        { "cavityCoupling", &taikor::EngineParameters::cavityCoupling, 0.0f, 1.0f },
        { "headDamping", &taikor::EngineParameters::headDamping, 0.0f, 1.0f },
        { "shellResonance", &taikor::EngineParameters::shellResonance, 0.0f, 1.0f },
        { "pitch", &taikor::EngineParameters::pitch, -24.0f, 24.0f },
        { "bachiHardness", &taikor::EngineParameters::bachiHardness, 0.0f, 1.0f },
        { "strikePosition", &taikor::EngineParameters::strikePosition, -1.0f, 1.0f },
        { "strikeAzimuth", &taikor::EngineParameters::strikeAzimuth,
          -3.14159265358979f, 3.14159265358979f },
        { "velocityDepth", &taikor::EngineParameters::velocityDepth, 0.0f, 1.0f },
        { "velocityCurve", &taikor::EngineParameters::velocityCurve, -1.0f, 1.0f },
        { "tensionModulation", &taikor::EngineParameters::tensionModulation, 0.0f, 1.0f },
        { "strikeNoise", &taikor::EngineParameters::strikeNoise, 0.0f, 1.0f },
        { "humanise", &taikor::EngineParameters::humanise, 0.0f, 1.0f },
        { "micDistance", &taikor::EngineParameters::micDistance, 0.0f, 1.0f },
        { "micSpread", &taikor::EngineParameters::micSpread, 0.0f, 1.0f },
        { "stereoWidth", &taikor::EngineParameters::stereoWidth, 0.0f, 1.0f },
        { "drive", &taikor::EngineParameters::drive, 0.0f, 1.0f },
        { "outputGain", &taikor::EngineParameters::outputGain, 0.0f, 2.0f },
        { "outputHighPassHz", &taikor::EngineParameters::outputHighPassHz, 0.0f, 500.0f },
    };

    for (const auto& field : fields)
    {
        taikor::EngineParameters high;
        high.*field.member = field.high + 1.0e9f;
        expect (TaikoEngineTestAccess::sanitise (high).*field.member == field.high,
                std::string (field.name) + " must clamp a huge value to its upper bound");

        taikor::EngineParameters low;
        low.*field.member = field.low - 1.0e9f;
        expect (TaikoEngineTestAccess::sanitise (low).*field.member == field.low,
                std::string (field.name) + " must clamp a huge negative value to its lower bound");

        taikor::EngineParameters positiveInfinity;
        positiveInfinity.*field.member = infinity;
        expect (TaikoEngineTestAccess::sanitise (positiveInfinity).*field.member == field.high,
                std::string (field.name) + " must clamp +infinity to its upper bound");

        taikor::EngineParameters negativeInfinity;
        negativeInfinity.*field.member = -infinity;
        expect (TaikoEngineTestAccess::sanitise (negativeInfinity).*field.member == field.low,
                std::string (field.name) + " must clamp -infinity to its lower bound");

        taikor::EngineParameters nanParameters;
        nanParameters.*field.member = nan;
        expect (TaikoEngineTestAccess::sanitise (nanParameters).*field.member == field.low,
                std::string (field.name)
                    + " must fold NaN to its lower bound, the same as clampFloat's own NaN rule");
    }

    taikor::EngineParameters lowPerformer;
    lowPerformer.performer = -100;
    expect (TaikoEngineTestAccess::sanitise (lowPerformer).performer == 0,
            "Performer must clamp to P1");
    taikor::EngineParameters highPerformer;
    highPerformer.performer = 100;
    expect (TaikoEngineTestAccess::sanitise (highPerformer).performer == 3,
            "Performer must clamp to P4");

    // octaveBody collapses the old continuous morph to its two physical
    // endpoints at a 0.5 threshold, rather than clamping to a continuous
    // range like every other field above - so it is checked on its own terms.
    taikor::EngineParameters belowThreshold;
    belowThreshold.octaveBody = 0.4999f;
    expect (TaikoEngineTestAccess::sanitise (belowThreshold).octaveBody == 0.0f,
            "octaveBody just under one half must settle at the Drums endpoint");

    taikor::EngineParameters atThreshold;
    atThreshold.octaveBody = 0.5f;
    expect (TaikoEngineTestAccess::sanitise (atThreshold).octaveBody == 1.0f,
            "octaveBody at exactly one half must settle at the Body endpoint, not the Drums one");

    taikor::EngineParameters aboveThreshold;
    aboveThreshold.octaveBody = 0.5001f;
    expect (TaikoEngineTestAccess::sanitise (aboveThreshold).octaveBody == 1.0f,
            "octaveBody just over one half must settle at the Body endpoint");

    taikor::EngineParameters octaveBodyOutOfRange;
    octaveBodyOutOfRange.octaveBody = 40.0f;
    expect (TaikoEngineTestAccess::sanitise (octaveBodyOutOfRange).octaveBody == 1.0f,
            "an out-of-range octaveBody must clamp before the threshold is taken");

    taikor::EngineParameters octaveBodyNegative;
    octaveBodyNegative.octaveBody = -5.0f;
    expect (TaikoEngineTestAccess::sanitise (octaveBodyNegative).octaveBody == 0.0f,
            "a negative octaveBody must clamp to zero before the threshold is taken");

    taikor::EngineParameters octaveBodyNan;
    octaveBodyNan.octaveBody = nan;
    expect (TaikoEngineTestAccess::sanitise (octaveBodyNan).octaveBody == 0.0f,
            "a NaN octaveBody must fold to the Drums endpoint, the low side of the clamp");
}

// parametersForOctave's own comment names two identity cases: Octave Body at
// 0 (guarded by "if (! (body > 0.0f)) return applied;") and octave 0 at any
// Octave Body, because getDrumDescription(0) is both the reference and the
// drum it is compared against, so every delta the transform would add is
// exactly zero. Every other test reaches both paths only indirectly, by
// rendering a full drum through resolveDrumFor, and never asserts either
// identity directly - nor that away from both endpoints the transform
// actually moves something, which is what makes the two identities
// meaningful rather than a coincidence of an always-identity function.
void testParametersForOctaveIsIdentityAtBothOfItsOwnEndpoints()
{
    using taikor::TaikoEngineTestAccess;
    taikor::EngineParameters applied;
    applied.headDiameter = 1.1f;
    applied.bodyDepth = 0.35f;
    applied.tension = 0.42f;
    applied.headMaterial = 0.6f;
    applied.shellMaterial = 0.15f;

    const auto fieldsMatch = [] (const taikor::EngineParameters& a,
                                  const taikor::EngineParameters& b)
    {
        return a.headDiameter == b.headDiameter && a.bodyDepth == b.bodyDepth
            && a.tension == b.tension && a.headMaterial == b.headMaterial
            && a.shellMaterial == b.shellMaterial;
    };

    // At Octave Body 0, the family collapses onto the reference drum and the
    // parameter block must come back untouched, whatever octave is asked for.
    taikor::EngineParameters atBodyZero = applied;
    atBodyZero.octaveBody = 0.0f;
    expect (fieldsMatch (TaikoEngineTestAccess::parametersForOctave (atBodyZero, 2), atBodyZero),
            "Octave Body 0 must return the parameter block untouched at a non-zero octave");

    // A negative or NaN Octave Body clamps to that same zero before the guard
    // is taken, so both must be an identity too, not just a literal 0.0f.
    taikor::EngineParameters negativeBody = applied;
    negativeBody.octaveBody = -3.0f;
    expect (fieldsMatch (TaikoEngineTestAccess::parametersForOctave (negativeBody, -2), negativeBody),
            "a negative Octave Body must clamp to zero before the guard, staying an identity");

    taikor::EngineParameters nanBody = applied;
    nanBody.octaveBody = std::numeric_limits<float>::quiet_NaN();
    expect (fieldsMatch (TaikoEngineTestAccess::parametersForOctave (nanBody, 3), nanBody),
            "a NaN Octave Body must fold to zero before the guard, staying an identity");

    // At octave 0, the reference drum is also the drum itself, so the
    // identity must hold at full Octave Body too, not just at zero.
    taikor::EngineParameters atOctaveZero = applied;
    atOctaveZero.octaveBody = 1.0f;
    expect (fieldsMatch (TaikoEngineTestAccess::parametersForOctave (atOctaveZero, 0), atOctaveZero),
            "octave 0 must return the parameter block untouched at a non-zero Octave Body too");

    // Away from both endpoints the transform must actually move at least one
    // of the five fields it owns, so the two identities above are not simply
    // true of every input.
    taikor::EngineParameters awayFromEndpoints = applied;
    awayFromEndpoints.octaveBody = 1.0f;
    expect (! fieldsMatch (
                TaikoEngineTestAccess::parametersForOctave (awayFromEndpoints, 2), awayFromEndpoints),
            "a non-zero octave at full Octave Body must actually transform at least one field");
}

// contactCollisionMass() reduces the bachi's mass against the membrane's own
// inverse mass at the strike point, but falls back to the bachi's mass alone
// when that inverse mass comes out zero or non-finite. Every real call site
// passes a profile from the fixed strikeProfile() table, whose membraneGain
// is always positive, against a drum resolved by resolveDrumFor(), whose
// batterDensity is always a positive, finite geometric interpolation - so
// neither half of that guard is ever taken from Source. Nothing in the test
// suite exercised it directly before this.
void testContactCollisionMassFallsBackWhenTheMembraneContributesNothing()
{
    using taikor::TaikoEngineTestAccess;
    const float strikeRadius = 0.3f;
    const float strikerMass = 0.5f;
    const double expectedFallback =
        1.0 / (1.0 / std::max (static_cast<double> (strikerMass), 1.0e-6));

    // A profile with no membrane coupling at all drives every mode's term to
    // exactly zero, so the accumulated inverse head mass is 0.0 - the
    // "!(inverseHeadMass > 0.0)" half of the guard.
    const float zeroCoupling = TaikoEngineTestAccess::contactCollisionMass (
        0.0f, strikeRadius, strikerMass);
    expect (zeroCoupling == static_cast<float> (expectedFallback),
            "zero membrane coupling must fall back to the bachi's own mass alone");

    // An infinite membrane gain drives the accumulated inverse head mass to a
    // non-finite value (some combination of +infinity and NaN, depending on
    // which mode shapes are zero at this radius) - the "!isfinite(...)" half
    // of the same guard, which the zero-coupling case above cannot reach.
    const float nonFiniteCoupling = TaikoEngineTestAccess::contactCollisionMass (
        std::numeric_limits<float>::infinity(), strikeRadius, strikerMass);
    expect (nonFiniteCoupling == static_cast<float> (expectedFallback),
            "a non-finite accumulated inverse head mass must fall back to the bachi's own mass alone");

    // Sanity check that the fallback is not simply always taken: an ordinary,
    // fully-coupled profile at the same radius must resolve to a strictly
    // smaller collision mass, since a genuine membrane contribution can only
    // reduce the reduced mass below the bachi's own.
    const float coupled = TaikoEngineTestAccess::contactCollisionMass (
        1.0f, strikeRadius, strikerMass);
    expect (coupled < strikerMass,
            "a genuinely coupled membrane must reduce the collision mass below the bachi's own");
}

void testColumnStiffnessFactorGuardsItsOwnDomain()
{
    using taikor::TaikoEngineTestAccess;
    constexpr float piFloat = 3.14159265358979f;
    constexpr float quarterWave = 0.5f * piFloat;
    const auto nan = std::numeric_limits<float>::quiet_NaN();

    // resolveDrumGeometry's sole call site reaches x == 0 exactly whenever
    // volumeBranchOmega's eigenvalue has collapsed to non-positive and it
    // returns 0.0 rather than a frequency - the "!(x > 0.0f)" guard's
    // low-frequency-limit fallback, previously exercised only indirectly
    // through a resolved drum's cavityStiffnessFactor.
    expect (TaikoEngineTestAccess::columnStiffnessFactor (0.0f) == 1.0f,
            "a zero column length/frequency product must report the "
            "uncorrected low-frequency limit");
    expect (TaikoEngineTestAccess::columnStiffnessFactor (-1.0f) == 1.0f,
            "a negative x must fall back the same way as zero");
    expect (TaikoEngineTestAccess::columnStiffnessFactor (nan) == 1.0f,
            "a NaN x must fall back the same way as zero");

    // At and beyond the quarter-wave the column is past the one branch on
    // which a lumped stiffness has a meaning, so the factor is floored to
    // exactly zero (the decoupled pair the readout describes at Air
    // Coupling zero) rather than following x cot x negative.
    expect (TaikoEngineTestAccess::columnStiffnessFactor (quarterWave) == 0.0f,
            "the quarter-wave itself must report exactly zero");
    expect (TaikoEngineTestAccess::columnStiffnessFactor (quarterWave + 0.5f) == 0.0f,
            "past the quarter-wave the factor must stay floored at zero rather "
            "than following x cot x negative");

    // The truncation is documented as continuous: x cot x itself reaches
    // zero at the quarter-wave, so the ordinary formula just below it must
    // already sit close to the floor rather than jump onto it.
    const float justBelow =
        TaikoEngineTestAccess::columnStiffnessFactor (quarterWave - 0.01f);
    expect (justBelow > 0.0f && justBelow < 0.02f,
            "the factor must fall continuously to zero approaching the "
            "quarter-wave, not step onto its floor");

    // And an ordinary mid-range x must match the closed form directly,
    // pinning the formula itself rather than only its two domain guards.
    const float mid = TaikoEngineTestAccess::columnStiffnessFactor (1.0f);
    const float expectedMid = 1.0f * std::cos (1.0f) / std::sin (1.0f);
    expect (std::abs (mid - expectedMid) < 1.0e-6f,
            "an ordinary x must resolve to x*cot(x)");
}

void testStiffnessStretchGuardsItsOwnDomain()
{
    using taikor::TaikoEngineTestAccess;
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float besselZero = 2.4048255577f; // an arbitrary mode's own zero

    // resolveDrumGeometry's rigidity is a positive modulus times a positive
    // thickness cubed, divided by a positive tension and radius squared, so
    // stiffnessBatter/stiffnessResonant are always strictly positive for any
    // sanitised EngineParameters - the "!(stiffness > 0.0f)" fallback below
    // is never reached from Source/, and testHeadStiffnessOpensTheModalRatios
    // only ever exercises the ordinary branch through a resolved drum.
    expect (TaikoEngineTestAccess::stiffnessStretch (besselZero, 0.0f) == 1.0f,
            "zero stiffness must report an ideal membrane, no stretch");
    expect (TaikoEngineTestAccess::stiffnessStretch (besselZero, -1.0f) == 1.0f,
            "a negative stiffness must fall back the same way as zero");
    expect (TaikoEngineTestAccess::stiffnessStretch (besselZero, nan) == 1.0f,
            "a NaN stiffness must fall back the same way as zero");

    // The stretch is taken relative to the (0,1) mode's own zero, so a mode
    // evaluated at that same zero must see no stretch at all regardless of
    // how stiff the head is - the numerator and denominator coincide.
    expect (std::abs (TaikoEngineTestAccess::stiffnessStretch (
                std::sqrt (5.7831859629467f), 0.4f) - 1.0f) < 1.0e-5f,
            "the (0,1) mode itself must never be stretched by its own "
            "reference stiffness");

    // An ordinary higher mode must match the closed form directly, pinning
    // the formula itself rather than only its domain guard.
    constexpr float higherZero = 5.5200781103f; // the (0,2) mode's zero
    constexpr float stiffness = 0.4f;
    const float stretched =
        TaikoEngineTestAccess::stiffnessStretch (higherZero, stiffness);
    const float expected = std::sqrt (
        (1.0f + stiffness * higherZero * higherZero)
        / (1.0f + stiffness * 5.7831859629467f));
    expect (std::abs (stretched - expected) < 1.0e-6f,
            "an ordinary mode must resolve to the documented sqrt ratio");
    expect (stretched > 1.0f,
            "a higher mode with positive stiffness must open out above the "
            "unstretched ratio, not below it");
}

void testMountingLossGuardsItsOwnDomain()
{
    using taikor::TaikoEngineTestAccess;

    // resolveDrumGeometry only ever derives mountCorner as a fixed constant
    // over drum.radius, itself clamped to [radiusFloor, radiusCeiling], so
    // the corner is always strictly positive and never near zero - roughly
    // 7 Hz for the largest describable drum up to 3.3 kHz for the smallest.
    // mountingLossAt's own "std::max(mountCorner, 1.0f)" floor guards a
    // corner at or below 1 Hz, which nothing in Source/ ever passes it; a
    // zero or negative corner would otherwise divide the ratio by zero (or
    // flip its sign, which the following square erases anyway) rather than
    // reading as the pinned 1 Hz shelf edge the floor documents.
    constexpr float mountLoss = 5.0f;
    constexpr float frequency = 10.0f;
    const float floored = TaikoEngineTestAccess::mountingLossAt (
        mountLoss, 1.0f, frequency);

    expect (TaikoEngineTestAccess::mountingLossAt (mountLoss, 0.0f, frequency)
                == floored,
            "a zero corner must read as the floored 1 Hz corner, not divide "
            "the ratio by zero");
    expect (TaikoEngineTestAccess::mountingLossAt (mountLoss, -5.0f, frequency)
                == floored,
            "a negative corner must fall back to the same floor as zero");
    expect (TaikoEngineTestAccess::mountingLossAt (mountLoss, 0.5f, frequency)
                == floored,
            "a corner inside the floor but still positive must also be "
            "pinned to it, not used as-is");

    // The floor's own reason to exist: at the corner it protects, a request
    // for the shelf at frequency 0 divides 0 by 0. Every real call site's
    // corner sits far above zero, so this is otherwise unreachable, but the
    // floored function must still answer with the finite unbent-shelf loss
    // rather than a NaN.
    const float atOrigin = TaikoEngineTestAccess::mountingLossAt (
        mountLoss, 0.0f, 0.0f);
    expect (atOrigin == mountLoss,
            "a zero corner and a zero frequency must read as the unbent "
            "shelf, not propagate a 0/0 NaN");

    // An ordinary corner, comfortably above the floor, must match the
    // documented fourth-order shelf directly rather than only its guard.
    constexpr float corner = 55.0f;
    const float ordinary = TaikoEngineTestAccess::mountingLossAt (
        mountLoss, corner, frequency);
    const float ratio = frequency / corner;
    const float expected = mountLoss / (1.0f + ratio * ratio * ratio * ratio);
    expect (std::abs (ordinary - expected) < 1.0e-6f,
            "an ordinary corner must resolve to the fourth-order shelf");
}

void testContinuumBandVarianceGuardsItsOwnDomain()
{
    using taikor::TaikoEngineTestAccess;
    const auto nan = std::numeric_limits<float>::quiet_NaN();

    // An inverted pair - a high coefficient at or below the low one - is the
    // only way to reach the fallback, and it must report the documented unit
    // variance rather than whatever sign or magnitude the ill-posed solve
    // produced (measured: -4.0e-32 for this exact pair, which would have
    // handed a live band a negative variance and an imaginary level).
    expect (TaikoEngineTestAccess::continuumBandVariance (0.5f, 1.0e-6f) == 1.0f,
            "a high coefficient below the low one must fall back to unit "
            "variance rather than propagate a negative or vanishing solve");
    expect (TaikoEngineTestAccess::continuumBandVariance (1.0f, 1.0f) == 1.0f,
            "coincident low/high coefficients must fall back the same way");
    // A NaN argument does not itself reach the fallback: the internal clamp
    // (clampFloat's own "!(value == value)" branch) turns a NaN low or high
    // coefficient into the floor, 1e-7, same as any other out-of-range input,
    // and a floor paired with an ordinary partner is not degenerate - it is
    // clampFloat, not this guard, that keeps a NaN from ever reaching the
    // Lyapunov solve. Pairing NaN on both sides collapses them to the same
    // floor and does reach the fallback, exactly as the coincident case above.
    expect (TaikoEngineTestAccess::continuumBandVariance (nan, nan) == 1.0f,
            "two NaN coefficients must clamp to the same floor and fall back "
            "as a coincident pair");
    expect (TaikoEngineTestAccess::continuumBandVariance (nan, 0.1f) != 1.0f,
            "a NaN low coefficient paired with an ordinary high one must "
            "clamp to the floor and resolve normally, not fall back");

    // Every reachable pair keeps lowCoefficient < highCoefficient by
    // construction (see the wrapper's own comment), and a real drum's live
    // bands must therefore never take the fallback branch. Cross-checking
    // against continuumVarianceCache_'s own stored result ties this direct
    // call to the one buildVoiceModes actually relies on, rather than
    // trusting the formula in isolation.
    taikor::TaikoEngine engine;
    auto parameters = defaultParameters();
    parameters.humanise = 0.0f;
    engine.setParameters (parameters);
    engine.prepare (48000.0, 64);
    engine.trigger (taikor::Articulation::Ka, 0, 0.9f);
    const auto bands = TaikoEngineTestAccess::continuumBands (engine);
    expect (! bands.empty(),
            "a Ka at velocity 0.9 must light at least one continuum band");

    bool checkedOne = false;
    for (const auto& band : bands)
    {
        expect (band.lowCoefficient < band.highCoefficient,
                "every live band's low coefficient must sit strictly below "
                "its high one, keeping it off the fallback branch");
        expect (band.cachedVariance != 1.0f,
                "a live band must not be reading back the fallback constant");

        const float direct = TaikoEngineTestAccess::continuumBandVariance (
            band.lowCoefficient, band.highCoefficient);
        expect (direct == band.cachedVariance,
                "a direct call with a live band's own coefficients must "
                "reproduce continuumVarianceCache_'s stored value bit for bit");
        checkedOne = true;
    }
    expect (checkedOne, "the cross-check must run on at least one live band");
}

void testInvalidInputSafety()
{
    taikor::TaikoEngine engine;
    engine.prepare (48000.0, defaultBlockSize);

    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto infinity = std::numeric_limits<float>::infinity();

    taikor::EngineParameters hostile;
    hostile.headDiameter = nan;
    hostile.bodyDepth = infinity;
    hostile.tension = -5.0f;
    hostile.headMaterial = 1.0e9f;
    hostile.shellMaterial = -infinity;
    hostile.resonantTension = nan;
    hostile.cavityCoupling = 12.0f;
    hostile.headDamping = -1.0f;
    hostile.shellResonance = nan;
    hostile.pitch = 1.0e9f;
    hostile.bachiHardness = nan;
    hostile.strikePosition = -20.0f;
    hostile.strikeAzimuth = infinity;
    hostile.velocityDepth = infinity;
    hostile.velocityCurve = nan;
    hostile.tensionModulation = nan;
    hostile.strikeNoise = -3.0f;
    hostile.humanise = nan;
    hostile.octaveBody = 40.0f;
    hostile.micDistance = nan;
    hostile.micSpread = -2.0f;
    hostile.stereoWidth = infinity;
    hostile.drive = nan;
    hostile.outputGain = 1.0e6f;
    hostile.performer = std::numeric_limits<int>::max();
    engine.setParameters (hostile);

    for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        engine.trigger (static_cast<taikor::Articulation> (index), 0, 0.9f);

    const auto rendered = render (engine, 24000);
    expect (rendered.finite, "hostile parameters produced non-finite audio");
    expect (rendered.peak <= 1.0001, "hostile parameters exceeded full scale");

    // Invalid strokes must simply do nothing.
    engine.allSoundsOff();
    engine.setParameters (defaultParameters());
    engine.trigger (taikor::Articulation::Don, 0, nan);
    engine.trigger (taikor::Articulation::Don, 0, -1.0f);
    engine.trigger (taikor::Articulation::Don, 0, 0.0f);
    engine.trigger (static_cast<taikor::Articulation> (99), 0, 0.9f);
    expect (engine.getActiveVoiceCount() == 0,
            "an invalid stroke must not allocate a voice");

    // Out-of-range octaves must be clamped rather than read out of bounds.
    engine.trigger (taikor::Articulation::Don, -50, 0.9f);
    engine.trigger (taikor::Articulation::Don, 50, 0.9f);
    const auto clamped = render (engine, 12000);
    expect (clamped.finite, "an out-of-range octave produced non-finite audio");

    // A null or empty buffer must be ignored rather than crash.
    engine.process (nullptr, nullptr, 64);
    std::vector<float> left (16), right (16);
    engine.process (left.data(), right.data(), 0);
    engine.process (left.data(), right.data(), -5);
}

void testUiPresentationMath()
{
    using namespace taikor::ui;

    expect (std::abs (onePoleCoefficient (0.0f, 30.0f) - 1.0f) < 1.0e-6f,
            "a zero time constant must be an immediate jump");
    expect (onePoleCoefficient (1.0f, 0.0f) == 1.0f,
            "a zero update rate must not divide by zero");
    expect (onePoleCoefficient (-1.0f, 30.0f) == 1.0f,
            "a negative time constant must fall back to an immediate jump");
    expect (onePoleCoefficient (1.0f, -30.0f) == 1.0f,
            "a negative update rate must not divide by zero");
    const auto coefficient = onePoleCoefficient (0.1f, 30.0f);
    expect (coefficient > 0.0f && coefficient < 1.0f,
            "a smoothing coefficient must stay inside the unit interval");

    expect (decayMultiplier (-12.0f, 1.0f, 30.0f) < 1.0f,
            "a decay multiplier must be less than one");
    expect (decayMultiplier (-12.0f, 0.0f, 30.0f) == 0.0f,
            "a zero decay time must be handled");
    expect (decayMultiplier (-12.0f, 1.0f, -30.0f) == 0.0f,
            "a negative update rate must be handled the same as a zero one");

    expect (std::abs (meterPositionForLinear (1.0f, -48.0f) - 1.0f) < 1.0e-5f,
            "full scale must sit at the top of the meter");
    expect (meterPositionForLinear (0.0f, -48.0f) == 0.0f,
            "silence must sit at the bottom of the meter");
    for (const float position : { 0.1f, 0.35f, 0.7f, 1.0f })
    {
        const auto linear = linearForMeterPosition (position, -48.0f);
        expect (std::abs (meterPositionForLinear (linear, -48.0f) - position) < 1.0e-4f,
                "the meter scale must round-trip");
    }
    // A non-negative floor is nonsensical (there would be no dynamic range
    // to map onto), and both directions guard against it identically.
    expect (meterPositionForLinear (0.5f, 0.0f) == 0.0f,
            "a non-negative floor must not be used for the meter position");
    expect (linearForMeterPosition (0.5f, 0.0f) == 0.0f,
            "a non-negative floor must not be used for the meter's inverse");

    MeterBallistics ballistics;
    ballistics.reset();
    for (int index = 0; index < 60; ++index)
        ballistics.update (0.8f, 0.5f, 0.05f, 0.9f, 10.0f);
    expect (ballistics.level > 0.7f, "the meter must reach a sustained level");
    expect (ballistics.peak >= ballistics.level, "the peak marker must lead the level");

    // update()'s attack/release/peak-fall coefficients are all run through
    // the shared clamp(), which (unlike a per-argument sanitize) folds a NaN
    // input to the clamp's own low bound rather than to some other default -
    // exercised by every call above but never asserted on its own.
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    MeterBallistics frozenAttack;
    frozenAttack.update (0.6f, nan, 1.0f, 0.9f, 0.0f);
    expect (frozenAttack.level == 0.0f,
            "a NaN attack coefficient must clamp to zero (no movement), not one");
    MeterBallistics stuckRelease;
    stuckRelease.update (0.6f, 1.0f, 1.0f, 0.9f, 0.0f);
    stuckRelease.update (0.0f, 1.0f, nan, 0.9f, 0.0f);
    expect (stuckRelease.level == 0.6f,
            "a NaN release coefficient must clamp to zero (no fallback), not one");
    MeterBallistics frozenPeak;
    frozenPeak.update (0.6f, 1.0f, 1.0f, 0.9f, 0.0f);
    frozenPeak.update (0.0f, 1.0f, 1.0f, nan, 0.0f);
    expect (frozenPeak.peak == frozenPeak.level,
            "a NaN peak-fall multiplier must clamp to zero, collapsing the peak to the level");

    const auto beforeRelease = ballistics.level;
    for (int index = 0; index < 30; ++index)
        ballistics.update (0.0f, 0.5f, 0.05f, 0.9f, 10.0f);
    expect (ballistics.level < beforeRelease, "the meter must fall back");

    const auto layout = rowLayout (600, 12, 6, 12);
    expect (layout.cellSize > 0, "a row layout must produce a usable cell size");
    expect (cellOffset (layout, 6, 0) == layout.origin,
            "the first cell must sit at the row origin");
    expect (cellOffset (layout, 6, 11) + layout.cellSize <= 600,
            "the last cell must fit inside the row");
    // Every call site in the editor loops an index up from zero, so the
    // `index <= 0` guard's negative side is never actually reached there.
    expect (cellOffset (layout, 6, -1) == layout.origin,
            "a negative index must fall back to the row origin, not extrapolate past it");
    expect (rowLayout (0, 12, 6, 12).cellSize == 1,
            "a degenerate row must not divide by zero");
    expect (rowLayout (600, 0, 6, 0).cellSize == 1,
            "a row with no columns must not divide by zero");
    // A positive extent that still cannot fit its columns takes the same
    // floor as the two all-degenerate cases above, but through the
    // `available > columns` comparison rather than a zero or negative
    // `available`: nine pixels shared across twelve columns divides down to
    // zero without it. No call site in the editor reaches this - the
    // editor's own minimum width is more than an order of magnitude wider
    // than any row's column count needs - so only the two degenerate cases
    // just above had ever been asserted directly.
    const auto crampedRow = rowLayout (20, 12, 1, 12);
    expect (crampedRow.cellSize == 1,
            "a row too narrow for its columns must floor to a one-pixel cell, not zero");
    expect (crampedRow.origin == 0,
            "a cramped row's overflowing centring math must clamp to zero, not go negative");

    // A short row stays centred under a longer one.
    const auto shortRow = rowLayout (600, 12, 6, 5);
    expect (shortRow.origin > layout.origin,
            "a partly filled row must be centred");

    const auto centre = headPointFor (0.0f, 1.2f);
    expect (std::abs (centre.x) < 1.0e-6f && std::abs (centre.y) < 1.0e-6f,
            "a centre strike must map to the middle of the head");
    const auto rim = headPointFor (1.0f, 0.0f);
    expect (std::abs (rim.x - 1.0f) < 1.0e-6f,
            "a rim strike at zero radians must sit at the right of the head");
    const auto clampedPoint = headPointFor (5.0f, 0.0f);
    expect (std::abs (clampedPoint.x - 1.0f) < 1.0e-6f,
            "a head point must be clamped to the head");
    // Every call site passes a radius already clamped to [0, 1] (or a
    // literal within that range), so the low side of headPointFor's own
    // clamp is never reached from Source/ at all.
    const auto negativeRadius = headPointFor (-1.0f, 0.0f);
    expect (std::abs (negativeRadius.x) < 1.0e-6f && std::abs (negativeRadius.y) < 1.0e-6f,
            "a negative radius must clamp to the centre, not mirror across it");

    expect (std::abs (semitonesBetween (880.0f, 440.0f) - 12.0f) < 1.0e-4f,
            "an octave must measure twelve semitones");
    expect (semitonesBetween (0.0f, 440.0f) == 0.0f,
            "an invalid frequency must not produce a logarithm of zero");
    expect (semitonesBetween (-100.0f, 440.0f) == 0.0f,
            "a negative frequency must not produce a logarithm of a negative number");
    expect (semitonesBetween (440.0f, 0.0f) == 0.0f,
            "an invalid reference must not divide by zero");
    expect (semitonesBetween (440.0f, -1.0f) == 0.0f,
            "a negative reference must not produce a logarithm of a negative ratio");
    expect (semitonesBetween (std::numeric_limits<float>::quiet_NaN(), 440.0f) == 0.0f,
            "a NaN frequency must fall back to the same guard, not propagate the NaN");

    expect (std::abs (mix (0.0f, 10.0f, 0.25f) - 2.5f) < 1.0e-6f, "mix is wrong");
    expect (mix (0.0f, 10.0f, -3.0f) == 0.0f && mix (0.0f, 10.0f, 4.0f) == 10.0f,
            "mix must clamp its amount to the unit interval");
    expect (mix (0.0f, 10.0f, std::numeric_limits<float>::quiet_NaN()) == 0.0f,
            "a NaN mix amount must clamp to zero, staying at the start value");
    expect (smoothStep (0.0f, 1.0f, -1.0f) == 0.0f, "smoothStep must clamp low");
    expect (smoothStep (0.0f, 1.0f, 2.0f) == 1.0f, "smoothStep must clamp high");
    expect (smoothStep (1.0f, 1.0f, 2.0f) == 1.0f, "smoothStep must handle a zero span");
    expect (smoothStep (1.0f, 1.0f, 0.5f) == 0.0f,
            "a zero span must also fall to zero below its collapsed edge, not just clamp above it");
    expect (clamp (std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f) == 0.0f,
            "clamp must reject NaN");
}

void testIdleCostAndStressPerformance()
{
    taikor::TaikoEngine engine;
    engine.setParameters (defaultParameters());
    engine.prepare (48000.0, defaultBlockSize);

    // An idle drum must cost almost nothing: a track is silent most of the time.
    const auto idleStart = std::chrono::steady_clock::now();
    render (engine, 48000 * 10);
    const double idleSeconds =
        std::chrono::duration<double> (std::chrono::steady_clock::now() - idleStart)
            .count();
    expect (idleSeconds < 2.0, "an idle engine cost far more than it should");

    // A dense roll across every stroke and every octave.
    std::vector<float> left (static_cast<std::size_t> (defaultBlockSize));
    std::vector<float> right (static_cast<std::size_t> (defaultBlockSize));
    bool finite = true;
    double peak = 0.0;

    const auto start = std::chrono::steady_clock::now();
    const int totalSamples = 48000 * 2;
    for (int rendered = 0, stroke = 0; rendered < totalSamples; ++stroke)
    {
        engine.trigger (
            static_cast<taikor::Articulation> (stroke % taikor::articulationCount),
            (stroke % 6) - 2, 0.4f + 0.06f * static_cast<float> (stroke % 10));

        const int count = std::min (defaultBlockSize, totalSamples - rendered);
        engine.process (left.data(), right.data(), count);
        for (int sample = 0; sample < count; ++sample)
        {
            const float l = left[static_cast<std::size_t> (sample)];
            const float r = right[static_cast<std::size_t> (sample)];
            finite = finite && std::isfinite (l) && std::isfinite (r);
            peak = std::max ({ peak, std::abs (static_cast<double> (l)),
                               std::abs (static_cast<double> (r)) });
        }
        rendered += count;
    }
    const double elapsed =
        std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();

    expect (finite && peak <= 1.0001, "the dense stress render produced unsafe audio");
    expect (elapsed < 20.0,
            "a two-second dense render exceeded the generous performance guardrail");
}

// Regressions for control endpoints and performance gestures. Each of these
// was a real defect: they are checked here because every one of them is
// invisible to a test that only looks at the solved drum, and audible to a
// player immediately.
void testControlEndpointsAndGestures()
{
    const auto parameters = defaultParameters();

    // Air Coupling at exactly zero must not be a cliff. The axisymmetric modes
    // are solved as a two-by-two eigenproblem, and at zero coupling that system
    // becomes degenerate; resolving the degeneracy by branch index rather than
    // by which head the eigenvalue belongs to handed both branches to the
    // resonant head and silenced the drum's boom at the endpoint.
    const auto bodyEnergy = [&parameters] (float coupling)
    {
        auto tuned = parameters;
        tuned.cavityCoupling = coupling;
        tuned.humanise = 0.0f;
        const auto rendered =
            strike (tuned, taikor::Articulation::Don, 0, 0.9f, 48000.0, 48000);
        const auto mono = rendered.mono();
        double sum = 0.0;
        for (std::size_t index = 4800; index < 24000 && index < mono.size(); ++index)
            sum += static_cast<double> (mono[index]) * mono[index];
        return std::sqrt (sum / 19200.0);
    };

    const auto sealedBody = bodyEnergy (0.85f);
    const auto openBody = bodyEnergy (0.0f);
    const auto barelyCoupled = bodyEnergy (0.001f);

    expect (openBody > sealedBody * 0.5,
            "an uncoupled body must still have a centre boom");
    expect (std::abs (openBody - barelyCoupled) < barelyCoupled * 0.10,
            "the Air Coupling control must be continuous at zero");

    // The wheel presses the head, so a stroke that is already ringing has to
    // bend with it - not merely the strokes that follow it.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.tensionModulation = 0.0f;
        // A small head, so the fifty-millisecond first reading has several
        // cycles of the fundamental to work with rather than two.
        tuned.headDiameter = 0.30f;

        taikor::TaikoEngine engine;
        engine.setParameters (tuned);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.95f);

        const auto before = render (engine, 24000).mono();
        engine.setPitchBend (1.0f);
        const auto after = render (engine, 72000).mono();

        // The search band follows the drum the engine reports rather than a
        // range written for one particular default, so retuning the instrument
        // cannot silently move the measurement off the partial it is watching.
        // It is also narrow enough to hold one partial: the two branches of the
        // axisymmetric pair are damped differently, so which of them is loudest
        // changes over a stroke, and a band wide enough for both measures the
        // fundamental at one end and the breathing branch at the other - which
        // reports a bend that never happened, or hides one that did.
        const auto resting = engine.measureDrum (0).loadedFundamentalHz;
        const auto restingPitch = dominantFrequency (
            before, 48000.0, static_cast<double> (resting) * 0.90,
            static_cast<double> (resting) * 1.15, 0.05, 2400u);
        // Skip the glide itself and measure where the drum settled. The band
        // starts at the resting pitch, so a wheel that did nothing reads as no
        // bend rather than as some other partial.
        const auto bentPitch = dominantFrequency (
            after, 48000.0, restingPitch * 0.99, restingPitch * 1.32, 0.05, 16000u);
        const auto semitones = 12.0 * std::log2 (bentPitch / restingPitch);

        expect (semitones > 1.5 && semitones < 2.5,
                "a fully raised wheel must bend a ringing stroke by about two semitones");
    }

    // The live bank follows the wheel by moving its poles; the solved drum
    // cache is needed only when another stick arrives. Re-solving all four
    // geometries at every host block while the smoother moves is expensive and
    // cannot affect the already-ringing bank.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.tensionModulation = 0.0f;

        taikor::TaikoEngine engine;
        engine.setParameters (tuned);
        engine.prepare (48000.0, 64);
        engine.trigger (taikor::Articulation::Don, 0, 0.8f);
        const float before = taikor::TaikoEngineTestAccess::cachedPitchBend (engine);

        engine.setPitchBend (1.0f);
        render (engine, 9600, 64);
        expect (! taikor::TaikoEngineTestAccess::drumCacheValid (engine),
                "a moved wheel must invalidate geometry for the next strike");
        expect (taikor::TaikoEngineTestAccess::cachedPitchBend (engine) == before,
                "wheel smoothing re-solved dormant strike geometry on the audio thread");

        engine.trigger (taikor::Articulation::Don, 0, 0.8f);
        expect (taikor::TaikoEngineTestAccess::drumCacheValid (engine),
                "the next strike did not refresh its invalid geometry cache");
        expect (taikor::TaikoEngineTestAccess::cachedPitchBend (engine) > 0.9f,
                "the refreshed strike geometry did not follow the settled wheel");
    }

    // reset() snaps the smoother to the wheel target. If that jump crosses the
    // geometry cached by the preceding stroke, the first stroke after reset
    // must not reuse the old bend merely because no process block ran between
    // the reset and the note.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.tensionModulation = 0.0f;

        taikor::TaikoEngine engine;
        engine.setParameters (tuned);
        engine.prepare (48000.0, 64);
        engine.trigger (taikor::Articulation::Don, 0, 0.8f);
        engine.setPitchBend (1.0f);
        engine.reset();
        engine.trigger (taikor::Articulation::Don, 0, 0.8f);

        expect (taikor::TaikoEngineTestAccess::cachedPitchBend (engine) > 0.99f,
                "reset reused strike geometry from before its pitch-bend snap");
    }

    // Pitch automation uses that same live pole shift. It invalidates the next
    // strike's projection cache, but must not schedule a structural bank solve
    // on the audio thread.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.tensionModulation = 0.0f;

        taikor::TaikoEngine engine;
        engine.setParameters (tuned);
        engine.prepare (48000.0, 64);
        engine.trigger (taikor::Articulation::Don, 0, 0.8f);
        const auto revision =
            taikor::TaikoEngineTestAccess::physicalConfigurationRevision (engine);
        const float originalBuildPitch =
            taikor::TaikoEngineTestAccess::physicalConfigurationPitch (engine);

        // Use the full musical range: over a semitone the uniform live shift
        // is an excellent tail approximation, while two octaves materially
        // change the coupled-head eigenvectors and modal masses a new contact
        // must use.
        tuned.pitch += 24.0f;
        engine.setParameters (tuned);
        expect (! taikor::TaikoEngineTestAccess::drumCacheValid (engine),
                "Pitch automation did not invalidate the next strike's geometry");
        expect (taikor::TaikoEngineTestAccess::physicalConfigurationRevision (engine)
                    == revision,
                "Pitch automation scheduled a structural physical-bank rebuild");
        render (engine, 256, 64);
        expect (taikor::TaikoEngineTestAccess::physicalConfigurationRevision (engine)
                    == revision,
                "processing Pitch automation rebuilt the shared drum geometry");
        expect (taikor::TaikoEngineTestAccess::physicalConfigurationPitch (engine)
                    == originalBuildPitch,
                "tail-only Pitch automation rebuilt the exact bank before a strike");

        engine.trigger (taikor::Articulation::Don, 0, 0.8f);
        expect (taikor::TaikoEngineTestAccess::physicalConfigurationPitch (engine)
                    == tuned.pitch,
                "the first strike after Pitch automation kept the old exact bank");

        taikor::TaikoEngine fresh;
        fresh.setParameters (tuned);
        fresh.prepare (48000.0, 64);
        fresh.trigger (taikor::Articulation::Don, 0, 0.8f);
        const auto rebuilt = taikor::TaikoEngineTestAccess::newestVoiceBank (engine);
        const auto expected = taikor::TaikoEngineTestAccess::newestVoiceBank (fresh);
        expect (rebuilt.size() == expected.size(),
                "Pitch retrigger did not rebuild the fresh bank's mode set");
        double configurationError = rebuilt.size() == expected.size() ? 0.0 : 1.0;
        for (std::size_t mode = 0; mode < std::min (rebuilt.size(), expected.size()); ++mode)
            for (std::size_t term = 0; term < rebuilt[mode].size(); ++term)
                configurationError = std::max (
                    configurationError,
                    std::abs (static_cast<double> (rebuilt[mode][term])
                              - expected[mode][term]));
        expect (configurationError < 1.0e-5,
                "Pitch retrigger's physical bank/projection differs from a fresh solve");

        tuned.shellResonance = 1.0f - tuned.shellResonance;
        engine.setParameters (tuned);
        expect (! taikor::TaikoEngineTestAccess::drumCacheValid (engine),
                "Shell Resonance did not invalidate the next hoop-contact projection");
        expect (taikor::TaikoEngineTestAccess::physicalConfigurationRevision (engine)
                    == revision,
                "Shell Resonance scheduled an unrelated physical-pole rebuild");
    }

    // A panic snaps the wheel to wherever it is actually being held, because
    // there is nothing left for a gesture in transit to be continuous with. The
    // wheel is geometry, so that snap has to take the solved drum with it: if a
    // note arrives in the same block - which is exactly what a host doing an
    // all-notes-off and then playing looks like - it would otherwise be built
    // on the drum as it stood before the wheel moved, and nothing afterwards
    // corrects it, because the voice records the tuning it was told it had.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.tensionModulation = 0.0f;
        tuned.headDiameter = 0.30f;

        const auto pitchOf = [&tuned] (bool bendBeforePanic)
        {
            taikor::TaikoEngine engine;
            engine.setParameters (tuned);
            engine.prepare (48000.0, defaultBlockSize);

            if (bendBeforePanic)
            {
                // Settle at centre first, so the cache is valid and solved for
                // no bend at all when the wheel moves.
                render (engine, 5120);
                engine.setPitchBend (1.0f);
                engine.allSoundsOff();
            }
            else
            {
                engine.setPitchBend (1.0f);
                render (engine, 51200);
                engine.allSoundsOff();
            }

            engine.trigger (taikor::Articulation::Don, 0, 0.95f);
            const auto mono = render (engine, 51200).mono();
            return dominantFrequency (mono, 48000.0, 180.0, 320.0, 0.05, 24000u);
        };

        const auto settled = pitchOf (false);
        const auto snapped = pitchOf (true);
        expect (settled > 0.0 && snapped > 0.0,
                "the panic-then-play probe produced no pitch to measure");
        expect (std::abs (12.0 * std::log2 (snapped / std::max (1.0, settled))) < 0.1,
                "a stroke played straight after a panic must be built on the drum "
                "the wheel is asking for, not the one it was asking for before");
    }

    // The attack glide stretches the head. It must not stretch the wooden body
    // the head is nailed to: a stick-on-stick stroke drives no membrane mode at
    // all, so Tension Mod has nothing it could legitimately change there.
    {
        auto without = parameters;
        without.humanise = 0.0f;
        without.tensionModulation = 0.0f;
        auto with = without;
        with.tensionModulation = 1.0f;

        // The wooden bank must not be retuned by the glide. State it on the bank
        // itself rather than trying to separate the head and direct-hoop paths
        // from a finished Don Rim waveform. Stretching a head does not stretch
        // the body it is nailed to, and the assertion is exact.
        {
            taikor::TaikoEngine engine;
            engine.setParameters (with);
            engine.prepare (48000.0, defaultBlockSize);
            engine.trigger (taikor::Articulation::DonRim, 0, 1.0f);

            using Access = taikor::TaikoEngineTestAccess;
            const auto woodBefore = Access::poles (engine, false);
            const auto headBefore = Access::poles (engine, true);
            expect (! woodBefore.empty(), "a rim shot must ring the body");

            render (engine, 4800);

            const auto woodAfter = Access::poles (engine, false);
            const auto headAfter = Access::poles (engine, true);

            expect (woodAfter == woodBefore,
                    "the tension glide retuned the wooden shell");
            expect (headAfter != headBefore,
                    "the tension glide did not move the head at all, so the "
                    "clause above is vacuous");
        }

        // And it must still do its job on the head, which is many times larger
        // than anything the shell stroke is allowed to move by.
        const auto openWithout =
            strike (without, taikor::Articulation::Don, 0, 0.95f, 48000.0, 24000);
        const auto openWith =
            strike (with, taikor::Articulation::Don, 0, 0.95f, 48000.0, 24000);
        const auto headChange =
            maximumAbsoluteDifference (openWithout.left, openWith.left);
        expect (headChange > 1.0e-2, "the tension glide must still bend the head");
    }

    // Width multiplies the side signal, so automating it must not step the
    // audio at a block boundary. Measured as the jump across the exact sample
    // where the control changed, against the steps the signal is making anyway
    // just after it: a smoothed control disappears into the signal, an
    // unsmoothed one stands several times above it.
    {
        const auto boundaryAgainstSignal = [&parameters] (bool slam)
        {
            auto tuned = parameters;
            tuned.humanise = 0.0f;
            tuned.micSpread = 1.0f;
            tuned.stereoWidth = 0.0f;

            constexpr int block = 256;
            constexpr int slamBlock = 3;

            taikor::TaikoEngine engine;
            engine.setParameters (tuned);
            engine.prepare (48000.0, block);
            engine.trigger (taikor::Articulation::Ka, 0, 0.95f);

            std::vector<float> left (static_cast<std::size_t> (block));
            std::vector<float> right (static_cast<std::size_t> (block));
            std::vector<float> history;

            for (int index = 0; index < slamBlock + 4; ++index)
            {
                if (slam && index == slamBlock)
                {
                    auto opened = tuned;
                    opened.stereoWidth = 1.0f;
                    engine.setParameters (opened);
                }
                engine.process (left.data(), right.data(), block);
                history.insert (history.end(), left.begin(), left.end());
            }

            const auto at = static_cast<std::size_t> (slamBlock * block);
            const auto boundary =
                std::abs (static_cast<double> (history[at]) - history[at - 1]);

            double typical = 0.0;
            for (std::size_t index = at + 8; index < at + 200; ++index)
                typical = std::max (typical,
                                    std::abs (static_cast<double> (history[index])
                                              - history[index - 1]));

            return boundary / std::max (typical, 1.0e-9);
        };

        expect (boundaryAgainstSignal (true) < 2.0,
                "automating the width stepped the audio at the block boundary");
        expect (boundaryAgainstSignal (false) < 2.0,
                "the width step measurement is picking up ordinary signal content");
    }

    // A stroke struck after a bend has settled must be in tune with the bend.
    // The cached drums the render path builds strokes from were invalidated on
    // the wheel's per-sample increment, which at high sample rates falls below
    // any sensible epsilon long before the glide has actually arrived - leaving
    // the cache, and every new stroke, flat.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.tensionModulation = 0.0f;

        constexpr double highRate = 384000.0;
        taikor::TaikoEngine engine;
        engine.setParameters (tuned);
        engine.prepare (highRate, 512);

        engine.setPitchBend (1.0f);
        render (engine, static_cast<int> (highRate * 1.5), 512);

        const auto predicted = engine.measureDrum (0).loadedFundamentalHz;
        engine.trigger (taikor::Articulation::Don, 0, 0.95f);
        const auto rendered = render (engine, static_cast<int> (highRate * 0.4), 512);
        const auto sounded = dominantFrequency (
            rendered.mono(), highRate, predicted * 0.85, predicted * 1.15, 0.05,
            static_cast<std::size_t> (highRate * 0.02));

        const auto cents = 1200.0 * std::log2 (sounded / predicted);
        expect (std::abs (cents) < 10.0,
                "a stroke struck after the bend settled is "
                    + std::to_string (cents) + " cents from the bend it was struck at");
    }

    // A low-loss drum can ring far longer than the tail the host is told to
    // expect, so a voice still has to end at the cap - but it has to be faded
    // out, not cut. A cut at an audible level is a click, and it rings the
    // shared DC blocker on the way out.
    {
        // A large, tight, undamped head. The longest-ringing drum this
        // instrument can be asked for is the biggest one, which is what a drum
        // family sounds like: bigger drums put their modes lower, where they
        // radiate less and the head's own hysteresis takes less per cycle, so
        // they ring longer. This check used to have to reach for a thirty
        // centimetre head instead, because the mounting shelf was pinned at an
        // absolute fifty-five hertz and swallowed the fundamental of anything
        // large. A 1.3 m head rings for thirty seconds; a 30 cm one for under
        // five.
        auto ringing = parameters;
        ringing.humanise = 0.0f;
        ringing.headDiameter = 1.30f;
        ringing.headMaterial = 0.0f;
        ringing.headDamping = 0.0f;
        ringing.shellMaterial = 1.0f;
        ringing.octaveBody = 1.0f;
        // This is a fade-path test, so use the output control's valid 0 dB end
        // to make the otherwise very quiet twelve-second tail observable.
        ringing.outputGain = 1.0f;

        taikor::TaikoEngine engine;
        engine.setParameters (ringing);
        engine.prepare (48000.0, defaultBlockSize);

        // The configuration has to actually outlast the cap, or this proves
        // nothing about what happens when a voice reaches it.
        expect (engine.measureDrum (0).tailSeconds
                    > static_cast<float> (taikor::maximumTailSeconds) * 1.2f,
                "the long-tail check no longer uses a drum that outlasts the cap");

        engine.trigger (taikor::Articulation::Don, 0, 1.0f);
        const auto rendered =
            render (engine, static_cast<int> (48000 * (taikor::maximumTailSeconds + 1.0)));
        const auto mono = rendered.mono();

        const auto cap = static_cast<std::size_t> (48000 * taikor::maximumTailSeconds);

        // How loud the drum still is as it reaches the cap. Cutting it here
        // would put a step of about this size into the output; fading it out
        // leaves steps a couple of orders of magnitude smaller.
        double amplitudeAtCap = 0.0;
        for (std::size_t index = cap - 9600; index < cap - 4800 && index < mono.size();
             ++index)
            amplitudeAtCap = std::max (amplitudeAtCap,
                                       std::abs (static_cast<double> (mono[index])));
        // An absolute level, because that is what a cut here would put into the
        // buffer as a step. At unity monitor gain the tail is well above the
        // engine's numerical floor even though it is correctly almost gone.
        expect (amplitudeAtCap > 2.0e-5,
                "this drum is no longer audible at the cap, so the check is "
                "vacuous: " + std::to_string (amplitudeAtCap));

        double largestStep = 0.0;
        for (std::size_t index = cap - 4800; index + 1 < mono.size()
             && index < cap + 9600; ++index)
            largestStep = std::max (largestStep,
                                    std::abs (static_cast<double> (mono[index])
                                              - mono[index - 1]));

        expect (largestStep < amplitudeAtCap * 0.05,
                "the voice was cut at the tail cap instead of faded out");
        expect (windowedRms (mono, cap + 14400, cap + 24000) < 1.0e-6,
                "the voice did not actually end at the tail cap");
    }

    // Drive must converge on bypass rather than switching into a shaper: the
    // first nonzero step should be a hair of saturation, not a jump to a fully
    // shaped signal.
    {
        auto dry = parameters;
        dry.humanise = 0.0f;
        dry.drive = 0.0f;
        auto barely = dry;
        barely.drive = 0.002f;
        auto full = dry;
        full.drive = 1.0f;

        const auto rendered = [] (const taikor::EngineParameters& p)
        {
            return strike (p, taikor::Articulation::DonRim, -1, 1.0f, 48000.0, 12000);
        };

        const auto clean = rendered (dry);
        const auto nudged = rendered (barely);
        const auto driven = rendered (full);

        const auto nudgedChange = maximumAbsoluteDifference (clean.left, nudged.left);
        const auto drivenChange = maximumAbsoluteDifference (clean.left, driven.left);

        expect (drivenChange > 1.0e-3, "the drive control must do something");
        expect (nudgedChange < drivenChange * 0.05,
                "the first step of Drive jumped most of the way to full saturation");
    }

    // Shell Resonance belongs to direct hoop/body excitation. Ordinary head
    // strokes retain the shell's boundary loss, but must not grow an audible
    // proper-shell pole from this observation control.
    {
        auto quiet = parameters;
        quiet.humanise = 0.0f;
        quiet.shellResonance = 0.0f;
        auto loud = quiet;
        loud.shellResonance = 1.0f;

        const auto a = strike (quiet, taikor::Articulation::Don, 0, 0.95f, 48000.0, 24000);
        const auto b = strike (loud, taikor::Articulation::Don, 0, 0.95f, 48000.0, 24000);
        const auto openChange = maximumAbsoluteDifference (a.left, b.left);
        expect (openChange == 0.0,
                "Shell Resonance fed a head-only stroke into the shell bank");

        const auto c = strike (quiet, taikor::Articulation::DonRim, 0, 0.95f, 48000.0, 24000);
        const auto d = strike (loud, taikor::Articulation::DonRim, 0, 0.95f, 48000.0, 24000);
        expect (maximumAbsoluteDifference (c.left, d.left) > 1.0e-4,
                "Shell Resonance stopped colouring the direct hoop strike");
    }

    // Automating Pitch must retune a stroke that is already ringing, for the
    // same reason the wheel does: both are head tension.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.tensionModulation = 0.0f;
        // Small, for the same reason the wheel's own check uses a small head:
        // the first reading is fifty milliseconds long and needs cycles in it.
        tuned.headDiameter = 0.30f;

        taikor::TaikoEngine engine;
        engine.setParameters (tuned);
        engine.prepare (48000.0, defaultBlockSize);
        engine.trigger (taikor::Articulation::Don, 0, 0.95f);

        // Read before the automation moves, or the band is centred on where
        // the drum ends up and cannot see where it started.
        const auto resting = engine.measureDrum (0).loadedFundamentalHz;

        const auto before = render (engine, 24000).mono();

        auto raised = tuned;
        raised.pitch = 7.0f;
        engine.setParameters (raised);
        const auto after = render (engine, 72000).mono();

        // One partial per band, and the second band anchored on where the first
        // one actually landed - see the wheel's own check for why a band wide
        // enough to hold both branches measures a different partial at each end.
        const auto restingPitch = dominantFrequency (
            before, 48000.0, static_cast<double> (resting) * 0.90,
            static_cast<double> (resting) * 1.15, 0.05, 2400u);
        const auto raisedPitch = dominantFrequency (
            after, 48000.0, restingPitch * 0.99, restingPitch * 1.68, 0.05, 16000u);
        const auto semitones = 12.0 * std::log2 (raisedPitch / restingPitch);

        expect (semitones > 6.0 && semitones < 8.0,
                "automating Pitch must retune a ringing stroke by the amount asked for");
    }

    // A hand laid on the head damps the head. It is not resting on the wooden
    // shell, and it has nothing at all to do with two sticks clicked together.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;

        // Measured over the settled tail rather than the whole stroke: a hand
        // takes about a fifth of a second to arrive and press, so the attack is
        // barely touched either way and averaging it in hides the effect.
        // Each stroke is measured over its own tail, because they are wildly
        // different lengths: the drum rings for seconds and a stick click is
        // gone in a tenth of a second. Measuring the click over the drum's
        // window compares silence with silence and proves nothing.
        const auto tail = [&tuned] (taikor::Articulation articulation, bool handDown)
        {
            taikor::TaikoEngine engine;
            engine.setParameters (tuned);
            engine.prepare (48000.0, defaultBlockSize);
            if (handDown)
                engine.setHandDamping (1.0f);
            engine.trigger (articulation, 0, 0.95f);
            return render (engine, 48000 * 2).mono();
        };

        const auto tailEnergy = [&tail] (taikor::Articulation articulation,
                                         bool handDown, std::size_t first,
                                         std::size_t last)
        {
            return windowedRms (tail (articulation, handDown), first, last);
        };

        // Measured at the head's own two modes rather than broadband. A hand
        // damps the head; it does not touch the wooden body, and it cannot
        // reach the airborne click of the stick landing. Those two set a floor
        // on the total that has nothing to do with how well the hand works, and
        // a broadband ratio is really measuring that floor - it moved the
        // moment the shell's ring changed, which is exactly the wrong
        // sensitivity for a test of the hand.
        const auto measurements = taikor::TaikoEngine::measure (tuned, 0);
        const auto headModes = [&measurements] (const std::vector<float>& samples)
        {
            return binMagnitude (samples, measurements.loadedFundamentalHz, 48000.0,
                                 24000u, 72000u)
                 + binMagnitude (samples, measurements.breathingModeHz, 48000.0,
                                 24000u, 72000u);
        };

        const auto openHead = headModes (tail (taikor::Articulation::Don, false));
        const auto dampedHead = headModes (tail (taikor::Articulation::Don, true));
        expect (openHead > 1.0e-3,
                "the head's tail measurement window caught no signal");
        expect (dampedHead < openHead * 0.05,
                "a hand on the head must damp the head");

        // Broadband as well as at the modes. Measuring the modes alone is
        // sharper about what the hand reaches, but it is blind to everything
        // the head does above them - and when the continuum was first added it
        // was not damped at all, which this check would have caught and the
        // modal one did not.
        // Early enough to still contain the continuum, which empties from the
        // top down and is largely gone by a quarter of a second.
        const auto openBroad =
            tailEnergy (taikor::Articulation::Don, false, 2400u, 12000u);
        const auto dampedBroad =
            tailEnergy (taikor::Articulation::Don, true, 2400u, 12000u);
        expect (openBroad > 1.0e-5,
                "the broadband tail measurement window caught no signal");
        expect (dampedBroad < openBroad * 0.68,
                "a hand on the head must damp everything the head is doing, "
                "not only its resolved modes");

    }

    // The tail readout has to describe the drum the fundamental readout
    // describes. Reporting the breathing mode's decay beside the other
    // branch's frequency described neither, and the two diverge on a sealed
    // drum because only one of them radiates.
    {
        taikor::TaikoEngine engine;
        engine.prepare (48000.0, defaultBlockSize);

        auto sealed = parameters;
        sealed.cavityCoupling = 1.0f;
        engine.setParameters (sealed);
        const auto sealedDrum = engine.measureDrum (0);

        expect (sealedDrum.tailSeconds > 0.0f, "a drum must report a tail");

        // The rendered tail must be in the same country as the reported one.
        const auto rendered =
            strike (sealed, taikor::Articulation::Don, 0, 0.95f, 48000.0, 48000 * 6);
        const auto measured = decayTime (rendered.mono(), 48000.0, -60.0);
        expect (measured > sealedDrum.tailSeconds * 0.4
                    && measured < sealedDrum.tailSeconds * 2.5,
                "the reported tail does not match the tail the drum actually has");
    }

    // Every stroke of the grid must still schedule its contact and keep its
    // whole bank alive past it. This used to be stated on the press roll, which
    // scheduled eleven contacts and reached its last bounce with nine of its
    // thirty modes because lifetimes were measured from the voice's start; with
    // that stroke retired the retirement offset is still what keeps a stroke's
    // bank alive through its own contact, and this says so on all four.
    {
        for (std::size_t index = 0; index < taikor::articulationCount; ++index)
        {
            const auto articulation = static_cast<taikor::Articulation> (index);
            for (const float damping : { 0.35f, 1.0f })
            {
                auto tuned = parameters;
                tuned.humanise = 0.0f;
                tuned.headDamping = damping;

                taikor::TaikoEngine engine;
                engine.setParameters (tuned);
                engine.prepare (48000.0, 64);
                engine.trigger (articulation, 0, 0.95f);

                using Access = taikor::TaikoEngineTestAccess;
                const auto atStart = Access::activeModeCount (engine);
                const auto lastContact = Access::lastContactEnd (engine);
                expect (lastContact > 0u,
                        "a stroke must schedule a contact");

                render (engine, static_cast<int> (lastContact), 64);
                const auto atLastContact = Access::activeModeCount (engine);

                expect (atLastContact == atStart,
                        std::string (taikor::getArticulationDisplayName (articulation))
                            + " lost modes before its contact finished");
            }
        }
    }

    // The airborne delay line must reach across the largest drum the controls
    // can produce at the highest supported rate. If the line is too short the
    // delays clamp, two different strike positions collapse onto the same
    // arrival time, and the cue that places a stroke across the image stops
    // working. The clamp is checked directly: the membrane modes reach both
    // microphones instantaneously here, so an onset measured from the rendered
    // audio is dominated by them and cannot see the airborne path at all.
    {
        auto extreme = parameters;
        extreme.humanise = 0.0f;
        extreme.headDiameter = 1.20f;   // the largest head
        extreme.octaveBody = 1.0f;      // realised as size, so two octaves down
        extreme.micSpread = 1.0f;       // microphones fully opened
        extreme.micDistance = 1.0f;

        const auto delaysAt = [&extreme] (float position, float& left, float& right)
        {
            auto tuned = extreme;
            tuned.strikePosition = position;

            taikor::TaikoEngine engine;
            engine.setParameters (tuned);
            engine.prepare (384000.0, 256);
            engine.trigger (taikor::Articulation::Ka, taikor::lowestOctaveOffset,
                            0.95f);
            taikor::TaikoEngineTestAccess::directDelays (engine, left, right);
        };

        float rimLeft = 0.0f, rimRight = 0.0f;
        float centreLeft = 0.0f, centreRight = 0.0f;
        delaysAt (1.0f, rimLeft, rimRight);
        delaysAt (-1.0f, centreLeft, centreRight);

        const auto ceiling = taikor::TaikoEngineTestAccess::delayCeiling;
        for (const float delay : { rimLeft, rimRight, centreLeft, centreRight })
            expect (delay < ceiling,
                    "an airborne path delay reached the end of the delay line, so "
                    "the line is too short for the geometry the controls allow");

        expect (std::abs ((rimLeft - rimRight) - (centreLeft - centreRight)) > 1.0f,
                "two strike positions resolved to the same inter-microphone delay");
    }

    // The airborne delay line's fractional read, checked exactly. A ramp makes
    // the correct answer arithmetic: reading it back at delay d must return the
    // ramp's value d samples ago, to within interpolation error. Reading the
    // wrong neighbour adds the fractional part instead of subtracting it, which
    // this catches at every non-integer delay.
    {
        using Access = taikor::TaikoEngineTestAccess;
        std::array<float, Access::lineSize> line {};
        constexpr int writeIndex = 600;
        for (int index = 0; index < Access::lineSize; ++index)
            line[static_cast<std::size_t> (index)] = static_cast<float> (index);

        for (const float delay : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 7.5f,
                                   19.75f, 63.0f, 100.5f })
        {
            const auto value = Access::readDelayLine (line, writeIndex, delay);
            const auto expected = static_cast<float> (writeIndex) - delay;
            expect (std::abs (value - expected) < 1.0e-3f,
                    "the airborne delay line read " + std::to_string (value)
                        + " where a delay of " + std::to_string (delay)
                        + " should have returned " + std::to_string (expected));
        }
    }

    // The airborne path is what places a stroke across the image, and it
    // carries a real time difference. An off-centre stroke must therefore reach
    // the nearer microphone first.
    {
        auto tuned = parameters;
        tuned.humanise = 0.0f;
        tuned.micSpread = 1.0f;
        tuned.micDistance = 0.0f;
        tuned.strikePosition = 1.0f;

        const auto rendered =
            strike (tuned, taikor::Articulation::Ka, 0, 0.95f, 48000.0, 4000, 64);

        const auto onset = [] (const std::vector<float>& samples)
        {
            double peak = 0.0;
            for (const float value : samples)
                peak = std::max (peak, std::abs (static_cast<double> (value)));
            for (std::size_t index = 0; index < samples.size(); ++index)
                if (std::abs (static_cast<double> (samples[index])) > 0.25 * peak)
                    return static_cast<double> (index);
            return 0.0;
        };

        const auto separation = std::abs (onset (rendered.left) - onset (rendered.right));
        expect (separation > 1.0,
                "an off-centre stroke must reach the two microphones at different times");
        // A path difference cannot exceed the drum crossed at the speed of
        // sound; anything larger means the delay line is being read wrongly.
        expect (separation < 0.005 * 48000.0,
                "the inter-microphone delay is longer than the drum is wide");
    }
}

} // namespace

int main()
{
    testArticulationMetadataAndMidiMapping();
    testOctavesRaisePitch();
    testTheGridIsFourByFourAndTheRestIsSilent();
    testTheFourDrumsAreFourInstruments();
    testTheFourStrokesAreMutuallyDistinct();
    testTheCavityIsAColumnNotAnInfiniteSpring();
    testTheBodyMovesWithThePair();
    testTheEnclosedAirIsLosslessOnlyWhereThatIsInaudible();
    testTheContactPatchWouldNotBeAudibleOnTheResolvedBank();
    testTheDrumIsTunedByThePitchItSounds();
    testTheFourDrumsStepInHeardOctaves();
    testThePitchTransformIsContinuousUnderAutomation();
    testTheReadoutFollowsTheStrikePosition();
    testTheReadoutNamesThePartialTheDrumIsHeardAt();
    testTheReadoutIsAlwaysAFrequency();
    testTheReadoutNamesAPartialTheRendererBuilds();
    testDrumLayoutHasOnlyPhysicalEndpoints();
    testEveryArticulationAndSampleRate();
    testSampleRateConsistency();
    testContinuumFollowsModalImpulseDisplacement();
    testTheContinuumDoesNotDependOnTheSampleRate();
    testContinuumFollowsActualForceHistory();
    testContactRoughnessDoesNotBecomeAnAirborneNoiseShelf();
    testContinuumDistanceFollowsItsWavelength();
    testContinuumDistanceMovesARingingTail();
    testContinuumDistanceMovesAPendingContact();
    testStructuralAutomationPreservesTwoHeadState();
    testContinuumBandsOwnTheirOctaves();
    testTheGlideDoesNotBrightenTheTopOfTheSpectrum();
    testVelocitySensitivity();
    testVelocityCurveShapesImpactSpeed();
    testPhysicalParameterInfluence();
    testStrikePositionShapesTheSpectrum();
    testStrikePositionHasNoDeadTravel();
    testExplicitPolarStrikeControl();
    testZeroAzimuthObservesTheRenderedSplitBranch();
    testHumaniseScattersInHeadCoordinates();
    testPerformerIdentityMakesRealEnsembles();
    testCloseMicrophonePair();
    testTailsTerminateAndVoicesRetire();
    testVoiceStealingStaysBounded();
    testTheDrumSoundsLikeADrumAndNotLikeATone();
    testOnlyTheHoopStrikeDrivesTheShell();
    testShellResonanceHasNoStepInIt();
    testCollisionChangesVelocityNotDisplacement();
    testCollisionRetentionFallsBackOnACollapsedPole();
    testMutedStrokeChokesTheRingingHead();
    testHandControllerIsAPhysicalPalm();
    testAStrokeLandsOnAHeadThatIsAlreadyMoving();
    testTheTackLineRattlesOnlyWhenItIsBeaten();
    testTheDynamicRangeReachesFromAGhostStrokeToAFullBlow();
    testTheAttackGlideComesFromTheHead();
    testAttackGlideChangesOnlyBatterTension();
    testHeadStiffnessOpensTheModalRatios();
    testTheContinuumFollowsTheHead();
    testEveryParameterSurvivesTheCache();
    testContinuumRetuningPreservesCalibratedPower();
    testContinuumVarianceCacheLifecycle();
    testRadiationEfficiencyStaysFinite();
    testFiniteHeadSeparationRadiatesADipole();
    testReleasedMultipolesKeepTheirNodalAzimuth();
    testPhaseAwareResolvedObservationArchitecture();
    testReportedModesAreActuallySounded();
    testStrokesShareOnePhysicalDrumState();
    testPassiveNonlinearContact();
    testHertzReferencePulsePreservesCollisionImpulse();
    testContinuumBoundsLegacyResidualExposurePerContact();
    testSimultaneousStrokesDoNotShareOneVoice();
    testDeterminismAndBlockPartitioning();
    testSuccessiveStrokesVaryPhysically();
    testPerformanceControls();
    testSanitiseClampsEveryField();
    testParametersForOctaveIsIdentityAtBothOfItsOwnEndpoints();
    testContactCollisionMassFallsBackWhenTheMembraneContributesNothing();
    testColumnStiffnessFactorGuardsItsOwnDomain();
    testStiffnessStretchGuardsItsOwnDomain();
    testMountingLossGuardsItsOwnDomain();
    testContinuumBandVarianceGuardsItsOwnDomain();
    testInvalidInputSafety();
    testUiPresentationMath();
    testControlEndpointsAndGestures();
    testIdleCostAndStressPerformance();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " Taikor DSP test(s) failed\n";
        return 1;
    }
    std::cout << "All Taikor DSP tests passed\n";
    return 0;
}
