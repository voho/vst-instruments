// Circuit-reference suite.
//
// Evidence-backed checks compare the realtime model against an independent
// numerical solve, closed-form result, firmware vector or service-document
// figure. Explicitly named compatibility profiles also carry broad safety and
// monotonicity regressions; those are product invariants, not hardware claims.

#include "DSP/YouKnow106Chorus.h"
#include "DSP/YouKnow106Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace youknow106
{
// Narrow seam into the private blocks. Not part of the plug-in API.
struct YouKnow106TestAccess
{
    using Cascade = YouKnow106Engine::OtaCascade;

    static ChorusMode runningChorusMode(const Chorus& chorus) noexcept
    {
        return chorus.runningMode_;
    }

    static constexpr float headroom() noexcept
    {
        return YouKnow106Engine::otaHeadroomVolts;
    }

    static constexpr float feedbackHeadroom() noexcept
    {
        return YouKnow106Engine::VoicedResonanceCompatibilityProfile::
            loopHeadroomVolts;
    }

    static constexpr float earlyEffectCoefficient() noexcept
    {
        return YouKnow106Engine::otaEarlyEffectCoefficient;
    }

    static constexpr float pwmFirstPoleSeconds() noexcept
    {
        return YouKnow106Engine::pwmHoldFirstPoleSeconds;
    }

    static constexpr float pwmSecondPoleSeconds() noexcept
    {
        return YouKnow106Engine::pwmHoldSecondPoleSeconds;
    }

    static constexpr float subHoldSlewSeconds() noexcept
    {
        return YouKnow106Engine::subHoldSlewSeconds;
    }

    static constexpr float voiceVcaHoldSlewSeconds() noexcept
    {
        return YouKnow106Engine::voiceVcaHoldSlewSeconds;
    }

    static std::vector<float> renderCascade(const std::vector<float>& input,
                                            float omegaStep, float feedback)
    {
        Cascade cascade;
        cascade.reset();
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = cascade.process(
                input[index], omegaStep, feedback);
        return output;
    }

    static std::array<float, 4> cascadeVoltagesAfterRetime(
        const std::array<float, 4>& voltage,
        float previousOmegaStep, float nextOmegaStep) noexcept
    {
        Cascade cascade;
        for (std::size_t stage = 0; stage < voltage.size(); ++stage)
            cascade.state[stage] = voltage[stage];
        cascade.retime(previousOmegaStep, nextOmegaStep);
        std::array<float, 4> result {};
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = static_cast<float>(cascade.state[stage]);
        return result;
    }

    static float cascadeRetimeStepError(float previousOmegaStep,
                                        float nextOmegaStep)
    {
        // Settle under a DC drive at one grid, change grids, and report how
        // far the next output steps from the settled value.
        Cascade cascade;
        cascade.reset();
        float settled = 0.0f;
        for (int index = 0; index < 20000; ++index)
            settled = cascade.process(0.5f, previousOmegaStep, 0.0f);
        cascade.retime(previousOmegaStep, nextOmegaStep);
        return std::abs(
            cascade.process(0.5f, nextOmegaStep, 0.0f) - settled);
    }

    static float cascadeIdentityRetimeError()
    {
        // Two cascades share a transient history; one takes an identity
        // retime mid-ring. If the carried states fully describe the filter,
        // the pair must stay bit-identical afterwards.
        Cascade a;
        Cascade b;
        a.reset();
        b.reset();
        constexpr float omegaStep = 0.05f;
        for (int index = 0; index < 64; ++index)
        {
            const float x = index == 0 ? 1.0f : 0.0f;
            a.process(x, omegaStep, 3.0f);
            b.process(x, omegaStep, 3.0f);
        }
        b.retime(omegaStep, omegaStep);
        float worst = 0.0f;
        for (int index = 0; index < 256; ++index)
            worst = std::max(worst,
                             std::abs(a.process(0.0f, omegaStep, 3.0f)
                                      - b.process(0.0f, omegaStep, 3.0f)));
        return worst;
    }

    // The decimation kernel as built, so the suite can measure the transfer
    // the last stage actually applies rather than trusting a design formula.
    static std::vector<float> halfbandKernel()
    {
        YouKnow106Engine engine;
        engine.buildHalfbandKernel();
        return { engine.halfbandKernel_.begin(), engine.halfbandKernel_.end() };
    }

    // The IR3109's own internal input divider and the differential pair's
    // linear span it implies. Private on the engine because nothing outside it
    // needs them; the suite needs them to check the drive budget.
    static constexpr float stageAttenuation() noexcept
    {
        return YouKnow106Engine::stageAttenuation;
    }

    static constexpr float otaHeadroomVolts() noexcept
    {
        return YouKnow106Engine::otaHeadroomVolts;
    }

    static float bbdTransfer(float input) noexcept
    {
        return Chorus::bbdTransfer(input);
    }

    static float transferLossStep(float& state, float input) noexcept
    {
        return Chorus::transferLossStep(state, input);
    }

    static float interpolateBbdInput(float current, float previous,
                                     float previous2, float previous3,
                                     double ageInSamples) noexcept
    {
        return Chorus::interpolateBbdInput(
            current, previous, previous2, previous3, ageInSamples);
    }

    static double bbdPolyBlepResidual(double distanceInSamples) noexcept
    {
        return Chorus::bbdPolyBlepResidual(distanceInSamples);
    }

    struct BbdCorePhysicalState
    {
        std::array<float, Chorus::cellPairs> cells {};
        int writeIndex { 0 };
        double clockPhase { 0.0 };
        float held { 0.0f };
        float previousInput { 0.0f };
        float previousInput2 { 0.0f };
        float previousInput3 { 0.0f };
        float transferState { 0.0f };
        std::uint32_t noiseState { 0u };

        bool operator==(const BbdCorePhysicalState&) const = default;
    };

    static void configureBbdCore(
        Chorus& chorus,
        const std::array<float, Chorus::cellPairs>& cells,
        int writeIndex, double clockPhase, float held,
        float previousInput, float transferState,
        std::uint32_t noiseState) noexcept
    {
        auto& line = chorus.lineA_;
        line.cells = cells;
        line.writeIndex = writeIndex;
        line.clockPhase = clockPhase;
        line.held = held;
        line.previousInput = previousInput;
        line.previousInput2 = previousInput;
        line.previousInput3 = previousInput;
        line.transferState = transferState;
        line.noiseState = noiseState;
        line.pastBlepEvents.fill({});
        line.pastBlepEventCount = 0;
    }

    static float processBbdCore(Chorus& chorus, float input,
                                float clockHz, float sampleRate,
                                float noiseScale) noexcept
    {
        return chorus.lineA_.processClockedCore(
            input, clockHz, sampleRate, noiseScale);
    }

    static BbdCorePhysicalState bbdCorePhysicalState(
        const Chorus& chorus) noexcept
    {
        const auto& line = chorus.lineA_;
        return { line.cells, line.writeIndex, line.clockPhase, line.held,
                 line.previousInput, line.previousInput2, line.previousInput3,
                 line.transferState, line.noiseState };
    }

    static void setBbdInputHistory(Chorus& chorus, float previous,
                                   float previous2, float previous3) noexcept
    {
        chorus.lineA_.previousInput = previous;
        chorus.lineA_.previousInput2 = previous2;
        chorus.lineA_.previousInput3 = previous3;
    }

    static std::array<float, 3> bbdInputHistory(
        const Chorus& chorus) noexcept
    {
        return { chorus.lineA_.previousInput,
                 chorus.lineA_.previousInput2,
                 chorus.lineA_.previousInput3 };
    }

    static double bbdCoreCorrection(const Chorus& chorus,
                                    double clockIncrement) noexcept
    {
        return chorus.lineA_.deterministicBlepCorrection(clockIncrement);
    }

    static int bbdBlepEventCount(const Chorus& chorus) noexcept
    {
        return chorus.lineA_.pastBlepEventCount;
    }

    static float bbdRawHeld(const Chorus& chorus) noexcept
    {
        return chorus.lineA_.held;
    }

    static float processBbdFullLine(Chorus& chorus, float input,
                                    float clockHz, bool useBlep) noexcept
    {
        auto& line = chorus.lineA_;
        const auto& support = chorus.support_;
        // Both sides use the exact same physical support. The seam changes
        // only whether its output side sees the BLEP-corrected or raw held
        // BBD staircase, so the comparison cannot accidentally measure two
        // different filter implementations. The input side is shared by the
        // two branches and belongs to the Chorus, so drive it here exactly as
        // Chorus::process does.
        const float limited = chorus.advanceInputSupport(input);
        return line.process(limited, clockHz, chorus.sampleRate_,
                            support.exactOutputConnected, 0.0f, useBlep);
    }

    static float processBbdExactMode(Chorus& chorus, float input,
                                     float clockHz,
                                     bool wetConnected) noexcept
    {
        auto& line = chorus.lineA_;
        const auto& support = chorus.support_;
        const auto& transition = wetConnected
            ? support.exactOutputConnected : support.exactOutputMuted;
        const float limited = chorus.advanceInputSupport(input);
        return line.process(limited, clockHz, chorus.sampleRate_,
                            transition, 0.0f, true);
    }

    static const std::array<double, 6>& chorusExactInputState(
        const Chorus& chorus) noexcept
    {
        return chorus.inputSupport_.exactState;
    }

    static const std::array<double, 6>& chorusExactOutputState(
        const Chorus& chorus) noexcept
    {
        return chorus.lineA_.exactOutputState;
    }

    static bool chorusExactHistoriesAreFinite(
        const Chorus& chorus) noexcept
    {
        const auto& line = chorus.lineA_;
        return std::isfinite(chorus.inputSupport_.exactPrevious)
            && std::isfinite(chorus.inputSupport_.exactPrevious2)
            && std::isfinite(chorus.inputSupport_.exactPrevious3)
            && std::isfinite(line.exactOutputPrevious)
            && std::isfinite(line.exactOutputPrevious2)
            && std::isfinite(line.exactOutputPrevious3);
    }

    static std::array<float, 2> correlatedChorusNoiseStep(
        std::uint32_t& commonState, std::uint32_t& orthogonalState,
        float correlation) noexcept
    {
        const auto sample = Chorus::correlatedRandomStep(
            commonState, orthogonalState, correlation);
        return { sample.lineA, sample.lineB };
    }

    static float chorusToneStep(double& phase, float frequencyHz,
                                float sampleRate) noexcept
    {
        return Chorus::deterministicToneStep(phase, frequencyHz, sampleRate);
    }

    static void configureOptionalChorusNoise(
        Chorus& chorus, float commonAmplitude, float correlation,
        float humAmplitude, float humFrequencyHz,
        float clockSpurAmplitude, float clockSpurHarmonic) noexcept
    {
        chorus.optionalNoise_.commonRandomAmplitude = commonAmplitude;
        chorus.optionalNoise_.commonRandomCorrelation = correlation;
        chorus.optionalNoise_.humAmplitude = humAmplitude;
        chorus.optionalNoise_.humFrequencyHz = humFrequencyHz;
        chorus.optionalNoise_.clockSpurAmplitude = clockSpurAmplitude;
        chorus.optionalNoise_.clockSpurHarmonic = clockSpurHarmonic;
    }

    static float chorusWetGain(const Chorus& chorus) noexcept
    {
        return chorus.wetGain_;
    }

    struct ChorusPhysicalState
    {
        std::array<float, Chorus::cellPairs> cellsA {};
        int writeIndexA { 0 };
        double clockPhaseA { 0.0 };
        float heldA { 0.0f };
        float transferStateA { 0.0f };
        std::uint32_t lineNoiseA { 0u };
        double lfoPhase { 0.0 };
        float wetGain { 0.0f };
        std::uint32_t commonNoise { 0u };
        std::uint32_t orthogonalNoise { 0u };
        double humPhase { 0.0 };
        double clockSpurPhaseA { 0.0 };
        bool primed { false };

        bool operator==(const ChorusPhysicalState&) const = default;
    };

    static ChorusPhysicalState chorusPhysicalState(
        const Chorus& chorus) noexcept
    {
        return { chorus.lineA_.cells,
                 chorus.lineA_.writeIndex,
                 chorus.lineA_.clockPhase,
                 chorus.lineA_.held,
                 chorus.lineA_.transferState,
                 chorus.lineA_.noiseState,
                 chorus.lfoPhase_,
                 chorus.wetGain_,
                 chorus.commonNoiseState_,
                 chorus.orthogonalNoiseState_,
                 chorus.humPhase_,
                 chorus.clockSpurPhaseA_,
                 chorus.primed_ };
    }

    static bool chorusAudioRateSupportIsClear(
        const Chorus& chorus) noexcept
    {
        const auto clear = [](const Chorus::Line& line) {
            const auto allZero = [](const auto& values) {
                return std::all_of(values.begin(), values.end(),
                    [](double value) { return value == 0.0; });
            };
            return line.previousInput == 0.0f
                && line.previousInput2 == 0.0f
                && line.previousInput3 == 0.0f
                && allZero(line.exactOutputState)
                && line.exactOutputPrevious == 0.0
                && line.exactOutputPrevious2 == 0.0
                && line.exactOutputPrevious3 == 0.0
                && line.pastBlepEventCount == 0;
        };
        const auto& shared = chorus.inputSupport_;
        const auto sharedIsClear = shared.couplingState == 0.0f
            && shared.passiveState == 0.0f
            && shared.antiAliasFirst.s1 == 0.0f
            && shared.antiAliasFirst.s2 == 0.0f
            && shared.antiAliasSecond.s1 == 0.0f
            && shared.antiAliasSecond.s2 == 0.0f
            && std::all_of(shared.exactState.begin(), shared.exactState.end(),
                           [](double value) { return value == 0.0; })
            && shared.exactPrevious == 0.0
            && shared.exactPrevious2 == 0.0
            && shared.exactPrevious3 == 0.0;
        return sharedIsClear && clear(chorus.lineA_) && clear(chorus.lineB_);
    }

    static std::vector<float> renderOutputCoupling(
        const std::vector<float>& input, double sampleRate)
    {
        YouKnow106Engine::HighPass coupling;
        coupling.reset();
        const float g = std::tan(
            static_cast<float>(3.14159265358979323846)
            * YouKnow106Engine::outputCouplingCornerHz()
            / static_cast<float>(sampleRate));
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = coupling.process(
                input[index], g, 0.0f,
                YouKnow106Engine::outputCouplingHighGain());
        return output;
    }

    static std::vector<float> renderLoadedOutputCoupling(
        const std::vector<float>& input, double sampleRate,
        float volumePosition)
    {
        YouKnow106Engine::HighPass coupling;
        coupling.reset();
        const float g = std::tan(
            static_cast<float>(3.14159265358979323846)
            * YouKnow106Engine::outputCouplingCornerHz(volumePosition)
            / static_cast<float>(sampleRate));
        const float gain =
            YouKnow106Engine::outputCouplingHighGain(volumePosition);
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = coupling.process(input[index], g, 0.0f, gain);
        return output;
    }

    static std::vector<float> renderVoiceBusCoupling(
        const std::vector<float>& input, double sampleRate)
    {
        YouKnow106Engine::HighPass coupling;
        coupling.reset();
        const float g = std::tan(
            static_cast<float>(3.14159265358979323846)
            * YouKnow106Engine::voiceBusCouplingCornerHz()
            / static_cast<float>(sampleRate));
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = coupling.process(input[index], g, 0.0f, 1.0f);
        return output;
    }

    static std::vector<float> renderVoiceBusCoupling(
        const std::vector<float>& input, double sampleRate,
        HighPassMode mode)
    {
        YouKnow106Engine::HighPass coupling;
        coupling.reset();
        const float g = std::tan(
            static_cast<float>(3.14159265358979323846)
            * YouKnow106Engine::voiceBusCouplingCornerHz(mode)
            / static_cast<float>(sampleRate));
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = coupling.process(input[index], g, 0.0f, 1.0f);
        return output;
    }

    static std::vector<float> renderVoiceBusCouplingInBlocks(
        const std::vector<float>& input, double sampleRate,
        HighPassMode mode, std::size_t blockSize)
    {
        YouKnow106Engine engine;
        engine.prepare(sampleRate, 256, false);
        EngineParameters parameters;
        parameters.highPass = mode;
        blockSize = std::max<std::size_t>(1, blockSize);

        std::vector<float> output(input.size());
        for (std::size_t begin = 0; begin < input.size(); begin += blockSize)
        {
            engine.updateSharedHighPass(parameters);
            const std::size_t end = std::min(input.size(), begin + blockSize);
            for (std::size_t index = begin; index < end; ++index)
                output[index] = engine.voiceBusCoupling_.process(
                    input[index], engine.voiceBusCouplingG_, 0.0f, 1.0f);
        }
        return output;
    }

    struct VoiceBusCouplingStateContract
    {
        double beforeModeChange {};
        double afterModeChange {};
        double afterRateChange {};
        double afterPreservingClear {};
        double afterHardReset {};
    };

    static VoiceBusCouplingStateContract voiceBusCouplingStateContract()
    {
        YouKnow106Engine engine;
        engine.prepare(48000.0, 64, true);
        engine.voiceBusCoupling_.state = 0.375;

        VoiceBusCouplingStateContract result;
        result.beforeModeChange = engine.voiceBusCoupling_.state;
        EngineParameters parameters;
        parameters.highPass = HighPassMode::Two;
        engine.setParameters(parameters);
        engine.updateSharedHighPass(engine.activeParameters_);
        result.afterModeChange = engine.voiceBusCoupling_.state;

        engine.oversamplingApplied_ = 1;
        engine.updateProcessingRate(true);
        engine.updateSharedHighPass(parameters);
        result.afterRateChange = engine.voiceBusCoupling_.state;
        engine.clearRateDependentOutputPath(true);
        result.afterPreservingClear = engine.voiceBusCoupling_.state;
        engine.reset();
        result.afterHardReset = engine.voiceBusCoupling_.state;
        return result;
    }

    static float noiseSourceLowPassG(
        const YouKnow106Engine& engine) noexcept
    {
        return engine.noiseSourceLowPassG_;
    }

    static double noiseSourceProcessingRate(
        const YouKnow106Engine& engine) noexcept
    {
        return engine.oversampledRate_;
    }

    static double outputSlewVoltsPerSecond(
        const YouKnow106Engine& engine) noexcept
    {
        return static_cast<double>(
                   engine.processingCoefficients_.outputSlewMaxStep)
             * engine.oversampledRate_
             * YouKnow106Engine::internalVoltsPerUnit;
    }

    static std::array<double, 2> noiseSourceSupportState(
        const YouKnow106Engine& engine) noexcept
    {
        return { engine.noiseSourceHighPass_.state,
                 engine.noiseSourceLowPass_.state };
    }

    static void setNoiseSourceSupportState(
        YouKnow106Engine& engine, double highPass, double lowPass) noexcept
    {
        engine.noiseSourceHighPass_.state = highPass;
        engine.noiseSourceLowPass_.state = lowPass;
    }

    static float processNoiseSourceSupport(
        YouKnow106Engine& engine, float input) noexcept
    {
        return engine.noiseSourceLowPass_.process(
            engine.noiseSourceHighPass_.process(
                input, engine.noiseSourceHighPassG_, 0.0f, 1.0f),
            engine.noiseSourceLowPassG_, 1.0f, 0.0f);
    }

    static float processMainNoiseSource(
        YouKnow106Engine& engine, float input, float level,
        bool levelBeforeC41) noexcept
    {
        return engine.processMainNoiseSource(
            input, level, levelBeforeC41);
    }

    static float noiseHeld(const YouKnow106Engine& engine) noexcept
    {
        return engine.noiseCv_;
    }

    // HighPass::process's own non-finite-state guard: every one of the
    // engine's HighPass instances (voiceBusCoupling_, highPass_,
    // commonVcaInputCoupling_, both output-coupling filters, each voice's
    // moduleCoupling/vcaInputCoupling, and noiseSourceHighPass_) starts at
    // state 0.0 and is only ever updated by the filter's own finite
    // arithmetic, so the guard has never actually fired outside a test.
    // Build a standalone filter with a poisoned state and call process()
    // directly rather than relying on an upstream sanitiser that does not
    // know this guard exists.
    static double highPassStateAfterProcess(
        double state, float input, float g, float shelfGain,
        float highGain) noexcept
    {
        YouKnow106Engine::HighPass filter;
        filter.state = state;
        filter.process(input, g, shelfGain, highGain);
        return filter.state;
    }

    // One poisoned call followed by one ordinary call on the *same* filter
    // object, so the second call's output depends on whether the guard
    // actually healed the state left behind by the first rather than on a
    // freshly reconstructed filter that was never poisoned to begin with.
    static float highPassOutputAfterPoisonedCallHeals(
        double poisonedState, float input, float g, float shelfGain,
        float highGain) noexcept
    {
        YouKnow106Engine::HighPass filter;
        filter.state = poisonedState;
        filter.process(input, g, shelfGain, highGain);
        return filter.process(input, g, shelfGain, highGain);
    }

    static void forceNoiseSourceProcessingQuality(
        YouKnow106Engine& engine, bool enabled) noexcept
    {
        engine.oversamplingRequested_ = enabled ? 4 : 1;
        engine.oversamplingApplied_ = engine.oversamplingRequested_;
        engine.updateProcessingRate(true);
        engine.clearRateDependentOutputPath(true);
    }

    static std::vector<float> renderCommonVcaInputCoupling(
        const std::vector<float>& input, double sampleRate)
    {
        YouKnow106Engine::HighPass coupling;
        coupling.reset();
        const float g = std::tan(
            static_cast<float>(3.14159265358979323846)
            * YouKnow106Engine::commonVcaInputCouplingCornerHz()
            / static_cast<float>(sampleRate));
        std::vector<float> output(input.size());
        for (std::size_t index = 0; index < input.size(); ++index)
            output[index] = coupling.process(input[index], g, 0.0f, 1.0f);
        return output;
    }

    static void setChorusWetGain(Chorus& chorus, float gain) noexcept
    {
        chorus.wetGain_ = gain;
    }

    static std::uint16_t attackLevelAfterRetrigger(std::uint16_t level,
                                                   std::uint16_t increment) noexcept
    {
        YouKnow106Engine::Envelope envelope;
        envelope.level = level;
        envelope.value = YouKnow106Engine::envelopeDacFraction(level);
        envelope.noteOn();
        envelope.tick(increment, 0xffffu, 0u, 0xffffu);
        return envelope.level;
    }
};
} // namespace youknow106

namespace
{
using namespace youknow106;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectNear(double actual, double expected, double tolerance,
                const std::string& message)
{
    if (!(std::abs(actual - expected) <= tolerance))
    {
        ++failures;
        std::cerr << "FAIL: " << message << " (got " << actual << ", expected "
                  << expected << " +/- " << tolerance << ")\n";
    }
}

constexpr double pi = 3.14159265358979323846;

// Independent transcription of the DAFx 2025 companion algorithm. The
// production residual uses Horner form; spelling the two polynomials out here
// and maintaining a separate edge/history loop makes this a correspondence
// test rather than an assertion of the implementation against itself.
double referenceBbdPolyBlep(double distance)
{
    if (!(distance >= 0.0) || distance >= 2.0)
        return 0.0;
    if (distance < 1.0)
        return 0.125 * distance * distance * distance * distance
             - distance * distance * distance / 3.0
             - 0.25 * distance * distance + distance - 0.5;
    return -distance * distance * distance * distance / 24.0
           + distance * distance * distance / 3.0
           - 11.0 * distance * distance / 12.0
           + distance - 1.0 / 3.0;
}

std::uint32_t referenceXorshift32(std::uint32_t state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

struct ReferenceBbdCore
{
    struct Event
    {
        float jump {};
        double age {};
    };

    std::array<float, Chorus::cellPairs> cells {};
    int writeIndex {};
    double clockPhase {};
    float held {};
    float previousInput {};
    float previousInput2 {};
    float previousInput3 {};
    float transferState {};
    std::uint32_t noiseState { 1u };
    std::vector<Event> pastEvents;

    float process(float input, float clockHz, float sampleRate)
    {
        std::size_t retained = 0;
        for (auto event : pastEvents)
        {
            event.age += 1.0;
            if (event.age < 2.0)
                pastEvents[retained++] = event;
        }
        pastEvents.resize(retained);

        const double increment = static_cast<double>(clockHz)
                               / static_cast<double>(sampleRate);
        clockPhase += increment;
        while (clockPhase >= 1.0)
        {
            clockPhase -= 1.0;
            const double age = clockPhase / increment;
            // Express the causal cubic on a coordinate whose zero is the
            // preceding sample: current is at u=1 and the three histories are
            // at u=0,-1,-2. Production uses age and a different association,
            // so this remains an independent transcription of the same
            // four-point Lagrange interpolant.
            const double u = 1.0 - age;
            const double currentWeight = u * (u + 1.0) * (u + 2.0) / 6.0;
            const double previousWeight =
                -(u - 1.0) * (u + 1.0) * (u + 2.0) / 2.0;
            const double previous2Weight =
                u * (u - 1.0) * (u + 2.0) / 2.0;
            const double previous3Weight =
                -u * (u - 1.0) * (u + 1.0) / 6.0;
            const float atEdge = static_cast<float>(
                currentWeight * static_cast<double>(input)
                + previousWeight * static_cast<double>(previousInput)
                + previous2Weight * static_cast<double>(previousInput2)
                + previous3Weight * static_cast<double>(previousInput3));
            const float bounded = YouKnow106TestAccess::bbdTransfer(atEdge);

            writeIndex = writeIndex + 1 < Chorus::cellPairs
                ? writeIndex + 1 : 0;
            const float emerging = cells[static_cast<std::size_t>(writeIndex)];
            cells[static_cast<std::size_t>(writeIndex)] = bounded;

            const float before = transferState;
            transferState += 0.8654743f * (emerging - transferState);
            pastEvents.push_back({ transferState - before, age });

            // Noise is disabled in correspondence renders, but the hardware
            // state advances at every edge irrespective of its level control.
            noiseState = referenceXorshift32(noiseState);
            held = transferState;
        }
        previousInput3 = previousInput2;
        previousInput2 = previousInput;
        previousInput = input;

        double correction = 0.0;
        for (const auto& event : pastEvents)
            correction += static_cast<double>(event.jump)
                        * referenceBbdPolyBlep(event.age);

        const double inverseIncrement = 1.0 / increment;
        double futureDistance = (1.0 - clockPhase) * inverseIncrement;
        float predicted = transferState;
        int futureIndex = writeIndex;
        while (futureDistance < 2.0)
        {
            futureIndex = futureIndex + 1 < Chorus::cellPairs
                ? futureIndex + 1 : 0;
            const float before = predicted;
            predicted += 0.8654743f
                       * (cells[static_cast<std::size_t>(futureIndex)]
                          - predicted);
            correction -= static_cast<double>(predicted - before)
                        * referenceBbdPolyBlep(futureDistance);
            futureDistance += inverseIncrement;
        }

        return held + static_cast<float>(correction);
    }
};

double sinusoidMagnitude(const std::vector<float>& signal, std::size_t start,
                         std::size_t length, double frequency,
                         double sampleRate)
{
    std::complex<double> accumulator {};
    for (std::size_t offset = 0; offset < length; ++offset)
    {
        const double phase = -2.0 * pi * frequency
                           * static_cast<double>(offset) / sampleRate;
        accumulator += static_cast<double>(signal[start + offset])
                     * std::exp(std::complex<double>(0.0, phase));
    }
    return 2.0 * std::abs(accumulator) / static_cast<double>(length);
}

double singleToneResidualRms(const std::vector<float>& signal,
                             std::size_t start, std::size_t length,
                             double frequency, double sampleRate)
{
    double mean = 0.0;
    double cosine = 0.0;
    double sine = 0.0;
    for (std::size_t offset = 0; offset < length; ++offset)
    {
        const double value = signal[start + offset];
        const double phase = 2.0 * pi * frequency
                           * static_cast<double>(offset) / sampleRate;
        mean += value;
        cosine += value * std::cos(phase);
        sine += value * std::sin(phase);
    }
    mean /= static_cast<double>(length);
    cosine *= 2.0 / static_cast<double>(length);
    sine *= 2.0 / static_cast<double>(length);

    double residualSquared = 0.0;
    for (std::size_t offset = 0; offset < length; ++offset)
    {
        const double phase = 2.0 * pi * frequency
                           * static_cast<double>(offset) / sampleRate;
        const double fitted = mean + cosine * std::cos(phase)
                                   + sine * std::sin(phase);
        const double error = static_cast<double>(signal[start + offset]) - fitted;
        residualSquared += error * error;
    }
    return std::sqrt(residualSquared / static_cast<double>(length));
}

double selectedToneResidualRms(const std::vector<float>& signal,
                               std::size_t start, std::size_t length,
                               const std::vector<double>& frequencies,
                               double sampleRate)
{
    std::vector<double> cosines(frequencies.size());
    std::vector<double> sines(frequencies.size());
    double mean = 0.0;
    for (std::size_t offset = 0; offset < length; ++offset)
    {
        const double value = signal[start + offset];
        mean += value;
        for (std::size_t tone = 0; tone < frequencies.size(); ++tone)
        {
            const double phase = 2.0 * pi * frequencies[tone]
                               * static_cast<double>(offset) / sampleRate;
            cosines[tone] += value * std::cos(phase);
            sines[tone] += value * std::sin(phase);
        }
    }
    mean /= static_cast<double>(length);
    for (std::size_t tone = 0; tone < frequencies.size(); ++tone)
    {
        cosines[tone] *= 2.0 / static_cast<double>(length);
        sines[tone] *= 2.0 / static_cast<double>(length);
    }

    double residualSquared = 0.0;
    for (std::size_t offset = 0; offset < length; ++offset)
    {
        double fitted = mean;
        for (std::size_t tone = 0; tone < frequencies.size(); ++tone)
        {
            const double phase = 2.0 * pi * frequencies[tone]
                               * static_cast<double>(offset) / sampleRate;
            fitted += cosines[tone] * std::cos(phase)
                    + sines[tone] * std::sin(phase);
        }
        const double error = static_cast<double>(signal[start + offset]) - fitted;
        residualSquared += error * error;
    }
    return std::sqrt(residualSquared / static_cast<double>(length));
}

// --------------------------------------------------------------------------
// Independent reference: the four transconductor stages integrated with a
// fourth-order Runge-Kutta step at 16x the model's rate by default, straight
// from
//     C dVn/dt = Ig(Vn) tanh((V(n-1) - Vn) / H),
//     V0 = input - k fb(V4).
// It evaluates the declared Early-effect term and the causal current-plus-past
// input polynomial directly, without sharing the production integrator. This
// checks the implementation's continuous ODE; it is not evidence that the
// compatibility-profile constants are measured original-unit transfers.
// --------------------------------------------------------------------------
std::vector<double> referenceCascade(const std::vector<double>& input, double sampleRate,
                                     double cutoffHz, double feedback,
                                     int oversample = 16,
                                     bool enableEarlyEffect = true,
                                     double calibration = 0.70)
{
    const double headroom = YouKnow106TestAccess::headroom();
    const double loopHeadroom = YouKnow106TestAccess::feedbackHeadroom();
    const double earlyCoefficient =
        YouKnow106TestAccess::earlyEffectCoefficient();
    const double omega = 2.0 * pi * cutoffHz;
    const double step = 1.0 / (sampleRate * oversample);

    std::array<double, 4> voltage {};
    std::vector<double> output(input.size());

    const auto derivative = [&](const std::array<double, 4>& state, double drive) {
        std::array<double, 4> slope {};
        double previous = drive
            - feedback * loopHeadroom * std::tanh(state[3] / loopHeadroom);
        for (int stage = 0; stage < 4; ++stage)
        {
            const double early = enableEarlyEffect
                ? 1.0 + earlyCoefficient * calibration
                    * std::tanh(state[static_cast<std::size_t>(stage)]
                                / headroom)
                : 1.0;
            slope[static_cast<std::size_t>(stage)] =
                omega * early * headroom
                * std::tanh((previous - state[static_cast<std::size_t>(stage)]) / headroom);
            previous = state[static_cast<std::size_t>(stage)];
        }
        return slope;
    };

    std::array<double, 3> inputHistory {};
    int inputHistoryCount = 0;
    for (std::size_t index = 0; index < input.size(); ++index)
    {
        const double target = input[index];
        const auto inputAt = [&](double position) {
            const double t = position;
            if (inputHistoryCount == 0)
                return t * target + (1.0 - t) * inputHistory[0];
            if (inputHistoryCount == 1)
                return 0.5 * t * (t + 1.0) * target
                    + (1.0 - t * t) * inputHistory[0]
                    + 0.5 * t * (t - 1.0) * inputHistory[1];
            return t * (t + 1.0) * (t + 2.0) / 6.0 * target
                - (t - 1.0) * (t + 1.0) * (t + 2.0) / 2.0
                    * inputHistory[0]
                + (t - 1.0) * t * (t + 2.0) / 2.0 * inputHistory[1]
                - (t - 1.0) * t * (t + 1.0) / 6.0 * inputHistory[2];
        };
        for (int sub = 0; sub < oversample; ++sub)
        {
            const double a = inputAt(sub / static_cast<double>(oversample));
            const double b = inputAt((sub + 0.5) / oversample);
            const double c = inputAt((sub + 1.0) / oversample);

            const auto k1 = derivative(voltage, a);
            std::array<double, 4> temp {};
            for (int n = 0; n < 4; ++n)
                temp[static_cast<std::size_t>(n)] = voltage[static_cast<std::size_t>(n)]
                    + 0.5 * step * k1[static_cast<std::size_t>(n)];
            const auto k2 = derivative(temp, b);
            for (int n = 0; n < 4; ++n)
                temp[static_cast<std::size_t>(n)] = voltage[static_cast<std::size_t>(n)]
                    + 0.5 * step * k2[static_cast<std::size_t>(n)];
            const auto k3 = derivative(temp, b);
            for (int n = 0; n < 4; ++n)
                temp[static_cast<std::size_t>(n)] = voltage[static_cast<std::size_t>(n)]
                    + step * k3[static_cast<std::size_t>(n)];
            const auto k4 = derivative(temp, c);

            for (int n = 0; n < 4; ++n)
                voltage[static_cast<std::size_t>(n)] +=
                    (step / 6.0) * (k1[static_cast<std::size_t>(n)]
                                    + 2.0 * k2[static_cast<std::size_t>(n)]
                                    + 2.0 * k3[static_cast<std::size_t>(n)]
                                    + k4[static_cast<std::size_t>(n)]);
        }
        inputHistory[2] = inputHistory[1];
        inputHistory[1] = inputHistory[0];
        inputHistory[0] = target;
        inputHistoryCount = std::min(inputHistoryCount + 1, 2);
        output[index] = voltage[3];
    }
    return output;
}

double steadyStatePeak(const std::vector<double>& signal)
{
    double peak = 0.0;
    for (std::size_t index = signal.size() * 3 / 4; index < signal.size(); ++index)
        peak = std::max(peak, std::abs(signal[index]));
    return peak;
}

double steadyStatePeak(const std::vector<float>& signal)
{
    double peak = 0.0;
    for (std::size_t index = signal.size() * 3 / 4; index < signal.size(); ++index)
        peak = std::max(peak, static_cast<double>(std::abs(signal[index])));
    return peak;
}

template <typename Actual, typename Reference>
double relativeRmsError(const std::vector<Actual>& actual,
                        const std::vector<Reference>& reference,
                        std::size_t start = 0)
{
    double errorSquared = 0.0;
    double referenceSquared = 0.0;
    for (std::size_t index = start; index < actual.size(); ++index)
    {
        const double wanted = static_cast<double>(reference[index]);
        const double error = static_cast<double>(actual[index]) - wanted;
        errorSquared += error * error;
        referenceSquared += wanted * wanted;
    }
    return std::sqrt(errorSquared / std::max(referenceSquared, 1.0e-30));
}

// --------------------------------------------------------------------------

void testCascadeAgainstReferenceSolve()
{
    constexpr double sampleRate = 192000.0;

    for (double cutoff : { 120.0, 900.0, 4200.0 })
    {
        for (double feedback : { 0.0, 2.0, 3.0, 3.8 })
        {
            // A resonant cascade takes about Q cycles to settle, and Q here is
            // 1/(4 - k). Measuring before it has settled would compare a
            // transient against a steady-state result.
            const int length = static_cast<int>(std::min(
                65536.0, std::max(8192.0, 90.0 * sampleRate / cutoff)));
            // Small enough that the differential pairs stay linear, so the
            // closed-form 1/(4 - k) result at cutoff also applies.
            const double amplitude = 1.0e-4;
            std::vector<double> referenceInput(static_cast<std::size_t>(length));
            std::vector<float> modelInput(static_cast<std::size_t>(length));
            for (int index = 0; index < length; ++index)
            {
                const double value = amplitude
                    * std::sin(2.0 * pi * cutoff * index / sampleRate);
                referenceInput[static_cast<std::size_t>(index)] = value;
                modelInput[static_cast<std::size_t>(index)] = static_cast<float>(value);
            }

            const auto reference =
                referenceCascade(referenceInput, sampleRate, cutoff, feedback);
            const float omegaStep = static_cast<float>(
                2.0 * pi * cutoff / sampleRate);
            const auto model = YouKnow106TestAccess::renderCascade(
                modelInput, omegaStep, static_cast<float>(feedback));

            const double referenceGain = steadyStatePeak(reference) / amplitude;
            const double modelGain = steadyStatePeak(model) / amplitude;
            const double theory = 1.0 / (4.0 - feedback);

            const std::string label = "cascade fc=" + std::to_string(cutoff)
                                    + " k=" + std::to_string(feedback);
            expectNear(20.0 * std::log10(referenceGain / theory), 0.0, 0.6,
                       label + ": reference solve does not meet the analytic result");
            expectNear(20.0 * std::log10(modelGain / referenceGain), 0.0, 0.6,
                       label + ": model disagrees with the reference solve");
        }
    }

    // Changing HQ changes dt, not a voice card's capacitor charge. A retime
    // must leave every physical voltage in place, a settled cascade must not
    // step when the grid changes, and an identity retime must be invisible.
    constexpr std::array voltage { 0.25f, -0.5f, 0.75f, -1.0f };
    const auto kept = YouKnow106TestAccess::cascadeVoltagesAfterRetime(
        voltage, 0.03125f, 0.125f);
    for (std::size_t stage = 0; stage < voltage.size(); ++stage)
        expectNear(kept[stage], voltage[stage], 0.0,
                   "HQ retiming disturbed OTA capacitor charge "
                       + std::to_string(stage));
    expect(YouKnow106TestAccess::cascadeRetimeStepError(0.03125f, 0.125f)
               < 1.0e-5f,
           "a settled cascade stepped when the numerical grid refined");
    expect(YouKnow106TestAccess::cascadeRetimeStepError(0.125f, 0.03125f)
               < 1.0e-5f,
           "a settled cascade stepped when the numerical grid coarsened");
    expect(YouKnow106TestAccess::cascadeIdentityRetimeError() == 0.0f,
           "an identity retime altered the cascade");
}

void testCascadeOscillationThreshold()
{
    constexpr double sampleRate = 192000.0;
    constexpr double cutoff = 500.0;
    const float omegaStep = static_cast<float>(
        2.0 * pi * cutoff / sampleRate);

    const auto ring = [&](float feedback) {
        std::vector<float> input(24000, 0.0f);
        input[0] = 0.5f;
        const auto output = YouKnow106TestAccess::renderCascade(
            input, omegaStep, feedback);
        double peak = 0.0;
        for (std::size_t index = output.size() - 4000; index < output.size(); ++index)
            peak = std::max(peak, static_cast<double>(std::abs(output[index])));
        return peak;
    };

    expect(ring(3.6f) < 1.0e-4,
           "four-pole cascade sustains below its oscillation threshold");
    expect(ring(4.3f) > 1.0e-3,
           "four-pole cascade does not oscillate above its threshold");

    // A cascade driven far past the threshold must still settle to a bounded
    // limit cycle rather than running away.
    const double hard = ring(8.0f);
    expect(std::isfinite(hard) && hard < 40.0,
           "cascade is unbounded when driven far past its threshold");
}

void testCascadeSurvivesAdversarialControl()
{
    // Sweeping the control voltage violently across the whole range at audio
    // rate is not musical, but automation can produce it and the bounded
    // integrator must not diverge or emit a non-finite sample.
    constexpr double sampleRate = 192000.0;
    constexpr float lowestOmegaStep = static_cast<float>(
        2.0 * pi * 5.0 / sampleRate);
    constexpr float highestOmegaStep = static_cast<float>(2.0 * pi * 0.45);
    std::vector<float> input(20000);
    for (std::size_t index = 0; index < input.size(); ++index)
        input[index] = 3.0f * std::sin(0.31f * static_cast<float>(index));

    YouKnow106TestAccess::Cascade cascade;
    cascade.reset();
    bool finite = true;
    for (std::size_t index = 0; index < input.size(); ++index)
    {
        const float omegaStep = index % 2 == 0
            ? lowestOmegaStep : highestOmegaStep;
        const float k = (index % 3 == 0) ? 0.0f : 4.4f;
        const float value = cascade.process(input[index], omegaStep, k);
        finite = finite && std::isfinite(value) && std::abs(value) < 1.0e4f;
    }
    expect(finite, "cascade diverged under an adversarial control sweep");
}

void testCascadeTracksHotContinuousReference()
{
    // A sampled render of an analogue nonlinear ODE can still contain folded
    // energy unless an explicit output-bandlimit is part of the experiment.
    // The honest gate here is therefore correspondence to a much finer solve
    // of the declared continuous ODE under the former foldback stress case.
    // Dedicated quality audits own spectral alias/off-mask admission.
    constexpr double sampleRate = 192000.0;
    constexpr double fundamental = 1046.502;
    constexpr double cutoff = 16000.0;
    constexpr float amplitude = 2.4f;
    constexpr float feedback = 3.8f;
    const float omegaStep = static_cast<float>(
        2.0 * pi * cutoff / sampleRate);

    constexpr std::size_t settle = std::size_t { 1 } << 12;
    constexpr std::size_t capture = std::size_t { 1 } << 14;
    const int harmonics = static_cast<int>(0.5 * sampleRate / fundamental);
    std::vector<double> referenceInput(settle + capture);
    std::vector<float> modelInput(settle + capture);
    for (std::size_t index = 0; index < modelInput.size(); ++index)
    {
        double sum = 0.0;
        const double phase =
            2.0 * pi * fundamental * static_cast<double>(index) / sampleRate;
        for (int h = 1; h <= harmonics; ++h)
            sum += std::sin(h * phase) / h;
        const double value = amplitude * sum * 2.0 / pi;
        referenceInput[index] = value;
        modelInput[index] = static_cast<float>(value);
    }
    const auto model = YouKnow106TestAccess::renderCascade(
        modelInput, omegaStep, feedback);
    const auto reference = referenceCascade(
        referenceInput, sampleRate, cutoff, feedback, 64);
    const double error = relativeRmsError(model, reference, settle);
    expect(std::isfinite(error) && error < 0.01,
           "hot resonant cascade misses the RK64 continuous-ODE reference");
}

void testNoteTimerLaw()
{
    // One reference divided by the B-2 firmware's interpolated integer count.
    // RANGE changes only the reference clock, so the paired count/CV law is
    // identical in all three positions.
    struct Case { DcoRange range; double clock; };
    const Case cases[] = { { DcoRange::Sixteen, 1.0e6 },
                           { DcoRange::Eight, 2.0e6 },
                           { DcoRange::Four, 4.0e6 } };

    // The compact generators deliberately do not embed Roland's 104-word ROM
    // tables. These public B-2 vectors pin the exact unsigned clamp and
    // truncating interpolation around their declared approximation: no more
    // than four timer counts or one CV code at an anchor, and exact at the
    // upper saturation/discontinuity where one count is most audible.
    struct PitchVector
    {
        std::uint16_t word;
        std::uint32_t divider;
        std::uint16_t cvCode;
        std::uint32_t dividerTolerance;
        std::uint16_t cvTolerance;
    };
    constexpr std::array vectors {
        PitchVector { 0x2fffu, 61550u,   32u, 4u, 1u },
        PitchVector { 0x3000u, 61550u,   32u, 4u, 1u },
        PitchVector { 0x83ffu,   480u, 4094u, 0u, 0u },
        PitchVector { 0x8400u,   479u, 4095u, 0u, 0u },
        PitchVector { 0x9600u,   169u, 4095u, 0u, 0u },
        PitchVector { 0x96ffu,   161u, 4095u, 0u, 0u },
        PitchVector { 0x9700u,   169u, 4095u, 0u, 0u },
    };
    for (const auto& vector : vectors)
    {
        const auto pair = YouKnow106Engine::dcoPitchPair(vector.word);
        expect(std::abs(static_cast<long long>(pair.divider)
                        - static_cast<long long>(vector.divider))
                   <= vector.dividerTolerance,
               "derived B-2 divider anchor left its declared error bound");
        expect(std::abs(static_cast<int>(pair.cvCode)
                        - static_cast<int>(vector.cvCode))
                   <= vector.cvTolerance,
               "derived B-2 CV anchor left its declared error bound");
    }
    expect(YouKnow106Engine::dcoPitchPair(0x0000u).divider
               == YouKnow106Engine::dcoPitchPair(0x2fffu).divider
               && YouKnow106Engine::dcoPitchPair(0xffffu).divider
                      == YouKnow106Engine::dcoPitchPair(0x9700u).divider,
           "the B-2 lower or upper coordinate clamp retained a fraction");
    expect(YouKnow106Engine::dcoPitchPair(0x96ffu).divider
               < YouKnow106Engine::dcoPitchPair(0x9700u).divider,
           "the real B-2 upper-clamp count discontinuity disappeared");

    for (const auto& item : cases)
    {
        expectNear(YouKnow106Engine::rangeClockHz(item.range), item.clock, 1.0,
                   "range divider clock");

        // Neutral B-2 starts at 0x1818 rather than turning MIDI frequency
        // directly into its nearest timer integer. That intentional offset
        // and the recovered interpolation leave the physical C2-C7 keyboard
        // about 0.2..3.7 cents sharp in the image; the derived anchors remain
        // within five cents while preserving one count across all ranges.
        const double octaves = std::log2(item.clock / 2.0e6);
        double worstCents = 0.0;
        for (int note = 36; note <= 96; ++note)
        {
            const double wanted = 440.0 * std::pow(2.0, (note - 69) / 12.0);
            const auto divider = YouKnow106Engine::dcoDivider(wanted);
            const auto pitchWord = static_cast<std::uint16_t>(
                (note << 8) + 0x1818);
            const auto pair = YouKnow106Engine::dcoPitchPair(pitchWord);
            const double produced =
                YouKnow106Engine::dcoQuantisedFrequency(divider, item.range);
            const double sounding = wanted * std::pow(2.0, octaves);
            expect(divider == pair.divider,
                   "the hertz adapter left the production 8.8 pitch grid");
            worstCents = std::max(worstCents,
                                  std::abs(1200.0 * std::log2(produced / sounding)));
        }
        expect(worstCents < 5.0,
               "the derived B-2 law left its five-cent keybed bound");
    }

    // B-2 clamps below coordinate 0x3000 to its first table entry rather than
    // using all 65535 counter states. In 16' the derived count therefore
    // floors near 16.25 Hz; lower positive requests do not transpose further.
    const auto floored = YouKnow106Engine::dcoDivider(8.0);
    expectNear(YouKnow106Engine::dcoQuantisedFrequency(floored, DcoRange::Sixteen),
               16.25, 0.02, "the 16' range does not use the B-2 low clamp");
    expect(YouKnow106Engine::dcoQuantisedFrequency(
               YouKnow106Engine::dcoDivider(4.0), DcoRange::Sixteen)
           == YouKnow106Engine::dcoQuantisedFrequency(floored, DcoRange::Sixteen),
           "asking for an impossible pitch keeps transposing downwards");

    // Every voice divides the same reference, so two voices asked for the same
    // pitch get exactly the same integer: no spread at all.
    expect(YouKnow106Engine::dcoDivider(261.63)
           == YouKnow106Engine::dcoDivider(261.63),
           "note timer is not deterministic");
}

void testNoteTimerDividerDefensiveGuard()
{
    // dcoDivider's only production call site (updateVoiceEnvelopeAndPitch)
    // is now only the circuit-test hertz adapter; production constructs the
    // firmware word directly. Pin its invalid-input guard independently of
    // B-2's narrower valid low-coordinate clamp: hostile values still return
    // a safe nonzero 16-bit count rather than reaching log2 or the PIT.
    constexpr auto maximumDivider = std::uint32_t { 65535u };
    expect(YouKnow106Engine::dcoDivider(0.0) == maximumDivider,
           "a zero frequency did not saturate to the widest divider");
    expect(YouKnow106Engine::dcoDivider(-440.0) == maximumDivider,
           "a negative frequency did not saturate to the widest divider");
    expect(YouKnow106Engine::dcoDivider(
               std::numeric_limits<double>::quiet_NaN()) == maximumDivider,
           "a NaN frequency did not saturate to the widest divider");
    expect(YouKnow106Engine::dcoDivider(
               std::numeric_limits<double>::infinity()) == maximumDivider,
           "a +infinity frequency did not saturate to the widest divider");
    expect(YouKnow106Engine::dcoDivider(
               -std::numeric_limits<double>::infinity()) == maximumDivider,
           "a -infinity frequency did not saturate to the widest divider");
}

void testCutoffControlLaw()
{
    // The instrument's own service anchor: converter code 6272 must self-
    // oscillate at 248 Hz, and code 6272 + 2286 two octaves above that.
    expectNear(YouKnow106Engine::vcfCutoffHz(6272.0f), 248.0, 1.0,
               "cutoff law misses the service calibration anchor");
    expectNear(YouKnow106Engine::vcfCutoffHz(6272.0f + 2286.0f), 992.0, 4.0,
               "cutoff law misses the second calibration anchor");

    // One octave is 1143 counts throughout the validated, uncapped range.
    for (float counts : { 0.0f, 2000.0f, 6000.0f, 9000.0f })
        expectNear(YouKnow106Engine::vcfCutoffHz(counts + 1143.0f)
                       / YouKnow106Engine::vcfCutoffHz(counts),
                   2.0, 0.01, "cutoff law is not 1143 counts per octave");

    expectNear(YouKnow106Engine::vcfCutoffHz(0.0f), 5.53, 0.01,
               "cutoff law base frequency");

    // The panel reads as a 0..127 byte driving the converter 128 counts at a
    // time, so the whole travel is 16256 counts.
    expectNear(YouKnow106Engine::vcfPanelCounts(1.0f), 16256.0, 0.5,
               "cutoff panel travel");
    expectNear(YouKnow106Engine::vcfPanelCounts(0.0f), 0.0, 0.5,
               "cutoff panel travel starts at zero");
    expect(YouKnow106Engine::vcfPanelCounts(0.5f)
               == YouKnow106Engine::vcfPanelCounts(0.503f),
           "cutoff panel position is not quantised to the converter's byte");

    // OQ-18 does not justify the former 24 kHz tanh knee or 52.2 kHz
    // asymptote. The default product policy is the unchanged exponential law
    // followed by a plainly named 50 kHz numerical safety cap.
    for (float counts : { 0.0f, 6272.0f, 9000.0f, 12000.0f, 13716.0f })
    {
        const double exponential = YouKnow106Engine::vcfBaseFrequencyHz
            * std::exp2(counts / YouKnow106Engine::vcfCountsPerOctave);
        expectNear(YouKnow106Engine::vcfCutoffHz(counts), exponential,
                   std::max(1.0e-4, exponential * 2.0e-6),
                   "50 kHz policy altered the validated exponential range");
    }

    float previousCutoff = YouKnow106Engine::vcfCutoffHz(0.0f);
    for (int counts = 1; counts <= 20000; ++counts)
    {
        const float cutoff = YouKnow106Engine::vcfCutoffHz(static_cast<float>(counts));
        expect(std::isfinite(cutoff) && cutoff >= previousCutoff,
               "default cutoff policy is not finite and monotone");
        expect(cutoff <= 50000.0f,
               "default cutoff policy exceeds its named 50 kHz safety cap");
        previousCutoff = cutoff;
    }
    expectNear(YouKnow106Engine::vcfCutoffHz(20000.0f), 50000.0, 1.0e-3,
               "default cutoff policy never reaches its 50 kHz safety cap");
    expectNear(YouKnow106Engine::vcfEffectiveCutoffHz(20000.0f, 8.0f),
               50000.0, 1.0e-3,
               "resonance trim escaped the final 50 kHz cutoff safety cap");

    // The transconductor's control-current saturation has to be invisible
    // through the musical range and only bend the top: what it replaced was a
    // single pole that pulled a 5 kHz cutoff 48 cents flat and a 16 kHz one by
    // 143. Below 2.7 kHz the correction is under five cents.
    for (float counts = 0.0f; counts <= 10100.0f; counts += 100.0f)
    {
        const float effective = YouKnow106Engine::vcfEffectiveCutoffHz(counts, 0.0f);
        const float law = YouKnow106Engine::vcfCutoffHz(counts);
        const double cents = 1200.0 * std::log2(effective / law);
        expect(cents <= 0.0 && cents > -5.0,
               "control-current saturation is not transparent below 2.7 kHz: "
                   + std::to_string(cents) + " cents at " + std::to_string(counts)
                   + " counts");
    }
    // And the saturation must never lift a cutoff, at any code.
    for (float counts = 0.0f; counts <= 20000.0f; counts += 50.0f)
        expect(YouKnow106Engine::vcfEffectiveCutoffHz(counts, 0.0f)
                   <= YouKnow106Engine::vcfCutoffHz(counts) + 1.0e-3f,
               "control-current saturation raised a cutoff above the anti-log law");

    // A measured code-to-frequency table for a real voice card, gain
    // calibrated so DAC 1568 reads the service manual's own 248 Hz anchor.
    // Third-party and not independently verified, so it is a comparison
    // fixture rather than a hardware assertion -- but the shipping law has to
    // stay inside a musically meaningful distance of it. The revision this
    // replaced was 143 cents flat at DAC 3328.
    struct MeasuredCutoff { float dacCode; double hertz; };
    constexpr std::array<MeasuredCutoff, 8> measured {{
        { 1024.0f, 67.2 },   { 1568.0f, 248.0 },   { 2560.0f, 2725.0 },
        { 2816.0f, 5048.0 }, { 3072.0f, 9297.0 },  { 3328.0f, 16779.0 },
        { 3584.0f, 27876.0 }, { 4064.0f, 50792.0 }
    }};
    for (const auto& point : measured)
    {
        const float counts = point.dacCode * YouKnow106Engine::vcfDacCountStep
                           + YouKnow106Engine::vcfConverterCarryCounts(
                                 point.dacCode * YouKnow106Engine::vcfDacCountStep);
        const double cents = 1200.0 * std::log2(
            YouKnow106Engine::vcfEffectiveCutoffHz(counts, 0.0f) / point.hertz);
        expect(std::abs(cents) < 45.0,
               "cutoff law is " + std::to_string(cents)
                   + " cents from the measured card at DAC "
                   + std::to_string(static_cast<int>(point.dacCode)));
    }

    // The R-2R ladder's major carry. A slow sweep crossing mid-scale steps by
    // roughly 23 cents, and the two smaller boundary errors sit either side of
    // it. An ideal ladder has none of this, so it is scaled by Unit Character
    // in the converter write rather than living in the law.
    constexpr float perCent = YouKnow106Engine::vcfCountsPerOctave / 1200.0f;
    expectNear(YouKnow106Engine::vcfConverterCarryCounts(0.0f), 0.0, 1.0e-6,
               "the converter carries an offset below its first bit boundary");
    expectNear(YouKnow106Engine::vcfConverterCarryCounts(8192.0f)
                   - YouKnow106Engine::vcfConverterCarryCounts(8188.0f),
               23.31 * perCent, 1.0e-4,
               "the major carry at DAC 2048 is not the measured 23.31 cents");
    expectNear(YouKnow106Engine::vcfConverterCarryCounts(4096.0f)
                   - YouKnow106Engine::vcfConverterCarryCounts(4092.0f),
               -4.64 * perCent, 1.0e-4,
               "the carry at DAC 1024 is not the measured -4.64 cents");
    expectNear(YouKnow106Engine::vcfConverterCarryCounts(12288.0f)
                   - YouKnow106Engine::vcfConverterCarryCounts(12284.0f),
               -4.48 * perCent, 1.0e-4,
               "the carry at DAC 3072 is not the measured -4.48 cents");
}

void testStoredControlDigitalVectors()
{
    // This firmware-verified byte -> work-word -> DAC path is independent of
    // whichever replaceable analogue resonance profile consumes the voltage.
    struct Vector
    {
        int byte;
        std::uint16_t alignedWord;
        std::uint16_t dacCode;
    };
    constexpr std::array<Vector, 3> vectors {{
        { 0,   0x0000u, 0x0000u },
        { 64,  0x2000u, 0x0800u },
        { 127, 0x3f80u, 0x0fe0u }
    }};
    for (const auto& vector : vectors)
    {
        const float position = static_cast<float>(vector.byte) / 127.0f;
        expect(YouKnow106Engine::storedControlAlignedWord(position)
                   == vector.alignedWord,
               "stored control has the wrong aligned work word at byte "
                   + std::to_string(vector.byte));
        expect(YouKnow106Engine::storedControlDacCode(position) == vector.dacCode,
               "stored control has the wrong physical DAC code at byte "
                   + std::to_string(vector.byte));
    }
}

void testResonanceLeavesTheCornerAloneBelowOscillation()
{
    using Profile = YouKnow106Engine::VoicedResonanceCompatibilityProfile;

    // The frequency correction exists to cancel the droop the cascade's own
    // limit cycle puts on its corner: a compressive nonlinearity inside an
    // integrator lowers that integrator's pole in proportion to its
    // first-harmonic gain. Below the oscillation threshold there is no limit
    // cycle, so there is no droop, so the correction has to be identically
    // one and the control law has to read the same frequency at every
    // resonance setting below it.
    //
    // The revision this replaced carried a quadratic in loop gain fitted to
    // the oscillating endpoint and applied everywhere, which made RESONANCE a
    // second, hidden CUTOFF slider: it lifted the corner by +8.76 cents at
    // panel 0.30, +32.24 at 0.50, +80.17 at 0.70 and +116.25 at 0.80, where
    // the cascade is not oscillating at all.
    for (const float counts : { 3840.0f, 6272.0f, 11520.0f })
    {
        const double reference = YouKnow106Engine::vcfEffectiveCutoffHz(
            counts, Profile::loopGain(0.0f));
        for (const float panel : { 0.00f, 0.30f, 0.50f, 0.70f, 0.80f })
        {
            const double hertz = YouKnow106Engine::vcfEffectiveCutoffHz(
                counts, Profile::loopGain(panel));
            expectNear(1200.0 * std::log2(hertz / reference), 0.0, 10.0,
                       "resonance panel " + std::to_string(panel)
                           + " moves the cutoff law at "
                           + std::to_string(counts) + " counts");
        }
    }

    // And at every panel byte the slider can actually take, not merely at the
    // five sampled above. Reported as the worst byte rather than one failure
    // per byte, because a fitted correction fails all of them at once.
    int worstByte = -1;
    double worstCents = 0.0;
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float panel = static_cast<float>(byte) / 127.0f;
        const float k = Profile::loopGain(panel);
        if (k > Profile::nominalOscillationFeedback)
            break;
        const double cents =
            1200.0 * std::log2(static_cast<double>(Profile::frequencyTrim(k)));
        if (std::abs(cents) > std::abs(worstCents))
        {
            worstCents = cents;
            worstByte = byte;
        }
    }
    expect(worstByte < 0,
           "the frequency correction is not identically one below the "
           "oscillation threshold: panel byte " + std::to_string(worstByte)
               + " lifts the corner by " + std::to_string(worstCents)
               + " cents");

    // Four identical one-poles carry 45 degrees and 1/sqrt(2) each at their
    // own corner, so the loop closes at a gain of exactly four: that is where
    // a limit cycle first exists and where the correction may first depart
    // from one. It has to be continuous across it, and it has to grow with
    // the limit cycle beyond it.
    expect(Profile::frequencyTrim(Profile::nominalOscillationFeedback) == 1.0f,
           "the frequency correction is already correcting at the oscillation "
           "threshold");
    expectNear(1200.0 * std::log2(static_cast<double>(Profile::frequencyTrim(
                   Profile::nominalOscillationFeedback + 1.0e-3f))),
               0.0, 1.0,
               "the frequency correction steps discontinuously as the cascade "
               "starts to oscillate");
    expect(Profile::frequencyTrim(Profile::maximumFeedback) > 1.05f,
           "the frequency correction does not cancel the droop at the loop "
           "gain the service trim sets");
}

void testCircuitDerivedResonanceProfile()
{
    using Voiced = YouKnow106Engine::VoicedResonanceCompatibilityProfile;
    using Derived = YouKnow106Engine::CircuitDerivedResonanceProfile;

    // The shipping shape is linear in the stored byte above one
    // emitter-junction drop of the drawn 0..+10 V control chain, sharing the
    // voiced profile's amplitude-anchored endpoint. OQ-09's measured family
    // still owns the final calibration, but this traced topology is the
    // stronger default prior than the retained compatibility voicing.
    expect(EngineParameters {}.useCircuitDerivedResonanceShape,
           "the default left the circuit-derived resonance shape");

    expectNear(Derived::controlFullScaleVolts, 10.0 * 4064.0 / 4096.0, 0.0,
               "the derived profile's full-scale voltage left the p. 8 "
               "0..+10 V branch at physical code 4064");
    expectNear(Derived::onsetTravel, 0.6 / 9.921875, 1.0e-7,
               "the derived profile's onset left one junction drop of the "
               "control span");
    expect(Derived::onsetTravel * 127.0f > 7.0f
               && Derived::onsetTravel * 127.0f < 9.0f,
           "the derived onset left the byte-8 region");

    expect(Derived::loopGain(0.0f) == 0.0f
               && Derived::loopGain(Derived::onsetTravel) == 0.0f,
           "the derived shape conducts below its junction onset");
    expectNear(Derived::loopGain(1.0f), Voiced::maximumFeedback, 0.0f,
               "the derived shape left the amplitude-anchored endpoint");
    float previous = -1.0f;
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float panel = static_cast<float>(byte) / 127.0f;
        const float gain = Derived::loopGain(panel);
        expect(std::isfinite(gain) && gain >= previous,
               "the derived resonance shape is not finite and monotone");
        previous = gain;
    }
    // Linearity above the onset: equal byte steps add equal loop gain.
    const float low = Derived::loopGain(0.25f);
    const float mid = Derived::loopGain(0.50f);
    const float high = Derived::loopGain(0.75f);
    expectNear(mid - low, high - mid, 1.0e-6,
               "the derived shape is not linear above its onset");
    // Its oscillation threshold lands at travel 0.895, independently close
    // to the voiced curve's 0.9 -- a convergence recorded, not enforced, so
    // the tolerance is the derivation's own, not the voiced constant's.
    const float threshold = Derived::onsetTravel
                          + (Voiced::nominalOscillationFeedback
                             / Voiced::maximumFeedback)
                                * (1.0f - Derived::onsetTravel);
    expect(threshold > 0.885f && threshold < 0.905f,
           "the derived shape's oscillation threshold left the 0.895 region");
    expectNear(Derived::loopGain(threshold),
               Voiced::nominalOscillationFeedback, 1.0e-5,
               "the derived shape's threshold no longer solves its own law");
}

void testVoicedResonanceCompatibilityProfile()
{
    using Profile = YouKnow106Engine::VoicedResonanceCompatibilityProfile;

    // These deliberately avoid treating today's voiced coefficients as
    // hardware anchors. A measured profile may replace every analogue number
    // while retaining this minimal realtime safety/shape contract.
    float previousLoopGain = -1.0f;
    float previousCompensation = -1.0f;
    float previousTrim = -1.0f;
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float panel = static_cast<float>(byte) / 127.0f;
        const float loopGain = Profile::loopGain(panel);
        const float compensation = Profile::inputCompensation(loopGain);
        const float trim = Profile::frequencyTrim(loopGain);

        expect(std::isfinite(loopGain) && loopGain >= previousLoopGain,
               "voiced resonance loop-gain profile is not finite and monotone");
        expect(std::isfinite(compensation)
                   && compensation >= previousCompensation,
               "voiced resonance compensation is not finite and monotone");
        expect(std::isfinite(trim) && trim >= previousTrim,
               "voiced resonance frequency correction is not finite and monotone");

        previousLoopGain = loopGain;
        previousCompensation = compensation;
        previousTrim = trim;
    }
    expect(previousLoopGain > Profile::loopGain(0.0f),
           "voiced resonance loop-gain profile does not respond to its control");
    expect(previousCompensation > Profile::inputCompensation(0.0f),
           "voiced resonance compensation does not respond to loop gain");
    expect(previousTrim > Profile::frequencyTrim(0.0f),
           "voiced resonance frequency correction does not respond to loop gain");
}

void testEnvelopeAndAmplifierLaws()
{
    // Hash-matched B-2 coefficient vectors. The compact generators in the
    // engine must reproduce these values without embedding a proprietary table.
    struct EnvelopeVector
    {
        int code;
        std::uint16_t attack;
        std::uint16_t fall;
        int attackPasses;
        int releasePasses;
    };
    constexpr std::array<EnvelopeVector, 3> vectors {{
        { 0,   16384u,  4096u,   1,    4 },
        { 64,    127u, 65276u, 129,  984 },
        { 127,    21u, 65524u, 781, 6083 }
    }};
    for (const auto& vector : vectors)
    {
        const float position = static_cast<float>(vector.code) / 127.0f;
        expect(YouKnow106Engine::envelopeAttackIncrement(position) == vector.attack,
               "B-2 attack coefficient vector mismatch at code "
                   + std::to_string(vector.code));
        expect(YouKnow106Engine::envelopeDecayReleaseMultiplier(position) == vector.fall,
               "B-2 decay/release coefficient vector mismatch at code "
                   + std::to_string(vector.code));

        std::uint16_t release = YouKnow106Engine::envelopePeak;
        int passes = 0;
        while (release != 0u && passes <= vector.releasePasses)
        {
            release = YouKnow106Engine::envelopeReleaseLevel(release, vector.fall);
            ++passes;
        }
        expect(passes == vector.releasePasses && release == 0u,
               "B-2 release-to-zero pass count mismatch at code "
                   + std::to_string(vector.code));
        expectNear(YouKnow106Engine::envelopeAttackSeconds(position),
                   vector.attackPasses * 0.0042, 1.0e-5,
                   "B-2 attack duration mismatch at code "
                       + std::to_string(vector.code));
        expectNear(YouKnow106Engine::envelopeReleaseSeconds(position),
                   vector.releasePasses * 0.0042, 1.0e-4,
                   "B-2 release duration mismatch at code "
                       + std::to_string(vector.code));
    }

    // The multiply helper intentionally omits the low-byte x low-byte term.
    // For this vector the complete 16x16 product would yield 0x0626, while
    // the B-2 helper yields 0x0625.
    expect(YouKnow106Engine::envelopeReleaseLevel(0x1234u, 0x5678u) == 0x0625u,
           "decay helper restored the intentionally omitted low-low product");
    expect(YouKnow106Engine::envelopeDecayLevel(
               0x1334u, 0x0100u, 0x5678u) == 0x0725u,
           "decay is not sustain plus the exact truncated distance product");
    expect(YouKnow106Engine::envelopeAttackLevel(0x3ff0u, 0x0020u) == 0x3fffu,
           "integer attack does not saturate at 14 bits");
    expectNear(YouKnow106Engine::envelopeDacFraction(0x0003u), 0.0, 0.0,
               "envelope low recurrence bits leaked into the physical DAC");
    expectNear(YouKnow106Engine::envelopeDacFraction(0x0004u), 1.0 / 4095.0,
               1.0e-9, "envelope DAC does not discard exactly two low bits");
    expectNear(YouKnow106Engine::envelopeDacFraction(0x3fffu), 1.0, 0.0,
               "envelope DAC does not reach full scale at the 14-bit peak");
    expect(YouKnow106TestAccess::attackLevelAfterRetrigger(0x1800u, 0x007fu)
               == 0x187fu,
           "retrigger clears the live envelope accumulator before attack");

    // Sustain is the stored byte shifted by seven, including the exact midpoint.
    expect(YouKnow106Engine::storedControlAlignedWord(0.0f) == 0u
           && YouKnow106Engine::storedControlAlignedWord(64.0f / 127.0f) == 0x2000u
           && YouKnow106Engine::storedControlAlignedWord(1.0f) == 0x3f80u,
           "sustain byte does not map to 0 / 0x2000 / 0x3f80");

    // Decay's UI convention remains time to -20 dB, but the reported value is
    // now obtained by iterating the exact integer recurrence.
    expectNear(YouKnow106Engine::envelopeDecaySeconds(0.0f), 0.0042, 1.0e-6,
               "fastest decay-to-minus-20-dB is not one pass");
    expectNear(YouKnow106Engine::envelopeDecaySeconds(64.0f / 127.0f),
               527.0 * 0.0042, 1.0e-4,
               "mid decay-to-minus-20-dB misses the integer recurrence");
    expectNear(YouKnow106Engine::envelopeDecaySeconds(1.0f),
               5137.0 * 0.0042, 1.0e-4,
               "slowest decay-to-minus-20-dB misses the integer recurrence");

    // The voice amplifier is a current-controlled OTA behind a grounded-base
    // volts-to-amps stage, so its gain is linear in the control voltage above
    // the turn-on with the transistor's own exponential knee below it. These
    // check that shape against the schematic rather than against a chosen
    // curve; the remaining open part of OQ-19 is a measured BA662 gain sweep,
    // which would fix the turn-on point, not the law.
    using VoiceVcaLaw = YouKnow106Engine::VoiceVcaControlLaw;
    float previousGain = -1.0f;
    for (int step = 0; step <= 1000; ++step)
    {
        const float gain = VoiceVcaLaw::gain(step / 1000.0f);
        expect(std::isfinite(gain) && gain >= 0.0f && gain >= previousGain,
               "the voice-VCA control law is not finite and monotone");
        previousGain = gain;
    }
    expectNear(VoiceVcaLaw::gain(1.0f), 1.0, 1.0e-6,
               "full control does not give the voice VCA unity gain");

    // Linear in control above the turn-on: equal control steps are equal gain
    // steps, which an exponential law cannot do. Checked well clear of the
    // knee, whose whole width is a couple of per cent of the span.
    for (float control = 0.1f; control <= 0.9f; control += 0.1f)
    {
        const double linear = (static_cast<double>(control) - VoiceVcaLaw::turnOn)
                            / (1.0 - VoiceVcaLaw::turnOn);
        expectNear(VoiceVcaLaw::gain(control), linear, 2.0e-6,
                   "the voice VCA is not linear in control above its turn-on");
    }

    // And exponential below it, at the grounded-base stage's own 60 mV per
    // decade -- kT/q times ln 10, referred to the converter's 10 V span. One
    // decade of control below the turn-on against two decades below it, where
    // the softplus is already close to its exponential asymptote.
    {
        const float decade = VoiceVcaLaw::knee * 2.302585f;
        const double upper = VoiceVcaLaw::gain(VoiceVcaLaw::turnOn - decade);
        const double lower = VoiceVcaLaw::gain(VoiceVcaLaw::turnOn - 2.0f * decade);
        expectNear(upper / lower, 10.0, 0.5,
                   "the voice VCA's low-level knee is not 60 mV per decade");
        expect(VoiceVcaLaw::gain(VoiceVcaLaw::deadband) == 0.0f,
               "the voice VCA does not shut below its declared deadband");
        // A card sitting at the largest control offset the Unit Character
        // ceiling can present must still count as shut, or its voice never
        // retires. 0.004 per unit of Unit Character, bounded at two.
        expect(VoiceVcaLaw::gain(2.0f * 0.004f) < VoiceVcaLaw::silenceGain,
               "the worst card control offset escapes the silence threshold");
    }

    // VCA LEVEL is not this per-voice law. It drives the common jack-board VCA
    // after the six voices are summed. Solve Roland's p. 8 converter and p. 15
    // resistor network independently in double precision, then apply NEC's
    // -5.9 mV/dB typical control constant. This guards every stage instead of
    // pinning a few outputs copied from the implementation.
    constexpr double dacSteps = 4096.0;
    constexpr double dacReferenceVolts = 5.0;
    constexpr double r30 = 2200.0;
    constexpr double r32 = 1500.0;
    constexpr double r31 = 47.0;
    constexpr double r165 = 15000.0;
    constexpr double biasVolts = 15.0;
    constexpr double voltsPerDb = -5.9e-3;
    const auto expectedCommonVcaControl = [=](int storedByte) {
        const double physicalCode = 32.0 * storedByte;
        const double converterVolts =
            dacReferenceVolts * physicalCode / dacSteps;
        const double holdVolts = 4.0 - 2.0 * converterVolts;
        const double holdSeries = r30 + r32;
        return (holdVolts / holdSeries + biasVolts / r165)
             / (1.0 / holdSeries + 1.0 / r31 + 1.0 / r165);
    };
    double previousPatchGain = -1.0;
    for (const int storedByte : { 0, 32, 64, 96, 127 })
    {
        const float position = static_cast<float>(storedByte) / 127.0f;
        const double expectedControl = expectedCommonVcaControl(storedByte);
        const double expectedDb = expectedControl / voltsPerDb;
        const double modelControl =
            YouKnow106Engine::commonVcaControlVolts(position);
        const double modelGain = YouKnow106Engine::patchLevelGain(position);
        const double modelDb = 20.0 * std::log10(modelGain);
        const std::string where =
            " at stored byte " + std::to_string(storedByte);
        expectNear(modelControl, expectedControl, 2.0e-8,
                   "common-VCA GC1 resistor solve" + where);
        expectNear(modelDb, expectedDb, 2.0e-5,
                   "common-VCA NEC dB conversion" + where);
        expect(modelGain > previousPatchGain,
               "common-VCA gain is not monotone" + where);
        previousPatchGain = modelGain;
    }
    expect(YouKnow106Engine::patchLevelGain(0.0f) > 0.0f,
           "VCA LEVEL minimum incorrectly mutes the patch");

    // C7 sees R30 || (R32 + (R31 || R165)) with the independent sources
    // grounded. This is a distinct control smoother from the C12/R36 audio
    // coupling network tested below.
    const double gcBiasResistance = r31 * r165 / (r31 + r165);
    const double farSideResistance = r32 + gcBiasResistance;
    const double c7TheveninResistance =
        r30 * farSideResistance / (r30 + farSideResistance);
    const double expectedC7Tau = c7TheveninResistance * 10.0e-6;
    expectNear(YouKnow106Engine::commonVcaHoldTimeConstantSeconds(),
               expectedC7Tau, 1.0e-9,
               "common-VCA C7 loaded time constant");

    // The jack-board gain chain is fixed by resistor ratios. IC1a attenuates
    // every voice before the shared VCA and BBDs; IC6 supplies the dry/wet
    // output gains after the BBDs. Their net small-signal gains must therefore
    // be 10/39 and 10/47, not the unity voice sum used by the old model.
    expectNear(YouKnow106Engine::voiceSummerGain, 3.3 / 33.0, 1.0e-7,
               "voice summer is not 3.3 kOhm / 33 kOhm");
    expectNear(YouKnow106Engine::voiceBusInput(6.0f), 0.6, 1.0e-7,
               "the six-voice bus does not enter the shared path at 0.1 per voice");
    constexpr double busCapacitance = 10.0e-6;
    constexpr double busResistance = 33000.0;
    const double expectedBusCorner =
        1.0 / (2.0 * pi * busCapacitance * busResistance);
    expectNear(YouKnow106Engine::voiceBusCouplingCornerHz(), expectedBusCorner,
               1.0e-6, "voice-bus C14/R39 coupling corner");
    constexpr double selectedInputResistance = 47000.0;
    const double loadedBusResistance =
        busResistance * selectedInputResistance
        / (busResistance + selectedInputResistance);
    const double loadedBusCorner =
        1.0 / (2.0 * pi * busCapacitance * loadedBusResistance);
    expectNear(YouKnow106Engine::voiceBusCouplingCornerHz(HighPassMode::One),
               loadedBusCorner, 1.0e-6,
               "flat HPF leg does not load C14 through its 47 kOhm input");
    expectNear(YouKnow106Engine::voiceBusCouplingCornerHz(HighPassMode::Boost),
               loadedBusCorner, 1.0e-6,
               "boost HPF leg does not load C14 through R25");
    constexpr double selectedCutBleedResistance = 1.0e6;
    const double cutLoadedBusResistance =
        busResistance * selectedCutBleedResistance
        / (busResistance + selectedCutBleedResistance);
    const double cutLoadedBusCorner =
        1.0 / (2.0 * pi * busCapacitance * cutLoadedBusResistance);
    expectNear(YouKnow106Engine::voiceBusCouplingCornerHz(HighPassMode::Two),
               cutLoadedBusCorner, 1.0e-6,
               "Cut II does not load C14 through its mux-side R21 bleed");
    expectNear(YouKnow106Engine::voiceBusCouplingCornerHz(HighPassMode::Three),
               cutLoadedBusCorner, 1.0e-6,
               "Cut III does not load C14 through its mux-side R23 bleed");
    expectNear(YouKnow106Engine::commonVcaInputCouplingCornerHz(),
               expectedBusCorner, 1.0e-6,
               "common-VCA C12/R36 input-coupling corner");
    expectNear(YouKnow106Engine::voiceSummerGain * Chorus::dryMixGain,
               10.0 / 47.0, 1.0e-6,
               "net per-voice dry gain misses the jack-board ratios");
    expectNear(YouKnow106Engine::voiceSummerGain * Chorus::wetMixGain,
               10.0 / 39.0, 1.0e-6,
               "net per-voice wet gain misses the jack-board ratios");
}

void testPulseWidthAndHighPassLaws()
{
    // The whole calibrated converter path is affine: B-2's full twelve-bit
    // code is Roland's +6 V / 50% state and its square-off code zero is the
    // -0.8 V comparator-pinning state. The loaded physical PWM pot normally
    // stops around byte 101; bytes above that are valid SysEx overrange.
    const auto manualCode = [](int raw) {
        return YouKnow106Engine::pwmDacCode(
            static_cast<float>(raw) / 127.0f,
            PwmSource::Manual, 0u, true);
    };
    expect(YouKnow106Engine::pwmDacCode(
               0.0f, PwmSource::Manual, 0u, true) == 0x0fffu
               && manualCode(101) == 0x0360u
               && manualCode(127) == 0x0020u,
           "PWM DAC vectors left B-2's manual partial-product law");
    expectNear(YouKnow106Engine::pwmDacVolts(0x0fffu), 6.0, 1.0e-6,
               "pulse threshold with the control at rest");
    expectNear(YouKnow106Engine::pwmDacVolts(0u), -0.8, 1.0e-6,
               "pulse-off DAC code missed its documented control voltage");
    expectNear(YouKnow106Engine::pwmDutyCycle(6.0f), 0.5, 1.0e-5,
               "threshold at half the ramp does not bisect it");
    expectNear(YouKnow106Engine::pwmDutyCycle(0.6f), 0.95, 1.0e-5,
               "the printed +0.6 V point is not a 95% pulse");
    const float physicalMaximum = YouKnow106Engine::pwmDacVolts(
        manualCode(101));
    expectNear(YouKnow106Engine::pwmDutyCycle(physicalMaximum),
               0.947106227, 1.0e-6,
               "the loaded knob maximum left Roland's 93-97% service window");
    expect(YouKnow106Engine::pwmDutyCycle(
               YouKnow106Engine::pwmDacVolts(manualCode(127))) == 1.0f,
           "the raw seven-bit PWM overrange no longer pins the comparator");
    expectNear(YouKnow106Engine::pwmDutyCycle(-0.8f), 1.0, 1.0e-6,
               "pulse-off control does not pin the comparator high");
    expectNear(YouKnow106Engine::pwmDutyCycle(3.0f, 1.03f),
               1.0 - 3.0 / (12.0 * 1.03), 1.0e-6,
               "a stronger physical ramp did not widen the comparator pulse");
    expectNear(YouKnow106Engine::pwmDutyCycle(3.0f, 0.97f),
               1.0 - 3.0 / (12.0 * 0.97), 1.0e-6,
               "a weaker physical ramp did not narrow the comparator pulse");
    expectNear(YouKnow106Engine::pwmDutyCycle(6.0f, 0.25f), 0.0, 0.0,
               "an under-compensated ramp cannot pin the comparator low");
    expectNear(YouKnow106Engine::pwmDutyCycle(-0.8f, 1.03f), 1.0, 1.0e-6,
               "ramp variation defeated the pulse-off pinned state");
    for (int raw = 0; raw <= 101; ++raw)
    {
        const float duty = YouKnow106Engine::pwmDutyCycle(
            YouKnow106Engine::pwmDacVolts(manualCode(raw)));
        expect(duty >= 0.5f && duty < 1.0f,
               "the loaded physical PWM travel reached a comparator rail");
    }

    // The held PWM CV reaches the comparators through its p. 13 smoothing
    // chain -- R117 100 kOhm into C62 47 nF, then R116 560 kOhm with C63
    // 4.7 nF around IC17a -- and the stored SUB level crosses R11 1 kOhm into
    // C1 10 uF ahead of the R9/R10 inverter. The shipped time constants must
    // stay the products of those anchored designators.
    expectNear(YouKnow106TestAccess::pwmFirstPoleSeconds(),
               100.0e3 * 47.0e-9, 1.0e-9,
               "PWM first smoothing pole is not R117 * C62");
    expectNear(YouKnow106TestAccess::pwmSecondPoleSeconds(),
               560.0e3 * 4.7e-9, 1.0e-9,
               "PWM second smoothing pole is not R116 * C63");
    expectNear(YouKnow106TestAccess::subHoldSlewSeconds(),
               1.0e3 * 10.0e-6, 1.0e-9,
               "SUB hold slew is not R11 * C1");

    // The per-card comparator offset is calibrated to leave a 48% to 52% duty
    // window across the six voices, so the voltage the engine draws against has
    // to be the one that produces exactly two points either way. Asserting the
    // duty rather than the voltage is the point: the voltage is a means, and a
    // change to the duty law that quietly rescaled it would pass otherwise.
    {
        const float offsetVolts = 0.24f;
        const float mid = 3.0f;   // away from the law's 50% floor, so it moves both ways
        const double wide = YouKnow106Engine::pwmDutyCycle(mid - offsetVolts);
        const double narrow = YouKnow106Engine::pwmDutyCycle(mid + offsetVolts);
        expectNear(0.5 * (wide - narrow), 0.02, 5.0e-4,
                   "the per-card comparator offset is not +/-2 points of duty");
        // And at the panel's own 50% end the law floors, so the same offset can
        // only widen the pulse -- 50% is the narrowest the control can ask for.
        expectNear(YouKnow106Engine::pwmDutyCycle(6.0f + offsetVolts), 0.5, 1.0e-5,
                   "an offset pushed the pulse below the panel's 50% floor");
        expectNear(YouKnow106Engine::pwmDutyCycle(6.0f - offsetVolts), 0.52, 5.0e-4,
                   "an offset at the 50% floor does not widen by two points");
    }

    // Only two of the four high-pass legs filter; one boosts and one passes.
    // The boost shelf is derived from the p. 15 branch itself -- the dry R25
    // leg at unity plus IC4b's DC-coupled leg -- so each constant is asserted
    // against its own closed form. The third-party noise sweep these figures
    // were once fitted to now corroborates them instead of sourcing them.
    expectNear(YouKnow106Engine::highPassShelfGain(HighPassMode::Boost),
               1.0 + (47.0 / 220.0) * (1.0 + 100.0 / 10.0), 1.0e-6,
               "boost DC gain is not 1 + (R29/R24)(1 + R18/R19)");
    expectNear(YouKnow106Engine::highPassHighGain(HighPassMode::Boost),
               1.0 + (47.0 / 220.0) * (47.0 / 57.0), 1.0e-6,
               "boost plateau is not 1 + (R29/R24) * C9/(C9 + C8)");
    expectNear(YouKnow106Engine::highPassCornerHz(HighPassMode::Boost),
               1.0 / (2.0 * pi * 47.0e3 * 57.0e-9), 0.05,
               "boost corner is not the R22 * (C9 + C8) pole");
    // The branch is two stages -- C9 parallel R22 into the C8 shunt, then
    // IC4b's gain-of-11 with C6 bypassing R18 -- yet one pole describes it,
    // because the first stage's 72.05 Hz zero all but cancels the feedback's
    // 72.34 Hz pole. Solve the complete network and hold the shipped one-pole
    // shelf to the exact response everywhere in the band.
    {
        const std::complex<double> j { 0.0, 1.0 };
        constexpr double r29 = 47.0e3, r24 = 220.0e3, r18 = 100.0e3;
        constexpr double r19 = 10.0e3, r22 = 47.0e3;
        constexpr double c9 = 47.0e-9, c8 = 10.0e-9, c6 = 22.0e-9;
        const double shelf =
            YouKnow106Engine::highPassShelfGain(HighPassMode::Boost);
        const double high =
            YouKnow106Engine::highPassHighGain(HighPassMode::Boost);
        const double poleHz =
            YouKnow106Engine::highPassCornerHz(HighPassMode::Boost);
        double worstDb = 0.0;
        for (double freq = 1.0; freq < 20000.0; freq *= 1.1)
        {
            const std::complex<double> s = j * (2.0 * pi * freq);
            const std::complex<double> firstStage =
                (1.0 + s * r22 * c9) / (1.0 + s * r22 * (c9 + c8));
            const std::complex<double> amplifier =
                (1.0 + r18 / r19 + s * r18 * c6) / (1.0 + s * r18 * c6);
            const std::complex<double> exact =
                1.0 + (r29 / r24) * firstStage * amplifier;
            const std::complex<double> shipped =
                high + (shelf - high) / (1.0 + s / (2.0 * pi * poleHz));
            worstDb = std::max(worstDb,
                               std::abs(20.0 * std::log10(std::abs(exact))
                                        - 20.0 * std::log10(std::abs(shipped))));
        }
        expect(worstDb < 0.02,
               "one-pole boost shelf drifts past 0.02 dB from the solved branch");
    }
    expectNear(YouKnow106Engine::highPassShelfGain(HighPassMode::One), 1.0, 1.0e-6,
               "the flat leg does not pass the low band untouched");
    expectNear(YouKnow106Engine::highPassHighGain(HighPassMode::One), 1.0, 1.0e-6,
               "the flat leg does not pass the high band untouched");
    expect(YouKnow106Engine::highPassShelfGain(HighPassMode::Two) == 0.0f
           && YouKnow106Engine::highPassShelfGain(HighPassMode::Three) == 0.0f,
           "a cutting leg returns part of the low band");
    // Computed from the schematic parts rather than written down, so the
    // assertion says where the number comes from: the two cutting legs use
    // C10 15 nF and C11 4.7 nF through the 47 kOhm resistor pack.
    const double feedOhms = 47.0e3;
    const auto corner = [feedOhms] (double farads) {
        return 1.0 / (2.0 * pi * feedOhms * farads);
    };
    expectNear(YouKnow106Engine::highPassCornerHz(HighPassMode::Two),
               corner(15.0e-9), 0.5,
               "middle high-pass corner is not 47 kOhm against 15 nF");
    expectNear(YouKnow106Engine::highPassCornerHz(HighPassMode::Three),
               corner(4.7e-9), 0.5,
               "top high-pass corner is not 47 kOhm against 4.7 nF");

    // The service schematic's final stereo coupling paths are identical:
    // IC6 -> C17/C20 10 uF -> R54/R57 1.5 kOhm -> one 10 kOhm VOLUME
    // track. These no-argument helpers retain the earlier unloaded/full-track
    // comparison boundary; the runtime's internally loaded law follows below.
    constexpr double capacitance = 10.0e-6;
    constexpr double seriesResistance = 1500.0;
    constexpr double potResistance = 10000.0;
    const double expectedCorner =
        1.0 / (2.0 * pi * capacitance
               * (seriesResistance + potResistance));
    const double expectedHighGain =
        potResistance / (seriesResistance + potResistance);
    expectNear(YouKnow106Engine::outputCouplingCornerHz(), expectedCorner,
               1.0e-5, "final output-coupling corner");
    expectNear(YouKnow106Engine::outputCouplingHighGain(), expectedHighGain,
               1.0e-7, "final output-coupling high-frequency gain");

    // The selector ladder and headphone amplifier are connected to each wiper
    // internally even with no external jack load. A nominal-linear B track is
    // therefore slightly loaded in circuit, and that same load moves the C17/
    // C20 pole with shaft position.
    constexpr double selectorLadder = 33000.0 + 6800.0 + 1500.0;
    constexpr double headphoneInput = 1000.0 + 100000.0;
    const double internalWiperLoad =
        selectorLadder * headphoneInput / (selectorLadder + headphoneInput);
    const auto expectedLoadedGain = [&](double position) {
        const double lower = position * potResistance;
        const double loadedLower = lower > 0.0
            ? lower * internalWiperLoad / (lower + internalWiperLoad) : 0.0;
        const double upper = (1.0 - position) * potResistance;
        return loadedLower / (seriesResistance + upper + loadedLower);
    };
    const auto expectedLoadedCorner = [&](double position) {
        const double lower = position * potResistance;
        const double loadedLower = lower > 0.0
            ? lower * internalWiperLoad / (lower + internalWiperLoad) : 0.0;
        const double upper = (1.0 - position) * potResistance;
        return 1.0 / (2.0 * pi * capacitance
                      * (seriesResistance + upper + loadedLower));
    };
    for (const float position : { 0.0f, 0.5f, 1.0f })
    {
        expectNear(YouKnow106Engine::outputCouplingHighGain(position),
                   expectedLoadedGain(position), 1.0e-7,
                   "loaded 10KB wiper gain at shaft position "
                       + std::to_string(position));
        expectNear(YouKnow106Engine::outputCouplingCornerHz(position),
                   expectedLoadedCorner(position), 1.0e-5,
                   "loaded output-coupling corner at shaft position "
                       + std::to_string(position));
    }
    expectNear(YouKnow106Engine::outputCouplingHighGain(0.5f)
                   / YouKnow106Engine::outputCouplingHighGain(1.0f),
               0.4763, 5.0e-4,
               "the loaded nominal-linear pot midpoint escaped its circuit law");

    // Exercise the realised loaded pole too, not only its static helper. At a
    // fixed shaft position the exact TPT step is highGain*pole^n/(1+g).
    constexpr double loadedRate = 48000.0;
    constexpr int loadedSamples = 48000;
    const std::vector<float> loadedStep(loadedSamples, 1.0f);
    for (const float position : { 0.25f, 0.5f, 1.0f })
    {
        const double corner =
            YouKnow106Engine::outputCouplingCornerHz(position);
        const double gain =
            YouKnow106Engine::outputCouplingHighGain(position);
        const double g = std::tan(pi * corner / loadedRate);
        const double pole = (1.0 - g) / (1.0 + g);
        const auto response = YouKnow106TestAccess::renderLoadedOutputCoupling(
            loadedStep, loadedRate, position);
        for (const int sample : { 0, 12000, loadedSamples - 1 })
            expectNear(response[static_cast<std::size_t>(sample)],
                       gain * std::pow(pole, sample) / (1.0 + g), 2.0e-7,
                       "loaded output pole misses its fixed-position response at "
                           + std::to_string(position));
    }

    // The realised topology-preserving pole must retain the same physical
    // time constant at every supported host-rate family. A unit step through
    // a high-pass is highGain*exp(-t/tau), independent of block processing.
    constexpr double timeConstant = capacitance
                                  * (seriesResistance + potResistance);
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const int samples = static_cast<int>(std::ceil(sampleRate * 1.2));
        const std::vector<float> step(static_cast<std::size_t>(samples), 1.0f);
        const auto response =
            YouKnow106TestAccess::renderOutputCoupling(step, sampleRate);
        const int atTau = static_cast<int>(std::llround(
            sampleRate * timeConstant));
        const double g = std::tan(pi * expectedCorner / sampleRate);
        const double pole = (1.0 - g) / (1.0 + g);
        const auto expectedStepAt = [&](int sample) {
            return expectedHighGain * std::pow(pole, sample) / (1.0 + g);
        };
        expectNear(response[static_cast<std::size_t>(atTau)],
                   expectedStepAt(atTau), 2.0e-6,
                   "final output coupling misses its 115 ms time constant at "
                       + std::to_string(static_cast<int>(sampleRate)) + " Hz");
        expectNear(response.back(), expectedStepAt(samples - 1), 2.0e-7,
                   "final output coupling has a sample-rate-dependent decay");
    }


    // Keep the no-argument C14/R39 reference boundary independently realised.
    // Runtime mode selection applies the documented leg loads tested below.
    constexpr double busCapacitance = 10.0e-6;
    constexpr double busResistance = 33000.0;
    constexpr double busTimeConstant = busCapacitance * busResistance;
    const double busCorner =
        1.0 / (2.0 * pi * busTimeConstant);
    for (const double sampleRate : { 44100.0, 48000.0, 192000.0 })
    {
        const int samples = static_cast<int>(std::ceil(sampleRate * 1.2));
        const std::vector<float> step(static_cast<std::size_t>(samples), 1.0f);
        const auto response =
            YouKnow106TestAccess::renderVoiceBusCoupling(step, sampleRate);
        const int atTau = static_cast<int>(std::llround(
            sampleRate * busTimeConstant));
        const double g = std::tan(pi * busCorner / sampleRate);
        const double pole = (1.0 - g) / (1.0 + g);
        const auto expectedStepAt = [&](int sample) {
            return std::pow(pole, sample) / (1.0 + g);
        };
        expectNear(response[static_cast<std::size_t>(atTau)],
                   expectedStepAt(atTau), 2.0e-6,
                   "voice-bus coupling misses its 330 ms time constant at "
                       + std::to_string(static_cast<int>(sampleRate)) + " Hz");
        expectNear(response.back(), expectedStepAt(samples - 1), 2.0e-7,
                   "voice-bus coupling has a sample-rate-dependent decay");

        const auto vcaResponse =
            YouKnow106TestAccess::renderCommonVcaInputCoupling(step, sampleRate);
        expectNear(vcaResponse[static_cast<std::size_t>(atTau)],
                   expectedStepAt(atTau), 2.0e-6,
                   "common-VCA input coupling misses its 330 ms time constant at "
                       + std::to_string(static_cast<int>(sampleRate)) + " Hz");
        expectNear(vcaResponse.back(), expectedStepAt(samples - 1), 2.0e-7,
                   "common-VCA input coupling has a sample-rate-dependent decay");
    }

    // In either Cut position, R21/R23 remains a direct 1 MOhm shunt from the
    // selected mux input to ground even though C10/C11 opens the far side at
    // the sub-hertz asymptote. Exercise the realised 319.458 ms TPT state at
    // each relevant processing-rate family, not just the scalar helper.
    constexpr double cutBleedResistance = 1.0e6;
    constexpr double cutLoadedResistance =
        busResistance * cutBleedResistance
        / (busResistance + cutBleedResistance);
    constexpr double cutBusTimeConstant =
        busCapacitance * cutLoadedResistance;
    for (const double sampleRate : { 44100.0, 48000.0, 192000.0 })
    {
        const int samples = static_cast<int>(std::ceil(sampleRate * 1.2));
        const std::vector<float> step(static_cast<std::size_t>(samples), 1.0f);
        const int atTau = static_cast<int>(std::llround(
            sampleRate * cutBusTimeConstant));
        for (const HighPassMode mode :
             { HighPassMode::Two, HighPassMode::Three })
        {
            const double corner =
                YouKnow106Engine::voiceBusCouplingCornerHz(mode);
            const double g = std::tan(pi * corner / sampleRate);
            const double pole = (1.0 - g) / (1.0 + g);
            const auto expectedStepAt = [&](int sample) {
                return std::pow(pole, sample) / (1.0 + g);
            };
            const auto response = YouKnow106TestAccess::renderVoiceBusCoupling(
                step, sampleRate, mode);
            expectNear(response[static_cast<std::size_t>(atTau)],
                       expectedStepAt(atTau), 2.0e-6,
                       "selected-Cut C14 state misses its loaded time constant at "
                           + std::to_string(static_cast<int>(sampleRate)) + " Hz");
            expectNear(response.back(), expectedStepAt(samples - 1), 2.0e-7,
                       "selected-Cut C14 decay changes with processing rate");
        }
    }

    // Re-reading the shared coefficient at a host block boundary must not
    // restart the physical C14 state. An irregular partition therefore has
    // to be sample-identical to one large block.
    std::vector<float> blockInput(4099);
    for (std::size_t index = 0; index < blockInput.size(); ++index)
        blockInput[index] = static_cast<float>(
            0.4 * std::sin(2.0 * pi * 37.0 * static_cast<double>(index) / 48000.0)
            + (index >= 997 ? 0.2 : -0.1));
    const auto wholeBlock = YouKnow106TestAccess::renderVoiceBusCouplingInBlocks(
        blockInput, 48000.0, HighPassMode::Two, blockInput.size());
    const auto splitBlocks = YouKnow106TestAccess::renderVoiceBusCouplingInBlocks(
        blockInput, 48000.0, HighPassMode::Two, 37);
    expect(wholeBlock == splitBlocks,
           "selected-Cut C14 state changes with host block partitioning");

    // A mode or numerical-rate change only replaces g. The existing C14
    // coordinate survives both it and the live-HQ preserving-clear path. Hard
    // output-path clears (including public panic) and engine reset discharge
    // it; this fixture exercises the reset boundary.
    const auto stateContract =
        YouKnow106TestAccess::voiceBusCouplingStateContract();
    expect(stateContract.afterModeChange == stateContract.beforeModeChange,
           "changing HPF mode reinterprets or clears the C14 state");
    expect(stateContract.afterRateChange == stateContract.beforeModeChange,
           "changing the internal rate reinterprets or clears the C14 state");
    expect(stateContract.afterPreservingClear == stateContract.beforeModeChange,
           "the live-HQ output rebuild clears the C14 state");
    expect(stateContract.afterHardReset == 0.0,
           "a hard output reset does not clear the C14 state");
}

void testPwmDutyCycleDefensiveGuard()
{
    // pwmDutyCycle's only production call site (updatePulseComparator) builds
    // both arguments from panel calibration, per-card trims and the DCO's own
    // render scale, none of which can go non-finite, so its "sanitised(...,
    // fallback)" branches for controlVolts and rampAmplitudeScale had never
    // fired outside a test. Pin the documented fallback contract directly.
    const double nominal = YouKnow106Engine::pwmDutyCycle(6.0f);
    expectNear(YouKnow106Engine::pwmDutyCycle(
                   std::numeric_limits<float>::quiet_NaN()),
               nominal, 1.0e-9,
               "NaN control volts did not fall back to the nominal 6 V ramp threshold");
    expectNear(YouKnow106Engine::pwmDutyCycle(
                   std::numeric_limits<float>::infinity()),
               nominal, 1.0e-9,
               "+infinity control volts did not fall back to the nominal 6 V ramp threshold");
    // Negative infinity is not finite, so it must not take the separate
    // "Pulse Off pins the comparator high" early-return either -- that branch
    // is explicitly guarded by std::isfinite -- it has to fall through to the
    // same sanitised fallback as the other non-finite cases.
    expectNear(YouKnow106Engine::pwmDutyCycle(
                   -std::numeric_limits<float>::infinity()),
               nominal, 1.0e-9,
               "-infinity control volts incorrectly pinned the comparator high");

    const double unscaled = YouKnow106Engine::pwmDutyCycle(3.0f, 1.0f);
    expectNear(YouKnow106Engine::pwmDutyCycle(
                   3.0f, std::numeric_limits<float>::quiet_NaN()),
               unscaled, 1.0e-9,
               "NaN ramp amplitude scale did not fall back to unity");
    expectNear(YouKnow106Engine::pwmDutyCycle(
                   3.0f, std::numeric_limits<float>::infinity()),
               unscaled, 1.0e-9,
               "+infinity ramp amplitude scale did not fall back to unity");
    expectNear(YouKnow106Engine::pwmDutyCycle(
                   3.0f, -std::numeric_limits<float>::infinity()),
               unscaled, 1.0e-9,
               "-infinity ramp amplitude scale did not fall back to unity");
}

void testSharedHighPassAgainstNominalNetwork()
{
    // Solve the nominal p. 15 network twice: first as a dense nodal system,
    // then as independently reduced closed forms. The realtime path remains
    // the deliberately small cascade, so the final gate bounds its residual
    // without claiming unmeasured TC4052 switching transients.
    using Complex = std::complex<double>;
    using Matrix = std::array<std::array<Complex, 3>, 3>;
    using Vector = std::array<Complex, 3>;

    constexpr double c14 = 10.0e-6;
    constexpr double r39 = 33.0e3;
    constexpr double rBleed = 1.0e6;
    constexpr double r = 47.0e3;
    constexpr double r22 = 47.0e3;
    constexpr double r24 = 220.0e3;
    constexpr double r25 = 47.0e3;
    constexpr double r29 = 47.0e3;
    constexpr double r18 = 100.0e3;
    constexpr double r19 = 10.0e3;
    constexpr double c9 = 47.0e-9;
    constexpr double c8 = 10.0e-9;
    constexpr double c6 = 22.0e-9;

    const auto solve = [](Matrix matrix, Vector right) {
        for (std::size_t column = 0; column < matrix.size(); ++column)
        {
            std::size_t pivot = column;
            for (std::size_t row = column + 1; row < matrix.size(); ++row)
                if (std::abs(matrix[row][column])
                    > std::abs(matrix[pivot][column]))
                    pivot = row;
            std::swap(matrix[column], matrix[pivot]);
            std::swap(right[column], right[pivot]);

            const Complex diagonal = matrix[column][column];
            for (std::size_t entry = 0; entry < matrix.size(); ++entry)
                matrix[column][entry] /= diagonal;
            right[column] /= diagonal;
            for (std::size_t row = 0; row < matrix.size(); ++row)
            {
                if (row == column)
                    continue;
                const Complex factor = matrix[row][column];
                for (std::size_t entry = 0; entry < matrix.size(); ++entry)
                    matrix[row][entry] -= factor * matrix[column][entry];
                right[row] -= factor * right[column];
            }
        }
        return right;
    };

    const auto mnaResponse = [&](HighPassMode mode, Complex s) {
        Matrix matrix {};
        Vector right {};
        const Complex y14 = s * c14;
        right[0] = y14;

        if (mode == HighPassMode::One)
        {
            matrix[0][0] = y14 + 1.0 / r39 + 1.0 / r;
            matrix[1][1] = 1.0;
            matrix[2][2] = 1.0;
            return solve(matrix, right)[0];
        }
        if (mode == HighPassMode::Two || mode == HighPassMode::Three)
        {
            const double capacitance = mode == HighPassMode::Two
                ? 15.0e-9 : 4.7e-9;
            const Complex yCut = s * capacitance;
            matrix[0][0] = y14 + 1.0 / r39 + 1.0 / rBleed + yCut;
            matrix[0][1] = -yCut;
            matrix[1][0] = -yCut;
            matrix[1][1] = yCut + 1.0 / r;
            matrix[2][2] = 1.0;
            return solve(matrix, right)[1];
        }

        const Complex yLink = 1.0 / r22 + s * c9;
        matrix[0][0] = y14 + 1.0 / r39 + 1.0 / r25 + yLink;
        matrix[0][1] = -yLink;
        matrix[1][0] = -yLink;
        matrix[1][1] = yLink + s * c8;
        matrix[2][1] = 1.0 / r19 + 1.0 / r18 + s * c6;
        matrix[2][2] = -(1.0 / r18 + s * c6);
        const Vector nodes = solve(matrix, right);
        return (r29 / r25) * nodes[0] + (r29 / r24) * nodes[2];
    };

    const auto analyticResponse = [&](HighPassMode mode, Complex s) {
        const Complex y14 = s * c14;
        if (mode == HighPassMode::One)
            return y14 / (y14 + 1.0 / r39 + 1.0 / r);
        if (mode == HighPassMode::Two || mode == HighPassMode::Three)
        {
            const double capacitance = mode == HighPassMode::Two
                ? 15.0e-9 : 4.7e-9;
            const Complex cutAdmittance =
                s * capacitance / (1.0 + s * r * capacitance);
            const Complex common = y14
                / (y14 + 1.0 / r39 + 1.0 / rBleed + cutAdmittance);
            return common * (s * r * capacitance)
                / (1.0 + s * r * capacitance);
        }

        const Complex firstStage =
            (1.0 + s * r22 * c9) / (1.0 + s * r22 * (c9 + c8));
        const Complex amplifier =
            (1.0 + r18 / r19 + s * r18 * c6)
            / (1.0 + s * r18 * c6);
        const Complex common = y14
            / (y14 + 1.0 / r39 + 1.0 / r25 + s * c8 * firstStage);
        return common
            * (r29 / r25 + (r29 / r24) * firstStage * amplifier);
    };

    const auto shippedResponse = [](HighPassMode mode, Complex s) {
        const double couplingCorner =
            YouKnow106Engine::voiceBusCouplingCornerHz(mode);
        const Complex coupling = s / (s + 2.0 * pi * couplingCorner);
        if (mode == HighPassMode::One)
            return coupling;
        const double corner = YouKnow106Engine::highPassCornerHz(mode);
        if (mode == HighPassMode::Two || mode == HighPassMode::Three)
            return coupling * s / (s + 2.0 * pi * corner);
        const double shelf =
            YouKnow106Engine::highPassShelfGain(HighPassMode::Boost);
        const double high =
            YouKnow106Engine::highPassHighGain(HighPassMode::Boost);
        const Complex boost = high
            + (shelf - high) / (1.0 + s / (2.0 * pi * corner));
        return coupling * boost;
    };

    struct ResponseCase
    {
        HighPassMode mode;
        const char* name;
        double maximumDb;
        double maximumPhaseDegrees;
    };
    constexpr std::array<ResponseCase, 4> cases {{
        { HighPassMode::Boost, "Boost", 0.010, 0.060 },
        { HighPassMode::One, "Flat", 1.0e-5, 1.0e-5 },
        { HighPassMode::Two, "Cut II", 0.014, 0.045 },
        { HighPassMode::Three, "Cut III", 0.005, 0.015 }
    }};
    constexpr int frequencyPoints = 4097;
    for (const auto& responseCase : cases)
    {
        double worstMnaRelative = 0.0;
        double worstDb = 0.0;
        double worstPhase = 0.0;
        for (int index = 0; index < frequencyPoints; ++index)
        {
            const double fraction = static_cast<double>(index)
                                  / static_cast<double>(frequencyPoints - 1);
            const double frequency = 0.1 * std::pow(200000.0, fraction);
            const Complex s { 0.0, 2.0 * pi * frequency };
            const Complex mna = mnaResponse(responseCase.mode, s);
            const Complex analytic = analyticResponse(responseCase.mode, s);
            const Complex shipped = shippedResponse(responseCase.mode, s);
            worstMnaRelative = std::max(
                worstMnaRelative,
                std::abs(mna - analytic)
                    / std::max(std::abs(analytic), 1.0e-30));
            const Complex ratio = analytic / shipped;
            worstDb = std::max(
                worstDb, std::abs(20.0 * std::log10(std::abs(ratio))));
            worstPhase = std::max(
                worstPhase, std::abs(std::arg(ratio) * 180.0 / pi));
        }
        const std::string where = " for " + std::string(responseCase.name);
        expect(worstMnaRelative < 2.0e-12,
               "closed-form HPF solve disagrees with nodal MNA" + where);
        expect(worstDb < responseCase.maximumDb,
               "shared HPF approximation exceeds its dB bound" + where);
        expect(worstPhase < responseCase.maximumPhaseDegrees,
               "shared HPF approximation exceeds its phase bound" + where);
    }
}

void testServiceSpecificationEndpointReconciliation()
{
    // Service Notes p. 1 prints ENV ATTACK 1.5 ms-3 s, DECAY/RELEASE
    // 1.5 ms-12 s, LFO RATE 0.1-30 Hz and DELAY 0-3 s without any threshold
    // convention. The 2026-08-20 evidence pass reconciled those printed
    // endpoints against the hash-matched B-2 generators at the nominal
    // 4.2 ms pass: every reconcilable ceiling inverts to a pass period
    // within 9% of 4.2 ms, the identical 1.5 ms floors sit on the analogue
    // hold slew rather than any table time, and two figures (LFO 0.1 Hz,
    // decay/release 12 s read as anything but one time constant) stay
    // recorded contradictions. These fixtures pin that reconciliation so a
    // later pass-period or generator change that silently breaks the
    // printed corroboration fails here instead of passing unnoticed.
    constexpr double nominalPassSeconds = 0.0042;
    constexpr double clusterTolerance = 0.09 * nominalPassSeconds;

    // Attack ceiling. The slowest increment reaches 90% of the 14-bit peak
    // in 703 passes (2.9526 s) and saturates in 781 (3.2802 s); the printed
    // "3s" sits between the two readings, so both invert into the cluster.
    const std::uint16_t slowestAttack =
        YouKnow106Engine::envelopeAttackIncrement(1.0f);
    expect(slowestAttack == 21u,
           "the slowest B-2 attack increment moved from 21");
    {
        std::uint16_t level = 0u;
        int passes = 0;
        int passesToNinety = 0;
        while (level != YouKnow106Engine::envelopePeak && passes < 1000)
        {
            level = YouKnow106Engine::envelopeAttackLevel(level, slowestAttack);
            ++passes;
            if (passesToNinety == 0
                && static_cast<double>(level)
                       >= 0.9 * YouKnow106Engine::envelopePeak)
                passesToNinety = passes;
        }
        expect(passesToNinety == 703 && passes == 781,
               "slowest attack no longer reaches 90% in 703 and full scale "
               "in 781 passes");
        expectNear(3.0 / passes, nominalPassSeconds, clusterTolerance,
                   "the printed 3 s attack ceiling read as time-to-full no "
                   "longer inverts near the 4.2 ms pass");
        expectNear(3.0 / passesToNinety, nominalPassSeconds, clusterTolerance,
                   "the printed 3 s attack ceiling read as time-to-90% no "
                   "longer inverts near the 4.2 ms pass");
    }

    // Decay/release ceiling. Only the one-time-constant reading of the
    // printed "12s" is arithmetically compatible with the slowest fall:
    // the exact e^-1 crossing lands at pass 3008 (12.6336 s, +5.3%). The
    // digital-zero and -20 dB readings imply pass periods at which the same
    // firmware's LFO top would print above 45 Hz, not 30 - that exclusion
    // is asserted, not assumed.
    const std::uint16_t slowestFall =
        YouKnow106Engine::envelopeDecayReleaseMultiplier(1.0f);
    expect(slowestFall == 65524u,
           "the slowest B-2 decay/release coefficient moved from 65524");
    {
        const double oneTau =
            std::exp(-1.0) * YouKnow106Engine::envelopePeak;
        std::uint16_t level = YouKnow106Engine::envelopePeak;
        int passes = 0;
        while (static_cast<double>(level) >= oneTau && passes < 10000)
        {
            level = YouKnow106Engine::envelopeReleaseLevel(level, slowestFall);
            ++passes;
        }
        expect(passes == 3008,
               "slowest fall no longer crosses one time constant at pass 3008");
        expectNear(12.0 / passes, nominalPassSeconds, clusterTolerance,
                   "the printed 12 s ceiling read as one time constant no "
                   "longer inverts near the 4.2 ms pass");
        // -20 dB at the same coefficient takes 5137 passes (asserted by the
        // decay-seconds law elsewhere); forcing 12 s onto that reading gives
        // T = 2.336 ms, at which the B-2 LFO top (two passes per ramp) would
        // print 1/(8T) = 53.5 Hz.
        expect(1.0 / (8.0 * (12.0 / 5137.0)) > 45.0,
               "the -20 dB reading of the 12 s ceiling stopped excluding "
               "itself against the printed 30 Hz LFO top");
    }

    // LFO endpoints. The printed 30 Hz top inverts to 4.1667 ms (inside the
    // cluster); the printed 0.1 Hz floor is unreconcilable with rate byte 0
    // at any pass period (endpoint ratio 819.5 against the spec's 300). Byte
    // 1 happens to land near 0.1 Hz, but the assigner maps raw ADC codes 0--5
    // to reachable byte 0, so that coincidence is not an endpoint correction.
    expectNear(1.0 / (8.0 * 30.0), nominalPassSeconds, clusterTolerance,
               "the printed 30 Hz LFO top no longer inverts near the 4.2 ms "
               "pass");
    expect(YouKnow106Engine::lfoRateHz(0.0f) > 0.03f
               && YouKnow106Engine::lfoRateHz(0.0f) < 0.04f,
           "rate byte 0 was bent toward the spec's irreconcilable 0.1 Hz "
           "floor");

    // Delay ceiling. The printed "0 to 3s" quotes the silent hold phase
    // alone: the slowest hold is ceil(16384/21) = 781 passes = 3.2802 s,
    // while hold plus fade (4.3512 s, asserted elsewhere) cannot round to 3.
    {
        const int holdPasses = (16384 + slowestAttack - 1) / slowestAttack;
        expect(holdPasses == 781,
               "the slowest delay hold no longer lasts 781 passes");
        expectNear(3.0 / holdPasses, nominalPassSeconds, clusterTolerance,
                   "the printed 3 s delay ceiling read as the hold phase no "
                   "longer inverts near the 4.2 ms pass");
    }

    // The three identical 1.5 ms floors are analogue-scale: no pass period
    // reproduces them from the tables (the fastest attack is one pass, the
    // fastest fall reaches digital zero in four), while the derived voice-VCA
    // hold's 10-90% rise, ln(9) * 687 us, lands on the printed figure.
    expectNear(std::log(9.0) * YouKnow106TestAccess::voiceVcaHoldSlewSeconds(),
               0.0015, 5.0e-5,
               "the derived hold slew no longer reproduces the spec's 1.5 ms "
               "floor");

    // SUSTAIN "0 to 100%" is nominal: the stored maximum S = 128*127 sits
    // 0.068 dB under the attack peak.
    expectNear(20.0 * std::log10(16256.0 / 16383.0), -0.0677, 1.0e-3,
               "the sustain ceiling's distance from the attack peak moved");
}

void testModulationAndGlideLaws()
{
    // Exact hash-matched B-2 rate vectors. A signed cycle is four clamped
    // 0..0x1fff ramps, each lasting ceil(8192 / coefficient) scan passes.
    struct LfoVector { int code; std::uint16_t coefficient; int rampPasses; };
    constexpr std::array<LfoVector, 4> lfoVectors {{
        { 0,      5u, 1639 },
        { 1,     15u,  547 },
        { 64,   666u,   13 },
        { 127, 4096u,    2 }
    }};
    for (const auto& vector : lfoVectors)
    {
        const float position = static_cast<float>(vector.code) / 127.0f;
        expect(YouKnow106Engine::lfoRateIncrement(position) == vector.coefficient,
               "B-2 LFO coefficient vector mismatch at code "
                   + std::to_string(vector.code));
        expectNear(YouKnow106Engine::lfoRateHz(position),
                   1.0 / (4.0 * vector.rampPasses * 0.0042), 2.0e-6,
                   "B-2 LFO rate vector mismatch at code "
                       + std::to_string(vector.code));
    }

    // Delay is never an immediate jump: it is an attack-table silent hold plus
    // one of eight exact fade bins. The threshold-crossing pass also performs
    // the first fade add, so the two pass counts overlap by one. Verify every
    // stored byte against that integer construction so the hold cannot drift
    // away from attack's source.
    for (int byte = 0; byte <= 127; ++byte)
    {
        const float position = static_cast<float>(byte) / 127.0f;
        const int attack = YouKnow106Engine::envelopeAttackIncrement(position);
        const int fade = YouKnow106Engine::lfoDelayFadeIncrement(position);
        const int holdPasses = (16384 + attack - 1) / attack;
        const int fadePasses = (65536 + fade - 1) / fade;
        expectNear(YouKnow106Engine::lfoDelaySeconds(position),
                   (holdPasses + fadePasses - 1) * 0.0042, 1.0e-5,
                   "LFO delay does not use exact attack-hold plus fade at byte "
                       + std::to_string(byte));
    }
    expectNear(YouKnow106Engine::lfoDelaySeconds(0.0f), 0.0084, 1.0e-6,
               "LFO delay byte zero misses its two-pass completion");
    expectNear(YouKnow106Engine::lfoDelaySeconds(1.0f), 4.3512, 1.0e-4,
               "longest LFO delay misses the exact hold-plus-fade duration");

    struct FadeBin { int first; int last; std::uint16_t coefficient; int passes; };
    constexpr std::array<FadeBin, 8> fadeBins {{
        {   0,  15, 65535u,   2 },
        {  16,  31,  1049u,  63 },
        {  32,  47,   524u, 126 },
        {  48,  63,   350u, 188 },
        {  64,  79,   256u, 256 },
        {  80,  95,   256u, 256 },
        {  96, 111,   256u, 256 },
        { 112, 127,   256u, 256 }
    }};
    for (const auto& bin : fadeBins)
    {
        for (const int byte : { bin.first, bin.last })
        {
            const auto coefficient = YouKnow106Engine::lfoDelayFadeIncrement(
                static_cast<float>(byte) / 127.0f);
            expect(coefficient == bin.coefficient,
                   "LFO delay fade bin coefficient mismatch at byte "
                       + std::to_string(byte));
            expect((65536 + coefficient - 1) / coefficient == bin.passes,
                   "LFO delay fade pass count mismatch at byte "
                       + std::to_string(byte));
        }
    }

    // Portamento is an eight-bit ADC. Raw 0/1 are immediate, then 2n and 2n+1
    // share an eight-bit coefficient selected by raw>>1.
    expect(YouKnow106Engine::portamentoSeconds(0.0f) == 0.0f,
           "portamento is not switched off at the bottom of its travel");
    expect(YouKnow106Engine::portamentoIncrement(0.0f) == 0u,
           "raw portamento code zero is not immediate");
    expect(YouKnow106Engine::portamentoSeconds(1.0f / 255.0f) == 0.0f,
           "raw portamento code one does not retain the zero/immediate entry");
    expect(YouKnow106Engine::portamentoIncrement(1.0f / 255.0f) == 0u,
           "raw portamento code one selects a nonzero coefficient");
    expect(YouKnow106Engine::portamentoIncrement(2.0f / 255.0f) == 255u
           && YouKnow106Engine::portamentoIncrement(3.0f / 255.0f) == 255u,
           "raw portamento codes 2/3 miss coefficient 255");
    expectNear(YouKnow106Engine::portamentoSeconds(2.0f / 255.0f),
               13.0 * 0.0042, 1.0e-6,
               "first active portamento code does not respect the 8-bit coefficient ceiling");
    expect(YouKnow106Engine::portamentoIncrement(127.0f / 255.0f) == 13u
           && YouKnow106Engine::portamentoIncrement(128.0f / 255.0f) == 13u,
           "raw portamento codes 127/128 miss coefficient 13");
    expectNear(YouKnow106Engine::portamentoSeconds(127.0f / 255.0f),
               237.0 * 0.0042, 1.0e-6,
               "raw portamento code 127 misses the 995.4 ms vector");
    expectNear(YouKnow106Engine::portamentoSeconds(128.0f / 255.0f),
               237.0 * 0.0042, 1.0e-6,
               "raw portamento code 128 misses the 995.4 ms vector");
    expect(YouKnow106Engine::portamentoIncrement(254.0f / 255.0f) == 1u
           && YouKnow106Engine::portamentoIncrement(1.0f) == 1u,
           "raw portamento codes 254/255 miss coefficient one");
    expectNear(YouKnow106Engine::portamentoSeconds(254.0f / 255.0f),
               3072.0 * 0.0042, 1.0e-4,
               "raw portamento code 254 misses the 12.9024 second vector");
    expectNear(YouKnow106Engine::portamentoSeconds(1.0f),
               3072.0 * 0.0042, 1.0e-4,
               "raw portamento code 255 misses the 12.9024 second vector");
    for (int raw = 2; raw < 255; raw += 2)
        expect(YouKnow106Engine::portamentoIncrement(raw / 255.0f)
                   == YouKnow106Engine::portamentoIncrement((raw + 1) / 255.0f),
               "paired portamento ADC codes select different coefficients at raw "
                   + std::to_string(raw));

    // Between the knob and those raw codes sits the bender board's loaded
    // divider (p. 16, 2026-08-20 read): a 50KB linear track whose wiper
    // drives the slave ADC against R16 47 kOhm to ground. The mapping is
    // exact at both track ends, sags below linear everywhere between them
    // (0.39496 at half travel), inverts exactly, and reaches the raw-code
    // law only through this one function.
    expect(YouKnow106Engine::portamentoTravelAdcFraction(0.0f) == 0.0f
               && YouKnow106Engine::portamentoTravelAdcFraction(1.0f) == 1.0f,
           "the loaded portamento divider moved its track endpoints");
    expectNear(YouKnow106Engine::portamentoTravelAdcFraction(0.5f),
               23.5 / 59.5, 1.0e-6,
               "the loaded portamento divider misses its half-travel value");
    float previousFraction = -1.0f;
    for (int step = 0; step <= 64; ++step)
    {
        const float travel = static_cast<float>(step) / 64.0f;
        const float fraction =
            YouKnow106Engine::portamentoTravelAdcFraction(travel);
        expect(fraction > previousFraction,
               "the loaded portamento divider is not strictly monotone");
        if (step != 0 && step != 64)
            expect(fraction < travel,
                   "the loaded portamento divider stopped sagging below "
                   "linear travel");
        expectNear(YouKnow106Engine::portamentoTravelForAdcFraction(fraction),
                   travel, 1.0e-5,
                   "the portamento divider inverse does not round-trip");
        previousFraction = fraction;
    }
    // Half knob travel therefore realises raw code 101's glide, not raw 128's.
    expectNear(YouKnow106Engine::portamentoSeconds(
                   YouKnow106Engine::portamentoTravelAdcFraction(0.5f)),
               YouKnow106Engine::portamentoSeconds(101.0f / 255.0f), 0.0f,
               "half knob travel stopped selecting the loaded divider's raw "
               "code");
}

void testConverterQueueAndOutputReference()
{
    using Destination = YouKnow106Engine::ConverterDestination;
    using Write = YouKnow106Engine::ConverterWrite;
    constexpr std::array<Write, YouKnow106Engine::converterWritesPerPass> expected {{
        { Destination::Resonance, -1 },
        { Destination::CommonVca, -1 },
        { Destination::Sub, -1 },
        { Destination::Pitch, 0 },
        { Destination::Pitch, 1 },
        { Destination::Pitch, 2 },
        { Destination::Pitch, 3 },
        { Destination::Pitch, 4 },
        { Destination::Pitch, 5 },
        { Destination::Pwm, -1 },
        { Destination::Vcf, 0 },
        { Destination::VoiceVca, 0 },
        { Destination::Vcf, 1 },
        { Destination::VoiceVca, 1 },
        { Destination::Vcf, 2 },
        { Destination::VoiceVca, 2 },
        { Destination::Vcf, 3 },
        { Destination::VoiceVca, 3 },
        { Destination::Vcf, 4 },
        { Destination::VoiceVca, 4 },
        { Destination::Vcf, 5 },
        { Destination::VoiceVca, 5 },
        { Destination::Noise, -1 }
    }};
    const auto& actual = YouKnow106Engine::converterWriteOrder();
    for (std::size_t index = 0; index < expected.size(); ++index)
        expect(actual[index].destination == expected[index].destination
                   && actual[index].voice == expected[index].voice,
               "B-2 converter write-order mismatch at ordinal "
                   + std::to_string(index));
    const auto resonanceWrites = std::count_if(
        actual.begin(), actual.end(), [](const Write& write) {
            return write.destination == Destination::Resonance
                && write.voice == -1;
        });
    expect(resonanceWrites == 1,
           "a converter pass does not contain exactly one shared resonance write");

    const auto normalized = YouKnow106Engine::converterEventPhases(
        YouKnow106Engine::ConverterTimingProfile::NormalizedServiceChart);
    expect(normalized.front() == 0.0 && normalized.back() < 1.0,
           "normalized converter profile does not fit inside one pass");
    for (std::size_t index = 1; index < normalized.size(); ++index)
        expect(normalized[index] > normalized[index - 1],
               "normalized converter profile collapsed or reordered an event");
    expect(normalized[3] != normalized[8],
           "normalized profile phase-locks the first and sixth DCO writes");

    const auto phaseZero = YouKnow106Engine::converterEventPhases(
        YouKnow106Engine::ConverterTimingProfile::PhaseZeroDiagnostic);
    expect(std::all_of(phaseZero.begin(), phaseZero.end(), [](double phase) {
               return phase == 0.0;
           }),
           "phase-zero diagnostic profile contains an invented timestamp");

    // The default compatibility profile stays the exact ordinal grid; the
    // measured-chart profile below is a selectable comparison, not a new
    // default, so the grid itself is pinned value-for-value.
    for (std::size_t index = 0; index < normalized.size(); ++index)
        expect(normalized[index]
                   == static_cast<double>(index)
                          / static_cast<double>(normalized.size()),
               "the normalized compatibility profile left its ordinal grid");

    // MeasuredChartGeometry carries the 2026-08-20 pixel measurement of the
    // p. 8 chart (drawn-artwork proportions, +/-3 px per stroke, rebased to
    // this queue's RESONANCE-first origin). Its deliberate non-uniformity is
    // asserted here exactly as measured; none of this promotes the drawing
    // to hardware timestamps.
    const auto measured = YouKnow106Engine::converterEventPhases(
        YouKnow106Engine::ConverterTimingProfile::MeasuredChartGeometry);
    expect(measured.front() == 0.0 && measured.back() < 1.0,
           "measured-chart profile does not fit inside one pass");
    for (std::size_t index = 1; index < measured.size(); ++index)
        expect(measured[index] > measured[index - 1],
               "measured-chart profile collapsed or reordered an event");
    constexpr double chartSpanPixels = 1886.5;
    const auto chartPhase = [&](double pixelsFromResonance) {
        return pixelsFromResonance / chartSpanPixels;
    };
    expectNear(measured[1], chartPhase(97.5), 1.0e-12,
               "measured VCA LEVEL phase does not match the chart stroke");
    expectNear(measured[9], chartPhase(905.0), 1.0e-12,
               "measured PWM phase does not match the chart stroke");
    expectNear(measured[10], chartPhase(956.0), 1.0e-12,
               "measured VCF1 phase does not match the chart stroke");
    expectNear(measured[22], chartPhase(1836.0), 1.0e-12,
               "measured NOISE phase does not match the chart stroke");
    // Three drawn width classes (full / VCF-VCA / half on a 10:7:5 grid).
    // The six DCO slots are full-width and near-equal; the VCF/VCA pairs are
    // narrower; VCF6 is the chart's one anomaly at ~1.5 VCF-widths.
    for (std::size_t ordinal = 3; ordinal + 1 <= 9; ++ordinal)
    {
        const double slotSeconds =
            (measured[ordinal + 1] - measured[ordinal]) * 0.0042;
        expect(slotSeconds > 219.0e-6 && slotSeconds < 229.0e-6,
               "a drawn DCO slot left its measured full-width class");
    }
    const double vcf1Slot = (measured[11] - measured[10]) * 0.0042;
    expect(vcf1Slot > 148.0e-6 && vcf1Slot < 166.0e-6,
           "the drawn VCF1 slot left its measured VCF/VCA width class");
    const double vcf6Slot = (measured[21] - measured[20]) * 0.0042;
    expect(vcf6Slot > 228.0e-6 && vcf6Slot < 240.0e-6,
           "the chart's VCF6 width anomaly disappeared");
    // The drawing's largest departure from the ordinal grid is the PWM write,
    // +371.4 us later than 9/23 of the pass at the drawn scale.
    expectNear((measured[9] - normalized[9]) * 0.0042, 371.4e-6, 8.0e-6,
               "the measured chart's PWM deviation from the ordinal grid "
               "moved");

    // The compatibility reference is chosen to make the newly explicit final
    // boundary unity, preserving existing sessions while declaring -18 dBFS
    // RMS as a product convention rather than an analogue property.
    expectNear(YouKnow106Engine::outputReferenceGain(
                   YouKnow106Engine::compatibilityOutputReferenceRmsVolts),
               1.0, 1.0e-6,
               "compatibility output reference does not preserve unity gain");
    expectNear(YouKnow106Engine::outputReferenceGain(1.0f),
               YouKnow106Engine::internalVoltsPerUnit
                   * YouKnow106Engine::minus18DbfsAmplitude,
               1.0e-7, "one-volt RMS output reference violates the boundary law");
    const double lower = 0.25 * YouKnow106Engine::outputReferenceGain(1.0f);
    const double doubled = 0.50 * YouKnow106Engine::outputReferenceGain(1.0f);
    expectNear(20.0 * std::log10(doubled / lower), 6.020599913, 1.0e-7,
               "doubling analogue output does not add 6.0206 dB at the boundary");
    expectNear(YouKnow106Engine::outputReferenceGain(2.0f),
               0.5 * YouKnow106Engine::outputReferenceGain(1.0f), 1.0e-7,
               "doubling Vref does not halve only the final-boundary gain");

    // Vref is a plain function argument, not a clamped panel control, so
    // nothing upstream stops a caller from passing a non-positive or
    // non-finite reference. The guard exists precisely to fall back to unity
    // gain there instead of dividing by zero or a negative figure; the only
    // call site in process() always passes the positive compile-time
    // compatibility constant, so this fallback had no direct coverage.
    expect(YouKnow106Engine::outputReferenceGain(0.0f) == 1.0f,
           "a zero output reference did not fall back to unity gain");
    expect(YouKnow106Engine::outputReferenceGain(-1.0f) == 1.0f,
           "a negative output reference did not fall back to unity gain");
    expect(YouKnow106Engine::outputReferenceGain(
               std::numeric_limits<float>::quiet_NaN()) == 1.0f,
           "a NaN output reference did not fall back to unity gain");

    // Zero, -1 and NaN above all fail the `> 0.0f` positivity half of the
    // guard and short-circuit before the `!std::isfinite` half is ever
    // evaluated, so none of them actually exercises that second branch.
    // Positive infinity is the one input that passes positivity and reaches
    // isfinite specifically, isolating that half of the guard.
    expect(YouKnow106Engine::outputReferenceGain(
               std::numeric_limits<float>::infinity()) == 1.0f,
           "a positive-infinity output reference did not fall back to unity "
           "gain");
}

void testPanelLawsInvert()
{
    // What a control displays is the value the circuit produces, so a host
    // letting someone type that value needs a way back to the travel. Round
    // tripping is the only thing that makes the typed number mean anything.
    const auto roundTrip = [](float position, auto forward, auto inverse,
                              const std::string& name) {
        const float value = forward(position);
        const float back = inverse(value);
        expectNear(back, position, 0.01,
                   name + " does not round trip through its displayed value");
    };

    for (float position = 0.0f; position <= 1.0f; position += 0.125f)
    {
        roundTrip(position, YouKnow106Engine::envelopeAttackSeconds,
                  YouKnow106Engine::panelPositionForAttack, "attack");
        roundTrip(position, YouKnow106Engine::envelopeDecaySeconds,
                  YouKnow106Engine::panelPositionForDecay, "decay");
        roundTrip(position, YouKnow106Engine::envelopeReleaseSeconds,
                  YouKnow106Engine::panelPositionForRelease, "release");
        roundTrip(position, YouKnow106Engine::lfoDelaySeconds,
                  YouKnow106Engine::panelPositionForLfoDelay, "modulation delay");

        // The modulator's rate is quantised onto whole passes, so many
        // positions read the same frequency and a typed value can only land
        // on a canonical one. What must hold is that the position it lands
        // on *produces* the typed frequency.
        const float rate = YouKnow106Engine::lfoRateHz(position);
        expectNear(YouKnow106Engine::lfoRateHz(
                       YouKnow106Engine::panelPositionForLfoRate(rate)),
                   rate, 1.0e-4,
                   "modulation rate does not round trip through its displayed value");
    }

    // Cutoff round trips only while the exponential result remains below the
    // explicit 50 kHz product cap; all higher codes necessarily share it.
    for (float position = 0.0f; position <= 0.80f; position += 0.1f)
        roundTrip(position, [](float p) {
                      return YouKnow106Engine::vcfCutoffHz(
                          YouKnow106Engine::vcfPanelCounts(p));
                  },
                  YouKnow106Engine::panelPositionForCutoff, "cutoff");

    const double uncappedAt13716 = YouKnow106Engine::vcfBaseFrequencyHz
        * std::exp2(13716.0 / YouKnow106Engine::vcfCountsPerOctave);
    expectNear(YouKnow106Engine::vcfCutoffHz(13716.0f), uncappedAt13716,
               uncappedAt13716 * 2.0e-6,
               "the cutoff safety policy introduced a pre-cap knee");
    const float ceilingHz = YouKnow106Engine::vcfCutoffHz(
        YouKnow106Engine::vcfPanelCounts(1.0f));
    expectNear(ceilingHz, 50000.0, 1.0e-3,
               "the panel top does not land on the named 50 kHz product cap");
    expectNear(YouKnow106Engine::vcfCutoffHz(16383.0f), 50000.0, 1.0e-3,
               "the accumulator top does not retain the 50 kHz product cap");

    // Portamento's bottom detent is Off, and both raw-code pairing and 8.8-step
    // projection create plateaus. Its inverse therefore returns a canonical
    // code producing the same displayed time, not necessarily the original
    // position inside that plateau.
    for (float position = 0.1f; position <= 1.0f; position += 0.15f)
    {
        const float seconds = YouKnow106Engine::portamentoSeconds(position);
        expectNear(YouKnow106Engine::portamentoSeconds(
                       YouKnow106Engine::panelPositionForPortamento(seconds)),
                   seconds, 1.0e-6,
                   "portamento does not round trip through its displayed value");
    }
    expectNear(YouKnow106Engine::panelPositionForPortamento(0.0f), 0.0, 1.0e-6,
               "a glide time of zero does not read as switched off");

    // A typed value outside the control's range must land on its nearest end
    // rather than anywhere else.
    expectNear(YouKnow106Engine::panelPositionForCutoff(1.0f), 0.0, 1.0e-6,
               "a cutoff below the range does not clamp to the bottom");
    expectNear(YouKnow106Engine::panelPositionForLfoRate(1000.0f), 1.0, 1.0e-6,
               "a rate above the range does not clamp to the top");
}

void testPanelLawInversesRejectNonPositiveInput()
{
    // Each of these inverse laws is the host's way back from a typed
    // automation-lane string (secondsFromText/hertzFromText in
    // PluginProcessor.cpp) to a panel position, and that parse is entirely
    // capable of handing one a non-positive or non-finite value: an empty
    // or unit-only string parses to zero, a leading '-' parses negative, and
    // the round-trip tests above only ever probe positive, in-range values
    // (panelPositionForPortamento's zero case above is the one exception,
    // and even it was never paired with a negative, NaN or infinite input),
    // so this fallback-to-bottom branch had no direct coverage of its own.
    const auto expectFloorsToZero = [](auto law, const std::string& name) {
        expectNear(law(0.0f), 0.0, 1.0e-9,
                   name + " did not fall back to the panel bottom for a zero input");
        expectNear(law(-1.0f), 0.0, 1.0e-9,
                   name + " did not fall back to the panel bottom for a negative input");
        expectNear(law(std::numeric_limits<float>::quiet_NaN()), 0.0, 1.0e-9,
                   name + " did not fall back to the panel bottom for a NaN input");
        expectNear(law(std::numeric_limits<float>::infinity()), 0.0, 1.0e-9,
                   name + " did not fall back to the panel bottom for a +infinity input");
        expectNear(law(-std::numeric_limits<float>::infinity()), 0.0, 1.0e-9,
                   name + " did not fall back to the panel bottom for a -infinity input");
    };

    expectFloorsToZero(YouKnow106Engine::panelPositionForAttack, "attack");
    expectFloorsToZero(YouKnow106Engine::panelPositionForDecay, "decay");
    expectFloorsToZero(YouKnow106Engine::panelPositionForRelease, "release");
    expectFloorsToZero(YouKnow106Engine::panelPositionForLfoRate, "modulation rate");
    expectFloorsToZero(YouKnow106Engine::panelPositionForLfoDelay, "modulation delay");
    expectFloorsToZero(YouKnow106Engine::panelPositionForCutoff, "cutoff");
    expectFloorsToZero(YouKnow106Engine::panelPositionForPortamento, "portamento");
}

void testComparatorEdgesSitOnOneThreshold()
{
    // The comparator is one threshold on the ramp, so its two edges per cycle
    // have to sit at the same ramp voltage. The falling edge therefore belongs
    // partway into the reset, where the descending segment passes back through
    // that voltage -- not at the start of the reset, where the ramp is still at
    // its positive rail. This solves the ramp's own geometry independently of
    // the model rather than re-deriving the model's formula.
    const auto rampAt = [](double phase, double reset) {
        const double rise = std::max(1.0 - reset, 1.0e-4);
        if (phase < rise)
            return static_cast<double>(YouKnow106Engine::rampSegmentVoltage(
                static_cast<float>(phase / rise)));
        return 1.0 - 2.0 * (phase - rise) / reset;
    };

    for (double reset : { 0.0001, 0.01, 0.055, 0.25 })
    {
        for (float duty : { 0.05f, 0.25f, 0.5f, 0.75f, 0.95f })
        {
            const double riseEdge = YouKnow106Engine::pulseRisePhase(
                duty, static_cast<float>(reset));
            const double fallEdge = YouKnow106Engine::pulseFallPhase(
                duty, static_cast<float>(reset));

            expectNear(rampAt(fallEdge, reset), rampAt(riseEdge, reset), 2.0e-3,
                       "the comparator's two edges are not at the same ramp "
                       "voltage");
            expect(fallEdge > riseEdge,
                   "the comparator falls before it rises");
            expect(fallEdge <= 1.0 + 1.0e-6,
                   "the comparator's falling edge left the cycle");
            // And it is inside the reset segment, not at its start: that is the
            // whole point.
            expect(fallEdge >= 1.0 - reset - 1.0e-6,
                   "the falling edge is not inside the reset segment");
        }
    }

    // With a negligible reset the high interval is the requested duty, so the
    // correction cannot have moved the ordinary case.
    for (float duty : { 0.05f, 0.5f, 0.95f })
    {
        const double high = YouKnow106Engine::pulseFallPhase(duty, 1.0e-6f)
                          - YouKnow106Engine::pulseRisePhase(duty, 1.0e-6f);
        expectNear(high, duty, 1.0e-4,
                   "a vanishing reset does not give the requested duty");
    }
}

void testChorusIsAtItsSettingFromTheFirstSample()
{
    // A patch loaded with the effect switched on is not a player reaching for
    // the button: there is nothing to glide from. Measured on the wet path
    // alone -- the line adds its output to dry at IC6, so subtracting IC6's
    // amplified dry contribution leaves exactly what the effect contributed,
    // with no note onset or modulation depth mixed into the reading.
    constexpr double sampleRate = 192000.0;
    Chorus chorus;
    chorus.prepare(sampleRate);

    const int length = static_cast<int>(sampleRate * 0.5);
    std::vector<float> wet(static_cast<std::size_t>(length));
    for (int index = 0; index < length; ++index)
    {
        const float input = std::sin(2.0f * static_cast<float>(pi) * 220.0f
                                     * static_cast<float>(index)
                                     / static_cast<float>(sampleRate));
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(input, ChorusMode::Two, 0.0f, left, right);
        wet[static_cast<std::size_t>(index)] =
            left - input * Chorus::dryMixGain;
    }

    const auto rms = [&](double fromSeconds, double toSeconds) {
        const auto from = static_cast<std::size_t>(sampleRate * fromSeconds);
        const auto to = std::min(static_cast<std::size_t>(sampleRate * toSeconds),
                                 wet.size());
        double sum = 0.0;
        for (std::size_t index = from; index < to; ++index)
            sum += static_cast<double>(wet[index]) * wet[index];
        return to > from ? std::sqrt(sum / static_cast<double>(to - from)) : 0.0;
    };

    // From the moment the longest delay has filled, against the settled level.
    const double early = rms(0.006, 0.016);
    const double settled = rms(0.4, 0.5);
    expect(settled > 0.1, "the wet path never reached its level at all");
    expectNear(early, settled, settled * 0.1,
               "the effect glided up to its setting instead of starting there");
}

void testJuno60FallbackBucketBrigadeTiming()
{
    // The part is 256 stages clocked in two phases, so its delay is
    // 128 / clock -- which is the datasheet's 12.8 ms at its 10 kHz minimum.
    expectNear(Chorus::delaySecondsForClock(10000.0f), 0.0128, 1.0e-9,
               "line delay at the minimum clock");
    expectNear(Chorus::delaySecondsForClock(200000.0f), 0.00064, 1.0e-9,
               "line delay at the maximum clock");
    expectNear(Chorus::clockForDelaySeconds(Chorus::delaySecondsForClock(43210.0f)),
               43210.0, 1.0e-3, "delay and clock are not reciprocal");

    // Both modes sweep the same delay range and differ only in rate. The rates
    // are this instrument's own, derived from its timing network, the
    // summing-node comparator ratio and the 0.1 uF integrator capacitor; the
    // depth is the family sweep measured on the sibling's identical driver.
    const auto one = Chorus::settingsFor(ChorusMode::One);
    const auto two = Chorus::settingsFor(ChorusMode::Two);

    // The T-network the mode switch drives, straight off the schematic. Assert
    // both legs, because the ratio alone would still pass if each were wrong by
    // the same factor.
    expectNear(Chorus::lfoTimingOhms(true), 6.4352941e6, 1.0,
               "mode I effective timing resistance");
    expectNear(Chorus::lfoTimingOhms(false), 3.9638889e6, 1.0,
               "mode II effective timing resistance");
    expectNear(Chorus::modeRateRatio(), 1.6234799, 1.0e-6,
               "the schematic's mode-rate ratio changed");
    expect(Chorus::lfoTimingOhms(true) > Chorus::lfoTimingOhms(false),
           "mode I must integrate through the larger resistance, so it is the "
           "slower mode");

    // The derivation's own terms, each asserted separately so a drive-by
    // "correction" of one of them fails here rather than in a listening test.
    // The comparator ratio is the summing-node reading; the falsified 1/48
    // divider reading put both modes 34x high and must not come back.
    expectNear(Chorus::lfoThresholdRatio, 33.0 / 47.0, 1.0e-12,
               "the comparator's summing-node ratio changed");
    expectNear(Chorus::lfoIntegratorFarads, 1.0e-7, 1.0e-15,
               "the integrator capacitor is no longer 0.1 uF");

    // The rates the derivation lands, and the relation the schematic fixes:
    // the literals catch a changed term, the ratio catches a wrong split.
    expectNear(one.rateHz, 0.5532933, 1.0e-4, "mode I derived rate");
    expectNear(two.rateHz, 0.8982608, 1.0e-4, "mode II derived rate");
    expectNear(two.rateHz / one.rateHz, Chorus::modeRateRatio(), 1.0e-5,
               "the mode rates do not carry the schematic's timing ratio");
    expect(two.rateHz > one.rateHz,
           "mode II is not the faster leg the manual and the ratio both need");
    // Corroboration, not calibration: scope readings of a 106-chorus clone
    // report 0.537 and 0.879 Hz. If the derivation drifts away from the only
    // 106-specific measurements on record, it is broken even while it stays
    // self-consistent.
    expect(std::abs(one.rateHz / 0.537 - 1.0) < 0.035,
           "mode I left the 106-clone scope reading's 3.5% corroboration band");
    expect(std::abs(two.rateHz / 0.879 - 1.0) < 0.035,
           "mode II left the 106-clone scope reading's 3.5% corroboration band");
    // Roland's published figures read as truncations -- "about 0.5" and
    // "about 0.8" -- and the derived pair still carries them. Assert the
    // floor, not nearest rounding, which 0.898 would fail against 0.8.
    expectNear(std::floor(one.rateHz * 10.0) / 10.0, 0.5, 1.0e-9,
               "mode I no longer truncates to the published about-0.5 Hz");
    expectNear(std::floor(two.rateHz * 10.0) / 10.0, 0.8, 1.0e-9,
               "mode II no longer truncates to the published about-0.8 Hz");
    // The superseded sibling scale must stay superseded, ratio and geometric
    // mean alike. Fail loudly if either comes back.
    expect(std::abs(two.rateHz / one.rateHz
                    - Chorus::siblingMeasuredRateTwoHz
                          / Chorus::siblingMeasuredRateOneHz) > 1.0e-3,
           "the sibling's 1.682 rate ratio was reintroduced");
    expect(std::abs(std::sqrt(one.rateHz * two.rateHz)
                    - std::sqrt(Chorus::siblingMeasuredRateOneHz
                                * Chorus::siblingMeasuredRateTwoHz)) > 1.0e-3,
           "the sibling's absolute scale was reintroduced");

    expectNear(one.sweepSeconds, two.sweepSeconds, 1.0e-9,
               "the two modes do not share a sweep depth");
    // The 106's own third-party-measured sweep: 1.4-6.4 ms scoped on a
    // designator-faithful p. 15 build with genuine MN3009s and compared
    // directly against a real JUNO-106 by its owner. The superseded
    // sibling-instrument capture (1.66-5.35 ms, Juno-60) must stay
    // superseded; a calibrated original-unit capture remains OQ-01.
    expectNear(one.centreDelaySeconds - one.sweepSeconds, 0.0014, 1.0e-6,
               "shortest modulated delay");
    expectNear(one.centreDelaySeconds + one.sweepSeconds, 0.0064, 1.0e-6,
               "longest modulated delay");
    expect(std::abs(one.centreDelaySeconds - one.sweepSeconds - 0.00166)
               > 1.0e-4,
           "the sibling's 1.66 ms endpoint was reintroduced");
    // Both ends have to land inside the part's own clock window, which is what
    // says the capture describes this circuit rather than some other one: 256
    // stages give a delay of 128 / f_clock, and the MN3009 is rated 10-200 kHz.
    for (const double delay : { one.centreDelaySeconds - one.sweepSeconds,
                                one.centreDelaySeconds + one.sweepSeconds })
    {
        const double clockHz = 128.0 / delay;
        expect(clockHz > 10.0e3 && clockHz < 200.0e3,
               "a sweep endpoint needs a clock outside the part's rated window");
    }
    const auto off = Chorus::settingsFor(ChorusMode::Off);
    expectNear(off.wetGain, 0.0, 1.0e-9,
               "the wet path is not silent when the effect is switched out");
    expectNear(off.sweepSeconds, one.sweepSeconds, 1.0e-9,
               "bypass stopped the modulation behind the wet mute");
    expectNear(off.centreDelaySeconds, one.centreDelaySeconds, 1.0e-9,
               "bypass moved the running delay line away from mode I");
    // Wet over dry is the ratio of the two input resistors into IC6's shared
    // 100 kOhm feedback: dry arrives through R71/R73 47 kOhm off the IC2b
    // bus, wet through R72/R74 39 kOhm from the mute JFETs (Service Notes
    // p. 15, designator-level read). Keep both absolute gains as well as
    // their ratio: the absolute factor belongs after the nonlinear BBD and
    // cannot be folded into its input without changing the sound.
    expectNear(Chorus::dryMixGain, 100.0 / 47.0, 1.0e-6,
               "IC6 dry gain");
    expectNear(Chorus::wetMixGain, 100.0 / 39.0, 1.0e-6,
               "IC6 wet gain");
    expectNear(one.wetGain, 47.0 / 39.0, 1.0e-3, "line gain");
    expectNear(20.0 * std::log10(one.wetGain), 1.62, 0.01,
               "the wet path does not sit 1.62 dB above the dry");
    expectNear(Chorus::wetMuteTimeConstantSeconds, 0.005, 1.0e-9,
               "the labelled product mute time constant changed");
    expectNear(Chorus::wetMuteTimeConstantSeconds * std::log(9.0f),
               0.010986, 1.0e-6,
               "the mute's 10-90% duration is not its 5 ms exponential tau");

    // Observe the dry law through the actual processor too. With the effect
    // off, the wet return is muted but both BBDs keep running behind it.
    {
        Chorus chorus;
        chorus.prepare(48000.0);
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(0.1f, ChorusMode::Off, 0.0f, left, right);
        expectNear(left, 0.1 * Chorus::dryMixGain, 1.0e-6,
                   "IC6 dry gain is absent from the rendered chorus output");
        expectNear(right, left, 1.0e-9,
                   "chorus bypass moved the dry signal off centre");
    }

    // The product supports the user-requested both-button combination, while
    // neither the original control path nor the audited Roland Cloud model
    // provides a third analogue rate. The established summed rate is therefore
    // explicit compatibility policy, not a conductance derivation. Keep it
    // stable and distinct until a qualifying original-unit capture replaces it.
    const auto both = Chorus::settingsFor(ChorusMode::OneTwo);
    expectNear(both.rateHz, static_cast<double>(one.rateHz) + two.rateHz, 1.0e-7,
               "I+II lost its established summed-rate policy");
    expect(both.rateHz > two.rateHz,
           "I+II is not audibly distinct from mode II by rate");
    expectNear(both.sweepSeconds, one.sweepSeconds, 1.0e-9,
               "I+II changed the shared sweep depth");
    expectNear(both.centreDelaySeconds, one.centreDelaySeconds, 1.0e-9,
               "I+II changed the shared centre delay");
    expectNear(both.wetGain, one.wetGain, 1.0e-9,
               "I+II changed the shared line gain");
    expectNear(Chorus::measuredModeNoiseGain(ChorusMode::OneTwo),
               Chorus::measuredModeNoiseGain(ChorusMode::Two), 1.0e-9,
               "I+II changed its provisional II noise profile without a capture");

    // The original-unit recollection that prompted the I+II correction is
    // qualitative (near-mono), so the implementation must not hide a fitted
    // width constant. It folds the established two wet returns to their exact
    // mid: side is zero, while the mono sum is unchanged from the former wide
    // path and therefore retains its characteristic comb colour.
    {
        Chorus narrow;
        Chorus wide;
        narrow.prepare(48000.0);
        wide.prepare(48000.0);
        double maximumNarrowSide = 0.0;
        double maximumWideSide = 0.0;
        double maximumMonoDifference = 0.0;
        for (int sample = 0; sample < 24000; ++sample)
        {
            const float input = static_cast<float>(
                0.22 * std::sin(2.0 * pi * 311.0 * sample / 48000.0)
                + 0.13 * std::sin(2.0 * pi * 997.0 * sample / 48000.0));
            float narrowLeft = 0.0f, narrowRight = 0.0f;
            float wideLeft = 0.0f, wideRight = 0.0f;
            narrow.process(input, ChorusMode::OneTwo, 0.0f,
                           narrowLeft, narrowRight,
                           false, false, 1.0f, false, true);
            wide.process(input, ChorusMode::OneTwo, 0.0f,
                         wideLeft, wideRight,
                         false, false, 1.0f, false, false);
            maximumNarrowSide = std::max(
                maximumNarrowSide,
                std::abs(static_cast<double>(narrowLeft - narrowRight)));
            maximumWideSide = std::max(
                maximumWideSide,
                std::abs(static_cast<double>(wideLeft - wideRight)));
            maximumMonoDifference = std::max(
                maximumMonoDifference,
                std::abs(static_cast<double>(narrowLeft + narrowRight)
                         - static_cast<double>(wideLeft + wideRight)));
        }
        expect(maximumNarrowSide < 1.0e-7,
               "the narrow I+II path retained an unsupported stereo side");
        expect(maximumWideSide > 1.0e-3,
               "the legacy I+II A/B path did not expose its former width");
        expect(maximumMonoDifference < 2.0e-6,
               "narrowing I+II changed its established mono sum");
    }
    expectNear(Chorus::measuredModeNoiseGain(ChorusMode::One), 1.0, 1.0e-9,
               "the empirical mode calibration moved mode I's product anchor");
    expectNear(Chorus::measuredModeNoiseGain(ChorusMode::Off), 1.0, 1.0e-9,
               "chorus bypass invented a separate noise calibration");

    // Bypass only mutes the return. It must not silently reset the hidden
    // oscillator/noise profile to I while the last-selected II clock continues
    // to run, or the two pieces of the model would contradict each other.
    {
        Chorus chorus;
        chorus.prepare(48000.0);
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(0.0f, ChorusMode::Two, 0.0f, left, right);
        expect(YouKnow106TestAccess::runningChorusMode(chorus)
                   == ChorusMode::Two,
               "mode II did not select its hidden running noise profile");
        chorus.process(0.0f, ChorusMode::Off, 0.0f, left, right);
        expect(YouKnow106TestAccess::runningChorusMode(chorus)
                   == ChorusMode::Two,
               "chorus bypass reset the hidden mode-II noise profile");
        chorus.process(0.0f, ChorusMode::One, 0.0f, left, right);
        expect(YouKnow106TestAccess::runningChorusMode(chorus)
                   == ChorusMode::One,
               "mode I did not replace the hidden running noise profile");
        chorus.process(0.0f, ChorusMode::OneTwo, 0.0f, left, right);
        expect(YouKnow106TestAccess::runningChorusMode(chorus)
                   == ChorusMode::OneTwo,
               "I+II was canonicalised before selecting its DSP profile");
        chorus.process(0.0f, ChorusMode::Off, 0.0f, left, right);
        expect(YouKnow106TestAccess::runningChorusMode(chorus)
                   == ChorusMode::OneTwo,
               "chorus bypass discarded the hidden I+II profile");
    }

    // And it has to be observable, not just tabulated: run the effect in each
    // mode and count how far the modulation oscillator actually travels.
    const auto phaseTravel = [](ChorusMode mode) {
        Chorus chorus;
        chorus.prepare(48000.0);
        float left = 0.0f, right = 0.0f;
        double previous = chorus.getLfoPhase();
        double travel = 0.0;
        for (int n = 0; n < 48000; ++n)
        {
            chorus.process(0.0f, mode, 0.0f, left, right);
            const double now = chorus.getLfoPhase();
            travel += now >= previous ? now - previous : now + 1.0 - previous;
            previous = now;
        }
        return travel;
    };
    const double travelOne = phaseTravel(ChorusMode::One);
    const double travelTwo = phaseTravel(ChorusMode::Two);
    const double travelBoth = phaseTravel(ChorusMode::OneTwo);
    const double travelOff = phaseTravel(ChorusMode::Off);
    expectNear(travelBoth, travelOne + travelTwo, 1.0e-4,
               "rendered I+II did not retain the summed-rate policy");
    expect(travelBoth > travelTwo,
           "rendered I+II is not distinct from mode II");
    expectNear(travelOff, travelOne, 1.0e-6,
               "the chorus oscillator stopped or changed speed in bypass");
    // The derived ratio has to survive all the way to rendered audio, not just
    // sit in the table the renderer reads.
    expectNear(travelTwo / travelOne, Chorus::modeRateRatio(), 1.0e-3,
               "the rendered mode rates do not carry the schematic ratio");

    // The whole modulated range must stay inside the part's rated clock window,
    // otherwise the model would be running a part outside its specification.
    const float slowest = Chorus::clockForDelaySeconds(
        one.centreDelaySeconds + one.sweepSeconds);
    const float fastest = Chorus::clockForDelaySeconds(
        one.centreDelaySeconds - one.sweepSeconds);
    expect(slowest >= Chorus::minimumClockHz && fastest <= Chorus::maximumClockHz,
           "modulation drives the delay line outside its rated clock range");

    // Two lines clocked in antiphase must actually differ, and the effect must
    // be silent -- apart from its own noise -- when switched out.
    Chorus chorus;
    chorus.prepare(192000.0);
    double difference = 0.0;
    for (int index = 0; index < 96000; ++index)
    {
        const float input = std::sin(2.0f * static_cast<float>(pi) * 220.0f
                                     * static_cast<float>(index) / 192000.0f);
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(input, ChorusMode::Two, 1.0f, left, right);
        if (index > 48000)
            difference += static_cast<double>(left - right) * (left - right);
    }
    expect(difference > 1.0, "the two delay lines are producing the same signal");
}

// IEC 61672 A-weighting. The engine has none -- nothing it renders is
// weighted -- so this is the suite's own, and it is spelled out rather than
// named because the datasheet row checked below is an A-weighted figure and
// the weighting is therefore part of the measurand. The analogue prototype is
//
//   H(s) = K s^4 / ((s + w1)^2 (s + w2)(s + w3)(s + w4)^2),
//
// with f1 = 20.598997, f2 = 107.65265, f3 = 737.86223 and f4 = 12194.217 Hz,
// discretised by the plain bilinear transform and normalised to unity at
// 1 kHz. At the 192 kHz rate this suite uses it reads -39.57, -19.14, 0.00,
// +0.96 and -4.38 dB at 31.5 Hz, 100 Hz, 1 kHz, 4 kHz and 12.5 kHz, against
// the standard's -39.4, -19.1, 0.0, +1.0 and -4.3 dB.
class AWeightingFilter
{
public:
    explicit AWeightingFilter(double sampleRate) noexcept
    {
        const double w1 = 2.0 * pi * 20.598997;
        const double w2 = 2.0 * pi * 107.65265;
        const double w3 = 2.0 * pi * 737.86223;
        const double w4 = 2.0 * pi * 12194.217;
        const double k = 2.0 * sampleRate;
        const auto section = [&](double wa, double wb, bool zeroAtDc,
                                 Section& out) {
            const double a0 = k * k + k * (wa + wb) + wa * wb;
            out.a1 = 2.0 * (wa * wb - k * k) / a0;
            out.a2 = (k * k - k * (wa + wb) + wa * wb) / a0;
            out.b0 = (zeroAtDc ? k * k : 1.0) / a0;
            out.b1 = (zeroAtDc ? -2.0 * k * k : 2.0) / a0;
            out.b2 = (zeroAtDc ? k * k : 1.0) / a0;
        };
        section(w1, w1, true, first_);
        section(w2, w3, true, second_);
        section(w4, w4, false, third_);

        const double w = 2.0 * pi * 1000.0 / sampleRate;
        gain_ = 1.0 / std::abs(first_.response(w) * second_.response(w)
                             * third_.response(w));
    }

    double step(double input) noexcept
    {
        return gain_ * third_.step(second_.step(first_.step(input)));
    }

private:
    struct Section
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

        double step(double x) noexcept
        {
            const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }

        [[nodiscard]] std::complex<double> response(double w) const noexcept
        {
            const std::complex<double> z =
                std::exp(std::complex<double>(0.0, -w));
            return (b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z);
        }
    };

    Section first_, second_, third_;
    double gain_ = 1.0;
};

void testChorusRateProportionalNoiseGainMatchesTheDerivedRatio()
{
    // `Chorus::rateProportionalNoiseGain` is the causal alternative to the
    // empirical 3.95 dB mode-II calibration: process() substitutes it whole
    // rather than multiplying the two, and its header comment states its
    // exact contract -- it "predicts exactly 20*log10(modeRateRatio())". No
    // test called this public static function directly before now; the only
    // existing coverage (testChorusNoiseProfilesReproduceTheMeasuredModeDelta
    // in the engine suite) measures the resulting audio energy through a full
    // engine render across a whole modulation cycle, which is close enough to
    // catch a badly broken hypothesis but cannot distinguish "close to the
    // formula" from "exactly the formula" the way a direct call can.
    const auto one = Chorus::settingsFor(ChorusMode::One);
    const auto two = Chorus::settingsFor(ChorusMode::Two);

    // Mode I is the reference leg: process() documents that both noise
    // profiles "reference mode I and therefore preserve its established floor
    // exactly", so its own rate must map to exactly unity gain, not merely
    // something close to it.
    expectNear(Chorus::rateProportionalNoiseGain(one.rateHz), 1.0, 1.0e-9,
               "mode I's own rate did not stay the hypothesis's unity "
               "reference");
    expectNear(Chorus::rateProportionalNoiseGain(two.rateHz),
               Chorus::modeRateRatio(), 1.0e-6,
               "mode II's rate-proportional gain no longer carries the "
               "schematic's own mode-rate ratio");

    // The function is a public static entry point that process() always
    // calls with a positive, finite rate -- but nothing stops another caller
    // from passing it something else, and the implementation's own guard
    // exists precisely to fall back to unity rather than dividing by, or
    // returning, something non-finite. None of these three inputs reach the
    // guard through process(), so it had no direct coverage of its own.
    expect(Chorus::rateProportionalNoiseGain(0.0f) == 1.0f,
           "a zero rate did not fall back to unity gain");
    expect(Chorus::rateProportionalNoiseGain(-1.0f) == 1.0f,
           "a negative rate did not fall back to unity gain");
    expect(Chorus::rateProportionalNoiseGain(
               std::numeric_limits<float>::quiet_NaN()) == 1.0f,
           "a NaN rate did not fall back to unity gain");
}

void testChorusNoiseMeasurementPointsAndProductPolicy()
{
    // Panasonic's 0.2 mVrms row is a MAXIMUM at the part output with a
    // 100 kHz clock, 100 kOhm load and A weighting. It is not a target after
    // Roland's external tap sum and reconstruction filters. Guard the model's
    // raw composite BBD node at those fixed clock conditions first, with no
    // lower fence: a quieter part is conforming.
    constexpr double datasheetMaximum = 0.200e-3;
    constexpr double estimatorMargin = 1.0057900;  // +0.05 dB
    constexpr double rawSampleRate = 192000.0;
    constexpr float datasheetClockHz = 100000.0f;
    {
        Chorus raw;
        raw.prepare(rawSampleRate);
        AWeightingFilter weight(rawSampleRate);
        const auto settle = static_cast<long long>(rawSampleRate * 0.5);
        const auto window = static_cast<long long>(rawSampleRate * 16.0);
        double sum = 0.0;
        for (long long sample = 0; sample < settle + window; ++sample)
        {
            const double volts = static_cast<double>(
                YouKnow106TestAccess::processBbdCore(
                    raw, 0.0f, datasheetClockHz,
                    static_cast<float>(rawSampleRate), 1.0f))
                * static_cast<double>(Chorus::nodeVoltsPerUnit);
            const double weighted = weight.step(volts);
            if (sample >= settle)
                sum += weighted * weighted;
        }
        const double rawAWeighted = std::sqrt(
            sum / static_cast<double>(window));
        expect(rawAWeighted
                   <= datasheetMaximum * estimatorMargin,
               "fixed-100-kHz raw BBD noise is "
                   + std::to_string(rawAWeighted * 1000.0)
                   + " mVrms A-weighted, above Panasonic's 0.200 mVrms max");
    }

    // HISS 100% separately retains the established product convention: the
    // recovered wet line is normalised to the same numerical 0.200 mVrms.
    // That is a declared post-board target, not Panasonic's measurement point.
    //
    // The measurand, stated in full because every part of it moves the number:
    //
    //  * Both 176.4 and 192 kHz: the two internal grids shipping HQ reaches.
    //    A coarser grid folds more of the held sequence back into the band, so
    //    rate is part of the measurand rather than an incidental fixture
    //    choice. The exact output support keeps these HQ families within
    //    0.004 dB; HQ-off remains a separately documented numerical error.
    //  * Silence in. Both output channels. The wet line is recovered by
    //    undoing IC6's dry and wet summing gains only, and referred to volts
    //    through the 2.6 V node coordinate; Roland's reconstruction sections
    //    remain in because this second measurement is the product policy.
    //  * A 0.5 s settle for the wet-mute glide and the support filters, then a
    //    16 s window. Both clock programmes are measured after dividing out
    //    the empirical instrument-output II-I factor; that reported ~3.95 dB
    //    observation belongs to the engine contract, not the standalone row.
    //
    // The default position is preserved for session compatibility. It is not
    // derived from the incompatible 1.5 V input-swing and 88 dB maximum-output
    // S/N rows.
    constexpr double target =
        Chorus::productWetLineNoiseTargetAWeightedVrms;
    expectNear(static_cast<double>(Chorus::defaultNoiseScale),
               0.29858038, 1.0e-8,
               "the session-compatible default HISS policy changed");
    std::array<double, 4> recoveredByRateAndMode {};
    std::size_t resultIndex = 0;

    for (const double sampleRate : { 176400.0, 192000.0 })
    {
        for (const auto mode : { ChorusMode::One, ChorusMode::Two })
        {
            Chorus chorus;
            chorus.prepare(sampleRate);
            AWeightingFilter weightLeft(sampleRate);
            AWeightingFilter weightRight(sampleRate);
            // process() returns dryMixGain * (input + wet * wetToDryGain) on
            // each side, so with silence in this recovers the line output.
            const double recover = static_cast<double>(Chorus::nodeVoltsPerUnit)
                / (static_cast<double>(Chorus::dryMixGain)
                   * static_cast<double>(Chorus::wetToDryGain));

            const auto advance = [&](long long samples, double* sum) {
                for (long long index = 0; index < samples; ++index)
                {
                    float left = 0.0f;
                    float right = 0.0f;
                    const float isolatePartScale =
                        1.0f / Chorus::measuredModeNoiseGain(mode);
                    chorus.process(
                        0.0f, mode, isolatePartScale, left, right);
                    const double weightedLeft = weightLeft.step(
                        static_cast<double>(left) * recover);
                    const double weightedRight = weightRight.step(
                        static_cast<double>(right) * recover);
                    if (sum != nullptr)
                        *sum += weightedLeft * weightedLeft
                              + weightedRight * weightedRight;
                }
            };

            const auto settle = static_cast<long long>(sampleRate * 0.5);
            const auto window = static_cast<long long>(sampleRate * 16.0);
            advance(settle, nullptr);
            double sum = 0.0;
            advance(window, &sum);
            const double recovered =
                std::sqrt(sum / static_cast<double>(window * 2));
            recoveredByRateAndMode[resultIndex++] = recovered;

            const std::string label = mode == ChorusMode::One ? "I" : "II";
            const std::string fixture = label + " at "
                + std::to_string(static_cast<int>(sampleRate)) + " Hz";
            expect(std::abs(20.0 * std::log10(recovered / target)) < 0.15,
                   "chorus " + fixture
                       + " wet-line noise left the product target by "
                       + std::to_string(20.0 * std::log10(recovered / target))
                       + " dB");
        }
    }

    const auto [minimumRecovered, maximumRecovered] = std::minmax_element(
        recoveredByRateAndMode.begin(), recoveredByRateAndMode.end());
    const double hqSpreadDb = 20.0 * std::log10(
        *maximumRecovered / *minimumRecovered);
    expect(hqSpreadDb < 0.05,
           "chorus noise changed across HQ families/modes by "
               + std::to_string(hqSpreadDb) + " dB");

    // The derivation is one equation and the header publishes every term of
    // it. Re-solving it here stops the amplitude from being edited to a number
    // that no longer answers to the datasheet row it cites -- the render
    // assertions above would still pass if the transfer were quietly retuned
    // to match a hand-picked amplitude, and this one would not.
    expectNear(static_cast<double>(Chorus::independentLineRandomAmplitude)
                   * static_cast<double>(Chorus::nodeVoltsPerUnit)
                   * static_cast<double>(
                       Chorus::productWetLineAWeightedTransfer),
               static_cast<double>(
                   Chorus::productWetLineNoiseTargetAWeightedVrms),
               1.0e-9,
               "the line-noise amplitude no longer solves its product policy");
}

void testChorusNoiseComponents()
{
    constexpr float sampleRate = 48000.0f;
    constexpr int renderLength = 16384;

    // The existing independent per-line xorshift sources retain fixed seeds.
    // Two fresh instances must therefore render bit-identically, including the
    // asynchronous clock-edge sequence on which that noise is generated.
    Chorus first;
    Chorus second;
    first.prepare(sampleRate);
    second.prepare(sampleRate);
    bool identical = true;
    double independentEnergy = 0.0;
    for (int index = 0; index < renderLength; ++index)
    {
        float firstLeft = 0.0f;
        float firstRight = 0.0f;
        float secondLeft = 0.0f;
        float secondRight = 0.0f;
        first.process(0.0f, ChorusMode::Two, 1.0f, firstLeft, firstRight);
        second.process(0.0f, ChorusMode::Two, 1.0f, secondLeft, secondRight);
        identical = identical && firstLeft == secondLeft
                              && firstRight == secondRight;
        independentEnergy += static_cast<double>(firstLeft) * firstLeft
                           + static_cast<double>(firstRight) * firstRight;
    }
    expect(identical, "fixed chorus-noise seeds did not reproduce exactly");
    expect(independentEnergy > 0.0,
           "the preserved independent wet-line component is silent");

    // Explicit zeroes for every new component are the production default.
    // Keep this as an exact comparison so merely splitting the architecture
    // cannot alter compatibility renders through an extra add or multiply.
    Chorus explicitZero;
    Chorus implicitZero;
    explicitZero.prepare(sampleRate);
    implicitZero.prepare(sampleRate);
    YouKnow106TestAccess::configureOptionalChorusNoise(
        explicitZero, 0.0f, -0.75f, 0.0f, 60.0f, 0.0f, 2.0f);
    bool zeroProfileIsTransparent = true;
    for (int index = 0; index < 4096; ++index)
    {
        const float input = static_cast<float>(
            0.1 * std::sin(2.0 * pi * 173.0 * index / sampleRate));
        float explicitLeft = 0.0f;
        float explicitRight = 0.0f;
        float implicitLeft = 0.0f;
        float implicitRight = 0.0f;
        explicitZero.process(input, ChorusMode::One, 1.0f,
                             explicitLeft, explicitRight);
        implicitZero.process(input, ChorusMode::One, 1.0f,
                             implicitLeft, implicitRight);
        zeroProfileIsTransparent = zeroProfileIsTransparent
            && explicitLeft == implicitLeft && explicitRight == implicitRight;
    }
    expect(zeroProfileIsTransparent,
           "disabled optional chorus-noise components changed the render");

    // A single master still defeats independent, common, hum and clock-spur
    // hypotheses together.  The non-zero values below are synthetic fixtures,
    // explicitly not hardware calibration.
    Chorus masterMuted;
    masterMuted.prepare(sampleRate);
    YouKnow106TestAccess::configureOptionalChorusNoise(
        masterMuted, 0.01f, 0.4f, 0.01f, 50.0f, 0.01f, 1.0f);
    bool masterSilencesEverything = true;
    for (int index = 0; index < 2048; ++index)
    {
        float left = 0.0f;
        float right = 0.0f;
        masterMuted.process(0.0f, ChorusMode::Two, 0.0f, left, right);
        masterSilencesEverything = masterSilencesEverything
            && left == 0.0f && right == 0.0f;
    }
    expect(masterSilencesEverything,
           "noiseScale no longer mutes every declared chorus-noise component");

    // Including the heterodyne clock bleed, which the loop above cannot reach:
    // it passes the default sixth argument, so enableClockBleed is false.
    // A revision scaled the bleed by max(noiseScale, 0.1f), leaving a tenth of
    // it alive at zero and contradicting this class's own contract.
    Chorus bleedMuted;
    bleedMuted.prepare(sampleRate);
    YouKnow106TestAccess::configureOptionalChorusNoise(
        bleedMuted, 0.01f, 0.4f, 0.01f, 50.0f, 0.01f, 1.0f);
    bool bleedIsMutedToo = true;
    for (int index = 0; index < 2048; ++index)
    {
        float left = 0.0f;
        float right = 0.0f;
        bleedMuted.process(0.0f, ChorusMode::Two, 0.0f, left, right, true);
        bleedIsMutedToo = bleedIsMutedToo && left == 0.0f && right == 0.0f;
    }
    expect(bleedIsMutedToo,
           "the Chorus Noise master no longer defeats the clock bleed");

    // The bleed and the optional clock-spur hypothesis are separate mechanisms
    // at the same modulated clock. They kept one phase accumulator between
    // them, so enabling both advanced it twice a sample and doubled each
    // tone's frequency. Enabling the bleed must not move the spur.
    const auto spurOnly = [&](bool withBleed) {
        Chorus chorus;
        chorus.prepare(sampleRate);
        YouKnow106TestAccess::configureOptionalChorusNoise(
            chorus, 0.0f, 0.0f, 0.0f, 50.0f, 0.02f, 1.0f);
        std::vector<float> captured(1024);
        for (std::size_t index = 0; index < captured.size(); ++index)
        {
            float left = 0.0f;
            float right = 0.0f;
            chorus.process(0.0f, ChorusMode::Two, 1.0f, left, right, withBleed);
            // Subtract the bleed's own contribution by taking the difference
            // of the two channels, which the spur and bleed both drive but
            // with independent clocks -- any doubling shows as a mismatch.
            captured[index] = left;
        }
        return captured;
    };
    const auto withoutBleed = spurOnly(false);
    const auto withBleed = spurOnly(true);
    double spurDrift = 0.0;
    for (std::size_t index = 0; index < withoutBleed.size(); ++index)
        spurDrift = std::max(spurDrift,
                             std::abs(static_cast<double>(withBleed[index])
                                    - static_cast<double>(withoutBleed[index])));
    // The bleed itself is 0.005 at full scale, so anything much beyond that
    // means the spur's own frequency moved rather than a tone being added.
    expect(spurDrift < 0.02,
           "enabling the clock bleed displaced the optional clock spur by "
               + std::to_string(spurDrift)
               + ", so the two are sharing a phase accumulator again");

    // The common layer is built from one common and one orthogonal seeded
    // process. rho=+1 duplicates channels exactly; rho=-1 changes only sign.
    constexpr std::size_t syntheticLength = 2048;
    std::vector<float> positiveA(syntheticLength);
    std::vector<float> positiveB(syntheticLength);
    std::uint32_t commonOne = 0xd1b54a35u;
    std::uint32_t orthogonalOne = 0x94d049bbu;
    std::uint32_t commonReplay = commonOne;
    std::uint32_t orthogonalReplay = orthogonalOne;
    bool replayedExactly = true;
    bool duplicatedExactly = true;
    for (std::size_t index = 0; index < syntheticLength; ++index)
    {
        const auto sample = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonOne, orthogonalOne, 1.0f);
        const auto replay = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonReplay, orthogonalReplay, 1.0f);
        positiveA[index] = sample[0];
        positiveB[index] = sample[1];
        replayedExactly = replayedExactly
            && sample[0] == replay[0] && sample[1] == replay[1];
        duplicatedExactly = duplicatedExactly && sample[0] == sample[1];
    }
    expect(replayedExactly,
           "the synthetic common component did not replay from fixed seeds");
    expect(duplicatedExactly,
           "rho=+1 did not duplicate the synthetic common component");

    std::uint32_t commonNegative = 0xd1b54a35u;
    std::uint32_t orthogonalNegative = 0x94d049bbu;
    bool invertedExactly = true;
    for (std::size_t index = 0; index < syntheticLength; ++index)
    {
        const auto sample = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonNegative, orthogonalNegative, -1.0f);
        invertedExactly = invertedExactly && sample[1] == -sample[0];
    }
    expect(invertedExactly,
           "rho=-1 did not invert the synthetic common component");

    // Coherence is averaged over independent DFT blocks, rather than using a
    // single-block identity that would read one for any two non-zero vectors.
    constexpr int blockLength = 256;
    constexpr int blockCount = 8;
    constexpr int coherenceBin = 23;
    double autoA = 0.0;
    double autoB = 0.0;
    std::complex<double> cross {};
    for (int block = 0; block < blockCount; ++block)
    {
        std::complex<double> spectrumA {};
        std::complex<double> spectrumB {};
        for (int index = 0; index < blockLength; ++index)
        {
            const double angle = -2.0 * pi * coherenceBin * index / blockLength;
            const auto rotation = std::exp(std::complex<double>(0.0, angle));
            const auto offset = static_cast<std::size_t>(block * blockLength + index);
            spectrumA += static_cast<double>(positiveA[offset]) * rotation;
            spectrumB += static_cast<double>(positiveB[offset]) * rotation;
        }
        autoA += std::norm(spectrumA);
        autoB += std::norm(spectrumB);
        cross += spectrumA * std::conj(spectrumB);
    }
    const double coherence = std::norm(cross) / (autoA * autoB);
    expectNear(coherence, 1.0, 1.0e-12,
               "duplicated synthetic channels do not have coherence one");

    // Establish the A/B cross-spectrum convention explicitly.  Swapping the
    // line labels exchanges their individual spectra and conjugates A*conj(B).
    std::uint32_t commonMixed = 0x243f6a89u;
    std::uint32_t orthogonalMixed = 0xb7e15163u;
    std::vector<float> mixedA(4096);
    std::vector<float> mixedB(4096);
    for (std::size_t index = 0; index < mixedA.size(); ++index)
    {
        const auto sample = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonMixed, orthogonalMixed, 0.35f);
        mixedA[index] = sample[0];
        mixedB[index] = sample[1];
    }
    for (const int bin : { 19, 113, 509 })
    {
        std::complex<double> spectrumA {};
        std::complex<double> spectrumB {};
        for (std::size_t index = 0; index < mixedA.size(); ++index)
        {
            const double angle = -2.0 * pi * bin * index / mixedA.size();
            const auto rotation = std::exp(std::complex<double>(0.0, angle));
            spectrumA += static_cast<double>(mixedA[index]) * rotation;
            spectrumB += static_cast<double>(mixedB[index]) * rotation;
        }
        const auto originalCross = spectrumA * std::conj(spectrumB);
        const auto swappedCross = spectrumB * std::conj(spectrumA);
        const double originalPowerA = std::norm(spectrumA);
        const double originalPowerB = std::norm(spectrumB);
        const double swappedPowerA = std::norm(spectrumB);
        const double swappedPowerB = std::norm(spectrumA);
        expectNear(swappedPowerA, originalPowerB, 0.0,
                   "line swap changed B's individual spectrum");
        expectNear(swappedPowerB, originalPowerA, 0.0,
                   "line swap changed A's individual spectrum");
        expectNear(swappedCross.real(), std::conj(originalCross).real(), 1.0e-9,
                   "line swap changed cross-spectrum magnitude");
        expectNear(swappedCross.imag(), std::conj(originalCross).imag(), 1.0e-9,
                   "line swap did not conjugate the cross-spectrum");
    }

    // Hum and clock feedthrough are deterministic oscillators even though
    // their production amplitudes are zero and their frequencies are unknown.
    double tonePhase = 0.0;
    double replayPhase = 0.0;
    bool toneReplayed = true;
    double toneEnergy = 0.0;
    for (int index = 0; index < 2048; ++index)
    {
        const float tone = YouKnow106TestAccess::chorusToneStep(
            tonePhase, 997.0f, sampleRate);
        const float replay = YouKnow106TestAccess::chorusToneStep(
            replayPhase, 997.0f, sampleRate);
        toneReplayed = toneReplayed && tone == replay;
        toneEnergy += static_cast<double>(tone) * tone;
    }
    expect(toneReplayed && toneEnergy > 100.0,
           "the deterministic hum/clock-spur oscillator is not reproducible");
}

void testChorusToneStepFallbackGuard()
{
    // Both production call sites hand `deterministicToneStep` sampleRate_
    // (fixed positive by `prepare()`) and a finite frequency -- a configured
    // hum frequency or a clock rate times a harmonic multiplier, never
    // user-facing values that could carry a NaN or an infinity. So the
    // fixture above, and every full-engine render, only ever exercises this
    // function's ordinary path; the non-finite/non-positive-rate fallback
    // guard has never fired outside a test. Poison each argument in turn and
    // confirm the guard reports silence and leaves the caller's phase alone,
    // then confirm one clean call afterwards still advances normally instead
    // of continuing to propagate the poison.
    constexpr float sampleRate = 48000.0f;
    double phase = 0.25;

    expect(YouKnow106TestAccess::chorusToneStep(
               phase, std::numeric_limits<float>::quiet_NaN(), sampleRate)
               == 0.0f,
           "deterministicToneStep did not fall back to silence for a NaN frequency");
    expect(phase == 0.25,
           "deterministicToneStep advanced phase despite a NaN frequency");

    expect(YouKnow106TestAccess::chorusToneStep(
               phase, std::numeric_limits<float>::infinity(), sampleRate)
               == 0.0f,
           "deterministicToneStep did not fall back to silence for an infinite frequency");
    expect(phase == 0.25,
           "deterministicToneStep advanced phase despite an infinite frequency");

    expect(YouKnow106TestAccess::chorusToneStep(
               phase, 997.0f, std::numeric_limits<float>::quiet_NaN())
               == 0.0f,
           "deterministicToneStep did not fall back to silence for a NaN sample rate");
    expect(YouKnow106TestAccess::chorusToneStep(phase, 997.0f, 0.0f) == 0.0f,
           "deterministicToneStep did not fall back to silence for a zero sample rate");
    expect(YouKnow106TestAccess::chorusToneStep(phase, 997.0f, -sampleRate)
               == 0.0f,
           "deterministicToneStep did not fall back to silence for a negative sample rate");
    expect(phase == 0.25,
           "deterministicToneStep advanced phase despite a non-positive sample rate");

    const float recovered =
        YouKnow106TestAccess::chorusToneStep(phase, 997.0f, sampleRate);
    expect(std::isfinite(recovered) && phase != 0.25,
           "deterministicToneStep did not recover ordinary operation after "
           "repeated fallback calls");
}

void testCorrelatedRandomStepCorrelationGuard()
{
    // correlatedRandomStep's own `rho` line falls back to 0.0f (fully
    // uncorrelated) whenever `correlation` is not finite, and otherwise
    // clamps it to [-1, 1]. The only production call site always passes
    // `optionalNoise_.commonRandomCorrelation`, which has no setter reachable
    // from panel, preset or SysEx code and so stays fixed at its 1.0f default
    // member initialiser for the lifetime of every Chorus instance; every
    // fixture above this one, including the correlation sweep in
    // testChorusNoiseComponents(), only ever drives finite in-range values
    // (1.0f, -1.0f, 0.35f) through the friend seam directly. Neither branch
    // has fired outside a test before now.
    std::uint32_t commonNaN = 0x243f6a89u;
    std::uint32_t orthogonalNaN = 0xb7e15163u;
    std::uint32_t commonZero = 0x243f6a89u;
    std::uint32_t orthogonalZero = 0xb7e15163u;
    bool nanMatchedZeroCorrelation = true;
    for (int index = 0; index < 64; ++index)
    {
        const auto viaNaN = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonNaN, orthogonalNaN,
            std::numeric_limits<float>::quiet_NaN());
        const auto viaZero = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonZero, orthogonalZero, 0.0f);
        nanMatchedZeroCorrelation = nanMatchedZeroCorrelation
            && viaNaN[0] == viaZero[0] && viaNaN[1] == viaZero[1];
    }
    expect(nanMatchedZeroCorrelation,
           "correlatedRandomStep did not fall back to zero correlation for "
           "a NaN correlation");

    std::uint32_t commonPosInf = 0xd1b54a35u;
    std::uint32_t orthogonalPosInf = 0x94d049bbu;
    std::uint32_t commonNegInf = 0xd1b54a35u;
    std::uint32_t orthogonalNegInf = 0x94d049bbu;
    std::uint32_t commonReference = 0xd1b54a35u;
    std::uint32_t orthogonalReference = 0x94d049bbu;
    bool infinitiesMatchedZeroCorrelation = true;
    for (int index = 0; index < 64; ++index)
    {
        const auto viaPosInf = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonPosInf, orthogonalPosInf,
            std::numeric_limits<float>::infinity());
        const auto viaNegInf = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonNegInf, orthogonalNegInf,
            -std::numeric_limits<float>::infinity());
        const auto viaZero = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonReference, orthogonalReference, 0.0f);
        infinitiesMatchedZeroCorrelation = infinitiesMatchedZeroCorrelation
            && viaPosInf[0] == viaZero[0] && viaPosInf[1] == viaZero[1]
            && viaNegInf[0] == viaZero[0] && viaNegInf[1] == viaZero[1];
    }
    expect(infinitiesMatchedZeroCorrelation,
           "correlatedRandomStep did not fall back to zero correlation for "
           "positive/negative infinite correlation");

    // Out-of-range but finite correlation is clamped rather than substituted:
    // 2.0 must behave exactly like the already-covered rho=+1 case (channels
    // duplicated), and -2.0 exactly like rho=-1 (channels inverted).
    std::uint32_t commonAboveOne = 0x243f6a89u;
    std::uint32_t orthogonalAboveOne = 0xb7e15163u;
    bool aboveOneDuplicatedExactly = true;
    std::uint32_t commonBelowNegativeOne = 0x243f6a89u;
    std::uint32_t orthogonalBelowNegativeOne = 0xb7e15163u;
    bool belowNegativeOneInvertedExactly = true;
    for (int index = 0; index < 64; ++index)
    {
        const auto above = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonAboveOne, orthogonalAboveOne, 2.0f);
        aboveOneDuplicatedExactly =
            aboveOneDuplicatedExactly && above[0] == above[1];

        const auto below = YouKnow106TestAccess::correlatedChorusNoiseStep(
            commonBelowNegativeOne, orthogonalBelowNegativeOne, -2.0f);
        belowNegativeOneInvertedExactly =
            belowNegativeOneInvertedExactly && below[1] == -below[0];
    }
    expect(aboveOneDuplicatedExactly,
           "correlatedRandomStep did not clamp an above-range correlation to +1");
    expect(belowNegativeOneInvertedExactly,
           "correlatedRandomStep did not clamp a below-range correlation to -1");
}

void testChorusBypassStateAndWetMuteTiming()
{
    constexpr float sampleRate = 48000.0f;
    constexpr int excitationLength = 6000;

    Chorus alwaysOn;
    Chorus bypassedForComparison;
    Chorus bypassedNaturally;
    alwaysOn.prepare(sampleRate);
    bypassedForComparison.prepare(sampleRate);
    bypassedNaturally.prepare(sampleRate);

    // Off uses the same running mode-I clock programme behind its wet shunts.
    // Feed all three instances identically while only one return is audible.
    for (int index = 0; index < excitationLength; ++index)
    {
        const float input = static_cast<float>(
            0.2 * std::sin(2.0 * pi * 311.0 * index / sampleRate));
        float left = 0.0f;
        float right = 0.0f;
        alwaysOn.process(input, ChorusMode::One, 0.0f, left, right);
        bypassedForComparison.process(
            input, ChorusMode::Off, 0.0f, left, right);
        bypassedNaturally.process(input, ChorusMode::Off, 0.0f, left, right);
    }

    // Remove only the mute-gain difference through the test seam. The wet
    // output capacitor legitimately evolved against 22 kOhm alone while
    // bypassed (R103/R81) rather than 22 kOhm || 39 kOhm while connected
    // (IC6's wet input R72/R74 loading the same node), so the resumed samples
    // are no longer bit-identical. 47 kOhm is the *dry* leg R71/R73 and is the
    // retired mirror of this assignment -- it must not reappear here. Their small error energy still proves the
    // BBDs, main support filters and modulation oscillator kept running.
    YouKnow106TestAccess::setChorusWetGain(
        bypassedForComparison,
        YouKnow106TestAccess::chorusWetGain(alwaysOn));
    double resumedEnergy = 0.0;
    double referenceEnergy = 0.0;
    double differenceEnergy = 0.0;
    for (int index = 0; index < 2048; ++index)
    {
        float onLeft = 0.0f;
        float onRight = 0.0f;
        float bypassedLeft = 0.0f;
        float bypassedRight = 0.0f;
        alwaysOn.process(0.0f, ChorusMode::One, 0.0f, onLeft, onRight);
        bypassedForComparison.process(
            0.0f, ChorusMode::One, 0.0f, bypassedLeft, bypassedRight);
        const double differenceLeft = bypassedLeft - onLeft;
        const double differenceRight = bypassedRight - onRight;
        differenceEnergy += differenceLeft * differenceLeft
                          + differenceRight * differenceRight;
        referenceEnergy += static_cast<double>(onLeft) * onLeft
                         + static_cast<double>(onRight) * onRight;
        resumedEnergy += static_cast<double>(bypassedLeft) * bypassedLeft
                       + static_cast<double>(bypassedRight) * bypassedRight;
    }
    expect(differenceEnergy < referenceEnergy * 0.01 + 1.0e-12,
           "chorus Off froze/reset core state instead of changing only the "
           "documented wet-output capacitor load");
    expect(resumedEnergy > 1.0e-8,
           "the evolved BBD state contained no resumable signal");

    // Exercise the actual unmute too: an evolved, previously bypassed line has
    // a tail as its gain rises, while a fresh line fed silence has none.
    Chorus fresh;
    fresh.prepare(sampleRate);
    double naturalEnergy = 0.0;
    double freshEnergy = 0.0;
    for (int index = 0; index < 2048; ++index)
    {
        float naturalLeft = 0.0f;
        float naturalRight = 0.0f;
        float freshLeft = 0.0f;
        float freshRight = 0.0f;
        bypassedNaturally.process(
            0.0f, ChorusMode::One, 0.0f, naturalLeft, naturalRight);
        fresh.process(0.0f, ChorusMode::One, 0.0f, freshLeft, freshRight);
        naturalEnergy += static_cast<double>(naturalLeft) * naturalLeft
                       + static_cast<double>(naturalRight) * naturalRight;
        freshEnergy += static_cast<double>(freshLeft) * freshLeft
                     + static_cast<double>(freshRight) * freshRight;
    }
    expect(naturalEnergy > freshEnergy + 1.0e-8,
           "re-enabling chorus restarted empty BBDs instead of evolved state");

    const auto tenToNinetySeconds = [](float rate) {
        Chorus chorus;
        chorus.prepare(rate);
        float left = 0.0f;
        float right = 0.0f;
        // Prime in Off so switching on is a player action and therefore glides.
        chorus.process(0.0f, ChorusMode::Off, 0.0f, left, right);
        const float target = Chorus::settingsFor(ChorusMode::One).wetGain;
        int tenPercent = -1;
        int ninetyPercent = -1;
        for (int index = 0; index < static_cast<int>(rate); ++index)
        {
            chorus.process(0.0f, ChorusMode::One, 0.0f, left, right);
            const float fraction = YouKnow106TestAccess::chorusWetGain(chorus)
                                 / target;
            if (tenPercent < 0 && fraction >= 0.1f)
                tenPercent = index;
            if (ninetyPercent < 0 && fraction >= 0.9f)
            {
                ninetyPercent = index;
                break;
            }
        }
        return static_cast<double>(ninetyPercent - tenPercent) / rate;
    };

    const double at48k = tenToNinetySeconds(48000.0f);
    const double at192k = tenToNinetySeconds(192000.0f);
    const double expected = Chorus::wetMuteTimeConstantSeconds * std::log(9.0);
    expectNear(at48k, expected, 2.0 / 48000.0,
               "wet-mute 10-90% time changed at 48 kHz");
    expectNear(at192k, expected, 2.0 / 192000.0,
               "wet-mute 10-90% time changed at 192 kHz");
    expectNear(at48k, at192k, 2.0 / 48000.0,
               "wet-mute timing is not sample-rate invariant");
}

void testChorusRateChangePreservesPhysicalState()
{
    Chorus chorus;
    chorus.prepare(192000.0);
    YouKnow106TestAccess::configureOptionalChorusNoise(
        chorus, 1.0e-4f, 0.3f, 1.0e-4f, 50.0f, 1.0e-4f, 1.0f);
    float left = 0.0f;
    float right = 0.0f;
    for (int sample = 0; sample < 8192; ++sample)
    {
        const float input = 0.2f * std::sin(
            static_cast<float>(2.0 * pi * 440.0 * sample / 192000.0));
        chorus.process(input, ChorusMode::One, 1.0f, left, right);
    }

    const auto before = YouKnow106TestAccess::chorusPhysicalState(chorus);
    double bucketEnergy = 0.0;
    for (const float value : before.cellsA)
        bucketEnergy += static_cast<double>(value) * value;
    expect(bucketEnergy > 1.0e-6,
           "chorus rate-change fixture never filled the physical BBD");

    chorus.prepare(48000.0, true);
    const auto after = YouKnow106TestAccess::chorusPhysicalState(chorus);
    expect(after == before,
           "a numerical sample-rate change power-cycled BBD/free-running state");
    expect(YouKnow106TestAccess::chorusAudioRateSupportIsClear(chorus),
           "a chorus rate change reused support coordinates or grid history");
}

void testBucketBrigadeDatasheetAnchors()
{
    // Measure the static line transfer independently of the delay and support
    // filters. The first amplitude is the MN3009 datasheet table's typical
    // distortion condition; the second is read from its typical THD-Vi curve.
    // The 1.5 Vrms row is a guaranteed minimum input swing at the 2.5% THD
    // criterion, so it remains an upper bound rather than a fitted point.
    const auto thdAt = [](double rmsVolts) {
        constexpr int samples = 32768;
        constexpr int cycles = 37;
        constexpr int lastHarmonic = 63;
        constexpr double voltsPerUnit = 2.6;
        const double peak = std::sqrt(2.0) * rmsVolts / voltsPerUnit;

        std::vector<float> output(samples);
        for (int index = 0; index < samples; ++index)
        {
            const double phase = 2.0 * pi * cycles * index / samples;
            output[static_cast<std::size_t>(index)] =
                YouKnow106TestAccess::bbdTransfer(
                    static_cast<float>(peak * std::sin(phase)));
        }

        double fundamental = 0.0;
        double distortionSquared = 0.0;
        for (int harmonic = 1; harmonic <= lastHarmonic; ++harmonic)
        {
            std::complex<double> accumulator {};
            for (int index = 0; index < samples; ++index)
            {
                const double phase = 2.0 * pi * cycles * harmonic * index / samples;
                accumulator += static_cast<double>(
                    output[static_cast<std::size_t>(index)])
                    * std::exp(std::complex<double>(0.0, -phase));
            }
            const double amplitude = 2.0 * std::abs(accumulator) / samples;
            if (harmonic == 1)
                fundamental = amplitude;
            else
                distortionSquared += amplitude * amplitude;
        }
        return std::sqrt(distortionSquared) / fundamental;
    };

    expectNear(thdAt(0.78), 0.003, 2.0e-4,
               "BBD distortion at the 0.78 Vrms datasheet condition");
    expectNear(thdAt(2.0), 0.020, 1.0e-3,
               "BBD distortion at the typical curve's 2.0 Vrms point");
    expect(thdAt(1.5) <= 0.025,
           "BBD distortion exceeded the 1.5 Vrms guaranteed input-swing bound");

    // The datasheet's -3 dB response at 12 kHz / 40 kHz describes the complete
    // held-output device. The rendered line already supplies the rectangular
    // zero-order hold, whose aperture contributes sinc(f/f_clock), so the
    // charge-transfer pole must supply only the residual loss. Measure that
    // pole as it is actually stepped and combine it with the independent ZOH
    // aperture; applying -3 dB to both mechanisms would fail at about -4.33 dB.
    // There is deliberately no clock argument on transferLossStep: one call is
    // one modeled BBD shift, or one fCP period, so its fixed coefficient already
    // scales the absolute pole with clock. A second clock multiplier used to make
    // this test false-green at its default 26 kHz while the comment and phase grid
    // claimed 40 kHz.
    constexpr double clockRate = 40000.0;
    constexpr int settle = 2048;
    constexpr int window = 40000; // integer cycles at both test frequencies
    const auto completeHeldGainAt = [=](double frequency) {
        float transferState = 0.0f;
        std::complex<double> accumulator {};
        for (int index = 0; index < settle + window; ++index)
        {
            const double phase = 2.0 * pi * frequency * index / clockRate;
            const float output = YouKnow106TestAccess::transferLossStep(
                transferState, static_cast<float>(std::sin(phase)));
            if (index >= settle)
                accumulator += static_cast<double>(output)
                             * std::exp(std::complex<double>(0.0, -phase));
        }
        const double transferGain = 2.0 * std::abs(accumulator) / window;
        const double ratio = frequency / clockRate;
        const double heldAperture = std::abs(std::sin(pi * ratio) / (pi * ratio));
        return transferGain * heldAperture;
    };

    const double gainAtOneKhz = completeHeldGainAt(1000.0);
    const double gainAtTwelveKhz = completeHeldGainAt(12000.0);
    expectNear(20.0 * std::log10(gainAtTwelveKhz), -3.0, 0.01,
               "charge-transfer loss double-counts the held-output aperture");

    // Panasonic references the bandwidth row to 1 kHz, not DC. The deliberate
    // DC-referenced fit is -2.9719 dB on that basis: within 0.03 dB of the
    // minimum-bandwidth row, too small to justify a second inaudible retune.
    expectNear(20.0 * std::log10(gainAtTwelveKhz / gainAtOneKhz), -3.0, 0.03,
               "charge-transfer response misses the 1 kHz-referenced anchor");

    // The 2026-08-07 two-phase output-stage solve left the typical part's
    // coefficient a span, because the datasheet's own panels contradict each
    // other about what the Gi-fi curves measure: the tracked reading puts
    // the raw held node at -1.33 dB here, the broadband reading near
    // -3.45 dB, and the guaranteed-minimum bound at -4.355 dB. Any future
    // retune must land inside that cross-reading band and confront the
    // session record before moving the anchor above.
    const double heldDecibels = 20.0 * std::log10(gainAtTwelveKhz);
    expect(heldDecibels > -4.355 && heldDecibels < -1.33,
           "raw held node left the cross-reading guard band at 40 kHz / 12 kHz");
}

void testBbdTransferDefensiveGuards()
{
    // `Chorus::Line::process` already sanitises its own input before it ever
    // reaches `bbdTransfer` (anything non-finite or beyond the 64-unit
    // corrupt-sample bound becomes 0 at the line's own gate), so the
    // combined-support fixtures that drive non-finite input through the full
    // line never actually exercise `bbdTransfer`'s own guard. Call it
    // directly so that guard has coverage of its own rather than relying on
    // an upstream sanitiser it does not know exists.
    expect(YouKnow106TestAccess::bbdTransfer(
               std::numeric_limits<float>::quiet_NaN()) == 0.0f,
           "bbdTransfer did not zero a NaN input");
    expect(YouKnow106TestAccess::bbdTransfer(
               std::numeric_limits<float>::infinity()) == 0.0f,
           "bbdTransfer did not zero a positive-infinite input");
    expect(YouKnow106TestAccess::bbdTransfer(
               -std::numeric_limits<float>::infinity()) == 0.0f,
           "bbdTransfer did not zero a negative-infinite input");

    // The double-precision power evaluation exists precisely so an extreme
    // but finite float approaches the saturation rail instead of overflowing
    // an intermediate `std::pow` and folding back to zero (see the comment
    // on `bbdTransfer`). Confirm both signs actually land there rather than
    // only checking finiteness, which a silent fold-to-zero would also pass.
    const float positiveRail = YouKnow106TestAccess::bbdTransfer(
        std::numeric_limits<float>::max());
    const float negativeRail = YouKnow106TestAccess::bbdTransfer(
        -std::numeric_limits<float>::max());
    expect(std::isfinite(positiveRail) && positiveRail > 1.0f
               && positiveRail < 1.2f,
           "bbdTransfer did not approach its saturation rail for an extreme "
           "positive finite input");
    expectNear(negativeRail, -positiveRail, 1.0e-6f,
               "bbdTransfer's saturation rail is not sign-symmetric");
}

void testBbdTransferApproximationTracksItsReference()
{
    // The realtime path tabulates the fitted generalized clip through four
    // rails. Pin its numerical contract independently against the original
    // double-power expression: at most one float ULP, odd and monotone. The
    // points immediately around every cell boundary make interpolation seams
    // part of the regression rather than relying on a uniform scan to land on
    // them by chance.
    constexpr float level = 1.1246614f;
    constexpr float curvature = 1.2044546f;
    constexpr float exponent = 12.9395323f;
    constexpr int intervals = 512;
    const auto reference = [](float input) {
        const double magnitude = std::abs(static_cast<double>(input));
        const double normalised = magnitude / level;
        const double base = 1.0
            + static_cast<double>(curvature) * normalised * normalised
            + std::pow(normalised, static_cast<double>(exponent));
        const double denominator = std::pow(
            base, 1.0 / static_cast<double>(exponent));
        return static_cast<float>(static_cast<double>(input) / denominator);
    };
    expect(!std::signbit(YouKnow106TestAccess::bbdTransfer(0.0f))
               && std::signbit(YouKnow106TestAccess::bbdTransfer(-0.0f)),
           "BBD Hermite transfer did not preserve signed zero");
    const auto expectReferenceUlp = [&](float input) {
        const float exact = reference(input);
        const float actual = YouKnow106TestAccess::bbdTransfer(input);
        expect(actual >= std::nextafter(
                   exact, -std::numeric_limits<float>::infinity())
                   && actual <= std::nextafter(
                       exact, std::numeric_limits<float>::infinity()),
               "BBD Hermite transfer differs from its reference by more than "
               "one float ULP at " + std::to_string(input));
    };

    float previous = YouKnow106TestAccess::bbdTransfer(0.0f);
    for (int index = 0; index <= 65536; ++index)
    {
        const float input = static_cast<float>(
            4.0 * level * static_cast<double>(index) / 65536.0);
        const float positive = YouKnow106TestAccess::bbdTransfer(input);
        const float negative = YouKnow106TestAccess::bbdTransfer(-input);
        expectReferenceUlp(input);
        expectReferenceUlp(-input);
        expect(positive >= previous,
               "BBD Hermite transfer is not monotone at "
                   + std::to_string(input));
        expect(negative == -positive,
               "BBD Hermite transfer is not odd at " + std::to_string(input));
        previous = positive;
    }
    for (int boundary = 0; boundary <= intervals; ++boundary)
    {
        const float input = static_cast<float>(
            4.0 * level * static_cast<double>(boundary) / intervals);
        expectReferenceUlp(std::nextafter(input, 0.0f));
        expectReferenceUlp(input);
        expectReferenceUlp(std::nextafter(
            input, std::numeric_limits<float>::infinity()));
    }
}

void testBbdInputCubicInterpolation()
{
    const auto interpolate = [](float current, float previous,
                                float previous2, float previous3,
                                double age) {
        return YouKnow106TestAccess::interpolateBbdInput(
            current, previous, previous2, previous3, age);
    };

    constexpr std::array<float, 4> endpointVector {
        0.375f, -0.625f, 0.875f, -0.125f
    };
    expect(interpolate(endpointVector[0], endpointVector[1],
                       endpointVector[2], endpointVector[3], 0.0)
               == endpointVector[0],
           "BBD cubic input interpolation moved the current endpoint");
    expect(interpolate(endpointVector[0], endpointVector[1],
                       endpointVector[2], endpointVector[3], 1.0)
               == endpointVector[1],
           "BBD cubic input interpolation moved the preceding endpoint");

    // A four-point Lagrange interpolant must reproduce every polynomial up to
    // degree three. Use binary-exact coefficients and an independent Horner
    // evaluation so this checks the whole interval, not merely its nodes.
    constexpr std::array<std::array<double, 4>, 4> polynomials {{
        { 0.25, 0.0, 0.0, 0.0 },
        { 0.125, 0.25, 0.0, 0.0 },
        { 0.125, 0.25, -0.0625, 0.0 },
        { 0.125, 0.25, -0.0625, 0.015625 }
    }};
    constexpr std::array<double, 7> ages {
        0.0, 0.125, 0.25, 0.5, 0.75, 0.875, 1.0
    };
    const auto evaluate = [](const auto& coefficients, double time) {
        return ((coefficients[3] * time + coefficients[2]) * time
                 + coefficients[1]) * time + coefficients[0];
    };
    double worstPolynomialError = 0.0;
    for (const auto& polynomial : polynomials)
    {
        const float current = static_cast<float>(evaluate(polynomial, 0.0));
        const float previous = static_cast<float>(evaluate(polynomial, -1.0));
        const float previous2 = static_cast<float>(evaluate(polynomial, -2.0));
        const float previous3 = static_cast<float>(evaluate(polynomial, -3.0));
        for (const double age : ages)
        {
            const double actual = interpolate(
                current, previous, previous2, previous3, age);
            const double expected = evaluate(polynomial, -age);
            worstPolynomialError = std::max(
                worstPolynomialError, std::abs(actual - expected));
        }
    }
    expect(worstPolynomialError < 5.0e-7,
           "BBD cubic input interpolation is not constant/linear/quadratic/"
           "cubic exact (max error "
               + std::to_string(worstPolynomialError) + ")");

    // Causal cubic Lagrange has small negative side lobes. Exhaust all sign
    // combinations over a dense fractional grid: their L1 envelope peaks at
    // 1.63114, so 1.632 is an independent finite overshoot fence rather than
    // an assumption that the interpolant is monotone.
    double worstAdversarialMagnitude = 0.0;
    bool adversarialFinite = true;
    for (int step = 0; step <= 4096; ++step)
    {
        const double age = static_cast<double>(step) / 4096.0;
        for (unsigned int pattern = 0; pattern < 16u; ++pattern)
        {
            std::array<float, 4> values {};
            for (unsigned int sample = 0; sample < values.size(); ++sample)
                values[sample] = (pattern & (1u << sample)) != 0u
                    ? 1.0f : -1.0f;
            const float actual = interpolate(
                values[0], values[1], values[2], values[3], age);
            adversarialFinite = adversarialFinite && std::isfinite(actual);
            worstAdversarialMagnitude = std::max(
                worstAdversarialMagnitude,
                std::abs(static_cast<double>(actual)));
        }
    }
    expect(adversarialFinite && worstAdversarialMagnitude < 1.632,
           "BBD cubic input interpolation left its finite L1 bound (peak "
               + std::to_string(worstAdversarialMagnitude) + ")");

    const float maximum = std::numeric_limits<float>::max();
    const float saturated = interpolate(
        maximum, maximum, -maximum, maximum, 0.451416);
    expect(std::isfinite(saturated) && saturated == maximum,
           "BBD cubic input interpolation overflowed an adversarial float");
    expect(interpolate(
               std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f,
               0.5) == 0.0f
               && interpolate(0.0f, 0.0f, 0.0f, 0.0f,
                              std::numeric_limits<double>::infinity()) == 0.0f,
           "BBD cubic input interpolation did not contain non-finite input");

    Chorus history;
    history.prepare(48000.0);
    YouKnow106TestAccess::setBbdInputHistory(history, 0.25f, -0.5f, 0.75f);
    history.reset();
    expect(YouKnow106TestAccess::bbdInputHistory(history)
               == std::array<float, 3> {},
           "BBD cubic input history survived reset");
    YouKnow106TestAccess::setBbdInputHistory(history, -0.25f, 0.5f, -0.75f);
    history.prepare(96000.0, true);
    expect(YouKnow106TestAccess::bbdInputHistory(history)
               == std::array<float, 3> {},
           "BBD cubic input history survived an audio-grid change");
}

void testBbdOutputPolyBlepReferenceAndBounds()
{
    // Exact residual landmarks from the authors' companion implementation.
    // The curve rings through zero between 0.625 and 0.75 samples; asserting
    // both signs catches replacing it with a monotone smoothing function.
    expectNear(YouKnow106TestAccess::bbdPolyBlepResidual(0.0), -0.5, 1.0e-15,
               "BBD polyBLEP residual at the discontinuity");
    expect(YouKnow106TestAccess::bbdPolyBlepResidual(0.5) < 0.0,
           "BBD polyBLEP lost its pre-zero residual sign");
    expect(YouKnow106TestAccess::bbdPolyBlepResidual(0.75) > 0.0,
           "BBD polyBLEP lost its post-zero residual sign");
    expectNear(YouKnow106TestAccess::bbdPolyBlepResidual(1.0), 1.0 / 24.0,
               1.0e-14, "BBD polyBLEP residual at one sample");
    expectNear(YouKnow106TestAccess::bbdPolyBlepResidual(2.0), 0.0, 1.0e-15,
               "BBD polyBLEP did not end at two samples");
    expectNear(YouKnow106TestAccess::bbdPolyBlepResidual(1.0 - 1.0e-8),
               YouKnow106TestAccess::bbdPolyBlepResidual(1.0 + 1.0e-8),
               1.0e-8, "BBD polyBLEP pieces are discontinuous at one sample");

    // A short, deliberately irregular vector crosses from 1.25 through 25
    // edges per numerical sample. The independent implementation above uses a
    // separately associated coordinate form for the causal cubic and the
    // companion source's past/future output-correction signs rather than
    // calling any production reconstruction helper. The prefilled ring makes
    // every edge a useful non-zero case.
    std::array<float, Chorus::cellPairs> initialCells {};
    for (int index = 0; index < Chorus::cellPairs; ++index)
    {
        initialCells[static_cast<std::size_t>(index)] = static_cast<float>(
            0.31 * std::sin(2.0 * pi * (index + 3) / 29.0)
            + 0.07 * std::cos(2.0 * pi * (index + 1) / 11.0));
    }

    Chorus production;
    production.prepare(8000.0);
    constexpr int initialIndex = 17;
    constexpr double initialPhase = 0.37;
    constexpr float initialTransfer = -0.12f;
    constexpr float initialInput = 0.05f;
    constexpr std::uint32_t initialNoise = 0x1234567u;
    YouKnow106TestAccess::configureBbdCore(
        production, initialCells, initialIndex, initialPhase,
        initialTransfer, initialInput, initialTransfer, initialNoise);

    ReferenceBbdCore reference;
    reference.cells = initialCells;
    reference.writeIndex = initialIndex;
    reference.clockPhase = initialPhase;
    reference.held = initialTransfer;
    reference.previousInput = initialInput;
    reference.previousInput2 = initialInput;
    reference.previousInput3 = initialInput;
    reference.transferState = initialTransfer;
    reference.noiseState = initialNoise;
    reference.pastEvents.reserve(Chorus::maximumBlepEvents);

    constexpr std::array<float, 8> clocks {
        20000.0f, 64000.0f, 200000.0f, 10000.0f,
        80000.0f, 24000.0f, 160000.0f, 40000.0f
    };
    double maximumError = 0.0;
    for (std::size_t sample = 0; sample < clocks.size(); ++sample)
    {
        const float input = static_cast<float>(
            0.18 * std::sin(0.71 * static_cast<double>(sample))
            - 0.04 * std::cos(0.23 * static_cast<double>(sample)));
        const float actual = YouKnow106TestAccess::processBbdCore(
            production, input, clocks[sample], 8000.0f, 0.0f);
        const float expected = reference.process(
            input, clocks[sample], 8000.0f);
        maximumError = std::max(maximumError,
                                std::abs(static_cast<double>(actual) - expected));
    }
    expect(maximumError < 2.0e-6,
           "BBD core differs from the independent multi-edge reference by "
               + std::to_string(maximumError));
    if (std::getenv("YOUKNOW106_AUDIT_BBD_BLEP") != nullptr)
        std::cout << "BBD BLEP independent-vector max error: "
                  << maximumError << '\n';

    const auto coreState = YouKnow106TestAccess::bbdCorePhysicalState(production);
    double maximumBucketError = 0.0;
    for (std::size_t index = 0; index < coreState.cells.size(); ++index)
        maximumBucketError = std::max(
            maximumBucketError,
            std::abs(static_cast<double>(coreState.cells[index])
                     - static_cast<double>(reference.cells[index])));
    expect(maximumBucketError < 2.0e-7,
           "BBD cubic input writes differ from the independent reference by "
               + std::to_string(maximumBucketError));
    expect(coreState.writeIndex == reference.writeIndex,
           "BBD reference correspondence moved a different number of buckets");
    expectNear(coreState.clockPhase, reference.clockPhase, 1.0e-12,
               "BBD reference correspondence lost fractional clock phase");
    expectNear(coreState.transferState, reference.transferState, 1.0e-7,
               "BBD reference correspondence changed transfer-loss state");
    expect(coreState.noiseState == reference.noiseState,
           "BBD output reconstruction changed the per-edge RNG sequence");
    expect(coreState.previousInput == reference.previousInput
               && coreState.previousInput2 == reference.previousInput2
               && coreState.previousInput3 == reference.previousInput3,
           "BBD cubic input history differs from the independent reference");

    // Reconstruction is a const read of already-realised deterministic state.
    // Evaluate it twice around a full physical snapshot to guard against a
    // tempting implementation that advances the real ring/transfer state while
    // looking ahead.
    const auto beforeCorrection =
        YouKnow106TestAccess::bbdCorePhysicalState(production);
    const int eventsBefore = YouKnow106TestAccess::bbdBlepEventCount(production);
    const double correctionA = YouKnow106TestAccess::bbdCoreCorrection(
        production, static_cast<double>(clocks.back()) / 8000.0);
    const double correctionB = YouKnow106TestAccess::bbdCoreCorrection(
        production, static_cast<double>(clocks.back()) / 8000.0);
    const auto afterCorrection =
        YouKnow106TestAccess::bbdCorePhysicalState(production);
    expect(std::isfinite(correctionA) && correctionA == correctionB,
           "BBD lookahead is not a pure deterministic evaluation");
    expect(afterCorrection == beforeCorrection
               && YouKnow106TestAccess::bbdBlepEventCount(production)
                    == eventsBefore,
           "BBD lookahead mutated buckets/index/phase/transfer/held/RNG");

    // At the full declared ratio there are exactly 25 edges per sample and 50
    // in the residual's open two-sample support. The 54-slot fixed array must
    // hold that state indefinitely without clipping or growing.
    Chorus maximumRate;
    maximumRate.prepare(8000.0);
    std::array<float, Chorus::cellPairs> zeroCells {};
    YouKnow106TestAccess::configureBbdCore(
        maximumRate, zeroCells, 0, 0.0, 0.0f, 0.0f, 0.0f, 0x9e3779b9u);
    for (int sample = 0; sample < 16; ++sample)
    {
        YouKnow106TestAccess::processBbdCore(
            maximumRate, 0.0f, Chorus::maximumClockHz,
            Chorus::minimumSampleRate, 0.0f);
        expect(YouKnow106TestAccess::bbdBlepEventCount(maximumRate)
                   <= Chorus::maximumBlepEvents,
               "BBD polyBLEP event history overflowed its fixed bound");
    }
    expect(YouKnow106TestAccess::bbdBlepEventCount(maximumRate) == 50,
           "BBD polyBLEP did not retain every edge in the worst two-sample window");

    // A noise-only line has no deterministic transfer jumps, hence a zero BLEP
    // delta. Its output, held sample and xorshift sequence remain bit-identical
    // to the pre-correction path even while 25 edges occur per sample.
    Chorus noiseOnly;
    noiseOnly.prepare(8000.0);
    YouKnow106TestAccess::configureBbdCore(
        noiseOnly, zeroCells, 0, 0.0, 0.0f, 0.0f, 0.0f, 0x9e3779b9u);
    std::uint32_t expectedNoiseState = 0x9e3779b9u;
    for (int sample = 0; sample < 4; ++sample)
    {
        const float output = YouKnow106TestAccess::processBbdCore(
            noiseOnly, 0.0f, 200000.0f, 8000.0f, 1.0f);
        for (int edge = 0; edge < 25; ++edge)
            expectedNoiseState = referenceXorshift32(expectedNoiseState);
        const auto state = YouKnow106TestAccess::bbdCorePhysicalState(noiseOnly);
        const float expectedNoise =
            (static_cast<float>(expectedNoiseState & 0xffffffu)
                 * (2.0f / 16777215.0f) - 1.0f)
            * Chorus::independentLineRandomAmplitude;
        expect(output == state.held && state.held == expectedNoise,
               "deterministic BBD BLEP coloured or rerounded held line noise");
        expect(state.transferState == 0.0f,
               "noise-only BBD developed a deterministic transfer signal");
    }
    expect(expectedNoiseState == 0x5f34ccddu,
           "noise-only BBD golden edge count/RNG state changed");

    // Rate-change policy: physical state is covered separately, while this
    // sample-grid correction history must be discarded at the zero-gain
    // boundary.
    maximumRate.prepare(16000.0, true);
    expect(YouKnow106TestAccess::bbdBlepEventCount(maximumRate) == 0,
           "BBD polyBLEP history survived a numerical grid change");
}

// bbdPolyBlepResidual's own comment documents that distances are always
// non-negative in this model, and both production call sites only ever pass
// a tracked non-negative event age or a distance built up from a
// [0, 1)-range clock phase, so the negative/NaN early-return this pure
// helper carries for defensiveness never actually fires outside a test.
void testBbdPolyBlepResidualDefensiveGuard()
{
    expectNear(YouKnow106TestAccess::bbdPolyBlepResidual(-0.5), 0.0, 1.0e-15,
               "BBD polyBLEP residual did not fall back to zero for a negative distance");
    expectNear(YouKnow106TestAccess::bbdPolyBlepResidual(
                   -std::numeric_limits<double>::infinity()),
               0.0, 1.0e-15,
               "BBD polyBLEP residual did not fall back to zero for negative infinity");
    expectNear(YouKnow106TestAccess::bbdPolyBlepResidual(
                   std::numeric_limits<double>::quiet_NaN()),
               0.0, 1.0e-15,
               "BBD polyBLEP residual did not fall back to zero for NaN");

    // Confirms the fallback is the negativity/NaN check, not an accidental
    // wide-bound clamp: a call just past the documented support still lands
    // back on the same zero the in-support boundary already does.
    expectNear(YouKnow106TestAccess::bbdPolyBlepResidual(2.0 + 1.0e-9), 0.0,
               1.0e-15,
               "BBD polyBLEP residual did not stay zero just past two samples");
}

void testBbdOutputPolyBlepSeparatesPhysicalAndNumericalAliases()
{
    constexpr float sampleRate = 44100.0f;
    constexpr double inputFrequency = 784.0;
    constexpr int seconds = 2;
    constexpr std::size_t analysisLength = 44100;

    const auto render = [](float clockHz) {
        Chorus line;
        line.prepare(sampleRate);
        std::array<float, Chorus::cellPairs> cells {};
        YouKnow106TestAccess::configureBbdCore(
            line, cells, 0, 0.0, 0.0f, 0.0f, 0.0f, 0x9e3779b9u);

        std::pair<std::vector<float>, std::vector<float>> result;
        result.first.resize(static_cast<std::size_t>(seconds * sampleRate));
        result.second.resize(result.first.size());
        for (std::size_t sample = 0; sample < result.first.size(); ++sample)
        {
            const float input = static_cast<float>(
                0.02 * std::sin(2.0 * pi * inputFrequency
                                * static_cast<double>(sample) / sampleRate));
            result.second[sample] = YouKnow106TestAccess::processBbdCore(
                line, input, clockHz, sampleRate, 0.0f);
            result.first[sample] = YouKnow106TestAccess::bbdRawHeld(line);
        }
        return result; // first raw ZOH, second host-grid BLEP
    };

    // Above the numerical sample rate, the first physical clock images lie
    // outside its Nyquist band. In-band energy other than the coherent input
    // tone is therefore simulation-grid aliasing (the very small fitted BBD
    // nonlinearity is held 34 dB below its datasheet drive here).
    const auto highClock = render(50000.0f);
    const std::size_t start = highClock.first.size() - analysisLength;
    const double rawResidual = singleToneResidualRms(
        highClock.first, start, analysisLength, inputFrequency, sampleRate);
    const double correctedResidual = singleToneResidualRms(
        highClock.second, start, analysisLength, inputFrequency, sampleRate);
    const double highClockImprovementDb = 20.0 * std::log10(
        (rawResidual + 1.0e-20) / (correctedResidual + 1.0e-20));
    expect(highClockImprovementDb > 20.0,
           "BBD host-grid alias floor improved by only "
               + std::to_string(highClockImprovementDb) + " dB at 50 kHz");

    // The companion paper's other above-host case is also an integration test
    // for more than two BBD edges per numerical sample. This implementation
    // sums every discontinuity in the residual support instead of silently
    // truncating the fifth one to a fixed three-neighbour example.
    const auto multipleEdgeClock = render(90000.0f);
    const std::size_t multipleStart =
        multipleEdgeClock.first.size() - analysisLength;
    const double rawMultipleResidual = singleToneResidualRms(
        multipleEdgeClock.first, multipleStart, analysisLength,
        inputFrequency, sampleRate);
    const double correctedMultipleResidual = singleToneResidualRms(
        multipleEdgeClock.second, multipleStart, analysisLength,
        inputFrequency, sampleRate);
    const double multipleEdgeImprovementDb = 20.0 * std::log10(
        (rawMultipleResidual + 1.0e-20)
        / (correctedMultipleResidual + 1.0e-20));
    expect(multipleEdgeImprovementDb > 20.0,
           "BBD multi-edge host-grid alias floor improved by only "
               + std::to_string(multipleEdgeImprovementDb) + " dB at 90 kHz");

    // At 10 kHz the images k*fCP +/- f0 below host Nyquist are genuine BBD
    // output, not errors to optimize away. Compare their amplitudes with the
    // closed-form rectangular-hold aperture, referenced to the fundamental.
    // This guards against a generic low-pass "fix" that would sound cleaner
    // precisely by deleting the hardware's characteristic aliases.
    const auto lowClock = render(10000.0f);
    const std::size_t lowStart = lowClock.first.size() - analysisLength;
    const double fundamental = sinusoidMagnitude(
        lowClock.second, lowStart, analysisLength, inputFrequency, sampleRate);
    const double rawFundamental = sinusoidMagnitude(
        lowClock.first, lowStart, analysisLength, inputFrequency, sampleRate);
    expect(fundamental > 1.0e-4,
           "BBD physical-image fixture produced no fundamental");

    const auto sinc = [](double value) {
        return std::abs(value) < 1.0e-15
            ? 1.0 : std::sin(pi * value) / (pi * value);
    };
    const double referenceAperture = std::abs(sinc(inputFrequency / 10000.0));
    double worstAudibleImageErrorDb = 0.0;
    double worstPhysicalImageErrorDb = 0.0;
    for (const double image : { 9216.0, 10784.0, 19216.0, 20784.0 })
    {
        const double measured = sinusoidMagnitude(
            lowClock.second, lowStart, analysisLength, image, sampleRate)
                              / fundamental;
        const double expected = std::abs(sinc(image / 10000.0))
                              / referenceAperture;
        const double errorDb = 20.0 * std::log10(
            (measured + 1.0e-20) / (expected + 1.0e-20));
        if (std::getenv("YOUKNOW106_AUDIT_BBD_BLEP") != nullptr)
        {
            const double rawRatio = sinusoidMagnitude(
                lowClock.first, lowStart, analysisLength, image, sampleRate)
                                  / rawFundamental;
            std::cout << "BBD image " << image << " Hz: raw "
                      << 20.0 * std::log10(rawRatio + 1.0e-20)
                      << " dBr, BLEP "
                      << 20.0 * std::log10(measured + 1.0e-20)
                      << " dBr, ZOH reference "
                      << 20.0 * std::log10(expected + 1.0e-20)
                      << " dBr\n";
        }
        worstPhysicalImageErrorDb = std::max(
            worstPhysicalImageErrorDb, std::abs(errorDb));
        if (image < 15000.0)
            worstAudibleImageErrorDb = std::max(
                worstAudibleImageErrorDb, std::abs(errorDb));
    }
    // The first image pair, in the sensitive 9--11 kHz band, stays within
    // 0.61 dB. The third-order polynomial deliberately becomes its numerical
    // antialias transition close to the 22.05 kHz Nyquist limit; the second
    // pair is attenuated by at most 5.96 dB rather than folded elsewhere. Both
    // limits are explicit so a future generic smoother cannot quietly erase
    // the physical images and call the lower alias floor an improvement.
    expect(worstAudibleImageErrorDb < 0.75,
           "BBD polyBLEP moved an audible physical k*fCP+/-f image by "
               + std::to_string(worstAudibleImageErrorDb) + " dB");
    expect(worstPhysicalImageErrorDb < 6.2,
           "BBD polyBLEP erased a near-Nyquist physical k*fCP+/-f image by "
               + std::to_string(worstPhysicalImageErrorDb) + " dB");

    const std::vector<double> physicalTones {
        inputFrequency, 9216.0, 10784.0, 19216.0, 20784.0
    };
    const double rawLowResidual = selectedToneResidualRms(
        lowClock.first, lowStart, analysisLength, physicalTones, sampleRate);
    const double correctedLowResidual = selectedToneResidualRms(
        lowClock.second, lowStart, analysisLength, physicalTones, sampleRate);
    const double lowClockImprovementDb = 20.0 * std::log10(
        (rawLowResidual + 1.0e-20) / (correctedLowResidual + 1.0e-20));
    expect(lowClockImprovementDb > 15.0,
           "BBD image-excluded host-grid floor improved by only "
               + std::to_string(lowClockImprovementDb) + " dB at 10 kHz");

    // Repeat the physical-image observation through the complete modeled Line:
    // five input poles plus coupling, nonlinear/transfer BBD, tap pole, four
    // output poles and wet-output coupling. The 44.1 kHz path is the explicit
    // low-quality case; 176.4 kHz is the engine's default 4x processing grid on
    // a 44.1 kHz host. The latter's images are far inside numerical Nyquist,
    // and the shared linear decimator cannot change their before/after ratio.
    const auto renderFullLine = [](float processingRate) {
        Chorus rawLine;
        Chorus correctedLine;
        rawLine.prepare(processingRate);
        correctedLine.prepare(processingRate);

        const std::size_t frames = static_cast<std::size_t>(processingRate);
        std::pair<std::vector<float>, std::vector<float>> rendered;
        rendered.first.resize(frames);
        rendered.second.resize(frames);
        for (std::size_t sample = 0; sample < frames; ++sample)
        {
            const float input = static_cast<float>(
                0.02 * std::sin(2.0 * pi * inputFrequency
                                * static_cast<double>(sample)
                                / processingRate));
            rendered.first[sample] = YouKnow106TestAccess::processBbdFullLine(
                rawLine, input, 10000.0f, false);
            rendered.second[sample] = YouKnow106TestAccess::processBbdFullLine(
                correctedLine, input, 10000.0f, true);
        }
        expect(YouKnow106TestAccess::bbdCorePhysicalState(rawLine)
                   == YouKnow106TestAccess::bbdCorePhysicalState(correctedLine),
               "full-Line BLEP changed physical BBD state relative to raw output");
        return rendered;
    };

    const auto imageDeltasFor = [&](const auto& rendered,
                                    double processingRate) {
        std::array<double, 4> deltas {};
        const std::size_t length = static_cast<std::size_t>(processingRate / 2.0);
        const std::size_t offset = rendered.first.size() - length;
        const std::array<double, 4> images { 9216.0, 10784.0,
                                             19216.0, 20784.0 };
        for (std::size_t image = 0; image < images.size(); ++image)
        {
            const double rawMagnitude = sinusoidMagnitude(
                rendered.first, offset, length, images[image], processingRate);
            const double correctedMagnitude = sinusoidMagnitude(
                rendered.second, offset, length, images[image], processingRate);
            deltas[image] = 20.0 * std::log10(
                (correctedMagnitude + 1.0e-20) / (rawMagnitude + 1.0e-20));
        }
        return deltas;
    };

    const auto fullLowQuality = renderFullLine(44100.0f);
    const auto fullLowQualityDeltas = imageDeltasFor(
        fullLowQuality, 44100.0);
    expect(std::abs(fullLowQualityDeltas[0]) < 0.75
               && std::abs(fullLowQualityDeltas[1]) < 0.75,
           "full low-quality Line moved the first physical BBD image pair");
    expect(fullLowQualityDeltas[2] > -6.2
               && fullLowQualityDeltas[3] > -6.2,
           "full low-quality Line erased the near-Nyquist BBD image pair");

    const auto fullHq = renderFullLine(176400.0f);
    const auto fullHqDeltas = imageDeltasFor(fullHq, 176400.0);
    double worstHqImageDelta = 0.0;
    for (const double delta : fullHqDeltas)
        worstHqImageDelta = std::max(worstHqImageDelta, std::abs(delta));
    expect(worstHqImageDelta < 0.1,
           "default-HQ Line moved a physical BBD image by "
               + std::to_string(worstHqImageDelta) + " dB");

    if (std::getenv("YOUKNOW106_AUDIT_BBD_BLEP") != nullptr)
    {
        std::cout << "BBD BLEP 50 kHz SGA improvement: "
                  << highClockImprovementDb << " dB\n"
                  << "BBD BLEP 90 kHz SGA improvement: "
                  << multipleEdgeImprovementDb << " dB\n"
                  << "BBD BLEP 10 kHz image-excluded improvement: "
                  << lowClockImprovementDb << " dB\n"
                  << "BBD BLEP worst physical-image amplitude error: "
                  << worstPhysicalImageErrorDb << " dB\n"
                  << "BBD full Line LQ image deltas: ";
        for (const double delta : fullLowQualityDeltas)
            std::cout << delta << " dB ";
        std::cout << "\nBBD full Line HQ image deltas: ";
        for (const double delta : fullHqDeltas)
            std::cout << delta << " dB ";
        std::cout << '\n';
    }
}

// A filter coefficient is only right in company with the update it is used
// with. Asserting the formula would prove nothing, so this measures where the
// corner of the pairing the line actually runs ends up, at the two rates the
// engine uses. The mismatched pairing this catches put the 9.9 kHz corner at
// 4.6 kHz -- inaudible as a bug report, obvious as a dull chorus.
// The high-pass as the signal actually meets it, not as a table of laws.
//
// This exists because the stage was moved from inside each voice to the summed
// bus and every other check in the suite passed either way: the laws are pure
// functions and stayed true, so nothing was guarding that the filter was still
// reached at all. A stage wired to nothing would have gone unnoticed.
void testHighPassReachesTheSummedSignal()
{
    const auto rmsFor = [](HighPassMode mode, int note) {
        YouKnow106Engine engine;
        engine.prepare(48000.0, 512, false);
        EngineParameters parameters;
        parameters.highPass = mode;
        parameters.cutoff = 1.0f;      // filter wide open, so the high-pass is
        parameters.resonance = 0.0f;   // the only thing shaping the band
        parameters.attack = 0.0f;
        parameters.sustain = 1.0f;
        parameters.calibration = 0.0f; // no dispersion, so this is repeatable
        engine.setParameters(parameters);
        engine.reset();
        engine.noteOn(note, 0.9f);

        std::vector<float> left(24000), right(24000);
        engine.process(left.data(), right.data(), static_cast<int>(left.size()));
        double sum = 0.0;
        const std::size_t half = left.size() / 2;
        for (std::size_t index = half; index < left.size(); ++index)
            sum += static_cast<double>(left[index]) * left[index];
        return std::sqrt(sum / static_cast<double>(left.size() - half));
    };

    // A low note, where every leg of the network has something to act on.
    const double flatLow = rmsFor(HighPassMode::One, 28);
    expect(flatLow > 1.0e-3, "the flat leg rendered nothing to measure");

    const double boostLow = rmsFor(HighPassMode::Boost, 28);
    const double cutTwoLow = rmsFor(HighPassMode::Two, 28);
    const double cutThreeLow = rmsFor(HighPassMode::Three, 28);

    expect(boostLow > flatLow * 1.2,
           "the boost leg did not lift a low note above the flat leg");
    expect(cutTwoLow < flatLow * 0.8,
           "the middle cut leg did not attenuate a low note");
    expect(cutThreeLow < cutTwoLow,
           "the top cut leg is not darker than the middle one on a low note");

    // And the ordering has to come from the corner rather than from a gain
    // trim, so it must fade as the note rises above the corners.
    const double flatHigh = rmsFor(HighPassMode::One, 76);
    const double cutThreeHigh = rmsFor(HighPassMode::Three, 76);
    expect(flatHigh > 1.0e-3, "the flat leg rendered nothing at the top");
    expect(cutThreeHigh > flatHigh * 0.6,
           "the top cut leg attenuates a high note as if it were a level control");
    expect(cutThreeLow / flatLow < cutThreeHigh / flatHigh,
           "the high-pass cuts a high note as hard as a low one");
}

void testHighPassStateGuardSelfHeals()
{
    // Nothing that feeds any of the engine's HighPass instances today can
    // hand one a non-finite state, since each starts at 0.0 and is only ever
    // updated by the filter's own finite arithmetic, so this branch has
    // never fired outside a test. Poison the state directly and confirm it
    // lands on the documented fallback of 0.0.
    const float g = 0.5f;
    expect(YouKnow106TestAccess::highPassStateAfterProcess(
               std::numeric_limits<double>::quiet_NaN(), 1.0f, g, 0.0f, 1.0f)
               == 0.0,
           "HighPass::process did not reset a NaN state to 0.0");
    expect(YouKnow106TestAccess::highPassStateAfterProcess(
               std::numeric_limits<double>::infinity(), 1.0f, g, 0.0f, 1.0f)
               == 0.0,
           "HighPass::process did not reset a positive-infinite state to 0.0");
    expect(YouKnow106TestAccess::highPassStateAfterProcess(
               -std::numeric_limits<double>::infinity(), 1.0f, g, 0.0f, 1.0f)
               == 0.0,
           "HighPass::process did not reset a negative-infinite state to 0.0");

    // The healed state has to let the filter recover, not just stop the
    // member itself from being NaN forever: the *next* call on the same
    // object, after the poisoned one, must return a normal, finite output
    // rather than continuing to propagate the poison through `low`/`high`
    // indefinitely. (The poisoning call's own output is deliberately left
    // unspecified here -- a future change that also sanitizes it would be
    // strictly safer, not a regression.)
    const float recovered =
        YouKnow106TestAccess::highPassOutputAfterPoisonedCallHeals(
            std::numeric_limits<double>::quiet_NaN(), 1.0f, g, 0.0f, 1.0f);
    expect(std::isfinite(recovered),
           "HighPass::process did not recover a finite output on the call "
           "after its state healed");
}

void testNoiseSourceShapingFollowsItsCircuit()
{
    // Module board p. 13: the shared noise source is band-shaped by its own
    // support parts -- C42 1 uF into the level OTA's 4.7 kOhm input bias,
    // then C41 100 pF against R79 330 kOhm on the OTA's output -- so the
    // rail is not flat white. The corners must come from those designators.
    expectNear(YouKnow106Engine::noiseSourceHighPassHz(),
               1.0 / (2.0 * pi * 1.0e-6 * 4700.0), 1.0e-3,
               "noise-source high-pass is not at C42/4.7k's corner");
    expectNear(YouKnow106Engine::noiseSourceLowPassHz(),
               1.0 / (2.0 * pi * 100.0e-12 * 330000.0), 1.0e-3,
               "noise-source low-pass is not at C41/R79's corner");

    // And the shaping has to reach the rendered rail. The normalised
    // first-difference energy of a noise render separates a flat generator
    // from one low-passed near 4.8 kHz decisively: flat white at a 48 kHz
    // internal rate sits near 2.0, the shaped source well below 1.2.
    YouKnow106Engine engine;
    engine.prepare(48000.0, 512, false);
    EngineParameters parameters;
    parameters.sawEnabled = false;
    parameters.pulseEnabled = false;
    parameters.subLevel = 0.0f;
    parameters.noiseLevel = 1.0f;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.attack = 0.0f;
    parameters.sustain = 1.0f;
    parameters.calibration = 0.0f;
    engine.setParameters(parameters);
    engine.reset();
    engine.noteOn(60, 1.0f);

    std::vector<float> left(24000), right(24000);
    engine.process(left.data(), right.data(), static_cast<int>(left.size()));
    double energy = 0.0;
    double differenceEnergy = 0.0;
    const std::size_t half = left.size() / 2;
    for (std::size_t index = half + 1; index < left.size(); ++index)
    {
        const double sample = left[index];
        const double difference = sample - static_cast<double>(left[index - 1]);
        energy += sample * sample;
        differenceEnergy += difference * difference;
    }
    expect(energy > 1.0e-6, "the noise fixture rendered nothing to measure");
    const double rms = std::sqrt(
        energy / static_cast<double>(left.size() - half - 1));
    // The declared coordinate is +/-2 V at the SHAPED rail, which the source
    // reaches by being generated at 7.4161 V and losing 11.383 dB to its own
    // C41/R79 pole; the result is then read at an output whose full scale is
    // the summer model's provisional asymptote. Those three are what this bound
    // watches, and it is deliberately wide enough to fail only on a real
    // mistake rather than on the last digit of any of them. Writing the
    // coordinate onto the source ahead of the shaping instead -- which a
    // previous revision did -- reads
    // 11.4 dB low here, and a dead source falls through the floor.
    expect(std::isfinite(rms) && rms > 0.0035 && rms < 0.0070,
           "full-level main-noise RMS left its shaped +/-2 V range: "
               + std::to_string(rms));
    expect(differenceEnergy / energy < 1.2,
           "the rendered noise is too bright for the C41/R79-shaped source");
    expect(differenceEnergy / energy > 0.05,
           "the rendered noise lost its passband as well as its top");
}

void testNoiseLevelControlPrecedesC41()
{
    expect(EngineParameters {}.enableNoiseLevelBeforeC41,
           "the schematic-ordered NOISE OTA/C41 path is not the default");

    constexpr double sampleRate = 48000.0;
    YouKnow106Engine circuit;
    YouKnow106Engine legacy;
    circuit.prepare(sampleRate, 64, false);
    legacy.prepare(sampleRate, 64, false);
    YouKnow106TestAccess::setNoiseSourceSupportState(circuit, 0.0, 0.0);
    YouKnow106TestAccess::setNoiseSourceSupportState(legacy, 0.0, 0.0);

    const auto sourceAt = [](int sample) {
        // A deterministic broad two-tone keeps C42 and C41 continuously
        // excited without making this state-ordering contract depend on RNG.
        return static_cast<float>(
            0.63 * std::sin(2.0 * pi * 997.0
                            * static_cast<double>(sample) / sampleRate)
            + 0.31 * std::sin(2.0 * pi * 3137.0
                              * static_cast<double>(sample) / sampleRate));
    };

    // At a fixed gain, linearity makes gain-before-C41 and gain-after-C41 the
    // same transfer. Pin that non-regression before exercising a level move.
    double fixedLevelMaximumDifference = 0.0;
    for (int sample = 0; sample < 2048; ++sample)
    {
        const float raw = sourceAt(sample);
        const float ordered = YouKnow106TestAccess::processMainNoiseSource(
            circuit, raw, 0.5f, true);
        const float former = YouKnow106TestAccess::processMainNoiseSource(
            legacy, raw, 0.5f, false);
        fixedLevelMaximumDifference = std::max(
            fixedLevelMaximumDifference,
            std::abs(static_cast<double>(ordered)
                     - static_cast<double>(former)));
    }
    expect(fixedLevelMaximumDifference < 2.0e-6,
           "moving the NOISE level around C41 changed a fixed-level transfer");
    const auto fixedCircuitState =
        YouKnow106TestAccess::noiseSourceSupportState(circuit);
    const auto fixedLegacyState =
        YouKnow106TestAccess::noiseSourceSupportState(legacy);
    expect(fixedCircuitState[0] == fixedLegacyState[0],
           "the level control leaked upstream through C42");

    // At the supported 8 kHz HQ-off endpoint, C41's physical 33 us memory is
    // shorter than one 125 us sample and the qualified TPT pole is negative.
    // The circuit default must not expose that numerical pole as a fake
    // alternating mute tail; it deliberately collapses to the legacy scalar
    // while retaining the established fixed-level filter.
    YouKnow106Engine endpointCircuit;
    YouKnow106Engine endpointLegacy;
    endpointCircuit.prepare(8000.0, 64, false);
    endpointLegacy.prepare(8000.0, 64, false);
    for (int sample = 0; sample < 256; ++sample)
    {
        const float raw = sourceAt(sample);
        const float level = sample < 128 ? 1.0f : 0.0f;
        expect(YouKnow106TestAccess::processMainNoiseSource(
                   endpointCircuit, raw, level, true)
                   == YouKnow106TestAccess::processMainNoiseSource(
                       endpointLegacy, raw, level, false),
               "the sub-sample C41 fallback exposed an alternating tail");
    }
    expect(YouKnow106TestAccess::noiseSourceSupportState(endpointCircuit)
               == YouKnow106TestAccess::noiseSourceSupportState(endpointLegacy),
           "the sub-sample C41 fallback changed the qualified endpoint filter");

    // Exercise the public control path as well as the scalar circuit above:
    // setParameters reaches the one shared NOISE hold through the converter
    // scan, its 522 us slew drives process(), and the default flag chooses the
    // ordered path without a test-only call to processMainNoiseSource().
    YouKnow106Engine integratedCircuit;
    YouKnow106Engine integratedLegacy;
    integratedCircuit.prepare(sampleRate, 64, false);
    integratedLegacy.prepare(sampleRate, 64, false);
    EngineParameters noisePatch;
    noisePatch.noiseLevel = 1.0f;
    integratedCircuit.setParameters(noisePatch);
    noisePatch.enableNoiseLevelBeforeC41 = false;
    integratedLegacy.setParameters(noisePatch);
    integratedCircuit.reset();
    integratedLegacy.reset();

    std::array<float, 64> left {};
    std::array<float, 64> right {};
    const auto processBlock = [&](YouKnow106Engine& engine, int frames) {
        engine.process(left.data(), right.data(), frames);
    };
    for (int block = 0; block < 32; ++block)
    {
        processBlock(integratedCircuit, 64);
        processBlock(integratedLegacy, 64);
    }
    expect(YouKnow106TestAccess::noiseSourceSupportState(integratedCircuit)
               == YouKnow106TestAccess::noiseSourceSupportState(integratedLegacy),
           "the integrated unity-level NOISE paths did not precharge identically");
    expect(YouKnow106TestAccess::noiseHeld(integratedCircuit) > 0.999f,
           "the integrated NOISE hold did not reach full level");

    noisePatch.enableNoiseLevelBeforeC41 = true;
    noisePatch.noiseLevel = 0.0f;
    integratedCircuit.setParameters(noisePatch);
    noisePatch.enableNoiseLevelBeforeC41 = false;
    integratedLegacy.setParameters(noisePatch);
    for (int block = 0; block < 56; ++block)
    {
        processBlock(integratedCircuit, 64);
        processBlock(integratedLegacy, 64);
    }

    double circuitStateEnergy = 0.0;
    double legacyStateEnergy = 0.0;
    double maximumStateDifference = 0.0;
    for (int sample = 0; sample < 512; ++sample)
    {
        processBlock(integratedCircuit, 1);
        processBlock(integratedLegacy, 1);
        const double circuitState =
            YouKnow106TestAccess::noiseSourceSupportState(integratedCircuit)[1];
        const double legacyState =
            YouKnow106TestAccess::noiseSourceSupportState(integratedLegacy)[1];
        circuitStateEnergy += circuitState * circuitState;
        legacyStateEnergy += legacyState * legacyState;
        maximumStateDifference = std::max(
            maximumStateDifference, std::abs(circuitState - legacyState));
    }
    const auto integratedCircuitState =
        YouKnow106TestAccess::noiseSourceSupportState(integratedCircuit);
    const auto integratedLegacyState =
        YouKnow106TestAccess::noiseSourceSupportState(integratedLegacy);
    expect(integratedCircuitState[0] == integratedLegacyState[0],
           "the integrated mute altered the upstream C42 state");
    expect(YouKnow106TestAccess::noiseHeld(integratedCircuit) < 1.0e-6f,
           "the scanned NOISE hold did not settle at zero");
    expect(std::sqrt(circuitStateEnergy / 512.0) < 1.0e-6,
           "the integrated circuit path kept C41 excited behind Noise zero");
    expect(std::sqrt(legacyStateEnergy / 512.0) > 1.0e-3,
           "the integrated legacy path unexpectedly discharged C41");
    expect(maximumStateDifference > 1.0e-3,
           "the integrated NOISE control did not reach C41's stored charge");
}

void testNoiseSourceLowRateSafetyAndStateSemantics()
{
    constexpr double endpointRate = 8000.0;
    const double physicalCorner = YouKnow106Engine::noiseSourceLowPassHz();
    const auto expectedCoefficient = [=](double internalRate) {
        const float designCorner = std::min(
            static_cast<float>(physicalCorner),
            static_cast<float>(internalRate) * 0.45f);
        const float inverseRate = static_cast<float>(1.0 / internalRate);
        return std::tan(static_cast<float>(pi) * designCorner * inverseRate);
    };
    const auto statePole = [](double g) {
        return (1.0 - g) / (1.0 + g);
    };

    // The helper remains the physical C41/R79 corner. Only the TPT design
    // corner is limited, and it must be limited against the internal rate --
    // 8 kHz HQ runs this source at 32 kHz and needs no endpoint substitution.
    struct RateCase
    {
        double hostRate;
        bool hq;
        double internalRate;
        const char* name;
    };
    constexpr std::array<RateCase, 7> rateCases {{
        { 8000.0,  false, 8000.0,  "8k q1" },
        { 9000.0,  false, 9000.0,  "9k q1" },
        { 10000.0, false, 10000.0, "10k q1" },
        { 11025.0, false, 11025.0, "11.025k q1" },
        { 8000.0,  true,  32000.0, "8k q4" },
        { 44100.0, false, 44100.0, "44.1k q1" },
        { 48000.0, false, 48000.0, "48k q1" }
    }};
    for (const auto& item : rateCases)
    {
        YouKnow106Engine engine;
        engine.prepare(item.hostRate, 64, item.hq);
        const double actualRate =
            YouKnow106TestAccess::noiseSourceProcessingRate(engine);
        const double g =
            YouKnow106TestAccess::noiseSourceLowPassG(engine);
        const std::string where = std::string(" at ") + item.name;
        expectNear(actualRate, item.internalRate, 0.0,
                   "noise-source support used the wrong internal rate" + where);
        expectNear(g, expectedCoefficient(item.internalRate), 1.0e-5,
                   "noise-source low-pass used the wrong design corner" + where);
        expect(std::isfinite(g) && g > 0.0
                   && std::abs(statePole(g)) < 1.0,
               "noise-source low-pass has an unstable TPT pole" + where);
    }

    {
        YouKnow106Engine hqEndpoint;
        hqEndpoint.prepare(endpointRate, 64, true);
        const double actual =
            YouKnow106TestAccess::noiseSourceLowPassG(hqEndpoint);
        const double hostRateMutation = std::tan(
            pi * (0.45 * endpointRate) / (4.0 * endpointRate));
        expect(std::abs(actual - hostRateMutation) > 0.05,
               "noise-source safety cap used the host rate on the 8k q4 path");
    }

    // Drive the two private support states directly. The former unclamped
    // 8 kHz coefficient has pole -2.007: one impulse reaches float infinity
    // in roughly 128 steps before the double-state finite guard eventually
    // resets it. The bounded endpoint recursion must instead decay normally.
    {
        YouKnow106Engine engine;
        engine.prepare(endpointRate, 64, false);
        bool finiteAndBounded = true;
        double worstState = 0.0;
        for (int index = 0; index < 4096; ++index)
        {
            const float output = YouKnow106TestAccess::processNoiseSourceSupport(
                engine, index == 0 ? 1.0f : 0.0f);
            const auto state =
                YouKnow106TestAccess::noiseSourceSupportState(engine);
            worstState = std::max({ worstState, std::abs(state[0]),
                                    std::abs(state[1]) });
            finiteAndBounded = finiteAndBounded
                && std::isfinite(output)
                && std::isfinite(state[0]) && std::isfinite(state[1])
                && std::abs(state[0]) < 8.0 && std::abs(state[1]) < 8.0;
            if (!finiteAndBounded)
                break;
        }
        expect(finiteAndBounded,
               "8k q1 noise-support impulse poisoned or escaped its states");
        expect(worstState > 0.01,
               "8k q1 noise-support state fixture did not exercise the pole");
    }

    EngineParameters noisePatch;
    noisePatch.sawEnabled = false;
    noisePatch.pulseEnabled = false;
    noisePatch.subLevel = 0.0f;
    noisePatch.noiseLevel = 1.0f;
    noisePatch.cutoff = 1.0f;
    noisePatch.resonance = 0.0f;
    noisePatch.envDepth = 0.0f;
    noisePatch.vcaMode = VcaMode::Gate;
    noisePatch.vcaLevel = 1.0f;
    noisePatch.attack = 0.0f;
    noisePatch.decay = 0.0f;
    noisePatch.sustain = 1.0f;
    noisePatch.release = 0.0f;
    noisePatch.chorus = ChorusMode::Off;
    noisePatch.volume = 1.0f;
    noisePatch.calibration = 0.0f;

    // The shared generator advances even with Noise at zero and no assigned
    // note. Check that idle path first, then enable a noise voice without a
    // reset: an endpoint bug must not hide as latent state poison behind the
    // later VCF/output finite guards.
    {
        YouKnow106Engine engine;
        engine.prepare(endpointRate, 32, false);
        std::array<float, 32> left {};
        std::array<float, 32> right {};
        bool idleStateSafe = true;
        for (int block = 0; block < 64; ++block)
        {
            engine.process(left.data(), right.data(),
                           static_cast<int>(left.size()));
            const auto state =
                YouKnow106TestAccess::noiseSourceSupportState(engine);
            idleStateSafe = idleStateSafe
                && std::isfinite(state[0]) && std::isfinite(state[1])
                && std::abs(state[0]) < 8.0 && std::abs(state[1]) < 8.0;
            if (!idleStateSafe)
                break;
        }
        const auto idleState =
            YouKnow106TestAccess::noiseSourceSupportState(engine);
        expect(idleStateSafe,
               "idle 8k q1 processing poisoned the unconditional noise source");
        expect(idleState[0] != 0.0 || idleState[1] != 0.0,
               "idle processing did not advance the shared noise support");

        engine.setParameters(noisePatch);
        engine.noteOn(60, 1.0f);
        double energy = 0.0;
        bool enabledStateSafe = true;
        for (int block = 0; block < 64; ++block)
        {
            engine.process(left.data(), right.data(),
                           static_cast<int>(left.size()));
            const auto state =
                YouKnow106TestAccess::noiseSourceSupportState(engine);
            enabledStateSafe = enabledStateSafe
                && std::isfinite(state[0]) && std::isfinite(state[1])
                && std::abs(state[0]) < 8.0 && std::abs(state[1]) < 8.0;
            for (const float sample : left)
            {
                enabledStateSafe = enabledStateSafe && std::isfinite(sample);
                energy += static_cast<double>(sample) * sample;
            }
            if (!enabledStateSafe)
                break;
        }
        expect(enabledStateSafe,
               "enabling Noise after an idle 8k q1 run exposed latent poison");
        expect(energy > 1.0e-8,
               "the post-idle 8k q1 noise voice rendered nothing");
    }

    // Host block boundaries must not change the free-running generator, its
    // two physical support states, or the downstream audio sequence.
    struct BlockRender
    {
        std::vector<float> left;
        std::vector<float> right;
        std::array<double, 2> state {};
    };
    const auto renderInBlocks = [&](int blockSize) {
        constexpr int length = 2048;
        YouKnow106Engine engine;
        engine.prepare(endpointRate, 256, false);
        engine.setParameters(noisePatch);
        engine.noteOn(60, 1.0f);
        BlockRender result;
        result.left.resize(length);
        result.right.resize(length);
        for (int offset = 0; offset < length; offset += blockSize)
        {
            const int count = std::min(blockSize, length - offset);
            engine.process(result.left.data() + offset,
                           result.right.data() + offset, count);
        }
        result.state =
            YouKnow106TestAccess::noiseSourceSupportState(engine);
        return result;
    };
    const auto singleBlock = renderInBlocks(2048);
    const auto oddBlocks = renderInBlocks(37);
    expect(singleBlock.left == oddBlocks.left
               && singleBlock.right == oddBlocks.right
               && singleBlock.state == oddBlocks.state,
           "8k q1 noise support changed with host block partitioning");

    // A hard reset clears both support voltages but does not redesign the
    // coefficient. A live numerical-rate rebuild preserves both voltages and
    // switches between the component corner at 32 kHz and the endpoint cap at
    // 8 kHz. This also rejects a cap accidentally based on the host rate.
    {
        YouKnow106Engine engine;
        engine.prepare(endpointRate, 64, true);
        const double hqG =
            YouKnow106TestAccess::noiseSourceLowPassG(engine);
        YouKnow106TestAccess::setNoiseSourceSupportState(engine, -0.125, 0.375);
        YouKnow106TestAccess::forceNoiseSourceProcessingQuality(engine, false);
        expect(engine.getOversamplingFactor() == 1,
               "forced 8k noise-support rate did not select q1");
        expect(YouKnow106TestAccess::noiseSourceSupportState(engine)
                   == std::array<double, 2> { -0.125, 0.375 },
               "q4-to-q1 rebuild reset the physical noise-support state");
        expectNear(YouKnow106TestAccess::noiseSourceLowPassG(engine),
                   expectedCoefficient(endpointRate), 1.0e-5,
                   "q4-to-q1 rebuild missed the 8k endpoint cap");

        YouKnow106TestAccess::forceNoiseSourceProcessingQuality(engine, true);
        expect(engine.getOversamplingFactor() == 4,
               "forced 8k noise-support rate did not return to q4");
        expect(YouKnow106TestAccess::noiseSourceSupportState(engine)
                   == std::array<double, 2> { -0.125, 0.375 },
               "q1-to-q4 rebuild reset the physical noise-support state");
        expectNear(YouKnow106TestAccess::noiseSourceLowPassG(engine), hqG,
                   0.0, "q1-to-q4 rebuild did not restore the physical corner");

        engine.reset();
        expect(YouKnow106TestAccess::noiseSourceSupportState(engine)
                   == std::array<double, 2> { 0.0, 0.0 },
               "hard reset retained noise-support voltage");
        expectNear(YouKnow106TestAccess::noiseSourceLowPassG(engine), hqG,
                   0.0, "hard reset redesigned the noise-support coefficient");
    }

    // Exercise the same q4<->q1 choice through the public idle safety fade,
    // not only the narrow state seam above.
    {
        YouKnow106Engine engine;
        engine.prepare(endpointRate, 16, true);
        std::array<float, 16> left {};
        std::array<float, 16> right {};
        const auto finishToggle = [&](bool enabled, int wantedFactor) {
            expect(!engine.setOversamplingEnabled(enabled),
                   "idle 8k quality toggle skipped its safety fade");
            bool safe = true;
            for (int block = 0;
                 block < 64 && engine.getOversamplingFactor() != wantedFactor;
                 ++block)
            {
                engine.process(left.data(), right.data(),
                               static_cast<int>(left.size()));
                const auto state =
                    YouKnow106TestAccess::noiseSourceSupportState(engine);
                safe = safe && std::isfinite(state[0]) && std::isfinite(state[1])
                    && std::abs(state[0]) < 8.0 && std::abs(state[1]) < 8.0;
            }
            expect(safe, "live 8k quality toggle poisoned noise-support state");
            expect(engine.getOversamplingFactor() == wantedFactor,
                   "live 8k quality toggle did not reach its requested factor");
        };

        finishToggle(false, 1);
        expectNear(YouKnow106TestAccess::noiseSourceLowPassG(engine),
                   expectedCoefficient(endpointRate), 1.0e-5,
                   "live 8k q1 path missed its internal-rate cap");
        finishToggle(true, 4);
        expectNear(YouKnow106TestAccess::noiseSourceLowPassG(engine),
                   expectedCoefficient(4.0 * endpointRate), 1.0e-5,
                   "live 8k q4 path retained a host-rate cap");
    }
}

void testCombinedBbdSupportTransitionAndStateSafety()
{
    using Transition = Chorus::SupportChain::ExactTransition;
    using Matrix = std::array<std::array<double, 6>, 6>;
    using State = std::array<double, 6>;

    // Independent component solve for the loaded tap node. Panasonic's
    // Gi-RL curve gives about 3.7 kOhm per output follower near the JUNO's
    // approximately 100 kOhm per-pin load. Roland then gives each follower a
    // 3.3 kOhm leg into the shared 47 kOhm / 2.2 nF node.
    constexpr float mn3009OutputSourceEstimate = 3700.0f;
    constexpr float tapSeries = 3300.0f;
    constexpr float tapReturn = 47000.0f;
    constexpr float tapShunt = 2.2e-9f;
    constexpr float parallelDrive =
        0.5f * (mn3009OutputSourceEstimate + tapSeries);
    constexpr float tapNodeResistance =
        parallelDrive * tapReturn / (parallelDrive + tapReturn);
    constexpr float isolatedTapCornerHzFloat = 1.0f
        / (2.0f * static_cast<float>(pi) * tapNodeResistance * tapShunt);
    constexpr double isolatedTapCornerHz =
        static_cast<double>(isolatedTapCornerHzFloat);
    expectNear(isolatedTapCornerHz, 22208.689, 0.01,
               "isolated MN3009 tap-corner component solve changed");

    // The tap capacitance is loaded by the first 22 kOhm resistor, so its
    // response is not the separable tap-pole times Sallen-Key approximation.
    // Check one closed-form phasor independently of the state rows/RK4 below.
    {
        constexpr double frequency = 10000.0;
        constexpr double series = 22000.0;
        constexpr double feedback = 820.0e-12;
        constexpr double shunt = 680.0e-12;
        const std::complex<double> s(0.0, 2.0 * pi * frequency);
        const double source = 0.5 * (3700.0 + 3300.0);
        const double sourceConductance = 1.0 / source + 1.0 / 47000.0;
        const double seriesConductance = 1.0 / series;
        const auto denominator = 1.0
            + s * shunt * (2.0 * series)
            + s * s * series * series * feedback * shunt;
        const auto numeratorShape = 1.0 + s * series * shunt;
        const auto loaded = sourceConductance
            / ((s * 2.2e-9 + sourceConductance + seriesConductance)
                   * denominator
               - seriesConductance * numeratorShape);
        const double tapResistance = source * 47000.0
                                   / (source + 47000.0);
        const auto separable = 1.0
            / ((1.0 + s * tapResistance * 2.2e-9) * denominator);
        const auto relative = loaded / separable;
        expectNear(20.0 * std::log10(std::abs(relative)),
                   -0.7883156, 1.0e-5,
                   "loaded tap/first-section magnitude left its MNA result");
        expectNear(std::arg(relative) * 180.0 / pi,
                   -2.0290835, 1.0e-5,
                   "loaded tap/first-section phase left its MNA result");
    }

    const auto transitionFinite = [](const Transition& transition) {
        for (const auto& column : transition.stateByColumn)
            for (double value : column)
                if (!std::isfinite(value))
                    return false;
        for (const auto& sample : transition.driveBySample)
            for (double value : sample)
                if (!std::isfinite(value))
                    return false;
        return true;
    };
    const auto equilibriumResidual = [](const Transition& transition,
                                        const State& equilibrium) {
        double worst = 0.0;
        for (std::size_t row = 0; row < 6; ++row)
        {
            double next = 0.0;
            for (std::size_t column = 0; column < 6; ++column)
                next += transition.stateByColumn[column][row]
                      * equilibrium[column];
            for (std::size_t sample = 0; sample < 4; ++sample)
                next += transition.driveBySample[sample][row];
            worst = std::max(worst, std::abs(next - equilibrium[row]));
        }
        return worst;
    };
    const auto transitionStep = [](const Transition& transition,
                                   const State& state,
                                   const std::array<double, 4>& samples) {
        State next {};
        for (std::size_t column = 0; column < 6; ++column)
            for (std::size_t row = 0; row < 6; ++row)
                next[row] += transition.stateByColumn[column][row]
                           * state[column];
        for (std::size_t sample = 0; sample < 4; ++sample)
            for (std::size_t row = 0; row < 6; ++row)
                next[row] += transition.driveBySample[sample][row]
                           * samples[sample];
        return next;
    };
    const auto cubic = [](const std::array<double, 4>& samples, double tau) {
        const double first = samples[0] / 3.0 + samples[1] / 2.0
                           - samples[2] + samples[3] / 6.0;
        const double second = samples[0] - 2.0 * samples[1] + samples[2];
        const double third = samples[0] - 3.0 * samples[1]
                           + 3.0 * samples[2] - samples[3];
        return samples[1] + tau * (first + tau * (
            0.5 * second + tau * third / 6.0));
    };
    const auto rk4 = [&](const Matrix& matrix, const State& drive,
                         State state, const std::array<double, 4>& samples,
                         double sampleRate) {
        constexpr int substeps = 4096;
        const double step = 1.0 / static_cast<double>(substeps);
        const double interval = 1.0 / sampleRate;
        const auto derivative = [&](const State& at, double input) {
            State slope {};
            for (std::size_t row = 0; row < 6; ++row)
            {
                for (std::size_t column = 0; column < 6; ++column)
                    slope[row] += matrix[row][column] * at[column];
                slope[row] = interval * (slope[row] + drive[row] * input);
            }
            return slope;
        };
        const auto advanced = [](const State& at, const State& slope,
                                 double amount) {
            State result {};
            for (std::size_t index = 0; index < 6; ++index)
                result[index] = at[index] + amount * slope[index];
            return result;
        };
        for (int substep = 0; substep < substeps; ++substep)
        {
            const double tau = static_cast<double>(substep) * step;
            const auto k1 = derivative(state, cubic(samples, tau));
            const auto k2 = derivative(
                advanced(state, k1, 0.5 * step),
                cubic(samples, tau + 0.5 * step));
            const auto k3 = derivative(
                advanced(state, k2, 0.5 * step),
                cubic(samples, tau + 0.5 * step));
            const auto k4 = derivative(
                advanced(state, k3, step), cubic(samples, tau + step));
            for (std::size_t index = 0; index < 6; ++index)
                state[index] += step * (k1[index] + 2.0 * k2[index]
                                      + 2.0 * k3[index] + k4[index]) / 6.0;
        }
        return state;
    };
    const auto maximumDifference = [](const State& left,
                                      const State& right) {
        double worst = 0.0;
        for (std::size_t index = 0; index < 6; ++index)
            worst = std::max(worst, std::abs(left[index] - right[index]));
        return worst;
    };
    const auto inputMatrix = [] {
        const double w1 = 2.0 * pi * static_cast<double>(9688.0f);
        const double w2 = 2.0 * pi * static_cast<double>(10377.0f);
        const double wc = 2.0 * pi * static_cast<double>(15.9155f);
        const double wp = 2.0 * pi * static_cast<double>(7234.0f);
        const double q1 = Chorus::sallenKeyQ(820.0e-12f, 680.0e-12f);
        const double q2 = Chorus::sallenKeyQ(1.8e-9f, 270.0e-12f);
        Matrix matrix {};
        matrix[0][0] = -w1 / q1;
        matrix[0][1] = -w1;
        matrix[1][0] = w1;
        matrix[2][1] = w2;
        matrix[2][2] = -w2 / q2;
        matrix[2][3] = -w2;
        matrix[3][2] = w2;
        matrix[4][3] = wc;
        matrix[4][4] = -wc;
        matrix[5][3] = wp;
        matrix[5][4] = -wp;
        matrix[5][5] = -wp;
        return matrix;
    };
    const auto outputMatrix = [](bool connected) {
        const double source = 0.5 * (
            static_cast<double>(3300.0f) + static_cast<double>(3700.0f));
        const double tapReturn = static_cast<double>(47000.0f);
        const double tapCap = static_cast<double>(2.2e-9f);
        const double series = static_cast<double>(22000.0f);
        const double firstFeedback = static_cast<double>(820.0e-12f);
        const double firstShunt = static_cast<double>(680.0e-12f);
        const double secondFeedback = static_cast<double>(1.8e-9f);
        const double secondShunt = static_cast<double>(270.0e-12f);
        const double wc = 2.0 * pi * static_cast<double>(
            Chorus::wetOutputCouplingCornerHz(connected));
        Matrix matrix {};
        matrix[0][0] = -(1.0 / source + 1.0 / tapReturn + 1.0 / series)
                     / tapCap;
        matrix[0][1] = 1.0 / (series * tapCap);
        matrix[1][0] = 1.0 / (series * firstFeedback);
        matrix[1][1] = 1.0 / (series * firstShunt)
                     - 2.0 / (series * firstFeedback);
        matrix[1][2] = -1.0 / (series * firstShunt)
                     + 1.0 / (series * firstFeedback);
        matrix[2][1] = 1.0 / (series * firstShunt);
        matrix[2][2] = -1.0 / (series * firstShunt);
        matrix[3][2] = 1.0 / (series * secondFeedback);
        matrix[3][3] = 1.0 / (series * secondShunt)
                     - 2.0 / (series * secondFeedback);
        matrix[3][4] = -1.0 / (series * secondShunt)
                     + 1.0 / (series * secondFeedback);
        matrix[4][3] = 1.0 / (series * secondShunt);
        matrix[4][4] = -1.0 / (series * secondShunt);
        matrix[5][4] = wc;
        matrix[5][5] = -wc;
        return matrix;
    };

    State inputDrive {};
    inputDrive[0] = 2.0 * pi * static_cast<double>(9688.0f);
    State outputDrive {};
    constexpr double outputSource = 0.5 * (
        static_cast<double>(3300.0f) + static_cast<double>(3700.0f));
    constexpr double outputReturn = static_cast<double>(47000.0f);
    constexpr double outputCap = static_cast<double>(2.2e-9f);
    outputDrive[0] = (1.0 / outputSource + 1.0 / outputReturn) / outputCap;
    constexpr State inputEquilibrium { 0.0, 1.0, 0.0, 1.0, 1.0, 0.0 };
    constexpr State outputEquilibrium { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
    constexpr State initial { 0.03, -0.07, 0.11, -0.13, 0.17, -0.19 };
    constexpr std::array<double, 4> samples { 0.2, -0.1, 0.05, -0.03 };

    // Endpoint rates exercise the matrix exponential's widest conditioning
    // range; the middle rows cover both common hosts and both shipping HQ
    // grids so a rate-specific matrix defect cannot hide between endpoints.
    for (double sampleRate : {
             8000.0, 44100.0, 48000.0, 176400.0, 192000.0, 768000.0 })
    {
        const auto support = Chorus::supportChainFor(
            static_cast<float>(sampleRate));
        expect(transitionFinite(support.exactInput)
                   && transitionFinite(support.exactOutputMuted)
                   && transitionFinite(support.exactOutputConnected),
               "combined BBD support transition is non-finite at "
                   + std::to_string(sampleRate) + " Hz");
        expect(equilibriumResidual(support.exactInput, inputEquilibrium)
                   <= 2.0e-15,
               "combined input support leaks its exact DC equilibrium");
        expect(equilibriumResidual(
                   support.exactOutputMuted, outputEquilibrium) <= 2.0e-15
                   && equilibriumResidual(
                       support.exactOutputConnected, outputEquilibrium)
                          <= 2.0e-15,
               "combined output support leaks its exact DC equilibrium");

        const double inputError = maximumDifference(
            transitionStep(support.exactInput, initial, samples),
            rk4(inputMatrix(), inputDrive, initial, samples, sampleRate));
        const double mutedError = maximumDifference(
            transitionStep(support.exactOutputMuted, initial, samples),
            rk4(outputMatrix(false), outputDrive, initial, samples,
                sampleRate));
        const double connectedError = maximumDifference(
            transitionStep(support.exactOutputConnected, initial, samples),
            rk4(outputMatrix(true), outputDrive, initial, samples,
                sampleRate));
        expect(std::max({ inputError, mutedError, connectedError }) <= 2.0e-11,
               "combined BBD support disagrees with independent RK4 at "
                   + std::to_string(sampleRate) + " Hz");

        Chorus chorus;
        chorus.prepare(sampleRate);
        const int driveFrames = std::max(
            16, static_cast<int>(sampleRate * 0.02));
        const int decayFrames = std::max(
            16, static_cast<int>(sampleRate * 0.50));
        const int toggleFrames = std::max(
            1, static_cast<int>(sampleRate * 0.003));
        std::uint32_t random = 0x1234567u;
        double maximumState = 0.0;
        bool allFinite = true;
        for (int frame = 0; frame < driveFrames + decayFrames; ++frame)
        {
            random = random * 1664525u + 1013904223u;
            const float randomSample = static_cast<float>(random >> 8)
                                     * (2.0f / 16777215.0f) - 1.0f;
            const float input = frame < driveFrames
                ? 0.2f * randomSample : 0.0f;
            const bool connected = ((frame / toggleFrames) & 1) == 0;
            const float output = YouKnow106TestAccess::processBbdExactMode(
                chorus, input, 200000.0f, connected);
            allFinite = allFinite && std::isfinite(output)
                && YouKnow106TestAccess::chorusExactHistoriesAreFinite(chorus);
            for (double value : YouKnow106TestAccess::chorusExactInputState(
                     chorus))
            {
                allFinite = allFinite && std::isfinite(value);
                maximumState = std::max(maximumState, std::abs(value));
            }
            for (double value : YouKnow106TestAccess::chorusExactOutputState(
                     chorus))
            {
                allFinite = allFinite && std::isfinite(value);
                maximumState = std::max(maximumState, std::abs(value));
            }
        }
        expect(allFinite && maximumState < 10.0,
               "combined BBD support is unstable under coupling switches at "
                   + std::to_string(sampleRate) + " Hz");

        const float nanOutput = YouKnow106TestAccess::processBbdExactMode(
            chorus, std::numeric_limits<float>::quiet_NaN(), 200000.0f, true);
        const float infiniteOutput = YouKnow106TestAccess::processBbdExactMode(
            chorus, std::numeric_limits<float>::infinity(), 200000.0f, false);
        const float maximumOutput = YouKnow106TestAccess::processBbdExactMode(
            chorus, std::numeric_limits<float>::max(), 200000.0f, true);
        expect(std::isfinite(nanOutput) && std::isfinite(infiniteOutput)
                   && std::isfinite(maximumOutput),
               "combined BBD support emitted a non-finite hostile response");
        expect(YouKnow106TestAccess::chorusExactHistoriesAreFinite(chorus),
               "combined BBD support retained a hostile non-finite input");
        for (double value : YouKnow106TestAccess::chorusExactInputState(chorus))
            expect(std::isfinite(value),
                   "combined BBD input state escaped finite containment");
        for (double value : YouKnow106TestAccess::chorusExactOutputState(chorus))
            expect(std::isfinite(value),
                   "combined BBD output state escaped finite containment");

        double recoveryPeak = 0.0;
        for (int frame = 0; frame < 1024; ++frame)
            recoveryPeak = std::max(recoveryPeak, std::abs(static_cast<double>(
                YouKnow106TestAccess::processBbdExactMode(
                    chorus, 0.0f, 200000.0f, true))));
        expect(recoveryPeak < 0.05,
               "one corrupt BBD input poisoned later support state (peak "
                   + std::to_string(recoveryPeak) + ")");

        chorus.prepare(sampleRate == 768000.0 ? 8000.0
                                              : sampleRate + 1.0, true);
        expect(YouKnow106TestAccess::chorusAudioRateSupportIsClear(chorus),
               "rate reset retained combined support state or cubic history");
    }

    // The selector is an explicit numerical-quality policy, not an emergent
    // consequence of a host-rate comparison. Fence both adjacent float grids:
    // the lower one must leave the exact input state untouched, while the HQ
    // threshold must route the same signal through it.
    const float belowExactRate = std::nextafter(
        Chorus::minimumExactInputSupportRate, 0.0f);
    for (const auto& [rate, shouldUseExact] : {
             std::pair { belowExactRate, false },
             std::pair { Chorus::minimumExactInputSupportRate, true } })
    {
        Chorus chorus;
        chorus.prepare(rate);
        for (int frame = 0; frame < 32; ++frame)
            (void) YouKnow106TestAccess::processBbdExactMode(
                chorus, frame == 0 ? 0.25f : 0.0f, 50000.0f, true);
        const bool exactStateMoved = std::any_of(
            YouKnow106TestAccess::chorusExactInputState(chorus).begin(),
            YouKnow106TestAccess::chorusExactInputState(chorus).end(),
            [](double value) { return value != 0.0; });
        expect(exactStateMoved == shouldUseExact,
               "BBD exact-input selector moved at its 176.4 kHz boundary");
    }
}

void testSupportFilterCornersLandWhereAsked()
{
    const auto gainAt = [](double frequency, float g, float sampleRate) {
        // A whole number of cycles in the window, so the correlation is exact
        // and no leakage creeps into the estimate.
        constexpr int cycles = 64;
        constexpr int settle = 4096;
        const int window = static_cast<int>(
            std::llround(cycles * static_cast<double>(sampleRate) / frequency));
        float state = 0.0f;
        std::complex<double> accumulator {};
        for (int index = 0; index < settle + window; ++index)
        {
            const double phase = 2.0 * pi * frequency * index / sampleRate;
            const float output = Chorus::supportFilterStep(
                state, static_cast<float>(std::sin(phase)), g);
            if (index >= settle)
                accumulator += static_cast<double>(output)
                             * std::exp(std::complex<double>(0.0, -phase));
        }
        return 2.0 * std::abs(accumulator) / window;
    };

    const auto highPassGainAt = [](double frequency, float g, float sampleRate) {
        constexpr int cycles = 64;
        constexpr int settle = 4096;
        const int window = static_cast<int>(
            std::llround(cycles * static_cast<double>(sampleRate) / frequency));
        float state = 0.0f;
        std::complex<double> accumulator {};
        for (int index = 0; index < settle + window; ++index)
        {
            const double phase = 2.0 * pi * frequency * index / sampleRate;
            const float input = static_cast<float>(std::sin(phase));
            const float low = Chorus::supportFilterStep(state, input, g);
            if (index >= settle)
                accumulator += static_cast<double>(input - low)
                             * std::exp(std::complex<double>(0.0, -phase));
        }
        return 2.0 * std::abs(accumulator) / window;
    };

    // A lowpass passes direct current untouched, whatever its corner. Checking
    // it separately means the corner search below can be normalised against an
    // exact figure rather than another measurement.
    {
        const float g = Chorus::onePoleG(9900.0f, 192000.0f);
        float state = 0.0f;
        float settled = 0.0f;
        for (int index = 0; index < 8192; ++index)
            settled = Chorus::supportFilterStep(state, 1.0f, g);
        expectNear(settled, 1.0, 1.0e-5, "the support filter does not pass DC");
    }

    // The TPT checks below now own only the low-rate input fallback. Output
    // support is exclusively the independently checked exact transition in
    // testCombinedBbdSupportTransitionAndStateSafety().
    {
        const auto chain = Chorus::supportChainFor(48000.0f);
        expectNear(highPassGainAt(15.9155, chain.inputCouplingG, 48000.0f),
                   0.70710678, 0.005,
                   "the wet coupling high-pass is not at C44/R120's corner");
        constexpr double capacitor = 1.0e-6;
        constexpr double bleed = 22000.0;
        constexpr double mixer = 39000.0;
        const double connectedResistance = bleed * mixer / (bleed + mixer);
        const double mutedCorner = 1.0 / (2.0 * pi * capacitor * bleed);
        const double connectedCorner =
            1.0 / (2.0 * pi * capacitor * connectedResistance);
        expectNear(Chorus::wetOutputCouplingCornerHz(false), mutedCorner,
                   1.0e-5, "muted wet-output coupling corner");
        expectNear(Chorus::wetOutputCouplingCornerHz(true), connectedCorner,
                   1.0e-5, "engaged wet-output coupling corner");
    }

    const auto measureCorner = [&gainAt](float requested, float sampleRate) {
        const float g = Chorus::onePoleG(requested, sampleRate);
        // Bisect for the half-power point rather than assuming the shape. The
        // lower bound is far below any corner the circuit uses; a filter that
        // converged onto it would fail the assertion, which is the point.
        double low = 200.0;
        double high = sampleRate * 0.45;
        for (int step = 0; step < 40; ++step)
        {
            const double middle = 0.5 * (low + high);
            if (gainAt(middle, g, sampleRate) > 0.70710678)
                low = middle;
            else
                high = middle;
        }
        return 0.5 * (low + high);
    };

    // The internal rate the engine runs the chorus at, and the lowest host rate
    // it accepts -- where the prewarping matters most.
    expectNear(measureCorner(9900.0f, 192000.0f), 9900.0, 60.0,
               "anti-alias corner at the internal rate");
    expectNear(measureCorner(9900.0f, 48000.0f), 9900.0, 60.0,
               "one-pole corner without oversampling");

    // The two-pole sections either side of the line. Their Q comes from the
    // capacitor ratio alone, so it is asserted from the parts rather than
    // written down -- and the corner is measured the same way the one-pole's
    // is, by bisecting the realised half-power point, because a coefficient
    // that agrees with its own recursion is the only thing worth checking.
    expectNear(Chorus::sallenKeyQ(820.0e-12f, 680.0e-12f), 0.5494, 1.0e-3,
               "the first section's Q is not its capacitor ratio");
    expectNear(Chorus::sallenKeyQ(1.8e-9f, 270.0e-12f), 1.2910, 1.0e-3,
               "the second section's Q is not its capacitor ratio");

    const auto biquadGainAt = [](double frequency, const Chorus::BiquadCoefficients& c,
                                 float sampleRate) {
        constexpr int cycles = 64;
        constexpr int settle = 4096;
        const int window = static_cast<int>(
            std::llround(cycles * static_cast<double>(sampleRate) / frequency));
        Chorus::BiquadState state {};
        std::complex<double> accumulator {};
        for (int index = 0; index < settle + window; ++index)
        {
            const double phase = 2.0 * pi * frequency * index / sampleRate;
            const float output = Chorus::biquadStep(
                state, static_cast<float>(std::sin(phase)), c);
            if (index >= settle)
                accumulator += static_cast<double>(output)
                             * std::exp(std::complex<double>(0.0, -phase));
        }
        return 2.0 * std::abs(accumulator) / window;
    };

    // A two-pole lowpass has one property that pins both of its coefficients at
    // once: its gain *at* the corner is exactly Q. Asserting that is sharper
    // than bisecting for a half-power point, because the half-power point moves
    // with Q and so would pass for a section whose damping was wrong in a way
    // that happened to shift the -3 dB frequency back.
    //
    // A section running at half its intended Q -- which is what an extra factor
    // of two in the damping term produces -- fails this by a factor of two.
    for (const float rate : { 192000.0f, 48000.0f })
    {
        struct Section { float hz; float q; const char* name; };
        for (const auto& section : { Section { 9688.0f, 0.5494f, "first" },
                                     Section { 10377.0f, 1.2910f, "second" } })
        {
            const auto c = Chorus::sallenKeyCoefficients(section.hz, section.q, rate);
            const double reference = biquadGainAt(20.0, c, rate);
            const double atCorner = biquadGainAt(section.hz, c, rate);
            expectNear(atCorner / reference, section.q, 0.02,
                       std::string("the ") + section.name
                           + " two-pole section's gain at its corner is not its Q");
        }
    }

    // And a two-pole section passes DC like any other lowpass.
    {
        const auto c = Chorus::sallenKeyCoefficients(9688.0f, 0.5494f, 192000.0f);
        Chorus::BiquadState state {};
        float settled = 0.0f;
        for (int index = 0; index < 8192; ++index)
            settled = Chorus::biquadStep(state, 1.0f, c);
        expectNear(settled, 1.0, 1.0e-5, "a two-pole section does not pass DC");
    }
}

// `sallenKeyQ`'s own non-positive shunt-capacitance fallback guard -- its only
// two production call sites (the anti-alias sections in `supportChainFor`)
// always pass one of the four compile-time capacitor constants exercised
// above, all of them positive, so the guard has never fired outside a test.
// Without it, a zero or negative shunt would divide by zero or take a square
// root of a negative ratio, handing an infinite or NaN Q into
// `sallenKeyCoefficients`'s `1.0f / std::max(q, 0.05f)` and, from there, into
// the Sallen-Key recursion itself.
void testSallenKeyQNonPositiveShuntGuard()
{
    expect(Chorus::sallenKeyQ(820.0e-12f, 0.0f) == 0.5f,
           "sallenKeyQ did not fall back to 0.5 for a zero shunt capacitance");
    expect(Chorus::sallenKeyQ(820.0e-12f, -680.0e-12f) == 0.5f,
           "sallenKeyQ did not fall back to 0.5 for a negative shunt capacitance");
}

void testCorrectionResidualsVanishAtTheEdges()
{
    // The bandlimiting residuals are built by integration at construction, so
    // the cheapest way to catch a broken table is to confirm a rendered ramp
    // has the harmonic series a ramp should have and nothing else. The engine
    // suite measures the alias floor; here we only confirm the oscillator is
    // producing a ramp at the pitch the note timer was programmed for.
    YouKnow106Engine engine;
    engine.prepare(192000.0, 512, false);

    EngineParameters parameters;
    parameters.sawEnabled = true;
    parameters.pulseEnabled = false;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.envDepth = 0.0f;
    parameters.keyFollow = 0.0f;
    parameters.attack = 0.0f;
    parameters.sustain = 1.0f;
    parameters.vcaLevel = 1.0f;
    parameters.volume = 1.0f;
    parameters.calibration = 0.0f;
    parameters.chorus = ChorusMode::Off;
    engine.setParameters(parameters);
    engine.noteOn(69, 1.0f);

    constexpr int blockSize = 512;
    constexpr int blocks = 192;
    std::vector<float> left(blockSize * blocks);
    std::vector<float> right(blockSize * blocks);
    for (int block = 0; block < blocks; ++block)
        engine.process(left.data() + block * blockSize,
                       right.data() + block * blockSize, blockSize);

    // Measure between the first and last rising crossing rather than counting
    // them over a fixed window: the count alone only resolves to one cycle.
    std::size_t first = 0;
    std::size_t last = 0;
    int intervals = -1;
    for (std::size_t index = left.size() / 2 + 1; index < left.size(); ++index)
        if (left[index - 1] <= 0.0f && left[index] > 0.0f)
        {
            if (intervals < 0)
                first = index;
            last = index;
            ++intervals;
        }
    expect(intervals > 50, "the oscillator produced too few cycles to measure");
    const double measured = intervals > 0
        ? intervals * 192000.0 / static_cast<double>(last - first) : 0.0;
    const double programmed = YouKnow106Engine::dcoQuantisedFrequency(
        YouKnow106Engine::dcoDivider(440.0), DcoRange::Eight);
    expectNear(measured, programmed, 0.5,
               "the rendered ramp is not at the frequency the timer was given");
}
void testOutputSummerIsLinearBelowItsAsymptote()
{
    // IC6 runs on +/-15 V and the audio it carries is a few volts, so the stage
    // should be numerically linear there. The provisional model bends only as
    // it nears its 13.5 V loaded-swing asymptote. A tanh cannot do this: its
    // distortion rises as (V/asymptote)^2 from zero, which is what put roughly
    // 0.3% third harmonic on every sample.
    const auto thirdHarmonicFraction = [](double peakVolts) {
        constexpr int points = 4096;
        const double amplitude =
            peakVolts / static_cast<double>(YouKnow106Engine::internalVoltsPerUnit);
        double first = 0.0;
        double third = 0.0;
        for (int index = 0; index < points; ++index)
        {
            const double angle = 2.0 * 3.14159265358979323846
                               * static_cast<double>(index) / points;
            const double output = YouKnow106Engine::outputSummerClip(
                static_cast<float>(amplitude * std::cos(angle)));
            first += output * std::cos(angle);
            third += output * std::cos(3.0 * angle);
        }
        return std::abs(third) / std::max(std::abs(first), 1.0e-18);
    };

    // The declared nominal internal coordinate, and a hot passage above it.
    expect(thirdHarmonicFraction(2.6) < 5.0e-4,
           "the output summer distorts at its own nominal level");
    expect(thirdHarmonicFraction(5.0) < 5.0e-3,
           "the output summer distorts well below its modelled asymptote");

    // It must still be a bound: no input may pass the modelled asymptote.
    constexpr float asymptote =
        YouKnow106Engine::outputSummerSwingAsymptoteVolts
        / YouKnow106Engine::internalVoltsPerUnit;
    for (const float drive : {
             0.0f, std::numeric_limits<float>::denorm_min(),
             -std::numeric_limits<float>::denorm_min(), 0.25f, -1.0f,
             3.0f, -asymptote, 10.0f, std::numeric_limits<float>::max() })
    {
        const double normalised = std::abs(static_cast<double>(drive))
                                / static_cast<double>(asymptote);
        const float reference = static_cast<float>(
            static_cast<double>(drive)
            / algebraicSoftClipDenominator(normalised, 8.0));
        expectNear(YouKnow106Engine::outputSummerClip(drive), reference,
                   2.0e-6,
                   "the fixed exponent-eight output clip left its reference "
                   "curve");
    }
    for (const float drive : { 10.0f, 100.0f, 1.0e6f })
    {
        expect(std::abs(YouKnow106Engine::outputSummerClip(drive)) <= asymptote,
               "the output summer passed its positive model asymptote");
        expect(std::abs(YouKnow106Engine::outputSummerClip(-drive)) <= asymptote,
               "the output summer passed its negative model asymptote");
    }
    expect(std::isfinite(YouKnow106Engine::outputSummerClip(
               std::numeric_limits<float>::max())),
           "the output summer did not survive an extreme finite input");

    // Every value process() feeds in is already sanitised upstream, so the
    // function's own non-finite guard (returning silence rather than
    // propagating NaN or an infinity through std::pow) never fires there.
    // Call it directly with the inputs the guard exists for.
    expect(YouKnow106Engine::outputSummerClip(
               std::numeric_limits<float>::quiet_NaN()) == 0.0f,
           "the output summer did not silence a NaN input");
    expect(YouKnow106Engine::outputSummerClip(
               std::numeric_limits<float>::infinity()) == 0.0f,
           "the output summer did not silence a positive-infinity input");
    expect(YouKnow106Engine::outputSummerClip(
               -std::numeric_limits<float>::infinity()) == 0.0f,
           "the output summer did not silence a negative-infinity input");

    // Odd symmetry: the summer has no offset to add.
    expectNear(YouKnow106Engine::outputSummerClip(3.0f),
               -YouKnow106Engine::outputSummerClip(-3.0f), 1.0e-6,
               "the output summer is not symmetric");
}

void testOutputSummerBandwidthFollowsNoiseGain()
{
    // The inverting summer's signal gains are not its bandwidth-setting gain.
    // Reconstruct the noise gain independently from the two live input legs;
    // omitting either leg would overstate IC6's closed-loop bandwidth.
    constexpr double feedback = 100000.0;
    constexpr double dry = 47000.0;
    constexpr double wet = 39000.0;
    const double parallel = dry * wet / (dry + wet);
    const double noiseGain = 1.0 + feedback / parallel;
    expectNear(noiseGain, 5.691762, 1.0e-6,
               "the output-summer resistor noise gain is wrong");
    expectNear(YouKnow106Engine::outputSummerBandwidthHz(),
               3.0e6 / noiseGain, 0.1,
               "the TA75558 gain-bandwidth did not close through IC6's "
               "noise gain");
}

void testOutputResistorNoiseFollowsJohnsonNyquistLaw()
{
    // Re-refer the three independent IC6 resistor sources to its output. The
    // density is deliberately tested in V/sqrt(Hz), before any sample-rate or
    // engine-normalisation policy can hide a circuit error.
    constexpr double k = 1.380649e-23;
    constexpr double temperature = 298.15;
    constexpr double rf = 100000.0;
    constexpr double rdry = 47000.0;
    constexpr double rwet = 39000.0;
    const double equivalent = rf + rf * rf / rdry + rf * rf / rwet;
    const double expectedDensity = std::sqrt(4.0 * k * temperature * equivalent);
    expectNear(YouKnow106Engine::outputSummerResistorNoiseDensity(),
               expectedDensity, 1.0e-12,
               "IC6 resistor noise did not add as independent powers");

    // Solve the post-coupling wiper Thevenin resistance independently at both
    // stops and mid travel. At zero the grounded lower pot segment shorts the
    // output noise; at other positions all three paths remain in parallel.
    constexpr double pot = 10000.0;
    constexpr double series = 1500.0;
    constexpr double selector = 41300.0;
    constexpr double headphone = 101000.0;
    constexpr double load = selector * headphone / (selector + headphone);
    const auto reference = [=](double position) {
        const double upper = series + (1.0 - position) * pot;
        const double lower = position * pot;
        if (lower == 0.0)
            return 0.0;
        return 1.0 / (1.0 / upper + 1.0 / lower + 1.0 / load);
    };
    for (const double position : { 0.0, 0.5, 1.0 })
        expectNear(YouKnow106Engine::outputWiperNoiseResistance(
                       static_cast<float>(position)),
                   reference(position), 0.01,
                   "the loaded output wiper has the wrong noise resistance");
}

void testOutputSummerSlewMatchesDatasheetTypical()
{
    // Toshiba specifies 1.0 V/us typical at +/-15 V, 25 C, unity gain and a
    // 2 kOhm load. The installed load is different and the table gives no
    // minimum or maximum, so this pins a nominal policy rather than closing
    // OQ-05. Reconstruct volts/second from the prepared per-sample step at
    // several host rates and quality factors so neither grid can retime it.
    struct RateCase { double hostRate; int factor; };
    constexpr std::array<RateCase, 6> rates {{
        { 8000.0, 1 }, { 44100.0, 1 }, { 44100.0, 2 },
        { 44100.0, 4 }, { 48000.0, 4 }, { 192000.0, 4 }
    }};
    for (const auto& rate : rates)
    {
        YouKnow106Engine engine;
        engine.prepare(rate.hostRate, 64, rate.factor);
        expectNear(YouKnow106TestAccess::outputSlewVoltsPerSecond(engine),
                   1.0e6, 1.0,
                   "IC6 slew changed with host rate or quality factor");
    }
}

void testFilterCoreDividerMatchesSchematic()
{
    // Roland's BANK 3 procedure measures a self-oscillating VCF with every
    // source off, then trims VCA output. It cannot establish the still-voiced
    // WAVE-to-VCF-input level. What the schematic does establish is the
    // IR3109's internal 68 kOhm / 560 Ohm divider and the 2 Vt differential-
    // pair span it refers back to the module-node coordinate.
    expectNear(YouKnow106TestAccess::stageAttenuation(),
               560.0 / (68000.0 + 560.0), 1.0e-9,
               "the filter-core input divider left the Roland schematic");
    expectNear(YouKnow106TestAccess::otaHeadroomVolts(), 2.0 * 0.026
                   / (560.0 / 68560.0), 1.0e-4,
               "the transconductor headroom is no longer 2 Vt over the "
               "IR3109's own input divider");
}

void testDecimatorProtectsTheTopOfTheBand()
{
    // The last decimation stage runs at twice the host rate, so everything it
    // fails to remove around the host rate lands back inside the audio band.
    // A 44.1 kHz host is the hard case: 20 kHz sits at 0.227 of the stage's
    // own rate, only just below the quarter-rate crossover, and the content
    // that folds onto it comes from just above.
    const auto kernel = YouKnow106TestAccess::halfbandKernel();
    expect(kernel.size() == 95, "the decimation kernel is not 95 taps");
    for (std::size_t tap = 0; tap < kernel.size() / 2; ++tap)
        expect(kernel[tap] == kernel[kernel.size() - 1 - tap],
               "the paired decimator requires a bit-symmetric kernel");

    const auto response = [&kernel](double hertz, double stageRate) {
        std::complex<double> accumulator {};
        const double omega = 2.0 * pi * hertz / stageRate;
        for (std::size_t tap = 0; tap < kernel.size(); ++tap)
            accumulator += static_cast<double>(kernel[tap])
                         * std::exp(std::complex<double>(
                               0.0, -omega * static_cast<double>(tap)));
        return 20.0 * std::log10(std::max(std::abs(accumulator), 1.0e-30));
    };

    expectNear(response(0.0, 96000.0), 0.0, 1.0e-6,
               "the decimation kernel does not have unity gain at DC");

    struct HostCase { double host; double passbandDb; double foldDb; };
    // Both bounds are what this kernel measures, with a little margin. The
    // previous 63-tap Kaiser passed its original narrow transfer check but
    // rejected the 25.1 kHz pulse harmonic folding onto 19.0 kHz by only
    // about 50 dB in the expanded DCO matrix.
    constexpr std::array<HostCase, 2> cases {{
        { 44100.0, -0.05, -79.0 },
        { 48000.0, -0.01, -84.0 }
    }};
    for (const auto& host : cases)
    {
        const double stageRate = 2.0 * host.host;
        for (double hertz = 0.0; hertz <= 20000.0; hertz += 250.0)
        {
            const double gain = response(hertz, stageRate);
            expect(gain <= 0.05 && gain >= host.passbandDb,
                   "the decimator is " + std::to_string(gain)
                       + " dB at " + std::to_string(hertz) + " Hz on a "
                       + std::to_string(static_cast<int>(host.host))
                       + " Hz host");
        }
        // Content folding onto 19.1 kHz arrives from either side of the host
        // rate, and both images have to be rejected.
        for (const double source : { host.host - 19100.0, host.host + 19100.0 })
            expect(response(source, stageRate) < host.foldDb,
                   "content at " + std::to_string(source)
                       + " Hz folds onto 19.1 kHz at only "
                       + std::to_string(response(source, stageRate)) + " dB");
    }

    // Pin the exact expanded-matrix failure that selected this length: the
    // sixth harmonic of the high 4' pulse at 44.1 kHz. Both spectral images
    // fold onto about 18.995 kHz at the host boundary.
    constexpr double causalLeakHz = 25104.602510;
    constexpr double hardStageRate = 88200.0;
    for (const double source : { causalLeakHz, hardStageRate - causalLeakHz })
        expect(response(source, hardStageRate) < -79.0,
               "the high-pulse sixth harmonic is rejected by only "
                   + std::to_string(response(source, hardStageRate)) + " dB");
}
} // namespace

int main()
{
    testOutputSummerIsLinearBelowItsAsymptote();
    testOutputSummerBandwidthFollowsNoiseGain();
    testOutputResistorNoiseFollowsJohnsonNyquistLaw();
    testOutputSummerSlewMatchesDatasheetTypical();
    testDecimatorProtectsTheTopOfTheBand();
    testFilterCoreDividerMatchesSchematic();
    testCascadeAgainstReferenceSolve();
    testCascadeOscillationThreshold();
    testCascadeSurvivesAdversarialControl();
    testCascadeTracksHotContinuousReference();
    testNoteTimerLaw();
    testNoteTimerDividerDefensiveGuard();
    testCutoffControlLaw();
    testStoredControlDigitalVectors();
    testResonanceLeavesTheCornerAloneBelowOscillation();
    testVoicedResonanceCompatibilityProfile();
    testCircuitDerivedResonanceProfile();
    testEnvelopeAndAmplifierLaws();
    testPulseWidthAndHighPassLaws();
    testPwmDutyCycleDefensiveGuard();
    testSharedHighPassAgainstNominalNetwork();
    testServiceSpecificationEndpointReconciliation();
    testModulationAndGlideLaws();
    testConverterQueueAndOutputReference();
    testPanelLawsInvert();
    testPanelLawInversesRejectNonPositiveInput();
    testComparatorEdgesSitOnOneThreshold();
    testChorusIsAtItsSettingFromTheFirstSample();
    testJuno60FallbackBucketBrigadeTiming();
    testChorusRateProportionalNoiseGainMatchesTheDerivedRatio();
    testChorusNoiseMeasurementPointsAndProductPolicy();
    testChorusNoiseComponents();
    testCorrelatedRandomStepCorrelationGuard();
    testChorusToneStepFallbackGuard();
    testChorusBypassStateAndWetMuteTiming();
    testChorusRateChangePreservesPhysicalState();
    testBucketBrigadeDatasheetAnchors();
    testBbdTransferDefensiveGuards();
    testBbdTransferApproximationTracksItsReference();
    testBbdInputCubicInterpolation();
    testBbdOutputPolyBlepReferenceAndBounds();
    testBbdPolyBlepResidualDefensiveGuard();
    testBbdOutputPolyBlepSeparatesPhysicalAndNumericalAliases();
    testCombinedBbdSupportTransitionAndStateSafety();
    testSupportFilterCornersLandWhereAsked();
    testSallenKeyQNonPositiveShuntGuard();
    testHighPassReachesTheSummedSignal();
    testHighPassStateGuardSelfHeals();
    testNoiseSourceShapingFollowsItsCircuit();
    testNoiseLevelControlPrecedesC41();
    testNoiseSourceLowRateSafetyAndStateSemantics();
    testCorrectionResidualsVanishAtTheEdges();
    
    // SOTA physical modeling tests
    {
        YouKnow106TestAccess::Cascade cascade;
        cascade.reset();
        cascade.offsetVoltage = { 0.0020f, -0.0015f, 0.0018f, -0.0010f };

        float positiveSum = 0.0f;
        float negativeSum = 0.0f;
        for (int i = 0; i < 1000; ++i)
        {
            float in = 2.0f * std::sin(2.0f * 3.141592653589793f * static_cast<float>(i) / 100.0f);
            float out = cascade.process(in, 0.5f, 2.0f);
            if (!std::isfinite(out))
            {
                std::cerr << "FAIL: OtaCascade output not finite with stage offsets\n";
                ++failures;
                break;
            }
            if (out > 0.0f) positiveSum += out;
            else negativeSum += std::abs(out);
        }
        if (std::abs(positiveSum - negativeSum) <= 1.0e-4f)
        {
            std::cerr << "FAIL: OtaCascade stage offsets did not produce asymmetric response\n";
            ++failures;
        }

        // Test Op-Amp Slew-Rate Limiting
        {
            EngineParameters params;
            params.enableOpAmpSlewLimiting = true;
            YouKnow106Engine engine;
            engine.prepare(44100.0, 256, true);
            engine.setParameters(params);

            std::vector<float> left(256, 0.0f);
            std::vector<float> right(256, 0.0f);
            engine.noteOn(72, 1.0f);
            engine.process(left.data(), right.data(), 256);

            for (int i = 1; i < 256; ++i)
            {
                if (!std::isfinite(left[i]) || !std::isfinite(right[i]))
                {
                    std::cerr << "FAIL: Op-Amp slew limiting output not finite\n";
                    ++failures;
                    break;
                }
            }
        }

    }

    if (failures != 0)
    {
        std::cerr << failures << " YouKnow106 circuit check(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All YouKnow106 circuit checks passed.\n";
    return EXIT_SUCCESS;
}
