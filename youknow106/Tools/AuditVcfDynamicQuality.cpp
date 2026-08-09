// Time-varying VCF numerical-quality admission.
//
// The common-host VCF/BBD audit holds the filter controls and Unit Character
// nominal.  This executable owns the complementary boundary: it runs the
// normalized 23-write service-chart schedule, analytically continuous 522 us
// cutoff/resonance holds and the actual deterministic six-card profiles.  The
// production cascade is compared with a separately written double-precision
// continuous-ODE oracle.  One physical oracle grid serves each processing-rate
// family at 16x its base; four/eight RK4 subdivisions make the two references
// RK64/RK128 relative to that base.  The oracle is reduced once to the family's
// physical VCF grid, then both sides cross identical production 4x/2x/1x
// output boundaries. Four-times processing is never used as truth.

#include "DSP/YouKnow106Engine.h"
#include "OversamplingQualitySupport.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace youknow106
{
// Executable-local friend seam.  The oracle below calls no OtaCascade helper;
// this probe exposes only the declared control laws, deterministic card data,
// production boundary and physical state needed to audit recovery.
struct YouKnow106TestAccess
{
    using Cascade = YouKnow106Engine::OtaCascade;
    using ControlTrajectory = Cascade::ControlTrajectory;
    using VcfHoldInterval = YouKnow106Engine::VcfHoldInterval;

    enum class Destination : std::uint8_t
    {
        Resonance, CommonVca, Sub, Pitch, Pwm, Vcf, VoiceVca, Noise
    };

    struct Write
    {
        Destination destination;
        int voice;
    };

    struct CardProfile
    {
        int card {};
        float character {};
        double warmSeconds {};
        std::array<float, 4> gScale {};
        std::array<float, 4> offsetVoltage {};
        float cutoffScaleError {};
        float cutoffOffsetError {};
        float resonanceError {};
        float headroom {};
        float thermalCutoffSpread {};
    };

    struct ProcessingRate
    {
        int factor {};
        double internalRate {};
    };

    static constexpr std::size_t writeCount() noexcept
    {
        return YouKnow106Engine::converterWritesPerPass;
    }

    static std::array<double, YouKnow106Engine::converterWritesPerPass>
    eventPhases() noexcept
    {
        return YouKnow106Engine::converterEventPhases(
            YouKnow106Engine::ConverterTimingProfile::NormalizedServiceChart);
    }

    static std::array<Write, YouKnow106Engine::converterWritesPerPass>
    writes() noexcept
    {
        std::array<Write, YouKnow106Engine::converterWritesPerPass> result {};
        const auto& source = YouKnow106Engine::converterWriteOrder();
        for (std::size_t index = 0; index < source.size(); ++index)
        {
            Destination destination = Destination::Noise;
            switch (source[index].destination)
            {
                case YouKnow106Engine::ConverterDestination::Resonance:
                    destination = Destination::Resonance;
                    break;
                case YouKnow106Engine::ConverterDestination::CommonVca:
                    destination = Destination::CommonVca;
                    break;
                case YouKnow106Engine::ConverterDestination::Sub:
                    destination = Destination::Sub;
                    break;
                case YouKnow106Engine::ConverterDestination::Pitch:
                    destination = Destination::Pitch;
                    break;
                case YouKnow106Engine::ConverterDestination::Pwm:
                    destination = Destination::Pwm;
                    break;
                case YouKnow106Engine::ConverterDestination::Vcf:
                    destination = Destination::Vcf;
                    break;
                case YouKnow106Engine::ConverterDestination::VoiceVca:
                    destination = Destination::VoiceVca;
                    break;
                case YouKnow106Engine::ConverterDestination::Noise:
                    destination = Destination::Noise;
                    break;
            }
            result[index] = { destination, source[index].voice };
        }
        return result;
    }

    static constexpr double scanRate() noexcept
    {
        return YouKnow106Engine::controlScanHz;
    }

    static constexpr double holdSeconds() noexcept
    {
        static_assert(YouKnow106Engine::vcfHoldSlewSeconds
                      == YouKnow106Engine::resonanceHoldSlewSecondsVoiced);
        return YouKnow106Engine::vcfHoldSlewSeconds;
    }

    static CardProfile cardProfile(int card, float character,
                                   double warmSeconds)
    {
        YouKnow106Engine engine;
        engine.prepare(48000.0, 1, false);
        EngineParameters parameters;
        parameters.calibration = character;
        parameters.enableVcfStageOffsets = true;
        parameters.enableVcfEarlyEffect = true;
        parameters.enableSpatialThermalGradient = true;
        engine.setParameters(parameters);
        engine.thermalWarmupSeconds_ = warmSeconds;
        engine.thermalWarmupFraction_ = 1.0f - std::exp(
            -static_cast<float>(warmSeconds) / 900.0f);

        const auto index = static_cast<std::size_t>(card);
        const auto& voice = engine.voices_[index];
        const auto& source = engine.cards_[index];
        const float gradient = YouKnow106Engine::chassisGradientCelsius(card);
        const float thermalSpread = 1.0f
            + YouKnow106Engine::vcfCutoffTempcoPerCelsius * character
                * (gradient - YouKnow106Engine::chassisGradientMeanCelsius());
        return {
            card, character, warmSeconds,
            voice.filter.gScale, voice.filter.offsetVoltage,
            source.cutoffScaleError, source.cutoffOffsetError,
            source.resonanceError,
            engine.dynamicOtaHeadroomVolts(parameters, card),
            thermalSpread
        };
    }

    static ProcessingRate shippingProcessingRate(double hostRate)
    {
        YouKnow106Engine engine;
        engine.prepare(hostRate, 1, true);
        return { engine.getOversamplingFactor(), engine.oversampledRate_ };
    }

    static float cutoffTargetForByte(int byte, float character) noexcept
    {
        const float panel = static_cast<float>(byte) / 127.0f;
        float counts = YouKnow106Engine::vcfPanelCounts(panel);
        counts = std::clamp(counts, 0.0f,
                            YouKnow106Engine::vcfCountsCeiling);
        const float code = YouKnow106Engine::vcfDacCountStep
            * std::floor(counts / YouKnow106Engine::vcfDacCountStep);
        return code
            + YouKnow106Engine::vcfConverterCarryCounts(code) * character;
    }

    static float resonanceTargetForByte(int byte) noexcept
    {
        const float panel = static_cast<float>(byte) / 127.0f;
        return static_cast<float>(
            YouKnow106Engine::storedControlDacCode(panel)) / 4064.0f;
    }

    static float feedback(float heldResonance,
                          const CardProfile& profile) noexcept
    {
        const float panel = std::clamp(
            heldResonance
                + profile.resonanceError * 0.02f * profile.character,
            0.0f, 1.0f);
        return YouKnow106Engine::VoicedResonanceCompatibilityProfile::
            loopGain(panel);
    }

    static float omegaStep(float heldCutoffCounts, float feedbackValue,
                           const CardProfile& profile,
                           double internalRate) noexcept
    {
        const float analogCounts = heldCutoffCounts
            * (1.0f + profile.cutoffScaleError * 0.05f * profile.character)
            + profile.cutoffOffsetError * 0.07f
                * YouKnow106Engine::vcfCountsPerOctave * profile.character;
        const float cutoff = YouKnow106Engine::vcfEffectiveCutoffHz(
            analogCounts, feedbackValue);
        const float limited = std::min(
            cutoff, static_cast<float>(internalRate) * 0.45f);
        const double thermallySpread = 2.0 * 3.14159265358979323846
            * static_cast<double>(limited) / internalRate
            * static_cast<double>(profile.thermalCutoffSpread);
        return static_cast<float>(std::min(
            thermallySpread, Cascade::maximumOmegaStep));
    }

    static double unclampedOmegaStep(float heldCutoffCounts,
                                     float feedbackValue,
                                     const CardProfile& profile,
                                     double internalRate) noexcept
    {
        const float analogCounts = heldCutoffCounts
            * (1.0f + profile.cutoffScaleError * 0.05f * profile.character)
            + profile.cutoffOffsetError * 0.07f
                * YouKnow106Engine::vcfCountsPerOctave * profile.character;
        const float cutoff = YouKnow106Engine::vcfEffectiveCutoffHz(
            analogCounts, feedbackValue);
        const float limited = std::min(
            cutoff, static_cast<float>(internalRate) * 0.45f);
        return 2.0 * 3.14159265358979323846
            * static_cast<double>(limited) / internalRate
            * static_cast<double>(profile.thermalCutoffSpread);
    }

    static constexpr double maximumOmegaStep() noexcept
    {
        return Cascade::maximumOmegaStep;
    }

    static double angularFrequency(float heldCutoffCounts,
                                   float feedbackValue,
                                   const CardProfile& profile,
                                   double internalRate) noexcept
    {
        return static_cast<double>(omegaStep(
            heldCutoffCounts, feedbackValue, profile, internalRate))
            * internalRate;
    }

    static float inputCompensation(float feedbackValue) noexcept
    {
        return YouKnow106Engine::VoicedResonanceCompatibilityProfile::
            inputCompensation(feedbackValue);
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

    static void configure(Cascade& cascade,
                          const CardProfile& profile) noexcept
    {
        cascade.gScale = profile.gScale;
        cascade.offsetVoltage = profile.offsetVoltage;
    }

    static constexpr auto controlNodePositions() noexcept
    {
        return Cascade::controlNodePositions;
    }

    static VcfHoldInterval exactVcfHoldInterval(
        float state, float target, bool hasEvent, double eventPosition,
        float eventTarget, double intervalSeconds) noexcept
    {
        return YouKnow106Engine::exactVcfHoldInterval(
            state, target, hasEvent, eventPosition, eventTarget,
            intervalSeconds);
    }

    struct SchedulerContract
    {
        std::size_t peeks {};
        std::size_t commits {};
        bool purePeek { true };
        bool payloadLatched { true };
        bool cursorExact { true };
        bool orderExact { true };
        bool passWrapExact { true };

        [[nodiscard]] bool passed() const noexcept
        {
            return peeks == 7u && commits == 7u && purePeek
                && payloadLatched && cursorExact && orderExact
                && passWrapExact;
        }
    };

    static SchedulerContract schedulerContract()
    {
        SchedulerContract result;
        const auto phases = YouKnow106Engine::converterEventPhases(
            YouKnow106Engine::ConverterTimingProfile::NormalizedServiceChart);
        const auto& writes = YouKnow106Engine::converterWriteOrder();

        // Exercise every per-card VCF payload after its Pitch ordinal. The
        // envelope, pitch and pass-latched LFO are changed after the peek;
        // committing through the real write path must still use the payload
        // captured at the physical event.
        for (int card = 0; card < YouKnow106Engine::hardwareVoices; ++card)
        {
            YouKnow106Engine engine;
            engine.prepare(8000.0, 1, true);
            engine.converterEventPhases_ = phases;
            const std::size_t ordinal = 10u
                + 2u * static_cast<std::size_t>(card);
            engine.nextConverterWrite_ = ordinal;
            engine.vcfEventLatch_ = {};
            engine.converterPassLfoGated_ = -0.21f;
            auto& voice = engine.voices_[static_cast<std::size_t>(card)];
            voice.envelope.value = 0.18f + 0.03f * static_cast<float>(card);
            voice.currentMidi = 43.0f + 5.0f * static_cast<float>(card);
            voice.cutoffCountsTarget = 123.0f + static_cast<float>(card);

            EngineParameters atEvent;
            atEvent.calibration = 0.65f;
            atEvent.cutoff = 0.27f;
            atEvent.envDepth = 0.61f;
            atEvent.keyFollow = 0.42f;
            atEvent.vcfLfoDepth = 0.38f;
            const auto& write = writes[ordinal];
            const float wanted = engine.vcfWriteTarget(
                write, atEvent, engine.converterPassLfoGated_);
            const float targetBefore = voice.cutoffCountsTarget;
            const auto lfoBefore = engine.lfoAccumulator_;
            const float envelopeBefore = voice.envelope.value;
            const double delta = YouKnow106Engine::controlScanHz
                               / engine.oversampledRate_;
            const double phase = phases[ordinal] - 0.5 * delta;
            const std::size_t cursorBefore = engine.nextConverterWrite_;
            const bool peeked = engine.latchUpcomingVcfEvent(
                phase, delta, atEvent);
            const bool duplicatePeek = engine.latchUpcomingVcfEvent(
                phase, delta, atEvent);
            result.peeks += peeked ? 1u : 0u;
            result.purePeek = result.purePeek && peeked && !duplicatePeek
                && engine.nextConverterWrite_ == cursorBefore
                && voice.cutoffCountsTarget == targetBefore
                && engine.lfoAccumulator_ == lfoBefore
                && voice.envelope.value == envelopeBefore;
            result.orderExact = result.orderExact
                && engine.vcfEventLatch_.valid
                && !engine.vcfEventLatch_.nextPass
                && engine.vcfEventLatch_.ordinal == ordinal
                && engine.vcfEventLatch_.write.destination
                       == YouKnow106Engine::ConverterDestination::Vcf
                && engine.vcfEventLatch_.write.voice == card
                && std::abs(engine.vcfEventLatch_.eventPosition - 0.5)
                       <= 1.0e-12;
            const float latched = engine.vcfEventLatch_.target;
            result.payloadLatched = result.payloadLatched
                && latched == wanted;

            EngineParameters afterEvent = atEvent;
            afterEvent.cutoff = 0.86f;
            afterEvent.envDepth = 0.09f;
            afterEvent.keyFollow = 0.91f;
            afterEvent.vcfLfoDepth = 0.02f;
            voice.envelope.value = 0.93f;
            voice.currentMidi = 91.0f;
            engine.converterPassLfoGated_ = 0.74f;
            const float changed = engine.vcfWriteTarget(
                write, afterEvent, engine.converterPassLfoGated_);
            engine.performConverterWrite(
                write, afterEvent, engine.converterPassLfoGated_, &latched);
            ++engine.nextConverterWrite_;
            engine.vcfEventLatch_ = {};
            ++result.commits;
            result.payloadLatched = result.payloadLatched
                && voice.cutoffCountsTarget == wanted
                && wanted != changed;
            result.cursorExact = result.cursorExact
                && engine.nextConverterWrite_ == ordinal + 1u;
        }

        // Ordinal zero is special: it is peeked while the previous cursor is
        // already 23, then committed once after the pass-wrap reset. Neither
        // LFO nor envelope state may advance during that speculative peek.
        YouKnow106Engine engine;
        engine.prepare(8000.0, 1, true);
        engine.converterEventPhases_ = phases;
        engine.nextConverterWrite_ = writes.size();
        engine.vcfEventLatch_ = {};
        engine.resonanceCvTarget_ = 0.17f;
        EngineParameters atEvent;
        atEvent.resonance = 0.34f;
        const double delta = YouKnow106Engine::controlScanHz
                           / engine.oversampledRate_;
        const double phase = 1.0 - 0.5 * delta;
        const auto lfoBefore = engine.lfoAccumulator_;
        const float envelopeBefore = engine.voices_[0].envelope.value;
        const float targetBefore = engine.resonanceCvTarget_;
        const float wanted = engine.vcfWriteTarget(
            writes[0], atEvent, engine.converterPassLfoGated_);
        const bool peeked = engine.latchUpcomingVcfEvent(
            phase, delta, atEvent);
        ++result.peeks;
        result.purePeek = result.purePeek && peeked
            && engine.nextConverterWrite_ == writes.size()
            && engine.resonanceCvTarget_ == targetBefore
            && engine.lfoAccumulator_ == lfoBefore
            && engine.voices_[0].envelope.value == envelopeBefore;
        result.passWrapExact = result.passWrapExact
            && engine.vcfEventLatch_.valid
            && engine.vcfEventLatch_.nextPass
            && engine.vcfEventLatch_.ordinal == 0u
            && engine.vcfEventLatch_.write.destination
                   == YouKnow106Engine::ConverterDestination::Resonance
            && std::abs(engine.vcfEventLatch_.eventPosition - 0.5)
                   <= 1.0e-12;
        const float latched = engine.vcfEventLatch_.target;
        EngineParameters afterEvent = atEvent;
        afterEvent.resonance = 0.91f;
        const float changed = engine.vcfWriteTarget(
            writes[0], afterEvent, engine.converterPassLfoGated_);
        engine.nextConverterWrite_ = 0u;
        engine.performConverterWrite(
            writes[0], afterEvent, engine.converterPassLfoGated_, &latched);
        engine.vcfEventLatch_ = {};
        ++engine.nextConverterWrite_;
        ++result.commits;
        result.payloadLatched = result.payloadLatched
            && engine.resonanceCvTarget_ == wanted && wanted != changed;
        result.cursorExact = result.cursorExact
            && engine.nextConverterWrite_ == 1u;
        return result;
    }

    struct ShippingWiringContract
    {
        std::size_t probes {};
        double maximumConnectedStateDifference {};
        double minimumDisconnectedStateDifference {
            std::numeric_limits<double>::infinity()
        };
        bool connectedExact { true };
        bool disconnectedRejected { true };
        bool finite { true };

        [[nodiscard]] bool passed() const noexcept
        {
            return probes == 4u && connectedExact
                && disconnectedRejected && finite
                && maximumConnectedStateDifference == 0.0
                && minimumDisconnectedStateDifference > 0.0;
        }
    };

    // This is deliberately a renderVoice probe, not another direct cascade
    // candidate. It supplies a strongly curved physical hold interval to the
    // shipping voice, recovers the exact input endpoint which renderVoice
    // handed to its OtaCascade, and replays that one interval from the saved
    // capacitor/input-history state. The connected replay must be bit exact;
    // the explicit nullptr replay is the sensitivity mutation and must differ.
    // Consequently changing Engine.cpp's renderVoice call to pass nullptr
    // makes the actual shipping state coincide with the rejected mutation and
    // fails this contract even if every local OtaCascade test still passes.
    static ShippingWiringContract shippingWiringContract()
    {
        constexpr std::array<double, 4> hostRates {
            8000.0, 44100.0, 48000.0, 768000.0
        };
        constexpr float twoPi = 6.28318530717958647692f;
        ShippingWiringContract result;
        for (std::size_t probe = 0; probe < hostRates.size(); ++probe)
        {
            YouKnow106Engine engine;
            engine.prepare(hostRates[probe], 1, true);
            EngineParameters parameters;
            parameters.calibration = 0.0f;
            parameters.sawEnabled = true;
            parameters.pulseEnabled = false;
            parameters.subLevel = 0.0f;
            parameters.noiseLevel = 0.0f;
            parameters.enableVcfStageOffsets = false;
            parameters.enableSpatialThermalGradient = false;
            engine.setParameters(parameters);

            const int card = static_cast<int>(probe);
            auto& voice = engine.voices_[static_cast<std::size_t>(card)];
            engine.initialiseVoice(voice, card, 48 + 4 * card, 1.0f);
            voice.dco.reset();
            voice.dco.periodSamples = engine.oversampledRate_
                                    / (173.0 + 29.0 * card);
            voice.dco.phase = 0.271 + 0.071 * static_cast<double>(card);
            voice.dco.renderScale = 1.0f;
            voice.dcoCv = static_cast<float>(173.0 + 29.0 * card);
            voice.dcoCvTarget = voice.dcoCv;
            voice.moduleCoupling.reset();
            voice.vcaInputCoupling.reset();
            voice.filter.reset();
            voice.filter.state = { 0.17, -0.11, 0.073, -0.047 };
            voice.filter.inputHistory = { 0.31, -0.19, 0.09 };
            voice.filter.inputHistoryCount = 2;
            voice.filter.gScale.fill(1.0f);
            voice.filter.offsetVoltage.fill(0.0f);

            const float oldCutoff = cutoffTargetForByte(51, 0.0f);
            const float newCutoff = cutoffTargetForByte(104, 0.0f);
            const float oldResonance = resonanceTargetForByte(91);
            const float resonanceTarget = resonanceTargetForByte(19);
            const double interval = 1.0 / engine.oversampledRate_;
            const double eventPosition = 0.37;
            const auto cutoff = YouKnow106Engine::exactVcfHoldInterval(
                oldCutoff, oldCutoff, true, eventPosition, newCutoff,
                interval);
            const auto resonance = YouKnow106Engine::exactVcfHoldInterval(
                oldResonance, resonanceTarget, false, 1.0,
                resonanceTarget, interval);
            engine.cutoffVcfHoldIntervals_[
                static_cast<std::size_t>(card)] = cutoff;
            engine.resonanceVcfHoldInterval_ = resonance;
            engine.exactVcfControlInterval_.fill(false);
            engine.exactVcfControlInterval_[
                static_cast<std::size_t>(card)] = true;
            voice.cutoffCounts = cutoff.endpoint;
            voice.cutoffCountsTarget = newCutoff;
            engine.resonanceCv_ = resonance.endpoint;
            engine.resonanceCvTarget_ = resonanceTarget;

            struct MappedControl
            {
                double omega {};
                double feedback {};
            };
            const auto mapHeld = [&](double cutoffCounts,
                                     double resonanceCv) {
                const float feedback =
                    YouKnow106Engine::VoicedResonanceCompatibilityProfile::
                        loopGain(std::clamp(
                            static_cast<float>(resonanceCv), 0.0f, 1.0f));
                const float cutoffHz = YouKnow106Engine::vcfEffectiveCutoffHz(
                    static_cast<float>(cutoffCounts), feedback);
                const float limited = std::min(
                    cutoffHz,
                    static_cast<float>(engine.oversampledRate_) * 0.45f);
                const float baseOmega = twoPi * limited
                                      * engine.inverseOversampledRate_;
                return MappedControl {
                    YouKnow106Engine::boundedThermalFilterOmegaStep(
                        baseOmega, parameters, card),
                    feedback
                };
            };

            const auto start = mapHeld(
                cutoff.value.front(), resonance.value.front());
            const auto endpoint = mapHeld(
                cutoff.value.back(), resonance.value.back());
            const float headroom = engine.dynamicOtaHeadroomVolts(
                parameters, card);
            voice.filter.previousOmegaStep = start.omega;
            voice.filter.previousFeedback = start.feedback;
            voice.filter.previousHeadroom = headroom;
            voice.filter.parameterHistoryPrimed = true;
            voice.filterOmegaStep = static_cast<float>(endpoint.omega);
            voice.feedback = static_cast<float>(endpoint.feedback);
            voice.inputCompensation =
                YouKnow106Engine::VoicedResonanceCompatibilityProfile::
                    inputCompensation(voice.feedback);

            ControlTrajectory trajectory;
            for (std::size_t point = 0;
                 point < trajectory.omegaStep.size(); ++point)
            {
                const auto mapped = mapHeld(
                    cutoff.value[point], resonance.value[point]);
                trajectory.omegaStep[point] = mapped.omega;
                trajectory.feedback[point] = mapped.feedback;
                trajectory.headroom[point] = headroom;
            }
            trajectory.omegaStep.front() = start.omega;
            trajectory.feedback.front() = start.feedback;
            trajectory.headroom.front() = headroom;
            trajectory.omegaStep.back() = endpoint.omega;
            trajectory.feedback.back() = endpoint.feedback;
            trajectory.headroom.back() = headroom;

            Cascade connected = voice.filter;
            Cascade disconnected = voice.filter;
            (void) engine.renderVoice(voice, parameters, 0.0f);
            const float renderedInput = static_cast<float>(
                voice.filter.inputHistory[0]);
            (void) connected.process(
                renderedInput, voice.filterOmegaStep, voice.feedback,
                headroom, parameters.enableVcfEarlyEffect,
                parameters.calibration, &trajectory);
            (void) disconnected.process(
                renderedInput, voice.filterOmegaStep, voice.feedback,
                headroom, parameters.enableVcfEarlyEffect,
                parameters.calibration, nullptr);

            double connectedDifference = 0.0;
            double disconnectedDifference = 0.0;
            for (std::size_t stage = 0; stage < voice.filter.state.size();
                 ++stage)
            {
                result.finite = result.finite
                    && std::isfinite(voice.filter.state[stage])
                    && std::isfinite(connected.state[stage])
                    && std::isfinite(disconnected.state[stage]);
                connectedDifference = std::max(
                    connectedDifference,
                    std::abs(voice.filter.state[stage]
                             - connected.state[stage]));
                disconnectedDifference = std::max(
                    disconnectedDifference,
                    std::abs(voice.filter.state[stage]
                             - disconnected.state[stage]));
            }
            result.maximumConnectedStateDifference = std::max(
                result.maximumConnectedStateDifference,
                connectedDifference);
            result.minimumDisconnectedStateDifference = std::min(
                result.minimumDisconnectedStateDifference,
                disconnectedDifference);
            result.connectedExact = result.connectedExact
                && connectedDifference == 0.0;
            result.disconnectedRejected = result.disconnectedRejected
                && disconnectedDifference > 0.0;
            result.finite = result.finite
                && std::isfinite(renderedInput)
                && std::isfinite(connectedDifference)
                && std::isfinite(disconnectedDifference);
            ++result.probes;
        }
        return result;
    }

    static bool stateExactlyZero(const Cascade& cascade) noexcept
    {
        return std::all_of(cascade.state.begin(), cascade.state.end(),
                           [](const auto value) { return value == 0; });
    }

    static bool stateFiniteAndBounded(const Cascade& cascade) noexcept
    {
        return std::all_of(cascade.state.begin(), cascade.state.end(),
                           [](const auto value) {
            const double converted = static_cast<double>(value);
            return std::isfinite(converted) && std::abs(converted) <= 64.0;
        });
    }

    static double maximumStateMagnitude(const Cascade& cascade) noexcept
    {
        double maximum = 0.0;
        for (const auto value : cascade.state)
            maximum = std::max(maximum,
                               std::abs(static_cast<double>(value)));
        return maximum;
    }

    template <typename Sample>
    static std::vector<double> decimate(const std::vector<Sample>& input,
                                        int factor)
    {
        if (factor != 1 && factor != 2 && factor != 4)
            throw std::runtime_error("factor must be 1, 2 or 4");
        if (input.size() % static_cast<std::size_t>(factor) != 0u)
            throw std::runtime_error("input is not host-frame aligned");
        if (factor == 1)
        {
            std::vector<double> result(input.size());
            std::transform(input.begin(), input.end(), result.begin(),
                           [](const Sample value) {
                return static_cast<double>(value);
            });
            return result;
        }

        YouKnow106Engine engine;
        std::vector<double> output(
            input.size() / static_cast<std::size_t>(factor));
        for (std::size_t host = 0; host < output.size(); ++host)
        {
            const auto base = host * static_cast<std::size_t>(factor);
            float left = 0.0f;
            float right = 0.0f;
            if (factor == 2)
            {
                engine.downsamplePair(
                    engine.firstDecimator_,
                    static_cast<float>(input[base]),
                    static_cast<float>(input[base]),
                    static_cast<float>(input[base + 1u]),
                    static_cast<float>(input[base + 1u]),
                    left, right);
            }
            else
            {
                float firstLeft = 0.0f;
                float firstRight = 0.0f;
                float secondLeft = 0.0f;
                float secondRight = 0.0f;
                engine.downsamplePair(
                    engine.firstDecimator_,
                    static_cast<float>(input[base]),
                    static_cast<float>(input[base]),
                    static_cast<float>(input[base + 1u]),
                    static_cast<float>(input[base + 1u]),
                    firstLeft, firstRight);
                engine.downsamplePair(
                    engine.firstDecimator_,
                    static_cast<float>(input[base + 2u]),
                    static_cast<float>(input[base + 2u]),
                    static_cast<float>(input[base + 3u]),
                    static_cast<float>(input[base + 3u]),
                    secondLeft, secondRight);
                engine.downsamplePair(
                    engine.secondDecimator_,
                    firstLeft, firstRight, secondLeft, secondRight,
                    left, right);
            }
            output[host] = left;
        }
        return output;
    }
};
} // namespace youknow106

namespace
{
using youknow106::YouKnow106TestAccess;
namespace quality = youknow106::oversampling_quality;

constexpr double pi = 3.14159265358979323846;
constexpr double toneHz = 220.0;
constexpr double toneVolts = 2.4;
constexpr double warmSeconds = 900.0;
constexpr double converterPassSeconds = 21.0 / 5000.0;
constexpr double converterScanRate = 5000.0 / 21.0;
constexpr int renderedPasses = 4;
constexpr int captureFirstPass = 1;
constexpr int rk64Substeps = 4;
constexpr int rk128Substeps = 8;
constexpr double relativeRmsGate = 0.01;
constexpr double convergenceGate = 1.0e-4;
constexpr std::size_t referenceFilterTaps = 4097u;
constexpr std::size_t endpointReferenceFilterTaps = 1025u;

// Four distinct, reachable service-scan snapshots.  They cross more than five
// cutoff octaves and a broad sub-oscillation resonance span without turning
// the audit into a self-oscillation phase test.  The panel values are exact
// stored bytes, not arbitrary floating-point controls.
constexpr std::array<int, renderedPasses> cutoffBytes { 58, 82, 66, 91 };
constexpr std::array<int, renderedPasses> resonanceBytes { 20, 72, 38, 84 };

struct Cell
{
    double hostRate;
    int factor;
    int family;
};

constexpr std::array<Cell, 8> shippingCells {{
    { 8000.0, 4, 2 },
    { 44100.0, 4, 0 },
    { 48000.0, 4, 1 },
    { 88200.0, 2, 0 },
    { 96000.0, 2, 1 },
    { 176400.0, 1, 0 },
    { 192000.0, 1, 1 },
    { 768000.0, 1, 3 },
}};

constexpr std::array<double, 4> familyBaseRates {
    44100.0, 48000.0, 8000.0, 192000.0
};

struct ScheduledEvent
{
    double time;
    int pass;
    std::size_t ordinal;
    YouKnow106TestAccess::Destination destination;
    int voice;
};

struct Schedule
{
    std::vector<ScheduledEvent> events;
    bool phasesExact {};
    bool orderExact {};
    bool scanRateExact {};
};

Schedule buildSchedule()
{
    using Destination = YouKnow106TestAccess::Destination;
    constexpr std::array<YouKnow106TestAccess::Write, 23> expected {{
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
        { Destination::Noise, -1 },
    }};
    static_assert(expected.size() == YouKnow106TestAccess::writeCount());

    const auto shippingPhases = YouKnow106TestAccess::eventPhases();
    const auto shippingWrites = YouKnow106TestAccess::writes();
    Schedule schedule;
    schedule.phasesExact = true;
    schedule.orderExact = true;
    schedule.scanRateExact = std::abs(
        1.0 / YouKnow106TestAccess::scanRate()
            - converterPassSeconds) <= 1.0e-15
        && std::abs(YouKnow106TestAccess::scanRate() / converterScanRate
                    - 1.0) <= 1.0e-15;
    for (std::size_t ordinal = 0; ordinal < expected.size(); ++ordinal)
    {
        const double wanted = static_cast<double>(ordinal)
                            / static_cast<double>(expected.size());
        schedule.phasesExact = schedule.phasesExact
            && std::abs(shippingPhases[ordinal] - wanted) <= 1.0e-15;
        schedule.orderExact = schedule.orderExact
            && shippingWrites[ordinal].destination
                   == expected[ordinal].destination
            && shippingWrites[ordinal].voice == expected[ordinal].voice;
    }

    // The oracle timeline and payload order are intentionally constructed
    // from independent rational constants and the hard-coded service order
    // above.  The shipping tables are observed only by the exactness checks;
    // they cannot silently move the reference along with the candidate.
    schedule.events.reserve(renderedPasses * expected.size());
    for (int pass = 0; pass < renderedPasses; ++pass)
    {
        for (std::size_t ordinal = 0; ordinal < expected.size(); ++ordinal)
        {
            const double phase = static_cast<double>(ordinal)
                               / static_cast<double>(expected.size());
            schedule.events.push_back({
                (static_cast<double>(pass) + phase) * converterPassSeconds,
                pass, ordinal, expected[ordinal].destination,
                expected[ordinal].voice
            });
        }
    }
    return schedule;
}

Schedule snapScheduleToInternalGrid(const Schedule& exact,
                                    double internalRate,
                                    bool early)
{
    Schedule snapped = exact;
    for (auto& event : snapped.events)
    {
        const double grid = event.time * internalRate;
        event.time = (early
                ? std::floor(grid + 1.0e-12)
                : std::ceil(grid - 1.0e-12))
            / internalRate;
    }
    return snapped;
}

class ExactHold
{
public:
    struct TargetEvent
    {
        double time;
        double target;
    };

    ExactHold(double initial, std::vector<TargetEvent> targetEvents,
              double timeConstant)
        : initial_(initial), timeConstant_(timeConstant)
    {
        double state = initial;
        double target = initial;
        double previousTime = 0.0;
        transitions_.reserve(targetEvents.size());
        for (const auto& event : targetEvents)
        {
            state = target + (state - target)
                * std::exp(-(event.time - previousTime) / timeConstant_);
            transitions_.push_back(
                { event.time, state, event.target });
            previousTime = event.time;
            target = event.target;
        }
    }

    [[nodiscard]] double value(double time) const noexcept
    {
        for (auto iterator = transitions_.rbegin();
             iterator != transitions_.rend(); ++iterator)
        {
            if (iterator->time <= time + 1.0e-15)
            {
                return iterator->targetAfter
                    + (iterator->stateAtEvent - iterator->targetAfter)
                        * std::exp(-(time - iterator->time) / timeConstant_);
            }
        }
        return initial_;
    }

private:
    struct Transition
    {
        double time;
        double stateAtEvent;
        double targetAfter;
    };

    double initial_ {};
    double timeConstant_ {};
    std::vector<Transition> transitions_;
};

struct ControlTrajectories
{
    ExactHold cutoff;
    ExactHold resonance;
    std::size_t totalWrites {};
    std::size_t cutoffWrites {};
    std::size_t resonanceWrites {};
};

ControlTrajectories buildTrajectories(
    const Schedule& schedule, int card,
    const YouKnow106TestAccess::CardProfile& profile,
    double eventGridRate = 0.0)
{
    const auto eventTime = [eventGridRate](double time) {
        if (!(eventGridRate > 0.0))
            return time;
        return std::ceil(time * eventGridRate - 1.0e-12) / eventGridRate;
    };
    std::vector<ExactHold::TargetEvent> cutoffEvents;
    std::vector<ExactHold::TargetEvent> resonanceEvents;
    for (const auto& event : schedule.events)
    {
        if (event.destination == YouKnow106TestAccess::Destination::Vcf
            && event.voice == card)
        {
            cutoffEvents.push_back({ eventTime(event.time),
                YouKnow106TestAccess::cutoffTargetForByte(
                    cutoffBytes[static_cast<std::size_t>(event.pass)],
                    profile.character) });
        }
        else if (event.destination
                     == YouKnow106TestAccess::Destination::Resonance)
        {
            resonanceEvents.push_back({ eventTime(event.time),
                YouKnow106TestAccess::resonanceTargetForByte(
                    resonanceBytes[static_cast<std::size_t>(event.pass)]) });
        }
    }

    const double initialCutoff =
        YouKnow106TestAccess::cutoffTargetForByte(
            cutoffBytes[0], profile.character);
    const double initialResonance =
        YouKnow106TestAccess::resonanceTargetForByte(resonanceBytes[0]);
    const std::size_t cutoffWriteCount = cutoffEvents.size();
    const std::size_t resonanceWriteCount = resonanceEvents.size();
    return {
        ExactHold(initialCutoff, std::move(cutoffEvents),
                  YouKnow106TestAccess::holdSeconds()),
        ExactHold(initialResonance, std::move(resonanceEvents),
                  YouKnow106TestAccess::holdSeconds()),
        schedule.events.size(),
        cutoffWriteCount,
        resonanceWriteCount
    };
}

struct InstantaneousControls
{
    double angularFrequency;
    double feedback;
    double headroom;
    double input;
};

InstantaneousControls controlsAt(
    double time, const ControlTrajectories& trajectories,
    const YouKnow106TestAccess::CardProfile& profile,
    double internalRate)
{
    const float heldResonance = static_cast<float>(
        trajectories.resonance.value(time));
    const float feedback = YouKnow106TestAccess::feedback(
        heldResonance, profile);
    const float heldCutoff = static_cast<float>(
        trajectories.cutoff.value(time));
    const double angular = YouKnow106TestAccess::angularFrequency(
        heldCutoff, feedback, profile, internalRate);
    const double input = toneVolts
        * static_cast<double>(
            YouKnow106TestAccess::inputCompensation(feedback))
        * std::sin(2.0 * pi * toneHz * time);
    return { angular, feedback, profile.headroom, input };
}

// Independent double continuous-time equations.  The implementation uses
// std::tanh directly and shares no interpolation, derivative or integration
// helper with production.
class ReferenceCascade
{
public:
    ReferenceCascade(const YouKnow106TestAccess::CardProfile& profile,
                     const ControlTrajectories& trajectories,
                     double internalRate)
        : profile_(profile), trajectories_(trajectories),
          internalRate_(internalRate)
    {
    }

    double advance(double start, double interval, int subdivisions)
    {
        const double step = interval / static_cast<double>(subdivisions);
        for (int subdivision = 0; subdivision < subdivisions; ++subdivision)
        {
            const double time = start
                + static_cast<double>(subdivision) * step;
            const auto k1 = derivative(state_, time);
            const auto k2 = derivative(add(state_, k1, 0.5 * step),
                                       time + 0.5 * step);
            const auto k3 = derivative(add(state_, k2, 0.5 * step),
                                       time + 0.5 * step);
            const auto k4 = derivative(add(state_, k3, step), time + step);
            for (std::size_t stage = 0; stage < state_.size(); ++stage)
                state_[stage] += step * (k1[stage] + 2.0 * k2[stage]
                                      + 2.0 * k3[stage] + k4[stage]) / 6.0;
        }
        return state_[3];
    }

    [[nodiscard]] bool finite() const noexcept
    {
        return std::all_of(state_.begin(), state_.end(), [](double value) {
            return std::isfinite(value) && std::abs(value) <= 64.0;
        });
    }

private:
    static std::array<double, 4> add(
        const std::array<double, 4>& origin,
        const std::array<double, 4>& slope, double scale) noexcept
    {
        std::array<double, 4> result {};
        for (std::size_t stage = 0; stage < result.size(); ++stage)
            result[stage] = origin[stage] + slope[stage] * scale;
        return result;
    }

    std::array<double, 4> derivative(
        const std::array<double, 4>& value, double time) const
    {
        const auto controls = controlsAt(
            time, trajectories_, profile_, internalRate_);
        const double headroom = std::max(controls.headroom, 1.0e-5);
        const double beta = YouKnow106TestAccess::earlyCoefficient()
                          * static_cast<double>(profile_.character);
        const double feedbackHeadroom =
            YouKnow106TestAccess::feedbackHeadroom();
        std::array<double, 4> result {};
        double drive = controls.input
            - controls.feedback * feedbackHeadroom
                * std::tanh(value[3] / feedbackHeadroom);
        for (std::size_t stage = 0; stage < result.size(); ++stage)
        {
            const double early = 1.0
                + beta * std::tanh(value[stage] / headroom);
            result[stage] = controls.angularFrequency
                * static_cast<double>(profile_.gScale[stage])
                * headroom * early
                * std::tanh((drive - value[stage]
                             + static_cast<double>(
                                 profile_.offsetVoltage[stage]))
                            / headroom);
            drive = value[stage];
        }
        return result;
    }

    const YouKnow106TestAccess::CardProfile& profile_;
    const ControlTrajectories& trajectories_;
    double internalRate_ {};
    std::array<double, 4> state_ {};
};

class EndpointCandidateScan
{
public:
    EndpointCandidateScan(const YouKnow106TestAccess::CardProfile& profile,
                          double internalRate)
        : profile_(profile), internalRate_(internalRate),
          cutoffState_(YouKnow106TestAccess::cutoffTargetForByte(
              cutoffBytes[0], profile.character)),
          cutoffTarget_(cutoffState_),
          resonanceState_(YouKnow106TestAccess::resonanceTargetForByte(
              resonanceBytes[0])),
          resonanceTarget_(resonanceState_)
    {
    }

    InstantaneousControls advanceEndpoint()
    {
        if (phase_ >= 1.0)
        {
            phase_ -= 1.0;
            nextWrite_ = 0;
            currentPass_ = std::min(currentPass_ + 1, renderedPasses - 1);
        }

        const auto writes = YouKnow106TestAccess::writes();
        const auto phases = YouKnow106TestAccess::eventPhases();
        while (nextWrite_ < writes.size()
               && phases[nextWrite_] <= phase_ + 1.0e-12)
        {
            const auto& write = writes[nextWrite_];
            if (write.destination
                    == YouKnow106TestAccess::Destination::Resonance)
            {
                resonanceTarget_ =
                    YouKnow106TestAccess::resonanceTargetForByte(
                        resonanceBytes[static_cast<std::size_t>(currentPass_)]);
                ++resonanceWrites_;
            }
            else if (write.destination
                         == YouKnow106TestAccess::Destination::Vcf
                     && write.voice == profile_.card)
            {
                cutoffTarget_ = YouKnow106TestAccess::cutoffTargetForByte(
                    cutoffBytes[static_cast<std::size_t>(currentPass_)],
                    profile_.character);
                ++cutoffWrites_;
            }
            ++nextWrite_;
            ++totalWrites_;
        }

        // Match the production hold endpoint arithmetic exactly: its inverse
        // internal rate and physical time constant are both float states.
        const float inverseRate = static_cast<float>(1.0 / internalRate_);
        const float holdSeconds = static_cast<float>(
            YouKnow106TestAccess::holdSeconds());
        const float slew = 1.0f - std::exp(-inverseRate / holdSeconds);
        cutoffState_ += (cutoffTarget_ - cutoffState_) * slew;
        resonanceState_ += (resonanceTarget_ - resonanceState_) * slew;
        const float feedback = YouKnow106TestAccess::feedback(
            resonanceState_, profile_);
        const float omegaStep = YouKnow106TestAccess::omegaStep(
            cutoffState_, feedback, profile_, internalRate_);
        time_ += 1.0 / internalRate_;
        phase_ += YouKnow106TestAccess::scanRate() / internalRate_;
        const double input = toneVolts
            * static_cast<double>(
                YouKnow106TestAccess::inputCompensation(feedback))
            * std::sin(2.0 * pi * toneHz * time_);
        return {
            static_cast<double>(omegaStep) * internalRate_,
            feedback, profile_.headroom, input
        };
    }

    [[nodiscard]] std::size_t totalWrites() const noexcept
    {
        return totalWrites_;
    }

    [[nodiscard]] std::size_t cutoffWrites() const noexcept
    {
        return cutoffWrites_;
    }

    [[nodiscard]] std::size_t resonanceWrites() const noexcept
    {
        return resonanceWrites_;
    }

private:
    const YouKnow106TestAccess::CardProfile& profile_;
    double internalRate_ {};
    float cutoffState_ {};
    float cutoffTarget_ {};
    float resonanceState_ {};
    float resonanceTarget_ {};
    double phase_ { 1.0 };
    double time_ {};
    std::size_t nextWrite_ {};
    int currentPass_ { -1 };
    std::size_t totalWrites_ {};
    std::size_t cutoffWrites_ {};
    std::size_t resonanceWrites_ {};
};

class EventAwareCandidateScan
{
public:
    struct Frame
    {
        InstantaneousControls endpoint {};
        YouKnow106TestAccess::ControlTrajectory trajectory {};
        bool exactControl {};
    };

    EventAwareCandidateScan(
        const Schedule& schedule,
        const ControlTrajectories& exactTrajectories,
        const YouKnow106TestAccess::CardProfile& profile,
        double internalRate)
        : schedule_(schedule), exactTrajectories_(exactTrajectories),
          profile_(profile), internalRate_(internalRate),
          cutoffState_(YouKnow106TestAccess::cutoffTargetForByte(
              cutoffBytes[0], profile.character)),
          cutoffTarget_(cutoffState_),
          resonanceState_(YouKnow106TestAccess::resonanceTargetForByte(
              resonanceBytes[0])),
          resonanceTarget_(resonanceState_)
    {
    }

    Frame advance()
    {
        const double interval = 1.0 / internalRate_;
        const double start = time_;
        const double end = start + interval;
        PhysicalEvent physicalEvent;
        commitDueEvents(start, physicalEvent);

        const std::size_t cursorBeforePeek = cursor_;
        const float cutoffTargetBeforePeek = cutoffTarget_;
        const float resonanceTargetBeforePeek = resonanceTarget_;
        if (!physicalEvent.active)
            peekUpcomingEvent(start, end);
        peekPure_ = peekPure_ && cursor_ == cursorBeforePeek
            && cutoffTarget_ == cutoffTargetBeforePeek
            && resonanceTarget_ == resonanceTargetBeforePeek;

        if (!physicalEvent.active && pending_.valid)
        {
            const auto& event = schedule_.events[pending_.index];
            physicalEvent = {
                true, event.destination, event.voice, pending_.position,
                event.destination
                        == YouKnow106TestAccess::Destination::Resonance
                    ? resonanceTarget_ : cutoffTarget_,
                pending_.target
            };
        }

        const bool cutoffEvent = physicalEvent.active
            && physicalEvent.destination
                   == YouKnow106TestAccess::Destination::Vcf
            && physicalEvent.voice == profile_.card;
        const bool resonanceEvent = physicalEvent.active
            && physicalEvent.destination
                   == YouKnow106TestAccess::Destination::Resonance;
        const bool exactControl = cutoffEvent || resonanceEvent;
        YouKnow106TestAccess::VcfHoldInterval cutoff;
        YouKnow106TestAccess::VcfHoldInterval resonance;
        if (exactControl)
        {
            ++exactControlIntervals_;
            const float cutoffTargetBeforeEvent = cutoffEvent
                ? physicalEvent.previousTarget : cutoffTarget_;
            const float resonanceTargetBeforeEvent = resonanceEvent
                ? physicalEvent.previousTarget : resonanceTarget_;
            cutoff = YouKnow106TestAccess::exactVcfHoldInterval(
                cutoffState_, cutoffTargetBeforeEvent, cutoffEvent,
                physicalEvent.position,
                cutoffEvent ? physicalEvent.target
                            : cutoffTargetBeforeEvent,
                interval);
            resonance = YouKnow106TestAccess::exactVcfHoldInterval(
                resonanceState_, resonanceTargetBeforeEvent,
                resonanceEvent, physicalEvent.position,
                resonanceEvent ? physicalEvent.target
                               : resonanceTargetBeforeEvent,
                interval);
            checkHoldInterval(
                cutoff, cutoffState_, cutoffTargetBeforeEvent, cutoffEvent,
                physicalEvent.position,
                cutoffEvent ? physicalEvent.target
                            : cutoffTargetBeforeEvent,
                exactTrajectories_.cutoff, start, interval);
            checkHoldInterval(
                resonance, resonanceState_, resonanceTargetBeforeEvent,
                resonanceEvent, physicalEvent.position,
                resonanceEvent ? physicalEvent.target
                               : resonanceTargetBeforeEvent,
                exactTrajectories_.resonance, start, interval);
        }
        else
        {
            // Match the ordinary shipping endpoint arithmetic. Only an
            // interval containing a physical VCF/resonance write needs the
            // seven-node override; all other intervals retain Step 10's
            // endpoint-linear Merson control path.
            const float inverseRate = static_cast<float>(1.0 / internalRate_);
            const float holdSeconds = static_cast<float>(
                YouKnow106TestAccess::holdSeconds());
            const float slew = 1.0f
                - std::exp(-inverseRate / holdSeconds);
            cutoffState_ += (cutoffTarget_ - cutoffState_) * slew;
            resonanceState_ +=
                (resonanceTarget_ - resonanceState_) * slew;
        }

        Frame result;
        result.exactControl = exactControl;
        if (exactControl)
        {
            constexpr auto positions =
                YouKnow106TestAccess::controlNodePositions();
            static_assert(positions.size()
                          == std::tuple_size_v<decltype(
                              result.trajectory.omegaStep)>);
            for (std::size_t point = 0; point < positions.size(); ++point)
            {
                const float feedback = YouKnow106TestAccess::feedback(
                    static_cast<float>(resonance.value[point]), profile_);
                result.trajectory.feedback[point] = feedback;
                result.trajectory.omegaStep[point] =
                    YouKnow106TestAccess::omegaStep(
                        static_cast<float>(cutoff.value[point]), feedback,
                        profile_, internalRate_);
                result.trajectory.headroom[point] = profile_.headroom;
            }
            cutoffState_ = cutoff.endpoint;
            resonanceState_ = resonance.endpoint;
        }
        const float endpointFeedback = YouKnow106TestAccess::feedback(
            resonanceState_, profile_);
        const float endpointOmega = YouKnow106TestAccess::omegaStep(
            cutoffState_, endpointFeedback, profile_, internalRate_);
        const double endpointInput = toneVolts
            * static_cast<double>(
                YouKnow106TestAccess::inputCompensation(endpointFeedback))
            * std::sin(2.0 * pi * toneHz * end);
        result.endpoint = {
            static_cast<double>(endpointOmega) * internalRate_,
            endpointFeedback, profile_.headroom, endpointInput
        };

        if (exactControl)
        {
            endpointExact_ = endpointExact_
                && result.trajectory.omegaStep.back() == endpointOmega
                && result.trajectory.feedback.back() == endpointFeedback
                && result.trajectory.headroom.back() == profile_.headroom;
        }
        time_ = end;
        return result;
    }

    [[nodiscard]] std::size_t totalWrites() const noexcept
    {
        return totalWrites_;
    }

    [[nodiscard]] std::size_t cutoffWrites() const noexcept
    {
        return cutoffWrites_;
    }

    [[nodiscard]] std::size_t resonanceWrites() const noexcept
    {
        return resonanceWrites_;
    }

    [[nodiscard]] std::size_t cursor() const noexcept
    {
        return cursor_;
    }

    [[nodiscard]] std::size_t peeks() const noexcept
    {
        return peeks_;
    }

    [[nodiscard]] double maximumHoldDifference() const noexcept
    {
        return maximumHoldDifference_;
    }

    [[nodiscard]] std::size_t exactControlIntervals() const noexcept
    {
        return exactControlIntervals_;
    }

    [[nodiscard]] bool contractExact() const noexcept
    {
        constexpr std::size_t relevantWritesPerPass = 7u;
        constexpr std::size_t expectedFractionalPeeks =
            renderedPasses * relevantWritesPerPass - 1u;
        constexpr std::size_t expectedControlIntervals =
            renderedPasses * 2u;
        return cursorExact_ && peekPure_ && payloadLatched_
            && singleEventBound_ && directHoldExact_
            && oracleHoldExact_ && endpointExact_
            && peeks_ == expectedFractionalPeeks
            && exactControlIntervals_ == expectedControlIntervals;
    }

private:
    struct PhysicalEvent
    {
        bool active {};
        YouKnow106TestAccess::Destination destination {
            YouKnow106TestAccess::Destination::Noise
        };
        int voice { -1 };
        double position {};
        float previousTarget {};
        float target {};
    };

    struct PendingEvent
    {
        bool valid {};
        std::size_t index {};
        double position {};
        float target {};
    };

    [[nodiscard]] float targetForEvent(
        const ScheduledEvent& event) const noexcept
    {
        if (event.destination
                == YouKnow106TestAccess::Destination::Resonance)
        {
            return YouKnow106TestAccess::resonanceTargetForByte(
                resonanceBytes[static_cast<std::size_t>(event.pass)]);
        }
        if (event.destination == YouKnow106TestAccess::Destination::Vcf)
        {
            return YouKnow106TestAccess::cutoffTargetForByte(
                cutoffBytes[static_cast<std::size_t>(event.pass)],
                profile_.character);
        }
        return 0.0f;
    }

    void commitDueEvents(double start, PhysicalEvent& physicalEvent) noexcept
    {
        while (cursor_ < schedule_.events.size()
               && schedule_.events[cursor_].time <= start + 1.0e-15)
        {
            const auto& event = schedule_.events[cursor_];
            const bool usesLatch = pending_.valid
                && pending_.index == cursor_;
            const float target = usesLatch
                ? pending_.target : targetForEvent(event);
            if (usesLatch)
            {
                payloadLatched_ = payloadLatched_
                    && target == pending_.target;
                pending_ = {};
            }
            cursorExact_ = cursorExact_
                && event.pass == static_cast<int>(
                    cursor_ / YouKnow106TestAccess::writeCount())
                && event.ordinal
                    == cursor_ % YouKnow106TestAccess::writeCount();
            const bool relevant = event.destination
                    == YouKnow106TestAccess::Destination::Resonance
                || event.destination
                       == YouKnow106TestAccess::Destination::Vcf;
            if (relevant && !usesLatch && !physicalEvent.active)
            {
                physicalEvent = {
                    true, event.destination, event.voice, 0.0,
                    event.destination
                            == YouKnow106TestAccess::Destination::Resonance
                        ? resonanceTarget_ : cutoffTarget_,
                    target
                };
            }
            if (event.destination
                    == YouKnow106TestAccess::Destination::Resonance)
            {
                resonanceTarget_ = target;
                ++resonanceWrites_;
            }
            else if (event.destination
                         == YouKnow106TestAccess::Destination::Vcf
                     && event.voice == profile_.card)
            {
                cutoffTarget_ = target;
                ++cutoffWrites_;
            }
            ++cursor_;
            ++totalWrites_;
        }
    }

    void peekUpcomingEvent(double start, double end) noexcept
    {
        if (pending_.valid || cursor_ >= schedule_.events.size())
            return;
        const auto& event = schedule_.events[cursor_];
        const bool relevant = event.destination
                == YouKnow106TestAccess::Destination::Resonance
            || event.destination == YouKnow106TestAccess::Destination::Vcf;
        if (!relevant)
            return;
        if (!(event.time > start + 1.0e-15
              && event.time <= end + 1.0e-15))
            return;
        if (cursor_ + 1u < schedule_.events.size())
            singleEventBound_ = singleEventBound_
                && schedule_.events[cursor_ + 1u].time > end + 1.0e-15;
        pending_ = {
            true, cursor_,
            std::clamp((event.time - start) * internalRate_, 0.0, 1.0),
            targetForEvent(event)
        };
        ++peeks_;
    }

    static double independentlyAdvancedHold(
        double state, double target, bool hasEvent, double eventPosition,
        double eventTarget, double position, double interval) noexcept
    {
        const auto advance = [](double value, double destination,
                                double duration) {
            return destination + (value - destination)
                * std::exp(-duration
                    / YouKnow106TestAccess::holdSeconds());
        };
        if (!hasEvent || position <= eventPosition)
            return advance(state, target, position * interval);
        const double atEvent = advance(
            state, target, eventPosition * interval);
        return advance(atEvent, eventTarget,
                       (position - eventPosition) * interval);
    }

    void checkHoldInterval(
        const YouKnow106TestAccess::VcfHoldInterval& actual,
        float state, float target, bool hasEvent, double eventPosition,
        float eventTarget, const ExactHold& globalExact,
        double start, double interval) noexcept
    {
        constexpr auto positions = YouKnow106TestAccess::controlNodePositions();
        for (std::size_t point = 0; point < positions.size(); ++point)
        {
            const double expected = independentlyAdvancedHold(
                state, target, hasEvent, eventPosition, eventTarget,
                positions[point], interval);
            const double difference = std::abs(
                actual.value[point] - expected);
            maximumHoldDifference_ = std::max(
                maximumHoldDifference_, difference);
            const double scale = std::max({
                1.0, std::abs(actual.value[point]), std::abs(expected)
            });
            directHoldExact_ = directHoldExact_
                && difference <= 256.0
                    * std::numeric_limits<double>::epsilon() * scale;

            const double global = globalExact.value(
                start + positions[point] * interval);
            const double globalDifference = std::abs(
                actual.value[point] - global);
            const double globalScale = std::max({
                1.0, std::abs(actual.value[point]), std::abs(global)
            });
            // The shipping capacitor endpoint is intentionally float state.
            // One endpoint rounding error is attenuated by every later RC
            // interval; eps/(1-exp(-dt/tau)) is the closed-form worst-case
            // accumulation bound, so this remains rate-derived rather than a
            // fitted tolerance (and is most demanding on the 768 kHz grid).
            const double intervalResponse = -std::expm1(
                -interval / YouKnow106TestAccess::holdSeconds());
            const double accumulatedFloatBound =
                std::numeric_limits<float>::epsilon() * globalScale
                / std::max(intervalResponse,
                           static_cast<double>(
                               std::numeric_limits<float>::epsilon()));
            oracleHoldExact_ = oracleHoldExact_
                && globalDifference <= accumulatedFloatBound;
        }
        const float expectedEndpoint = static_cast<float>(
            independentlyAdvancedHold(
                state, target, hasEvent, eventPosition, eventTarget,
                1.0, interval));
        directHoldExact_ = directHoldExact_
            && actual.endpoint == expectedEndpoint;
    }

    const Schedule& schedule_;
    const ControlTrajectories& exactTrajectories_;
    const YouKnow106TestAccess::CardProfile& profile_;
    double internalRate_ {};
    float cutoffState_ {};
    float cutoffTarget_ {};
    float resonanceState_ {};
    float resonanceTarget_ {};
    double time_ {};
    std::size_t cursor_ {};
    std::size_t peeks_ {};
    std::size_t totalWrites_ {};
    std::size_t cutoffWrites_ {};
    std::size_t resonanceWrites_ {};
    std::size_t exactControlIntervals_ {};
    double maximumHoldDifference_ {};
    PendingEvent pending_ {};
    bool cursorExact_ { true };
    bool peekPure_ { true };
    bool payloadLatched_ { true };
    bool singleEventBound_ { true };
    bool directHoldExact_ { true };
    bool oracleHoldExact_ { true };
    bool endpointExact_ { true };
};

// Diagnosis-only independent transcription of the production Merson tableau.
// Its selectable control source lets the audit separate the integrator from
// endpoint interpolation and converter-event quantisation. It deliberately
// owns its own state and interpolation arithmetic; the admission oracle above
// remains the unrelated high-grid RK4 implementation.
struct DiagnosticControlPolicy
{
    bool exactOmega {};
    bool exactFeedback {};
    bool exactHeadroom {};
    bool exactInput {};
    bool cubicEndpointControls {};
};

class DiagnosticMersonCascade
{
public:
    DiagnosticMersonCascade(
        const YouKnow106TestAccess::CardProfile& profile,
        double internalRate)
        : profile_(profile), internalRate_(internalRate)
    {
    }

    float process(const InstantaneousControls& endpoint,
                  double intervalStart,
                  const ControlTrajectories* exactTrajectories,
                  DiagnosticControlPolicy policy)
    {
        const double currentOmega = std::clamp(
            static_cast<double>(static_cast<float>(
                endpoint.angularFrequency / internalRate_)),
            0.0, YouKnow106TestAccess::maximumOmegaStep());
        const double currentFeedback = std::clamp(
            static_cast<double>(static_cast<float>(endpoint.feedback)),
            0.0, 8.0);
        const double currentHeadroom = std::max(
            static_cast<double>(static_cast<float>(endpoint.headroom)),
            1.0e-5);
        const double currentInput = static_cast<double>(
            static_cast<float>(endpoint.input));
        if (!parameterHistoryPrimed_)
        {
            previousOmega_ = currentOmega;
            previousFeedback_ = currentFeedback;
            previousHeadroom_ = currentHeadroom;
            omegaHistory_.fill(currentOmega);
            feedbackHistory_.fill(currentFeedback);
            headroomHistory_.fill(currentHeadroom);
            parameterHistoryPrimed_ = true;
        }

        constexpr std::array<double, 7> positions {
            0.0, 1.0 / 6.0, 1.0 / 4.0, 1.0 / 2.0,
            2.0 / 3.0, 3.0 / 4.0, 1.0
        };
        std::array<double, positions.size()> inputAt {};
        std::array<double, positions.size()> omegaAt {};
        std::array<double, positions.size()> feedbackAt {};
        std::array<double, positions.size()> headroomAt {};
        const double interval = 1.0 / internalRate_;
        for (std::size_t point = 0; point < positions.size(); ++point)
        {
            const double position = positions[point];
            if (inputHistoryCount_ == 0)
                inputAt[point] = position * currentInput
                    + (1.0 - position) * inputHistory_[0];
            else if (inputHistoryCount_ == 1)
                inputAt[point] = 0.5 * position * (position + 1.0)
                        * currentInput
                    + (1.0 - position * position) * inputHistory_[0]
                    + 0.5 * position * (position - 1.0)
                        * inputHistory_[1];
            else
                inputAt[point] = position * (position + 1.0)
                        * (position + 2.0) / 6.0 * currentInput
                    - (position - 1.0) * (position + 1.0)
                        * (position + 2.0) / 2.0 * inputHistory_[0]
                    + (position - 1.0) * position
                        * (position + 2.0) / 2.0 * inputHistory_[1]
                    - (position - 1.0) * position
                        * (position + 1.0) / 6.0 * inputHistory_[2];
            omegaAt[point] = previousOmega_
                + position * (currentOmega - previousOmega_);
            feedbackAt[point] = previousFeedback_
                + position * (currentFeedback - previousFeedback_);
            headroomAt[point] = std::max(previousHeadroom_
                + position * (currentHeadroom - previousHeadroom_), 1.0e-5);
            const auto reconstructControl = [position, this](
                double current, const std::array<double, 3>& history) {
                if (parameterHistoryCount_ == 0)
                    return position * current
                        + (1.0 - position) * history[0];
                if (parameterHistoryCount_ == 1)
                    return 0.5 * position * (position + 1.0) * current
                        + (1.0 - position * position) * history[0]
                        + 0.5 * position * (position - 1.0) * history[1];
                return position * (position + 1.0)
                        * (position + 2.0) / 6.0 * current
                    - (position - 1.0) * (position + 1.0)
                        * (position + 2.0) / 2.0 * history[0]
                    + (position - 1.0) * position
                        * (position + 2.0) / 2.0 * history[1]
                    - (position - 1.0) * position
                        * (position + 1.0) / 6.0 * history[2];
            };
            if (policy.cubicEndpointControls)
            {
                omegaAt[point] = reconstructControl(
                    currentOmega, omegaHistory_);
                feedbackAt[point] = reconstructControl(
                    currentFeedback, feedbackHistory_);
                headroomAt[point] = std::max(reconstructControl(
                    currentHeadroom, headroomHistory_), 1.0e-5);
            }

            if (exactTrajectories != nullptr
                && (policy.exactOmega || policy.exactFeedback
                    || policy.exactHeadroom || policy.exactInput))
            {
                const auto exact = controlsAt(
                    intervalStart + position * interval,
                    *exactTrajectories, profile_, internalRate_);
                if (policy.exactOmega)
                    omegaAt[point] = exact.angularFrequency / internalRate_;
                if (policy.exactFeedback)
                    feedbackAt[point] = exact.feedback;
                if (policy.exactHeadroom)
                    headroomAt[point] = exact.headroom;
                if (policy.exactInput)
                    inputAt[point] = exact.input;
            }
        }

        const auto derivative = [&](const std::array<double, 4>& value,
                                    double drive, std::size_t point) {
            std::array<double, 4> result {};
            const double headroom = headroomAt[point];
            const double beta = YouKnow106TestAccess::earlyCoefficient()
                              * static_cast<double>(profile_.character);
            const double feedbackHeadroom =
                YouKnow106TestAccess::feedbackHeadroom();
            double previous = drive - feedbackAt[point] * feedbackHeadroom
                * std::tanh(value[3] / feedbackHeadroom);
            for (std::size_t stage = 0; stage < result.size(); ++stage)
            {
                const double early = 1.0
                    + beta * std::tanh(value[stage] / headroom);
                result[stage] = omegaAt[point]
                    * static_cast<double>(profile_.gScale[stage])
                    * early * headroom
                    * std::tanh((previous - value[stage]
                                 + static_cast<double>(
                                     profile_.offsetVoltage[stage]))
                                / headroom);
                previous = value[stage];
            }
            return result;
        };
        const auto addOne = [](const std::array<double, 4>& origin,
                               const std::array<double, 4>& slope,
                               double distance) {
            std::array<double, 4> result {};
            for (std::size_t stage = 0; stage < result.size(); ++stage)
                result[stage] = origin[stage] + distance * slope[stage];
            return result;
        };
        const auto addTwo = [](const std::array<double, 4>& origin,
                               double step,
                               const std::array<double, 4>& a, double wa,
                               const std::array<double, 4>& b, double wb) {
            std::array<double, 4> result {};
            for (std::size_t stage = 0; stage < result.size(); ++stage)
                result[stage] = origin[stage]
                    + step * (wa * a[stage] + wb * b[stage]);
            return result;
        };
        const auto addThree = [](const std::array<double, 4>& origin,
                                 double step,
                                 const std::array<double, 4>& a, double wa,
                                 const std::array<double, 4>& b, double wb,
                                 const std::array<double, 4>& c, double wc) {
            std::array<double, 4> result {};
            for (std::size_t stage = 0; stage < result.size(); ++stage)
                result[stage] = origin[stage] + step
                    * (wa * a[stage] + wb * b[stage] + wc * c[stage]);
            return result;
        };
        constexpr double substep = 0.5;
        for (int step = 0; step < 2; ++step)
        {
            const std::size_t origin = static_cast<std::size_t>(3 * step);
            const auto k1 = derivative(state_, inputAt[origin], origin);
            const auto k2 = derivative(
                addOne(state_, k1, substep / 3.0),
                inputAt[origin + 1u], origin + 1u);
            const auto k3 = derivative(
                addTwo(state_, substep, k1, 1.0 / 6.0,
                       k2, 1.0 / 6.0),
                inputAt[origin + 1u], origin + 1u);
            const auto k4 = derivative(
                addTwo(state_, substep, k1, 1.0 / 8.0,
                       k3, 3.0 / 8.0),
                inputAt[origin + 2u], origin + 2u);
            const auto k5 = derivative(
                addThree(state_, substep, k1, 1.0 / 2.0,
                         k3, -3.0 / 2.0, k4, 2.0),
                inputAt[origin + 3u], origin + 3u);
            state_ = addThree(state_, substep, k1, 1.0 / 6.0,
                              k4, 2.0 / 3.0, k5, 1.0 / 6.0);
        }

        for (std::size_t point = inputHistory_.size() - 1u;
             point > 0u; --point)
            inputHistory_[point] = inputHistory_[point - 1u];
        inputHistory_[0] = currentInput;
        inputHistoryCount_ = std::min(inputHistoryCount_ + 1, 2);
        for (std::size_t point = omegaHistory_.size() - 1u;
             point > 0u; --point)
        {
            omegaHistory_[point] = omegaHistory_[point - 1u];
            feedbackHistory_[point] = feedbackHistory_[point - 1u];
            headroomHistory_[point] = headroomHistory_[point - 1u];
        }
        omegaHistory_[0] = currentOmega;
        feedbackHistory_[0] = currentFeedback;
        headroomHistory_[0] = currentHeadroom;
        parameterHistoryCount_ = std::min(parameterHistoryCount_ + 1, 2);
        previousOmega_ = currentOmega;
        previousFeedback_ = currentFeedback;
        previousHeadroom_ = currentHeadroom;
        return static_cast<float>(state_[3]);
    }

private:
    const YouKnow106TestAccess::CardProfile& profile_;
    double internalRate_ {};
    std::array<double, 4> state_ {};
    std::array<double, 3> inputHistory_ {};
    std::array<double, 3> omegaHistory_ {};
    std::array<double, 3> feedbackHistory_ {};
    std::array<double, 3> headroomHistory_ {};
    int inputHistoryCount_ {};
    int parameterHistoryCount_ {};
    double previousOmega_ {};
    double previousFeedback_ {};
    double previousHeadroom_ {};
    bool parameterHistoryPrimed_ {};
};

template <typename Sample>
bool allFinite(const std::vector<Sample>& samples)
{
    return std::all_of(samples.begin(), samples.end(), [](const Sample value) {
        return std::isfinite(static_cast<double>(value));
    });
}

std::uint64_t hashSamples(std::span<const float> samples) noexcept
{
    std::uint64_t hash = 1469598103934665603ull;
    for (const float sample : samples)
    {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(sample);
        for (unsigned int byte = 0; byte < 4u; ++byte)
        {
            hash ^= (bits >> (8u * byte)) & 0xffu;
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

struct Take
{
    std::string name;
    int card {};
    float character {};
    double warmupSeconds {};
    std::vector<float> candidate;
    std::vector<double> rk64High;
    std::vector<double> rk128High;
    std::size_t candidateWrites {};
    std::size_t candidateCutoffWrites {};
    std::size_t candidateResonanceWrites {};
    std::size_t oracleWrites {};
    std::size_t oracleCutoffWrites {};
    std::size_t oracleResonanceWrites {};
    std::size_t recoveries {};
    double maximumState {};
    bool resetExact {};
    bool finite { true };
};

struct CandidateRender
{
    std::vector<float> samples;
    std::size_t writes {};
    std::size_t cutoffWrites {};
    std::size_t resonanceWrites {};
    std::size_t cursor {};
    std::size_t peeks {};
    std::size_t exactControlIntervals {};
    std::size_t recoveries {};
    double maximumState {};
    double maximumHoldDifference {};
    bool resetExact {};
    bool controlContractExact {};
    bool finite { true };
};

CandidateRender renderCandidate(
    const Schedule& schedule,
    const YouKnow106TestAccess::CardProfile& profile,
    double internalRate)
{
    const auto framesUnaligned = static_cast<std::size_t>(std::floor(
        renderedPasses * converterPassSeconds * internalRate));
    const std::size_t frames = framesUnaligned - framesUnaligned % 4u;
    CandidateRender render;
    render.samples.resize(frames);

    YouKnow106TestAccess::Cascade candidate;
    candidate.reset();
    YouKnow106TestAccess::configure(candidate, profile);
    render.resetExact = YouKnow106TestAccess::stateExactlyZero(candidate);
    const auto exactTrajectories = buildTrajectories(
        schedule, profile.card, profile);
    EventAwareCandidateScan candidateScan(
        schedule, exactTrajectories, profile, internalRate);
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const auto controls = candidateScan.advance();
        render.samples[frame] = candidate.process(
            static_cast<float>(controls.endpoint.input),
            static_cast<float>(
                controls.endpoint.angularFrequency / internalRate),
            static_cast<float>(controls.endpoint.feedback),
            static_cast<float>(controls.endpoint.headroom), true,
            profile.character,
            controls.exactControl ? &controls.trajectory : nullptr);
        const bool stateOkay =
            YouKnow106TestAccess::stateFiniteAndBounded(candidate);
        render.finite = render.finite && stateOkay
            && std::isfinite(render.samples[frame]);
        render.maximumState = std::max(
            render.maximumState,
            YouKnow106TestAccess::maximumStateMagnitude(candidate));
        if (frame > 8u && YouKnow106TestAccess::stateExactlyZero(candidate))
            ++render.recoveries;
    }
    render.writes = candidateScan.totalWrites();
    render.cutoffWrites = candidateScan.cutoffWrites();
    render.resonanceWrites = candidateScan.resonanceWrites();
    render.cursor = candidateScan.cursor();
    render.peeks = candidateScan.peeks();
    render.exactControlIntervals = candidateScan.exactControlIntervals();
    render.maximumHoldDifference =
        candidateScan.maximumHoldDifference();
    render.controlContractExact = candidateScan.contractExact()
        && render.cursor == schedule.events.size();
    render.finite = render.finite && allFinite(render.samples);
    return render;
}

Take renderTake(const Schedule& schedule,
                const YouKnow106TestAccess::CardProfile& profile,
                double baseRate, std::string name)
{
    const double internalRate = baseRate * 4.0;
    const double oracleRate = baseRate * 16.0;
    const auto framesUnaligned = static_cast<std::size_t>(std::floor(
        renderedPasses * converterPassSeconds * internalRate));
    const std::size_t candidateFrames = framesUnaligned
                                      - framesUnaligned % 4u;
    const std::size_t oracleFrames = candidateFrames * 4u;
    const auto trajectories = buildTrajectories(
        schedule, profile.card, profile);

    Take take;
    take.name = std::move(name);
    take.card = profile.card;
    take.character = profile.character;
    take.warmupSeconds = profile.warmSeconds;
    take.oracleWrites = trajectories.totalWrites;
    take.oracleCutoffWrites = trajectories.cutoffWrites;
    take.oracleResonanceWrites = trajectories.resonanceWrites;
    auto candidateRender = renderCandidate(schedule, profile, internalRate);
    if (candidateRender.samples.size() != candidateFrames)
        throw std::runtime_error("candidate render length mismatch");
    take.candidate = std::move(candidateRender.samples);
    take.candidateWrites = candidateRender.writes;
    take.candidateCutoffWrites = candidateRender.cutoffWrites;
    take.candidateResonanceWrites = candidateRender.resonanceWrites;
    take.recoveries = candidateRender.recoveries;
    take.maximumState = candidateRender.maximumState;
    take.resetExact = candidateRender.resetExact;
    take.finite = candidateRender.finite;
    take.rk64High.resize(oracleFrames);
    take.rk128High.resize(oracleFrames);

    ReferenceCascade rk64(profile, trajectories, internalRate);
    ReferenceCascade rk128(profile, trajectories, internalRate);
    const double interval = 1.0 / oracleRate;
    for (std::size_t frame = 0; frame < oracleFrames; ++frame)
    {
        const double start = static_cast<double>(frame) * interval;
        take.rk64High[frame] = rk64.advance(
            start, interval, rk64Substeps);
        take.rk128High[frame] = rk128.advance(
            start, interval, rk128Substeps);
        take.finite = take.finite && rk64.finite() && rk128.finite()
            && std::isfinite(take.rk64High[frame])
            && std::isfinite(take.rk128High[frame]);
    }
    take.finite = take.finite && allFinite(take.candidate)
        && allFinite(take.rk64High) && allFinite(take.rk128High);
    return take;
}

std::vector<double> endpointPhase(const std::vector<double>& high,
                                  std::size_t factor)
{
    const std::size_t phase = factor - 1u;
    if (high.size() <= phase)
        throw std::runtime_error("oracle render is too short");
    return { high.begin() + static_cast<std::ptrdiff_t>(phase), high.end() };
}

struct ReducedTake
{
    std::string name;
    std::vector<float> candidate;
    std::vector<double> rk64;
    std::vector<double> rk128;
    std::int64_t firstInternalFrame {};
    std::size_t recoveries {};
    std::size_t candidateWrites {};
    std::size_t candidateCutoffWrites {};
    std::size_t candidateResonanceWrites {};
    std::size_t oracleWrites {};
    std::size_t oracleCutoffWrites {};
    std::size_t oracleResonanceWrites {};
    double maximumState {};
    bool resetExact {};
    bool finite {};
};

ReducedTake reduceTake(const Take& take,
                       const quality::KaiserLowPass& filter)
{
    constexpr std::size_t oracleToInternal = 4u;
    const auto rk64 = quality::decimateToHostBoundary(
        endpointPhase(take.rk64High, oracleToInternal),
        oracleToInternal, filter);
    const auto rk128 = quality::decimateToHostBoundary(
        endpointPhase(take.rk128High, oracleToInternal),
        oracleToInternal, filter);
    if (rk64.firstHostFrame != rk128.firstHostFrame
        || rk64.samples.size() != rk128.samples.size())
        throw std::runtime_error("oracle reductions do not share a timeline");
    const auto first = static_cast<std::size_t>(rk128.firstHostFrame);
    const std::size_t available = std::min({
        rk64.samples.size(), rk128.samples.size(),
        take.candidate.size() > first ? take.candidate.size() - first : 0u
    });
    const std::size_t count = available - available % 4u;
    if (count == 0u)
        throw std::runtime_error("reduced oracle has no aligned samples");
    ReducedTake result;
    result.name = take.name;
    result.firstInternalFrame = rk128.firstHostFrame;
    result.rk64.assign(rk64.samples.begin(),
                       rk64.samples.begin() + static_cast<std::ptrdiff_t>(count));
    result.rk128.assign(rk128.samples.begin(),
                        rk128.samples.begin() + static_cast<std::ptrdiff_t>(count));
    result.candidate.assign(
        take.candidate.begin() + static_cast<std::ptrdiff_t>(first),
        take.candidate.begin()
            + static_cast<std::ptrdiff_t>(first + count));
    result.recoveries = take.recoveries;
    result.candidateWrites = take.candidateWrites;
    result.candidateCutoffWrites = take.candidateCutoffWrites;
    result.candidateResonanceWrites = take.candidateResonanceWrites;
    result.oracleWrites = take.oracleWrites;
    result.oracleCutoffWrites = take.oracleCutoffWrites;
    result.oracleResonanceWrites = take.oracleResonanceWrites;
    result.maximumState = take.maximumState;
    result.resetExact = take.resetExact;
    result.finite = take.finite && allFinite(result.candidate)
        && allFinite(result.rk64) && allFinite(result.rk128);
    return result;
}

quality::RmsComparison compareEightKCandidate(
    const ReducedTake& referenceTake,
    std::span<const float> rawCandidate)
{
    constexpr int factor = 4;
    const auto firstInternal = static_cast<std::size_t>(
        referenceTake.firstInternalFrame);
    if (rawCandidate.size() < firstInternal + referenceTake.candidate.size())
        throw std::runtime_error("diagnostic candidate is too short");
    std::vector<float> aligned(
        rawCandidate.begin() + static_cast<std::ptrdiff_t>(firstInternal),
        rawCandidate.begin() + static_cast<std::ptrdiff_t>(
            firstInternal + referenceTake.candidate.size()));
    auto candidate = YouKnow106TestAccess::decimate(aligned, factor);
    auto reference = YouKnow106TestAccess::decimate(
        referenceTake.rk128, factor);
    const double internalRate = 32000.0;
    const double passSeconds = 1.0 / YouKnow106TestAccess::scanRate();
    const std::size_t relativeCapture = static_cast<std::size_t>(std::max(
        0.0, std::ceil(captureFirstPass * passSeconds * internalRate
                     - static_cast<double>(referenceTake.firstInternalFrame))));
    const std::size_t first = std::min(
        candidate.size(), (relativeCapture + factor - 1u) / factor);
    return quality::compareRms(
        std::span<const double>(reference).subspan(first),
        std::span<const double>(candidate).subspan(first));
}

std::vector<float> renderDiagnosticCandidate(
    const YouKnow106TestAccess::CardProfile& profile,
    const ControlTrajectories* exactTrajectories,
    DiagnosticControlPolicy policy)
{
    constexpr double internalRate = 32000.0;
    const double passSeconds = 1.0 / YouKnow106TestAccess::scanRate();
    const auto framesUnaligned = static_cast<std::size_t>(std::floor(
        renderedPasses * passSeconds * internalRate));
    const std::size_t frames = framesUnaligned - framesUnaligned % 4u;
    std::vector<float> result(frames);
    EndpointCandidateScan scan(profile, internalRate);
    DiagnosticMersonCascade cascade(profile, internalRate);
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        const auto endpoint = scan.advanceEndpoint();
        result[frame] = cascade.process(
            endpoint, static_cast<double>(frame) / internalRate,
            exactTrajectories, policy);
    }
    return result;
}

struct DiagnosticAggregate
{
    std::string name;
    double worstRelative {};
    double worstPeak {};
    std::string worstTake;
};

struct DiagnosticProfileMetric
{
    std::string name;
    double endpointCloneRelative {};
};

struct EightKDiagnostics
{
    bool ran {};
    bool finite { true };
    std::size_t takes {};
    std::size_t writeMismatches {};
    double maximumRelevantEventDelayMicroseconds {};
    std::string maximumDelayEvent;
    double maximumUnclampedOmegaStep {};
    bool omegaCapEngaged {};
    double endpointCloneMaximumDifference {};
    std::vector<DiagnosticAggregate> variants;
    std::vector<DiagnosticProfileMetric> profiles;
    DiagnosticAggregate steady;
};

void accumulateDiagnostic(DiagnosticAggregate& aggregate,
                          const quality::RmsComparison& comparison,
                          std::string_view take)
{
    if (comparison.relativeError > aggregate.worstRelative)
    {
        aggregate.worstRelative = comparison.relativeError;
        aggregate.worstPeak = comparison.maximumAbsoluteError;
        aggregate.worstTake = take;
    }
}

Take renderSteadyTake(
    const YouKnow106TestAccess::CardProfile& profile,
    std::string name)
{
    constexpr double baseRate = 8000.0;
    constexpr double internalRate = baseRate * 4.0;
    constexpr double oracleRate = baseRate * 16.0;
    const double passSeconds = 1.0 / YouKnow106TestAccess::scanRate();
    const auto framesUnaligned = static_cast<std::size_t>(std::floor(
        renderedPasses * passSeconds * internalRate));
    const std::size_t candidateFrames = framesUnaligned
                                      - framesUnaligned % 4u;
    const std::size_t oracleFrames = candidateFrames * 4u;
    const double initialCutoff = YouKnow106TestAccess::cutoffTargetForByte(
        cutoffBytes[1], profile.character);
    const double initialResonance =
        YouKnow106TestAccess::resonanceTargetForByte(resonanceBytes[1]);
    const ControlTrajectories trajectories {
        ExactHold(initialCutoff, {}, YouKnow106TestAccess::holdSeconds()),
        ExactHold(initialResonance, {}, YouKnow106TestAccess::holdSeconds()),
        0u, 0u, 0u
    };

    Take take;
    take.name = std::move(name);
    take.candidate.resize(candidateFrames);
    take.rk64High.resize(oracleFrames);
    take.rk128High.resize(oracleFrames);
    YouKnow106TestAccess::Cascade candidate;
    candidate.reset();
    YouKnow106TestAccess::configure(candidate, profile);
    take.resetExact = YouKnow106TestAccess::stateExactlyZero(candidate);
    for (std::size_t frame = 0; frame < candidateFrames; ++frame)
    {
        const double time = static_cast<double>(frame + 1u) / internalRate;
        const auto controls = controlsAt(
            time, trajectories, profile, internalRate);
        take.candidate[frame] = candidate.process(
            static_cast<float>(controls.input),
            static_cast<float>(controls.angularFrequency / internalRate),
            static_cast<float>(controls.feedback),
            static_cast<float>(controls.headroom), true, profile.character);
        take.maximumState = std::max(
            take.maximumState,
            YouKnow106TestAccess::maximumStateMagnitude(candidate));
        take.finite = take.finite
            && YouKnow106TestAccess::stateFiniteAndBounded(candidate)
            && std::isfinite(take.candidate[frame]);
        if (frame > 8u && YouKnow106TestAccess::stateExactlyZero(candidate))
            ++take.recoveries;
    }
    ReferenceCascade rk64(profile, trajectories, internalRate);
    ReferenceCascade rk128(profile, trajectories, internalRate);
    const double interval = 1.0 / oracleRate;
    for (std::size_t frame = 0; frame < oracleFrames; ++frame)
    {
        const double start = static_cast<double>(frame) * interval;
        take.rk64High[frame] = rk64.advance(start, interval, rk64Substeps);
        take.rk128High[frame] = rk128.advance(start, interval, rk128Substeps);
        take.finite = take.finite && rk64.finite() && rk128.finite();
    }
    return take;
}

EightKDiagnostics diagnoseEightK(
    const Schedule& schedule,
    const std::vector<YouKnow106TestAccess::CardProfile>& profiles,
    const std::vector<ReducedTake>& reducedTakes,
    const quality::KaiserLowPass& filter)
{
    if (profiles.size() != reducedTakes.size())
        throw std::runtime_error("8 kHz diagnostic profile mismatch");
    constexpr double internalRate = 32000.0;
    EightKDiagnostics result;
    result.ran = true;
    result.variants = {
        { .name = "endpoint-linear-clone" },
        { .name = "causal-cubic-endpoints" },
        { .name = "snapped-event-exponential-controls" },
        { .name = "continuous-exact-omega" },
        { .name = "continuous-exact-feedback" },
        { .name = "continuous-exact-omega-feedback" },
        { .name = "continuous-exact-input" },
        { .name = "continuous-exact-controls-input" }
    };
    result.steady.name = "no-write-steady-controls";

    for (const auto& event : schedule.events)
    {
        if (event.destination
                != YouKnow106TestAccess::Destination::Resonance
            && event.destination != YouKnow106TestAccess::Destination::Vcf)
            continue;
        const double snapped = std::ceil(
            event.time * internalRate - 1.0e-12) / internalRate;
        const double delay = (snapped - event.time) * 1.0e6;
        if (delay > result.maximumRelevantEventDelayMicroseconds)
        {
            result.maximumRelevantEventDelayMicroseconds = delay;
            result.maximumDelayEvent =
                event.destination
                        == YouKnow106TestAccess::Destination::Resonance
                    ? std::string("resonance-pass")
                        + std::to_string(event.pass + 1)
                    : std::string("vcf-card")
                        + std::to_string(event.voice + 1) + "-pass"
                        + std::to_string(event.pass + 1);
        }
    }

    for (std::size_t index = 0; index < profiles.size(); ++index)
    {
        const auto& profile = profiles[index];
        const auto& reduced = reducedTakes[index];
        const auto continuous = buildTrajectories(
            schedule, profile.card, profile);
        const auto snapped = buildTrajectories(
            schedule, profile.card, profile, internalRate);
        const auto endpointClone = renderDiagnosticCandidate(
            profile, nullptr, {});
        const auto cubic = renderDiagnosticCandidate(
            profile, nullptr,
            { false, false, false, false, true });
        const auto snappedExact = renderDiagnosticCandidate(
            profile, &snapped,
            { true, true, true, false, false });
        const auto exactOmega = renderDiagnosticCandidate(
            profile, &continuous,
            { true, false, false, false, false });
        const auto exactFeedback = renderDiagnosticCandidate(
            profile, &continuous,
            { false, true, false, false, false });
        const auto exactOmegaFeedback = renderDiagnosticCandidate(
            profile, &continuous,
            { true, true, true, false, false });
        const auto exactInput = renderDiagnosticCandidate(
            profile, &continuous,
            { false, false, false, true, false });
        const auto exactAll = renderDiagnosticCandidate(
            profile, &continuous,
            { true, true, true, true, false });
        const std::array<std::span<const float>, 8> candidates {
            endpointClone, cubic, snappedExact, exactOmega, exactFeedback,
            exactOmegaFeedback, exactInput, exactAll
        };
        for (std::size_t variant = 0; variant < candidates.size(); ++variant)
        {
            const auto comparison = compareEightKCandidate(
                reduced, candidates[variant]);
            accumulateDiagnostic(result.variants[variant], comparison,
                                 reduced.name);
            result.finite = result.finite
                && std::all_of(candidates[variant].begin(),
                               candidates[variant].end(), [](float value) {
                                   return std::isfinite(value);
                               })
                && std::isfinite(comparison.relativeError)
                && std::isfinite(comparison.maximumAbsoluteError);
        }

        constexpr std::size_t expectedWrites =
            renderedPasses * YouKnow106TestAccess::writeCount();
        if (continuous.totalWrites != expectedWrites
            || snapped.totalWrites != expectedWrites
            || continuous.cutoffWrites
                   != static_cast<std::size_t>(renderedPasses)
            || snapped.cutoffWrites
                   != static_cast<std::size_t>(renderedPasses)
            || continuous.resonanceWrites
                   != static_cast<std::size_t>(renderedPasses)
            || snapped.resonanceWrites
                   != static_cast<std::size_t>(renderedPasses))
            ++result.writeMismatches;

        const auto first = static_cast<std::size_t>(reduced.firstInternalFrame);
        for (std::size_t frame = 0; frame < reduced.candidate.size(); ++frame)
            result.endpointCloneMaximumDifference = std::max(
                result.endpointCloneMaximumDifference,
                std::abs(static_cast<double>(endpointClone[first + frame])
                         - static_cast<double>(reduced.candidate[frame])));
        const auto endpointCloneComparison = compareEightKCandidate(
            reduced, endpointClone);
        result.profiles.push_back(
            { reduced.name, endpointCloneComparison.relativeError });

        constexpr std::array<double, 7> nodePositions {
            0.0, 1.0 / 6.0, 1.0 / 4.0, 1.0 / 2.0,
            2.0 / 3.0, 3.0 / 4.0, 1.0
        };
        const std::size_t frameCount = endpointClone.size();
        for (std::size_t frame = 0; frame < frameCount; ++frame)
        {
            for (const double position : nodePositions)
            {
                const double time = (static_cast<double>(frame) + position)
                                  / internalRate;
                const float resonance = static_cast<float>(
                    continuous.resonance.value(time));
                const float feedback = YouKnow106TestAccess::feedback(
                    resonance, profile);
                const float cutoff = static_cast<float>(
                    continuous.cutoff.value(time));
                result.maximumUnclampedOmegaStep = std::max(
                    result.maximumUnclampedOmegaStep,
                    YouKnow106TestAccess::unclampedOmegaStep(
                        cutoff, feedback, profile, internalRate));
            }
        }

        auto steadyTake = renderSteadyTake(profile, reduced.name);
        const auto steadyReduced = reduceTake(steadyTake, filter);
        const auto steadyComparison = compareEightKCandidate(
            steadyReduced, steadyTake.candidate);
        accumulateDiagnostic(
            result.steady, steadyComparison,
            reduced.name);
        result.finite = result.finite && steadyTake.finite
            && steadyReduced.finite
            && std::isfinite(steadyComparison.relativeError)
            && std::isfinite(steadyComparison.maximumAbsoluteError);
        ++result.takes;
    }
    result.finite = result.finite && result.takes == profiles.size()
        && result.writeMismatches == 0u;
    result.omegaCapEngaged = result.maximumUnclampedOmegaStep
        >= YouKnow106TestAccess::maximumOmegaStep();
    return result;
}

struct CellMetrics
{
    Cell cell {};
    double worstRelative {};
    double worstConvergence {};
    double worstPeakError {};
    double maximumState {};
    double minimumReferenceRms { std::numeric_limits<double>::infinity() };
    std::size_t minimumFrames { std::numeric_limits<std::size_t>::max() };
    std::size_t takes {};
    std::size_t peeks {};
    std::size_t exactControlIntervals {};
    std::size_t recoveries {};
    std::size_t writeMismatches {};
    std::size_t controlContractMismatches {};
    double maximumHoldDifference {};
    std::uint64_t rawHash {};
    std::string worstTake;
    int preparedFactor {};
    double preparedInternalRate {};
    bool familyRateExact {};
    bool shippingSelectorExact {};
    bool finite { true };
    bool structuralPass {};
    bool qualityPass {};
    bool pass {};
};

CellMetrics auditCell(const Cell& cell,
                      const std::vector<ReducedTake>& takes,
                      const std::vector<CandidateRender>& selectorCandidates)
{
    if (takes.size() != selectorCandidates.size())
        throw std::runtime_error("selector candidate profile mismatch");
    const double internalRate = familyBaseRates[
        static_cast<std::size_t>(cell.family)] * 4.0;
    CellMetrics metrics;
    metrics.cell = cell;
    metrics.rawHash = 1469598103934665603ull;
    metrics.familyRateExact = cell.hostRate
        * static_cast<double>(cell.factor) == internalRate;
    const auto prepared = YouKnow106TestAccess::shippingProcessingRate(
        cell.hostRate);
    metrics.preparedFactor = prepared.factor;
    metrics.preparedInternalRate = prepared.internalRate;
    metrics.shippingSelectorExact = prepared.factor == cell.factor
        && prepared.internalRate == internalRate;
    for (std::size_t takeIndex = 0; takeIndex < takes.size(); ++takeIndex)
    {
        const auto& take = takes[takeIndex];
        const auto& rawCandidate = selectorCandidates[takeIndex];
        const auto firstInternal = static_cast<std::size_t>(
            take.firstInternalFrame);
        if (rawCandidate.samples.size()
                < firstInternal + take.candidate.size())
            throw std::runtime_error("selector candidate is too short");
        std::vector<float> alignedCandidate(
            rawCandidate.samples.begin()
                + static_cast<std::ptrdiff_t>(firstInternal),
            rawCandidate.samples.begin()
                + static_cast<std::ptrdiff_t>(
                    firstInternal + take.candidate.size()));
        auto candidate = YouKnow106TestAccess::decimate(
            alignedCandidate, cell.factor);
        auto rk64 = YouKnow106TestAccess::decimate(take.rk64, cell.factor);
        auto rk128 = YouKnow106TestAccess::decimate(take.rk128, cell.factor);
        const std::size_t relativeCapture = static_cast<std::size_t>(std::max(
            0.0, std::ceil(captureFirstPass * converterPassSeconds
                         * internalRate
                         - static_cast<double>(take.firstInternalFrame))));
        const std::size_t first = std::min(
            candidate.size(),
            (relativeCapture + static_cast<std::size_t>(cell.factor) - 1u)
                / static_cast<std::size_t>(cell.factor));
        if (first >= candidate.size() || candidate.size() != rk64.size()
            || candidate.size() != rk128.size())
            throw std::runtime_error("cell capture has no aligned samples");
        const auto candidateSpan = std::span<const double>(candidate).subspan(first);
        const auto rk64Span = std::span<const double>(rk64).subspan(first);
        const auto rk128Span = std::span<const double>(rk128).subspan(first);
        const auto comparison = quality::compareRms(rk128Span, candidateSpan);
        const auto convergence = quality::compareRms(rk128Span, rk64Span);
        if (comparison.relativeError > metrics.worstRelative)
        {
            metrics.worstRelative = comparison.relativeError;
            metrics.worstPeakError = comparison.maximumAbsoluteError;
            metrics.worstTake = take.name;
        }
        metrics.worstConvergence = std::max(
            metrics.worstConvergence, convergence.relativeError);
        metrics.minimumReferenceRms = std::min(
            metrics.minimumReferenceRms, comparison.referenceRms);
        metrics.maximumState = std::max(metrics.maximumState,
                                        rawCandidate.maximumState);
        metrics.minimumFrames = std::min(metrics.minimumFrames,
                                         candidateSpan.size());
        metrics.recoveries += rawCandidate.recoveries;
        metrics.peeks += rawCandidate.peeks;
        metrics.exactControlIntervals +=
            rawCandidate.exactControlIntervals;
        metrics.maximumHoldDifference = std::max(
            metrics.maximumHoldDifference,
            rawCandidate.maximumHoldDifference);
        metrics.rawHash ^= hashSamples(rawCandidate.samples);
        metrics.rawHash *= 1099511628211ull;
        constexpr std::size_t expectedWrites =
            renderedPasses * YouKnow106TestAccess::writeCount();
        if (rawCandidate.writes != expectedWrites
            || rawCandidate.cursor != expectedWrites
            || take.oracleWrites != expectedWrites
            || rawCandidate.cutoffWrites
                   != static_cast<std::size_t>(renderedPasses)
            || take.oracleCutoffWrites
                   != static_cast<std::size_t>(renderedPasses)
            || rawCandidate.resonanceWrites
                   != static_cast<std::size_t>(renderedPasses)
            || take.oracleResonanceWrites
                   != static_cast<std::size_t>(renderedPasses))
            ++metrics.writeMismatches;
        if (!rawCandidate.controlContractExact)
            ++metrics.controlContractMismatches;
        metrics.finite = metrics.finite && take.finite
            && rawCandidate.finite && rawCandidate.resetExact
            && std::isfinite(comparison.relativeError)
            && std::isfinite(convergence.relativeError)
            && allFinite(candidate) && allFinite(rk64) && allFinite(rk128);
        ++metrics.takes;
    }
    // Four service passes leave only 100 post-warmup host frames at 8 kHz;
    // requiring the high-rate 128-frame floor there would be impossible even
    // before the independent FIR removes unsupported endpoints. Its 64-frame
    // floor still covers the same three control transitions and nearly two
    // complete 220 Hz periods. Every pre-existing cell keeps its 128 frames.
    const std::size_t minimumRequiredFrames = cell.hostRate == 8000.0
        ? 64u : 128u;
    metrics.structuralPass = metrics.finite && metrics.familyRateExact
        && metrics.shippingSelectorExact
        && metrics.takes == takes.size()
        && metrics.minimumFrames >= minimumRequiredFrames
        && metrics.recoveries == 0u && metrics.writeMismatches == 0u
        && metrics.controlContractMismatches == 0u
        && metrics.minimumReferenceRms > 1.0e-5
        && metrics.maximumState > 0.1 && metrics.maximumState <= 64.0
        && metrics.worstConvergence <= convergenceGate;
    metrics.qualityPass = metrics.structuralPass
        && metrics.worstRelative <= relativeRmsGate;
    metrics.pass = metrics.qualityPass;
    return metrics;
}

bool sameProfile(const YouKnow106TestAccess::CardProfile& left,
                 const YouKnow106TestAccess::CardProfile& right) noexcept
{
    return left.gScale == right.gScale
        && left.offsetVoltage == right.offsetVoltage
        && left.headroom == right.headroom
        && left.thermalCutoffSpread == right.thermalCutoffSpread;
}

struct Audit
{
    std::array<CellMetrics, shippingCells.size()> cells {};
    std::array<quality::ReferenceFilterCheck,
               familyBaseRates.size()> filterChecks {};
    std::array<std::uint64_t, familyBaseRates.size()> familyHashes {};
    bool scheduleExact {};
    YouKnow106TestAccess::SchedulerContract schedulerContract;
    YouKnow106TestAccess::ShippingWiringContract shippingWiringContract;
    bool nominalCollapseExact {};
    bool nominalColdWarmRenderExact {};
    bool familyRowsShareRaw {};
    bool coverageCountsExact {};
    bool standardAdmissionPass {};
    bool endpointClassificationsExact {};
    std::size_t physicalTakesPerFamily {};
    std::size_t logicalProfilesPerFamily {};
    CellMetrics lateSnap;
    CellMetrics earlySnap;
    bool snapNegativeControlsExact {};
    EightKDiagnostics eightKDiagnostics;
    bool pass {};
};

Audit runAudit(bool diagnoseEightKCell = false)
{
    const Schedule schedule = buildSchedule();
    Audit audit;
    audit.schedulerContract = YouKnow106TestAccess::schedulerContract();
    audit.shippingWiringContract =
        YouKnow106TestAccess::shippingWiringContract();
    audit.scheduleExact = schedule.phasesExact && schedule.orderExact
        && schedule.scanRateExact
        && schedule.events.size()
               == renderedPasses * YouKnow106TestAccess::writeCount();

    // Character zero is a calibrated nominal circuit.  Its analogue card
    // values and both thermal milestones collapse exactly, but its six VCF
    // holds still occupy six different service-chart ordinals.  Render every
    // card timing; only cold/warm may share an equation at Character zero.
    const auto nominal = YouKnow106TestAccess::cardProfile(0, 0.0f, 0.0);
    audit.nominalCollapseExact = true;
    for (int card = 0; card < 6; ++card)
    {
        const auto cold = YouKnow106TestAccess::cardProfile(card, 0.0f, 0.0);
        const auto warm = YouKnow106TestAccess::cardProfile(
            card, 0.0f, warmSeconds);
        audit.nominalCollapseExact = audit.nominalCollapseExact
            && sameProfile(nominal, cold) && sameProfile(nominal, warm);
    }

    std::vector<YouKnow106TestAccess::CardProfile> profiles;
    profiles.push_back(nominal);
    // Render card 1 warm too: this turns the algebraic thermal-collapse fence
    // into a raw-float end-to-end equality assertion. The other five cards
    // retain their distinct scan ordinals and are rendered cold below.
    profiles.push_back(YouKnow106TestAccess::cardProfile(
        0, 0.0f, warmSeconds));
    for (int card = 1; card < 6; ++card)
        profiles.push_back(YouKnow106TestAccess::cardProfile(
            card, 0.0f, 0.0));
    for (int card = 0; card < 6; ++card)
    {
        profiles.push_back(YouKnow106TestAccess::cardProfile(
            card, 1.0f, 0.0));
        profiles.push_back(YouKnow106TestAccess::cardProfile(
            card, 1.0f, warmSeconds));
    }
    audit.physicalTakesPerFamily = profiles.size();
    audit.logicalProfilesPerFamily = 24u; // 6 cards x 2 Character x 2 thermal.
    audit.coverageCountsExact = audit.physicalTakesPerFamily == 19u
        && audit.logicalProfilesPerFamily == 24u;

    std::array<std::vector<ReducedTake>,
               familyBaseRates.size()> reducedFamilies;
    std::array<std::array<std::uint64_t, 2>,
               familyBaseRates.size()> nominalHashes {};
    for (std::size_t family = 0; family < familyBaseRates.size(); ++family)
    {
        const double baseRate = familyBaseRates[family];
        const double internalRate = baseRate * 4.0;
        const double oracleRate = baseRate * 16.0;
        const double passbandEdge = std::min(20000.0, 0.45 * internalRate);
        const bool endpointFamily = baseRate == 8000.0
                                 || baseRate == 192000.0;
        const std::size_t tapCount = endpointFamily
            ? endpointReferenceFilterTaps : referenceFilterTaps;
        const auto filter = quality::designKaiserLowPass({
            oracleRate, passbandEdge, 0.5 * internalRate,
            quality::referenceFilterAttenuationDb, tapCount
        });
        audit.filterChecks[family] = quality::checkReferenceFilter(filter);
        std::uint64_t familyHash = 1469598103934665603ull;
        for (std::size_t profileIndex = 0;
             profileIndex < profiles.size(); ++profileIndex)
        {
            const auto& profile = profiles[profileIndex];
            const std::string name = profile.character == 0.0f
                ? std::string("card") + std::to_string(profile.card + 1)
                    + "-character0-"
                    + (profile.warmSeconds > 0.0 ? "warm" : "cold")
                : std::string("card") + std::to_string(profile.card + 1)
                    + "-character1-"
                    + (profile.warmSeconds > 0.0 ? "warm" : "cold");
            auto take = renderTake(schedule, profile, baseRate, name);
            const std::uint64_t takeHash = hashSamples(take.candidate);
            if (profileIndex < 2u)
                nominalHashes[family][profileIndex] = takeHash;
            familyHash ^= takeHash;
            familyHash *= 1099511628211ull;
            reducedFamilies[family].push_back(reduceTake(take, filter));
        }
        audit.familyHashes[family] = familyHash;
        if (diagnoseEightKCell && baseRate == 8000.0)
            audit.eightKDiagnostics = diagnoseEightK(
                schedule, profiles, reducedFamilies[family], filter);
    }
    audit.nominalColdWarmRenderExact = true;
    for (std::size_t family = 0; family < nominalHashes.size(); ++family)
        audit.nominalColdWarmRenderExact =
            audit.nominalColdWarmRenderExact
            && nominalHashes[family][0] == nominalHashes[family][1];

    for (std::size_t index = 0; index < shippingCells.size(); ++index)
    {
        const auto& cell = shippingCells[index];
        const auto prepared = YouKnow106TestAccess::shippingProcessingRate(
            cell.hostRate);
        std::vector<CandidateRender> selectorCandidates;
        selectorCandidates.reserve(profiles.size());
        for (const auto& profile : profiles)
            selectorCandidates.push_back(renderCandidate(
                schedule, profile, prepared.internalRate));
        audit.cells[index] = auditCell(
            cell, reducedFamilies[static_cast<std::size_t>(cell.family)],
            selectorCandidates);
    }

    const auto auditSnapMutation = [&](bool early) {
        const auto snapped = snapScheduleToInternalGrid(
            schedule, 32000.0, early);
        std::vector<CandidateRender> candidates;
        candidates.reserve(profiles.size());
        for (const auto& profile : profiles)
            candidates.push_back(renderCandidate(
                snapped, profile, 32000.0));
        return auditCell(shippingCells[0], reducedFamilies[2], candidates);
    };
    audit.lateSnap = auditSnapMutation(false);
    audit.earlySnap = auditSnapMutation(true);
    audit.snapNegativeControlsExact = audit.lateSnap.structuralPass
        && !audit.lateSnap.qualityPass && audit.lateSnap.finite
        && audit.earlySnap.structuralPass
        && !audit.earlySnap.qualityPass && audit.earlySnap.finite;

    audit.familyRowsShareRaw = true;
    for (const auto& cell : audit.cells)
        audit.familyRowsShareRaw = audit.familyRowsShareRaw
            && cell.rawHash
                == audit.familyHashes[static_cast<std::size_t>(
                    cell.cell.family)];
    for (std::size_t first = 0; first < audit.cells.size(); ++first)
    {
        for (std::size_t second = first + 1u;
             second < audit.cells.size(); ++second)
        {
            if (audit.cells[first].cell.family
                    == audit.cells[second].cell.family)
                audit.familyRowsShareRaw = audit.familyRowsShareRaw
                    && audit.cells[first].rawHash
                           == audit.cells[second].rawHash;
        }
    }
    audit.standardAdmissionPass = true;
    audit.endpointClassificationsExact = true;
    for (const auto& cell : audit.cells)
    {
        if (cell.cell.hostRate == 8000.0)
        {
            // Fractional converter events are now part of the shipping VCF
            // trajectory.  The lowest selector endpoint therefore faces the
            // same unchanged -40 dB admission gate as every other cell.
            audit.endpointClassificationsExact =
                audit.endpointClassificationsExact
                && cell.qualityPass;
        }
        else if (cell.cell.hostRate == 768000.0)
            audit.endpointClassificationsExact =
                audit.endpointClassificationsExact && cell.qualityPass;
        else
            audit.standardAdmissionPass =
                audit.standardAdmissionPass && cell.qualityPass;
    }
    audit.pass = audit.scheduleExact && audit.schedulerContract.passed()
        && audit.shippingWiringContract.passed()
        && audit.nominalCollapseExact
        && audit.nominalColdWarmRenderExact && audit.familyRowsShareRaw
        && audit.coverageCountsExact;
    for (std::size_t family = 0; family < audit.filterChecks.size(); ++family)
        audit.pass = audit.pass && audit.filterChecks[family].passed();
    audit.pass = audit.pass && audit.standardAdmissionPass
        && audit.endpointClassificationsExact
        && audit.snapNegativeControlsExact;
    return audit;
}

double decibels(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-30));
}

void printAudit(const Audit& audit)
{
    std::cout << std::fixed << std::setprecision(6)
              << "VCF dynamic/Character numerical-quality audit\n"
              << "Oracle: family base x16 grid, RK4 x4/x8 = RK64/RK128 "
                 "relative to each family base; exact normalized service events "
                 "and analytic 522 us holds at every RK abscissa.\n"
              << "Stimulus: analytic precompensated 2.4 V, 220 Hz; "
              << renderedPasses << " converter passes, capture after pass "
              << captureFirstPass << ".\n"
              << "Coverage: " << audit.physicalTakesPerFamily
              << " physical takes/family represent "
              << audit.logicalProfilesPerFamily
              << " logical card/Character/thermal profiles.\n"
              << "Gates: candidate NRMS <= " << relativeRmsGate
              << " (" << decibels(relativeRmsGate)
              << " dB), RK64/RK128 NRMS <= " << convergenceGate
              << " (" << decibels(convergenceGate)
              << " dB), exact schedule/counts, finite state, zero recovery.\n"
              << "schedule=" << (audit.scheduleExact ? "PASS" : "FAIL")
              << " scheduler_peek_latch="
              << (audit.schedulerContract.passed() ? "PASS" : "FAIL")
              << " shipping_trajectory_wiring="
              << (audit.shippingWiringContract.passed() ? "PASS" : "FAIL")
              << " nominal_collapse="
              << (audit.nominalCollapseExact ? "PASS" : "FAIL")
              << " nominal_cold_warm_raw="
              << (audit.nominalColdWarmRenderExact ? "PASS" : "FAIL")
              << " family_raw_identity="
              << (audit.familyRowsShareRaw ? "PASS" : "FAIL")
              << " coverage_counts="
              << (audit.coverageCountsExact ? "PASS" : "FAIL")
              << " standard_admission="
              << (audit.standardAdmissionPass ? "PASS" : "FAIL")
              << " endpoint_classifications="
              << (audit.endpointClassificationsExact ? "PASS" : "FAIL")
              << " snap_negative_controls="
              << (audit.snapNegativeControlsExact ? "PASS" : "FAIL")
              << '\n';
    std::cout << "  scheduler peeks=" << audit.schedulerContract.peeks
              << " commits=" << audit.schedulerContract.commits
              << " pure_peek="
              << (audit.schedulerContract.purePeek ? "PASS" : "FAIL")
              << " payload_latched="
              << (audit.schedulerContract.payloadLatched ? "PASS" : "FAIL")
              << " cursor="
              << (audit.schedulerContract.cursorExact ? "PASS" : "FAIL")
              << " order="
              << (audit.schedulerContract.orderExact ? "PASS" : "FAIL")
              << " pass_wrap="
              << (audit.schedulerContract.passWrapExact ? "PASS" : "FAIL")
              << '\n';
    std::cout << "  shipping_wiring probes="
              << audit.shippingWiringContract.probes
              << " connected_max_diff=" << std::scientific
              << audit.shippingWiringContract.maximumConnectedStateDifference
              << " disconnected_min_diff="
              << audit.shippingWiringContract.minimumDisconnectedStateDifference
              << std::fixed
              << " connected="
              << (audit.shippingWiringContract.connectedExact
                      ? "PASS" : "FAIL")
              << " nullptr_mutation="
              << (audit.shippingWiringContract.disconnectedRejected
                      ? "REJECTED" : "MISSED")
              << '\n';

    for (std::size_t family = 0; family < audit.filterChecks.size(); ++family)
    {
        const auto& check = audit.filterChecks[family];
        std::cout << "  family " << familyBaseRates[family]
                  << " filter=" << (check.passed() ? "PASS" : "FAIL")
                  << " raw_hash=0x" << std::hex << audit.familyHashes[family]
                  << std::dec << '\n';
    }
    for (const auto& metrics : audit.cells)
    {
        const std::string_view disposition = metrics.pass ? "PASS" : "FAIL";
        std::cout << "  " << std::setprecision(1) << metrics.cell.hostRate
                  << " Hz q" << metrics.cell.factor << ' '
                  << disposition
                  << std::setprecision(3)
                  << " nrms=" << decibels(metrics.worstRelative) << " dB"
                  << " convergence=" << decibels(metrics.worstConvergence)
                  << " dB peak=" << metrics.worstPeakError << " V"
                  << " ref_rms_min=" << metrics.minimumReferenceRms << " V"
                  << " state=" << metrics.maximumState << " V"
                  << " frames=" << metrics.minimumFrames
                  << " takes=" << metrics.takes
                  << " recoveries=" << metrics.recoveries
                  << " write_mismatches=" << metrics.writeMismatches
                  << " control_mismatches="
                  << metrics.controlContractMismatches
                  << " peeks=" << metrics.peeks
                  << " exact_intervals="
                  << metrics.exactControlIntervals
                  << " hold_diff=" << std::scientific
                  << metrics.maximumHoldDifference << std::fixed
                  << " family_rate="
                  << (metrics.familyRateExact ? "exact" : "mismatch")
                  << " prepared=q" << metrics.preparedFactor << '@'
                  << metrics.preparedInternalRate
                  << (metrics.shippingSelectorExact ? " exact" : " mismatch")
                  << " worst=" << metrics.worstTake << '\n';
    }
    const auto printSnap = [](std::string_view name,
                              const CellMetrics& metrics) {
        const bool expectedReject = metrics.structuralPass
            && metrics.finite && !metrics.qualityPass;
        std::cout << "  negative " << name << ' '
                  << (expectedReject ? "PASS" : "FAIL")
                  << std::setprecision(3)
                  << " nrms=" << decibels(metrics.worstRelative) << " dB"
                  << " structural="
                  << (metrics.structuralPass ? "PASS" : "FAIL")
                  << " finite=" << (metrics.finite ? "PASS" : "FAIL")
                  << " write_mismatches=" << metrics.writeMismatches
                  << " control_mismatches="
                  << metrics.controlContractMismatches
                  << " peeks=" << metrics.peeks
                  << " exact_intervals="
                  << metrics.exactControlIntervals
                  << " hold_diff=" << std::scientific
                  << metrics.maximumHoldDifference << std::fixed
                  << " raw_hash=0x" << std::hex << metrics.rawHash
                  << std::dec << " worst=" << metrics.worstTake << '\n';
    };
    printSnap("late/ceil-snap", audit.lateSnap);
    printSnap("early/floor-snap", audit.earlySnap);
    std::cout << "Overall: " << (audit.pass ? "PASS" : "FAIL") << '\n';

    if (audit.eightKDiagnostics.ran)
    {
        const auto& diagnostic = audit.eightKDiagnostics;
        std::cout << "8 kHz isolation (diagnostic only; non-admission):\n"
                  << "  integrity="
                  << (diagnostic.finite ? "PASS" : "FAIL")
                  << " takes=" << diagnostic.takes
                  << " variants=" << diagnostic.variants.size()
                  << " write_mismatches=" << diagnostic.writeMismatches
                  << '\n'
                  << "  boundary max_relevant_event_delay="
                  << diagnostic.maximumRelevantEventDelayMicroseconds
                  << " us event=" << diagnostic.maximumDelayEvent << '\n'
                  << "  omega max_unclamped_step="
                  << diagnostic.maximumUnclampedOmegaStep
                  << " cap=" << YouKnow106TestAccess::maximumOmegaStep()
                  << " engaged="
                  << (diagnostic.omegaCapEngaged ? "yes" : "no") << '\n'
                  << "  endpoint_clone max_abs_difference="
                  << std::scientific << std::setprecision(9)
                  << diagnostic.endpointCloneMaximumDifference << " V\n"
                  << std::fixed << std::setprecision(6);
        for (const auto& variant : diagnostic.variants)
            std::cout << "  variant " << variant.name
                      << " nrms=" << decibels(variant.worstRelative) << " dB"
                      << " peak=" << variant.worstPeak << " V"
                      << " worst=" << variant.worstTake << '\n';
        std::cout << "  variant " << diagnostic.steady.name
                  << " nrms=" << decibels(diagnostic.steady.worstRelative)
                  << " dB peak=" << diagnostic.steady.worstPeak << " V"
                  << " worst=" << diagnostic.steady.worstTake << '\n';
        for (const auto& profile : diagnostic.profiles)
            std::cout << "  profile " << profile.name
                      << " endpoint_clone_nrms="
                      << decibels(profile.endpointCloneRelative) << " dB\n";
    }
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        bool selfTest = false;
        bool diagnoseEightKCell = false;
        if (argc == 2 && std::string_view(argv[1]) == "--self-test")
            selfTest = true;
        else if (argc == 2
                 && std::string_view(argv[1]) == "--diagnose-8k")
            diagnoseEightKCell = true;
        else if (argc != 1)
        {
            std::cerr << "Usage: YouKnow106VcfDynamicQualityAudit "
                         "[--self-test|--diagnose-8k]\n";
            return 2;
        }

        const Audit audit = runAudit(diagnoseEightKCell);
        printAudit(audit);
        if (selfTest && !audit.pass)
            std::cerr << "VCF dynamic quality self-test FAILED\n";
        return audit.pass ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "VCF dynamic quality audit failed: "
                  << error.what() << '\n';
        return 1;
    }
}
