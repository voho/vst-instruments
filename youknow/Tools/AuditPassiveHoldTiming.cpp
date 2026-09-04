// Fractional passive-hold timing qualification.
//
// The production scheduler and hold helpers are observed through the engine's
// executable-local friend seam, but neither is used as the reference.  This
// file independently advances the physical one-pole and cascaded two-pole
// equations in long double, then checks the states reached through the actual
// Engine::process wiring.  Explicit early, late, legacy-sequential PWM and
// disconnected-path candidates are retained as sensitivity controls.

#include "DSP/YouKnowEngine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace youknow
{
enum class PassiveHoldAuditPwmMode : std::uint8_t
{
    Manual,
    Lfo,
    PulseOff
};

// Keep the seam narrow: it translates private production coordinates into
// plain value objects and drives one real process block.  All equations used
// to decide correctness live below, outside this friend.
struct YouKnowTestAccess
{
    enum class Destination : std::uint8_t
    {
        Resonance,
        CommonVca,
        Sub,
        Pitch,
        Pwm,
        Vcf,
        VoiceVca,
        Noise
    };

    struct Write
    {
        Destination destination { Destination::Noise };
        int voice { -1 };
    };

    struct ProcessingRate
    {
        int factor {};
        double internalRate {};
    };

    struct Coordinates
    {
        std::array<double, 6> voiceVca {};
        double commonVca {};
        double sub {};
        double pwmFirst {};
        double pwmSecond {};
    };

    struct Targets
    {
        std::array<float, 6> voiceVca {};
        float commonVca {};
        float sub {};
        float pwm {};
    };

    struct Consumers
    {
        std::array<float, 6> voiceVcaGain {};
        std::array<float, 6> pulseDuty {};
    };

    struct Latch
    {
        bool valid {};
        bool nextPass {};
        std::size_t ordinal {};
        Write write {};
        float target {};
        double eventPosition {};
    };

    struct CommonConsumerProbe
    {
        double maximumRelativeError {};
        double disconnectedRelativeError {};
        std::size_t comparedSamples {};
    };

    struct SubConsumerProbe
    {
        double connectedRelativeError {};
        double disconnectedRelativeError {};
    };

    static constexpr std::size_t writeCount() noexcept
    {
        return YouKnowEngine::converterWritesPerPass;
    }

    static constexpr int physicalVoiceCount() noexcept
    {
        return YouKnowEngine::hardwareVoices;
    }

    static std::array<double, YouKnowEngine::converterWritesPerPass>
    eventPhases() noexcept
    {
        return YouKnowEngine::converterEventPhases(
            YouKnowEngine::ConverterTimingProfile::NormalizedServiceChart);
    }

    static Destination translate(
        YouKnowEngine::ConverterDestination destination) noexcept
    {
        switch (destination)
        {
            case YouKnowEngine::ConverterDestination::Resonance:
                return Destination::Resonance;
            case YouKnowEngine::ConverterDestination::CommonVca:
                return Destination::CommonVca;
            case YouKnowEngine::ConverterDestination::Sub:
                return Destination::Sub;
            case YouKnowEngine::ConverterDestination::Pitch:
                return Destination::Pitch;
            case YouKnowEngine::ConverterDestination::Pwm:
                return Destination::Pwm;
            case YouKnowEngine::ConverterDestination::Vcf:
                return Destination::Vcf;
            case YouKnowEngine::ConverterDestination::VoiceVca:
                return Destination::VoiceVca;
            case YouKnowEngine::ConverterDestination::Noise:
                return Destination::Noise;
        }
        return Destination::Noise;
    }

    static std::array<Write, YouKnowEngine::converterWritesPerPass>
    writes() noexcept
    {
        std::array<Write, YouKnowEngine::converterWritesPerPass> result {};
        const auto& source = YouKnowEngine::converterWriteOrder();
        for (std::size_t index = 0; index < source.size(); ++index)
            result[index] = { translate(source[index].destination),
                              source[index].voice };
        return result;
    }

    static bool isPassive(std::size_t ordinal) noexcept
    {
        return YouKnowEngine::isPassiveHoldWrite(
            YouKnowEngine::converterWriteOrder()[ordinal]);
    }

    static ProcessingRate processingRate(double hostRate, bool hq)
    {
        YouKnowEngine engine;
        engine.prepare(hostRate, 1, hq);
        return { engine.oversampling_, engine.oversampledRate_ };
    }

    static constexpr double scanRate() noexcept
    {
        return YouKnowEngine::controlScanHz;
    }

    static constexpr double voiceVcaSeconds() noexcept
    {
        return YouKnowEngine::voiceVcaHoldSlewSeconds;
    }

    static double commonVcaSeconds() noexcept
    {
        return YouKnowEngine::commonVcaHoldTimeConstantSeconds();
    }

    static constexpr double subSeconds() noexcept
    {
        return YouKnowEngine::subHoldSlewSeconds;
    }

    static constexpr double pwmFirstSeconds() noexcept
    {
        return YouKnowEngine::pwmHoldFirstPoleSeconds;
    }

    static constexpr double pwmSecondSeconds() noexcept
    {
        return YouKnowEngine::pwmHoldSecondPoleSeconds;
    }

    static double productionOnePole(double state, float target, bool hasEvent,
                                    double position, float eventTarget,
                                    double seconds, double timeConstant) noexcept
    {
        const double fullDecay = std::exp(-seconds / timeConstant);
        return YouKnowEngine::exactOnePoleHoldEndpoint(
            state, target, hasEvent, position, eventTarget, seconds,
            timeConstant, fullDecay);
    }

    static std::array<double, 2> productionPwm(
        std::array<double, 2> state, float target, bool hasEvent,
        double position, float eventTarget, double seconds) noexcept
    {
        YouKnowEngine::PwmHoldState production { state[0], state[1] };
        production = YouKnowEngine::exactPwmHoldEndpoint(
            production, target, hasEvent, position, eventTarget, seconds,
            YouKnowEngine::pwmHoldCoefficients(seconds));
        return { production.first, production.second };
    }

    static EngineParameters parametersFor(
        PassiveHoldAuditPwmMode pwmMode,
        VcaMode vcaMode = VcaMode::Envelope)
    {
        EngineParameters parameters;
        parameters.calibration = 0.0f;
        parameters.sawEnabled = false;
        parameters.pulseEnabled = pwmMode != PassiveHoldAuditPwmMode::PulseOff;
        parameters.subLevel = 0.93f;
        parameters.noiseLevel = 0.0f;
        parameters.cutoff = 0.18f;
        parameters.resonance = 0.31f;
        parameters.envDepth = 0.0f;
        parameters.vcfLfoDepth = 0.0f;
        parameters.keyFollow = 0.0f;
        parameters.vcaMode = vcaMode;
        parameters.vcaLevel = 0.89f;
        parameters.pwmDepth = 0.91f;
        parameters.pwmSource = pwmMode == PassiveHoldAuditPwmMode::Lfo
            ? PwmSource::Lfo : PwmSource::Manual;
        parameters.chorus = ChorusMode::Off;
        parameters.chorusNoise = 0.0f;
        parameters.volume = 0.0f;
        parameters.polyphony = 6;
        return parameters;
    }

    static YouKnowEngine fixture(
        double hostRate, bool hq, PassiveHoldAuditPwmMode pwmMode,
        VcaMode vcaMode = VcaMode::Envelope)
    {
        YouKnowEngine engine;
        engine.prepare(hostRate, 1, hq);
        engine.activeParameters_ = parametersFor(pwmMode, vcaMode);
        engine.targetParameters_ = engine.activeParameters_;
        engine.refreshVoiceRampCurrentScales();
        engine.converterPassLfoGated_ = -0.37f;
        engine.lfoAccumulator_ = 0x0bd7u;
        engine.lfoPolarity_ = -1.0f;
        stagePwmPassCode(engine, engine.activeParameters_);
        engine.passiveHoldEventLatch_ = {};
        engine.assignmentRescanPending_ = false;
        engine.assignmentRescanPassArmed_ = false;

        engine.sharedVca_ = 0.74;
        engine.sharedVcaTarget_ = 0.17f;
        engine.subCv_ = 0.68;
        engine.subCvTarget_ = 0.23f;
        engine.pwmVoltsFirstPole_ = 5.15;
        engine.pwmVolts_ = 2.65;
        engine.pwmVoltsTarget_ = 4.85f;
        engine.resonanceCv_ = engine.resonanceCvTarget_ = 0.2f;
        engine.noiseCv_ = engine.noiseCvTarget_ = 0.0f;
        engine.driftControlCountdown_ = std::numeric_limits<int>::max();

        for (int voiceIndex = 0; voiceIndex < YouKnowEngine::maxVoices;
             ++voiceIndex)
        {
            auto& voice = engine.voices_[static_cast<std::size_t>(voiceIndex)];
            voice.active = false;
            voice.cardIndex = voiceIndex < YouKnowEngine::hardwareVoices
                ? voiceIndex : 0;
            voice.vcaControl = 0.84 - 0.071 * voiceIndex;
            voice.vcaControlTarget = static_cast<float>(
                0.11 + 0.027 * voiceIndex);
            voice.envelope.value = static_cast<float>(
                std::clamp(0.91 - 0.083 * voiceIndex, 0.08, 0.91));
            voice.velocity = 1.0f;
            voice.keyDown = vcaMode == VcaMode::Gate
                && voiceIndex % 2 == 0;
            voice.sustained = false;
            voice.dco.periodSamples = 1.0e12;
            voice.dco.renderScale = 1.0f;
            voice.dcoCv = voice.dcoCvTarget = 261.6f;
            voice.cutoffCounts = voice.cutoffCountsTarget = 1000.0f;
        }
        return engine;
    }

    static Coordinates coordinates(const YouKnowEngine& engine) noexcept
    {
        Coordinates result;
        for (std::size_t voice = 0; voice < result.voiceVca.size(); ++voice)
            result.voiceVca[voice] = engine.voices_[voice].vcaControl;
        result.commonVca = engine.sharedVca_;
        result.sub = engine.subCv_;
        result.pwmFirst = engine.pwmVoltsFirstPole_;
        result.pwmSecond = engine.pwmVolts_;
        return result;
    }

    static Targets targets(const YouKnowEngine& engine) noexcept
    {
        Targets result;
        for (std::size_t voice = 0; voice < result.voiceVca.size(); ++voice)
            result.voiceVca[voice] = engine.voices_[voice].vcaControlTarget;
        result.commonVca = engine.sharedVcaTarget_;
        result.sub = engine.subCvTarget_;
        result.pwm = engine.pwmVoltsTarget_;
        return result;
    }

    static Consumers consumers(const YouKnowEngine& engine) noexcept
    {
        Consumers result;
        for (std::size_t voice = 0; voice < result.voiceVcaGain.size(); ++voice)
        {
            result.voiceVcaGain[voice] = engine.voices_[voice].vca;
            result.pulseDuty[voice] = engine.voices_[voice].pulseDuty;
        }
        return result;
    }

    static Consumers expectedConsumers(const Coordinates& coordinates) noexcept
    {
        Consumers result;
        for (std::size_t voice = 0; voice < result.voiceVcaGain.size(); ++voice)
        {
            result.voiceVcaGain[voice] =
                YouKnowEngine::VoiceVcaControlLaw::gain(
                    static_cast<float>(coordinates.voiceVca[voice]));
            result.pulseDuty[voice] = YouKnowEngine::pwmDutyCycle(
                static_cast<float>(coordinates.pwmSecond), 1.0f);
        }
        return result;
    }

    static float eventTarget(const YouKnowEngine& engine,
                             std::size_t ordinal,
                             const EngineParameters& parameters,
                             float lfoGated) noexcept
    {
        return engine.passiveHoldWriteTarget(
            YouKnowEngine::converterWriteOrder()[ordinal], parameters,
            lfoGated);
    }

    static float heldTarget(const YouKnowEngine& engine,
                            std::size_t ordinal) noexcept
    {
        const auto& write = YouKnowEngine::converterWriteOrder()[ordinal];
        switch (write.destination)
        {
            case YouKnowEngine::ConverterDestination::Resonance:
                return engine.resonanceCvTarget_;
            case YouKnowEngine::ConverterDestination::CommonVca:
                return engine.sharedVcaTarget_;
            case YouKnowEngine::ConverterDestination::Sub:
                return engine.subCvTarget_;
            case YouKnowEngine::ConverterDestination::Pwm:
                return engine.pwmVoltsTarget_;
            case YouKnowEngine::ConverterDestination::Vcf:
                return write.voice >= 0
                    ? engine.voices_[static_cast<std::size_t>(write.voice)]
                          .cutoffCountsTarget
                    : 0.0f;
            case YouKnowEngine::ConverterDestination::VoiceVca:
                return write.voice >= 0
                    ? engine.voices_[static_cast<std::size_t>(write.voice)]
                          .vcaControlTarget
                    : 0.0f;
            case YouKnowEngine::ConverterDestination::Pitch:
            case YouKnowEngine::ConverterDestination::Noise:
                return 0.0f;
        }
        return 0.0f;
    }

    static void stagePwmPassCode(YouKnowEngine& engine,
                                 const EngineParameters& parameters) noexcept
    {
        engine.converterPassPwmDacCode_ = parameters.pulseEnabled
            ? YouKnowEngine::pwmDacCode(
                  parameters.pwmDepth, parameters.pwmSource,
                  engine.lfoAccumulator_, engine.lfoPolarity_ >= 0.0f)
            : 0u;
    }

    static void mutateStagedPwmPayloadAfterPeek(
        YouKnowEngine& engine) noexcept
    {
        // A panel edit cannot rewrite FF4F in the middle of a B-2 pass. Move
        // the staged word explicitly to model the next pass and prove that the
        // fractional-event latch still commits the payload it already peeked.
        engine.converterPassPwmDacCode_ =
            engine.converterPassPwmDacCode_ == 0u ? 0x0fffu : 0u;
    }

    static void setCursor(YouKnowEngine& engine, std::size_t ordinal,
                          double requestedPosition,
                          int internalSubstep = 0) noexcept
    {
        const double delta = YouKnowEngine::controlScanHz
                           / engine.oversampledRate_;
        const double event = engine.converterEventPhases_[ordinal];
        engine.controlScanPhase_ = event
            - (static_cast<double>(internalSubstep) + requestedPosition)
                * delta;
        engine.nextConverterWrite_ = ordinal;
        engine.passiveHoldEventLatch_ = {};
    }

    static double geometricEventPosition(const YouKnowEngine& engine,
                                         std::size_t ordinal,
                                         int internalSubstep) noexcept
    {
        const double delta = YouKnowEngine::controlScanHz
                           / engine.oversampledRate_;
        const double intervalStart = engine.controlScanPhase_
                                   + internalSubstep * delta;
        return std::clamp(
            (engine.converterEventPhases_[ordinal] - intervalStart) / delta,
            0.0, 1.0);
    }

    static void processOne(YouKnowEngine& engine)
    {
        float left = 0.0f;
        float right = 0.0f;
        engine.process(&left, &right, 1);
        if (!std::isfinite(left) || !std::isfinite(right))
            throw std::runtime_error("passive-hold fixture produced non-finite audio");
    }

    static Latch latch(const YouKnowEngine& engine) noexcept
    {
        const auto& source = engine.passiveHoldEventLatch_;
        return { source.valid, source.nextPass, source.ordinal,
                 { translate(source.write.destination), source.write.voice },
                 source.target, source.eventPosition };
    }

    static bool peek(YouKnowEngine& engine, double phase, double delta,
                     const EngineParameters& parameters) noexcept
    {
        return engine.latchUpcomingPassiveHoldEvent(
            phase, delta, parameters);
    }

    static void setSchedulerCursor(YouKnowEngine& engine,
                                   std::size_t ordinal) noexcept
    {
        engine.nextConverterWrite_ = ordinal;
        engine.passiveHoldEventLatch_ = {};
    }

    static std::size_t schedulerCursor(const YouKnowEngine& engine) noexcept
    {
        return engine.nextConverterWrite_;
    }

    static void setAutomationAfterPeek(YouKnowEngine& engine,
                                       EngineParameters& parameters,
                                       float& lfoGated) noexcept
    {
        parameters.resonance = 0.94f;
        parameters.vcaLevel = 0.07f;
        parameters.subLevel = 0.04f;
        parameters.pulseEnabled = false;
        parameters.pwmDepth = 0.02f;
        parameters.cutoff = 0.84f;
        parameters.envDepth = 0.73f;
        parameters.keyFollow = 0.81f;
        lfoGated = 0.79f;
        for (int voice = 0; voice < YouKnowEngine::hardwareVoices; ++voice)
        {
            auto& item = engine.voices_[static_cast<std::size_t>(voice)];
            item.envelope.value = 0.03f + 0.02f * voice;
            item.currentMidi = 91.0f - 3.0f * voice;
            item.keyDown = !item.keyDown;
        }
        engine.activeParameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.converterPassLfoGated_ = lfoGated;
    }

    static void commitOverride(YouKnowEngine& engine,
                               std::size_t ordinal,
                               const EngineParameters& parameters,
                               float lfoGated, float target) noexcept
    {
        engine.performConverterWrite(
            YouKnowEngine::converterWriteOrder()[ordinal], parameters,
            lfoGated, &target);
    }

    static constexpr bool scalarLatchIsAllocationFree() noexcept
    {
        return std::is_trivially_copyable_v<
                   YouKnowEngine::PassiveHoldEventLatch>
            && std::is_trivially_destructible_v<
                   YouKnowEngine::PassiveHoldEventLatch>
            && sizeof(YouKnowEngine::PassiveHoldEventLatch) <= 64u;
    }

    static CommonConsumerProbe commonConsumerProbe()
    {
        constexpr int frames = 512;
        YouKnowEngine base;
        base.prepare(48000.0, frames, false);
        EngineParameters parameters;
        parameters.calibration = 0.0f;
        parameters.sawEnabled = true;
        parameters.pulseEnabled = false;
        parameters.subLevel = 0.0f;
        parameters.noiseLevel = 0.0f;
        parameters.cutoff = 1.0f;
        parameters.resonance = 0.0f;
        parameters.envDepth = 0.0f;
        parameters.keyFollow = 0.0f;
        parameters.vcaMode = VcaMode::Gate;
        parameters.vcaLevel = 0.8f;
        parameters.highPass = HighPassMode::One;
        parameters.chorus = ChorusMode::Off;
        parameters.chorusNoise = 0.0f;
        parameters.volume = 0.12f;
        parameters.enableOpAmpSlewLimiting = false;
        base.setParameters(parameters);
        base.noteOn(60, 1.0f);
        std::array<float, frames> preLeft {};
        std::array<float, frames> preRight {};
        for (int pass = 0; pass < 8; ++pass)
            base.process(preLeft.data(), preRight.data(), frames);

        YouKnowEngine low = base;
        YouKnowEngine high = base;
        constexpr float lowControl = 0.72f;
        constexpr float highControl = 0.94f;
        for (auto* engine : { &low, &high })
        {
            engine->controlScanPhase_ = -100.0;
            engine->nextConverterWrite_ =
                YouKnowEngine::converterWritesPerPass;
            engine->passiveHoldEventLatch_ = {};
            engine->latencyPadLeft_.fill(0.0f);
            engine->latencyPadRight_.fill(0.0f);
            engine->latencyPadWriteIndex_ = 0;
            engine->outputCouplingLeft_.reset();
            engine->outputCouplingRight_.reset();
            engine->outputSlewStateLeft_ = 0.0f;
            engine->outputSlewStateRight_ = 0.0f;
            engine->glidedVolume_ = parameters.volume;
            engine->panelGlidePrimed_ = true;
        }
        low.sharedVca_ = lowControl;
        low.sharedVcaTarget_ = lowControl;
        high.sharedVca_ = highControl;
        high.sharedVcaTarget_ = highControl;

        std::array<float, frames> lowLeft {};
        std::array<float, frames> lowRight {};
        std::array<float, frames> highLeft {};
        std::array<float, frames> highRight {};
        low.process(lowLeft.data(), lowRight.data(), frames);
        high.process(highLeft.data(), highRight.data(), frames);

        const double ratio = static_cast<double>(
                                 YouKnowEngine::patchLevelGain(highControl))
                           / static_cast<double>(
                                 YouKnowEngine::patchLevelGain(lowControl));
        CommonConsumerProbe result;
        result.disconnectedRelativeError = std::abs(ratio - 1.0);
        const std::size_t first = static_cast<std::size_t>(
            std::max(low.latencyPadSamples_, high.latencyPadSamples_));
        for (std::size_t sample = first; sample < lowLeft.size(); ++sample)
        {
            const double expected = static_cast<double>(lowLeft[sample]) * ratio;
            const double actual = highLeft[sample];
            if (std::abs(expected) < 1.0e-7)
                continue;
            result.maximumRelativeError = std::max(
                result.maximumRelativeError,
                std::abs(actual - expected) / std::abs(expected));
            ++result.comparedSamples;
        }
        return result;
    }

    static float recoveredSubMixerInput(double subControl)
    {
        auto engine = fixture(
            192000.0, false, PassiveHoldAuditPwmMode::PulseOff);
        auto parameters = engine.activeParameters_;
        parameters.sawEnabled = false;
        parameters.pulseEnabled = false;
        parameters.enablePulseOffWaveNodeCoupling = false;
        parameters.noiseLevel = 0.0f;
        parameters.calibration = 0.0f;
        engine.activeParameters_ = parameters;
        engine.targetParameters_ = parameters;
        engine.subCv_ = subControl;
        engine.subCvTarget_ = static_cast<float>(subControl);
        auto& voice = engine.voices_[0];
        voice.cardIndex = 0;
        voice.dco.reset();
        voice.dco.periodSamples = 1.0e12;
        voice.dco.renderScale = 1.0f;
        voice.moduleCoupling.reset();
        voice.inputCompensation = 1.0f;
        const double before = voice.moduleCoupling.state;
        (void) engine.renderVoice(voice, parameters, 0.0f);
        const double after = voice.moduleCoupling.state;
        const double g = static_cast<double>(engine.moduleCouplingG_);
        return static_cast<float>(
            before + (after - before) * (1.0 + g) / (2.0 * g));
    }

    static SubConsumerProbe subConsumerProbe()
    {
        constexpr double lowControl = 0.25;
        constexpr double highControl = 0.75;
        const double low = recoveredSubMixerInput(lowControl);
        const double high = recoveredSubMixerInput(highControl);
        const double expected = low * (highControl / lowControl);
        const double scale = std::max(std::abs(expected), 1.0e-12);
        return {
            std::abs(high - expected) / scale,
            std::abs(high - low) / scale
        };
    }
};
} // namespace youknow

namespace
{
using Access = youknow::YouKnowTestAccess;
using Destination = Access::Destination;
using Coordinates = Access::Coordinates;
using Targets = Access::Targets;
using PwmMode = youknow::PassiveHoldAuditPwmMode;
using youknow::EngineParameters;

constexpr double stateGate = 2.0e-10;
constexpr std::uint64_t floatUlpGate = 2u;
constexpr double mutationRejectionGate = 1.0e-8;
constexpr double justBelowOne =
    std::bit_cast<double>(std::uint64_t { 0x3fefffffffffffffULL });
// Literal nextafter(1,0) is covered at the helper boundary above. Subtracting
// its one-ULP suffix from an ordinary converter phase cannot survive that
// phase's coarser representation, so process wiring uses an explicitly
// representable near-endpoint instead of pretending the literal survived.
constexpr double resolvableNearOne = 1.0 - 1.0e-8;

struct Cell
{
    const char* name;
    double hostRate;
    bool hq;
    int factor;
};

constexpr std::array cells {
    Cell { "8k-hq", 8000.0, true, 4 },
    Cell { "8k-hq-off-endpoint", 8000.0, false, 1 },
    Cell { "44.1k-hq", 44100.0, true, 4 },
    Cell { "48k-hq", 48000.0, true, 4 },
    Cell { "88.2k-hq", 88200.0, true, 2 },
    Cell { "96k-hq", 96000.0, true, 2 },
    Cell { "176.4k-hq", 176400.0, true, 1 },
    Cell { "192k-hq", 192000.0, true, 1 },
    Cell { "44.1k-hq-off", 44100.0, false, 1 },
    Cell { "48k-hq-off", 48000.0, false, 1 },
    Cell { "88.2k-hq-off", 88200.0, false, 1 },
    Cell { "96k-hq-off", 96000.0, false, 1 },
    Cell { "768k-hq", 768000.0, true, 1 },
};

constexpr std::array helperPositions {
    0.0,
    std::numeric_limits<double>::denorm_min(),
    0.125,
    0.5,
    justBelowOne,
    1.0
};

constexpr std::array processPositions {
    0.0, 0.125, 0.5, resolvableNearOne, 1.0
};

constexpr std::array newPassiveOrdinals {
    std::size_t { 1 }, std::size_t { 2 }, std::size_t { 9 },
    std::size_t { 11 }, std::size_t { 13 }, std::size_t { 15 },
    std::size_t { 17 }, std::size_t { 19 }, std::size_t { 21 }
};

constexpr std::array pwmModes {
    PwmMode::Manual, PwmMode::Lfo, PwmMode::PulseOff
};

const char* destinationName(Destination destination) noexcept
{
    switch (destination)
    {
        case Destination::Resonance: return "resonance";
        case Destination::CommonVca: return "common-vca";
        case Destination::Sub: return "sub";
        case Destination::Pitch: return "pitch";
        case Destination::Pwm: return "pwm";
        case Destination::Vcf: return "vcf";
        case Destination::VoiceVca: return "voice-vca";
        case Destination::Noise: return "noise";
    }
    return "unknown";
}

bool expectedPassive(const Access::Write& write) noexcept
{
    switch (write.destination)
    {
        case Destination::Resonance:
        case Destination::CommonVca:
        case Destination::Sub:
        case Destination::Pwm:
            return true;
        case Destination::Vcf:
        case Destination::VoiceVca:
            return write.voice >= 0
                && write.voice < Access::physicalVoiceCount();
        case Destination::Pitch:
        case Destination::Noise:
            return false;
    }
    return false;
}

long double onePoleOracle(long double state, long double target,
                          bool hasEvent, long double eventPosition,
                          long double eventTarget, long double seconds,
                          long double timeConstant) noexcept
{
    const long double event = std::clamp(eventPosition, 0.0L, 1.0L);
    const long double prefix = hasEvent ? event : 1.0L;
    state = target + (state - target)
        * std::exp(-prefix * seconds / timeConstant);
    if (hasEvent)
        state = eventTarget + (state - eventTarget)
            * std::exp(-(1.0L - event) * seconds / timeConstant);
    return state;
}

std::array<long double, 2> pwmConstantOracle(
    std::array<long double, 2> state, long double target,
    long double seconds) noexcept
{
    const long double firstTime = Access::pwmFirstSeconds();
    const long double secondTime = Access::pwmSecondSeconds();
    const long double firstDecay = std::exp(-seconds / firstTime);
    const long double secondDecay = std::exp(-seconds / secondTime);
    const long double cross = firstTime / (firstTime - secondTime)
                            * (firstDecay - secondDecay);
    const long double initialFirst = state[0];
    state[0] = firstDecay * initialFirst
             + (1.0L - firstDecay) * target;
    state[1] = cross * initialFirst + secondDecay * state[1]
             + (1.0L - secondDecay - cross) * target;
    return state;
}

std::array<long double, 2> pwmOracle(
    std::array<long double, 2> state, long double target, bool hasEvent,
    long double eventPosition, long double eventTarget,
    long double seconds) noexcept
{
    if (!hasEvent)
        return pwmConstantOracle(state, target, seconds);
    const long double event = std::clamp(eventPosition, 0.0L, 1.0L);
    state = pwmConstantOracle(state, target, event * seconds);
    return pwmConstantOracle(
        state, eventTarget, (1.0L - event) * seconds);
}

std::array<long double, 2> sequentialPwmMutation(
    std::array<long double, 2> state, long double target,
    long double seconds) noexcept
{
    const long double firstGain = -std::expm1(
        -seconds / static_cast<long double>(Access::pwmFirstSeconds()));
    const long double secondGain = -std::expm1(
        -seconds / static_cast<long double>(Access::pwmSecondSeconds()));
    state[0] += (target - state[0]) * firstGain;
    // Rejected legacy ordering: the second pole sees the first pole's endpoint
    // as though it had been constant for the whole segment.
    state[1] += (state[0] - state[1]) * secondGain;
    return state;
}

std::uint64_t ulpDistance(float first, float second) noexcept
{
    if (std::isnan(first) || std::isnan(second))
        return std::numeric_limits<std::uint64_t>::max();
    const auto ordered = [](float value) {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
        return (bits & 0x80000000u) != 0u ? ~bits : bits | 0x80000000u;
    };
    const std::uint32_t a = ordered(first);
    const std::uint32_t b = ordered(second);
    return a > b ? static_cast<std::uint64_t>(a - b)
                 : static_cast<std::uint64_t>(b - a);
}

struct Metrics
{
    std::size_t helperSettledCases {};
    std::size_t helperUnsettledCases {};
    std::size_t helperAlternatingCases {};
    std::size_t processCases {};
    std::size_t blockWrapCases {};
    std::size_t schedulerOrdinals {};
    std::size_t schedulerPassiveOrdinals {};
    std::size_t schedulerPitchPeeks {};
    std::size_t schedulerNoisePeeks {};
    std::size_t schedulerDuplicatePeeks {};
    std::size_t schedulerPayloadFailures {};
    std::size_t schedulerClassificationFailures {};
    std::size_t schedulerCursorFailures {};
    std::size_t schedulerOrderFailures {};
    std::size_t schedulerPassWrapFailures {};
    std::size_t processingRateFailures {};
    std::size_t nonFiniteCases {};
    double maximumHelperError {};
    double maximumProcessError {};
    std::uint64_t maximumStateFloatUlps {};
    std::uint64_t maximumConsumerUlps {};
    double maximumLateMutationError {};
    double maximumEarlyMutationError {};
    double maximumSequentialPwmMutationError {};
    double maximumDisconnectedMutationError {};
    double maximumProcessGeometryError {};
    std::size_t collapsedNearEndpointCases {};
    std::array<double, 4> lateMutationByDestination {};
    std::array<double, 4> earlyMutationByDestination {};
    std::array<double, 4> disconnectedMutationByDestination {};
    double commonConsumerMaximumRelativeError {};
    double commonConsumerDisconnectedRelativeError {};
    std::size_t commonConsumerSamples {};
    double subConsumerRelativeError {};
    double subConsumerDisconnectedRelativeError {};
    std::string worstHelper;
    std::string worstProcess;
};

void runConsumerWiringAudit(Metrics& metrics)
{
    const auto common = Access::commonConsumerProbe();
    metrics.commonConsumerMaximumRelativeError =
        common.maximumRelativeError;
    metrics.commonConsumerDisconnectedRelativeError =
        common.disconnectedRelativeError;
    metrics.commonConsumerSamples = common.comparedSamples;
    const auto sub = Access::subConsumerProbe();
    metrics.subConsumerRelativeError = sub.connectedRelativeError;
    metrics.subConsumerDisconnectedRelativeError =
        sub.disconnectedRelativeError;
}

std::size_t mutationDestinationIndex(Destination destination)
{
    switch (destination)
    {
        case Destination::CommonVca: return 0u;
        case Destination::Sub: return 1u;
        case Destination::Pwm: return 2u;
        case Destination::VoiceVca: return 3u;
        case Destination::Resonance:
        case Destination::Pitch:
        case Destination::Vcf:
        case Destination::Noise:
            break;
    }
    throw std::logic_error("non-new destination entered mutation matrix");
}

void recordError(double error, double& maximum, std::string& worst,
                 std::string_view label)
{
    if (error > maximum)
    {
        maximum = error;
        worst = std::string(label);
    }
}

double coordinateError(const Coordinates& candidate,
                       const Coordinates& reference) noexcept
{
    double result = 0.0;
    for (std::size_t voice = 0; voice < candidate.voiceVca.size(); ++voice)
        result = std::max(result, std::abs(
            candidate.voiceVca[voice] - reference.voiceVca[voice]));
    result = std::max(result,
                      std::abs(candidate.commonVca - reference.commonVca));
    result = std::max(result, std::abs(candidate.sub - reference.sub));
    // PWM's physical target range is -0.8..6 V. Report its error as a
    // fraction of that 6.8 V span, so all coordinates share one gate.
    result = std::max(result,
        std::abs(candidate.pwmFirst - reference.pwmFirst) / 6.8);
    result = std::max(result,
        std::abs(candidate.pwmSecond - reference.pwmSecond) / 6.8);
    return result;
}

std::uint64_t stateFloatUlps(const Coordinates& candidate,
                             const Coordinates& reference) noexcept
{
    std::uint64_t result = 0u;
    for (std::size_t voice = 0; voice < candidate.voiceVca.size(); ++voice)
        result = std::max(result, ulpDistance(
            static_cast<float>(candidate.voiceVca[voice]),
            static_cast<float>(reference.voiceVca[voice])));
    result = std::max(result, ulpDistance(
        static_cast<float>(candidate.commonVca),
        static_cast<float>(reference.commonVca)));
    result = std::max(result, ulpDistance(
        static_cast<float>(candidate.sub),
        static_cast<float>(reference.sub)));
    result = std::max(result, ulpDistance(
        static_cast<float>(candidate.pwmFirst),
        static_cast<float>(reference.pwmFirst)));
    result = std::max(result, ulpDistance(
        static_cast<float>(candidate.pwmSecond),
        static_cast<float>(reference.pwmSecond)));
    return result;
}

std::uint64_t consumerUlps(const Access::Consumers& candidate,
                           const Access::Consumers& reference) noexcept
{
    std::uint64_t result = 0u;
    for (std::size_t voice = 0; voice < candidate.voiceVcaGain.size(); ++voice)
    {
        result = std::max(result, ulpDistance(
            candidate.voiceVcaGain[voice], reference.voiceVcaGain[voice]));
        result = std::max(result, ulpDistance(
            candidate.pulseDuty[voice], reference.pulseDuty[voice]));
    }
    return result;
}

void changeTarget(Targets& targets, const Access::Write& write,
                  float target) noexcept
{
    switch (write.destination)
    {
        case Destination::CommonVca:
            targets.commonVca = target;
            break;
        case Destination::Sub:
            targets.sub = target;
            break;
        case Destination::Pwm:
            targets.pwm = target;
            break;
        case Destination::VoiceVca:
            if (write.voice >= 0 && write.voice < 6)
                targets.voiceVca[static_cast<std::size_t>(write.voice)] = target;
            break;
        case Destination::Resonance:
        case Destination::Pitch:
        case Destination::Vcf:
        case Destination::Noise:
            break;
    }
}

Coordinates advanceCoordinates(Coordinates state, const Targets& targets,
                               double seconds, const Access::Write* event,
                               double eventPosition, float eventTarget,
                               bool sequentialPwm = false) noexcept
{
    for (std::size_t voice = 0; voice < state.voiceVca.size(); ++voice)
    {
        const bool active = event != nullptr
            && event->destination == Destination::VoiceVca
            && event->voice == static_cast<int>(voice);
        state.voiceVca[voice] = static_cast<double>(onePoleOracle(
            state.voiceVca[voice], targets.voiceVca[voice], active,
            eventPosition, active ? eventTarget : targets.voiceVca[voice],
            seconds, Access::voiceVcaSeconds()));
    }
    const bool common = event != nullptr
        && event->destination == Destination::CommonVca;
    state.commonVca = static_cast<double>(onePoleOracle(
        state.commonVca, targets.commonVca, common, eventPosition,
        common ? eventTarget : targets.commonVca, seconds,
        Access::commonVcaSeconds()));
    const bool sub = event != nullptr && event->destination == Destination::Sub;
    state.sub = static_cast<double>(onePoleOracle(
        state.sub, targets.sub, sub, eventPosition,
        sub ? eventTarget : targets.sub, seconds, Access::subSeconds()));

    const bool pwm = event != nullptr && event->destination == Destination::Pwm;
    if (!sequentialPwm)
    {
        const auto result = pwmOracle(
            { state.pwmFirst, state.pwmSecond }, targets.pwm, pwm,
            eventPosition, pwm ? eventTarget : targets.pwm, seconds);
        state.pwmFirst = static_cast<double>(result[0]);
        state.pwmSecond = static_cast<double>(result[1]);
    }
    else
    {
        std::array<long double, 2> result { state.pwmFirst, state.pwmSecond };
        if (pwm)
        {
            result = sequentialPwmMutation(
                result, targets.pwm, eventPosition * seconds);
            result = sequentialPwmMutation(
                result, eventTarget, (1.0 - eventPosition) * seconds);
        }
        else
            result = sequentialPwmMutation(result, targets.pwm, seconds);
        state.pwmFirst = static_cast<double>(result[0]);
        state.pwmSecond = static_cast<double>(result[1]);
    }
    return state;
}

void runHelperOracleAudit(Metrics& metrics)
{
    constexpr std::array timeConstants {
        Access::voiceVcaSeconds(), Access::subSeconds()
    };
    for (const auto& cell : cells)
    {
        const auto rate = Access::processingRate(cell.hostRate, cell.hq);
        if (rate.factor != cell.factor)
        {
            ++metrics.processingRateFailures;
            continue;
        }
        const double seconds = 1.0 / rate.internalRate;

        const auto testOne = [&](double timeConstant, double state,
                                 float target, bool event, double position,
                                 float next, std::string_view scenario) {
            const double production = Access::productionOnePole(
                state, target, event, position, next, seconds, timeConstant);
            const double reference = static_cast<double>(onePoleOracle(
                state, target, event, position, next, seconds, timeConstant));
            const double error = std::abs(production - reference);
            recordError(error, metrics.maximumHelperError,
                        metrics.worstHelper,
                        std::string(cell.name) + "/one/" + std::string(scenario));
            if (!std::isfinite(production))
                ++metrics.nonFiniteCases;
        };

        for (const double timeConstant : timeConstants)
        {
            for (const double position : helperPositions)
            {
                testOne(timeConstant, 0.35, 0.35f, false, position, 0.35f,
                        "settled");
                ++metrics.helperSettledCases;
                testOne(timeConstant, 0.83, 0.17f, true, position, 0.91f,
                        "unsettled");
                ++metrics.helperUnsettledCases;
            }

            double production = 0.42;
            long double reference = 0.42L;
            float target = 0.12f;
            for (int interval = 0; interval < 32; ++interval)
            {
                const float next = interval % 2 == 0 ? 0.92f : 0.08f;
                const double position = helperPositions[
                    static_cast<std::size_t>(interval) % helperPositions.size()];
                production = Access::productionOnePole(
                    production, target, true, position, next, seconds,
                    timeConstant);
                reference = onePoleOracle(
                    reference, target, true, position, next, seconds,
                    timeConstant);
                target = next;
                recordError(std::abs(production - static_cast<double>(reference)),
                            metrics.maximumHelperError, metrics.worstHelper,
                            std::string(cell.name) + "/one/alternating");
                ++metrics.helperAlternatingCases;
            }
        }

        for (const double position : helperPositions)
        {
            const double settledState = 3.2;
            const float settledTarget = 3.2f;
            const auto settled = Access::productionPwm(
                { settledState, settledState }, settledTarget, false,
                position, settledTarget, seconds);
            const auto settledReference = pwmOracle(
                { static_cast<long double>(settledState),
                  static_cast<long double>(settledState) },
                static_cast<long double>(settledTarget), false, position,
                static_cast<long double>(settledTarget), seconds);
            recordError(std::max(
                            std::abs(settled[0]
                                     - static_cast<double>(settledReference[0])),
                            std::abs(settled[1]
                                     - static_cast<double>(settledReference[1])))
                            / 6.8,
                        metrics.maximumHelperError, metrics.worstHelper,
                        std::string(cell.name) + "/pwm/settled");
            ++metrics.helperSettledCases;

            const double unsettledFirst = 5.4;
            const double unsettledSecond = 1.3;
            const float unsettledTarget = 4.7f;
            const float unsettledNext = -0.8f;
            const auto unsettled = Access::productionPwm(
                { unsettledFirst, unsettledSecond }, unsettledTarget, true,
                position, unsettledNext, seconds);
            const auto unsettledReference = pwmOracle(
                { static_cast<long double>(unsettledFirst),
                  static_cast<long double>(unsettledSecond) },
                static_cast<long double>(unsettledTarget), true, position,
                static_cast<long double>(unsettledNext), seconds);
            recordError(std::max(
                            std::abs(unsettled[0]
                                     - static_cast<double>(unsettledReference[0])),
                            std::abs(unsettled[1]
                                     - static_cast<double>(unsettledReference[1])))
                            / 6.8,
                        metrics.maximumHelperError, metrics.worstHelper,
                        std::string(cell.name) + "/pwm/unsettled");
            ++metrics.helperUnsettledCases;
        }

        std::array<double, 2> production { 4.9, 2.1 };
        std::array<long double, 2> reference { 4.9L, 2.1L };
        float target = 5.8f;
        for (int interval = 0; interval < 32; ++interval)
        {
            const float next = interval % 2 == 0 ? -0.8f : 5.7f;
            const double position = helperPositions[
                static_cast<std::size_t>(interval) % helperPositions.size()];
            production = Access::productionPwm(
                production, target, true, position, next, seconds);
            reference = pwmOracle(
                reference, target, true, position, next, seconds);
            target = next;
            recordError(std::max(
                            std::abs(production[0]
                                     - static_cast<double>(reference[0])),
                            std::abs(production[1]
                                     - static_cast<double>(reference[1])))
                            / 6.8,
                        metrics.maximumHelperError, metrics.worstHelper,
                        std::string(cell.name) + "/pwm/alternating");
            ++metrics.helperAlternatingCases;
        }
    }
}

void compareProcessState(const Coordinates& actual,
                         const Coordinates& reference,
                         const Access::Consumers& consumers,
                         Metrics& metrics, std::string_view label)
{
    const double error = coordinateError(actual, reference);
    recordError(error, metrics.maximumProcessError,
                metrics.worstProcess, label);
    metrics.maximumStateFloatUlps = std::max(
        metrics.maximumStateFloatUlps, stateFloatUlps(actual, reference));
    metrics.maximumConsumerUlps = std::max(
        metrics.maximumConsumerUlps,
        consumerUlps(consumers, Access::expectedConsumers(reference)));
    if (!std::isfinite(error))
        ++metrics.nonFiniteCases;
}

void runProcessWiringAudit(Metrics& metrics)
{
    const auto writes = Access::writes();
    for (const auto& cell : cells)
    {
        const auto rate = Access::processingRate(cell.hostRate, cell.hq);
        if (rate.factor != cell.factor)
        {
            ++metrics.processingRateFailures;
            continue;
        }
        const double seconds = 1.0 / rate.internalRate;

        for (const std::size_t ordinal : newPassiveOrdinals)
        {
            const auto modes = ordinal == 9u ? pwmModes
                                             : std::array { PwmMode::Manual,
                                                            PwmMode::Manual,
                                                            PwmMode::Manual };
            const int modeCount = ordinal == 9u ? 3 : 1;
            for (int modeIndex = 0; modeIndex < modeCount; ++modeIndex)
            {
                const PwmMode mode = modes[static_cast<std::size_t>(modeIndex)];
                const int vcaModeCount =
                    writes[ordinal].destination == Destination::VoiceVca
                    ? 2 : 1;
                for (int vcaModeIndex = 0;
                     vcaModeIndex < vcaModeCount; ++vcaModeIndex)
                {
                const auto vcaMode = vcaModeIndex == 0
                    ? youknow::VcaMode::Envelope
                    : youknow::VcaMode::Gate;
                for (const double requestedPosition : processPositions)
                {
                    auto engine = Access::fixture(
                        cell.hostRate, cell.hq, mode, vcaMode);
                    const Coordinates initial = Access::coordinates(engine);
                    Targets targets = Access::targets(engine);
                    const auto parameters = Access::parametersFor(mode, vcaMode);
                    const float lfo = -0.37f;
                    const float eventTarget = Access::eventTarget(
                        engine, ordinal, parameters, lfo);
                    Access::setCursor(engine, ordinal, requestedPosition);
                    const double eventPosition = Access::geometricEventPosition(
                        engine, ordinal, 0);
                    metrics.maximumProcessGeometryError = std::max(
                        metrics.maximumProcessGeometryError,
                        std::abs(eventPosition - requestedPosition));
                    if (requestedPosition == resolvableNearOne
                        && eventPosition == 1.0)
                        ++metrics.collapsedNearEndpointCases;

                    Coordinates reference = initial;
                    Targets referenceTargets = targets;
                    Coordinates late = initial;
                    Targets lateTargets = targets;
                    Coordinates early = initial;
                    Targets earlyTargets = targets;
                    Coordinates sequential = initial;
                    Targets sequentialTargets = targets;
                    Coordinates disconnected = initial;
                    Targets disconnectedTargets = targets;

                    for (int step = 0; step < rate.factor; ++step)
                    {
                        const Access::Write* event = step == 0
                            ? &writes[ordinal] : nullptr;
                        const double position = step == 0 ? eventPosition : 1.0;
                        reference = advanceCoordinates(
                            reference, referenceTargets, seconds, event,
                            position, eventTarget);

                        // Ceil/disconnected retain the old target for the
                        // event interval and begin the new target at the next
                        // internal boundary. Floor applies it for the whole
                        // event interval.
                        late = advanceCoordinates(
                            late, lateTargets, seconds, event,
                            1.0, eventTarget);
                        // Disconnected means the helper result never reaches
                        // the shipping state: the event interval follows its
                        // old target normally, then the official target takes
                        // effect at the following boundary. Passing nullptr
                        // here is deliberately distinct from merely asking
                        // the exact helper for a different event position.
                        disconnected = advanceCoordinates(
                            disconnected, disconnectedTargets, seconds,
                            nullptr, 1.0, eventTarget);
                        early = advanceCoordinates(
                            early, earlyTargets, seconds, event,
                            0.0, eventTarget);
                        sequential = advanceCoordinates(
                            sequential, sequentialTargets, seconds, event,
                            position, eventTarget, true);

                        if (event != nullptr)
                        {
                            changeTarget(referenceTargets, *event, eventTarget);
                            changeTarget(lateTargets, *event, eventTarget);
                            changeTarget(earlyTargets, *event, eventTarget);
                            changeTarget(sequentialTargets, *event, eventTarget);
                            changeTarget(disconnectedTargets, *event, eventTarget);
                        }
                    }

                    Access::processOne(engine);
                    const Coordinates actual = Access::coordinates(engine);
                    const std::string label = std::string(cell.name) + "/"
                        + destinationName(writes[ordinal].destination)
                        + (vcaMode == youknow::VcaMode::Gate
                               ? "/gate" : "/envelope")
                        + "/p=" + std::to_string(requestedPosition);
                    compareProcessState(actual, reference,
                                        Access::consumers(engine), metrics,
                                        label);
                    ++metrics.processCases;

                    metrics.maximumLateMutationError = std::max(
                        metrics.maximumLateMutationError,
                        coordinateError(late, reference));
                    metrics.maximumEarlyMutationError = std::max(
                        metrics.maximumEarlyMutationError,
                        coordinateError(early, reference));
                    metrics.maximumSequentialPwmMutationError = std::max(
                        metrics.maximumSequentialPwmMutationError,
                        coordinateError(sequential, reference));
                    metrics.maximumDisconnectedMutationError = std::max(
                        metrics.maximumDisconnectedMutationError,
                        coordinateError(disconnected, reference));
                    const std::size_t mutationIndex =
                        mutationDestinationIndex(
                            writes[ordinal].destination);
                    metrics.lateMutationByDestination[mutationIndex] = std::max(
                        metrics.lateMutationByDestination[mutationIndex],
                        coordinateError(late, reference));
                    metrics.earlyMutationByDestination[mutationIndex] = std::max(
                        metrics.earlyMutationByDestination[mutationIndex],
                        coordinateError(early, reference));
                    metrics.disconnectedMutationByDestination[mutationIndex] =
                        std::max(
                            metrics.disconnectedMutationByDestination[
                                mutationIndex],
                            coordinateError(disconnected, reference));
                }
                }
                }
            }
    }
}

void runBlockWrapAutomationAudit(Metrics& metrics)
{
    constexpr double hostRate = 48000.0;
    constexpr bool hq = true;
    const auto rate = Access::processingRate(hostRate, hq);
    const auto writes = Access::writes();
    const double seconds = 1.0 / rate.internalRate;

    for (const std::size_t ordinal : newPassiveOrdinals)
    {
        const auto modes = ordinal == 9u ? pwmModes
                                         : std::array { PwmMode::Manual,
                                                        PwmMode::Manual,
                                                        PwmMode::Manual };
        const int modeCount = ordinal == 9u ? 3 : 1;
        for (int modeIndex = 0; modeIndex < modeCount; ++modeIndex)
        {
            const PwmMode mode = modes[static_cast<std::size_t>(modeIndex)];
            const int vcaModeCount =
                writes[ordinal].destination == Destination::VoiceVca ? 2 : 1;
            for (int vcaModeIndex = 0;
                 vcaModeIndex < vcaModeCount; ++vcaModeIndex)
            {
            const auto vcaMode = vcaModeIndex == 0
                ? youknow::VcaMode::Envelope
                : youknow::VcaMode::Gate;
            auto engine = Access::fixture(hostRate, hq, mode, vcaMode);
            Coordinates reference = Access::coordinates(engine);
            Targets targets = Access::targets(engine);
            const auto parameters = Access::parametersFor(mode, vcaMode);
            const float lfo = -0.37f;
            const float eventTarget = Access::eventTarget(
                engine, ordinal, parameters, lfo);
            Access::setCursor(engine, ordinal, 0.5, rate.factor - 1);
            const double eventPosition = Access::geometricEventPosition(
                engine, ordinal, rate.factor - 1);

            for (int step = 0; step < rate.factor; ++step)
            {
                const Access::Write* event = step == rate.factor - 1
                    ? &writes[ordinal] : nullptr;
                reference = advanceCoordinates(
                    reference, targets, seconds, event,
                    event != nullptr ? eventPosition : 1.0, eventTarget);
                if (event != nullptr)
                    changeTarget(targets, *event, eventTarget);
            }
            Access::processOne(engine);
            const auto firstLatch = Access::latch(engine);
            compareProcessState(
                Access::coordinates(engine), reference,
                Access::consumers(engine), metrics,
                std::string("block-before/")
                    + destinationName(writes[ordinal].destination));
            if (!firstLatch.valid || firstLatch.ordinal != ordinal
                || firstLatch.target != eventTarget)
                ++metrics.schedulerPayloadFailures;

            EngineParameters after = parameters;
            float afterLfo = lfo;
            Access::setAutomationAfterPeek(engine, after, afterLfo);
            if (ordinal == 9u)
                Access::mutateStagedPwmPayloadAfterPeek(engine);
            const float changed = Access::eventTarget(
                engine, ordinal, after, afterLfo);
            if (changed == eventTarget)
                ++metrics.schedulerPayloadFailures;

            for (int step = 0; step < rate.factor; ++step)
                reference = advanceCoordinates(
                    reference, targets, seconds, nullptr, 1.0, eventTarget);
            Access::processOne(engine);
            compareProcessState(
                Access::coordinates(engine), reference,
                Access::consumers(engine), metrics,
                std::string("block-after/")
                    + destinationName(writes[ordinal].destination));
            if (Access::heldTarget(engine, ordinal) != eventTarget
                || Access::latch(engine).valid)
                ++metrics.schedulerPayloadFailures;
            ++metrics.blockWrapCases;
            }
        }
    }
}

void runSchedulerAudit(Metrics& metrics)
{
    const auto writes = Access::writes();
    const auto phases = Access::eventPhases();
    constexpr double hostRate = 8000.0;
    constexpr bool hq = true;
    const auto rate = Access::processingRate(hostRate, hq);
    const double delta = Access::scanRate() / rate.internalRate;

    std::array<std::size_t, 8> destinationCounts {};
    for (std::size_t ordinal = 0; ordinal < writes.size(); ++ordinal)
    {
        ++metrics.schedulerOrdinals;
        ++destinationCounts[static_cast<std::size_t>(writes[ordinal].destination)];
        auto engine = Access::fixture(hostRate, hq, PwmMode::Manual);
        auto parameters = Access::parametersFor(PwmMode::Manual);
        const float lfoAtEvent = -0.37f;
        Access::setSchedulerCursor(engine, ordinal);
        const float targetBefore = Access::heldTarget(engine, ordinal);
        const bool expectedRelevant = expectedPassive(writes[ordinal]);
        if (Access::isPassive(ordinal) != expectedRelevant)
            ++metrics.schedulerClassificationFailures;
        const float expected = expectedRelevant
            ? Access::eventTarget(engine, ordinal, parameters, lfoAtEvent)
            : 0.0f;
        const double phase = phases[ordinal] - 0.5 * delta;
        const std::size_t cursorBefore = Access::schedulerCursor(engine);
        const bool peeked = Access::peek(engine, phase, delta, parameters);
        const bool duplicate = Access::peek(engine, phase, delta, parameters);
        metrics.schedulerDuplicatePeeks += duplicate ? 1u : 0u;

        if (!expectedRelevant)
        {
            if (peeked || Access::latch(engine).valid)
            {
                if (writes[ordinal].destination == Destination::Pitch)
                    ++metrics.schedulerPitchPeeks;
                if (writes[ordinal].destination == Destination::Noise)
                    ++metrics.schedulerNoisePeeks;
            }
            if (Access::schedulerCursor(engine) != cursorBefore)
                ++metrics.schedulerCursorFailures;
            continue;
        }

        ++metrics.schedulerPassiveOrdinals;
        const auto latch = Access::latch(engine);
        if (!peeked || !latch.valid || latch.nextPass
            || latch.ordinal != ordinal
            || latch.write.destination != writes[ordinal].destination
            || latch.write.voice != writes[ordinal].voice
            || std::abs(latch.eventPosition - 0.5) > 2.0e-12)
            ++metrics.schedulerOrderFailures;
        if (latch.target != expected || Access::heldTarget(engine, ordinal)
                                         != targetBefore)
            ++metrics.schedulerPayloadFailures;
        if (Access::schedulerCursor(engine) != cursorBefore)
            ++metrics.schedulerCursorFailures;

        float afterLfo = lfoAtEvent;
        Access::setAutomationAfterPeek(engine, parameters, afterLfo);
        if (writes[ordinal].destination == Destination::Pwm)
            Access::mutateStagedPwmPayloadAfterPeek(engine);
        const float changed = Access::eventTarget(
            engine, ordinal, parameters, afterLfo);
        Access::commitOverride(
            engine, ordinal, parameters, afterLfo, latch.target);
        if (Access::heldTarget(engine, ordinal) != expected
            || changed == expected)
            ++metrics.schedulerPayloadFailures;
    }

    const std::array<std::size_t, 8> expectedCounts {
        1u, 1u, 1u, 6u, 1u, 6u, 6u, 1u
    };
    if (destinationCounts != expectedCounts)
        ++metrics.schedulerOrderFailures;

    // Ordinal zero crosses a pass and a block boundary with the cursor still
    // parked at 23. It must be peeked without advancing the cursor, then use
    // the captured payload after the pass reset.
    auto wrap = Access::fixture(hostRate, hq, PwmMode::Manual);
    auto parameters = Access::parametersFor(PwmMode::Manual);
    Access::setSchedulerCursor(wrap, Access::writeCount());
    const double phase = 1.0 - 0.5 * delta;
    const float expected = Access::eventTarget(wrap, 0u, parameters, -0.37f);
    const bool peeked = Access::peek(wrap, phase, delta, parameters);
    const auto latch = Access::latch(wrap);
    if (!peeked || !latch.valid || !latch.nextPass || latch.ordinal != 0u
        || latch.write.destination != Destination::Resonance
        || std::abs(latch.eventPosition - 0.5) > 2.0e-12
        || Access::schedulerCursor(wrap) != Access::writeCount())
        ++metrics.schedulerPassWrapFailures;
    float afterLfo = -0.37f;
    Access::setAutomationAfterPeek(wrap, parameters, afterLfo);
    Access::commitOverride(wrap, 0u, parameters, afterLfo, latch.target);
    if (Access::heldTarget(wrap, 0u) != expected)
        ++metrics.schedulerPassWrapFailures;
}

bool passed(const Metrics& metrics) noexcept
{
    constexpr std::size_t expectedProcessCases =
        cells.size() * processPositions.size() * 17u;
    const auto allMutationsReject = [](const std::array<double, 4>& values) {
        return std::all_of(values.begin(), values.end(), [](double value) {
            return value > mutationRejectionGate;
        });
    };
    return Access::scalarLatchIsAllocationFree()
        && metrics.helperSettledCases > 0u
        && metrics.helperUnsettledCases > 0u
        && metrics.helperAlternatingCases > 0u
        && metrics.processCases == expectedProcessCases
        && metrics.blockWrapCases == 17u
        && metrics.schedulerOrdinals == 23u
        && metrics.schedulerPassiveOrdinals == 16u
        && metrics.schedulerPitchPeeks == 0u
        && metrics.schedulerNoisePeeks == 0u
        && metrics.schedulerDuplicatePeeks == 0u
        && metrics.schedulerPayloadFailures == 0u
        && metrics.schedulerClassificationFailures == 0u
        && metrics.schedulerCursorFailures == 0u
        && metrics.schedulerOrderFailures == 0u
        && metrics.schedulerPassWrapFailures == 0u
        && metrics.processingRateFailures == 0u
        && metrics.nonFiniteCases == 0u
        && metrics.maximumHelperError <= stateGate
        && metrics.maximumProcessError <= stateGate
        && metrics.maximumStateFloatUlps <= floatUlpGate
        && metrics.maximumConsumerUlps <= floatUlpGate
        && metrics.maximumProcessGeometryError <= 1.0e-10
        && metrics.collapsedNearEndpointCases == 0u
        && metrics.commonConsumerSamples >= 128u
        && metrics.commonConsumerMaximumRelativeError <= 2.0e-5
        && metrics.commonConsumerDisconnectedRelativeError > 0.01
        && metrics.subConsumerRelativeError <= 2.0e-6
        && metrics.subConsumerDisconnectedRelativeError > 0.01
        && metrics.maximumLateMutationError > mutationRejectionGate
        && metrics.maximumEarlyMutationError > mutationRejectionGate
        && metrics.maximumSequentialPwmMutationError > mutationRejectionGate
        && metrics.maximumDisconnectedMutationError > mutationRejectionGate
        && allMutationsReject(metrics.lateMutationByDestination)
        && allMutationsReject(metrics.earlyMutationByDestination)
        && allMutationsReject(metrics.disconnectedMutationByDestination);
}

void printReport(const Metrics& metrics)
{
    std::cout << "schema passive_hold_timing 1\n"
              << "boundary actual Engine::process scheduler and physical "
                 "VoiceVca[6]/CommonVca/Sub/Pwm states\n"
              << "grid endpoints={8k/q1,8k/q4,768k/q1}; standard_hq={"
                 "44.1/q4,48/q4,88.2/q2,96/q2,"
                 "176.4/q1,192/q1}; hq_off={44.1,48,88.2,96}/q1; "
                 "8k/q1 is endpoint coverage, not a standard host claim\n"
              << "oracle long-double piecewise one-pole and exact affine "
                 "two-pole cascade; state_gate=" << std::scientific
              << stateGate << " span float_consumer_ulp_gate="
              << floatUlpGate << '\n'
              << "coverage helper_settled=" << metrics.helperSettledCases
              << " helper_unsettled=" << metrics.helperUnsettledCases
              << " helper_alternating=" << metrics.helperAlternatingCases
              << " process_cases=" << metrics.processCases
              << " block_wrap_cases=" << metrics.blockWrapCases
              << " later_q4_internal_substep="
              << (metrics.blockWrapCases == 17u ? "PASS" : "FAIL") << '\n'
              << "scheduler ordinals=" << metrics.schedulerOrdinals
              << " passive=" << metrics.schedulerPassiveOrdinals
              << " pitch_peeks=" << metrics.schedulerPitchPeeks
              << " noise_peeks=" << metrics.schedulerNoisePeeks
              << " duplicate_peeks=" << metrics.schedulerDuplicatePeeks
              << " payload_failures=" << metrics.schedulerPayloadFailures
              << " classification_failures="
              << metrics.schedulerClassificationFailures
              << " cursor_failures=" << metrics.schedulerCursorFailures
              << " order_failures=" << metrics.schedulerOrderFailures
              << " pass_wrap_failures="
              << metrics.schedulerPassWrapFailures << '\n'
              << "accuracy helper_max=" << metrics.maximumHelperError
              << " helper_worst=" << metrics.worstHelper
              << " process_max=" << metrics.maximumProcessError
              << " process_worst=" << metrics.worstProcess
              << " state_float_max_ulp=" << metrics.maximumStateFloatUlps
              << " consumer_max_ulp=" << metrics.maximumConsumerUlps
              << " geometry_max=" << metrics.maximumProcessGeometryError
              << " near_endpoint_collapses="
              << metrics.collapsedNearEndpointCases << '\n'
              << "negative late_ceil=" << metrics.maximumLateMutationError
              << " early_floor=" << metrics.maximumEarlyMutationError
              << " sequential_pwm="
              << metrics.maximumSequentialPwmMutationError
              << " disconnected_shipping="
              << metrics.maximumDisconnectedMutationError << '\n';
    constexpr std::array<const char*, 4> mutationNames {
        "common-vca", "sub", "pwm", "voice-vca"
    };
    for (std::size_t index = 0; index < mutationNames.size(); ++index)
        std::cout << "negative_destination " << mutationNames[index]
                  << " late=" << metrics.lateMutationByDestination[index]
                  << " early=" << metrics.earlyMutationByDestination[index]
                  << " disconnected="
                  << metrics.disconnectedMutationByDestination[index] << '\n';
    std::cout << "consumer_wiring common_samples="
              << metrics.commonConsumerSamples
              << " common_max_relative="
              << metrics.commonConsumerMaximumRelativeError
              << " common_disconnected="
              << metrics.commonConsumerDisconnectedRelativeError
              << " sub_relative=" << metrics.subConsumerRelativeError
              << " sub_disconnected="
              << metrics.subConsumerDisconnectedRelativeError << '\n';
    std::cout
              << "invariants scalar_latch="
              << (Access::scalarLatchIsAllocationFree() ? "PASS" : "FAIL")
              << " rate_failures=" << metrics.processingRateFailures
              << " nonfinite=" << metrics.nonFiniteCases << '\n'
              << "Overall: " << (passed(metrics) ? "PASS" : "FAIL")
              << '\n';
}
} // namespace

int main(int argc, char** argv)
{
    bool selfTest = false;
    if (argc == 2 && std::string_view(argv[1]) == "--self-test")
        selfTest = true;
    else if (argc == 2 && std::string_view(argv[1]) == "--help")
    {
        std::cout << "usage: YouKnowPassiveHoldTimingAudit [--self-test]\n";
        return 0;
    }
    else if (argc != 1)
    {
        std::cerr << "usage: YouKnowPassiveHoldTimingAudit [--self-test]\n";
        return 2;
    }

    try
    {
        Metrics metrics;
        runHelperOracleAudit(metrics);
        runProcessWiringAudit(metrics);
        runBlockWrapAutomationAudit(metrics);
        runSchedulerAudit(metrics);
        runConsumerWiringAudit(metrics);
        printReport(metrics);
        if (selfTest && !passed(metrics))
            std::cerr << "passive-hold timing self-test FAILED\n";
        return passed(metrics) ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "passive-hold timing audit failed: "
                  << error.what() << '\n';
        return 1;
    }
}
